// MIT License - see LICENSE
#include <QtWidgets/qspinbox.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

namespace {

// The line edit of a spin box: it only accepts digits, and leaves the keys that step the value to the spin box.
class SpinEdit : public QLineEdit {
public:
    explicit SpinEdit(QWidget *parent) : QLineEdit(parent) {}

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
        if (plain && !text.isEmpty() && text.at(0).isPrint() && !text.at(0).isDigit() && text.at(0) != QChar('-')) {
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
};

} // namespace

/*!
    \class QSpinBox
    \brief The QSpinBox class lets the user pick an integer by typing it or stepping with two buttons.

    A spin box shows a number in a text that the user can edit, with a button to step up and one to step down. The
    arrow keys, Page Up and Page Down, and the mouse wheel step too. A prefix and a suffix, such as a unit,
    are shown around the number:

    \code
    QSpinBox *size = new QSpinBox(window);
    size->setRange(8, 72);
    size->setSuffix(" pt");
    connect(size, &QSpinBox::valueChanged, [](int value) { qDebug() << value; });
    \endcode

    \differences
    There is no \c QAbstractSpinBox base class, \c QDoubleSpinBox, \c setSpecialValueText(), \c displayIntegerBase(),
    \c setAccelerated(), \c setButtonSymbols() or \c setKeyboardTracking(), and the buttons do not repeat while they
    are held. The value is always updated as the user types a valid number.
    \enddifferences

    \sa QSlider, QLineEdit
*/

/*!
    Constructs a spin box with the range 0 to 99 and the value 0, with the given \a parent.
*/
QSpinBox::QSpinBox(QWidget *parent) : QWidget(parent), m_edit(new SpinEdit(this)) {
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_edit->setFrame(false);
    m_edit->setText(textFromValue(m_value));
    QObject::connect(m_edit, &QLineEdit::textEdited, this, [this](const QString &text) { editEdited(text); });
    QObject::connect(m_edit, &QLineEdit::editingFinished, this, [this] { editFinished(); });
    layoutEdit();
}

/*!
    Destroys the spin box.
*/
QSpinBox::~QSpinBox() = default;

/*!
    \fn int QSpinBox::minimum() const

    Returns the smallest value, 0 by default.

    \sa setMinimum(), setRange()
*/

/*!
    Sets the smallest value to \a minimum. The largest value grows to the same if it was smaller.

    \sa minimum()
*/
void QSpinBox::setMinimum(int minimum) { setRange(minimum, std::max(minimum, m_maximum)); }

/*!
    \fn int QSpinBox::maximum() const

    Returns the largest value, 99 by default.

    \sa setMaximum(), setRange()
*/

/*!
    Sets the largest value to \a maximum. The smallest value falls to the same if it was larger.

    \sa maximum()
*/
void QSpinBox::setMaximum(int maximum) { setRange(std::min(m_minimum, maximum), maximum); }

/*!
    Sets the range to \a minimum to \a maximum. If \a maximum is less than \a minimum, it is made equal. The value is brought
    into the range.

    \sa minimum(), maximum()
*/
void QSpinBox::setRange(int minimum, int maximum) {
    m_minimum = minimum;
    m_maximum = std::max(minimum, maximum);
    setValue(m_value);
    updateGeometry();
    update();
}

/*!
    \fn int QSpinBox::value() const

    Returns the value.

    \sa setValue()
*/

/*!
    Sets the value to \a value, which is brought into the range, and shows it. Emits valueChanged() if the value changes.

    \sa value()
*/
void QSpinBox::setValue(int value) {
    const int clamped = std::clamp(value, m_minimum, m_maximum);
    const bool changed = clamped != m_value;
    m_value = clamped;
    m_edit->setText(textFromValue(m_value));
    update();
    if (changed)
        emit valueChanged(m_value);
}

/*!
    \fn int QSpinBox::singleStep() const

    Returns how much a step changes the value by, 1 by default.

    \sa setSingleStep()
*/

/*!
    Sets the size of a step to \a step.

    \sa singleStep()
*/
void QSpinBox::setSingleStep(int step) { m_step = std::max(1, step); }

/*!
    \fn QString QSpinBox::prefix() const

    Returns the text that is shown before the number.

    \sa setPrefix()
*/

/*!
    Sets the text that is shown before the number to \a prefix.

    \sa prefix()
*/
void QSpinBox::setPrefix(const QString &prefix) {
    m_prefix = prefix;
    m_edit->setText(textFromValue(m_value));
    updateGeometry();
}

/*!
    \fn QString QSpinBox::suffix() const

    Returns the text that is shown after the number.

    \sa setSuffix()
*/

/*!
    Sets the text that is shown after the number to \a suffix.

    \sa suffix()
*/
void QSpinBox::setSuffix(const QString &suffix) {
    m_suffix = suffix;
    m_edit->setText(textFromValue(m_value));
    updateGeometry();
}

/*!
    \fn bool QSpinBox::wrapping() const

    Returns whether stepping past the largest value goes to the smallest, and the other way round. The default is
    \c false, where the value stops at the ends.

    \sa setWrapping()
*/

/*!
    Sets whether the value wraps around to \a wrapping.

    \sa wrapping()
*/
void QSpinBox::setWrapping(bool wrapping) {
    m_wrapping = wrapping;
    update();
}

/*!
    Returns the text that is shown: the prefix, the number and the suffix.

    \sa cleanText()
*/
QString QSpinBox::text() const { return textFromValue(m_value); }

/*!
    Returns the number as a text, without the prefix and the suffix.

    \sa text()
*/
QString QSpinBox::cleanText() const { return QString::number(m_value); }

/*!
    Changes the value by \a steps times the single step, which may be negative. The value wraps around if wrapping
    is on, and stops at the ends otherwise.

    \sa stepUp(), stepDown()
*/
void QSpinBox::stepBy(int steps) {
    qint64 value = static_cast<qint64>(m_value) + static_cast<qint64>(steps) * m_step;
    if (m_wrapping && (value > m_maximum || value < m_minimum)) {
        const qint64 range = static_cast<qint64>(m_maximum) - m_minimum + 1;
        value = m_minimum + (((value - m_minimum) % range) + range) % range;
    }
    setValue(static_cast<int>(std::clamp<qint64>(value, m_minimum, m_maximum)));
}

/*!
    Steps the value up by one single step.

    \sa stepBy()
*/
void QSpinBox::stepUp() { stepBy(1); }

/*!
    Steps the value down by one single step.

    \sa stepBy()
*/
void QSpinBox::stepDown() { stepBy(-1); }

/*!
    Selects the number in the text, without the prefix and the suffix.
*/
void QSpinBox::selectAll() {
    m_edit->setSelection(static_cast<int>(m_prefix.size()),
                         static_cast<int>(m_edit->text().size() - m_prefix.size() - m_suffix.size()));
}

QString QSpinBox::textFromValue(int value) const { return m_prefix + QString::number(value) + m_suffix; }

void QSpinBox::layoutEdit() {
    QStyleOptionSpinBox option;
    option.initFrom(this);
    m_edit->setGeometry(style()->subElementRect(QStyle::SE_SpinBoxEditField, &option, this));
}

// The user typed: take the number as the value as soon as it is one, without rewriting the text.
void QSpinBox::editEdited(const QString &text) {
    if (!text.startsWith(m_prefix) || !text.endsWith(m_suffix) || text.size() < m_prefix.size() + m_suffix.size())
        return;
    bool ok = false;
    const int number = text.mid(m_prefix.size(), text.size() - m_prefix.size() - m_suffix.size()).toInt(&ok);
    if (!ok || number < m_minimum || number > m_maximum || number == m_value)
        return;
    m_value = number;
    update();
    emit valueChanged(m_value);
}

void QSpinBox::editFinished() {
    // Settle on a valid text: a number that is out of range is brought into it, and anything else is dropped.
    const QString text = m_edit->text();
    bool ok = false;
    int number = m_value;
    if (text.startsWith(m_prefix) && text.endsWith(m_suffix) && text.size() >= m_prefix.size() + m_suffix.size())
        number = text.mid(m_prefix.size(), text.size() - m_prefix.size() - m_suffix.size()).toInt(&ok);
    setValue(ok ? number : m_value);
    emit editingFinished();
}

/*!
    \reimp
*/
QSize QSpinBox::sizeHint() const {
    const QFontMetrics fm = fontMetrics();
    const int width =
        std::max(fm.horizontalAdvance(textFromValue(m_minimum)), fm.horizontalAdvance(textFromValue(m_maximum)));
    QStyleOptionSpinBox option;
    option.initFrom(this);
    return style()->sizeFromContents(QStyle::CT_SpinBox, &option, QSize(width, fm.height()), this);
}

/*!
    \reimp
*/
QSize QSpinBox::minimumSizeHint() const { return sizeHint(); }

/*!
    \reimp
*/
void QSpinBox::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOptionSpinBox option;
    option.initFrom(this);
    if (m_edit->hasFocus())
        option.state |= QStyle::State_HasFocus;
    option.upEnabled = m_wrapping || m_value < m_maximum;
    option.downEnabled = m_wrapping || m_value > m_minimum;
    option.upPressed = m_pressed == UpButton;
    option.downPressed = m_pressed == DownButton;
    style()->drawControl(QStyle::CE_SpinBox, &option, &painter, this);
}

/*!
    \reimp
*/
void QSpinBox::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    layoutEdit();
}

/*!
    \reimp
*/
void QSpinBox::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    QStyleOptionSpinBox option;
    option.initFrom(this);
    m_edit->setFocus();
    if (style()->subElementRect(QStyle::SE_SpinBoxUp, &option, this).contains(event->pos())) {
        m_pressed = UpButton;
        stepUp();
    } else if (style()->subElementRect(QStyle::SE_SpinBoxDown, &option, this).contains(event->pos())) {
        m_pressed = DownButton;
        stepDown();
    }
    update();
    event->accept();
}

/*!
    \reimp
*/
void QSpinBox::mouseReleaseEvent(QMouseEvent *event) {
    m_pressed = NoButton;
    update();
    event->accept();
}

/*!
    \reimp
*/
void QSpinBox::wheelEvent(QWheelEvent *event) {
    const int notches = event->angleDelta().y() / 120;
    if (notches == 0) {
        event->ignore();
        return;
    }
    stepBy(notches);
    event->accept();
}

/*!
    \reimp
*/
void QSpinBox::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
    case Qt::Key_Up:
        stepUp();
        break;
    case Qt::Key_Down:
        stepDown();
        break;
    case Qt::Key_PageUp:
        stepBy(10);
        break;
    case Qt::Key_PageDown:
        stepBy(-10);
        break;
    default:
        event->ignore();
        return;
    }
    event->accept();
}

/*!
    \reimp
*/
void QSpinBox::changeEvent(QEvent *event) {
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        updateGeometry();
        layoutEdit();
    }
    update();
    QWidget::changeEvent(event);
}

#include "moc_qspinbox.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
