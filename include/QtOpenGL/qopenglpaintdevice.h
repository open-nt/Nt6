// MIT License - see LICENSE
#pragma once

#include <QtCore/qsize.h>
#include <QtGui/qpaintdevice.h>

#include <memory>

class QOpenGLContext;

class QOpenGLPaintDevice : public QPaintDevice {
public:
    QOpenGLPaintDevice();
    explicit QOpenGLPaintDevice(const QSize &size);
    QOpenGLPaintDevice(int width, int height);
    ~QOpenGLPaintDevice() override;

    QPaintEngine *paintEngine() const override;

    QOpenGLContext *context() const;
    QSize size() const;
    void setSize(const QSize &size);
    void setDevicePixelRatio(qreal devicePixelRatio);

    qreal dotsPerMeterX() const;
    qreal dotsPerMeterY() const;
    void setDotsPerMeterX(qreal dpmx);
    void setDotsPerMeterY(qreal dpmy);

    void setPaintFlipped(bool flipped);
    bool paintFlipped() const;

    virtual void ensureActiveTarget();

protected:
    int metric(QPaintDevice::PaintDeviceMetric metric) const override;

private:
    Q_DISABLE_COPY(QOpenGLPaintDevice)
    struct Data;
    std::unique_ptr<Data> m_d;
};
