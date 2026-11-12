// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractbarseries.h>

class QStackedBarSeries : public QAbstractBarSeries {
    Q_OBJECT
public:
    explicit QStackedBarSeries(QObject *parent = nullptr);
    ~QStackedBarSeries() override;

    QAbstractSeries::SeriesType type() const override;
};
