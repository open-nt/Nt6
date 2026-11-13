// MIT License - see LICENSE
//
// Nt Multimedia is tested against a fake backend: its output devices record what they are given
// and play it at four times real time, and its input device records a counting pattern. So the
// streams, their worker threads, the timers that feed them and the WAV decoding all run for real,
// without sound hardware. The ALSA backend is only checked to list devices without failing.
#include <QtCore>
#include <QtMultimedia>

#include "qaudiobackend_p.h"
#include "qwavedecoder_p.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
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

namespace {

struct FakeWorld {
    std::mutex mutex;
    QByteArray played;
    QByteArray openedId;
    QAudioFormat openedFormat;
    QString directory;
    bool failOpen = false;
    bool secondOutput = false;
    std::atomic<int> pauses{0};
    std::atomic<int> resumes{0};
    std::atomic<int> drains{0};
    std::atomic<int> drops{0};
    std::atomic<int> failAfterWrites{-1};

    QByteArray takePlayed() {
        const std::lock_guard<std::mutex> lock(mutex);
        return std::exchange(played, QByteArray());
    }
    qsizetype playedSize() {
        const std::lock_guard<std::mutex> lock(mutex);
        return played.size();
    }
};

FakeWorld *g_world = nullptr;

class FakePcm : public AudioPcm {
public:
    explicit FakePcm(const QAudioFormat &format) : m_format(format) {}

    qint64 write(const char *data, qint64 bytes) override {
        if (g_world->failAfterWrites.load() == 0)
            return -1;
        if (g_world->failAfterWrites.load() > 0)
            --g_world->failAfterWrites;
        {
            const std::lock_guard<std::mutex> lock(g_world->mutex);
            g_world->played.append(data, qsizetype(bytes));
        }
        sleepFor(bytes);
        return bytes;
    }

    qint64 read(char *data, qint64 bytes) override {
        for (qint64 i = 0; i < bytes; ++i)
            data[i] = char(m_counter++ & 0x7f);
        sleepFor(bytes);
        return bytes;
    }

    void setPaused(bool paused) override { ++(paused ? g_world->pauses : g_world->resumes); }
    void drop() override { ++g_world->drops; }
    void drain() override { ++g_world->drains; }

private:
    void sleepFor(qint64 bytes) {
        const qint64 usecs = m_format.durationForBytes(qint32(bytes)) / 4;
        std::this_thread::sleep_for(std::chrono::microseconds(std::max<qint64>(usecs, 200)));
    }

    QAudioFormat m_format;
    quint32 m_counter = 0;
};

class FakeBackend : public AudioBackend {
public:
    QList<QAudioDevice> devices(QAudioDevice::Mode mode) override {
        QList<QAudioDevice> list;
        QAudioDevicePrivate info;
        info.mode = mode;
        info.preferredFormat.setSampleRate(48000);
        info.preferredFormat.setChannelCount(2);
        info.preferredFormat.setSampleFormat(QAudioFormat::Int16);
        info.maximumSampleRate = 96000;
        if (mode == QAudioDevice::Output) {
            info.id = "fake-out";
            info.description = QStringLiteral("Fake speakers");
            info.isDefault = true;
            list.append(makeAudioDevice(info));
            if (g_world->secondOutput) {
                info.id = "fake-out-2";
                info.description = QStringLiteral("Fake headphones");
                info.isDefault = false;
                list.append(makeAudioDevice(info));
            }
        } else if (mode == QAudioDevice::Input) {
            info.id = "fake-in";
            info.description = QStringLiteral("Fake microphone");
            info.isDefault = true;
            info.sampleFormats = {QAudioFormat::Int16};
            list.append(makeAudioDevice(info));
        }
        return list;
    }

    std::unique_ptr<AudioPcm> open(const QByteArray &id, QAudioDevice::Mode, const QAudioFormat &format, qint64,
                                   QString *error) override {
        if (g_world->failOpen) {
            *error = QStringLiteral("refused");
            return nullptr;
        }
        g_world->openedId = id;
        g_world->openedFormat = format;
        return std::make_unique<FakePcm>(format);
    }

    QString deviceDirectory() const override { return g_world->directory; }
};

QAudioFormat stereo16(int rate = 8000) {
    QAudioFormat format;
    format.setSampleRate(rate);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);
    return format;
}

// count frames of stereo 16-bit samples: left counts up, right counts down.
QByteArray stereoPattern(int count) {
    QByteArray data(count * 4, Qt::Uninitialized);
    for (int i = 0; i < count; ++i) {
        const auto left = qint16(i);
        const auto right = qint16(-i);
        std::memcpy(data.data() + i * 4, &left, 2);
        std::memcpy(data.data() + i * 4 + 2, &right, 2);
    }
    return data;
}

void put16(QByteArray &b, quint16 v) { b.append(char(v & 0xff)).append(char(v >> 8)); }
void put32(QByteArray &b, quint32 v) {
    put16(b, quint16(v & 0xffff));
    put16(b, quint16(v >> 16));
}

QByteArray wavFile(quint16 tag, int channels, int rate, int bits, const QByteArray &samples, bool extraChunk = false) {
    QByteArray b("RIFF");
    put32(b, 0);
    b.append("WAVE");
    if (extraChunk) {
        b.append("LIST");
        put32(b, 3);
        b.append("abc").append('\0'); // odd size, padded
    }
    b.append("fmt ");
    put32(b, 16);
    put16(b, tag);
    put16(b, quint16(channels));
    put32(b, quint32(rate));
    put32(b, quint32(rate * channels * bits / 8));
    put16(b, quint16(channels * bits / 8));
    put16(b, quint16(bits));
    b.append("data");
    put32(b, quint32(samples.size()));
    b.append(samples);
    return b;
}

QString writeFile(const QTemporaryDir &dir, const QString &name, const QByteArray &data) {
    const QString path = dir.filePath(name);
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(data);
    return path;
}

void testFormat() {
    QAudioFormat format;
    CHECK(!format.isValid());
    CHECK(format.bytesPerFrame() == 0);
    format = stereo16(48000);
    CHECK(format.isValid());
    CHECK(format.bytesPerSample() == 2);
    CHECK(format.bytesPerFrame() == 4);
    CHECK(format.bytesForDuration(20000) == 3840);
    CHECK(format.durationForBytes(3840) == 20000);
    CHECK(format.framesForBytes(3843) == 960);
    CHECK(format.bytesForFrames(960) == 3840);
    CHECK(format.framesForDuration(1000000) == 48000);
    CHECK(format.durationForFrames(48) == 1000);
    CHECK(format.channelConfig() == QAudioFormat::ChannelConfigUnknown);
    CHECK(format.channelOffset(QAudioFormat::FrontRight) == 1);
    CHECK(format.channelOffset(QAudioFormat::LFE) == -1);

    format.setChannelConfig(QAudioFormat::ChannelConfigSurround5Dot1);
    CHECK(format.channelCount() == 6);
    CHECK(format.channelOffset(QAudioFormat::FrontLeft) == 0);
    CHECK(format.channelOffset(QAudioFormat::LFE) == 3);
    CHECK(format.channelOffset(QAudioFormat::BackRight) == 5);
    format.setChannelCount(1);
    CHECK(format.channelConfig() == QAudioFormat::ChannelConfigUnknown);
    CHECK(format.channelOffset(QAudioFormat::FrontCenter) == 0);
    constexpr auto quad = QAudioFormat::channelConfig(QAudioFormat::FrontLeft, QAudioFormat::FrontRight,
                                                      QAudioFormat::BackLeft, QAudioFormat::BackRight);
    CHECK(QAudioFormat::defaultChannelConfigForChannelCount(4) == quad);
    CHECK(QAudioFormat::defaultChannelConfigForChannelCount(2) == QAudioFormat::ChannelConfigStereo);
    CHECK(QAudioFormat::defaultChannelConfigForChannelCount(9) == QAudioFormat::ChannelConfigUnknown);

    CHECK(stereo16() == stereo16());
    CHECK(stereo16() != stereo16(44100));

    QAudioFormat f;
    f.setSampleFormat(QAudioFormat::Int16);
    const qint16 s16 = -16384;
    CHECK(f.normalizedSampleValue(&s16) == -0.5f);
    f.setSampleFormat(QAudioFormat::UInt8);
    const quint8 u8 = 192;
    CHECK(f.normalizedSampleValue(&u8) == 0.5f);
    f.setSampleFormat(QAudioFormat::Float);
    const float fl = 0.25f;
    CHECK(f.normalizedSampleValue(&fl) == 0.25f);
    f.setSampleFormat(QAudioFormat::Int32);
    const qint32 s32 = 1 << 30;
    CHECK(f.normalizedSampleValue(&s32) == 0.5f);
}

void testVolume() {
    using namespace QtAudio;
    CHECK(convertVolume(0.5f, LinearVolumeScale, LinearVolumeScale) == 0.5f);
    CHECK(std::fabs(convertVolume(0.125f, LinearVolumeScale, CubicVolumeScale) - 0.5f) < 1e-6f);
    CHECK(std::fabs(convertVolume(0.5f, CubicVolumeScale, LinearVolumeScale) - 0.125f) < 1e-6f);
    CHECK(std::fabs(convertVolume(0.1f, LinearVolumeScale, DecibelVolumeScale) + 20.0f) < 1e-4f);
    CHECK(std::fabs(convertVolume(-20.0f, DecibelVolumeScale, LinearVolumeScale) - 0.1f) < 1e-6f);
    CHECK(convertVolume(0.0f, LinearVolumeScale, DecibelVolumeScale) == -200.0f);
    const float log = convertVolume(0.3f, LinearVolumeScale, LogarithmicVolumeScale);
    CHECK(std::fabs(convertVolume(log, LogarithmicVolumeScale, LinearVolumeScale) - 0.3f) < 1e-5f);
    CHECK(convertVolume(-1.0f, LinearVolumeScale, CubicVolumeScale) == 0.0f);
    CHECK(int(QAudio::IdleState) == int(QtAudio::IdleState));
}

void testDevices() {
    const QList<QAudioDevice> outputs = QMediaDevices::audioOutputs();
    CHECK(outputs.size() == 1);
    const QAudioDevice out = QMediaDevices::defaultAudioOutput();
    CHECK(!out.isNull());
    CHECK(out.id() == "fake-out");
    CHECK(out.description() == QStringLiteral("Fake speakers"));
    CHECK(out.isDefault());
    CHECK(out.mode() == QAudioDevice::Output);
    CHECK(out == outputs.first());
    CHECK(out.preferredFormat() == stereo16(48000));
    CHECK(out.isFormatSupported(stereo16(48000)));
    CHECK(!out.isFormatSupported(stereo16(192000)));
    CHECK(!out.isFormatSupported(QAudioFormat()));
    CHECK(out.maximumSampleRate() == 96000);
    CHECK(out.supportedSampleFormats().size() == 4);
    CHECK(out.channelConfiguration() == QAudioFormat::ChannelConfigStereo);

    const QAudioDevice in = QMediaDevices::defaultAudioInput();
    CHECK(in.mode() == QAudioDevice::Input);
    CHECK(in != out);
    QAudioFormat floatFormat = stereo16();
    floatFormat.setSampleFormat(QAudioFormat::Float);
    CHECK(!in.isFormatSupported(floatFormat));

    QAudioDevice null;
    CHECK(null.isNull());
    CHECK(null.mode() == QAudioDevice::Null);
    CHECK(null.id().isEmpty());
    CHECK(null == QAudioDevice());
    CHECK(null != out);
    QAudioDevice copy = out;
    copy.swap(null);
    CHECK(null == out && copy.isNull());

    // Plugging a device in is noticed through the device directory.
    QMediaDevices devices;
    int outputChanges = 0;
    int inputChanges = 0;
    QObject::connect(&devices, &QMediaDevices::audioOutputsChanged, [&] { ++outputChanges; });
    QObject::connect(&devices, &QMediaDevices::audioInputsChanged, [&] { ++inputChanges; });
    g_world->secondOutput = true;
    QFile node(g_world->directory + QStringLiteral("/pcmC1D0p"));
    CHECK(node.open(QIODevice::WriteOnly));
    node.close();
    CHECK(waitFor([&] { return outputChanges == 1; }));
    CHECK(inputChanges == 0);
    CHECK(QMediaDevices::audioOutputs().size() == 2);
    CHECK(QMediaDevices::defaultAudioOutput().id() == "fake-out");
    g_world->secondOutput = false;
    node.remove();
    CHECK(waitFor([&] { return outputChanges == 2; }));
}

void testSinkPull() {
    g_world->takePlayed();
    const QByteArray data = stereoPattern(4000); // half a second
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);

    QAudioSink sink(stereo16());
    CHECK(!sink.isNull());
    CHECK(sink.format() == stereo16());
    CHECK(sink.state() == QtAudio::StoppedState);
    CHECK(sink.bufferSize() == stereo16().bytesForDuration(100000));
    QList<QtAudio::State> states;
    QObject::connect(&sink, &QAudioSink::stateChanged, [&](QtAudio::State s) { states.append(s); });
    sink.start(&buffer);
    CHECK(sink.state() == QtAudio::ActiveState);
    CHECK(sink.error() == QtAudio::NoError);
    CHECK(g_world->openedId == "fake-out");
    CHECK(g_world->openedFormat == stereo16());
    CHECK(waitFor([&] { return sink.state() == QtAudio::IdleState; }));
    CHECK(sink.error() == QtAudio::NoError); // the source ended, it did not underrun
    CHECK(g_world->takePlayed() == data);
    CHECK(sink.processedUSecs() == 500000);
    CHECK(sink.elapsedUSecs() > 0);
    CHECK(states == (QList<QtAudio::State>{QtAudio::ActiveState, QtAudio::IdleState}));
    const int drains = g_world->drains.load();
    sink.stop();
    CHECK(sink.state() == QtAudio::StoppedState);
    CHECK(waitFor([&] { return g_world->drains.load() == drains + 1; })); // the tail still plays
    CHECK(sink.processedUSecs() == 0);
    CHECK(sink.elapsedUSecs() == 0);

    // The volume scales the samples.
    QBuffer loud;
    loud.setData(stereoPattern(800));
    loud.open(QIODevice::ReadOnly);
    sink.setVolume(0.5);
    CHECK(sink.volume() == 0.5);
    sink.setVolume(2.0);
    CHECK(sink.volume() == 1.0);
    sink.setVolume(0.5);
    sink.start(&loud);
    CHECK(waitFor([&] { return sink.state() == QtAudio::IdleState; }));
    const QByteArray quiet = g_world->takePlayed();
    CHECK(quiet.size() == 3200);
    qint16 sample = 0;
    std::memcpy(&sample, quiet.constData() + 400 * 4, 2);
    CHECK(sample == 200);
    std::memcpy(&sample, quiet.constData() + 400 * 4 + 2, 2);
    CHECK(sample == -200);
    sink.reset();
    CHECK(sink.state() == QtAudio::StoppedState);
}

void testSinkPush() {
    g_world->takePlayed();
    QAudioSink sink(QMediaDevices::defaultAudioOutput(), stereo16());
    sink.setBufferSize(1000); // rounded down to whole frames
    QIODevice *io = sink.start();
    CHECK(io);
    CHECK(io->isWritable());
    CHECK(sink.bufferSize() == 1000);
    CHECK(sink.state() == QtAudio::IdleState);
    CHECK(sink.bytesFree() == 1000);
    const QByteArray data = stereoPattern(2000);
    qsizetype written = 0;
    CHECK(io->write(data.constData(), 3) == 0); // less than a frame
    while (written < data.size()) {
        const qint64 n = io->write(data.constData() + written, data.size() - written);
        CHECK(n % 4 == 0);
        written += qsizetype(n);
        if (written < data.size())
            spinEvents(2);
        CHECK(sink.state() == QtAudio::ActiveState);
    }
    CHECK(waitFor([&] { return sink.state() == QtAudio::IdleState; }));
    CHECK(sink.error() == QtAudio::UnderrunError);
    CHECK(g_world->takePlayed() == data);
    CHECK(io->write(data.constData(), 400) == 400);
    CHECK(sink.state() == QtAudio::ActiveState);
    sink.stop();
    CHECK(sink.bytesFree() == 0);
}

void testSinkSuspendAndErrors() {
    g_world->takePlayed();
    QBuffer buffer;
    buffer.setData(stereoPattern(8000));
    buffer.open(QIODevice::ReadOnly);
    QAudioSink sink(stereo16());
    sink.start(&buffer);
    CHECK(waitFor([&] { return g_world->playedSize() > 0; }));
    const int pauses = g_world->pauses.load();
    sink.suspend();
    CHECK(sink.state() == QtAudio::SuspendedState);
    CHECK(waitFor([&] { return g_world->pauses.load() == pauses + 1; }));
    spinEvents(30);
    const qsizetype held = g_world->playedSize();
    spinEvents(60);
    CHECK(g_world->playedSize() == held); // nothing plays while suspended
    sink.resume();
    CHECK(sink.state() == QtAudio::ActiveState);
    CHECK(waitFor([&] { return g_world->resumes.load() > 0; }));
    CHECK(waitFor([&] { return sink.state() == QtAudio::IdleState; }));
    CHECK(g_world->takePlayed().size() == 32000);
    const int drops = g_world->drops.load();
    sink.reset();
    CHECK(g_world->drops.load() == drops + 1);

    // A device that cannot be opened.
    g_world->failOpen = true;
    buffer.seek(0);
    sink.start(&buffer);
    CHECK(sink.state() == QtAudio::StoppedState);
    CHECK(sink.error() == QtAudio::OpenError);
    CHECK(sink.start() == nullptr);
    g_world->failOpen = false;

    // A null device, an invalid format, a source that is not readable.
    QAudioSink nowhere(QAudioDevice(), stereo16());
    CHECK(nowhere.isNull());
    nowhere.start(&buffer);
    CHECK(nowhere.error() == QtAudio::OpenError);
    QAudioSink noFormat;
    noFormat.start(&buffer);
    CHECK(noFormat.error() == QtAudio::OpenError);
    QBuffer closed;
    sink.start(&closed);
    CHECK(sink.error() == QtAudio::OpenError);

    // A device that fails while playing.
    g_world->failAfterWrites = 2;
    buffer.seek(0);
    sink.start(&buffer);
    CHECK(waitFor([&] { return sink.state() == QtAudio::StoppedState; }));
    CHECK(sink.error() == QtAudio::IOError);
    g_world->failAfterWrites = -1;
    g_world->takePlayed();
}

void testSource() {
    QAudioFormat mono;
    mono.setSampleRate(8000);
    mono.setChannelCount(1);
    mono.setSampleFormat(QAudioFormat::Int16);

    // Pull mode: recorded samples go into the buffer.
    QBuffer recording;
    recording.open(QIODevice::WriteOnly);
    QAudioSource source(mono);
    CHECK(!source.isNull());
    CHECK(source.format() == mono);
    source.start(&recording);
    CHECK(source.state() == QtAudio::ActiveState);
    CHECK(g_world->openedId == "fake-in");
    CHECK(waitFor([&] { return recording.data().size() >= 2000; }));
    source.stop();
    CHECK(source.state() == QtAudio::StoppedState);
    const QByteArray got = recording.data();
    bool counting = got.size() >= 2000;
    for (qsizetype i = 0; counting && i < got.size(); ++i)
        counting = got.at(i) == char(i & 0x7f);
    CHECK(counting);
    CHECK(source.processedUSecs() == 0);

    // Push mode: readyRead() reports samples.
    QIODevice *io = source.start();
    CHECK(io && io->isReadable());
    QByteArray pushed;
    QObject::connect(io, &QIODevice::readyRead, [&] { pushed += io->readAll(); });
    CHECK(waitFor([&] { return pushed.size() >= 2000; }));
    CHECK(pushed.at(5) == char(5));
    CHECK(source.processedUSecs() > 0);
    CHECK(source.elapsedUSecs() > 0);
    source.suspend();
    CHECK(source.state() == QtAudio::SuspendedState);
    source.resume();
    CHECK(source.state() == QtAudio::ActiveState);
    source.setVolume(0.5);
    CHECK(source.volume() == 0.5);
    source.reset();
    source.stop();
    CHECK(source.bytesAvailable() == 0);

    QAudioSource none(QAudioDevice(), mono);
    QBuffer sinkBuffer;
    sinkBuffer.open(QIODevice::WriteOnly);
    none.start(&sinkBuffer);
    CHECK(none.error() == QtAudio::OpenError);
}

void testWave(const QTemporaryDir &dir) {
    // 24-bit samples become 32-bit; an odd-sized chunk before "fmt " is skipped.
    QByteArray samples;
    for (int i = 0; i < 4; ++i) {
        const qint32 v = (i - 2) * 1000;
        samples.append(char(v & 0xff)).append(char((v >> 8) & 0xff)).append(char((v >> 16) & 0xff));
    }
    QBuffer device;
    device.setData(wavFile(1, 1, 22050, 24, samples, true));
    device.open(QIODevice::ReadOnly);
    WaveInfo info;
    QString error;
    CHECK(parseWave(&device, &info, &error));
    CHECK(info.format.sampleFormat() == QAudioFormat::Int32);
    CHECK(info.format.sampleRate() == 22050);
    CHECK(info.frames == 4);
    WaveReader reader(&device, info);
    QByteArray out(16, '\0');
    CHECK(reader.read(out.data(), 16) == 16);
    qint32 v = 0;
    std::memcpy(&v, out.constData(), 4);
    CHECK(v == -2000 * 256);
    std::memcpy(&v, out.constData() + 12, 4);
    CHECK(v == 1000 * 256);
    CHECK(reader.atEnd());

    QBuffer bad;
    bad.setData(QByteArray("RIFF\0\0\0\0AVI ", 12));
    bad.open(QIODevice::ReadOnly);
    CHECK(!parseWave(&bad, &info, &error));
    QBuffer compressed;
    compressed.setData(wavFile(2, 1, 8000, 4, QByteArray(10, '\0')));
    compressed.open(QIODevice::ReadOnly);
    CHECK(!parseWave(&compressed, &info, &error));
    CHECK(!error.isEmpty());
    (void)dir;
}

void testSoundEffect(const QTemporaryDir &dir) {
    g_world->takePlayed();
    const QByteArray pcm = stereoPattern(800); // 100 ms
    const QString path = writeFile(dir, QStringLiteral("click.wav"), wavFile(1, 2, 8000, 16, pcm));
    QSoundEffect effect;
    CHECK(effect.status() == QSoundEffect::Null);
    CHECK(QSoundEffect::supportedMimeTypes().contains(QStringLiteral("audio/x-wav")));
    QList<QSoundEffect::Status> statuses;
    int loaded = 0;
    QObject::connect(&effect, &QSoundEffect::statusChanged, [&] { statuses.append(effect.status()); });
    QObject::connect(&effect, &QSoundEffect::loadedChanged, [&] { ++loaded; });
    effect.setSource(QUrl::fromLocalFile(path));
    CHECK(effect.source() == QUrl::fromLocalFile(path));
    CHECK(statuses == (QList<QSoundEffect::Status>{QSoundEffect::Loading, QSoundEffect::Ready}));
    CHECK(effect.isLoaded() && loaded == 1);
    CHECK(effect.audioDevice().id() == "fake-out");

    int playingChanges = 0;
    QObject::connect(&effect, &QSoundEffect::playingChanged, [&] { ++playingChanges; });
    effect.play();
    CHECK(effect.isPlaying());
    CHECK(effect.loopsRemaining() == 1);
    CHECK(waitFor([&] { return !effect.isPlaying(); }));
    CHECK(playingChanges == 2);
    CHECK(effect.loopsRemaining() == 0);
    CHECK(g_world->takePlayed() == pcm);

    // Three loops, counted down.
    QList<int> remaining;
    QObject::connect(&effect, &QSoundEffect::loopsRemainingChanged, [&] { remaining.append(effect.loopsRemaining()); });
    effect.setLoopCount(3);
    CHECK(effect.loopCount() == 3);
    effect.setVolume(0.5f);
    CHECK(effect.volume() == 0.5f);
    effect.play();
    CHECK(waitFor([&] { return !effect.isPlaying(); }));
    CHECK(remaining == (QList<int>{3, 2, 1, 0}));
    const QByteArray three = g_world->takePlayed();
    CHECK(three.size() == pcm.size() * 3);
    qint16 sample = 0;
    std::memcpy(&sample, three.constData() + 600 * 4, 2);
    CHECK(sample == 300); // half volume

    // Infinite, until stop(); muted.
    effect.setLoopCount(QSoundEffect::Infinite);
    effect.setMuted(true);
    CHECK(effect.isMuted());
    effect.play();
    spinEvents(150);
    CHECK(effect.isPlaying());
    CHECK(effect.loopsRemaining() == QSoundEffect::Infinite);
    effect.stop();
    CHECK(!effect.isPlaying());
    CHECK(effect.loopsRemaining() == 0);
    const QByteArray silent = g_world->takePlayed();
    CHECK(silent.size() > pcm.size());
    CHECK(silent.count('\0') == silent.size());

    effect.setLoopCount(0);
    CHECK(effect.loopCount() == 1);
    effect.setLoopCount(-5);
    CHECK(effect.loopCount() == 1);

    // Errors, and unloading.
    effect.setSource(QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing.wav"))));
    CHECK(effect.status() == QSoundEffect::Error);
    CHECK(!effect.isLoaded());
    effect.play();
    CHECK(!effect.isPlaying());
    effect.setSource(QUrl(QStringLiteral("https://example.com/a.wav")));
    CHECK(effect.status() == QSoundEffect::Error);
    effect.setSource(QUrl::fromLocalFile(writeFile(dir, QStringLiteral("empty.wav"), wavFile(1, 1, 8000, 16, {}))));
    CHECK(effect.status() == QSoundEffect::Error);
    effect.setSource(QUrl());
    CHECK(effect.status() == QSoundEffect::Null);

    // No output device: nothing plays.
    QSoundEffect nowhere(QAudioDevice{});
    nowhere.setSource(QUrl::fromLocalFile(path));
    nowhere.play();
    CHECK(!nowhere.isPlaying());
}

void testPlayer(const QTemporaryDir &dir) {
    g_world->takePlayed();
    const QByteArray pcm = stereoPattern(4000); // 500 ms
    const QString path = writeFile(dir, QStringLiteral("song.wav"), wavFile(1, 2, 8000, 16, pcm));

    QMediaPlayer player;
    CHECK(player.mediaStatus() == QMediaPlayer::NoMedia);
    CHECK(player.isAvailable());
    QAudioOutput output;
    CHECK(output.device().id() == "fake-out");
    player.setAudioOutput(&output);
    CHECK(player.audioOutput() == &output);

    QList<QMediaPlayer::MediaStatus> statuses;
    QObject::connect(&player, &QMediaPlayer::mediaStatusChanged,
                     [&](QMediaPlayer::MediaStatus s) { statuses.append(s); });
    qint64 lastPosition = -1;
    int positionChanges = 0;
    QObject::connect(&player, &QMediaPlayer::positionChanged, [&](qint64 p) {
        lastPosition = p;
        ++positionChanges;
    });
    player.setSource(QUrl::fromLocalFile(path));
    CHECK(statuses == (QList<QMediaPlayer::MediaStatus>{QMediaPlayer::LoadingMedia, QMediaPlayer::LoadedMedia}));
    CHECK(player.duration() == 500);
    CHECK(player.hasAudio() && !player.hasVideo());
    CHECK(player.isSeekable());
    CHECK(player.bufferProgress() == 1.0f);
    CHECK(player.error() == QMediaPlayer::NoError);

    player.play();
    CHECK(player.playbackState() == QMediaPlayer::PlayingState);
    CHECK(player.isPlaying());
    CHECK(player.mediaStatus() == QMediaPlayer::BufferedMedia);
    CHECK(waitFor([&] { return player.mediaStatus() == QMediaPlayer::EndOfMedia; }));
    CHECK(player.playbackState() == QMediaPlayer::StoppedState);
    CHECK(player.position() == 500 && lastPosition == 500);
    CHECK(positionChanges >= 2);
    CHECK(g_world->takePlayed() == pcm);

    // Play again from the start; pause and resume; seek.
    output.setVolume(0.5f);
    player.play();
    CHECK(waitFor([&] { return player.position() >= 100; }));
    player.pause();
    CHECK(player.playbackState() == QMediaPlayer::PausedState);
    const qint64 pausedAt = player.position();
    spinEvents(80);
    CHECK(player.position() == pausedAt);
    player.setPosition(400);
    CHECK(player.position() == 400);
    CHECK(player.playbackState() == QMediaPlayer::PausedState);
    player.play();
    CHECK(waitFor([&] { return player.mediaStatus() == QMediaPlayer::EndOfMedia; }));
    const QByteArray tail = g_world->takePlayed();
    // The last 100 ms, at half volume, end the stream.
    qint16 sample = 0;
    std::memcpy(&sample, tail.constData() + tail.size() - 4, 2);
    CHECK(sample == qint16(std::lround(3999 * 0.5)));

    // Two loops.
    player.setLoops(2);
    CHECK(player.loops() == 2);
    output.setVolume(1.0f);
    player.play();
    CHECK(waitFor([&] { return player.mediaStatus() == QMediaPlayer::EndOfMedia; }));
    CHECK(g_world->takePlayed() == pcm + pcm);
    player.setLoops(1);

    // stop() goes back to the beginning.
    player.play();
    spinEvents(50);
    player.stop();
    CHECK(player.position() == 0);
    CHECK(player.playbackState() == QMediaPlayer::StoppedState);
    CHECK(player.mediaStatus() == QMediaPlayer::LoadedMedia);
    g_world->takePlayed();

    // Without an output the position still moves, in real time.
    player.setAudioOutput(nullptr);
    player.setPosition(350);
    player.play();
    CHECK(waitFor([&] { return player.mediaStatus() == QMediaPlayer::EndOfMedia; }, 2000));
    CHECK(player.position() == 500);
    CHECK(g_world->playedSize() == 0);

    // From a device.
    QBuffer buffer;
    buffer.setData(wavFile(3, 1, 8000, 32, QByteArray(8000 * 4, '\0')));
    buffer.open(QIODevice::ReadOnly);
    player.setSourceDevice(&buffer, QUrl(QStringLiteral("memory:tone")));
    CHECK(player.sourceDevice() == &buffer);
    CHECK(player.source() == QUrl(QStringLiteral("memory:tone")));
    CHECK(player.duration() == 1000);

    // Errors.
    QList<QMediaPlayer::Error> errors;
    QObject::connect(&player, &QMediaPlayer::errorOccurred,
                     [&](QMediaPlayer::Error e, const QString &) { errors.append(e); });
    player.setSource(QUrl::fromLocalFile(dir.filePath(QStringLiteral("missing.wav"))));
    CHECK(player.mediaStatus() == QMediaPlayer::InvalidMedia);
    CHECK(player.error() == QMediaPlayer::ResourceError);
    const QString notWav = writeFile(dir, QStringLiteral("song.mp3"), QByteArray("ID3 not a wav file"));
    player.setSource(QUrl::fromLocalFile(notWav));
    CHECK(player.error() == QMediaPlayer::FormatError);
    CHECK(!player.errorString().isEmpty());
    CHECK(errors == (QList<QMediaPlayer::Error>{QMediaPlayer::ResourceError, QMediaPlayer::FormatError}));
    player.play();
    CHECK(player.playbackState() == QMediaPlayer::StoppedState);
    // An empty file ends as soon as it starts, from the event loop.
    const QString empty = writeFile(dir, QStringLiteral("empty.wav"), wavFile(1, 2, 8000, 16, QByteArray()));
    player.setAudioOutput(&output);
    player.setSource(QUrl::fromLocalFile(empty));
    CHECK(player.mediaStatus() == QMediaPlayer::LoadedMedia);
    CHECK(player.duration() == 0);
    player.play();
    CHECK(waitFor([&] { return player.mediaStatus() == QMediaPlayer::EndOfMedia; }));
    CHECK(player.playbackState() == QMediaPlayer::StoppedState);
    player.setAudioOutput(nullptr);

    player.setSource(QUrl());
    CHECK(player.mediaStatus() == QMediaPlayer::NoMedia);
    CHECK(player.duration() == 0);

    player.setPlaybackRate(2.0);
    CHECK(player.playbackRate() == 2.0);
    QObject video;
    player.setVideoOutput(&video);
    CHECK(player.videoOutput() == &video);

    // The output goes away while playing.
    auto *temporary = new QAudioOutput;
    player.setSource(QUrl::fromLocalFile(path));
    player.setAudioOutput(temporary);
    player.play();
    spinEvents(30);
    delete temporary;
    CHECK(player.audioOutput() == nullptr);
    CHECK(player.isPlaying());
    player.stop();
    g_world->takePlayed();
}

void testAudioOutput() {
    QAudioOutput output;
    int volumeChanges = 0;
    int mutedChanges = 0;
    int deviceChanges = 0;
    QObject::connect(&output, &QAudioOutput::volumeChanged, [&](float) { ++volumeChanges; });
    QObject::connect(&output, &QAudioOutput::mutedChanged, [&](bool) { ++mutedChanges; });
    QObject::connect(&output, &QAudioOutput::deviceChanged, [&] { ++deviceChanges; });
    CHECK(output.volume() == 1.0f);
    output.setVolume(1.5f);
    CHECK(output.volume() == 1.0f && volumeChanges == 0);
    output.setVolume(0.25f);
    CHECK(output.volume() == 0.25f && volumeChanges == 1);
    output.setMuted(true);
    output.setMuted(true);
    CHECK(output.isMuted() && mutedChanges == 1);
    g_world->secondOutput = true;
    output.setDevice(QMediaDevices::audioOutputs().at(1));
    CHECK(output.device().id() == "fake-out-2" && deviceChanges == 1);
    output.setDevice(QAudioDevice());
    CHECK(output.device().id() == "fake-out" && deviceChanges == 2);
    g_world->secondOutput = false;
}

void testAlsa() {
    // The real backend lists devices, or none where ALSA is missing, without failing.
    auto alsa = createPlatformAudioBackend();
    const QList<QAudioDevice> outputs = alsa->devices(QAudioDevice::Output);
    for (const QAudioDevice &device : outputs)
        CHECK(device.mode() == QAudioDevice::Output && !device.id().isEmpty());
    CHECK(outputs.isEmpty() || outputs.first().isDefault());
    CHECK(alsa->devices(QAudioDevice::Null).isEmpty());
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    FakeWorld world;
    world.directory = dir.filePath(QStringLiteral("snd"));
    QDir().mkpath(world.directory);
    g_world = &world;
    testAlsa();
    setAudioBackend(std::make_unique<FakeBackend>());

    testFormat();
    testVolume();
    testDevices();
    testSinkPull();
    testSinkPush();
    testSinkSuspendAndErrors();
    testSource();
    testWave(dir);
    testSoundEffect(dir);
    testAudioOutput();
    testPlayer(dir);

    spinEvents(50); // let detached drains finish
    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("all multimedia tests passed\n");
    return 0;
}
