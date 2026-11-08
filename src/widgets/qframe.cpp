// MIT License - see LICENSE
#include <QtWidgets/qframe.h>

#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

namespace {

// Fills a ring of the given width inside `r`: the top and left sides with `topLeft`, the bottom and right with `bottomRight`.
void drawRing(QPainter *p, const QRect &r, int width, const QColor &topLeft, const QColor &bottomRight) {
    width = std::min(width, std::min(r.width(), r.height()));
    if (width <= 0)
        return;
    p->fillRect(QRect(r.left(), r.top(), r.width(), width), topLeft);
    p->fillRect(QRect(r.left(), r.top(), width, r.height()), topLeft);
    p->fillRect(QRect(r.left(), r.bottom() - width + 1, r.width(), width), bottomRight);
    p->fillRect(QRect(r.right() - width + 1, r.top(), width, r.height()), bottomRight);
}

} // namespace

/*!
    \class QFrame
    \brief The QFrame class is the base class of widgets that can have a frame.

    A frame has a shape, which says what is drawn, and a shadow, which says how it looks: flat, raised or sunken.
    Set both at once with setFrameStyle():

    \code
    QFrame *line = new QFrame(window);
    line->setFrameStyle(QFrame::HLine | QFrame::Sunken);
    \endcode

    A frame draws its shape in lineWidth() pixels, and, for a raised or sunken box, midLineWidth() pixels of the window
    color between the two shaded lines. A \c StyledPanel is drawn by the style, so it looks like the frames of the
    other widgets; its width comes from the style.

    A QFrame draws nothing but its frame. A widget that is a QFrame keeps its own contents inside contentsRect().

    Frame shapes in the light color scheme: a box, a panel, a sunken and a raised line, and a styled panel.

    \image widgets/qframe.png QFrame in the light color scheme

    \differences
    There is no \c WinPanel shape, no \c setFrameRect(), and no \c QFrame::StyleMask or \c QFrame::Shadow_Mask.
    The frame is always the whole widget. \c contentsRect() is a member of QFrame, not of QWidget, and is the
    rectangle inside the frame; there are no contents margins.
    \enddifferences

    \sa QLabel, QGroupBox
*/

/*!
    \enum QFrame::Shape

    This enum says what the frame draws.

    \value NoFrame Nothing.
    \value Box A rectangle around the contents.
    \value Panel A rectangle that looks raised or sunken.
    \value HLine A horizontal line, with nothing above or below it.
    \value VLine A vertical line, with nothing at its sides.
    \value StyledPanel A rectangle that the style draws, as it does the frame of other widgets.
*/

/*!
    \enum QFrame::Shadow

    This enum says how the lines of the frame look.

    \value Plain Flat, in the text color of the palette.
    \value Raised Raised: light on the top and left, dark on the bottom and right.
    \value Sunken Sunken: dark on the top and left, light on the bottom and right.
*/

/*!
    Constructs a frame with no frame shape, and the given \a parent.
*/
QFrame::QFrame(QWidget *parent) : QWidget(parent) {}

/*!
    Destroys the frame.
*/
QFrame::~QFrame() = default;

/*!
    Returns the shape of the frame.

    \sa setFrameShape(), frameShadow()
*/
QFrame::Shape QFrame::frameShape() const { return static_cast<Shape>(m_style & 0x0f); }

/*!
    Sets the shape of the frame to \a shape.

    \sa frameShape()
*/
void QFrame::setFrameShape(Shape shape) { setFrameStyle((m_style & 0xf0) | shape); }

/*!
    Returns how the lines of the frame look.

    \sa setFrameShadow(), frameShape()
*/
QFrame::Shadow QFrame::frameShadow() const { return static_cast<Shadow>(m_style & 0xf0); }

/*!
    Sets the shadow of the frame to \a shadow.

    \sa frameShadow()
*/
void QFrame::setFrameShadow(Shadow shadow) { setFrameStyle((m_style & 0x0f) | shadow); }

/*!
    Returns the shape and the shadow of the frame, as in \c{QFrame::Box | QFrame::Raised}.

    \sa setFrameStyle()
*/
int QFrame::frameStyle() const { return m_style; }

/*!
    Sets the shape and the shadow of the frame to \a style, a shape and a shadow combined with \c |. A shadow of 0 is
    the same as \c Plain.

    \sa frameStyle(), setFrameShape(), setFrameShadow()
*/
void QFrame::setFrameStyle(int style) {
    if ((style & 0xf0) == 0)
        style |= Plain;
    if (m_style == style)
        return;
    m_style = style;
    updateGeometry();
    update();
}

/*!
    \fn int QFrame::lineWidth() const

    Returns the width of the lines of the frame, 1 by default.

    \sa setLineWidth(), midLineWidth()
*/

/*!
    Sets the width of the lines of the frame to \a width pixels.

    \sa lineWidth()
*/
void QFrame::setLineWidth(int width) {
    width = std::max(0, width);
    if (m_lineWidth == width)
        return;
    m_lineWidth = width;
    updateGeometry();
    update();
}

/*!
    \fn int QFrame::midLineWidth() const

    Returns the width of the line between the two shaded lines of a raised or sunken box, 0 by default.

    \sa setMidLineWidth(), lineWidth()
*/

/*!
    Sets the width of the middle line of the frame to \a width pixels.

    \sa midLineWidth()
*/
void QFrame::setMidLineWidth(int width) {
    width = std::max(0, width);
    if (m_midLineWidth == width)
        return;
    m_midLineWidth = width;
    updateGeometry();
    update();
}

/*!
    Returns the width of the frame: how far its contents are from the edge of the widget. It is 0 without a frame, the
    width from the style for a styled panel, and otherwise from lineWidth() and midLineWidth().

    \sa contentsRect()
*/
int QFrame::frameWidth() const {
    const bool shaded = frameShadow() != Plain;
    switch (frameShape()) {
    case NoFrame:
        return 0;
    case Box:
    case HLine:
    case VLine:
        return shaded ? 2 * m_lineWidth + m_midLineWidth : m_lineWidth;
    case Panel:
        return m_lineWidth;
    case StyledPanel:
        return style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, this);
    }
    return 0;
}

/*!
    \fn QRect QFrame::frameRect() const

    Returns the rectangle of the frame, which is the whole widget.
*/

/*!
    Returns the rectangle inside the frame.

    \sa frameWidth()
*/
QRect QFrame::contentsRect() const {
    const int w = frameWidth();
    return rect().adjusted(w, w, -w, -w);
}

/*!
    \reimp
*/
void QFrame::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    drawFrame(&painter);
}

/*!
    Draws the frame with \a painter, in the rectangle of the widget. paintEvent() does it; a subclass that draws
    something else in its own paintEvent() calls it to have the frame.
*/
void QFrame::drawFrame(QPainter *painter) {
    const Shape shape = frameShape();
    const Shadow shadow = frameShadow();
    if (shape == NoFrame)
        return;
    const QRect r = rect();
    const QPalette &pal = palette();
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const QColor light = pal.color(group, QPalette::Light), dark = pal.color(group, QPalette::Dark);
    const QColor text = pal.color(group, QPalette::WindowText), mid = pal.color(group, QPalette::Window);
    painter->save();
    switch (shape) {
    case StyledPanel: {
        QStyleOptionFrame option;
        option.initFrom(this);
        option.lineWidth = frameWidth();
        style()->drawPrimitive(QStyle::PE_Frame, &option, painter, this);
        break;
    }
    case Box:
        if (shadow == Plain) {
            drawRing(painter, r, m_lineWidth, text, text);
        } else {
            const QColor outerTop = shadow == Raised ? light : dark, outerBottom = shadow == Raised ? dark : light;
            drawRing(painter, r, m_lineWidth, outerTop, outerBottom);
            const int inset = m_lineWidth;
            if (m_midLineWidth > 0)
                drawRing(painter, r.adjusted(inset, inset, -inset, -inset), m_midLineWidth, mid, mid);
            const int inner = inset + m_midLineWidth;
            drawRing(painter, r.adjusted(inner, inner, -inner, -inner), m_lineWidth, outerBottom, outerTop);
        }
        break;
    case Panel:
        if (shadow == Plain)
            drawRing(painter, r, m_lineWidth, text, text);
        else
            drawRing(painter, r, m_lineWidth, shadow == Raised ? light : dark, shadow == Raised ? dark : light);
        break;
    case HLine:
    case VLine: {
        const bool horizontal = shape == HLine;
        const int thickness = frameWidth();
        const QRect band = horizontal ? QRect(r.left(), r.center().y() - thickness / 2, r.width(), thickness)
                                      : QRect(r.center().x() - thickness / 2, r.top(), thickness, r.height());
        if (shadow == Plain) {
            painter->fillRect(band, text);
            break;
        }
        const QColor first = shadow == Sunken ? dark : light, second = shadow == Sunken ? light : dark;
        const int w = m_lineWidth;
        const QRect a = horizontal ? QRect(band.left(), band.top(), band.width(), w)
                                   : QRect(band.left(), band.top(), w, band.height());
        const QRect b = horizontal ? QRect(band.left(), band.bottom() - w + 1, band.width(), w)
                                   : QRect(band.right() - w + 1, band.top(), w, band.height());
        painter->fillRect(band, mid);
        painter->fillRect(a, first);
        painter->fillRect(b, second);
        break;
    }
    case NoFrame:
        break;
    }
    painter->restore();
}

#include "moc_qframe.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
