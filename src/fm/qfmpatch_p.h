// MIT License - see LICENSE
#pragma once

// The fields of a patch, in the units of the JSON format: seconds, Hz, semitones, MIDI notes,
// linear levels 0..1 and decibels where the format says so.

#include <QtCore/qmap.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>
#include <QtFM/qfmglobal.h>

namespace QtFM {

struct FmEnvelope {
    float delay = 0;
    float attack = 0.002f;
    float hold = 0;
    float decay = 0.3f;
    float sustain = 0;
    float release = 0.05f;
    float curve = -3;
    bool operator==(const FmEnvelope &o) const;
};

struct FmSweep {
    float start = 0;
    float end = 0;
    float time = 0;
    float curve = 0;
    bool operator==(const FmSweep &o) const;
};

struct FmOperator {
    float ratio = 1;
    float offset = 0;
    float fixed = 0;
    float level = 1;
    Waveform wave = Waveform::Sine;
    float keyScaleRate = 0;
    float keyScaleLevel = 0;
    FmEnvelope env;
    bool operator==(const FmOperator &o) const;
};

enum class NoiseMode { White, Lfsr7, Lfsr15 };
enum class FilterType { None, LowPass, HighPass, BandPass };
enum class LfoWave { Sine, Triangle, Square, Saw, SampleHold };

class PatchData : public QSharedData {
public:
    QString name;
    QString category;
    ChipType chip = ChipType::None;
    int algorithm = 0;
    float feedback = 0;
    int opCount = 4;
    FmOperator ops[4];

    float baseNote = 69;
    FmSweep pitchEnv;
    float slide = 0;
    float deltaSlide = 0;
    float arpSteps[4] = {0, 0, 0, 0};
    int arpCount = 0;
    float arpRate = 0;
    bool arpRepeat = false;

    LfoWave lfoWave = LfoWave::Sine;
    float lfoRate = 5;
    float lfoPitch = 0;
    float lfoAmp = 0;
    float lfoIndex = 0;
    float lfoCutoff = 0;

    FmSweep index{1, 1, 0, 0};

    NoiseMode noiseMode = NoiseMode::White;
    float noiseLevel = 0;
    float noiseRate = 0;
    int noiseModulate = 0;
    FmEnvelope noiseEnv;

    FilterType filterType = FilterType::None;
    float cutoff = 20000;
    float resonance = 0;
    float sweepEnd = 0;
    float sweepTime = 0;
    float sweepCurve = 0;

    float crushBits = 16;
    float crushRate = 0;

    float gain = 0;
    float pan = 0;
    bool loop = false;
    float duration = -1;

    QMap<QString, float> random;

    bool operator==(const PatchData &o) const;

    // Clamps every field into its documented range.
    void normalise();
    // Returns the field a "random" path names, or nullptr.
    float *field(const QString &path);
    // Applies the "random" block, drawing from the xorshift32 state in seed.
    void randomise(quint32 &seed);
};

namespace Private {
inline quint32 xorshift32(quint32 &state) {
    quint32 x = state ? state : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    return x;
}
// Returns a float in [-1, 1).
inline float randomBipolar(quint32 &state) {
    return static_cast<float>(xorshift32(state) >> 8) * (2.0f / 16777216.0f) - 1.0f;
}
// Returns a float in [lo, hi).
inline float randomRange(quint32 &state, float lo, float hi) {
    return lo + (hi - lo) * static_cast<float>(xorshift32(state) >> 8) * (1.0f / 16777216.0f);
}
// The ratio of each value of an OPL multiplier register, 0..15.
extern const float oplMultiples[16];
// Returns the native patch of the 11 OPL2 instrument registers at r, in the SBI order.
PatchData *patchFromOpl(const quint8 *r, const QString &name);
} // namespace Private

} // namespace QtFM
