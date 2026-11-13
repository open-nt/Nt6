// MIT License - see LICENSE
//
// SDL GameControllerDB mapping strings: parsing them, writing them back, and evaluating them over
// the raw controls of a device.
#include "qgamepadbackend_p.h"

#include <algorithm>

namespace QtPrivate {

namespace {

const char *const g_buttonNames[GamepadButtonCount] = {
    "a",     "b",     "x",         "y",          "leftshoulder", "rightshoulder", "lefttrigger", "righttrigger",
    "back",  "start", "leftstick", "rightstick", "dpup",         "dpdown",        "dpright",     "dpleft",
    "misc1", "guide"};
const char *const g_axisNames[GamepadAxisCount] = {"leftx", "lefty", "rightx", "righty"};

QString platformName() {
#if defined(__ANDROID__)
    return QStringLiteral("Android");
#elif defined(__linux__)
    return QStringLiteral("Linux");
#elif defined(_WIN32)
    return QStringLiteral("Windows");
#elif defined(__APPLE__)
    return QStringLiteral("Mac OS X");
#else
    return QString();
#endif
}

bool isTrigger(int button) { return button == QGamepadManager::ButtonL2 || button == QGamepadManager::ButtonR2; }

// Parses "b3", "a2", "+a2", "-a2~" or "h0.4".
bool parseSource(QString text, GamepadBinding &binding) {
    if (text.startsWith(QLatin1Char('+'))) {
        binding.inputRange = GamepadBinding::Positive;
        text.remove(0, 1);
    } else if (text.startsWith(QLatin1Char('-'))) {
        binding.inputRange = GamepadBinding::Negative;
        text.remove(0, 1);
    }
    if (text.endsWith(QLatin1Char('~'))) {
        binding.invert = true;
        text.chop(1);
    }
    if (text.size() < 2)
        return false;
    bool ok = false;
    const QChar kind = text.at(0);
    if (kind == QLatin1Char('h')) {
        const qsizetype dot = text.indexOf(QLatin1Char('.'));
        if (dot < 0)
            return false;
        binding.source = GamepadBinding::Hat;
        binding.index = text.mid(1, dot - 1).toInt(&ok);
        if (!ok)
            return false;
        binding.hatMask = text.mid(dot + 1).toInt(&ok);
        return ok && binding.index >= 0 && binding.hatMask > 0;
    }
    if (kind == QLatin1Char('b'))
        binding.source = GamepadBinding::Button;
    else if (kind == QLatin1Char('a'))
        binding.source = GamepadBinding::Axis;
    else
        return false;
    binding.index = text.mid(1).toInt(&ok);
    return ok && binding.index >= 0;
}

QString sourceText(const GamepadBinding &binding) {
    QString text;
    if (binding.inputRange == GamepadBinding::Positive)
        text += QLatin1Char('+');
    else if (binding.inputRange == GamepadBinding::Negative)
        text += QLatin1Char('-');
    switch (binding.source) {
    case GamepadBinding::Button:
        text += QLatin1Char('b') + QString::number(binding.index);
        break;
    case GamepadBinding::Axis:
        text += QLatin1Char('a') + QString::number(binding.index);
        break;
    case GamepadBinding::Hat:
        text += QLatin1Char('h') + QString::number(binding.index) + QLatin1Char('.') + QString::number(binding.hatMask);
        break;
    case GamepadBinding::None:
        break;
    }
    if (binding.invert)
        text += QLatin1Char('~');
    return text;
}

double rawAxis(const GamepadBinding &binding, const GamepadRawState &state) {
    const auto i = static_cast<std::size_t>(binding.index);
    const double v = i < state.axes.size() ? state.axes[i] : 0.0;
    return binding.invert ? -v : v;
}

// The binding's input as an amount from 0.0 to 1.0; a whole axis is taken from end to end.
double magnitude(const GamepadBinding &binding, const GamepadRawState &state) {
    const auto i = static_cast<std::size_t>(binding.index);
    switch (binding.source) {
    case GamepadBinding::Button:
        return i < state.buttons.size() ? state.buttons[i] : 0.0;
    case GamepadBinding::Hat:
        return i < state.hats.size() && (state.hats[i] & binding.hatMask) != 0 ? 1.0 : 0.0;
    case GamepadBinding::Axis: {
        const double v = rawAxis(binding, state);
        if (binding.inputRange == GamepadBinding::Positive)
            return std::max(0.0, v);
        if (binding.inputRange == GamepadBinding::Negative)
            return std::max(0.0, -v);
        return (v + 1.0) / 2.0;
    }
    case GamepadBinding::None:
        break;
    }
    return 0.0;
}

} // namespace

void GamepadRawState::apply(const GamepadInput &input) {
    if (input.index < 0)
        return;
    const auto i = static_cast<std::size_t>(input.index);
    switch (input.kind) {
    case GamepadInput::Button:
        if (buttons.size() <= i)
            buttons.resize(i + 1, 0.0);
        buttons[i] = input.value;
        break;
    case GamepadInput::Axis:
        if (axes.size() <= i)
            axes.resize(i + 1, 0.0);
        axes[i] = input.value;
        break;
    case GamepadInput::Hat:
        if (hats.size() <= i)
            hats.resize(i + 1, 0);
        hats[i] = static_cast<int>(input.value);
        break;
    }
}

QString gamepadButtonMappingName(QGamepadManager::GamepadButton button) {
    if (button < 0 || button >= GamepadButtonCount)
        return QString();
    return QString::fromLatin1(g_buttonNames[button]);
}

QString gamepadAxisMappingName(QGamepadManager::GamepadAxis axis) {
    if (axis < 0 || axis >= GamepadAxisCount)
        return QString();
    return QString::fromLatin1(g_axisNames[axis]);
}

QString gamepadGuid(quint16 bus, quint16 vendor, quint16 product, quint16 version, const QString &name) {
    unsigned char bytes[16] = {};
    const auto put = [&](int at, quint16 value) {
        bytes[at] = static_cast<unsigned char>(value & 0xff);
        bytes[at + 1] = static_cast<unsigned char>(value >> 8);
    };
    put(0, bus);
    if (vendor != 0 && product != 0) {
        put(4, vendor);
        put(8, product);
        put(12, version);
    } else {
        const QByteArray utf8 = name.toUtf8();
        const qsizetype n = std::min<qsizetype>(utf8.size(), 12);
        for (qsizetype i = 0; i < n; ++i)
            bytes[4 + i] = static_cast<unsigned char>(utf8.at(i));
    }
    static const char digits[] = "0123456789abcdef";
    QString guid;
    for (const unsigned char b : bytes) {
        guid += QLatin1Char(digits[b >> 4]);
        guid += QLatin1Char(digits[b & 0xf]);
    }
    return guid;
}

GuidMatch matchGamepadGuid(const QString &device, const QString &mapping) {
    if (device.size() != 32 || mapping.size() != 32)
        return GuidMatch::None;
    QString a = device.toLower();
    QString b = mapping.toLower();
    if (a == b)
        return GuidMatch::Exact;
    const QString zeros = QStringLiteral("0000");
    a.replace(4, 4, zeros); // the CRC of the name, from SDL 2.26
    b.replace(4, 4, zeros);
    if (a == b)
        return GuidMatch::Close;
    a.replace(24, 4, zeros); // the version
    b.replace(24, 4, zeros);
    return a == b ? GuidMatch::Loose : GuidMatch::None;
}

bool GamepadMapping::parse(const QString &line) {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
        return false;
    const QStringList fields = trimmed.split(QLatin1Char(','));
    if (fields.size() < 2 || fields.at(0).size() != 32)
        return false;
    bool hex = true;
    for (const QChar c : fields.at(0))
        hex = hex && (c.isDigit() || (c.toLower() >= QLatin1Char('a') && c.toLower() <= QLatin1Char('f')));
    if (!hex)
        return false;
    for (const QString &field : fields) {
        if (field.startsWith(QLatin1String("platform:")) && field.mid(9) != platformName())
            return false;
    }
    guid = fields.at(0).toLower();
    name = fields.at(1);
    QStringList rest = fields.mid(2);
    parseBindings(rest.join(QLatin1Char(',')));
    return true;
}

void GamepadMapping::parseBindings(const QString &bindings) {
    bool replaced[GamepadButtonCount + GamepadAxisCount] = {};
    for (const QString &field : bindings.split(QLatin1Char(','))) {
        const qsizetype colon = field.indexOf(QLatin1Char(':'));
        if (colon <= 0)
            continue;
        QString key = field.left(colon).trimmed();
        GamepadBinding binding;
        if (key.startsWith(QLatin1Char('+'))) {
            binding.outputRange = GamepadBinding::Positive;
            key.remove(0, 1);
        } else if (key.startsWith(QLatin1Char('-'))) {
            binding.outputRange = GamepadBinding::Negative;
            key.remove(0, 1);
        }
        int slot = -1;
        for (int i = 0; i < GamepadButtonCount && slot < 0; ++i) {
            if (key == QLatin1String(g_buttonNames[i]))
                slot = i;
        }
        for (int i = 0; i < GamepadAxisCount && slot < 0; ++i) {
            if (key == QLatin1String(g_axisNames[i]))
                slot = GamepadButtonCount + i;
        }
        if (slot < 0 || !parseSource(field.mid(colon + 1).trimmed(), binding))
            continue; // "platform", "crc", "hint", paddles, the touchpad and other controls Qt has no name for
        std::vector<GamepadBinding> &list = slot < GamepadButtonCount
                                                ? buttons[static_cast<std::size_t>(slot)]
                                                : axes[static_cast<std::size_t>(slot - GamepadButtonCount)];
        if (!replaced[slot]) {
            list.clear();
            replaced[slot] = true;
        }
        list.push_back(binding);
    }
}

QString GamepadMapping::toString() const {
    QString text = guid + QLatin1Char(',') + QString(name).remove(QLatin1Char(','));
    const auto add = [&](const char *key, const std::vector<GamepadBinding> &list) {
        for (const GamepadBinding &binding : list) {
            text += QLatin1Char(',');
            if (binding.outputRange == GamepadBinding::Positive)
                text += QLatin1Char('+');
            else if (binding.outputRange == GamepadBinding::Negative)
                text += QLatin1Char('-');
            text += QLatin1String(key);
            text += QLatin1Char(':') + sourceText(binding);
        }
    };
    for (int i = 0; i < GamepadButtonCount; ++i)
        add(g_buttonNames[i], buttons[static_cast<std::size_t>(i)]);
    for (int i = 0; i < GamepadAxisCount; ++i)
        add(g_axisNames[i], axes[static_cast<std::size_t>(i)]);
    const QString platform = platformName();
    if (!platform.isEmpty())
        text += QStringLiteral(",platform:") + platform;
    return text + QLatin1Char(',');
}

double GamepadMapping::buttonValue(QGamepadManager::GamepadButton button, const GamepadRawState &state) const {
    if (button < 0 || button >= GamepadButtonCount)
        return 0.0;
    double value = 0.0;
    for (const GamepadBinding &binding : buttons[static_cast<std::size_t>(button)])
        value = std::max(value, magnitude(binding, state));
    value = std::clamp(value, 0.0, 1.0);
    if (isTrigger(button))
        return value;
    return value > 0.5 ? 1.0 : 0.0;
}

double GamepadMapping::axisValue(QGamepadManager::GamepadAxis axis, const GamepadRawState &state) const {
    if (axis < 0 || axis >= GamepadAxisCount)
        return 0.0;
    double value = 0.0;
    for (const GamepadBinding &binding : axes[static_cast<std::size_t>(axis)]) {
        if (binding.source == GamepadBinding::Axis && binding.inputRange == GamepadBinding::Full &&
            binding.outputRange == GamepadBinding::Full) {
            value += rawAxis(binding, state);
            continue;
        }
        const double m = magnitude(binding, state);
        if (binding.outputRange == GamepadBinding::Positive)
            value += m;
        else if (binding.outputRange == GamepadBinding::Negative)
            value -= m;
        else
            value += m * 2.0 - 1.0;
    }
    return std::clamp(value, -1.0, 1.0);
}

} // namespace QtPrivate
