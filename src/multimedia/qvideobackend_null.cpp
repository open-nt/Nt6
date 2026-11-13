// MIT License - see LICENSE
//
// The camera backend on platforms that Nt Multimedia does not support yet: no cameras.

#include "qvideobackend_p.h"

namespace QtMultimediaPrivate {

namespace {

class NullVideoBackend : public VideoBackend {
public:
    QList<QCameraDevice> videoInputs() override { return {}; }
    std::unique_ptr<CameraStream> open(const QCameraDevice &, const QCameraFormat &, FrameHandler, ErrorHandler,
                                       QString *error) override {
        *error = QStringLiteral("no camera backend on this platform");
        return nullptr;
    }
};

} // namespace

std::unique_ptr<VideoBackend> createPlatformVideoBackend() { return std::make_unique<NullVideoBackend>(); }

} // namespace QtMultimediaPrivate
