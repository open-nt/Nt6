// MIT License - see LICENSE
//
// fmplay: plays or renders FM sound effects with Nt FM.
//
//   fmplay laser                     generate a laser and play it
//   fmplay coin.json                 play a patch file
//   fmplay explosion -o boom.wav     render to a WAV file instead
//   fmplay pickup --chip opl2 --seed 7 --save pickup.json
//   fmplay --demo                    a short tour: effects, a delayed jingle and an engine loop
//   fmplay --bench 32                render 32 voices as fast as possible and report the CPU load
#include <QtCore>
#include <QtFM>

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

using namespace QtFM;

namespace {

//! [patch]
const char *laserJson = R"({
  "format": "qtfm-patch", "version": 1, "name": "laser-small", "category": "laser",
  "algorithm": 5, "feedback": 3,
  "operators": [
    { "ratio": 1.0, "level": 1.0, "env": { "a": 0, "d": 0.12, "s": 0, "r": 0.05 } },
    { "ratio": 3.5, "level": 0.6, "env": { "a": 0, "d": 0.08, "s": 0, "r": 0.02 } },
    { "ratio": 1.0, "level": 0.8, "env": { "a": 0, "d": 0.15, "s": 0, "r": 0.05 } },
    { "ratio": 7.0, "level": 0.3, "env": { "a": 0, "d": 0.05, "s": 0, "r": 0.02 } }
  ],
  "pitch": { "base": "A5", "env": { "start": 12, "end": -24, "time": 0.18, "curve": -2 } },
  "filter": { "type": "lowpass", "cutoff": 8000, "resonance": 0.2 },
  "random": { "pitch.base": 2, "operators.1.ratio": 0.25 },
  "gain": -6
})";
//! [patch]

//! [output]
// An output with no device: a thread that pulls blocks as fast as it can, to measure the engine.
class BenchmarkOutput : public AudioOutput {
public:
    bool open(int sampleRate, int channels, SampleFormat format) override {
        Q_UNUSED(format);
        m_rate = sampleRate;
        m_buffer.assign(static_cast<std::size_t>(256 * channels), 0.0f);
        m_running = true;
        m_thread = std::thread([this] {
            while (m_running.load())
                m_frames += pull(m_buffer.data(), 256);
        });
        return true;
    }
    void close() override {
        m_running = false;
        if (m_thread.joinable())
            m_thread.join();
    }
    qint64 frames() const { return m_frames.load(); }
    int rate() const { return m_rate; }

private:
    std::vector<float> m_buffer;
    std::atomic<bool> m_running{false};
    std::atomic<qint64> m_frames{0};
    std::thread m_thread;
    int m_rate = 0;
};
//! [output]

const struct {
    const char *name;
    Category category;
} categories[] = {
    {"pickup", Category::Pickup},       {"coin", Category::Pickup},     {"laser", Category::Laser},
    {"explosion", Category::Explosion}, {"powerup", Category::PowerUp}, {"hit", Category::Hit},
    {"jump", Category::Jump},           {"blip", Category::Blip},       {"alarm", Category::Alarm},
    {"engine", Category::Engine},
};

bool categoryNamed(const QString &name, Category *category) {
    for (const auto &c : categories) {
        if (name == QLatin1String(c.name)) {
            *category = c.category;
            return true;
        }
    }
    return false;
}

ChipType chipNamed(const QString &name) {
    const QString n = name.toLower();
    if (n == QLatin1String("opl2"))
        return ChipType::OPL2;
    if (n == QLatin1String("opl3"))
        return ChipType::OPL3;
    if (n == QLatin1String("opll"))
        return ChipType::OPLL;
    if (n == QLatin1String("opn2"))
        return ChipType::OPN2;
    if (n == QLatin1String("opm"))
        return ChipType::OPM;
    return ChipType::None;
}

// Plays patch on a started engine and returns when it has finished.
int playAndWait(QCoreApplication &app, Engine &engine, const Patch &patch, const PlayOptions &options) {
    VoiceHandle voice = engine.play(patch, options);
    if (!voice.isValid())
        return 1;
    QObject::connect(&engine, &Engine::voiceFinished, &app, [&app, &voice](quint32 id) {
        if (id == voice.id())
            QTimer::singleShot(100, &app, &QCoreApplication::quit);
    });
    if (patch.isLoop() && options.duration < 0) {
        // A loop plays until stopped: give it three seconds.
        QTimer::singleShot(3000, &app, [voice]() mutable { voice.stop(); });
    }
    return app.exec();
}

int demo(QCoreApplication &app) {
    //! [engine]
    Engine engine;
    if (!engine.start()) {
        std::fprintf(stderr, "fmplay: no sound output\n");
        return 1;
    }
    const Patch coin = Patch::generate(Category::Pickup, 3);
    engine.play(coin);
    //! [engine]

    //! [generate]
    // A jingle: four coins a fifth apart, each starting on its own sample.
    for (int i = 0; i < 4; ++i) {
        PlayOptions options;
        options.delay = 0.6f + 0.12f * float(i);
        options.pitch = 7.0f * float(i % 2);
        engine.play(Patch::generate(Category::Pickup, 11), options);
    }
    //! [generate]

    const Patch laser = Patch::fromJson(laserJson);
    for (int i = 0; i < 3; ++i) {
        PlayOptions options;
        options.delay = 1.4f + 0.2f * float(i);
        engine.play(laser, options); // the random block makes each shot a little different
    }

    //! [loop]
    PlayOptions engineOptions;
    engineOptions.delay = 2.3f;
    VoiceHandle hum = engine.play(Patch::generate(Category::Engine, 42), engineOptions);
    auto *rev = new QTimer(&app);
    QElapsedTimer clock;
    clock.start();
    QObject::connect(rev, &QTimer::timeout, &app, [hum, &clock]() mutable {
        const float t = float(clock.elapsed()) / 1000.0f - 2.3f;
        hum.setPitch(std::clamp(t * 6.0f, 0.0f, 14.0f)); // smoothed on the audio thread
        if (t > 3.0f)
            hum.stop();
    });
    rev->start(20);
    //! [loop]

    PlayOptions boom;
    boom.delay = 5.5f;
    engine.play(Patch::generate(Category::Explosion, 5), boom);
    QTimer::singleShot(7500, &app, &QCoreApplication::quit);
    return app.exec();
}

int bench(int voices) {
    EngineConfig config;
    config.maxVoices = voices;
    Engine engine(config);
    const Patch patch = Patch::generate(Category::Engine, 1);
    for (int i = 0; i < voices; ++i) {
        PlayOptions options;
        options.pitch = float(i % 12);
        engine.play(patch, options);
    }
    BenchmarkOutput output;
    engine.start(&output);
    QElapsedTimer timer;
    timer.start();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    engine.stop();
    const double seconds = double(timer.nsecsElapsed()) / 1e9;
    const double audio = double(output.frames()) / output.rate();
    std::printf("%d voices: %.1f s of audio in %.2f s, %.2f%% of one core in real time\n", voices, audio, seconds,
                100.0 * seconds / audio);
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Plays or renders FM sound effects."));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("sound"),
                                 QStringLiteral("A patch file, or a category: pickup, laser, explosion, powerup, "
                                                "hit, jump, blip, alarm or engine."));
    const QCommandLineOption outOption({QStringLiteral("o"), QStringLiteral("output")},
                                       QStringLiteral("Render to a WAV file instead of playing."),
                                       QStringLiteral("file"));
    const QCommandLineOption seedOption(QStringLiteral("seed"), QStringLiteral("Seed of the patch and its variance."),
                                        QStringLiteral("n"), QStringLiteral("1"));
    const QCommandLineOption rateOption(QStringLiteral("rate"), QStringLiteral("Sample rate."), QStringLiteral("hz"),
                                        QStringLiteral("48000"));
    const QCommandLineOption pitchOption(QStringLiteral("pitch"), QStringLiteral("Semitones to transpose by."),
                                         QStringLiteral("semitones"), QStringLiteral("0"));
    const QCommandLineOption chipOption(
        QStringLiteral("chip"), QStringLiteral("Generate within a chip's limits: opl2, opl3, opll, opn2 or opm."),
        QStringLiteral("chip"));
    const QCommandLineOption saveOption(QStringLiteral("save"), QStringLiteral("Write the patch as JSON."),
                                        QStringLiteral("file"));
    const QCommandLineOption demoOption(QStringLiteral("demo"), QStringLiteral("Play a short tour."));
    const QCommandLineOption benchOption(QStringLiteral("bench"), QStringLiteral("Measure the CPU load of n voices."),
                                         QStringLiteral("n"));
    parser.addOptions(
        {outOption, seedOption, rateOption, pitchOption, chipOption, saveOption, demoOption, benchOption});
    parser.process(app);

    if (parser.isSet(benchOption))
        return bench(std::max(1, parser.value(benchOption).toInt()));
    if (parser.isSet(demoOption))
        return demo(app);
    if (parser.positionalArguments().size() != 1)
        parser.showHelp(1);

    const QString sound = parser.positionalArguments().constFirst();
    const quint32 seed = parser.value(seedOption).toUInt();
    Patch patch;
    Category category;
    if (categoryNamed(sound, &category)) {
        patch = Patch::generate(category, seed, chipNamed(parser.value(chipOption)));
    } else {
        QFile file(sound);
        if (!file.open(QIODevice::ReadOnly)) {
            std::fprintf(stderr, "fmplay: cannot read %s\n", qPrintable(sound));
            return 1;
        }
        QString error;
        const QByteArray data = file.readAll();
        patch = data.startsWith("SBI\x1a") ? Patch::fromChipInstrument(data, InstrumentFormat::Sbi, &error)
                                           : Patch::fromJson(data, &error);
        if (!patch.isValid()) {
            std::fprintf(stderr, "fmplay: %s: %s\n", qPrintable(sound), qPrintable(error));
            return 1;
        }
    }
    if (parser.isSet(saveOption)) {
        QFile out(parser.value(saveOption));
        if (!out.open(QIODevice::WriteOnly) || out.write(patch.toJson()) < 0)
            return 1;
    }

    PlayOptions options;
    options.seed = seed;
    options.pitch = parser.value(pitchOption).toFloat();
    const int rate = parser.value(rateOption).toInt();
    if (parser.isSet(outOption)) {
        //! [wav]
        const QByteArray wav = Engine::renderToWav(patch, rate, options);
        QFile file(parser.value(outOption));
        if (!file.open(QIODevice::WriteOnly) || file.write(wav) != wav.size())
            return 1;
        //! [wav]
        return 0;
    }

    EngineConfig config;
    config.sampleRate = rate;
    Engine engine(config);
    if (!engine.start()) {
        std::fprintf(stderr, "fmplay: no sound output; use -o to render to a file\n");
        return 1;
    }
    return playAndWait(app, engine, patch, options);
}
