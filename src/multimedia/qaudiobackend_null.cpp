// MIT License - see LICENSE
//
// The audio backend on platforms that Nt Multimedia does not support yet: no devices.

#include "qaudiobackend_p.h"

namespace QtMultimediaPrivate {

namespace {

class NullBackend : public AudioBackend {
public:
    QList<QAudioDevice> devices(QAudioDevice::Mode) override { return {}; }
    std::unique_ptr<AudioPcm> open(const QByteArray &, QAudioDevice::Mode, const QAudioFormat &, qint64,
                                   QString *error) override {
        *error = QStringLiteral("no audio backend on this platform");
        return nullptr;
    }
};

} // namespace

std::unique_ptr<AudioBackend> createPlatformAudioBackend() { return std::make_unique<NullBackend>(); }

} // namespace QtMultimediaPrivate
