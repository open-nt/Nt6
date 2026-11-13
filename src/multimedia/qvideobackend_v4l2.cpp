// MIT License - see LICENSE
//
// The camera backend on Linux: Video4Linux2. Each /dev/videoN that can capture and stream is a
// camera; its formats, frame sizes and frame rates come from the VIDIOC_ENUM_* ioctls. A stream maps
// four buffers and waits for them in poll() on a thread of its own, together with a pipe that
// wakes it to stop.

#include "qvideobackend_p.h"

#include <QtCore/qdir.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace QtMultimediaPrivate {

namespace {

using PF = QVideoFrameFormat::PixelFormat;

PF pixelFormatOf(quint32 fourcc) {
    switch (fourcc) {
    case V4L2_PIX_FMT_YUYV:
        return QVideoFrameFormat::Format_YUYV;
    case V4L2_PIX_FMT_UYVY:
        return QVideoFrameFormat::Format_UYVY;
    case V4L2_PIX_FMT_NV12:
        return QVideoFrameFormat::Format_NV12;
    case V4L2_PIX_FMT_NV21:
        return QVideoFrameFormat::Format_NV21;
    case V4L2_PIX_FMT_YUV420:
        return QVideoFrameFormat::Format_YUV420P;
    case V4L2_PIX_FMT_YVU420:
        return QVideoFrameFormat::Format_YV12;
    case V4L2_PIX_FMT_YUV422P:
        return QVideoFrameFormat::Format_YUV422P;
    case V4L2_PIX_FMT_MJPEG:
    case V4L2_PIX_FMT_JPEG:
        return QVideoFrameFormat::Format_Jpeg;
    case V4L2_PIX_FMT_GREY:
        return QVideoFrameFormat::Format_Y8;
    case V4L2_PIX_FMT_Y16:
        return QVideoFrameFormat::Format_Y16;
    case V4L2_PIX_FMT_XBGR32:
        return QVideoFrameFormat::Format_BGRX8888;
    case V4L2_PIX_FMT_ABGR32:
        return QVideoFrameFormat::Format_BGRA8888;
    default:
        return QVideoFrameFormat::Format_Invalid;
    }
}

quint32 fourccOf(PF format) {
    switch (format) {
    case QVideoFrameFormat::Format_YUYV:
        return V4L2_PIX_FMT_YUYV;
    case QVideoFrameFormat::Format_UYVY:
        return V4L2_PIX_FMT_UYVY;
    case QVideoFrameFormat::Format_NV12:
        return V4L2_PIX_FMT_NV12;
    case QVideoFrameFormat::Format_NV21:
        return V4L2_PIX_FMT_NV21;
    case QVideoFrameFormat::Format_YUV420P:
        return V4L2_PIX_FMT_YUV420;
    case QVideoFrameFormat::Format_YV12:
        return V4L2_PIX_FMT_YVU420;
    case QVideoFrameFormat::Format_YUV422P:
        return V4L2_PIX_FMT_YUV422P;
    case QVideoFrameFormat::Format_Jpeg:
        return V4L2_PIX_FMT_MJPEG;
    case QVideoFrameFormat::Format_Y8:
        return V4L2_PIX_FMT_GREY;
    case QVideoFrameFormat::Format_Y16:
        return V4L2_PIX_FMT_Y16;
    case QVideoFrameFormat::Format_BGRX8888:
        return V4L2_PIX_FMT_XBGR32;
    case QVideoFrameFormat::Format_BGRA8888:
        return V4L2_PIX_FMT_ABGR32;
    default:
        return 0;
    }
}

int xioctl(int fd, unsigned long request, void *arg) {
    int r;
    do {
        r = ioctl(fd, request, arg); // NOLINT(cppcoreguidelines-pro-type-vararg): the ioctl interface
    } while (r < 0 && errno == EINTR);
    return r;
}

// The frame rates of a size, as {min, max}.
std::pair<float, float> frameRates(int fd, quint32 fourcc, quint32 width, quint32 height) {
    v4l2_frmivalenum ival{};
    ival.pixel_format = fourcc;
    ival.width = width;
    ival.height = height;
    float lo = 0.0f;
    float hi = 0.0f;
    for (ival.index = 0; xioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &ival) == 0; ++ival.index) {
        if (ival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
            if (ival.discrete.numerator == 0)
                continue;
            const float rate = float(ival.discrete.denominator) / float(ival.discrete.numerator);
            lo = lo == 0.0f ? rate : std::min(lo, rate);
            hi = std::max(hi, rate);
        } else {
            const auto &s = ival.stepwise;
            if (s.min.numerator && s.max.numerator) {
                hi = float(s.min.denominator) / float(s.min.numerator);
                lo = float(s.max.denominator) / float(s.max.numerator);
            }
            break;
        }
    }
    if (hi == 0.0f)
        lo = hi = 30.0f; // the driver does not say
    return {lo, hi};
}

QList<QCameraFormat> videoFormatsOf(int fd) {
    QList<QCameraFormat> formats;
    v4l2_fmtdesc desc{};
    desc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    for (desc.index = 0; xioctl(fd, VIDIOC_ENUM_FMT, &desc) == 0; ++desc.index) {
        const PF pf = pixelFormatOf(desc.pixelformat);
        if (pf == QVideoFrameFormat::Format_Invalid)
            continue;
        v4l2_frmsizeenum size{};
        size.pixel_format = desc.pixelformat;
        std::vector<std::pair<quint32, quint32>> sizes;
        for (size.index = 0; xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &size) == 0; ++size.index) {
            if (size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                sizes.emplace_back(size.discrete.width, size.discrete.height);
            } else {
                sizes.emplace_back(size.stepwise.min_width, size.stepwise.min_height);
                sizes.emplace_back(size.stepwise.max_width, size.stepwise.max_height);
                break;
            }
        }
        for (const auto &[w, h] : sizes) {
            const auto [lo, hi] = frameRates(fd, desc.pixelformat, w, h);
            const QCameraFormat format = makeCameraFormat(pf, QSize(int(w), int(h)), lo, hi);
            if (!formats.contains(format)) // MJPEG and JPEG both map to Format_Jpeg
                formats.append(format);
        }
    }
    return formats;
}

class V4l2Stream : public CameraStream {
public:
    ~V4l2Stream() override { close(); }

    bool open(const QByteArray &path, const QCameraFormat &format, VideoBackend::FrameHandler frames,
              VideoBackend::ErrorHandler failed, QString *error) {
        auto fail = [&](const char *what) {
            *error =
                QStringLiteral("%1: %2: %3")
                    .arg(QString::fromUtf8(path), QString::fromUtf8(what), QString::fromUtf8(std::strerror(errno)));
            close();
            return false;
        };
        m_fd = ::open(path.constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (m_fd < 0)
            return fail("cannot open");
        v4l2_format fmt{};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = quint32(format.resolution().width());
        fmt.fmt.pix.height = quint32(format.resolution().height());
        fmt.fmt.pix.pixelformat = fourccOf(format.pixelFormat());
        fmt.fmt.pix.field = V4L2_FIELD_ANY;
        if (xioctl(m_fd, VIDIOC_S_FMT, &fmt) < 0)
            return fail("cannot set the format");
        const PF pf = pixelFormatOf(fmt.fmt.pix.pixelformat);
        if (pf == QVideoFrameFormat::Format_Invalid) {
            errno = EINVAL;
            return fail("the camera chose an unsupported format");
        }
        m_format = QVideoFrameFormat(QSize(int(fmt.fmt.pix.width), int(fmt.fmt.pix.height)), pf);
        m_bytesPerLine = pf == QVideoFrameFormat::Format_Jpeg ? 0 : int(fmt.fmt.pix.bytesperline);
        if (format.maxFrameRate() > 0.0f) {
            v4l2_streamparm parm{};
            parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            parm.parm.capture.timeperframe.numerator = 1000;
            parm.parm.capture.timeperframe.denominator = quint32(format.maxFrameRate() * 1000.0f);
            xioctl(m_fd, VIDIOC_S_PARM, &parm); // a camera that cannot set it keeps its own rate
            m_format.setStreamFrameRate(qreal(format.maxFrameRate()));
        }

        v4l2_requestbuffers req{};
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (xioctl(m_fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 2)
            return fail("cannot allocate buffers");
        for (quint32 i = 0; i < req.count; ++i) {
            v4l2_buffer buf{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            if (xioctl(m_fd, VIDIOC_QUERYBUF, &buf) < 0)
                return fail("cannot query a buffer");
            void *mem = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, buf.m.offset);
            if (mem == MAP_FAILED) // NOLINT(performance-no-int-to-ptr): MAP_FAILED is ((void *) -1)
                return fail("cannot map a buffer");
            m_buffers.push_back({mem, buf.length});
            if (xioctl(m_fd, VIDIOC_QBUF, &buf) < 0)
                return fail("cannot queue a buffer");
        }
        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(m_fd, VIDIOC_STREAMON, &type) < 0)
            return fail("cannot start streaming");
        m_streaming = true;
        if (pipe2(m_wake, O_CLOEXEC | O_NONBLOCK) < 0)
            return fail("cannot make a pipe");
        m_frames = std::move(frames);
        m_failed = std::move(failed);
        m_thread = std::thread([this] { run(); });
        return true;
    }

private:
    struct Buffer {
        void *start;
        std::size_t length;
    };

    void run() {
        qint64 firstTimestamp = -1;
        for (;;) {
            pollfd fds[2] = {{m_fd, POLLIN, 0}, {m_wake[0], POLLIN, 0}};
            const int n = poll(fds, 2, -1);
            if (n < 0 && errno == EINTR)
                continue;
            if (n < 0 || (fds[1].revents & POLLIN))
                return;
            if (fds[0].revents & (POLLERR | POLLHUP)) {
                m_failed(QStringLiteral("the camera was disconnected"));
                return;
            }
            v4l2_buffer buf{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            if (xioctl(m_fd, VIDIOC_DQBUF, &buf) < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    continue;
                m_failed(QStringLiteral("the camera stopped: %1").arg(QString::fromUtf8(std::strerror(errno))));
                return;
            }
            const Buffer &b = m_buffers[buf.index];
            const QByteArray data(static_cast<const char *>(b.start),
                                  qsizetype(std::min<std::size_t>(buf.bytesused, b.length)));
            const qint64 timestamp = qint64(buf.timestamp.tv_sec) * 1000000 + buf.timestamp.tv_usec;
            xioctl(m_fd, VIDIOC_QBUF, &buf);
            if (buf.flags & V4L2_BUF_FLAG_ERROR)
                continue;
            QVideoFrame frame = makeVideoFrame(m_format, data, m_bytesPerLine);
            if (!frame.isValid())
                continue; // a short frame
            if (firstTimestamp < 0)
                firstTimestamp = timestamp;
            frame.setStartTime(timestamp - firstTimestamp);
            m_frames(frame);
        }
    }

    void close() {
        if (m_thread.joinable()) {
            const char c = 0;
            (void)!write(m_wake[1], &c, 1);
            m_thread.join();
        }
        if (m_streaming) {
            v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            xioctl(m_fd, VIDIOC_STREAMOFF, &type);
            m_streaming = false;
        }
        for (const Buffer &b : m_buffers)
            munmap(b.start, b.length);
        m_buffers.clear();
        for (int &fd : m_wake) {
            if (fd >= 0)
                ::close(fd);
            fd = -1;
        }
        if (m_fd >= 0)
            ::close(m_fd);
        m_fd = -1;
    }

    int m_fd = -1;
    int m_wake[2] = {-1, -1};
    bool m_streaming = false;
    int m_bytesPerLine = 0;
    QVideoFrameFormat m_format;
    std::vector<Buffer> m_buffers;
    VideoBackend::FrameHandler m_frames;
    VideoBackend::ErrorHandler m_failed;
    std::thread m_thread;
};

class V4l2Backend : public VideoBackend {
public:
    QList<QCameraDevice> videoInputs() override {
        QList<QCameraDevice> list;
        QStringList nodes = QDir(QStringLiteral("/dev")).entryList({QStringLiteral("video*")}, QDir::System);
        std::sort(nodes.begin(), nodes.end(),
                  [](const QString &a, const QString &b) { return a.mid(5).toInt() < b.mid(5).toInt(); });
        for (const QString &node : nodes) {
            const QByteArray path = QStringLiteral("/dev/%1").arg(node).toUtf8();
            const int fd = ::open(path.constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0)
                continue;
            v4l2_capability cap{};
            if (xioctl(fd, VIDIOC_QUERYCAP, &cap) == 0) {
                const quint32 caps = (cap.capabilities & V4L2_CAP_DEVICE_CAPS) ? cap.device_caps : cap.capabilities;
                if ((caps & V4L2_CAP_VIDEO_CAPTURE) && (caps & V4L2_CAP_STREAMING)) {
                    QCameraDevicePrivate info;
                    info.id = path;
                    info.description = QString::fromUtf8(reinterpret_cast<const char *>(cap.card));
                    info.videoFormats = videoFormatsOf(fd);
                    for (const QCameraFormat &f : std::as_const(info.videoFormats))
                        if (!info.photoResolutions.contains(f.resolution()))
                            info.photoResolutions.append(f.resolution());
                    info.isDefault = list.isEmpty();
                    if (!info.videoFormats.isEmpty())
                        list.append(makeCameraDevice(info));
                }
            }
            ::close(fd);
        }
        return list;
    }

    std::unique_ptr<CameraStream> open(const QCameraDevice &device, const QCameraFormat &format, FrameHandler frames,
                                       ErrorHandler failed, QString *error) override {
        auto stream = std::make_unique<V4l2Stream>();
        if (!stream->open(device.id(), format, std::move(frames), std::move(failed), error))
            return nullptr;
        return stream;
    }

    QString deviceDirectory() const override { return QStringLiteral("/dev"); }
};

} // namespace

std::unique_ptr<VideoBackend> createPlatformVideoBackend() { return std::make_unique<V4l2Backend>(); }

} // namespace QtMultimediaPrivate
