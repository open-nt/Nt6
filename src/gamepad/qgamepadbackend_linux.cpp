// MIT License - see LICENSE
//
// Nt Gamepad on Linux, through the evdev nodes /dev/input/event*. Each gamepad is read from its own
// non-blocking descriptor, rumble goes through the force feedback interface, and controls are
// numbered as SDL numbers them so that GameControllerDB mappings apply.
#include "qgamepadbackend_linux_p.h"
#include "qgamepadbackend_p.h"

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>

#include <linux/input.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace QtPrivate {

bool evdevIsGamepad(const EvdevHasCode &hasKey, const EvdevHasCode &hasAbs, const EvdevHasCode &hasProp) {
    if (hasProp(INPUT_PROP_ACCELEROMETER) || hasProp(INPUT_PROP_POINTER) || hasKey(BTN_TOUCH) ||
        hasKey(BTN_TOOL_FINGER) || hasKey(BTN_TOOL_PEN) || hasAbs(ABS_MT_POSITION_X))
        return false;
    bool joystickButton = false;
    for (int code = BTN_JOYSTICK; code <= BTN_THUMBR && !joystickButton; ++code)
        joystickButton = hasKey(code);
    for (int code = BTN_DPAD_UP; code <= BTN_DPAD_RIGHT && !joystickButton; ++code)
        joystickButton = hasKey(code);
    const bool stick = hasAbs(ABS_X) && hasAbs(ABS_Y);
    const bool hat = hasAbs(ABS_HAT0X) && hasAbs(ABS_HAT0Y);
    return joystickButton && (stick || hat);
}

EvdevLayout evdevLayout(const EvdevHasCode &hasKey, const EvdevHasCode &hasAbs) {
    EvdevLayout layout;
    for (int code = BTN_JOYSTICK; code < KEY_MAX; ++code) {
        if (hasKey(code))
            layout.buttonCodes.push_back(code);
    }
    for (int code = BTN_MISC; code < BTN_JOYSTICK; ++code) {
        if (hasKey(code))
            layout.buttonCodes.push_back(code);
    }
    for (int code = ABS_X; code < ABS_MAX; ++code) {
        if (code >= ABS_HAT0X && code <= ABS_HAT3Y)
            continue;
        if (hasAbs(code))
            layout.axisCodes.push_back(code);
    }
    for (int pair = 0; pair < 4; ++pair) {
        if (hasAbs(ABS_HAT0X + 2 * pair) || hasAbs(ABS_HAT0Y + 2 * pair))
            layout.hatPairs.push_back(pair);
    }
    return layout;
}

QString evdevDefaultMapping(const EvdevLayout &layout, quint16 vendor, bool &standard) {
    const auto indexOf = [](const std::vector<int> &codes, int code) {
        const auto it = std::find(codes.begin(), codes.end(), code);
        return it == codes.end() ? -1 : static_cast<int>(it - codes.begin());
    };
    QStringList fields;
    const auto button = [&](const char *key, int code) {
        const int i = indexOf(layout.buttonCodes, code);
        if (i < 0)
            return false;
        fields << QLatin1String(key) + QStringLiteral(":b") + QString::number(i);
        return true;
    };
    const auto axis = [&](const char *key, int code) {
        const int i = indexOf(layout.axisCodes, code);
        if (i < 0)
            return false;
        fields << QLatin1String(key) + QStringLiteral(":a") + QString::number(i);
        return true;
    };
    const auto hasAxis = [&](int code) { return indexOf(layout.axisCodes, code) >= 0; };

    standard = indexOf(layout.buttonCodes, BTN_SOUTH) >= 0;
    if (standard) {
        button("a", BTN_SOUTH);
        button("b", BTN_EAST);
        // The drivers for Xbox pads, and most others, report the left face button as BTN_X and the
        // top one as BTN_Y. Sony's follow the kernel's documentation, where BTN_WEST (the same
        // code as BTN_Y) is the left one.
        const bool positional = vendor == 0x054c;
        button("x", positional ? BTN_WEST : BTN_X);
        button("y", positional ? BTN_NORTH : BTN_Y);
        button("leftshoulder", BTN_TL);
        button("rightshoulder", BTN_TR);
        button("back", BTN_SELECT);
        button("start", BTN_START);
        button("guide", BTN_MODE);
        button("leftstick", BTN_THUMBL);
        button("rightstick", BTN_THUMBR);
    } else {
        // A pad that numbers its buttons, as cheap USB pads do: the usual order of those.
        static const char *const order[] = {
            "a",    "b",     "x",         "y",         "leftshoulder", "rightshoulder", "lefttrigger", "righttrigger",
            "back", "start", "leftstick", "rightstick"};
        const int n = std::min(static_cast<int>(layout.buttonCodes.size()), static_cast<int>(std::size(order)));
        for (int i = 0; i < n; ++i)
            fields << QLatin1String(order[i]) + QStringLiteral(":b") + QString::number(i);
    }

    axis("leftx", ABS_X);
    axis("lefty", ABS_Y);
    bool zIsStick = false;
    if (hasAxis(ABS_RX) && hasAxis(ABS_RY)) {
        axis("rightx", ABS_RX);
        axis("righty", ABS_RY);
    } else if (hasAxis(ABS_Z) && hasAxis(ABS_RZ)) {
        axis("rightx", ABS_Z);
        axis("righty", ABS_RZ);
        zIsStick = true;
    }
    if (!zIsStick && hasAxis(ABS_Z) && hasAxis(ABS_RZ)) {
        axis("lefttrigger", ABS_Z);
        axis("righttrigger", ABS_RZ);
    } else if (hasAxis(ABS_BRAKE) && hasAxis(ABS_GAS)) {
        axis("lefttrigger", ABS_BRAKE);
        axis("righttrigger", ABS_GAS);
    } else if (standard) {
        button("lefttrigger", BTN_TL2);
        button("righttrigger", BTN_TR2);
    }

    const auto firstHat = std::find(layout.hatPairs.begin(), layout.hatPairs.end(), 0);
    if (firstHat != layout.hatPairs.end()) {
        const QString h = QStringLiteral(":h") + QString::number(firstHat - layout.hatPairs.begin()) + QLatin1Char('.');
        fields << QStringLiteral("dpup") + h + QString::number(HatUp)
               << QStringLiteral("dpdown") + h + QString::number(HatDown)
               << QStringLiteral("dpleft") + h + QString::number(HatLeft)
               << QStringLiteral("dpright") + h + QString::number(HatRight);
    } else {
        button("dpup", BTN_DPAD_UP);
        button("dpdown", BTN_DPAD_DOWN);
        button("dpleft", BTN_DPAD_LEFT);
        button("dpright", BTN_DPAD_RIGHT);
    }
    return fields.join(QLatin1Char(','));
}

namespace {

constexpr std::size_t bitsPerLong = 8 * sizeof(unsigned long);

std::vector<unsigned long> bitArray(int bits) {
    return std::vector<unsigned long>(static_cast<std::size_t>(bits) / bitsPerLong + 1, 0UL);
}

bool testBit(const std::vector<unsigned long> &bits, int bit) {
    if (bit < 0)
        return false;
    const auto b = static_cast<std::size_t>(bit);
    return b / bitsPerLong < bits.size() && ((bits[b / bitsPerLong] >> (b % bitsPerLong)) & 1UL) != 0;
}

unsigned int byteSize(const std::vector<unsigned long> &bits) {
    return static_cast<unsigned int>(bits.size() * sizeof(unsigned long));
}

unsigned long absRequest(int code) { return EVIOCGABS(static_cast<unsigned int>(code)); }

QString ioctlString(int fd, unsigned long request) {
    char buffer[256] = {};
    if (::ioctl(fd, request, buffer) < 0)
        return QString();
    return QString::fromUtf8(buffer, static_cast<qsizetype>(::strnlen(buffer, sizeof buffer)));
}

// The value of an axis from -1.0 to 1.0, with the kernel's flat zone around the middle taken out.
double normalize(int value, const input_absinfo &info) {
    const double half = (static_cast<double>(info.maximum) - info.minimum) / 2.0;
    if (half <= 0.0)
        return 0.0;
    const double centre = (static_cast<double>(info.maximum) + info.minimum) / 2.0;
    double v = std::clamp((value - centre) / half, -1.0, 1.0);
    const double flat = std::min(info.flat / half, 0.9);
    if (flat > 0.0) {
        const double a = std::abs(v);
        v = a <= flat ? 0.0 : (v < 0 ? -1.0 : 1.0) * (a - flat) / (1.0 - flat);
    }
    return v;
}

class EvdevDevice final : public GamepadDevice {
public:
    EvdevDevice(int fd, bool writable) : m_fd(fd), m_writable(writable) {}
    ~EvdevDevice() override {
        if (m_effect >= 0)
            ::ioctl(m_fd, EVIOCRMFF, m_effect);
        ::close(m_fd);
    }

    bool init(const QString &path) {
        auto evBits = bitArray(EV_MAX);
        auto keyBits = bitArray(KEY_MAX);
        auto absBits = bitArray(ABS_MAX);
        auto ffBits = bitArray(FF_MAX);
        auto propBits = bitArray(INPUT_PROP_MAX);
        if (::ioctl(m_fd, EVIOCGBIT(0, byteSize(evBits)), evBits.data()) < 0)
            return false;
        if (testBit(evBits, EV_KEY))
            ::ioctl(m_fd, EVIOCGBIT(EV_KEY, byteSize(keyBits)), keyBits.data());
        if (testBit(evBits, EV_ABS))
            ::ioctl(m_fd, EVIOCGBIT(EV_ABS, byteSize(absBits)), absBits.data());
        if (testBit(evBits, EV_FF))
            ::ioctl(m_fd, EVIOCGBIT(EV_FF, byteSize(ffBits)), ffBits.data());
        ::ioctl(m_fd, EVIOCGPROP(byteSize(propBits)), propBits.data());

        const auto hasKey = [&](int code) { return testBit(keyBits, code); };
        const auto hasAbs = [&](int code) { return testBit(absBits, code); };
        const auto hasProp = [&](int code) { return testBit(propBits, code); };
        if (!evdevIsGamepad(hasKey, hasAbs, hasProp))
            return false;

        m_layout = evdevLayout(hasKey, hasAbs);
        m_buttonOf.assign(KEY_CNT, -1);
        for (std::size_t i = 0; i < m_layout.buttonCodes.size(); ++i)
            m_buttonOf[static_cast<std::size_t>(m_layout.buttonCodes[i])] = static_cast<int>(i);
        m_axisOf.assign(ABS_CNT, -1);
        m_hatOf.assign(ABS_CNT, -1);
        m_absInfo.assign(ABS_CNT, input_absinfo{});
        for (std::size_t i = 0; i < m_layout.axisCodes.size(); ++i) {
            const int code = m_layout.axisCodes[i];
            m_axisOf[static_cast<std::size_t>(code)] = static_cast<int>(i);
            ::ioctl(m_fd, absRequest(code), &m_absInfo[static_cast<std::size_t>(code)]);
        }
        for (std::size_t i = 0; i < m_layout.hatPairs.size(); ++i) {
            const auto x = static_cast<std::size_t>(ABS_HAT0X + 2 * m_layout.hatPairs[i]);
            m_hatOf[x] = static_cast<int>(i);
            m_hatOf[x + 1] = static_cast<int>(i);
        }
        m_hatX.assign(m_layout.hatPairs.size(), 0);
        m_hatY.assign(m_layout.hatPairs.size(), 0);

        input_id id{};
        ::ioctl(m_fd, EVIOCGID, &id);
        m_info.name = ioctlString(m_fd, EVIOCGNAME(256));
        m_info.guid = gamepadGuid(id.bustype, id.vendor, id.product, id.version, m_info.name);
        m_info.identity = ioctlString(m_fd, EVIOCGUNIQ(256));
        if (m_info.identity.isEmpty())
            m_info.identity = ioctlString(m_fd, EVIOCGPHYS(256));
        if (m_info.identity.isEmpty())
            m_info.identity = path;
        m_info.buttons = static_cast<int>(m_layout.buttonCodes.size());
        m_info.axes = static_cast<int>(m_layout.axisCodes.size());
        m_info.hats = static_cast<int>(m_layout.hatPairs.size());
        m_info.rumble = m_writable && testBit(ffBits, FF_RUMBLE);
        m_info.defaultMapping = evdevDefaultMapping(m_layout, id.vendor, m_info.standardLayout);
        return true;
    }

    GamepadDeviceInfo info() const override { return m_info; }
    int eventDescriptor() const override { return m_fd; }

    bool readInputs(std::vector<GamepadInput> &inputs) override {
        input_event events[64];
        for (;;) {
            const ssize_t n = ::read(m_fd, events, sizeof events);
            if (n < 0) {
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    return true;
                return false; // ENODEV once the pad is unplugged
            }
            if (n == 0)
                return false;
            const auto count = static_cast<std::size_t>(n) / sizeof(input_event);
            for (std::size_t i = 0; i < count; ++i)
                take(events[i], inputs);
        }
    }

    void readState(std::vector<GamepadInput> &inputs) override {
        auto keys = bitArray(KEY_MAX);
        if (::ioctl(m_fd, EVIOCGKEY(byteSize(keys)), keys.data()) >= 0) {
            for (std::size_t i = 0; i < m_layout.buttonCodes.size(); ++i)
                inputs.push_back(
                    {GamepadInput::Button, static_cast<int>(i), testBit(keys, m_layout.buttonCodes[i]) ? 1.0 : 0.0});
        }
        for (std::size_t i = 0; i < m_layout.axisCodes.size(); ++i) {
            const int code = m_layout.axisCodes[i];
            input_absinfo &info = m_absInfo[static_cast<std::size_t>(code)];
            if (::ioctl(m_fd, absRequest(code), &info) >= 0)
                inputs.push_back({GamepadInput::Axis, static_cast<int>(i), normalize(info.value, info)});
        }
        for (std::size_t i = 0; i < m_layout.hatPairs.size(); ++i) {
            const int x = ABS_HAT0X + 2 * m_layout.hatPairs[i];
            input_absinfo info{};
            if (::ioctl(m_fd, absRequest(x), &info) >= 0)
                m_hatX[i] = info.value;
            if (::ioctl(m_fd, absRequest(x + 1), &info) >= 0)
                m_hatY[i] = info.value;
            inputs.push_back({GamepadInput::Hat, static_cast<int>(i), static_cast<double>(hatMask(i))});
        }
    }

    bool rumble(double strong, double weak, int msecs) override {
        if (!m_info.rumble)
            return false;
        if (msecs <= 0 || (strong <= 0.0 && weak <= 0.0)) {
            if (m_effect < 0)
                return true;
            return play(0);
        }
        ff_effect effect{};
        effect.type = FF_RUMBLE;
        effect.id = static_cast<qint16>(m_effect);
        effect.u.rumble.strong_magnitude = static_cast<quint16>(std::clamp(strong, 0.0, 1.0) * 0xffff);
        effect.u.rumble.weak_magnitude = static_cast<quint16>(std::clamp(weak, 0.0, 1.0) * 0xffff);
        effect.replay.length = static_cast<quint16>(std::min(msecs, 0xffff));
        if (::ioctl(m_fd, EVIOCSFF, &effect) < 0)
            return false;
        m_effect = effect.id;
        return play(1);
    }

private:
    bool play(int value) {
        input_event event{};
        event.type = EV_FF;
        event.code = static_cast<quint16>(m_effect);
        event.value = value;
        return ::write(m_fd, &event, sizeof event) == static_cast<ssize_t>(sizeof event);
    }

    int hatMask(std::size_t hat) const {
        int mask = 0;
        if (m_hatY[hat] < 0)
            mask |= HatUp;
        if (m_hatX[hat] > 0)
            mask |= HatRight;
        if (m_hatY[hat] > 0)
            mask |= HatDown;
        if (m_hatX[hat] < 0)
            mask |= HatLeft;
        return mask;
    }

    void take(const input_event &event, std::vector<GamepadInput> &inputs) {
        if (event.type == EV_SYN) {
            if (event.code == SYN_DROPPED) {
                m_dropped = true;
            } else if (event.code == SYN_REPORT && m_dropped) {
                // Events were lost: start again from the state of the device.
                m_dropped = false;
                readState(inputs);
            }
            return;
        }
        if (m_dropped)
            return;
        if (event.type == EV_KEY && event.code < KEY_CNT) {
            const int i = m_buttonOf[event.code];
            if (i >= 0)
                inputs.push_back({GamepadInput::Button, i, event.value != 0 ? 1.0 : 0.0});
        } else if (event.type == EV_ABS && event.code < ABS_CNT) {
            const int hat = m_hatOf[event.code];
            if (hat >= 0) {
                const auto h = static_cast<std::size_t>(hat);
                ((event.code - ABS_HAT0X) % 2 == 0 ? m_hatX : m_hatY)[h] = event.value;
                inputs.push_back({GamepadInput::Hat, hat, static_cast<double>(hatMask(h))});
                return;
            }
            const int i = m_axisOf[event.code];
            if (i >= 0)
                inputs.push_back({GamepadInput::Axis, i, normalize(event.value, m_absInfo[event.code])});
        }
    }

    int m_fd;
    bool m_writable;
    bool m_dropped = false;
    int m_effect = -1;
    GamepadDeviceInfo m_info;
    EvdevLayout m_layout;
    std::vector<int> m_buttonOf;
    std::vector<int> m_axisOf;
    std::vector<int> m_hatOf;
    std::vector<input_absinfo> m_absInfo;
    std::vector<int> m_hatX;
    std::vector<int> m_hatY;
};

int openNode(const QString &path, int flags) {
    const QByteArray name = QFile::encodeName(path);
    int fd = -1;
    do {
        fd = ::open(name.constData(), flags | O_NONBLOCK | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    return fd;
}

class EvdevPlatform final : public GamepadPlatform {
public:
    QString deviceDirectory() const override { return QStringLiteral("/dev/input"); }

    QStringList devicePaths() override {
        QStringList nodes = QDir(deviceDirectory()).entryList({QStringLiteral("event*")}, QDir::System);
        std::sort(nodes.begin(), nodes.end(), [](const QString &a, const QString &b) {
            return a.mid(5).toInt() < b.mid(5).toInt(); // event2 before event10
        });
        QStringList paths;
        for (const QString &node : nodes)
            paths << deviceDirectory() + QLatin1Char('/') + node;
        return paths;
    }

    std::unique_ptr<GamepadDevice> openDevice(const QString &path, bool &retry) override {
        retry = false;
        bool writable = true; // rumble needs to write to the node
        int fd = openNode(path, O_RDWR);
        if (fd < 0 && (errno == EACCES || errno == EPERM)) {
            writable = false;
            fd = openNode(path, O_RDONLY);
        }
        if (fd < 0) {
            // A new node belongs to root until udev has given it to the user.
            retry = errno == EACCES || errno == EPERM;
            return nullptr;
        }
        auto device = std::make_unique<EvdevDevice>(fd, writable);
        if (!device->init(path))
            return nullptr;
        return device;
    }
};

} // namespace

std::unique_ptr<GamepadPlatform> createPlatformGamepadPlatform() { return std::make_unique<EvdevPlatform>(); }

} // namespace QtPrivate
