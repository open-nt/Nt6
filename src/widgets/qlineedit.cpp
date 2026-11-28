// MIT License - see LICENSE
#include <QtWidgets/qlineedit.h>

#include "qwidget_p.h"
#include <QtWidgets/qmaterialstyle.h>

#include <QtGui/qaction.h>
#include <QtGui/qclipboard.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qmenu.h>
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

    Line edits: with a placeholder, with the cursor, with a selection, in password mode, read-only, and disabled.

    \image widgets/qlineedit.png QLineEdit in the light color scheme

    The usual keys cut, copy and paste through the clipboard (Ctrl+X, Ctrl+C, Ctrl+V) and undo and redo edits
    (Ctrl+Z, Ctrl+Shift+Z or Ctrl+Y), and a right click opens createStandardContextMenu().

    An input mask makes the user type text of a fixed form, such as a date or an IP address:

    \code
    QLineEdit *date = new QLineEdit(window);
    date->setInputMask("0000-00-00");
    \endcode

    \differences
    Text is not dragged and dropped (dragEnabled() is only kept), and the alignment is horizontal only. The icons of
    actions at the ends are drawn by the line edit, not shown as tool buttons, and have no tooltips or hover. The cursor
    does not blink, there is no \c PasswordEchoOnEdit, and cursorMoveStyle() is only kept, as text is not laid
    out right to left. Text is edited one code unit at a time, so characters outside the Basic Multilingual Plane
    count twice. There is no \c inputMethodEvent().
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
    \enum QLineEdit::ActionPosition
    \since 6.0

    This enum says at which end of a line edit addAction() puts an action.

    \value LeadingPosition Before the text.
    \value TrailingPosition After the text.
*/

/*!
    Constructs an empty line edit with the given \a parent.
*/
QLineEdit::QLineEdit(QWidget *parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true); // for the icon button under the pointer
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
    Returns the text. The default is an empty string. With an input mask, the blanks that the user did not fill are
    left out; the separators of the mask are kept.

    \sa setText(), displayText()
*/
QString QLineEdit::text() const { return m_mask.empty() ? m_text : strippedText(m_text); }

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
    Makes \a completer complete what the user types, or removes the completer with \c nullptr. The line edit does not
    own the completer.

    \sa completer(), QCompleter
*/
void QLineEdit::setCompleter(QCompleter *completer) {
    if (m_completer == completer)
        return;
    if (m_completer && m_completer->widget() == this)
        m_completer->setWidget(nullptr);
    m_completer = completer;
    if (completer)
        completer->setWidget(this);
}

/*!
    \fn QCompleter *QLineEdit::completer() const

    Returns the completer of the line edit, or \c nullptr.

    \sa setCompleter()
*/

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
    if (!m_mask.empty())
        return;
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
    \fn bool QLineEdit::isClearButtonEnabled() const

    Returns whether the line edit has a button that clears its text. The default is \c false.

    \sa setClearButtonEnabled()
*/

/*!
    Gives the line edit a button at its right end that clears the text if \a enable is \c true, or takes it away.
    The button shows while there is text and the line edit can be edited; a click on it clears the text and emits
    textEdited().

    \sa isClearButtonEnabled(), clear()
*/
void QLineEdit::setClearButtonEnabled(bool enable) {
    if (m_clearButton == enable)
        return;
    m_clearButton = enable;
    m_clearPressed = false;
    scrollToCursor();
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
    QtPrivate::WidgetAccess::inputHintsChanged(this);
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
    const int x = pos.x() - textOrigin();
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
    m_undo.clear();
    m_redo.clear();
    m_modified = false;
    if (!m_mask.empty()) {
        const QString shown = maskedText(text);
        applyText(shown, static_cast<int>(strippedText(shown).size()), 0);
        return;
    }
    const QString cut = text.left(m_maxLength);
    changeText(cut, static_cast<int>(cut.size()), false);
}

/*!
    Removes the text. The removal can be undone.
*/
void QLineEdit::clear() { applyText(m_mask.empty() ? QString() : maskedText(QString()), 0, Record); }

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
    if (!m_mask.empty()) {
        // The typed characters fill the blanks of the mask, from the start of the selection, which is blanked.
        QString shown = m_text;
        const auto n = static_cast<qsizetype>(m_mask.size());
        for (int i = start; i < end; ++i)
            if (!m_mask[static_cast<std::size_t>(i)].separator)
                shown[i] = m_blank;
        qsizetype pos = start;
        bool rejected = false;
        for (const QChar c : text) {
            if (pos < n && m_mask[static_cast<std::size_t>(pos)].separator &&
                c == m_mask[static_cast<std::size_t>(pos)].ch) {
                ++pos;
                continue;
            }
            qsizetype at = pos;
            while (at < n && m_mask[static_cast<std::size_t>(at)].separator)
                ++at;
            if (at >= n)
                break;
            if (!maskAccepts(at, c)) {
                rejected = true;
                continue;
            }
            const int caseMode = m_mask[static_cast<std::size_t>(at)].caseMode;
            shown[at] = caseMode == 1 ? c.toUpper() : caseMode == 2 ? c.toLower() : c;
            pos = at + 1;
        }
        while (pos < n && m_mask[static_cast<std::size_t>(pos)].separator)
            ++pos;
        if (rejected && shown == m_text) {
            emit inputRejected();
            return;
        }
        applyText(shown, static_cast<int>(pos), UserEdit | Validate | Record);
        return;
    }
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
    if (!m_mask.empty() && !hasSelectedText()) {
        int pos = m_cursor - 1;
        while (pos >= 0 && m_mask[static_cast<std::size_t>(pos)].separator)
            --pos;
        if (pos < 0)
            return;
        QString shown = m_text;
        shown[pos] = m_blank;
        applyText(shown, pos, UserEdit | Validate | Record);
        return;
    }
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
    if (!m_mask.empty() && !hasSelectedText()) {
        qsizetype pos = m_cursor;
        while (pos < m_text.size() && m_mask[static_cast<std::size_t>(pos)].separator)
            ++pos;
        if (pos >= m_text.size())
            return;
        QString shown = m_text;
        shown[pos] = m_blank;
        applyText(shown, m_cursor, UserEdit | Validate | Record);
        return;
    }
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

QRect QLineEdit::textArea() const {
    QStyleOptionFrame option;
    initStyleOption(&option);
    return style()->subElementRect(QStyle::SE_LineEditContents, &option, this);
}

int QLineEdit::sideShift() const {
    if (const auto *material = qobject_cast<const QMaterialStyle *>(style()))
        return material->fieldIconShift(this);
    return style()->pixelMetric(QStyle::PM_LineEditIconMargin, nullptr, this) / 4;
}

int QLineEdit::sideInset() const {
    const auto *material = qobject_cast<const QMaterialStyle *>(style());
    return material ? material->fieldIconInset(this) : 0;
}

// An avatar at the trailing end goes past the clear button, at the edge.
bool QLineEdit::isAvatar(const QAction *action) const {
    return qobject_cast<const QMaterialStyle *>(style()) && QMaterialStyle::isFieldAvatar(action);
}

// The prefix or the suffix that the Material style shows beside the text.
QString QLineEdit::affix(bool suffix) const {
    const auto *material = qobject_cast<const QMaterialStyle *>(style());
    return material ? material->fieldAffix(this, suffix) : QString();
}

std::vector<QLineEdit::Side> QLineEdit::sides() const {
    std::vector<Side> out;
    const QRect area = textArea();
    const int icon = style()->pixelMetric(QStyle::PM_LineEditIconSize, nullptr, this);
    const int slot = icon + style()->pixelMetric(QStyle::PM_LineEditIconMargin, nullptr, this);
    const int shift = sideShift();
    const int inset = sideInset();
    const int top = area.top() + (area.height() - icon) / 2;
    int left = area.left() - shift;
    int right = area.right() + 1 + shift;
    // Avatars at the trailing end first, at the edge; then the clear button, and the other actions.
    for (const bool avatars : {true, false}) {
        if (!avatars && m_clearButton)
            right -= clearButtonRect().width();
        for (const auto &[action, position] : m_sideActions) {
            if (!action || !action->isVisible() || (position == TrailingPosition && isAvatar(action)) != avatars)
                continue;
            Side side{action.data(), position, QRect(), QRect()};
            if (position == LeadingPosition) {
                side.slot = QRect(left, area.top(), slot, area.height());
                side.icon = QRect(left + inset, top, icon, icon);
                left += slot;
            } else {
                right -= slot;
                side.slot = QRect(right, area.top(), slot, area.height());
                side.icon = QRect(right + slot - inset - icon, top, icon, icon);
            }
            out.push_back(side);
        }
    }
    return out;
}

QRect QLineEdit::contentsRect() const {
    QRect contents = textArea();
    if (m_clearButton)
        contents.setRight(contents.right() - clearButtonRect().width());
    for (const Side &side : sides()) {
        if (side.position == LeadingPosition)
            contents.setLeft(std::max(contents.left(), side.slot.right() + 1));
        else
            contents.setRight(std::min(contents.right(), side.slot.left() - 1));
    }
    const QFontMetrics fm = fontMetrics();
    if (const QString before = affix(false); !before.isEmpty())
        contents.setLeft(contents.left() + fm.horizontalAdvance(before));
    if (const QString after = affix(true); !after.isEmpty())
        contents.setRight(contents.right() - fm.horizontalAdvance(after) - fm.horizontalAdvance(QLatin1Char(' ')));
    return contents.marginsRemoved(m_textMargins);
}

/*!
    \since 6.0

    Adds \a action at the start or the end of the line edit, as \a position says, and to the actions of the widget. Its
    icon is shown there while the action is visible, and a click on it triggers the action. Actions at one end are
    shown in the order they were added, from the edge inward; the text is between them.

    \code
    QAction *search = edit->addAction(QIcon(":/search.svg"), QLineEdit::LeadingPosition);
    connect(search, &QAction::triggered, this, &Window::find);
    \endcode

    \sa QWidget::removeAction()
*/
void QLineEdit::addAction(QAction *action, ActionPosition position) {
    if (!action)
        return;
    for (auto &entry : m_sideActions) {
        if (entry.first == action) {
            entry.second = position;
            update();
            return;
        }
    }
    m_sideActions.emplace_back(action, position);
    QWidget::addAction(action);
    scrollToCursor();
    update();
}

/*!
    \since 6.0
    \overload

    Makes an action with \a icon, owned by the line edit, adds it at \a position and returns it.
*/
QAction *QLineEdit::addAction(const QIcon &icon, ActionPosition position) {
    auto *action = new QAction(icon, QString(), this);
    addAction(action, position);
    return action;
}

/*!
    \reimp

    An action at an end that is removed from the widget leaves it, and one that changes is shown as it now is.
*/
void QLineEdit::actionEvent(QActionEvent *event) {
    if (event->type() == QEvent::ActionRemoved) {
        QAction *gone = event->action();
        m_sideActions.erase(std::remove_if(m_sideActions.begin(), m_sideActions.end(),
                                           [gone](const auto &entry) { return !entry.first || entry.first == gone; }),
                            m_sideActions.end());
    }
    scrollToCursor();
    update();
    QWidget::actionEvent(event);
}

/*!
    \since 6.0

    Fills \a option with the state of the line edit, for the style to draw its frame.
*/
void QLineEdit::initStyleOption(QStyleOptionFrame *option) const {
    if (!option)
        return;
    option->initFrom(this);
    option->lineWidth = m_frame ? style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, this) : 0;
    option->midLineWidth = 0;
    option->state |= QStyle::State_Sunken;
    if (m_readOnly)
        option->state |= QStyle::State_ReadOnly;
}

int QLineEdit::textOrigin() const {
    const QRect contents = contentsRect();
    const int free = contents.width() - xOf(static_cast<int>(m_text.size())) - 1;
    int offset = 0;
    if (free > 0) {
        const Qt::Alignment h = QStyle::visualAlignment(layoutDirection(), m_alignment) & Qt::AlignHorizontal_Mask;
        if (h & Qt::AlignRight)
            offset = free;
        else if (h & Qt::AlignHCenter)
            offset = free / 2;
    }
    return contents.left() - m_scroll + offset;
}

/*!
    \since 6.0

    Returns the rectangle of the text cursor, in the coordinates of the line edit.
*/
QRect QLineEdit::cursorRect() const {
    const QRect contents = contentsRect();
    const QFontMetrics fm = fontMetrics();
    const int top = contents.center().y() - fm.height() / 2;
    return QRect(textOrigin() + xOf(m_cursor) - 1, top, 3, fm.height());
}

// Where the clear button is: at the right end of the text area, as the outermost of the actions there.
QRect QLineEdit::clearButtonRect() const {
    const QRect text = textArea();
    const int icon = style()->pixelMetric(QStyle::PM_LineEditIconSize, nullptr, this);
    const int margin = style()->pixelMetric(QStyle::PM_LineEditIconMargin, nullptr, this);
    const int width = std::min(icon + margin, std::max(0, text.width() / 2));
    int avatars = 0; // past which it is
    for (const auto &[action, position] : m_sideActions)
        if (action && action->isVisible() && position == TrailingPosition && isAvatar(action))
            avatars += icon + margin;
    return QRect(text.right() + 1 + sideShift() - avatars - width, text.top(), width, text.height());
}

bool QLineEdit::clearButtonShown() const { return m_clearButton && !m_text.isEmpty() && !m_readOnly && isEnabled(); }

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

void QLineEdit::changeText(const QString &newText, int cursor, bool userEdit) {
    applyText(newText, cursor, userEdit ? UserEdit | Validate | Record : 0);
}

void QLineEdit::applyText(const QString &newText, int cursor, int flags) {
    const bool userEdit = flags & UserEdit;
    QString text = newText;
    if ((flags & Validate) && m_validator && m_mask.empty()) {
        // The validator may change the text, for example to upper case, and the cursor.
        if (m_validator->validate(text, cursor) == QValidator::Invalid) {
            emit inputRejected();
            return;
        }
    }
    const bool changed = text != m_text;
    const int oldCursor = m_cursor;
    const bool hadSelection = hasSelectedText();
    if (changed && (flags & Record)) {
        m_undo.push_back({m_text, m_cursor});
        m_redo.clear();
    }
    if (changed && userEdit)
        m_modified = true;
    m_text = text;
    m_cursor = m_anchor = std::clamp(cursor, 0, static_cast<int>(m_text.size()));
    scrollToCursor();
    update();
    if (changed) {
        const QString plain = this->text();
        emit textChanged(plain);
        if (userEdit) {
            emit textEdited(plain);
            if (m_completer)
                m_completer->textEdited(plain, m_deleting);
        }
    }
    if (oldCursor != m_cursor)
        emit cursorPositionChanged(oldCursor, m_cursor);
    if (hadSelection)
        emit selectionChanged();
}

bool QLineEdit::finishEditing() {
    if (hasAcceptableInput() || !m_validator)
        return hasAcceptableInput();
    QString fixed = m_text;
    m_validator->fixup(fixed);
    if (fixed != m_text)
        changeText(fixed, static_cast<int>(fixed.size()), true);
    return hasAcceptableInput();
}

/*!
    \since 6.0

    Sets the validator that checks the text the user types to \a validator, or removes it if \a validator is
    \c nullptr. The line edit does not take ownership of it.

    Edits that make the text QValidator::Invalid are refused, and inputRejected() is emitted. Return emits
    returnPressed() and editingFinished() only when the text is acceptable, after QValidator::fixup() if it is not.
    setText() is not checked.

    \sa hasAcceptableInput()
*/
void QLineEdit::setValidator(const QValidator *validator) { m_validator = validator; }

/*!
    \fn const QValidator *QLineEdit::validator() const

    Returns the validator of the line edit, or \c nullptr if it has none.
*/

/*!
    \since 6.0

    Returns \c true if the text is acceptable to the validator(), or if there is no validator.
*/
bool QLineEdit::hasAcceptableInput() const {
    for (std::size_t i = 0; i < m_mask.size() && i < static_cast<std::size_t>(m_text.size()); ++i) {
        const MaskChar &m = m_mask[i];
        if (m.separator)
            continue;
        const QChar c = m_text.at(static_cast<qsizetype>(i));
        const bool required = m.ch.isUpper() || m.ch == QLatin1Char('9');
        if (c == m_blank ? required : !maskAccepts(static_cast<qsizetype>(i), c))
            return false;
    }
    if (!m_validator)
        return true;
    QString text = this->text();
    int pos = m_cursor;
    return m_validator->validate(text, pos) == QValidator::Acceptable;
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
    const QSize margins(m_textMargins.left() + m_textMargins.right(), m_textMargins.top() + m_textMargins.bottom());
    return style()->sizeFromContents(QStyle::CT_LineEdit, &option,
                                     QSize(fm.horizontalAdvance(QChar('x')) * 17, fm.height()) + margins, this);
}

/*!
    \reimp
*/
QSize QLineEdit::minimumSizeHint() const {
    QStyleOptionFrame option;
    option.initFrom(this);
    const QFontMetrics fm = fontMetrics();
    const QSize margins(m_textMargins.left() + m_textMargins.right(), m_textMargins.top() + m_textMargins.bottom());
    return style()->sizeFromContents(QStyle::CT_LineEdit, &option,
                                     QSize(fm.horizontalAdvance(QChar('x')) * 4, fm.height()) + margins, this);
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
    const int origin = textOrigin();
    const QRect textRect(origin, top, xOf(static_cast<int>(m_text.size())) + 4, fm.height());
    painter.setClipRect(contents);
    // In the Material style, a label that rests stands where the placeholder would.
    const auto *material = qobject_cast<const QMaterialStyle *>(style());
    if (m_text.isEmpty() && !(material && material->hidesPlaceholder(this))) {
        painter.setPen(palette().color(group, QPalette::PlaceholderText));
        const Qt::Alignment h = QStyle::visualAlignment(layoutDirection(), m_alignment) & Qt::AlignHorizontal_Mask;
        painter.drawText(QRect(contents.left(), top, contents.width(), fm.height()),
                         int(h | Qt::AlignVCenter) | Qt::TextSingleLine, m_placeholder);
    }
    painter.setPen(palette().color(group, QPalette::Text));
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextDontClip, shown);
    if (hasSelectedText() && m_echoMode != NoEcho) {
        const QRect selection(origin + xOf(selectionStart()), top,
                              xOf(selectionEnd()) - xOf(selectionStart()), fm.height());
        painter.fillRect(selection, palette().color(group, QPalette::Highlight));
        painter.setClipRect(selection.intersected(contents));
        painter.setPen(palette().color(group, QPalette::HighlightedText));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextDontClip, shown);
        painter.setClipRect(contents);
    }
    if (hasFocus() && !m_readOnly) {
        // As wide as the style says, and in the Material style in its accent color.
        painter.fillRect(origin + xOf(m_cursor), top,
                         std::max(1, style()->pixelMetric(QStyle::PM_TextCursorWidth, nullptr, this)), fm.height(),
                         material ? material->caretColor(this) : palette().color(group, QPalette::Text));
    }
    painter.setClipping(false);
    // A prefix just before the text, and a suffix at the end, in the color of the placeholder.
    const QString before = affix(false), after = affix(true);
    if (!before.isEmpty() || !after.isEmpty()) {
        painter.setPen(palette().color(group, QPalette::PlaceholderText));
        const int beforeWidth = fm.horizontalAdvance(before);
        if (!before.isEmpty())
            painter.drawText(QRect(contents.left() - beforeWidth, top, beforeWidth, fm.height()),
                             Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, before);
        if (!after.isEmpty())
            painter.drawText(
                QRect(contents.right() + 1, top, fm.horizontalAdvance(QLatin1Char(' ') + after), fm.height()),
                Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, after);
    }
    for (const Side &side : sides()) {
        const bool usable = side.action->isEnabled() && isEnabled();
        const QIcon::Mode mode = usable ? QIcon::Normal : QIcon::Disabled;
        if (material)
            material->drawFieldIcon(&painter, side.slot, side.icon, side.action, mode, this,
                                    side.position == TrailingPosition, usable && side.action == m_sideHovered.data(),
                                    usable && side.action == m_sidePressed.data());
        else
            side.action->icon().paint(&painter, side.icon, Qt::AlignCenter, mode);
    }
    if (clearButtonShown() && material) {
        const QRect area = clearButtonRect();
        const int icon = style()->pixelMetric(QStyle::PM_LineEditIconSize, nullptr, this);
        material->drawClearButton(
            &painter, area,
            QRect(area.right() + 1 - sideInset() - icon, area.top() + (area.height() - icon) / 2, icon, icon), this,
            m_clearHovered, m_clearPressed);
    } else if (clearButtonShown()) {
        painter.setClipping(false);
        QStyleOption iconOption;
        iconOption.initFrom(this);
        const QPixmap cross = style()->standardPixmap(QStyle::SP_LineEditClearButton, &iconOption, this);
        style()->drawItemPixmap(&painter, clearButtonRect(), Qt::AlignCenter, cross);
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
    auto *material = qobject_cast<QMaterialStyle *>(style());
    if (clearButtonShown() && clearButtonRect().contains(event->pos())) {
        m_clearPressed = true;
        if (material)
            material->pressFieldIcon(this, event->position());
        update();
        event->accept();
        return;
    }
    for (const Side &side : sides()) {
        if (side.slot.contains(event->pos())) {
            m_sidePressed = side.action;
            if (material && side.action->isEnabled())
                material->pressFieldIcon(this, event->position());
            update();
            event->accept();
            return;
        }
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
        updateHover(event->pos());
        event->accept();
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
    // A click on the icon of an action triggers it, if it is let go there.
    if (QAction *pressed = m_sidePressed.data()) {
        m_sidePressed.clear();
        for (const Side &side : sides()) {
            if (side.action == pressed && event->button() == Qt::LeftButton && side.slot.contains(event->pos()) &&
                pressed->isEnabled())
                pressed->trigger();
        }
        update();
        event->accept();
        return;
    }
    // A click on the clear button clears the text as if the user had deleted it.
    if (m_clearPressed) {
        m_clearPressed = false;
        if (event->button() == Qt::LeftButton && clearButtonShown() && clearButtonRect().contains(event->pos()))
            changeText(QString(), 0, true);
        updateHover(event->pos());
        update();
    }
    event->accept();
}

void QLineEdit::updateHover(const QPoint &at) {
    const bool clear = clearButtonShown() && clearButtonRect().contains(at);
    QAction *over = nullptr;
    if (!clear && isEnabled()) {
        for (const Side &side : sides()) {
            if (side.slot.contains(at) && side.action->isEnabled())
                over = side.action;
        }
    }
    if (over == m_sideHovered.data() && clear == m_clearHovered)
        return;
    m_sideHovered = over;
    m_clearHovered = clear;
    update();
}

/*!
    \reimp

    The icon button that the pointer was over shows that it no longer is.
*/
void QLineEdit::leaveEvent(QEvent *event) {
    if (m_sideHovered || m_clearHovered) {
        m_sideHovered.clear();
        m_clearHovered = false;
        update();
    }
    QWidget::leaveEvent(event);
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
    if (m_completer && m_completer->handleKey(event))
        return;
    m_deleting = event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete;
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    const bool control = event->modifiers() & Qt::ControlModifier;
    const struct {
        void (QLineEdit::*edit)();
        QKeySequence::StandardKey key;
        bool changesText;
    } editKeys[] = {
        {&QLineEdit::cut, QKeySequence::Cut, true},   {&QLineEdit::paste, QKeySequence::Paste, true},
        {&QLineEdit::undo, QKeySequence::Undo, true}, {&QLineEdit::redo, QKeySequence::Redo, true},
        {&QLineEdit::selectAll, QKeySequence::SelectAll, false},
    };
    if (event->matches(QKeySequence::Copy)) {
        copy();
        event->accept();
        return;
    }
    for (const auto &k : editKeys) {
        if (event->matches(k.key)) {
            if (k.changesText && m_readOnly) {
                event->ignore();
                return;
            }
            (this->*k.edit)();
            event->accept();
            return;
        }
    }
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_Right: {
        const bool left = event->key() == Qt::Key_Left;
        int target = m_cursor + (left ? -1 : 1);
        if (control) {
            // To the start of the previous word, or the end of the next one.
            target = left ? wordStart(m_cursor) : wordEnd(m_cursor);
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
        if (finishEditing()) {
            emit returnPressed();
            emit editingFinished();
        }
        // As in Qt the key goes on to the parent, so that a dialog can click its default button.
        event->ignore();
        return;
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

int QLineEdit::wordStart(int from) const {
    int target = from;
    while (target > 0 && m_text.at(target - 1).isSpace())
        --target;
    while (target > 0 && !m_text.at(target - 1).isSpace())
        --target;
    return target;
}

int QLineEdit::wordEnd(int from) const {
    const int size = static_cast<int>(m_text.size());
    int target = from;
    while (target < size && m_text.at(target).isSpace())
        ++target;
    while (target < size && !m_text.at(target).isSpace())
        ++target;
    return target;
}

/*!
    \since 6.0

    Moves the cursor \a steps characters forward, selecting what it moves over if \a mark is \c true.

    \sa cursorBackward(), cursorWordForward()
*/
void QLineEdit::cursorForward(bool mark, int steps) { moveCursor(m_cursor + steps, mark); }

/*!
    \since 6.0

    Moves the cursor \a steps characters back, selecting what it moves over if \a mark is \c true.

    \sa cursorForward()
*/
void QLineEdit::cursorBackward(bool mark, int steps) { moveCursor(m_cursor - steps, mark); }

/*!
    \since 6.0

    Moves the cursor to the end of the next word, selecting what it moves over if \a mark is \c true.

    \sa cursorWordBackward()
*/
void QLineEdit::cursorWordForward(bool mark) { moveCursor(wordEnd(m_cursor), mark); }

/*!
    \since 6.0

    Moves the cursor to the start of the previous word, selecting what it moves over if \a mark is \c true.

    \sa cursorWordForward()
*/
void QLineEdit::cursorWordBackward(bool mark) { moveCursor(wordStart(m_cursor), mark); }

/*!
    \fn int QLineEdit::selectionLength() const
    \since 6.0

    Returns the number of selected characters.

    \sa selectionStart(), selectionEnd()
*/

/*!
    \fn Qt::Alignment QLineEdit::alignment() const
    \since 6.0

    Returns the alignment of the text. The default is Qt::AlignLeft | Qt::AlignVCenter.

    \sa setAlignment()
*/

/*!
    \since 6.0

    Aligns the text to \a flag: left, right or centered while it is narrower than the line edit.

    \sa alignment()
*/
void QLineEdit::setAlignment(Qt::Alignment flag) {
    m_alignment = flag;
    update();
}

/*!
    \fn bool QLineEdit::isModified() const
    \since 6.0

    Returns \c true if the user edited the text since setText() or setModified(false).

    \sa setModified()
*/

/*!
    \fn void QLineEdit::setModified(bool modified)
    \since 6.0

    Sets whether the text counts as edited by the user to \a modified.

    \sa isModified()
*/

/*!
    \fn bool QLineEdit::isUndoAvailable() const
    \since 6.0

    Returns \c true if there is an edit that undo() can undo.
*/

/*!
    \fn bool QLineEdit::isRedoAvailable() const
    \since 6.0

    Returns \c true if there is an edit that redo() can redo.
*/

/*!
    \fn QString QLineEdit::inputMask() const
    \since 6.0

    Returns the input mask, or an empty string if there is none.

    \sa setInputMask()
*/

/*!
    \since 6.0

    Sets the input mask to \a inputMask, or removes it with an empty string. Each character of the mask stands for
    one character of the text:

    \table
    \header \li Mask \li Character
    \row \li \c A, \c a \li a letter; \c A requires one, \c a allows one
    \row \li \c N, \c n \li a letter or digit
    \row \li \c X, \c x \li any character
    \row \li \c 9, \c 0 \li a digit
    \row \li \c D, \c d \li a digit from 1 to 9
    \row \li \c # \li a digit or a sign, allowed
    \row \li \c H, \c h \li a hexadecimal digit
    \row \li \c B, \c b \li a binary digit
    \endtable

    \c > and \c < make the following letters upper or lower case, \c ! stops that, \c \\ makes the next
    character a literal, and other characters are separators that the user does not type. A \c ; and a character
    at the end name the blank that stands for a character not typed yet, a space by default.

    \sa inputMask(), hasAcceptableInput()
*/
void QLineEdit::setInputMask(const QString &inputMask) {
    if (inputMask == m_inputMask)
        return;
    const QString plain = text();
    m_inputMask = inputMask;
    m_mask.clear();
    m_blank = QLatin1Char(' ');
    QString mask = inputMask;
    if (const auto semicolon = mask.lastIndexOf(QLatin1Char(';')); semicolon >= 0 && semicolon == mask.size() - 2) {
        m_blank = mask.at(semicolon + 1);
        mask.truncate(semicolon);
    } else if (semicolon >= 0 && semicolon == mask.size() - 1) {
        mask.truncate(semicolon);
    }
    int caseMode = 0;
    for (qsizetype i = 0; i < mask.size(); ++i) {
        const QChar c = mask.at(i);
        if (c == QLatin1Char('\\') && i + 1 < mask.size()) {
            m_mask.push_back({mask.at(++i), true, 0});
        } else if (c == QLatin1Char('>')) {
            caseMode = 1;
        } else if (c == QLatin1Char('<')) {
            caseMode = 2;
        } else if (c == QLatin1Char('!')) {
            caseMode = 0;
        } else {
            const bool input = QStringLiteral("AaNnXx90Dd#HhBb").contains(c);
            m_mask.push_back({c, !input, input ? caseMode : 0});
        }
    }
    m_undo.clear();
    m_redo.clear();
    if (m_mask.empty())
        applyText(plain.left(m_maxLength), static_cast<int>(std::min<qsizetype>(plain.size(), m_maxLength)), 0);
    else
        applyText(maskedText(plain), 0, 0);
}

bool QLineEdit::maskAccepts(qsizetype position, QChar c) const {
    switch (m_mask[static_cast<std::size_t>(position)].ch.unicode()) {
    case 'A':
    case 'a':
        return c.isLetter();
    case 'N':
    case 'n':
        return c.isLetterOrNumber();
    case 'X':
    case 'x':
        return c.isPrint();
    case '9':
    case '0':
        return c.isDigit();
    case 'D':
    case 'd':
        return c.isDigit() && c != QLatin1Char('0');
    case '#':
        return c.isDigit() || c == QLatin1Char('+') || c == QLatin1Char('-');
    case 'H':
    case 'h':
        return c.isDigit() || (c.toLower() >= QLatin1Char('a') && c.toLower() <= QLatin1Char('f'));
    case 'B':
    case 'b':
        return c == QLatin1Char('0') || c == QLatin1Char('1');
    default:
        return false;
    }
}

// The text as the mask shows it: the separators in place, `plain` in the other positions, blanks where it runs out.
QString QLineEdit::maskedText(const QString &plain) const {
    QString shown;
    qsizetype p = 0;
    for (std::size_t i = 0; i < m_mask.size(); ++i) {
        const MaskChar &m = m_mask[i];
        if (m.separator) {
            shown += m.ch;
            if (p < plain.size() && plain.at(p) == m.ch)
                ++p;
            continue;
        }
        while (p < plain.size() && plain.at(p) != m_blank && !maskAccepts(static_cast<qsizetype>(i), plain.at(p)))
            ++p;
        if (p < plain.size() && plain.at(p) != m_blank) {
            const QChar c = plain.at(p++);
            shown += m.caseMode == 1 ? c.toUpper() : m.caseMode == 2 ? c.toLower() : c;
        } else {
            if (p < plain.size())
                ++p;
            shown += m_blank;
        }
    }
    return shown;
}

// The text without the blanks of the mask.
QString QLineEdit::strippedText(const QString &shown) const {
    QString plain;
    for (qsizetype i = 0; i < shown.size() && i < static_cast<qsizetype>(m_mask.size()); ++i) {
        if (m_mask[static_cast<std::size_t>(i)].separator || shown.at(i) != m_blank)
            plain += shown.at(i);
    }
    return plain;
}

/*!
    \fn bool QLineEdit::dragEnabled() const
    \since 6.0

    Returns whether the selected text can be dragged. Nt does not drag text; the value is only kept.
*/

/*!
    \fn void QLineEdit::setDragEnabled(bool b)
    \since 6.0

    Sets whether the selected text can be dragged to \a b.
*/

/*!
    \fn Qt::CursorMoveStyle QLineEdit::cursorMoveStyle() const
    \since 6.0

    Returns how the cursor moves in text that mixes directions.
*/

/*!
    \fn void QLineEdit::setCursorMoveStyle(Qt::CursorMoveStyle style)
    \since 6.0

    Sets how the cursor moves to \a style. Nt lays text out left to right, so this only keeps the value.
*/

/*!
    \fn void QLineEdit::setTextMargins(int left, int top, int right, int bottom)
    \since 6.0
    \overload

    Sets the margins around the text to \a left, \a top, \a right and \a bottom pixels.
*/

/*!
    \since 6.0

    Sets the margins between the frame and the text to \a margins.

    \sa textMargins()
*/
void QLineEdit::setTextMargins(const QMargins &margins) {
    m_textMargins = margins;
    updateGeometry();
    scrollToCursor();
    update();
}

/*!
    \fn QMargins QLineEdit::textMargins() const
    \since 6.0

    Returns the margins between the frame and the text.

    \sa setTextMargins()
*/

/*!
    \since 6.0

    Copies the selected text to the clipboard and deletes it. Nothing happens in the Password and NoEcho modes, or
    if the line edit is read-only.

    \sa copy(), paste()
*/
void QLineEdit::cut() {
    if (!hasSelectedText() || m_echoMode != Normal || m_readOnly)
        return;
    copy();
    insert(QString());
}

/*!
    \since 6.0

    Copies the selected text to the clipboard. Nothing happens in the Password and NoEcho modes.

    \sa cut(), paste()
*/
void QLineEdit::copy() const {
    if (!hasSelectedText() || m_echoMode != Normal)
        return;
    if (QClipboard *clipboard = QGuiApplication::clipboard())
        clipboard->setText(selectedText());
}

/*!
    \since 6.0

    Inserts the text of the clipboard at the cursor, in place of the selection. Line breaks become spaces.
    Nothing happens if the line edit is read-only.

    \sa copy(), cut()
*/
void QLineEdit::paste() {
    if (m_readOnly)
        return;
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return;
    QString text = clipboard->text();
    text.replace(QLatin1String("\r\n"), QLatin1String(" "));
    text.replace(QLatin1Char('\n'), QLatin1Char(' '));
    text.replace(QLatin1Char('\r'), QLatin1Char(' '));
    if (!text.isEmpty() || hasSelectedText())
        insert(text);
}

/*!
    \since 6.0

    Undoes the last edit, if there is one.

    \sa redo(), isUndoAvailable()
*/
void QLineEdit::undo() {
    if (m_readOnly || m_undo.empty())
        return;
    Edit last = std::move(m_undo.back());
    m_undo.pop_back();
    m_redo.push_back({m_text, m_cursor});
    std::vector<Edit> redo = std::move(m_redo);
    applyText(last.text, last.cursor, UserEdit);
    m_redo = std::move(redo);
}

/*!
    \since 6.0

    Redoes the last edit that undo() undid.

    \sa undo(), isRedoAvailable()
*/
void QLineEdit::redo() {
    if (m_readOnly || m_redo.empty())
        return;
    Edit next = std::move(m_redo.back());
    m_redo.pop_back();
    m_undo.push_back({m_text, m_cursor});
    std::vector<Edit> redo = std::move(m_redo);
    applyText(next.text, next.cursor, UserEdit);
    m_redo = std::move(redo);
}

/*!
    \since 6.0

    Returns a new menu with the standard editing actions: Undo, Redo, Cut, Copy, Paste, Delete and Select All,
    each enabled when it applies. The caller owns the menu.

    \code
    void MyLineEdit::contextMenuEvent(QContextMenuEvent *event) {
        QMenu *menu = createStandardContextMenu();
        menu->addAction(tr("Insert Date"), this, &MyLineEdit::insertDate);
        menu->exec(event->globalPos());
        delete menu;
    }
    \endcode
*/
QMenu *QLineEdit::createStandardContextMenu() {
    auto *menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("qt_edit_menu"));
    const bool canCopy = hasSelectedText() && m_echoMode == Normal;
    const auto add = [&](const QString &text, QKeySequence::StandardKey key, bool enabled, auto slot) {
        QAction *action = menu->addAction(text);
        action->setShortcut(QKeySequence(key));
        action->setEnabled(enabled);
        QObject::connect(action, &QAction::triggered, this, slot);
        return action;
    };
    if (!m_readOnly) {
        add(tr("&Undo"), QKeySequence::Undo, isUndoAvailable(), [this] { undo(); });
        add(tr("&Redo"), QKeySequence::Redo, isRedoAvailable(), [this] { redo(); });
        menu->addSeparator();
        add(tr("Cu&t"), QKeySequence::Cut, canCopy, [this] { cut(); });
    }
    add(tr("&Copy"), QKeySequence::Copy, canCopy, [this] { copy(); });
    if (!m_readOnly) {
        const QClipboard *clipboard = QGuiApplication::clipboard();
        add(tr("&Paste"), QKeySequence::Paste, clipboard && !clipboard->text().isEmpty(), [this] { paste(); });
        add(tr("Delete"), QKeySequence::Delete, hasSelectedText(), [this] { insert(QString()); });
    }
    menu->addSeparator();
    add(tr("Select All"), QKeySequence::SelectAll, !m_text.isEmpty() && selectionLength() != m_text.size(),
        [this] { selectAll(); });
    return menu;
}

/*!
    \reimp
*/
void QLineEdit::contextMenuEvent(QContextMenuEvent *event) {
    QMenu *menu = createStandardContextMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(event->globalPos());
    event->accept();
}

/*!
    \reimp
*/
QVariant QLineEdit::inputMethodQuery(Qt::InputMethodQuery property) const {
    switch (property) {
    case Qt::ImHints: {
        // As in Qt, a field that hides what is typed says so, whatever hints it was given.
        Qt::InputMethodHints hints = inputMethodHints();
        if (m_echoMode != Normal)
            hints |= Qt::InputMethodHints(Qt::ImhHiddenText) | Qt::ImhSensitiveData | Qt::ImhNoPredictiveText |
                     Qt::ImhNoAutoUppercase;
        return QVariant(static_cast<int>(hints.toInt()));
    }
    case Qt::ImEnabled:
        return QVariant(isEnabled() && !m_readOnly);
    case Qt::ImCursorRectangle:
        return QVariant(cursorRect());
    case Qt::ImFont:
        return QVariant::fromValue(font());
    case Qt::ImCursorPosition:
        return QVariant(m_cursor);
    case Qt::ImSurroundingText:
        return QVariant(m_text);
    case Qt::ImCurrentSelection:
        return QVariant(selectedText());
    case Qt::ImMaximumTextLength:
        return QVariant(m_maxLength);
    case Qt::ImAnchorPosition:
        return QVariant(m_anchor);
    default:
        return QWidget::inputMethodQuery(property);
    }
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
    if (finishEditing())
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

/*!
    \reimp

    Inserts the commit string of \a event at the cursor, in the place of the selection or of the characters the
    event replaces. The preedit string is not shown.
*/
void QLineEdit::inputMethodEvent(QInputMethodEvent *event) {
    if (m_readOnly) {
        event->ignore();
        return;
    }
    if (event->replacementLength() > 0 && !hasSelectedText()) {
        const int from = std::clamp(m_cursor + event->replacementStart(), 0, static_cast<int>(m_text.size()));
        setSelection(from, event->replacementLength());
    }
    if (!event->commitString().isEmpty() || hasSelectedText())
        insert(event->commitString());
    event->accept();
}

#include "moc_qlineedit.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
