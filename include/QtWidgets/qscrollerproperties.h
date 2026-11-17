// MIT License - see LICENSE
#pragma once

#include <QtCore/qmap.h>
#include <QtCore/qvariant.h>

class QScrollerProperties {
public:
    enum OvershootPolicy { OvershootWhenScrollable, OvershootAlwaysOff, OvershootAlwaysOn };
    enum FrameRates { Standard, Fps60, Fps30, Fps20 };
    enum ScrollMetric {
        MousePressEventDelay,
        DragStartDistance,
        DragVelocitySmoothingFactor,
        AxisLockThreshold,
        ScrollingCurve,
        DecelerationFactor,
        MinimumVelocity,
        MaximumVelocity,
        MaximumClickThroughVelocity,
        AcceleratingFlickMaximumTime,
        AcceleratingFlickSpeedupFactor,
        SnapPositionRatio,
        SnapTime,
        OvershootDragResistanceFactor,
        OvershootDragDistanceFactor,
        OvershootScrollDistanceFactor,
        OvershootScrollTime,
        HorizontalOvershootPolicy,
        VerticalOvershootPolicy,
        FrameRate,
        ScrollMetricCount
    };

    QScrollerProperties();
    QScrollerProperties(const QScrollerProperties &other) = default;
    QScrollerProperties &operator=(const QScrollerProperties &other) = default;
    ~QScrollerProperties() = default;

    bool operator==(const QScrollerProperties &other) const { return m_values == other.m_values; }
    bool operator!=(const QScrollerProperties &other) const { return !(*this == other); }

    static void setDefaultScrollerProperties(const QScrollerProperties &sp);
    static void unsetDefaultScrollerProperties();

    QVariant scrollMetric(ScrollMetric metric) const;
    void setScrollMetric(ScrollMetric metric, const QVariant &value);

private:
    QMap<int, QVariant> m_values; // the metrics that differ from the built-in defaults
};
