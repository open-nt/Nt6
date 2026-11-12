// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractbarseries.h>

class QHorizontalPercentBarSeries : public QAbstractBarSeries {
    Q_OBJECT
public:
    explicit QHorizontalPercentBarSeries(QObject *parent = nullptr);
    ~QHorizontalPercentBarSeries() override;

    QAbstractSeries::SeriesType type() const override;
};
