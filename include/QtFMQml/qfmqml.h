// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qurl.h>
#include <QtCore/qvariant.h>
#include <QtFM/qfmbank.h>
#include <QtFM/qfmengine.h>
#include <QtQml/qqmlparserstatus.h>

#include <memory>

namespace QtFM {

void registerQmlTypes(const char *uri = "QtFM");

class FmChip : public QObject {
    Q_OBJECT
public:
    enum Chip { None, OPL2, OPL3, OPLL, OPN2, OPM };
    Q_ENUM(Chip)
    explicit FmChip(QObject *parent = nullptr);
};

class FmPatchValue : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool valid READ isValid CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString category READ category CONSTANT)
    Q_PROPERTY(bool loop READ isLoop CONSTANT)
    Q_PROPERTY(int chip READ chip CONSTANT)
    Q_PROPERTY(QString json READ json CONSTANT)
public:
    explicit FmPatchValue(const Patch &patch = Patch(), QObject *parent = nullptr);

    Patch patch() const;
    bool isValid() const;
    QString name() const;
    QString category() const;
    bool isLoop() const;
    int chip() const;
    QString json() const;

private:
    Patch m_patch;
};

class FmPatch : public QObject {
    Q_OBJECT
public:
    enum Category { Pickup, Laser, Explosion, PowerUp, Hit, Jump, Blip, Alarm, Engine };
    Q_ENUM(Category)
    explicit FmPatch(QObject *parent = nullptr);

    Q_INVOKABLE QtFM::FmPatchValue *generate(int category, int seed, int chip = 0) const;
    Q_INVOKABLE QtFM::FmPatchValue *fromJson(const QString &json) const;
    Q_INVOKABLE QtFM::FmPatchValue *mutated(QtFM::FmPatchValue *patch, double amount, int seed) const;
    Q_INVOKABLE bool fitsChip(QtFM::FmPatchValue *patch, int chip) const;
};

class FmEngine : public QObject {
    Q_OBJECT
    Q_PROPERTY(int sampleRate READ sampleRate WRITE setSampleRate NOTIFY sampleRateChanged)
    Q_PROPERTY(int maxVoices READ maxVoices WRITE setMaxVoices NOTIFY maxVoicesChanged)
    Q_PROPERTY(double masterGain READ masterGain WRITE setMasterGain NOTIFY masterGainChanged)
    Q_PROPERTY(int activeVoices READ activeVoices NOTIFY activeVoicesChanged)
    Q_PROPERTY(bool started READ isStarted NOTIFY startedChanged)
    Q_PROPERTY(bool autoStart READ autoStart WRITE setAutoStart NOTIFY autoStartChanged)
public:
    static FmEngine *instance();

    int sampleRate() const;
    void setSampleRate(int rate);
    int maxVoices() const;
    void setMaxVoices(int voices);
    double masterGain() const;
    void setMasterGain(double dB);
    int activeVoices() const;
    bool isStarted() const;
    bool autoStart() const;
    void setAutoStart(bool start);

    Q_INVOKABLE double busGain(int bus) const;
    Q_INVOKABLE void setBusGain(int bus, double dB);
    Q_INVOKABLE bool start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void stopAll();

    QtFM::Engine *engine();
    void requireChip(ChipType chip);
    static void reset();

signals:
    void sampleRateChanged();
    void maxVoicesChanged();
    void masterGainChanged();
    void activeVoicesChanged();
    void startedChanged();
    void autoStartChanged();
    void voiceFinished(quint32 voiceId);
    void musicFinished();

private:
    explicit FmEngine(QObject *parent = nullptr);
    void poll();
    std::unique_ptr<QtFM::Engine> m_engine;
    EngineConfig m_config;
    double m_masterGain = 0;
    double m_busGain[3] = {0, 0, 0};
    int m_activeVoices = 0;
    bool m_autoStart = true;
    bool m_started = false;
};

class FmSound : public QObject, public QQmlParserStatus {
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QtFM::FmPatchValue *patch READ patch WRITE setPatch NOTIFY patchChanged)
    Q_PROPERTY(int chip READ chip WRITE setChip NOTIFY chipChanged)
    Q_PROPERTY(double pitch READ pitch WRITE setPitch NOTIFY pitchChanged)
    Q_PROPERTY(double gain READ gain WRITE setGain NOTIFY gainChanged)
    Q_PROPERTY(double pan READ pan WRITE setPan NOTIFY panChanged)
    Q_PROPERTY(bool loop READ loop WRITE setLoop NOTIFY loopChanged)
    Q_PROPERTY(double pitchVariance READ pitchVariance WRITE setPitchVariance NOTIFY pitchVarianceChanged)
    Q_PROPERTY(int bus READ bus WRITE setBus NOTIFY busChanged)
    Q_PROPERTY(int priority READ priority WRITE setPriority NOTIFY priorityChanged)
    Q_PROPERTY(int maxInstances READ maxInstances WRITE setMaxInstances NOTIFY maxInstancesChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    explicit FmSound(QObject *parent = nullptr);
    ~FmSound() override;

    QUrl source() const;
    void setSource(const QUrl &source);
    FmPatchValue *patch() const;
    void setPatch(FmPatchValue *patch);
    int chip() const;
    void setChip(int chip);
    double pitch() const;
    void setPitch(double semitones);
    double gain() const;
    void setGain(double dB);
    double pan() const;
    void setPan(double pan);
    bool loop() const;
    void setLoop(bool loop);
    double pitchVariance() const;
    void setPitchVariance(double semitones);
    int bus() const;
    void setBus(int bus);
    int priority() const;
    void setPriority(int priority);
    int maxInstances() const;
    void setMaxInstances(int count);
    bool isPlaying() const;
    QString error() const;

    Patch currentPatch() const;

    Q_INVOKABLE void play();
    Q_INVOKABLE void stop(bool immediate = false);

    void classBegin() override;
    void componentComplete() override;

signals:
    void sourceChanged();
    void patchChanged();
    void chipChanged();
    void pitchChanged();
    void gainChanged();
    void panChanged();
    void loopChanged();
    void pitchVarianceChanged();
    void busChanged();
    void priorityChanged();
    void maxInstancesChanged();
    void playingChanged();
    void errorChanged();
    void finished();

private:
    void load();
    void refresh();
    void updatePlaying();
    void voiceFinished(quint32 id);
    void setError(const QString &error);
    QUrl m_source;
    QPointer<FmPatchValue> m_patchObject;
    Patch m_patch;
    Patch m_loaded;
    struct Instance {
        VoiceHandle voice;
        double pitch;
        double gain;
    };
    QList<Instance> m_voices;
    Patch m_effective;
    QString m_error;
    double m_pitch = 0;
    double m_gain = 0;
    double m_pan = 0;
    double m_pitchVariance = 0;
    quint32 m_rng = 0x2545F491u;
    int m_chip = 0;
    int m_bus = 0;
    int m_priority = 0;
    int m_maxInstances = 0;
    bool m_loop = false;
    bool m_loopSet = false;
    bool m_complete = false;
    bool m_playing = false;
};

class FmSoundBank : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QStringList names READ names NOTIFY namesChanged)
    Q_PROPERTY(int count READ count NOTIFY namesChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    explicit FmSoundBank(QObject *parent = nullptr);

    QUrl source() const;
    void setSource(const QUrl &source);
    QStringList names() const;
    int count() const;
    QString error() const;
    PatchBank bank() const;

    Q_INVOKABLE QtFM::FmPatchValue *sound(const QString &name) const;

signals:
    void sourceChanged();
    void namesChanged();
    void errorChanged();

private:
    QUrl m_source;
    PatchBank m_bank;
    QString m_error;
};

class FmSequence : public QObject {
    Q_OBJECT
    Q_PROPERTY(QtFM::FmSoundBank *bank READ bank WRITE setBank NOTIFY bankChanged)
    Q_PROPERTY(double tempo READ tempo WRITE setTempo NOTIFY tempoChanged)
    Q_PROPERTY(QVariantList notes READ notes WRITE setNotes NOTIFY notesChanged)
    Q_PROPERTY(bool loops READ loops WRITE setLoops NOTIFY loopsChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)
public:
    explicit FmSequence(QObject *parent = nullptr);
    ~FmSequence() override;

    FmSoundBank *bank() const;
    void setBank(FmSoundBank *bank);
    double tempo() const;
    void setTempo(double beatsPerMinute);
    QVariantList notes() const;
    void setNotes(const QVariantList &notes);
    bool loops() const;
    void setLoops(bool loops);
    bool isPlaying() const;
    Sequence sequence() const;

    Q_INVOKABLE void play();
    Q_INVOKABLE void stop();

signals:
    void bankChanged();
    void tempoChanged();
    void notesChanged();
    void loopsChanged();
    void playingChanged();
    void finished();

private:
    void schedule();
    QPointer<FmSoundBank> m_bank;
    QVariantList m_notes;
    QList<VoiceHandle> m_voices;
    class QTimer *m_timer;
    double m_tempo = 120;
    bool m_loops = false;
    bool m_playing = false;
};

class FmMusic : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool loops READ loops WRITE setLoops NOTIFY loopsChanged)
    Q_PROPERTY(double gain READ gain WRITE setGain NOTIFY gainChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    explicit FmMusic(QObject *parent = nullptr);

    QUrl source() const;
    void setSource(const QUrl &source);
    bool loops() const;
    void setLoops(bool loops);
    double gain() const;
    void setGain(double dB);
    double position() const;
    double duration() const;
    bool isPlaying() const;
    QString error() const;

    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();

signals:
    void sourceChanged();
    void loopsChanged();
    void gainChanged();
    void positionChanged();
    void durationChanged();
    void playingChanged();
    void errorChanged();
    void finished();

private:
    bool ensureLoaded();
    void poll();
    QUrl m_source;
    QByteArray m_data;
    QString m_error;
    double m_gain = 0;
    double m_position = 0;
    double m_duration = 0;
    bool m_loops = true;
    bool m_loaded = false;
    bool m_playing = false;
};

} // namespace QtFM
