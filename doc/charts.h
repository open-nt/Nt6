// MIT License - see LICENSE
//
// Overview page about Nt Charts for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page nt-charts.html
    \title Using Nt Charts
    \brief How to draw line, area, scatter, bar and pie charts, and show them in a widget.

    Nt Charts draws charts for dashboards and reports in applications made with Nt Widgets. It has the API
    of Qt Charts: a QChart holds series, axes, a legend and a title, and a QChartView shows it.

    \code
    find_package(Nt6 REQUIRED)
    add_executable(dashboard main.cpp)
    target_link_libraries(dashboard PRIVATE Nt::Charts)
    \endcode

    \section1 A First Chart

    Make a series, add it to a chart, give the chart axes, and show it in a view:

    \snippet charts/dashboard.cpp line

    \snippet charts/dashboard.cpp view

    The chart owns its series and axes, and the view owns the chart. createDefaultAxes() makes a pair of
    axes that fit the data of all series. For more control, make the axes yourself, add them with
    QChart::addAxis() and attach them to the series with QAbstractSeries::attachAxis():

    \snippet charts/dashboard.cpp bars

    \section1 The Kinds of Charts

    QLineSeries joins its points with straight lines.

    \image charts/qlineseries.png A line chart

    QSplineSeries draws a smooth curve through its points.

    \image charts/qsplineseries.png A spline chart

    QScatterSeries draws a marker at each point.

    \image charts/qscatterseries.png A scatter chart

    QAreaSeries fills between two lines, or below one.

    \image charts/qareaseries.png An area chart

    QBarSeries draws the values of each QBarSet side by side.

    \image charts/qbarseries.png A bar chart

    QStackedBarSeries stacks them.

    \image charts/qstackedbarseries.png A stacked bar chart

    QPercentBarSeries shows each set's share of a category.

    \image charts/qpercentbarseries.png A percent bar chart

    QHorizontalBarSeries, QHorizontalStackedBarSeries and QHorizontalPercentBarSeries lay the bars down.

    \image charts/qhorizontalbarseries.png A horizontal bar chart

    QPieSeries draws its slices as a pie.

    \image charts/qpieseries.png A pie chart

    With a hole, a pie series is a donut.

    \image charts/donut.png A donut chart

    \section1 Axes

    QValueAxis maps real numbers, QLogValueAxis a logarithmic scale, QDateTimeAxis dates and times given
    in milliseconds since the epoch, and QBarCategoryAxis named categories. An axis whose range was not
    set takes the extent of the data of its series when they are attached; set the range yourself for
    data that changes.

    \image charts/qdatetimeaxis.png A date and time axis

    \image charts/qlogvalueaxis.png A logarithmic axis

    \section1 Live Data

    For data that keeps arriving, replace all points of a series at once with QXYSeries::replace(), and
    move the axis along with it:

    \code
    QList<QPointF> points = series->points();
    points.removeFirst();
    points.append(QPointF(t, sample));
    series->replace(points);
    chart->axes(Qt::Horizontal).first()->setRange(t - 59, t);
    \endcode

    The view draws the chart again after each change, once per frame at most.

    \section1 Themes

    QChart::setTheme() sets the colors of the whole chart. Colors set on the chart, its series and axes win
    over the theme until it is changed again.

    QChart::ChartThemeLight:

    \image charts/theme-light.png The ChartThemeLight theme

    QChart::ChartThemeDark:

    \image charts/theme-dark.png The ChartThemeDark theme

    QChart::ChartThemeBlueCerulean:

    \image charts/theme-blue-cerulean.png The ChartThemeBlueCerulean theme

    QChart::ChartThemeBrownSand:

    \image charts/theme-brown-sand.png The ChartThemeBrownSand theme

    QChart::ChartThemeBlueNcs:

    \image charts/theme-blue-ncs.png The ChartThemeBlueNcs theme

    QChart::ChartThemeHighContrast:

    \image charts/theme-high-contrast.png The ChartThemeHighContrast theme

    QChart::ChartThemeBlueIcy:

    \image charts/theme-blue-icy.png The ChartThemeBlueIcy theme

    QChart::ChartThemeQt:

    \image charts/theme-qt.png The ChartThemeQt theme

    \section1 The Mouse

    Series emit signals when they are clicked or hovered: QBarSet::clicked(), QPieSlice::hovered(),
    QXYSeries::clicked() and so on.

    \snippet charts/dashboard.cpp pie

    With QChartView::setRubberBand(), dragging over the plot area zooms into it, and a right click zooms
    out. QChart::zoomIn(), QChart::zoom(), QChart::scroll() and QChart::zoomReset() do the same from code.

    \section1 Without a View

    QChart::paint() draws a chart on any painter, so a chart can be written to an image or printed:

    \snippet charts/dashboard.cpp image

    The \c chart_dashboard example shows four charts that update live. The pictures on this page are
    drawn by \c render_charts; \c{cmake --build build --target chart_images} draws them again.

    \section1 Differences from Qt Charts

    QChart is a QObject and QChartView a QWidget: there is no graphics scene, so charts cannot be
    transformed or mixed with other graphics items. There are no animations, polar charts, box plots,
    candlesticks, category axes with ranges, color axes, model mappers or \c QLegendMarker objects.
    Labels are drawn upright. Each class lists its own differences.
*/
