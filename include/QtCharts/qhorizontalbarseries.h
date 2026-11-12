// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractbarseries.h>

class QHorizontalBarSeries : public QAbstractBarSeries {
    Q_OBJECT
public:
    explicit QHorizontalBarSeries(QObject *parent = nullptr);
    ~QHorizontalBarSeries() override;

    QAbstractSeries::SeriesType type() const override;
};
