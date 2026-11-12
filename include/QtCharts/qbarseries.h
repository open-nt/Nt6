// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractbarseries.h>

class QBarSeries : public QAbstractBarSeries {
    Q_OBJECT
public:
    explicit QBarSeries(QObject *parent = nullptr);
    ~QBarSeries() override;

    QAbstractSeries::SeriesType type() const override;
};
