// MIT License - see LICENSE
#include <QtWidgets/qabstractspinbox.h>

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
    SpinEdit(QAbstractSpinBox *box, std::function<bool(QChar)> accepts)
        : QLineEdit(box), m_accepts(std::move(accepts)) {}

protected:
    void keyPressEvent(QKeyEvent *event) override {
        switch (event->key()) {
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            event->ignore();
            return;
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

    \differences
    There is no \c alignment, \c buttonSymbols, \c accelerated, \c correctionMode, \c frame or
    \c showGroupSeparator, and no \c validate() or \c fixup(): the subclasses check what the user types. The buttons
    do not repeat while they are held. A subclass outside Nt can reimplement stepBy() and stepEnabled(), but the text
    is always that of a QSpinBox or a QDoubleSpinBox.
    \enddifferences

    \sa QSpinBox, QDoubleSpinBox
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
    : QWidget(parent),
      m_edit(new SpinEdit(this, [this](QChar c) { return acceptsCharacter(c) || m_special.contains(c); })) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_edit->setFrame(false);
    QObject::connect(m_edit, &QLineEdit::textEdited, this, [this](const QString &text) { editEdited(text); });
    QObject::connect(m_edit, &QLineEdit::editingFinished, this, [this] { editFinished(); });
    layoutEdit();
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

void QAbstractSpinBox::refreshText() {
    m_edit->setText(valueText());
    update();
}

void QAbstractSpinBox::layoutEdit() {
    QStyleOptionSpinBox option;
    option.initFrom(this);
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
    option.initFrom(this);
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
    option.initFrom(this);
    if (m_edit->hasFocus())
        option.state |= QStyle::State_HasFocus;
    const StepEnabled steps = stepEnabled();
    option.upEnabled = steps.testFlag(StepUpEnabled);
    option.downEnabled = steps.testFlag(StepDownEnabled);
    option.upPressed = m_pressed == UpButton;
    option.downPressed = m_pressed == DownButton;
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
    QStyleOptionSpinBox option;
    option.initFrom(this);
    m_edit->setFocus();
    const StepEnabled steps = stepEnabled();
    if (style()->subElementRect(QStyle::SE_SpinBoxUp, &option, this).contains(event->pos())) {
        m_pressed = UpButton;
        if (steps.testFlag(StepUpEnabled))
            stepBy(1);
    } else if (style()->subElementRect(QStyle::SE_SpinBoxDown, &option, this).contains(event->pos())) {
        m_pressed = DownButton;
        if (steps.testFlag(StepDownEnabled))
            stepBy(-1);
    }
    update();
    event->accept();
}

/*!
    \reimp
*/
void QAbstractSpinBox::mouseReleaseEvent(QMouseEvent *event) {
    m_pressed = NoButton;
    update();
    event->accept();
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
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        updateGeometry();
        layoutEdit();
    }
    update();
    QWidget::changeEvent(event);
}

#include "moc_qabstractspinbox.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
