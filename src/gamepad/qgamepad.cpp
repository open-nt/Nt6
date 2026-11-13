// MIT License - see LICENSE
#include "qgamepadbackend_p.h"

#include <QtGamepad/qgamepad.h>

#include <array>

using Button = QGamepadManager::GamepadButton;
using Axis = QGamepadManager::GamepadAxis;

class QGamepadPrivate {
public:
    explicit QGamepadPrivate(QGamepad *owner, int id) : q(owner), deviceId(id) {}

    void emitButton(Button button, double value) {
        const bool on = value > 0.0;
        switch (button) {
        case QGamepadManager::ButtonA:
            emit q->buttonAChanged(on);
            break;
        case QGamepadManager::ButtonB:
            emit q->buttonBChanged(on);
            break;
        case QGamepadManager::ButtonX:
            emit q->buttonXChanged(on);
            break;
        case QGamepadManager::ButtonY:
            emit q->buttonYChanged(on);
            break;
        case QGamepadManager::ButtonL1:
            emit q->buttonL1Changed(on);
            break;
        case QGamepadManager::ButtonR1:
            emit q->buttonR1Changed(on);
            break;
        case QGamepadManager::ButtonL2:
            emit q->buttonL2Changed(value);
            break;
        case QGamepadManager::ButtonR2:
            emit q->buttonR2Changed(value);
            break;
        case QGamepadManager::ButtonSelect:
            emit q->buttonSelectChanged(on);
            break;
        case QGamepadManager::ButtonStart:
            emit q->buttonStartChanged(on);
            break;
        case QGamepadManager::ButtonL3:
            emit q->buttonL3Changed(on);
            break;
        case QGamepadManager::ButtonR3:
            emit q->buttonR3Changed(on);
            break;
        case QGamepadManager::ButtonUp:
            emit q->buttonUpChanged(on);
            break;
        case QGamepadManager::ButtonDown:
            emit q->buttonDownChanged(on);
            break;
        case QGamepadManager::ButtonRight:
            emit q->buttonRightChanged(on);
            break;
        case QGamepadManager::ButtonLeft:
            emit q->buttonLeftChanged(on);
            break;
        case QGamepadManager::ButtonCenter:
            emit q->buttonCenterChanged(on);
            break;
        case QGamepadManager::ButtonGuide:
            emit q->buttonGuideChanged(on);
            break;
        case QGamepadManager::ButtonInvalid:
            return;
        }
        emit q->buttonValueChanged(button, value);
    }

    void emitAxis(Axis axis, double value) {
        switch (axis) {
        case QGamepadManager::AxisLeftX:
            emit q->axisLeftXChanged(value);
            break;
        case QGamepadManager::AxisLeftY:
            emit q->axisLeftYChanged(value);
            break;
        case QGamepadManager::AxisRightX:
            emit q->axisRightXChanged(value);
            break;
        case QGamepadManager::AxisRightY:
            emit q->axisRightYChanged(value);
            break;
        case QGamepadManager::AxisInvalid:
            return;
        }
        emit q->axisValueChanged(axis, value);
    }

    void setButton(Button button, double value) {
        if (button < 0 || button >= QtPrivate::GamepadButtonCount)
            return;
        double &stored = buttons[static_cast<std::size_t>(button)];
        if (stored == value)
            return;
        stored = value;
        emitButton(button, value);
    }

    void setAxis(Axis axis, double value) {
        if (axis < 0 || axis >= QtPrivate::GamepadAxisCount)
            return;
        double &stored = axes[static_cast<std::size_t>(axis)];
        if (stored == value)
            return;
        stored = value;
        emitAxis(axis, value);
    }

    // Takes the state of the pad from the manager, reporting what changed.
    void sync() {
        QGamepadManager *manager = QGamepadManager::instance();
        const bool nowConnected = manager->isGamepadConnected(deviceId);
        const QString nowName = manager->gamepadName(deviceId);
        if (nowConnected != connected) {
            connected = nowConnected;
            emit q->connectedChanged(connected);
        }
        if (nowName != name) {
            name = nowName;
            emit q->nameChanged(name);
        }
        for (int a = 0; a < QtPrivate::GamepadAxisCount; ++a)
            setAxis(static_cast<Axis>(a), manager->axisValue(deviceId, static_cast<Axis>(a)));
        for (int b = 0; b < QtPrivate::GamepadButtonCount; ++b)
            setButton(static_cast<Button>(b), manager->buttonValue(deviceId, static_cast<Button>(b)));
    }

    double button(Button b) const { return buttons[static_cast<std::size_t>(b)]; }
    double axis(Axis a) const { return axes[static_cast<std::size_t>(a)]; }

    QGamepad *q;
    int deviceId;
    bool connected = false;
    QString name;
    std::array<double, QtPrivate::GamepadButtonCount> buttons{};
    std::array<double, QtPrivate::GamepadAxisCount> axes{};
};

/*!
    \class QGamepad
    \inmodule NtGamepad
    \brief The QGamepad class follows the buttons and sticks of one gamepad.
    \since 6.0

    A QGamepad has a property for each button and axis of the pad with the id deviceId(), and a
    signal for each change:

    \code
    QGamepad gamepad; // the first pad
    QObject::connect(&gamepad, &QGamepad::buttonAChanged, [](bool pressed) { if (pressed) jump(); });
    QObject::connect(&gamepad, &QGamepad::axisLeftXChanged, [](double x) { steer(x); });
    \endcode

    buttonValueChanged() and axisValueChanged() report every control through one signal each, for
    code that handles them from a table.

    The pad need not be connected when the QGamepad is made. isConnected() tells whether it is,
    and connectedChanged() reports when it is plugged in or out; while it is away, every control
    reads as at rest. A pad that is plugged in again keeps its id, so the QGamepad picks it up
    again.

    \differences
    Added: buttonValue(), axisValue(), buttonValueChanged(), axisValueChanged(), canRumble() and
    rumble().
    \enddifferences

    \sa QGamepadManager, {Using Nt Gamepad}
*/

/*!
    Constructs a QGamepad for the gamepad \a deviceId, with the given \a parent.
*/
QGamepad::QGamepad(int deviceId, QObject *parent)
    : QObject(parent), d(std::make_unique<QGamepadPrivate>(this, deviceId)) {
    QGamepadManager *manager = QGamepadManager::instance();
    connect(manager, &QGamepadManager::gamepadConnected, this, [this](int id) {
        if (id == d->deviceId)
            d->sync();
    });
    connect(manager, &QGamepadManager::gamepadDisconnected, this, [this](int id) {
        if (id == d->deviceId)
            d->sync();
    });
    connect(manager, &QGamepadManager::gamepadNameChanged, this, [this](int id, const QString &) {
        if (id == d->deviceId)
            d->sync();
    });
    connect(manager, &QGamepadManager::gamepadAxisEvent, this, [this](int id, Axis axis, double value) {
        if (id == d->deviceId)
            d->setAxis(axis, value);
    });
    connect(manager, &QGamepadManager::gamepadButtonPressEvent, this, [this](int id, Button button, double value) {
        if (id == d->deviceId)
            d->setButton(button, value);
    });
    connect(manager, &QGamepadManager::gamepadButtonReleaseEvent, this, [this](int id, Button button) {
        if (id == d->deviceId)
            d->setButton(button, 0.0);
    });
    d->connected = manager->isGamepadConnected(deviceId);
    d->name = manager->gamepadName(deviceId);
    for (int a = 0; a < QtPrivate::GamepadAxisCount; ++a)
        d->axes[static_cast<std::size_t>(a)] = manager->axisValue(deviceId, static_cast<Axis>(a));
    for (int b = 0; b < QtPrivate::GamepadButtonCount; ++b)
        d->buttons[static_cast<std::size_t>(b)] = manager->buttonValue(deviceId, static_cast<Button>(b));
}

/*!
    Destroys the QGamepad.
*/
QGamepad::~QGamepad() = default;

/*!
    Returns the id of the gamepad this object follows.

    Ids come from QGamepadManager::connectedGamepads(). The default, 0, is the first pad plugged
    in.
*/
int QGamepad::deviceId() const { return d->deviceId; }

/*!
    Makes this object follow the gamepad \a deviceId, and reports the changes this brings.
*/
void QGamepad::setDeviceId(int deviceId) {
    if (deviceId == d->deviceId)
        return;
    d->deviceId = deviceId;
    emit deviceIdChanged(deviceId);
    d->sync();
}

/*!
    Returns \c true if the gamepad is connected; otherwise returns \c false.
*/
bool QGamepad::isConnected() const { return d->connected; }
/*!
    Returns the name of the gamepad, or an empty string while it is not connected.
*/
QString QGamepad::name() const { return d->name; }
/*!
    Returns the position of the left stick from left (-1.0) to right (1.0).
*/
double QGamepad::axisLeftX() const { return d->axis(QGamepadManager::AxisLeftX); }
/*!
    Returns the position of the left stick from up (-1.0) to down (1.0).
*/
double QGamepad::axisLeftY() const { return d->axis(QGamepadManager::AxisLeftY); }
/*!
    Returns the position of the right stick from left (-1.0) to right (1.0).
*/
double QGamepad::axisRightX() const { return d->axis(QGamepadManager::AxisRightX); }
/*!
    Returns the position of the right stick from up (-1.0) to down (1.0).
*/
double QGamepad::axisRightY() const { return d->axis(QGamepadManager::AxisRightY); }
/*!
    Returns \c true if the bottom face button is pressed; otherwise returns \c false.
*/
bool QGamepad::buttonA() const { return d->button(QGamepadManager::ButtonA) > 0.0; }
/*!
    Returns \c true if the right face button is pressed; otherwise returns \c false.
*/
bool QGamepad::buttonB() const { return d->button(QGamepadManager::ButtonB) > 0.0; }
/*!
    Returns \c true if the left face button is pressed; otherwise returns \c false.
*/
bool QGamepad::buttonX() const { return d->button(QGamepadManager::ButtonX) > 0.0; }
/*!
    Returns \c true if the top face button is pressed; otherwise returns \c false.
*/
bool QGamepad::buttonY() const { return d->button(QGamepadManager::ButtonY) > 0.0; }
/*!
    Returns \c true if the left shoulder button is pressed; otherwise returns \c false.
*/
bool QGamepad::buttonL1() const { return d->button(QGamepadManager::ButtonL1) > 0.0; }
/*!
    Returns \c true if the right shoulder button is pressed; otherwise returns \c false.
*/
bool QGamepad::buttonR1() const { return d->button(QGamepadManager::ButtonR1) > 0.0; }
/*!
    Returns how far the left trigger is pressed, from 0.0 to 1.0.
*/
double QGamepad::buttonL2() const { return d->button(QGamepadManager::ButtonL2); }
/*!
    Returns how far the right trigger is pressed, from 0.0 to 1.0.
*/
double QGamepad::buttonR2() const { return d->button(QGamepadManager::ButtonR2); }
/*!
    Returns \c true if the Select (Back, View, Share) button is pressed; otherwise returns \c false.
*/
bool QGamepad::buttonSelect() const { return d->button(QGamepadManager::ButtonSelect) > 0.0; }
/*!
    Returns \c true if the Start (Menu, Options) button is pressed; otherwise returns \c false.
*/
bool QGamepad::buttonStart() const { return d->button(QGamepadManager::ButtonStart) > 0.0; }
/*!
    Returns \c true if the left stick is pressed in; otherwise returns \c false.
*/
bool QGamepad::buttonL3() const { return d->button(QGamepadManager::ButtonL3) > 0.0; }
/*!
    Returns \c true if the right stick is pressed in; otherwise returns \c false.
*/
bool QGamepad::buttonR3() const { return d->button(QGamepadManager::ButtonR3) > 0.0; }
/*!
    Returns \c true if up on the directional pad is pressed; otherwise returns \c false.
*/
bool QGamepad::buttonUp() const { return d->button(QGamepadManager::ButtonUp) > 0.0; }
/*!
    Returns \c true if down on the directional pad is pressed; otherwise returns \c false.
*/
bool QGamepad::buttonDown() const { return d->button(QGamepadManager::ButtonDown) > 0.0; }
/*!
    Returns \c true if left on the directional pad is pressed; otherwise returns \c false.
*/
bool QGamepad::buttonLeft() const { return d->button(QGamepadManager::ButtonLeft) > 0.0; }
/*!
    Returns \c true if right on the directional pad is pressed; otherwise returns \c false.
*/
bool QGamepad::buttonRight() const { return d->button(QGamepadManager::ButtonRight) > 0.0; }
/*!
    Returns \c true if the center button is pressed; otherwise returns \c false.

    \sa QGamepadManager::ButtonCenter
*/
bool QGamepad::buttonCenter() const { return d->button(QGamepadManager::ButtonCenter) > 0.0; }
/*!
    Returns \c true if the Guide (Home, PS) button is pressed; otherwise returns \c false.
*/
bool QGamepad::buttonGuide() const { return d->button(QGamepadManager::ButtonGuide) > 0.0; }

/*!
    \since 6.0

    Returns the value of \a button: 0.0 or 1.0, or from 0.0 to 1.0 for a trigger.

    \sa buttonValueChanged(), axisValue()
*/
double QGamepad::buttonValue(QGamepadManager::GamepadButton button) const {
    if (button < 0 || button >= QtPrivate::GamepadButtonCount)
        return 0.0;
    return d->button(button);
}

/*!
    \since 6.0

    Returns the value of \a axis, from -1.0 to 1.0.

    \sa axisValueChanged(), buttonValue()
*/
double QGamepad::axisValue(QGamepadManager::GamepadAxis axis) const {
    if (axis < 0 || axis >= QtPrivate::GamepadAxisCount)
        return 0.0;
    return d->axis(axis);
}

/*!
    \since 6.0

    Returns \c true if the gamepad is connected and has rumble motors the program may drive; otherwise returns \c false.

    \sa rumble()
*/
bool QGamepad::canRumble() const { return QGamepadManager::instance()->canRumble(d->deviceId); }

/*!
    \since 6.0

    Runs the rumble motors for \a msecs milliseconds, the strong (low frequency) one at
    \a strong and the weak (high frequency) one at \a weak, each from 0.0 to 1.0. Returns
    \c false if the pad cannot rumble.

    \sa QGamepadManager::rumble()
*/
bool QGamepad::rumble(double strong, double weak, int msecs) {
    return QGamepadManager::instance()->rumble(d->deviceId, strong, weak, msecs);
}

#include "moc_qgamepad.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
