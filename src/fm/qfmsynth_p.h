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
    std::atomic<int> state{Queued};
};

// Compiles patch for one play. noteOff is in samples: -1 holds until stopped.
std::shared_ptr<Program> compileProgram(const PatchData &patch, const PlayOptions &options, int sampleRate,
                                        quint32 seed);

// Fills carrier and modMask with the routing of algorithm for opCount operators.
void routing(int opCount, int algorithm, bool carrier[4], quint8 modMask[4]);

struct Command {
    enum Type : quint8 { Play, Stop, StopAll, SetParam };
    Type type = Play;
    bool immediate = false;
    quint8 param = 0;
    float value = 0;
    quint32 voiceId = 0;
    Program *program = nullptr;
    quint64 frame = 0;
};
static_assert(sizeof(Command) <= 64, "commands are fixed 64-byte messages");

struct Voice;

class Renderer {
public:
    using ReleaseFn = void (*)(void *context, Program *program);

    Renderer(int sampleRate, int maxVoices, int pendingCapacity, ReleaseFn release, void *context);
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    // Applies a command now, or keeps it until its frame comes.
    void command(const Command &command);
    // Writes frames of interleaved stereo.
    void render(float *out, int frames);

    quint64 frame() const { return m_frame; }
    int activeVoices() const;
    int sampleRate() const { return m_sampleRate; }

    float busGain[3] = {1, 1, 1};
    float masterGain = 1;

private:
    void apply(const Command &command);
    void startProgram(Program *program);
    void startVoice(Voice &voice, Program *program);
    void finishVoice(Voice &voice);
    void finishProgram(Program *program);
    int renderVoice(Voice &voice, float *out, int frames);
    void controlTick(Voice &voice);

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
