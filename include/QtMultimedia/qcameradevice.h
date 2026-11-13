// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtMultimedia/qvideoframeformat.h>

class QCameraFormatPrivate;
class QCameraDevicePrivate;

class QCameraFormat {
public:
    QCameraFormat() noexcept;
    QCameraFormat(const QCameraFormat &other) noexcept;
    QCameraFormat &operator=(const QCameraFormat &other) noexcept;
    ~QCameraFormat();

    QVideoFrameFormat::PixelFormat pixelFormat() const noexcept;
    QSize resolution() const noexcept;
    float minFrameRate() const noexcept;
    float maxFrameRate() const noexcept;

    bool isNull() const noexcept { return !d; }

    bool operator==(const QCameraFormat &other) const;
    bool operator!=(const QCameraFormat &other) const { return !(*this == other); }

    explicit QCameraFormat(QCameraFormatPrivate *data);

private:
    QExplicitlySharedDataPointer<QCameraFormatPrivate> d;
};

class QCameraDevice {
public:
    enum Position { UnspecifiedPosition, BackFace, FrontFace };

    QCameraDevice();
    QCameraDevice(const QCameraDevice &other);
    QCameraDevice &operator=(const QCameraDevice &other);
    ~QCameraDevice();

    bool operator==(const QCameraDevice &other) const;
    bool operator!=(const QCameraDevice &other) const { return !(*this == other); }

    bool isNull() const;

    QByteArray id() const;
    QString description() const;
    bool isDefault() const;
    Position position() const;

    QList<QSize> photoResolutions() const;
    QList<QCameraFormat> videoFormats() const;

    explicit QCameraDevice(QCameraDevicePrivate *data);

private:
    QExplicitlySharedDataPointer<QCameraDevicePrivate> d;
};
