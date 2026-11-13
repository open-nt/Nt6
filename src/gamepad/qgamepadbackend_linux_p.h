// MIT License - see LICENSE
//
// The parts of the evdev backend that do not touch a device, so that the tests can check them:
// which input devices are gamepads, how SDL numbers their controls, and the mapping guessed from
// the codes the kernel gives them.
#pragma once

#include <QtCore/qstring.h>

#include <functional>
#include <vector>

namespace QtPrivate {

struct EvdevLayout {
    std::vector<int> buttonCodes; // KEY_* or BTN_* code of b0, b1, ...
    std::vector<int> axisCodes;   // ABS_* code of a0, a1, ...
    std::vector<int> hatPairs;    // for h0, h1, ...: which of ABS_HAT0X..ABS_HAT3Y (0 to 3)
};

using EvdevHasCode = std::function<bool(int code)>;

// Whether a device with these keys, absolute axes and properties is a gamepad or joystick, rather
// than a touchpad, tablet, accelerometer or keyboard.
bool evdevIsGamepad(const EvdevHasCode &hasKey, const EvdevHasCode &hasAbs, const EvdevHasCode &hasProp);
// Numbers the controls as SDL does on Linux, so that GameControllerDB mappings apply.
EvdevLayout evdevLayout(const EvdevHasCode &hasKey, const EvdevHasCode &hasAbs);
// The bindings for a device with this layout, from the codes the kernel gives its controls. Sets
// standard to whether the device names its buttons (BTN_SOUTH and the rest).
QString evdevDefaultMapping(const EvdevLayout &layout, quint16 vendor, bool &standard);

} // namespace QtPrivate
