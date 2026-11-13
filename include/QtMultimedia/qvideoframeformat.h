// MIT License - see LICENSE
#pragma once

#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qimage.h>

class QVideoFrameFormat {
public:
    enum PixelFormat {
        Format_Invalid,
        Format_ARGB8888,
        Format_ARGB8888_Premultiplied,
        Format_XRGB8888,
        Format_BGRA8888,
        Format_BGRA8888_Premultiplied,
        Format_BGRX8888,
        Format_ABGR8888,
        Format_XBGR8888,
        Format_RGBA8888,
        Format_RGBX8888,
        Format_AYUV,
        Format_AYUV_Premultiplied,
        Format_YUV420P,
        Format_YUV422P,
        Format_YV12,
        Format_UYVY,
        Format_YUYV,
        Format_NV12,
        Format_NV21,
        Format_IMC1,
        Format_IMC2,
        Format_IMC3,
        Format_IMC4,
        Format_Y8,
        Format_Y16,
        Format_P010,
        Format_P016,
        Format_SamplerExternalOES,
        Format_Jpeg,
        Format_SamplerRect,
        Format_YUV420P10
    };
    static constexpr int NPixelFormats = Format_YUV420P10 + 1;

    enum Direction { TopToBottom, BottomToTop };

    enum YCbCrColorSpace { YCbCr_Undefined = 0, YCbCr_BT601, YCbCr_BT709, YCbCr_xvYCC601, YCbCr_xvYCC709, YCbCr_JPEG };

    enum ColorSpace {
        ColorSpace_Undefined = 0,
        ColorSpace_BT601 = 1,
        ColorSpace_BT709 = 2,
        ColorSpace_AdobeRgb = 5,
        ColorSpace_BT2020 = 6
    };

    enum ColorRange { ColorRange_Unknown, ColorRange_Video, ColorRange_Full };

    QVideoFrameFormat() = default;
    QVideoFrameFormat(const QSize &size, PixelFormat pixelFormat);

    bool isValid() const;

    PixelFormat pixelFormat() const { return m_pixelFormat; }

    QSize frameSize() const { return m_size; }
    void setFrameSize(const QSize &size);
    void setFrameSize(int width, int height) { setFrameSize(QSize(width, height)); }
    int frameWidth() const { return m_size.width(); }
    int frameHeight() const { return m_size.height(); }

    int planeCount() const;

    QRect viewport() const { return m_viewport; }
    void setViewport(const QRect &viewport) { m_viewport = viewport; }

    Direction scanLineDirection() const { return m_direction; }
    void setScanLineDirection(Direction direction) { m_direction = direction; }

    qreal streamFrameRate() const { return m_frameRate; }
    void setStreamFrameRate(qreal rate) { m_frameRate = rate; }

    ColorSpace colorSpace() const { return m_colorSpace; }
    void setColorSpace(ColorSpace colorSpace) { m_colorSpace = colorSpace; }

    ColorRange colorRange() const { return m_colorRange; }
    void setColorRange(ColorRange range) { m_colorRange = range; }

    bool isMirrored() const { return m_mirrored; }
    void setMirrored(bool mirrored) { m_mirrored = mirrored; }

    static PixelFormat pixelFormatFromImageFormat(QImage::Format format);
    static QImage::Format imageFormatFromPixelFormat(PixelFormat format);
    static QString pixelFormatToString(PixelFormat pixelFormat);

    bool operator==(const QVideoFrameFormat &other) const;
    bool operator!=(const QVideoFrameFormat &other) const { return !(*this == other); }

private:
    PixelFormat m_pixelFormat = Format_Invalid;
    QSize m_size;
    QRect m_viewport;
    Direction m_direction = TopToBottom;
    qreal m_frameRate = 0.0;
    ColorSpace m_colorSpace = ColorSpace_Undefined;
    ColorRange m_colorRange = ColorRange_Unknown;
    bool m_mirrored = false;
};
