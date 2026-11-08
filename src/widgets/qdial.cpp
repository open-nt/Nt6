// MIT License - see LICENSE
#include <QtWidgets/qdial.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>
#include <cmath>

/*!
    \class QDial
    \brief The QDial class provides a round knob that picks a value from a range.

    The user turns the knob by dragging its handle round, or turns the mouse wheel over it, or uses the keys when it has
    the focus. Connect to valueChanged():

    \code
    QDial *volume = new QDial(window);
    volume->setRange(0, 100);
    volume->setNotchesVisible(true);
    connect(volume, &QDial::valueChanged, [](int value) { qDebug() << value; });
    \endcode

    A dial that does not wrap turns from the bottom left, over the top, to the bottom right. With setWrapping(), it
    turns the full circle with its smallest value at the bottom, and the keys and the wheel take the value round from the
    largest to the smallest.

    Dials with the handle at the start, the middle and the end, one with notches, one that wraps, and a disabled one.

    \image widgets/qdial.png QDial in the light color scheme

    \differences
    There is no \c notchSize(), \c notchTarget() or \c setNotchTarget(): the notches are at ten places for a dial that does not
    wrap and twelve for one that does. The dial is drawn without antialiasing. There is no \c invertedAppearance. A dial
    that wraps keeps setValue() in the range rather than taking a value past the end round to the start.
    \enddifferences

    \sa QSlider, QScrollBar
*/

/*!
    Constructs a dial with the given \a parent. It goes from 0 to 99.
*/
QDial::QDial(QWidget *parent) : QAbstractSlider(parent) {
    setFocusPolicy(Qt::WheelFocus);
    setSizePolicy(QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred));
}

/*!
    Destroys the dial.
*/
QDial::~QDial() = default;

/*!
    \fn bool QDial::wrapping() const

    Returns whether the dial turns the full circle. The default is \c false.

    \sa setWrapping()
*/

/*!
    Sets whether the dial turns the full circle to \a on.

    \sa wrapping()
*/
void QDial::setWrapping(bool on) {
    if (m_wrapSteps == on)
        return;
    m_wrapSteps = on;
    update();
}

/*!
    \fn bool QDial::notchesVisible() const

    Returns whether marks are drawn around the dial. The default is \c false.

    \sa setNotchesVisible()
*/

/*!
    Sets whether marks are drawn around the dial to \a visible.

    \sa notchesVisible()
*/
void QDial::setNotchesVisible(bool visible) {
    if (m_notches == visible)
        return;
    m_notches = visible;
    update();
}

/*!
    \reimp
*/
QSize QDial::sizeHint() const { return QSize(100, 100); }

/*!
    \reimp
*/
QSize QDial::minimumSizeHint() const { return QSize(50, 50); }

// The value for the handle at `pos`, a point in the dial: from the angle of the point round the center.
int QDial::valueAt(const QPoint &pos) const {
    const qreal pi = 3.14159265358979323846;
    const qreal dx = pos.x() - rect().center().x(), dy = rect().center().y() - pos.y();
    qreal angle = std::atan2(dy, dx) * 180.0 / pi;
    qreal fraction;
    if (wrapping()) {
        // The smallest value is at the bottom, and the dial turns clockwise.
        qreal turn = std::fmod(270.0 - angle + 720.0, 360.0);
        fraction = turn / 360.0;
    } else {
        qreal turn = std::fmod(225.0 - angle + 720.0, 360.0);
        if (turn > 270.0)
            turn = turn < 270.0 + 45.0 ? 270.0 : 0.0; // between the ends: the nearer one
        fraction = turn / 270.0;
    }
    return minimum() + static_cast<int>(std::lround(fraction * (static_cast<qreal>(maximum()) - minimum())));
}

/*!
    \reimp
*/
void QDial::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOptionSlider option;
    option.initFrom(this);
    option.minimum = minimum();
    option.maximum = maximum();
    option.sliderPosition = sliderPosition();
    option.tickMarks = m_notches;
    option.wrapping = wrapping();
    if (isSliderDown())
        option.state |= QStyle::State_Sunken;
    style()->drawControl(QStyle::CE_Dial, &option, &painter, this);
}

/*!
    \reimp
*/
void QDial::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        event->ignore();
        return;
    }
    // As in Qt the dial jumps to the point before it is down, so the press itself emits no sliderMoved().
    setSliderPosition(valueAt(event->pos()));
    setSliderDown(true);
    event->accept();
}

/*!
    \reimp
*/
void QDial::mouseMoveEvent(QMouseEvent *event) {
    if (!isSliderDown()) {
        event->ignore();
        return;
    }
    setSliderPosition(valueAt(event->pos()));
    event->accept();
}

/*!
    \reimp
*/
void QDial::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton || !isSliderDown()) {
        event->ignore();
        return;
    }
    setSliderDown(false);
    event->accept();
}

#include "moc_qdial.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
