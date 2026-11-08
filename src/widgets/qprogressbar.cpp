// MIT License - see LICENSE
#include <QtWidgets/qprogressbar.h>

#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

/*!
    \class QProgressBar
    \brief The QProgressBar class shows how far an operation has come.

    A progress bar fills from the left as its value goes from the minimum to the maximum, and shows the progress as
    a text on the bar. The text comes from format(), in which \c %p stands for the percentage, \c %v for the value
    and \c %m for the number of steps:

    \code
    QProgressBar *bar = new QProgressBar(window);
    bar->setRange(0, 200);
    bar->setFormat("%v of %m");
    bar->setValue(50);
    \endcode

    \differences
    The bar is horizontal and grows from the left: there is no \c setOrientation() and no \c setInvertedAppearance(). There
    is no busy indicator for a range of 0 to 0, which shows an empty bar, and no \c setTextDirection(). The text is
    always centered.
    \enddifferences

    \sa QSlider
*/

/*!
    Constructs a progress bar with the range 0 to 100 and no value, with the given \a parent.
*/
QProgressBar::QProgressBar(QWidget *parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

/*!
    Destroys the progress bar.
*/
QProgressBar::~QProgressBar() = default;

/*!
    \fn int QProgressBar::minimum() const

    Returns the value that stands for no progress, 0 by default.

    \sa setMinimum()
*/

/*!
    Sets the minimum to \a minimum. The maximum grows to the same if it was smaller.

    \sa minimum()
*/
void QProgressBar::setMinimum(int minimum) { setRange(minimum, std::max(minimum, m_maximum)); }

/*!
    \fn int QProgressBar::maximum() const

    Returns the value that stands for completion, 100 by default.

    \sa setMaximum()
*/

/*!
    Sets the maximum to \a maximum. The minimum falls to the same if it was larger.

    \sa maximum()
*/
void QProgressBar::setMaximum(int maximum) { setRange(std::min(m_minimum, maximum), maximum); }

/*!
    Sets the range to \a minimum to \a maximum. If \a maximum is less than \a minimum, it is made equal. A value that
    falls outside the range is reset.

    \sa reset()
*/
void QProgressBar::setRange(int minimum, int maximum) {
    m_minimum = minimum;
    m_maximum = std::max(minimum, maximum);
    if (m_value < m_minimum - 1 || m_value > m_maximum)
        reset();
    update();
}

/*!
    \fn int QProgressBar::value() const

    Returns the value, one less than the minimum if there is none.

    \sa setValue(), reset()
*/

/*!
    Sets the value to \a value. It is ignored if it is outside the range. Emits valueChanged() if the value changes.

    \sa value()
*/
void QProgressBar::setValue(int value) {
    if (value == m_value || value < m_minimum || value > m_maximum)
        return;
    m_value = value;
    update();
    emit valueChanged(m_value);
}

/*!
    Resets the progress bar: the value goes to one less than the minimum, which shows an empty bar and no text.

    \sa setValue()
*/
void QProgressBar::reset() {
    if (m_value == m_minimum - 1)
        return;
    m_value = m_minimum - 1;
    update();
    emit valueChanged(m_value);
}

/*!
    \fn bool QProgressBar::isTextVisible() const

    Returns whether the text is shown on the bar. The default is \c true.

    \sa setTextVisible()
*/

/*!
    Sets whether the text is shown to \a visible.

    \sa isTextVisible()
*/
void QProgressBar::setTextVisible(bool visible) {
    if (m_textVisible == visible)
        return;
    m_textVisible = visible;
    update();
}

/*!
    \fn QString QProgressBar::format() const

    Returns the format of the text, \c{%p%} by default.

    \sa setFormat()
*/

/*!
    Sets the format of the text to \a format: \c %p is replaced by the percentage, \c %v by the value and \c %m by the number
    of steps from the minimum to the maximum.

    \sa format(), text()
*/
void QProgressBar::setFormat(const QString &format) {
    if (m_format == format)
        return;
    m_format = format;
    update();
}

/*!
    Returns the text that is shown on the bar: the format with its codes replaced. It is empty if there is no value.

    \sa format()
*/
QString QProgressBar::text() const {
    if (m_value < m_minimum)
        return QString();
    const qint64 steps = static_cast<qint64>(m_maximum) - m_minimum;
    const qint64 done = static_cast<qint64>(m_value) - m_minimum;
    const int percent = steps > 0 ? static_cast<int>((done * 100) / steps) : 100;
    QString out = m_format;
    out.replace(QString::fromLatin1("%p"), QString::number(percent));
    out.replace(QString::fromLatin1("%v"), QString::number(m_value));
    out.replace(QString::fromLatin1("%m"), QString::number(steps));
    return out;
}

/*!
    \reimp
*/
QSize QProgressBar::sizeHint() const {
    const QFontMetrics fm = fontMetrics();
    const int digit = std::max(9, fm.horizontalAdvance(QChar('0')));
    QStyleOptionProgressBar option;
    option.initFrom(this);
    return style()->sizeFromContents(QStyle::CT_ProgressBar, &option,
                                     QSize(digit * 7 + fm.horizontalAdvance(QChar('%')) + 4, fm.height()), this);
}

/*!
    \reimp
*/
QSize QProgressBar::minimumSizeHint() const { return QSize(sizeHint().height(), sizeHint().height()); }

/*!
    \reimp
*/
void QProgressBar::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    painter.setFont(font());
    QStyleOptionProgressBar option;
    option.initFrom(this);
    option.minimum = m_minimum;
    option.maximum = m_maximum;
    option.progress = m_value;
    option.textVisible = m_textVisible;
    option.text = text();
    style()->drawControl(QStyle::CE_ProgressBar, &option, &painter, this);
}

/*!
    \reimp
*/
void QProgressBar::changeEvent(QEvent *event) {
    update();
    updateGeometry();
    QWidget::changeEvent(event);
}

#include "moc_qprogressbar.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
