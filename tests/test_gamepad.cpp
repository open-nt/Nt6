// MIT License - see LICENSE
//
// Nt Gamepad is tested against a fake platform: each fake pad has a pipe for its event
// descriptor, and plugging one in or out touches a file in a directory the manager watches, so
// that hot plugging, reading and the signals go through the real event loop and QFileSystemWatcher.
// The evdev backend is checked for which devices it takes, how it numbers their controls and the
// mappings it guesses, against GameControllerDB's entries for the same pads; reading a real pad
// needs /dev/uinput, which a test run does not have.
#include <QtCore>
#include <QtGamepad>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qwindow.h>

#include "qgamepadbackend_linux_p.h"
#include "qgamepadbackend_p.h"

#include <linux/input.h>

#include <cstdio>
#include <deque>
#include <fcntl.h>
#include <set>
#include <unistd.h>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static void spinEvents(int msecs) {
    QEventLoop loop;
    QTimer::singleShot(msecs, [&] { loop.quit(); });
    loop.exec();
}

template <typename F> static bool waitFor(F cond, int timeoutMs = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!cond() && timer.elapsed() < timeoutMs)
        spinEvents(5);
    return cond();
}

using QtPrivate::GamepadInput;

namespace {

const QString xboxGuid = QStringLiteral("030000005e0400008e02000014010000");
const QString xboxBindings =
    QStringLiteral("a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b8,leftshoulder:b4,"
                   "leftstick:b9,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b10,righttrigger:a5,"
                   "rightx:a3,righty:a4,start:b7,x:b2,y:b3");

struct FakePad {
    QString path;
    QtPrivate::GamepadDeviceInfo info;
    std::vector<GamepadInput> state; // what readState() reports
    std::deque<GamepadInput> pending;
    bool gone = false;
    int denials = 0; // openDevice() asks to retry this many times first
    int pipe[2] = {-1, -1};
    double strong = -1, weak = -1;
    int msecs = -1;

    FakePad() { CHECK(::pipe2(pipe, O_NONBLOCK | O_CLOEXEC) == 0); }
    ~FakePad() {
        for (const int fd : pipe)
            ::close(fd);
    }
    FakePad(const FakePad &) = delete;
    FakePad &operator=(const FakePad &) = delete;

    void wake() {
        const char c = 'e';
        CHECK(::write(pipe[1], &c, 1) == 1);
    }
    void send(std::initializer_list<GamepadInput> inputs) {
        pending.insert(pending.end(), inputs.begin(), inputs.end());
        wake();
    }
};

struct FakeWorld {
    QTemporaryDir dir;
    std::vector<std::shared_ptr<FakePad>> pads; // plugged in
    std::set<QString> opened;

    void touch() {
        static int n = 0;
        QFile f(dir.path() + QStringLiteral("/change") + QString::number(++n));
        CHECK(f.open(QIODevice::WriteOnly));
    }
    std::shared_ptr<FakePad> find(const QString &path) {
        for (const auto &pad : pads) {
            if (pad->path == path)
                return pad;
        }
        return nullptr;
    }
};

FakeWorld *g_world = nullptr;

std::shared_ptr<FakePad> makePad(const QString &path, const QString &identity, bool standard = true) {
    auto pad = std::make_shared<FakePad>();
    pad->path = path;
    pad->info.name = QStringLiteral("Fake pad ") + identity;
    pad->info.guid = xboxGuid;
    pad->info.identity = identity;
    pad->info.buttons = 11;
    pad->info.axes = 6;
    pad->info.hats = 1;
    pad->info.rumble = true;
    pad->info.standardLayout = standard;
    pad->info.defaultMapping = xboxBindings;
    // Triggers rest at -1 on a whole axis.
    pad->state = {{GamepadInput::Axis, 2, -1.0}, {GamepadInput::Axis, 5, -1.0}};
    return pad;
}

void plug(const std::shared_ptr<FakePad> &pad) {
    g_world->pads.push_back(pad);
    g_world->touch();
}

void unplug(const std::shared_ptr<FakePad> &pad) {
    pad->gone = true;
    pad->wake();
    auto &pads = g_world->pads;
    pads.erase(std::remove(pads.begin(), pads.end(), pad), pads.end());
    g_world->touch();
}

class FakeDevice final : public QtPrivate::GamepadDevice {
public:
    explicit FakeDevice(std::shared_ptr<FakePad> pad) : m_pad(std::move(pad)) {}
    QtPrivate::GamepadDeviceInfo info() const override { return m_pad->info; }
    int eventDescriptor() const override { return m_pad->pipe[0]; }
    bool readInputs(std::vector<GamepadInput> &inputs) override {
        char buffer[64];
        while (::read(m_pad->pipe[0], buffer, sizeof buffer) > 0) {
        }
        inputs.assign(m_pad->pending.begin(), m_pad->pending.end());
        m_pad->pending.clear();
        return !m_pad->gone;
    }
    void readState(std::vector<GamepadInput> &inputs) override {
        inputs.insert(inputs.end(), m_pad->state.begin(), m_pad->state.end());
    }
    bool rumble(double strong, double weak, int msecs) override {
        m_pad->strong = strong;
        m_pad->weak = weak;
        m_pad->msecs = msecs;
        return true;
    }

private:
    std::shared_ptr<FakePad> m_pad;
};

class FakePlatform final : public QtPrivate::GamepadPlatform {
public:
    QString deviceDirectory() const override { return g_world->dir.path(); }
    QStringList devicePaths() override {
        QStringList paths{QStringLiteral("/fake/keyboard")};
        for (const auto &pad : g_world->pads)
            paths << pad->path;
        return paths;
    }
    std::unique_ptr<QtPrivate::GamepadDevice> openDevice(const QString &path, bool &retry) override {
        g_world->opened.insert(path);
        retry = false;
        auto pad = g_world->find(path);
        if (!pad)
            return nullptr;
        if (pad->denials > 0) {
            --pad->denials;
            retry = true;
            return nullptr;
        }
        pad->gone = false;
        return std::make_unique<FakeDevice>(pad);
    }
};

std::unique_ptr<QtPrivate::GamepadPlatform> makeFakePlatform() { return std::make_unique<FakePlatform>(); }

GamepadInput button(int index, bool pressed) { return {GamepadInput::Button, index, pressed ? 1.0 : 0.0}; }
GamepadInput axis(int index, double value) { return {GamepadInput::Axis, index, value}; }
GamepadInput hat(int index, int mask) { return {GamepadInput::Hat, index, static_cast<double>(mask)}; }

QString normalized(const QString &bindings) {
    QtPrivate::GamepadMapping m;
    m.guid = xboxGuid;
    m.parseBindings(bindings);
    return m.toString();
}

} // namespace

static void testMappingParse() {
    using QtPrivate::GamepadMapping;
    GamepadMapping m;
    if (!m.parse(xboxGuid + QStringLiteral(",Xbox 360 Controller,") + xboxBindings +
                 QStringLiteral(",platform:Linux,"))) {
        CHECK(!"the mapping parses");
        return;
    }
    CHECK(m.guid == xboxGuid);
    CHECK(m.name == QStringLiteral("Xbox 360 Controller"));
    CHECK(m.buttons[QGamepadManager::ButtonA].size() == 1);
    CHECK(m.buttons[QGamepadManager::ButtonA][0].source == QtPrivate::GamepadBinding::Button);
    CHECK(m.buttons[QGamepadManager::ButtonUp][0].source == QtPrivate::GamepadBinding::Hat);
    CHECK(m.buttons[QGamepadManager::ButtonUp][0].hatMask == 1);
    CHECK(m.buttons[QGamepadManager::ButtonL2][0].source == QtPrivate::GamepadBinding::Axis);
    CHECK(!m.hasBinding(QGamepadManager::ButtonCenter));

    // Written back and read again, it is the same.
    GamepadMapping again;
    CHECK(again.parse(m.toString()));
    CHECK(again.toString() == m.toString());
    CHECK(m.toString().startsWith(xboxGuid + QStringLiteral(",Xbox 360 Controller,a:b0,")));

    // Other platforms, comments and junk are skipped; unknown controls are ignored.
    GamepadMapping other;
    CHECK(!other.parse(xboxGuid + QStringLiteral(",Pad,a:b0,platform:Windows,")));
    CHECK(!other.parse(QStringLiteral("# a comment")));
    CHECK(!other.parse(QStringLiteral("xyz,Pad,a:b0")));
    CHECK(!other.parse(QString()));
    CHECK(other.parse(xboxGuid + QStringLiteral(",Pad,a:b0,paddle1:b20,touchpad:b21,b:q7,crc:1234,")));
    CHECK(other.hasBinding(QGamepadManager::ButtonA) && !other.hasBinding(QGamepadManager::ButtonB));

    // Half axes, inverted axes and half outputs.
    GamepadMapping half;
    half.parseBindings(QStringLiteral("lefttrigger:+a2,righty:a3~,-leftx:b0,+leftx:b1,x:-a4"));
    CHECK(half.buttons[QGamepadManager::ButtonL2][0].inputRange == QtPrivate::GamepadBinding::Positive);
    CHECK(half.axes[QGamepadManager::AxisRightY][0].invert);
    CHECK(half.axes[QGamepadManager::AxisLeftX].size() == 2);
    CHECK(half.toString().contains(QStringLiteral(",-leftx:b0,+leftx:b1")));
    CHECK(half.toString().contains(QStringLiteral("lefttrigger:+a2")));
    CHECK(half.toString().contains(QStringLiteral("righty:a3~")));

    // A later binding of the same control replaces the earlier ones.
    half.parseBindings(QStringLiteral("lefttrigger:b9"));
    CHECK(half.buttons[QGamepadManager::ButtonL2].size() == 1);
    CHECK(half.buttons[QGamepadManager::ButtonL2][0].source == QtPrivate::GamepadBinding::Button);
    CHECK(half.axes[QGamepadManager::AxisLeftX].size() == 2);
}

static void testMappingValues() {
    using QtPrivate::GamepadMapping;
    GamepadMapping m;
    m.parseBindings(QStringLiteral("lefttrigger:a2,righttrigger:+a5,a:+a6,b:-a6,-leftx:b0,+leftx:b1,lefty:a1~,"
                                   "dpup:h0.1,dpleft:h0.8,x:b7"));
    QtPrivate::GamepadRawState s;
    s.apply(axis(2, -1.0));
    CHECK(m.buttonValue(QGamepadManager::ButtonL2, s) == 0.0);
    s.apply(axis(2, 0.0));
    CHECK(m.buttonValue(QGamepadManager::ButtonL2, s) == 0.5);
    s.apply(axis(2, 1.0));
    CHECK(m.buttonValue(QGamepadManager::ButtonL2, s) == 1.0);
    s.apply(axis(5, -0.6));
    CHECK(m.buttonValue(QGamepadManager::ButtonR2, s) == 0.0);
    s.apply(axis(5, 0.25));
    CHECK(m.buttonValue(QGamepadManager::ButtonR2, s) == 0.25);

    // A digital button on half an axis is pressed past halfway.
    s.apply(axis(6, 0.4));
    CHECK(m.buttonValue(QGamepadManager::ButtonA, s) == 0.0);
    s.apply(axis(6, 0.8));
    CHECK(m.buttonValue(QGamepadManager::ButtonA, s) == 1.0);
    CHECK(m.buttonValue(QGamepadManager::ButtonB, s) == 0.0);
    s.apply(axis(6, -0.8));
    CHECK(m.buttonValue(QGamepadManager::ButtonB, s) == 1.0);

    // An axis made of two buttons.
    CHECK(m.axisValue(QGamepadManager::AxisLeftX, s) == 0.0);
    s.apply(button(0, true));
    CHECK(m.axisValue(QGamepadManager::AxisLeftX, s) == -1.0);
    s.apply(button(1, true));
    CHECK(m.axisValue(QGamepadManager::AxisLeftX, s) == 0.0);
    s.apply(button(0, false));
    CHECK(m.axisValue(QGamepadManager::AxisLeftX, s) == 1.0);

    s.apply(axis(1, 0.3));
    CHECK(m.axisValue(QGamepadManager::AxisLeftY, s) == -0.3);
    s.apply(hat(0, QtPrivate::HatUp | QtPrivate::HatLeft));
    CHECK(m.buttonValue(QGamepadManager::ButtonUp, s) == 1.0);
    CHECK(m.buttonValue(QGamepadManager::ButtonLeft, s) == 1.0);
    // Controls the device does not have read as at rest.
    CHECK(m.buttonValue(QGamepadManager::ButtonX, s) == 0.0);
    CHECK(m.buttonValue(QGamepadManager::ButtonInvalid, s) == 0.0);
    CHECK(m.axisValue(QGamepadManager::AxisInvalid, s) == 0.0);
}

static void testGuid() {
    CHECK(QtPrivate::gamepadGuid(BUS_USB, 0x045e, 0x028e, 0x0114, QStringLiteral("X")) == xboxGuid);
    // Without vendor and product, the name fills the rest.
    const QString named = QtPrivate::gamepadGuid(BUS_BLUETOOTH, 0, 0, 0, QStringLiteral("Pad"));
    CHECK(named == QStringLiteral("05000000506164000000000000000000"));

    using QtPrivate::GuidMatch;
    CHECK(QtPrivate::matchGamepadGuid(xboxGuid, xboxGuid.toUpper()) == GuidMatch::Exact);
    QString crc = xboxGuid;
    crc.replace(4, 4, QStringLiteral("abcd"));
    CHECK(QtPrivate::matchGamepadGuid(crc, xboxGuid) == GuidMatch::Close);
    QString version = crc;
    version.replace(24, 4, QStringLiteral("0000"));
    CHECK(QtPrivate::matchGamepadGuid(version, xboxGuid) == GuidMatch::Loose);
    QString product = xboxGuid;
    product.replace(8, 4, QStringLiteral("ffff"));
    CHECK(QtPrivate::matchGamepadGuid(product, xboxGuid) == GuidMatch::None);
    CHECK(QtPrivate::matchGamepadGuid(QStringLiteral("short"), xboxGuid) == GuidMatch::None);
}

static void testEvdev() {
    const auto has = [](std::set<int> codes) { return [codes](int code) { return codes.count(code) > 0; }; };
    const auto none = [](int) { return false; };

    // The codes of an Xbox 360 pad on xpad, and of a DualShock 4 on hid-sony.
    const std::set<int> xboxKeys{BTN_A,      BTN_B,     BTN_X,    BTN_Y,      BTN_TL,    BTN_TR,
                                 BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR};
    const std::set<int> sticks{ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ, ABS_HAT0X, ABS_HAT0Y};
    const std::set<int> sonyKeys{BTN_SOUTH, BTN_EAST,   BTN_NORTH, BTN_WEST, BTN_TL,     BTN_TR,    BTN_TL2,
                                 BTN_TR2,   BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR};

    CHECK(QtPrivate::evdevIsGamepad(has(xboxKeys), has(sticks), none));
    CHECK(!QtPrivate::evdevIsGamepad(has({KEY_A, KEY_B}), none, none));            // a keyboard
    CHECK(!QtPrivate::evdevIsGamepad(has({BTN_LEFT}), has({ABS_X, ABS_Y}), none)); // a touchscreen-ish mouse
    CHECK(!QtPrivate::evdevIsGamepad(has({BTN_TOUCH, BTN_SOUTH}), has({ABS_X, ABS_Y}), none)); // a touchpad
    CHECK(!QtPrivate::evdevIsGamepad(has(sonyKeys), has(sticks), has({INPUT_PROP_ACCELEROMETER})));
    CHECK(QtPrivate::evdevIsGamepad(has({BTN_TRIGGER}), has({ABS_X, ABS_Y}), none)); // a joystick

    const QtPrivate::EvdevLayout xbox = QtPrivate::evdevLayout(has(xboxKeys), has(sticks));
    CHECK(xbox.buttonCodes.size() == 11 && xbox.axisCodes.size() == 6 && xbox.hatPairs.size() == 1);
    CHECK(xbox.buttonCodes[0] == BTN_A && xbox.axisCodes[5] == ABS_RZ);
    bool standard = false;
    CHECK(normalized(QtPrivate::evdevDefaultMapping(xbox, 0x045e, standard)) == normalized(xboxBindings));
    CHECK(standard);

    const QtPrivate::EvdevLayout sony = QtPrivate::evdevLayout(has(sonyKeys), has(sticks));
    const QString ds4 = QStringLiteral(
        "a:b0,b:b1,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b11,"
        "lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b12,righttrigger:a5,rightx:a3,righty:a4,"
        "start:b9,x:b3,y:b2");
    CHECK(normalized(QtPrivate::evdevDefaultMapping(sony, 0x054c, standard)) == normalized(ds4));

    // A generic pad: numbered buttons, the right stick on Z and RZ, no hat.
    const QtPrivate::EvdevLayout generic = QtPrivate::evdevLayout(
        has({BTN_TRIGGER, BTN_THUMB, BTN_THUMB2, BTN_TOP, BTN_TOP2, BTN_PINKIE, BTN_BASE, BTN_BASE2}),
        has({ABS_X, ABS_Y, ABS_Z, ABS_RZ}));
    const QString genericMapping = QtPrivate::evdevDefaultMapping(generic, 0x0079, standard);
    CHECK(!standard);
    CHECK(normalized(genericMapping) ==
          normalized(QStringLiteral("a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,lefttrigger:b6,"
                                    "righttrigger:b7,leftx:a0,lefty:a1,rightx:a2,righty:a3")));

    // Buttons below BTN_JOYSTICK come after the others, as in SDL; hats are not axes.
    const QtPrivate::EvdevLayout misc =
        QtPrivate::evdevLayout(has({BTN_0, BTN_SOUTH, BTN_DPAD_UP}), has({ABS_HAT1X, ABS_X, ABS_MISC}));
    CHECK(misc.buttonCodes == std::vector<int>({BTN_SOUTH, BTN_DPAD_UP, BTN_0}));
    CHECK(misc.axisCodes == std::vector<int>({ABS_X, ABS_MISC}));
    CHECK(misc.hatPairs == std::vector<int>({1}));

    // The real platform skips what is not a gamepad.
    auto platform = QtPrivate::createPlatformGamepadPlatform();
    CHECK(platform->deviceDirectory() == QStringLiteral("/dev/input"));
    bool retry = true;
    CHECK(!platform->openDevice(QStringLiteral("/dev/null"), retry));
    CHECK(!retry);
    CHECK(!platform->openDevice(QStringLiteral("/nonexistent/event0"), retry));
    CHECK(!retry);
}

static void testManager(const std::shared_ptr<FakePad> &first) {
    QGamepadManager *manager = QGamepadManager::instance();
    CHECK(manager == QGamepadManager::instance());
    CHECK(manager->connectedGamepads() == QList<int>{0});
    CHECK(manager->isGamepadConnected(0) && !manager->isGamepadConnected(1));
    CHECK(manager->gamepadName(0) == QStringLiteral("Fake pad one"));
    CHECK(manager->gamepadName(5).isEmpty());
    CHECK(manager->gamepadGuid(0) == xboxGuid);
    CHECK(manager->mapping(0).startsWith(xboxGuid + QStringLiteral(",Fake pad one,a:b0,")));
    CHECK(!manager->isConfigurationNeeded(0));
    CHECK(g_world->opened.count(QStringLiteral("/fake/keyboard")) == 1);

    QGamepad gamepad; // pad 0
    CHECK(gamepad.deviceId() == 0 && gamepad.isConnected());
    CHECK(gamepad.name() == QStringLiteral("Fake pad one"));
    CHECK(gamepad.buttonL2() == 0.0 && !gamepad.buttonA()); // the trigger at rest
    CHECK(gamepad.canRumble());

    QList<bool> aChanges;
    QList<QPair<int, double>> generic;
    QList<QString> managerEvents;
    QObject::connect(&gamepad, &QGamepad::buttonAChanged, [&](bool v) { aChanges << v; });
    QObject::connect(&gamepad, &QGamepad::buttonValueChanged,
                     [&](QGamepadManager::GamepadButton b, double v) { generic << qMakePair(int(b), v); });
    QObject::connect(manager, &QGamepadManager::gamepadButtonPressEvent, &gamepad,
                     [&](int id, QGamepadManager::GamepadButton b, double v) {
                         managerEvents << QStringLiteral("press %1 %2 %3").arg(id).arg(int(b)).arg(v);
                     });
    QObject::connect(manager, &QGamepadManager::gamepadButtonReleaseEvent, &gamepad,
                     [&](int id, QGamepadManager::GamepadButton b) {
                         managerEvents << QStringLiteral("release %1 %2").arg(id).arg(int(b));
                     });

    first->send({button(0, true)});
    CHECK(waitFor([&] { return gamepad.buttonA(); }));
    CHECK(aChanges == QList<bool>{true});
    CHECK(generic == (QList<QPair<int, double>>{qMakePair(int(QGamepadManager::ButtonA), 1.0)}));
    CHECK(managerEvents == QStringList{QStringLiteral("press 0 0 1")});
    CHECK(manager->buttonValue(0, QGamepadManager::ButtonA) == 1.0);
    first->send({button(0, false)});
    CHECK(waitFor([&] { return !gamepad.buttonA(); }));
    CHECK(managerEvents.last() == QStringLiteral("release 0 0"));
    // A tap read in one go is reported as a press and a release.
    first->send({button(3, true), button(3, false)});
    CHECK(waitFor([&] { return managerEvents.size() == 4; }));
    CHECK(managerEvents.mid(2) == (QStringList{QStringLiteral("press 0 3 1"), QStringLiteral("release 0 3")}));
    CHECK(!gamepad.buttonY());

    QList<double> xs;
    QList<int> axesSeen;
    QObject::connect(&gamepad, &QGamepad::axisLeftXChanged, [&](double v) { xs << v; });
    QObject::connect(&gamepad, &QGamepad::axisValueChanged,
                     [&](QGamepadManager::GamepadAxis a, double) { axesSeen << int(a); });
    first->send({axis(0, 0.5), axis(4, -0.25)});
    CHECK(waitFor([&] { return gamepad.axisLeftX() == 0.5; }));
    CHECK(gamepad.axisRightY() == -0.25);
    CHECK(gamepad.axisValue(QGamepadManager::AxisRightY) == -0.25);
    CHECK(xs == QList<double>{0.5});
    CHECK(axesSeen == (QList<int>{QGamepadManager::AxisLeftX, QGamepadManager::AxisRightY}));

    QList<double> l2;
    QObject::connect(&gamepad, &QGamepad::buttonL2Changed, [&](double v) { l2 << v; });
    first->send({axis(2, 0.0)});
    CHECK(waitFor([&] { return gamepad.buttonL2() == 0.5; }));
    first->send({axis(2, 1.0)});
    CHECK(waitFor([&] { return gamepad.buttonL2() == 1.0; }));
    CHECK(l2 == (QList<double>{0.5, 1.0}));
    first->send({hat(0, QtPrivate::HatUp | QtPrivate::HatRight)});
    CHECK(waitFor([&] { return gamepad.buttonUp(); }));
    CHECK(gamepad.buttonRight() && !gamepad.buttonDown() && !gamepad.buttonLeft());
    first->send({hat(0, 0), axis(2, -1.0)});
    CHECK(waitFor([&] { return !gamepad.buttonUp() && gamepad.buttonL2() == 0.0; }));

    // Dead zone: round, over both axes of a stick, and scaled to still reach the edge.
    manager->setDeadZone(0.2);
    CHECK(manager->deadZone() == 0.2);
    CHECK(waitFor([&] { return gamepad.axisLeftX() == (0.5 - 0.2) / 0.8; }));
    first->send({axis(0, 0.1), axis(1, 0.1)});
    CHECK(waitFor([&] { return gamepad.axisLeftX() == 0.0 && gamepad.axisLeftY() == 0.0; }));
    first->send({axis(0, 1.0), axis(1, 0.0)});
    CHECK(waitFor([&] { return gamepad.axisLeftX() == 1.0; }));
    manager->setDeadZone(5.0);
    CHECK(manager->deadZone() == 0.9);
    manager->setDeadZone(0.0);
    first->send({axis(0, 0.0), axis(4, 0.0)});
    CHECK(waitFor([&] { return gamepad.axisLeftX() == 0.0 && gamepad.axisRightY() == 0.0; }));

    // Rumble goes to the device, with the strengths clamped.
    CHECK(gamepad.rumble(2.0, 0.25, 300));
    CHECK(first->strong == 1.0 && first->weak == 0.25 && first->msecs == 300);
    CHECK(!manager->rumble(7, 1.0, 1.0, 100));
    CHECK(!manager->canRumble(7));
}

static void testHotplug(const std::shared_ptr<FakePad> &first) {
    QGamepadManager *manager = QGamepadManager::instance();
    QGamepad gamepad(0);
    QGamepad second(1);
    CHECK(!second.isConnected() && second.name().isEmpty());
    QList<int> connected, disconnected;
    int listChanges = 0;
    QObject::connect(manager, &QGamepadManager::gamepadConnected, &gamepad, [&](int id) { connected << id; });
    QObject::connect(manager, &QGamepadManager::gamepadDisconnected, &gamepad, [&](int id) { disconnected << id; });
    QObject::connect(manager, &QGamepadManager::connectedGamepadsChanged, &gamepad, [&] { ++listChanges; });
    QList<bool> secondConnected;
    QObject::connect(&second, &QGamepad::connectedChanged, [&](bool c) { secondConnected << c; });

    auto two = makePad(QStringLiteral("/fake/event7"), QStringLiteral("two"));
    plug(two);
    CHECK(waitFor([&] { return second.isConnected(); }));
    CHECK(connected == QList<int>{1} && listChanges == 1);
    CHECK(manager->connectedGamepads() == (QList<int>{0, 1}));
    CHECK(second.name() == QStringLiteral("Fake pad two"));
    CHECK(secondConnected == QList<bool>{true});

    // Unplugged while a button is held: the QGamepad lets go of it.
    first->send({button(1, true)});
    CHECK(waitFor([&] { return gamepad.buttonB(); }));
    QList<bool> bChanges;
    QObject::connect(&gamepad, &QGamepad::buttonBChanged, [&](bool v) { bChanges << v; });
    unplug(first);
    CHECK(waitFor([&] { return !gamepad.isConnected(); }));
    CHECK(disconnected == QList<int>{0});
    CHECK(!gamepad.buttonB() && bChanges == QList<bool>{false});
    CHECK(gamepad.name().isEmpty());
    CHECK(manager->connectedGamepads() == QList<int>{1});

    // Plugged in again, on another node: the same pad gets its id back.
    first->path = QStringLiteral("/fake/event9");
    first->pending.clear();
    first->state = {{GamepadInput::Axis, 2, -1.0}, {GamepadInput::Axis, 5, -1.0}};
    plug(first);
    CHECK(waitFor([&] { return gamepad.isConnected(); }));
    CHECK(connected == (QList<int>{1, 0}));
    CHECK(manager->connectedGamepads() == (QList<int>{0, 1}));

    // Another pad of the same kind gets a new id, and a node that is not readable yet is retried.
    auto three = makePad(QStringLiteral("/fake/event11"), QStringLiteral("three"));
    three->denials = 2;
    plug(three);
    CHECK(waitFor([&] { return manager->isGamepadConnected(2); }));
    CHECK(three->denials == 0);

    // Following another id.
    QList<int> ids;
    QObject::connect(&second, &QGamepad::deviceIdChanged, [&](int id) { ids << id; });
    second.setDeviceId(2);
    CHECK(ids == QList<int>{2} && second.name() == QStringLiteral("Fake pad three"));
    second.setDeviceId(42);
    CHECK(!second.isConnected() && second.name().isEmpty());
    unplug(two);
    unplug(three);
    CHECK(waitFor([&] { return manager->connectedGamepads() == QList<int>{0}; }));
}

static void testMappings(const std::shared_ptr<FakePad> &first) {
    QGamepadManager *manager = QGamepadManager::instance();
    QGamepad gamepad(0);
    // A mapping for this kind of pad, with A and B the other way round, applies at once.
    const int added = manager->addMappings(QStringLiteral("# swapped\n") + xboxGuid +
                                           QStringLiteral(",Swapped,a:b1,b:b0,platform:Linux,\n"
                                                          "0300000000000000000000000000000,Bad\n") +
                                           xboxGuid.left(28) + QStringLiteral("0000,Other,a:b5,platform:Windows,"));
    CHECK(added == 1);
    CHECK(manager->mapping(0).startsWith(xboxGuid + QStringLiteral(",Swapped,a:b1,b:b0,")));
    first->send({button(0, true)});
    CHECK(waitFor([&] { return gamepad.buttonB(); }));
    CHECK(!gamepad.buttonA());
    first->send({button(0, false)});
    CHECK(waitFor([&] { return !gamepad.buttonB(); }));

    // From a file.
    QTemporaryDir dir;
    const QString db = dir.path() + QStringLiteral("/gamecontrollerdb.txt");
    QFile file(db);
    CHECK(file.open(QIODevice::WriteOnly));
    file.write(
        (xboxGuid + QStringLiteral(",Database,") + xboxBindings + QStringLiteral(",platform:Linux,\n")).toUtf8());
    file.close();
    CHECK(manager->addMappingsFromFile(db) == 1);
    CHECK(manager->addMappingsFromFile(dir.path() + QStringLiteral("/missing.txt")) == -1);
    CHECK(manager->mapping(0).startsWith(xboxGuid + QStringLiteral(",Database,a:b0,")));
}

static void testConfiguration(const std::shared_ptr<FakePad> &first) {
    QGamepadManager *manager = QGamepadManager::instance();
    QGamepad gamepad(0);
    QTemporaryDir dir;
    const QString ini = dir.path() + QStringLiteral("/gamepads.ini");
    manager->setSettingsFile(ini);

    QList<int> configured;
    QList<int> axesConfigured;
    int canceled = 0;
    auto c1 = QObject::connect(manager, &QGamepadManager::buttonConfigured,
                               [&](int, QGamepadManager::GamepadButton b) { configured << int(b); });
    auto c2 = QObject::connect(manager, &QGamepadManager::axisConfigured,
                               [&](int, QGamepadManager::GamepadAxis a) { axesConfigured << int(a); });
    auto c3 = QObject::connect(manager, &QGamepadManager::configurationCanceled, [&](int) { ++canceled; });

    CHECK(!manager->configureButton(9, QGamepadManager::ButtonX));
    CHECK(!manager->configureButton(0, QGamepadManager::ButtonInvalid));
    CHECK(!manager->configureAxis(0, QGamepadManager::AxisInvalid));

    // X becomes raw button 6.
    CHECK(manager->configureButton(0, QGamepadManager::ButtonX));
    first->send({axis(0, 0.3)}); // too small to count
    first->send({button(6, true)});
    CHECK(waitFor([&] { return configured == QList<int>{QGamepadManager::ButtonX}; }));
    CHECK(waitFor([&] { return gamepad.buttonX(); }));
    CHECK(manager->mapping(0).contains(QStringLiteral(",x:b6,")));
    first->send({button(6, false)});
    CHECK(waitFor([&] { return !gamepad.buttonX(); }));
    {
        QSettings settings(ini, QSettings::IniFormat);
        CHECK(settings.value(QStringLiteral("mappings/") + xboxGuid).toString().contains(QStringLiteral(",x:b6,")));
    }

    // A button from half of a stick, and from a hat direction.
    CHECK(manager->configureButton(0, QGamepadManager::ButtonY));
    first->send({axis(3, -0.9)});
    CHECK(waitFor([&] { return configured.size() == 2; }));
    CHECK(manager->mapping(0).contains(QStringLiteral(",y:-a3,")));
    CHECK(waitFor([&] { return gamepad.buttonY(); }));
    first->send({axis(3, 0.0)});
    CHECK(waitFor([&] { return !gamepad.buttonY(); }));
    CHECK(manager->configureButton(0, QGamepadManager::ButtonCenter));
    first->send({hat(0, QtPrivate::HatDown | QtPrivate::HatLeft)});
    CHECK(waitFor([&] { return configured.size() == 3; }));
    CHECK(manager->mapping(0).contains(QStringLiteral(",misc1:h0.4,")));
    first->send({hat(0, 0)});
    CHECK(waitFor([&] { return !gamepad.buttonCenter(); }));

    // An axis, from a trigger that rests at -1: buttons do not count.
    CHECK(manager->configureAxis(0, QGamepadManager::AxisRightX));
    first->send({button(2, true), button(2, false)});
    first->send({axis(5, 0.0)});
    CHECK(waitFor([&] { return axesConfigured == QList<int>{QGamepadManager::AxisRightX}; }));
    CHECK(manager->mapping(0).contains(QStringLiteral(",rightx:a5,")));
    first->send({axis(5, -1.0)});
    CHECK(waitFor([&] { return gamepad.axisRightX() == -1.0; }));

    // Canceled with Start.
    CHECK(manager->setCancelConfigureButton(0, QGamepadManager::ButtonStart));
    CHECK(manager->configureButton(0, QGamepadManager::ButtonA));
    first->send({button(7, true)});
    CHECK(waitFor([&] { return canceled == 1; }));
    first->send({button(7, false), button(4, true), button(4, false)});
    spinEvents(20);
    CHECK(configured.size() == 3);
    CHECK(!manager->setCancelConfigureButton(9, QGamepadManager::ButtonStart));

    // The saved mapping comes back when the pad is plugged in again, and after a reset it is gone.
    unplug(first);
    CHECK(waitFor([&] { return !gamepad.isConnected(); }));
    first->pending.clear();
    plug(first);
    CHECK(waitFor([&] { return gamepad.isConnected(); }));
    CHECK(manager->mapping(0).contains(QStringLiteral(",x:b6,")));
    manager->resetConfiguration(0);
    CHECK(!manager->mapping(0).contains(QStringLiteral(",x:b6,")));
    {
        QSettings settings(ini, QSettings::IniFormat);
        CHECK(!settings.contains(QStringLiteral("mappings/") + xboxGuid));
    }
    manager->setSettingsFile(QString());
    QObject::disconnect(c1);
    QObject::disconnect(c2);
    QObject::disconnect(c3);
}

static void testConfigurationNeeded() {
    QGamepadManager *manager = QGamepadManager::instance();
    auto odd = makePad(QStringLiteral("/fake/event20"), QStringLiteral("odd"), false);
    odd->info.guid = QStringLiteral("03000000790000000600000010010000");
    odd->info.defaultMapping = QStringLiteral("a:b0,b:b1");
    plug(odd);
    int id = -1;
    CHECK(waitFor([&] {
        for (int i : manager->connectedGamepads()) {
            if (manager->gamepadName(i) == QStringLiteral("Fake pad odd"))
                id = i;
        }
        return id >= 0;
    }));
    CHECK(manager->isConfigurationNeeded(id));
    manager->addMappings(odd->info.guid + QStringLiteral(",Odd,a:b2,b:b3,"));
    CHECK(!manager->isConfigurationNeeded(id));
    CHECK(!manager->isConfigurationNeeded(99));
    unplug(odd);
    CHECK(waitFor([&] { return !manager->isGamepadConnected(id); }));
}

namespace {

class KeyWindow : public QWindow {
public:
    QStringList log;

protected:
    void keyPressEvent(QKeyEvent *e) override {
        log << QStringLiteral("press %1%2%3")
                   .arg(e->key(), 0, 16)
                   .arg(e->isAutoRepeat() ? QStringLiteral(" repeat") : QString())
                   .arg(e->text().isEmpty() ? QString() : QStringLiteral(" text"));
    }
    void keyReleaseEvent(QKeyEvent *e) override { log << QStringLiteral("release %1").arg(e->key(), 0, 16); }
};

QString press(Qt::Key key, const char *extra = "") {
    return QStringLiteral("press %1").arg(int(key), 0, 16) + QLatin1String(extra);
}
QString release(Qt::Key key) { return QStringLiteral("release %1").arg(int(key), 0, 16); }

} // namespace

static void testKeyNavigation(const std::shared_ptr<FakePad> &first) {
    KeyWindow window;
    window.resize(100, 100);
    window.show();
    CHECK(QGuiApplication::focusWindow() == &window);

    QGamepadKeyNavigation navigation;
    CHECK(navigation.active() && navigation.gamepad() == nullptr);
    CHECK(navigation.upKey() == Qt::Key_Up && navigation.downKey() == Qt::Key_Down);
    CHECK(navigation.leftKey() == Qt::Key_Left && navigation.rightKey() == Qt::Key_Right);
    CHECK(navigation.buttonAKey() == Qt::Key_Return && navigation.buttonBKey() == Qt::Key_Back);
    CHECK(navigation.buttonXKey() == Qt::Key_unknown && navigation.buttonR3Key() == Qt::Key_unknown);
    CHECK(!navigation.autoRepeat() && navigation.autoRepeatDelay() == 400 && navigation.autoRepeatInterval() == 100);
    CHECK(navigation.keyForButton(QGamepadManager::ButtonInvalid) == Qt::Key_unknown);

    first->send({button(0, true)});
    CHECK(waitFor([&] { return window.log.size() == 1; }));
    first->send({button(0, false), hat(0, QtPrivate::HatDown)});
    CHECK(waitFor([&] { return window.log.size() == 3; }));
    first->send({hat(0, 0), button(2, true), button(2, false)}); // X sends nothing
    CHECK(waitFor([&] { return window.log.size() == 4; }));
    spinEvents(20);
    CHECK(window.log == (QStringList{press(Qt::Key_Return, " text"), release(Qt::Key_Return), press(Qt::Key_Down),
                                     release(Qt::Key_Down)}));
    window.log.clear();

    // Keys can be changed; the trigger presses once past halfway.
    QList<int> changes;
    QObject::connect(&navigation, &QGamepadKeyNavigation::buttonL2KeyChanged, [&](Qt::Key k) { changes << int(k); });
    navigation.setButtonL2Key(Qt::Key_PageDown);
    navigation.setButtonL2Key(Qt::Key_PageDown);
    navigation.setKeyForButton(QGamepadManager::ButtonX, Qt::Key_X);
    CHECK(changes == QList<int>{Qt::Key_PageDown});
    CHECK(navigation.buttonXKey() == Qt::Key_X &&
          navigation.keyForButton(QGamepadManager::ButtonL2) == Qt::Key_PageDown);
    first->send({axis(2, -0.2)}); // 40%
    spinEvents(20);
    CHECK(window.log.isEmpty());
    first->send({axis(2, 0.6)});
    CHECK(waitFor([&] { return window.log.size() == 1; }));
    first->send({axis(2, 0.8), button(2, true)});
    CHECK(waitFor([&] { return window.log.size() == 2; }));
    first->send({axis(2, -1.0), button(2, false)});
    CHECK(waitFor([&] { return window.log.size() == 4; }));
    // Keys follow the order the controls changed in.
    CHECK(window.log == (QStringList{press(Qt::Key_PageDown), press(Qt::Key_X, " text"), release(Qt::Key_PageDown),
                                     release(Qt::Key_X)}));
    window.log.clear();

    // Auto repeat.
    navigation.setAutoRepeat(true);
    navigation.setAutoRepeatDelay(30);
    navigation.setAutoRepeatInterval(10);
    first->send({hat(0, QtPrivate::HatUp)});
    CHECK(waitFor([&] { return window.log.size() >= 3; }));
    first->send({hat(0, 0)});
    CHECK(waitFor([&] { return window.log.last() == release(Qt::Key_Up); }));
    CHECK(window.log.first() == press(Qt::Key_Up));
    CHECK(window.log.at(1) == press(Qt::Key_Up, " repeat"));
    const qsizetype count = window.log.size();
    spinEvents(60);
    CHECK(window.log.size() == count);
    navigation.setAutoRepeat(false);
    window.log.clear();

    // Inactive, or bound to another pad: nothing. A held key is let go when turned off.
    first->send({button(1, true)});
    CHECK(waitFor([&] { return window.log.size() == 1; }));
    QList<bool> activeChanges;
    QObject::connect(&navigation, &QGamepadKeyNavigation::activeChanged, [&](bool a) { activeChanges << a; });
    navigation.setActive(false);
    CHECK(window.log == (QStringList{press(Qt::Key_Back), release(Qt::Key_Back)}));
    first->send({button(1, false), button(0, true), button(0, false)});
    spinEvents(30);
    CHECK(window.log.size() == 2);
    navigation.setActive(true);
    CHECK(activeChanges == (QList<bool>{false, true}));
    window.log.clear();

    QList<QGamepad *> gamepads;
    {
        QGamepad other(5);
        QObject::connect(&navigation, &QGamepadKeyNavigation::gamepadChanged, [&](QGamepad *g) { gamepads << g; });
        navigation.setGamepad(&other);
        CHECK(navigation.gamepad() == &other);
        first->send({button(0, true), button(0, false)});
        spinEvents(30);
        CHECK(window.log.isEmpty());
        CHECK(gamepads.size() == 1);
    }
    CHECK(navigation.gamepad() == nullptr); // the QGamepad went away
    CHECK(gamepads.size() == 2 && gamepads.last() == nullptr);
    first->send({button(0, true), button(0, false)});
    // A press and release read together are still a press.
    CHECK(waitFor([&] { return window.log.size() == 2; }));
    CHECK(window.log == (QStringList{press(Qt::Key_Return, " text"), release(Qt::Key_Return)}));

    const QMetaObject *mo = &QGamepadKeyNavigation::staticMetaObject;
    CHECK(mo->indexOfProperty("buttonAKey") >= 0 && mo->indexOfProperty("autoRepeat") >= 0);
}

static void testMetaObjects() {
    const QMetaObject *mo = &QGamepad::staticMetaObject;
    CHECK(mo->indexOfProperty("buttonL2") >= 0 && mo->indexOfProperty("axisRightY") >= 0);
    CHECK(mo->indexOfProperty("canRumble") >= 0);
    CHECK(QGamepadManager::staticMetaObject.indexOfEnumerator("GamepadButton") >= 0);
    CHECK(QGamepadManager::staticMetaObject.indexOfProperty("connectedGamepads") >= 0);
    QGamepad gamepad(0);
    CHECK(gamepad.property("connected").toBool());
    CHECK(QGamepadManager::instance()->property("connectedGamepads").value<QList<int>>() == QList<int>{0});
}

int main(int argc, char **argv) {
    testMappingParse();
    testMappingValues();
    testGuid();
    testEvdev();

    QGuiApplication app(argc, argv);
    FakeWorld world;
    g_world = &world;
    QtPrivate::setGamepadPlatformFactory(makeFakePlatform);
    auto first = makePad(QStringLiteral("/fake/event3"), QStringLiteral("one"));
    world.pads.push_back(first);

    testManager(first);
    testHotplug(first);
    testMappings(first);
    testConfiguration(first);
    testConfigurationNeeded();
    testKeyNavigation(first);
    testMetaObjects();

    QtPrivate::setGamepadPlatformFactory(nullptr);
    if (g_failures == 0)
        std::printf("All gamepad tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
