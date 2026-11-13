// MIT License - see LICENSE
#pragma once

#include <QtCore/qiodevice.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qurl.h>

#include <memory>

class QAudioOutput;
class QMediaPlayerPrivate;

class QMediaPlayer : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(float bufferProgress READ bufferProgress NOTIFY bufferProgressChanged)
    Q_PROPERTY(bool hasAudio READ hasAudio NOTIFY hasAudioChanged)
    Q_PROPERTY(bool hasVideo READ hasVideo NOTIFY hasVideoChanged)
    Q_PROPERTY(bool seekable READ isSeekable NOTIFY seekableChanged)
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(qreal playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(int loops READ loops WRITE setLoops NOTIFY loopsChanged)
    Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(MediaStatus mediaStatus READ mediaStatus NOTIFY mediaStatusChanged)
    Q_PROPERTY(Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(QAudioOutput *audioOutput READ audioOutput WRITE setAudioOutput NOTIFY audioOutputChanged)
    Q_PROPERTY(QObject *videoOutput READ videoOutput WRITE setVideoOutput NOTIFY videoOutputChanged)
public:
    enum PlaybackState { StoppedState, PlayingState, PausedState };
    Q_ENUM(PlaybackState)

    enum MediaStatus {
        NoMedia,
        LoadingMedia,
        LoadedMedia,
        StalledMedia,
        BufferingMedia,
        BufferedMedia,
        EndOfMedia,
        InvalidMedia
    };
    Q_ENUM(MediaStatus)

    enum Error { NoError, ResourceError, FormatError, NetworkError, AccessDeniedError };
    Q_ENUM(Error)

    enum Loops { Infinite = -1, Once = 1 };
    Q_ENUM(Loops)

    explicit QMediaPlayer(QObject *parent = nullptr);
    ~QMediaPlayer() override;

    QUrl source() const;
    const QIODevice *sourceDevice() const;

    QAudioOutput *audioOutput() const;
    void setAudioOutput(QAudioOutput *output);
    QObject *videoOutput() const;
    void setVideoOutput(QObject *output);

    PlaybackState playbackState() const;
    MediaStatus mediaStatus() const;

    qint64 duration() const;
    qint64 position() const;

    bool hasAudio() const;
    bool hasVideo() const;
    float bufferProgress() const;
    bool isSeekable() const;
    bool isPlaying() const;
    qreal playbackRate() const;

    int loops() const;
    void setLoops(int loops);

    Error error() const;
    QString errorString() const;

    bool isAvailable() const;

public slots:
    void play();
    void pause();
    void stop();

    void setPosition(qint64 position);
    void setPlaybackRate(qreal rate);

    void setSource(const QUrl &source);
    void setSourceDevice(QIODevice *device, const QUrl &sourceUrl = QUrl());

signals:
    /*!
        This signal is emitted when the source changes to \a media.
    */
    void sourceChanged(const QUrl &media) { emitSignal(&QMediaPlayer::sourceChanged, media); }
    /*!
        This signal is emitted when the playback state changes to \a newState.
    */
    void playbackStateChanged(QMediaPlayer::PlaybackState newState) {
        emitSignal(&QMediaPlayer::playbackStateChanged, newState);
    }
    /*!
        This signal is emitted when the media status changes to \a status.
    */
    void mediaStatusChanged(QMediaPlayer::MediaStatus status) { emitSignal(&QMediaPlayer::mediaStatusChanged, status); }
    /*!
        This signal is emitted when the duration of the media changes to \a duration milliseconds.
    */
    void durationChanged(qint64 duration) { emitSignal(&QMediaPlayer::durationChanged, duration); }
    /*!
        This signal is emitted when the playback position changes to \a position milliseconds:
        while playing, about every 50 ms, and on a seek.
    */
    void positionChanged(qint64 position) { emitSignal(&QMediaPlayer::positionChanged, position); }
    /*!
        This signal is emitted when the media gains or loses audio; \a available is the new state.
    */
    void hasAudioChanged(bool available) { emitSignal(&QMediaPlayer::hasAudioChanged, available); }
    /*!
        This signal is emitted when the media gains or loses video; \a videoAvailable is the new state.
    */
    void hasVideoChanged(bool videoAvailable) { emitSignal(&QMediaPlayer::hasVideoChanged, videoAvailable); }
    /*!
        This signal is emitted when the buffer fills to \a progress, from 0 to 1.
    */
    void bufferProgressChanged(float progress) { emitSignal(&QMediaPlayer::bufferProgressChanged, progress); }
    /*!
        This signal is emitted when the media becomes seekable or stops being seekable; \a seekable
        is the new state.
    */
    void seekableChanged(bool seekable) { emitSignal(&QMediaPlayer::seekableChanged, seekable); }
    /*!
        This signal is emitted when the player starts or stops playing; \a playing is the new state.
    */
    void playingChanged(bool playing) { emitSignal(&QMediaPlayer::playingChanged, playing); }
    /*!
        This signal is emitted when the playback rate changes to \a rate.
    */
    void playbackRateChanged(qreal rate) { emitSignal(&QMediaPlayer::playbackRateChanged, rate); }
    /*!
        This signal is emitted when the number of loops changes.
    */
    void loopsChanged() { emitSignal(&QMediaPlayer::loopsChanged); }
    /*!
        This signal is emitted when error() or errorString() changes.
    */
    void errorChanged() { emitSignal(&QMediaPlayer::errorChanged); }
    /*!
        This signal is emitted when an \a error occurs, with \a errorString describing it.
    */
    void errorOccurred(QMediaPlayer::Error error, const QString &errorString) {
        emitSignal(&QMediaPlayer::errorOccurred, error, errorString);
    }
    /*!
        This signal is emitted when the audio output changes.
    */
    void audioOutputChanged() { emitSignal(&QMediaPlayer::audioOutputChanged); }
    /*!
        This signal is emitted when the video output changes.
    */
    void videoOutputChanged() { emitSignal(&QMediaPlayer::videoOutputChanged); }

private:
    friend class QMediaPlayerPrivate;
    std::unique_ptr<QMediaPlayerPrivate> d;
};
