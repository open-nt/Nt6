// MIT License - see LICENSE
#include <QtWidgets/qtextcursor.h>

#include <QtWidgets/qplaintextedit.h>

#include <algorithm>

/*!
    \class QTextCursor
    \brief The QTextCursor class is a position in the text of a QPlainTextEdit, with a selection, which can move and change the
    text.

    A cursor has a position, and an anchor: the selection is the text between them. Get one from
    QPlainTextEdit::textCursor(), change it, and give it back with QPlainTextEdit::setTextCursor():

    \code
    QTextCursor cursor = edit->textCursor();
    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.insertText(" (done)");
    cursor.select(QTextCursor::BlockUnderCursor);
    edit->setTextCursor(cursor);
    \endcode

    Positions count the characters from the start of the text; a new line counts as one. Changing the text through a
    cursor changes the text of the editor at once, and the cursors that are after the change follow the text.

    \differences
    The cursor belongs to a QPlainTextEdit, not to a \c QTextDocument, so it is in QtWidgets. There are no frames, tables,
    lists or formats (\c charFormat(), \c insertHtml(), \c insertFragment(), \c mergeCharFormat()), no \c joinPreviousEditBlock()
    and no \c visualNavigation(). \c selectedText() separates blocks with a new line and not with the paragraph separator.
    \enddifferences

    \sa QPlainTextEdit
*/

/*!
    \enum QTextCursor::MoveMode

    This enum says what happens to the anchor when the cursor moves.

    \value MoveAnchor The anchor moves with the cursor: there is no selection.
    \value KeepAnchor The anchor stays, so the text that the cursor passed is selected.
*/

/*!
    \enum QTextCursor::MoveOperation

    This enum says where movePosition() moves the cursor.

    \value NoMove It stays.
    \value Start The start of the text.
    \value Up One line up, in the same column if it can.
    \value StartOfLine The start of the line that is shown, which is a part of a block if the text wraps.
    \value StartOfBlock The start of the block: of the text up to a new line.
    \value StartOfWord The start of the word.
    \value PreviousBlock The start of the previous block.
    \value PreviousCharacter The character before.
    \value PreviousWord The start of the word before.
    \value Left One character to the left.
    \value WordLeft The start of the word before.
    \value End The end of the text.
    \value Down One line down, in the same column if it can.
    \value EndOfLine The end of the line that is shown.
    \value EndOfWord The end of the word.
    \value EndOfBlock The end of the block.
    \value NextBlock The start of the next block.
    \value NextCharacter The next character.
    \value NextWord The start of the next word.
    \value Right One character to the right.
    \value WordRight The start of the next word.
*/

/*!
    \enum QTextCursor::SelectionType

    This enum says what select() selects.

    \value WordUnderCursor The word at the cursor.
    \value LineUnderCursor The line at the cursor, as it is shown.
    \value BlockUnderCursor The block at the cursor.
    \value Document All the text.
*/

/*!
    \fn QTextCursor::QTextCursor()

    Constructs a null cursor, which is in no editor.
*/

QTextCursor::QTextCursor(QPlainTextEdit *edit, int position, int anchor)
    : m_edit(edit), m_position(position), m_anchor(anchor) {}

namespace {
QPlainTextEdit *editorOf(const QPointer<QObject> &p) { return static_cast<QPlainTextEdit *>(p.data()); }
} // namespace

/*!
    \fn bool QTextCursor::isNull() const

    Returns \c true if the cursor is in no editor, or the editor was deleted.
*/

/*!
    \fn int QTextCursor::position() const

    Returns the position of the cursor, which is the end of the selection that moves.

    \sa anchor(), setPosition()
*/

/*!
    \fn int QTextCursor::anchor() const

    Returns the position of the other end of the selection, which is the position if there is no selection.

    \sa position()
*/

/*!
    Moves the cursor to \a pos, which is brought into the text. The anchor moves too if \a mode is \c MoveAnchor, and
    otherwise the text between it and the new position is selected.

    \sa position(), movePosition()
*/
void QTextCursor::setPosition(int pos, MoveMode mode) {
    QPlainTextEdit *edit = editorOf(m_edit);
    if (!edit)
        return;
    m_position = std::clamp(pos, 0, static_cast<int>(edit->m_text.size()));
    if (mode == MoveAnchor)
        m_anchor = m_position;
}

/*!
    Moves the cursor by \a operation, \a n times for the operations that can repeat. Returns \c true if the cursor moved.
    With \c KeepAnchor for \a mode, the text that it passes is selected.

    \sa setPosition()
*/
bool QTextCursor::movePosition(MoveOperation operation, MoveMode mode, int n) {
    QPlainTextEdit *edit = editorOf(m_edit);
    if (!edit)
        return false;
    int goal = -1;
    const int target = edit->moved(m_position, operation, std::max(1, n), &goal);
    const bool changed = target != m_position;
    m_position = target;
    if (mode == MoveAnchor)
        m_anchor = m_position;
    return changed;
}

/*!
    Returns \c true if the cursor is at the start of the text.
*/
bool QTextCursor::atStart() const { return m_position == 0; }

/*!
    Returns \c true if the cursor is at the end of the text.
*/
bool QTextCursor::atEnd() const {
    const QPlainTextEdit *edit = editorOf(m_edit);
    return edit && m_position == static_cast<int>(edit->m_text.size());
}

/*!
    Returns \c true if the cursor is at the start of a block.
*/
bool QTextCursor::atBlockStart() const { return columnNumber() == 0; }

/*!
    Returns \c true if the cursor is at the end of a block.
*/
bool QTextCursor::atBlockEnd() const {
    const QPlainTextEdit *edit = editorOf(m_edit);
    return edit && m_position == edit->blockEnd(edit->blockOf(m_position));
}

/*!
    Returns the number of the block that the cursor is in, from 0.

    \sa columnNumber()
*/
int QTextCursor::blockNumber() const {
    const QPlainTextEdit *edit = editorOf(m_edit);
    return edit ? edit->blockOf(m_position) : 0;
}

/*!
    Returns the number of characters between the start of the block and the cursor.

    \sa blockNumber()
*/
int QTextCursor::columnNumber() const {
    const QPlainTextEdit *edit = editorOf(m_edit);
    return edit ? m_position - edit->blockStart(edit->blockOf(m_position)) : 0;
}

/*!
    \fn int QTextCursor::positionInBlock() const

    Returns the same as columnNumber().
*/

/*!
    \fn bool QTextCursor::hasSelection() const

    Returns \c true if the cursor has selected text.

    \sa selectedText()
*/

/*!
    \fn int QTextCursor::selectionStart() const

    Returns the start of the selection: the smaller of the position and the anchor.
*/

/*!
    \fn int QTextCursor::selectionEnd() const

    Returns the end of the selection: the larger of the position and the anchor.
*/

/*!
    Returns the selected text. A selection of several blocks has a new line between them.

    \sa hasSelection()
*/
QString QTextCursor::selectedText() const {
    const QPlainTextEdit *edit = editorOf(m_edit);
    return edit ? edit->m_text.mid(selectionStart(), selectionEnd() - selectionStart()) : QString();
}

/*!
    Removes the selection, leaving the cursor where it is.

    \sa select()
*/
void QTextCursor::clearSelection() { m_anchor = m_position; }

/*!
    Selects the text that \a selection says: the cursor is at the end of it, and the anchor at the start.
*/
void QTextCursor::select(SelectionType selection) {
    QPlainTextEdit *edit = editorOf(m_edit);
    if (!edit)
        return;
    int start = m_position, end = m_position;
    switch (selection) {
    case WordUnderCursor:
        start = edit->wordStart(m_position);
        end = edit->wordEnd(m_position);
        break;
    case LineUnderCursor: {
        const int line = edit->vlineOf(m_position);
        start = edit->m_lines[static_cast<std::size_t>(line)].start;
        end = start + edit->m_lines[static_cast<std::size_t>(line)].length;
        break;
    }
    case BlockUnderCursor:
        start = edit->blockStart(edit->blockOf(m_position));
        end = edit->blockEnd(edit->blockOf(m_position));
        break;
    case Document:
        start = 0;
        end = static_cast<int>(edit->m_text.size());
        break;
    }
    m_anchor = start;
    m_position = end;
}

/*!
    Replaces the selection with \a text, or inserts it at the cursor if there is none. The cursor is after the text.

    \sa removeSelectedText()
*/
void QTextCursor::insertText(const QString &text) {
    QPlainTextEdit *edit = editorOf(m_edit);
    if (!edit)
        return;
    const int from = selectionStart(), to = selectionEnd();
    edit->applyEdit(from, to, text);
    m_position = m_anchor = from + static_cast<int>(text.size());
}

/*!
    Starts a new block at the cursor, by inserting a new line.

    \sa insertText()
*/
void QTextCursor::insertBlock() { insertText(QString("\n")); }

/*!
    Removes the selected text, if there is some.

    \sa deleteChar()
*/
void QTextCursor::removeSelectedText() {
    QPlainTextEdit *edit = editorOf(m_edit);
    if (!edit || !hasSelection())
        return;
    const int from = selectionStart(), to = selectionEnd();
    edit->applyEdit(from, to, QString());
    m_position = m_anchor = from;
}

/*!
    Removes the selected text, or the character after the cursor if nothing is selected.

    \sa deletePreviousChar()
*/
void QTextCursor::deleteChar() {
    QPlainTextEdit *edit = editorOf(m_edit);
    if (!edit)
        return;
    if (hasSelection()) {
        removeSelectedText();
    } else if (m_position < static_cast<int>(edit->m_text.size())) {
        edit->applyEdit(m_position, m_position + 1, QString());
        m_anchor = m_position;
    }
}

/*!
    Removes the selected text, or the character before the cursor if nothing is selected.

    \sa deleteChar()
*/
void QTextCursor::deletePreviousChar() {
    QPlainTextEdit *edit = editorOf(m_edit);
    if (!edit)
        return;
    if (hasSelection()) {
        removeSelectedText();
    } else if (m_position > 0) {
        edit->applyEdit(m_position - 1, m_position, QString());
        m_position = m_anchor = m_position - 1;
    }
}

/*!
    Starts a group of changes that undo() undoes as one, until the matching endEditBlock(). The groups can be nested.

    \sa endEditBlock()
*/
void QTextCursor::beginEditBlock() {
    QPlainTextEdit *edit = editorOf(m_edit);
    if (!edit)
        return;
    if (edit->m_editBlock == 0)
        edit->pushUndo();
    ++edit->m_editBlock;
}

/*!
    Ends the group of changes that beginEditBlock() started.

    \sa beginEditBlock()
*/
void QTextCursor::endEditBlock() {
    QPlainTextEdit *edit = editorOf(m_edit);
    if (edit && edit->m_editBlock > 0)
        --edit->m_editBlock;
}

/*!
    \fn bool QTextCursor::operator==(const QTextCursor &other) const

    Returns \c true if this cursor and \a other are in the same editor, at the same position and with the same anchor.
*/

/*!
    \fn bool QTextCursor::operator!=(const QTextCursor &other) const

    Returns \c true if this cursor and \a other are not equal.
*/
