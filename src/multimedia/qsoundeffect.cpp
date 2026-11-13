// MIT License - see LICENSE
#include <QtMultimedia/qaudiosink.h>
#include <QtMultimedia/qmediadevices.h>
#include <QtMultimedia/qsoundeffect.h>

#include "qwavedecoder_p.h"

#include <QtCore/qbuffer.h>
#include <QtCore/qdebug.h>
#include <QtCore/qfile.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>

#include <algorithm>

using QtMultimediaPrivate::WaveInfo;
using QtMultimediaPrivate::WaveReader;

namespace {

// The file a URL names: a local file, a resource (qrc:/ or :/), or a relative path.
QString fileNameOf(const QUrl &url) {
    if (url.isLocalFile())
        return url.toLocalFile();
    if (url.scheme() == QLatin1String("qrc"))
        return QLatin1Char(':') + url.path();
    if (url.scheme().isEmpty())
        return url.path();
    return QString();
}

} // namespace

class QSoundEffectPrivate {
public:
    QSoundEffectPrivate(QSoundEffect *owner, const QAudioDevice &dev) : q(owner), device(dev) {}

    void setStatus(QSoundEffect::Status s) {
        if (status == s)
            return;
        const bool wasLoaded = status == QSoundEffect::Ready;
        status = s;
        emit q->statusChanged();
        if (wasLoaded != (s == QSoundEffect::Ready))
            emit q->loadedChanged();
    }

    void setPlaying(bool value) {
        if (playing == value)
            return;
        playing = value;
        emit q->playingChanged();
    }

    void setLoopsRemaining(int value) {
        if (loopsRemaining == value)
            return;
        loopsRemaining = value;
        emit q->loopsRemainingChanged();
    }

    bool load(const QUrl &url) {
        const QString fileName = fileNameOf(url);
        if (fileName.isEmpty())
            return false;
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly))
            return false;
        WaveInfo wave;
        QString message;
        if (!QtMultimediaPrivate::parseWave(&file, &wave, &message)) {
            qWarning("QSoundEffect: %s: %s", qPrintable(fileName), qPrintable(message));
            return false;
        }
        // Decode it all now, so that playing never touches the file.
        WaveReader decoder(&file, wave);
        QByteArray samples;
        samples.resize(qsizetype(std::min<qint64>(wave.frames, 0x7FFFFFF) * wave.format.bytesPerFrame()));
        const qint64 n = decoder.read(samples.data(), samples.size());
        samples.resize(qsizetype(std::max<qint64>(n, 0)));
        if (samples.isEmpty())
            return false;
        data = samples;
        format = wave.format;
        info = wave;
        info.dataOffset = 0;
        info.sourceBytesPerSample = format.bytesPerSample();
        info.sourceFloat = format.sampleFormat() == QAudioFormat::Float;
        info.frames = data.size() / std::max(format.bytesPerFrame(), 1);
        return true;
    }

    void stopSink() {
        sink.reset();
        reader.reset();
        buffer.reset();
    }

    // The sink may be inside one of its own functions, so it is deleted later.
    void finished() {
        if (QAudioSink *s = sink.release()) {
            QObject::disconnect(s, nullptr, q, nullptr);
            s->stop(); // lets the device play out its last samples
            s->deleteLater();
        }
        stopSink();
        setLoopsRemaining(0);
        setPlaying(false);
    }

    float effectiveVolume() const { return muted ? 0.0f : volume; }

    QSoundEffect *q;
    QAudioDevice device;
    QUrl source;
    int loopCount = 1;
    int loopsRemaining = 0;
    float volume = 1.0f;
    bool muted = false;
    bool playing = false;
    QSoundEffect::Status status = QSoundEffect::Null;
    QByteArray data;
    QAudioFormat format;
    WaveInfo info;
    std::unique_ptr<QBuffer> buffer;
    std::unique_ptr<WaveReader> reader;
    std::unique_ptr<QAudioSink> sink;
};

/*!
    \class QSoundEffect
    \inmodule NtMultimedia
    \brief The QSoundEffect class plays a short sound with low latency.

    A sound effect loads a WAV file when its source is set and keeps the samples in memory, so
    play() starts at once. It suits interface sounds and game effects; use QMediaPlayer for music
    and long recordings.

    \snippet multimedia/audioplayer.cpp effect

    WAV files with 8, 16, 24 or 32-bit integer samples or 32 or 64-bit floating point samples, and
    up to 8 channels, can be played.

    \differences
    The source is loaded synchronously by setSource(), so the status goes from Loading to Ready, or
    Error, before it returns. Only local files and resources (\c{qrc:/} or \c{:/}) can be loaded,
    not network URLs. play() while playing starts again from the beginning.
    \enddifferences

    \sa QMediaPlayer, QAudioSink
*/

/*!
    \enum QSoundEffect::Loop

    \value Infinite Play the sound until stop() is called.
*/

/*!
    \enum QSoundEffect::Status

    \value Null No source is set.
    \value Loading The source is being loaded.
    \value Ready The source is loaded and can be played.
    \value Error The source could not be loaded.
*/

/*!
    Constructs a sound effect that plays on the default output device, with the given \a parent.
*/
QSoundEffect::QSoundEffect(QObject *parent) : QSoundEffect(QMediaDevices::defaultAudioOutput(), parent) {}

/*!
    Constructs a sound effect that plays on \a audioDevice, with the given \a parent.
*/
QSoundEffect::QSoundEffect(const QAudioDevice &audioDevice, QObject *parent)
    : QObject(parent), d(std::make_unique<QSoundEffectPrivate>(this, audioDevice)) {}

/*!
    Destroys the sound effect, stopping it.
*/
QSoundEffect::~QSoundEffect() { d->stopSink(); }

/*!
    Returns the MIME types of the files a sound effect can play.
*/
QStringList QSoundEffect::supportedMimeTypes() {
    return {QStringLiteral("audio/x-wav"), QStringLiteral("audio/wav"), QStringLiteral("audio/wave"),
            QStringLiteral("audio/x-pn-wav")};
}

/*!
    Returns the URL of the sound.
*/
QUrl QSoundEffect::source() const { return d->source; }

/*!
    Sets the sound to the WAV file at \a url and loads it, stopping what is playing. An empty URL
    unloads the sound.

    \sa status()
*/
void QSoundEffect::setSource(const QUrl &url) {
    if (url == d->source)
        return;
    stop();
    d->source = url;
    d->data.clear();
    emit sourceChanged();
    if (url.isEmpty()) {
        d->setStatus(Null);
        return;
    }
    d->setStatus(Loading);
    d->setStatus(d->load(url) ? Ready : Error);
}

/*!
    Returns how many times play() plays the sound, or Infinite.
*/
int QSoundEffect::loopCount() const { return d->loopCount; }

/*!
    Returns how many more times the sound plays, counting the current time, or Infinite. It is 0
    when the effect is not playing.
*/
int QSoundEffect::loopsRemaining() const { return d->loopsRemaining; }

/*!
    Sets how many times play() plays the sound to \a loopCount, or Infinite to play it until
    stop(). 0 means 1. While playing, the count starts again from \a loopCount.
*/
void QSoundEffect::setLoopCount(int loopCount) {
    if (loopCount < 0 && loopCount != Infinite) {
        qWarning("QSoundEffect::setLoopCount: loops should be Infinite or a positive number");
        return;
    }
    if (loopCount == 0)
        loopCount = 1;
    if (loopCount == d->loopCount)
        return;
    d->loopCount = loopCount;
    emit loopCountChanged();
    if (d->playing && d->reader) {
        d->reader->setLoops(loopCount == Infinite ? -1 : loopCount);
        d->setLoopsRemaining(loopCount);
    }
}

/*!
    Returns the device the sound plays on.
*/
QAudioDevice QSoundEffect::audioDevice() { return d->device; }

/*!
    Sets the device to play on to \a device, from the next play().
*/
void QSoundEffect::setAudioDevice(const QAudioDevice &device) {
    if (device == d->device)
        return;
    d->device = device;
    emit audioDeviceChanged();
}

/*!
    Returns the volume, from 0.0 to 1.0.
*/
float QSoundEffect::volume() const { return d->volume; }

/*!
    Sets the volume to \a volume, on the linear scale from 0.0 (silent) to 1.0 (as recorded).
*/
void QSoundEffect::setVolume(float volume) {
    volume = std::clamp(volume, 0.0f, 1.0f);
    if (volume == d->volume)
        return;
    d->volume = volume;
    if (d->sink)
        d->sink->setVolume(qreal(d->effectiveVolume()));
    emit volumeChanged();
}

/*!
    Returns \c true if the effect is muted; otherwise returns \c false.
*/
bool QSoundEffect::isMuted() const { return d->muted; }

/*!
    Mutes the effect if \a muted is \c true, or unmutes it.
*/
void QSoundEffect::setMuted(bool muted) {
    if (muted == d->muted)
        return;
    d->muted = muted;
    if (d->sink)
        d->sink->setVolume(qreal(d->effectiveVolume()));
    emit mutedChanged();
}

/*!
    Returns \c true if the sound is loaded and can be played; otherwise returns \c false.
*/
bool QSoundEffect::isLoaded() const { return d->status == Ready; }

/*!
    Returns \c true while the sound plays; otherwise returns \c false.
*/
bool QSoundEffect::isPlaying() const { return d->playing; }

/*!
    Returns whether the sound is loaded.
*/
QSoundEffect::Status QSoundEffect::status() const { return d->status; }

/*!
    Plays the sound loopCount() times. A sound that is playing starts again from the beginning.
    Nothing happens if the sound is not loaded or there is no output device.
*/
void QSoundEffect::play() {
    if (d->status != Ready)
        return;
    d->stopSink();
    d->buffer = std::make_unique<QBuffer>(&d->data);
    d->buffer->open(QIODevice::ReadOnly);
    d->reader = std::make_unique<WaveReader>(d->buffer.get(), d->info);
    d->reader->setLoops(d->loopCount == Infinite ? -1 : d->loopCount);
    d->reader->setLoopCallback([this] {
        if (d->loopsRemaining != Infinite && d->loopsRemaining > 1)
            d->setLoopsRemaining(d->loopsRemaining - 1);
    });
    d->sink = std::make_unique<QAudioSink>(d->device, d->format);
    d->sink->setVolume(qreal(d->effectiveVolume()));
    QAudioSink *sink = d->sink.get();
    connect(sink, &QAudioSink::stateChanged, this, [this, sink](QtAudio::State state) {
        const bool ended = state == QtAudio::IdleState && sink->error() == QtAudio::NoError;
        const bool failed = state == QtAudio::StoppedState && sink->error() != QtAudio::NoError;
        if (!ended && !failed)
            return;
        // Not from inside the sink's signal, which may come from inside start().
        QTimer::singleShot(0, this, [this, which = QPointer<QAudioSink>(sink)] {
            if (which && which == d->sink.get() &&
                (d->sink->state() == QtAudio::IdleState || d->sink->state() == QtAudio::StoppedState))
                d->finished();
        });
    });
    d->setLoopsRemaining(d->loopCount);
    d->sink->start(d->reader.get());
    if (d->sink->state() == QtAudio::StoppedState) {
        d->stopSink();
        d->setLoopsRemaining(0);
        return;
    }
    d->setPlaying(true);
}

/*!
    Stops the sound at once.
*/
void QSoundEffect::stop() {
    if (!d->playing && !d->sink)
        return;
    d->stopSink();
    d->setLoopsRemaining(0);
    d->setPlaying(false);
}

#include "moc_qsoundeffect.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
