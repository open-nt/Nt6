// MIT License - see LICENSE
//
// The engine: control calls from any thread compile programs and send fixed-size commands through
// a lock-free queue; the audio thread applies them in render() and hands finished programs back
// through a second queue, which a control thread drains and frees. Only control threads take the
// mutex; render() never does.

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
        : config(c), commands(static_cast<std::size_t>(c.queueCapacity)),
          returns(static_cast<std::size_t>(c.queueCapacity + 2 * c.maxVoices + 16)),
          renderer(c.sampleRate, c.maxVoices, c.queueCapacity, &EngineCore::released, this),
          scratch(static_cast<std::size_t>(c.blockSize) * 2) {}

    static void released(void *context, Program *program) {
        // A full queue leaves the program to the engine's destructor; it is already Finished.
        static_cast<EngineCore *>(context)->returns.push(program);
    }

    // Frees the programs the audio thread has finished with. Called with mutex held.
    void drain() {
        Program *program = nullptr;
        while (returns.pop(program)) {
            finished.append(program->id);
            inFlight.remove(program->id);
        }
    }

    bool send(const Command &command) { return commands.push(command); }

    EngineConfig config;
    MpscQueue<Command> commands;
    SpscQueue<Program *> returns;
    Renderer renderer;
    std::vector<float> scratch;
    std::atomic<quint64> frame{0};
    std::atomic<int> active{0};
    std::atomic<float> busGain[3] = {{1.0f}, {1.0f}, {1.0f}};
    std::atomic<float> masterGain{1.0f};
    std::atomic<bool> underrun{false};

    // Control side, under mutex.
    QMutex mutex;
    quint32 rng = 1;
    quint32 nextId = 1;
    float busDb[3] = {0, 0, 0};
    float masterDb = 0;
    QHash<quint32, std::shared_ptr<Program>> inFlight;
    QList<quint32> finished;
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
    There is no list of chip cores to instantiate: Nt FM has no chip emulation yet.
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
void VoiceHandle::setParameter(ParamId id, float value) {
    if (!m_program || !std::isfinite(value))
        return;
    if (auto core = m_core.lock()) {
        Command c;
        c.type = Command::SetParam;
        c.param = static_cast<quint8>(id);
        c.value = value;
        c.voiceId = m_program->id;
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

    \differences
    The native engine only: there are no chip cores (\c chip()), no music player (\c music()) and no
    sequences. There is no 2x oversampling, and no fixed-point synthesis path: render() into
    \c qint16 converts the floating-point output. The default output of start() is ALSA on Linux;
    elsewhere, pass an AudioOutput.
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
    if (options.delay > 0.0f && std::isfinite(options.delay)) {
        program->frame =
            core.frame.load(std::memory_order_acquire) +
            static_cast<quint64>(std::min(options.delay, 3600.0f) * static_cast<float>(core.config.sampleRate));
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
}

/*!
    \fn void QtFM::Engine::voiceFinished(quint32 voiceId)

    This signal is emitted when the voice \a voiceId has ended, or was dropped because no voice
    could be stolen for it.

    \sa QtFM::VoiceHandle::id()
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
    \fn QByteArray QtFM::renderToWav(const QtFM::Patch &patch, int sampleRate, const QtFM::PlayOptions &options)

    Renders \a patch at \a sampleRate with \a options and returns a WAV file; the same as
    Engine::renderToWav().
*/
QByteArray renderToWav(const Patch &patch, int sampleRate, const PlayOptions &options) {
    return Engine::renderToWav(patch, sampleRate, options);
}

} // namespace QtFM

#include "moc_qfmengine.cpp"
