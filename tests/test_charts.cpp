// MIT License - see LICENSE
//
// Tests for Nt Charts on the offscreen platform: the model (series, sets, slices, axes and their ranges),
// number formatting and ticks through the private header, drawing through pixels, the mouse through
// QChartView and the signals of the series, and zooming.
#include <QtCharts/QtCharts>
#include <QtCore/qdatetime.h>
#include <QtGui/qevent.h>
#include <QtGui/qimage.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>

#include "qchartengine_p.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using QtChartsPrivate::ChartEngine;

static bool near(double a, double b, double tolerance = 1e-6) { return std::fabs(a - b) <= tolerance; }

static bool isColor(QRgb pixel, const QColor &color, int tolerance) {
    return std::abs(qRed(pixel) - color.red()) <= tolerance && std::abs(qGreen(pixel) - color.green()) <= tolerance &&
           std::abs(qBlue(pixel) - color.blue()) <= tolerance;
}

static bool sameColor(QRgb pixel, const QColor &color, int tolerance = 12) {
    const bool ok = isColor(pixel, color, tolerance);
    if (!ok)
        std::printf("  pixel is %d,%d,%d, expected %d,%d,%d\n", qRed(pixel), qGreen(pixel), qBlue(pixel), color.red(),
                    color.green(), color.blue());
    return ok;
}

static QImage draw(QChart *chart, int width = 400, int height = 300) {
    chart->setGeometry(QRectF(0, 0, width, height));
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::black);
    QPainter painter(&image);
    chart->paint(&painter);
    return image;
}

static QRgb at(const QImage &image, QPointF p) { return image.pixel(static_cast<int>(p.x()), static_cast<int>(p.y())); }

static void testFormatting() {
    using QtChartsPrivate::formatNumber;
    using QtChartsPrivate::formatTick;
    CHECK(formatNumber("%d", 3.6) == "4");
    CHECK(formatNumber("%d ms", -2.4) == "-2 ms");
    CHECK(formatNumber("%.2f", 3.14159) == "3.14");
    CHECK(formatNumber("%5.1f|", 2.25) == "  2.2|" || formatNumber("%5.1f|", 2.25) == "  2.3|");
    CHECK(formatNumber("%-4d|", 7) == "7   |");
    CHECK(formatNumber("%04d", 42) == "0042");
    CHECK(formatNumber("%+d", 5) == "+5");
    CHECK(formatNumber("%x", 255) == "ff");
    CHECK(formatNumber("%#X", 255) == "0XFF");
    CHECK(formatNumber("%d%%", 50) == "50%");
    CHECK(formatNumber("no conversion", 1) == "no conversion");
    CHECK(formatNumber("%d and %d", 1) == "1 and %d");
    CHECK(formatNumber("%q", 1) == "%q");
    CHECK(formatNumber("%.3e", 12345.0) == "1.234e+04");
    CHECK(formatNumber("%.1E", 0.00125) == "1.2E-03" || formatNumber("%.1E", 0.00125) == "1.3E-03");
    // Widths and precisions are capped, so a hostile format cannot ask for a huge string.
    CHECK(formatNumber("%999999999d", 1).size() <= 64);

    const std::vector<double> ticks = {0.0, 2.5, 5.0, 7.5, 10.0};
    CHECK(formatTick(5.0, ticks) == "5.0");
    CHECK(formatTick(2.5, ticks) == "2.5");
    const std::vector<double> whole = {0.0, 25.0, 50.0};
    CHECK(formatTick(25.0, whole) == "25");
    CHECK(formatTick(-0.0, whole) == "0");
}

static void testXYSeries() {
    QLineSeries series;
    int added = 0, replaced = 0, removed = 0, allReplaced = 0;
    QObject::connect(&series, &QXYSeries::pointAdded, [&](int) { ++added; });
    QObject::connect(&series, &QXYSeries::pointReplaced, [&](int) { ++replaced; });
    QObject::connect(&series, &QXYSeries::pointRemoved, [&](int) { ++removed; });
    QObject::connect(&series, &QXYSeries::pointsReplaced, [&] { ++allReplaced; });
    series.append(0, 1);
    series << QPointF(1, 2) << QPointF(2, 3);
    CHECK(series.count() == 3 && added == 3);
    series.replace(1, QPointF(1, 5));
    CHECK(series.at(1) == QPointF(1, 5) && replaced == 1);
    series.replace(QPointF(1, 5), QPointF(1, 6));
    CHECK(series.at(1).y() == 6.0);
    series.insert(0, QPointF(-1, 0));
    CHECK(series.at(0) == QPointF(-1, 0) && series.count() == 4);
    series.remove(0);
    CHECK(series.count() == 3 && removed == 1);
    series.replace(QList<QPointF>{{0, 0}, {1, 1}});
    CHECK(series.count() == 2 && allReplaced == 1);
    series.removePoints(0, 10);
    CHECK(series.count() == 0);
    CHECK(series.at(5) == QPointF()); // out of range is harmless
    CHECK(series.type() == QAbstractSeries::SeriesTypeLine);
    CHECK(QSplineSeries().type() == QAbstractSeries::SeriesTypeSpline);
    CHECK(QScatterSeries().markerSize() == 15.0);

    QColor changed;
    QObject::connect(&series, &QXYSeries::colorChanged, [&](QColor c) { changed = c; });
    series.setColor(Qt::red);
    CHECK(series.color() == QColor(Qt::red) && changed == QColor(Qt::red));
    CHECK(series.pen().color() == QColor(Qt::red));
}

static void testBarsAndPie() {
    auto *set = new QBarSet("A");
    *set << 1 << 2 << 3;
    CHECK(set->count() == 3 && near(set->sum(), 6.0) && (*set)[1] == 2.0);
    set->replace(1, 4);
    set->remove(0);
    CHECK(set->count() == 2 && set->at(0) == 4.0);

    QBarSeries series;
    CHECK(series.append(set));
    CHECK(!series.append(set));
    CHECK(set->parent() == &series);
    auto *other = new QBarSet("B");
    CHECK(series.append(QList<QBarSet *>{other}));
    CHECK(series.count() == 2);
    CHECK(series.take(other) && series.count() == 1 && other->parent() == nullptr);
    delete other;
    delete set; // a deleted set leaves its series
    CHECK(series.count() == 0);

    QPieSeries pie;
    QPieSlice *a = pie.append("a", 1);
    QPieSlice *b = pie.append("b", 3);
    CHECK(near(pie.sum(), 4.0));
    CHECK(near(a->percentage(), 0.25) && near(b->percentage(), 0.75));
    CHECK(near(a->startAngle(), 0.0) && near(a->angleSpan(), 90.0) && near(b->startAngle(), 90.0));
    b->setValue(1);
    CHECK(near(a->angleSpan(), 180.0));
    pie.setPieEndAngle(180);
    CHECK(near(b->startAngle(), 90.0) && near(b->angleSpan(), 90.0));
    CHECK(a->series() == &pie);
    CHECK(pie.remove(a) && pie.count() == 1);
    delete b;
    CHECK(pie.isEmpty() && near(pie.sum(), 0.0));
}

static void testAxes() {
    QChart chart;
    auto *series = new QLineSeries;
    series->append(2, 10);
    series->append(8, 30);
    chart.addSeries(series);
    CHECK(series->chart() == &chart && series->parent() == &chart);
    chart.createDefaultAxes();
    CHECK(chart.axes().size() == 2);
    auto *x = qobject_cast<QValueAxis *>(chart.axes(Qt::Horizontal).value(0));
    auto *y = qobject_cast<QValueAxis *>(chart.axes(Qt::Vertical).value(0));
    CHECK(x && y);
    if (!x || !y)
        return;
    // A range that was not set fits the data.
    CHECK(near(x->min(), 2) && near(x->max(), 8) && near(y->min(), 10) && near(y->max(), 30));
    CHECK(x->alignment() == Qt::AlignBottom && y->orientation() == Qt::Vertical);
    CHECK(series->attachedAxes().size() == 2);

    // A range that was set is kept when another series is attached.
    y->setRange(0, 100);
    auto *more = new QLineSeries;
    more->append(0, 500);
    chart.addSeries(more);
    more->attachAxis(y);
    CHECK(near(y->max(), 100));

    y->setRange(0.3, 9.6);
    y->applyNiceNumbers();
    CHECK(near(y->min(), 0) && near(y->max(), 10));

    const QtChartsPrivate::AxisTicks ticks = ChartEngine::ticks(y);
    CHECK(ticks.labels.size() == y->tickCount());
    y->setTickType(QValueAxis::TicksDynamic);
    y->setTickInterval(2.5);
    const QtChartsPrivate::AxisTicks dynamic = ChartEngine::ticks(y);
    CHECK(dynamic.labels == QStringList({"0.0", "2.5", "5.0", "7.5", "10.0"}));
    y->setLabelFormat("%d s");
    CHECK(ChartEngine::ticks(y).labels.value(1) == "3 s" || ChartEngine::ticks(y).labels.value(1) == "2 s");

    // Removing an axis detaches it; the caller owns it.
    chart.removeAxis(x);
    CHECK(x->parent() == nullptr && !series->attachedAxes().contains(x) && chart.axes(Qt::Horizontal).isEmpty());
    delete x;

    QLogValueAxis log;
    log.setRange(1, 1000);
    CHECK(log.tickCount() == 4);
    const QtChartsPrivate::AxisTicks logTicks = ChartEngine::ticks(&log);
    CHECK(logTicks.labels == QStringList({"1", "10", "100", "1000"}));
    log.setRange(-1, 10); // not positive: ignored
    CHECK(near(log.min(), 1));

    QBarCategoryAxis categories;
    categories.append({"a", "b", "c", "d"});
    categories.append("a"); // duplicates are skipped
    CHECK(categories.count() == 4 && categories.at(2) == "c");
    categories.setRange("b", "c");
    CHECK(categories.min() == "b" && categories.max() == "c");
    CHECK(ChartEngine::ticks(&categories).labels == QStringList({"b", "c"}));
    categories.remove("d");
    CHECK(categories.count() == 3);

    QDateTimeAxis time;
    const QDateTime start(QDate(2026, 1, 2), QTime(10, 0));
    time.setRange(start, start.addSecs(3600));
    time.setFormat("hh:mm");
    time.setTickCount(3);
    CHECK(ChartEngine::ticks(&time).labels == QStringList({"10:00", "10:30", "11:00"}));
    CHECK(time.min() == start);
}

static void testDefaultAxesForBars() {
    QChart chart;
    auto *series = new QBarSeries;
    auto *set = new QBarSet("x");
    *set << 3 << -1 << 4;
    series->append(set);
    chart.addSeries(series);
    chart.createDefaultAxes();
    auto *categories = qobject_cast<QBarCategoryAxis *>(chart.axes(Qt::Horizontal).value(0));
    auto *values = qobject_cast<QValueAxis *>(chart.axes(Qt::Vertical).value(0));
    CHECK(categories && values);
    if (!categories || !values)
        return;
    // An empty category axis gets numbers, and the value axis includes zero.
    CHECK(categories->categories() == QStringList({"1", "2", "3"}));
    CHECK(near(values->min(), -1) && near(values->max(), 4));

    auto *stacked = new QStackedBarSeries;
    auto *s1 = new QBarSet("1");
    auto *s2 = new QBarSet("2");
    *s1 << 2 << 3;
    *s2 << 5 << -4;
    stacked->append(s1);
    stacked->append(s2);
    const QtChartsPrivate::Bounds b = ChartEngine::seriesBounds(stacked);
    CHECK(b.valid && near(b.minY, -4) && near(b.maxY, 7) && near(b.minX, -0.5) && near(b.maxX, 1.5));
    delete stacked;
}

static void testDrawing() {
    QChart chart;
    chart.legend()->hide();
    chart.setMargins(QMargins(10, 10, 10, 10));
    auto *set = new QBarSet("x");
    *set << 10;
    set->setColor(QColor(200, 30, 40));
    auto *series = new QBarSeries;
    series->setBarWidth(1.0);
    series->append(set);
    chart.addSeries(series);
    const QImage image = draw(&chart);
    const QRectF plot = chart.plotArea();
    CHECK(near(plot.left(), 10) && near(plot.width(), 380) && near(plot.height(), 280));
    // Without axes the bar fills its category: the whole plot area.
    CHECK(sameColor(at(image, plot.center()), QColor(200, 30, 40)));
    // The background is the theme's.
    CHECK(sameColor(image.pixel(3, 3), QtChartsPrivate::theme(QChart::ChartThemeLight).background));
    chart.setTheme(QChart::ChartThemeDark);
    CHECK(set->color() == QtChartsPrivate::theme(QChart::ChartThemeDark).palette[0]); // the set color is forgotten
    const QImage dark = draw(&chart);
    CHECK(sameColor(dark.pixel(3, 3), QtChartsPrivate::theme(QChart::ChartThemeDark).background));

    // A line from corner to corner of the plot area.
    QChart lineChart;
    lineChart.legend()->hide();
    auto *line = new QLineSeries;
    line->append(0, 0);
    line->append(10, 10);
    line->setPen(QPen(QBrush(QColor(0, 0, 255)), 4.0));
    lineChart.addSeries(line);
    lineChart.createDefaultAxes();
    const QImage lineImage = draw(&lineChart);
    const QPointF mid = lineChart.mapToPosition(QPointF(5, 5), line);
    CHECK(lineChart.plotArea().contains(mid));
    CHECK(sameColor(at(lineImage, mid), QColor(0, 0, 255)));
    CHECK(near(lineChart.mapToValue(mid, line).x(), 5, 1e-6));
    // Off the line, the plot area is background.
    const QPointF off = lineChart.mapToPosition(QPointF(8, 2), line);
    CHECK(!isColor(at(lineImage, off), QColor(0, 0, 255), 40));

    // A series is cut to the plot area.
    lineChart.axes(Qt::Horizontal).first()->setRange(0, 5);
    const QImage cut = draw(&lineChart);
    const QRectF p = lineChart.plotArea();
    CHECK(!isColor(at(cut, QPointF(p.right() + 3, p.top() + 1)), QColor(0, 0, 255), 40));

    // A pie: the first slice starts at 12 o'clock and goes clockwise.
    QChart pieChart;
    pieChart.legend()->hide();
    auto *pie = new QPieSeries;
    pie->append("a", 1)->setColor(QColor(0, 160, 0));
    pie->append("b", 3)->setColor(QColor(160, 0, 160));
    pieChart.addSeries(pie);
    const QImage pieImage = draw(&pieChart, 300, 300);
    const QPointF c = pieChart.plotArea().center();
    CHECK(sameColor(at(pieImage, c + QPointF(30, -30)), QColor(0, 160, 0)));
    CHECK(sameColor(at(pieImage, c + QPointF(-30, 30)), QColor(160, 0, 160)));

    // A chart without series, or of no size, draws without trouble.
    QChart empty;
    draw(&empty, 10, 10);
    empty.setGeometry(QRectF(0, 0, 0, 0));
    QImage none(4, 4, QImage::Format_RGB32);
    QPainter painter(&none);
    empty.paint(&painter);
}

static void testZoom() {
    QChart chart;
    auto *series = new QLineSeries;
    series->append(0, 0);
    series->append(100, 100);
    chart.addSeries(series);
    chart.createDefaultAxes();
    chart.setGeometry(QRectF(0, 0, 400, 300));
    auto *x = qobject_cast<QValueAxis *>(chart.axes(Qt::Horizontal).value(0));
    CHECK(x && !chart.isZoomed());
    if (!x)
        return;
    chart.zoomIn();
    CHECK(chart.isZoomed() && near(x->min(), 25, 1e-6) && near(x->max(), 75, 1e-6));
    chart.zoomOut();
    CHECK(near(x->min(), 0, 1e-6) && near(x->max(), 100, 1e-6));
    const QRectF plot = chart.plotArea();
    chart.scroll(plot.width() / 10.0, 0);
    CHECK(near(x->min(), 10, 1e-6) && near(x->max(), 110, 1e-6));
    chart.zoomReset();
    CHECK(!chart.isZoomed() && near(x->min(), 0) && near(x->max(), 100));
}

static void testView() {
    auto *chart = new QChart;
    auto *set = new QBarSet("x");
    *set << 5 << 5;
    auto *series = new QBarSeries;
    series->setBarWidth(0.8);
    series->append(set);
    chart->addSeries(series);
    chart->createDefaultAxes();
    chart->legend()->hide();

    QChartView view(chart);
    CHECK(chart->parent() == &view && view.chart() == chart);
    view.resize(400, 300);
    view.show();
    QApplication::processEvents();
    CHECK(near(chart->size().width(), 400) && near(chart->size().height(), 300));

    int clicked = -1, seriesClicked = -1, hovered = 0;
    QObject::connect(set, &QBarSet::clicked, [&](int index) { clicked = index; });
    QObject::connect(series, &QBarSeries::clicked,
                     [&](int index, QBarSet *s) { seriesClicked = s == set ? index : -2; });
    QObject::connect(set, &QBarSet::hovered, [&](bool state, int) { hovered += state ? 1 : -1; });

    const QPointF bar = chart->mapToPosition(QPointF(1, 2.5), series);
    QMouseEvent move(QEvent::MouseMove, bar, bar, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &move);
    CHECK(hovered == 1);
    QMouseEvent press(QEvent::MouseButtonPress, bar, bar, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, bar, bar, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &press);
    QApplication::sendEvent(&view, &release);
    CHECK(clicked == 1 && seriesClicked == 1);
    const QPointF away = chart->mapToPosition(QPointF(0.5, 4.9), series);
    QMouseEvent leave(QEvent::MouseMove, away, away, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &leave);
    CHECK(hovered == 0);

    // A pie slice is clicked where it is drawn.
    auto *pieChart = new QChart;
    auto *pie = new QPieSeries;
    QPieSlice *right = pie->append("right", 1);
    pie->append("left", 1);
    pieChart->addSeries(pie);
    pieChart->legend()->hide();
    view.setChart(pieChart);
    CHECK(chart->parent() == nullptr); // given up, not deleted
    delete chart;
    QApplication::processEvents();
    QPieSlice *got = nullptr;
    QObject::connect(pie, &QPieSeries::clicked, [&](QPieSlice *s) { got = s; });
    const QPointF r = pieChart->plotArea().center() + QPointF(30, 0);
    QMouseEvent p2(QEvent::MouseButtonPress, r, r, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent r2(QEvent::MouseButtonRelease, r, r, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &p2);
    QApplication::sendEvent(&view, &r2);
    CHECK(got == right);

    // Rubber band zoom.
    auto *lineChart = new QChart;
    auto *line = new QLineSeries;
    line->append(0, 0);
    line->append(100, 100);
    lineChart->addSeries(line);
    lineChart->createDefaultAxes();
    view.setChart(lineChart);
    delete pieChart;
    view.setRubberBand(QChartView::HorizontalRubberBand);
    QApplication::processEvents();
    auto *x = qobject_cast<QValueAxis *>(lineChart->axes(Qt::Horizontal).value(0));
    const QPointF from = lineChart->mapToPosition(QPointF(20, 50), line);
    const QPointF to = lineChart->mapToPosition(QPointF(60, 50), line);
    QMouseEvent bp(QEvent::MouseButtonPress, from, from, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent bm(QEvent::MouseMove, to, to, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent br(QEvent::MouseButtonRelease, to, to, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &bp);
    QApplication::sendEvent(&view, &bm);
    QApplication::sendEvent(&view, &br);
    CHECK(x && lineChart->isZoomed() && near(x->min(), 20, 1.0) && near(x->max(), 60, 1.0));
}

static void testOwnership() {
    auto *chart = new QChart;
    auto *series = new QLineSeries;
    QPointer<QLineSeries> watch(series);
    chart->addSeries(series);
    chart->removeSeries(series);
    CHECK(series->chart() == nullptr && series->parent() == nullptr);
    chart->addSeries(series);
    auto *axis = new QValueAxis;
    QPointer<QValueAxis> watchAxis(axis);
    chart->addAxis(axis, Qt::AlignLeft);
    CHECK(series->attachAxis(axis));
    delete axis; // leaves the series and the chart
    CHECK(series->attachedAxes().isEmpty() && chart->axes().isEmpty());
    delete chart;
    CHECK(watch.isNull());

    // A series in one chart cannot join another, and an axis in another chart cannot be attached.
    QChart a, b;
    auto *s = new QLineSeries;
    a.addSeries(s);
    b.addSeries(s);
    CHECK(s->chart() == &a && b.series().isEmpty());
    auto *foreign = new QValueAxis;
    b.addAxis(foreign, Qt::AlignBottom);
    CHECK(!s->attachAxis(foreign));
    a.removeAllSeries();
    CHECK(a.series().isEmpty());
}

int main(int argc, char *argv[]) {
    ::setenv("QT_QPA_PLATFORM", "offscreen", 1);
    QApplication app(argc, argv);
    testFormatting();
    testXYSeries();
    testBarsAndPie();
    testAxes();
    testDefaultAxesForBars();
    testDrawing();
    testZoom();
    testView();
    testOwnership();
    if (g_failures)
        std::printf("%d failure(s)\n", g_failures);
    else
        std::printf("all chart tests passed\n");
    return g_failures ? 1 : 0;
}
