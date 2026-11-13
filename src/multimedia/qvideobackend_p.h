// MIT License - see LICENSE
//
// The video part of Nt Multimedia: how a QVideoFrame keeps its pixels, the data behind QCameraDevice
// and QCameraFormat, and the platform part that lists cameras and streams frames from them.
//
// A VideoBackend lists the cameras and opens one; qvideobackend_v4l2.cpp does this with Video4Linux2,
// and qvideobackend_null.cpp has no cameras. An open CameraStream calls its frame handler on a thread
// of its own until it is destroyed. The tests replace the backend with setVideoBackend().
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>
#include <QtMultimedia/qcameradevice.h>
#include <QtMultimedia/qvideoframe.h>

#include <array>
#include <functional>
#include <memory>

class QVideoFramePrivate : public QSharedData {
public:
    QVideoFrameFormat format;
    QByteArray data;
    int planes = 0;
    std::array<int, 3> offset{};
    std::array<int, 3> stride{};
    std::array<int, 3> size{};
    QVideoFrame::MapMode mapMode = QVideoFrame::NotMapped;
    qint64 startTime = -1;
    qint64 endTime = -1;
    QtVideo::Rotation rotation = QtVideo::Rotation::None;
    bool mirrored = false;

    // Lays out the planes of format with bytesPerLine for the first plane (0 for the natural one),
    // sizing data to fit unless keepSize. Returns false for formats that have no CPU layout.
    bool layOut(int bytesPerLine = 0, bool keepSize = false);
};

class QCameraFormatPrivate : public QSharedData {
public:
    QVideoFrameFormat::PixelFormat pixelFormat = QVideoFrameFormat::Format_Invalid;
    QSize resolution;
    float minFrameRate = 0.0f;
    float maxFrameRate = 0.0f;
};

class QCameraDevicePrivate : public QSharedData {
public:
    QByteArray id;
    QString description;
    bool isDefault = false;
    QCameraDevice::Position position = QCameraDevice::UnspecifiedPosition;
    QList<QSize> photoResolutions;
    QList<QCameraFormat> videoFormats;
};

namespace QtMultimediaPrivate {

// Makes a frame of format from data whose first plane has bytesPerLine bytes a line (0 for packed
// lines). Returns an invalid frame if data is too small.
QVideoFrame makeVideoFrame(const QVideoFrameFormat &format, const QByteArray &data, int bytesPerLine = 0);

QCameraFormat makeCameraFormat(QVideoFrameFormat::PixelFormat format, const QSize &resolution, float minRate,
                               float maxRate);
QCameraDevice makeCameraDevice(const QCameraDevicePrivate &info);

class CameraStream {
public:
    CameraStream() = default;
    virtual ~CameraStream() = default; // stops the stream
    CameraStream(const CameraStream &) = delete;
    CameraStream &operator=(const CameraStream &) = delete;
};

class VideoBackend {
public:
    using FrameHandler = std::function<void(const QVideoFrame &)>;
    using ErrorHandler = std::function<void(const QString &)>;

    VideoBackend() = default;
    virtual ~VideoBackend() = default;
    VideoBackend(const VideoBackend &) = delete;
    VideoBackend &operator=(const VideoBackend &) = delete;

    // The cameras, the default one first.
    virtual QList<QCameraDevice> videoInputs() = 0;
    // Starts streaming from device in format. frames and failed are called on the stream's own
    // thread, failed at most once, after which no more frames come. Returns nullptr and sets error
    // if the camera cannot be started.
    virtual std::unique_ptr<CameraStream> open(const QCameraDevice &device, const QCameraFormat &format,
                                               FrameHandler frames, ErrorHandler failed, QString *error) = 0;
    // A directory whose entries change when cameras come and go, or an empty string.
    virtual QString deviceDirectory() const { return QString(); }
};

VideoBackend *videoBackend();
void setVideoBackend(std::unique_ptr<VideoBackend> backend);
std::unique_ptr<VideoBackend> createPlatformVideoBackend();

} // namespace QtMultimediaPrivate
