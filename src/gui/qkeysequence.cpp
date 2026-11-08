// MIT License - see LICENSE
#include <QtGui/qkeysequence.h>

namespace {

struct KeyName {
    int key;
    const char *name;
};

const KeyName kNames[] = {
    {Qt::Key_Escape, "Esc"},      {Qt::Key_Tab, "Tab"},      {Qt::Key_Backtab, "Backtab"}, {Qt::Key_Backspace, "Backspace"},
    {Qt::Key_Return, "Return"},   {Qt::Key_Enter, "Enter"},  {Qt::Key_Insert, "Ins"},      {Qt::Key_Delete, "Del"},
    {Qt::Key_Pause, "Pause"},     {Qt::Key_Print, "Print"},  {Qt::Key_Home, "Home"},       {Qt::Key_End, "End"},
    {Qt::Key_Left, "Left"},       {Qt::Key_Up, "Up"},        {Qt::Key_Right, "Right"},     {Qt::Key_Down, "Down"},
    {Qt::Key_PageUp, "PgUp"},     {Qt::Key_PageDown, "PgDown"}, {Qt::Key_Space, "Space"},  {Qt::Key_Menu, "Menu"},
    {Qt::Key_CapsLock, "CapsLock"}, {Qt::Key_NumLock, "NumLock"}, {Qt::Key_ScrollLock, "ScrollLock"},
};

struct ModifierName {
    int modifier;
    const char *name;
};

// The order in which the modifiers are written
const ModifierName kModifiers[] = {{Qt::META, "Meta"}, {Qt::CTRL, "Ctrl"}, {Qt::ALT, "Alt"}, {Qt::SHIFT, "Shift"}};

QString keyName(int key) {
    for (const KeyName &entry : kNames)
        if (entry.key == key)
            return QString::fromLatin1(entry.name);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F12)
        return QString("F%1").arg(key - Qt::Key_F1 + 1);
    if (key > Qt::Key_Space && key < 0x7f)
        return QString(QChar(static_cast<char16_t>(key)));
    return QString();
}

int keyFromName(const QString &name) {
    for (const KeyName &entry : kNames)
        if (name.compare(QString::fromLatin1(entry.name), Qt::CaseInsensitive) == 0)
            return entry.key;
    if (name.size() >= 2 && (name.at(0) == 'F' || name.at(0) == 'f')) {
        bool ok = false;
        const int number = name.mid(1).toInt(&ok);
        if (ok && number >= 1 && number <= 12)
            return Qt::Key_F1 + number - 1;
    }
    if (name.size() == 1) {
        const char16_t c = name.at(0).toUpper().unicode();
        if (c > ' ' && c < 0x7f)
            return static_cast<int>(c);
    }
    return 0;
}

} // namespace

/*!
    \class QKeySequence
    \brief The QKeySequence class encapsulates a key combination used by a shortcut.

    A key sequence is a key with modifiers, such as Ctrl+S. It is written as a string, \c{"Ctrl+Shift+S"}, as a number,
    \c{Qt::CTRL | Qt::SHIFT | Qt::Key_S}, or as one of the standard keys that a platform has for an action:

    \code
    QAction *save = new QAction("Save", this);
    save->setShortcut(QKeySequence::Save);
    QAction *quit = new QAction("Quit", this);
    quit->setShortcut(QKeySequence("Ctrl+Q"));
    \endcode

    \differences
    A sequence is a single key with its modifiers: there are no sequences of several keys such as \c{"Ctrl+X, Ctrl+C"},
    so count() is 0 or 1 and matches() never returns \c PartialMatch. The key names are in English, and the list of
    standard keys is shorter and has the bindings of Linux only (\c keyBindings() has one binding for each key).
    \enddifferences

    \sa QAction::setShortcut()
*/

/*!
    \enum QKeySequence::StandardKey

    This enum names the key combinations that a platform has for common actions.

    \value UnknownKey No key.
    \value HelpContents Open the help: F1.
    \value Open Open a document: Ctrl+O.
    \value Close Close a document: Ctrl+W.
    \value Save Save a document: Ctrl+S.
    \value New Create a document: Ctrl+N.
    \value Delete Delete the selection: Del.
    \value Cut Cut the selection: Ctrl+X.
    \value Copy Copy the selection: Ctrl+C.
    \value Paste Paste from the clipboard: Ctrl+V.
    \value Undo Undo the last action: Ctrl+Z.
    \value Redo Redo the last undone action: Ctrl+Shift+Z.
    \value Back Go back: Alt+Left.
    \value Forward Go forward: Alt+Right.
    \value Refresh Refresh the view: F5.
    \value ZoomIn Zoom in: Ctrl++.
    \value ZoomOut Zoom out: Ctrl+-.
    \value Print Print: Ctrl+P.
    \value Find Find: Ctrl+F.
    \value FindNext Find the next match: F3.
    \value FindPrevious Find the previous match: Shift+F3.
    \value Replace Find and replace: Ctrl+H.
    \value SelectAll Select everything: Ctrl+A.
    \value Quit Quit the application: Ctrl+Q.
    \value SaveAs Save a document with another name: Ctrl+Shift+S.
    \value Cancel Cancel the operation: Esc.
    \value FullScreen Switch to full screen: F11.
    \value NextChild Go to the next tab or window: Ctrl+Tab.
    \value PreviousChild Go to the previous tab or window: Ctrl+Shift+Backtab.
*/

/*!
    \enum QKeySequence::SequenceFormat

    This enum says how a key sequence is written as a string.

    \value NativeText The way the platform writes it. On Linux this is the same as \c PortableText.
    \value PortableText The English names, such as \c{"Ctrl+Shift+S"}, that do not depend on the platform.
*/

/*!
    \enum QKeySequence::SequenceMatch

    This enum says how much of a sequence another one has.

    \value NoMatch The sequences are different.
    \value PartialMatch The first one is the start of the second one; this never happens, as a sequence is one key.
    \value ExactMatch The sequences are the same.
*/

/*!
    \fn QKeySequence::QKeySequence()

    Constructs an empty key sequence.
*/

/*!
    Constructs a key sequence from the string \a key, such as \c{"Ctrl+O"}, in the given \a format. The sequence is empty
    if the string is not a key combination.

    \sa fromString()
*/
QKeySequence::QKeySequence(const QString &key, SequenceFormat format) { *this = fromString(key, format); }

/*!
    Constructs a key sequence from \a k1, a key with its modifiers, as in \c{Qt::CTRL | Qt::Key_O}.
*/
QKeySequence::QKeySequence(int k1) : m_key(k1 == Qt::Key_unknown ? 0 : k1) {}

/*!
    Constructs the key sequence that the platform has for the standard key \a key; it is empty for \c UnknownKey.

    \sa keyBindings()
*/
QKeySequence::QKeySequence(StandardKey key) {
    const QList<QKeySequence> bindings = keyBindings(key);
    if (!bindings.isEmpty())
        *this = bindings.first();
}

/*!
    \fn int QKeySequence::count() const

    Returns the number of keys in the sequence: 1, or 0 if it is empty.
*/

/*!
    \fn bool QKeySequence::isEmpty() const

    Returns \c true if the sequence has no key; otherwise returns \c false.
*/

/*!
    Returns the sequence as a string in the given \a format, such as \c{"Ctrl+Shift+S"}, or an empty string for an empty
    sequence.

    \sa fromString()
*/
QString QKeySequence::toString(SequenceFormat format) const {
    (void)format;
    const QString name = keyName(m_key & 0x01ffffff);
    if (m_key == 0 || name.isEmpty())
        return QString();
    QString text;
    for (const ModifierName &modifier : kModifiers)
        if (m_key & modifier.modifier)
            text += QString::fromLatin1(modifier.name) + '+';
    return text + name;
}

/*!
    Returns the key sequence that the string \a str has, in the given \a format. The modifiers \c Ctrl, \c Shift, \c Alt
    and \c Meta, in any case and in any order, are joined to a key by \c{+}. The key is a character, \c F1 to \c F12, or a
    name such as \c Esc, \c Del, \c Return, \c PgUp or \c Space. An empty sequence is returned for a string that is not a
    key combination.

    \sa toString()
*/
QKeySequence QKeySequence::fromString(const QString &str, SequenceFormat format) {
    (void)format;
    QString rest = str.trimmed();
    int modifiers = 0;
    for (;;) {
        bool found = false;
        for (const ModifierName &modifier : kModifiers) {
            const QString prefix = QString::fromLatin1(modifier.name) + '+';
            if (rest.size() > prefix.size() && rest.left(prefix.size()).compare(prefix, Qt::CaseInsensitive) == 0) {
                modifiers |= modifier.modifier;
                rest = rest.mid(prefix.size()).trimmed();
                found = true;
                break;
            }
        }
        if (!found)
            break;
    }
    const int key = keyFromName(rest);
    return key == 0 ? QKeySequence() : QKeySequence(key | modifiers);
}

/*!
    Returns \c ExactMatch if \a seq is the same sequence as this one, and \c NoMatch if it is not; two empty sequences
    do not match.
*/
QKeySequence::SequenceMatch QKeySequence::matches(const QKeySequence &seq) const {
    return m_key != 0 && m_key == seq.m_key ? ExactMatch : NoMatch;
}

/*!
    Returns the key sequences that the platform has for the standard key \a key: one on Linux, and none for \c UnknownKey.
*/
QList<QKeySequence> QKeySequence::keyBindings(StandardKey key) {
    // The modifiers and a key, as ints: they are two enums
    auto with = [](int modifiers, int keyCode) { return modifiers | keyCode; };
    const int Ctrl = Qt::CTRL, Shift = Qt::SHIFT, Alt = Qt::ALT;
    int code = 0;
    switch (key) {
    case HelpContents: code = Qt::Key_F1; break;
    case Open: code = with(Ctrl, Qt::Key_O); break;
    case Close: code = with(Ctrl, Qt::Key_W); break;
    case Save: code = with(Ctrl, Qt::Key_S); break;
    case New: code = with(Ctrl, Qt::Key_N); break;
    case Delete: code = Qt::Key_Delete; break;
    case Cut: code = with(Ctrl, Qt::Key_X); break;
    case Copy: code = with(Ctrl, Qt::Key_C); break;
    case Paste: code = with(Ctrl, Qt::Key_V); break;
    case Undo: code = with(Ctrl, Qt::Key_Z); break;
    case Redo: code = with(Ctrl | Shift, Qt::Key_Z); break;
    case Back: code = with(Alt, Qt::Key_Left); break;
    case Forward: code = with(Alt, Qt::Key_Right); break;
    case Refresh: code = Qt::Key_F5; break;
    case ZoomIn: code = with(Ctrl, Qt::Key_Plus); break;
    case ZoomOut: code = with(Ctrl, Qt::Key_Minus); break;
    case Print: code = with(Ctrl, Qt::Key_P); break;
    case Find: code = with(Ctrl, Qt::Key_F); break;
    case FindNext: code = Qt::Key_F3; break;
    case FindPrevious: code = with(Shift, Qt::Key_F3); break;
    case Replace: code = with(Ctrl, Qt::Key_H); break;
    case SelectAll: code = with(Ctrl, Qt::Key_A); break;
    case Quit: code = with(Ctrl, Qt::Key_Q); break;
    case SaveAs: code = with(Ctrl | Shift, Qt::Key_S); break;
    case Cancel: code = Qt::Key_Escape; break;
    case FullScreen: code = Qt::Key_F11; break;
    case NextChild: code = with(Ctrl, Qt::Key_Tab); break;
    case PreviousChild: code = with(Ctrl | Shift, Qt::Key_Backtab); break;
    case UnknownKey: break;
    }
    QList<QKeySequence> list;
    if (code != 0)
        list.append(QKeySequence(code));
    return list;
}

/*!
    Returns the mnemonic of \a text as a key sequence: Alt and the letter that follows the first \c{&}, as \c Alt+F for
    \c{"&File"}. A \c{&&} is an ampersand and not a mnemonic. The sequence is empty if there is no mnemonic.
*/
QKeySequence QKeySequence::mnemonic(const QString &text) {
    for (qsizetype i = 0; i + 1 < text.size(); ++i) {
        if (text.at(i) != '&')
            continue;
        if (text.at(i + 1) == '&') {
            ++i;
            continue;
        }
        const QChar c = text.at(i + 1).toUpper();
        if (c.unicode() > ' ' && c.unicode() < 0x7f)
            return QKeySequence(Qt::ALT | static_cast<int>(c.unicode()));
        return QKeySequence();
    }
    return QKeySequence();
}

/*!
    \fn int QKeySequence::operator[](uint index) const

    Returns the key with its modifiers at \a index, which is 0 for the one key that there is; 0 for any other index.
*/

/*!
    \fn bool QKeySequence::operator==(const QKeySequence &other) const

    Returns \c true if this sequence and \a other are the same key combination; otherwise returns \c false.
*/

/*!
    \fn bool QKeySequence::operator!=(const QKeySequence &other) const

    Returns \c true if this sequence and \a other are different key combinations; otherwise returns \c false.
*/

/*!
    \fn bool QKeySequence::operator<(const QKeySequence &other) const

    Returns \c true if this sequence sorts before \a other, which has no meaning but an order; otherwise returns
    \c false.
*/
