// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

class QGamepadManagerPrivate;

class QGamepadManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QList<int> connectedGamepads READ connectedGamepads NOTIFY connectedGamepadsChanged)
    Q_PROPERTY(double deadZone READ deadZone WRITE setDeadZone)
public:
    enum GamepadButton {
        ButtonInvalid = -1,
        ButtonA = 0,
        ButtonB,
        ButtonX,
        ButtonY,
        ButtonL1,
        ButtonR1,
        ButtonL2,
        ButtonR2,
        ButtonSelect,
        ButtonStart,
        ButtonL3,
        ButtonR3,
        ButtonUp,
        ButtonDown,
        ButtonRight,
        ButtonLeft,
        ButtonCenter,
        ButtonGuide
    };
    Q_ENUM(GamepadButton)
    enum GamepadAxis { AxisInvalid = -1, AxisLeftX = 0, AxisLeftY, AxisRightX, AxisRightY };
    Q_ENUM(GamepadAxis)

    static QGamepadManager *instance();

    bool isGamepadConnected(int deviceId) const;
    QString gamepadName(int deviceId) const;
    QList<int> connectedGamepads() const;

    QString gamepadGuid(int deviceId) const;
    QString mapping(int deviceId) const;
    int addMappings(const QString &mappings);
    int addMappingsFromFile(const QString &fileName);

    double buttonValue(int deviceId, QGamepadManager::GamepadButton button) const;
    double axisValue(int deviceId, QGamepadManager::GamepadAxis axis) const;
    void setDeadZone(double deadZone);
    double deadZone() const;

    bool canRumble(int deviceId) const;
    bool rumble(int deviceId, double strong, double weak, int msecs);

public slots:
    bool isConfigurationNeeded(int deviceId) const;
    bool configureButton(int deviceId, QGamepadManager::GamepadButton button);
    bool configureAxis(int deviceId, QGamepadManager::GamepadAxis axis);
    bool setCancelConfigureButton(int deviceId, QGamepadManager::GamepadButton button);
    void resetConfiguration(int deviceId);
    void setSettingsFile(const QString &file);

signals:
    /*!
        This signal is emitted when a gamepad is connected or disconnected.

        \sa connectedGamepads(), gamepadConnected(), gamepadDisconnected()
    */
    void connectedGamepadsChanged() { emitSignal(&QGamepadManager::connectedGamepadsChanged); }
    /*!
        This signal is emitted when the gamepad \a deviceId is connected.

        \sa gamepadDisconnected()
    */
    void gamepadConnected(int deviceId) { emitSignal(&QGamepadManager::gamepadConnected, deviceId); }
    /*!
        This signal is emitted when the name of the gamepad \a deviceId changes to \a name.
    */
    void gamepadNameChanged(int deviceId, const QString &name) {
        emitSignal(&QGamepadManager::gamepadNameChanged, deviceId, name);
    }
    /*!
        This signal is emitted when the gamepad \a deviceId is disconnected.

        \sa gamepadConnected()
    */
    void gamepadDisconnected(int deviceId) { emitSignal(&QGamepadManager::gamepadDisconnected, deviceId); }
    /*!
        This signal is emitted when the \a axis of the gamepad \a deviceId moves to \a value, from
        -1.0 to 1.0.
    */
    void gamepadAxisEvent(int deviceId, QGamepadManager::GamepadAxis axis, double value) {
        emitSignal(&QGamepadManager::gamepadAxisEvent, deviceId, axis, value);
    }
    /*!
        This signal is emitted when the \a button of the gamepad \a deviceId is pressed, and again
        each time the \a value of an analog button such as a trigger changes while it is pressed.
        \a value is 1.0 for a digital button.

        \sa gamepadButtonReleaseEvent()
    */
    void gamepadButtonPressEvent(int deviceId, QGamepadManager::GamepadButton button, double value) {
        emitSignal(&QGamepadManager::gamepadButtonPressEvent, deviceId, button, value);
    }
    /*!
        This signal is emitted when the \a button of the gamepad \a deviceId is released.

        \sa gamepadButtonPressEvent()
    */
    void gamepadButtonReleaseEvent(int deviceId, QGamepadManager::GamepadButton button) {
        emitSignal(&QGamepadManager::gamepadButtonReleaseEvent, deviceId, button);
    }
    /*!
        This signal is emitted when configureButton() has bound \a button of the gamepad
        \a deviceId to the control that was pressed.
    */
    void buttonConfigured(int deviceId, QGamepadManager::GamepadButton button) {
        emitSignal(&QGamepadManager::buttonConfigured, deviceId, button);
    }
    /*!
        This signal is emitted when configureAxis() has bound \a axis of the gamepad \a deviceId
        to the control that was moved.
    */
    void axisConfigured(int deviceId, QGamepadManager::GamepadAxis axis) {
        emitSignal(&QGamepadManager::axisConfigured, deviceId, axis);
    }
    /*!
        This signal is emitted when the configuration of the gamepad \a deviceId is canceled with
        the button set by setCancelConfigureButton().
    */
    void configurationCanceled(int deviceId) { emitSignal(&QGamepadManager::configurationCanceled, deviceId); }

private:
    QGamepadManager();
    ~QGamepadManager() override;
    friend class QGamepadManagerPrivate;
    friend struct QGamepadManagerHolder;
    std::unique_ptr<QGamepadManagerPrivate> d;
};
