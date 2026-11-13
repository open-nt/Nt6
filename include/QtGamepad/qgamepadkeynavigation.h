// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtGamepad/qgamepadmanager.h>

#include <memory>

class QGamepad;
class QGamepadKeyNavigationPrivate;

class QGamepadKeyNavigation : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QGamepad *gamepad READ gamepad WRITE setGamepad NOTIFY gamepadChanged)
    Q_PROPERTY(Qt::Key upKey READ upKey WRITE setUpKey NOTIFY upKeyChanged)
    Q_PROPERTY(Qt::Key downKey READ downKey WRITE setDownKey NOTIFY downKeyChanged)
    Q_PROPERTY(Qt::Key leftKey READ leftKey WRITE setLeftKey NOTIFY leftKeyChanged)
    Q_PROPERTY(Qt::Key rightKey READ rightKey WRITE setRightKey NOTIFY rightKeyChanged)
    Q_PROPERTY(Qt::Key buttonAKey READ buttonAKey WRITE setButtonAKey NOTIFY buttonAKeyChanged)
    Q_PROPERTY(Qt::Key buttonBKey READ buttonBKey WRITE setButtonBKey NOTIFY buttonBKeyChanged)
    Q_PROPERTY(Qt::Key buttonXKey READ buttonXKey WRITE setButtonXKey NOTIFY buttonXKeyChanged)
    Q_PROPERTY(Qt::Key buttonYKey READ buttonYKey WRITE setButtonYKey NOTIFY buttonYKeyChanged)
    Q_PROPERTY(Qt::Key buttonSelectKey READ buttonSelectKey WRITE setButtonSelectKey NOTIFY buttonSelectKeyChanged)
    Q_PROPERTY(Qt::Key buttonStartKey READ buttonStartKey WRITE setButtonStartKey NOTIFY buttonStartKeyChanged)
    Q_PROPERTY(Qt::Key buttonGuideKey READ buttonGuideKey WRITE setButtonGuideKey NOTIFY buttonGuideKeyChanged)
    Q_PROPERTY(Qt::Key buttonL1Key READ buttonL1Key WRITE setButtonL1Key NOTIFY buttonL1KeyChanged)
    Q_PROPERTY(Qt::Key buttonR1Key READ buttonR1Key WRITE setButtonR1Key NOTIFY buttonR1KeyChanged)
    Q_PROPERTY(Qt::Key buttonL2Key READ buttonL2Key WRITE setButtonL2Key NOTIFY buttonL2KeyChanged)
    Q_PROPERTY(Qt::Key buttonR2Key READ buttonR2Key WRITE setButtonR2Key NOTIFY buttonR2KeyChanged)
    Q_PROPERTY(Qt::Key buttonL3Key READ buttonL3Key WRITE setButtonL3Key NOTIFY buttonL3KeyChanged)
    Q_PROPERTY(Qt::Key buttonR3Key READ buttonR3Key WRITE setButtonR3Key NOTIFY buttonR3KeyChanged)
    Q_PROPERTY(bool autoRepeat READ autoRepeat WRITE setAutoRepeat)
    Q_PROPERTY(int autoRepeatDelay READ autoRepeatDelay WRITE setAutoRepeatDelay)
    Q_PROPERTY(int autoRepeatInterval READ autoRepeatInterval WRITE setAutoRepeatInterval)
public:
    explicit QGamepadKeyNavigation(QObject *parent = nullptr);
    ~QGamepadKeyNavigation() override;

    bool active() const;
    QGamepad *gamepad() const;

    Qt::Key upKey() const;
    Qt::Key downKey() const;
    Qt::Key leftKey() const;
    Qt::Key rightKey() const;
    Qt::Key buttonAKey() const;
    Qt::Key buttonBKey() const;
    Qt::Key buttonXKey() const;
    Qt::Key buttonYKey() const;
    Qt::Key buttonSelectKey() const;
    Qt::Key buttonStartKey() const;
    Qt::Key buttonGuideKey() const;
    Qt::Key buttonL1Key() const;
    Qt::Key buttonR1Key() const;
    Qt::Key buttonL2Key() const;
    Qt::Key buttonR2Key() const;
    Qt::Key buttonL3Key() const;
    Qt::Key buttonR3Key() const;

    Q_INVOKABLE Qt::Key keyForButton(QGamepadManager::GamepadButton button) const;
    Q_INVOKABLE void setKeyForButton(QGamepadManager::GamepadButton button, Qt::Key key);

    bool autoRepeat() const;
    void setAutoRepeat(bool enable);
    int autoRepeatDelay() const;
    void setAutoRepeatDelay(int msecs);
    int autoRepeatInterval() const;
    void setAutoRepeatInterval(int msecs);

public slots:
    void setActive(bool isActive);
    void setGamepad(QGamepad *gamepad);

    void setUpKey(Qt::Key key);
    void setDownKey(Qt::Key key);
    void setLeftKey(Qt::Key key);
    void setRightKey(Qt::Key key);
    void setButtonAKey(Qt::Key key);
    void setButtonBKey(Qt::Key key);
    void setButtonXKey(Qt::Key key);
    void setButtonYKey(Qt::Key key);
    void setButtonSelectKey(Qt::Key key);
    void setButtonStartKey(Qt::Key key);
    void setButtonGuideKey(Qt::Key key);
    void setButtonL1Key(Qt::Key key);
    void setButtonR1Key(Qt::Key key);
    void setButtonL2Key(Qt::Key key);
    void setButtonR2Key(Qt::Key key);
    void setButtonL3Key(Qt::Key key);
    void setButtonR3Key(Qt::Key key);

signals:
    /*!
        This signal is emitted when key navigation is turned on or off; \a isActive is the new
        state.
    */
    void activeChanged(bool isActive) { emitSignal(&QGamepadKeyNavigation::activeChanged, isActive); }
    /*!
        This signal is emitted when the gamepad whose buttons are turned into keys changes to
        \a gamepad.
    */
    void gamepadChanged(QGamepad *gamepad) { emitSignal(&QGamepadKeyNavigation::gamepadChanged, gamepad); }
    /*!
        This signal is emitted when the key for up on the directional pad changes to \a key.
    */
    void upKeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::upKeyChanged, key); }
    /*!
        This signal is emitted when the key for down on the directional pad changes to \a key.
    */
    void downKeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::downKeyChanged, key); }
    /*!
        This signal is emitted when the key for left on the directional pad changes to \a key.
    */
    void leftKeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::leftKeyChanged, key); }
    /*!
        This signal is emitted when the key for right on the directional pad changes to \a key.
    */
    void rightKeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::rightKeyChanged, key); }
    /*!
        This signal is emitted when the key for the A button changes to \a key.
    */
    void buttonAKeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::buttonAKeyChanged, key); }
    /*!
        This signal is emitted when the key for the B button changes to \a key.
    */
    void buttonBKeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::buttonBKeyChanged, key); }
    /*!
        This signal is emitted when the key for the X button changes to \a key.
    */
    void buttonXKeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::buttonXKeyChanged, key); }
    /*!
        This signal is emitted when the key for the Y button changes to \a key.
    */
    void buttonYKeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::buttonYKeyChanged, key); }
    /*!
        This signal is emitted when the key for the Select button changes to \a key.
    */
    void buttonSelectKeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::buttonSelectKeyChanged, key); }
    /*!
        This signal is emitted when the key for the Start button changes to \a key.
    */
    void buttonStartKeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::buttonStartKeyChanged, key); }
    /*!
        This signal is emitted when the key for the Guide button changes to \a key.
    */
    void buttonGuideKeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::buttonGuideKeyChanged, key); }
    /*!
        This signal is emitted when the key for the left shoulder button changes to \a key.
    */
    void buttonL1KeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::buttonL1KeyChanged, key); }
    /*!
        This signal is emitted when the key for the right shoulder button changes to \a key.
    */
    void buttonR1KeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::buttonR1KeyChanged, key); }
    /*!
        This signal is emitted when the key for the left trigger changes to \a key.
    */
    void buttonL2KeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::buttonL2KeyChanged, key); }
    /*!
        This signal is emitted when the key for the right trigger changes to \a key.
    */
    void buttonR2KeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::buttonR2KeyChanged, key); }
    /*!
        This signal is emitted when the key for pressing the left stick changes to \a key.
    */
    void buttonL3KeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::buttonL3KeyChanged, key); }
    /*!
        This signal is emitted when the key for pressing the right stick changes to \a key.
    */
    void buttonR3KeyChanged(Qt::Key key) { emitSignal(&QGamepadKeyNavigation::buttonR3KeyChanged, key); }

private:
    friend class QGamepadKeyNavigationPrivate;
    std::unique_ptr<QGamepadKeyNavigationPrivate> d;
};
