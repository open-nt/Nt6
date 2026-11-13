// MIT License - see LICENSE
//
// The platform part of Nt Gamepad, and the SDL mapping strings that turn raw controls into the
// buttons and axes of QGamepadManager.
//
// A GamepadPlatform lists the devices that may be gamepads and opens them; qgamepadbackend_linux.cpp
// does this with evdev. Each open GamepadDevice reports raw controls the way SDL numbers them
// (buttons b0.., axes a0.., hats h0..), so that mappings from SDL's GameControllerDB apply as they
// are. The tests replace the platform with setGamepadPlatformFactory().
#pragma once

#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtGamepad/qgamepadmanager.h>

#include <array>
#include <memory>
#include <vector>

namespace QtPrivate {

constexpr int GamepadButtonCount = QGamepadManager::ButtonGuide + 1;
constexpr int GamepadAxisCount = QGamepadManager::AxisRightY + 1;

// Hat directions, as SDL numbers them in mappings ("h0.1" is up on the first hat).
enum GamepadHatMask { HatUp = 1, HatRight = 2, HatDown = 4, HatLeft = 8 };

struct GamepadDeviceInfo {
    QString name;
    QString guid;     // 32 hex digits, as SDL forms it for this device
    QString identity; // the same each time this pad is plugged in: serial, physical path or node
    int buttons = 0;
    int axes = 0;
    int hats = 0;
    bool rumble = false;
    bool standardLayout = false; // the device names its buttons, so the default mapping is not a guess
    QString defaultMapping;      // SDL bindings for the device, without GUID and name: "a:b0,b:b1,..."
};

// One raw control that changed: a button (0 or 1), an axis (-1.0 to 1.0) or a hat (a GamepadHatMask
// combination).
struct GamepadInput {
    enum Kind { Button, Axis, Hat };
    Kind kind = Button;
    int index = 0;
    double value = 0.0;
};

class GamepadDevice {
public:
    GamepadDevice() = default;
    virtual ~GamepadDevice() = default;
    GamepadDevice(const GamepadDevice &) = delete;
    GamepadDevice &operator=(const GamepadDevice &) = delete;

    virtual GamepadDeviceInfo info() const = 0;
    // A descriptor that becomes readable when controls change.
    virtual int eventDescriptor() const = 0;
    // Appends the controls that changed; returns false when the device has gone away.
    virtual bool readInputs(std::vector<GamepadInput> &inputs) = 0;
    // Appends the current state of every control.
    virtual void readState(std::vector<GamepadInput> &inputs) = 0;
    // Starts both rumble motors at strong and weak (0.0 to 1.0) for msecs; zero stops them.
    virtual bool rumble(double strong, double weak, int msecs) = 0;
};

class GamepadPlatform {
public:
    GamepadPlatform() = default;
    virtual ~GamepadPlatform() = default;
    GamepadPlatform(const GamepadPlatform &) = delete;
    GamepadPlatform &operator=(const GamepadPlatform &) = delete;

    // A directory that changes when devices come and go, or an empty string.
    virtual QString deviceDirectory() const = 0;
    // The devices that may be gamepads.
    virtual QStringList devicePaths() = 0;
    // Opens path if it is a gamepad. Sets retry when it may become one that can be opened soon,
    // as a node does before udev has given it its permissions.
    virtual std::unique_ptr<GamepadDevice> openDevice(const QString &path, bool &retry) = 0;
};

using GamepadPlatformFactory = std::unique_ptr<GamepadPlatform> (*)();

std::unique_ptr<GamepadPlatform> createPlatformGamepadPlatform();
// Makes the QGamepadManager created next use factory, or the platform again when it is null.
void setGamepadPlatformFactory(GamepadPlatformFactory factory);

// Forms the GUID SDL gives a device from its bus, vendor, product and version; a device without a
// vendor and product gets the start of its name instead.
QString gamepadGuid(quint16 bus, quint16 vendor, quint16 product, quint16 version, const QString &name);

struct GamepadBinding {
    enum Source { None, Button, Axis, Hat };
    enum Range { Full, Positive, Negative };
    Source source = None;
    int index = 0;
    int hatMask = 0;
    Range inputRange = Full;  // "+a2" or "-a2" use half of the axis
    bool invert = false;      // "a2~"
    Range outputRange = Full; // "+leftx:..." drives half of the axis
};

struct GamepadRawState {
    std::vector<double> buttons;
    std::vector<double> axes;
    std::vector<int> hats;
    void apply(const GamepadInput &input);
};

struct GamepadMapping {
    QString guid;
    QString name;
    std::array<std::vector<GamepadBinding>, GamepadButtonCount> buttons;
    std::array<std::vector<GamepadBinding>, GamepadAxisCount> axes;

    // Parses "guid,name,a:b0,...". Fields of other platforms, and controls Qt has no name for, are
    // skipped. Returns false if the line is not a mapping for this platform.
    bool parse(const QString &line);
    // Parses the bindings alone, "a:b0,b:b1,...", over the ones already set.
    void parseBindings(const QString &bindings);
    QString toString() const;
    bool hasBinding(QGamepadManager::GamepadButton button) const { return !buttons[button].empty(); }
    bool hasBinding(QGamepadManager::GamepadAxis axis) const { return !axes[axis].empty(); }

    double buttonValue(QGamepadManager::GamepadButton button, const GamepadRawState &state) const;
    double axisValue(QGamepadManager::GamepadAxis axis, const GamepadRawState &state) const;
};

// Whether two GUIDs name the same kind of device: equal, or equal once the name checksum SDL 2.26
// added, or the version too, is ignored.
enum class GuidMatch { None, Loose, Close, Exact };
GuidMatch matchGamepadGuid(const QString &device, const QString &mapping);

// The name of a button or axis in SDL mappings, such as "leftshoulder".
QString gamepadButtonMappingName(QGamepadManager::GamepadButton button);
QString gamepadAxisMappingName(QGamepadManager::GamepadAxis axis);

} // namespace QtPrivate
