// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qurl.h>
#include <QtMultimedia/qaudiodevice.h>

#include <memory>

class QSoundEffectPrivate;

class QSoundEffect : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(int loops READ loopCount WRITE setLoopCount NOTIFY loopCountChanged)
    Q_PROPERTY(int loopsRemaining READ loopsRemaining NOTIFY loopsRemainingChanged)
    Q_PROPERTY(float volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QAudioDevice audioDevice READ audioDevice WRITE setAudioDevice NOTIFY audioDeviceChanged)
public:
    enum Loop { Infinite = -2 };
    Q_ENUM(Loop)

    enum Status { Null, Loading, Ready, Error };
    Q_ENUM(Status)

    explicit QSoundEffect(QObject *parent = nullptr);
    explicit QSoundEffect(const QAudioDevice &audioDevice, QObject *parent = nullptr);
    ~QSoundEffect() override;

    static QStringList supportedMimeTypes();

    QUrl source() const;
    void setSource(const QUrl &url);

    int loopCount() const;
    int loopsRemaining() const;
    void setLoopCount(int loopCount);

    QAudioDevice audioDevice();
    void setAudioDevice(const QAudioDevice &device);

    float volume() const;
    void setVolume(float volume);

    bool isMuted() const;
    void setMuted(bool muted);

    bool isLoaded() const;
    bool isPlaying() const;
    Status status() const;

public slots:
    void play();
    void stop();

signals:
    /*!
        This signal is emitted when the source changes.
    */
    void sourceChanged() { emitSignal(&QSoundEffect::sourceChanged); }
    /*!
        This signal is emitted when the loop count changes.
    */
    void loopCountChanged() { emitSignal(&QSoundEffect::loopCountChanged); }
    /*!
        This signal is emitted when the number of loops left to play changes.
    */
    void loopsRemainingChanged() { emitSignal(&QSoundEffect::loopsRemainingChanged); }
    /*!
        This signal is emitted when the volume changes.
    */
    void volumeChanged() { emitSignal(&QSoundEffect::volumeChanged); }
    /*!
        This signal is emitted when the effect is muted or unmuted.
    */
    void mutedChanged() { emitSignal(&QSoundEffect::mutedChanged); }
    /*!
        This signal is emitted when the sound has been loaded, or unloaded.
    */
    void loadedChanged() { emitSignal(&QSoundEffect::loadedChanged); }
    /*!
        This signal is emitted when the effect starts or stops playing.
    */
    void playingChanged() { emitSignal(&QSoundEffect::playingChanged); }
    /*!
        This signal is emitted when the status changes.
    */
    void statusChanged() { emitSignal(&QSoundEffect::statusChanged); }
    /*!
        This signal is emitted when the audio device changes.
    */
    void audioDeviceChanged() { emitSignal(&QSoundEffect::audioDeviceChanged); }

private:
    friend class QSoundEffectPrivate;
    std::unique_ptr<QSoundEffectPrivate> d;
};
