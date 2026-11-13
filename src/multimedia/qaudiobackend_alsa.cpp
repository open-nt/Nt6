// MIT License - see LICENSE
//
// The audio backend on Linux: ALSA, through the "default" device (which PipeWire and PulseAudio
// take over) and the devices that ALSA's name hints list. libasound is loaded on first use rather
// than linked, so Nt Multimedia builds without its headers and runs, without devices, on systems
// without it. The few functions used are declared here; their ABI has been stable since ALSA 1.0.

#include "qaudiobackend_p.h"

#include <QtCore/qlibrary.h>
#include <QtCore/qstringlist.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <memory>
#include <mutex>

namespace QtMultimediaPrivate {

namespace {

struct snd_pcm_t;
using snd_pcm_sframes_t = long;
using snd_pcm_uframes_t = unsigned long;

// Values of the ALSA enumerations, from <alsa/pcm.h>.
constexpr int kStreamPlayback = 0;
constexpr int kStreamCapture = 1;
constexpr int kFormatU8 = 1;
constexpr int kFormatS16Le = 2;
constexpr int kFormatS32Le = 10;
constexpr int kFormatFloatLe = 14;
constexpr int kAccessRwInterleaved = 3;

struct AlsaApi {
    QLibrary library;
    int (*open)(snd_pcm_t **, const char *, int, int) = nullptr;
    int (*setParams)(snd_pcm_t *, int, int, unsigned, unsigned, int, unsigned) = nullptr;
    snd_pcm_sframes_t (*writei)(snd_pcm_t *, const void *, snd_pcm_uframes_t) = nullptr;
    snd_pcm_sframes_t (*readi)(snd_pcm_t *, void *, snd_pcm_uframes_t) = nullptr;
    int (*recover)(snd_pcm_t *, int, int) = nullptr;
    int (*pause)(snd_pcm_t *, int) = nullptr;
    int (*drop)(snd_pcm_t *) = nullptr;
    int (*drain)(snd_pcm_t *) = nullptr;
    int (*close)(snd_pcm_t *) = nullptr;
    const char *(*strerror)(int) = nullptr;
    int (*nameHint)(int, const char *, void ***) = nullptr;
    char *(*nameGetHint)(const void *, const char *) = nullptr;
    int (*nameFreeHint)(void **) = nullptr;

    bool load() {
        library.setFileNameAndVersion(QStringLiteral("asound"), 2);
        if (!library.load())
            return false;
        resolve(open, "snd_pcm_open");
        resolve(setParams, "snd_pcm_set_params");
        resolve(writei, "snd_pcm_writei");
        resolve(readi, "snd_pcm_readi");
        resolve(recover, "snd_pcm_recover");
        resolve(pause, "snd_pcm_pause");
        resolve(drop, "snd_pcm_drop");
        resolve(drain, "snd_pcm_drain");
        resolve(close, "snd_pcm_close");
        resolve(strerror, "snd_strerror");
        resolve(nameHint, "snd_device_name_hint");
        resolve(nameGetHint, "snd_device_name_get_hint");
        resolve(nameFreeHint, "snd_device_name_free_hint");
        return open && setParams && writei && readi && recover && pause && drop && drain && close && strerror &&
               nameHint && nameGetHint && nameFreeHint;
    }

    template <typename Fn> void resolve(Fn &fn, const char *name) { fn = reinterpret_cast<Fn>(library.resolve(name)); }

    QString errorString(int err) const { return QString::fromUtf8(strerror(err)); }
};

int alsaFormat(QAudioFormat::SampleFormat format) {
    switch (format) {
    case QAudioFormat::UInt8:
        return kFormatU8;
    case QAudioFormat::Int16:
        return kFormatS16Le;
    case QAudioFormat::Int32:
        return kFormatS32Le;
    case QAudioFormat::Float:
        return kFormatFloatLe;
    case QAudioFormat::Unknown:
    case QAudioFormat::NSampleFormats:
        break;
    }
    return -1;
}

class AlsaPcm : public AudioPcm {
public:
    AlsaPcm(std::shared_ptr<AlsaApi> api, snd_pcm_t *pcm, int bytesPerFrame)
        : m_api(std::move(api)), m_pcm(pcm), m_frame(bytesPerFrame) {}
    ~AlsaPcm() override { m_api->close(m_pcm); }

    qint64 write(const char *data, qint64 bytes) override {
        auto frames = static_cast<snd_pcm_uframes_t>(bytes / m_frame);
        qint64 done = 0;
        while (frames > 0) {
            const snd_pcm_sframes_t n = m_api->writei(m_pcm, data + done, frames);
            if (n == -EAGAIN || n == -EWOULDBLOCK)
                continue;
            if (n < 0) {
                if (m_api->recover(m_pcm, static_cast<int>(n), 1) < 0)
                    return -1;
                continue;
            }
            frames -= static_cast<snd_pcm_uframes_t>(n);
            done += qint64(n) * m_frame;
        }
        return done;
    }

    qint64 read(char *data, qint64 bytes) override {
        const auto frames = static_cast<snd_pcm_uframes_t>(bytes / m_frame);
        for (;;) {
            const snd_pcm_sframes_t n = m_api->readi(m_pcm, data, frames);
            if (n == -EAGAIN || n == -EWOULDBLOCK)
                continue;
            if (n < 0) {
                if (m_api->recover(m_pcm, static_cast<int>(n), 1) < 0)
                    return -1;
                continue;
            }
            return qint64(n) * m_frame;
        }
    }

    // Devices that cannot pause underrun instead, and recover when writing starts again.
    void setPaused(bool paused) override { m_api->pause(m_pcm, paused ? 1 : 0); }
    void drop() override { m_api->drop(m_pcm); }
    void drain() override { m_api->drain(m_pcm); }

private:
    std::shared_ptr<AlsaApi> m_api;
    snd_pcm_t *m_pcm;
    int m_frame;
};

// Plugins that the name hints list but that are not useful to choose: raw hardware, fixed channel
// layouts, digital outputs and rate converters.
bool isHiddenDevice(const QString &name) {
    static const char *const prefixes[] = {"null",  "hw:",    "front:",    "surround", "iec958",  "spdif",
                                           "dmix",  "dsnoop", "upmix",     "vdownmix", "lavrate", "samplerate",
                                           "speex", "a52",    "usbstream", "oss"};
    return std::any_of(std::begin(prefixes), std::end(prefixes),
                       [&](const char *prefix) { return name.startsWith(QLatin1String(prefix)); });
}

class AlsaBackend : public AudioBackend {
public:
    QList<QAudioDevice> devices(QAudioDevice::Mode mode) override {
        QList<QAudioDevice> list;
        AlsaApi *api = alsa();
        if (!api || mode == QAudioDevice::Null)
            return list;
        void **hints = nullptr;
        if (api->nameHint(-1, "pcm", &hints) < 0 || !hints)
            return list;
        const char *wanted = mode == QAudioDevice::Output ? "Output" : "Input";
        QAudioDevicePrivate info;
        info.mode = mode;
        info.preferredFormat.setSampleRate(48000);
        info.preferredFormat.setChannelCount(mode == QAudioDevice::Output ? 2 : 1);
        info.preferredFormat.setSampleFormat(QAudioFormat::Int16);
        info.channelConfiguration =
            mode == QAudioDevice::Output ? QAudioFormat::ChannelConfigStereo : QAudioFormat::ChannelConfigMono;
        bool haveDefault = false;
        for (void **hint = hints; *hint; ++hint) {
            const QString name = take(api->nameGetHint(*hint, "NAME"));
            const QString ioid = take(api->nameGetHint(*hint, "IOID"));
            QString description = take(api->nameGetHint(*hint, "DESC"));
            if (name.isEmpty() || isHiddenDevice(name) || (!ioid.isEmpty() && ioid != QLatin1String(wanted)))
                continue;
            description.replace(QLatin1Char('\n'), QLatin1String(" - "));
            info.id = name.toUtf8();
            info.description = description.isEmpty() ? name : description;
            info.isDefault = name == QLatin1String("default");
            if (info.isDefault) {
                haveDefault = true;
                list.prepend(makeAudioDevice(info));
            } else {
                list.append(makeAudioDevice(info));
            }
        }
        api->nameFreeHint(hints);
        if (!haveDefault) {
            info.id = "default";
            info.description = QStringLiteral("Default");
            info.isDefault = true;
            list.prepend(makeAudioDevice(info));
        }
        return list;
    }

    std::unique_ptr<AudioPcm> open(const QByteArray &id, QAudioDevice::Mode mode, const QAudioFormat &format,
                                   qint64 periodBytes, QString *error) override {
        std::shared_ptr<AlsaApi> api = alsaShared();
        if (!api) {
            *error = QStringLiteral("ALSA (libasound.so.2) is not available");
            return nullptr;
        }
        const int sampleFormat = alsaFormat(format.sampleFormat());
        if (!format.isValid() || sampleFormat < 0) {
            *error = QStringLiteral("unsupported audio format");
            return nullptr;
        }
        snd_pcm_t *pcm = nullptr;
        const QByteArray name = id.isEmpty() ? QByteArray("default") : id;
        int err = api->open(&pcm, name.constData(), mode == QAudioDevice::Output ? kStreamPlayback : kStreamCapture, 0);
        if (err < 0) {
            *error = QStringLiteral("cannot open %1: %2").arg(QString::fromUtf8(name), api->errorString(err));
            return nullptr;
        }
        // The device buffers about two periods, so that what is queued in the ring stays small.
        const qint64 periodFrames = std::max<qint64>(periodBytes / format.bytesPerFrame(), 1);
        const auto latency =
            static_cast<unsigned>(std::max<qint64>(2 * format.durationForFrames(qint32(periodFrames)), 10000));
        err = api->setParams(pcm, sampleFormat, kAccessRwInterleaved, static_cast<unsigned>(format.channelCount()),
                             static_cast<unsigned>(format.sampleRate()), 1, latency);
        if (err < 0) {
            *error =
                QStringLiteral("cannot set the format of %1: %2").arg(QString::fromUtf8(name), api->errorString(err));
            api->close(pcm);
            return nullptr;
        }
        return std::make_unique<AlsaPcm>(api, pcm, format.bytesPerFrame());
    }

    QString deviceDirectory() const override { return QStringLiteral("/dev/snd"); }

private:
    // Takes a string that ALSA allocated with malloc().
    static QString take(char *s) {
        if (!s)
            return QString();
        QString result = QString::fromUtf8(s);
        std::free(s); // NOLINT(cppcoreguidelines-no-malloc): ALSA allocates hints with malloc()
        return result;
    }

    std::shared_ptr<AlsaApi> alsaShared() {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_tried) {
            m_tried = true;
            auto api = std::make_shared<AlsaApi>();
            if (api->load())
                m_api = std::move(api);
        }
        return m_api;
    }
    AlsaApi *alsa() { return alsaShared().get(); }

    std::mutex m_mutex;
    bool m_tried = false;
    std::shared_ptr<AlsaApi> m_api;
};

} // namespace

std::unique_ptr<AudioBackend> createPlatformAudioBackend() { return std::make_unique<AlsaBackend>(); }

} // namespace QtMultimediaPrivate
