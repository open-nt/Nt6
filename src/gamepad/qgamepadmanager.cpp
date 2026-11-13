// MIT License - see LICENSE
#include "qgamepadbackend_p.h"

#include <QtCore/qapplicationstatic.h>
#include <QtCore/qfile.h>
#include <QtCore/qfilesystemwatcher.h>
#include <QtCore/qhash.h>
#include <QtCore/qset.h>
#include <QtCore/qsettings.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qtenvironmentvariables.h>
#include <QtCore/qtimer.h>

#include <algorithm>
#include <cmath>

/*!
    \module NtGamepad
    \title Nt Gamepad C++ Classes
    \brief Classes for reading gamepads and joysticks.
    \cmaketarget Nt::Gamepad

    Nt Gamepad brings back the API of Qt Gamepad, which was part of Qt 5 and was not carried over
    to Qt 6: QGamepad for the state of one pad, QGamepadManager for all of them, and
    QGamepadKeyNavigation to drive a user interface with key events. It adds what Qt Gamepad was
    missing: SDL's GameControllerDB mappings, ids that stay with a pad when it is plugged in
    again, generic button and axis signals, dead zones and rumble.

    See \l{Using Nt Gamepad} for a walk through, and \c{examples/gamepad/gamepadinfo.cpp} for a
    complete program.
*/

/*!
    \page nt-gamepad.html
    \title Using Nt Gamepad
    \brief Reading a gamepad, hot plugging, mappings from SDL's GameControllerDB, rumble, menus driven by a gamepad, and permission on Linux.

    Link \c Nt::Gamepad (or \c Nt6::Gamepad from an installed package) and include
    \c{<QGamepad>}:

    \code
    find_package(Nt6 6 REQUIRED)
    add_executable(app main.cpp)
    target_link_libraries(app PRIVATE Nt6::Gamepad)
    \endcode

    The API is that of Qt Gamepad from Qt 5, so code written for it builds again; what Nt adds is
    listed under \l{Changes from Qt Gamepad}.

    \section1 Reading a Gamepad

    A QGamepad follows one pad, the first one plugged in unless given another id. Its properties
    hold the state of each button and stick, and each has a signal for when it changes:

    \snippet gamepad/gamepadinfo.cpp properties

    To handle every control in one place, as an input table of a game does, use the generic
    signals and buttonValue() and axisValue() instead:

    \snippet gamepad/gamepadinfo.cpp generic

    Sticks go from -1.0 to 1.0, with up and left negative. The triggers, buttonL2 and buttonR2,
    go from 0.0 to 1.0; the other buttons are \c true or \c false.

    The face buttons are named by where they are, as on an Xbox pad: A is the bottom one, B the
    right, X the left and Y the top. On a PlayStation pad A is the cross; on a Nintendo pad,
    which has its letters the other way round, A is the button labeled B.

    \section1 Plugging Pads In and Out

    QGamepadManager::connectedGamepads() lists the ids of the pads, and gamepadConnected() and
    gamepadDisconnected() report changes. The first pad gets id 0, the next 1, and so on. An id is
    not given to another pad: when a pad is unplugged and plugged in again it gets its old id
    back, so a QGamepad that followed it carries on by itself:

    \snippet gamepad/gamepadinfo.cpp hotplug

    While a pad is away, its QGamepad reports \c connected as \c false and every control at rest.

    \section1 Mappings

    The kernel tells which control is which for most pads made in the last decade, and those work
    as they are. For others, give a mapping in the format of SDL's
    \l{https://github.com/mdqinc/SDL_GameControllerDB}{GameControllerDB}, either as text or as the
    whole \c gamecontrollerdb.txt:

    \snippet gamepad/gamepadinfo.cpp mappings

    Mappings in \c SDL_GAMECONTROLLERCONFIG, and in the file named by
    \c SDL_GAMECONTROLLERCONFIG_FILE, are added when the manager starts, as SDL does.
    QGamepadManager::mapping() returns the mapping a pad uses, in the same format, so that it can
    be saved or given to SDL.

    \section2 Letting the User Configure a Pad

    isConfigurationNeeded() returns \c true for a pad that names neither its buttons nor has a
    mapping. configureButton() then binds a button to whatever the user presses next, and
    configureAxis() binds an axis to what the user moves. With a settings file, the result is
    saved and used each time the pad is plugged in:

    \code
    auto *manager = QGamepadManager::instance();
    manager->setSettingsFile(QStringLiteral("gamepads.ini"));
    manager->setCancelConfigureButton(id, QGamepadManager::ButtonStart);
    manager->configureButton(id, QGamepadManager::ButtonA);  // "press the jump button"
    QObject::connect(manager, &QGamepadManager::buttonConfigured, [](int, QGamepadManager::GamepadButton b) { next(b); });
    \endcode

    \section1 Dead Zones

    The kernel reports a flat zone around the middle of each stick, and that is taken out. Worn
    sticks may drift further; QGamepadManager::setDeadZone() sets a round dead zone on top, over
    both axes of a stick together.

    \section1 Rumble

    \snippet gamepad/gamepadinfo.cpp rumble

    rumble() runs the strong (low frequency) and weak (high frequency) motors for the given
    number of milliseconds. canRumble() is \c false for a pad without motors, and on Linux when
    the program may read the pad but not write to it.

    \section1 Menus Driven by a Gamepad

    QGamepadKeyNavigation turns buttons into key events for the window that has the focus, so
    that widgets and windows work with a gamepad as they do with a keyboard. By default the
    directional pad sends the arrow keys, A sends Return and B sends Back. With autoRepeat, a
    held direction repeats like a held key:

    \code
    QGamepadKeyNavigation navigation;
    navigation.setAutoRepeat(true);
    navigation.setButtonBKey(Qt::Key_Escape);
    \endcode

    \section1 Permission on Linux

    Pads are read from \c /dev/input/event*. On a desktop, udev gives the user at the seat access
    to joysticks and gamepads, and nothing needs doing. Elsewhere, add the user to the group
    \c input, or add a udev rule such as this as \c /etc/udev/rules.d/60-gamepad.rules:

    \code
    SUBSYSTEM=="input", ENV{ID_INPUT_JOYSTICK}=="1", GROUP="plugdev", MODE="0660"
    \endcode

    Rumble needs write access too.

    \section1 Changes from Qt Gamepad

    \list
    \li Ids stay with a pad when it is unplugged and plugged in again, and are not reused.
    \li Mappings are SDL GameControllerDB strings: addMappings(), addMappingsFromFile(),
        mapping(), and \c SDL_GAMECONTROLLERCONFIG. The settings file stores them in the same
        format.
    \li QGamepad::buttonValue(), QGamepad::axisValue(), QGamepad::buttonValueChanged() and
        QGamepad::axisValueChanged().
    \li Dead zones, rumble, and QGamepadManager::gamepadGuid().
    \li QGamepadKeyNavigation can repeat held keys.
    \li Pads are read as their state changes, not polled, and there are no plugins: Nt reads
        evdev on Linux. Other platforms are not supported yet.
    \li There are no QML types yet; register the classes with qmlRegisterType() to use them from
        Nt QML.
    \endlist
*/

namespace QtPrivate {

namespace {
GamepadPlatformFactory g_platformFactory = nullptr;
} // namespace

void setGamepadPlatformFactory(GamepadPlatformFactory factory) { g_platformFactory = factory; }

} // namespace QtPrivate

using QtPrivate::GamepadBinding;
using QtPrivate::GamepadInput;
using QtPrivate::GamepadMapping;
using QtPrivate::GamepadRawState;

namespace {

constexpr int maxRetries = 20;
constexpr int retryInterval = 250;

QString settingsGroup() { return QStringLiteral("mappings"); }

} // namespace

struct QGamepadManagerHolder {
    QGamepadManagerHolder() : manager(new QGamepadManager) {}
    ~QGamepadManagerHolder() { delete manager; }
    QGamepadManagerHolder(const QGamepadManagerHolder &) = delete;
    QGamepadManagerHolder &operator=(const QGamepadManagerHolder &) = delete;
    QGamepadManager *manager;
};

Q_APPLICATION_STATIC(QGamepadManagerHolder, g_managerHolder)

class QGamepadManagerPrivate {
public:
    enum MappingSource { DefaultMapping, DatabaseMapping, SavedMapping };
    enum Configuring { NotConfiguring, ConfiguringButton, ConfiguringAxis };

    struct Pad {
        int id = -1;
        QString path;
        std::unique_ptr<QtPrivate::GamepadDevice> device;
        QtPrivate::GamepadDeviceInfo info;
        QSocketNotifier *notifier = nullptr;
        GamepadMapping mapping;
        MappingSource source = DefaultMapping;
        GamepadRawState raw;
        std::array<double, QtPrivate::GamepadButtonCount> buttons{};
        std::array<double, QtPrivate::GamepadAxisCount> axes{};
        Configuring configuring = NotConfiguring;
        int configTarget = -1;
        GamepadRawState rest; // the state when configuration started
        QGamepadManager::GamepadButton cancelButton = QGamepadManager::ButtonInvalid;
    };

    explicit QGamepadManagerPrivate(QGamepadManager *owner) : q(owner) {}

    void start() {
        platform =
            QtPrivate::g_platformFactory ? QtPrivate::g_platformFactory() : QtPrivate::createPlatformGamepadPlatform();
        const QString config = qEnvironmentVariable("SDL_GAMECONTROLLERCONFIG");
        if (!config.isEmpty())
            addMappings(config);
        const QString configFile = qEnvironmentVariable("SDL_GAMECONTROLLERCONFIG_FILE");
        if (!configFile.isEmpty())
            q->addMappingsFromFile(configFile);

        rescanTimer.setSingleShot(true);
        QObject::connect(&rescanTimer, &QTimer::timeout, q, [this] { rescan(); });
        retryTimer.setSingleShot(true);
        QObject::connect(&retryTimer, &QTimer::timeout, q, [this] { rescan(); });
        const QString dir = platform->deviceDirectory();
        if (!dir.isEmpty()) {
            watcher = new QFileSystemWatcher(q);
            watcher->addPath(dir);
            QObject::connect(watcher, &QFileSystemWatcher::directoryChanged, q, [this] { rescanTimer.start(0); });
        }
        rescan();
        started = true;
    }

    Pad *find(int id) const {
        for (const auto &pad : pads) {
            if (pad->id == id)
                return pad.get();
        }
        return nullptr;
    }

    void rescan() {
        const QStringList paths = platform->devicePaths();
        const QSet<QString> present(paths.begin(), paths.end());
        for (auto it = rejected.begin(); it != rejected.end();) {
            if (present.contains(*it))
                ++it;
            else
                it = rejected.erase(it);
        }
        for (auto it = retries.begin(); it != retries.end();) {
            if (present.contains(it.key()))
                ++it;
            else
                it = retries.erase(it);
        }
        std::vector<int> gone;
        for (const auto &pad : pads) {
            if (!present.contains(pad->path))
                gone.push_back(pad->id);
        }
        for (const int id : gone)
            remove(id);

        bool retryLater = false;
        for (const QString &path : paths) {
            if (rejected.contains(path) ||
                std::any_of(pads.begin(), pads.end(), [&](const auto &p) { return p->path == path; }))
                continue;
            bool retry = false;
            std::unique_ptr<QtPrivate::GamepadDevice> device = platform->openDevice(path, retry);
            if (device) {
                retries.remove(path);
                add(path, std::move(device));
            } else if (retry && started && ++retries[path] <= maxRetries) {
                retryLater = true; // a node that has just appeared, waiting for udev
            } else {
                retries.remove(path);
                rejected.insert(path);
            }
        }
        if (retryLater)
            retryTimer.start(retryInterval);
    }

    void add(const QString &path, std::unique_ptr<QtPrivate::GamepadDevice> device) {
        auto pad = std::make_unique<Pad>();
        pad->path = path;
        pad->info = device->info();
        pad->device = std::move(device);
        const QString key = pad->info.guid + QLatin1Char('|') + pad->info.identity;
        int id = knownIds.value(key, -1);
        if (id < 0 || find(id)) {
            id = nextId++;
            knownIds.insert(key, id);
        }
        pad->id = id;
        resolveMapping(*pad);
        std::vector<GamepadInput> inputs;
        pad->device->readState(inputs);
        for (const GamepadInput &input : inputs)
            pad->raw.apply(input);
        compute(*pad, pad->buttons, pad->axes);
        pad->notifier = new QSocketNotifier(pad->device->eventDescriptor(), QSocketNotifier::Read, q);
        QObject::connect(pad->notifier, &QSocketNotifier::activated, q, [this, id] { readable(id); });
        const auto at =
            std::find_if(pads.begin(), pads.end(), [&](const std::unique_ptr<Pad> &p) { return p->id > id; });
        pads.insert(at, std::move(pad));
        emit q->gamepadConnected(id);
        emit q->connectedGamepadsChanged();
    }

    void remove(int id) {
        const auto it =
            std::find_if(pads.begin(), pads.end(), [&](const std::unique_ptr<Pad> &p) { return p->id == id; });
        if (it == pads.end())
            return;
        // The notifier may be the one whose signal is being handled.
        (*it)->notifier->setEnabled(false);
        (*it)->notifier->deleteLater();
        pads.erase(it);
        emit q->gamepadDisconnected(id);
        emit q->connectedGamepadsChanged();
    }

    void readable(int id) {
        Pad *pad = find(id);
        if (!pad)
            return;
        std::vector<GamepadInput> inputs;
        const bool alive = pad->device->readInputs(inputs);
        // One input at a time, so that a press and release read together is still a press.
        for (const GamepadInput &input : inputs) {
            pad = find(id); // a slot may have run an event loop that removed the pad
            if (!pad)
                return;
            pad->raw.apply(input);
            if (pad->configuring != NotConfiguring)
                configure(*pad, {input});
            update(id);
        }
        if (!alive)
            remove(id);
    }

    void resolveMapping(Pad &pad) {
        if (!settingsFile.isEmpty()) {
            QSettings settings(settingsFile, QSettings::IniFormat);
            settings.beginGroup(settingsGroup());
            const QString saved = settings.value(pad.info.guid).toString();
            GamepadMapping mapping;
            if (!saved.isEmpty() && mapping.parse(saved)) {
                pad.mapping = mapping;
                pad.mapping.guid = pad.info.guid;
                pad.source = SavedMapping;
                return;
            }
        }
        const GamepadMapping *best = nullptr;
        QtPrivate::GuidMatch bestMatch = QtPrivate::GuidMatch::None;
        for (const GamepadMapping &mapping : database) {
            const QtPrivate::GuidMatch match = QtPrivate::matchGamepadGuid(pad.info.guid, mapping.guid);
            if (match != QtPrivate::GuidMatch::None && match >= bestMatch) { // later mappings win
                best = &mapping;
                bestMatch = match;
            }
        }
        if (best) {
            pad.mapping = *best;
            pad.mapping.guid = pad.info.guid;
            pad.source = DatabaseMapping;
            return;
        }
        pad.mapping = GamepadMapping();
        pad.mapping.guid = pad.info.guid;
        pad.mapping.name = pad.info.name;
        pad.mapping.parseBindings(pad.info.defaultMapping);
        pad.source = DefaultMapping;
    }

    void compute(const Pad &pad, std::array<double, QtPrivate::GamepadButtonCount> &buttons,
                 std::array<double, QtPrivate::GamepadAxisCount> &axes) const {
        for (int b = 0; b < QtPrivate::GamepadButtonCount; ++b)
            buttons[static_cast<std::size_t>(b)] =
                pad.mapping.buttonValue(static_cast<QGamepadManager::GamepadButton>(b), pad.raw);
        for (int a = 0; a < QtPrivate::GamepadAxisCount; ++a)
            axes[static_cast<std::size_t>(a)] =
                pad.mapping.axisValue(static_cast<QGamepadManager::GamepadAxis>(a), pad.raw);
        if (deadZone > 0.0) {
            for (std::size_t x = 0; x < axes.size(); x += 2) {
                const double length = std::hypot(axes[x], axes[x + 1]);
                const double scale = length <= deadZone ? 0.0 : (length - deadZone) / (1.0 - deadZone) / length;
                axes[x] = std::clamp(axes[x] * scale, -1.0, 1.0);
                axes[x + 1] = std::clamp(axes[x + 1] * scale, -1.0, 1.0);
            }
        }
    }

    // Works out the buttons and axes of the pad again and reports the ones that changed.
    void update(int id) {
        Pad *pad = find(id);
        if (!pad)
            return;
        std::array<double, QtPrivate::GamepadButtonCount> buttons{};
        std::array<double, QtPrivate::GamepadAxisCount> axes{};
        compute(*pad, buttons, axes);
        const auto oldButtons = pad->buttons;
        const auto oldAxes = pad->axes;
        pad->buttons = buttons;
        pad->axes = axes;
        // The pad may go away in a slot, so only the copies are used from here on.
        for (std::size_t a = 0; a < axes.size(); ++a) {
            if (axes[a] != oldAxes[a])
                emit q->gamepadAxisEvent(id, static_cast<QGamepadManager::GamepadAxis>(a), axes[a]);
        }
        for (std::size_t b = 0; b < buttons.size(); ++b) {
            if (buttons[b] == oldButtons[b])
                continue;
            const auto button = static_cast<QGamepadManager::GamepadButton>(b);
            if (buttons[b] > 0.0)
                emit q->gamepadButtonPressEvent(id, button, buttons[b]);
            else
                emit q->gamepadButtonReleaseEvent(id, button);
        }
    }

    void updateAll() {
        std::vector<int> ids;
        ids.reserve(pads.size());
        for (const auto &pad : pads)
            ids.push_back(pad->id);
        for (const int id : ids)
            update(id);
    }

    void configure(Pad &pad, const std::vector<GamepadInput> &inputs) {
        const int id = pad.id;
        if (pad.cancelButton != QGamepadManager::ButtonInvalid &&
            pad.mapping.buttonValue(pad.cancelButton, pad.raw) > 0.5 &&
            pad.buttons[static_cast<std::size_t>(pad.cancelButton)] <= 0.5) {
            pad.configuring = NotConfiguring;
            emit q->configurationCanceled(id);
            return;
        }
        for (const GamepadInput &input : inputs) {
            GamepadBinding binding;
            binding.index = input.index;
            if (input.kind == GamepadInput::Axis) {
                const auto i = static_cast<std::size_t>(input.index);
                const double rest = i < pad.rest.axes.size() ? pad.rest.axes[i] : 0.0;
                if (std::abs(input.value - rest) <= 0.5)
                    continue;
                binding.source = GamepadBinding::Axis;
                if (pad.configuring == ConfiguringButton && rest > -0.5) // a stick, not a trigger at rest
                    binding.inputRange = input.value > rest ? GamepadBinding::Positive : GamepadBinding::Negative;
            } else if (pad.configuring == ConfiguringAxis) {
                continue;
            } else if (input.kind == GamepadInput::Button) {
                if (input.value <= 0.0)
                    continue;
                binding.source = GamepadBinding::Button;
            } else {
                const int mask = static_cast<int>(input.value);
                if (mask == 0)
                    continue;
                binding.source = GamepadBinding::Hat;
                binding.hatMask = mask & -mask; // one direction
            }
            const Configuring what = pad.configuring;
            const int target = pad.configTarget;
            pad.configuring = NotConfiguring;
            if (what == ConfiguringButton)
                pad.mapping.buttons[static_cast<std::size_t>(target)] = {binding};
            else
                pad.mapping.axes[static_cast<std::size_t>(target)] = {binding};
            pad.source = SavedMapping;
            save(pad);
            update(id);
            if (what == ConfiguringButton)
                emit q->buttonConfigured(id, static_cast<QGamepadManager::GamepadButton>(target));
            else
                emit q->axisConfigured(id, static_cast<QGamepadManager::GamepadAxis>(target));
            return;
        }
    }

    void save(const Pad &pad) const {
        if (settingsFile.isEmpty())
            return;
        QSettings settings(settingsFile, QSettings::IniFormat);
        settings.beginGroup(settingsGroup());
        settings.setValue(pad.info.guid, pad.mapping.toString());
        settings.sync();
    }

    int addMappings(const QString &text) {
        int added = 0;
        for (const QString &line : text.split(QLatin1Char('\n'))) {
            GamepadMapping mapping;
            if (!mapping.parse(line))
                continue;
            database.erase(std::remove_if(database.begin(), database.end(),
                                          [&](const GamepadMapping &m) { return m.guid == mapping.guid; }),
                           database.end());
            database.push_back(mapping);
            ++added;
        }
        if (added > 0) {
            for (const auto &pad : pads) {
                if (pad->source != SavedMapping)
                    resolveMapping(*pad);
            }
            updateAll();
        }
        return added;
    }

    QGamepadManager *q;
    std::unique_ptr<QtPrivate::GamepadPlatform> platform;
    std::vector<std::unique_ptr<Pad>> pads; // in order of id
    QHash<QString, int> knownIds;           // the id of each pad seen, by GUID and identity
    int nextId = 0;
    std::vector<GamepadMapping> database;
    QString settingsFile;
    double deadZone = 0.0;
    QFileSystemWatcher *watcher = nullptr;
    QTimer rescanTimer;
    QTimer retryTimer;
    QSet<QString> rejected; // nodes that are not gamepads
    QHash<QString, int> retries;
    bool started = false;
};

/*!
    \class QGamepadManager
    \inmodule NtGamepad
    \brief The QGamepadManager class lists the connected gamepads and reports what they do.
    \since 6.0

    There is one QGamepadManager, returned by instance(). It finds the gamepads and joysticks of
    the machine, gives each an id, follows them as they are plugged in and out, and reports
    their buttons and axes through signals. Most programs use QGamepad, which follows one pad,
    and use the manager to list pads and to set up mappings:

    \code
    auto *manager = QGamepadManager::instance();
    for (int id : manager->connectedGamepads())
        qDebug() << id << manager->gamepadName(id);
    \endcode

    Each pad's controls are turned into the buttons and axes of the enumerations by a mapping in
    the format of SDL's GameControllerDB; see \l{Using Nt Gamepad}.

    The manager must be used from the thread of the application object.

    \differences
    Nt has no backend plugins: it reads evdev on Linux, and finds no pads elsewhere yet. Ids are
    not reused, and a pad that is plugged in again gets its old id. Mappings are SDL strings, and
    so is what the settings file holds. Added: gamepadGuid(), mapping(), addMappings(),
    addMappingsFromFile(), buttonValue(), axisValue(), deadZone, canRumble() and rumble().
    \enddifferences

    \sa QGamepad, QGamepadKeyNavigation
*/

/*!
    \enum QGamepadManager::GamepadButton

    The buttons of a gamepad. The face buttons are named by position, as on an Xbox pad.

    \value ButtonInvalid No button.
    \value ButtonA The bottom face button.
    \value ButtonB The right face button.
    \value ButtonX The left face button.
    \value ButtonY The top face button.
    \value ButtonL1 The left shoulder button.
    \value ButtonR1 The right shoulder button.
    \value ButtonL2 The left trigger, from 0.0 to 1.0.
    \value ButtonR2 The right trigger, from 0.0 to 1.0.
    \value ButtonSelect The Select button, also called Back, View or Share.
    \value ButtonStart The Start button, also called Menu or Options.
    \value ButtonL3 Pressing the left stick.
    \value ButtonR3 Pressing the right stick.
    \value ButtonUp Up on the directional pad.
    \value ButtonDown Down on the directional pad.
    \value ButtonRight Right on the directional pad.
    \value ButtonLeft Left on the directional pad.
    \value ButtonCenter A button in the middle that is neither Guide, Select nor Start, such as the
           Share button of an Xbox Series pad (\c misc1 in SDL mappings).
    \value ButtonGuide The Guide button, also called Home or PS.
*/

/*!
    \enum QGamepadManager::GamepadAxis

    The axes of the two sticks, from -1.0 to 1.0, with up and left negative.

    \value AxisInvalid No axis.
    \value AxisLeftX The left stick, left to right.
    \value AxisLeftY The left stick, up to down.
    \value AxisRightX The right stick, left to right.
    \value AxisRightY The right stick, up to down.
*/

QGamepadManager::QGamepadManager() : d(std::make_unique<QGamepadManagerPrivate>(this)) { d->start(); }

QGamepadManager::~QGamepadManager() = default;

/*!
    Returns the gamepad manager. It is created the first time it is asked for, and destroyed with
    the application object.
*/
QGamepadManager *QGamepadManager::instance() { return g_managerHolder->manager; }

/*!
    Returns \c true if the gamepad \a deviceId is connected.
*/
bool QGamepadManager::isGamepadConnected(int deviceId) const { return d->find(deviceId) != nullptr; }

/*!
    Returns the name of the gamepad \a deviceId, or an empty string if it is not connected.
*/
QString QGamepadManager::gamepadName(int deviceId) const {
    const auto *pad = d->find(deviceId);
    return pad ? pad->info.name : QString();
}

/*!
    Returns the ids of the connected gamepads, in increasing order.
*/
QList<int> QGamepadManager::connectedGamepads() const {
    QList<int> ids;
    for (const auto &pad : d->pads)
        ids.append(pad->id);
    return ids;
}

/*!
    \since 6.0

    Returns the GUID of the gamepad \a deviceId as SDL forms it, 32 hexadecimal digits, or an empty
    string if it is not connected. Mappings name the kind of pad they are for by this GUID.

    \sa mapping()
*/
QString QGamepadManager::gamepadGuid(int deviceId) const {
    const auto *pad = d->find(deviceId);
    return pad ? pad->info.guid : QString();
}

/*!
    \since 6.0

    Returns the mapping the gamepad \a deviceId uses, as an SDL GameControllerDB line, or an empty
    string if it is not connected. The line can be given to addMappings() or to SDL.

    \sa addMappings()
*/
QString QGamepadManager::mapping(int deviceId) const {
    const auto *pad = d->find(deviceId);
    return pad ? pad->mapping.toString() : QString();
}

/*!
    \since 6.0

    Adds the SDL GameControllerDB \a mappings, one per line, and returns how many were taken.
    Comments, blank lines and mappings for other platforms are skipped. A mapping replaces an
    earlier one for the same GUID, and applies at once to the connected pads it is for, unless
    they were configured with configureButton() or configureAxis().

    \sa addMappingsFromFile(), mapping()
*/
int QGamepadManager::addMappings(const QString &mappings) { return d->addMappings(mappings); }

/*!
    \since 6.0

    Adds the mappings in the file \a fileName, such as SDL's \c gamecontrollerdb.txt, and returns
    how many were taken, or -1 if the file cannot be read.

    \sa addMappings()
*/
int QGamepadManager::addMappingsFromFile(const QString &fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return -1;
    return d->addMappings(QString::fromUtf8(file.readAll()));
}

/*!
    \since 6.0

    Returns the value of \a button on the gamepad \a deviceId: 0.0 or 1.0, or from 0.0 to 1.0 for
    a trigger. Returns 0.0 if the pad is not connected.
*/
double QGamepadManager::buttonValue(int deviceId, QGamepadManager::GamepadButton button) const {
    const auto *pad = d->find(deviceId);
    if (!pad || button < 0 || button >= QtPrivate::GamepadButtonCount)
        return 0.0;
    return pad->buttons[static_cast<std::size_t>(button)];
}

/*!
    \since 6.0

    Returns the value of \a axis on the gamepad \a deviceId, from -1.0 to 1.0, or 0.0 if the pad is
    not connected.
*/
double QGamepadManager::axisValue(int deviceId, QGamepadManager::GamepadAxis axis) const {
    const auto *pad = d->find(deviceId);
    if (!pad || axis < 0 || axis >= QtPrivate::GamepadAxisCount)
        return 0.0;
    return pad->axes[static_cast<std::size_t>(axis)];
}

/*!
    Sets the radius of the dead zone around the middle of each stick, from 0.0 to 0.9 to \a deadZone.
*/
void QGamepadManager::setDeadZone(double deadZone) {
    const double zone = std::isnan(deadZone) ? 0.0 : std::clamp(deadZone, 0.0, 0.9);
    if (zone == d->deadZone)
        return;
    d->deadZone = zone;
    d->updateAll();
}

/*!
    Returns the radius of the dead zone around the middle of each stick, from 0.0 to 0.9.

    A stick inside the dead zone reads 0.0 on both axes; outside it, its position is scaled so
    that it still reaches 1.0 at the edge. The default is 0.0, which leaves only the flat zone the
    platform reports.
*/
double QGamepadManager::deadZone() const { return d->deadZone; }

/*!
    \since 6.0

    Returns \c true if the gamepad \a deviceId has rumble motors that the program may drive.

    \sa rumble()
*/
bool QGamepadManager::canRumble(int deviceId) const {
    const auto *pad = d->find(deviceId);
    return pad && pad->info.rumble;
}

/*!
    \since 6.0

    Runs the rumble motors of the gamepad \a deviceId for \a msecs milliseconds, the strong (low
    frequency) one at \a strong and the weak (high frequency) one at \a weak, each from 0.0 to
    1.0. A new call replaces the running effect; zero for \a msecs, or for both strengths, stops
    it. Returns \c false if the pad cannot rumble.

    \sa canRumble()
*/
bool QGamepadManager::rumble(int deviceId, double strong, double weak, int msecs) {
    auto *pad = d->find(deviceId);
    if (!pad || !pad->info.rumble)
        return false;
    return pad->device->rumble(std::clamp(strong, 0.0, 1.0), std::clamp(weak, 0.0, 1.0), msecs);
}

/*!
    Returns \c true if the gamepad \a deviceId neither tells which of its controls is which nor
    has a mapping, so that the user should configure it with configureButton() and
    configureAxis().
*/
bool QGamepadManager::isConfigurationNeeded(int deviceId) const {
    const auto *pad = d->find(deviceId);
    return pad && pad->source == QGamepadManagerPrivate::DefaultMapping && !pad->info.standardLayout;
}

/*!
    Binds \a button of the gamepad \a deviceId to the next button, hat direction or axis the user
    presses or moves, and emits buttonConfigured() when done. Returns \c false if the pad is not
    connected or \a button is not valid.

    The binding replaces what the button had, and is saved to the settings file if there is one.

    \sa configureAxis(), setCancelConfigureButton(), setSettingsFile()
*/
bool QGamepadManager::configureButton(int deviceId, QGamepadManager::GamepadButton button) {
    auto *pad = d->find(deviceId);
    if (!pad || button < 0 || button >= QtPrivate::GamepadButtonCount)
        return false;
    pad->configuring = QGamepadManagerPrivate::ConfiguringButton;
    pad->configTarget = button;
    pad->rest = pad->raw;
    return true;
}

/*!
    Binds \a axis of the gamepad \a deviceId to the next axis the user moves more than halfway,
    and emits axisConfigured() when done. Returns \c false if the pad is not connected or \a axis
    is not valid.

    \sa configureButton()
*/
bool QGamepadManager::configureAxis(int deviceId, QGamepadManager::GamepadAxis axis) {
    auto *pad = d->find(deviceId);
    if (!pad || axis < 0 || axis >= QtPrivate::GamepadAxisCount)
        return false;
    pad->configuring = QGamepadManagerPrivate::ConfiguringAxis;
    pad->configTarget = axis;
    pad->rest = pad->raw;
    return true;
}

/*!
    Makes \a button cancel configureButton() and configureAxis() on the gamepad \a deviceId: when
    it is pressed while either waits, configurationCanceled() is emitted. Returns \c false if the
    pad is not connected.
*/
bool QGamepadManager::setCancelConfigureButton(int deviceId, QGamepadManager::GamepadButton button) {
    auto *pad = d->find(deviceId);
    if (!pad)
        return false;
    pad->cancelButton = button >= 0 && button < QtPrivate::GamepadButtonCount ? button : ButtonInvalid;
    return true;
}

/*!
    Drops the configuration of the gamepad \a deviceId, also from the settings file, so that it
    goes back to a mapping from addMappings() or to the one the platform gives it.
*/
void QGamepadManager::resetConfiguration(int deviceId) {
    auto *pad = d->find(deviceId);
    if (!pad)
        return;
    if (!d->settingsFile.isEmpty()) {
        QSettings settings(d->settingsFile, QSettings::IniFormat);
        settings.beginGroup(settingsGroup());
        settings.remove(pad->info.guid);
        settings.sync();
    }
    pad->configuring = QGamepadManagerPrivate::NotConfiguring;
    pad->source = QGamepadManagerPrivate::DefaultMapping;
    d->resolveMapping(*pad);
    d->update(deviceId);
}

/*!
    Sets the INI \a file in which configureButton() and configureAxis() save their results. The
    mappings saved there are used for the pads they are for, now and when they are plugged in.
*/
void QGamepadManager::setSettingsFile(const QString &file) {
    d->settingsFile = file;
    for (const auto &pad : d->pads)
        d->resolveMapping(*pad);
    d->updateAll();
}

#include "moc_qgamepadmanager.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
