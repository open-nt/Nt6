// MIT License - see LICENSE
#pragma once

#include <QtCharts/qxyseries.h>

class QLineSeries : public QXYSeries {
    Q_OBJECT
public:
    explicit QLineSeries(QObject *parent = nullptr);
    ~QLineSeries() override;

    QAbstractSeries::SeriesType type() const override;
};
