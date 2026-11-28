// MIT License - see LICENSE
//
// The camera backend on Android: the NDK's Camera2 API (ACameraManager), which needs API level 24. Each camera the
// manager lists is a QCameraDevice; its formats are the YUV_420_888 output sizes of the stream configurations, with
// the frame rate their minimum frame duration allows. A stream opens the camera, points a repeating preview request
// at an AImageReader and copies each image into an NV12, NV21 or YUV420P frame on the reader's own thread. The frame
// carries the rotation that turns the sensor's picture upright on the screen as the display is turned when the
// stream starts, and front cameras are mirrored, as Qt does. The application needs the CAMERA permission (see
// QCameraPermission) before it starts the camera.

#include "qvideobackend_p.h"

#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjniobject.h>

#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCaptureRequest.h>
#include <media/NdkImage.h>
#include <media/NdkImageReader.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

namespace QtMultimediaPrivate {

namespace {

constexpr int kReaderImages = 3; // one being copied, one queued, one being filled

QString cameraError(const char *what, camera_status_t status) {
    return QStringLiteral("%1 (camera error %2)").arg(QString::fromUtf8(what)).arg(int(status));
}

// The clockwise rotation, in degrees, of the display from its natural orientation: Display.getRotation() * 90.
int displayRotation() {
    const QJniObject context(QNativeInterface::QAndroidApplication::context());
    if (!context.isValid())
        return 0;
    const QJniObject service =
        QJniObject::getStaticObjectField("android/content/Context", "WINDOW_SERVICE", "Ljava/lang/String;");
    const QJniObject manager =
        context.callObjectMethod("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;", service.object());
    if (!manager.isValid())
        return 0;
    const QJniObject display = manager.callObjectMethod("getDefaultDisplay", "()Landroid/view/Display;");
    return display.isValid() ? display.callMethod<jint>("getRotation") * 90 : 0;
}

QtVideo::Rotation rotationOf(int degrees) {
    switch (((degrees % 360) + 360) % 360) {
    case 90:
        return QtVideo::Rotation::Clockwise90;
    case 180:
        return QtVideo::Rotation::Clockwise180;
    case 270:
        return QtVideo::Rotation::Clockwise270;
    default:
        return QtVideo::Rotation::None;
    }
}

struct CameraInfo {
    int facing = ACAMERA_LENS_FACING_EXTERNAL;
    int orientation = 0; // ACAMERA_SENSOR_ORIENTATION, degrees
    QList<QCameraFormat> formats;
};

CameraInfo cameraInfo(ACameraManager *manager, const char *id) {
    CameraInfo info;
    ACameraMetadata *metadata = nullptr;
    if (ACameraManager_getCameraCharacteristics(manager, id, &metadata) != ACAMERA_OK || !metadata)
        return info;
    ACameraMetadata_const_entry entry{};
    if (ACameraMetadata_getConstEntry(metadata, ACAMERA_LENS_FACING, &entry) == ACAMERA_OK && entry.count > 0)
        info.facing = entry.data.u8[0];
    if (ACameraMetadata_getConstEntry(metadata, ACAMERA_SENSOR_ORIENTATION, &entry) == ACAMERA_OK && entry.count > 0)
        info.orientation = entry.data.i32[0];

    // The lowest frame rate the auto exposure can hold, and the shortest frame of each YUV size.
    float minRate = 0.0f;
    if (ACameraMetadata_getConstEntry(metadata, ACAMERA_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES, &entry) == ACAMERA_OK) {
        for (uint32_t i = 0; i + 1 < entry.count; i += 2) {
            const auto lo = float(entry.data.i32[i]);
            minRate = minRate == 0.0f ? lo : std::min(minRate, lo);
        }
    }
    struct Size {
        int width;
        int height;
        float maxRate;
    };
    std::vector<Size> sizes;
    if (ACameraMetadata_getConstEntry(metadata, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry) == ACAMERA_OK) {
        for (uint32_t i = 0; i + 3 < entry.count; i += 4) {
            const int32_t *c = entry.data.i32 + i;
            if (c[0] == AIMAGE_FORMAT_YUV_420_888 && c[3] == ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT)
                sizes.push_back({c[1], c[2], 30.0f});
        }
    }
    if (ACameraMetadata_getConstEntry(metadata, ACAMERA_SCALER_AVAILABLE_MIN_FRAME_DURATIONS, &entry) == ACAMERA_OK) {
        for (uint32_t i = 0; i + 3 < entry.count; i += 4) {
            const int64_t *d = entry.data.i64 + i;
            if (d[0] != AIMAGE_FORMAT_YUV_420_888 || d[3] <= 0)
                continue;
            for (Size &s : sizes)
                if (s.width == int(d[1]) && s.height == int(d[2]))
                    s.maxRate = float(1e9 / double(d[3]));
        }
    }
    ACameraMetadata_free(metadata);
    // Largest first, as the camera lists them.
    std::sort(sizes.begin(), sizes.end(),
              [](const Size &a, const Size &b) { return qint64(a.width) * a.height > qint64(b.width) * b.height; });
    for (const Size &s : sizes) {
        const QCameraFormat format =
            makeCameraFormat(QVideoFrameFormat::Format_NV12, QSize(s.width, s.height),
                             std::min(minRate == 0.0f ? s.maxRate : minRate, s.maxRate), s.maxRate);
        if (!info.formats.contains(format))
            info.formats.append(format);
    }
    return info;
}

class AndroidStream : public CameraStream {
public:
    ~AndroidStream() override { close(); }

    bool open(ACameraManager *manager, const QCameraDevice &device, const QCameraFormat &format,
              VideoBackend::FrameHandler frames, VideoBackend::ErrorHandler failed, QString *error) {
        auto fail = [&](const char *what, camera_status_t status) {
            *error = status == ACAMERA_ERROR_PERMISSION_DENIED
                         ? QStringLiteral("the application has no CAMERA permission")
                         : cameraError(what, status);
            close();
            return false;
        };
        m_frames = std::move(frames);
        m_failed = std::move(failed);
        const QByteArray id = device.id();
        const CameraInfo info = cameraInfo(manager, id.constData());
        const bool front = info.facing == ACAMERA_LENS_FACING_FRONT;
        const int display = displayRotation();
        // As Camera.CameraInfo documents: the front camera turns the other way, and is shown mirrored.
        m_rotation = rotationOf(front ? (info.orientation + display) % 360 : info.orientation - display);
        m_mirrored = front;

        const QSize size = format.resolution();
        if (size.isEmpty())
            return fail("no frame size", ACAMERA_ERROR_INVALID_PARAMETER);
        if (AImageReader_new(size.width(), size.height(), AIMAGE_FORMAT_YUV_420_888, kReaderImages, &m_reader) !=
            AMEDIA_OK)
            return fail("cannot make an image reader", ACAMERA_ERROR_UNKNOWN);
        AImageReader_ImageListener listener{};
        listener.context = this;
        listener.onImageAvailable = &AndroidStream::onImage;
        AImageReader_setImageListener(m_reader, &listener);
        if (AImageReader_getWindow(m_reader, &m_window) != AMEDIA_OK)
            return fail("cannot get the window of the image reader", ACAMERA_ERROR_UNKNOWN);
        ANativeWindow_acquire(m_window);

        m_deviceCallbacks.context = this;
        m_deviceCallbacks.onDisconnected = &AndroidStream::onDisconnected;
        m_deviceCallbacks.onError = &AndroidStream::onError;
        camera_status_t status = ACameraManager_openCamera(manager, id.constData(), &m_deviceCallbacks, &m_device);
        if (status != ACAMERA_OK)
            return fail("cannot open the camera", status);
        if ((status = ACaptureSessionOutputContainer_create(&m_outputs)) != ACAMERA_OK ||
            (status = ACaptureSessionOutput_create(m_window, &m_output)) != ACAMERA_OK ||
            (status = ACaptureSessionOutputContainer_add(m_outputs, m_output)) != ACAMERA_OK)
            return fail("cannot set up the camera output", status);
        m_sessionCallbacks.context = this;
        if ((status = ACameraDevice_createCaptureSession(m_device, m_outputs, &m_sessionCallbacks, &m_session)) !=
            ACAMERA_OK)
            return fail("cannot start a capture session", status);
        if ((status = ACameraDevice_createCaptureRequest(m_device, TEMPLATE_PREVIEW, &m_request)) != ACAMERA_OK ||
            (status = ACameraOutputTarget_create(m_window, &m_target)) != ACAMERA_OK ||
            (status = ACaptureRequest_addTarget(m_request, m_target)) != ACAMERA_OK)
            return fail("cannot make the capture request", status);
        if (format.maxFrameRate() > 0.0f) {
            const int32_t range[2] = {int32_t(std::max(1.0f, format.minFrameRate())), int32_t(format.maxFrameRate())};
            ACaptureRequest_setEntry_i32(m_request, ACAMERA_CONTROL_AE_TARGET_FPS_RANGE, 2, range);
            m_format.setStreamFrameRate(qreal(format.maxFrameRate()));
        }
        const uint8_t focus = ACAMERA_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
        ACaptureRequest_setEntry_u8(m_request, ACAMERA_CONTROL_AF_MODE, 1, &focus);
        if ((status = ACameraCaptureSession_setRepeatingRequest(m_session, nullptr, 1, &m_request, nullptr)) !=
            ACAMERA_OK)
            return fail("cannot start the camera", status);
        m_running = true;
        return true;
    }

private:
    static void onDisconnected(void *context, ACameraDevice *) {
        static_cast<AndroidStream *>(context)->fail(QStringLiteral("the camera was disconnected"));
    }
    static void onError(void *context, ACameraDevice *, int error) {
        static_cast<AndroidStream *>(context)->fail(QStringLiteral("the camera failed (error %1)").arg(error));
    }
    static void onImage(void *context, AImageReader *reader) {
        static_cast<AndroidStream *>(context)->readImage(reader);
    }

    void fail(const QString &why) {
        if (!m_failedOnce.exchange(true) && m_failed)
            m_failed(why);
    }

    void readImage(AImageReader *reader) {
        AImage *image = nullptr;
        // The newest image only: frames that piled up while one was being copied are dropped.
        if (AImageReader_acquireLatestImage(reader, &image) != AMEDIA_OK || !image)
            return;
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_failedOnce.load())
            deliver(image);
        AImage_delete(image);
    }

    // Copies the planes of a YUV_420_888 image into a packed frame. Camera2 hands out either three planes (pixel
    // stride 1) or a Y plane and interleaved chroma (pixel stride 2) whose U and V planes are one byte apart.
    void deliver(AImage *image) {
        int32_t width = 0;
        int32_t height = 0;
        int64_t timestamp = 0;
        AImage_getWidth(image, &width);
        AImage_getHeight(image, &height);
        AImage_getTimestamp(image, &timestamp);
        if (width <= 0 || height <= 0)
            return;
        uint8_t *planes[3] = {};
        int lengths[3] = {};
        int32_t rowStrides[3] = {};
        int32_t pixelStrides[3] = {};
        for (int p = 0; p < 3; ++p) {
            if (AImage_getPlaneData(image, p, &planes[p], &lengths[p]) != AMEDIA_OK ||
                AImage_getPlaneRowStride(image, p, &rowStrides[p]) != AMEDIA_OK ||
                AImage_getPlanePixelStride(image, p, &pixelStrides[p]) != AMEDIA_OK)
                return;
        }
        const int chromaWidth = (width + 1) / 2;
        const int chromaHeight = (height + 1) / 2;
        QVideoFrameFormat::PixelFormat pixelFormat = QVideoFrameFormat::Format_YUV420P;
        const uint8_t *chroma = nullptr;
        if (pixelStrides[1] == 2 && pixelStrides[2] == 2 && rowStrides[1] == rowStrides[2]) {
            if (planes[2] == planes[1] + 1) {
                pixelFormat = QVideoFrameFormat::Format_NV12;
                chroma = planes[1];
            } else if (planes[1] == planes[2] + 1) {
                pixelFormat = QVideoFrameFormat::Format_NV21;
                chroma = planes[2];
            }
        }
        const qsizetype lumaBytes = qsizetype(width) * height;
        QByteArray data(lumaBytes + 2 * qsizetype(chromaWidth) * chromaHeight, Qt::Uninitialized);
        auto *out = reinterpret_cast<uint8_t *>(data.data());
        for (int y = 0; y < height; ++y)
            std::memcpy(out + qsizetype(y) * width, planes[0] + qsizetype(y) * rowStrides[0], std::size_t(width));
        out += lumaBytes;
        if (chroma) {
            // Interleaved: one row of chroma is chromaWidth pairs; the last pair of the plane may be one byte short.
            const int rowBytes = 2 * chromaWidth;
            const qsizetype available = qsizetype(lengths[1]) + 1;
            for (int y = 0; y < chromaHeight; ++y) {
                const qsizetype from = qsizetype(y) * rowStrides[1];
                const auto n = std::size_t(std::clamp<qsizetype>(available - from, 0, rowBytes));
                std::memcpy(out + qsizetype(y) * rowBytes, chroma + from, n);
            }
        } else {
            pixelFormat = QVideoFrameFormat::Format_YUV420P;
            for (int p = 1; p < 3; ++p) {
                for (int y = 0; y < chromaHeight; ++y) {
                    const uint8_t *row = planes[p] + qsizetype(y) * rowStrides[p];
                    uint8_t *to = out + qsizetype(y) * chromaWidth;
                    if (pixelStrides[p] == 1) {
                        std::memcpy(to, row, std::size_t(chromaWidth));
                    } else {
                        for (int x = 0; x < chromaWidth; ++x)
                            to[x] = row[qsizetype(x) * pixelStrides[p]];
                    }
                }
                out += qsizetype(chromaWidth) * chromaHeight;
            }
        }
        if (m_format.frameWidth() != width || m_format.frameHeight() != height ||
            m_format.pixelFormat() != pixelFormat) {
            const qreal rate = m_format.streamFrameRate();
            m_format = QVideoFrameFormat(QSize(width, height), pixelFormat);
            m_format.setStreamFrameRate(rate);
        }
        QVideoFrame frame = makeVideoFrame(m_format, data);
        if (!frame.isValid())
            return;
        if (m_firstTimestamp < 0)
            m_firstTimestamp = timestamp;
        frame.setStartTime((timestamp - m_firstTimestamp) / 1000);
        frame.setRotation(m_rotation);
        frame.setMirrored(m_mirrored);
        m_frames(frame);
    }

    void close() {
        if (m_session) {
            if (m_running)
                ACameraCaptureSession_stopRepeating(m_session);
            ACameraCaptureSession_close(m_session);
            m_session = nullptr;
        }
        m_running = false;
        if (m_request) {
            if (m_target)
                ACaptureRequest_removeTarget(m_request, m_target);
            ACaptureRequest_free(m_request);
            m_request = nullptr;
        }
        if (m_target) {
            ACameraOutputTarget_free(m_target);
            m_target = nullptr;
        }
        if (m_device) {
            ACameraDevice_close(m_device);
            m_device = nullptr;
        }
        if (m_outputs) {
            if (m_output)
                ACaptureSessionOutputContainer_remove(m_outputs, m_output);
            ACaptureSessionOutputContainer_free(m_outputs);
            m_outputs = nullptr;
        }
        if (m_output) {
            ACaptureSessionOutput_free(m_output);
            m_output = nullptr;
        }
        {
            // No image callback may be copying when the reader goes, nor deliver a frame after the stream is closed.
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_failedOnce = true;
        }
        if (m_reader) {
            AImageReader_delete(m_reader); // waits for its listener to return
            m_reader = nullptr;
        }
        if (m_window) {
            ANativeWindow_release(m_window);
            m_window = nullptr;
        }
    }

    ACameraDevice *m_device = nullptr;
    ACameraDevice_StateCallbacks m_deviceCallbacks{};
    ACameraCaptureSession_stateCallbacks m_sessionCallbacks{};
    ACaptureSessionOutputContainer *m_outputs = nullptr;
    ACaptureSessionOutput *m_output = nullptr;
    ACameraCaptureSession *m_session = nullptr;
    ACaptureRequest *m_request = nullptr;
    ACameraOutputTarget *m_target = nullptr;
    AImageReader *m_reader = nullptr;
    ANativeWindow *m_window = nullptr;
    bool m_running = false;

    std::mutex m_mutex;
    std::atomic<bool> m_failedOnce{false};
    QVideoFrameFormat m_format;
    qint64 m_firstTimestamp = -1;
    QtVideo::Rotation m_rotation = QtVideo::Rotation::None;
    bool m_mirrored = false;
    VideoBackend::FrameHandler m_frames;
    VideoBackend::ErrorHandler m_failed;
};

class AndroidVideoBackend : public VideoBackend {
public:
    AndroidVideoBackend() : m_manager(ACameraManager_create()) {}
    ~AndroidVideoBackend() override {
        if (m_manager)
            ACameraManager_delete(m_manager);
    }
    AndroidVideoBackend(const AndroidVideoBackend &) = delete;
    AndroidVideoBackend &operator=(const AndroidVideoBackend &) = delete;

    QList<QCameraDevice> videoInputs() override {
        QList<QCameraDevice> list;
        ACameraIdList *ids = nullptr;
        if (!m_manager || ACameraManager_getCameraIdList(m_manager, &ids) != ACAMERA_OK || !ids)
            return list;
        for (int i = 0; i < ids->numCameras; ++i) {
            const CameraInfo info = cameraInfo(m_manager, ids->cameraIds[i]);
            if (info.formats.isEmpty())
                continue;
            QCameraDevicePrivate d;
            d.id = QByteArray(ids->cameraIds[i]);
            switch (info.facing) {
            case ACAMERA_LENS_FACING_BACK:
                d.position = QCameraDevice::BackFace;
                d.description = QStringLiteral("Back camera");
                break;
            case ACAMERA_LENS_FACING_FRONT:
                d.position = QCameraDevice::FrontFace;
                d.description = QStringLiteral("Front camera");
                break;
            default:
                d.description = QStringLiteral("External camera");
                break;
            }
            d.description += QStringLiteral(" (%1)").arg(QString::fromUtf8(d.id));
            d.videoFormats = info.formats;
            for (const QCameraFormat &f : std::as_const(info.formats))
                if (!d.photoResolutions.contains(f.resolution()))
                    d.photoResolutions.append(f.resolution());
            list.append(makeCameraDevice(d));
        }
        ACameraManager_deleteCameraIdList(ids);
        // The first back camera is the default, as on Qt.
        const auto back = std::find_if(list.begin(), list.end(),
                                       [](const QCameraDevice &c) { return c.position() == QCameraDevice::BackFace; });
        if (back != list.end() && back != list.begin())
            std::rotate(list.begin(), back, back + 1);
        for (qsizetype i = 0; i < list.size(); ++i) {
            QCameraDevicePrivate d;
            d.id = list[i].id();
            d.description = list[i].description();
            d.position = list[i].position();
            d.photoResolutions = list[i].photoResolutions();
            d.videoFormats = list[i].videoFormats();
            d.isDefault = i == 0;
            list[i] = makeCameraDevice(d);
        }
        return list;
    }

    std::unique_ptr<CameraStream> open(const QCameraDevice &device, const QCameraFormat &format, FrameHandler frames,
                                       ErrorHandler failed, QString *error) override {
        if (!m_manager) {
            *error = QStringLiteral("no camera service");
            return nullptr;
        }
        auto stream = std::make_unique<AndroidStream>();
        if (!stream->open(m_manager, device, format, std::move(frames), std::move(failed), error))
            return nullptr;
        return stream;
    }

private:
    ACameraManager *m_manager;
};

} // namespace

std::unique_ptr<VideoBackend> createPlatformVideoBackend() { return std::make_unique<AndroidVideoBackend>(); }

} // namespace QtMultimediaPrivate
