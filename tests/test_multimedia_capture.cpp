// MIT License - see LICENSE
//
// The capture and video side of Nt Multimedia, against fake backends: a camera that streams frames
// of a known picture from a thread of its own, and a microphone that records a counting pattern.
// Video frames and their conversion to images, the camera, the capture session, image capture, the
// recorder, the decoder, audio buffers and media formats are covered.
#include <QtCore>
#include <QtMultimedia>

#include "qaudiobackend_p.h"
#include "qvideobackend_p.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static void spinEvents(int msecs) {
    QEventLoop loop;
    QTimer::singleShot(msecs, [&] { loop.quit(); });
    loop.exec();
}

template <typename F> static bool waitFor(F cond, int timeoutMs = 5000) {
    QElapsedTimer timer;
    timer.start();
    while (!cond() && timer.elapsed() < timeoutMs)
        spinEvents(5);
    return cond();
}

using namespace QtMultimediaPrivate;
using PF = QVideoFrameFormat::PixelFormat;

namespace {

struct World {
    QString directory;
    std::atomic<int> opens{0};
    std::atomic<int> failAfterFrames{-1};
    bool failOpen = false;
    bool secondCamera = false;
};
World *g_world = nullptr;

// A YUYV, NV12 or 32-bit frame of size, all one color: Y=81 U=90 V=240 (red in BT.601 video range)
// or, for 32-bit frames, red.
QVideoFrame redFrame(const QVideoFrameFormat &format) {
    QVideoFrame frame(format);
    frame.map(QVideoFrame::WriteOnly);
    const int w = format.frameWidth();
    const int h = format.frameHeight();
    switch (format.pixelFormat()) {
    case QVideoFrameFormat::Format_YUYV:
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; x += 2) {
                uchar *p = frame.bits(0) + y * frame.bytesPerLine(0) + x * 2;
                p[0] = 81, p[1] = 90, p[2] = 81, p[3] = 240;
            }
        break;
    case QVideoFrameFormat::Format_NV12:
        std::memset(frame.bits(0), 81, std::size_t(frame.mappedBytes(0)));
        for (int i = 0; i < frame.mappedBytes(1); i += 2)
            frame.bits(1)[i] = 90, frame.bits(1)[i + 1] = 240;
        break;
    default:
        break;
    }
    frame.unmap();
    return frame;
}

class FakeCamera : public CameraStream {
public:
    FakeCamera(const QCameraFormat &format, VideoBackend::FrameHandler frames, VideoBackend::ErrorHandler failed)
        : m_format(format.resolution(), format.pixelFormat()) {
        m_thread = std::thread([this, frames = std::move(frames), failed = std::move(failed)] {
            const QVideoFrame frame = redFrame(m_format);
            int sent = 0;
            while (!m_stop.load()) {
                if (g_world->failAfterFrames.load() >= 0 && sent >= g_world->failAfterFrames.load()) {
                    failed(QStringLiteral("unplugged"));
                    return;
                }
                QVideoFrame copy = frame;
                frames(copy);
                ++sent;
                std::this_thread::sleep_for(std::chrono::milliseconds(4));
            }
        });
    }
    ~FakeCamera() override {
        m_stop.store(true);
        m_thread.join();
    }

private:
    QVideoFrameFormat m_format;
    std::atomic<bool> m_stop{false};
    std::thread m_thread;
};

class FakeVideoBackend : public VideoBackend {
public:
    QList<QCameraDevice> videoInputs() override {
        QList<QCameraDevice> list;
        QCameraDevicePrivate info;
        info.id = "fake-cam";
        info.description = QStringLiteral("Fake camera");
        info.isDefault = true;
        info.position = QCameraDevice::FrontFace;
        info.videoFormats = {makeCameraFormat(QVideoFrameFormat::Format_YUYV, QSize(64, 48), 5, 30),
                             makeCameraFormat(QVideoFrameFormat::Format_NV12, QSize(32, 24), 5, 30),
                             makeCameraFormat(QVideoFrameFormat::Format_Jpeg, QSize(80, 60), 5, 5)};
        info.photoResolutions = {QSize(64, 48)};
        list.append(makeCameraDevice(info));
        if (g_world->secondCamera) {
            info.id = "fake-cam-2";
            info.isDefault = false;
            info.position = QCameraDevice::BackFace;
            list.append(makeCameraDevice(info));
        }
        return list;
    }

    std::unique_ptr<CameraStream> open(const QCameraDevice &, const QCameraFormat &format, FrameHandler frames,
                                       ErrorHandler failed, QString *error) override {
        if (g_world->failOpen) {
            *error = QStringLiteral("busy");
            return nullptr;
        }
        ++g_world->opens;
        return std::make_unique<FakeCamera>(format, std::move(frames), std::move(failed));
    }

    QString deviceDirectory() const override { return g_world->directory; }
};

class FakeMic : public AudioPcm {
public:
    qint64 write(const char *, qint64) override { return -1; }
    qint64 read(char *data, qint64 bytes) override {
        for (qint64 i = 0; i < bytes; ++i)
            data[i] = char(m_counter++ & 0x7f);
        // In real time (8000 Hz, 2 channels, 16 bits), so the source's buffer never overflows.
        std::this_thread::sleep_for(std::chrono::microseconds(bytes * 1000000 / 32000));
        return bytes;
    }

private:
    quint32 m_counter = 0;
};

class FakeAudioBackend : public AudioBackend {
public:
    QList<QAudioDevice> devices(QAudioDevice::Mode mode) override {
        if (mode != QAudioDevice::Input)
            return {};
        QAudioDevicePrivate info;
        info.id = "fake-mic";
        info.description = QStringLiteral("Fake microphone");
        info.mode = mode;
        info.isDefault = true;
        info.preferredFormat.setSampleRate(8000);
        info.preferredFormat.setChannelCount(2);
        info.preferredFormat.setSampleFormat(QAudioFormat::Int16);
        return {makeAudioDevice(info)};
    }
    std::unique_ptr<AudioPcm> open(const QByteArray &, QAudioDevice::Mode, const QAudioFormat &, qint64,
                                   QString *) override {
        return std::make_unique<FakeMic>();
    }
};

bool near(QRgb a, int r, int g, int b, int tolerance = 3) {
    const bool ok = std::abs(qRed(a) - r) <= tolerance && std::abs(qGreen(a) - g) <= tolerance &&
                    std::abs(qBlue(a) - b) <= tolerance;
    if (!ok)
        std::printf("  pixel is %d,%d,%d, expected %d,%d,%d\n", qRed(a), qGreen(a), qBlue(a), r, g, b);
    return ok;
}

QRgb pixel(const QImage &image, int x, int y) { return reinterpret_cast<const QRgb *>(image.constScanLine(y))[x]; }

void put16(QByteArray &b, quint16 v) { b.append(char(v & 0xff)).append(char(v >> 8)); }
void put32(QByteArray &b, quint32 v) {
    put16(b, quint16(v & 0xffff));
    put16(b, quint16(v >> 16));
}

QByteArray wav16(int channels, int rate, const QByteArray &samples) {
    QByteArray b("RIFF");
    put32(b, quint32(36 + samples.size()));
    b.append("WAVEfmt ");
    put32(b, 16);
    put16(b, 1);
    put16(b, quint16(channels));
    put32(b, quint32(rate));
    put32(b, quint32(rate * channels * 2));
    put16(b, quint16(channels * 2));
    put16(b, 16);
    b.append("data");
    put32(b, quint32(samples.size()));
    b.append(samples);
    return b;
}

void testFrameFormat() {
    QVideoFrameFormat invalid;
    CHECK(!invalid.isValid());
    QVideoFrameFormat format(QSize(64, 48), QVideoFrameFormat::Format_NV12);
    CHECK(format.isValid());
    CHECK(format.planeCount() == 2);
    CHECK(format.frameWidth() == 64 && format.frameHeight() == 48);
    CHECK(format.viewport() == QRect(0, 0, 64, 48));
    CHECK(QVideoFrameFormat(QSize(4, 4), QVideoFrameFormat::Format_YUV420P).planeCount() == 3);
    CHECK(QVideoFrameFormat(QSize(4, 4), QVideoFrameFormat::Format_YUYV).planeCount() == 1);
    CHECK(QVideoFrameFormat::pixelFormatToString(QVideoFrameFormat::Format_YUYV) == QStringLiteral("YUYV"));
    CHECK(QVideoFrameFormat::pixelFormatToString(QVideoFrameFormat::Format_YUV420P10) == QStringLiteral("YUV420P10"));
    const PF rgb32 = QVideoFrameFormat::pixelFormatFromImageFormat(QImage::Format_RGB32);
    CHECK(rgb32 == (QSysInfo::ByteOrder == QSysInfo::LittleEndian ? QVideoFrameFormat::Format_BGRX8888
                                                                  : QVideoFrameFormat::Format_XRGB8888));
    CHECK(QVideoFrameFormat::imageFormatFromPixelFormat(rgb32) == QImage::Format_RGB32);
    CHECK(QVideoFrameFormat::imageFormatFromPixelFormat(QVideoFrameFormat::Format_NV12) == QImage::Format_Invalid);
    QVideoFrameFormat copy = format;
    CHECK(copy == format);
    copy.setStreamFrameRate(30);
    CHECK(copy != format);
    copy.setFrameSize(32, 16);
    CHECK(copy.viewport() == QRect(0, 0, 32, 16));
}

void testFrames() {
    QVideoFrame invalid;
    CHECK(!invalid.isValid());
    CHECK(invalid.toImage().isNull());
    CHECK(!invalid.map(QVideoFrame::ReadOnly));

    // Every YUV layout converts the same color to the same pixels.
    for (PF pf : {QVideoFrameFormat::Format_YUYV, QVideoFrameFormat::Format_NV12}) {
        const QImage image = redFrame(QVideoFrameFormat(QSize(6, 4), pf)).toImage();
        CHECK(image.format() == QImage::Format_RGB32);
        CHECK(image.size() == QSize(6, 4));
        CHECK(near(pixel(image, 5, 3), 255, 0, 0, 4));
    }
    {
        QVideoFrame planar(QVideoFrameFormat(QSize(4, 2), QVideoFrameFormat::Format_YUV420P));
        CHECK(planar.map(QVideoFrame::WriteOnly));
        CHECK(planar.planeCount() == 3);
        CHECK(planar.mappedBytes(0) == 8 && planar.mappedBytes(1) == 2 && planar.mappedBytes(2) == 2);
        std::memset(planar.bits(0), 145, 8); // green: Y=145 U=54 V=34
        std::memset(planar.bits(1), 54, 2);
        std::memset(planar.bits(2), 34, 2);
        planar.unmap();
        CHECK(near(pixel(planar.toImage(), 3, 1), 0, 255, 0, 5));
        // YV12 swaps the chroma planes.
        QVideoFrame yv12(QVideoFrameFormat(QSize(4, 2), QVideoFrameFormat::Format_YV12));
        yv12.map(QVideoFrame::WriteOnly);
        std::memset(yv12.bits(0), 145, 8);
        std::memset(yv12.bits(1), 34, 2);
        std::memset(yv12.bits(2), 54, 2);
        yv12.unmap();
        CHECK(near(pixel(yv12.toImage(), 0, 0), 0, 255, 0, 5));
    }
    {
        QVideoFrame grey(QVideoFrameFormat(QSize(2, 2), QVideoFrameFormat::Format_Y8));
        grey.map(QVideoFrame::WriteOnly);
        std::memset(grey.bits(0), 200, 4);
        grey.unmap();
        CHECK(near(pixel(grey.toImage(), 1, 1), 200, 200, 200, 0));
    }
    {
        // Byte orders of the 32-bit formats.
        QVideoFrame rgba(QVideoFrameFormat(QSize(1, 1), QVideoFrameFormat::Format_RGBA8888));
        rgba.map(QVideoFrame::WriteOnly);
        const uchar bytes[4] = {10, 20, 30, 128};
        std::memcpy(rgba.bits(0), bytes, 4);
        rgba.unmap();
        const QImage image = rgba.toImage();
        CHECK(image.format() == QImage::Format_ARGB32);
        CHECK(near(pixel(image, 0, 0), 10, 20, 30, 0));
        CHECK(qAlpha(pixel(image, 0, 0)) == 128);
        QVideoFrame abgr(QVideoFrameFormat(QSize(1, 1), QVideoFrameFormat::Format_XBGR8888));
        abgr.map(QVideoFrame::WriteOnly);
        std::memcpy(abgr.bits(0), bytes, 4);
        abgr.unmap();
        CHECK(near(pixel(abgr.toImage(), 0, 0), 128, 30, 20, 0));
    }

    // From an image and back, mirrored and rotated.
    QImage image(2, 1, QImage::Format_RGB32);
    reinterpret_cast<QRgb *>(image.scanLine(0))[0] = qRgb(255, 0, 0);
    reinterpret_cast<QRgb *>(image.scanLine(0))[1] = qRgb(0, 0, 255);
    QVideoFrame frame(image);
    CHECK(frame.isValid());
    CHECK(frame.pixelFormat() == QVideoFrameFormat::pixelFormatFromImageFormat(QImage::Format_RGB32));
    CHECK(frame.size() == QSize(2, 1));
    QImage back = frame.toImage();
    CHECK(near(pixel(back, 0, 0), 255, 0, 0, 0) && near(pixel(back, 1, 0), 0, 0, 255, 0));
    frame.setMirrored(true);
    CHECK(frame.mirrored());
    back = frame.toImage();
    CHECK(near(pixel(back, 0, 0), 0, 0, 255, 0));
    frame.setMirrored(false);
    frame.setRotation(QtVideo::Rotation::Clockwise90);
    back = frame.toImage();
    CHECK(back.size() == QSize(1, 2));
    CHECK(near(pixel(back, 0, 0), 255, 0, 0, 0) && near(pixel(back, 0, 1), 0, 0, 255, 0));
    frame.setRotation(QtVideo::Rotation::Clockwise180);
    back = frame.toImage();
    CHECK(near(pixel(back, 0, 0), 0, 0, 255, 0));

    // Mapping: copies share pixels until one is mapped for writing.
    QVideoFrame a(QVideoFrameFormat(QSize(2, 2), QVideoFrameFormat::Format_Y8));
    QVideoFrame b = a;
    CHECK(a == b);
    CHECK(b.bits(0) == nullptr); // not mapped
    CHECK(b.map(QVideoFrame::WriteOnly));
    CHECK(a != b);
    CHECK(b.isWritable() && !b.isReadable() && b.isMapped());
    CHECK(b.mapMode() == QVideoFrame::WriteOnly);
    b.bits(0)[0] = 99;
    CHECK(!b.map(QVideoFrame::ReadOnly)); // already mapped in another mode
    b.unmap();
    a.map(QVideoFrame::ReadOnly);
    CHECK(std::as_const(a).bits(0)[0] == 0);
    CHECK(a.bits(0) == nullptr); // read-only frames give no writable bits
    CHECK(a.bytesPerLine(0) == 2);
    a.unmap();
    a.setStartTime(1000);
    a.setEndTime(2000);
    CHECK(a.startTime() == 1000 && a.endTime() == 2000);
    CHECK(a.handleType() == QVideoFrame::NoHandle);
    CHECK(!QVideoFrame(QVideoFrameFormat(QSize(4, 4), QVideoFrameFormat::Format_IMC1)).isValid());

    // A sink holds the last frame.
    QVideoSink sink;
    int frames = 0;
    int sizeChanges = 0;
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, [&](const QVideoFrame &) { ++frames; });
    QObject::connect(&sink, &QVideoSink::videoSizeChanged, [&] { ++sizeChanges; });
    sink.setVideoFrame(frame);
    sink.setVideoFrame(frame);
    CHECK(frames == 2 && sizeChanges == 1);
    CHECK(sink.videoFrame() == frame);
    CHECK(sink.videoSize() == QSize(2, 1));
    sink.setSubtitleText(QStringLiteral("hello"));
    CHECK(sink.subtitleText() == QStringLiteral("hello"));
}

void testCameraDevices() {
    const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
    CHECK(cameras.size() == 1);
    const QCameraDevice camera = QMediaDevices::defaultVideoInput();
    CHECK(camera.id() == "fake-cam");
    CHECK(camera.description() == QStringLiteral("Fake camera"));
    CHECK(camera.isDefault());
    CHECK(camera.position() == QCameraDevice::FrontFace);
    CHECK(camera.videoFormats().size() == 3);
    CHECK(camera.photoResolutions() == QList<QSize>{QSize(64, 48)});
    const QCameraFormat format = camera.videoFormats().first();
    CHECK(format.pixelFormat() == QVideoFrameFormat::Format_YUYV);
    CHECK(format.resolution() == QSize(64, 48));
    CHECK(format.minFrameRate() == 5.0f && format.maxFrameRate() == 30.0f);
    CHECK(format == camera.videoFormats().first());
    CHECK(format != camera.videoFormats().at(1));
    CHECK(QCameraFormat().isNull() && QCameraDevice().isNull());
    CHECK(QCameraDevice() != camera);

    QMediaDevices devices;
    int changes = 0;
    QObject::connect(&devices, &QMediaDevices::videoInputsChanged, [&] { ++changes; });
    g_world->secondCamera = true;
    QFile node(g_world->directory + QStringLiteral("/video9"));
    CHECK(node.open(QIODevice::WriteOnly));
    node.close();
    CHECK(waitFor([&] { return changes == 1; }));
    QCamera back(QCameraDevice::BackFace);
    CHECK(back.cameraDevice().id() == "fake-cam-2");
    g_world->secondCamera = false;
    node.remove();
    CHECK(waitFor([&] { return changes == 2; }));
}

void testCamera() {
    QMediaCaptureSession session;
    QCamera camera;
    QVideoSink sink;
    CHECK(camera.isAvailable());
    CHECK(!camera.isActive());
    CHECK(camera.cameraFormat().isNull());
    session.setCamera(&camera);
    session.setVideoSink(&sink);
    CHECK(session.camera() == &camera);
    CHECK(camera.captureSession() == &session);
    CHECK(session.videoSink() == &sink && session.videoOutput() == &sink);

    int frames = 0;
    QVideoFrame last;
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, [&](const QVideoFrame &f) {
        ++frames;
        last = f;
    });
    QList<bool> active;
    QObject::connect(&camera, &QCamera::activeChanged, [&](bool a) { active.append(a); });
    camera.start();
    CHECK(camera.isActive());
    CHECK(waitFor([&] { return frames >= 3; }));
    // The default format: YUYV 64x48, larger than NV12 and faster than the JPEG one.
    CHECK(last.pixelFormat() == QVideoFrameFormat::Format_YUYV);
    CHECK(last.size() == QSize(64, 48));
    CHECK(near(pixel(last.toImage(), 10, 10), 255, 0, 0, 4));

    camera.setCameraFormat(camera.cameraDevice().videoFormats().at(1));
    CHECK(camera.isActive());
    CHECK(waitFor([&] { return last.pixelFormat() == QVideoFrameFormat::Format_NV12; }));
    CHECK(last.size() == QSize(32, 24));

    camera.stop();
    CHECK(!camera.isActive());
    spinEvents(20);
    const int stopped = frames;
    spinEvents(50);
    CHECK(frames == stopped);
    CHECK(active == (QList<bool>{true, false}));

    // A camera that fails while streaming stops, with an error.
    QList<QCamera::Error> errors;
    QObject::connect(&camera, &QCamera::errorOccurred, [&](QCamera::Error e, const QString &) { errors.append(e); });
    g_world->failAfterFrames = 2;
    camera.start();
    CHECK(waitFor([&] { return !camera.isActive(); }));
    CHECK(camera.error() == QCamera::CameraError);
    CHECK(camera.errorString() == QStringLiteral("unplugged"));
    CHECK(errors == QList<QCamera::Error>{QCamera::CameraError});
    g_world->failAfterFrames = -1;

    // A camera that cannot be opened stays inactive.
    g_world->failOpen = true;
    camera.start();
    CHECK(!camera.isActive());
    CHECK(camera.errorString() == QStringLiteral("busy"));
    g_world->failOpen = false;
    camera.start();
    CHECK(camera.isActive() && camera.error() == QCamera::NoError);

    // Moving the camera to another session takes it out of the first.
    QMediaCaptureSession other;
    other.setCamera(&camera);
    CHECK(session.camera() == nullptr);
    CHECK(camera.captureSession() == &other);
    camera.stop();

    // A camera with no device.
    QCamera none(QCameraDevice{});
    CHECK(!none.isAvailable());
    none.start();
    CHECK(!none.isActive() && none.error() == QCamera::CameraError);

    // A camera destroyed while in a session and streaming.
    {
        auto *temporary = new QCamera;
        session.setCamera(temporary);
        temporary->start();
        spinEvents(20);
        delete temporary;
        CHECK(session.camera() == nullptr);
    }
    spinEvents(20);
}

void testImageCapture(const QTemporaryDir &dir) {
    QMediaCaptureSession session;
    QCamera camera;
    QImageCapture capture;
    CHECK(!capture.isAvailable());
    CHECK(capture.capture() == -1);
    CHECK(capture.error() == QImageCapture::NotReadyError);
    session.setImageCapture(&capture);
    CHECK(capture.captureSession() == &session);
    session.setCamera(&camera);
    CHECK(capture.isAvailable());
    CHECK(!capture.isReadyForCapture());
    QList<bool> ready;
    QObject::connect(&capture, &QImageCapture::readyForCaptureChanged, [&](bool r) { ready.append(r); });
    camera.start();
    CHECK(capture.isReadyForCapture());
    CHECK(ready == QList<bool>{true});

    QList<int> exposed;
    QImage captured;
    int capturedId = 0;
    QVideoFrame available;
    QObject::connect(&capture, &QImageCapture::imageExposed, [&](int id) { exposed.append(id); });
    QObject::connect(&capture, &QImageCapture::imageCaptured, [&](int id, const QImage &image) {
        capturedId = id;
        captured = image;
    });
    QObject::connect(&capture, &QImageCapture::imageAvailable,
                     [&](int, const QVideoFrame &frame) { available = frame; });
    const int id = capture.capture();
    CHECK(id == 1);
    CHECK(waitFor([&] { return capturedId == id; }));
    CHECK(exposed == QList<int>{1});
    CHECK(captured.size() == QSize(64, 48));
    CHECK(near(pixel(captured, 0, 0), 255, 0, 0, 4));
    CHECK(available.isValid());

    capture.setResolution(32, 24);
    CHECK(capture.resolution() == QSize(32, 24));
    capture.capture();
    CHECK(waitFor([&] { return captured.size() == QSize(32, 24); }));

    // To files: PNG in a folder, then to a name without a suffix.
    QStringList saved;
    QObject::connect(&capture, &QImageCapture::imageSaved, [&](int, const QString &name) { saved.append(name); });
    capture.setFileFormat(QImageCapture::PNG);
    CHECK(capture.fileFormat() == QImageCapture::PNG);
    capture.captureToFile(dir.path());
    capture.captureToFile(dir.filePath(QStringLiteral("shot")));
    CHECK(waitFor([&] { return saved.size() == 2; }));
    CHECK(saved.value(0) == dir.filePath(QStringLiteral("image_0001.png")));
    CHECK(saved.value(1) == dir.filePath(QStringLiteral("shot.png")));
    CHECK(QImage(saved.value(0)).size() == QSize(32, 24));

    QList<QImageCapture::Error> errors;
    QObject::connect(&capture, &QImageCapture::errorOccurred,
                     [&](int, QImageCapture::Error e, const QString &) { errors.append(e); });
    capture.setFileFormat(QImageCapture::WebP);
    CHECK(capture.captureToFile(dir.path()) == -1);
    CHECK(errors == QList<QImageCapture::Error>{QImageCapture::FormatError});
    CHECK(QImageCapture::supportedFormats().contains(QImageCapture::PNG));
    CHECK(QImageCapture::fileFormatName(QImageCapture::JPEG) == QStringLiteral("JPEG"));
    CHECK(!QImageCapture::fileFormatDescription(QImageCapture::Tiff).isEmpty());
    capture.setQuality(QImageCapture::HighQuality);
    CHECK(capture.quality() == QImageCapture::HighQuality);

    camera.stop();
    CHECK(!capture.isReadyForCapture());
    CHECK(ready == (QList<bool>{true, false}));
    session.setImageCapture(nullptr);
    CHECK(capture.captureSession() == nullptr);
}

void testRecorder(const QTemporaryDir &dir) {
    QMediaCaptureSession session;
    QMediaRecorder recorder;
    CHECK(recorder.isAvailable());
    CHECK(recorder.recorderState() == QMediaRecorder::StoppedState);
    session.setRecorder(&recorder);
    CHECK(recorder.captureSession() == &session);

    // Nothing to record yet.
    QList<QMediaRecorder::Error> errors;
    QObject::connect(&recorder, &QMediaRecorder::errorOccurred,
                     [&](QMediaRecorder::Error e, const QString &) { errors.append(e); });
    recorder.record();
    CHECK(recorder.recorderState() == QMediaRecorder::StoppedState);
    CHECK(recorder.error() == QMediaRecorder::ResourceError);

    QAudioInput input;
    CHECK(input.device().id() == "fake-mic");
    session.setAudioInput(&input);
    CHECK(session.audioInput() == &input);

    recorder.setMediaFormat(QMediaFormat(QMediaFormat::MP3));
    recorder.record();
    CHECK(recorder.error() == QMediaRecorder::FormatError);
    recorder.setMediaFormat(QMediaFormat(QMediaFormat::Wave));

    recorder.setOutputLocation(QUrl::fromLocalFile(QStringLiteral("/proc/nt-cannot-write")));
    recorder.record();
    CHECK(recorder.error() == QMediaRecorder::LocationNotWritable);
    CHECK(errors == (QList<QMediaRecorder::Error>{QMediaRecorder::ResourceError, QMediaRecorder::FormatError,
                                                  QMediaRecorder::LocationNotWritable}));

    // Record, pause, go on, stop.
    recorder.setOutputLocation(QUrl::fromLocalFile(dir.filePath(QStringLiteral("take"))));
    QList<QMediaRecorder::RecorderState> states;
    QObject::connect(&recorder, &QMediaRecorder::recorderStateChanged,
                     [&](QMediaRecorder::RecorderState s) { states.append(s); });
    int durationChanges = 0;
    QObject::connect(&recorder, &QMediaRecorder::durationChanged, [&](qint64) { ++durationChanges; });
    recorder.record();
    CHECK(recorder.recorderState() == QMediaRecorder::RecordingState);
    CHECK(recorder.error() == QMediaRecorder::NoError);
    CHECK(recorder.actualLocation() == QUrl::fromLocalFile(dir.filePath(QStringLiteral("take.wav"))));
    CHECK(waitFor([&] { return recorder.duration() >= 250; }));
    recorder.pause();
    CHECK(recorder.recorderState() == QMediaRecorder::PausedState);
    recorder.record();
    CHECK(recorder.recorderState() == QMediaRecorder::RecordingState);
    CHECK(waitFor([&] { return recorder.duration() >= 500; }));
    recorder.stop();
    CHECK(recorder.recorderState() == QMediaRecorder::StoppedState);
    CHECK(states ==
          (QList<QMediaRecorder::RecorderState>{QMediaRecorder::RecordingState, QMediaRecorder::PausedState,
                                                QMediaRecorder::RecordingState, QMediaRecorder::StoppedState}));
    CHECK(durationChanges >= 3);
    const qint64 duration = recorder.duration();
    CHECK(duration >= 500);

    // The file is a WAV file of the counting pattern, as long as the duration says.
    QFile file(dir.filePath(QStringLiteral("take.wav")));
    CHECK(file.open(QIODevice::ReadOnly));
    const QByteArray data = file.readAll();
    CHECK(data.startsWith("RIFF") && data.mid(8, 4) == "WAVE");
    const quint32 dataSize = qFromLittleEndian<quint32>(data.constData() + 40);
    CHECK(qsizetype(dataSize) == data.size() - 44);
    CHECK(dataSize % 4 == 0);
    CHECK(qint64(dataSize) / 4 * 1000 / 8000 == duration);
    CHECK(data.at(44) == 0 && data.at(45) == 1 && data.at(46) == 2);
    bool counting = true;
    for (qsizetype i = 44; counting && i < data.size(); ++i)
        counting = data.at(i) == char((i - 44) & 0x7f);
    CHECK(counting);

    // Into a folder: numbered files.
    recorder.setOutputLocation(QUrl::fromLocalFile(dir.path()));
    recorder.record();
    CHECK(recorder.actualLocation() == QUrl::fromLocalFile(dir.filePath(QStringLiteral("record_0001.wav"))));
    input.setMuted(true);
    spinEvents(100);
    recorder.stop();
    QFile muted(dir.filePath(QStringLiteral("record_0001.wav")));
    CHECK(muted.open(QIODevice::ReadOnly));
    CHECK(muted.size() > 44);

    // The settings are kept.
    recorder.setAudioSampleRate(16000);
    recorder.setAudioChannelCount(1);
    recorder.setQuality(QMediaRecorder::HighQuality);
    recorder.setEncodingMode(QMediaRecorder::ConstantBitRateEncoding);
    recorder.setVideoResolution(640, 480);
    recorder.setVideoFrameRate(25);
    recorder.setVideoBitRate(1000000);
    recorder.setAudioBitRate(128000);
    CHECK(recorder.audioSampleRate() == 16000 && recorder.audioChannelCount() == 1);
    CHECK(recorder.quality() == QMediaRecorder::HighQuality);
    CHECK(recorder.encodingMode() == QMediaRecorder::ConstantBitRateEncoding);
    CHECK(recorder.videoResolution() == QSize(640, 480) && recorder.videoFrameRate() == 25.0);
    CHECK(recorder.videoBitRate() == 1000000 && recorder.audioBitRate() == 128000);
    recorder.setOutputLocation(QUrl::fromLocalFile(dir.filePath(QStringLiteral("mono.wav"))));
    recorder.record();
    spinEvents(50);
    recorder.stop();
    QFile mono(dir.filePath(QStringLiteral("mono.wav")));
    CHECK(mono.open(QIODevice::ReadOnly));
    const QByteArray header = mono.read(44);
    CHECK(qFromLittleEndian<quint16>(header.constData() + 22) == 1);
    CHECK(qFromLittleEndian<quint32>(header.constData() + 24) == 16000);

    // Metadata goes into the file's LIST/INFO chunk, and comes back through QMediaPlayer.
    QMediaMetaData tags;
    tags.insert(QMediaMetaData::Title, QStringLiteral("Take two"));
    tags.insert(QMediaMetaData::Date, QDate(2026, 11, 13));
    int metaChanges = 0;
    QObject::connect(&recorder, &QMediaRecorder::metaDataChanged, [&] { ++metaChanges; });
    recorder.setMetaData(tags);
    QMediaMetaData more;
    more.insert(QMediaMetaData::Author, QStringLiteral("Nt"));
    more.insert(QMediaMetaData::TrackNumber, 2);
    recorder.addMetaData(more);
    CHECK(metaChanges == 2);
    CHECK(recorder.metaData().value(QMediaMetaData::Title).toString() == QStringLiteral("Take two"));
    CHECK(recorder.metaData().value(QMediaMetaData::TrackNumber).toInt() == 2);
    recorder.setOutputLocation(QUrl::fromLocalFile(dir.filePath(QStringLiteral("tagged.wav"))));
    recorder.record();
    CHECK(waitFor([&] { return recorder.duration() >= 100; }));
    recorder.stop();
    QMediaPlayer player;
    player.setSource(QUrl::fromLocalFile(dir.filePath(QStringLiteral("tagged.wav"))));
    CHECK(player.mediaStatus() == QMediaPlayer::LoadedMedia);
    const QMediaMetaData back = player.metaData();
    CHECK(back.value(QMediaMetaData::Title).toString() == QStringLiteral("Take two"));
    CHECK(back.value(QMediaMetaData::Date).toDate() == QDate(2026, 11, 13));
    CHECK(back.value(QMediaMetaData::ContributingArtist).toString() == QStringLiteral("Nt")); // Author, as IART
    CHECK(back.value(QMediaMetaData::TrackNumber).toInt() == 2);
    CHECK(std::abs(player.duration() - recorder.duration()) <= 1);
    {
        QFile tagged(dir.filePath(QStringLiteral("tagged.wav")));
        CHECK(tagged.open(QIODevice::ReadOnly));
        const QByteArray bytes = tagged.readAll();
        CHECK(qFromLittleEndian<quint32>(bytes.constData() + 4) == quint32(bytes.size() - 8));
    }
    recorder.setMetaData(QMediaMetaData());

    // Taking the recorder out of the session stops it.
    recorder.record();
    CHECK(recorder.recorderState() == QMediaRecorder::RecordingState);
    session.setRecorder(nullptr);
    CHECK(recorder.recorderState() == QMediaRecorder::StoppedState);
    CHECK(recorder.captureSession() == nullptr);

    // QAudioInput.
    int volumeChanges = 0;
    QObject::connect(&input, &QAudioInput::volumeChanged, [&](float) { ++volumeChanges; });
    input.setVolume(2.0f);
    CHECK(input.volume() == 1.0f && volumeChanges == 0);
    input.setVolume(0.5f);
    CHECK(input.volume() == 0.5f && volumeChanges == 1);
    CHECK(input.isMuted());
    input.setDevice(QAudioDevice());
    CHECK(input.device().id() == "fake-mic");
}

void testDecoder(const QTemporaryDir &dir) {
    // Half a second of stereo: left counts up, right is constant.
    QByteArray pcm;
    for (int i = 0; i < 4000; ++i) {
        put16(pcm, quint16(i));
        put16(pcm, quint16(8192));
    }
    const QString path = dir.filePath(QStringLiteral("decode.wav"));
    {
        QFile f(path);
        CHECK(f.open(QIODevice::WriteOnly));
        f.write(wav16(2, 8000, pcm));
    }
    QAudioDecoder decoder;
    CHECK(decoder.isSupported());
    CHECK(!decoder.isDecoding());
    CHECK(decoder.duration() == -1);
    decoder.setSource(QUrl::fromLocalFile(path));
    CHECK(decoder.source() == QUrl::fromLocalFile(path));
    QByteArray decoded;
    qint64 lastStart = -1;
    bool finished = false;
    QList<bool> available;
    QObject::connect(&decoder, &QAudioDecoder::bufferReady, [&] {
        const QAudioBuffer buffer = decoder.read();
        CHECK(buffer.isValid());
        CHECK(buffer.startTime() > lastStart);
        lastStart = buffer.startTime();
        decoded.append(buffer.constData<char>(), buffer.byteCount());
    });
    QObject::connect(&decoder, &QAudioDecoder::finished, [&] { finished = true; });
    QObject::connect(&decoder, &QAudioDecoder::bufferAvailableChanged, [&](bool a) { available.append(a); });
    decoder.start();
    CHECK(decoder.isDecoding());
    CHECK(waitFor([&] { return finished; }));
    CHECK(!decoder.isDecoding());
    CHECK(decoded == pcm);
    CHECK(decoder.duration() == 500);
    CHECK(decoder.position() == 500);
    CHECK(!available.isEmpty() && available.last() == false);
    CHECK(!decoder.bufferAvailable());
    CHECK(!decoder.read().isValid());

    // Converted to mono float at twice the rate: (left + 8192) / 2, interpolated.
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);
    decoder.setAudioFormat(format);
    CHECK(decoder.audioFormat() == format);
    QList<float> samples;
    QObject::disconnect(&decoder, &QAudioDecoder::bufferReady, nullptr, nullptr);
    QObject::connect(&decoder, &QAudioDecoder::bufferReady, [&] {
        const QAudioBuffer buffer = decoder.read();
        CHECK(buffer.format() == format);
        const auto *frames = buffer.constData<QAudioBuffer::F32M>();
        for (qsizetype i = 0; i < buffer.frameCount(); ++i)
            samples.append(frames[i].channels[0]);
    });
    finished = false;
    decoder.start();
    CHECK(waitFor([&] { return finished; }));
    CHECK(std::abs(samples.size() - 8000) <= 2);
    const float expected = (101.0f / 32768.0f + 8192.0f / 32768.0f) / 2.0f; // frame 101 of the input
    CHECK(samples.size() > 202 && std::fabs(samples.at(202) - expected) < 1e-4f);
    const float between = (101.5f / 32768.0f + 8192.0f / 32768.0f) / 2.0f;
    CHECK(std::fabs(samples.at(203) - between) < 1e-4f);

    // Errors.
    QList<QAudioDecoder::Error> errors;
    QObject::connect(&decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error),
                     [&](QAudioDecoder::Error e) { errors.append(e); });
    decoder.setSource(QUrl::fromLocalFile(dir.filePath(QStringLiteral("none.wav"))));
    decoder.start();
    CHECK(decoder.error() == QAudioDecoder::ResourceError);
    CHECK(!decoder.isDecoding());
    QBuffer junk;
    junk.setData("not a wav file at all");
    junk.open(QIODevice::ReadOnly);
    decoder.setSourceDevice(&junk);
    CHECK(decoder.sourceDevice() == &junk);
    CHECK(decoder.source().isEmpty());
    decoder.start();
    CHECK(decoder.error() == QAudioDecoder::FormatError);
    CHECK(!decoder.errorString().isEmpty());
    CHECK(errors == (QList<QAudioDecoder::Error>{QAudioDecoder::ResourceError, QAudioDecoder::FormatError}));

    // Stop throws away what is queued.
    QBuffer device;
    device.setData(wav16(2, 8000, pcm));
    device.open(QIODevice::ReadOnly);
    decoder.setAudioFormat(QAudioFormat());
    QObject::disconnect(&decoder, &QAudioDecoder::bufferReady, nullptr, nullptr);
    decoder.setSourceDevice(&device);
    decoder.start();
    CHECK(waitFor([&] { return decoder.bufferAvailable(); }));
    decoder.stop();
    CHECK(!decoder.bufferAvailable() && !decoder.isDecoding());
    CHECK(decoder.position() == -1);
}

void testBuffers() {
    QAudioFormat format;
    format.setSampleRate(8000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);
    QAudioBuffer silence(800, format, 5000);
    CHECK(silence.isValid());
    CHECK(silence.frameCount() == 800 && silence.sampleCount() == 1600 && silence.byteCount() == 3200);
    CHECK(silence.duration() == 100000);
    CHECK(silence.startTime() == 5000);
    CHECK(silence.constData<QAudioBuffer::S16S>()[10].channels[1] == 0);

    QByteArray bytes;
    put16(bytes, 100);
    put16(bytes, quint16(-200));
    bytes.append('x'); // not a whole frame
    QAudioBuffer buffer(bytes, format);
    CHECK(buffer.frameCount() == 1);
    CHECK(buffer.startTime() == -1);
    QAudioBuffer copy = buffer;
    copy.data<QAudioBuffer::S16S>()[0].channels[0] = 7;
    CHECK(buffer.constData<qint16>()[0] == 100); // the copy detached
    CHECK(copy.constData<qint16>()[0] == 7);

    QAudioBuffer::S16S frame{{100, -200}};
    CHECK(frame.value(QAudioFormat::FrontLeft) == 100);
    CHECK(frame.value(QAudioFormat::FrontRight) == -200);
    CHECK(frame.value(QAudioFormat::LFE) == 0);
    CHECK(frame.average() == -50);
    frame.setValue(QAudioFormat::FrontRight, 300);
    CHECK(frame.channels[1] == 300);
    frame.clear();
    CHECK(frame.channels[0] == 0);
    QAudioBuffer::U8M u8{{0}};
    u8.clear();
    CHECK(u8.channels[0] == 128);
    CHECK(!QAudioBuffer().isValid());
    CHECK(!QAudioBuffer(10, QAudioFormat()).isValid());
}

void testMediaFormat() {
    QMediaFormat wave(QMediaFormat::Wave);
    CHECK(wave.isSupported(QMediaFormat::Encode));
    CHECK(wave.isSupported(QMediaFormat::Decode));
    CHECK(QMediaFormat().isSupported(QMediaFormat::Encode));
    QMediaFormat mp4(QMediaFormat::MPEG4);
    mp4.setVideoCodec(QMediaFormat::VideoCodec::H264);
    CHECK(!mp4.isSupported(QMediaFormat::Decode));
    CHECK(mp4.supportedAudioCodecs(QMediaFormat::Encode).isEmpty());
    CHECK(wave.supportedFileFormats(QMediaFormat::Encode) == QList<QMediaFormat::FileFormat>{QMediaFormat::Wave});
    CHECK(wave.supportedVideoCodecs(QMediaFormat::Encode).isEmpty());
    CHECK(QMediaFormat::fileFormatName(QMediaFormat::Wave) == QStringLiteral("Wave"));
    CHECK(QMediaFormat::fileFormatName(QMediaFormat::UnspecifiedFormat) == QStringLiteral("Unspecified"));
    CHECK(QMediaFormat::audioCodecName(QMediaFormat::AudioCodec::FLAC) == QStringLiteral("FLAC"));
    CHECK(QMediaFormat::videoCodecName(QMediaFormat::VideoCodec::H265) == QStringLiteral("H265"));
    CHECK(!QMediaFormat::fileFormatDescription(QMediaFormat::Matroska).isEmpty());
    CHECK(!QMediaFormat::audioCodecDescription(QMediaFormat::AudioCodec::Opus).isEmpty());
    CHECK(!QMediaFormat::videoCodecDescription(QMediaFormat::VideoCodec::AV1).isEmpty());
    mp4.resolveForEncoding(QMediaFormat::NoFlags);
    CHECK(mp4.fileFormat() == QMediaFormat::Wave && mp4.audioCodec() == QMediaFormat::AudioCodec::Wave);
    CHECK(mp4.videoCodec() == QMediaFormat::VideoCodec::Unspecified);
    (void)wave.mimeType(); // depends on the system's MIME database
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    World world;
    world.directory = dir.filePath(QStringLiteral("dev"));
    QDir().mkpath(world.directory);
    g_world = &world;
    setVideoBackend(std::make_unique<FakeVideoBackend>());
    setAudioBackend(std::make_unique<FakeAudioBackend>());

    testFrameFormat();
    testFrames();
    testCameraDevices();
    testCamera();
    testImageCapture(dir);
    testRecorder(dir);
    testDecoder(dir);
    testBuffers();
    testMediaFormat();

    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("all multimedia capture tests passed\n");
    return 0;
}
