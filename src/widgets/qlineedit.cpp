// MIT License - see LICENSE
#include <QtWidgets/qlineedit.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

/*!
    \class QLineEdit
    \brief The QLineEdit class is a one-line text editor.

    A line edit shows a text that the user can edit: type to insert, use the arrow keys, Home, End, Backspace
    and Delete to move and remove text, hold Shift to select, and select with the mouse. Connect to textChanged() for
    every change, to textEdited() for the changes the user makes, and to returnPressed() or editingFinished() for
    the end of an edit:

    \code
    QLineEdit *name = new QLineEdit(window);
    name->setPlaceholderText("Your name");
    connect(name, &QLineEdit::textChanged, [](const QString &text) { qDebug() << text; });
    \endcode

    \differences
    There is no clipboard (cut, copy, paste), undo and redo, drag and drop, context menu, input mask,
    \c QValidator, \c QCompleter, alignment, \c setClearButtonEnabled() or \c inputMethodHints(). The cursor does not
    blink, and there are no \c PasswordEchoOnEdit and no \c cursorMoveStyle(). Text is edited one code unit at a time, so
    characters outside the Basic Multilingual Plane count twice.
    \enddifferences

    \sa QLabel
*/

/*!
    \enum QLineEdit::EchoMode

    This enum says how the text of a line edit is shown.

    \value Normal The characters are shown as they are.
    \value NoEcho Nothing is shown, which hides even the length of the text.
    \value Password A bullet is shown for each character.
*/

/*!
    Constructs an empty line edit with the given \a parent.
*/
QLineEdit::QLineEdit(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

/*!
    Constructs a line edit that contains \a text, with the given \a parent.
*/
QLineEdit::QLineEdit(const QString &text, QWidget *parent) : QLineEdit(parent) {
    m_text = text.left(m_maxLength);
    m_cursor = m_anchor = static_cast<int>(m_text.size());
}

/*!
    Destroys the line edit.
*/
QLineEdit::~QLineEdit() = default;

/*!
    \fn QString QLineEdit::text() const

    Returns the text. The default is an empty string.

    \sa setText(), displayText()
*/

/*!
    Returns the text as it is shown: the same as text(), a bullet for each character with the Password echo mode, or
    nothing with NoEcho.

    \sa echoMode()
*/
QString QLineEdit::displayText() const {
    switch (m_echoMode) {
    case Password:
        return QString(m_text.size(), QChar(0x2022));
    case NoEcho:
        return QString();
    case Normal:
        break;
    }
    return m_text;
}

/*!
    \fn QString QLineEdit::placeholderText() const

    Returns the text that is shown in gray while the line edit is empty.

    \sa setPlaceholderText()
*/

/*!
    Sets the text that is shown while the line edit is empty to \a text.

    \sa placeholderText()
*/
void QLineEdit::setPlaceholderText(const QString &text) {
    if (m_placeholder == text)
        return;
    m_placeholder = text;
    update();
}

/*!
    \fn int QLineEdit::maxLength() const

    Returns the longest text the line edit accepts, 32767 characters by default.

    \sa setMaxLength()
*/

/*!
    Sets the longest text the line edit accepts to \a length characters. A text that is longer is cut.

    \sa maxLength()
*/
void QLineEdit::setMaxLength(int length) {
    m_maxLength = std::max(0, length);
    if (m_text.size() > m_maxLength)
        changeText(m_text.left(m_maxLength), std::min(m_cursor, m_maxLength), false);
}

/*!
    \fn bool QLineEdit::isReadOnly() const

    Returns whether the user cannot change the text. The text can still be selected.

    \sa setReadOnly()
*/

/*!
    Sets whether the text is read-only to \a readOnly.

    \sa isReadOnly()
*/
void QLineEdit::setReadOnly(bool readOnly) {
    if (m_readOnly == readOnly)
        return;
    m_readOnly = readOnly;
    update();
}

/*!
    \fn bool QLineEdit::hasFrame() const

    Returns whether the line edit draws a frame around its text. The default is \c true.

    \sa setFrame()
*/

/*!
    Sets whether the line edit has a frame to \a frame.

    \sa hasFrame()
*/
void QLineEdit::setFrame(bool frame) {
    if (m_frame == frame)
        return;
    m_frame = frame;
    updateGeometry();
    update();
}

/*!
    \fn QLineEdit::EchoMode QLineEdit::echoMode() const

    Returns how the text is shown.

    \sa setEchoMode()
*/

/*!
    Sets how the text is shown to \a mode.

    \sa echoMode()
*/
void QLineEdit::setEchoMode(EchoMode mode) {
    if (m_echoMode == mode)
        return;
    m_echoMode = mode;
    scrollToCursor();
    update();
}

/*!
    \fn int QLineEdit::cursorPosition() const

    Returns the position of the cursor in the text, from 0 for before the first character to the length of the text.

    \sa setCursorPosition()
*/

/*!
    Moves the cursor to \a position, which is limited to the text, and clears the selection.

    \sa cursorPosition()
*/
void QLineEdit::setCursorPosition(int position) { moveCursor(position, false); }

/*!
    Returns the cursor position that \a pos, a point in the line edit, is closest to.

    \sa setCursorPosition()
*/
int QLineEdit::cursorPositionAt(const QPoint &pos) const {
    if (m_echoMode == NoEcho)
        return 0;
    const int x = pos.x() - contentsRect().left() + m_scroll;
    int best = 0;
    int bestDistance = std::abs(x);
    for (int i = 1; i <= m_text.size(); ++i) {
        const int distance = std::abs(x - xOf(i));
        if (distance >= bestDistance && xOf(i) > x)
            break;
        if (distance < bestDistance) {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

/*!
    \fn bool QLineEdit::hasSelectedText() const

    Returns whether some of the text is selected.

    \sa selectedText()
*/

/*!
    Returns the selected text, or an empty string if nothing is selected.

    \sa hasSelectedText()
*/
QString QLineEdit::selectedText() const { return m_text.mid(selectionStart(), selectionEnd() - selectionStart()); }

/*!
    Returns where the selection starts, or the position of the cursor if nothing is selected.

    \sa selectionEnd()
*/
int QLineEdit::selectionStart() const { return std::min(m_cursor, m_anchor); }

/*!
    Returns where the selection ends, or the position of the cursor if nothing is selected.

    \sa selectionStart()
*/
int QLineEdit::selectionEnd() const { return std::max(m_cursor, m_anchor); }

/*!
    Selects \a length characters from \a start. A negative \a length selects backwards from \a start, with the cursor
    at the beginning of the selection.

    \sa selectAll(), deselect()
*/
void QLineEdit::setSelection(int start, int length) {
    const int size = static_cast<int>(m_text.size());
    const int anchor = std::clamp(start, 0, size);
    const int cursor = std::clamp(start + length, 0, size);
    const int oldStart = selectionStart(), oldEnd = selectionEnd(), oldCursor = m_cursor;
    m_anchor = anchor;
    m_cursor = cursor;
    if (oldCursor != m_cursor)
        emit cursorPositionChanged(oldCursor, m_cursor);
    if (oldStart != selectionStart() || oldEnd != selectionEnd())
        emit selectionChanged();
    scrollToCursor();
    update();
}

/*!
    Sets the text to \a text, cuts it to maxLength() and puts the cursor at its end. The text counts as not edited by
    the user, so textEdited() is not emitted.

    \sa text()
*/
void QLineEdit::setText(const QString &text) {
    const QString cut = text.left(m_maxLength);
    changeText(cut, static_cast<int>(cut.size()), false);
}

/*!
    Removes the text.
*/
void QLineEdit::clear() { changeText(QString(), 0, false); }

/*!
    Selects all of the text and puts the cursor at its end.

    \sa deselect()
*/
void QLineEdit::selectAll() { setSelection(0, static_cast<int>(m_text.size())); }

/*!
    Clears the selection and keeps the cursor where it is.

    \sa selectAll()
*/
void QLineEdit::deselect() { moveCursor(m_cursor, false); }

/*!
    Inserts \a text at the cursor, in place of the selection, as far as maxLength() allows.

    \sa backspace(), del()
*/
void QLineEdit::insert(const QString &text) {
    const int start = selectionStart(), end = selectionEnd();
    const qsizetype room = m_maxLength - (m_text.size() - (end - start));
    if (room <= 0)
        return;
    const QString added = text.left(room);
    changeText(m_text.left(start) + added + m_text.mid(end), start + static_cast<int>(added.size()), true);
}

/*!
    Deletes the selection, or the character before the cursor if nothing is selected.

    \sa del()
*/
void QLineEdit::backspace() {
    if (hasSelectedText()) {
        insert(QString());
    } else if (m_cursor > 0) {
        changeText(m_text.left(m_cursor - 1) + m_text.mid(m_cursor), m_cursor - 1, true);
    }
}

/*!
    Deletes the selection, or the character after the cursor if nothing is selected.

    \sa backspace()
*/
void QLineEdit::del() {
    if (hasSelectedText()) {
        insert(QString());
    } else if (m_cursor < m_text.size()) {
        changeText(m_text.left(m_cursor) + m_text.mid(m_cursor + 1), m_cursor, true);
    }
}

/*!
    Moves the cursor to the beginning of the text, and selects what it moves over if \a mark is \c true.

    \sa end()
*/
void QLineEdit::home(bool mark) { moveCursor(0, mark); }

/*!
    Moves the cursor to the end of the text, and selects what it moves over if \a mark is \c true.

    \sa home()
*/
void QLineEdit::end(bool mark) { moveCursor(static_cast<int>(m_text.size()), mark); }

QRect QLineEdit::contentsRect() const {
    QStyleOptionFrame option;
    option.initFrom(this);
    option.lineWidth = m_frame ? style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, this) : 0;
    return style()->subElementRect(QStyle::SE_LineEditContents, &option, this);
}

int QLineEdit::xOf(int position) const { return fontMetrics().horizontalAdvance(displayText().left(position)); }

void QLineEdit::moveCursor(int position, bool mark) {
    const int size = static_cast<int>(m_text.size());
    const int target = std::clamp(position, 0, size);
    const int oldStart = selectionStart(), oldEnd = selectionEnd(), oldCursor = m_cursor;
    m_cursor = target;
    if (!mark)
        m_anchor = target;
    if (oldCursor != m_cursor)
        emit cursorPositionChanged(oldCursor, m_cursor);
    if (oldStart != selectionStart() || oldEnd != selectionEnd())
        emit selectionChanged();
    scrollToCursor();
    update();
}

void QLineEdit::changeText(const QString &text, int cursor, bool userEdit) {
    const bool changed = text != m_text;
    const int oldCursor = m_cursor;
    const bool hadSelection = hasSelectedText();
    m_text = text;
    m_cursor = m_anchor = std::clamp(cursor, 0, static_cast<int>(m_text.size()));
    scrollToCursor();
    update();
    if (changed) {
        emit textChanged(m_text);
        if (userEdit)
            emit textEdited(m_text);
    }
    if (oldCursor != m_cursor)
        emit cursorPositionChanged(oldCursor, m_cursor);
    if (hadSelection)
        emit selectionChanged();
}

// Scrolls the text sideways so that the cursor is in view.
void QLineEdit::scrollToCursor() {
    const int visible = std::max(1, contentsRect().width() - 1);
    const int textWidth = xOf(static_cast<int>(m_text.size()));
    const int cursorX = xOf(m_cursor);
    if (textWidth < visible) {
        m_scroll = 0;
        return;
    }
    if (cursorX - m_scroll > visible)
        m_scroll = cursorX - visible;
    if (cursorX - m_scroll < 0)
        m_scroll = cursorX;
    m_scroll = std::clamp(m_scroll, 0, textWidth - visible + 1);
}

/*!
    \reimp
*/
QSize QLineEdit::sizeHint() const {
    QStyleOptionFrame option;
    option.initFrom(this);
    const QFontMetrics fm = fontMetrics();
    return style()->sizeFromContents(QStyle::CT_LineEdit, &option,
                                     QSize(fm.horizontalAdvance(QChar('x')) * 17, fm.height()), this);
}

/*!
    \reimp
*/
QSize QLineEdit::minimumSizeHint() const {
    QStyleOptionFrame option;
    option.initFrom(this);
    const QFontMetrics fm = fontMetrics();
    return style()->sizeFromContents(QStyle::CT_LineEdit, &option,
                                     QSize(fm.horizontalAdvance(QChar('x')) * 4, fm.height()), this);
}

/*!
    \reimp
*/
void QLineEdit::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    painter.setFont(font());
    QStyleOptionFrame option;
    option.initFrom(this);
    option.lineWidth = m_frame ? 1 : 0;
    style()->drawPrimitive(QStyle::PE_PanelLineEdit, &option, &painter, this);
    if (m_frame)
        style()->drawPrimitive(QStyle::PE_FrameLineEdit, &option, &painter, this);

    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const QRect contents = contentsRect();
    const QFontMetrics fm = fontMetrics();
    const int top = contents.center().y() - fm.height() / 2;
    const QString shown = displayText();
    const QRect textRect(contents.left() - m_scroll, top, xOf(static_cast<int>(m_text.size())) + 4, fm.height());
    painter.setClipRect(contents);
    if (m_text.isEmpty()) {
        painter.setPen(palette().color(group, QPalette::PlaceholderText));
        painter.drawText(QRect(contents.left(), top, contents.width(), fm.height()),
                         Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, m_placeholder);
    }
    painter.setPen(palette().color(group, QPalette::Text));
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextDontClip, shown);
    if (hasSelectedText() && m_echoMode != NoEcho) {
        const QRect selection(contents.left() - m_scroll + xOf(selectionStart()), top,
                              xOf(selectionEnd()) - xOf(selectionStart()), fm.height());
        painter.fillRect(selection, palette().color(group, QPalette::Highlight));
        painter.setClipRect(selection.intersected(contents));
        painter.setPen(palette().color(group, QPalette::HighlightedText));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextDontClip, shown);
        painter.setClipRect(contents);
    }
    if (hasFocus() && !m_readOnly) {
        painter.fillRect(contents.left() - m_scroll + xOf(m_cursor), top, 1, fm.height(),
                         palette().color(group, QPalette::Text));
    }
}

/*!
    \reimp
*/
void QLineEdit::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    m_dragging = true;
    moveCursor(cursorPositionAt(event->pos()), event->modifiers() & Qt::ShiftModifier);
    event->accept();
}

/*!
    \reimp
*/
void QLineEdit::mouseMoveEvent(QMouseEvent *event) {
    if (!m_dragging) {
        event->ignore();
        return;
    }
    moveCursor(cursorPositionAt(event->pos()), true);
    event->accept();
}

/*!
    \reimp
*/
void QLineEdit::mouseReleaseEvent(QMouseEvent *event) {
    m_dragging = false;
    event->accept();
}

/*!
    \reimp
*/
void QLineEdit::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || m_echoMode != Normal) {
        event->ignore();
        return;
    }
    // Select the word under the mouse: the run of characters of the same kind, spaces or not.
    const int at = std::min(cursorPositionAt(event->pos()), static_cast<int>(m_text.size()) - 1);
    if (at >= 0) {
        const bool space = m_text.at(at).isSpace();
        int start = at, end = at + 1;
        while (start > 0 && m_text.at(start - 1).isSpace() == space)
            --start;
        while (end < m_text.size() && m_text.at(end).isSpace() == space)
            ++end;
        setSelection(start, end - start);
    }
    event->accept();
}

/*!
    \reimp
*/
void QLineEdit::keyPressEvent(QKeyEvent *event) {
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    const bool control = event->modifiers() & Qt::ControlModifier;
    const int size = static_cast<int>(m_text.size());
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Right: {
        const bool left = event->key() == Qt::Key_Left;
        int target = m_cursor + (left ? -1 : 1);
        if (control) {
            // To the start of the previous word, or the end of the next one.
            target = m_cursor;
            if (left) {
                while (target > 0 && m_text.at(target - 1).isSpace())
                    --target;
                while (target > 0 && !m_text.at(target - 1).isSpace())
                    --target;
            } else {
                while (target < size && m_text.at(target).isSpace())
                    ++target;
                while (target < size && !m_text.at(target).isSpace())
                    ++target;
            }
        } else if (hasSelectedText() && !shift) {
            target = left ? selectionStart() : selectionEnd();
        }
        moveCursor(target, shift);
        break;
    }
    case Qt::Key_Home:
        home(shift);
        break;
    case Qt::Key_End:
        end(shift);
        break;
    case Qt::Key_Backspace:
        if (m_readOnly) {
            event->ignore();
            return;
        }
        backspace();
        break;
    case Qt::Key_Delete:
        if (m_readOnly) {
            event->ignore();
            return;
        }
        del();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        emit returnPressed();
        emit editingFinished();
        break;
    case Qt::Key_A:
        if (control) {
            selectAll();
            break;
        }
        [[fallthrough]];
    default: {
        const QString text = event->text();
        const bool plain = !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
        if (!m_readOnly && plain && !text.isEmpty() && text.at(0).isPrint()) {
            insert(text);
            break;
        }
        event->ignore();
        return;
    }
    }
    event->accept();
}

/*!
    \reimp
*/
void QLineEdit::focusInEvent(QFocusEvent *event) {
    (void)event;
    update();
}

/*!
    \reimp
*/
void QLineEdit::focusOutEvent(QFocusEvent *event) {
    (void)event;
    m_dragging = false;
    update();
    emit editingFinished();
}

/*!
    \reimp
*/
void QLineEdit::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        updateGeometry();
        scrollToCursor();
    }
    update();
    QWidget::changeEvent(event);
}

/*!
    \reimp
*/
void QLineEdit::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    scrollToCursor();
}

#include "moc_qlineedit.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
