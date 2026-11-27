// MIT License - see LICENSE
#include <QtWidgets/qabstractspinbox.h>

#include "gui/qpa/qplatformintegration_p.h"
#include "gui/qwindow_p.h"

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>
#include <functional>

namespace {

// The line edit of a spin box: it leaves the keys that step the value to the spin box, and refuses the characters
// that the spin box does not take.
class SpinEdit : public QLineEdit {
public:
    // `number` gives where the number begins and ends in the text, between the prefix and the suffix; false when the
    // text is not made of those.
    SpinEdit(QAbstractSpinBox *box, std::function<bool(QChar)> accepts, std::function<bool(int *, int *)> number)
        : QLineEdit(box), m_accepts(std::move(accepts)), m_number(std::move(number)) {}

protected:
    void keyPressEvent(QKeyEvent *event) override {
        // Select All is of the number, as in Qt.
        if (event->matches(QKeySequence::SelectAll)) {
            if (auto *box = qobject_cast<QAbstractSpinBox *>(parentWidget())) {
                box->selectAll();
                event->accept();
                return;
            }
        }
        switch (event->key()) {
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            event->ignore();
            return;
        case Qt::Key_Backspace:
        case Qt::Key_Delete: {
            // Neither takes anything of the prefix or the suffix.
            int begin = 0, end = 0;
            if (!hasSelectedText() && m_number(&begin, &end) &&
                cursorPosition() == (event->key() == Qt::Key_Backspace ? begin : end)) {
                event->accept();
                return;
            }
            break;
        }
        default:
            break;
        }
        const QString text = event->text();
        const bool plain = !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
        if (plain && !text.isEmpty() && text.at(0).isPrint() && !m_accepts(text.at(0))) {
            event->accept();
            return;
        }
        QLineEdit::keyPressEvent(event);
    }
    void focusInEvent(QFocusEvent *event) override {
        QLineEdit::focusInEvent(event);
        if (parentWidget())
            parentWidget()->update();
    }
    void focusOutEvent(QFocusEvent *event) override {
        QLineEdit::focusOutEvent(event);
        if (parentWidget())
            parentWidget()->update();
    }

private:
    std::function<bool(QChar)> m_accepts;
    std::function<bool(int *, int *)> m_number;
};

} // namespace

/*!
    \class QAbstractSpinBox
    \brief The QAbstractSpinBox class is the base of the widgets that show a value in a text with two buttons that step
    it.

    A spin box has a line edit and a button to step up and one to step down. The arrow keys, Page Up and Page Down,
    and the mouse wheel step too. QSpinBox holds an integer and QDoubleSpinBox a floating-point number; a subclass of
    QAbstractSpinBox that holds another kind of value reimplements stepBy() and stepEnabled().

    When the value is the smallest, the spin box can show a word instead of the number:

    \code
    QSpinBox *delay = new QSpinBox(window);
    delay->setRange(0, 60);
    delay->setSuffix(" s");
    delay->setSpecialValueText("Off");
    \endcode

    Spin boxes, built on QAbstractSpinBox, with a prefix, a suffix, the keyboard focus and disabled.

    \image widgets/qspinbox.png QSpinBox in the light color scheme

    \differences
    There is no \c alignment, \c accelerated, \c correctionMode, \c frame or
    \c showGroupSeparator, and no \c validate() or \c fixup(): the subclasses check what the user types. The buttons
    do not repeat while they are held. A subclass outside Nt can reimplement stepBy() and stepEnabled(), but the text
    is always that of a QSpinBox or a QDoubleSpinBox.
    \enddifferences

    \sa QSpinBox, QDoubleSpinBox
*/

/*!
    \enum QAbstractSpinBox::ButtonSymbols
    \since 6.0

    What the buttons of a spin box show.

    \value UpDownArrows Arrows.
    \value PlusMinus A plus and a minus sign.
    \value NoButtons There are no buttons.
*/

/*!
    \enum QAbstractSpinBox::StepEnabledFlag

    Which ways the spin box can step, as returned by stepEnabled().

    \value StepNone Neither.
    \value StepUpEnabled Up.
    \value StepDownEnabled Down.
*/

/*!
    Constructs a spin box with the given \a parent.
*/
QAbstractSpinBox::QAbstractSpinBox(QWidget *parent)
    : QWidget(parent), m_edit(new SpinEdit(
                           this, [this](QChar c) { return acceptsCharacter(c) || m_special.contains(c); },
                           [this](int *begin, int *end) { return numberRange(begin, end); })) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_edit->setFrame(false);
    connectEdit();
    layoutEdit();
}

bool QAbstractSpinBox::numberRange(int *begin, int *end) const {
    const QString text = m_edit->text();
    if ((!m_special.isEmpty() && text == m_special) || text.size() < m_prefix.size() + m_suffix.size() ||
        !text.startsWith(m_prefix) || !text.endsWith(m_suffix))
        return false;
    *begin = static_cast<int>(m_prefix.size());
    *end = static_cast<int>(text.size() - m_suffix.size());
    return true;
}

// As in Qt, the cursor does not go into the prefix or the suffix: what is typed goes into the number, wherever the text
// was clicked. A selection may reach into them, as one of all of the text does, which the special value text is typed
// over.
void QAbstractSpinBox::keepCursorInNumber() {
    int begin = 0, end = 0;
    if (m_edit->hasSelectedText() || !numberRange(&begin, &end))
        return;
    const int cursor = m_edit->cursorPosition();
    if (cursor < begin || cursor > end)
        m_edit->setCursorPosition(std::clamp(cursor, begin, end));
}

void QAbstractSpinBox::connectEdit() {
    QObject::connect(m_edit, &QLineEdit::cursorPositionChanged, this, [this] { keepCursorInNumber(); });
    QObject::connect(m_edit, &QLineEdit::textEdited, this, [this](const QString &text) { editEdited(text); });
    QObject::connect(m_edit, &QLineEdit::editingFinished, this, [this] { editFinished(); });
    QObject::connect(m_edit, &QLineEdit::returnPressed, this, [this] { emit returnPressed(); });
}

/*!
    Destroys the spin box.
*/
QAbstractSpinBox::~QAbstractSpinBox() = default;

/*!
    Returns the text of the spin box: the value with the prefix and the suffix, or what the user is typing.

    \sa QSpinBox::cleanText()
*/
QString QAbstractSpinBox::text() const { return m_edit->text(); }

/*!
    \fn QString QAbstractSpinBox::specialValueText() const

    Returns the text that is shown instead of the smallest value, or an empty string if the number is shown.

    \sa setSpecialValueText()
*/

/*!
    Sets the text shown instead of the smallest value to \a text. Typing the text sets the smallest value. An empty
    text shows the number.

    \sa specialValueText()
*/
void QAbstractSpinBox::setSpecialValueText(const QString &text) {
    m_special = text;
    refreshText();
    updateGeometry();
}

/*!
    \fn bool QAbstractSpinBox::wrapping() const

    Returns whether stepping past one end goes round to the other. The default is \c false.

    \sa setWrapping()
*/

/*!
    Sets whether stepping past one end goes round to the other to \a wrapping.

    \sa wrapping()
*/
void QAbstractSpinBox::setWrapping(bool wrapping) {
    m_wrapping = wrapping;
    update();
}

/*!
    \fn bool QAbstractSpinBox::isReadOnly() const

    Returns whether the user cannot change the value. The default is \c false.

    \sa setReadOnly()
*/

/*!
    Sets whether the user cannot change the value to \a readOnly. A read-only spin box shows its value and lets the
    text be selected, but it does not step and its text cannot be edited.

    \sa isReadOnly()
*/
void QAbstractSpinBox::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
    m_edit->setReadOnly(readOnly);
    update();
}

/*!
    \fn bool QAbstractSpinBox::keyboardTracking() const

    Returns whether the value follows the text as the user types. The default is \c true.

    \sa setKeyboardTracking()
*/

/*!
    \fn void QAbstractSpinBox::setKeyboardTracking(bool enable)

    Sets whether the value follows the text as the user types to \a enable. Without it, the value changes when the
    user presses Return or the spin box loses the focus.

    \sa keyboardTracking()
*/

/*!
    Takes the text that the user typed as the value, as when editing finishes, without emitting editingFinished().
*/
void QAbstractSpinBox::interpretText() { interpret(true); }

/*!
    Steps the value by \a steps single steps, up if \a steps is positive. The default implementation does nothing;
    QSpinBox and QDoubleSpinBox reimplement it.

    \sa stepUp(), stepDown(), stepEnabled()
*/
void QAbstractSpinBox::stepBy(int steps) { (void)steps; }

/*!
    Steps the value up by one single step.

    \sa stepBy()
*/
void QAbstractSpinBox::stepUp() { stepBy(1); }

/*!
    Steps the value down by one single step.

    \sa stepBy()
*/
void QAbstractSpinBox::stepDown() { stepBy(-1); }

/*!
    Selects the text between the prefix and the suffix.
*/
void QAbstractSpinBox::selectAll() {
    const QString text = m_edit->text();
    if (!text.startsWith(m_prefix) || !text.endsWith(m_suffix) || text.size() < m_prefix.size() + m_suffix.size()) {
        m_edit->selectAll();
        return;
    }
    m_edit->setSelection(static_cast<int>(m_prefix.size()),
                         static_cast<int>(text.size() - m_prefix.size() - m_suffix.size()));
}

/*!
    Clears the text but for the prefix and the suffix. The value does not change until the user types a new one.
*/
void QAbstractSpinBox::clear() {
    m_edit->setText(m_prefix + m_suffix);
    m_edit->setCursorPosition(static_cast<int>(m_prefix.size()));
}

/*!
    Returns which ways the spin box can step. The default implementation returns StepNone for a read-only spin box
    and both ways otherwise; QSpinBox and QDoubleSpinBox look at the value and the range.

    \sa stepBy()
*/
QAbstractSpinBox::StepEnabled QAbstractSpinBox::stepEnabled() const {
    return m_readOnly ? StepEnabled(StepNone) : (StepUpEnabled | StepDownEnabled);
}

/*!
    \fn QLineEdit *QAbstractSpinBox::lineEdit() const

    Returns the line edit that shows the text.
*/

/*!
    \fn QAbstractSpinBox::ButtonSymbols QAbstractSpinBox::buttonSymbols() const
    \since 6.0

    Returns what the buttons show: arrows by default.
*/

/*!
    \since 6.0

    Sets what the buttons show to \a bs: arrows, plus and minus signs, or no buttons, which leaves the keys and the
    wheel to step.
*/
void QAbstractSpinBox::setButtonSymbols(ButtonSymbols bs) {
    if (bs == m_buttonSymbols)
        return;
    m_buttonSymbols = bs;
    layoutEdit();
    update();
}

void QAbstractSpinBox::refreshText() {
    m_edit->setText(valueText());
    keepCursorInNumber();
    update();
}

void QAbstractSpinBox::layoutEdit() {
    // Not initStyleOption(): this runs from the constructor, where stepEnabled() is not the subclass's yet.
    QStyleOptionSpinBox option;
    option.initFrom(this);
    option.buttonSymbols = m_buttonSymbols;
    option.frame = m_frame;
    m_edit->setGeometry(style()->subElementRect(QStyle::SE_SpinBoxEditField, &option, this));
}

QString QAbstractSpinBox::valueText() const { return m_edit->text(); }

QStringList QAbstractSpinBox::sizeTexts() const { return {QStringLiteral("0000")}; }

void QAbstractSpinBox::takeText(const QString &number, bool atMinimum, bool finished) {
    (void)number;
    (void)atMinimum;
    (void)finished;
}

bool QAbstractSpinBox::acceptsCharacter(QChar c) const {
    (void)c;
    return true;
}

// Hands the text between the prefix and the suffix to the subclass.
void QAbstractSpinBox::interpret(bool finished) {
    const QString text = m_edit->text();
    const bool special = !m_special.isEmpty() && text == m_special;
    QString number;
    if (!special && text.startsWith(m_prefix) && text.endsWith(m_suffix) &&
        text.size() >= m_prefix.size() + m_suffix.size())
        number = text.mid(m_prefix.size(), text.size() - m_prefix.size() - m_suffix.size()).trimmed();
    takeText(number, special, finished);
}

void QAbstractSpinBox::editEdited(const QString &text) {
    (void)text;
    if (m_keyboardTracking)
        interpret(false);
}

void QAbstractSpinBox::editFinished() {
    interpret(true);
    emit editingFinished();
}

/*!
    \reimp
*/
QSize QAbstractSpinBox::sizeHint() const {
    const QFontMetrics fm = fontMetrics();
    int width = 0;
    QStringList texts = sizeTexts();
    if (!m_special.isEmpty())
        texts << m_special;
    for (const QString &text : texts)
        width = std::max(width, fm.horizontalAdvance(text));
    QStyleOptionSpinBox option;
    initStyleOption(&option);
    return style()->sizeFromContents(QStyle::CT_SpinBox, &option, QSize(width, fm.height()), this);
}

/*!
    \reimp
*/
QSize QAbstractSpinBox::minimumSizeHint() const { return sizeHint(); }

/*!
    \reimp
*/
void QAbstractSpinBox::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOptionSpinBox option;
    initStyleOption(&option);
    style()->drawControl(QStyle::CE_SpinBox, &option, &painter, this);
}

/*!
    \reimp
*/
void QAbstractSpinBox::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    layoutEdit();
}

/*!
    \reimp
*/
void QAbstractSpinBox::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    const Button button = buttonAt(event->pos());
    const bool up = button == UpButton;
    const bool down = button == DownButton;
    // Where the keys are on the screen, a button that steps leaves them be: they would cover the page, and slide it so
    // that the button is no longer under the finger.
    const QPlatformIntegration *platform = QtPrivate::GuiAccess::integration();
    if (!(up || down) || !platform || !platform->hasInputPanel())
        m_edit->setFocus();
    stopRepeat();
    if (up || down) {
        m_pressed = button;
        if (stepPressed())
            startRepeat(false);
    }
    update();
    event->accept();
}

/*!
    \reimp

    The steps of a button that is held stop when the pointer leaves it.
*/
void QAbstractSpinBox::mouseMoveEvent(QMouseEvent *event) {
    if (m_pressed == NoButton) {
        event->ignore();
        return;
    }
    if (buttonAt(event->pos()) != m_pressed) {
        stopRepeat();
        m_pressed = NoButton;
        update();
    }
    event->accept();
}

// The button that steps under `pos`, if there is one.
QAbstractSpinBox::Button QAbstractSpinBox::buttonAt(const QPoint &pos) const {
    QStyleOptionSpinBox option;
    option.initFrom(this);
    option.buttonSymbols = m_buttonSymbols;
    if (style()->subElementRect(QStyle::SE_SpinBoxUp, &option, this).contains(pos))
        return UpButton;
    if (style()->subElementRect(QStyle::SE_SpinBoxDown, &option, this).contains(pos))
        return DownButton;
    return NoButton;
}

/*!
    \reimp
*/
void QAbstractSpinBox::mouseReleaseEvent(QMouseEvent *event) {
    stopRepeat();
    m_pressed = NoButton;
    update();
    event->accept();
}

// Steps once the way of the button that is held. As in Qt, the steps repeat while it is held: the first after the
// style's threshold, the others at its rate.
bool QAbstractSpinBox::stepPressed() {
    const bool up = m_pressed == UpButton;
    if (m_pressed == NoButton || !stepEnabled().testFlag(up ? StepUpEnabled : StepDownEnabled))
        return false;
    stepBy(up ? 1 : -1);
    return true;
}

void QAbstractSpinBox::startRepeat(bool atRate) {
    stopRepeat();
    const QStyle::StyleHint hint =
        atRate ? QStyle::SH_SpinBox_ClickAutoRepeatRate : QStyle::SH_SpinBox_ClickAutoRepeatThreshold;
    m_repeatTimer = startTimer(std::max(1, style()->styleHint(hint, nullptr, this)));
    m_repeating = atRate;
}

void QAbstractSpinBox::stopRepeat() {
    if (m_repeatTimer != 0)
        killTimer(m_repeatTimer);
    m_repeatTimer = 0;
    m_repeating = false;
}

/*!
    \reimp
*/
void QAbstractSpinBox::timerEvent(QTimerEvent *event) {
    if (m_repeatTimer == 0 || event->timerId() != m_repeatTimer) {
        QWidget::timerEvent(event);
        return;
    }
    if (!isEnabled() || !isVisible() || !stepPressed()) {
        stopRepeat();
        return;
    }
    if (!m_repeating)
        startRepeat(true); // the threshold has passed: from now on the steps come at the rate
}

/*!
    \reimp
*/
void QAbstractSpinBox::wheelEvent(QWheelEvent *event) {
    const int notches = event->angleDelta().y() / 120;
    if (notches == 0) {
        event->ignore();
        return;
    }
    if (stepEnabled().testFlag(notches > 0 ? StepUpEnabled : StepDownEnabled))
        stepBy(notches);
    event->accept();
}

/*!
    \reimp

    Up and Down step by one single step, Page Up and Page Down by ten.
*/
void QAbstractSpinBox::keyPressEvent(QKeyEvent *event) {
    int steps = 0;
    switch (event->key()) {
    case Qt::Key_Up:
        steps = 1;
        break;
    case Qt::Key_Down:
        steps = -1;
        break;
    case Qt::Key_PageUp:
        steps = 10;
        break;
    case Qt::Key_PageDown:
        steps = -10;
        break;
    default:
        event->ignore();
        return;
    }
    if (stepEnabled().testFlag(steps > 0 ? StepUpEnabled : StepDownEnabled)) {
        stepBy(steps);
        selectAll();
    }
    event->accept();
}

/*!
    \reimp
*/
void QAbstractSpinBox::changeEvent(QEvent *event) {
    if (event->type() == QEvent::EnabledChange && !isEnabled()) {
        stopRepeat();
        m_pressed = NoButton;
    }
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        updateGeometry();
        layoutEdit();
    }
    update();
    QWidget::changeEvent(event);
}

/*!
    \enum QAbstractSpinBox::CorrectionMode
    \since 6.0

    What happens to a value the user typed that is out of the range when editing finishes.

    \value CorrectToPreviousValue The value goes back to what it was.
    \value CorrectToNearestValue The value becomes the nearest one in the range.
*/

/*!
    \enum QAbstractSpinBox::StepType
    \since 6.0

    How large a step of a spin box is.

    \value DefaultStepType The single step.
    \value AdaptiveDecimalStepType A power of ten one digit below the leading digit of the value, at least the
           single step: a step of 10 at 150, of 100 at 1500.
*/

/*!
    \since 6.0

    Fills \a option with the state of the spin box, for the style to draw it.
*/
void QAbstractSpinBox::initStyleOption(QStyleOptionSpinBox *option) const {
    if (!option)
        return;
    option->initFrom(this);
    option->buttonSymbols = m_buttonSymbols;
    option->frame = m_frame;
    if (m_edit->hasFocus())
        option->state |= QStyle::State_HasFocus;
    const StepEnabled steps = stepEnabled();
    option->upEnabled = steps.testFlag(StepUpEnabled);
    option->downEnabled = steps.testFlag(StepDownEnabled);
    option->upPressed = m_pressed == UpButton;
    option->downPressed = m_pressed == DownButton;
}

/*!
    \fn bool QAbstractSpinBox::hasFrame() const
    \since 6.0

    Returns whether the spin box draws a frame. The default is \c true.
*/

/*!
    \since 6.0

    Sets whether the spin box draws a frame to \a frame.
*/
void QAbstractSpinBox::setFrame(bool frame) {
    m_frame = frame;
    update();
}

/*!
    \since 6.0

    Returns the alignment of the text.
*/
Qt::Alignment QAbstractSpinBox::alignment() const { return m_edit->alignment(); }

/*!
    \since 6.0

    Aligns the text to \a flag.
*/
void QAbstractSpinBox::setAlignment(Qt::Alignment flag) { m_edit->setAlignment(flag); }

/*!
    \fn bool QAbstractSpinBox::isAccelerated() const
    \since 6.0

    Returns whether holding a button steps faster as time goes. Nt keeps the value.
*/

/*!
    \fn void QAbstractSpinBox::setAccelerated(bool on)
    \since 6.0

    Sets whether holding a button steps faster to \a on.
*/

/*!
    \fn QAbstractSpinBox::CorrectionMode QAbstractSpinBox::correctionMode() const
    \since 6.0

    Returns what happens to a typed value out of the range, CorrectToPreviousValue by default.
*/

/*!
    \fn void QAbstractSpinBox::setCorrectionMode(CorrectionMode cm)
    \since 6.0

    Sets what happens to a typed value out of the range to \a cm.
*/

/*!
    \since 6.0

    Returns \c true if validate() takes the text as it is.
*/
bool QAbstractSpinBox::hasAcceptableInput() const {
    QString input = m_edit->text();
    int pos = m_edit->cursorPosition();
    return validate(input, pos) == QValidator::Acceptable;
}

/*!
    \fn bool QAbstractSpinBox::isGroupSeparatorShown() const
    \since 6.0

    Returns whether the thousands are separated in the text. The default is \c false.
*/

/*!
    \since 6.0

    Separates the thousands of the number with the group separator of the locale if \a shown is \c true.
*/
void QAbstractSpinBox::setGroupSeparatorShown(bool shown) {
    m_groupSeparator = shown;
    refreshText();
    updateGeometry();
}

/*!
    \since 6.0

    Returns whether \a input, with the cursor at \a pos, is a value the spin box takes. The default takes any text.
*/
QValidator::State QAbstractSpinBox::validate(QString &input, int &pos) const {
    (void)input;
    (void)pos;
    return QValidator::Acceptable;
}

/*!
    \since 6.0

    Corrects \a input, a text that validate() did not take. The default does nothing.
*/
void QAbstractSpinBox::fixup(QString &input) const { (void)input; }

/*!
    \since 6.0

    Makes \a edit the line edit of the spin box, and takes ownership of it; the line edit it had is deleted.
*/
void QAbstractSpinBox::setLineEdit(QLineEdit *edit) {
    if (!edit || edit == m_edit)
        return;
    delete m_edit;
    m_edit = edit;
    m_edit->setParent(this);
    m_edit->setFrame(false);
    connectEdit();
    layoutEdit();
    refreshText();
    m_edit->show();
}

#include "moc_qabstractspinbox.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
