// MIT License - see LICENSE
//
// The QML types of Nt FM. One engine serves the process: FmEngine creates it on first use, with the
// chips that the sounds of the loaded documents asked for, and starts it on the default output.

#include "qfmpatch_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qfile.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qtimer.h>
#include <QtFMQml/qfmqml.h>
#include <QtQml/qqml.h>
#include <QtQml/qqmlcontext.h>

#include <algorithm>
#include <cmath>

namespace QtFM {

namespace {

FmEngine *g_engine = nullptr;
const QObject *g_musicOwner = nullptr;

ChipType toChipType(int chip) {
    return chip >= int(ChipType::None) && chip <= int(ChipType::OPM) ? static_cast<ChipType>(chip) : ChipType::None;
}

// Reads the file a QML url names: qrc: and :/ resources, file: urls and paths, relative to the
// document of object.
QByteArray readUrl(const QObject *object, const QUrl &url, QString *error) {
    QUrl resolved = url;
    if (QQmlContext *context = qmlContext(object))
        resolved = context->resolvedUrl(url);
    QString path;
    if (resolved.scheme() == QLatin1String("qrc"))
        path = QLatin1Char(':') + resolved.path();
    else if (resolved.isLocalFile())
        path = resolved.toLocalFile();
    else if (resolved.scheme().isEmpty())
        path = resolved.toString();
    else {
        *error = QStringLiteral("unsupported url: %1").arg(url.toString());
        return QByteArray();
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("cannot read %1").arg(path);
        return QByteArray();
    }
    return file.readAll();
}

// Returns patch with its loop flag and chip changed, through its JSON.
Patch withOverrides(const Patch &patch, bool setLoop, bool loop, ChipType chip) {
    if (!patch.isValid() || ((!setLoop || patch.isLoop() == loop) && patch.chip() == chip))
        return patch;
    QJsonObject o = QJsonDocument::fromJson(patch.toJson()).object();
    if (setLoop)
        o.insert(QLatin1String("loop"), loop);
    static const char *const names[] = {"", "opl2", "opl3", "opll", "opn2", "opm"};
    o.insert(QLatin1String("chip"), chip == ChipType::None ? QJsonValue(QJsonValue::Null)
                                                           : QJsonValue(QLatin1String(names[static_cast<int>(chip)])));
    return Patch::fromJson(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace

/*!
    \module NtFMQml
    \title Nt FM QML Types
    \brief QML types for Nt FM: a sound is one declarative element.
    \cmaketarget Nt::FMQml

    Nt FM QML makes FM sound effects and chip music declarative. Register the types once, then
    import \c QtFM in a document:

    \snippet fm/fmqml.cpp register

    See \l{Using Nt FM} for the sounds themselves.
*/

/*!
    \fn void QtFM::registerQmlTypes(const char *uri)

    Registers the Nt FM types under the module \a uri, version 1.0: FmEngine and FmPatch as
    singletons, FmSound, FmSoundBank, FmSequence and FmMusic as elements, and FmChip and
    FmPatchValue for their enumerations and values. Call it before loading documents.
*/
void registerQmlTypes(const char *uri) {
    qmlRegisterSingletonInstance(uri, 1, 0, "FmEngine", FmEngine::instance());
    qmlRegisterSingletonType<FmPatch>(uri, 1, 0, "FmPatch",
                                      [](QQmlEngine *, QJSEngine *) -> QObject * { return new FmPatch; });
    qmlRegisterUncreatableType<FmChip>(uri, 1, 0, "FmChip", QStringLiteral("FmChip holds the chip enumeration"));
    qmlRegisterUncreatableType<FmPatchValue>(uri, 1, 0, "FmPatchValue", QStringLiteral("make patches with FmPatch"));
    qmlRegisterType<FmSound>(uri, 1, 0, "FmSound");
    qmlRegisterType<FmSoundBank>(uri, 1, 0, "FmSoundBank");
    qmlRegisterType<FmSequence>(uri, 1, 0, "FmSequence");
    qmlRegisterType<FmMusic>(uri, 1, 0, "FmMusic");
}

// --- FmChip ---------------------------------------------------------------------------------------

/*!
    \class QtFM::FmChip
    \inmodule NtFMQml
    \brief The FmChip type holds the chips that FmSound::chip names, as \c{FmChip.OPL2}.

    \differences
    An enumeration holder only; it cannot be created.
    \enddifferences
*/

/*!
    \enum QtFM::FmChip::Chip

    A chip, as QtFM::ChipType.

    \value None The native engine.
    \value OPL2 The YM3812 of the AdLib.
    \value OPL3 The YMF262 of later Sound Blasters.
    \value OPLL The YM2413; not emulated, so its sounds play on the native engine.
    \value OPN2 The YM2612; not emulated, so its sounds play on the native engine.
    \value OPM The YM2151; not emulated, so its sounds play on the native engine.
*/

/*!
    Constructs the holder with \a parent.
*/
FmChip::FmChip(QObject *parent) : QObject(parent) {}

// --- FmPatchValue ------------------------------------------------------------------------------------

/*!
    \class QtFM::FmPatchValue
    \inmodule NtFMQml
    \brief The FmPatchValue type holds one patch in QML, as FmPatch makes it.

    Assign it to FmSound::patch. It is immutable: FmPatch.mutated() returns a new one.

    \differences
    The patch is an object rather than a value type, as Nt QML has no value types of its own.
    \enddifferences
*/

/*!
    Constructs a value holding \a patch, with \a parent.
*/
FmPatchValue::FmPatchValue(const Patch &patch, QObject *parent) : QObject(parent), m_patch(patch) {}

/*!
    Returns the patch.
*/
Patch FmPatchValue::patch() const { return m_patch; }

/*!
    Returns \c true if the value holds a patch.
*/
bool FmPatchValue::isValid() const { return m_patch.isValid(); }

/*!
    Returns the name of the patch.
*/
QString FmPatchValue::name() const { return m_patch.name(); }

/*!
    Returns the category of the patch.
*/
QString FmPatchValue::category() const { return m_patch.category(); }

/*!
    Returns \c true if the patch sustains until stopped.
*/
bool FmPatchValue::isLoop() const { return m_patch.isLoop(); }

/*!
    Returns the chip that the patch targets, an FmChip value.
*/
int FmPatchValue::chip() const { return static_cast<int>(m_patch.chip()); }

/*!
    Returns the patch as JSON.
*/
QString FmPatchValue::json() const { return QString::fromUtf8(m_patch.toJson()); }

// --- FmPatch ------------------------------------------------------------------------------------------

/*!
    \class QtFM::FmPatch
    \inmodule NtFMQml
    \brief The FmPatch singleton makes patches: generated, parsed or mutated.

    \code
    FmSound { patch: FmPatch.generate(FmPatch.Laser, 7) }
    \endcode

    \differences
    The patches it returns are FmPatchValue objects, owned by the script engine.
    \enddifferences
*/

/*!
    \enum QtFM::FmPatch::Category

    The categories of generate(), as QtFM::Category.

    \value Pickup A coin or pick-up.
    \value Laser A laser.
    \value Explosion An explosion.
    \value PowerUp A power-up.
    \value Hit A hit.
    \value Jump A jump.
    \value Blip A user-interface blip.
    \value Alarm An alarm.
    \value Engine A looping engine hum.
*/

/*!
    Constructs the factory with \a parent.
*/
FmPatch::FmPatch(QObject *parent) : QObject(parent) {}

/*!
    Returns a patch of \a category generated from \a seed, within the limits of \a chip (an
    FmChip value; FmChip.None for the native engine).

    \sa QtFM::Patch::generate()
*/
FmPatchValue *FmPatch::generate(int category, int seed, int chip) const {
    const int c = std::clamp(category, 0, static_cast<int>(Category::Engine));
    return new FmPatchValue(
        Patch::generate(static_cast<QtFM::Category>(c), static_cast<quint32>(seed), toChipType(chip)));
}

/*!
    Returns the patch in the JSON text \a json; it is not FmPatchValue::valid if the text is not a
    patch.
*/
FmPatchValue *FmPatch::fromJson(const QString &json) const { return new FmPatchValue(Patch::fromJson(json.toUtf8())); }

/*!
    Returns a variation of \a patch by up to \a amount, drawn from \a seed.

    \sa QtFM::Patch::mutated()
*/
FmPatchValue *FmPatch::mutated(FmPatchValue *patch, double amount, int seed) const {
    return new FmPatchValue(patch ? patch->patch().mutated(static_cast<float>(amount), static_cast<quint32>(seed))
                                  : Patch());
}

/*!
    Returns \c true if \a patch fits the limits of \a chip.
*/
bool FmPatch::fitsChip(FmPatchValue *patch, int chip) const {
    return patch && patch->patch().fitsChip(toChipType(chip));
}

// --- FmEngine -----------------------------------------------------------------------------------------

/*!
    \class QtFM::FmEngine
    \inmodule NtFMQml
    \brief The FmEngine singleton is the engine that every Nt FM QML type plays through.

    The engine is made the first time a sound plays, with the chips that the sounds of the loaded
    documents asked for, and started on the default sound output unless autoStart is \c false.
    Its sampleRate and maxVoices only take effect if they are set before that.

    \code
    Component.onCompleted: FmEngine.setBusGain(1, -12) // quieter music
    \endcode

    \differences
    One engine serves the whole process. Its chips are fixed once it has been made.
    \enddifferences
*/

/*!
    Returns the engine of the process, making it on first use.
*/
FmEngine *FmEngine::instance() {
    if (!g_engine)
        g_engine = new FmEngine(QCoreApplication::instance());
    return g_engine;
}

FmEngine::FmEngine(QObject *parent) : QObject(parent) {
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &FmEngine::poll);
    timer->start(16);
}

/*!
    Stops and destroys the engine of the process, so that the next sound makes a new one; for tests.
*/
void FmEngine::reset() {
    if (!g_engine)
        return;
    g_engine->m_engine.reset();
    g_engine->m_started = false;
    g_engine->m_config.chips.clear();
    g_musicOwner = nullptr;
}

/*!
    Returns the engine, making it, and starting it if autoStart is set, if it does not exist yet.
*/
Engine *FmEngine::engine() {
    if (!m_engine) {
        m_engine = std::make_unique<Engine>(m_config);
        m_engine->setMasterGain(static_cast<float>(m_masterGain));
        for (int b = 0; b < 3; ++b)
            m_engine->setBusGain(b, static_cast<float>(m_busGain[b]));
        connect(m_engine.get(), &Engine::voiceFinished, this, &FmEngine::voiceFinished);
        connect(m_engine.get(), &Engine::musicFinished, this, &FmEngine::musicFinished);
        if (m_autoStart)
            start();
    }
    return m_engine.get();
}

/*!
    Asks for \a chip to be emulated by the engine, if it has not been made yet.
*/
void FmEngine::requireChip(ChipType chip) {
    if (chip == ChipType::None || m_engine || m_config.chips.contains(chip))
        return;
    m_config.chips.append(chip);
}

/*!
    Returns the sample rate in Hz, 48000 by default.
*/
int FmEngine::sampleRate() const { return m_config.sampleRate; }

/*!
    Sets the sampleRate property to \a rate.

    \sa sampleRate()
*/
void FmEngine::setSampleRate(int rate) {
    if (m_engine || rate == m_config.sampleRate)
        return;
    m_config.sampleRate = rate;
    emit sampleRateChanged();
}

/*!
    Returns the number of native voices, 32 by default.
*/
int FmEngine::maxVoices() const { return m_config.maxVoices; }

/*!
    Sets the maxVoices property to \a voices.

    \sa maxVoices()
*/
void FmEngine::setMaxVoices(int voices) {
    if (m_engine || voices == m_config.maxVoices)
        return;
    m_config.maxVoices = voices;
    emit maxVoicesChanged();
}

/*!
    Returns the gain of the mix in decibels.
*/
double FmEngine::masterGain() const { return m_masterGain; }

/*!
    Sets the masterGain property to \a dB.

    \sa masterGain()
*/
void FmEngine::setMasterGain(double dB) {
    if (dB == m_masterGain)
        return;
    m_masterGain = dB;
    if (m_engine)
        m_engine->setMasterGain(static_cast<float>(dB));
    emit masterGainChanged();
}

/*!
    Returns the number of voices playing.
*/
int FmEngine::activeVoices() const { return m_activeVoices; }

/*!
    Returns \c true if the engine plays to a sound output.
*/
bool FmEngine::isStarted() const { return m_started; }

/*!
    Returns \c true if the engine starts on the default output when it is made; \c true by default. A test sets it to \c false and pulls samples from engine() instead.
*/
bool FmEngine::autoStart() const { return m_autoStart; }

/*!
    Sets the autoStart property to \a start.

    \sa autoStart()
*/
void FmEngine::setAutoStart(bool start) {
    if (start == m_autoStart)
        return;
    m_autoStart = start;
    emit autoStartChanged();
}

/*!
    Returns the gain of \a bus in decibels.
*/
double FmEngine::busGain(int bus) const { return bus >= 0 && bus < 3 ? m_busGain[bus] : 0.0; }

/*!
    Sets the gain of \a bus (0 effects, 1 music, 2 interface) to \a dB.
*/
void FmEngine::setBusGain(int bus, double dB) {
    if (bus < 0 || bus > 2)
        return;
    m_busGain[bus] = dB;
    if (m_engine)
        m_engine->setBusGain(bus, static_cast<float>(dB));
}

/*!
    Starts the engine on the default sound output; returns \c false if there is none.
*/
bool FmEngine::start() {
    const bool started = engine()->isStarted() || m_engine->start();
    if (started != m_started) {
        m_started = started;
        emit startedChanged();
    }
    return started;
}

/*!
    Stops the sound output.
*/
void FmEngine::stop() {
    if (m_engine)
        m_engine->stop();
    if (m_started) {
        m_started = false;
        emit startedChanged();
    }
}

/*!
    Fades out every voice.
*/
void FmEngine::stopAll() {
    if (m_engine)
        m_engine->stopAll();
}

void FmEngine::poll() {
    const int active = m_engine ? m_engine->activeVoices() : 0;
    if (active != m_activeVoices) {
        m_activeVoices = active;
        emit activeVoicesChanged();
    }
}

/*!
    \fn void QtFM::FmEngine::voiceFinished(quint32 voiceId)
    This signal is emitted when the voice \a voiceId ends.
*/

/*!
    \fn void QtFM::FmEngine::musicFinished()
    This signal is emitted when music that does not loop ends.
*/

/*!
    \fn void QtFM::FmEngine::sampleRateChanged()

    This signal is emitted when sampleRate() changes.
*/

/*!
    \fn void QtFM::FmEngine::maxVoicesChanged()

    This signal is emitted when maxVoices() changes.
*/

/*!
    \fn void QtFM::FmEngine::masterGainChanged()

    This signal is emitted when masterGain() changes.
*/

/*!
    \fn void QtFM::FmEngine::activeVoicesChanged()

    This signal is emitted when activeVoices() changes.
*/

/*!
    \fn void QtFM::FmEngine::startedChanged()

    This signal is emitted when isStarted() changes.
*/

/*!
    \fn void QtFM::FmEngine::autoStartChanged()

    This signal is emitted when autoStart() changes.
*/

// --- FmSound ------------------------------------------------------------------------------------------

/*!
    \class QtFM::FmSound
    \inmodule NtFMQml
    \brief The FmSound type is one sound effect: a patch, and how it plays.

    Set source to a patch file (JSON, or an AdLib SBI instrument) or patch to an FmPatchValue,
    then call play(). Each play() starts another voice; maxInstances caps how many run at once.
    pitch, gain and pan apply to the voices that play, smoothed, so they can be bound to the game:

    \snippet fm/fmqml.cpp sound

    Setting chip to FmChip.OPL2 or FmChip.OPL3 plays the sound on the emulated chip; FmChip.None,
    the default, on the native engine.

    \differences
    Network urls are not loaded. pan replaces the pan of playing voices rather than adding to it.
    \enddifferences
*/

/*!
    Constructs a sound with \a parent.
*/
FmSound::FmSound(QObject *parent) : QObject(parent) {
    connect(FmEngine::instance(), &FmEngine::voiceFinished, this, &FmSound::voiceFinished);
    m_rng ^= static_cast<quint32>(reinterpret_cast<quintptr>(this) >> 4);
}

/*!
    Destroys the sound. Its voices play on.
*/
FmSound::~FmSound() = default;

/*!
    \reimp
*/
void FmSound::classBegin() {}

/*!
    \reimp
*/
void FmSound::componentComplete() {
    m_complete = true;
    if (m_chip)
        FmEngine::instance()->requireChip(toChipType(m_chip));
    load();
}

void FmSound::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

void FmSound::load() {
    if (!m_complete || m_source.isEmpty())
        return;
    QString error;
    const QByteArray data = readUrl(this, m_source, &error);
    Patch patch;
    if (error.isEmpty()) {
        patch = data.startsWith("SBI\x1a") ? Patch::fromChipInstrument(data, InstrumentFormat::Sbi, &error)
                                           : Patch::fromJson(data, &error);
    }
    m_loaded = patch;
    setError(patch.isValid() ? QString() : error);
    refresh();
}

void FmSound::refresh() {
    const Patch base = m_patch.isValid() ? m_patch : m_loaded;
    m_effective = withOverrides(base, m_loopSet, m_loop, toChipType(m_chip));
}

/*!
    Returns the patch that play() plays: patch if set, else the one loaded from source, with loop
    and chip applied.
*/
Patch FmSound::currentPatch() const { return m_effective; }

/*!
    Returns the url of a patch file: JSON, or an SBI instrument. Relative urls are relative to the document.
*/
QUrl FmSound::source() const { return m_source; }

/*!
    Sets the source property to \a source.

    \sa source()
*/
void FmSound::setSource(const QUrl &source) {
    if (source == m_source)
        return;
    m_source = source;
    load();
    emit sourceChanged();
}

/*!
    Returns the patch set from FmPatch, which is played instead of source, or \c nullptr.
*/
FmPatchValue *FmSound::patch() const { return m_patchObject; }

/*!
    Sets the patch property to \a patch.

    \sa patch()
*/
void FmSound::setPatch(FmPatchValue *patch) {
    m_patchObject = patch;
    m_patch = patch ? patch->patch() : Patch();
    refresh();
    emit patchChanged();
}

/*!
    Returns the chip to play on, an FmChip value; FmChip.None for the native engine. Set it before the engine plays its first sound.
*/
int FmSound::chip() const { return m_chip; }

/*!
    Sets the chip property to \a chip.

    \sa chip()
*/
void FmSound::setChip(int chip) {
    if (chip == m_chip)
        return;
    m_chip = chip;
    if (m_complete)
        FmEngine::instance()->requireChip(toChipType(chip));
    refresh();
    emit chipChanged();
}

/*!
    Returns the semitones added to the patch's note; changing them also moves the voices playing.
*/
double FmSound::pitch() const { return m_pitch; }

/*!
    Sets the pitch property to \a semitones.

    \sa pitch()
*/
void FmSound::setPitch(double semitones) {
    if (semitones == m_pitch || !std::isfinite(semitones))
        return;
    m_pitch = semitones;
    for (Instance &i : m_voices)
        i.voice.setPitch(static_cast<float>(m_pitch - i.pitch));
    emit pitchChanged();
}

/*!
    Returns the gain in decibels; changing it also affects the voices playing.
*/
double FmSound::gain() const { return m_gain; }

/*!
    Sets the gain property to \a dB.

    \sa gain()
*/
void FmSound::setGain(double dB) {
    if (dB == m_gain || !std::isfinite(dB))
        return;
    m_gain = dB;
    for (Instance &i : m_voices)
        i.voice.setGain(static_cast<float>(m_gain - i.gain));
    emit gainChanged();
}

/*!
    Returns the pan, from -1 to 1; changing it also moves the voices playing.
*/
double FmSound::pan() const { return m_pan; }

/*!
    Sets the pan property to \a pan.

    \sa pan()
*/
void FmSound::setPan(double pan) {
    if (pan == m_pan || !std::isfinite(pan))
        return;
    m_pan = pan;
    for (Instance &i : m_voices)
        i.voice.setPan(static_cast<float>(pan));
    emit panChanged();
}

/*!
    Returns \c true if the sound sustains until stop(); the patch's own setting until it is set.
*/
bool FmSound::loop() const { return m_loopSet ? m_loop : m_effective.isLoop(); }

/*!
    Sets the loop property to \a loop.

    \sa loop()
*/
void FmSound::setLoop(bool loop) {
    if (m_loopSet && loop == m_loop)
        return;
    m_loop = loop;
    m_loopSet = true;
    refresh();
    emit loopChanged();
}

/*!
    Returns the pitch variance: each play() moves the pitch by a random amount of up to this many
    semitones either way.
*/
double FmSound::pitchVariance() const { return m_pitchVariance; }

/*!
    Sets the pitchVariance property to \a semitones.

    \sa pitchVariance()
*/
void FmSound::setPitchVariance(double semitones) {
    if (semitones == m_pitchVariance)
        return;
    m_pitchVariance = std::max(0.0, semitones);
    emit pitchVarianceChanged();
}

/*!
    Returns the bus: 0 effects (the default), 1 music, 2 interface.
*/
int FmSound::bus() const { return m_bus; }

/*!
    Sets the bus property to \a bus.

    \sa bus()
*/
void FmSound::setBus(int bus) {
    if (bus == m_bus)
        return;
    m_bus = bus;
    emit busChanged();
}

/*!
    Returns the priority of the voices: a higher one survives voice stealing.
*/
int FmSound::priority() const { return m_priority; }

/*!
    Sets the priority property to \a priority.

    \sa priority()
*/
void FmSound::setPriority(int priority) {
    if (priority == m_priority)
        return;
    m_priority = priority;
    emit priorityChanged();
}

/*!
    Returns the number of voices of this sound that play at once; 0, the default, for no limit. A play() beyond it stops the oldest.
*/
int FmSound::maxInstances() const { return m_maxInstances; }

/*!
    Sets the maxInstances property to \a count.

    \sa maxInstances()
*/
void FmSound::setMaxInstances(int count) {
    if (count == m_maxInstances)
        return;
    m_maxInstances = std::max(0, count);
    emit maxInstancesChanged();
}

/*!
    Returns \c true if a voice of the sound plays.
*/
bool FmSound::isPlaying() const { return m_playing; }

/*!
    Returns why source could not be loaded, or an empty string.
*/
QString FmSound::error() const { return m_error; }

/*!
    Starts a voice of the sound.
*/
void FmSound::play() {
    if (!m_effective.isValid())
        return;
    updatePlaying();
    while (m_maxInstances > 0 && m_voices.size() >= m_maxInstances) {
        m_voices.first().voice.stop(true);
        m_voices.removeFirst();
    }
    PlayOptions options;
    const double variance = m_pitchVariance > 0.0 ? m_pitchVariance * double(Private::randomBipolar(m_rng)) : 0.0;
    options.pitch = static_cast<float>(m_pitch + variance);
    options.gain = static_cast<float>(m_gain);
    options.pan = static_cast<float>(m_pan);
    options.bus = m_bus;
    options.priority = m_priority;
    const VoiceHandle voice = FmEngine::instance()->engine()->play(m_effective, options);
    if (!voice.isValid())
        return;
    m_voices.append(Instance{voice, m_pitch, m_gain});
    updatePlaying();
}

/*!
    Releases every voice of the sound; if \a immediate is \c true, they fade out in 2 ms instead.
*/
void FmSound::stop(bool immediate) {
    for (Instance &i : m_voices)
        i.voice.stop(immediate);
}

void FmSound::voiceFinished(quint32 id) {
    for (const Instance &i : std::as_const(m_voices)) {
        if (i.voice.id() == id) {
            updatePlaying();
            return;
        }
    }
}

void FmSound::updatePlaying() {
    m_voices.erase(
        std::remove_if(m_voices.begin(), m_voices.end(), [](const Instance &i) { return !i.voice.isPlaying(); }),
        m_voices.end());
    const bool playing = !m_voices.isEmpty();
    if (playing == m_playing)
        return;
    m_playing = playing;
    emit playingChanged();
    if (!playing)
        emit finished();
}

/*!
    \fn void QtFM::FmSound::finished()
    This signal is emitted when the last voice of the sound ends.
*/

/*!
    \fn void QtFM::FmSound::sourceChanged()

    This signal is emitted when source() changes.
*/

/*!
    \fn void QtFM::FmSound::patchChanged()

    This signal is emitted when patch() changes.
*/

/*!
    \fn void QtFM::FmSound::chipChanged()

    This signal is emitted when chip() changes.
*/

/*!
    \fn void QtFM::FmSound::pitchChanged()

    This signal is emitted when pitch() changes.
*/

/*!
    \fn void QtFM::FmSound::gainChanged()

    This signal is emitted when gain() changes.
*/

/*!
    \fn void QtFM::FmSound::panChanged()

    This signal is emitted when pan() changes.
*/

/*!
    \fn void QtFM::FmSound::loopChanged()

    This signal is emitted when loop() changes.
*/

/*!
    \fn void QtFM::FmSound::pitchVarianceChanged()

    This signal is emitted when pitchVariance() changes.
*/

/*!
    \fn void QtFM::FmSound::busChanged()

    This signal is emitted when bus() changes.
*/

/*!
    \fn void QtFM::FmSound::priorityChanged()

    This signal is emitted when priority() changes.
*/

/*!
    \fn void QtFM::FmSound::maxInstancesChanged()

    This signal is emitted when maxInstances() changes.
*/

/*!
    \fn void QtFM::FmSound::playingChanged()

    This signal is emitted when isPlaying() changes.
*/

/*!
    \fn void QtFM::FmSound::errorChanged()

    This signal is emitted when error() changes.
*/

// --- FmSoundBank --------------------------------------------------------------------------------------

/*!
    \class QtFM::FmSoundBank
    \inmodule NtFMQml
    \brief The FmSoundBank type loads a bank of patches, by name.

    \snippet fm/fmqml.cpp bank

    \differences
    Banks load from JSON files only: CBOR and IBK banks are read from C++ with QtFM::PatchBank.
    \enddifferences
*/

/*!
    Constructs a bank with \a parent.
*/
FmSoundBank::FmSoundBank(QObject *parent) : QObject(parent) {}

/*!
    Returns the url of a bank's JSON file.
*/
QUrl FmSoundBank::source() const { return m_source; }

/*!
    Sets the source property to \a source.

    \sa source()
*/
void FmSoundBank::setSource(const QUrl &source) {
    if (source == m_source)
        return;
    m_source = source;
    QString error;
    const QByteArray data = readUrl(this, source, &error);
    m_bank = error.isEmpty() ? PatchBank::fromJson(data, &error) : PatchBank();
    if (error != m_error) {
        m_error = error;
        emit errorChanged();
    }
    emit sourceChanged();
    emit namesChanged();
}

/*!
    Returns the names of the patches.
*/
QStringList FmSoundBank::names() const { return m_bank.names(); }

/*!
    Returns the number of patches.
*/
int FmSoundBank::count() const { return m_bank.size(); }

/*!
    Returns why source could not be loaded, or an empty string.
*/
QString FmSoundBank::error() const { return m_error; }

/*!
    Returns the bank.
*/
PatchBank FmSoundBank::bank() const { return m_bank; }

/*!
    Returns the patch named \a name, for FmSound::patch.
*/
FmPatchValue *FmSoundBank::sound(const QString &name) const { return new FmPatchValue(m_bank.patch(name)); }

/*!
    \fn void QtFM::FmSoundBank::sourceChanged()

    This signal is emitted when source() changes.
*/

/*!
    \fn void QtFM::FmSoundBank::namesChanged()

    This signal is emitted when names() changes.
*/

/*!
    \fn void QtFM::FmSoundBank::errorChanged()

    This signal is emitted when error() changes.
*/

// --- FmSequence ---------------------------------------------------------------------------------------

/*!
    \class QtFM::FmSequence
    \inmodule NtFMQml
    \brief The FmSequence type plays notes on a tempo grid, for jingles.

    Each note is an object with a \c beat and either the \c sound name of a patch in bank or a
    \c patch from FmPatch, and optionally \c pitch (semitones), \c length (beats) and \c gain (dB).
    Every note starts on its exact sample.

    \snippet fm/fmqml.cpp sequence

    \differences
    A looping sequence is restarted by a timer, so its repeats are not sample-accurate.
    \enddifferences
*/

/*!
    Constructs a sequence with \a parent.
*/
FmSequence::FmSequence(QObject *parent) : QObject(parent), m_timer(new QTimer(this)) {
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, [this] {
        if (m_loops && m_playing) {
            schedule();
            return;
        }
        m_playing = false;
        emit playingChanged();
        emit finished();
    });
}

/*!
    Destroys the sequence. Its notes play on.
*/
FmSequence::~FmSequence() = default;

/*!
    Returns the bank that notes name their sounds in.
*/
FmSoundBank *FmSequence::bank() const { return m_bank; }

/*!
    Sets the bank property to \a bank.

    \sa bank()
*/
void FmSequence::setBank(FmSoundBank *bank) {
    if (bank == m_bank)
        return;
    m_bank = bank;
    emit bankChanged();
}

/*!
    Returns the tempo in beats a minute, 120 by default.
*/
double FmSequence::tempo() const { return m_tempo; }

/*!
    Sets the tempo property to \a beatsPerMinute.

    \sa tempo()
*/
void FmSequence::setTempo(double beatsPerMinute) {
    if (beatsPerMinute == m_tempo || !(beatsPerMinute > 0))
        return;
    m_tempo = beatsPerMinute;
    emit tempoChanged();
}

/*!
    Returns the notes, as a list of objects.
*/
QVariantList FmSequence::notes() const { return m_notes; }

/*!
    Sets the notes property to \a notes.

    \sa notes()
*/
void FmSequence::setNotes(const QVariantList &notes) {
    m_notes = notes;
    emit notesChanged();
}

/*!
    Returns \c true if the sequence starts again when it ends.
*/
bool FmSequence::loops() const { return m_loops; }

/*!
    Sets the loops property to \a loops.

    \sa loops()
*/
void FmSequence::setLoops(bool loops) {
    if (loops == m_loops)
        return;
    m_loops = loops;
    emit loopsChanged();
}

/*!
    Returns \c true if the sequence plays.
*/
bool FmSequence::isPlaying() const { return m_playing; }

/*!
    Returns the notes as a QtFM::Sequence, with their patches looked up.
*/
Sequence FmSequence::sequence() const {
    Sequence seq;
    seq.setTempo(static_cast<float>(m_tempo));
    for (const QVariant &v : m_notes) {
        const QVariantMap note = v.toMap();
        Patch patch;
        if (auto *value = qobject_cast<FmPatchValue *>(note.value(QStringLiteral("patch")).value<QObject *>()))
            patch = value->patch();
        else if (m_bank)
            patch = m_bank->bank().patch(note.value(QStringLiteral("sound")).toString());
        seq.addNote(note.value(QStringLiteral("beat")).toFloat(), patch, note.value(QStringLiteral("pitch")).toFloat(),
                    note.contains(QStringLiteral("length")) ? note.value(QStringLiteral("length")).toFloat() : -1.0f,
                    note.value(QStringLiteral("gain")).toFloat());
    }
    return seq;
}

void FmSequence::schedule() {
    const Sequence seq = sequence();
    m_voices = FmEngine::instance()->engine()->play(seq);
    m_timer->start(static_cast<int>(std::ceil(seq.secondsAt(seq.duration()) * 1000.0)));
}

/*!
    Plays the sequence from its start.
*/
void FmSequence::play() {
    stop();
    m_playing = true;
    schedule();
    emit playingChanged();
}

/*!
    Stops the sequence and fades out its notes.
*/
void FmSequence::stop() {
    for (VoiceHandle &v : m_voices)
        v.stop(true);
    m_voices.clear();
    m_timer->stop();
    if (m_playing) {
        m_playing = false;
        emit playingChanged();
    }
}

/*!
    \fn void QtFM::FmSequence::finished()
    This signal is emitted when a sequence that does not loop has played its last beat.
*/

/*!
    \fn void QtFM::FmSequence::bankChanged()

    This signal is emitted when bank() changes.
*/

/*!
    \fn void QtFM::FmSequence::tempoChanged()

    This signal is emitted when tempo() changes.
*/

/*!
    \fn void QtFM::FmSequence::notesChanged()

    This signal is emitted when notes() changes.
*/

/*!
    \fn void QtFM::FmSequence::loopsChanged()

    This signal is emitted when loops() changes.
*/

/*!
    \fn void QtFM::FmSequence::playingChanged()

    This signal is emitted when isPlaying() changes.
*/

// --- FmMusic ------------------------------------------------------------------------------------------

/*!
    \class QtFM::FmMusic
    \inmodule NtFMQml
    \brief The FmMusic type plays chip music: VGM, VGZ, DRO and IMF files.

    \snippet fm/fmqml.cpp music

    \differences
    The engine has one music player: playing an FmMusic stops any other.
    \enddifferences

    \sa QtFM::MusicPlayer
*/

/*!
    Constructs a music element with \a parent.
*/
FmMusic::FmMusic(QObject *parent) : QObject(parent) {
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &FmMusic::poll);
    timer->start(50);
    connect(FmEngine::instance(), &FmEngine::musicFinished, this, [this] {
        if (g_musicOwner == this) {
            poll();
            emit finished();
        }
    });
}

/*!
    Returns the url of the music file.
*/
QUrl FmMusic::source() const { return m_source; }

/*!
    Sets the source property to \a source.

    \sa source()
*/
void FmMusic::setSource(const QUrl &source) {
    if (source == m_source)
        return;
    m_source = source;
    m_loaded = false;
    QString error;
    m_data = readUrl(this, source, &error);
    if (error != m_error) {
        m_error = error;
        emit errorChanged();
    }
    emit sourceChanged();
}

bool FmMusic::ensureLoaded() {
    MusicPlayer *player = FmEngine::instance()->engine()->music();
    if (g_musicOwner == this && m_loaded)
        return true;
    QString error;
    if (!player->load(m_data, MusicFormat::Auto, &error)) {
        m_error = error;
        emit errorChanged();
        return false;
    }
    g_musicOwner = this;
    m_loaded = true;
    player->setLoops(m_loops);
    player->setGain(static_cast<float>(m_gain));
    m_duration = player->duration();
    emit durationChanged();
    return true;
}

/*!
    Returns \c true if the music loops; \c true by default.
*/
bool FmMusic::loops() const { return m_loops; }

/*!
    Sets the loops property to \a loops.

    \sa loops()
*/
void FmMusic::setLoops(bool loops) {
    if (loops == m_loops)
        return;
    m_loops = loops;
    if (g_musicOwner == this)
        FmEngine::instance()->engine()->music()->setLoops(loops);
    emit loopsChanged();
}

/*!
    Returns the gain in decibels.
*/
double FmMusic::gain() const { return m_gain; }

/*!
    Sets the gain property to \a dB.

    \sa gain()
*/
void FmMusic::setGain(double dB) {
    if (dB == m_gain)
        return;
    m_gain = dB;
    if (g_musicOwner == this)
        FmEngine::instance()->engine()->music()->setGain(static_cast<float>(dB));
    emit gainChanged();
}

/*!
    Returns the position in seconds.
*/
double FmMusic::position() const { return m_position; }

/*!
    Returns the length in seconds, known once the music has been played.
*/
double FmMusic::duration() const { return m_duration; }

/*!
    Returns \c true if the music plays.
*/
bool FmMusic::isPlaying() const { return m_playing; }

/*!
    Returns why the music could not be loaded, or an empty string.
*/
QString FmMusic::error() const { return m_error; }

/*!
    Starts or resumes the music.
*/
void FmMusic::play() {
    if (ensureLoaded())
        FmEngine::instance()->engine()->music()->play();
}

/*!
    Pauses the music.
*/
void FmMusic::pause() {
    if (g_musicOwner == this)
        FmEngine::instance()->engine()->music()->pause();
}

/*!
    Stops the music and returns to its start.
*/
void FmMusic::stop() {
    if (g_musicOwner == this)
        FmEngine::instance()->engine()->music()->stop();
}

void FmMusic::poll() {
    const bool mine = g_musicOwner == this;
    const MusicPlayer *player = mine ? FmEngine::instance()->engine()->music() : nullptr;
    const bool playing = player && player->isPlaying();
    const double position = player ? player->position() : 0.0;
    if (position != m_position) {
        m_position = position;
        emit positionChanged();
    }
    if (playing != m_playing) {
        m_playing = playing;
        emit playingChanged();
    }
}

/*!
    \fn void QtFM::FmMusic::finished()
    This signal is emitted when music that does not loop ends.
*/

/*!
    \fn void QtFM::FmMusic::sourceChanged()

    This signal is emitted when source() changes.
*/

/*!
    \fn void QtFM::FmMusic::loopsChanged()

    This signal is emitted when loops() changes.
*/

/*!
    \fn void QtFM::FmMusic::gainChanged()

    This signal is emitted when gain() changes.
*/

/*!
    \fn void QtFM::FmMusic::positionChanged()

    This signal is emitted when position() changes.
*/

/*!
    \fn void QtFM::FmMusic::durationChanged()

    This signal is emitted when duration() changes.
*/

/*!
    \fn void QtFM::FmMusic::playingChanged()

    This signal is emitted when isPlaying() changes.
*/

/*!
    \fn void QtFM::FmMusic::errorChanged()

    This signal is emitted when error() changes.
*/
} // namespace QtFM

#include "moc_qfmqml.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
