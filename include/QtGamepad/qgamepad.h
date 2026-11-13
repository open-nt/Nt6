// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtGamepad/qgamepadmanager.h>

#include <memory>

class QGamepadPrivate;

class QGamepad : public QObject {
    Q_OBJECT
    Q_PROPERTY(int deviceId READ deviceId WRITE setDeviceId NOTIFY deviceIdChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(double axisLeftX READ axisLeftX NOTIFY axisLeftXChanged)
    Q_PROPERTY(double axisLeftY READ axisLeftY NOTIFY axisLeftYChanged)
    Q_PROPERTY(double axisRightX READ axisRightX NOTIFY axisRightXChanged)
    Q_PROPERTY(double axisRightY READ axisRightY NOTIFY axisRightYChanged)
    Q_PROPERTY(bool buttonA READ buttonA NOTIFY buttonAChanged)
    Q_PROPERTY(bool buttonB READ buttonB NOTIFY buttonBChanged)
    Q_PROPERTY(bool buttonX READ buttonX NOTIFY buttonXChanged)
    Q_PROPERTY(bool buttonY READ buttonY NOTIFY buttonYChanged)
    Q_PROPERTY(bool buttonL1 READ buttonL1 NOTIFY buttonL1Changed)
    Q_PROPERTY(bool buttonR1 READ buttonR1 NOTIFY buttonR1Changed)
    Q_PROPERTY(double buttonL2 READ buttonL2 NOTIFY buttonL2Changed)
    Q_PROPERTY(double buttonR2 READ buttonR2 NOTIFY buttonR2Changed)
    Q_PROPERTY(bool buttonSelect READ buttonSelect NOTIFY buttonSelectChanged)
    Q_PROPERTY(bool buttonStart READ buttonStart NOTIFY buttonStartChanged)
    Q_PROPERTY(bool buttonL3 READ buttonL3 NOTIFY buttonL3Changed)
    Q_PROPERTY(bool buttonR3 READ buttonR3 NOTIFY buttonR3Changed)
    Q_PROPERTY(bool buttonUp READ buttonUp NOTIFY buttonUpChanged)
    Q_PROPERTY(bool buttonDown READ buttonDown NOTIFY buttonDownChanged)
    Q_PROPERTY(bool buttonLeft READ buttonLeft NOTIFY buttonLeftChanged)
    Q_PROPERTY(bool buttonRight READ buttonRight NOTIFY buttonRightChanged)
    Q_PROPERTY(bool buttonCenter READ buttonCenter NOTIFY buttonCenterChanged)
    Q_PROPERTY(bool buttonGuide READ buttonGuide NOTIFY buttonGuideChanged)
    Q_PROPERTY(bool canRumble READ canRumble NOTIFY connectedChanged)
public:
    explicit QGamepad(int deviceId = 0, QObject *parent = nullptr);
    ~QGamepad() override;

    int deviceId() const;
    bool isConnected() const;
    QString name() const;

    double axisLeftX() const;
    double axisLeftY() const;
    double axisRightX() const;
    double axisRightY() const;
    bool buttonA() const;
    bool buttonB() const;
    bool buttonX() const;
    bool buttonY() const;
    bool buttonL1() const;
    bool buttonR1() const;
    double buttonL2() const;
    double buttonR2() const;
    bool buttonSelect() const;
    bool buttonStart() const;
    bool buttonL3() const;
    bool buttonR3() const;
    bool buttonUp() const;
    bool buttonDown() const;
    bool buttonLeft() const;
    bool buttonRight() const;
    bool buttonCenter() const;
    bool buttonGuide() const;

    double buttonValue(QGamepadManager::GamepadButton button) const;
    double axisValue(QGamepadManager::GamepadAxis axis) const;

    bool canRumble() const;
    bool rumble(double strong, double weak, int msecs);

public slots:
    void setDeviceId(int deviceId);

signals:
    /*!
        This signal is emitted when the device id of the gamepad changes to \a value.
    */
    void deviceIdChanged(int value) { emitSignal(&QGamepad::deviceIdChanged, value); }
    /*!
        This signal is emitted when the gamepad is connected or disconnected; \a value is the new
        state.
    */
    void connectedChanged(bool value) { emitSignal(&QGamepad::connectedChanged, value); }
    /*!
        This signal is emitted when the name of the gamepad changes to \a value.
    */
    void nameChanged(QString value) { emitSignal(&QGamepad::nameChanged, value); }
    /*!
        This signal is emitted when the left stick moves along its horizontal axis to \a value.
    */
    void axisLeftXChanged(double value) { emitSignal(&QGamepad::axisLeftXChanged, value); }
    /*!
        This signal is emitted when the left stick moves along its vertical axis to \a value.
    */
    void axisLeftYChanged(double value) { emitSignal(&QGamepad::axisLeftYChanged, value); }
    /*!
        This signal is emitted when the right stick moves along its horizontal axis to \a value.
    */
    void axisRightXChanged(double value) { emitSignal(&QGamepad::axisRightXChanged, value); }
    /*!
        This signal is emitted when the right stick moves along its vertical axis to \a value.
    */
    void axisRightYChanged(double value) { emitSignal(&QGamepad::axisRightYChanged, value); }
    /*!
        This signal is emitted when the A button is pressed or released; \a value is \c true
        while it is held.
    */
    void buttonAChanged(bool value) { emitSignal(&QGamepad::buttonAChanged, value); }
    /*!
        This signal is emitted when the B button is pressed or released; \a value is \c true
        while it is held.
    */
    void buttonBChanged(bool value) { emitSignal(&QGamepad::buttonBChanged, value); }
    /*!
        This signal is emitted when the X button is pressed or released; \a value is \c true
        while it is held.
    */
    void buttonXChanged(bool value) { emitSignal(&QGamepad::buttonXChanged, value); }
    /*!
        This signal is emitted when the Y button is pressed or released; \a value is \c true
        while it is held.
    */
    void buttonYChanged(bool value) { emitSignal(&QGamepad::buttonYChanged, value); }
    /*!
        This signal is emitted when the left shoulder button is pressed or released; \a value is
        \c true while it is held.
    */
    void buttonL1Changed(bool value) { emitSignal(&QGamepad::buttonL1Changed, value); }
    /*!
        This signal is emitted when the right shoulder button is pressed or released; \a value is
        \c true while it is held.
    */
    void buttonR1Changed(bool value) { emitSignal(&QGamepad::buttonR1Changed, value); }
    /*!
        This signal is emitted when the left trigger moves to \a value, from 0.0 to 1.0.
    */
    void buttonL2Changed(double value) { emitSignal(&QGamepad::buttonL2Changed, value); }
    /*!
        This signal is emitted when the right trigger moves to \a value, from 0.0 to 1.0.
    */
    void buttonR2Changed(double value) { emitSignal(&QGamepad::buttonR2Changed, value); }
    /*!
        This signal is emitted when the Select (Back, View, Share) button is pressed or released;
        \a value is \c true while it is held.
    */
    void buttonSelectChanged(bool value) { emitSignal(&QGamepad::buttonSelectChanged, value); }
    /*!
        This signal is emitted when the Start (Menu, Options) button is pressed or released;
        \a value is \c true while it is held.
    */
    void buttonStartChanged(bool value) { emitSignal(&QGamepad::buttonStartChanged, value); }
    /*!
        This signal is emitted when the left stick is pressed in or released; \a value is
        \c true while it is held.
    */
    void buttonL3Changed(bool value) { emitSignal(&QGamepad::buttonL3Changed, value); }
    /*!
        This signal is emitted when the right stick is pressed in or released; \a value is
        \c true while it is held.
    */
    void buttonR3Changed(bool value) { emitSignal(&QGamepad::buttonR3Changed, value); }
    /*!
        This signal is emitted when up on the directional pad is pressed or released; \a value
        is \c true while it is held.
    */
    void buttonUpChanged(bool value) { emitSignal(&QGamepad::buttonUpChanged, value); }
    /*!
        This signal is emitted when down on the directional pad is pressed or released; \a value
        is \c true while it is held.
    */
    void buttonDownChanged(bool value) { emitSignal(&QGamepad::buttonDownChanged, value); }
    /*!
        This signal is emitted when left on the directional pad is pressed or released; \a value
        is \c true while it is held.
    */
    void buttonLeftChanged(bool value) { emitSignal(&QGamepad::buttonLeftChanged, value); }
    /*!
        This signal is emitted when right on the directional pad is pressed or released;
        \a value is \c true while it is held.
    */
    void buttonRightChanged(bool value) { emitSignal(&QGamepad::buttonRightChanged, value); }
    /*!
        This signal is emitted when the center button is pressed or released; \a value is
        \c true while it is held.
    */
    void buttonCenterChanged(bool value) { emitSignal(&QGamepad::buttonCenterChanged, value); }
    /*!
        This signal is emitted when the Guide (Home, PS) button is pressed or released; \a value
        is \c true while it is held.
    */
    void buttonGuideChanged(bool value) { emitSignal(&QGamepad::buttonGuideChanged, value); }
    /*!
        \since 6.0

        This signal is emitted when any \a button changes, with its new \a value: 0.0 or 1.0 for
        a digital button, from 0.0 to 1.0 for a trigger. It is emitted after the signal of the
        button's own property.

        \sa buttonValue(), axisValueChanged()
    */
    void buttonValueChanged(QGamepadManager::GamepadButton button, double value) {
        emitSignal(&QGamepad::buttonValueChanged, button, value);
    }
    /*!
        \since 6.0

        This signal is emitted when any \a axis moves, with its new \a value from -1.0 to 1.0.
        It is emitted after the signal of the axis's own property.

        \sa axisValue(), buttonValueChanged()
    */
    void axisValueChanged(QGamepadManager::GamepadAxis axis, double value) {
        emitSignal(&QGamepad::axisValueChanged, axis, value);
    }

private:
    friend class QGamepadPrivate;
    std::unique_ptr<QGamepadPrivate> d;
};
