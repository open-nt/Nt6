// MIT License - see LICENSE

#include "qfmpatch_p.h"

#include "qfmopl_p.h"

#include <QtCore/qcborvalue.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtFM/qfmpatch.h>

#include <algorithm>
#include <cmath>

namespace QtFM {

namespace Private {
const float oplMultiples[16] = {0.5f, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10, 12, 12, 15, 15};
} // namespace Private

using Private::randomBipolar;
using Private::randomRange;
using Private::xorshift32;

namespace {

const char *const waveNames[] = {"sine", "half-sine", "abs-sine", "quarter-sine", "square", "saw", "triangle"};
const char *const noiseNames[] = {"white", "lfsr7", "lfsr15"};
const char *const filterNames[] = {"none", "lowpass", "highpass", "bandpass"};
const char *const lfoNames[] = {"sine", "triangle", "square", "saw", "sample-hold"};
const char *const chipNames[] = {"", "opl2", "opl3", "opll", "opn2", "opm"};
const char *const categoryNames[] = {"pickup", "laser", "explosion", "powerup", "hit",
                                     "jump",   "blip",  "alarm",     "engine"};

template <std::size_t N> int indexOf(const char *const (&names)[N], const QString &name, int fallback) {
    QString key = name.toLower();
    key.remove(QLatin1Char('_'));
    for (std::size_t i = 0; i < N; ++i) {
        QString candidate = QString::fromLatin1(names[i]);
        if (candidate == key || candidate.remove(QLatin1Char('-')) == QString(key).remove(QLatin1Char('-')))
            return static_cast<int>(i);
    }
    return fallback;
}

float num(const QJsonObject &o, const char *key, float fallback) {
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isDouble() ? static_cast<float>(v.toDouble()) : fallback;
}

// Parses a note: a MIDI number, or a name such as "A4", "C#5" or "Bb3" (A4 = 69).
float parseNote(const QJsonValue &v, float fallback) {
    if (v.isDouble())
        return static_cast<float>(v.toDouble());
    const QString s = v.toString().trimmed();
    if (s.isEmpty())
        return fallback;
    static const int semitones[] = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
    const QChar letter = s.at(0).toUpper();
    if (letter < QLatin1Char('A') || letter > QLatin1Char('G'))
        return fallback;
    int note = semitones[letter.unicode() - 'A'];
    qsizetype i = 1;
    while (i < s.size() && (s.at(i) == QLatin1Char('#') || s.at(i) == QLatin1Char('b'))) {
        note += s.at(i) == QLatin1Char('#') ? 1 : -1;
        ++i;
    }
    bool ok = false;
    const int octave = s.mid(i).toInt(&ok);
    if (!ok)
        return fallback;
    return static_cast<float>(12 * (octave + 1) + note);
}

FmEnvelope readEnv(const QJsonObject &o, const FmEnvelope &defaults) {
    FmEnvelope e = defaults;
    e.delay = num(o, "delay", e.delay);
    e.attack = num(o, "a", e.attack);
    e.hold = num(o, "h", e.hold);
    e.decay = num(o, "d", e.decay);
    e.sustain = num(o, "s", e.sustain);
    e.release = num(o, "r", e.release);
    e.curve = num(o, "curve", e.curve);
    return e;
}

QJsonObject writeEnv(const FmEnvelope &e) {
    QJsonObject o;
    o.insert(QLatin1String("delay"), e.delay);
    o.insert(QLatin1String("a"), e.attack);
    o.insert(QLatin1String("h"), e.hold);
    o.insert(QLatin1String("d"), e.decay);
    o.insert(QLatin1String("s"), e.sustain);
    o.insert(QLatin1String("r"), e.release);
    o.insert(QLatin1String("curve"), e.curve);
    return o;
}

FmSweep readSweep(const QJsonObject &o, const FmSweep &defaults) {
    FmSweep s = defaults;
    s.start = num(o, "start", s.start);
    s.end = num(o, "end", s.end);
    s.time = num(o, "time", s.time);
    s.curve = num(o, "curve", s.curve);
    return s;
}

QJsonObject writeSweep(const FmSweep &s) {
    QJsonObject o;
    o.insert(QLatin1String("start"), s.start);
    o.insert(QLatin1String("end"), s.end);
    o.insert(QLatin1String("time"), s.time);
    o.insert(QLatin1String("curve"), s.curve);
    return o;
}

bool readPatch(const QJsonObject &root, PatchData &p, QString *error) {
    const QJsonValue format = root.value(QLatin1String("format"));
    if (!format.isUndefined() && format.toString() != QLatin1String("qtfm-patch")) {
        if (error)
            *error = QStringLiteral("not a qtfm-patch document");
        return false;
    }
    if (root.value(QLatin1String("version")).toInt(1) > 1) {
        if (error)
            *error = QStringLiteral("unsupported patch version %1").arg(root.value(QLatin1String("version")).toInt());
        return false;
    }
    p.name = root.value(QLatin1String("name")).toString();
    p.category = root.value(QLatin1String("category")).toString();
    const QJsonValue chip = root.value(QLatin1String("chip"));
    p.chip = chip.isString() ? static_cast<ChipType>(indexOf(chipNames, chip.toString(), 0)) : ChipType::None;
    p.algorithm = static_cast<int>(num(root, "algorithm", 0));
    p.feedback = num(root, "feedback", 0);
    const QJsonArray ops = root.value(QLatin1String("operators")).toArray();
    // Two operators make a two-operator patch; otherwise missing operators are silent.
    p.opCount = ops.size() == 2 ? 2 : 4;
    for (int i = 0; i < 4; ++i) {
        FmOperator &op = p.ops[i];
        op = FmOperator();
        if (i >= ops.size()) {
            if (!ops.isEmpty())
                op.level = 0;
            continue;
        }
        const QJsonObject o = ops.at(i).toObject();
        op.ratio = num(o, "ratio", op.ratio);
        op.offset = num(o, "offset", op.offset);
        op.fixed = num(o, "fixed", op.fixed);
        op.level = num(o, "level", op.level);
        op.wave = static_cast<Waveform>(indexOf(waveNames, o.value(QLatin1String("wave")).toString(), 0));
        op.keyScaleRate = num(o, "keyScaleRate", op.keyScaleRate);
        op.keyScaleLevel = num(o, "keyScaleLevel", op.keyScaleLevel);
        op.env = readEnv(o.value(QLatin1String("env")).toObject(), op.env);
    }
    if (ops.isEmpty()) {
        // No operators: one sine carrier.
        for (int i = 0; i < 3; ++i)
            p.ops[i].level = 0;
    }

    const QJsonObject pitch = root.value(QLatin1String("pitch")).toObject();
    p.baseNote = parseNote(pitch.value(QLatin1String("base")), 69);
    p.pitchEnv = readSweep(pitch.value(QLatin1String("env")).toObject(), FmSweep());
    p.slide = num(pitch, "slide", 0);
    p.deltaSlide = num(pitch, "deltaSlide", 0);
    const QJsonObject arp = pitch.value(QLatin1String("arp")).toObject();
    const QJsonArray steps = arp.value(QLatin1String("steps")).toArray();
    p.arpCount = static_cast<int>(std::min<qsizetype>(steps.size(), 4));
    for (int i = 0; i < 4; ++i)
        p.arpSteps[i] = i < p.arpCount ? static_cast<float>(steps.at(i).toDouble()) : 0.0f;
    p.arpRate = num(arp, "rate", 0);
    p.arpRepeat = arp.value(QLatin1String("repeat")).toBool(false);

    const QJsonObject lfo = root.value(QLatin1String("lfo")).toObject();
    p.lfoWave = static_cast<LfoWave>(indexOf(lfoNames, lfo.value(QLatin1String("wave")).toString(), 0));
    p.lfoRate = num(lfo, "rate", 5);
    p.lfoPitch = num(lfo, "pitch", 0);
    p.lfoAmp = num(lfo, "amp", 0);
    p.lfoIndex = num(lfo, "index", 0);
    p.lfoCutoff = num(lfo, "cutoff", 0);

    p.index = readSweep(root.value(QLatin1String("index")).toObject(), FmSweep{1, 1, 0, 0});

    const QJsonObject noise = root.value(QLatin1String("noise")).toObject();
    p.noiseMode = static_cast<NoiseMode>(indexOf(noiseNames, noise.value(QLatin1String("mode")).toString(), 0));
    p.noiseLevel = num(noise, "level", 0);
    p.noiseRate = num(noise, "rate", 0);
    p.noiseModulate = static_cast<int>(num(noise, "modulate", 0));
    p.noiseEnv = readEnv(noise.value(QLatin1String("env")).toObject(), FmEnvelope());

    const QJsonObject filter = root.value(QLatin1String("filter")).toObject();
    p.filterType = static_cast<FilterType>(indexOf(filterNames, filter.value(QLatin1String("type")).toString(), 0));
    p.cutoff = num(filter, "cutoff", 20000);
    p.resonance = num(filter, "resonance", 0);
    const QJsonObject sweep = filter.value(QLatin1String("sweep")).toObject();
    p.sweepEnd = num(sweep, "end", 0);
    p.sweepTime = num(sweep, "time", 0);
    p.sweepCurve = num(sweep, "curve", 0);

    const QJsonObject crush = root.value(QLatin1String("crush")).toObject();
    p.crushBits = num(crush, "bits", 16);
    p.crushRate = num(crush, "rate", 0);

    p.gain = num(root, "gain", 0);
    p.pan = num(root, "pan", 0);
    p.loop = root.value(QLatin1String("loop")).toBool(false);
    p.duration = num(root, "duration", -1);

    p.random.clear();
    const QJsonObject random = root.value(QLatin1String("random")).toObject();
    for (auto it = random.begin(); it != random.end(); ++it) {
        if (it.value().isDouble() && p.field(it.key()))
            p.random.insert(it.key(), static_cast<float>(it.value().toDouble()));
    }
    p.normalise();
    return true;
}

QJsonObject writePatch(const PatchData &p) {
    QJsonObject root;
    root.insert(QLatin1String("format"), QLatin1String("qtfm-patch"));
    root.insert(QLatin1String("version"), 1);
    root.insert(QLatin1String("name"), p.name);
    root.insert(QLatin1String("category"), p.category);
    root.insert(QLatin1String("chip"), p.chip == ChipType::None
                                           ? QJsonValue(QJsonValue::Null)
                                           : QJsonValue(QLatin1String(chipNames[static_cast<int>(p.chip)])));
    root.insert(QLatin1String("algorithm"), p.algorithm);
    root.insert(QLatin1String("feedback"), p.feedback);
    QJsonArray ops;
    for (int i = 0; i < p.opCount; ++i) {
        const FmOperator &op = p.ops[i];
        QJsonObject o;
        o.insert(QLatin1String("ratio"), op.ratio);
        o.insert(QLatin1String("offset"), op.offset);
        o.insert(QLatin1String("fixed"), op.fixed);
        o.insert(QLatin1String("level"), op.level);
        o.insert(QLatin1String("wave"), QLatin1String(waveNames[static_cast<int>(op.wave)]));
        o.insert(QLatin1String("keyScaleRate"), op.keyScaleRate);
        o.insert(QLatin1String("keyScaleLevel"), op.keyScaleLevel);
        o.insert(QLatin1String("env"), writeEnv(op.env));
        ops.append(o);
    }
    root.insert(QLatin1String("operators"), ops);

    QJsonObject pitch;
    pitch.insert(QLatin1String("base"), p.baseNote);
    pitch.insert(QLatin1String("env"), writeSweep(p.pitchEnv));
    pitch.insert(QLatin1String("slide"), p.slide);
    pitch.insert(QLatin1String("deltaSlide"), p.deltaSlide);
    QJsonObject arp;
    QJsonArray steps;
    for (int i = 0; i < p.arpCount; ++i)
        steps.append(p.arpSteps[i]);
    arp.insert(QLatin1String("steps"), steps);
    arp.insert(QLatin1String("rate"), p.arpRate);
    arp.insert(QLatin1String("repeat"), p.arpRepeat);
    pitch.insert(QLatin1String("arp"), arp);
    root.insert(QLatin1String("pitch"), pitch);

    QJsonObject lfo;
    lfo.insert(QLatin1String("wave"), QLatin1String(lfoNames[static_cast<int>(p.lfoWave)]));
    lfo.insert(QLatin1String("rate"), p.lfoRate);
    lfo.insert(QLatin1String("pitch"), p.lfoPitch);
    lfo.insert(QLatin1String("amp"), p.lfoAmp);
    lfo.insert(QLatin1String("index"), p.lfoIndex);
    lfo.insert(QLatin1String("cutoff"), p.lfoCutoff);
    root.insert(QLatin1String("lfo"), lfo);
    root.insert(QLatin1String("index"), writeSweep(p.index));

    QJsonObject noise;
    noise.insert(QLatin1String("mode"), QLatin1String(noiseNames[static_cast<int>(p.noiseMode)]));
    noise.insert(QLatin1String("level"), p.noiseLevel);
    noise.insert(QLatin1String("rate"), p.noiseRate);
    noise.insert(QLatin1String("modulate"), p.noiseModulate);
    noise.insert(QLatin1String("env"), writeEnv(p.noiseEnv));
    root.insert(QLatin1String("noise"), noise);

    QJsonObject filter;
    filter.insert(QLatin1String("type"), QLatin1String(filterNames[static_cast<int>(p.filterType)]));
    filter.insert(QLatin1String("cutoff"), p.cutoff);
    filter.insert(QLatin1String("resonance"), p.resonance);
    QJsonObject sweep;
    sweep.insert(QLatin1String("end"), p.sweepEnd);
    sweep.insert(QLatin1String("time"), p.sweepTime);
    sweep.insert(QLatin1String("curve"), p.sweepCurve);
    filter.insert(QLatin1String("sweep"), sweep);
    root.insert(QLatin1String("filter"), filter);

    QJsonObject crush;
    crush.insert(QLatin1String("bits"), p.crushBits);
    crush.insert(QLatin1String("rate"), p.crushRate);
    root.insert(QLatin1String("crush"), crush);

    QJsonObject random;
    for (auto it = p.random.cbegin(); it != p.random.cend(); ++it)
        random.insert(it.key(), it.value());
    root.insert(QLatin1String("random"), random);
    root.insert(QLatin1String("gain"), p.gain);
    root.insert(QLatin1String("pan"), p.pan);
    root.insert(QLatin1String("loop"), p.loop);
    root.insert(QLatin1String("duration"), p.duration);
    return root;
}

void clampEnv(FmEnvelope &e) {
    e.delay = std::clamp(e.delay, 0.0f, 10.0f);
    e.attack = std::clamp(e.attack, 0.0f, 10.0f);
    e.hold = std::clamp(e.hold, 0.0f, 10.0f);
    e.decay = std::clamp(e.decay, 0.0f, 10.0f);
    e.sustain = std::clamp(e.sustain, 0.0f, 1.0f);
    e.release = std::clamp(e.release, 0.0f, 10.0f);
    e.curve = std::clamp(e.curve, -20.0f, 20.0f);
}

void clampSweep(FmSweep &s, float range) {
    s.start = std::clamp(s.start, -range, range);
    s.end = std::clamp(s.end, -range, range);
    s.time = std::clamp(s.time, 0.0f, 10.0f);
    s.curve = std::clamp(s.curve, -20.0f, 20.0f);
}

bool finite(float x) { return std::isfinite(x); }

} // namespace

bool FmEnvelope::operator==(const FmEnvelope &o) const {
    return delay == o.delay && attack == o.attack && hold == o.hold && decay == o.decay && sustain == o.sustain &&
           release == o.release && curve == o.curve;
}

bool FmSweep::operator==(const FmSweep &o) const {
    return start == o.start && end == o.end && time == o.time && curve == o.curve;
}

bool FmOperator::operator==(const FmOperator &o) const {
    return ratio == o.ratio && offset == o.offset && fixed == o.fixed && level == o.level && wave == o.wave &&
           keyScaleRate == o.keyScaleRate && keyScaleLevel == o.keyScaleLevel && env == o.env;
}

bool PatchData::operator==(const PatchData &o) const {
    if (opCount != o.opCount)
        return false;
    for (int i = 0; i < opCount; ++i) {
        if (!(ops[i] == o.ops[i]))
            return false;
    }
    for (int i = 0; i < 4; ++i) {
        if (arpSteps[i] != o.arpSteps[i])
            return false;
    }
    return name == o.name && category == o.category && chip == o.chip && algorithm == o.algorithm &&
           feedback == o.feedback && baseNote == o.baseNote && pitchEnv == o.pitchEnv && slide == o.slide &&
           deltaSlide == o.deltaSlide && arpCount == o.arpCount && arpRate == o.arpRate && arpRepeat == o.arpRepeat &&
           lfoWave == o.lfoWave && lfoRate == o.lfoRate && lfoPitch == o.lfoPitch && lfoAmp == o.lfoAmp &&
           lfoIndex == o.lfoIndex && lfoCutoff == o.lfoCutoff && index == o.index && noiseMode == o.noiseMode &&
           noiseLevel == o.noiseLevel && noiseRate == o.noiseRate && noiseModulate == o.noiseModulate &&
           noiseEnv == o.noiseEnv && filterType == o.filterType && cutoff == o.cutoff && resonance == o.resonance &&
           sweepEnd == o.sweepEnd && sweepTime == o.sweepTime && sweepCurve == o.sweepCurve &&
           crushBits == o.crushBits && crushRate == o.crushRate && gain == o.gain && pan == o.pan && loop == o.loop &&
           duration == o.duration && random == o.random;
}

void PatchData::normalise() {
    // Non-finite values from a hostile file are replaced rather than reaching the audio thread.
    auto fix = [](float &x, float fallback) {
        if (!finite(x))
            x = fallback;
    };
    algorithm = std::clamp(algorithm, 0, opCount == 2 ? 1 : 7);
    fix(feedback, 0);
    feedback = std::clamp(feedback, 0.0f, 7.0f);
    for (FmOperator &op : ops) {
        fix(op.ratio, 1);
        fix(op.offset, 0);
        fix(op.fixed, 0);
        fix(op.level, 1);
        fix(op.keyScaleRate, 0);
        fix(op.keyScaleLevel, 0);
        for (float *f : {&op.env.delay, &op.env.attack, &op.env.hold, &op.env.decay, &op.env.sustain, &op.env.release,
                         &op.env.curve})
            fix(*f, 0);
        op.ratio = std::clamp(op.ratio, 0.5f, 32.0f);
        op.offset = std::clamp(op.offset, -1000.0f, 1000.0f);
        op.fixed = std::clamp(op.fixed, 0.0f, 20000.0f);
        op.level = std::clamp(op.level, 0.0f, 1.0f);
        op.keyScaleRate = std::clamp(op.keyScaleRate, 0.0f, 1.0f);
        op.keyScaleLevel = std::clamp(op.keyScaleLevel, 0.0f, 24.0f);
        clampEnv(op.env);
    }
    for (float *f : {&baseNote,   &pitchEnv.start, &pitchEnv.end, &pitchEnv.time, &pitchEnv.curve, &slide,
                     &deltaSlide, &arpRate,        &lfoRate,      &lfoPitch,      &lfoAmp,         &lfoIndex,
                     &lfoCutoff,  &index.start,    &index.end,    &index.time,    &index.curve,    &noiseLevel,
                     &noiseRate,  &cutoff,         &resonance,    &sweepEnd,      &sweepTime,      &sweepCurve,
                     &crushBits,  &crushRate,      &gain,         &pan,           &duration})
        fix(*f, 0);
    for (float &s : arpSteps)
        fix(s, 0);
    baseNote = std::clamp(baseNote, 0.0f, 127.0f);
    clampSweep(pitchEnv, 48);
    slide = std::clamp(slide, -480.0f, 480.0f);
    deltaSlide = std::clamp(deltaSlide, -4800.0f, 4800.0f);
    for (float &s : arpSteps)
        s = std::clamp(s, -48.0f, 48.0f);
    arpCount = std::clamp(arpCount, 0, 4);
    arpRate = std::clamp(arpRate, 0.0f, 100.0f);
    lfoRate = std::clamp(lfoRate, 0.01f, 50.0f);
    lfoPitch = std::clamp(lfoPitch, -48.0f, 48.0f);
    lfoAmp = std::clamp(lfoAmp, 0.0f, 1.0f);
    lfoIndex = std::clamp(lfoIndex, -1.0f, 1.0f);
    lfoCutoff = std::clamp(lfoCutoff, -8.0f, 8.0f);
    clampSweep(index, 16);
    index.start = std::max(index.start, 0.0f);
    index.end = std::max(index.end, 0.0f);
    noiseLevel = std::clamp(noiseLevel, 0.0f, 1.0f);
    noiseRate = std::clamp(noiseRate, 0.0f, 200000.0f);
    noiseModulate = std::clamp(noiseModulate, 0, opCount);
    clampEnv(noiseEnv);
    cutoff = std::clamp(cutoff, 20.0f, 20000.0f);
    resonance = std::clamp(resonance, 0.0f, 1.0f);
    sweepEnd = sweepEnd <= 0.0f ? 0.0f : std::clamp(sweepEnd, 20.0f, 20000.0f);
    sweepTime = std::clamp(sweepTime, 0.0f, 10.0f);
    sweepCurve = std::clamp(sweepCurve, -20.0f, 20.0f);
    crushBits = std::clamp(crushBits, 1.0f, 16.0f);
    crushRate = std::clamp(crushRate, 0.0f, 200000.0f);
    gain = std::clamp(gain, -96.0f, 24.0f);
    pan = std::clamp(pan, -1.0f, 1.0f);
    duration = duration < 0.0f ? -1.0f : std::min(duration, 600.0f);
}

float *PatchData::field(const QString &path) {
    const QStringList parts = path.split(QLatin1Char('.'));
    if (parts.size() >= 3 && parts.at(0) == QLatin1String("operators")) {
        bool ok = false;
        const int i = parts.at(1).toInt(&ok);
        if (!ok || i < 0 || i >= 4)
            return nullptr;
        FmOperator &op = ops[i];
        const QString &f = parts.at(2);
        if (parts.size() == 3) {
            if (f == QLatin1String("ratio"))
                return &op.ratio;
            if (f == QLatin1String("offset"))
                return &op.offset;
            if (f == QLatin1String("fixed"))
                return &op.fixed;
            if (f == QLatin1String("level"))
                return &op.level;
            return nullptr;
        }
        if (parts.size() == 4 && f == QLatin1String("env")) {
            const QString &e = parts.at(3);
            if (e == QLatin1String("delay"))
                return &op.env.delay;
            if (e == QLatin1String("a"))
                return &op.env.attack;
            if (e == QLatin1String("h"))
                return &op.env.hold;
            if (e == QLatin1String("d"))
                return &op.env.decay;
            if (e == QLatin1String("s"))
                return &op.env.sustain;
            if (e == QLatin1String("r"))
                return &op.env.release;
        }
        return nullptr;
    }
    static const struct {
        const char *path;
        float PatchData::*member;
    } simple[] = {
        {"feedback", &PatchData::feedback},
        {"pitch.base", &PatchData::baseNote},
        {"pitch.slide", &PatchData::slide},
        {"pitch.deltaSlide", &PatchData::deltaSlide},
        {"lfo.rate", &PatchData::lfoRate},
        {"lfo.pitch", &PatchData::lfoPitch},
        {"lfo.amp", &PatchData::lfoAmp},
        {"noise.level", &PatchData::noiseLevel},
        {"noise.rate", &PatchData::noiseRate},
        {"filter.cutoff", &PatchData::cutoff},
        {"filter.resonance", &PatchData::resonance},
        {"gain", &PatchData::gain},
        {"pan", &PatchData::pan},
        {"duration", &PatchData::duration},
    };
    for (const auto &s : simple) {
        if (path == QLatin1String(s.path))
            return &(this->*s.member);
    }
    if (path == QLatin1String("pitch.env.start"))
        return &pitchEnv.start;
    if (path == QLatin1String("pitch.env.end"))
        return &pitchEnv.end;
    if (path == QLatin1String("pitch.env.time"))
        return &pitchEnv.time;
    return nullptr;
}

void PatchData::randomise(quint32 &seed) {
    if (random.isEmpty())
        return;
    // Draw in key order, so the same seed gives the same sound whatever order the file used.
    for (auto it = random.cbegin(); it != random.cend(); ++it) {
        float *f = field(it.key());
        const float r = randomBipolar(seed);
        if (f)
            *f += r * it.value();
    }
    normalise();
}

// --- Patch ---------------------------------------------------------------------------------------

/*!
    \namespace QtFM
    \inmodule NtFM
    \brief The QtFM namespace holds Nt FM: an FM synthesiser for game sound effects.

    \sa {Using Nt FM}
*/

/*!
    \module NtFM
    \title Nt FM C++ Classes
    \brief Real-time FM synthesis of game sound effects from patches of a few hundred bytes.
    \cmaketarget Nt::FM

    Nt FM synthesises lasers, coins, jumps, explosions, alarms and engine loops with four-operator
    FM, from JSON patches instead of sample files. QtFM::Engine plays QtFM::Patch values in real time
    or renders them offline; QtFM::AudioOutput connects it to a sound device. Qt has no such module.

    See \l{Using Nt FM} for a walk through and the patch format, and \c{examples/fm/fmplay.cpp}
    for a complete program.
*/

/*!
    \enum QtFM::SampleFormat

    The sample format of an engine's output.

    \value Float32 32-bit floating point samples in [-1, 1].
    \value Q15 16-bit signed fixed-point samples.
*/

/*!
    \enum QtFM::ChipType

    A Yamaha FM chip that a patch targets. Patch::fitsChip() checks a patch against the chip's limits.

    \value None No chip: the native engine, without limits.
    \value OPL2 The YM3812 of the AdLib and Sound Blaster: two operators, four waveforms.
    \value OPL3 The YMF262 of the Sound Blaster Pro 2 and 16: two or four operators, eight waveforms.
    \value OPLL The YM2413 of MSX-Music: two operators, sine and half-sine.
    \value OPN2 The YM2612 of the Mega Drive: four operators, sine only.
    \value OPM The YM2151 of arcade boards and the X68000: four operators, sine only.
*/

/*!
    \enum QtFM::Category

    A kind of sound effect that Patch::generate() makes.

    \value Pickup A coin or pick-up: a short arpeggio.
    \value Laser A laser or zap: a falling pitch sweep.
    \value Explosion An explosion: filtered noise and heavy feedback.
    \value PowerUp A power-up: a rising sweep with an arpeggio.
    \value Hit A hit or punch: a noise burst over a falling thud.
    \value Jump A jump: a short rising sweep.
    \value Blip A user-interface blip.
    \value Alarm An alarm: a tone warbled by a square LFO.
    \value Engine A looping engine hum, for live pitch control.
*/

/*!
    \enum QtFM::InstrumentFormat

    A file format of chip instruments that Patch::fromChipInstrument() reads.

    \value Sbi The Sound Blaster Instrument format of OPL2 instruments (52 bytes).
    \value Ibk A bank of 128 SBI-like instruments with names, as PatchBank::fromChipBank() reads.
*/

/*!
    \enum QtFM::MusicFormat

    A file format of chip music that MusicPlayer::load() reads.

    \value Auto Recognised by its header: VGM, VGZ, DRO, and otherwise IMF.
    \value Vgm Video Game Music logs, gzipped (VGZ) or not, with an OPL2 or OPL3 part.
    \value Dro DOSBox Raw OPL captures, versions 0.1 and 2.0.
    \value Imf id Software Music Format, types 0 and 1, at MusicPlayer::imfRate().
*/

/*!
    \enum QtFM::Waveform

    The waveform of an operator.

    \value Sine A sine.
    \value HalfSine The positive half of a sine, then silence (OPL2 waveform 1).
    \value AbsSine The absolute value of a sine (OPL2 waveform 2).
    \value QuarterSine The rising quarters of an absolute sine (OPL2 waveform 3).
    \value Square A square wave.
    \value Saw A rising sawtooth.
    \value Triangle A triangle wave.
*/

/*!
    \enum QtFM::ParamId

    A parameter of a playing voice that VoiceHandle::setParameter() changes. Changes are smoothed
    over about 5 ms on the audio thread.

    \value Pitch Semitones added to the voice's pitch.
    \value Gain Gain in decibels.
    \value Pan Pan, from -1 (left) to 1 (right).
    \value FilterCutoff The filter cutoff in Hz, replacing the patch's cutoff and sweep.
    \value FilterResonance The filter resonance, 0 to 1.
    \value LfoRate The LFO rate in Hz.
    \value ModulationIndex A factor applied to every modulator, 1 by default.
*/

/*!
    \class QtFM::Patch
    \inmodule NtFM
    \brief The Patch class holds one complete, self-describing sound.

    A patch describes four operators and their routing, envelopes, pitch movement, an LFO, noise,
    a filter, a bit crusher and a block of random variance. It plays at any pitch and length, and
    is stored as JSON of a few hundred bytes:

    \snippet fm/fmplay.cpp patch

    Patch is a value type and implicitly shared. A default-constructed patch is invalid; make one
    with fromJson(), fromCbor(), fromChipInstrument() or generate(). Unknown JSON keys are ignored,
    missing keys take their defaults, and out-of-range values are clamped, so isValid() is false
    only for a document that is not a patch at all.

    \differences
    A patch cannot yet be edited field by field from C++: edit its JSON. Only the SBI instrument
    format is imported, and patches are not yet exported to chip formats.
    \enddifferences

    \sa QtFM::Engine, {Using Nt FM}
*/

/*!
    Constructs an invalid patch.
*/
Patch::Patch() = default;

Patch::Patch(PatchData *data) : d(data) {}

/*!
    Constructs a copy of \a other.
*/
Patch::Patch(const Patch &other) = default;

/*!
    Move-constructs a patch from \a other.
*/
Patch::Patch(Patch &&other) noexcept = default;

/*!
    Assigns \a other to this patch and returns a reference to it.
*/
Patch &Patch::operator=(const Patch &other) = default;

/*!
    Move-assigns \a other to this patch and returns a reference to it.
*/
Patch &Patch::operator=(Patch &&other) noexcept = default;

/*!
    Destroys the patch.
*/
Patch::~Patch() = default;

/*!
    Returns the patch in the JSON document \a json, or an invalid patch, with the reason in
    \a error, if it is not a patch.

    \sa toJson()
*/
Patch Patch::fromJson(const QByteArray &json, QString *error) {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (!doc.isObject()) {
        if (error)
            *error = doc.isNull() ? parseError.errorString() : QStringLiteral("a patch is a JSON object");
        return Patch();
    }
    auto *data = new PatchData;
    if (!readPatch(doc.object(), *data, error)) {
        delete data;
        return Patch();
    }
    return Patch(data);
}

/*!
    Returns the patch in the CBOR encoding \a cbor, as toCbor() writes it, or an invalid patch, with
    the reason in \a error.
*/
Patch Patch::fromCbor(const QByteArray &cbor, QString *error) {
    QCborParserError parseError;
    const QCborValue value = QCborValue::fromCbor(cbor, &parseError);
    if (!value.isMap()) {
        if (error)
            *error = QStringLiteral("a patch is a CBOR map");
        return Patch();
    }
    auto *data = new PatchData;
    if (!readPatch(value.toJsonValue().toObject(), *data, error)) {
        delete data;
        return Patch();
    }
    return Patch(data);
}

/*!
    Returns the JSON document of the patch, with every field written, so that fromJson() gives back
    an equal patch. Returns an empty array for an invalid patch.
*/
QByteArray Patch::toJson() const {
    if (!d)
        return QByteArray();
    return QJsonDocument(writePatch(*d)).toJson(QJsonDocument::Compact);
}

/*!
    Returns the patch encoded as CBOR, a smaller form for embedded targets.

    \sa fromCbor()
*/
QByteArray Patch::toCbor() const {
    if (!d)
        return QByteArray();
    return QCborValue::fromJsonValue(writePatch(*d)).toCbor();
}

/*!
    Returns \c true if the patch holds a sound.
*/
bool Patch::isValid() const { return d; }

/*!
    Returns the name of the patch.
*/
QString Patch::name() const { return d ? d->name : QString(); }

/*!
    Returns the category of the patch, such as \c laser, as written in its \c category field.
*/
QString Patch::category() const { return d ? d->category : QString(); }

/*!
    Returns the chip that the patch targets, or ChipType::None.
*/
ChipType Patch::chip() const { return d ? d->chip : ChipType::None; }

/*!
    Returns the number of operators of the patch: 2 or 4.
*/
int Patch::operatorCount() const { return d ? d->opCount : 0; }

/*!
    Returns \c true if the patch sustains until it is stopped.
*/
bool Patch::isLoop() const { return d && d->loop; }

/*!
    Returns the length of a note in seconds before it is released, or -1 if the patch has no fixed
    length: it then runs through its envelopes once, or sustains if isLoop().
*/
float Patch::duration() const { return d ? d->duration : -1.0f; }

/*!
    Returns \c true if this patch and \a other hold the same sound.
*/
bool Patch::operator==(const Patch &other) const {
    if (d == other.d)
        return true;
    if (!d || !other.d)
        return false;
    return *d == *other.d;
}

/*!
    \fn bool QtFM::Patch::operator!=(const QtFM::Patch &other) const

    Returns \c true if this patch and \a other differ.
*/

/*!
    Returns a variation of the patch: its times, ratios, levels, pitch movement and cutoff are each
    scaled by a random factor of up to \a amount (0.1 for 10%), drawn from \a seed. A patch for a
    chip keeps its ratios on the chip's multipliers.
*/
Patch Patch::mutated(float amount, quint32 seed) const {
    if (!d)
        return Patch();
    auto *p = new PatchData(*d);
    quint32 s = seed ? seed : 1;
    amount = std::clamp(amount, 0.0f, 1.0f);
    auto scale = [&](float &x) { x *= 1.0f + amount * randomBipolar(s); };
    for (int i = 0; i < p->opCount; ++i) {
        FmOperator &op = p->ops[i];
        scale(op.ratio);
        if (p->chip == ChipType::OPL2 || p->chip == ChipType::OPL3 || p->chip == ChipType::OPLL) {
            const float *best =
                std::min_element(std::begin(Private::oplMultiples), std::end(Private::oplMultiples),
                                 [&](float a, float b) { return std::fabs(a - op.ratio) < std::fabs(b - op.ratio); });
            op.ratio = *best;
        } else if (p->chip == ChipType::OPN2 || p->chip == ChipType::OPM) {
            op.ratio = op.ratio < 0.75f ? 0.5f : std::round(std::min(op.ratio, 15.0f));
        }
        scale(op.level);
        scale(op.env.attack);
        scale(op.env.decay);
        scale(op.env.release);
    }
    scale(p->pitchEnv.start);
    scale(p->pitchEnv.end);
    scale(p->pitchEnv.time);
    scale(p->cutoff);
    scale(p->noiseEnv.decay);
    p->baseNote += amount * 2.0f * randomBipolar(s);
    p->normalise();
    return Patch(p);
}

/*!
    Returns \c true if the patch can be played by \a chip as it is: its operator count, waveforms,
    multipliers, feedback and envelopes are within the chip's limits, and it uses no noise, filter,
    crusher or modulation-index envelope. The fields that are not are appended to \a violations.
    Every patch fits ChipType::None.
*/
bool Patch::fitsChip(ChipType chip, QStringList *violations) const {
    QStringList found;
    if (!d) {
        found.append(QStringLiteral("invalid patch"));
    } else if (chip != ChipType::None) {
        const PatchData &p = *d;
        const bool opl = chip == ChipType::OPL2 || chip == ChipType::OPL3 || chip == ChipType::OPLL;
        const bool twoOpOnly = chip == ChipType::OPL2 || chip == ChipType::OPLL;
        if (twoOpOnly && p.opCount != 2)
            found.append(QStringLiteral("operators: the chip has two operators per channel"));
        for (int i = 0; i < p.opCount; ++i) {
            const FmOperator &op = p.ops[i];
            const QString prefix = QStringLiteral("operators.%1.").arg(i);
            bool waveOk = op.wave == Waveform::Sine;
            if (chip == ChipType::OPL2)
                waveOk = op.wave <= Waveform::QuarterSine;
            else if (chip == ChipType::OPL3)
                waveOk = op.wave <= Waveform::Square;
            else if (chip == ChipType::OPLL)
                waveOk = op.wave <= Waveform::HalfSine;
            if (!waveOk)
                found.append(prefix + QStringLiteral("wave"));
            const bool ratioOk =
                opl ? std::find(std::begin(Private::oplMultiples), std::end(Private::oplMultiples), op.ratio) !=
                          std::end(Private::oplMultiples)
                    : (op.ratio == 0.5f || (op.ratio >= 1.0f && op.ratio <= 15.0f && op.ratio == std::round(op.ratio)));
            if (!ratioOk)
                found.append(prefix + QStringLiteral("ratio"));
            if (op.fixed > 0.0f)
                found.append(prefix + QStringLiteral("fixed"));
            if (op.offset != 0.0f && opl)
                found.append(prefix + QStringLiteral("offset"));
            if (op.env.delay > 0.0f || op.env.hold > 0.0f)
                found.append(prefix + QStringLiteral("env"));
        }
        if (p.feedback != std::round(p.feedback))
            found.append(QStringLiteral("feedback"));
        if (p.noiseLevel > 0.0f)
            found.append(QStringLiteral("noise"));
        if (p.filterType != FilterType::None)
            found.append(QStringLiteral("filter"));
        if (p.crushBits < 16.0f || p.crushRate > 0.0f)
            found.append(QStringLiteral("crush"));
        if (!(p.index == FmSweep{1, 1, 0, 0}))
            found.append(QStringLiteral("index"));
        // The OPL chips have a fixed vibrato (7 or 14 cents) and tremolo (1 or 4.8 dB), and no other LFO.
        const bool oplLfo = (p.lfoPitch == 0.0f || p.lfoPitch == 0.07f || p.lfoPitch == 0.14f) &&
                            (p.lfoAmp == 0.0f || p.lfoAmp == 0.11f || p.lfoAmp == 0.55f) && p.lfoIndex == 0.0f &&
                            p.lfoCutoff == 0.0f;
        if (opl && !oplLfo)
            found.append(QStringLiteral("lfo"));
    }
    if (violations)
        violations->append(found);
    return found.isEmpty();
}

/*!
    Returns the native patch of a chip instrument in \a data, stored in \a format, or an invalid
    patch, with the reason in \a error. The parameters map onto the native engine: an OPL2
    instrument becomes a two-operator patch with the same multipliers, waveforms, feedback,
    connection and envelopes. The result sounds close to the chip, without its quantisation.
*/
Patch Patch::fromChipInstrument(const QByteArray &data, InstrumentFormat format, QString *error) {
    if (format != InstrumentFormat::Sbi || data.size() < 47 || !data.startsWith("SBI\x1a")) {
        if (error)
            *error = format == InstrumentFormat::Sbi ? QStringLiteral("not an SBI instrument")
                                                     : QStringLiteral("a bank format: use PatchBank::fromChipBank()");
        return Patch();
    }
    const QByteArray name = data.mid(4, 32);
    return Patch(Private::patchFromOpl(
        reinterpret_cast<const quint8 *>(data.constData()) + 36,
        QString::fromLatin1(name.constData(), static_cast<qsizetype>(qstrnlen(name.constData(), 32))).trimmed()));
}

/*!
    Returns the patch as a chip instrument in \a format, or an empty array, with the reason in
    \a error, if the patch cannot be expressed in it. Only InstrumentFormat::Sbi is a single
    instrument; it needs a patch of two operators, and maps it to the nearest OPL2 values.

    \sa fitsChip(), QtFM::PatchBank::toChipBank()
*/
QByteArray Patch::toChipInstrument(InstrumentFormat format, QString *error) const {
    quint8 regs[11];
    if (format != InstrumentFormat::Sbi || !d || !Private::oplInstrument(*d, regs)) {
        if (error)
            *error = format != InstrumentFormat::Sbi
                         ? QStringLiteral("not a single-instrument format")
                         : QStringLiteral("an SBI instrument needs a patch of two operators");
        return QByteArray();
    }
    QByteArray sbi("SBI\x1a", 4);
    QByteArray name = d->name.toLatin1().left(31);
    name.resize(32, '\0');
    sbi += name;
    sbi.append(reinterpret_cast<const char *>(regs), 11);
    sbi.append(QByteArray(5, '\0'));
    return sbi;
}

namespace Private {

PatchData *patchFromOpl(const quint8 *r, const QString &name) {
    auto *p = new PatchData;
    p->name = name;
    p->chip = ChipType::OPL2;
    p->category = QStringLiteral("instrument");
    p->opCount = 2;
    // Times of the OPL2 envelope at rate 1, for 0 to 100% (attack) and 0 to -96 dB (decay), halving
    // with each step of the rate.
    auto attackTime = [](int rate) { return rate == 0 ? 10.0f : rate == 15 ? 0.0f : 2.826f / float(1 << (rate - 1)); };
    auto decayTime = [](int rate) { return rate == 0 ? 10.0f : 39.28f / float(1 << (rate - 1)); };
    for (int op = 0; op < 2; ++op) {
        FmOperator &o = p->ops[op];
        const quint8 chr = r[op];
        const quint8 ksl = r[2 + op];
        const quint8 ad = r[4 + op];
        const quint8 sr = r[6 + op];
        const quint8 wave = r[8 + op];
        o.ratio = oplMultiples[chr & 0x0f];
        o.keyScaleRate = (chr & 0x10) ? 0.5f : 0.0f;
        static const float kslDb[] = {0.0f, 3.0f, 1.5f, 6.0f};
        o.keyScaleLevel = kslDb[ksl >> 6];
        o.level = (ksl & 0x3f) == 63 ? 0.0f : std::pow(10.0f, -0.75f * float(ksl & 0x3f) / 20.0f);
        o.wave = static_cast<Waveform>(wave & 0x03);
        const int sl = sr >> 4;
        o.env.attack = attackTime(ad >> 4);
        const float sustainDb = sl == 15 ? 93.0f : 3.0f * float(sl);
        o.env.sustain = sl == 15 ? 0.0f : std::pow(10.0f, -sustainDb / 20.0f);
        // The decay rate is for the whole 96 dB range; the decay stage only falls to the sustain level.
        o.env.decay = (ad & 0x0f) == 15 ? 0.0f : decayTime(ad & 0x0f) * std::min(sustainDb, 96.0f) / 96.0f;
        o.env.release = decayTime(sr & 0x0f);
        o.env.curve = -4.6f;
        if (chr & 0x80)
            p->lfoAmp = 0.11f; // 1 dB
        if (chr & 0x40)
            p->lfoPitch = 0.07f;
    }
    p->lfoRate = p->lfoAmp > 0.0f ? 3.7f : 6.1f;
    p->feedback = float((r[10] >> 1) & 0x07);
    p->algorithm = r[10] & 0x01;
    p->baseNote = 60;
    // A sustaining envelope (EG-TYP set) holds until note-off; a percussive one releases once it
    // reaches the sustain level, as a one-shot patch does.
    p->loop = (r[1] & 0x20) != 0;
    p->normalise();
    return p;
}

} // namespace Private

/*!
    Returns a random but plausible patch of \a category, drawn from \a seed: the same seed always
    gives the same patch. If \a target is a chip, the patch keeps to that chip's limits (two
    operators and the OPL waveforms for ChipType::OPL2, no noise or filter), for authentic chip
    sounds.

    \snippet fm/fmplay.cpp generate
*/
Patch Patch::generate(Category category, quint32 seed, ChipType target) {
    quint32 s = seed ? seed : 1;
    for (int i = 0; i < 4; ++i)
        xorshift32(s);
    auto r = [&](float lo, float hi) { return randomRange(s, lo, hi); };
    auto chance = [&](float p) { return randomRange(s, 0, 1) < p; };
    auto *p = new PatchData;
    p->name = QStringLiteral("%1-%2").arg(QLatin1String(categoryNames[static_cast<int>(category)])).arg(seed);
    p->category = QLatin1String(categoryNames[static_cast<int>(category)]);
    p->chip = target;
    const bool chip = target != ChipType::None;
    const bool twoOp = target == ChipType::OPL2 || target == ChipType::OPLL;
    p->opCount = twoOp ? 2 : 4;
    p->algorithm = twoOp ? 0 : 4;
    p->pitchEnv = FmSweep();
    for (int i = 0; i < 4; ++i)
        p->ops[i] = FmOperator();
    if (!twoOp) {
        // Two pairs of modulator and carrier; the second pair stays silent unless a recipe uses it.
        p->ops[2].level = 0;
        p->ops[3].level = 0;
    }
    FmOperator &mod = p->ops[0];
    FmOperator &car = p->ops[1];
    auto env = [&](FmOperator &op, float a, float d, float sus, float rel) {
        op.env.attack = a;
        op.env.decay = d;
        op.env.sustain = sus;
        op.env.release = rel;
    };
    const Waveform bright = target == ChipType::OPN2 || target == ChipType::OPM || target == ChipType::OPLL
                                ? Waveform::Sine
                                : (chip ? Waveform::QuarterSine : Waveform::Square);

    switch (category) {
    case Category::Pickup:
        p->baseNote = std::round(r(76, 88));
        mod.ratio = chance(0.5f) ? 1 : 2;
        mod.level = r(0.1f, 0.35f);
        env(mod, 0, 0.4f, 0, 0.05f);
        car.wave = chance(0.6f) ? bright : Waveform::Sine;
        env(car, 0, r(0.25f, 0.45f), 0, 0.05f);
        p->arpCount = 2;
        p->arpSteps[1] = std::round(r(4, 12));
        p->arpRate = r(10, 18);
        break;
    case Category::Laser:
        p->baseNote = std::round(r(76, 92));
        p->pitchEnv = {r(4, 16), -r(12, 36), r(0.1f, 0.3f), -2};
        mod.ratio = chip ? Private::oplMultiples[1 + int(r(0, 6))] : std::round(r(1, 7) * 2) / 2;
        mod.level = r(0.3f, 0.9f);
        env(mod, 0, r(0.08f, 0.2f), 0, 0.02f);
        car.wave = chance(0.3f) ? bright : Waveform::Sine;
        env(car, 0, r(0.12f, 0.3f), 0, 0.04f);
        p->feedback = std::round(r(0, 4));
        if (!chip)
            p->index = {1, r(0.1f, 0.5f), r(0.1f, 0.3f), 0};
        break;
    case Category::Explosion:
        p->baseNote = std::round(r(28, 40));
        p->pitchEnv = {0, -r(6, 18), r(0.3f, 0.8f), -2};
        p->feedback = chip ? 7 : std::round(r(5, 7));
        mod.ratio = chip ? 1 : r(0.5f, 1.5f);
        mod.level = r(0.6f, 1.0f);
        env(mod, 0, r(0.4f, 1.0f), 0, 0.2f);
        env(car, 0, r(0.5f, 1.2f), 0, 0.3f);
        if (!chip) {
            p->noiseMode = chance(0.5f) ? NoiseMode::White : NoiseMode::Lfsr15;
            p->noiseLevel = r(0.6f, 1.0f);
            p->noiseRate = r(2000, 9000);
            p->noiseEnv.attack = 0;
            p->noiseEnv.decay = r(0.5f, 1.4f);
            p->noiseEnv.curve = -4;
            p->filterType = FilterType::LowPass;
            p->cutoff = r(2000, 5000);
            p->sweepEnd = r(120, 400);
            p->sweepTime = r(0.4f, 1.0f);
            p->sweepCurve = -3;
            p->resonance = r(0, 0.3f);
        }
        break;
    case Category::PowerUp:
        p->baseNote = std::round(r(60, 72));
        p->pitchEnv = {-12, r(7, 19), r(0.3f, 0.6f), 0};
        mod.ratio = 1;
        mod.level = r(0.1f, 0.4f);
        env(mod, 0, 0.6f, 0.3f, 0.1f);
        car.wave = bright;
        env(car, 0.01f, r(0.45f, 0.7f), 0, 0.08f);
        p->arpCount = 4;
        p->arpSteps[1] = 4;
        p->arpSteps[2] = 7;
        p->arpSteps[3] = 12;
        p->arpRate = r(14, 24);
        p->arpRepeat = true;
        break;
    case Category::Hit:
        p->baseNote = std::round(r(40, 55));
        p->pitchEnv = {r(6, 12), -r(6, 12), r(0.05f, 0.12f), -3};
        mod.ratio = chip ? 1 : r(0.5f, 2);
        mod.level = r(0.4f, 0.9f);
        p->feedback = std::round(r(3, 6));
        env(mod, 0, 0.08f, 0, 0.02f);
        env(car, 0, r(0.08f, 0.18f), 0, 0.03f);
        if (!chip) {
            p->noiseLevel = r(0.3f, 0.7f);
            p->noiseEnv.attack = 0;
            p->noiseEnv.decay = r(0.03f, 0.1f);
            p->filterType = FilterType::LowPass;
            p->cutoff = r(1500, 4000);
        }
        break;
    case Category::Jump:
        p->baseNote = std::round(r(62, 74));
        p->pitchEnv = {-r(4, 10), r(4, 12), r(0.15f, 0.3f), -2};
        mod.level = r(0.05f, 0.3f);
        env(mod, 0, 0.3f, 0, 0.05f);
        car.wave = chance(0.5f) ? bright : (chip ? Waveform::Sine : Waveform::Triangle);
        env(car, 0, r(0.2f, 0.35f), 0, 0.05f);
        break;
    case Category::Blip:
        p->baseNote = std::round(r(72, 96));
        mod.level = r(0, 0.3f);
        mod.ratio = chance(0.5f) ? 1 : 2;
        env(mod, 0, 0.05f, 0, 0.01f);
        car.wave = chance(0.5f) ? bright : Waveform::Sine;
        env(car, 0, r(0.03f, 0.08f), 0, 0.02f);
        break;
    case Category::Alarm:
        p->baseNote = std::round(r(70, 82));
        p->duration = r(0.8f, 1.5f);
        mod.ratio = chance(0.5f) ? 1 : 2;
        mod.level = r(0.2f, 0.5f);
        env(mod, 0.01f, 0.1f, 1, 0.05f);
        car.wave = chip ? bright : (chance(0.5f) ? Waveform::Saw : Waveform::Square);
        env(car, 0.01f, 0.1f, 1, 0.08f);
        if (target == ChipType::OPL2 || target == ChipType::OPL3 || target == ChipType::OPLL) {
            // The OPL chips have no free LFO: warble with a repeating two-step arpeggio instead.
            p->arpCount = 2;
            p->arpSteps[1] = std::round(r(3, 7));
            p->arpRate = r(4, 12);
            p->arpRepeat = true;
        } else {
            p->lfoWave = LfoWave::Square;
            p->lfoRate = r(2, 6);
            p->lfoPitch = std::round(r(3, 7));
        }
        if (!chip) {
            p->filterType = FilterType::LowPass;
            p->cutoff = r(2500, 6000);
        }
        break;
    case Category::Engine:
        p->baseNote = std::round(r(28, 40));
        p->loop = true;
        mod.ratio = chance(0.5f) ? 1 : 0.5f;
        mod.level = r(0.4f, 0.8f);
        env(mod, 0.05f, 0.1f, 1, 0.2f);
        car.wave = chip ? bright : Waveform::Saw;
        env(car, 0.05f, 0.1f, 1, 0.25f);
        p->feedback = std::round(r(2, 5));
        if (!chip || target == ChipType::OPN2 || target == ChipType::OPM) {
            p->lfoWave = LfoWave::Sine;
            p->lfoRate = r(8, 25);
            p->lfoAmp = r(0.2f, 0.5f);
        }
        if (!chip) {
            p->noiseMode = NoiseMode::Lfsr7;
            p->noiseLevel = r(0.05f, 0.2f);
            p->noiseRate = r(200, 1200);
            p->noiseEnv = car.env;
            p->filterType = FilterType::LowPass;
            p->cutoff = r(700, 2000);
            p->random.insert(QStringLiteral("lfo.rate"), 2);
        }
        break;
    }
    if (category != Category::Engine)
        p->random.insert(QStringLiteral("pitch.base"),
                         category == Category::Explosion || category == Category::Hit ? 2.0f : 1.0f);
    for (int i = 0; i < p->opCount; ++i) {
        if (chip)
            p->ops[i].env.curve = -4.6f;
    }
    p->gain = -6;
    p->normalise();
    return Patch(p);
}

} // namespace QtFM
