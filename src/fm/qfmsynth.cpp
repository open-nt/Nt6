// MIT License - see LICENSE
//
// The native FM synthesiser: four phase-modulation operators per voice, routed by one of the eight
// OPN/OPM algorithms, with DAHDSR envelopes, a pitch envelope, slide and arpeggio, one LFO, a noise
// source, a state-variable filter and a bit crusher. Renderer::render() runs on the audio thread and
// never allocates, locks or blocks: the voices and the queue of timed commands are allocated in the
// constructor, and finished programs go back to the control thread through the release callback.

#include "qfmsynth_p.h"

#include "qfmmusic_p.h"
#include "qfmopl_p.h"

#include <QtCore/qendian.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#if defined(__SSE2__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#define NT_FM_X86_FTZ 1
#endif

namespace QtFM::Private {

namespace {

constexpr int kTableBits = 12;
constexpr int kTableSize = 1 << kTableBits;
constexpr int kControlInterval = 16;
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPow32 = 4294967296.0f;
constexpr float kStealFade = 0.002f;
constexpr float kSmoothTime = 0.005f;
constexpr float kMaxModulation = 256.0f;

struct SineTable {
    float values[kTableSize + 1];
    SineTable() {
        for (int i = 0; i <= kTableSize; ++i)
            values[i] = static_cast<float>(std::sin(2.0 * 3.14159265358979323846 * i / kTableSize));
    }
};

inline float sineAt(const float *table, quint32 phase) {
    const quint32 index = phase >> (32 - kTableBits);
    const float frac = static_cast<float>(phase & ((1u << (32 - kTableBits)) - 1)) * (1.0f / (1u << (32 - kTableBits)));
    return table[index] + (table[index + 1] - table[index]) * frac;
}

inline float waveAt(Waveform wave, const float *table, quint32 phase) {
    switch (wave) {
    case Waveform::Sine:
        return sineAt(table, phase);
    case Waveform::HalfSine:
        return phase < 0x80000000u ? sineAt(table, phase) : 0.0f;
    case Waveform::AbsSine:
        return std::fabs(sineAt(table, phase));
    case Waveform::QuarterSine:
        return (phase & 0x40000000u) ? 0.0f : std::fabs(sineAt(table, phase));
    case Waveform::Square:
        return phase < 0x80000000u ? 1.0f : -1.0f;
    case Waveform::Saw:
        return static_cast<float>(phase) * (2.0f / kTwoPow32) - 1.0f;
    case Waveform::Triangle: {
        const float x = static_cast<float>(phase) * (1.0f / kTwoPow32);
        return x < 0.5f ? 4.0f * x - 1.0f : 3.0f - 4.0f * x;
    }
    }
    return 0.0f;
}

// Returns the shape of a curved segment at x in [0, 1]: linear for curve 0, fast-start for a
// negative curve (exponential decay) and slow-start for a positive one.
inline float curveAt(float x, float curve) {
    x = std::clamp(x, 0.0f, 1.0f);
    if (std::fabs(curve) < 1e-3f)
        return x;
    return (1.0f - std::exp(curve * x)) / (1.0f - std::exp(curve));
}

inline float sweepAt(const FmSweep &sweep, float t) {
    if (sweep.time <= 0.0f)
        return sweep.end;
    return sweep.start + (sweep.end - sweep.start) * curveAt(t / sweep.time, sweep.curve);
}

inline float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }

inline float noteToHz(float note) { return 440.0f * std::exp2((note - 69.0f) / 12.0f); }

inline quint32 toSamples(float seconds, float sampleRate, float scale) {
    const float n = seconds * sampleRate * scale;
    return n <= 0.0f ? 0u : static_cast<quint32>(std::min(n, 4.0e9f));
}

// Leaves the signal untouched up to kKnee, then bends it smoothly towards full scale.
constexpr float kKnee = 0.75f;
inline float softClip(float x) {
    const float a = std::fabs(x);
    if (a <= kKnee)
        return x;
    const float y = kKnee + (1.0f - kKnee) * std::tanh((a - kKnee) / (1.0f - kKnee));
    return x < 0.0f ? -y : y;
}

// A DAHDSR envelope. A segment of n samples follows curveAt(); the exponential is advanced by a
// constant factor per sample, so the audio thread calls exp() once per segment.
struct Env {
    enum Stage : quint8 { Delay, Attack, Hold, Decay, Sustain, Release, Off };
    Stage stage = Off;
    bool oneShot = false;
    float value = 0;
    float from = 0;
    float to = 0;
    float e = 1;
    float mul = 1;
    float norm = 1;
    float sustain = 0;
    float curve = 0;
    quint32 length = 0;
    quint32 count = 0;
    quint32 lengths[6] = {};

    void setup(const FmEnvelope &env, float sampleRate, float rateScale, bool isOneShot) {
        lengths[Delay] = toSamples(env.delay, sampleRate, 1.0f);
        lengths[Attack] = toSamples(env.attack, sampleRate, rateScale);
        lengths[Hold] = toSamples(env.hold, sampleRate, 1.0f);
        lengths[Decay] = toSamples(env.decay, sampleRate, rateScale);
        lengths[Release] = toSamples(env.release, sampleRate, rateScale);
        sustain = env.sustain;
        curve = env.curve;
        oneShot = isOneShot;
        value = 0;
        enter(Delay);
    }

    void segment(float target, quint32 n) {
        from = value;
        to = target;
        length = n;
        count = 0;
        if (std::fabs(curve) >= 1e-3f && n > 0) {
            e = 1.0f;
            mul = std::exp(curve / static_cast<float>(n));
            norm = 1.0f / (1.0f - std::exp(curve));
        }
    }

    void enter(Stage s) {
        for (;;) {
            stage = s;
            switch (s) {
            case Delay:
                value = 0;
                if (lengths[Delay] == 0) {
                    s = Attack;
                    continue;
                }
                segment(0, lengths[Delay]);
                return;
            case Attack:
                if (lengths[Attack] == 0) {
                    value = 1;
                    s = Hold;
                    continue;
                }
                segment(1, lengths[Attack]);
                return;
            case Hold:
                if (lengths[Hold] == 0) {
                    s = Decay;
                    continue;
                }
                segment(1, lengths[Hold]);
                return;
            case Decay:
                if (lengths[Decay] == 0) {
                    value = sustain;
                    s = oneShot ? Release : Sustain;
                    continue;
                }
                segment(sustain, lengths[Decay]);
                return;
            case Sustain:
                value = sustain;
                if (sustain <= 0.0f) {
                    s = Off;
                    continue;
                }
                return;
            case Release:
                if (lengths[Release] == 0 || value <= 0.0f) {
                    s = Off;
                    continue;
                }
                segment(0, lengths[Release]);
                return;
            case Off:
                value = 0;
                return;
            }
        }
    }

    void noteOff() {
        if (stage < Release)
            enter(Release);
    }

    float tick() {
        if (stage == Sustain || stage == Off)
            return value;
        ++count;
        float shape;
        if (std::fabs(curve) < 1e-3f) {
            shape = static_cast<float>(count) / static_cast<float>(length);
        } else {
            e *= mul;
            shape = (1.0f - e) * norm;
        }
        value = from + (to - from) * shape;
        if (count >= length) {
            value = to;
            switch (stage) {
            case Delay:
                enter(Attack);
                break;
            case Attack:
                enter(Hold);
                break;
            case Hold:
                enter(Decay);
                break;
            case Decay:
                enter(oneShot ? Release : Sustain);
                break;
            default:
                enter(Off);
                break;
            }
        }
        return value;
    }
};

} // namespace

const float *sineTable() {
    static const SineTable table;
    return table.values;
}

void routing(int opCount, int algorithm, bool carrier[4], quint8 modMask[4]) {
    for (int i = 0; i < 4; ++i) {
        carrier[i] = false;
        modMask[i] = 0;
    }
    if (opCount == 2) {
        // OPL connections: 0 is FM (1 -> 2), 1 is additive (1 + 2).
        carrier[1] = true;
        if (algorithm == 1)
            carrier[0] = true;
        else
            modMask[1] = 1u << 0;
        return;
    }
    // OPN/OPM numbering, with operators 1..4 as indexes 0..3.
    switch (algorithm) {
    case 0: // 1 -> 2 -> 3 -> 4
        modMask[1] = 1u << 0;
        modMask[2] = 1u << 1;
        modMask[3] = 1u << 2;
        carrier[3] = true;
        break;
    case 1: // (1 + 2) -> 3 -> 4
        modMask[2] = (1u << 0) | (1u << 1);
        modMask[3] = 1u << 2;
        carrier[3] = true;
        break;
    case 2: // (1 + (2 -> 3)) -> 4
        modMask[2] = 1u << 1;
        modMask[3] = (1u << 0) | (1u << 2);
        carrier[3] = true;
        break;
    case 3: // ((1 -> 2) + 3) -> 4
        modMask[1] = 1u << 0;
        modMask[3] = (1u << 1) | (1u << 2);
        carrier[3] = true;
        break;
    case 4: // (1 -> 2) + (3 -> 4)
        modMask[1] = 1u << 0;
        modMask[3] = 1u << 2;
        carrier[1] = carrier[3] = true;
        break;
    case 5: // 1 -> 2, 1 -> 3, 1 -> 4
        modMask[1] = modMask[2] = modMask[3] = 1u << 0;
        carrier[1] = carrier[2] = carrier[3] = true;
        break;
    case 6: // (1 -> 2) + 3 + 4
        modMask[1] = 1u << 0;
        carrier[1] = carrier[2] = carrier[3] = true;
        break;
    default: // 1 + 2 + 3 + 4
        carrier[0] = carrier[1] = carrier[2] = carrier[3] = true;
        break;
    }
}

std::shared_ptr<Program> compileProgram(const PatchData &patch, const PlayOptions &options, int sampleRate,
                                        quint32 seed) {
    auto program = std::make_shared<Program>(patch);
    program->seed = seed ? seed : 1;
    quint32 state = program->seed;
    program->patch.randomise(state);
    program->seed = state;
    PatchData &p = program->patch;
    program->note = p.baseNote + options.pitch;
    program->gain = dbToGain(p.gain + options.gain);
    program->pan = std::clamp(p.pan + options.pan, -1.0f, 1.0f);
    program->priority = options.priority;
    program->bus = std::clamp(options.bus, 0, 2);
    const float duration = options.duration >= 0.0f ? options.duration : p.duration;
    if (duration >= 0.0f) {
        program->noteOff = static_cast<qint64>(duration * static_cast<float>(sampleRate));
    } else {
        program->noteOff = -1;
        program->oneShot = !p.loop;
    }
    const float octaves = (program->note - 69.0f) / 12.0f;
    for (int i = 0; i < p.opCount; ++i) {
        const FmOperator &op = p.ops[i];
        program->levelScale[i] = dbToGain(-op.keyScaleLevel * std::max(0.0f, octaves));
        program->rateScale[i] = std::clamp(std::exp2(-op.keyScaleRate * octaves), 0.0625f, 16.0f);
    }
    routing(p.opCount, p.algorithm, program->carrier, program->modMask);
    return program;
}

struct Voice {
    Program *program = nullptr;
    Program *next = nullptr;
    quint64 started = 0;
    qint64 sample = 0;
    quint32 phase[4] = {};
    quint32 inc[4] = {};
    float incF[4] = {};
    float incStep[4] = {};
    float out[4] = {};
    float feedback[2] = {};
    float feedbackAmount = 0;
    Env env[4];
    Env noiseEnv;
    quint32 rng = 1;
    quint32 lfsr = 1;
    float noise = 0;
    float noisePhase = 0;
    float noiseStep = 1;
    float lfoPhase = 0;
    float lfo = 0;
    float lfoHeld = 0;
    float ic1 = 0;
    float ic2 = 0;
    float a1 = 0;
    float a2 = 0;
    float a3 = 0;
    float k = 2;
    float crushPhase = 0;
    float crushHeld = 0;
    float index = 1;
    float amp = 1;
    // Live parameters: targets set by commands, and the values smoothed towards them.
    float pitchTarget = 0;
    float pitch = 0;
    float gainTarget = 1;
    float gain = 1;
    float panTarget = 0;
    float pan = 0;
    float panL = 0.7071f;
    float panR = 0.7071f;
    float cutoffTarget = -1;
    float resonanceTarget = -1;
    float lfoRateTarget = -1;
    float indexTarget = 1;
    float indexLive = 1;
    float busGain = 1;
    float fade = 1;
    float fadeStep = 0;
    int tick = 0;
    bool released = false;
};

Renderer::Renderer(int sampleRate, int maxVoices, int pendingCapacity, ReleaseFn release, void *context)
    : m_sampleRate(std::max(sampleRate, 1000)), m_maxVoices(std::max(maxVoices, 1)),
      m_voices(new Voice[static_cast<std::size_t>(std::max(maxVoices, 1))]),
      m_pending(new Command[static_cast<std::size_t>(std::max(pendingCapacity, 1))]),
      m_pendingCapacity(std::max(pendingCapacity, 1)), m_release(release), m_context(context), m_sine(sineTable()) {
    m_smooth =
        1.0f - std::exp(-static_cast<float>(kControlInterval) / (kSmoothTime * static_cast<float>(m_sampleRate)));
}

Renderer::~Renderer() {
    // The audio thread has stopped: music still held here is freed on this thread.
    delete m_music;
    for (int i = 0; i < m_pendingCount; ++i) {
        if (m_pending[static_cast<std::size_t>(i)].type == Command::MusicLoad)
            delete m_pending[static_cast<std::size_t>(i)].music;
    }
}

// Removes a pending command, keeping the others in order: writes to a chip at the same frame
// must reach it in the order they were sent.
void Renderer::removePending(int index) {
    for (int i = index; i + 1 < m_pendingCount; ++i)
        m_pending[static_cast<std::size_t>(i)] = m_pending[static_cast<std::size_t>(i) + 1];
    --m_pendingCount;
}

void Renderer::addChip(ChipCore *core, OplCore *opl) {
    if (m_chipCount == kMaxChips || !core)
        return;
    ChipStream &stream = m_chips[m_chipCount];
    stream.core = core;
    stream.step = double(core->nativeRate()) / m_sampleRate;
    m_opl[m_chipCount] = opl;
    if (opl)
        opl->write(0x01, 0x20); // voices use the OPL2 waveforms
    ++m_chipCount;
}

int Renderer::activeVoices() const {
    int n = 0;
    for (int i = 0; i < m_maxVoices; ++i)
        n += m_voices[static_cast<std::size_t>(i)].program ? 1 : 0;
    for (int c = 0; c < m_chipCount; ++c) {
        for (const ChipChannel &channel : m_channels[c])
            n += channel.program ? 1 : 0;
    }
    return n;
}

void Renderer::finishProgram(Program *program) {
    program->state.store(Finished, std::memory_order_release);
    if (m_release)
        m_release(m_context, program);
}

void Renderer::finishVoice(Voice &voice) {
    Program *next = voice.next;
    finishProgram(voice.program);
    voice.program = nullptr;
    voice.next = nullptr;
    if (next)
        startVoice(voice, next);
}

void Renderer::command(const Command &command) {
    if (command.frame > m_frame) {
        if (m_pendingCount == m_pendingCapacity) {
            if (command.type == Command::Play)
                finishProgram(command.program);
            else if (command.type == Command::MusicLoad && releaseMusic)
                releaseMusic(m_context, command.music);
            return;
        }
        m_pending[static_cast<std::size_t>(m_pendingCount++)] = command;
        return;
    }
    apply(command);
}

void Renderer::apply(const Command &command) {
    switch (command.type) {
    case Command::Play:
        if (command.program->chip >= 0)
            startChipVoice(command.program);
        else
            startProgram(command.program);
        return;
    case Command::ChipWrite:
    case Command::ChipReset:
    case Command::ChipGain:
        chipCommand(command);
        return;
    case Command::MusicLoad:
    case Command::MusicPlay:
    case Command::MusicPause:
    case Command::MusicStop:
    case Command::MusicLoops:
    case Command::MusicGain:
        musicCommand(command);
        return;
    case Command::Stop:
    case Command::StopAll: {
        const bool all = command.type == Command::StopAll;
        for (int i = 0; i < m_pendingCount;) {
            const Command &pending = m_pending[static_cast<std::size_t>(i)];
            if (pending.type == Command::Play && (all || pending.program->id == command.voiceId)) {
                finishProgram(pending.program);
                removePending(i);
            } else {
                ++i;
            }
        }
        for (int c = 0; c < m_chipCount; ++c) {
            OplCore *core = m_opl[c];
            if (!core)
                continue;
            for (int ch = 0; ch < core->channelCount(); ++ch) {
                ChipChannel &channel = m_channels[c][ch];
                if (!channel.program || (!all && channel.program->id != command.voiceId))
                    continue;
                const int bank = (ch / 9) << 8;
                const int local = ch % 9;
                if (command.immediate) {
                    // The fastest release on both operators, then key off.
                    const int off = (local / 3) * 8 + local % 3;
                    for (int op = 0; op < 2; ++op) {
                        const quint8 sr = channel.program->opl[6 + op];
                        core->write(static_cast<quint16>(bank | (0x80 + off + 3 * op)), static_cast<quint8>(sr | 0x0f));
                    }
                }
                writeChannelFrequency(core, ch, channel.program->hz, false);
                channel.released = true;
            }
        }
        for (int i = 0; i < m_maxVoices; ++i) {
            Voice &v = m_voices[static_cast<std::size_t>(i)];
            if (!v.program)
                continue;
            if (v.next && (all || v.next->id == command.voiceId)) {
                finishProgram(v.next);
                v.next = nullptr;
            }
            if (!all && v.program->id != command.voiceId)
                continue;
            if (command.immediate) {
                if (v.fadeStep == 0.0f)
                    v.fadeStep = 1.0f / (kStealFade * static_cast<float>(m_sampleRate));
            } else if (!v.released) {
                v.released = true;
                for (int op = 0; op < v.program->patch.opCount; ++op)
                    v.env[op].noteOff();
                v.noiseEnv.noteOff();
            }
        }
        return;
    }
    case Command::SetParam:
        if (static_cast<ParamId>(command.param) == ParamId::Pitch) {
            for (int c = 0; c < m_chipCount; ++c) {
                for (int ch = 0; m_opl[c] && ch < m_opl[c]->channelCount(); ++ch) {
                    ChipChannel &channel = m_channels[c][ch];
                    if (channel.program && channel.program->id == command.voiceId)
                        writeChannelFrequency(m_opl[c], ch, channel.program->hz * std::exp2(command.value / 12.0f),
                                              !channel.released);
                }
            }
        }
        for (int i = 0; i < m_maxVoices; ++i) {
            Voice &v = m_voices[static_cast<std::size_t>(i)];
            if (!v.program || v.program->id != command.voiceId)
                continue;
            switch (static_cast<ParamId>(command.param)) {
            case ParamId::Pitch:
                v.pitchTarget = command.value;
                break;
            case ParamId::Gain:
                v.gainTarget = dbToGain(command.value);
                break;
            case ParamId::Pan:
                v.panTarget = std::clamp(command.value, -1.0f, 1.0f);
                break;
            case ParamId::FilterCutoff:
                v.cutoffTarget = command.value;
                break;
            case ParamId::FilterResonance:
                v.resonanceTarget = std::clamp(command.value, 0.0f, 1.0f);
                break;
            case ParamId::LfoRate:
                v.lfoRateTarget = std::clamp(command.value, 0.0f, 50.0f);
                break;
            case ParamId::ModulationIndex:
                v.indexTarget = std::max(command.value, 0.0f);
                break;
            }
        }
        return;
    }
}

void Renderer::startProgram(Program *program) {
    for (int i = 0; i < m_maxVoices; ++i) {
        if (!m_voices[static_cast<std::size_t>(i)].program) {
            startVoice(m_voices[static_cast<std::size_t>(i)], program);
            return;
        }
    }
    // Steal: the lowest priority, then the quietest, then the oldest, fading out over 2 ms.
    Voice *victim = nullptr;
    float victimLevel = 0;
    for (int i = 0; i < m_maxVoices; ++i) {
        Voice &v = m_voices[static_cast<std::size_t>(i)];
        if (v.next)
            continue;
        float level = 0;
        for (int op = 0; op < v.program->patch.opCount; ++op) {
            if (v.program->carrier[op])
                level = std::max(level, v.env[op].value * v.program->patch.ops[op].level);
        }
        level *= v.gain * v.fade;
        if (!victim || v.program->priority < victim->program->priority ||
            (v.program->priority == victim->program->priority &&
             (level < victimLevel || (level == victimLevel && v.started < victim->started)))) {
            victim = &v;
            victimLevel = level;
        }
    }
    if (!victim || victim->program->priority > program->priority) {
        finishProgram(program);
        return;
    }
    victim->next = program;
    if (victim->fadeStep == 0.0f)
        victim->fadeStep = 1.0f / (kStealFade * static_cast<float>(m_sampleRate));
}

void Renderer::startVoice(Voice &v, Program *program) {
    const PatchData &p = program->patch;
    const float rate = static_cast<float>(m_sampleRate);
    v = Voice();
    v.program = program;
    v.started = m_frame;
    for (int op = 0; op < p.opCount; ++op)
        v.env[op].setup(p.ops[op].env, rate, program->rateScale[op], program->oneShot);
    v.noiseEnv.setup(p.noiseEnv, rate, 1.0f, program->oneShot);
    v.feedbackAmount = p.feedback <= 0.0f ? 0.0f : std::exp2(p.feedback - 6.0f);
    v.rng = program->seed ? program->seed : 1;
    v.lfsr = 1;
    v.noiseStep = p.noiseRate > 0.0f ? std::min(p.noiseRate / rate, 1.0f) : 1.0f;
    v.noisePhase = 1.0f;
    v.gain = v.gainTarget = 1.0f;
    v.pan = v.panTarget = program->pan;
    v.cutoffTarget = -1;
    v.resonanceTarget = -1;
    v.lfoRateTarget = -1;
    v.lfoHeld = randomBipolar(v.rng);
    program->state.store(Playing, std::memory_order_release);
    controlTick(v);
    for (int op = 0; op < 4; ++op) {
        v.incF[op] += v.incStep[op] * kControlInterval;
        v.incStep[op] = 0;
    }
}

// Updates everything that runs at the control rate, once every kControlInterval samples: the smoothed
// live parameters, the LFO, pitch, the modulation index and the filter. Pitch is ramped linearly
// in Hz to the value at the next tick, so sweeps have no steps.
void Renderer::controlTick(Voice &v) {
    const Program &prog = *v.program;
    const PatchData &p = prog.patch;
    const float rate = static_cast<float>(m_sampleRate);
    const float dt = static_cast<float>(kControlInterval) / rate;
    const float t = static_cast<float>(v.sample) / rate;

    v.pitch += (v.pitchTarget - v.pitch) * m_smooth;
    v.gain += (v.gainTarget - v.gain) * m_smooth;
    v.pan += (v.panTarget - v.pan) * m_smooth;
    v.indexLive += (v.indexTarget - v.indexLive) * m_smooth;
    v.busGain += (busGain[prog.bus] - v.busGain) * m_smooth;
    const float angle = (v.pan + 1.0f) * (kPi / 4.0f);
    v.panL = std::cos(angle);
    v.panR = std::sin(angle);

    const float lfoRate = v.lfoRateTarget >= 0.0f ? v.lfoRateTarget : p.lfoRate;
    v.lfoPhase += lfoRate * dt;
    if (v.lfoPhase >= 1.0f) {
        v.lfoPhase -= std::floor(v.lfoPhase);
        v.lfoHeld = randomBipolar(v.rng);
    }
    switch (p.lfoWave) {
    case LfoWave::Sine:
        v.lfo = std::sin(2.0f * kPi * v.lfoPhase);
        break;
    case LfoWave::Triangle:
        v.lfo = v.lfoPhase < 0.5f ? 4.0f * v.lfoPhase - 1.0f : 3.0f - 4.0f * v.lfoPhase;
        break;
    case LfoWave::Square:
        v.lfo = v.lfoPhase < 0.5f ? 1.0f : -1.0f;
        break;
    case LfoWave::Saw:
        v.lfo = 2.0f * v.lfoPhase - 1.0f;
        break;
    case LfoWave::SampleHold:
        v.lfo = v.lfoHeld;
        break;
    }

    // Pitch at the next tick.
    const float tn = t + dt;
    float note = prog.note + sweepAt(p.pitchEnv, tn) + p.slide * tn + 0.5f * p.deltaSlide * tn * tn + v.pitch +
                 p.lfoPitch * v.lfo;
    if (p.arpCount > 0 && p.arpRate > 0.0f) {
        const auto step = static_cast<qint64>(tn * p.arpRate);
        const qint64 n = p.arpCount;
        note += p.arpSteps[p.arpRepeat ? step % n : std::min(step, n - 1)];
    }
    const float hz = noteToHz(std::clamp(note, -24.0f, 160.0f));
    const float nyquist = 0.5f * rate;
    for (int op = 0; op < p.opCount; ++op) {
        const FmOperator &o = p.ops[op];
        float f = o.fixed > 0.0f ? o.fixed : hz * o.ratio + o.offset;
        f = std::clamp(f, 0.0f, nyquist);
        const float target = f / rate * kTwoPow32;
        v.incStep[op] = (target - v.incF[op]) / static_cast<float>(kControlInterval);
    }

    v.index = std::max(0.0f, sweepAt(p.index, t)) * std::max(0.0f, 1.0f + p.lfoIndex * v.lfo) * v.indexLive;
    v.amp = 1.0f - p.lfoAmp * (1.0f - v.lfo) * 0.5f;

    if (p.filterType != FilterType::None) {
        float cutoff = v.cutoffTarget > 0.0f ? v.cutoffTarget : p.cutoff;
        if (v.cutoffTarget <= 0.0f && p.sweepEnd > 0.0f && p.sweepTime > 0.0f) {
            const float x = curveAt(t / p.sweepTime, p.sweepCurve);
            cutoff = std::exp(std::log(p.cutoff) + (std::log(p.sweepEnd) - std::log(p.cutoff)) * x);
        }
        cutoff *= std::exp2(p.lfoCutoff * v.lfo);
        cutoff = std::clamp(cutoff, 20.0f, 0.49f * rate);
        const float resonance = v.resonanceTarget >= 0.0f ? v.resonanceTarget : p.resonance;
        const float g = std::tan(kPi * cutoff / rate);
        v.k = std::max(2.0f - 2.0f * resonance, 0.02f);
        v.a1 = 1.0f / (1.0f + g * (g + v.k));
        v.a2 = g * v.a1;
        v.a3 = g * v.a2;
    }
}

int Renderer::renderVoice(Voice &v, float *out, int frames) {
    const Program &prog = *v.program;
    const PatchData &p = prog.patch;
    const int ops = p.opCount;
    const float rate = static_cast<float>(m_sampleRate);
    const bool noiseOn = p.noiseLevel > 0.0f;
    const bool noiseMixed = noiseOn && p.noiseModulate == 0;
    const float crushQ = p.crushBits < 16.0f ? std::exp2(std::floor(p.crushBits) - 1.0f) : 0.0f;
    const float crushStep = p.crushRate > 0.0f ? std::min(p.crushRate / rate, 1.0f) : 0.0f;
    float level[4] = {0, 0, 0, 0};
    for (int op = 0; op < ops; ++op)
        level[op] = p.ops[op].level * prog.levelScale[op];
    for (int i = 0; i < frames; ++i) {
        if (v.tick == 0)
            controlTick(v);
        v.tick = (v.tick + 1) % kControlInterval;
        if (prog.noteOff >= 0 && v.sample == prog.noteOff && !v.released) {
            v.released = true;
            for (int op = 0; op < ops; ++op)
                v.env[op].noteOff();
            v.noiseEnv.noteOff();
        }

        float noise = 0.0f;
        if (noiseOn) {
            v.noisePhase += v.noiseStep;
            if (v.noisePhase >= 1.0f) {
                v.noisePhase -= 1.0f;
                switch (p.noiseMode) {
                case NoiseMode::White:
                    v.noise = randomBipolar(v.rng);
                    break;
                case NoiseMode::Lfsr7: {
                    const quint32 bit = (v.lfsr ^ (v.lfsr >> 6)) & 1u;
                    v.lfsr = (v.lfsr >> 1) | (bit << 14);
                    v.noise = (v.lfsr & 1u) ? 1.0f : -1.0f;
                    break;
                }
                case NoiseMode::Lfsr15: {
                    const quint32 bit = (v.lfsr ^ (v.lfsr >> 1)) & 1u;
                    v.lfsr = (v.lfsr >> 1) | (bit << 14);
                    v.noise = (v.lfsr & 1u) ? 1.0f : -1.0f;
                    break;
                }
                }
            }
            noise = v.noise * v.noiseEnv.tick() * p.noiseLevel;
        }

        float sum = 0.0f;
        bool sounding = false;
        for (int op = 0; op < ops; ++op) {
            // A silent operator neither sounds nor modulates: skip it, and let it not keep the voice alive.
            if (level[op] == 0.0f)
                continue;
            float mod = 0.0f;
            const quint8 mask = prog.modMask[op];
            for (int j = 0; j < op; ++j) {
                if (mask & (1u << j))
                    mod += v.out[j];
            }
            mod *= v.index;
            if (op == 0 && v.feedbackAmount > 0.0f)
                mod += (v.feedback[0] + v.feedback[1]) * 0.5f * v.feedbackAmount;
            if (p.noiseModulate == op + 1)
                mod += noise;
            if (!(std::fabs(mod) < kMaxModulation))
                mod = 0.0f;
            const auto offset = static_cast<quint32>(static_cast<qint64>(mod * kTwoPow32) & 0xffffffff);
            const float env = v.env[op].tick();
            const float o = waveAt(p.ops[op].wave, m_sine, v.phase[op] + offset) * env * level[op];
            v.out[op] = o;
            if (op == 0) {
                v.feedback[1] = v.feedback[0];
                v.feedback[0] = o;
            }
            v.incF[op] += v.incStep[op];
            v.phase[op] += static_cast<quint32>(std::max(v.incF[op], 0.0f));
            if (prog.carrier[op]) {
                sum += o;
                sounding = sounding || v.env[op].stage != Env::Off;
            }
        }
        if (noiseMixed) {
            sum += noise;
            sounding = sounding || v.noiseEnv.stage != Env::Off;
        }

        sum *= v.amp;
        if (p.filterType != FilterType::None) {
            const float v3 = sum - v.ic2;
            const float v1 = v.a1 * v.ic1 + v.a2 * v3;
            const float v2 = v.ic2 + v.a2 * v.ic1 + v.a3 * v3;
            v.ic1 = 2.0f * v1 - v.ic1;
            v.ic2 = 2.0f * v2 - v.ic2;
            switch (p.filterType) {
            case FilterType::LowPass:
                sum = v2;
                break;
            case FilterType::HighPass:
                sum = sum - v.k * v1 - v2;
                break;
            case FilterType::BandPass:
                sum = v1;
                break;
            case FilterType::None:
                break;
            }
        }
        if (crushStep > 0.0f) {
            v.crushPhase += crushStep;
            if (v.crushPhase >= 1.0f) {
                v.crushPhase -= 1.0f;
                v.crushHeld = sum;
            }
            sum = v.crushHeld;
        }
        if (crushQ > 0.0f)
            sum = std::round(sum * crushQ) / crushQ;

        if (v.fadeStep > 0.0f) {
            v.fade -= v.fadeStep;
            if (v.fade <= 0.0f)
                v.fade = 0.0f;
        }
        const float g = sum * prog.gain * v.gain * v.busGain * v.fade;
        out[2 * i] += g * v.panL;
        out[2 * i + 1] += g * v.panR;
        ++v.sample;

        if (!sounding || (v.fadeStep > 0.0f && v.fade <= 0.0f)) {
            finishVoice(v);
            return i + 1;
        }
    }
    return frames;
}

void Renderer::render(float *out, int frames) {
    std::memset(out, 0, sizeof(float) * 2 * static_cast<std::size_t>(std::max(frames, 0)));
    int pos = 0;
    while (pos < frames) {
        quint64 next = m_frame + static_cast<quint64>(frames - pos);
        for (int i = 0; i < m_pendingCount;) {
            if (m_pending[static_cast<std::size_t>(i)].frame <= m_frame) {
                const Command c = m_pending[static_cast<std::size_t>(i)];
                removePending(i);
                apply(c);
                // A stop may have removed other pending commands: scan again from the start.
                i = 0;
                next = m_frame + static_cast<quint64>(frames - pos);
            } else {
                next = std::min(next, m_pending[static_cast<std::size_t>(i)].frame);
                ++i;
            }
        }
        const int n = static_cast<int>(next - m_frame);
        for (int i = 0; i < m_maxVoices; ++i) {
            Voice &v = m_voices[static_cast<std::size_t>(i)];
            // A voice that ends part way through a chunk hands its slot to the voice that stole it,
            // which renders the rest of the chunk.
            int done = 0;
            while (v.program && done < n)
                done += renderVoice(v, out + 2 * (pos + done), n - done);
        }
        for (int c = 0; c < m_chipCount; ++c)
            mixStream(m_chips[c], out + 2 * pos, n, false);
        if (m_music && m_musicPlaying)
            mixStream(m_musicStream, out + 2 * pos, n, true);
        updateChipVoices(n);
        m_frame += static_cast<quint64>(n);
        pos += n;
    }
    // The master gain ramps across the block, then the soft clipper limits the mix.
    const float step = (masterGain - m_master) / static_cast<float>(std::max(frames, 1));
    for (int i = 0; i < frames; ++i) {
        m_master += step;
        out[2 * i] = softClip(out[2 * i] * m_master);
        out[2 * i + 1] = softClip(out[2 * i + 1] * m_master);
    }
    m_master = masterGain;
}

void Renderer::writeChannelFrequency(OplCore *core, int channel, float hz, bool key) {
    int fnum = 0;
    int block = 0;
    if (!oplFrequency(hz, &fnum, &block)) {
        fnum = 1023;
        block = 7;
    }
    const int bank = (channel / 9) << 8;
    const int local = channel % 9;
    core->write(static_cast<quint16>(bank | (0xa0 + local)), static_cast<quint8>(fnum & 0xff));
    core->write(static_cast<quint16>(bank | (0xb0 + local)),
                static_cast<quint8>((key ? 0x20 : 0) | (block << 2) | (fnum >> 8)));
}

void Renderer::startChipVoice(Program *program) {
    const int c = program->chip;
    OplCore *core = c >= 0 && c < m_chipCount ? m_opl[c] : nullptr;
    if (!core) {
        finishProgram(program);
        return;
    }
    // A free channel, else the oldest of no higher priority.
    int chosen = -1;
    for (int ch = 0; ch < core->channelCount() && chosen < 0; ++ch) {
        if (!m_channels[c][ch].program)
            chosen = ch;
    }
    if (chosen < 0) {
        qint64 oldest = -1;
        for (int ch = 0; ch < core->channelCount(); ++ch) {
            const ChipChannel &channel = m_channels[c][ch];
            if (channel.program->priority <= program->priority && channel.elapsed > oldest) {
                oldest = channel.elapsed;
                chosen = ch;
            }
        }
        if (chosen < 0) {
            finishProgram(program);
            return;
        }
        finishProgram(m_channels[c][chosen].program);
    }
    ChipChannel &channel = m_channels[c][chosen];
    channel.program = program;
    channel.elapsed = 0;
    channel.released = false;
    const int bank = (chosen / 9) << 8;
    const int local = chosen % 9;
    const int off = (local / 3) * 8 + local % 3;
    writeChannelFrequency(core, chosen, program->hz, false);
    static const int bases[5] = {0x20, 0x40, 0x60, 0x80, 0xe0};
    for (int r = 0; r < 5; ++r) {
        for (int op = 0; op < 2; ++op)
            core->write(static_cast<quint16>(bank | (bases[r] + off + 3 * op)), program->opl[2 * r + op]);
    }
    core->write(static_cast<quint16>(bank | (0xc0 + local)), static_cast<quint8>(program->opl[10] | 0x30));
    writeChannelFrequency(core, chosen, program->hz, true);
    program->state.store(Playing, std::memory_order_release);
}

// Releases chip voices at their note-off, and frees the channels whose carriers have gone silent.
void Renderer::updateChipVoices(int frames) {
    for (int c = 0; c < m_chipCount; ++c) {
        OplCore *core = m_opl[c];
        for (int ch = 0; core && ch < core->channelCount(); ++ch) {
            ChipChannel &channel = m_channels[c][ch];
            if (!channel.program)
                continue;
            channel.elapsed += frames;
            const Program &p = *channel.program;
            if (!channel.released && p.noteOff >= 0 && channel.elapsed >= p.noteOff) {
                writeChannelFrequency(core, ch, p.hz, false);
                channel.released = true;
            }
            // A one-shot note keeps its key down: its percussive envelope decays on its own.
            if (channel.elapsed > m_sampleRate / 50 && core->channelSilent(ch)) {
                if (!channel.released)
                    writeChannelFrequency(core, ch, p.hz, false);
                finishProgram(channel.program);
                channel.program = nullptr;
            }
        }
    }
}

void Renderer::chipCommand(const Command &command) {
    const int c = command.param;
    if (c >= m_chipCount)
        return;
    switch (command.type) {
    case Command::ChipWrite:
        m_chips[c].core->write(static_cast<quint16>(command.voiceId & 0x1ff),
                               static_cast<quint8>(command.voiceId >> 16));
        break;
    case Command::ChipReset:
        m_chips[c].core->reset();
        if (m_opl[c])
            m_opl[c]->write(0x01, 0x20);
        for (ChipChannel &channel : m_channels[c]) {
            if (channel.program)
                finishProgram(channel.program);
            channel = ChipChannel();
        }
        break;
    case Command::ChipGain:
        m_chips[c].gain = command.value;
        break;
    default:
        break;
    }
}

void Renderer::musicCommand(const Command &command) {
    switch (command.type) {
    case Command::MusicLoad:
        if (m_music && releaseMusic)
            releaseMusic(m_context, m_music);
        m_music = command.music;
        m_musicStream = ChipStream();
        m_musicStream.core = m_music->core.get();
        m_musicStream.step = double(m_music->core->nativeRate()) / m_sampleRate;
        m_musicStream.gain = command.value;
        m_musicIndex = 0;
        m_musicTime = 0;
        m_musicPlaying = false;
        m_musicFinished = false;
        break;
    case Command::MusicPlay:
        if (m_music)
            m_musicPlaying = true;
        break;
    case Command::MusicPause:
        m_musicPlaying = false;
        break;
    case Command::MusicStop:
        if (m_music) {
            m_music->core->reset();
            m_musicIndex = 0;
            m_musicTime = 0;
            m_musicStream.prev[0] = m_musicStream.prev[1] = m_musicStream.next[0] = m_musicStream.next[1] = 0;
        }
        m_musicPlaying = false;
        break;
    case Command::MusicLoops:
        m_musicLoops = command.immediate;
        break;
    case Command::MusicGain:
        m_musicStream.gain = command.value;
        break;
    default:
        break;
    }
}

// Adds frames of a chip to out. The chip runs at its own rate: before each of its samples, the
// music's writes that are due are applied, so music keeps the timing of the chip's clock.
void Renderer::mixStream(ChipStream &stream, float *out, int frames, bool music) {
    constexpr float scale = 1.0f / 32768.0f;
    const double tick = music ? 1.0 / stream.core->nativeRate() : 0.0;
    for (int i = 0; i < frames; ++i) {
        while (stream.frac >= 1.0) {
            stream.frac -= 1.0;
            if (music) {
                const std::vector<MusicEvent> &events = m_music->events;
                while (m_musicIndex < events.size() && events[m_musicIndex].time <= m_musicTime) {
                    stream.core->write(events[m_musicIndex].reg, events[m_musicIndex].value);
                    ++m_musicIndex;
                }
                if (m_musicIndex >= events.size() && m_musicTime >= m_music->duration) {
                    if (m_musicLoops && m_music->loopTime >= 0) {
                        m_musicIndex = m_music->loopIndex;
                        m_musicTime = m_music->loopTime;
                    } else if (m_musicLoops) {
                        m_musicIndex = 0;
                        m_musicTime = 0;
                    } else {
                        m_musicPlaying = false;
                        m_musicFinished = true;
                        return;
                    }
                }
                m_musicTime += tick;
            }
            qint32 s[2];
            stream.core->generate(s, 1);
            stream.prev[0] = stream.next[0];
            stream.prev[1] = stream.next[1];
            stream.next[0] = static_cast<float>(s[0]) * scale;
            stream.next[1] = static_cast<float>(s[1]) * scale;
        }
        const auto f = static_cast<float>(stream.frac);
        out[2 * i] += (stream.prev[0] + (stream.next[0] - stream.prev[0]) * f) * stream.gain;
        out[2 * i + 1] += (stream.prev[1] + (stream.next[1] - stream.prev[1]) * f) * stream.gain;
        stream.frac += stream.step;
    }
}

QByteArray wavFile(const qint16 *samples, qsizetype frames, int sampleRate) {
    const auto dataBytes = static_cast<quint32>(frames * 4);
    const auto rate = static_cast<quint32>(sampleRate);
    QByteArray wav;
    wav.reserve(44 + static_cast<qsizetype>(dataBytes));
    auto u32 = [&wav](quint32 v) {
        char b[4];
        qToLittleEndian<quint32>(v, b);
        wav.append(b, 4);
    };
    auto u16 = [&wav](quint16 v) {
        char b[2];
        qToLittleEndian<quint16>(v, b);
        wav.append(b, 2);
    };
    wav.append("RIFF");
    u32(36 + dataBytes);
    wav.append("WAVEfmt ");
    u32(16);
    u16(1); // PCM
    u16(2); // stereo
    u32(rate);
    u32(rate * 4);
    u16(4);
    u16(16);
    wav.append("data");
    u32(dataBytes);
    for (qsizetype i = 0; i < frames * 2; ++i)
        u16(static_cast<quint16>(samples[i]));
    return wav;
}

void toQ15(const float *in, qint16 *out, qsizetype count) {
    for (qsizetype i = 0; i < count; ++i) {
        const float x = std::clamp(in[i], -1.0f, 1.0f) * 32767.0f;
        out[i] = static_cast<qint16>(std::lrint(x));
    }
}

DenormalGuard::DenormalGuard() {
#if defined(NT_FM_X86_FTZ)
    m_saved = _mm_getcsr();
    _mm_setcsr(static_cast<unsigned>(m_saved) | 0x8040u); // FTZ | DAZ
#elif defined(__aarch64__)
    quint64 fpcr;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
    m_saved = fpcr;
    fpcr |= (1ull << 24); // FZ
    __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#endif
}

DenormalGuard::~DenormalGuard() {
#if defined(NT_FM_X86_FTZ)
    _mm_setcsr(static_cast<unsigned>(m_saved));
#elif defined(__aarch64__)
    __asm__ __volatile__("msr fpcr, %0" : : "r"(m_saved));
#endif
}

} // namespace QtFM::Private
