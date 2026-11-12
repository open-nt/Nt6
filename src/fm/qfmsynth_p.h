// MIT License - see LICENSE
#pragma once

// The native FM synthesiser. A Program is one play of a patch: compiled on a control thread with
// its random draws and play options applied, then read (never written, apart from its state)
// by the audio thread. The Renderer owns the voices and runs on the audio thread; everything it
// needs is allocated in its constructor.

#include "qfmpatch_p.h"

#include <QtFM/qfmengine.h>

#include <atomic>
#include <memory>

namespace QtFM::Private {

enum ProgramState { Queued = 0, Playing = 1, Finished = 2 };

struct Program {
    explicit Program(const PatchData &p) : patch(p) {}
    PatchData patch;
    quint32 id = 0;
    int priority = 0;
    int bus = 0;
    quint64 frame = 0;
    quint32 seed = 1;
    float note = 69;
    float gain = 1;
    float pan = 0;
    qint64 noteOff = -1;
    bool oneShot = false;
    float levelScale[4] = {1, 1, 1, 1};
    float rateScale[4] = {1, 1, 1, 1};
    bool carrier[4] = {false, false, false, true};
    quint8 modMask[4] = {0, 0, 0, 0};
    // A voice on a chip channel: the index of the chip in the engine, the instrument's registers
    // in the SBI order, and the frequency.
    int chip = -1;
    quint8 opl[11] = {};
    float hz = 440;
    std::atomic<int> state{Queued};
};

// Compiles patch for one play. noteOff is in samples: -1 holds until stopped.
std::shared_ptr<Program> compileProgram(const PatchData &patch, const PlayOptions &options, int sampleRate,
                                        quint32 seed);

// Fills carrier and modMask with the routing of algorithm for opCount operators.
void routing(int opCount, int algorithm, bool carrier[4], quint8 modMask[4]);

struct MusicData;

struct Command {
    enum Type : quint8 {
        Play,
        Stop,
        StopAll,
        SetParam,
        ChipWrite,
        ChipReset,
        ChipGain,
        MusicLoad,
        MusicPlay,
        MusicPause,
        MusicStop,
        MusicLoops,
        MusicGain
    };
    Type type = Play;
    bool immediate = false;
    quint8 param = 0;
    float value = 0;
    quint32 voiceId = 0;
    Program *program = nullptr;
    MusicData *music = nullptr;
    quint64 frame = 0;
};
static_assert(sizeof(Command) <= 64, "commands are fixed 64-byte messages");

struct Voice;
class OplCore;

// A chip core resampled from its own rate to the engine's, with linear interpolation.
struct ChipStream {
    ChipCore *core = nullptr;
    double step = 1;
    double frac = 1;
    float prev[2] = {0, 0};
    float next[2] = {0, 0};
    float gain = 1;
};

// A voice on a chip channel.
struct ChipChannel {
    Program *program = nullptr;
    qint64 elapsed = 0;
    bool released = false;
};

class Renderer {
public:
    using ReleaseFn = void (*)(void *context, Program *program);
    using ReleaseMusicFn = void (*)(void *context, MusicData *music);

    Renderer(int sampleRate, int maxVoices, int pendingCapacity, ReleaseFn release, void *context);
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    // Applies a command now, or keeps it until its frame comes.
    void command(const Command &command);
    // Writes frames of interleaved stereo.
    void render(float *out, int frames);

    // Adds a chip, before rendering starts; opl is the core again if voices may play on it.
    void addChip(ChipCore *core, OplCore *opl);
    ReleaseMusicFn releaseMusic = nullptr;

    quint64 frame() const { return m_frame; }
    bool musicPlaying() const { return m_musicPlaying; }
    double musicPosition() const { return m_musicTime; }
    bool takeMusicFinished() {
        const bool f = m_musicFinished;
        m_musicFinished = false;
        return f;
    }
    int activeVoices() const;
    int sampleRate() const { return m_sampleRate; }

    float busGain[3] = {1, 1, 1};
    float masterGain = 1;

private:
    void apply(const Command &command);
    void removePending(int index);
    void startProgram(Program *program);
    void startVoice(Voice &voice, Program *program);
    void finishVoice(Voice &voice);
    void finishProgram(Program *program);
    int renderVoice(Voice &voice, float *out, int frames);
    void controlTick(Voice &voice);
    void startChipVoice(Program *program);
    void chipCommand(const Command &command);
    void musicCommand(const Command &command);
    void updateChipVoices(int frames);
    void mixStream(ChipStream &stream, float *out, int frames, bool music);
    void writeChannelFrequency(OplCore *core, int channel, float hz, bool key);

    int m_sampleRate;
    int m_maxVoices;
    std::unique_ptr<Voice[]> m_voices;
    std::unique_ptr<Command[]> m_pending;
    int m_pendingCount = 0;
    int m_pendingCapacity;
    ReleaseFn m_release;
    void *m_context;
    quint64 m_frame = 0;
    float m_master = 1;
    float m_smooth = 0;
    const float *m_sine;

    static constexpr int kMaxChips = 4;
    ChipStream m_chips[kMaxChips];
    OplCore *m_opl[kMaxChips] = {};
    ChipChannel m_channels[kMaxChips][18];
    int m_chipCount = 0;

    MusicData *m_music = nullptr;
    ChipStream m_musicStream;
    std::size_t m_musicIndex = 0;
    double m_musicTime = 0;
    bool m_musicPlaying = false;
    bool m_musicLoops = true;
    bool m_musicFinished = false;
};

// Returns the sine table: 4097 entries, the last equal to the first.
const float *sineTable();

// Encodes stereo 16-bit PCM as a WAV file.
QByteArray wavFile(const qint16 *samples, qsizetype frames, int sampleRate);

// Converts floats to Q15 with saturation.
void toQ15(const float *in, qint16 *out, qsizetype count);

// Sets flush-to-zero and denormals-are-zero for the life of the object.
class DenormalGuard {
public:
    DenormalGuard();
    ~DenormalGuard();
    DenormalGuard(const DenormalGuard &) = delete;
    DenormalGuard &operator=(const DenormalGuard &) = delete;

private:
    quint64 m_saved = 0;
};

} // namespace QtFM::Private
