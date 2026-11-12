// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractbarseries.h>

class QHorizontalStackedBarSeries : public QAbstractBarSeries {
    Q_OBJECT
public:
    explicit QHorizontalStackedBarSeries(QObject *parent = nullptr);
    ~QHorizontalStackedBarSeries() override;

    QAbstractSeries::SeriesType type() const override;
};
