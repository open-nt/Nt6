// MIT License - see LICENSE
//
// Nt FM: patches and their JSON, CBOR and SBI forms; the synthesiser, rendered offline and checked
// by measuring what it plays; the engine's command path, voice stealing and events; and the
// real-time rule that render() never allocates, checked with a replaced operator new.
#include <QtCore>
#include <QtFM>

#include "qfmmusic_p.h"
#include "qfmopl_p.h"
#include "qfmqueue_p.h"
#include "qfmsynth_p.h"

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>
#include <vector>

#ifdef NT_HAVE_ZLIB
#include <zlib.h>
#endif

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
void *operator new[](std::size_t size) { return operator new(size); }
void *operator new(std::size_t size, const std::nothrow_t &) noexcept {
    if (g_audioThread)
        ++g_audioAllocations;
    return std::malloc(size ? size : 1);
}
void *operator new[](std::size_t size, const std::nothrow_t &tag) noexcept { return operator new(size, tag); }
void operator delete(void *p, const std::nothrow_t &) noexcept { std::free(p); }
void operator delete[](void *p, const std::nothrow_t &) noexcept { std::free(p); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

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

// --- Chips -----------------------------------------------------------------------------------------

// Sets up channel ch of an OPL as a pure tone: a silent modulator and a carrier at level tl.
static void oplTone(ChipCore &opl, int ch, int fnum, int block, int tl = 0, quint8 c0 = 0x30) {
    const int bank = (ch / 9) << 8;
    const int local = ch % 9;
    const int off = (local / 3) * 8 + local % 3;
    auto w = [&](int reg, int v) { opl.write(static_cast<quint16>(bank | reg), static_cast<quint8>(v)); };
    w(0x20 + off, 0x21);
    w(0x40 + off, 0x3f);
    w(0x60 + off, 0xf0);
    w(0x80 + off, 0x0f);
    w(0x23 + off, 0x21);
    w(0x43 + off, tl);
    w(0x63 + off, 0xf0);
    w(0x83 + off, 0x0f);
    w(0xc0 + local, c0);
    w(0xa0 + local, fnum & 0xff);
    w(0xb0 + local, 0x20 | (block << 2) | (fnum >> 8));
}

static std::vector<float> generate(ChipCore &core, int frames, int channel = 0) {
    std::vector<qint32> raw(std::size_t(frames) * 2);
    core.generate(raw.data(), frames);
    std::vector<float> out(static_cast<std::size_t>(frames));
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = float(raw[2 * i + std::size_t(channel)]);
    return out;
}

static void testOplCore() {
    CHECK(ChipCore::create(ChipType::OPN2) == nullptr);
    CHECK(ChipCore::create(ChipType::None) == nullptr);
    std::unique_ptr<ChipCore> opl = ChipCore::create(ChipType::OPL2);
    CHECK(opl && opl->type() == ChipType::OPL2);
    CHECK(opl->nativeRate() == 49716);
    int fnum = 0;
    int block = 0;
    CHECK(Private::oplFrequency(440, &fnum, &block));
    CHECK(fnum > 512 && fnum < 1024);
    oplTone(*opl, 0, fnum, block);
    std::vector<float> x = generate(*opl, 49716);
    CHECK(std::fabs(frequency(x.data() + 1000, 40000, 1, 49716) - 440.0) < 1.0);
    const float full = peak(x);
    CHECK(full > 3500 && full < 4200);

    // Total level: 8 steps of 0.75 dB halve the level.
    opl->reset();
    oplTone(*opl, 0, fnum, block, 8);
    x = generate(*opl, 20000);
    CHECK(std::fabs(peak(x) / full - 0.5f) < 0.03f);

    // Key off: the release (rate 15) silences the channel.
    auto *core = static_cast<Private::OplCore *>(opl.get());
    CHECK(!core->channelSilent(0));
    opl->write(0xb0, static_cast<quint8>((block << 2) | (fnum >> 8)));
    x = generate(*opl, 5000);
    CHECK(std::fabs(x.back()) < 1.0f);
    CHECK(core->channelSilent(0));

    // Waveform select: half-sine only once the waveform-select bit is set.
    auto negativeShare = [](const std::vector<float> &v) {
        int neg = 0;
        for (float f : v)
            neg += f < -10.0f;
        return double(neg) / double(v.size());
    };
    opl->reset();
    opl->write(0xe3, 0x01);
    oplTone(*opl, 0, fnum, block);
    CHECK(negativeShare(generate(*opl, 20000)) > 0.4);
    opl->write(0x01, 0x20);
    CHECK(negativeShare(generate(*opl, 20000)) < 0.01);

    // Feedback on the modulator changes the sound.
    opl->reset();
    oplTone(*opl, 0, fnum, block);
    const std::vector<float> plain = generate(*opl, 4000);
    opl->reset();
    oplTone(*opl, 0, fnum, block);
    opl->write(0x40, 0x00); // modulator audible as a modulator
    opl->write(0xc0, 0x0e); // feedback 7
    const std::vector<float> rich = generate(*opl, 4000);
    double diff = 0;
    for (std::size_t i = 0; i < plain.size(); ++i)
        diff += double(std::fabs(plain[i] - rich[i]));
    CHECK(diff > 1e5);

    // Rhythm mode: the bass drum keys on through 0xBD.
    opl->reset();
    oplTone(*opl, 6, fnum, 2);
    opl->write(0xb6, 0x00); // channel key off: only the drum bit plays it
    CHECK(peak(generate(*opl, 2000)) < 1.0f);
    opl->write(0xbd, 0x30);
    CHECK(peak(generate(*opl, 4000)) > 1000.0f);

    // OPL3: the second bank, and panning once the OPL3 mode is on.
    std::unique_ptr<ChipCore> opl3 = ChipCore::create(ChipType::OPL3);
    CHECK(opl3 && opl3->type() == ChipType::OPL3);
    CHECK(static_cast<Private::OplCore *>(opl3.get())->channelCount() == 18);
    opl3->write(0x105, 0x01);
    oplTone(*opl3, 12, fnum, block, 0, 0x10); // left only
    CHECK(peak(generate(*opl3, 4000, 0)) > 3000.0f);
    CHECK(peak(generate(*opl3, 4000, 1)) == 0.0f);
    // A four-operator connection on channels 0 and 3.
    opl3->reset();
    opl3->write(0x105, 0x01);
    opl3->write(0x104, 0x01);
    oplTone(*opl3, 0, fnum, block);
    oplTone(*opl3, 3, fnum, block);
    CHECK(peak(generate(*opl3, 4000)) > 1000.0f);
}

static void testEngineChips() {
    EngineConfig config;
    config.chips = {ChipType::OPL2, ChipType::OPN2};
    Engine engine(config);
    CHECK(engine.chip(ChipType::OPL2) != nullptr);
    CHECK(engine.chip(ChipType::OPN2) == nullptr); // no core yet
    CHECK(engine.chip(ChipType::OPL3) == nullptr);
    Chip *opl = engine.chip(ChipType::OPL2);
    CHECK(opl->type() == ChipType::OPL2);

    // Register writes, the key-on delayed to an exact sample.
    const quint8 setup[][2] = {{0x20, 0x21}, {0x40, 0x3f}, {0x60, 0xf0}, {0x80, 0x0f}, {0x23, 0x21},
                               {0x43, 0x00}, {0x63, 0xf0}, {0x83, 0x0f}, {0xa0, 0x44}};
    for (const auto &w : setup)
        opl->writeRegister(w[0], w[1]);
    renderEngine(engine, 1000);
    opl->writeRegister(0xb0, 0x32, 2000.0f / 48000.0f);
    std::vector<float> out = renderEngine(engine, 6000);
    int first = -1;
    for (int i = 0; i < 6000 && first < 0; ++i) {
        if (std::fabs(out[std::size_t(2 * i)]) > 1e-4f)
            first = i;
    }
    CHECK(first >= 2000 && first < 2010);
    CHECK(std::fabs(frequency(out.data() + 2 * 3000, 3000, 2, 48000) - 440.0) < 5.0);
    opl->setGain(-120);
    out = renderEngine(engine, 2000);
    CHECK(peak(std::vector<float>(out.begin() + 1000, out.end())) < 1e-3f);
    opl->setGain(0);
    opl->reset();
    out = renderEngine(engine, 2000);
    CHECK(peak(std::vector<float>(out.begin() + 1000, out.end())) < 1e-3f);

    // Voices: a patch for the OPL2 plays on a chip channel, and ends.
    const Patch laser = Patch::generate(Category::Laser, 4, ChipType::OPL2);
    VoiceHandle v = engine.play(laser);
    out = renderEngine(engine, 4800);
    CHECK(engine.activeVoices() == 1);
    CHECK(peak(out) > 0.02f);
    renderEngine(engine, 48000 * 3);
    CHECK(!v.isPlaying());
    CHECK(engine.activeVoices() == 0);

    // A held chip voice follows its duration and stop().
    const Patch organ = Patch::fromChipInstrument(
        [] {
            QByteArray sbi("SBI\x1a", 4);
            sbi += QByteArray(32, '\0');
            const quint8 regs[16] = {0x21, 0x21, 0x10, 0x00, 0xf2, 0xf4, 0x45, 0x37, 0, 0, 0x06, 0, 0, 0, 0, 0};
            sbi.append(reinterpret_cast<const char *>(regs), 16);
            return sbi;
        }(),
        InstrumentFormat::Sbi);
    CHECK(organ.isLoop());
    VoiceHandle held = engine.play(organ);
    renderEngine(engine, 48000);
    CHECK(held.isPlaying());
    held.setPitch(12);
    held.stop(true);
    renderEngine(engine, 48000);
    CHECK(!held.isPlaying());
    // Ten voices on nine channels: one is stolen.
    QList<VoiceHandle> many;
    for (int i = 0; i < 10; ++i)
        many.append(engine.play(organ));
    renderEngine(engine, 4800);
    CHECK(engine.activeVoices() == 9);
    CHECK(!many.first().isPlaying());
    engine.stopAll(true);
    renderEngine(engine, 48000);
    CHECK(engine.activeVoices() == 0);

    // Without an OPL chip, the same patch plays on the native engine.
    Engine native;
    native.play(laser);
    CHECK(peak(renderEngine(native, 4800)) > 0.02f);
}

static QByteArray sbiBytes(const quint8 regs[11], const char *name) {
    QByteArray sbi("SBI\x1a", 4);
    QByteArray n(name);
    n.resize(32, '\0');
    sbi += n;
    sbi.append(reinterpret_cast<const char *>(regs), 11);
    sbi.append(QByteArray(5, '\0'));
    return sbi;
}

static void testInstruments() {
    // SBI round trip through a native patch.
    const quint8 regs[11] = {0x21, 0x31, 0x4f, 0x00, 0xf2, 0x52, 0x54, 0x37, 0x01, 0x00, 0x0a};
    const QByteArray sbi = sbiBytes(regs, "Round Trip");
    const Patch p = Patch::fromChipInstrument(sbi, InstrumentFormat::Sbi);
    CHECK(p.isValid());
    const QByteArray back = p.toChipInstrument(InstrumentFormat::Sbi);
    CHECK(back.size() == 52);
    CHECK(back == sbi);
    if (back != sbi) {
        for (int i = 36; i < 47; ++i)
            std::printf("  reg %d: %02x -> %02x\n", i - 36, quint8(sbi.at(i)), quint8(back.at(i)));
    }
    QString error;
    CHECK(Patch::fromJson(specPatch).toChipInstrument(InstrumentFormat::Sbi, &error).isEmpty());
    CHECK(!error.isEmpty());
    CHECK(p.toChipInstrument(InstrumentFormat::Ibk).isEmpty());
    // Generated OPL2 patches export.
    CHECK(Patch::generate(Category::Jump, 2, ChipType::OPL2).toChipInstrument(InstrumentFormat::Sbi).size() == 52);

    // IBK banks.
    QByteArray ibk(3204, '\0');
    std::copy_n("IBK\x1a", 4, ibk.data());
    for (int i = 0; i < 128; ++i) {
        std::copy_n(reinterpret_cast<const char *>(regs), 11, ibk.data() + 4 + i * 16);
        const QByteArray name = QByteArray("inst") + QByteArray::number(i);
        std::copy_n(name.constData(), name.size(), ibk.data() + 4 + 2048 + i * 9);
    }
    const PatchBank bank = PatchBank::fromChipBank(ibk, InstrumentFormat::Ibk, &error);
    CHECK(bank.size() == 128);
    CHECK(bank.names().at(5) == QLatin1String("inst5"));
    CHECK(bank.patch(QStringLiteral("inst5")).operatorCount() == 2);
    CHECK(bank.toChipBank(InstrumentFormat::Ibk) == ibk);
    CHECK(PatchBank::fromChipBank(ibk.left(100), InstrumentFormat::Ibk).isEmpty());
    CHECK(PatchBank::fromChipBank(sbi, InstrumentFormat::Sbi).size() == 1);
}

static void testBanks() {
    PatchBank bank;
    CHECK(bank.isEmpty());
    bank.setName(QStringLiteral("game"));
    bank.insert(QStringLiteral("laser"), Patch::fromJson(specPatch));
    bank.insert(QStringLiteral("coin"), Patch::generate(Category::Pickup, 1));
    bank.insert(QStringLiteral("boom"), Patch::generate(Category::Explosion, 1));
    bank.insert(QStringLiteral("coin"), Patch::generate(Category::Pickup, 2)); // replaces
    CHECK(bank.size() == 3);
    CHECK(bank.contains(QStringLiteral("coin")));
    CHECK(bank.patch(QStringLiteral("coin")).name() == QLatin1String("coin"));
    CHECK(bank.patch(QStringLiteral("coin")).data()->baseNote == Patch::generate(Category::Pickup, 2).data()->baseNote);
    CHECK(!bank.patch(QStringLiteral("none")).isValid());
    CHECK(!bank.at(7).isValid());
    CHECK(bank.at(0).name() == QLatin1String("laser"));

    const PatchBank fromJson = PatchBank::fromJson(bank.toJson());
    CHECK(fromJson == bank);
    CHECK(PatchBank::fromCbor(bank.toCbor()) == bank);
    CHECK(bank.toJson().size() < 64 * 1024);
    QString error;
    CHECK(PatchBank::fromJson("{}", &error).isEmpty());
    CHECK(!error.isEmpty());
    CHECK(PatchBank::fromJson("nope").isEmpty());
    CHECK(PatchBank::fromCbor("nope").isEmpty());

    const QByteArray header = bank.toCHeader(QStringLiteral("game sounds"));
    CHECK(header.contains("static const unsigned char game_sounds[] = {"));
    CHECK(header.contains("game_sounds_size = " + QByteArray::number(bank.toCbor().size()) + "u;"));
    CHECK(bank.toCHeader(QStringLiteral("9x")).contains("_9x[]"));
    // The array's bytes are the CBOR.
    QByteArray parsed;
    for (const QByteArray &token : header.split(',')) {
        const qsizetype at = token.indexOf("0x");
        if (at >= 0)
            parsed.append(static_cast<char>(token.mid(at + 2, 2).toInt(nullptr, 16)));
    }
    CHECK(parsed == bank.toCbor());

    CHECK(bank.remove(QStringLiteral("boom")));
    CHECK(!bank.remove(QStringLiteral("boom")));
    CHECK(bank.size() == 2);
    CHECK(bank != fromJson);
}

// A note on OPL2 channel 0 as register writes: setup, key on at 0, key off at off.
static QList<QPair<int, int>> noteWrites() {
    return {{0x01, 0x20}, {0x20, 0x21}, {0x40, 0x3f}, {0x60, 0xf0}, {0x80, 0x0f}, {0x23, 0x21},
            {0x43, 0x00}, {0x63, 0xf0}, {0x83, 0x0f}, {0xa0, 0x44}, {0xb0, 0x32}};
}

static QByteArray imfFile(int holdTicks) {
    QByteArray body;
    const auto writes = noteWrites();
    for (int i = 0; i < writes.size(); ++i) {
        body.append(char(writes[i].first)).append(char(writes[i].second));
        const int delay = i + 1 == writes.size() ? holdTicks : 0;
        body.append(char(delay & 0xff)).append(char(delay >> 8));
    }
    body.append(char(0xb0)).append(char(0x12)).append(char(0)).append(char(0));
    QByteArray imf;
    imf.append(char(body.size() & 0xff)).append(char(body.size() >> 8));
    return imf + body;
}

static QByteArray vgmFile(bool opl3) {
    QByteArray v(0x100, '\0');
    std::copy_n("Vgm ", 4, v.data());
    qToLittleEndian<quint32>(0x151, v.data() + 0x08);
    qToLittleEndian<quint32>(0x100 - 0x34, v.data() + 0x34);
    qToLittleEndian<quint32>(3579545, v.data() + (opl3 ? 0x5c : 0x50));
    for (const auto &w : noteWrites())
        v.append(char(opl3 ? 0x5e : 0x5a)).append(char(w.first)).append(char(w.second));
    const qsizetype loop = v.size();
    v.append(char(0x61)).append(char(0x44)).append(char(0xac)); // 44100 samples
    v.append(char(0x62));                                       // 735 more
    v.append(char(0x66));
    qToLittleEndian<quint32>(quint32(loop - 0x1c), v.data() + 0x1c);
    return v;
}

static QByteArray droV2File() {
    QByteArray d("DBRAWOPL", 8);
    QByteArray h(18, '\0');
    qToLittleEndian<quint16>(2, h.data());
    const auto writes = noteWrites();
    qToLittleEndian<quint32>(quint32(writes.size() + 1), h.data() + 4);
    qToLittleEndian<quint32>(1000, h.data() + 8);
    h[12] = 0;    // OPL2
    h[15] = 0x7e; // short delay
    h[16] = 0x7f; // long delay
    h[17] = char(writes.size());
    d += h;
    for (const auto &w : writes)
        d.append(char(w.first));
    for (int i = 0; i < writes.size(); ++i)
        d.append(char(i)).append(char(writes[i].second));
    d.append(char(0x7f)).append(char(3)); // 4 x 256 ms
    return d;
}

static void testMusic() {
    QString error;
    auto imf = Private::parseMusic(imfFile(560), MusicFormat::Imf, 560, &error);
    CHECK(imf && imf->chip == ChipType::OPL2);
    CHECK(imf && std::fabs(imf->duration - 1.0) < 1e-9);
    auto vgm = Private::parseMusic(vgmFile(false), MusicFormat::Auto, 560, &error);
    CHECK(vgm && vgm->chip == ChipType::OPL2);
    CHECK(vgm && std::fabs(vgm->duration - (44100.0 + 735.0) / 44100.0) < 1e-9);
    CHECK(vgm && vgm->loopIndex == std::size_t(noteWrites().size()) && vgm->loopTime == 0.0);
    auto vgm3 = Private::parseMusic(vgmFile(true), MusicFormat::Auto, 560, &error);
    CHECK(vgm3 && vgm3->chip == ChipType::OPL3);
    auto dro = Private::parseMusic(droV2File(), MusicFormat::Auto, 560, &error);
    CHECK(dro && dro->chip == ChipType::OPL2);
    CHECK(dro && std::fabs(dro->duration - 1.024) < 1e-9);
    CHECK(dro && dro->events.size() == std::size_t(noteWrites().size()));
#ifdef NT_HAVE_ZLIB
    {
        const QByteArray raw = vgmFile(false);
        QByteArray gz(raw.size() + 256, '\0');
        z_stream z{};
        deflateInit2(&z, 9, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
        z.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(raw.constData()));
        z.avail_in = uInt(raw.size());
        z.next_out = reinterpret_cast<Bytef *>(gz.data());
        z.avail_out = uInt(gz.size());
        deflate(&z, Z_FINISH);
        gz.resize(qsizetype(z.total_out));
        deflateEnd(&z);
        auto vgz = Private::parseMusic(gz, MusicFormat::Auto, 560, &error);
        CHECK(vgz && vgz->events.size() == vgm->events.size());
    }
#endif

    // Hostile input: nothing that parses badly crashes, and errors are reported.
    CHECK(!Private::parseMusic("Vgm ", MusicFormat::Auto, 560, &error));
    CHECK(!Private::parseMusic(QByteArray("\x1f\x8bgarbage"), MusicFormat::Auto, 560, &error));
    CHECK(!Private::parseMusic("DBRAWOPL", MusicFormat::Auto, 560, &error));
    CHECK(!Private::parseMusic("x", MusicFormat::Imf, 560, &error));
    CHECK(!Private::parseMusic(imfFile(10), MusicFormat::Vgm, 560, &error));
    quint32 rng = 99;
    for (int round = 0; round < 300; ++round) {
        QByteArray junk = round % 3 == 0 ? vgmFile(round % 2) : round % 3 == 1 ? droV2File() : imfFile(round);
        for (int k = 0; k < 8; ++k) {
            const auto at = qsizetype(Private::xorshift32(rng) % quint32(junk.size()));
            junk[at] = char(Private::xorshift32(rng));
        }
        junk.truncate(qsizetype(Private::xorshift32(rng) % quint32(junk.size() + 1)));
        auto music = Private::parseMusic(junk, MusicFormat::Auto, 560, &error);
        if (music) {
            std::vector<qint32> buf(64);
            for (const auto &e : music->events)
                music->core->write(e.reg, e.value);
            music->core->generate(buf.data(), 32);
        }
    }

    // Playing through the engine.
    Engine engine;
    MusicPlayer *player = engine.music();
    CHECK(player && !player->isLoaded());
    CHECK(player->imfRate() == 560);
    CHECK(!player->load("x", MusicFormat::Imf, &error));
    CHECK(player->load(imfFile(280), MusicFormat::Imf, &error));
    CHECK(player->isLoaded());
    CHECK(player->chip() == ChipType::OPL2);
    CHECK(std::fabs(player->duration() - 0.5) < 1e-9);
    player->setLoops(false);
    CHECK(!player->loops());
    CHECK(peak(renderEngine(engine, 4800)) == 0.0f); // loaded, not playing
    player->play();
    std::vector<float> out = renderEngine(engine, 4800);
    CHECK(player->isPlaying());
    CHECK(peak(out) > 0.05f);
    CHECK(std::fabs(frequency(out.data() + 2 * 1000, 3000, 2, 48000) - 440.0) < 5.0);
    CHECK(player->position() > 0.09 && player->position() < 0.11);
    player->pause();
    renderEngine(engine, 480);
    const double paused = player->position();
    renderEngine(engine, 4800);
    CHECK(player->position() == paused);
    CHECK(!player->isPlaying());
    bool finished = false;
    QObject::connect(&engine, &Engine::musicFinished, [&] { finished = true; });
    player->play();
    renderEngine(engine, 48000);
    CHECK(!player->isPlaying());
    spinEvents(50);
    CHECK(finished);
    // Looping music plays on.
    player->setGain(-6);
    player->setLoops(true);
    player->stop();
    player->play();
    renderEngine(engine, 48000 * 2);
    CHECK(player->isPlaying());
    player->stop();
    renderEngine(engine, 256);
    CHECK(!player->isPlaying());
    CHECK(player->position() == 0.0);
    // Replacing the music frees the old one on the control thread.
    CHECK(player->load(vgmFile(true), MusicFormat::Auto, &error));
    player->play();
    CHECK(peak(renderEngine(engine, 4800)) > 0.05f);
    player->setImfRate(700);
    CHECK(player->imfRate() == 700);
}

static void testSequence() {
    Sequence seq;
    CHECK(seq.tempo() == 120.0f);
    seq.setTempo(0);
    CHECK(seq.tempo() == 1.0f);
    seq.setTempo(120);
    const Patch square = tone(R"(, "operators": [{"level": 0}, {"level": 0}, {"level": 0},
        {"level": 0.5, "wave": "square", "env": {"a": 0, "d": 0, "s": 1, "r": 0}}], "duration": 0.01)");
    seq.addNote(0, square);
    seq.addNote(0.5f, square, 12, 0.02f, -6);
    seq.addNote(-1, square); // ignored
    seq.addNote(1, Patch()); // ignored
    CHECK(seq.noteCount() == 2);
    CHECK(seq.notes().at(1).pitch == 12.0f);
    CHECK(seq.duration() == 1.0f);
    CHECK(seq.secondsAt(0.5f) == 0.25);

    EngineConfig config;
    config.blockSize = 256;
    Engine engine(config);
    renderEngine(engine, 777);
    const QList<VoiceHandle> handles = engine.play(seq);
    CHECK(handles.size() == 2);
    const std::vector<float> out = renderEngine(engine, 20000);
    std::vector<int> onsets;
    for (int i = 0; i < 20000; ++i) {
        const bool on = out[std::size_t(2 * i)] != 0.0f;
        const bool before = i > 0 && out[std::size_t(2 * i - 2)] != 0.0f;
        if (on && !before)
            onsets.push_back(i);
    }
    CHECK(onsets.size() == 2);
    if (onsets.size() == 2)
        CHECK(onsets[1] - onsets[0] == 12000);
    seq.clear();
    CHECK(seq.noteCount() == 0);
    CHECK(engine.play(seq).isEmpty());
}

// An output with a real audio thread, pulling as fast as it can, while the control thread sends
// every kind of command: under ThreadSanitizer this checks the hand-over between the threads.
class ThreadedOutput : public AudioOutput {
public:
    bool open(int, int, SampleFormat) override {
        running = true;
        thread = std::thread([this] {
            std::vector<float> buffer(512);
            while (running.load())
                frames += pull(buffer.data(), 256);
        });
        return true;
    }
    void close() override {
        running = false;
        if (thread.joinable())
            thread.join();
    }
    std::atomic<bool> running{false};
    std::atomic<qint64> frames{0};
    std::thread thread;
};

static void testThreadedEngine() {
    EngineConfig config;
    config.maxVoices = 6;
    config.chips = {ChipType::OPL3};
    Engine engine(config);
    ThreadedOutput output;
    CHECK(engine.start(&output));
    QList<VoiceHandle> handles;
    QElapsedTimer timer;
    timer.start();
    int round = 0;
    int finished = 0;
    QObject::connect(&engine, &Engine::voiceFinished, [&](quint32) { ++finished; });
    while (timer.elapsed() < 1500) {
        const auto category = static_cast<Category>(round % 9);
        PlayOptions o;
        o.delay = float(round % 4) * 0.002f;
        handles.append(engine.play(
            Patch::generate(category, quint32(round), round % 3 == 0 ? ChipType::OPL2 : ChipType::None), o));
        handles.last().setPitch(float(round % 5));
        if (round % 4 == 0)
            handles[round / 2].stop(round % 8 == 0);
        engine.chip(ChipType::OPL3)->writeRegister(0xa8, quint8(round), 0.001f);
        if (round % 25 == 0)
            engine.music()->load(vgmFile(round % 50 == 0), MusicFormat::Auto);
        if (round % 25 == 1)
            engine.music()->play();
        if (round % 40 == 0)
            engine.stopAll();
        engine.setBusGain(0, -float(round % 6));
        (void)handles.last().isPlaying();
        (void)engine.activeVoices();
        (void)engine.music()->position();
        spinEvents(1);
        ++round;
    }
    engine.stop();
    CHECK(output.frames.load() > 48000);
    CHECK(finished > 0);
    CHECK(round > 100);
}

// --- MIDI -------------------------------------------------------------------------------------------

static QByteArray vlq(quint32 v) {
    QByteArray out(1, char(v & 0x7f));
    while (v >>= 7)
        out.prepend(char(0x80 | (v & 0x7f)));
    return out;
}

static QByteArray chunk(const char *id, const QByteArray &body) {
    QByteArray c(id, 4);
    char len[4];
    qToBigEndian<quint32>(quint32(body.size()), len);
    return c + QByteArray(len, 4) + body;
}

// A format-1 file at 480 ticks a quarter: a tempo track (120 bpm, then 240 bpm from tick 480) and a
// track of notes.
static QByteArray midiFile() {
    QByteArray tempo;
    tempo += vlq(0) + QByteArray("\xff\x51\x03\x07\xa1\x20", 6);   // 500000 us
    tempo += vlq(480) + QByteArray("\xff\x51\x03\x03\xd0\x90", 6); // 250000 us
    tempo += vlq(0) + QByteArray("\xff\x2f\x00", 3);
    QByteArray notes;
    auto ev = [&notes](quint32 delta, std::initializer_list<int> bytes) {
        notes += vlq(delta);
        for (int b : bytes)
            notes.append(char(b));
    };
    ev(0, {0xc0, 26});     // program 26 on channel 0
    ev(0, {0xb0, 101, 0}); // RPN 0: pitch bend range
    ev(0, {0xb0, 100, 0});
    ev(0, {0xb0, 6, 12});      // twelve semitones
    ev(0, {0x90, 60, 100});    // C4 at 0 s
    ev(480, {0x80, 60, 0});    // off at 0.5 s
    ev(0, {0x90, 64, 64});     // E4 at 0.5 s
    ev(480, {64, 0});          // running status, velocity 0: off at 0.75 s
    ev(0, {0xb0, 64, 127});    // sustain on
    ev(0, {0xb0, 7, 64});      // volume 64
    ev(0, {0xb0, 10, 0});      // pan left
    ev(0, {0xe0, 0x7f, 0x7f}); // bend fully up
    ev(0, {0x90, 67, 127});    // G4 at 0.75 s
    ev(120, {0x80, 67, 0});    // released at 0.8125 s, held by the pedal
    ev(240, {0xb0, 64, 0});    // pedal up at 0.9375 s
    ev(0, {0x99, 36, 100});    // kick on the drum channel
    ev(10, {0x89, 36, 0});
    ev(0, {0xff, 0x2f, 0x00});
    QByteArray header(6, '\0');
    qToBigEndian<quint16>(1, header.data());
    qToBigEndian<quint16>(2, header.data() + 2);
    qToBigEndian<quint16>(480, header.data() + 4);
    return chunk("MThd", header) + chunk("MTrk", tempo) + chunk("MTrk", notes);
}

static void testMidiFile() {
    QString error;
    const MidiFile f = MidiFile::fromData(midiFile(), &error);
    CHECK(f.isValid());
    CHECK(f.format() == 1);
    CHECK(f.trackCount() == 2);
    const QList<MidiFile::Note> n = f.notes();
    CHECK(n.size() == 4);
    if (n.size() != 4)
        return;
    CHECK(n[0].key == 60 && n[0].program == 26 && n[0].channel == 0);
    CHECK(std::fabs(n[0].time) < 1e-9 && std::fabs(n[0].length - 0.5) < 1e-9);
    CHECK(std::fabs(n[1].time - 0.5) < 1e-9 && std::fabs(n[1].length - 0.25) < 1e-9);
    CHECK(n[1].velocity == 64);
    CHECK(n[0].gain > n[1].gain); // softer velocity
    CHECK(std::fabs(n[2].time - 0.75) < 1e-9);
    CHECK(std::fabs(n[2].length - (0.9375 - 0.75)) < 1e-9); // held by the pedal
    CHECK(n[2].pan == -1.0f);
    CHECK(std::fabs(n[2].bend - 12.0f) < 0.01f);
    CHECK(n[2].gain < n[0].gain); // the channel volume dropped
    CHECK(n[3].channel == 9 && n[3].key == 36);
    CHECK(std::fabs(f.duration() - (0.9375 + 10 * 0.25 / 480)) < 1e-9);

    // SMPTE timing: 25 frames of 40 ticks, so a tick is a millisecond.
    QByteArray smpte = midiFile();
    smpte[12] = char(0xe7); // -25
    smpte[13] = char(40);
    const MidiFile s = MidiFile::fromData(smpte);
    CHECK(s.isValid() && std::fabs(s.notes().value(1).time - 0.48) < 1e-9);

    CHECK(!MidiFile::fromData("nope", &error).isValid());
    CHECK(!error.isEmpty());
    CHECK(!MidiFile().isValid());
    CHECK(!MidiFile::fromData(chunk("MThd", QByteArray(6, '\0'))).isValid()); // division 0

    // Hostile input never crashes.
    quint32 rng = 7;
    for (int round = 0; round < 500; ++round) {
        QByteArray junk = midiFile();
        for (int k = 0; k < 6; ++k) {
            const auto at = qsizetype(Private::xorshift32(rng) % quint32(junk.size()));
            junk[at] = char(Private::xorshift32(rng));
        }
        junk.truncate(qsizetype(Private::xorshift32(rng) % quint32(junk.size() + 1)));
        const MidiFile m = MidiFile::fromData(junk);
        for (const MidiFile::Note &note : m.notes())
            CHECK(note.length > 0 && std::isfinite(note.time));
    }
}

// One held note of program 0 from 0 to 1 s, with a change of controller at 0.5 s.
static QByteArray heldNoteMidi(std::initializer_list<int> change) {
    QByteArray track;
    auto ev = [&track](quint32 delta, std::initializer_list<int> bytes) {
        track += vlq(delta);
        for (int b : bytes)
            track.append(char(b));
    };
    ev(0, {0x90, 69, 127});
    track += vlq(480);
    for (int b : change)
        track.append(char(b));
    ev(480, {0x80, 69, 0});
    ev(0, {0xff, 0x2f, 0x00});
    QByteArray header(6, '\0');
    qToBigEndian<quint16>(0, header.data());
    qToBigEndian<quint16>(1, header.data() + 2);
    qToBigEndian<quint16>(480, header.data() + 4); // 480 a quarter at 120 bpm: 960 ticks a second
    return chunk("MThd", header) + chunk("MTrk", track);
}

static std::vector<float> playMidi(Engine &engine, MidiPlayer &player, int frames) {
    std::vector<float> out;
    std::vector<float> block(2 * 256);
    player.play();
    for (int done = 0; done < frames; done += 256) {
        player.schedule(engine.frame() + 256);
        engine.render(block.data(), 256);
        out.insert(out.end(), block.begin(), block.end());
    }
    return out;
}

static float rmsOf(const std::vector<float> &x, int from, int to) {
    double sum = 0;
    for (int i = from; i < to; ++i)
        sum += double(x[std::size_t(2 * i)]) * double(x[std::size_t(2 * i)]);
    return float(std::sqrt(sum / std::max(to - from, 1)));
}

static void testMidiControls() {
    const MidiFile f = MidiFile::fromData(midiFile());
    const QList<MidiFile::Control> c = f.controls();
    CHECK(c.size() == 3);
    if (c.size() == 3) {
        CHECK(c[0].kind == MidiFile::Control::Level && std::fabs(c[0].time - 0.75) < 1e-9);
        CHECK(c[1].kind == MidiFile::Control::Pan && c[1].value == -1.0f);
        CHECK(c[2].kind == MidiFile::Control::Bend && std::fabs(c[2].value - 12.0f) < 0.01f);
    }

    // A pure tone as program 0, so that its pitch and level can be measured.
    PatchBank bank = PatchBank::generalMidi();
    bank.insert(QStringLiteral("program-0"), tone(R"(, "duration": -1)"));

    // The volume drops to 20 half way through a held note.
    {
        Engine engine;
        MidiPlayer player(&engine);
        player.setInstruments(bank);
        player.setFile(MidiFile::fromData(heldNoteMidi({0xb0, 7, 20})));
        const std::vector<float> out = playMidi(engine, player, 48000);
        const float before = rmsOf(out, 4800, 23000);
        const float after = rmsOf(out, 26000, 46000);
        CHECK(before > 0.05f);
        CHECK(after < before * 0.1f); // 40 log10(20/127) is about -32 dB
    }
    // A pitch bend of two semitones half way through.
    {
        Engine engine;
        MidiPlayer player(&engine);
        player.setInstruments(bank);
        player.setFile(MidiFile::fromData(heldNoteMidi({0xe0, 0x7f, 0x7f})));
        const std::vector<float> out = playMidi(engine, player, 48000);
        CHECK(std::fabs(frequency(out.data() + 2 * 4800, 18000, 2, 48000) - 440.0) < 2.0);
        CHECK(std::fabs(frequency(out.data() + 2 * 26000, 18000, 2, 48000) - 493.9) < 3.0);
    }

    // A timed parameter lands on its frame.
    {
        Engine engine;
        VoiceHandle v = engine.play(tone(R"(, "duration": -1, "loop": true)"));
        renderEngine(engine, 1000);
        v.setParameterAt(ParamId::Gain, -120, engine.frame() + 2000);
        const std::vector<float> out = renderEngine(engine, 4000);
        // Unchanged up to the frame, then smoothed down over a few milliseconds.
        CHECK(std::fabs(rmsOf(out, 0, 1990) - 0.25f) < 0.01f);
        CHECK(rmsOf(out, 3000, 4000) < 0.01f);
    }
}

static void testMidiOnOpl() {
    const PatchBank opl = PatchBank::generalMidi(ChipType::OPL3);
    CHECK(opl.size() == 25);
    for (const QString &name : opl.names()) {
        const Patch p = opl.patch(name);
        CHECK(p.isValid());
        if (name.startsWith(QLatin1String("gm-"))) {
            CHECK(p.chip() == ChipType::OPL2);
            CHECK(p.fitsChip(ChipType::OPL2));
            CHECK(p.data()->baseNote == 60.0f);
        } else {
            CHECK(p.chip() == ChipType::None);
        }
    }
    CHECK(PatchBank::generalMidi(ChipType::OPN2) == PatchBank::generalMidi());

    // A note of the bank plays on the chip, with the velocity as the chip's level.
    auto peakAt = [&opl](int velocity) {
        EngineConfig config;
        config.chips = {ChipType::OPL3};
        Engine engine(config);
        MidiPlayer player(&engine);
        player.setInstruments(opl);
        QByteArray track = vlq(0) + QByteArray("\xc0\x18", 2); // guitar
        track += vlq(0) + QByteArray("\x90\x45", 2) + QByteArray(1, char(velocity));
        track += vlq(480) + QByteArray("\x80\x45\x00", 3) + vlq(0) + QByteArray("\xff\x2f\x00", 3);
        QByteArray header(6, '\0');
        qToBigEndian<quint16>(1, header.data() + 2);
        qToBigEndian<quint16>(960, header.data() + 4);
        player.setFile(MidiFile::fromData(chunk("MThd", header) + chunk("MTrk", track)));
        const std::vector<float> out = playMidi(engine, player, 9600);
        CHECK(engine.activeVoices() == 1);
        return peak(out);
    };
    const float loud = peakAt(127);
    const float soft = peakAt(32);
    CHECK(loud > 0.02f);
    CHECK(soft < loud * 0.5f && soft > 0.0f);
}

static void testMidiPlayer() {
    const PatchBank gm = PatchBank::generalMidi();
    CHECK(gm.size() == 25);
    for (const QString &name : gm.names()) {
        const Patch p = gm.patch(name);
        CHECK(p.isValid());
        PlayOptions o;
        o.duration = 0.2f;
        if (peak(leftChannel(Engine::renderToWav(p, 22050, o))) < 0.02f)
            std::printf("  silent: %s\n", qPrintable(name));
    }

    EngineConfig config;
    config.maxVoices = 16;
    Engine engine(config);
    MidiPlayer player(&engine);
    CHECK(player.instrument(0, 26, 60).name() == QLatin1String("gm-guitar"));
    CHECK(player.instrument(9, 0, 36).name() == QLatin1String("drum-kick"));
    CHECK(player.instrument(9, 0, 70).name() == QLatin1String("drum-rim"));
    PatchBank custom = gm;
    custom.insert(QStringLiteral("program-26"), Patch::generate(Category::Blip, 1));
    custom.insert(QStringLiteral("drum-36"), Patch::generate(Category::Hit, 1));
    player.setInstruments(custom);
    CHECK(player.instrument(0, 26, 60).name() == QLatin1String("program-26"));
    CHECK(player.instrument(0, 27, 60).name() == QLatin1String("gm-guitar"));
    CHECK(player.instrument(9, 0, 36).name() == QLatin1String("drum-36"));
    player.setInstruments(gm);
    CHECK(player.instruments().size() == 25);

    // Offline: each note starts on its sample. The first note of the file starts at frame 1000.
    renderEngine(engine, 1000);
    player.setFile(MidiFile::fromData(midiFile()));
    CHECK(player.file().notes().size() == 4);
    int finished = 0;
    QObject::connect(&player, &MidiPlayer::finished, [&finished] { ++finished; });
    player.play();
    CHECK(player.isPlaying());
    std::vector<float> out;
    std::vector<float> block(2 * 256);
    while (player.isPlaying() && out.size() < 48000u * 2 * 5) {
        player.schedule(engine.frame() + 256);
        engine.render(block.data(), 256);
        out.insert(out.end(), block.begin(), block.end());
    }
    CHECK(finished == 1);
    CHECK(!player.isPlaying());
    int first = -1;
    for (std::size_t i = 0; i < out.size() / 2 && first < 0; ++i) {
        if (out[2 * i] != 0.0f)
            first = int(i);
    }
    CHECK(first >= 0 && first <= 2); // the first note starts on frame 0, with a sine at phase 0
    // Rendered for about the length of the music and its tail.
    CHECK(out.size() / 2 >= std::size_t(48000 * 1.9) && out.size() / 2 < std::size_t(48000 * 2.1));
    CHECK(peak(out) > 0.05f);

    // Looping music plays on; stop() releases it.
    player.setLoops(true);
    CHECK(player.loops());
    player.setGain(-6);
    CHECK(player.gain() == -6.0f);
    player.setTranspose(12);
    CHECK(player.transpose() == 12);
    player.play();
    for (int i = 0; i < 48000 * 4 / 256; ++i) {
        player.schedule(engine.frame() + 256);
        engine.render(block.data(), 256);
    }
    CHECK(player.isPlaying());
    // About 4 s in: the second pass of the 0.94 s music, by its loop length.
    CHECK(std::fabs(player.position() - std::fmod(4.0, player.file().duration())) < 0.01);
    player.stop();
    CHECK(!player.isPlaying());
    CHECK(player.position() == 0.0);
    renderEngine(engine, 48000);
    CHECK(engine.activeVoices() == 0);

    // An invalid file does not play.
    player.setFile(MidiFile());
    player.play();
    CHECK(!player.isPlaying());
}

static void testRealTimeSafety() {
    EngineConfig config;
    config.maxVoices = 8;
    config.seed = 3;
    config.chips = {ChipType::OPL2, ChipType::OPL3};
    Engine engine(config);
    QString error;
    CHECK(engine.music()->load(vgmFile(true), MusicFormat::Auto, &error));
    engine.music()->play();
    std::vector<float> out(512);
    std::vector<qint16> pcm(512);
    QList<VoiceHandle> handles;
    for (int round = 0; round < 200; ++round) {
        // Control side: plays (beyond the voice count, so voices are stolen), stops and parameters.
        const auto category = static_cast<Category>(round % 9);
        PlayOptions o;
        o.delay = float(round % 3) * 0.001f;
        o.priority = round % 2;
        handles.append(engine.play(
            Patch::generate(category, quint32(round), round % 4 == 0 ? ChipType::OPL2 : ChipType::None), o));
        engine.chip(ChipType::OPL3)->writeRegister(0xa0, quint8(round), 0.001f);
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
    testOplCore();
    testEngineChips();
    testInstruments();
    testBanks();
    testMusic();
    testSequence();
    testMidiFile();
    testMidiPlayer();
    testMidiControls();
    testMidiOnOpl();
    testThreadedEngine();
    testRealTimeSafety();
    testQueues();
    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("All Nt FM tests passed\n");
    return 0;
}
