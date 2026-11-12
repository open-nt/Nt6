// MIT License - see LICENSE
#pragma once

#include <QtCharts/qlineseries.h>

class QSplineSeries : public QLineSeries {
    Q_OBJECT
public:
    explicit QSplineSeries(QObject *parent = nullptr);
    ~QSplineSeries() override;

    QAbstractSeries::SeriesType type() const override;
};
