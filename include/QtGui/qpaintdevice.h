// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qpoint.h>

class QPaintEngine;
class QRegion;

class QPaintDevice {
public:
    enum PaintDeviceMetric {
        PdmWidth = 1,
        PdmHeight = 2,
        PdmWidthMM = 3,
        PdmHeightMM = 4,
        PdmNumColors = 5,
        PdmDepth = 6,
        PdmDpiX = 7,
        PdmDpiY = 8,
        PdmDevicePixelRatio = 11,
        PdmDevicePixelRatioScaled = 12
    };

    virtual ~QPaintDevice();

    virtual QPaintEngine *paintEngine() const = 0;

    int width() const { return metric(PdmWidth); }
    int height() const { return metric(PdmHeight); }
    int widthMM() const { return metric(PdmWidthMM); }
    int heightMM() const { return metric(PdmHeightMM); }
    int colorCount() const { return metric(PdmNumColors); }
    int depth() const { return metric(PdmDepth); }
    int logicalDpiX() const { return metric(PdmDpiX); }
    int logicalDpiY() const { return metric(PdmDpiY); }
    qreal devicePixelRatio() const { return metric(PdmDevicePixelRatioScaled) / 65536.0; }
    bool paintingActive() const { return m_active; }

protected:
    QPaintDevice() noexcept = default;
    virtual int metric(PaintDeviceMetric metric) const;
    // A device that paints on another one returns that one here; offset is where it starts.
    virtual QPaintDevice *redirected(QPoint *offset) const;
    // A device that paints only in part of the device it is redirected to returns true and that part, in the
    // coordinates of that device.
    virtual bool systemClip(QRegion *region) const;

private:
    Q_DISABLE_COPY(QPaintDevice)
    friend class QPainter;
    mutable bool m_active = false;
};
