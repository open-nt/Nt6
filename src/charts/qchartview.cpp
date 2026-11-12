// MIT License - see LICENSE
#include <QtCharts/qchartview.h>

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qrubberband.h>

#include "qchartengine_p.h"

#include <cmath>

using QtChartsPrivate::ChartEngine;

/*!
    \class QChartView
    \brief The QChartView class is a widget that shows a chart.

    The view draws its chart over its whole area, and resizes the chart with it. It owns the chart.

    \snippet charts/dashboard.cpp view

    The mouse reaches the series: clicking a bar emits QBarSet::clicked(), hovering over a slice emits
    QPieSlice::hovered(), and so on. With setRubberBand(), dragging with the left button zooms into the
    rectangle dragged over, and clicking the right button zooms out.

    \differences
    The view is a QWidget, not a \c QGraphicsView: there is no scene, and the chart cannot be moved or
    transformed in it. The chart is always drawn antialiased; the render hints are kept but not used.
    \enddifferences

    \sa QChart
*/

/*!
    \enum QChartView::RubberBand

    How dragging with the mouse zooms.

    \value NoRubberBand Dragging does not zoom.
    \value VerticalRubberBand Dragging selects a range of the vertical axes.
    \value HorizontalRubberBand Dragging selects a range of the horizontal axes.
    \value RectangleRubberBand Dragging selects a rectangle.
    \value ClickThroughRubberBand Combined with one of the others: presses and releases reach the series
           too.
*/

/*!
    \typedef QChartView::RubberBands

    A QFlags of RubberBand values.
*/

/*!
    Constructs a view with the given \a parent, showing a new, empty chart.
*/
QChartView::QChartView(QWidget *parent) : QChartView(new QChart, parent) { m_createdChart = true; }

/*!
    Constructs a view with the given \a parent that shows \a chart and takes ownership of it.
*/
QChartView::QChartView(QChart *chart, QWidget *parent) : QWidget(parent) {
    m_hints = QPainter::Antialiasing;
    setMouseTracking(true);
    attach(chart);
}

/*!
    Destroys the view and its chart.
*/
QChartView::~QChartView() {
    if (m_chart)
        ChartEngine::setView(m_chart, nullptr);
}

void QChartView::attach(QChart *chart) {
    if (chart == m_chart)
        return;
    if (m_chart) {
        ChartEngine::setView(m_chart, nullptr);
        if (m_createdChart)
            delete m_chart;
        else if (m_chart->parent() == this)
            m_chart->setParent(nullptr);
    }
    m_createdChart = false;
    m_chart = chart;
    if (m_chart) {
        m_chart->setParent(this);
        ChartEngine::setView(m_chart, this);
        m_chart->setGeometry(QRectF(rect()));
    }
    update();
}

/*!
    Sets how dragging zooms to \a rubberBands.
*/
void QChartView::setRubberBand(const RubberBands &rubberBands) {
    m_rubberBand = rubberBands;
    if (!(m_rubberBand & RectangleRubberBand) && m_band) {
        m_band->hide();
        m_banding = false;
    }
}

/*!
    Returns how dragging zooms. The default is NoRubberBand.
*/
QChartView::RubberBands QChartView::rubberBand() const { return m_rubberBand; }

/*!
    Shows \a chart instead of the current one, and takes ownership of it. The view gives up the previous
    chart without deleting it, unless the view made it itself. A \c nullptr leaves the view empty.
*/
void QChartView::setChart(QChart *chart) { attach(chart); }

/*!
    Returns the chart the view shows.
*/
QChart *QChartView::chart() const { return m_chart; }

/*!
    Turns the render hint \a hint on if \a enabled is \c true, and off otherwise. Nt keeps the hints; the
    chart is always antialiased.
*/
void QChartView::setRenderHint(QPainter::RenderHint hint, bool enabled) {
    if (enabled)
        m_hints |= hint;
    else
        m_hints &= ~QPainter::RenderHints(hint);
}

/*!
    Sets the render hints to \a hints.
*/
void QChartView::setRenderHints(QPainter::RenderHints hints) { m_hints = hints; }

/*!
    Returns the render hints. The default is QPainter::Antialiasing.
*/
QPainter::RenderHints QChartView::renderHints() const { return m_hints; }

/*!
    \reimp
    Returns 400 by 300 pixels.
*/
QSize QChartView::sizeHint() const { return QSize(400, 300); }

/*!
    \reimp
    Draws the chart. \a event is not used.
*/
void QChartView::paintEvent(QPaintEvent *event) {
    (void)event;
    if (!m_chart)
        return;
    QPainter painter(this);
    m_chart->paint(&painter);
}

/*!
    \reimp
    Resizes the chart to the view. \a event is not used.
*/
void QChartView::resizeEvent(QResizeEvent *event) {
    (void)event;
    if (m_chart)
        m_chart->setGeometry(QRectF(rect()));
}

QRectF QChartView::bandRect() const { return m_band ? QRectF(m_band->geometry()) : QRectF(); }

/*!
    \reimp
    Starts a rubber band, or passes the press of \a event to the series under the mouse.
*/
void QChartView::mousePressEvent(QMouseEvent *event) {
    if (!m_chart)
        return;
    const QPointF pos = event->position();
    const bool band =
        (m_rubberBand & RectangleRubberBand) && event->button() == Qt::LeftButton && m_chart->plotArea().contains(pos);
    if (band) {
        if (!m_band)
            m_band = new QRubberBand(QRubberBand::Rectangle, this);
        m_banding = true;
        m_bandOrigin = pos;
        m_band->setGeometry(QRect(pos.toPoint(), QSize(0, 0)));
        m_band->show();
    }
    if (!band || (m_rubberBand & ClickThroughRubberBand))
        ChartEngine::mouse(m_chart, ChartEngine::Mouse::Press, pos);
}

/*!
    \reimp
    Stretches the rubber band, or tells the series under the mouse that it is hovered; \a event has the
    position.
*/
void QChartView::mouseMoveEvent(QMouseEvent *event) {
    if (!m_chart)
        return;
    const QPointF pos = event->position();
    if (m_banding && m_band) {
        const QRectF plot = m_chart->plotArea();
        QPointF to(std::clamp(pos.x(), plot.left(), plot.right()), std::clamp(pos.y(), plot.top(), plot.bottom()));
        QRectF r = QRectF(m_bandOrigin, to).normalized();
        if (!(m_rubberBand & VerticalRubberBand)) {
            r.setTop(plot.top());
            r.setBottom(plot.bottom());
        }
        if (!(m_rubberBand & HorizontalRubberBand)) {
            r.setLeft(plot.left());
            r.setRight(plot.right());
        }
        m_band->setGeometry(r.toRect());
        return;
    }
    ChartEngine::mouse(m_chart, ChartEngine::Mouse::Move, pos);
}

/*!
    \reimp
    Zooms into the rubber band, zooms out on a right click, or passes the release of \a event to the
    series under the mouse.
*/
void QChartView::mouseReleaseEvent(QMouseEvent *event) {
    if (!m_chart)
        return;
    const QPointF pos = event->position();
    if (m_banding && event->button() == Qt::LeftButton) {
        m_banding = false;
        const QRectF r = bandRect();
        m_band->hide();
        if (r.width() > 2.0 && r.height() > 2.0)
            m_chart->zoomIn(r);
        if (m_rubberBand & ClickThroughRubberBand)
            ChartEngine::mouse(m_chart, ChartEngine::Mouse::Release, pos);
        return;
    }
    if ((m_rubberBand & RectangleRubberBand) && event->button() == Qt::RightButton) {
        m_chart->zoomOut();
        return;
    }
    ChartEngine::mouse(m_chart, ChartEngine::Mouse::Release, pos);
}

/*!
    \reimp
    Passes the double click of \a event to the series under the mouse.
*/
void QChartView::mouseDoubleClickEvent(QMouseEvent *event) {
    if (m_chart)
        ChartEngine::mouse(m_chart, ChartEngine::Mouse::DoubleClick, event->position());
}

/*!
    \reimp
    Tells the series under the mouse that it is no longer hovered. \a event is not used.
*/
void QChartView::leaveEvent(QEvent *event) {
    (void)event;
    if (m_chart)
        ChartEngine::mouse(m_chart, ChartEngine::Mouse::Leave, QPointF());
}

#include "moc_qchartview.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
