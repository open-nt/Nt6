// MIT License - see LICENSE
//
// The audio backend on Linux: ALSA, through the "default" device (which PipeWire and PulseAudio
// take over) and the devices that ALSA's name hints list. libasound is loaded on first use rather
// than linked, so Nt Multimedia builds without its headers and runs, without devices, on systems
// without it. The few functions used are declared here; their ABI has been stable since ALSA 1.0.
//
// The headset control of QAudioHeadset uses the simple mixer and control interfaces of one card: the card that
// NT_AUDIO_HEADSET_CARD names (a number or an id such as PCH), or else the one with the most of the controls below.
// The gain is split between a boost control ("Headset Mic Boost", "Mic Boost") and the capture volume ("Capture"),
// the input pin selects an item of "Input Source" or "Capture Source", the bias turns a switch whose name has
// "Bias" in it, and a jack control ("Headset Mic Jack", "Mic Jack") tells whether a microphone is plugged in. Each
// is used where the card's driver provides it.

#include "qaudiobackend_p.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qlibrary.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qtenvironmentvariables.h>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <mutex>
#include <vector>

namespace QtMultimediaPrivate {

namespace {

struct snd_pcm_t;
struct snd_mixer_t;
struct snd_mixer_elem_t;
struct snd_mixer_class_t;
struct snd_mixer_selem_regopt;
struct snd_ctl_t;
struct snd_ctl_elem_value_t;
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
constexpr int kChannelFrontLeft = 0; // SND_MIXER_SCHN_FRONT_LEFT
constexpr int kChannelFrontRight = 1;
constexpr int kInterfaceCard = 0; // SND_CTL_ELEM_IFACE_CARD, where the jack controls are

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
        loadMixer();
        return open && setParams && writei && readi && recover && pause && drop && drain && close && strerror &&
               nameHint && nameGetHint && nameFreeHint;
    }

    // The mixer and control functions, for the headset control; the streams work without them.
    struct Mixer {
        int (*open)(snd_mixer_t **, int) = nullptr;
        int (*close)(snd_mixer_t *) = nullptr;
        int (*attach)(snd_mixer_t *, const char *) = nullptr;
        int (*registerSimple)(snd_mixer_t *, snd_mixer_selem_regopt *, snd_mixer_class_t **) = nullptr;
        int (*load)(snd_mixer_t *) = nullptr;
        int (*handleEvents)(snd_mixer_t *) = nullptr;
        snd_mixer_elem_t *(*first)(snd_mixer_t *) = nullptr;
        snd_mixer_elem_t *(*next)(snd_mixer_elem_t *) = nullptr;
        const char *(*name)(snd_mixer_elem_t *) = nullptr;
        int (*hasCaptureVolume)(snd_mixer_elem_t *) = nullptr;
        int (*hasPlaybackVolume)(snd_mixer_elem_t *) = nullptr;
        int (*hasCaptureSwitch)(snd_mixer_elem_t *) = nullptr;
        int (*hasPlaybackSwitch)(snd_mixer_elem_t *) = nullptr;
        int (*isEnumerated)(snd_mixer_elem_t *) = nullptr;
        int (*enumItems)(snd_mixer_elem_t *) = nullptr;
        int (*getEnumItem)(snd_mixer_elem_t *, int, unsigned *) = nullptr;
        int (*setEnumItem)(snd_mixer_elem_t *, int, unsigned) = nullptr;
        int (*captureRange)(snd_mixer_elem_t *, long *, long *) = nullptr;
        int (*playbackRange)(snd_mixer_elem_t *, long *, long *) = nullptr;
        int (*captureDb)(snd_mixer_elem_t *, long, long *) = nullptr;
        int (*playbackDb)(snd_mixer_elem_t *, long, long *) = nullptr;
        int (*getCapture)(snd_mixer_elem_t *, int, long *) = nullptr;
        int (*getPlayback)(snd_mixer_elem_t *, int, long *) = nullptr;
        int (*setCaptureAll)(snd_mixer_elem_t *, long) = nullptr;
        int (*setPlaybackAll)(snd_mixer_elem_t *, long) = nullptr;
        int (*getCaptureSwitch)(snd_mixer_elem_t *, int, int *) = nullptr;
        int (*getPlaybackSwitch)(snd_mixer_elem_t *, int, int *) = nullptr;
        int (*setCaptureSwitchAll)(snd_mixer_elem_t *, int) = nullptr;
        int (*setPlaybackSwitchAll)(snd_mixer_elem_t *, int) = nullptr;
        int (*cardNext)(int *) = nullptr;
        int (*cardName)(int, char **) = nullptr;
        int (*ctlOpen)(snd_ctl_t **, const char *, int) = nullptr;
        int (*ctlClose)(snd_ctl_t *) = nullptr;
        int (*valueMalloc)(snd_ctl_elem_value_t **) = nullptr;
        void (*valueFree)(snd_ctl_elem_value_t *) = nullptr;
        void (*valueSetInterface)(snd_ctl_elem_value_t *, int) = nullptr;
        void (*valueSetName)(snd_ctl_elem_value_t *, const char *) = nullptr;
        int (*ctlRead)(snd_ctl_t *, snd_ctl_elem_value_t *) = nullptr;
        int (*valueBoolean)(const snd_ctl_elem_value_t *, unsigned) = nullptr;
    } mixer;
    bool mixerLoaded = false;

    bool loadMixer() {
        Mixer &m = mixer;
        resolve(m.open, "snd_mixer_open");
        resolve(m.close, "snd_mixer_close");
        resolve(m.attach, "snd_mixer_attach");
        resolve(m.registerSimple, "snd_mixer_selem_register");
        resolve(m.load, "snd_mixer_load");
        resolve(m.handleEvents, "snd_mixer_handle_events");
        resolve(m.first, "snd_mixer_first_elem");
        resolve(m.next, "snd_mixer_elem_next");
        resolve(m.name, "snd_mixer_selem_get_name");
        resolve(m.hasCaptureVolume, "snd_mixer_selem_has_capture_volume");
        resolve(m.hasPlaybackVolume, "snd_mixer_selem_has_playback_volume");
        resolve(m.hasCaptureSwitch, "snd_mixer_selem_has_capture_switch");
        resolve(m.hasPlaybackSwitch, "snd_mixer_selem_has_playback_switch");
        resolve(m.isEnumerated, "snd_mixer_selem_is_enumerated");
        resolve(m.enumItems, "snd_mixer_selem_get_enum_items");
        resolve(m.getEnumItem, "snd_mixer_selem_get_enum_item");
        resolve(m.setEnumItem, "snd_mixer_selem_set_enum_item");
        resolve(m.captureRange, "snd_mixer_selem_get_capture_volume_range");
        resolve(m.playbackRange, "snd_mixer_selem_get_playback_volume_range");
        resolve(m.captureDb, "snd_mixer_selem_ask_capture_vol_dB");
        resolve(m.playbackDb, "snd_mixer_selem_ask_playback_vol_dB");
        resolve(m.getCapture, "snd_mixer_selem_get_capture_volume");
        resolve(m.getPlayback, "snd_mixer_selem_get_playback_volume");
        resolve(m.setCaptureAll, "snd_mixer_selem_set_capture_volume_all");
        resolve(m.setPlaybackAll, "snd_mixer_selem_set_playback_volume_all");
        resolve(m.getCaptureSwitch, "snd_mixer_selem_get_capture_switch");
        resolve(m.getPlaybackSwitch, "snd_mixer_selem_get_playback_switch");
        resolve(m.setCaptureSwitchAll, "snd_mixer_selem_set_capture_switch_all");
        resolve(m.setPlaybackSwitchAll, "snd_mixer_selem_set_playback_switch_all");
        resolve(m.cardNext, "snd_card_next");
        resolve(m.cardName, "snd_card_get_name");
        resolve(m.ctlOpen, "snd_ctl_open");
        resolve(m.ctlClose, "snd_ctl_close");
        resolve(m.valueMalloc, "snd_ctl_elem_value_malloc");
        resolve(m.valueFree, "snd_ctl_elem_value_free");
        resolve(m.valueSetInterface, "snd_ctl_elem_value_set_interface");
        resolve(m.valueSetName, "snd_ctl_elem_value_set_name");
        resolve(m.ctlRead, "snd_ctl_elem_read");
        resolve(m.valueBoolean, "snd_ctl_elem_value_get_boolean");
        mixerLoaded = m.open && m.close && m.attach && m.registerSimple && m.load && m.handleEvents && m.first &&
                      m.next && m.name && m.hasCaptureVolume && m.hasPlaybackVolume && m.hasCaptureSwitch &&
                      m.hasPlaybackSwitch && m.isEnumerated && m.enumItems && m.getEnumItem && m.setEnumItem &&
                      m.captureRange && m.playbackRange && m.captureDb && m.playbackDb && m.getCapture &&
                      m.getPlayback && m.setCaptureAll && m.setPlaybackAll && m.getCaptureSwitch &&
                      m.getPlaybackSwitch && m.setCaptureSwitchAll && m.setPlaybackSwitchAll && m.cardNext &&
                      m.cardName && m.ctlOpen && m.ctlClose && m.valueMalloc && m.valueFree && m.valueSetInterface &&
                      m.valueSetName && m.ctlRead && m.valueBoolean;
        return mixerLoaded;
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

// A volume control of the simple mixer, on its capture side if it has one, else on its playback side (a boost that
// is not split into playback and capture is listed as both).
class AlsaVolume {
public:
    AlsaVolume() = default;
    AlsaVolume(const AlsaApi::Mixer *api, snd_mixer_elem_t *elem) : m_api(api), m_elem(elem) {
        m_capture = api->hasCaptureVolume(elem) != 0;
        long low = 0;
        long high = 0;
        if ((m_capture ? api->captureRange(elem, &low, &high) : api->playbackRange(elem, &low, &high)) < 0)
            return;
        for (long raw = low; raw <= high && high - low < 1024; ++raw) {
            long centi = 0;
            if ((m_capture ? api->captureDb(elem, raw, &centi) : api->playbackDb(elem, raw, &centi)) < 0)
                continue;
            m_raw.push_back(raw);
            m_db.push_back(double(centi) / 100.0);
        }
    }

    bool isValid() const { return m_elem && !m_db.empty(); }
    const std::vector<double> &steps() const { return m_db; }
    bool set(std::size_t step) const {
        return (m_capture ? m_api->setCaptureAll(m_elem, m_raw[step]) : m_api->setPlaybackAll(m_elem, m_raw[step])) >=
               0;
    }
    // The gain now, in dB, from the left channel.
    double current() const {
        long raw = 0;
        if ((m_capture ? m_api->getCapture(m_elem, kChannelFrontLeft, &raw)
                       : m_api->getPlayback(m_elem, kChannelFrontLeft, &raw)) < 0)
            return 0.0;
        for (std::size_t i = 0; i < m_raw.size(); ++i)
            if (m_raw[i] == raw)
                return m_db[i];
        return 0.0;
    }

private:
    const AlsaApi::Mixer *m_api = nullptr;
    snd_mixer_elem_t *m_elem = nullptr;
    bool m_capture = false;
    std::vector<long> m_raw;
    std::vector<double> m_db;
};

class AlsaHeadset : public AudioHeadsetControl {
public:
    // Opens the card chosen as the comment at the top describes, or returns nullptr when no card has a capture volume.
    static std::unique_ptr<AlsaHeadset> create(const std::shared_ptr<AlsaApi> &api) {
        if (!api || !api->mixerLoaded)
            return nullptr;
        const QByteArray wanted = qgetenv("NT_AUDIO_HEADSET_CARD");
        std::unique_ptr<AlsaHeadset> best;
        int bestScore = 0;
        int card = -1;
        while (api->mixer.cardNext(&card) >= 0 && card >= 0) {
            QByteArray device = "hw:" + QByteArray::number(card);
            if (!wanted.isEmpty()) {
                bool number = false;
                const int index = wanted.toInt(&number);
                if (number && index != card)
                    continue;
                if (!number)
                    device = "hw:" + wanted;
            }
            auto candidate = std::unique_ptr<AlsaHeadset>(new AlsaHeadset(api, card, device));
            const int score = candidate->score();
            if (score > bestScore) {
                bestScore = score;
                best = std::move(candidate);
            }
            if (!wanted.isEmpty())
                break;
        }
        return best;
    }

    ~AlsaHeadset() override {
        if (m_value)
            m_api->mixer.valueFree(m_value);
        if (m_ctl)
            m_api->mixer.ctlClose(m_ctl);
        if (m_mixer)
            m_api->mixer.close(m_mixer);
    }
    AlsaHeadset(const AlsaHeadset &) = delete;
    AlsaHeadset &operator=(const AlsaHeadset &) = delete;

    QString codecName() const override { return m_name; }
    double minimumGain() const override { return m_minimum; }
    double maximumGain() const override { return m_maximum; }

    bool apply(const AudioHeadsetSettings &settings, QString *error) override {
        const std::lock_guard<std::mutex> lock(m_mutex);
        const AlsaApi::Mixer &m = m_api->mixer;
        AudioHeadsetSettings next = settings;
        next.input = std::clamp(next.input, 1, 4);
        std::size_t boost = 0;
        std::size_t fine = 0;
        next.gain = splitGain(m_boost.isValid() ? m_boost.steps() : std::vector<double>(), m_fine.steps(),
                              std::clamp(next.gain, m_minimum, m_maximum), &boost, &fine);
        bool ok = m_fine.set(fine);
        if (m_boost.isValid())
            ok = m_boost.set(boost) && ok;
        if (m_bias) {
            const int on = next.bias != 0 ? 1 : 0;
            ok = (m.hasCaptureSwitch(m_bias) ? m.setCaptureSwitchAll(m_bias, on)
                                             : m.setPlaybackSwitchAll(m_bias, on)) >= 0 &&
                 ok;
        }
        if (m_source && next.input <= m.enumItems(m_source)) {
            const auto item = static_cast<unsigned>(next.input - 1);
            ok = m.setEnumItem(m_source, kChannelFrontLeft, item) >= 0 && ok;
            m.setEnumItem(m_source, kChannelFrontRight, item); // mono selectors have no right channel
        }
        if (!ok) {
            if (error)
                *error = QStringLiteral("cannot set the mixer of %1").arg(m_name);
            return false;
        }
        m_settings = next;
        return true;
    }

    AudioHeadsetSettings settings() const override {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_settings;
    }

    int status() override {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_api->mixer.handleEvents(m_mixer); // keeps the mixer's queue of change events from filling up
        if (!m_ctl || !m_value || m_api->mixer.ctlRead(m_ctl, m_value) < 0)
            return -1;
        // The button of a headset comes as an input event, not through the mixer, so it is not reported.
        return m_api->mixer.valueBoolean(m_value, 0) ? MicrophonePresent : 0;
    }

private:
    AlsaHeadset(std::shared_ptr<AlsaApi> api, int card, const QByteArray &device) : m_api(std::move(api)) {
        const AlsaApi::Mixer &m = m_api->mixer;
        char *name = nullptr;
        if (m.cardName(card, &name) >= 0 && name) {
            m_name = QString::fromUtf8(name);
            std::free(name); // NOLINT(cppcoreguidelines-no-malloc): ALSA allocates the name with malloc()
        }
        if (m.open(&m_mixer, 0) < 0) {
            m_mixer = nullptr;
            return;
        }
        if (m.attach(m_mixer, device.constData()) < 0 || m.registerSimple(m_mixer, nullptr, nullptr) < 0 ||
            m.load(m_mixer) < 0)
            return;
        findControls();
        openJack(device);
        if (!m_fine.isValid())
            return;
        const std::vector<double> &fine = m_fine.steps();
        m_minimum = *std::min_element(fine.begin(), fine.end());
        m_maximum = *std::max_element(fine.begin(), fine.end());
        if (m_boost.isValid()) {
            const std::vector<double> &boost = m_boost.steps();
            m_minimum += *std::min_element(boost.begin(), boost.end());
            m_maximum += *std::max_element(boost.begin(), boost.end());
        }
        // Start from what the card has now.
        m_settings.gain = m_fine.current() + (m_boost.isValid() ? m_boost.current() : 0.0);
        if (m_bias) {
            int on = 1;
            if (m.hasCaptureSwitch(m_bias))
                m.getCaptureSwitch(m_bias, kChannelFrontLeft, &on);
            else
                m.getPlaybackSwitch(m_bias, kChannelFrontLeft, &on);
            m_settings.bias = on ? 2 : 0;
        }
        unsigned item = 0;
        if (m_source && m.getEnumItem(m_source, kChannelFrontLeft, &item) >= 0 && item < 4)
            m_settings.input = int(item) + 1;
    }

    // Higher for a card with more of the controls; 0 when it has no capture volume to set the gain with.
    int score() const {
        if (!m_fine.isValid())
            return 0;
        return 1 + (m_boost.isValid() ? 2 : 0) + (m_ctl ? 4 : 0) + (m_bias ? 1 : 0);
    }

    void findControls() {
        const AlsaApi::Mixer &m = m_api->mixer;
        static const char *const boostNames[] = {"Headset Mic Boost", "Headphone Mic Boost", "Mic Boost",
                                                 "Front Mic Boost", "Rear Mic Boost"};
        static const char *const fineNames[] = {"Capture", "Headset Mic", "Mic", "ADC"};
        int boostRank = int(std::size(boostNames));
        int fineRank = int(std::size(fineNames));
        snd_mixer_elem_t *firstCapture = nullptr;
        for (snd_mixer_elem_t *e = m.first(m_mixer); e; e = m.next(e)) {
            const QByteArray name(m.name(e));
            for (int i = 0; i < boostRank; ++i) {
                if (name == boostNames[i] && (m.hasCaptureVolume(e) || m.hasPlaybackVolume(e))) {
                    m_boost = AlsaVolume(&m, e);
                    boostRank = i;
                    break;
                }
            }
            if (m.hasCaptureVolume(e)) {
                if (!firstCapture && !name.contains("Boost"))
                    firstCapture = e;
                for (int i = 0; i < fineRank; ++i) {
                    if (name == fineNames[i]) {
                        m_fine = AlsaVolume(&m, e);
                        fineRank = i;
                        break;
                    }
                }
            }
            if (!m_source && m.isEnumerated(e) && (name == "Input Source" || name == "Capture Source"))
                m_source = e;
            if (!m_bias && name.toLower().contains("bias") && (m.hasCaptureSwitch(e) || m.hasPlaybackSwitch(e)))
                m_bias = e;
        }
        if (!m_fine.isValid() && firstCapture)
            m_fine = AlsaVolume(&m, firstCapture);
    }

    // The jack controls are on the card's control interface, not in the simple mixer.
    void openJack(const QByteArray &device) {
        const AlsaApi::Mixer &m = m_api->mixer;
        if (m.ctlOpen(&m_ctl, device.constData(), 0) < 0) {
            m_ctl = nullptr;
            return;
        }
        if (m.valueMalloc(&m_value) < 0) {
            m_value = nullptr;
            return;
        }
        static const char *const jackNames[] = {"Headset Mic Jack", "Headphone Mic Jack", "Mic Jack", "Front Mic Jack",
                                                "Headset Jack"};
        for (const char *jack : jackNames) {
            m.valueSetInterface(m_value, kInterfaceCard);
            m.valueSetName(m_value, jack);
            if (m.ctlRead(m_ctl, m_value) >= 0)
                return;
        }
        m.ctlClose(m_ctl);
        m_ctl = nullptr;
    }

    std::shared_ptr<AlsaApi> m_api;
    mutable std::mutex m_mutex;
    QString m_name;
    snd_mixer_t *m_mixer = nullptr;
    snd_ctl_t *m_ctl = nullptr;
    snd_ctl_elem_value_t *m_value = nullptr;
    AlsaVolume m_boost;
    AlsaVolume m_fine;
    snd_mixer_elem_t *m_source = nullptr;
    snd_mixer_elem_t *m_bias = nullptr;
    double m_minimum = 0.0;
    double m_maximum = 0.0;
    AudioHeadsetSettings m_settings;
};

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

    AudioHeadsetControl *headsetControl() override {
        const std::lock_guard<std::mutex> lock(m_headsetMutex);
        if (!m_headsetTried) {
            m_headsetTried = true;
            m_headset = AlsaHeadset::create(alsaShared());
        }
        return m_headset.get();
    }

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
    std::mutex m_headsetMutex;
    bool m_headsetTried = false;
    std::unique_ptr<AlsaHeadset> m_headset;
};

} // namespace

std::unique_ptr<AudioBackend> createPlatformAudioBackend() { return std::make_unique<AlsaBackend>(); }

} // namespace QtMultimediaPrivate
