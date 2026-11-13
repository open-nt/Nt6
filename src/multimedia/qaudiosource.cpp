// MIT License - see LICENSE
#include <QtMultimedia/qaudiosource.h>
#include <QtMultimedia/qmediadevices.h>

#include "qaudiostream_p.h"

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>

#include <algorithm>
#include <vector>

using QtMultimediaPrivate::AudioStream;

namespace {

// The device push mode returns: reading takes what has been recorded.
class SourcePushDevice : public QIODevice {
public:
    explicit SourcePushDevice(AudioStream *stream) : m_stream(stream) { setOpenMode(ReadOnly | Unbuffered); }
    bool isSequential() const override { return true; }
    qint64 bytesAvailable() const override { return m_stream->availableBytes() + QIODevice::bytesAvailable(); }

protected:
    qint64 readData(char *data, qint64 maxSize) override { return m_stream->read(data, maxSize); }
    qint64 writeData(const char *, qint64) override { return -1; }

private:
    AudioStream *m_stream;
};

constexpr qint64 kDefaultBufferUSecs = 100000;

} // namespace

class QAudioSourcePrivate {
public:
    QAudioSourcePrivate(QAudioSource *owner, const QAudioDevice &dev, const QAudioFormat &fmt)
        : q(owner), device(dev), format(fmt) {
        timer.setTimerType(Qt::PreciseTimer);
        QObject::connect(&timer, &QTimer::timeout, q, [this] { tick(); });
    }

    void setState(QtAudio::State newState, QtAudio::Error newError) {
        error = newError;
        if (state == newState)
            return;
        state = newState;
        emit q->stateChanged(state);
    }

    qint64 effectiveBufferSize() const {
        if (bufferSize > 0)
            return bufferSize;
        return std::max<qint64>(format.bytesForDuration(kDefaultBufferUSecs), format.bytesPerFrame() * 4);
    }

    bool open() {
        if (device.isNull() || !format.isValid()) {
            setState(QtAudio::StoppedState, QtAudio::OpenError);
            return false;
        }
        auto s = std::make_unique<AudioStream>(QAudioDevice::Input, format, effectiveBufferSize());
        s->setVolume(float(volume));
        QString message;
        if (!s->open(device, &message)) {
            setState(QtAudio::StoppedState, QtAudio::OpenError);
            return false;
        }
        stream = std::move(s);
        elapsed.start();
        const qint64 usecs = format.durationForBytes(qint32(stream->bufferBytes()));
        timer.start(int(std::clamp<qint64>(usecs / 4000, 2, 20)));
        return true;
    }

    void close() {
        timer.stop();
        push.reset();
        sink = nullptr;
        if (stream) {
            stream->close(false);
            stream.reset();
        }
    }

    // Hands what was recorded on: to the sink device in pull mode, or by readyRead() in push mode.
    void tick() {
        if (!stream || state == QtAudio::SuspendedState || state == QtAudio::StoppedState)
            return;
        if (stream->failed()) {
            close();
            setState(QtAudio::StoppedState, QtAudio::IOError);
            return;
        }
        const qint64 available = stream->availableBytes();
        if (available <= 0)
            return;
        if (push) {
            emit push->readyRead();
            return;
        }
        if (!sink) {
            close();
            setState(QtAudio::StoppedState, QtAudio::IOError);
            return;
        }
        buffer.resize(std::size_t(available));
        const qint64 n = stream->read(buffer.data(), available);
        if (n > 0 && sink->write(buffer.data(), n) < 0) {
            close();
            setState(QtAudio::StoppedState, QtAudio::IOError);
        }
    }

    QAudioSource *q;
    QAudioDevice device;
    QAudioFormat format;
    qint64 bufferSize = 0;
    qreal volume = 1.0;
    QtAudio::State state = QtAudio::StoppedState;
    QtAudio::Error error = QtAudio::NoError;
    std::unique_ptr<AudioStream> stream;
    std::unique_ptr<SourcePushDevice> push;
    QPointer<QIODevice> sink;
    std::vector<char> buffer;
    QElapsedTimer elapsed;
    QTimer timer;
};

/*!
    \class QAudioSource
    \inmodule NtMultimedia
    \brief The QAudioSource class records raw audio samples from an input device.

    A source records samples in the QAudioFormat it was constructed with. In pull mode, start()
    takes a QIODevice to write them to, such as a QFile or a QBuffer. In push mode, start() returns
    a QIODevice that emits readyRead() as samples arrive:

    \code
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    auto *source = new QAudioSource(format, this);
    QIODevice *mic = source->start();
    connect(mic, &QIODevice::readyRead, this, [mic, this] { process(mic->readAll()); });
    \endcode

    Samples that are not read before the buffer fills are lost.

    \differences
    Samples are handed on from a timer on the source's thread, every quarter of the buffer and at
    most every 20 ms, so that thread needs a running event loop. The state stays ActiveState while
    recording; it never becomes IdleState. The volume is applied to the samples.
    \enddifferences

    \sa QAudioSink, QMediaDevices
*/

/*!
    Constructs a source on the default input device that records samples in \a format, with the
    given \a parent.
*/
QAudioSource::QAudioSource(const QAudioFormat &format, QObject *parent)
    : QAudioSource(QMediaDevices::defaultAudioInput(), format, parent) {}

/*!
    Constructs a source on \a audioDevice that records samples in \a format, with the given
    \a parent.
*/
QAudioSource::QAudioSource(const QAudioDevice &audioDevice, const QAudioFormat &format, QObject *parent)
    : QObject(parent), d(std::make_unique<QAudioSourcePrivate>(this, audioDevice, format)) {}

/*!
    Destroys the source, stopping it.
*/
QAudioSource::~QAudioSource() { d->close(); }

/*!
    Returns \c true if the source has no device to record from; otherwise returns \c false.
*/
bool QAudioSource::isNull() const { return d->device.isNull(); }

/*!
    Returns the format the source records.
*/
QAudioFormat QAudioSource::format() const { return d->format; }

/*!
    Starts recording into \a device, which must be open for writing (pull mode). A source that is
    recording is stopped first. If the input device cannot be opened, the state stays StoppedState
    and error() is OpenError.
*/
void QAudioSource::start(QIODevice *device) {
    d->close();
    d->setState(QtAudio::StoppedState, QtAudio::NoError);
    if (!device || !device->isWritable()) {
        d->setState(QtAudio::StoppedState, QtAudio::OpenError);
        return;
    }
    if (!d->open())
        return;
    d->sink = device;
    d->setState(QtAudio::ActiveState, QtAudio::NoError);
}

/*!
    \overload

    Starts recording in push mode and returns the device to read samples from, or \c nullptr if the
    input device cannot be opened. The source owns the device, which is valid until the source is
    stopped or started again.
*/
QIODevice *QAudioSource::start() {
    d->close();
    d->setState(QtAudio::StoppedState, QtAudio::NoError);
    if (!d->open())
        return nullptr;
    d->push = std::make_unique<SourcePushDevice>(d->stream.get());
    d->setState(QtAudio::ActiveState, QtAudio::NoError);
    return d->push.get();
}

/*!
    Stops recording and closes the device. The state becomes StoppedState and error() NoError.
*/
void QAudioSource::stop() {
    if (d->state == QtAudio::StoppedState)
        return;
    d->close();
    d->setState(QtAudio::StoppedState, QtAudio::NoError);
}

/*!
    Throws away what has been recorded and not read yet.
*/
void QAudioSource::reset() {
    if (d->stream)
        d->stream->clear();
}

/*!
    Pauses recording. The state becomes SuspendedState.

    \sa resume()
*/
void QAudioSource::suspend() {
    if (d->state != QtAudio::ActiveState && d->state != QtAudio::IdleState)
        return;
    d->stream->setSuspended(true);
    d->setState(QtAudio::SuspendedState, QtAudio::NoError);
}

/*!
    Goes on recording after suspend().
*/
void QAudioSource::resume() {
    if (d->state != QtAudio::SuspendedState)
        return;
    d->stream->setSuspended(false);
    d->setState(QtAudio::ActiveState, QtAudio::NoError);
}

/*!
    Sets the size of the buffer between the device and the application to \a bytes, taking effect
    at the next start(). The default holds 100 ms.
*/
void QAudioSource::setBufferSize(qsizetype bytes) { d->bufferSize = std::max<qsizetype>(bytes, 0); }

/*!
    Returns the size of the buffer in bytes: the one in use while the source runs, otherwise the
    one the next start() will use.
*/
qsizetype QAudioSource::bufferSize() const {
    return d->stream ? qsizetype(d->stream->bufferBytes()) : qsizetype(d->effectiveBufferSize());
}

/*!
    Returns how many recorded bytes are waiting to be read.
*/
qsizetype QAudioSource::bytesAvailable() const { return d->stream ? qsizetype(d->stream->availableBytes()) : 0; }

/*!
    Returns how much audio has been recorded since start(), in microseconds.
*/
qint64 QAudioSource::processedUSecs() const {
    if (!d->stream)
        return 0;
    const qint64 frames = d->stream->processedBytes() / std::max(d->format.bytesPerFrame(), 1);
    return frames * 1000000 / std::max(d->format.sampleRate(), 1);
}

/*!
    Returns the time since start(), in microseconds, including time suspended.
*/
qint64 QAudioSource::elapsedUSecs() const {
    return d->state == QtAudio::StoppedState ? 0 : d->elapsed.nsecsElapsed() / 1000;
}

/*!
    Returns the last error.
*/
QtAudio::Error QAudioSource::error() const { return d->error; }

/*!
    Returns the state of the source.
*/
QtAudio::State QAudioSource::state() const { return d->state; }

/*!
    Sets the gain applied to recorded samples to \a volume, from 0.0 (silent) to 1.0 (as recorded).
*/
void QAudioSource::setVolume(qreal volume) {
    d->volume = std::clamp(volume, 0.0, 1.0);
    if (d->stream)
        d->stream->setVolume(float(d->volume));
}

/*!
    Returns the gain applied to recorded samples.
*/
qreal QAudioSource::volume() const { return d->volume; }

#include "moc_qaudiosource.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
