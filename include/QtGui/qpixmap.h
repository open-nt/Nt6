// MIT License - see LICENSE
#pragma once

#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qcolor.h>
#include <QtGui/qimage.h>
#include <QtGui/qpaintdevice.h>

class QPixmap : public QPaintDevice {
public:
    QPixmap();
    QPixmap(int width, int height);
    explicit QPixmap(const QSize &size);
    QPixmap(const QPixmap &other);
    QPixmap(QPixmap &&other) noexcept;
    ~QPixmap() override;
    QPixmap &operator=(const QPixmap &other);
    QPixmap &operator=(QPixmap &&other) noexcept;
    void swap(QPixmap &other) noexcept { m_image.swap(other.m_image); }

    bool isNull() const { return m_image.isNull(); }
    QSize size() const { return m_image.size(); }
    QRect rect() const { return m_image.rect(); }
    bool hasAlphaChannel() const { return m_image.hasAlphaChannel(); }
    void fill(const QColor &color = Qt::white);
    QPixmap copy(const QRect &rect = QRect()) const;
    QImage toImage() const { return m_image; }
    static QPixmap fromImage(const QImage &image);

    QPaintEngine *paintEngine() const override;

protected:
    int metric(PaintDeviceMetric metric) const override;
    QPaintDevice *redirected(QPoint *offset) const override;

private:
    mutable QImage m_image;
};
