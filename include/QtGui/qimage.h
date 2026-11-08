// MIT License - see LICENSE
#pragma once

#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtGui/qcolor.h>
#include <QtGui/qpaintdevice.h>
#include <QtGui/qrgb.h>

#include <memory>

class QImage : public QPaintDevice {
public:
    enum Format { Format_Invalid = 0, Format_RGB32 = 4, Format_ARGB32 = 5, Format_ARGB32_Premultiplied = 6 };

    QImage() noexcept;
    QImage(const QSize &size, Format format);
    QImage(int width, int height, Format format);
    QImage(uchar *data, int width, int height, Format format);
    QImage(uchar *data, int width, int height, qsizetype bytesPerLine, Format format);
    QImage(const uchar *data, int width, int height, Format format);
    QImage(const uchar *data, int width, int height, qsizetype bytesPerLine, Format format);
    QImage(const QImage &other);
    QImage(QImage &&other) noexcept;
    ~QImage() override;
    QImage &operator=(const QImage &other);
    QImage &operator=(QImage &&other) noexcept;
    void swap(QImage &other) noexcept;

    bool isNull() const noexcept;
    Format format() const noexcept;
    QSize size() const noexcept { return QSize(width(), height()); }
    QRect rect() const noexcept { return QRect(0, 0, width(), height()); }
    qsizetype bytesPerLine() const noexcept;
    qsizetype sizeInBytes() const noexcept;
    bool hasAlphaChannel() const noexcept;
    bool valid(int x, int y) const noexcept { return x >= 0 && y >= 0 && x < width() && y < height(); }
    bool valid(const QPoint &pt) const noexcept { return valid(pt.x(), pt.y()); }

    uchar *bits();
    const uchar *bits() const { return constBits(); }
    const uchar *constBits() const noexcept;
    uchar *scanLine(int y);
    const uchar *scanLine(int y) const { return constScanLine(y); }
    const uchar *constScanLine(int y) const noexcept;

    void fill(uint pixel);
    void fill(const QColor &color);
    void fill(Qt::GlobalColor color);
    QRgb pixel(int x, int y) const;
    QRgb pixel(const QPoint &pt) const { return pixel(pt.x(), pt.y()); }
    void setPixel(int x, int y, uint index_or_rgb);
    void setPixel(const QPoint &pt, uint index_or_rgb) { setPixel(pt.x(), pt.y(), index_or_rgb); }
    QColor pixelColor(int x, int y) const;
    void setPixelColor(int x, int y, const QColor &color);

    QImage copy(const QRect &rect = QRect()) const;
    QImage copy(int x, int y, int w, int h) const { return copy(QRect(x, y, w, h)); }
    QImage convertToFormat(Format format) const;
    void detach();
    void setDevicePixelRatio(qreal ratio);
    bool isDetached() const noexcept;

    QPaintEngine *paintEngine() const override;

    bool operator==(const QImage &other) const;
    bool operator!=(const QImage &other) const { return !(*this == other); }

protected:
    int metric(PaintDeviceMetric metric) const override;

private:
    struct Data;
    std::shared_ptr<Data> d;
    mutable std::unique_ptr<QPaintEngine> m_engine;
};
