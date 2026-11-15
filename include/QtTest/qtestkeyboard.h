// MIT License - see LICENSE
#pragma once

#include <QtCore/qlogging.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>
#include <QtCore/qtenvironmentvariables.h>
#include <QtGui/qkeysequence.h>
#include <QtGui/qtestsupport_gui.h>
#include <QtGui/qwindow.h>
#include <QtTest/qtest.h>

#ifdef QT_WIDGETS_LIB
#include <QtWidgets/qwidget.h>
#endif

namespace QTest {

/*!
    \enum QTest::KeyAction

    What a key does in keyEvent().

    \value Press The key goes down.
    \value Release The key comes up.
    \value Click The key goes down and comes up.
    \value Shortcut The key is clicked as a shortcut; in Nt the same as Click.
*/
enum KeyAction { Press, Release, Click, Shortcut };

/*!
    Returns the delay, in milliseconds, before each simulated key event when none is given: the value of the
    \c QTEST_KEYEVENT_DELAY environment variable, or 0.
*/
inline int defaultKeyDelay() {
    static const int delay = qEnvironmentVariableIntValue("QTEST_KEYEVENT_DELAY");
    return delay > 0 ? delay : 0;
}

} // namespace QTest

namespace QTestPrivate {

// The Qt::Key of the ASCII character `ascii`: letters give their upper-case key, control characters their key, and
// the other printable characters their own code.
inline int asciiToKey(char ascii) {
    switch (ascii) {
    case '\n':
    case '\r':
        return Qt::Key_Return;
    case '\t':
        return Qt::Key_Tab;
    case '\b':
        return Qt::Key_Backspace;
    case 0x1b:
        return Qt::Key_Escape;
    default:
        break;
    }
    if (ascii >= 'a' && ascii <= 'z')
        return Qt::Key_A + (ascii - 'a');
    if (ascii >= ' ' && ascii <= '~')
        return ascii;
    return Qt::Key_unknown;
}

// The text that `key` types with `modifiers` held: the character of a printable key, upper case with Shift for a
// letter, the control character of Return, Tab, Backspace and Escape, and nothing otherwise.
inline QString keyToText(int key, Qt::KeyboardModifiers modifiers) {
    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return QString(QChar(u'\r'));
    case Qt::Key_Tab:
        return QString(QChar(u'\t'));
    case Qt::Key_Backspace:
        return QString(QChar(u'\b'));
    case Qt::Key_Escape:
        return QString(QChar(char16_t(0x1b)));
    default:
        break;
    }
    if (key < ' ' || key > '~')
        return QString();
    if (key >= 'A' && key <= 'Z' && !(modifiers & Qt::ShiftModifier))
        return QString(QChar(char16_t(key - 'A' + 'a')));
    return QString(QChar(char16_t(key)));
}

// Simulates `action` for `key`, typing `text`, in `window` with `modifiers` held, after `delay` milliseconds (the
// default delay if negative). The modifier keys are pressed before the key and released after it.
inline void keyEventAt(QTest::KeyAction action, QWindow *window, int key, const QString &text,
                       Qt::KeyboardModifiers modifiers, int delay) {
    if (!window) {
        qWarning("QTest::keyEvent(): the window is null");
        return;
    }
    if (delay < 0 || delay < QTest::defaultKeyDelay())
        delay = QTest::defaultKeyDelay();
    if (delay > 0)
        QTest::qWait(delay);
    struct ModifierKey {
        Qt::KeyboardModifier modifier;
        Qt::Key key;
    };
    static constexpr ModifierKey kModifierKeys[] = {{Qt::ShiftModifier, Qt::Key_Shift},
                                                    {Qt::ControlModifier, Qt::Key_Control},
                                                    {Qt::AltModifier, Qt::Key_Alt},
                                                    {Qt::MetaModifier, Qt::Key_Meta}};
    const bool press = action != QTest::Release;
    const bool release = action != QTest::Press;
    if (press) {
        Qt::KeyboardModifiers held;
        for (const ModifierKey &m : kModifierKeys) {
            if (modifiers & m.modifier) {
                held |= m.modifier;
                sendKeyEvent(window, QEvent::KeyPress, m.key, held, QString());
            }
        }
        sendKeyEvent(window, QEvent::KeyPress, key, modifiers, text);
    }
    if (release) {
        sendKeyEvent(window, QEvent::KeyRelease, key, modifiers, text);
        Qt::KeyboardModifiers held = modifiers;
        for (int i = 3; i >= 0; --i) {
            const ModifierKey &m = kModifierKeys[i];
            if (modifiers & m.modifier) {
                held &= ~Qt::KeyboardModifiers(m.modifier);
                sendKeyEvent(window, QEvent::KeyRelease, m.key, held, QString());
            }
        }
    }
}

#ifdef QT_WIDGETS_LIB
// The window of `widget`, or null with a warning that names `function` if it has none.
inline QWindow *windowOfWidget(QWidget *widget, const char *function) {
    if (!widget) {
        qWarning("QTest::%s(): the widget is null", function);
        return nullptr;
    }
    QWindow *window = widget->window()->windowHandle();
    if (!window)
        qWarning("QTest::%s(): the widget has no window; show it first", function);
    return window;
}
#endif

} // namespace QTestPrivate

namespace QTest {

/*!
    Simulates \a action for \a key in \a window with the modifiers \a modifier held, after \a delay
    milliseconds (defaultKeyDelay() if negative). The modifier keys are pressed and released around the key,
    and a printable key types its character.

    \sa keyClick(), keyPress(), keyRelease()
*/
inline void keyEvent(KeyAction action, QWindow *window, Qt::Key key, Qt::KeyboardModifiers modifier = Qt::NoModifier,
                     int delay = -1) {
    QTestPrivate::keyEventAt(action, window, key, QTestPrivate::keyToText(key, modifier), modifier, delay);
}

/*!
    \overload

    Simulates \a action for the key that types \a ascii in \a window, with \a modifier held (and Shift for an
    upper-case letter), after \a delay milliseconds.
*/
inline void keyEvent(KeyAction action, QWindow *window, char ascii, Qt::KeyboardModifiers modifier = Qt::NoModifier,
                     int delay = -1) {
    if (ascii >= 'A' && ascii <= 'Z')
        modifier |= Qt::ShiftModifier;
    QTestPrivate::keyEventAt(action, window, QTestPrivate::asciiToKey(ascii), QString(QChar::fromLatin1(ascii)),
                             modifier, delay);
}

/*!
    Simulates clicking \a key in \a window with \a modifier held, after \a delay milliseconds.

    \sa keyClicks(), keyPress(), keyRelease()
*/
inline void keyClick(QWindow *window, Qt::Key key, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay = -1) {
    keyEvent(Click, window, key, modifier, delay);
}

/*!
    \overload

    Simulates clicking the key that types \a ascii in \a window with \a modifier held, after \a delay
    milliseconds.
*/
inline void keyClick(QWindow *window, char ascii, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay = -1) {
    keyEvent(Click, window, ascii, modifier, delay);
}

/*!
    Simulates pressing \a key in \a window with \a modifier held, after \a delay milliseconds.

    \sa keyRelease(), keyClick()
*/
inline void keyPress(QWindow *window, Qt::Key key, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay = -1) {
    keyEvent(Press, window, key, modifier, delay);
}

/*!
    \overload

    Simulates pressing the key that types \a ascii in \a window with \a modifier held, after \a delay
    milliseconds.
*/
inline void keyPress(QWindow *window, char ascii, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay = -1) {
    keyEvent(Press, window, ascii, modifier, delay);
}

/*!
    Simulates releasing \a key in \a window with \a modifier held, after \a delay milliseconds.

    \sa keyPress(), keyClick()
*/
inline void keyRelease(QWindow *window, Qt::Key key, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay = -1) {
    keyEvent(Release, window, key, modifier, delay);
}

/*!
    \overload

    Simulates releasing the key that types \a ascii in \a window with \a modifier held, after \a delay
    milliseconds.
*/
inline void keyRelease(QWindow *window, char ascii, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay = -1) {
    keyEvent(Release, window, ascii, modifier, delay);
}

/*!
    Simulates typing \a sequence in \a window, one key click per character, with \a modifier held and \a delay
    milliseconds before each click. A character outside ASCII is typed with Qt::Key_unknown as its key.

    \sa keyClick()
*/
inline void keyClicks(QWindow *window, const QString &sequence, Qt::KeyboardModifiers modifier = Qt::NoModifier,
                      int delay = -1) {
    for (qsizetype i = 0; i < sequence.size(); ++i) {
        const QChar ch = sequence.at(i);
        const char ascii = ch.toLatin1();
        Qt::KeyboardModifiers mods = modifier;
        if (ascii >= 'A' && ascii <= 'Z')
            mods |= Qt::ShiftModifier;
        const int key = ascii != '\0' ? QTestPrivate::asciiToKey(ascii) : int(Qt::Key_unknown);
        QTestPrivate::keyEventAt(Click, window, key, QString(ch), mods, delay);
    }
}

/*!
    Simulates clicking the key of \a keySequence, with its modifiers, in \a window.

    \sa keyClick()
*/
inline void keySequence(QWindow *window, const QKeySequence &keySequence) {
    constexpr int kModifierMask =
        Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier | Qt::KeypadModifier;
    for (int i = 0; i < keySequence.count(); ++i) {
        const int code = keySequence[static_cast<uint>(i)];
        keyClick(window, static_cast<Qt::Key>(code & ~kModifierMask),
                 Qt::KeyboardModifiers::fromInt(code & kModifierMask));
    }
}

#ifdef QT_WIDGETS_LIB

/*!
    \overload

    Simulates \a action for \a key in the window of \a widget, with \a modifier held, after \a delay
    milliseconds. The key goes to the widget that has the focus in that window.
*/
inline void keyEvent(KeyAction action, QWidget *widget, Qt::Key key, Qt::KeyboardModifiers modifier = Qt::NoModifier,
                     int delay = -1) {
    if (QWindow *window = QTestPrivate::windowOfWidget(widget, "keyEvent"))
        keyEvent(action, window, key, modifier, delay);
}

/*!
    \overload

    Simulates \a action for the key that types \a ascii in the window of \a widget, with \a modifier held,
    after \a delay milliseconds.
*/
inline void keyEvent(KeyAction action, QWidget *widget, char ascii, Qt::KeyboardModifiers modifier = Qt::NoModifier,
                     int delay = -1) {
    if (QWindow *window = QTestPrivate::windowOfWidget(widget, "keyEvent"))
        keyEvent(action, window, ascii, modifier, delay);
}

/*!
    \overload

    Simulates clicking \a key in the window of \a widget with \a modifier held, after \a delay milliseconds.
*/
inline void keyClick(QWidget *widget, Qt::Key key, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay = -1) {
    keyEvent(Click, widget, key, modifier, delay);
}

/*!
    \overload

    Simulates clicking the key that types \a ascii in the window of \a widget with \a modifier held, after
    \a delay milliseconds.
*/
inline void keyClick(QWidget *widget, char ascii, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay = -1) {
    keyEvent(Click, widget, ascii, modifier, delay);
}

/*!
    \overload

    Simulates pressing \a key in the window of \a widget with \a modifier held, after \a delay milliseconds.
*/
inline void keyPress(QWidget *widget, Qt::Key key, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay = -1) {
    keyEvent(Press, widget, key, modifier, delay);
}

/*!
    \overload

    Simulates pressing the key that types \a ascii in the window of \a widget with \a modifier held, after
    \a delay milliseconds.
*/
inline void keyPress(QWidget *widget, char ascii, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay = -1) {
    keyEvent(Press, widget, ascii, modifier, delay);
}

/*!
    \overload

    Simulates releasing \a key in the window of \a widget with \a modifier held, after \a delay milliseconds.
*/
inline void keyRelease(QWidget *widget, Qt::Key key, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay = -1) {
    keyEvent(Release, widget, key, modifier, delay);
}

/*!
    \overload

    Simulates releasing the key that types \a ascii in the window of \a widget with \a modifier held, after
    \a delay milliseconds.
*/
inline void keyRelease(QWidget *widget, char ascii, Qt::KeyboardModifiers modifier = Qt::NoModifier, int delay = -1) {
    keyEvent(Release, widget, ascii, modifier, delay);
}

/*!
    \overload

    Simulates typing \a sequence in the window of \a widget, with \a modifier held and \a delay milliseconds
    before each key.
*/
inline void keyClicks(QWidget *widget, const QString &sequence, Qt::KeyboardModifiers modifier = Qt::NoModifier,
                      int delay = -1) {
    if (QWindow *window = QTestPrivate::windowOfWidget(widget, "keyClicks"))
        keyClicks(window, sequence, modifier, delay);
}

/*!
    \overload

    Simulates clicking the key of \a keySequence, with its modifiers, in the window of \a widget.
*/
inline void keySequence(QWidget *widget, const QKeySequence &keySequence) {
    if (QWindow *window = QTestPrivate::windowOfWidget(widget, "keySequence"))
        QTest::keySequence(window, keySequence);
}

#endif // QT_WIDGETS_LIB

} // namespace QTest
