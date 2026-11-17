// MIT License - see LICENSE
#include <QtGui/qshortcut.h>

#include "qshortcut_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtGui/qevent.h>
#include <QtGui/qwindow.h>

#include <algorithm>
#include <vector>

namespace {

std::vector<QShortcut *> &shortcuts() {
    static auto *list = new std::vector<QShortcut *>;
    return *list;
}

// The window that `object`, or one of its parents, is; null if there is none.
QWindow *windowOf(QObject *object) {
    for (QObject *o = object; o; o = o->parent()) {
        if (auto *w = qobject_cast<QWindow *>(o))
            return w;
    }
    return nullptr;
}

bool defaultMatcher(QObject *parent, Qt::ShortcutContext context, QWindow *window) {
    if (context == Qt::ApplicationShortcut)
        return true;
    return windowOf(parent) == window;
}

QtPrivate::ShortcutContextMatcher g_matcher = defaultMatcher;

// The key and modifiers as a key sequence holds them, without Shift for a symbol: the Shift that makes a + from a =
// is not asked for in a shortcut.
int shortcutCode(int key, Qt::KeyboardModifiers modifiers) {
    int code = key;
    if (modifiers & Qt::ShiftModifier)
        code |= Qt::SHIFT;
    if (modifiers & Qt::ControlModifier)
        code |= Qt::CTRL;
    if (modifiers & Qt::AltModifier)
        code |= Qt::ALT;
    if (modifiers & Qt::MetaModifier)
        code |= Qt::META;
    return code;
}

int normalized(int code) {
    const int key = code & 0x01ffffff;
    if (key >= 'a' && key <= 'z')
        code = (code & ~0x01ffffff) | (key - 0x20);
    const bool symbol = key > Qt::Key_Space && key < 0x7f && !(key >= '0' && key <= '9') &&
                        !(key >= 'A' && key <= 'Z') && !(key >= 'a' && key <= 'z');
    return symbol ? code & ~static_cast<int>(Qt::SHIFT) : code;
}

} // namespace

void QtPrivate::setShortcutContextMatcher(ShortcutContextMatcher matcher) {
    g_matcher = matcher ? matcher : defaultMatcher;
}

bool QtPrivate::tryShortcut(QWindow *window, int key, Qt::KeyboardModifiers modifiers, bool autoRepeat) {
    if (key == 0 || key == Qt::Key_unknown || key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt ||
        key == Qt::Key_Meta)
        return false;
    const int code = normalized(shortcutCode(key, modifiers));
    std::vector<QShortcut *> matches;
    for (QShortcut *s : std::vector<QShortcut *>(shortcuts())) {
        if (!s->isEnabled() || (autoRepeat && !s->autoRepeat()))
            continue;
        bool keyMatches = false;
        for (const QKeySequence &seq : s->keys())
            keyMatches = keyMatches || (!seq.isEmpty() && normalized(seq[0]) == code);
        if (keyMatches && g_matcher(s->parent(), s->context(), window))
            matches.push_back(s);
    }
    if (matches.empty())
        return false;
    // The window, and the widget with the focus in it, may want the key press for itself.
    if (window) {
        QKeyEvent ask(QEvent::ShortcutOverride, key, modifiers, QString(), autoRepeat);
        ask.ignore();
        QCoreApplication::sendEvent(window, &ask);
        if (ask.isAccepted())
            return false;
    }
    if (matches.size() == 1)
        emit matches.front()->activated();
    else
        emit matches.front()->activatedAmbiguously();
    return true;
}

/*!
    \class QShortcut
    \brief The QShortcut class runs code when a key combination is pressed.

    A shortcut belongs to a parent, a window or a widget, and is active while its context() says so: by default
    while the parent is in the active window. When the key is pressed there, activated() is emitted and the key
    press goes no further:

    \code
    auto *help = new QShortcut(QKeySequence(Qt::Key_F1), mainWindow);
    QObject::connect(help, &QShortcut::activated, mainWindow, &MainWindow::showHelp);
    \endcode

    With Nt Widgets, a shortcut whose parent is a widget is active only while the widget is visible and enabled,
    and not blocked by a modal window.

    \differences
    Only the first key of a key sequence counts, as Nt key sequences have one key. When several active shortcuts
    have the same key, the first one made emits activatedAmbiguously(); Qt cycles through them. There is no
    \c QEvent::Shortcut and no \c QShortcutEvent.
    \enddifferences

    \sa QKeySequence, QAction::setShortcut()
*/

/*!
    Constructs a shortcut without a key, with the given \a parent.
*/
QShortcut::QShortcut(QObject *parent) : QObject(parent) { shortcuts().push_back(this); }

/*!
    Constructs a shortcut for \a key with the given \a parent, active in \a context. \a member and
    \a ambiguousMember, if given, are slots of the parent (\c SLOT() strings) connected to activated() and
    activatedAmbiguously().
*/
QShortcut::QShortcut(const QKeySequence &key, QObject *parent, const char *member, const char *ambiguousMember,
                     Qt::ShortcutContext context)
    : QObject(parent), m_context(context) {
    if (!key.isEmpty())
        m_keys.append(key);
    shortcuts().push_back(this);
    if (member && parent)
        QObject::connect(this, "2activated()", parent, member);
    if (ambiguousMember && parent)
        QObject::connect(this, "2activatedAmbiguously()", parent, ambiguousMember);
}

/*!
    Constructs a shortcut for the keys of the standard \a key with the given \a parent, active in \a context, with
    \a member and \a ambiguousMember as in the other constructor.
*/
QShortcut::QShortcut(QKeySequence::StandardKey key, QObject *parent, const char *member, const char *ambiguousMember,
                     Qt::ShortcutContext context)
    : QShortcut(QKeySequence(), parent, member, ambiguousMember, context) {
    m_keys = QKeySequence::keyBindings(key);
}

/*!
    Destroys the shortcut.
*/
QShortcut::~QShortcut() {
    auto &list = shortcuts();
    list.erase(std::remove(list.begin(), list.end(), this), list.end());
}

/*!
    Sets the key of the shortcut to \a key, replacing the others.

    \sa key(), setKeys()
*/
void QShortcut::setKey(const QKeySequence &key) {
    m_keys.clear();
    if (!key.isEmpty())
        m_keys.append(key);
}

/*!
    Returns the first key of the shortcut, or an empty sequence.
*/
QKeySequence QShortcut::key() const { return m_keys.isEmpty() ? QKeySequence() : m_keys.first(); }

/*!
    Sets the keys of the shortcut to those of the standard \a key on this platform.
*/
void QShortcut::setKeys(QKeySequence::StandardKey key) { m_keys = QKeySequence::keyBindings(key); }

/*!
    \overload

    Sets the keys of the shortcut to \a keys; any of them activates it.
*/
void QShortcut::setKeys(const QList<QKeySequence> &keys) { m_keys = keys; }

/*!
    Returns the keys of the shortcut.
*/
QList<QKeySequence> QShortcut::keys() const { return m_keys; }

/*!
    Sets whether the shortcut is active to \a enable. A disabled shortcut lets its key through to the window.
*/
void QShortcut::setEnabled(bool enable) { m_enabled = enable; }

/*!
    Returns \c true if the shortcut is enabled; the default.
*/
bool QShortcut::isEnabled() const { return m_enabled; }

/*!
    Sets where the shortcut is active to \a context.
*/
void QShortcut::setContext(Qt::ShortcutContext context) { m_context = context; }

/*!
    Returns where the shortcut is active; Qt::WindowShortcut by default.
*/
Qt::ShortcutContext QShortcut::context() const { return m_context; }

/*!
    Sets whether a key held down activates the shortcut again with each repeat to \a on.
*/
void QShortcut::setAutoRepeat(bool on) { m_autoRepeat = on; }

/*!
    Returns \c true if repeats of a held key activate the shortcut; the default.
*/
bool QShortcut::autoRepeat() const { return m_autoRepeat; }

/*!
    Sets the "What's This?" text to \a text. Nt keeps it but shows it nowhere.
*/
void QShortcut::setWhatsThis(const QString &text) { m_whatsThis = text; }

/*!
    Returns the "What's This?" text.
*/
QString QShortcut::whatsThis() const { return m_whatsThis; }

#include "moc_qshortcut.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
