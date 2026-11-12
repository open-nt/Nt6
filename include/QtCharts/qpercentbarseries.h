// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractbarseries.h>

class QPercentBarSeries : public QAbstractBarSeries {
    Q_OBJECT
public:
    explicit QPercentBarSeries(QObject *parent = nullptr);
    ~QPercentBarSeries() override;

    QAbstractSeries::SeriesType type() const override;
};
