// MIT License - see LICENSE
#include <QtMultimedia/qaudiooutput.h>
#include <QtMultimedia/qaudiosink.h>
#include <QtMultimedia/qmediaplayer.h>

#include "qwavedecoder_p.h"

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qfile.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>

#include <algorithm>

using QtMultimediaPrivate::WaveInfo;
using QtMultimediaPrivate::WaveReader;

class QMediaPlayerPrivate {
public:
    explicit QMediaPlayerPrivate(QMediaPlayer *owner) : q(owner) {
        timer.setInterval(50);
        QObject::connect(&timer, &QTimer::timeout, q, [this] { tick(); });
    }

    ~QMediaPlayerPrivate() { teardown(); }

    QIODevice *media() const { return file ? static_cast<QIODevice *>(file.get()) : sourceDevice.data(); }

    void setState(QMediaPlayer::PlaybackState s) {
        if (state == s)
            return;
        const bool wasPlaying = state == QMediaPlayer::PlayingState;
        state = s;
        emit q->playbackStateChanged(s);
        if (wasPlaying != (s == QMediaPlayer::PlayingState))
            emit q->playingChanged(s == QMediaPlayer::PlayingState);
    }

    void setStatus(QMediaPlayer::MediaStatus s) {
        if (status == s)
            return;
        status = s;
        emit q->mediaStatusChanged(s);
    }

    void setError(QMediaPlayer::Error e, const QString &message) {
        error = e;
        errorString = message;
        emit q->errorChanged();
        if (e != QMediaPlayer::NoError)
            emit q->errorOccurred(e, message);
    }

    void setPositionValue(qint64 ms) {
        if (ms == position)
            return;
        position = ms;
        emit q->positionChanged(ms);
    }

    // Stops the sink or the clock, throwing away what is queued.
    void teardown() {
        timer.stop();
        if (QAudioSink *s = sink.release()) {
            QObject::disconnect(s, nullptr, q, nullptr);
            s->reset();
            s->deleteLater(); // this may be inside the sink's own signal
        }
        reader.reset();
        clockRunning = false;
    }

    void unload() {
        teardown();
        const bool hadAudio = loaded;
        const qint64 oldDuration = info.durationMs();
        const bool wasSeekable = seekable;
        loaded = false;
        seekable = false;
        info = WaveInfo();
        file.reset();
        sourceDevice = nullptr;
        position = 0;
        setState(QMediaPlayer::StoppedState);
        if (oldDuration != 0)
            emit q->durationChanged(0);
        if (hadAudio) {
            emit q->hasAudioChanged(false);
            emit q->bufferProgressChanged(0.0f);
        }
        if (wasSeekable)
            emit q->seekableChanged(false);
    }

    void load() {
        setStatus(QMediaPlayer::LoadingMedia);
        QIODevice *device = media();
        QString message;
        if (!device || !device->isReadable()) {
            setStatus(QMediaPlayer::InvalidMedia);
            setError(QMediaPlayer::ResourceError, QStringLiteral("the media cannot be read"));
            return;
        }
        if (!QtMultimediaPrivate::parseWave(device, &info, &message)) {
            info = WaveInfo();
            setStatus(QMediaPlayer::InvalidMedia);
            setError(QMediaPlayer::FormatError, message);
            return;
        }
        loaded = true;
        seekable = !device->isSequential();
        if (error != QMediaPlayer::NoError)
            setError(QMediaPlayer::NoError, QString());
        emit q->durationChanged(info.durationMs());
        emit q->hasAudioChanged(true);
        emit q->bufferProgressChanged(1.0f);
        if (seekable)
            emit q->seekableChanged(true);
        setStatus(QMediaPlayer::LoadedMedia);
    }

    qint64 framesToMs(qint64 frames) const { return frames * 1000 / std::max(info.format.sampleRate(), 1); }
    qint64 msToFrames(qint64 ms) const { return ms * info.format.sampleRate() / 1000; }

    // The frames played since playback started at startFrame, counting loops.
    qint64 playedFrames() const {
        if (sink)
            return sink->processedUSecs() * info.format.sampleRate() / 1000000;
        if (clockRunning || clockAccumulated > 0) {
            const qint64 ms = clockAccumulated + (clockRunning ? clock.elapsed() : 0);
            return msToFrames(ms);
        }
        return 0;
    }

    // The frames that the loops still to play hold, from startFrame; -1 for ever.
    qint64 framesToPlay() const {
        if (loops == QMediaPlayer::Infinite)
            return -1;
        return std::max<qint64>(info.frames * loops - startFrame, 0);
    }

    qint64 livePosition() const {
        if (info.frames <= 0)
            return 0;
        const qint64 total = startFrame + playedFrames();
        if (loops != QMediaPlayer::Once)
            return framesToMs(total % info.frames);
        return framesToMs(std::min(total, info.frames));
    }

    bool startPlayback(qint64 ms) {
        teardown();
        startFrame = std::clamp<qint64>(msToFrames(ms), 0, info.frames);
        clockAccumulated = 0;
        if (output) {
            if (seekable && !media()->seek(info.dataOffset))
                return false;
            reader = std::make_unique<WaveReader>(media(), info);
            reader->seekFrame(startFrame);
            reader->setLoops(loops == QMediaPlayer::Infinite ? -1 : loops);
            sink = std::make_unique<QAudioSink>(output->device(), info.format);
            applyVolume();
            QObject::connect(sink.get(), &QAudioSink::stateChanged, q, [this](QtAudio::State s) { sinkState(s); });
            sink->start(reader.get());
            if (sink->state() == QtAudio::StoppedState) {
                teardown();
                setError(QMediaPlayer::ResourceError, QStringLiteral("the audio device cannot be opened"));
                return false;
            }
        } else {
            clock.start();
            clockRunning = true;
        }
        timer.start();
        return true;
    }

    void applyVolume() {
        if (sink && output)
            sink->setVolume(output->isMuted() ? 0.0 : qreal(output->volume()));
    }

    void sinkState(QtAudio::State s) {
        if (!sink)
            return;
        if (s == QtAudio::IdleState && sink->error() == QtAudio::NoError && reader && reader->atEnd()) {
            // Not from inside the sink's signal, which may come from inside start().
            QTimer::singleShot(0, q, [this, which = QPointer<QAudioSink>(sink.get())] {
                if (which && which == sink.get() && sink->state() == QtAudio::IdleState && reader && reader->atEnd())
                    endOfMedia();
            });
        } else if (s == QtAudio::StoppedState && sink->error() != QtAudio::NoError) {
            teardown();
            setState(QMediaPlayer::StoppedState);
            setError(QMediaPlayer::ResourceError, QStringLiteral("the audio device failed"));
        }
    }

    void endOfMedia() {
        if (QAudioSink *s = sink.release()) {
            QObject::disconnect(s, nullptr, q, nullptr);
            s->stop(); // lets the device play out its last samples
            s->deleteLater();
        }
        teardown();
        setPositionValue(info.durationMs());
        setState(QMediaPlayer::StoppedState);
        setStatus(QMediaPlayer::EndOfMedia);
    }

    void tick() {
        if (state != QMediaPlayer::PlayingState)
            return;
        if (!sink) {
            const qint64 toPlay = framesToPlay();
            if (toPlay >= 0 && playedFrames() >= toPlay) {
                endOfMedia();
                return;
            }
        }
        setPositionValue(livePosition());
    }

    // Restarts playback where it is, after the output changed.
    void restart() {
        if (state == QMediaPlayer::StoppedState)
            return;
        const bool paused = state == QMediaPlayer::PausedState;
        const qint64 at = paused ? position : livePosition();
        if (!startPlayback(at)) {
            setState(QMediaPlayer::StoppedState);
            return;
        }
        if (paused) {
            pausePlayback();
            setPositionValue(at);
        }
    }

    void pausePlayback() {
        if (sink)
            sink->suspend();
        if (clockRunning) {
            clockAccumulated += clock.elapsed();
            clockRunning = false;
        }
        timer.stop();
        // The worker may finish one more write after suspend(), so hold the position from here.
        setPositionValue(livePosition());
    }

    void resumePlayback() {
        if (sink)
            sink->resume();
        else {
            clock.start();
            clockRunning = true;
        }
        timer.start();
    }

    QMediaPlayer *q;
    QUrl source;
    QPointer<QIODevice> sourceDevice;
    std::unique_ptr<QFile> file;
    WaveInfo info;
    bool loaded = false;
    bool seekable = false;
    QPointer<QAudioOutput> output;
    QPointer<QObject> videoOutput;
    QMediaPlayer::PlaybackState state = QMediaPlayer::StoppedState;
    QMediaPlayer::MediaStatus status = QMediaPlayer::NoMedia;
    QMediaPlayer::Error error = QMediaPlayer::NoError;
    QString errorString;
    qint64 position = 0;
    qreal rate = 1.0;
    int loops = QMediaPlayer::Once;
    qint64 startFrame = 0;
    std::unique_ptr<WaveReader> reader;
    std::unique_ptr<QAudioSink> sink;
    QElapsedTimer clock;
    bool clockRunning = false;
    qint64 clockAccumulated = 0;
    QTimer timer;
};

/*!
    \class QMediaPlayer
    \inmodule NtMultimedia
    \brief The QMediaPlayer class plays audio files.

    Set a source and an audio output, then play():

    \snippet multimedia/audioplayer.cpp player

    The player reads the file as it plays, so long files take little memory. position() moves
    while it plays, and positionChanged() reports it about every 50 ms. At the end the playback
    state becomes StoppedState and the media status EndOfMedia, unless setLoops() asked for more.

    Without an audio output the player plays silently: the position still moves in real time.

    \differences
    Only WAV files can be played: 8, 16, 24 or 32-bit integer samples or 32 or 64-bit floating point
    samples, up to 8 channels. Other formats give FormatError. There is no video: setVideoOutput()
    stores the object but nothing is drawn, and hasVideo() is always \c false. The playback rate is
    stored but playback always runs at normal speed. There are no metadata, tracks,
    \c bufferedTimeRange() or \c setActiveAudioTrack(). Local files, resources (\c{qrc:/} or
    \c{:/}) and devices given to setSourceDevice() can be played, not network URLs.
    \enddifferences

    \sa QAudioOutput, QSoundEffect
*/

/*!
    \enum QMediaPlayer::PlaybackState

    \value StoppedState Not playing; play() starts from the position, or from the beginning after
        the end.
    \value PlayingState Playing.
    \value PausedState Paused; play() goes on from where it paused.
*/

/*!
    \enum QMediaPlayer::MediaStatus

    \value NoMedia No source is set.
    \value LoadingMedia The source is being loaded.
    \value LoadedMedia The source is loaded and the player is stopped.
    \value StalledMedia Playback stalled waiting for data. Nt does not report this.
    \value BufferingMedia Data is being buffered while playback goes on. Nt does not report this.
    \value BufferedMedia The source is playing or paused.
    \value EndOfMedia Playback reached the end.
    \value InvalidMedia The source cannot be played; see error().
*/

/*!
    \enum QMediaPlayer::Error

    \value NoError Nothing went wrong.
    \value ResourceError The source or the audio device could not be opened.
    \value FormatError The source is not in a format the player can play.
    \value NetworkError A network error. Nt does not report this.
    \value AccessDeniedError Access to the source was denied. Nt does not report this.
*/

/*!
    \enum QMediaPlayer::Loops

    \value Infinite Play until stop() is called.
    \value Once Play once, the default.
*/

/*!
    Constructs a player with the given \a parent.
*/
QMediaPlayer::QMediaPlayer(QObject *parent) : QObject(parent), d(std::make_unique<QMediaPlayerPrivate>(this)) {}

/*!
    Destroys the player, stopping it.
*/
QMediaPlayer::~QMediaPlayer() = default;

/*!
    Returns the source being played, or an empty URL.
*/
QUrl QMediaPlayer::source() const { return d->source; }

/*!
    Returns the device given to setSourceDevice(), or \c nullptr.
*/
const QIODevice *QMediaPlayer::sourceDevice() const { return d->file ? nullptr : d->sourceDevice.data(); }

/*!
    Returns the audio output, or \c nullptr.
*/
QAudioOutput *QMediaPlayer::audioOutput() const { return d->output.data(); }

/*!
    Plays through \a output, which sets the device and volume. The player does not take ownership.
    With \c nullptr the player plays silently.
*/
void QMediaPlayer::setAudioOutput(QAudioOutput *output) {
    if (output == d->output)
        return;
    if (d->output)
        disconnect(d->output.data(), nullptr, this, nullptr);
    d->output = output;
    if (output) {
        connect(output, &QAudioOutput::volumeChanged, this, [this](float) { d->applyVolume(); });
        connect(output, &QAudioOutput::mutedChanged, this, [this](bool) { d->applyVolume(); });
        connect(output, &QAudioOutput::deviceChanged, this, [this] { d->restart(); });
        connect(output, &QObject::destroyed, this, [this] {
            d->output = nullptr; // the QAudioOutput part is already gone
            d->restart();
        });
    }
    d->restart();
    emit audioOutputChanged();
}

/*!
    Returns the object set with setVideoOutput(), or \c nullptr.
*/
QObject *QMediaPlayer::videoOutput() const { return d->videoOutput.data(); }

/*!
    Stores \a output as the video output. Nt plays no video, so nothing is drawn on it.
*/
void QMediaPlayer::setVideoOutput(QObject *output) {
    if (output == d->videoOutput)
        return;
    d->videoOutput = output;
    emit videoOutputChanged();
}

/*!
    Returns whether the player is stopped, playing or paused.
*/
QMediaPlayer::PlaybackState QMediaPlayer::playbackState() const { return d->state; }

/*!
    Returns the status of the source.
*/
QMediaPlayer::MediaStatus QMediaPlayer::mediaStatus() const { return d->status; }

/*!
    Returns the length of the source in milliseconds, or 0 if none is loaded.
*/
qint64 QMediaPlayer::duration() const { return d->info.durationMs(); }

/*!
    Returns the playback position in milliseconds.
*/
qint64 QMediaPlayer::position() const { return d->state == PlayingState ? d->livePosition() : d->position; }

/*!
    Returns \c true if the source has audio; otherwise returns \c false.
*/
bool QMediaPlayer::hasAudio() const { return d->loaded; }

/*!
    Returns \c false: Nt plays no video.
*/
bool QMediaPlayer::hasVideo() const { return false; }

/*!
    Returns 1.0 when a source is loaded, since the player reads it as it plays, and 0.0 otherwise.
*/
float QMediaPlayer::bufferProgress() const { return d->loaded ? 1.0f : 0.0f; }

/*!
    Returns \c true if setPosition() can move in the source; otherwise returns \c false.
*/
bool QMediaPlayer::isSeekable() const { return d->seekable; }

/*!
    Returns \c true while the player plays; otherwise returns \c false.
*/
bool QMediaPlayer::isPlaying() const { return d->state == PlayingState; }

/*!
    Returns the playback rate.
*/
qreal QMediaPlayer::playbackRate() const { return d->rate; }

/*!
    Returns how many times the source plays, or Infinite.
*/
int QMediaPlayer::loops() const { return d->loops; }

/*!
    Sets how many times the source plays to \a loops, or Infinite to play until stop(). 0 is
    ignored. It takes effect at the next play().
*/
void QMediaPlayer::setLoops(int loops) {
    if (loops == 0 || loops < Infinite || loops == d->loops)
        return;
    d->loops = loops;
    emit loopsChanged();
}

/*!
    Returns the last error.
*/
QMediaPlayer::Error QMediaPlayer::error() const { return d->error; }

/*!
    Returns a description of the last error, or an empty string.
*/
QString QMediaPlayer::errorString() const { return d->errorString; }

/*!
    Returns \c true: the player is always available.
*/
bool QMediaPlayer::isAvailable() const { return true; }

/*!
    Starts or resumes playing. After the end, playing starts again from the beginning.
*/
void QMediaPlayer::play() {
    if (!d->loaded || d->state == PlayingState)
        return;
    if (d->state == PausedState) {
        d->resumePlayback();
        d->setState(PlayingState);
        return;
    }
    qint64 from = d->position;
    if (d->status == EndOfMedia || from >= duration())
        from = 0;
    if (!d->startPlayback(from))
        return;
    d->setPositionValue(from);
    d->setStatus(BufferedMedia);
    d->setState(PlayingState);
}

/*!
    Pauses playing. A stopped player becomes paused at its position.
*/
void QMediaPlayer::pause() {
    if (!d->loaded || d->state == PausedState)
        return;
    qint64 at = -1;
    if (d->state == StoppedState) {
        at = d->status == EndOfMedia ? 0 : d->position;
        if (!d->startPlayback(at))
            return;
        d->setStatus(BufferedMedia);
    }
    d->pausePlayback();
    if (at >= 0)
        d->setPositionValue(at);
    d->setState(PausedState);
}

/*!
    Stops playing and goes back to the beginning.
*/
void QMediaPlayer::stop() {
    if (d->state == StoppedState && d->position == 0)
        return;
    d->teardown();
    d->setPositionValue(0);
    d->setState(StoppedState);
    if (d->loaded)
        d->setStatus(LoadedMedia);
}

/*!
    Moves the playback position to \a position milliseconds, if the source is seekable.
*/
void QMediaPlayer::setPosition(qint64 position) {
    if (!d->loaded || !d->seekable)
        return;
    position = std::clamp<qint64>(position, 0, duration());
    if (d->state == StoppedState) {
        if (d->status == EndOfMedia)
            d->setStatus(LoadedMedia);
        d->setPositionValue(position);
        return;
    }
    const bool paused = d->state == PausedState;
    if (!d->startPlayback(position)) {
        d->setState(StoppedState);
        return;
    }
    if (paused)
        d->pausePlayback();
    d->setPositionValue(position);
}

/*!
    Sets the playback rate to \a rate. Nt stores it, but always plays at normal speed.
*/
void QMediaPlayer::setPlaybackRate(qreal rate) {
    if (rate == d->rate)
        return;
    d->rate = rate;
    emit playbackRateChanged(rate);
}

/*!
    Sets the source to the WAV file at \a source and loads it, stopping what is playing. An empty
    URL unloads the source.

    \sa mediaStatus(), error()
*/
void QMediaPlayer::setSource(const QUrl &source) {
    d->unload();
    d->source = source;
    emit sourceChanged(source);
    if (source.isEmpty()) {
        d->setStatus(NoMedia);
        return;
    }
    QString fileName;
    if (source.isLocalFile())
        fileName = source.toLocalFile();
    else if (source.scheme() == QLatin1String("qrc"))
        fileName = QLatin1Char(':') + source.path();
    else if (source.scheme().isEmpty())
        fileName = source.path();
    if (fileName.isEmpty()) {
        d->setStatus(InvalidMedia);
        d->setError(ResourceError, QStringLiteral("only local files and resources can be played"));
        return;
    }
    d->file = std::make_unique<QFile>(fileName);
    if (!d->file->open(QIODevice::ReadOnly)) {
        d->file.reset();
        d->setStatus(InvalidMedia);
        d->setError(ResourceError, QStringLiteral("cannot open %1").arg(fileName));
        return;
    }
    d->load();
}

/*!
    Plays the WAV data that \a device reads, stopping what is playing. \a sourceUrl names it for
    source(). The device must be open for reading and stay valid while the player uses it; it can
    be seeked only if it is not sequential.
*/
void QMediaPlayer::setSourceDevice(QIODevice *device, const QUrl &sourceUrl) {
    d->unload();
    d->source = sourceUrl;
    d->sourceDevice = device;
    emit sourceChanged(sourceUrl);
    if (!device) {
        d->setStatus(NoMedia);
        return;
    }
    d->load();
}

#include "moc_qmediaplayer.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
