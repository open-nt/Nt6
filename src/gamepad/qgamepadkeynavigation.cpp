// MIT License - see LICENSE
#include "qgamepadbackend_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qtimer.h>
#include <QtGamepad/qgamepad.h>
#include <QtGamepad/qgamepadkeynavigation.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qwindow.h>

#include <array>

using Button = QGamepadManager::GamepadButton;

class QGamepadKeyNavigationPrivate {
public:
    explicit QGamepadKeyNavigationPrivate(QGamepadKeyNavigation *owner) : q(owner) {
        keys.fill(Qt::Key_unknown);
        keys[QGamepadManager::ButtonUp] = Qt::Key_Up;
        keys[QGamepadManager::ButtonDown] = Qt::Key_Down;
        keys[QGamepadManager::ButtonLeft] = Qt::Key_Left;
        keys[QGamepadManager::ButtonRight] = Qt::Key_Right;
        keys[QGamepadManager::ButtonA] = Qt::Key_Return;
        keys[QGamepadManager::ButtonB] = Qt::Key_Back;
        repeatTimer.setSingleShot(true);
        QObject::connect(&repeatTimer, &QTimer::timeout, q, [this] { repeat(); });
    }

    bool accepts(int deviceId) const { return !gamepad || gamepad->deviceId() == deviceId; }

    static QString textFor(Qt::Key key) {
        if (key == Qt::Key_Return || key == Qt::Key_Enter)
            return QStringLiteral("\r");
        if (key >= 0x20 && key <= 0x7e)
            return QString(QChar(key)).toLower();
        return QString();
    }

    static void send(QEvent::Type type, Qt::Key key, bool autoRepeated) {
        QWindow *window = QGuiApplication::focusWindow();
        if (!window || key == Qt::Key_unknown)
            return;
        QKeyEvent event(type, key, Qt::NoModifier, textFor(key), autoRepeated);
        QCoreApplication::sendEvent(window, &event);
    }

    void press(Button button) {
        const auto b = static_cast<std::size_t>(button);
        if (held[b])
            return;
        held[b] = true;
        const Qt::Key key = keys[b];
        sentKeys[b] = key;
        send(QEvent::KeyPress, key, false);
        if (autoRepeat && key != Qt::Key_unknown) {
            repeating = button;
            repeatTimer.start(repeatDelay);
        }
    }

    void release(Button button) {
        const auto b = static_cast<std::size_t>(button);
        if (!held[b])
            return;
        held[b] = false;
        if (repeating == button) {
            repeatTimer.stop();
            repeating = QGamepadManager::ButtonInvalid;
        }
        send(QEvent::KeyRelease, sentKeys[b], false);
    }

    void releaseAll() {
        for (int b = 0; b < QtPrivate::GamepadButtonCount; ++b)
            release(static_cast<Button>(b));
    }

    void repeat() {
        if (repeating == QGamepadManager::ButtonInvalid || !active)
            return;
        send(QEvent::KeyPress, sentKeys[static_cast<std::size_t>(repeating)], true);
        repeatTimer.start(repeatInterval);
    }

    void setKey(Button button, Qt::Key key) {
        if (button < 0 || button >= QtPrivate::GamepadButtonCount)
            return;
        Qt::Key &stored = keys[static_cast<std::size_t>(button)];
        if (stored == key)
            return;
        stored = key;
        switch (button) {
        case QGamepadManager::ButtonUp:
            emit q->upKeyChanged(key);
            break;
        case QGamepadManager::ButtonDown:
            emit q->downKeyChanged(key);
            break;
        case QGamepadManager::ButtonLeft:
            emit q->leftKeyChanged(key);
            break;
        case QGamepadManager::ButtonRight:
            emit q->rightKeyChanged(key);
            break;
        case QGamepadManager::ButtonA:
            emit q->buttonAKeyChanged(key);
            break;
        case QGamepadManager::ButtonB:
            emit q->buttonBKeyChanged(key);
            break;
        case QGamepadManager::ButtonX:
            emit q->buttonXKeyChanged(key);
            break;
        case QGamepadManager::ButtonY:
            emit q->buttonYKeyChanged(key);
            break;
        case QGamepadManager::ButtonSelect:
            emit q->buttonSelectKeyChanged(key);
            break;
        case QGamepadManager::ButtonStart:
            emit q->buttonStartKeyChanged(key);
            break;
        case QGamepadManager::ButtonGuide:
            emit q->buttonGuideKeyChanged(key);
            break;
        case QGamepadManager::ButtonL1:
            emit q->buttonL1KeyChanged(key);
            break;
        case QGamepadManager::ButtonR1:
            emit q->buttonR1KeyChanged(key);
            break;
        case QGamepadManager::ButtonL2:
            emit q->buttonL2KeyChanged(key);
            break;
        case QGamepadManager::ButtonR2:
            emit q->buttonR2KeyChanged(key);
            break;
        case QGamepadManager::ButtonL3:
            emit q->buttonL3KeyChanged(key);
            break;
        case QGamepadManager::ButtonR3:
            emit q->buttonR3KeyChanged(key);
            break;
        case QGamepadManager::ButtonCenter:
        case QGamepadManager::ButtonInvalid:
            break;
        }
    }

    Qt::Key key(Button button) const { return keys[static_cast<std::size_t>(button)]; }

    QGamepadKeyNavigation *q;
    bool active = true;
    QGamepad *gamepad = nullptr;
    QMetaObject::Connection gamepadDestroyed;
    std::array<Qt::Key, QtPrivate::GamepadButtonCount> keys{};
    std::array<Qt::Key, QtPrivate::GamepadButtonCount> sentKeys{}; // the key each held button pressed
    std::array<bool, QtPrivate::GamepadButtonCount> held{};
    bool autoRepeat = false;
    int repeatDelay = 400;
    int repeatInterval = 100;
    Button repeating = QGamepadManager::ButtonInvalid;
    QTimer repeatTimer;
};

/*!
    \class QGamepadKeyNavigation
    \inmodule NtGamepad
    \brief The QGamepadKeyNavigation class turns gamepad buttons into key events.
    \since 6.0

    A QGamepadKeyNavigation sends a key press to the window that has the focus when a gamepad
    button is pressed, and a key release when it is let go, so that a user interface made for
    the keyboard can be used with a gamepad:

    \code
    QGamepadKeyNavigation navigation; // all gamepads, arrows, Return and Back
    navigation.setButtonBKey(Qt::Key_Escape);
    navigation.setAutoRepeat(true);
    \endcode

    By default the directional pad sends the arrow keys, the A button sends \c Key_Return and
    the B button sends \c Key_Back; the other buttons send nothing. A trigger counts as pressed
    once it is more than halfway down.

    The key events go to QGuiApplication::focusWindow(), so a QGuiApplication or QApplication is
    needed for them to go anywhere.

    \differences
    Added: autoRepeat, autoRepeatDelay and autoRepeatInterval. keyForButton() and
    setKeyForButton() also take \c ButtonCenter, which has no property.
    \enddifferences

    \sa QGamepad, QGamepadManager
*/

/*!
    Constructs a key navigation for every gamepad, with the given \a parent. It is active.
*/
QGamepadKeyNavigation::QGamepadKeyNavigation(QObject *parent)
    : QObject(parent), d(std::make_unique<QGamepadKeyNavigationPrivate>(this)) {
    QGamepadManager *manager = QGamepadManager::instance();
    connect(manager, &QGamepadManager::gamepadButtonPressEvent, this, [this](int id, Button button, double value) {
        if (!d->active || !d->accepts(id) || button < 0 || button >= QtPrivate::GamepadButtonCount)
            return;
        if (value > 0.5)
            d->press(button);
        else
            d->release(button);
    });
    connect(manager, &QGamepadManager::gamepadButtonReleaseEvent, this, [this](int id, Button button) {
        if (d->accepts(id) && button >= 0 && button < QtPrivate::GamepadButtonCount)
            d->release(button);
    });
    connect(manager, &QGamepadManager::gamepadDisconnected, this, [this](int id) {
        if (d->accepts(id))
            d->releaseAll();
    });
}

/*!
    Destroys the key navigation.
*/
QGamepadKeyNavigation::~QGamepadKeyNavigation() = default;

/*!
    Returns \c true if buttons are turned into key events; otherwise returns \c false. The default is \c true.
*/
bool QGamepadKeyNavigation::active() const { return d->active; }

/*!
    Turns key navigation on or off as \a isActive says. Turning it off releases the keys of the
    buttons that are held.
*/
void QGamepadKeyNavigation::setActive(bool isActive) {
    if (isActive == d->active)
        return;
    if (!isActive)
        d->releaseAll();
    d->active = isActive;
    emit activeChanged(isActive);
}

/*!
    Returns the gamepad whose buttons are turned into keys, or \c nullptr for every gamepad.

    The default is \c nullptr.
*/
QGamepad *QGamepadKeyNavigation::gamepad() const { return d->gamepad; }

/*!
    Makes the buttons of \a gamepad send keys, or of every gamepad if it is \c nullptr.
*/
void QGamepadKeyNavigation::setGamepad(QGamepad *gamepad) {
    if (gamepad == d->gamepad)
        return;
    d->releaseAll();
    disconnect(d->gamepadDestroyed);
    d->gamepad = gamepad;
    if (gamepad)
        d->gamepadDestroyed = connect(gamepad, &QObject::destroyed, this, [this] { setGamepad(nullptr); });
    emit gamepadChanged(gamepad);
}

/*!
    Returns the key sent for \a button.

    \sa setKeyForButton()
*/
Qt::Key QGamepadKeyNavigation::keyForButton(QGamepadManager::GamepadButton button) const {
    if (button < 0 || button >= QtPrivate::GamepadButtonCount)
        return Qt::Key_unknown;
    return d->key(button);
}

/*!
    Makes \a button send \a key; \c Qt::Key_unknown makes it send nothing.

    \sa keyForButton()
*/
void QGamepadKeyNavigation::setKeyForButton(QGamepadManager::GamepadButton button, Qt::Key key) {
    d->setKey(button, key);
}

/*!
    Returns the key sent for up on the directional pad. The default is \c Qt::Key_Up.
*/
Qt::Key QGamepadKeyNavigation::upKey() const { return d->key(QGamepadManager::ButtonUp); }
/*!
    Returns the key sent for down on the directional pad. The default is \c Qt::Key_Down.
*/
Qt::Key QGamepadKeyNavigation::downKey() const { return d->key(QGamepadManager::ButtonDown); }
/*!
    Returns the key sent for left on the directional pad. The default is \c Qt::Key_Left.
*/
Qt::Key QGamepadKeyNavigation::leftKey() const { return d->key(QGamepadManager::ButtonLeft); }
/*!
    Returns the key sent for right on the directional pad. The default is \c Qt::Key_Right.
*/
Qt::Key QGamepadKeyNavigation::rightKey() const { return d->key(QGamepadManager::ButtonRight); }
/*!
    Returns the key sent for the A button. The default is \c Qt::Key_Return.
*/
Qt::Key QGamepadKeyNavigation::buttonAKey() const { return d->key(QGamepadManager::ButtonA); }
/*!
    Returns the key sent for the B button. The default is \c Qt::Key_Back.
*/
Qt::Key QGamepadKeyNavigation::buttonBKey() const { return d->key(QGamepadManager::ButtonB); }
/*!
    Returns the key sent for the X button. The default is \c Qt::Key_unknown, which sends nothing.
*/
Qt::Key QGamepadKeyNavigation::buttonXKey() const { return d->key(QGamepadManager::ButtonX); }
/*!
    Returns the key sent for the Y button. The default is \c Qt::Key_unknown, which sends nothing.
*/
Qt::Key QGamepadKeyNavigation::buttonYKey() const { return d->key(QGamepadManager::ButtonY); }
/*!
    Returns the key sent for the Select button. The default is \c Qt::Key_unknown, which sends nothing.
*/
Qt::Key QGamepadKeyNavigation::buttonSelectKey() const { return d->key(QGamepadManager::ButtonSelect); }
/*!
    Returns the key sent for the Start button. The default is \c Qt::Key_unknown, which sends nothing.
*/
Qt::Key QGamepadKeyNavigation::buttonStartKey() const { return d->key(QGamepadManager::ButtonStart); }
/*!
    Returns the key sent for the Guide button. The default is \c Qt::Key_unknown, which sends nothing.
*/
Qt::Key QGamepadKeyNavigation::buttonGuideKey() const { return d->key(QGamepadManager::ButtonGuide); }
/*!
    Returns the key sent for the left shoulder button. The default is \c Qt::Key_unknown, which sends nothing.
*/
Qt::Key QGamepadKeyNavigation::buttonL1Key() const { return d->key(QGamepadManager::ButtonL1); }
/*!
    Returns the key sent for the right shoulder button. The default is \c Qt::Key_unknown, which sends nothing.
*/
Qt::Key QGamepadKeyNavigation::buttonR1Key() const { return d->key(QGamepadManager::ButtonR1); }
/*!
    Returns the key sent for the left trigger. The default is \c Qt::Key_unknown, which sends nothing.
*/
Qt::Key QGamepadKeyNavigation::buttonL2Key() const { return d->key(QGamepadManager::ButtonL2); }
/*!
    Returns the key sent for the right trigger. The default is \c Qt::Key_unknown, which sends nothing.
*/
Qt::Key QGamepadKeyNavigation::buttonR2Key() const { return d->key(QGamepadManager::ButtonR2); }
/*!
    Returns the key sent for pressing the left stick. The default is \c Qt::Key_unknown, which sends nothing.
*/
Qt::Key QGamepadKeyNavigation::buttonL3Key() const { return d->key(QGamepadManager::ButtonL3); }
/*!
    Returns the key sent for pressing the right stick. The default is \c Qt::Key_unknown, which sends nothing.
*/
Qt::Key QGamepadKeyNavigation::buttonR3Key() const { return d->key(QGamepadManager::ButtonR3); }

/*!
    Sets the key sent for up on the directional pad to \a key.
*/
void QGamepadKeyNavigation::setUpKey(Qt::Key key) { d->setKey(QGamepadManager::ButtonUp, key); }
/*!
    Sets the key sent for down on the directional pad to \a key.
*/
void QGamepadKeyNavigation::setDownKey(Qt::Key key) { d->setKey(QGamepadManager::ButtonDown, key); }
/*!
    Sets the key sent for left on the directional pad to \a key.
*/
void QGamepadKeyNavigation::setLeftKey(Qt::Key key) { d->setKey(QGamepadManager::ButtonLeft, key); }
/*!
    Sets the key sent for right on the directional pad to \a key.
*/
void QGamepadKeyNavigation::setRightKey(Qt::Key key) { d->setKey(QGamepadManager::ButtonRight, key); }
/*!
    Sets the key sent for the A button to \a key.
*/
void QGamepadKeyNavigation::setButtonAKey(Qt::Key key) { d->setKey(QGamepadManager::ButtonA, key); }
/*!
    Sets the key sent for the B button to \a key.
*/
void QGamepadKeyNavigation::setButtonBKey(Qt::Key key) { d->setKey(QGamepadManager::ButtonB, key); }
/*!
    Sets the key sent for the X button to \a key.
*/
void QGamepadKeyNavigation::setButtonXKey(Qt::Key key) { d->setKey(QGamepadManager::ButtonX, key); }
/*!
    Sets the key sent for the Y button to \a key.
*/
void QGamepadKeyNavigation::setButtonYKey(Qt::Key key) { d->setKey(QGamepadManager::ButtonY, key); }
/*!
    Sets the key sent for the Select button to \a key.
*/
void QGamepadKeyNavigation::setButtonSelectKey(Qt::Key key) { d->setKey(QGamepadManager::ButtonSelect, key); }
/*!
    Sets the key sent for the Start button to \a key.
*/
void QGamepadKeyNavigation::setButtonStartKey(Qt::Key key) { d->setKey(QGamepadManager::ButtonStart, key); }
/*!
    Sets the key sent for the Guide button to \a key.
*/
void QGamepadKeyNavigation::setButtonGuideKey(Qt::Key key) { d->setKey(QGamepadManager::ButtonGuide, key); }
/*!
    Sets the key sent for the left shoulder button to \a key.
*/
void QGamepadKeyNavigation::setButtonL1Key(Qt::Key key) { d->setKey(QGamepadManager::ButtonL1, key); }
/*!
    Sets the key sent for the right shoulder button to \a key.
*/
void QGamepadKeyNavigation::setButtonR1Key(Qt::Key key) { d->setKey(QGamepadManager::ButtonR1, key); }
/*!
    Sets the key sent for the left trigger to \a key.
*/
void QGamepadKeyNavigation::setButtonL2Key(Qt::Key key) { d->setKey(QGamepadManager::ButtonL2, key); }
/*!
    Sets the key sent for the right trigger to \a key.
*/
void QGamepadKeyNavigation::setButtonR2Key(Qt::Key key) { d->setKey(QGamepadManager::ButtonR2, key); }
/*!
    Sets the key sent for pressing the left stick to \a key.
*/
void QGamepadKeyNavigation::setButtonL3Key(Qt::Key key) { d->setKey(QGamepadManager::ButtonL3, key); }
/*!
    Sets the key sent for pressing the right stick to \a key.
*/
void QGamepadKeyNavigation::setButtonR3Key(Qt::Key key) { d->setKey(QGamepadManager::ButtonR3, key); }

/*!
    \since 6.0

    Returns \c true if a held button sends its key again and again, as a held key does; otherwise returns \c false.

    The first repeat comes after autoRepeatDelay milliseconds, the next ones every
    autoRepeatInterval milliseconds, as key presses whose \c isAutoRepeat() is \c true. Only the
    button pressed last repeats. The default is \c false.
*/
bool QGamepadKeyNavigation::autoRepeat() const { return d->autoRepeat; }

/*!
    \since 6.0

    Sets whether a held button sends its key again and again, as a held key does to \a enable.
*/
void QGamepadKeyNavigation::setAutoRepeat(bool enable) {
    d->autoRepeat = enable;
    if (!enable) {
        d->repeatTimer.stop();
        d->repeating = QGamepadManager::ButtonInvalid;
    }
}

/*!
    \since 6.0

    Returns the time in milliseconds before a held button first repeats. The default is 400.
*/
int QGamepadKeyNavigation::autoRepeatDelay() const { return d->repeatDelay; }
/*!
    \since 6.0

    Sets the time in milliseconds before a held button first repeats to \a msecs.
*/
void QGamepadKeyNavigation::setAutoRepeatDelay(int msecs) { d->repeatDelay = std::max(1, msecs); }
/*!
    \since 6.0

    Returns the time in milliseconds between repeats of a held button. The default is 100.
*/
int QGamepadKeyNavigation::autoRepeatInterval() const { return d->repeatInterval; }
/*!
    \since 6.0

    Sets the time in milliseconds between repeats of a held button to \a msecs.
*/
void QGamepadKeyNavigation::setAutoRepeatInterval(int msecs) { d->repeatInterval = std::max(1, msecs); }

#include "moc_qgamepadkeynavigation.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
