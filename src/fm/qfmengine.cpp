// MIT License - see LICENSE
//
// The engine: control calls from any thread compile programs and send fixed-size commands through
// a lock-free queue; the audio thread applies them in render() and hands finished programs back
// through a second queue, which a control thread drains and frees. Only control threads take the
// mutex; render() never does.

#include "qfmmusic_p.h"
#include "qfmopl_p.h"
#include "qfmoutput_p.h"
#include "qfmqueue_p.h"
#include "qfmsynth_p.h"

#include <QtCore/qhash.h>
#include <QtCore/qmutex.h>
#include <QtCore/qrandom.h>
#include <QtCore/qtimer.h>
#include <QtFM/qfmengine.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace QtFM {

namespace Private {

struct EngineCore {
    explicit EngineCore(const EngineConfig &c)
        : commands(static_cast<std::size_t>(c.queueCapacity)),
          returns(static_cast<std::size_t>(c.queueCapacity + 2 * c.maxVoices + 16)),
          scratch(static_cast<std::size_t>(c.blockSize) * 2),
          renderer(c.sampleRate, c.maxVoices, c.queueCapacity, &EngineCore::released, this),
          musicReturns(static_cast<std::size_t>(c.queueCapacity + 4)), config(c) {
        renderer.releaseMusic = &EngineCore::releasedMusic;
        for (ChipType type : c.chips) {
            if (chipCores.size() == 4)
                break;
            std::unique_ptr<ChipCore> core = ChipCore::create(type);
            if (!core)
                continue;
            OplCore *opl =
                type == ChipType::OPL2 || type == ChipType::OPL3 ? static_cast<OplCore *>(core.get()) : nullptr;
            renderer.addChip(core.get(), opl);
            chipTypes.push_back(type);
            chipCores.push_back(std::move(core));
        }
    }

    ~EngineCore() {
        // Nothing renders any more: free the music that never reached the audio thread, or came back.
        Command c;
        while (commands.pop(c)) {
            if (c.type == Command::MusicLoad)
                delete c.music;
        }
        MusicData *music = nullptr;
        while (musicReturns.pop(music))
            delete music;
    }
    EngineCore(const EngineCore &) = delete;
    EngineCore &operator=(const EngineCore &) = delete;

    static void releasedMusic(void *context, MusicData *music) {
        static_cast<EngineCore *>(context)->musicReturns.push(music);
    }

    static void released(void *context, Program *program) {
        // A full queue leaves the program to the engine's destructor; it is already Finished.
        static_cast<EngineCore *>(context)->returns.push(program);
    }

    // Frees the programs the audio thread has finished with. Called with mutex held.
    void drain() {
        MusicData *music = nullptr;
        while (musicReturns.pop(music))
            delete music;
        Program *program = nullptr;
        while (returns.pop(program)) {
            finished.append(program->id);
            inFlight.remove(program->id);
        }
    }

    bool send(const Command &command) { return commands.push(command); }

    // Ordered by size, to keep padding down. Control-side members (mutex, rng, nextId, the dB values,
    // inFlight and finished) are only touched with mutex held.
    MpscQueue<Command> commands;
    SpscQueue<Program *> returns;
    std::atomic<quint64> frame{0};
    std::vector<float> scratch;
    QList<quint32> finished;
    QMutex mutex;
    QHash<quint32, std::shared_ptr<Program>> inFlight;
    Renderer renderer;
    SpscQueue<MusicData *> musicReturns;
    std::vector<std::unique_ptr<ChipCore>> chipCores;
    std::vector<ChipType> chipTypes;
    std::atomic<double> musicPosition{0};
    std::atomic<bool> musicPlaying{false};
    std::atomic<bool> musicFinished{false};
    std::atomic<int> active{0};
    std::atomic<float> masterGain{1.0f};
    quint32 rng = 1;
    quint32 nextId = 1;
    float masterDb = 0;
    std::atomic<float> busGain[3] = {{1.0f}, {1.0f}, {1.0f}};
    float busDb[3] = {0, 0, 0};
    EngineConfig config;
    std::atomic<bool> underrun{false};
};

} // namespace Private

using namespace Private;

namespace {

EngineConfig sanitised(EngineConfig c) {
    c.sampleRate = std::clamp(c.sampleRate, 8000, 192000);
    c.maxVoices = std::clamp(c.maxVoices, 1, 256);
    c.blockSize = std::clamp(c.blockSize, 16, 8192);
    c.queueCapacity = std::clamp(c.queueCapacity, 16, 65536);
    return c;
}

float dbToGain(float db) { return std::pow(10.0f, std::clamp(db, -120.0f, 24.0f) / 20.0f); }

} // namespace

/*!
    \class QtFM::EngineConfig
    \inmodule NtFM
    \brief The EngineConfig struct holds the settings of an Engine, fixed when it is constructed.

    \differences
    Only the OPL2 and OPL3 have emulation cores.
    \enddifferences
*/

/*!
    \variable QtFM::EngineConfig::sampleRate
    The sample rate in Hz, 48000 by default.
*/

/*!
    \variable QtFM::EngineConfig::maxVoices
    The number of voices that play at once, 32 by default. A play() beyond it steals a voice.
*/

/*!
    \variable QtFM::EngineConfig::blockSize
    The number of frames that a sound output renders at a time, 256 by default.
*/

/*!
    \variable QtFM::EngineConfig::format
    The sample format that the sound output asks for, SampleFormat::Float32 by default.
*/

/*!
    \variable QtFM::EngineConfig::seed
    The seed of the engine's random numbers; 0, the default, picks a random seed.
*/

/*!
    \variable QtFM::EngineConfig::queueCapacity
    The number of commands that can wait for the audio thread, 1024 by default. Commands sent
    while the queue is full are dropped.
*/

/*!
    \variable QtFM::EngineConfig::chips
    The chip cores to run, such as \c{{ChipType::OPL2}}, at most four. Chips without an emulation
    core are left out.

    \sa QtFM::Engine::chip()
*/

/*!
    \class QtFM::PlayOptions
    \inmodule NtFM
    \brief The PlayOptions struct holds how Engine::play() plays a patch.
*/

/*!
    \variable QtFM::PlayOptions::pitch
    Semitones added to the patch's base note.
*/

/*!
    \variable QtFM::PlayOptions::gain
    Gain in decibels, added to the patch's gain.
*/

/*!
    \variable QtFM::PlayOptions::pan
    Pan from -1 (left) to 1 (right), added to the patch's pan.
*/

/*!
    \variable QtFM::PlayOptions::duration
    Seconds until the note is released; -1, the default, takes the patch's duration, or runs a
    one-shot patch through its envelopes and holds a looping one until VoiceHandle::stop().
*/

/*!
    \variable QtFM::PlayOptions::delay
    Seconds from the engine's current render position until the voice starts. The start is sample
    accurate, for rhythmic effects.
*/

/*!
    \variable QtFM::PlayOptions::priority
    The priority of the voice: a voice of higher priority survives voice stealing.
*/

/*!
    \variable QtFM::PlayOptions::seed
    The seed of the patch's random variance; 0, the default, takes the next number from the
    engine's generator. The same seed gives the same sound.
*/

/*!
    \variable QtFM::PlayOptions::bus
    The bus of the voice: 0 for effects, 1 for music, 2 for the user interface.

    \sa QtFM::Engine::setBusGain()
*/

/*!
    \class QtFM::VoiceHandle
    \inmodule NtFM
    \brief The VoiceHandle class controls a voice that Engine::play() started.

    A handle is weak: it stays safe to use after the voice ends, and after the engine is destroyed,
    when its calls do nothing. Every call is thread-safe and non-blocking.

    \snippet fm/fmplay.cpp loop

    \differences
    Commands sent while the engine's queue is full are dropped.
    \enddifferences
*/

/*!
    Constructs a handle that refers to no voice.
*/
VoiceHandle::VoiceHandle() = default;

VoiceHandle::VoiceHandle(std::shared_ptr<Program> program, std::weak_ptr<EngineCore> core)
    : m_program(std::move(program)), m_core(std::move(core)) {}

/*!
    Constructs a copy of \a other, referring to the same voice.
*/
VoiceHandle::VoiceHandle(const VoiceHandle &other) = default;

/*!
    Makes this handle refer to the voice of \a other, and returns a reference to it.
*/
VoiceHandle &VoiceHandle::operator=(const VoiceHandle &other) = default;

/*!
    Destroys the handle. The voice plays on.
*/
VoiceHandle::~VoiceHandle() = default;

/*!
    Returns \c true if the handle refers to a voice, even one that has ended.
*/
bool VoiceHandle::isValid() const { return m_program != nullptr; }

/*!
    Returns the identifier of the voice, as Engine::voiceFinished() gives it, or 0.
*/
quint32 VoiceHandle::id() const { return m_program ? m_program->id : 0; }

/*!
    Returns \c true while the voice waits to start or plays.
*/
bool VoiceHandle::isPlaying() const {
    return m_program && m_program->state.load(std::memory_order_acquire) != Finished && !m_core.expired();
}

/*!
    Releases the voice, which then fades out through its release envelope; if \a immediate is
    \c true, it fades out in 2 ms instead.
*/
void VoiceHandle::stop(bool immediate) {
    if (!m_program)
        return;
    if (auto core = m_core.lock()) {
        Command c;
        c.type = Command::Stop;
        c.immediate = immediate;
        c.voiceId = m_program->id;
        core->send(c);
    }
}

/*!
    Sets the pitch of the voice to \a semitones above the note it started on.
*/
void VoiceHandle::setPitch(float semitones) { setParameter(ParamId::Pitch, semitones); }

/*!
    Sets the gain of the voice to \a dB.
*/
void VoiceHandle::setGain(float dB) { setParameter(ParamId::Gain, dB); }

/*!
    Sets the pan of the voice to \a pan, from -1 (left) to 1 (right).
*/
void VoiceHandle::setPan(float pan) { setParameter(ParamId::Pan, pan); }

/*!
    Sets the parameter \a id of the voice to \a value. The change is smoothed over about 5 ms.
*/
void VoiceHandle::setParameter(ParamId id, float value) { setParameterAt(id, value, 0); }

/*!
    Sets the parameter \a id of the voice to \a value at the absolute \a frame of the engine's
    output, as Engine::frame() counts it; a frame already rendered applies it at once. A scheduler
    such as MidiPlayer places controller changes on their samples this way.

    \sa QtFM::Engine::playAt()
*/
void VoiceHandle::setParameterAt(ParamId id, float value, quint64 frame) {
    if (!m_program || !std::isfinite(value))
        return;
    if (auto core = m_core.lock()) {
        Command c;
        c.type = Command::SetParam;
        c.param = static_cast<quint8>(id);
        c.value = value;
        c.voiceId = m_program->id;
        c.frame = frame;
        core->send(c);
    }
}

/*!
    \class QtFM::Engine
    \inmodule NtFM
    \brief The Engine class plays patches in real time, or renders them offline.

    An engine owns a fixed number of voices. play() starts a patch on one and returns a
    VoiceHandle; when every voice is busy, the voice of lowest priority, then the quietest, then
    the oldest fades out in 2 ms to make room.

    \snippet fm/fmplay.cpp engine

    start() connects the engine to a sound output, which renders on its own audio thread. Without
    start(), call render() to pull samples, for example into a mixer of the application's own or
    into a file.

    \section1 Threads

    play(), stopAll(), the gains and every VoiceHandle call are thread-safe and never wait for the
    audio thread: they send fixed-size commands through a lock-free queue. The audio thread never
    locks, allocates or touches a QObject: everything it needs is allocated by the constructor, and
    patches are compiled on the calling thread. render() must be called from one thread at a time.
    Denormals are flushed to zero while it runs.

    Finished voices and underruns are reported by voiceFinished() and underrun(), emitted from the
    engine's thread about 60 times a second, so they need its event loop.

    \section1 Chips

    An engine configured with chips in EngineConfig::chips runs their emulation cores in the same
    render. chip() gives register-level access, for ports of DOS games and for trackers. A patch made
    for the OPL2 or OPL3 (Patch::chip()) plays on a free channel of such a chip instead of the native
    engine, through the same play() call; music() plays VGM, DRO and IMF files on cores of its own.

    \differences
    Only the OPL2 and OPL3 are emulated. A voice on a chip channel follows its note, duration and
    pitch changes only: pitch envelopes, arpeggios and the other native parameters do not apply.
    There is no 2x oversampling, and no fixed-point synthesis path: render() into \c qint16 converts
    the floating-point output. The default output of start() is ALSA on Linux; elsewhere, pass an
    AudioOutput.
    \enddifferences

    \sa QtFM::Patch, QtFM::AudioOutput, {Using Nt FM}
*/

/*!
    Constructs an engine with \a config, as a child of \a parent. All the memory that the audio
    thread uses is allocated here.
*/
Engine::Engine(const EngineConfig &config, QObject *parent)
    : QObject(parent), m_core(std::make_shared<EngineCore>(sanitised(config))) {
    m_core->rng = m_core->config.seed ? m_core->config.seed : (QRandomGenerator::global()->generate() | 1u);
    m_core->config.seed = m_core->rng;
    for (std::size_t i = 0; i < m_core->chipTypes.size(); ++i)
        m_chips.emplace_back(new Chip(m_core.get(), static_cast<int>(i), m_core->chipTypes[i]));
    m_music.reset(new MusicPlayer(m_core.get()));
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Engine::dispatchEvents);
    timer->start(16);
}

/*!
    Destroys the engine, stopping its output. Handles to its voices stop playing.
*/
Engine::~Engine() {
    stop();
    QMutexLocker locker(&m_core->mutex);
    for (const auto &program : std::as_const(m_core->inFlight))
        program->state.store(Finished, std::memory_order_release);
}

/*!
    Returns the configuration of the engine. Its seed is the one in use, even if the configuration
    passed to the constructor asked for a random one.
*/
EngineConfig Engine::config() const { return m_core->config; }

/*!
    Starts rendering into \a output, opening it at the engine's sample rate, in stereo. If
    \a output is \c nullptr, the engine opens the platform's default output: the ALSA \c default
    device on Linux. Returns \c false if the output cannot be opened, or the engine is already
    started. The engine does not take ownership of \a output.

    \sa stop()
*/
bool Engine::start(AudioOutput *output) {
    if (m_output)
        return false;
    if (!output) {
        m_ownedOutput = createDefaultAudioOutput(m_core->config.blockSize);
        output = m_ownedOutput.get();
        if (!output)
            return false;
    }
    output->m_engine.store(this, std::memory_order_release);
    if (!output->open(m_core->config.sampleRate, 2, m_core->config.format)) {
        output->m_engine.store(nullptr, std::memory_order_release);
        m_ownedOutput.reset();
        return false;
    }
    m_output = output;
    return true;
}

/*!
    Stops rendering and closes the output. Voices keep their state, and continue if the engine
    is started again.
*/
void Engine::stop() {
    if (!m_output)
        return;
    m_output->close();
    m_output->m_engine.store(nullptr, std::memory_order_release);
    m_output = nullptr;
    m_ownedOutput.reset();
}

/*!
    Returns \c true if the engine renders into an output.
*/
bool Engine::isStarted() const { return m_output != nullptr; }

/*!
    Starts playing \a patch with \a options and returns a handle to the voice, or an invalid handle
    if the patch is invalid or the command queue is full. The patch is compiled, with its random
    variance drawn, on the calling thread.
*/
VoiceHandle Engine::play(const Patch &patch, const PlayOptions &options) {
    quint64 frame = 0;
    if (options.delay > 0.0f && std::isfinite(options.delay)) {
        frame = m_core->frame.load(std::memory_order_acquire) +
                static_cast<quint64>(std::min(options.delay, 3600.0f) * static_cast<float>(m_core->config.sampleRate));
    }
    return playAt(patch, options, frame);
}

/*!
    \overload

    Starts every note of \a sequence, each on its exact sample, with \a options applied to all of
    them; the delay of \a options delays the whole sequence. Returns a handle to each voice.
*/
QList<VoiceHandle> Engine::play(const Sequence &sequence, const PlayOptions &options) {
    QList<VoiceHandle> handles;
    const float rate = static_cast<float>(m_core->config.sampleRate);
    const float delay = options.delay > 0.0f && std::isfinite(options.delay) ? std::min(options.delay, 3600.0f) : 0.0f;
    // One base frame for every note, so the notes keep their spacing however the audio thread runs.
    // It is a block ahead, so that the first note is not late either.
    const quint64 base = m_core->frame.load(std::memory_order_acquire) +
                         static_cast<quint64>(m_core->config.blockSize) + static_cast<quint64>(delay * rate);
    for (const Sequence::Note &note : sequence.notes()) {
        PlayOptions o = options;
        o.pitch += note.pitch;
        o.gain += note.gain;
        if (note.length >= 0.0f)
            o.duration = static_cast<float>(sequence.secondsAt(note.length));
        handles.append(
            playAt(note.patch, o, base + static_cast<quint64>(sequence.secondsAt(note.beat) * double(rate))));
    }
    return handles;
}

/*!
    Starts playing \a patch with \a options at the absolute \a frame of the engine's output, as
    frame() counts it, and returns a handle to the voice; the delay of \a options is ignored. A frame
    already rendered starts the voice at once. A scheduler that streams notes, such as MidiPlayer,
    places each one on its sample this way.

    \sa frame()
*/
VoiceHandle Engine::playAt(const Patch &patch, const PlayOptions &options, quint64 frame) {
    if (!patch.isValid())
        return VoiceHandle();
    EngineCore &core = *m_core;
    quint32 seed;
    quint32 id;
    {
        QMutexLocker locker(&core.mutex);
        core.drain();
        seed = options.seed ? options.seed : xorshift32(core.rng);
        id = core.nextId++;
        if (core.nextId == 0)
            core.nextId = 1;
    }
    std::shared_ptr<Program> program = compileProgram(*patch.data(), options, core.config.sampleRate, seed);
    program->id = id;
    program->frame = frame;
    // A patch for an OPL chip plays on a channel of that chip, or of an OPL3, if the engine has one.
    if (patch.chip() == ChipType::OPL2 || patch.chip() == ChipType::OPL3) {
        int chip = -1;
        for (std::size_t i = 0; i < core.chipTypes.size(); ++i) {
            if (core.chipTypes[i] == patch.chip() || (chip < 0 && core.chipTypes[i] == ChipType::OPL3))
                chip = static_cast<int>(i);
        }
        if (chip >= 0 && oplInstrument(program->patch, program->opl)) {
            program->chip = chip;
            program->hz = 440.0f * std::exp2((program->note - 69.0f) / 12.0f);
            // The gain of the patch and the play becomes total level, in steps of 0.75 dB, on the
            // operators that sound: the carrier, and the modulator too when the connection is additive.
            const int steps =
                static_cast<int>(std::lround(-20.0 * std::log10(std::max(double(program->gain), 1e-6)) / 0.75));
            for (int op = (program->opl[10] & 1) ? 0 : 1; op < 2; ++op) {
                quint8 &level = program->opl[2 + op];
                const int tl = std::clamp((level & 0x3f) + steps, 0, 63);
                level = static_cast<quint8>((level & 0xc0) | tl);
            }
        }
    }
    Command c;
    c.type = Command::Play;
    c.program = program.get();
    c.frame = program->frame;
    c.voiceId = id;
    QMutexLocker locker(&core.mutex);
    core.inFlight.insert(id, program);
    if (!core.send(c)) {
        core.inFlight.remove(id);
        return VoiceHandle();
    }
    return VoiceHandle(program, m_core);
}

/*!
    Returns the number of frames rendered so far, as of the last render().

    \sa playAt()
*/
quint64 Engine::frame() const { return m_core->frame.load(std::memory_order_acquire); }

/*!
    Returns the chip of \a type that the engine was configured with, for register-level access, or
    \c nullptr.

    \sa QtFM::EngineConfig::chips
*/
Chip *Engine::chip(ChipType type) const {
    for (const auto &c : m_chips) {
        if (c->type() == type)
            return c.get();
    }
    return nullptr;
}

/*!
    Returns the engine's music player.
*/
MusicPlayer *Engine::music() const { return m_music.get(); }

/*!
    Releases every voice, including those waiting to start; if \a immediate is \c true, they fade
    out in 2 ms instead.
*/
void Engine::stopAll(bool immediate) {
    Command c;
    c.type = Command::StopAll;
    c.immediate = immediate;
    m_core->send(c);
}

/*!
    Renders \a frames frames of interleaved stereo into \a interleaved and returns the number of
    frames rendered. A sound output calls it on its audio thread; without start(), call it to pull
    samples or to render offline.
*/
int Engine::render(float *interleaved, int frames) {
    if (!interleaved || frames <= 0)
        return 0;
    EngineCore &core = *m_core;
    DenormalGuard guard;
    Command c;
    while (core.commands.pop(c))
        core.renderer.command(c);
    for (int i = 0; i < 3; ++i)
        core.renderer.busGain[i] = core.busGain[i].load(std::memory_order_relaxed);
    core.renderer.masterGain = core.masterGain.load(std::memory_order_relaxed);
    core.renderer.render(interleaved, frames);
    core.frame.store(core.renderer.frame(), std::memory_order_release);
    core.musicPosition.store(core.renderer.musicPosition(), std::memory_order_relaxed);
    core.musicPlaying.store(core.renderer.musicPlaying(), std::memory_order_relaxed);
    if (core.renderer.takeMusicFinished())
        core.musicFinished.store(true, std::memory_order_relaxed);
    core.active.store(core.renderer.activeVoices(), std::memory_order_relaxed);
    return frames;
}

/*!
    \overload

    Renders \a frames frames of interleaved stereo into \a interleaved as 16-bit samples.
*/
int Engine::render(qint16 *interleaved, int frames) {
    if (!interleaved || frames <= 0)
        return 0;
    const int block = m_core->config.blockSize;
    float *scratch = m_core->scratch.data();
    int done = 0;
    while (done < frames) {
        const int n = std::min(block, frames - done);
        render(scratch, n);
        toQ15(scratch, interleaved + 2 * done, 2 * n);
        done += n;
    }
    return frames;
}

/*!
    Sets the gain of \a bus (0 for effects, 1 for music, 2 for the user interface) to \a dB.
*/
void Engine::setBusGain(int bus, float dB) {
    if (bus < 0 || bus > 2 || !std::isfinite(dB))
        return;
    QMutexLocker locker(&m_core->mutex);
    m_core->busDb[bus] = dB;
    m_core->busGain[bus].store(dbToGain(dB), std::memory_order_relaxed);
}

/*!
    Returns the gain of \a bus in decibels.
*/
float Engine::busGain(int bus) const {
    if (bus < 0 || bus > 2)
        return 0;
    QMutexLocker locker(&m_core->mutex);
    return m_core->busDb[bus];
}

/*!
    Sets the gain of the mix, before its soft-clip limiter, to \a dB.
*/
void Engine::setMasterGain(float dB) {
    if (!std::isfinite(dB))
        return;
    QMutexLocker locker(&m_core->mutex);
    m_core->masterDb = dB;
    m_core->masterGain.store(dbToGain(dB), std::memory_order_relaxed);
}

/*!
    Returns the gain of the mix in decibels.
*/
float Engine::masterGain() const {
    QMutexLocker locker(&m_core->mutex);
    return m_core->masterDb;
}

/*!
    Returns the number of voices playing at the end of the last render().
*/
int Engine::activeVoices() const { return m_core->active.load(std::memory_order_relaxed); }

void Engine::dispatchEvents() {
    QList<quint32> finished;
    {
        QMutexLocker locker(&m_core->mutex);
        m_core->drain();
        finished.swap(m_core->finished);
    }
    for (quint32 id : std::as_const(finished))
        emit voiceFinished(id);
    if (m_core->underrun.exchange(false))
        emit underrun();
    if (m_core->musicFinished.exchange(false))
        emit musicFinished();
}

/*!
    \fn void QtFM::Engine::voiceFinished(quint32 voiceId)

    This signal is emitted when the voice \a voiceId has ended, or was dropped because no voice
    could be stolen for it.

    \sa QtFM::VoiceHandle::id()
*/

/*!
    \fn void QtFM::Engine::musicFinished()

    This signal is emitted when music that does not loop has reached its end.

    \sa QtFM::MusicPlayer
*/

/*!
    \fn void QtFM::Engine::underrun()

    This signal is emitted when the sound output ran out of samples, and played silence.
*/

/*!
    Renders \a patch once, played with \a options at \a sampleRate, and returns it as a 16-bit
    stereo WAV file. The render ends when the voice does, or after 60 seconds; a looping patch
    without a duration is released after 2 seconds. A seed of 0 in \a options means 1, so the
    same patch always gives the same file.

    \snippet fm/fmplay.cpp wav
*/
QByteArray Engine::renderToWav(const Patch &patch, int sampleRate, const PlayOptions &options) {
    if (!patch.isValid())
        return QByteArray();
    sampleRate = std::clamp(sampleRate, 8000, 192000);
    DenormalGuard guard;
    Renderer renderer(sampleRate, 1, 1, nullptr, nullptr);
    std::shared_ptr<Program> program =
        compileProgram(*patch.data(), options, sampleRate, options.seed ? options.seed : 1);
    program->id = 1;
    if (program->noteOff < 0 && program->patch.loop)
        program->noteOff = 2LL * sampleRate;
    Command c;
    c.type = Command::Play;
    c.program = program.get();
    if (options.delay > 0.0f && std::isfinite(options.delay))
        c.frame = static_cast<quint64>(std::min(options.delay, 10.0f) * static_cast<float>(sampleRate));
    renderer.command(c);
    const int block = 256;
    const qint64 limit = 60LL * sampleRate;
    std::vector<float> buffer(static_cast<std::size_t>(block) * 2);
    std::vector<qint16> pcm;
    qint64 frames = 0;
    while (program->state.load() != Finished && frames < limit) {
        renderer.render(buffer.data(), block);
        const std::size_t at = pcm.size();
        pcm.resize(at + buffer.size());
        toQ15(buffer.data(), pcm.data() + at, static_cast<qsizetype>(buffer.size()));
        frames += block;
    }
    return wavFile(pcm.data(), static_cast<qsizetype>(frames), sampleRate);
}

/*!
    \class QtFM::AudioOutput
    \inmodule NtFM
    \brief The AudioOutput class is the interface of a sound output that an Engine renders into.

    Implement open() to start a device or callback at the given rate and format, and call pull()
    from its audio thread to fill each buffer; close() stops it and must not return until the
    audio thread has stopped calling pull().

    \snippet fm/fmplay.cpp output

    \differences
    There is no Qt Multimedia backend. Nt FM has ALSA on Linux as the default output.
    \enddifferences
*/

/*!
    Constructs an output, not attached to an engine.
*/
AudioOutput::AudioOutput() = default;

/*!
    Destroys the output.
*/
AudioOutput::~AudioOutput() = default;

/*!
    \fn bool QtFM::AudioOutput::open(int sampleRate, int channels, QtFM::SampleFormat format)

    Opens the output for \a sampleRate frames a second of \a channels channels in \a format, and
    starts calling pull(). Returns \c false if it cannot.
*/

/*!
    \fn void QtFM::AudioOutput::close()

    Stops calling pull() and closes the output.
*/

/*!
    Renders \a frames frames of interleaved stereo into \a interleaved from the attached engine, and
    returns the number of frames rendered. Writes silence and returns 0 if no engine is attached.
*/
int AudioOutput::pull(float *interleaved, int frames) {
    Engine *engine = m_engine.load(std::memory_order_acquire);
    if (!engine) {
        std::fill(interleaved, interleaved + 2 * std::max(frames, 0), 0.0f);
        return 0;
    }
    return engine->render(interleaved, frames);
}

/*!
    \overload

    Renders \a frames frames of 16-bit interleaved stereo into \a interleaved.
*/
int AudioOutput::pull(qint16 *interleaved, int frames) {
    Engine *engine = m_engine.load(std::memory_order_acquire);
    if (!engine) {
        std::fill(interleaved, interleaved + 2 * std::max(frames, 0), qint16(0));
        return 0;
    }
    return engine->render(interleaved, frames);
}

/*!
    Tells the attached engine that the device ran out of samples, so that it emits
    Engine::underrun(). Safe to call from the audio thread.
*/
void AudioOutput::reportUnderrun() {
    if (Engine *engine = m_engine.load(std::memory_order_acquire))
        engine->m_core->underrun.store(true, std::memory_order_relaxed);
}

/*!
    \class QtFM::ChipCore
    \inmodule NtFM
    \brief The ChipCore class is the interface of a chip emulation core.

    A core models a sound chip at its native rate: write() sets a register, and generate() produces
    the next frames of stereo output, in the chip's own 16-bit-scale integers. The engine resamples
    it to its own rate. create() makes the built-in cores: an OPL2 or an OPL3, emulated by Nt FM's
    own clean-room model of the chips.

    \code
    std::unique_ptr<QtFM::ChipCore> opl = QtFM::ChipCore::create(QtFM::ChipType::OPL2);
    opl->write(0x20, 0x01); // and so on, as an AdLib driver would
    \endcode

    \differences
    The OPLL, OPN2 and OPM have no core yet; nor does the SN76489 that VGM files of the Mega Drive
    also use. The OPL model is not cycle-accurate.
    \enddifferences
*/

/*!
    \fn QtFM::ChipCore::~ChipCore()

    Destroys the core.
*/

/*!
    \fn QtFM::ChipType QtFM::ChipCore::type() const

    Returns the chip that the core emulates.
*/

/*!
    \fn int QtFM::ChipCore::nativeRate() const

    Returns the rate in Hz at which the chip produces samples: 49716 for the OPL2 and OPL3.
*/

/*!
    \fn void QtFM::ChipCore::reset()

    Resets every register and silences the chip.
*/

/*!
    \fn void QtFM::ChipCore::write(quint16 reg, quint8 value)

    Writes \a value to the register \a reg. Registers of the second bank of the OPL3 are 0x100 to
    0x1FF.
*/

/*!
    \fn void QtFM::ChipCore::generate(qint32 *stereo, int frames)

    Writes the next \a frames frames of interleaved stereo into \a stereo.
*/

/*!
    \fn std::unique_ptr<QtFM::ChipCore> QtFM::ChipCore::create(QtFM::ChipType type)

    Returns a new core for \a type, or \c nullptr if Nt FM has none: ChipType::OPL2 and
    ChipType::OPL3 have one.
*/

/*!
    \class QtFM::Chip
    \inmodule NtFM
    \brief The Chip class gives register-level access to a chip core that an Engine runs.

    Get a chip from Engine::chip(), after naming it in EngineConfig::chips. Writes are sent to the
    audio thread like every other command, so they are thread-safe and never block, and a delay
    places a write on an exact sample:

    \snippet fm/fmplay.cpp chip

    Voices that Engine::play() starts on the chip use its channels too: a game that writes
    registers itself should not also play patches for that chip.

    \differences
    Registers cannot be read back.
    \enddifferences
*/

Chip::Chip(EngineCore *core, int index, ChipType type) : m_core(core), m_index(index), m_type(type) {}

/*!
    Returns the type of the chip.
*/
ChipType Chip::type() const { return m_type; }

/*!
    Writes \a value to the register \a reg of the chip, \a delay seconds after the engine's current
    render position.
*/
void Chip::writeRegister(quint16 reg, quint8 value, float delay) {
    Command c;
    c.type = Command::ChipWrite;
    c.param = static_cast<quint8>(m_index);
    c.voiceId = quint32(reg & 0x1ff) | (quint32(value) << 16);
    if (delay > 0.0f && std::isfinite(delay)) {
        c.frame = m_core->frame.load(std::memory_order_acquire) +
                  static_cast<quint64>(std::min(delay, 3600.0f) * static_cast<float>(m_core->config.sampleRate));
    }
    m_core->send(c);
}

/*!
    Resets the chip and frees every channel that voices were playing on.
*/
void Chip::reset() {
    Command c;
    c.type = Command::ChipReset;
    c.param = static_cast<quint8>(m_index);
    m_core->send(c);
}

/*!
    Sets the gain of the chip's output to \a dB.
*/
void Chip::setGain(float dB) {
    if (!std::isfinite(dB))
        return;
    Command c;
    c.type = Command::ChipGain;
    c.param = static_cast<quint8>(m_index);
    c.value = dbToGain(dB);
    m_core->send(c);
}

/*!
    \class QtFM::MusicPlayer
    \inmodule NtFM
    \brief The MusicPlayer class plays chip music from register logs.

    The player reads VGM and gzipped VGZ files with an OPL2 or OPL3 part, DOSBox Raw OPL (DRO)
    captures, and id Software's IMF files, and plays them on a chip core of its own, beside the
    engine's sound effects. Writes reach the chip on the chip's own clock, so the timing is exact,
    and VGM loop points are honoured.

    \snippet fm/fmplay.cpp music

    \differences
    RAD files and chips other than the OPL2 and OPL3 (the YM2612 and SN76489 parts of a Mega Drive
    VGM, for example) are not played. VGZ needs Nt to be built with zlib. Seeking is not supported.
    \enddifferences

    \sa QtFM::Engine::music()
*/

MusicPlayer::MusicPlayer(EngineCore *core) : m_core(core) {}

/*!
    Loads the music in \a data, in \a format or, for MusicFormat::Auto, recognised by its header
    (IMF has none, and is what remains). Returns \c false, with the reason in \a error, if it cannot
    be played. The new music replaces the old, stopped.
*/
bool MusicPlayer::load(const QByteArray &data, MusicFormat format, QString *error) {
    std::unique_ptr<MusicData> music = parseMusic(data, format, m_imfRate, error);
    if (!music)
        return false;
    const double duration = music->duration;
    const ChipType chip = music->chip;
    Command c;
    c.type = Command::MusicLoad;
    c.music = music.release(); // the audio thread owns it now, and gives it back to be freed
    c.value = m_gain;
    if (!m_core->send(c)) {
        delete c.music;
        if (error)
            *error = QStringLiteral("the command queue is full");
        return false;
    }
    m_duration = duration;
    m_chip = chip;
    setLoops(m_loops);
    return true;
}

/*!
    Starts or resumes playing.
*/
void MusicPlayer::play() {
    Command c;
    c.type = Command::MusicPlay;
    m_core->send(c);
}

/*!
    Pauses, keeping the position.
*/
void MusicPlayer::pause() {
    Command c;
    c.type = Command::MusicPause;
    m_core->send(c);
}

/*!
    Stops and returns to the start.
*/
void MusicPlayer::stop() {
    Command c;
    c.type = Command::MusicStop;
    m_core->send(c);
}

/*!
    Sets whether the music starts again at its loop point, or at its start if it has none, when it
    ends, to \a loops. Music loops by default.
*/
void MusicPlayer::setLoops(bool loops) {
    m_loops = loops;
    Command c;
    c.type = Command::MusicLoops;
    c.immediate = loops;
    m_core->send(c);
}

/*!
    Returns \c true if the music loops.
*/
bool MusicPlayer::loops() const { return m_loops; }

/*!
    Sets the gain of the music to \a dB.
*/
void MusicPlayer::setGain(float dB) {
    if (!std::isfinite(dB))
        return;
    m_gain = dbToGain(dB);
    Command c;
    c.type = Command::MusicGain;
    c.value = m_gain;
    m_core->send(c);
}

/*!
    Returns \c true once music has been loaded.
*/
bool MusicPlayer::isLoaded() const { return m_chip != ChipType::None; }

/*!
    Returns \c true while the music plays, as of the last render.
*/
bool MusicPlayer::isPlaying() const { return m_core->musicPlaying.load(std::memory_order_relaxed); }

/*!
    Returns the position in seconds, as of the last render.
*/
double MusicPlayer::position() const { return m_core->musicPosition.load(std::memory_order_relaxed); }

/*!
    Returns the length of the music in seconds.
*/
double MusicPlayer::duration() const { return m_duration; }

/*!
    Returns the chip that the music plays on.
*/
ChipType MusicPlayer::chip() const { return m_chip; }

/*!
    Sets the tick rate of IMF files loaded from now on to \a ticksPerSecond: 560 (the default) for
    most games, 700 for Wolfenstein 3D, 280 for Duke Nukem II.
*/
void MusicPlayer::setImfRate(int ticksPerSecond) { m_imfRate = std::clamp(ticksPerSecond, 1, 10000); }

/*!
    Returns the tick rate of IMF files.
*/
int MusicPlayer::imfRate() const { return m_imfRate; }

/*!
    \fn QByteArray QtFM::renderToWav(const QtFM::Patch &patch, int sampleRate, const QtFM::PlayOptions &options)

    Renders \a patch at \a sampleRate with \a options and returns a WAV file; the same as
    Engine::renderToWav().
*/
QByteArray renderToWav(const Patch &patch, int sampleRate, const PlayOptions &options) {
    return Engine::renderToWav(patch, sampleRate, options);
}

} // namespace QtFM

#include "moc_qfmengine.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
