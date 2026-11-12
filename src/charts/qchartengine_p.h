// MIT License - see LICENSE
//
// The private side of Nt Charts. The public classes hold their properties; ChartEngine, a friend of each of
// them, keeps the chart, its series and its axes consistent, and lays out, draws and hit-tests a chart.
// Shapes are filled and stroked by the antialiased rasterizer of Nt SVG; QPainter draws the result as an
// image, and the text on top of it.
#pragma once

#include <QtCharts/qabstractaxis.h>
#include <QtCharts/qabstractbarseries.h>
#include <QtCharts/qabstractseries.h>
#include <QtCharts/qchart.h>
#include <QtCore/qlist.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>
#include <QtGui/qcolor.h>

#include <vector>

class QBarSet;
class QPainter;
class QPieSlice;
class QPieSeries;
class QXYSeries;

namespace QtChartsPrivate {

// The colors of a chart theme.
struct Theme {
    QColor background;
    QColor title;
    QColor labels;
    QColor axisLine;
    QColor grid;
    QColor minorGrid;
    QColor shades;
    QColor legendBackground;
    QColor legendBorder;
    std::vector<QColor> palette;
};

const Theme &theme(QChart::ChartTheme which);

// Formats `value` with a printf-like format of one conversion (%d, %i, %u, %o, %x, %X, %f, %e, %g, %c and their
// flags, width and precision), with text around it. The format is data: anything else is copied as it is.
QString formatNumber(const QString &format, double value);
// Formats `value` with the fewest decimals that represent each of `values` exactly, or with 6 at most.
QString formatTick(double value, const std::vector<double> &values);

// A linear or logarithmic mapping between values and a span of pixels.
struct Scale {
    double min = 0.0;
    double max = 1.0;
    bool log = false;
    double base = 10.0;
    bool reverse = false;

    bool isValid() const;
    // Maps a value to [a0, a1]; a0 is where min goes unless the scale is reversed.
    double toPixel(double value, double a0, double a1) const;
    double toValue(double pixel, double a0, double a1) const;
};

// The extent of the data of a series, in the coordinates of its axes.
struct Bounds {
    double minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0;
    double minPositiveX = 0.0, minPositiveY = 0.0; // for logarithmic axes; 0 if there is none
    bool valid = false;

    void add(double x, double y);
};

struct AxisTicks {
    std::vector<double> grid;  // values with a grid line and a tick
    std::vector<double> minor; // values with a minor grid line
    std::vector<double> labelValues;
    QStringList labels;
};

struct LegendEntry {
    QString text;
    QColor color;
    QAbstractSeries *series = nullptr;
    QRectF marker;
    QRectF textRect;
};

struct AxisPlace {
    QAbstractAxis *axis = nullptr;
    double offset = 0.0; // distance from the plot area to the axis line
    double extent = 0.0; // room the labels and title take, beyond the line
};

struct Layout {
    QRectF titleRect;
    QRectF legendRect;
    std::vector<LegendEntry> legend;
    QRectF plot;
    std::vector<AxisPlace> axes;
};

// The geometry of one bar.
struct BarRect {
    QBarSet *set = nullptr;
    int index = 0;
    double value = 0.0;
    QRectF rect;
};

// The geometry of one pie slice, in degrees clockwise from 12 o'clock.
struct SliceGeometry {
    QPieSlice *slice = nullptr;
    QPointF center;
    double outer = 0.0;
    double inner = 0.0;
    double start = 0.0;
    double span = 0.0;
};

class ChartEngine {
public:
    // Structure.
    static void addSeries(QChart *chart, QAbstractSeries *series);
    static void removeSeries(QChart *chart, QAbstractSeries *series);
    static void addAxis(QChart *chart, QAbstractAxis *axis, Qt::Alignment alignment);
    static void removeAxis(QChart *chart, QAbstractAxis *axis);
    static bool attach(QAbstractSeries *series, QAbstractAxis *axis);
    static bool detach(QAbstractSeries *series, QAbstractAxis *axis);
    static void createDefaultAxes(QChart *chart);
    static QLegend *createLegend(QChart *chart);
    static void destroyChart(QChart *chart);
    static void seriesDestroyed(QAbstractSeries *series);
    static void axisDestroyed(QAbstractAxis *axis);
    static void barSetDestroyed(QBarSet *set);
    static void sliceDestroyed(QPieSlice *slice);
    static QAbstractBarSeries *barSetSeries(const QBarSet *set);
    static void setBarSetSeries(QBarSet *set, QAbstractBarSeries *series);
    static void releaseBarSet(QBarSet *set);
    static void forgetStyle(QChart *chart);
    static void forgetHits(QChart *chart, const QObject *object);

    // Notifies the view that the chart must be drawn again.
    static void changed(QChart *chart);
    static void changed(QAbstractSeries *series);
    static void changed(QAbstractAxis *axis);
    static void changed(QBarSet *set);
    static void changed(QPieSlice *slice);

    // Theme lookups for what the user has not set.
    static const Theme &themeOf(const QChart *chart);
    static QColor seriesColor(const QAbstractSeries *series);
    static QColor barSetColor(const QBarSet *set);
    static QColor sliceColor(const QPieSlice *slice);
    static QColor backgroundColor(const QChart *chart);

    // Axes and scales.
    static bool axisRange(const QAbstractAxis *axis, double *min, double *max);
    static void setAxisRange(QAbstractAxis *axis, double min, double max, bool byUser);
    // Called by the range setters of the axes: a range set from outside is kept when series are attached.
    static void rangeSetByUser(QAbstractAxis *axis);
    static bool hasUserRange(const QAbstractAxis *axis);
    static Scale axisScale(const QAbstractAxis *axis);
    static AxisTicks ticks(const QAbstractAxis *axis);
    static void fitAxis(QAbstractAxis *axis);
    static QAbstractAxis *seriesAxis(const QAbstractSeries *series, Qt::Orientation orientation);
    static Bounds seriesBounds(const QAbstractSeries *series);
    static Scale seriesScale(const QAbstractSeries *series, Qt::Orientation orientation);
    static QPointF toPosition(const QAbstractSeries *series, const QPointF &value, const QRectF &plot);
    static QPointF toValue(const QAbstractSeries *series, const QPointF &position, const QRectF &plot);

    // Geometry and drawing.
    static Layout layout(QChart *chart);
    static std::vector<BarRect> bars(const QAbstractBarSeries *series, const QRectF &plot);
    static std::vector<SliceGeometry> slices(const QPieSeries *series, const QRectF &plot);
    static void paint(QChart *chart, QPainter *painter);

    // Zooming.
    static void zoomTo(QChart *chart, const QRectF &rect);

    // The mouse, from QChartView.
    enum class Mouse { Press, Release, DoubleClick, Move, Leave };
    static void mouse(QChart *chart, Mouse what, const QPointF &position);
    static void setView(QChart *chart, QWidget *view);
};

} // namespace QtChartsPrivate
