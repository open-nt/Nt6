// MIT License - see LICENSE
#include <QtWidgets/qplaintextedit.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qscrollbar.h>

#include <algorithm>

namespace {

bool isWordChar(QChar c) { return c.isLetterOrNumber() || c == QChar('_'); }

} // namespace

/*!
    \class QPlainTextEdit
    \brief The QPlainTextEdit class is an editor for text of many lines.

    The user types, moves the cursor with the keys or the mouse, selects, and undoes. Long lines are wrapped at the width of
    the widget unless the wrap mode is changed. Read and set the text with toPlainText() and setPlainText(), and
    connect to textChanged():

    \code
    QPlainTextEdit *edit = new QPlainTextEdit(window);
    edit->setPlaceholderText("Write something");
    edit->appendPlainText("A first line.");
    connect(edit, &QPlainTextEdit::textChanged, [edit] { qDebug() << edit->toPlainText().size(); });
    \endcode

    The cursor is a QTextCursor, which can move through the text and change it. A log is a read-only editor to which
    lines are appended with appendPlainText(); it scrolls to follow the new text if it was at the bottom.

    A text edit with a selection, one with a placeholder and one that is read-only.

    \image widgets/qplaintextedit.png QPlainTextEdit in the light color scheme

    \differences
    The text has no formatting, no fonts or colors of parts of it, and there is no \c QTextDocument: the text is one
    string, and positions are the number of characters from its start, a new line counting as one. The clipboard is not there
    yet, so there are no \c cut(), \c copy() or \c paste(), and no drag and drop. There is no \c find(), no
    \c maximumBlockCount, no \c setCursorWidth(), and no context menu. The cursor does not blink. A selection that has several
    lines returns them separated by a new line from QTextCursor::selectedText(), where Qt uses the paragraph separator.
    \enddifferences

    \sa QTextEdit, QTextCursor, QLineEdit
*/

/*!
    \enum QPlainTextEdit::LineWrapMode

    This enum says what happens to a line that is longer than the widget.

    \value NoWrap The line goes on, and the editor has a horizontal scroll bar.
    \value WidgetWidth The line is broken at the width of the widget, at a space if there is one.
*/

/*!
    Constructs an empty editor with the given \a parent.
*/
QPlainTextEdit::QPlainTextEdit(QWidget *parent) : QAbstractScrollArea(parent) {
    setFocusPolicy(Qt::StrongFocus);
    m_blockStarts.push_back(0);
    m_lines.push_back(VLine{0, 0, 0});
    layoutText();
}

/*!
    Constructs an editor with the given \a text and \a parent.
*/
QPlainTextEdit::QPlainTextEdit(const QString &text, QWidget *parent) : QPlainTextEdit(parent) { setPlainText(text); }

/*!
    Destroys the editor.
*/
QPlainTextEdit::~QPlainTextEdit() = default;

/*!
    \fn QString QPlainTextEdit::toPlainText() const

    Returns the text. Lines are separated by a new line character.

    \sa setPlainText()
*/

/*!
    Replaces the text with \a text, and puts the cursor at the start. The undo history is cleared. Emits textChanged() if
    the text is not the same.

    \sa toPlainText(), clear()
*/
void QPlainTextEdit::setPlainText(const QString &text) {
    const bool changedText = text != m_text;
    const int oldPosition = m_position, oldAnchor = m_anchor;
    m_text = text;
    m_position = m_anchor = 0;
    m_goalX = -1;
    m_lastTypingEnd = -1;
    m_undo.clear();
    m_redo.clear();
    layoutText();
    horizontalScrollBar()->setValue(0);
    verticalScrollBar()->setValue(0);
    viewport()->update();
    didChange(changedText, oldPosition, oldAnchor);
    notifyUndo();
}

/*!
    Appends a new line with \a text to the end of the text. If the view was scrolled to the bottom, it stays there.

    \sa insertPlainText()
*/
void QPlainTextEdit::appendPlainText(const QString &text) {
    QScrollBar *bar = verticalScrollBar();
    const bool atBottom = bar->value() == bar->maximum();
    const int end = static_cast<int>(m_text.size());
    applyEdit(end, end, m_text.isEmpty() ? text : QString("\n") + text);
    if (atBottom)
        bar->setValue(bar->maximum());
}

/*!
    Inserts \a text at the cursor, in place of the selection if there is one, and puts the cursor after it.

    \sa appendPlainText()
*/
void QPlainTextEdit::insertPlainText(const QString &text) {
    const int from = std::min(m_position, m_anchor), to = std::max(m_position, m_anchor);
    applyEdit(from, to, text, false, from + static_cast<int>(text.size()));
}

/*!
    Removes all the text and the undo history.

    \sa setPlainText()
*/
void QPlainTextEdit::clear() { setPlainText(QString()); }

/*!
    Selects all the text, with the cursor at the end.
*/
void QPlainTextEdit::selectAll() { setSelection(0, static_cast<int>(m_text.size())); }

/*!
    \fn QString QPlainTextEdit::placeholderText() const

    Returns the text that is shown, dimmed, while the editor is empty.

    \sa setPlaceholderText()
*/

/*!
    Sets the text that is shown while the editor is empty to \a text.

    \sa placeholderText()
*/
void QPlainTextEdit::setPlaceholderText(const QString &text) {
    m_placeholder = text;
    viewport()->update();
}

/*!
    \fn bool QPlainTextEdit::isReadOnly() const

    Returns whether the user cannot change the text. The program still can. The default is \c false.

    \sa setReadOnly()
*/

/*!
    Sets whether the user cannot change the text to \a readOnly. The cursor can still be moved, and text selected.

    \sa isReadOnly()
*/
void QPlainTextEdit::setReadOnly(bool readOnly) {
    if (m_readOnly == readOnly)
        return;
    m_readOnly = readOnly;
    viewport()->update();
}

/*!
    \fn QPlainTextEdit::LineWrapMode QPlainTextEdit::lineWrapMode() const

    Returns how long lines are shown. The default is \c WidgetWidth.

    \sa setLineWrapMode()
*/

/*!
    Sets how long lines are shown to \a mode.

    \sa lineWrapMode()
*/
void QPlainTextEdit::setLineWrapMode(LineWrapMode mode) {
    if (m_wrap == mode)
        return;
    m_wrap = mode;
    layoutText();
    ensureCursorVisible();
    viewport()->update();
}

/*!
    \fn bool QPlainTextEdit::tabChangesFocus() const

    Returns whether the Tab key moves the focus to the next widget, instead of typing a tab. The default is \c false.

    \sa setTabChangesFocus()
*/

/*!
    \fn void QPlainTextEdit::setTabChangesFocus(bool enable)

    Sets whether the Tab key moves the focus to \a enable.

    \sa tabChangesFocus()
*/

/*!
    \fn qreal QPlainTextEdit::tabStopDistance() const

    Returns the distance between tab stops in pixels, 80 by default.

    \sa setTabStopDistance()
*/

/*!
    Sets the distance between tab stops to \a distance pixels.

    \sa tabStopDistance()
*/
void QPlainTextEdit::setTabStopDistance(qreal distance) {
    m_tabStop = std::max<qreal>(1, distance);
    layoutText();
    viewport()->update();
}

/*!
    \fn bool QPlainTextEdit::isUndoRedoEnabled() const

    Returns whether undo() and redo() work. The default is \c true.

    \sa setUndoRedoEnabled()
*/

/*!
    Sets whether the changes are kept so that they can be undone to \a enable. Turning it off clears the history.

    \sa isUndoRedoEnabled()
*/
void QPlainTextEdit::setUndoRedoEnabled(bool enable) {
    m_undoEnabled = enable;
    if (!enable) {
        m_undo.clear();
        m_redo.clear();
        notifyUndo();
    }
}

/*!
    Returns the number of blocks: lines that are separated by a new line character. There is at least one.
*/
int QPlainTextEdit::blockCount() const { return m_blocks; }

/*!
    Returns a cursor at the position of the editor's own cursor, and with its selection. A change to the cursor that
    is returned moves the editor's cursor only after setTextCursor().

    \sa setTextCursor()
*/
QTextCursor QPlainTextEdit::textCursor() const {
    return QTextCursor(const_cast<QPlainTextEdit *>(this), m_position, m_anchor);
}

/*!
    Makes the position and the selection of \a cursor those of the editor.

    \sa textCursor()
*/
void QPlainTextEdit::setTextCursor(const QTextCursor &cursor) {
    if (cursor.isNull())
        return;
    const int size = static_cast<int>(m_text.size());
    const int oldPosition = m_position, oldAnchor = m_anchor;
    m_position = std::clamp(cursor.position(), 0, size);
    m_anchor = std::clamp(cursor.anchor(), 0, size);
    m_lastTypingEnd = -1;
    didChange(false, oldPosition, oldAnchor);
    ensureCursorVisible();
    viewport()->update();
}

/*!
    Returns a cursor at the text position that is under \a pos, a point in the viewport.
*/
QTextCursor QPlainTextEdit::cursorForPosition(const QPoint &pos) const {
    const int p = positionAt(pos);
    return QTextCursor(const_cast<QPlainTextEdit *>(this), p, p);
}

/*!
    Returns the rectangle of the cursor in the viewport: one pixel wide, and as high as a line.

    \sa ensureCursorVisible()
*/
QRect QPlainTextEdit::cursorRect() const { return cursorRect(textCursor()); }

/*!
    \overload

    Returns the rectangle of \a cursor in the viewport.
*/
QRect QPlainTextEdit::cursorRect(const QTextCursor &cursor) const {
    const int line = vlineOf(cursor.position());
    const int lh = lineHeight();
    return QRect(margin() + xOf(cursor.position()) - horizontalScrollBar()->value(),
                 margin() + line * lh - verticalScrollBar()->value(), 1, lh);
}

/*!
    Moves the cursor by \a operation, extending the selection if \a mode is \c KeepAnchor.

    \sa textCursor()
*/
void QPlainTextEdit::moveCursor(QTextCursor::MoveOperation operation, QTextCursor::MoveMode mode) {
    int goal = -1;
    if (operation == QTextCursor::Up || operation == QTextCursor::Down)
        goal = m_goalX >= 0 ? m_goalX : xOf(m_position);
    const int target = moved(m_position, operation, 1, &goal);
    if (operation == QTextCursor::Up || operation == QTextCursor::Down)
        m_goalX = goal;
    else
        m_goalX = -1;
    setCursor(target, mode == QTextCursor::KeepAnchor);
}

/*!
    Scrolls the editor so that the cursor is in view.

    \sa cursorRect()
*/
void QPlainTextEdit::ensureCursorVisible() {
    // Before the editor is shown it has no size to scroll in: the scrolling waits until it has.
    if (!isVisible()) {
        m_scrollPosition = m_position;
        return;
    }
    m_scrollPosition = -1;
    const int lh = lineHeight();
    QScrollBar *vbar = verticalScrollBar(), *hbar = horizontalScrollBar();
    const int top = margin() + vlineOf(m_position) * lh, bottom = top + lh;
    const int viewHeight = viewport()->height(), viewWidth = viewport()->width();
    if (top - margin() < vbar->value())
        vbar->setValue(std::max(0, top - margin()));
    else if (bottom + margin() > vbar->value() + viewHeight)
        vbar->setValue(bottom + margin() - viewHeight);
    const int x = margin() + xOf(m_position);
    if (x - margin() < hbar->value())
        hbar->setValue(std::max(0, x - margin()));
    else if (x + margin() + 1 > hbar->value() + viewWidth)
        hbar->setValue(x + margin() + 1 - viewWidth);
}

// ---- Text and its layout ----

int QPlainTextEdit::lineHeight() const { return std::max(1, fontMetrics().height()); }

// The width of the text from `start` of `length` characters, when it begins `x0` pixels from the start of its line. A run of
// text is measured as it is drawn, in one piece, so that this agrees with the painting.
int QPlainTextEdit::advance(int start, int length, int x0) const {
    const QFontMetrics fm = fontMetrics();
    const int stop = std::max(1, static_cast<int>(m_tabStop));
    int x = x0, segment = start;
    for (int i = start; i < start + length; ++i) {
        if (m_text.at(i) != QChar('\t'))
            continue;
        if (i > segment)
            x += fm.horizontalAdvance(m_text.mid(segment, i - segment));
        x += (x / stop + 1) * stop - x;
        segment = i + 1;
    }
    if (start + length > segment)
        x += fm.horizontalAdvance(m_text.mid(segment, start + length - segment));
    return x - x0;
}

// Breaks the text into the lines that are shown, and sets the scroll bars from their number and width. A scroll bar that
// comes or goes changes the width at which lines are wrapped, so this goes on until the sizes agree.
void QPlainTextEdit::layoutText() {
    if (m_laying) {
        m_layAgain = true;
        return;
    }
    m_laying = true;
    const int lh = lineHeight();
    const int size = static_cast<int>(m_text.size());
    for (int pass = 0; pass < 4; ++pass) {
        m_layAgain = false;
        m_blockStarts.assign(1, 0);
        for (int i = 0; i < size; ++i)
            if (m_text.at(i) == QChar('\n'))
                m_blockStarts.push_back(i + 1);
        m_lines.clear();
        const int limit = std::max(1, viewport()->width() - 2 * margin());
        int widest = 0;
        for (int b = 0; b < static_cast<int>(m_blockStarts.size()); ++b) {
            const int start = m_blockStarts[static_cast<std::size_t>(b)];
            const int end = b + 1 < static_cast<int>(m_blockStarts.size())
                                ? m_blockStarts[static_cast<std::size_t>(b) + 1] - 1
                                : size;
            if (m_wrap == NoWrap || start == end) {
                m_lines.push_back(VLine{start, end - start, b});
                widest = std::max(widest, advance(start, end - start));
                continue;
            }
            // Words are measured whole; a line breaks before a word that does not fit, and a word that is
            // wider than a line is broken between characters.
            int lineStart = start, x = 0, i = start;
            while (i < end) {
                int j = i;
                while (j < end && m_text.at(j) != QChar(' '))
                    ++j;
                const int wordEnd = j;
                while (j < end && m_text.at(j) == QChar(' '))
                    ++j;
                const int fit = advance(i, wordEnd - i, x);
                if (x + fit > limit && i > lineStart) {
                    m_lines.push_back(VLine{lineStart, i - lineStart, b});
                    lineStart = i;
                    x = 0;
                    continue;
                }
                if (x + fit > limit) {
                    int k = 1;
                    while (i + k < wordEnd && advance(i, k + 1, 0) <= limit)
                        ++k;
                    m_lines.push_back(VLine{lineStart, i + k - lineStart, b});
                    lineStart = i + k;
                    x = 0;
                    i = lineStart;
                    continue;
                }
                x += advance(i, j - i, x);
                i = j;
            }
            m_lines.push_back(VLine{lineStart, end - lineStart, b});
            widest = std::max(widest, limit);
        }
        m_blocks = static_cast<int>(m_blockStarts.size());
        const int contentHeight = static_cast<int>(m_lines.size()) * lh + 2 * margin();
        const int contentWidth = m_wrap == NoWrap ? widest + 2 * margin() + 1 : 0;
        QScrollBar *vbar = verticalScrollBar(), *hbar = horizontalScrollBar();
        vbar->setSingleStep(lh);
        vbar->setPageStep(std::max(1, viewport()->height()));
        vbar->setRange(0, std::max(0, contentHeight - viewport()->height()));
        hbar->setSingleStep(20);
        hbar->setPageStep(std::max(1, viewport()->width()));
        hbar->setRange(0, std::max(0, contentWidth - viewport()->width()));
        if (!m_layAgain)
            break;
    }
    m_laying = false;
}

// The line that shows `position`: the position at the end of a line that wraps is at the start of the next one.
int QPlainTextEdit::vlineOf(int position) const {
    const int n = static_cast<int>(m_lines.size());
    for (int i = 0; i < n; ++i) {
        const VLine &line = m_lines[static_cast<std::size_t>(i)];
        if (position >= line.start && position < line.start + line.length)
            return i;
        if (position == line.start + line.length) {
            const bool wrapped = i + 1 < n && m_lines[static_cast<std::size_t>(i) + 1].block == line.block;
            if (!wrapped)
                return i;
        }
    }
    return std::max(0, n - 1);
}

int QPlainTextEdit::xOf(int position) const {
    const VLine &line = m_lines[static_cast<std::size_t>(vlineOf(position))];
    return advance(line.start, position - line.start);
}

// The offset in `line` whose edge is the nearest to `x`, a distance from the start of the line.
int QPlainTextEdit::offsetAtX(const VLine &line, int x) const {
    int low = 0, high = line.length;
    while (low < high) {
        const int middle = (low + high + 1) / 2;
        if (advance(line.start, middle) <= x)
            low = middle;
        else
            high = middle - 1;
    }
    if (low < line.length && x - advance(line.start, low) >= advance(line.start, low + 1) - x)
        ++low;
    return low;
}

int QPlainTextEdit::positionAt(const QPoint &viewportPos) const {
    const int lh = lineHeight();
    const int y = viewportPos.y() - margin() + verticalScrollBar()->value();
    const int index = std::clamp(y < 0 ? 0 : y / lh, 0, static_cast<int>(m_lines.size()) - 1);
    const VLine &line = m_lines[static_cast<std::size_t>(index)];
    return line.start + offsetAtX(line, viewportPos.x() - margin() + horizontalScrollBar()->value());
}

int QPlainTextEdit::blockStart(int block) const {
    return m_blockStarts[static_cast<std::size_t>(std::clamp(block, 0, static_cast<int>(m_blockStarts.size()) - 1))];
}

int QPlainTextEdit::blockEnd(int block) const {
    block = std::clamp(block, 0, static_cast<int>(m_blockStarts.size()) - 1);
    return block + 1 < static_cast<int>(m_blockStarts.size()) ? m_blockStarts[static_cast<std::size_t>(block) + 1] - 1
                                                              : static_cast<int>(m_text.size());
}

int QPlainTextEdit::blockOf(int position) const {
    int block = 0;
    for (int b = 0; b < static_cast<int>(m_blockStarts.size()); ++b)
        if (m_blockStarts[static_cast<std::size_t>(b)] <= position)
            block = b;
    return block;
}

int QPlainTextEdit::wordStart(int position) const {
    while (position > 0 && isWordChar(m_text.at(position - 1)))
        --position;
    return position;
}

int QPlainTextEdit::wordEnd(int position) const {
    const int size = static_cast<int>(m_text.size());
    while (position < size && isWordChar(m_text.at(position)))
        ++position;
    return position;
}

// Where `operation` takes `position`, `n` times; `goalX` is the column that Up and Down keep.
int QPlainTextEdit::moved(int position, QTextCursor::MoveOperation operation, int n, int *goalX) const {
    const int size = static_cast<int>(m_text.size());
    int p = position;
    switch (operation) {
    case QTextCursor::NoMove:
        break;
    case QTextCursor::Start:
        p = 0;
        break;
    case QTextCursor::End:
        p = size;
        break;
    case QTextCursor::Left:
    case QTextCursor::PreviousCharacter:
        p = std::max(0, p - n);
        break;
    case QTextCursor::Right:
    case QTextCursor::NextCharacter:
        p = std::min(size, p + n);
        break;
    case QTextCursor::WordLeft:
    case QTextCursor::PreviousWord:
        for (int i = 0; i < n; ++i) {
            while (p > 0 && !isWordChar(m_text.at(p - 1)))
                --p;
            p = wordStart(p);
        }
        break;
    case QTextCursor::WordRight:
    case QTextCursor::NextWord:
        for (int i = 0; i < n; ++i) {
            p = wordEnd(p);
            while (p < size && !isWordChar(m_text.at(p)))
                ++p;
        }
        break;
    case QTextCursor::StartOfWord:
        p = wordStart(p);
        break;
    case QTextCursor::EndOfWord:
        p = wordEnd(p);
        break;
    case QTextCursor::StartOfBlock:
        p = blockStart(blockOf(p));
        break;
    case QTextCursor::EndOfBlock:
        p = blockEnd(blockOf(p));
        break;
    case QTextCursor::PreviousBlock:
        p = blockStart(std::max(0, blockOf(p) - n));
        break;
    case QTextCursor::NextBlock:
        p = blockOf(p) + n >= m_blocks ? size : blockStart(blockOf(p) + n);
        break;
    case QTextCursor::StartOfLine:
        p = m_lines[static_cast<std::size_t>(vlineOf(p))].start;
        break;
    case QTextCursor::EndOfLine: {
        const int index = vlineOf(p);
        const VLine &line = m_lines[static_cast<std::size_t>(index)];
        p = line.start + line.length;
        // A line that wraps ends before the space that it breaks at, so that the cursor stays on it.
        const bool wrapped = index + 1 < static_cast<int>(m_lines.size()) &&
                             m_lines[static_cast<std::size_t>(index) + 1].block == line.block;
        if (wrapped && line.length > 0 && m_text.at(p - 1) == QChar(' '))
            --p;
        break;
    }
    case QTextCursor::Up:
    case QTextCursor::Down: {
        const int step = operation == QTextCursor::Up ? -1 : 1;
        int index = vlineOf(p);
        const int x = goalX && *goalX >= 0 ? *goalX : xOf(p);
        for (int i = 0; i < n; ++i) {
            const int next = index + step;
            if (next < 0 || next >= static_cast<int>(m_lines.size()))
                break;
            index = next;
        }
        const VLine &line = m_lines[static_cast<std::size_t>(index)];
        p = line.start + offsetAtX(line, x);
        if (goalX)
            *goalX = x;
        break;
    }
    }
    return p;
}

// ---- Editing ----

void QPlainTextEdit::pushUndo() {
    if (!m_undoEnabled)
        return;
    m_undo.push_back(Snapshot{m_text, m_position, m_anchor});
    if (m_undo.size() > 200)
        m_undo.erase(m_undo.begin());
}

void QPlainTextEdit::notifyUndo() {
    const bool undoNow = !m_undo.empty(), redoNow = !m_redo.empty();
    if (undoNow != m_undoAvailable) {
        m_undoAvailable = undoNow;
        emit undoAvailable(undoNow);
    }
    if (redoNow != m_redoAvailable) {
        m_redoAvailable = redoNow;
        emit redoAvailable(redoNow);
    }
}

// Tells what changed: the signals, and the view.
void QPlainTextEdit::didChange(bool textDidChange, int oldPosition, int oldAnchor) {
    if (textDidChange) {
        emit textChanged();
        emit blockCountChanged(m_blocks);
    }
    if (m_position != oldPosition)
        emit cursorPositionChanged();
    if (std::min(m_position, m_anchor) != std::min(oldPosition, oldAnchor) ||
        std::max(m_position, m_anchor) != std::max(oldPosition, oldAnchor))
        emit selectionChanged();
}

// Replaces the text from `from` to `to` with `text`. `typing` is for text that the user types, which is undone together;
// `cursorAfter`, if it is not -1, is where the cursor goes: otherwise it moves with the text.
void QPlainTextEdit::applyEdit(int from, int to, const QString &text, bool typing, int cursorAfter) {
    const int size = static_cast<int>(m_text.size());
    from = std::clamp(from, 0, size);
    to = std::clamp(to, 0, size);
    if (from > to)
        std::swap(from, to);
    if (from == to && text.isEmpty())
        return;
    const int oldPosition = m_position, oldAnchor = m_anchor;
    const int oldBlocks = m_blocks;
    const bool coalesce = typing && m_lastTypingEnd == from && !m_undo.empty();
    if (m_editBlock == 0 && !coalesce)
        pushUndo();
    m_lastTypingEnd = typing ? from + static_cast<int>(text.size()) : -1;
    m_redo.clear();
    const int length = static_cast<int>(text.size());
    m_text.remove(from, to - from);
    m_text.insert(from, text);
    auto follow = [&](int p) {
        if (p >= to)
            return p - (to - from) + length;
        return p > from ? from : p;
    };
    m_position = follow(m_position);
    m_anchor = follow(m_anchor);
    if (cursorAfter >= 0)
        m_position = m_anchor = std::clamp(cursorAfter, 0, static_cast<int>(m_text.size()));
    m_goalX = -1;
    layoutText();
    viewport()->update();
    emit textChanged();
    if (m_blocks != oldBlocks)
        emit blockCountChanged(m_blocks);
    if (m_position != oldPosition)
        emit cursorPositionChanged();
    if (std::min(m_position, m_anchor) != std::min(oldPosition, oldAnchor) ||
        std::max(m_position, m_anchor) != std::max(oldPosition, oldAnchor))
        emit selectionChanged();
    notifyUndo();
    if (hasFocus())
        ensureCursorVisible();
}

void QPlainTextEdit::setCursor(int position, bool extend) {
    const int oldPosition = m_position, oldAnchor = m_anchor;
    m_position = std::clamp(position, 0, static_cast<int>(m_text.size()));
    if (!extend)
        m_anchor = m_position;
    m_lastTypingEnd = -1;
    didChange(false, oldPosition, oldAnchor);
    ensureCursorVisible();
    viewport()->update();
}

void QPlainTextEdit::setSelection(int anchor, int position) {
    const int oldPosition = m_position, oldAnchor = m_anchor;
    const int size = static_cast<int>(m_text.size());
    m_anchor = std::clamp(anchor, 0, size);
    m_position = std::clamp(position, 0, size);
    m_lastTypingEnd = -1;
    didChange(false, oldPosition, oldAnchor);
    ensureCursorVisible();
    viewport()->update();
}

void QPlainTextEdit::restore(const Snapshot &snapshot) {
    const int oldPosition = m_position, oldAnchor = m_anchor;
    const bool changedText = snapshot.text != m_text;
    const int oldBlocks = m_blocks;
    m_text = snapshot.text;
    m_position = snapshot.position;
    m_anchor = snapshot.anchor;
    m_lastTypingEnd = -1;
    m_goalX = -1;
    layoutText();
    viewport()->update();
    if (changedText) {
        emit textChanged();
        if (m_blocks != oldBlocks)
            emit blockCountChanged(m_blocks);
    }
    if (m_position != oldPosition)
        emit cursorPositionChanged();
    if (std::min(m_position, m_anchor) != std::min(oldPosition, oldAnchor) ||
        std::max(m_position, m_anchor) != std::max(oldPosition, oldAnchor))
        emit selectionChanged();
    ensureCursorVisible();
}

/*!
    Undoes the last change: the text that was typed since the cursor moved, or a change of the program. Does nothing if
    there is nothing to undo.

    \sa redo()
*/
void QPlainTextEdit::undo() {
    if (m_undo.empty() || !m_undoEnabled)
        return;
    m_redo.push_back(Snapshot{m_text, m_position, m_anchor});
    const Snapshot snapshot = m_undo.back();
    m_undo.pop_back();
    restore(snapshot);
    notifyUndo();
}

/*!
    Does again the change that undo() undid. Does nothing if there is none.

    \sa undo()
*/
void QPlainTextEdit::redo() {
    if (m_redo.empty() || !m_undoEnabled)
        return;
    m_undo.push_back(Snapshot{m_text, m_position, m_anchor});
    const Snapshot snapshot = m_redo.back();
    m_redo.pop_back();
    restore(snapshot);
    notifyUndo();
}

// ---- Events ----

/*!
    \reimp
*/
void QPlainTextEdit::resizeEvent(QResizeEvent *event) {
    (void)event;
    layoutText();
    if (m_scrollPosition >= 0 && isVisible())
        ensureCursorVisible();
}

/*!
    \reimp
*/
void QPlainTextEdit::showEvent(QShowEvent *event) {
    (void)event;
    layoutText();
    if (m_scrollPosition >= 0)
        ensureCursorVisible();
}

/*!
    \reimp
*/
void QPlainTextEdit::scrollContentsBy(int dx, int dy) {
    (void)dx;
    (void)dy;
    viewport()->update();
}

/*!
    \reimp
*/
void QPlainTextEdit::changeEvent(QEvent *event) {
    layoutText();
    viewport()->update();
    QAbstractScrollArea::changeEvent(event);
}

/*!
    \reimp
*/
void QPlainTextEdit::focusInEvent(QFocusEvent *event) {
    (void)event;
    viewport()->update();
}

/*!
    \reimp
*/
void QPlainTextEdit::focusOutEvent(QFocusEvent *event) {
    (void)event;
    viewport()->update();
}

/*!
    \reimp

    A tab is typed in the editor, unless tabChangesFocus() is set.
*/
bool QPlainTextEdit::focusNextPrevChild(bool next) {
    if (!m_tabChangesFocus && !m_readOnly)
        return false;
    return QAbstractScrollArea::focusNextPrevChild(next);
}

/*!
    \reimp
*/
void QPlainTextEdit::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(viewport());
    const QPalette &pal = palette();
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const QRect all = viewport()->rect();
    painter.fillRect(all, pal.color(group, QPalette::Base));
    painter.setFont(font());
    const int lh = lineHeight();
    const int offsetX = margin() - horizontalScrollBar()->value(), offsetY = margin() - verticalScrollBar()->value();
    const int stop = std::max(1, static_cast<int>(m_tabStop));
    const QFontMetrics fm = fontMetrics();
    if (m_text.isEmpty() && !m_placeholder.isEmpty()) {
        painter.setPen(pal.color(group, QPalette::PlaceholderText));
        painter.drawText(QRect(offsetX, offsetY, all.width() - 2 * margin(), lh),
                         Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, m_placeholder);
    }
    auto drawText = [&](int x, int y, int start, int length, const QColor &color) {
        painter.setPen(color);
        int cursor = x, segment = start;
        auto flush = [&](int end) {
            if (end > segment) {
                const QString piece = m_text.mid(segment, end - segment);
                painter.drawText(QRect(cursor, y, fm.horizontalAdvance(piece) + 2, lh),
                                 Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextDontClip, piece);
                cursor += fm.horizontalAdvance(piece);
            }
        };
        for (int i = start; i < start + length; ++i)
            if (m_text.at(i) == QChar('\t')) {
                flush(i);
                const int rel = cursor - x;
                cursor += (rel / stop + 1) * stop - rel;
                segment = i + 1;
            }
        flush(start + length);
    };
    const int from = std::min(m_position, m_anchor), to = std::max(m_position, m_anchor);
    const int first = std::max(0, (-offsetY) / lh);
    const int spaceWidth = fm.horizontalAdvance(QChar(' '));
    for (int i = first; i < static_cast<int>(m_lines.size()); ++i) {
        const VLine &line = m_lines[static_cast<std::size_t>(i)];
        const int y = offsetY + i * lh;
        if (y > all.bottom())
            break;
        const int lineEnd = line.start + line.length;
        const bool lastOfBlock =
            i + 1 >= static_cast<int>(m_lines.size()) || m_lines[static_cast<std::size_t>(i) + 1].block != line.block;
        const int a = std::max(from, line.start), b = std::min(to, lineEnd);
        const bool newline =
            lastOfBlock && lineEnd < static_cast<int>(m_text.size()) && from <= lineEnd && to > lineEnd;
        QRect selected;
        if (from != to && (a < b || newline)) {
            const int xa = offsetX + advance(line.start, std::max(a, line.start) - line.start);
            const int xb = offsetX + advance(line.start, std::max(std::min(b, lineEnd), line.start) - line.start);
            selected = QRect(xa, y, std::max(0, xb - xa) + (newline ? spaceWidth : 0), lh);
            painter.fillRect(selected, pal.color(group, QPalette::Highlight));
        }
        drawText(offsetX, y, line.start, line.length, pal.color(group, QPalette::Text));
        if (!selected.isNull()) {
            painter.setClipRect(selected);
            drawText(offsetX, y, line.start, line.length, pal.color(group, QPalette::HighlightedText));
            painter.setClipping(false);
        }
    }
    if (hasFocus() && !m_readOnly && isEnabled()) {
        const QRect caret = cursorRect();
        painter.fillRect(QRect(caret.x(), caret.y(), 1, caret.height()), pal.color(group, QPalette::Text));
    }
}

/*!
    \reimp
*/
void QPlainTextEdit::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    m_goalX = -1;
    setCursor(positionAt(event->pos()), event->modifiers() & Qt::ShiftModifier);
    m_selecting = true;
    event->accept();
}

/*!
    \reimp
*/
void QPlainTextEdit::mouseMoveEvent(QMouseEvent *event) {
    if (!m_selecting) {
        event->ignore();
        return;
    }
    m_goalX = -1;
    setCursor(positionAt(event->pos()), true);
    event->accept();
}

/*!
    \reimp
*/
void QPlainTextEdit::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    m_selecting = false;
    event->accept();
}

/*!
    \reimp

    A double click selects the word under the mouse.
*/
void QPlainTextEdit::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const int p = positionAt(event->pos());
    int start = wordStart(p), end = wordEnd(p);
    if (start == end && p < static_cast<int>(m_text.size()) && !isWordChar(m_text.at(p)))
        end = p + 1;
    setSelection(start, end);
    m_selecting = false;
    event->accept();
}

/*!
    \reimp

    The arrow keys move the cursor, Control with them by a word, and Shift selects; Home and End go to the start and the end of the
    line, and with Control of the text; Page Up and Page Down move by a page. Control+A selects all, Control+Z undoes,
    and Control+Y or Control+Shift+Z does again. Unless the editor is read-only, Backspace and Delete remove a character
    (Control, a word), Return starts a line, Tab types a tab, and other keys type their text.
*/
void QPlainTextEdit::keyPressEvent(QKeyEvent *event) {
    const Qt::KeyboardModifiers mods = event->modifiers();
    const bool ctrl = mods & Qt::ControlModifier, shift = mods & Qt::ShiftModifier;
    const int size = static_cast<int>(m_text.size());
    const int selStart = std::min(m_position, m_anchor), selEnd = std::max(m_position, m_anchor);
    const int page = std::max(1, viewport()->height() / lineHeight() - 1);
    auto go = [&](QTextCursor::MoveOperation op, int n = 1) {
        int goal = -1;
        const bool vertical = op == QTextCursor::Up || op == QTextCursor::Down;
        if (vertical)
            goal = m_goalX >= 0 ? m_goalX : xOf(m_position);
        const int target = moved(m_position, op, n, &goal);
        m_goalX = vertical ? goal : -1;
        setCursor(target, shift);
    };
    event->accept();
    switch (event->key()) {
    case Qt::Key_Left:
        if (selStart != selEnd && !shift && !ctrl) {
            m_goalX = -1;
            setCursor(selStart, false);
        } else {
            go(ctrl ? QTextCursor::WordLeft : QTextCursor::Left);
        }
        return;
    case Qt::Key_Right:
        if (selStart != selEnd && !shift && !ctrl) {
            m_goalX = -1;
            setCursor(selEnd, false);
        } else {
            go(ctrl ? QTextCursor::WordRight : QTextCursor::Right);
        }
        return;
    case Qt::Key_Up:
        go(QTextCursor::Up);
        return;
    case Qt::Key_Down:
        go(QTextCursor::Down);
        return;
    case Qt::Key_PageUp:
        go(QTextCursor::Up, page);
        return;
    case Qt::Key_PageDown:
        go(QTextCursor::Down, page);
        return;
    case Qt::Key_Home:
        go(ctrl ? QTextCursor::Start : QTextCursor::StartOfLine);
        return;
    case Qt::Key_End:
        go(ctrl ? QTextCursor::End : QTextCursor::EndOfLine);
        return;
    case Qt::Key_A:
        if (ctrl) {
            selectAll();
            return;
        }
        break;
    case Qt::Key_Z:
        if (ctrl && !m_readOnly) {
            shift ? redo() : undo();
            return;
        }
        break;
    case Qt::Key_Y:
        if (ctrl && !m_readOnly) {
            redo();
            return;
        }
        break;
    default:
        break;
    }
    if (m_readOnly) {
        event->ignore();
        return;
    }
    switch (event->key()) {
    case Qt::Key_Backspace:
        if (selStart != selEnd)
            applyEdit(selStart, selEnd, QString(), false, selStart);
        else if (m_position > 0) {
            const int from = ctrl ? moved(m_position, QTextCursor::WordLeft, 1, nullptr) : m_position - 1;
            applyEdit(from, m_position, QString(), false, from);
        }
        return;
    case Qt::Key_Delete:
        if (selStart != selEnd)
            applyEdit(selStart, selEnd, QString(), false, selStart);
        else if (m_position < size) {
            const int to = ctrl ? moved(m_position, QTextCursor::WordRight, 1, nullptr) : m_position + 1;
            applyEdit(m_position, to, QString(), false, m_position);
        }
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        applyEdit(selStart, selEnd, QString("\n"), false, selStart + 1);
        return;
    case Qt::Key_Tab:
        if (!ctrl) {
            applyEdit(selStart, selEnd, QString("\t"), false, selStart + 1);
            return;
        }
        break;
    default:
        break;
    }
    if (!(mods & (Qt::ControlModifier | Qt::AltModifier)) && !event->text().isEmpty() &&
        event->text().at(0).isPrint()) {
        const QString text = event->text();
        applyEdit(selStart, selEnd, text, true, selStart + static_cast<int>(text.size()));
        return;
    }
    event->ignore();
}

#include "moc_qplaintextedit.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
