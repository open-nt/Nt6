// MIT License - see LICENSE
//
// The audio backend on Zephyr: the I2S controller that the devicetree alias i2s-codec-tx names (SAI1 on the
// i.MX RT1170 boards) and the audio codec labelled audio_codec, set up with Zephyr's audio codec API. One output
// and one input are listed, both named after the codec's node.
//
// The SAI of the i.MX RT receives in step with its transmitter (nxp,rx-sync-mode), and Zephyr's driver stops the
// transmitter whenever its queue runs dry, so recording needs the transmitter fed. One engine thread therefore runs
// the controller for both directions: each period it sends a block, of what QAudioSink queued or of silence, and
// while a QAudioSource is open it receives one. The AudioPcm objects only move bytes between the stream threads of
// Nt Multimedia and the engine's rings, converting between the format of the stream and that of the bus (stereo,
// 16 or 32-bit words).
//
// With the WM8962 codec, AudioHeadsetControl sets the bias, input pin and gain of the headset microphone and reads
// what the codec senses on its contact, writing the codec's registers over I2C beside Zephyr's driver. Zephyr's
// driver resets the codec whenever it is configured, so the settings are applied again after each configure.
#include "qaudiobackend_p.h"

#include "threaddata_p.h"

#include <zephyr/audio/codec.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>
#include <zephyr/linker/section_tags.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <vector>

#define NT_AUDIO_CODEC_NODE DT_NODELABEL(audio_codec)
#define NT_AUDIO_I2S_NODE DT_ALIAS(i2s_codec_tx)

#if DT_NODE_EXISTS(NT_AUDIO_CODEC_NODE) && DT_NODE_HAS_STATUS_OKAY(NT_AUDIO_CODEC_NODE) &&                             \
    DT_NODE_EXISTS(NT_AUDIO_I2S_NODE) && DT_NODE_HAS_STATUS_OKAY(NT_AUDIO_I2S_NODE)
#define NT_HAVE_AUDIO_CODEC 1
#else
#define NT_HAVE_AUDIO_CODEC 0
#endif

#if NT_HAVE_AUDIO_CODEC && DT_NODE_HAS_COMPAT(NT_AUDIO_CODEC_NODE, wolfson_wm8962)
#define NT_HAVE_WM8962 1
#else
#define NT_HAVE_WM8962 0
#endif

namespace QtMultimediaPrivate {

namespace {

#if NT_HAVE_AUDIO_CODEC

// Blocks of the bus: 10 ms each, so CONFIG_NT_AUDIO_BLOCK_SIZE bounds the sample rate and word size (3840 bytes hold
// 48 kHz stereo with 32-bit words). The driver keeps three receive blocks queued for its DMA besides those that wait
// to be read, so the receive slab has more.
constexpr std::size_t kBlockBytes = CONFIG_NT_AUDIO_BLOCK_SIZE;
constexpr std::size_t kTxBlocks = CONFIG_NT_AUDIO_BLOCK_COUNT;
constexpr std::size_t kRxBlocks = CONFIG_NT_AUDIO_BLOCK_COUNT + 4;
constexpr int kBusChannels = 2;
// How long a read or write of the driver waits; a stall this long means the stream has stopped.
constexpr int32_t kI2sTimeoutMs = 200;

static_assert(kBlockBytes % 32 == 0, "CONFIG_NT_AUDIO_BLOCK_SIZE must be a multiple of 32");

// The DMA reads and writes these behind the data cache, so they go in memory that is not cached.
alignas(32) char g_txSlabBuffer[kBlockBytes * kTxBlocks] __nocache;
alignas(32) char g_rxSlabBuffer[kBlockBytes * kRxBlocks] __nocache;
k_mem_slab g_txSlab;
k_mem_slab g_rxSlab;

constexpr std::array<int, 9> kSampleRates = {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000};

// A ring of bytes, in whole frames of its stream's format.
class ByteRing {
public:
    void reset(qint64 capacity) {
        m_data.assign(std::size_t(capacity), 0);
        m_head = 0;
        m_count = 0;
    }
    void clear() {
        m_head = 0;
        m_count = 0;
    }
    qint64 count() const { return m_count; }
    qint64 room() const { return qint64(m_data.size()) - m_count; }

    void push(const char *data, qint64 bytes) {
        const auto size = qint64(m_data.size());
        const qint64 tail = (m_head + m_count) % size;
        const qint64 first = std::min(bytes, size - tail);
        std::memcpy(m_data.data() + tail, data, std::size_t(first));
        if (bytes > first)
            std::memcpy(m_data.data(), data + first, std::size_t(bytes - first));
        m_count += bytes;
    }

    void pop(char *data, qint64 bytes) {
        const auto size = qint64(m_data.size());
        const qint64 first = std::min(bytes, size - m_head);
        std::memcpy(data, m_data.data() + m_head, std::size_t(first));
        if (bytes > first)
            std::memcpy(data + first, m_data.data(), std::size_t(bytes - first));
        m_head = (m_head + bytes) % size;
        m_count -= bytes;
    }

private:
    std::vector<char> m_data;
    qint64 m_head = 0;
    qint64 m_count = 0;
};

class AudioEngine;

#if NT_HAVE_WM8962

// The registers and fields of the WM8962 that are used here, from its register map.
namespace wm8962 {
constexpr uint16_t kLeftInputVolume = 0x00;
constexpr uint16_t kRightInputVolume = 0x01;
constexpr uint16_t kPowerManagement1 = 0x19;
constexpr uint16_t kLeftInputMixerVolume = 0x20;
constexpr uint16_t kRightInputMixerVolume = 0x21;
constexpr uint16_t kInputMixerControl2 = 0x22;
constexpr uint16_t kLeftInputPgaControl = 0x25;
constexpr uint16_t kRightInputPgaControl = 0x26;
constexpr uint16_t kAdditionalControl4 = 0x30;

constexpr uint16_t kInputVolumeUpdate = 0x0100; // IN_VU: latch both input volumes
constexpr uint16_t kInputVolumeMask = 0x003F;   // INL_VOL / INR_VOL: -23.25 dB + 0.75 dB a step
constexpr uint16_t kMicBiasEnable = 0x0002;     // MICBIAS_ENA in power management 1
constexpr uint16_t kPgaBoostMask = 0x0038;      // INPGAL_MIXINL_VOL / INPGAR_MIXINR_VOL
constexpr int kPgaBoostShift = 3;
constexpr uint16_t kLeftPgaToMixer = 0x0008;  // INPGAL_TO_MIXINL
constexpr uint16_t kRightPgaToMixer = 0x0001; // INPGAR_TO_MIXINR
constexpr uint16_t kPgaEnable = 0x0010;       // INPGAL_ENA / INPGAR_ENA; IN1 to IN4 are 0x8, 0x4, 0x2, 0x1
constexpr uint16_t kMicDetectEnable = 0x0200; // MICDET_ENA
constexpr uint16_t kMicDetectStatus = 0x0080; // MICDET_STS
constexpr uint16_t kMicShortStatus = 0x0040;  // MICSHORT_STS: the button shorts the microphone
constexpr uint16_t kMicBiasLevel = 0x0001;    // MICBIAS_LVL: set for the lower level

constexpr double kPgaMinimum = -23.25;
constexpr double kPgaMaximum = 24.0;
constexpr double kPgaStep = 0.75;
// The boost of the PGA into the input mixer, for codes 0 to 7.
constexpr std::array<double, 8> kBoost = {0, 6, 13, 18, 20, 24, 27, 30};
} // namespace wm8962

class Wm8962Headset : public AudioHeadsetControl {
public:
    QString codecName() const override { return QStringLiteral("WM8962"); }
    double minimumGain() const override { return wm8962::kPgaMinimum; }
    double maximumGain() const override { return wm8962::kPgaMaximum + wm8962::kBoost.back(); }

    bool apply(const AudioHeadsetSettings &settings, QString *error) override {
        const std::lock_guard<std::mutex> lock(m_mutex);
        AudioHeadsetSettings next = settings;
        if (!write(next)) {
            if (error)
                *error = QStringLiteral("cannot reach the WM8962 over I2C");
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
        uint16_t value = 0;
        if (!readRegister(wm8962::kAdditionalControl4, value))
            return -1;
        int result = 0;
        if (value & wm8962::kMicDetectStatus)
            result |= MicrophonePresent;
        if (value & wm8962::kMicShortStatus)
            result |= ButtonPressed;
        return result;
    }

    // Applies the settings again, after Zephyr's driver has reset the codec.
    void reapply() {
        const std::lock_guard<std::mutex> lock(m_mutex);
        AudioHeadsetSettings settings = m_settings;
        if (write(settings))
            m_settings = settings;
    }

private:
    // Writes the settings, rounding the gain to what the codec can do.
    bool write(AudioHeadsetSettings &s) {
        using namespace wm8962;
        const bool left = s.channel == 0;
        s.input = std::clamp(s.input, 1, 4);
        // The boost into the mixer is the coarse stage and the PGA, in steps of 0.75 dB, the fine one.
        std::vector<double> pgaSteps;
        for (int code = 0; code <= int(kInputVolumeMask); ++code)
            pgaSteps.push_back(kPgaMinimum + kPgaStep * code);
        std::size_t boost = 0;
        std::size_t pgaCode = 0;
        s.gain = splitGain(std::vector<double>(kBoost.begin(), kBoost.end()), pgaSteps,
                           std::clamp(s.gain, minimumGain(), maximumGain()), &boost, &pgaCode);
        const auto code = static_cast<uint16_t>(pgaCode);

        const auto pin = static_cast<uint16_t>(0x8U >> unsigned(s.input - 1));
        const bool bias = s.bias != 0;
        bool ok = writeRegister(left ? kLeftInputPgaControl : kRightInputPgaControl, uint16_t(kPgaEnable | pin));
        ok = ok && writeRegister(left ? kLeftInputVolume : kRightInputVolume,
                                 uint16_t(kInputVolumeUpdate | (code & kInputVolumeMask)));
        ok = ok && updateRegister(left ? kLeftInputMixerVolume : kRightInputMixerVolume, kPgaBoostMask,
                                  uint16_t(boost << kPgaBoostShift));
        ok = ok && updateRegister(kInputMixerControl2, left ? kLeftPgaToMixer : kRightPgaToMixer,
                                  left ? kLeftPgaToMixer : kRightPgaToMixer);
        ok = ok && updateRegister(kPowerManagement1, kMicBiasEnable, bias ? kMicBiasEnable : 0);
        ok = ok && updateRegister(kAdditionalControl4, uint16_t(kMicDetectEnable | kMicBiasLevel),
                                  uint16_t((bias ? kMicDetectEnable : 0) | (s.bias == 1 ? kMicBiasLevel : 0)));
        return ok;
    }

    // The WM8962 takes a 16-bit register address and 16-bit data, both most significant byte first.
    bool writeRegister(uint16_t reg, uint16_t value) {
        const uint8_t data[4] = {uint8_t(reg >> 8), uint8_t(reg & 0xff), uint8_t(value >> 8), uint8_t(value & 0xff)};
        return i2c_write_dt(&m_i2c, data, sizeof(data)) == 0;
    }

    bool readRegister(uint16_t reg, uint16_t &value) {
        const uint8_t address[2] = {uint8_t(reg >> 8), uint8_t(reg & 0xff)};
        uint8_t data[2] = {0, 0};
        if (i2c_write_read_dt(&m_i2c, address, sizeof(address), data, sizeof(data)) != 0)
            return false;
        value = uint16_t((unsigned(data[0]) << 8) | data[1]);
        return true;
    }

    bool updateRegister(uint16_t reg, uint16_t mask, uint16_t value) {
        uint16_t old = 0;
        if (!readRegister(reg, old))
            return false;
        return writeRegister(reg, uint16_t((old & ~mask) | (value & mask)));
    }

    mutable std::mutex m_mutex;
    // Not I2C_DT_SPEC_GET(), whose designated initializers are C++20.
    i2c_dt_spec m_i2c{DEVICE_DT_GET(DT_BUS(NT_AUDIO_CODEC_NODE)), DT_REG_ADDR(NT_AUDIO_CODEC_NODE)};
    AudioHeadsetSettings m_settings;
};

#endif // NT_HAVE_WM8962

// Runs the I2S controller and the codec for both directions; see the comment at the top.
class AudioEngine {
public:
    AudioEngine() {
        k_mem_slab_init(&g_txSlab, g_txSlabBuffer, kBlockBytes, kTxBlocks);
        k_mem_slab_init(&g_rxSlab, g_rxSlabBuffer, kBlockBytes, kRxBlocks);
    }

    ~AudioEngine() {
        std::unique_lock<std::mutex> lock(m_mutex);
        stopLocked(lock);
    }

    AudioEngine(const AudioEngine &) = delete;
    AudioEngine &operator=(const AudioEngine &) = delete;

    bool ready() const { return device_is_ready(m_i2s) && device_is_ready(m_codec); }

#if NT_HAVE_WM8962
    AudioHeadsetControl *headset() {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_headsetSetUp) {
            m_headsetSetUp = true;
            // The microphone bias and its sensing need the codec powered, so set it up at the preferred format.
            QString ignored;
            configureLocked(48000, 16, &ignored);
        }
        return &m_headset;
    }
#endif

    // Opens mode in format. Returns false and sets error when the format does not suit the bus as it runs.
    bool open(QAudioDevice::Mode mode, const QAudioFormat &format, QString *error) {
        std::unique_lock<std::mutex> lock(m_mutex);
        Direction &dir = direction(mode);
        // A stream that was closed with drain lets go once its last blocks have played.
        m_wake.wait_for(lock, std::chrono::milliseconds(kI2sTimeoutMs), [&] { return !dir.open; });
        if (dir.open) {
            *error = mode == QAudioDevice::Output ? QStringLiteral("the codec's output is in use")
                                                  : QStringLiteral("the codec's input is in use");
            return false;
        }
        const int word = format.sampleFormat() == QAudioFormat::Int32 ? 32 : 16;
        if (!configureLocked(format.sampleRate(), word, error))
            return false;
        dir.format = format;
        dir.ring.reset(4 * framesPerBlock() * format.bytesPerFrame());
        dir.paused = false;
        dir.open = true;
        if (!m_running && !startLocked(error)) {
            dir.open = false;
            return false;
        }
        m_wake.notify_all();
        return true;
    }

    void close(QAudioDevice::Mode mode) {
        std::unique_lock<std::mutex> lock(m_mutex);
        Direction &dir = direction(mode);
        dir.open = false;
        dir.ring.clear();
        m_wake.notify_all();
        if (!m_output.open && !m_input.open)
            stopLocked(lock);
    }

    // Queues all of data for playing, blocking while the ring is full.
    qint64 write(const char *data, qint64 bytes) {
        std::unique_lock<std::mutex> lock(m_mutex);
        const qint64 frame = m_output.format.bytesPerFrame();
        qint64 done = 0;
        while (done < bytes) {
            m_wake.wait(lock, [&] { return m_failed || !m_running || m_output.ring.room() >= frame; });
            if (m_failed || !m_running)
                return -1;
            const qint64 n = std::min(bytes - done, m_output.ring.room()) / frame * frame;
            m_output.ring.push(data + done, n);
            done += n;
        }
        return bytes;
    }

    // Takes up to bytes of what was recorded, waiting a while for some.
    qint64 read(char *data, qint64 bytes) {
        std::unique_lock<std::mutex> lock(m_mutex);
        const qint64 frame = m_input.format.bytesPerFrame();
        m_wake.wait_for(lock, std::chrono::milliseconds(kI2sTimeoutMs),
                        [&] { return m_failed || !m_running || m_input.ring.count() >= frame; });
        if (m_failed || !m_running)
            return -1;
        const qint64 n = std::min(bytes, m_input.ring.count()) / frame * frame;
        m_input.ring.pop(data, n);
        return n;
    }

    void setPaused(QAudioDevice::Mode mode, bool paused) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        Direction &dir = direction(mode);
        dir.paused = paused;
        // Recording goes on while paused; what it caught then is stale.
        if (mode == QAudioDevice::Input && !paused)
            dir.ring.clear();
    }

    void drop(QAudioDevice::Mode mode) {
        const std::lock_guard<std::mutex> lock(m_mutex);
        direction(mode).ring.clear();
        m_wake.notify_all();
    }

    void drain() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_wake.wait(lock, [&] { return m_failed || !m_running || m_output.paused || m_output.ring.count() == 0; });
    }

private:
    struct Direction {
        bool open = false;
        bool paused = false;
        QAudioFormat format;
        ByteRing ring;
    };

    Direction &direction(QAudioDevice::Mode mode) { return mode == QAudioDevice::Output ? m_output : m_input; }

    qint64 framesPerBlock() const { return std::max(m_rate / 100, 1); }
    qint64 busBlockBytes() const { return framesPerBlock() * kBusChannels * (m_word / 8); }

    // Sets up the controller and the codec for rate and word, unless they run so already. Fails when a stream runs
    // in another format.
    bool configureLocked(int rate, int word, QString *error) {
        if (m_configured && m_rate == rate && m_word == word)
            return true;
        if (m_running) {
            *error = QStringLiteral("the codec runs at %1 Hz with %2-bit samples for the other stream")
                         .arg(m_rate)
                         .arg(m_word);
            return false;
        }
        if (std::find(kSampleRates.begin(), kSampleRates.end(), rate) == kSampleRates.end()) {
            *error = QStringLiteral("the codec does not run at %1 Hz").arg(rate);
            return false;
        }
        const int oldRate = m_rate;
        const int oldWord = m_word;
        m_rate = rate;
        m_word = word;
        if (std::size_t(busBlockBytes()) > kBlockBytes) {
            *error = QStringLiteral("%1 Hz with %2-bit samples needs CONFIG_NT_AUDIO_BLOCK_SIZE of %3 or more")
                         .arg(rate)
                         .arg(word)
                         .arg(busBlockBytes());
            m_rate = oldRate;
            m_word = oldWord;
            return false;
        }

        audio_codec_cfg codecConfig{};
        codecConfig.dai_type = AUDIO_DAI_TYPE_I2S;
        codecConfig.dai_route = AUDIO_ROUTE_PLAYBACK_CAPTURE;
        codecConfig.dai_cfg.i2s.word_size = uint8_t(word);
        codecConfig.dai_cfg.i2s.channels = kBusChannels;
        codecConfig.dai_cfg.i2s.format = I2S_FMT_DATA_FORMAT_I2S;
        // The SAI drives the bit and frame clocks and MCLK; the codec follows them.
        codecConfig.dai_cfg.i2s.options = I2S_OPT_FRAME_CLK_TARGET | I2S_OPT_BIT_CLK_TARGET;
        codecConfig.dai_cfg.i2s.frame_clk_freq = uint32_t(rate);
        codecConfig.dai_cfg.i2s.mem_slab = &g_txSlab;
        codecConfig.dai_cfg.i2s.block_size = std::size_t(busBlockBytes());
        int err = audio_codec_configure(m_codec, &codecConfig);
        if (err == 0) {
            audio_codec_start_output(m_codec);
            err = configureI2s(I2S_DIR_TX, &g_txSlab);
        }
        if (err == 0)
            err = configureI2s(I2S_DIR_RX, &g_rxSlab);
        if (err != 0) {
            *error = QStringLiteral("cannot set up the codec for %1 Hz with %2-bit samples (error %3)")
                         .arg(rate)
                         .arg(word)
                         .arg(err);
            m_configured = false;
            return false;
        }
        m_configured = true;
#if NT_HAVE_WM8962
        m_headset.reapply();
#endif
        return true;
    }

    int configureI2s(i2s_dir dir, k_mem_slab *slab) {
        i2s_config config{};
        config.word_size = uint8_t(m_word);
        config.channels = kBusChannels;
        config.format = I2S_FMT_DATA_FORMAT_I2S;
        config.options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
        config.frame_clk_freq = uint32_t(m_rate);
        config.mem_slab = slab;
        config.block_size = std::size_t(busBlockBytes());
        config.timeout = kI2sTimeoutMs;
        return i2s_configure(m_i2s, dir, &config);
    }

    bool startLocked(QString *error) {
        m_stop = false;
        m_failed = false;
        m_running = true;
        if (!QtPrivate::startDetachedThread([this] { run(); })) {
            m_running = false;
            *error = QStringLiteral("cannot start the audio thread");
            return false;
        }
        return true;
    }

    void stopLocked(std::unique_lock<std::mutex> &lock) {
        if (!m_threadAlive && !m_running)
            return;
        m_stop = true;
        m_wake.notify_all();
        m_wake.wait(lock, [this] { return !m_threadAlive && !m_running; });
    }

    // Fills a bus block from the output ring, or with silence.
    void fillBusBlock(std::vector<char> &bus, std::vector<char> &frames) {
        std::fill(bus.begin(), bus.end(), 0);
        if (!m_output.open || m_output.paused)
            return;
        const qint64 frameBytes = m_output.format.bytesPerFrame();
        const qint64 want = framesPerBlock() * frameBytes;
        const qint64 n = std::min(want, m_output.ring.count()) / frameBytes * frameBytes;
        if (n <= 0)
            return;
        frames.resize(std::size_t(n));
        m_output.ring.pop(frames.data(), n);
        const int channels = m_output.format.channelCount();
        const int bytes = m_word / 8;
        for (qint64 f = 0; f < n / frameBytes; ++f) {
            char *out = bus.data() + f * kBusChannels * bytes;
            const char *in = frames.data() + f * frameBytes;
            std::memcpy(out, in, std::size_t(bytes));
            std::memcpy(out + bytes, channels > 1 ? in + bytes : in, std::size_t(bytes));
        }
    }

    // Moves a received bus block into the input ring; with one channel, the one the headset microphone is on.
    void takeBusBlock(const std::vector<char> &bus, std::size_t size, std::vector<char> &frames) {
        if (!m_input.open)
            return;
        const int bytes = m_word / 8;
        const int channels = m_input.format.channelCount();
        const qint64 count = qint64(size) / (kBusChannels * bytes);
        int pick = 1;
#if NT_HAVE_WM8962
        pick = m_headset.settings().channel;
#endif
        frames.resize(std::size_t(count * channels * bytes));
        for (qint64 f = 0; f < count; ++f) {
            const char *in = bus.data() + f * kBusChannels * bytes;
            char *out = frames.data() + f * channels * bytes;
            if (channels > 1)
                std::memcpy(out, in, std::size_t(2 * bytes));
            else
                std::memcpy(out, in + pick * bytes, std::size_t(bytes));
        }
        // What does not fit is lost, as when a device overruns.
        const qint64 frameBytes = channels * bytes;
        const qint64 n = std::min(qint64(frames.size()), m_input.ring.room()) / frameBytes * frameBytes;
        m_input.ring.push(frames.data(), n);
    }

    void run() {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_threadAlive = true;
        }
        std::vector<char> bus(static_cast<std::size_t>(busBlockBytes()));
        std::vector<char> frames;
        bool txStarted = false;
        bool rxStarted = false;
        int primed = 0;
        int failures = 0;
        for (;;) {
            bool wantInput = false;
            {
                const std::lock_guard<std::mutex> lock(m_mutex);
                if (m_stop)
                    break;
                fillBusBlock(bus, frames);
                wantInput = m_input.open;
            }
            m_wake.notify_all();

            // Blocks while the driver's queue is full, which paces the loop at one block a period.
            int err = i2s_buf_write(m_i2s, bus.data(), bus.size());
            if (err != 0) {
                // The transmitter ran dry and stopped, or failed: start it again from scratch.
                i2s_trigger(m_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP);
                if (rxStarted)
                    i2s_trigger(m_i2s, I2S_DIR_RX, I2S_TRIGGER_DROP);
                txStarted = rxStarted = false;
                primed = 0;
                if (++failures > 10) {
                    const std::lock_guard<std::mutex> lock(m_mutex);
                    m_failed = true;
                    break;
                }
                continue;
            }
            // Two blocks queued before starting, so that the transmitter does not run dry at once.
            if (!txStarted && ++primed >= 2) {
                txStarted = i2s_trigger(m_i2s, I2S_DIR_TX, I2S_TRIGGER_START) == 0;
                primed = 0;
            }
            if (!txStarted)
                continue;
            if (wantInput && !rxStarted) {
                rxStarted = i2s_trigger(m_i2s, I2S_DIR_RX, I2S_TRIGGER_START) == 0;
            } else if (!wantInput && rxStarted) {
                i2s_trigger(m_i2s, I2S_DIR_RX, I2S_TRIGGER_DROP);
                rxStarted = false;
            }
            if (rxStarted) {
                std::size_t size = bus.size();
                err = i2s_buf_read(m_i2s, bus.data(), &size);
                if (err == 0) {
                    const std::lock_guard<std::mutex> lock(m_mutex);
                    takeBusBlock(bus, size, frames);
                } else {
                    // An overrun or a stall: start the receiver again with the next block.
                    i2s_trigger(m_i2s, I2S_DIR_RX, I2S_TRIGGER_DROP);
                    rxStarted = false;
                }
            }
            failures = 0;
        }
        if (txStarted)
            i2s_trigger(m_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP);
        if (rxStarted)
            i2s_trigger(m_i2s, I2S_DIR_RX, I2S_TRIGGER_DROP);
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_threadAlive = false;
        m_running = false;
        // Notified under the lock, so that the engine cannot go before this is done.
        m_wake.notify_all();
    }

    const device *m_i2s = DEVICE_DT_GET(NT_AUDIO_I2S_NODE);
    const device *m_codec = DEVICE_DT_GET(NT_AUDIO_CODEC_NODE);

    std::mutex m_mutex;
    std::condition_variable m_wake;
    Direction m_output;
    Direction m_input;
    int m_rate = 48000;
    int m_word = 16;
    bool m_configured = false;
    bool m_running = false;     // a stream is open and the thread was started
    bool m_threadAlive = false; // the thread has not finished
    bool m_stop = false;
    bool m_failed = false;
#if NT_HAVE_WM8962
    Wm8962Headset m_headset;
    bool m_headsetSetUp = false;
#endif
};

class ZephyrPcm : public AudioPcm {
public:
    ZephyrPcm(AudioEngine *engine, QAudioDevice::Mode mode) : m_engine(engine), m_mode(mode) {}
    ~ZephyrPcm() override { m_engine->close(m_mode); }
    ZephyrPcm(const ZephyrPcm &) = delete;
    ZephyrPcm &operator=(const ZephyrPcm &) = delete;

    qint64 write(const char *data, qint64 bytes) override { return m_engine->write(data, bytes); }
    qint64 read(char *data, qint64 bytes) override { return m_engine->read(data, bytes); }
    void setPaused(bool paused) override { m_engine->setPaused(m_mode, paused); }
    void drop() override { m_engine->drop(m_mode); }
    void drain() override { m_engine->drain(); }

private:
    AudioEngine *m_engine;
    QAudioDevice::Mode m_mode;
};

class ZephyrBackend : public AudioBackend {
public:
    QList<QAudioDevice> devices(QAudioDevice::Mode mode) override {
        QList<QAudioDevice> list;
        if (mode == QAudioDevice::Null || !m_engine.ready())
            return list;
        QAudioDevicePrivate info;
        info.id = QByteArray(DT_NODE_FULL_NAME(NT_AUDIO_CODEC_NODE));
        info.mode = mode;
        info.isDefault = true;
        info.minimumSampleRate = kSampleRates.front();
        info.maximumSampleRate = kSampleRates.back();
        info.minimumChannelCount = 1;
        info.maximumChannelCount = kBusChannels;
        info.sampleFormats = {QAudioFormat::Int16, QAudioFormat::Int32};
        info.preferredFormat.setSampleRate(48000);
        info.preferredFormat.setSampleFormat(QAudioFormat::Int16);
        const QString codec = NT_HAVE_WM8962 ? QStringLiteral("WM8962") : QStringLiteral("Audio codec");
        if (mode == QAudioDevice::Output) {
            info.description = codec + QStringLiteral(" headphones");
            info.preferredFormat.setChannelCount(2);
            info.channelConfiguration = QAudioFormat::ChannelConfigStereo;
        } else {
            info.description = codec + QStringLiteral(" microphone");
            info.preferredFormat.setChannelCount(1);
            info.channelConfiguration = QAudioFormat::ChannelConfigMono;
        }
        list.append(makeAudioDevice(info));
        return list;
    }

    std::unique_ptr<AudioPcm> open(const QByteArray &id, QAudioDevice::Mode mode, const QAudioFormat &format,
                                   qint64 periodBytes, QString *error) override {
        (void)periodBytes;
        if (!m_engine.ready()) {
            *error = QStringLiteral("the audio codec or its I2S controller is not ready");
            return nullptr;
        }
        if (!id.isEmpty() && id != QByteArray(DT_NODE_FULL_NAME(NT_AUDIO_CODEC_NODE))) {
            *error = QStringLiteral("no audio device is called %1").arg(QString::fromUtf8(id));
            return nullptr;
        }
        const QAudioFormat::SampleFormat sample = format.sampleFormat();
        if (!format.isValid() || (sample != QAudioFormat::Int16 && sample != QAudioFormat::Int32) ||
            format.channelCount() < 1 || format.channelCount() > kBusChannels) {
            *error = QStringLiteral("the codec plays and records one or two channels of Int16 or Int32 samples");
            return nullptr;
        }
        if (mode != QAudioDevice::Output && mode != QAudioDevice::Input) {
            *error = QStringLiteral("unsupported audio mode");
            return nullptr;
        }
        if (!m_engine.open(mode, format, error))
            return nullptr;
        return std::make_unique<ZephyrPcm>(&m_engine, mode);
    }

    AudioHeadsetControl *headsetControl() override {
#if NT_HAVE_WM8962
        return m_engine.ready() ? m_engine.headset() : nullptr;
#else
        return nullptr;
#endif
    }

private:
    AudioEngine m_engine;
};

#else // NT_HAVE_AUDIO_CODEC

// The devicetree names no audio codec, or no I2S controller for it, as the alias i2s-codec-tx.
class ZephyrBackend : public AudioBackend {
public:
    QList<QAudioDevice> devices(QAudioDevice::Mode) override { return {}; }
    std::unique_ptr<AudioPcm> open(const QByteArray &, QAudioDevice::Mode, const QAudioFormat &, qint64,
                                   QString *error) override {
        *error = QStringLiteral("the devicetree has no audio_codec node and i2s-codec-tx alias");
        return nullptr;
    }
};

#endif // NT_HAVE_AUDIO_CODEC

} // namespace

std::unique_ptr<AudioBackend> createPlatformAudioBackend() { return std::make_unique<ZephyrBackend>(); }

} // namespace QtMultimediaPrivate
