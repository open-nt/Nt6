// MIT License - see LICENSE
#pragma once

#include <QtCore/qshareddata.h>
#include <QtCore/qsize.h>
#include <QtGui/qimage.h>
#include <QtMultimedia/qvideoframeformat.h>

class QVideoFramePrivate;

namespace QtVideo {
enum class Rotation { None = 0, Clockwise90 = 90, Clockwise180 = 180, Clockwise270 = 270 };
} // namespace QtVideo

class QVideoFrame {
public:
    enum HandleType { NoHandle, RhiTextureHandle };

    enum MapMode { NotMapped = 0x00, ReadOnly = 0x01, WriteOnly = 0x02, ReadWrite = ReadOnly | WriteOnly };

    QVideoFrame() noexcept;
    explicit QVideoFrame(const QVideoFrameFormat &format);
    explicit QVideoFrame(const QImage &image);
    QVideoFrame(const QVideoFrame &other) noexcept;
    QVideoFrame(QVideoFrame &&other) noexcept;
    ~QVideoFrame();
    QVideoFrame &operator=(const QVideoFrame &other);
    QVideoFrame &operator=(QVideoFrame &&other) noexcept;
    void swap(QVideoFrame &other) noexcept { d.swap(other.d); }

    bool operator==(const QVideoFrame &other) const { return d == other.d; }
    bool operator!=(const QVideoFrame &other) const { return d != other.d; }

    bool isValid() const;

    QVideoFrameFormat::PixelFormat pixelFormat() const;
    QVideoFrameFormat surfaceFormat() const;
    HandleType handleType() const { return NoHandle; }

    QSize size() const;
    int width() const;
    int height() const;

    bool isMapped() const;
    bool isReadable() const;
    bool isWritable() const;
    MapMode mapMode() const;

    bool map(MapMode mode);
    void unmap();

    int bytesPerLine(int plane) const;
    uchar *bits(int plane);
    const uchar *bits(int plane) const;
    int mappedBytes(int plane) const;
    int planeCount() const;

    qint64 startTime() const;
    void setStartTime(qint64 time);
    qint64 endTime() const;
    void setEndTime(qint64 time);

    void setRotation(QtVideo::Rotation angle);
    QtVideo::Rotation rotation() const;
    void setMirrored(bool mirrored);
    bool mirrored() const;

    QImage toImage() const;

    explicit QVideoFrame(QVideoFramePrivate *data);

private:
    QExplicitlySharedDataPointer<QVideoFramePrivate> d;
};
