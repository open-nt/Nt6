// MIT License - see LICENSE
//
// Nt FM: patches and their JSON, CBOR and SBI forms; the synthesiser, rendered offline and checked
// by measuring what it plays; the engine's command path, voice stealing and events; and the
// real-time rule that render() never allocates, checked with a replaced operator new.
#include <QtCore>
#include <QtFM>

#include "qfmqueue_p.h"
#include "qfmsynth_p.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>
#include <vector>

using namespace QtFM;

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

// Counts allocations made while g_audioThread is set on the allocating thread.
static thread_local bool g_audioThread = false;
static std::atomic<int> g_audioAllocations{0};

// GCC pairs the malloc() inside the replaced operator new with the free() inside the replaced
// operator delete wherever it inlines both, and warns; they do match.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif
void *operator new(std::size_t size) {
    if (g_audioThread)
        ++g_audioAllocations;
    if (void *p = std::malloc(size ? size : 1))
        return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }

static const char *specPatch = R"({
  "format": "qtfm-patch", "version": 1, "name": "laser-small", "category": "laser", "chip": null,
  "algorithm": 5, "feedback": 3,
  "operators": [
    { "ratio": 1.0, "level": 1.0, "wave": "sine", "env": { "a": 0.0, "d": 0.12, "s": 0.0, "r": 0.05 } },
    { "ratio": 3.5, "level": 0.6, "wave": "sine", "env": { "a": 0.0, "d": 0.08, "s": 0.0, "r": 0.02 } },
    { "ratio": 1.0, "level": 0.8, "wave": "sine", "env": { "a": 0.0, "d": 0.15, "s": 0.0, "r": 0.05 } },
    { "ratio": 7.0, "level": 0.3, "wave": "sine", "env": { "a": 0.0, "d": 0.05, "s": 0.0, "r": 0.02 } }
  ],
  "pitch": { "base": "A5", "env": { "start": 12, "end": -24, "time": 0.18, "curve": -2 } },
  "noise": { "mode": "white", "level": 0.0 },
  "filter": { "type": "lowpass", "cutoff": 8000, "resonance": 0.2 },
  "random": { "pitch.base": 2, "operators.1.ratio": 0.25 },
  "gain": -6,
  "somethingNew": { "ignored": true }
})";

// A pure tone: one sine carrier at A4 (four operators, algorithm 0, modulators silent).
static Patch tone(const char *extra = "") {
    const QByteArray json = QByteArray(R"({"algorithm": 7, "operators": [
        {"level": 0}, {"level": 0}, {"level": 0},
        {"level": 0.5, "wave": "sine", "env": {"a": 0, "d": 0, "s": 1, "r": 0.01, "curve": 0}}],
        "pitch": {"base": 69}, "duration": 0.5)") +
                            extra + "}";
    QString error;
    const Patch p = Patch::fromJson(json, &error);
    if (!p.isValid())
        std::printf("tone: %s\n", qPrintable(error));
    return p;
}

// Returns the samples of the left channel of a 16-bit stereo WAV file.
static std::vector<float> leftChannel(const QByteArray &wav) {
    std::vector<float> out;
    if (wav.size() < 44 || !wav.startsWith("RIFF"))
        return out;
    for (qsizetype i = 44; i + 3 < wav.size(); i += 4)
        out.push_back(float(qFromLittleEndian<qint16>(wav.constData() + i)) / 32767.0f);
    return out;
}

// Estimates the frequency of a signal from its rising zero crossings.
static double frequency(const float *x, std::size_t n, std::size_t stride, int sampleRate) {
    double first = -1;
    double last = -1;
    int crossings = 0;
    for (std::size_t i = 1; i < n; ++i) {
        const float a = x[(i - 1) * stride];
        const float b = x[i * stride];
        if (a < 0 && b >= 0) {
            const double t = double(i - 1) + double(-a / (b - a));
            if (first < 0)
                first = t;
            last = t;
            ++crossings;
        }
    }
    return crossings < 2 ? 0 : (crossings - 1) * sampleRate / (last - first);
}

static float peak(const std::vector<float> &x) {
    float m = 0;
    for (float v : x)
        m = std::max(m, std::fabs(v));
    return m;
}

static void spinEvents(int msecs) {
    QEventLoop loop;
    QTimer::singleShot(msecs, &loop, [&] { loop.quit(); });
    loop.exec();
}

static void testParsing() {
    QString error;
    const Patch p = Patch::fromJson(specPatch, &error);
    CHECK(p.isValid());
    CHECK(error.isEmpty());
    CHECK(p.name() == QLatin1String("laser-small"));
    CHECK(p.category() == QLatin1String("laser"));
    CHECK(p.chip() == ChipType::None);
    CHECK(p.operatorCount() == 4);
    CHECK(!p.isLoop());
    CHECK(p.duration() < 0);
    CHECK(p.data()->baseNote == 81.0f);
    CHECK(p.data()->algorithm == 5);
    CHECK(p.data()->ops[1].ratio == 3.5f);
    CHECK(p.data()->filterType == FilterType::LowPass);
    CHECK(p.data()->random.size() == 2);

    // Note names.
    CHECK(Patch::fromJson(R"({"pitch": {"base": "C4"}})").data()->baseNote == 60.0f);
    CHECK(Patch::fromJson(R"({"pitch": {"base": "C#4"}})").data()->baseNote == 61.0f);
    CHECK(Patch::fromJson(R"({"pitch": {"base": "Bb3"}})").data()->baseNote == 58.0f);

    // Missing keys take defaults; out-of-range values are clamped.
    const Patch empty = Patch::fromJson("{}");
    CHECK(empty.isValid());
    CHECK(empty.operatorCount() == 4);
    const Patch wild = Patch::fromJson(
        R"({"algorithm": 99, "feedback": -4, "operators": [{"ratio": 1e9, "level": 7, "env": {"d": 1e30}}, {}],
            "filter": {"type": "nonsense", "cutoff": 1e12}, "gain": 1e9, "random": {"no.such.field": 3}})");
    CHECK(wild.isValid());
    CHECK(wild.operatorCount() == 2);
    CHECK(wild.data()->algorithm == 1);
    CHECK(wild.data()->feedback == 0.0f);
    CHECK(wild.data()->ops[0].ratio == 32.0f);
    CHECK(wild.data()->ops[0].level == 1.0f);
    CHECK(wild.data()->ops[0].env.decay == 10.0f);
    CHECK(wild.data()->filterType == FilterType::None);
    CHECK(wild.data()->gain == 24.0f);
    CHECK(wild.data()->random.isEmpty());

    // Not a patch.
    error.clear();
    CHECK(!Patch::fromJson("{nope", &error).isValid());
    CHECK(!error.isEmpty());
    CHECK(!Patch::fromJson("[1, 2]").isValid());
    CHECK(!Patch::fromJson(R"({"format": "something-else"})").isValid());
    CHECK(!Patch::fromJson(R"({"version": 2})").isValid());
    CHECK(!Patch().isValid());
    CHECK(Patch().toJson().isEmpty());
}

static void testRoundTrip() {
    QList<Patch> patches{Patch::fromJson(specPatch), tone()};
    for (int c = 0; c <= static_cast<int>(Category::Engine); ++c) {
        patches.append(Patch::generate(static_cast<Category>(c), 1234));
        patches.append(Patch::generate(static_cast<Category>(c), 99, ChipType::OPL2));
        patches.append(Patch::generate(static_cast<Category>(c), 7, ChipType::OPN2));
    }
    for (const Patch &p : std::as_const(patches)) {
        CHECK(p.isValid());
        const QByteArray json = p.toJson();
        const Patch back = Patch::fromJson(json);
        CHECK(back == p);
        CHECK(back.toJson() == json);
        const QByteArray cbor = p.toCbor();
        CHECK(!cbor.isEmpty());
        CHECK(cbor.size() < json.size());
        CHECK(Patch::fromCbor(cbor) == p);
        CHECK(json.size() < 1024 * 2); // every field written; the authored form is smaller
    }
    CHECK(!Patch::fromCbor("not cbor").isValid());
    CHECK(Patch::fromJson(specPatch) != tone());
}

static void testGenerators() {
    for (int c = 0; c <= static_cast<int>(Category::Engine); ++c) {
        const auto category = static_cast<Category>(c);
        const Patch a = Patch::generate(category, 5);
        CHECK(a == Patch::generate(category, 5));
        CHECK(a != Patch::generate(category, 6));
        CHECK(a.fitsChip(ChipType::None));

        const Patch opl = Patch::generate(category, 5, ChipType::OPL2);
        QStringList violations;
        CHECK(opl.fitsChip(ChipType::OPL2, &violations));
        if (!violations.isEmpty())
            std::printf("  OPL2 %d: %s\n", c, qPrintable(violations.join(QLatin1String(", "))));
        CHECK(opl.operatorCount() == 2);
        CHECK(opl.chip() == ChipType::OPL2);
        const Patch opn = Patch::generate(category, 5, ChipType::OPN2);
        CHECK(opn.fitsChip(ChipType::OPN2));

        // Every generated sound is audible and ends.
        PlayOptions o;
        o.duration = category == Category::Engine ? 0.3f : -1.0f;
        for (const Patch &p : {a, opl}) {
            const std::vector<float> x = leftChannel(Engine::renderToWav(p, 22050, o));
            CHECK(peak(x) > 0.02f);
            CHECK(x.size() < 22050u * 5);
        }
    }
    QStringList violations;
    CHECK(!Patch::generate(Category::Explosion, 1).fitsChip(ChipType::OPL2, &violations));
    CHECK(violations.contains(QLatin1String("noise")));
    CHECK(violations.contains(QLatin1String("operators: the chip has two operators per channel")));
    CHECK(!Patch::fromJson(specPatch).fitsChip(ChipType::OPN2));
    CHECK(!Patch().fitsChip(ChipType::None));
}

static void testMutate() {
    const Patch p = Patch::fromJson(specPatch);
    const Patch m = p.mutated(0.2f, 9);
    CHECK(m.isValid());
    CHECK(m != p);
    CHECK(m == p.mutated(0.2f, 9));
    CHECK(m != p.mutated(0.2f, 10));
    CHECK(p.mutated(0, 9).data()->ops[1].ratio == p.data()->ops[1].ratio);
    const Patch opl = Patch::generate(Category::Laser, 3, ChipType::OPL2);
    CHECK(opl.mutated(0.5f, 4).fitsChip(ChipType::OPL2));
    CHECK(!Patch().mutated(0.1f, 1).isValid());
}

static void testSbi() {
    QByteArray sbi("SBI\x1a", 4);
    QByteArray name("Test Organ");
    name.resize(32, '\0');
    sbi += name;
    const quint8 regs[16] = {0x21, 0x21, // multiplier 1, sustaining (EG-TYP) on both
                             0x10, 0x00, // modulator attenuated 12 dB, carrier full
                             0xF2, 0xF4, // attack 15, decay 2 / 4
                             0x45, 0x37, // sustain level 4 / 3, release 5 / 7
                             0x01, 0x00, // half-sine modulator, sine carrier
                             0x06,       // feedback 3, FM
                             0,    0,    0, 0, 0};
    sbi.append(reinterpret_cast<const char *>(regs), 16);
    QString error;
    const Patch p = Patch::fromChipInstrument(sbi, InstrumentFormat::Sbi, &error);
    CHECK(p.isValid());
    CHECK(p.name() == QLatin1String("Test Organ"));
    CHECK(p.chip() == ChipType::OPL2);
    CHECK(p.operatorCount() == 2);
    CHECK(p.isLoop());
    CHECK(p.data()->feedback == 3.0f);
    CHECK(p.data()->algorithm == 0);
    CHECK(p.data()->ops[0].wave == Waveform::HalfSine);
    CHECK(std::fabs(p.data()->ops[0].level - std::pow(10.0f, -12.0f / 20.0f)) < 1e-4f);
    CHECK(p.data()->ops[1].level == 1.0f);
    CHECK(p.data()->ops[0].env.attack == 0.0f);
    CHECK(p.fitsChip(ChipType::OPL2));
    PlayOptions o;
    o.duration = 0.2f;
    CHECK(peak(leftChannel(Engine::renderToWav(p, 22050, o))) > 0.05f);

    CHECK(!Patch::fromChipInstrument("SBI", InstrumentFormat::Sbi, &error).isValid());
    CHECK(!Patch::fromChipInstrument(QByteArray(60, 'x'), InstrumentFormat::Sbi).isValid());
}

static void testRouting() {
    using Private::routing;
    bool carrier[4];
    quint8 mod[4];
    routing(4, 0, carrier, mod);
    CHECK(!carrier[0] && !carrier[1] && !carrier[2] && carrier[3]);
    CHECK(mod[1] == 1 && mod[2] == 2 && mod[3] == 4);
    routing(4, 4, carrier, mod);
    CHECK(carrier[1] && carrier[3] && !carrier[0] && !carrier[2]);
    routing(4, 5, carrier, mod);
    CHECK(mod[1] == 1 && mod[2] == 1 && mod[3] == 1);
    routing(4, 7, carrier, mod);
    CHECK(carrier[0] && carrier[1] && carrier[2] && carrier[3]);
    routing(2, 0, carrier, mod);
    CHECK(!carrier[0] && carrier[1] && mod[1] == 1);
    routing(2, 1, carrier, mod);
    CHECK(carrier[0] && carrier[1] && mod[1] == 0);
}

static void testSynthesis() {
    // Pitch: A4, an octave up, and a fixed transposition.
    const int rate = 48000;
    std::vector<float> x = leftChannel(Engine::renderToWav(tone(), rate));
    CHECK(x.size() >= std::size_t(rate / 2));
    CHECK(std::fabs(frequency(x.data() + 1000, 12000, 1, rate) - 440.0) < 0.5);
    PlayOptions up;
    up.pitch = 12;
    x = leftChannel(Engine::renderToWav(tone(), rate, up));
    CHECK(std::fabs(frequency(x.data() + 1000, 12000, 1, rate) - 880.0) < 1.0);
    // The level: 0.5 panned to the centre is about 0.354 per channel.
    CHECK(std::fabs(peak(x) - 0.3536f) < 0.01f);

    // Length: a duration of 0.5 s and a release of 10 ms.
    x = leftChannel(Engine::renderToWav(tone(), rate));
    CHECK(x.size() >= std::size_t(rate * 0.5) && x.size() <= std::size_t(rate * 0.52) + 256);

    // A pitch envelope: an octave down over the note.
    const Patch sweep = tone(R"(, "pitch": {"base": 69, "env": {"start": 0, "end": -12, "time": 0.5}})");
    x = leftChannel(Engine::renderToWav(sweep, rate));
    const double early = frequency(x.data() + 200, 2400, 1, rate);
    const double late = frequency(x.data() + 21000, 2400, 1, rate);
    CHECK(early > 400 && early < 445);
    CHECK(late > 220 && late < 260);

    // A filter removes the highs of a square wave.
    const Patch square = tone(R"(, "operators": [{"level": 0}, {"level": 0}, {"level": 0},
        {"level": 0.5, "wave": "square", "env": {"a": 0, "d": 0, "s": 1, "r": 0.01}}])");
    const Patch filtered = tone(R"(, "operators": [{"level": 0}, {"level": 0}, {"level": 0},
        {"level": 0.5, "wave": "square", "env": {"a": 0, "d": 0, "s": 1, "r": 0.01}}],
        "filter": {"type": "lowpass", "cutoff": 500})");
    // The second difference measures the highs: large at the edges of a square, small after a low pass.
    auto roughness = [](const std::vector<float> &s) {
        double d = 0;
        for (std::size_t i = 1000; i < 20000; ++i)
            d += double(std::fabs(s[i] - 2 * s[i - 1] + s[i - 2]));
        return d;
    };
    CHECK(roughness(leftChannel(Engine::renderToWav(filtered, rate))) <
          roughness(leftChannel(Engine::renderToWav(square, rate))) * 0.1);

    // Noise and the crusher are audible; the soft clipper keeps the mix within range.
    const Patch noisy = Patch::fromJson(R"({"operators": [{"level": 0}, {"level": 0}],
        "noise": {"mode": "lfsr15", "level": 1, "rate": 8000, "env": {"a": 0, "d": 0.2}},
        "crush": {"bits": 4, "rate": 4000}, "gain": 12})");
    x = leftChannel(Engine::renderToWav(noisy, rate));
    CHECK(peak(x) > 0.3f);
    CHECK(peak(x) <= 1.0f);

    // Determinism: the same seed gives the same file; another seed varies the random block.
    const Patch laser = Patch::fromJson(specPatch);
    PlayOptions a;
    a.seed = 42;
    const QByteArray w1 = Engine::renderToWav(laser, rate, a);
    CHECK(w1 == Engine::renderToWav(laser, rate, a));
    PlayOptions b;
    b.seed = 43;
    CHECK(w1 != Engine::renderToWav(laser, rate, b));
    CHECK(Engine::renderToWav(laser) == renderToWav(laser, 48000, PlayOptions()));
    CHECK(Engine::renderToWav(Patch()).isEmpty());

    // The WAV header.
    CHECK(w1.mid(8, 8) == "WAVEfmt ");
    CHECK(qFromLittleEndian<quint32>(w1.constData() + 24) == 48000u);
    CHECK(qFromLittleEndian<quint32>(w1.constData() + 40) == quint32(w1.size() - 44));

    // A loop without a duration is released after two seconds.
    const Patch hum = Patch::generate(Category::Engine, 2);
    x = leftChannel(Engine::renderToWav(hum, 8000));
    CHECK(x.size() >= 16000 && x.size() < 16000 + 8000);
}

static std::vector<float> renderEngine(Engine &engine, int frames) {
    std::vector<float> out(std::size_t(frames) * 2);
    for (int done = 0; done < frames; done += 256)
        engine.render(out.data() + 2 * done, std::min(256, frames - done));
    return out;
}

static void testEngine() {
    EngineConfig config;
    config.seed = 7;
    Engine engine(config);
    CHECK(engine.config().seed == 7);
    CHECK(engine.config().sampleRate == 48000);
    CHECK(!engine.isStarted());
    CHECK(engine.activeVoices() == 0);

    QList<quint32> finished;
    QObject::connect(&engine, &Engine::voiceFinished, [&](quint32 id) { finished.append(id); });

    VoiceHandle voice = engine.play(tone());
    CHECK(voice.isValid());
    CHECK(voice.id() != 0);
    CHECK(voice.isPlaying());
    std::vector<float> out = renderEngine(engine, 4800);
    CHECK(engine.activeVoices() == 1);
    CHECK(std::fabs(frequency(out.data() + 2000, 2000, 2, 48000) - 440.0) < 2.0);
    renderEngine(engine, 48000);
    CHECK(engine.activeVoices() == 0);
    CHECK(!voice.isPlaying());
    spinEvents(50);
    CHECK(finished.contains(voice.id()));

    // Invalid patches give invalid handles, whose calls do nothing.
    VoiceHandle none = engine.play(Patch());
    CHECK(!none.isValid());
    CHECK(!none.isPlaying());
    none.stop();
    none.setPitch(3);

    // A sample-accurate delayed start.
    const Patch square = tone(R"(, "operators": [{"level": 0}, {"level": 0}, {"level": 0},
        {"level": 0.5, "wave": "square", "env": {"a": 0, "d": 0, "s": 1, "r": 0}}])");
    renderEngine(engine, 1000); // the render position is now a multiple of 256 plus a remainder
    PlayOptions later;
    later.delay = 300.0f / 48000.0f;
    engine.play(square, later);
    out = renderEngine(engine, 1000);
    int first = -1;
    for (int i = 0; i < 1000 && first < 0; ++i) {
        if (out[std::size_t(2 * i)] != 0.0f)
            first = i;
    }
    CHECK(first == 300);
    engine.stopAll(true);
    renderEngine(engine, 1000);
    CHECK(engine.activeVoices() == 0);

    // stop() releases a loop, which otherwise plays on.
    const Patch hum = Patch::generate(Category::Engine, 3);
    VoiceHandle loop = engine.play(hum);
    renderEngine(engine, 48000);
    CHECK(loop.isPlaying());
    loop.stop();
    renderEngine(engine, 48000);
    CHECK(!loop.isPlaying());

    // Live parameters: an octave up, smoothed.
    const Patch held = tone(R"(, "duration": -1, "loop": true)");
    VoiceHandle live = engine.play(held);
    out = renderEngine(engine, 4800);
    CHECK(std::fabs(frequency(out.data() + 2000, 2000, 2, 48000) - 440.0) < 2.0);
    live.setPitch(12);
    out = renderEngine(engine, 4800);
    CHECK(std::fabs(frequency(out.data() + 4000, 2000, 2, 48000) - 880.0) < 4.0);
    live.setPan(-1);
    out = renderEngine(engine, 4800);
    CHECK(std::fabs(out[2 * 4000 + 1]) < 1e-3f);
    live.setParameter(ParamId::Gain, -120);
    out = renderEngine(engine, 4800);
    CHECK(std::fabs(out[2 * 4000]) < 1e-3f);
    live.setParameter(ParamId::Gain, 0);
    live.setPan(0);

    // Bus and master gains.
    engine.setBusGain(0, -120);
    CHECK(engine.busGain(0) == -120.0f);
    out = renderEngine(engine, 4800);
    CHECK(std::fabs(out[2 * 4000]) < 1e-3f);
    engine.setBusGain(0, 0);
    engine.setMasterGain(-6);
    CHECK(engine.masterGain() == -6.0f);
    CHECK(engine.busGain(5) == 0.0f);
    live.stop(true);
    renderEngine(engine, 1000);
    CHECK(!live.isPlaying());
    engine.setMasterGain(0);

    // 16-bit output.
    std::vector<qint16> pcm(2 * 1000);
    engine.play(tone());
    CHECK(engine.render(pcm.data(), 1000) == 1000);
    int nonZero = 0;
    for (qint16 s : pcm)
        nonZero += s != 0;
    CHECK(nonZero > 1000);
    CHECK(engine.render(static_cast<float *>(nullptr), 10) == 0);
}

static void testStealing() {
    EngineConfig config;
    config.maxVoices = 2;
    Engine engine(config);
    const Patch held = tone(R"(, "duration": -1, "loop": true)");
    VoiceHandle a = engine.play(held);
    renderEngine(engine, 256);
    VoiceHandle b = engine.play(held);
    renderEngine(engine, 256);
    VoiceHandle c = engine.play(held);
    renderEngine(engine, 1024);
    CHECK(engine.activeVoices() == 2);
    CHECK(!a.isPlaying()); // the oldest of equal priority and level
    CHECK(b.isPlaying());
    CHECK(c.isPlaying());

    // Voices of higher priority survive; a new voice of lower priority is dropped.
    engine.stopAll(true);
    renderEngine(engine, 1024);
    PlayOptions high;
    high.priority = 5;
    VoiceHandle h1 = engine.play(held, high);
    VoiceHandle h2 = engine.play(held, high);
    VoiceHandle low = engine.play(held);
    renderEngine(engine, 1024);
    CHECK(h1.isPlaying() && h2.isPlaying());
    CHECK(!low.isPlaying());
    engine.stopAll(true);
    renderEngine(engine, 1024);
    CHECK(engine.activeVoices() == 0);

    // A stolen voice fades out over 2 ms rather than stopping dead.
    VoiceHandle s1 = engine.play(held);
    VoiceHandle s2 = engine.play(held);
    renderEngine(engine, 2048);
    engine.play(held);
    std::vector<float> out = renderEngine(engine, 256);
    float biggestStep = 0;
    for (std::size_t i = 1; i < 200; ++i)
        biggestStep = std::max(biggestStep, std::fabs(out[2 * i] - out[2 * (i - 1)]));
    CHECK(biggestStep < 0.1f);
}

class FakeOutput : public AudioOutput {
public:
    bool open(int sampleRate, int channelCount, SampleFormat sampleFormat) override {
        rate = sampleRate;
        channels = channelCount;
        format = sampleFormat;
        opened = !fail;
        return !fail;
    }
    void close() override { opened = false; }
    int pullFloat(int frames) {
        buffer.assign(std::size_t(frames) * 2, 1.0f);
        return pull(buffer.data(), frames);
    }
    void underrunNow() { reportUnderrun(); }
    std::vector<float> buffer;
    int rate = 0;
    int channels = 0;
    SampleFormat format = SampleFormat::Q15;
    bool opened = false;
    bool fail = false;
};

static void testOutput() {
    FakeOutput output;
    CHECK(output.pullFloat(16) == 0); // not attached: silence
    CHECK(output.buffer[0] == 0.0f);
    {
        EngineConfig config;
        config.sampleRate = 32000;
        Engine engine(config);
        int underruns = 0;
        QObject::connect(&engine, &Engine::underrun, [&] { ++underruns; });
        output.fail = true;
        CHECK(!engine.start(&output));
        CHECK(!engine.isStarted());
        output.fail = false;
        CHECK(engine.start(&output));
        CHECK(engine.isStarted());
        CHECK(!engine.start(&output));
        CHECK(output.opened);
        CHECK(output.rate == 32000);
        CHECK(output.channels == 2);
        CHECK(output.format == SampleFormat::Float32);
        engine.play(tone());
        CHECK(output.pullFloat(512) == 512);
        CHECK(peak(output.buffer) > 0.1f);
        output.underrunNow();
        spinEvents(50);
        CHECK(underruns == 1);
        engine.stop();
        CHECK(!output.opened);
        CHECK(!engine.isStarted());
        CHECK(output.pullFloat(16) == 0);
        CHECK(engine.start(&output));
    }
    // The engine's destructor stopped and detached the output.
    CHECK(!output.opened);
    CHECK(output.pullFloat(16) == 0);

    // A handle outlives its engine.
    VoiceHandle orphan;
    {
        Engine engine;
        orphan = engine.play(Patch::generate(Category::Engine, 1));
        CHECK(orphan.isPlaying());
    }
    CHECK(orphan.isValid());
    CHECK(!orphan.isPlaying());
    orphan.stop();
    orphan.setGain(-3);
}

static void testRealTimeSafety() {
    EngineConfig config;
    config.maxVoices = 8;
    config.seed = 3;
    Engine engine(config);
    std::vector<float> out(512);
    std::vector<qint16> pcm(512);
    QList<VoiceHandle> handles;
    for (int round = 0; round < 200; ++round) {
        // Control side: plays (beyond the voice count, so voices are stolen), stops and parameters.
        const auto category = static_cast<Category>(round % 9);
        PlayOptions o;
        o.delay = float(round % 3) * 0.001f;
        o.priority = round % 2;
        handles.append(engine.play(Patch::generate(category, quint32(round)), o));
        if (round % 5 == 0)
            handles[round / 2].stop(round % 10 == 0);
        if (round % 7 == 0)
            handles.last().setParameter(ParamId::FilterCutoff, 1000);
        if (round % 50 == 0)
            engine.stopAll();
        // Audio side.
        g_audioThread = true;
        engine.render(out.data(), 256);
        engine.render(pcm.data(), 256);
        g_audioThread = false;
    }
    CHECK(g_audioAllocations.load() == 0);
}

static void testQueues() {
    Private::MpscQueue<int> queue(64);
    constexpr int producers = 4;
    constexpr int each = 20000;
    std::atomic<bool> done{false};
    std::vector<int> counts(producers, 0);
    std::vector<int> last(producers, -1);
    bool ordered = true;
    std::thread consumer([&] {
        int received = 0;
        int value;
        while (received < producers * each) {
            if (queue.pop(value)) {
                const int p = value / each;
                const int n = value % each;
                ordered = ordered && n > last[std::size_t(p)];
                last[std::size_t(p)] = n;
                ++counts[std::size_t(p)];
                ++received;
            }
        }
        done = true;
    });
    std::vector<std::thread> threads;
    for (int p = 0; p < producers; ++p) {
        threads.emplace_back([&queue, p] {
            for (int n = 0; n < each; ++n) {
                while (!queue.push(p * each + n))
                    std::this_thread::yield();
            }
        });
    }
    for (std::thread &t : threads)
        t.join();
    consumer.join();
    CHECK(done.load());
    CHECK(ordered);
    for (int c : counts)
        CHECK(c == each);

    Private::SpscQueue<int> spsc(3);
    CHECK(spsc.push(1) && spsc.push(2) && spsc.push(3));
    CHECK(!spsc.push(4));
    int v = 0;
    CHECK(spsc.pop(v) && v == 1);
    CHECK(spsc.push(4));
    CHECK(spsc.pop(v) && v == 2);
    CHECK(spsc.pop(v) && v == 3);
    CHECK(spsc.pop(v) && v == 4);
    CHECK(!spsc.pop(v));
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testParsing();
    testRoundTrip();
    testGenerators();
    testMutate();
    testSbi();
    testRouting();
    testSynthesis();
    testEngine();
    testStealing();
    testOutput();
    testRealTimeSafety();
    testQueues();
    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("All Nt FM tests passed\n");
    return 0;
}
