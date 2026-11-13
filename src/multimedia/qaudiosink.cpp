// MIT License - see LICENSE
#include <QtMultimedia/qaudiosink.h>
#include <QtMultimedia/qmediadevices.h>

#include "qaudiostream_p.h"

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>

#include <algorithm>
#include <functional>
#include <vector>

using QtMultimediaPrivate::AudioStream;

namespace {

// The device push mode returns: what is written goes into the ring.
class SinkPushDevice : public QIODevice {
public:
    SinkPushDevice(AudioStream *stream, std::function<void()> wrote) : m_stream(stream), m_wrote(std::move(wrote)) {
        setOpenMode(WriteOnly | Unbuffered);
    }
    bool isSequential() const override { return true; }

protected:
    qint64 readData(char *, qint64) override { return -1; }
    qint64 writeData(const char *data, qint64 maxSize) override {
        const qint64 n = m_stream->write(data, maxSize);
        if (n > 0)
            m_wrote();
        return n;
    }

private:
    AudioStream *m_stream;
    std::function<void()> m_wrote;
};

constexpr qint64 kDefaultBufferUSecs = 100000;

} // namespace

class QAudioSinkPrivate {
public:
    QAudioSinkPrivate(QAudioSink *owner, const QAudioDevice &dev, const QAudioFormat &fmt)
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
        auto s = std::make_unique<AudioStream>(QAudioDevice::Output, format, effectiveBufferSize());
        QString message;
        s->setVolume(float(volume));
        if (!s->open(device, &message)) {
            setState(QtAudio::StoppedState, QtAudio::OpenError);
            return false;
        }
        stream = std::move(s);
        pending.clear();
        elapsed.start();
        // Wake about four times a buffer, so it never runs dry between ticks.
        const qint64 usecs = format.durationForBytes(qint32(stream->bufferBytes()));
        timer.start(int(std::clamp<qint64>(usecs / 4000, 2, 20)));
        return true;
    }

    void close(bool drain) {
        timer.stop();
        push.reset();
        source = nullptr;
        if (stream) {
            stream->close(drain);
            stream.reset();
        }
    }

    // Pull mode: fills the ring from the source; both modes: notices running dry.
    void tick() {
        if (!stream || state == QtAudio::SuspendedState || state == QtAudio::StoppedState)
            return;
        if (stream->failed()) {
            close(false);
            setState(QtAudio::StoppedState, QtAudio::IOError);
            return;
        }
        bool fed = false;
        bool atEnd = false;
        if (pull) {
            if (!source) {
                close(false);
                setState(QtAudio::StoppedState, QtAudio::IOError);
                return;
            }
            const qint64 room = stream->freeBytes();
            if (room > 0) {
                const auto have = qint64(pending.size());
                if (have < room) {
                    pending.resize(std::size_t(room));
                    const qint64 n = source->read(pending.data() + have, room - have);
                    pending.resize(std::size_t(have + std::max<qint64>(n, 0)));
                }
                const qint64 taken = stream->write(pending.data(), qint64(pending.size()));
                pending.erase(pending.begin(), pending.begin() + taken);
                fed = taken > 0;
            }
            atEnd = source->atEnd() && pending.size() < std::size_t(format.bytesPerFrame());
        }
        if (fed && state == QtAudio::IdleState)
            setState(QtAudio::ActiveState, QtAudio::NoError);
        else if (!fed && state == QtAudio::ActiveState && stream->starved())
            setState(QtAudio::IdleState, atEnd ? QtAudio::NoError : QtAudio::UnderrunError);
    }

    QAudioSink *q;
    QAudioDevice device;
    QAudioFormat format;
    qint64 bufferSize = 0;
    qreal volume = 1.0;
    QtAudio::State state = QtAudio::StoppedState;
    QtAudio::Error error = QtAudio::NoError;
    std::unique_ptr<AudioStream> stream;
    std::unique_ptr<SinkPushDevice> push;
    QPointer<QIODevice> source;
    bool pull = false;
    std::vector<char> pending; // read from the source, not yet taken by the ring
    QElapsedTimer elapsed;
    QTimer timer;
};

/*!
    \class QAudioSink
    \inmodule NtMultimedia
    \brief The QAudioSink class plays raw audio samples on an output device.

    A sink plays samples in the QAudioFormat it was constructed with. In pull mode, start() takes a
    QIODevice to read them from, such as a QFile or a QBuffer, and reads as the device needs more:

    \snippet multimedia/audiodevices.cpp tone

    In push mode, start() returns a QIODevice to write samples to; bytesFree() is how much fits now.

    The state is ActiveState while samples flow and IdleState when there are none left to play:
    with error() NoError at the end of a pull-mode source, and UnderrunError when samples did not
    come fast enough. stop() lets what has been queued finish playing; reset() throws it away.

    \differences
    The sink is fed from a timer on its thread, every quarter of the buffer and at most every 20 ms,
    so that thread needs a running event loop. The volume is applied to the samples. stop() returns
    at once, and the device plays out its last few milliseconds on a thread of its own.
    \enddifferences

    \sa QAudioSource, QMediaDevices
*/

/*!
    Constructs a sink on the default output device that plays samples in \a format, with the given
    \a parent.
*/
QAudioSink::QAudioSink(const QAudioFormat &format, QObject *parent)
    : QAudioSink(QMediaDevices::defaultAudioOutput(), format, parent) {}

/*!
    Constructs a sink on \a audioDevice that plays samples in \a format, with the given \a parent.
*/
QAudioSink::QAudioSink(const QAudioDevice &audioDevice, const QAudioFormat &format, QObject *parent)
    : QObject(parent), d(std::make_unique<QAudioSinkPrivate>(this, audioDevice, format)) {}

/*!
    Destroys the sink, stopping it.
*/
QAudioSink::~QAudioSink() { d->close(false); }

/*!
    Returns \c true if the sink has no device to play on; otherwise returns \c false.
*/
bool QAudioSink::isNull() const { return d->device.isNull(); }

/*!
    Returns the format the sink plays.
*/
QAudioFormat QAudioSink::format() const { return d->format; }

/*!
    Starts playing samples read from \a device, which must be open for reading (pull mode). A sink
    that is playing is stopped first. If the output device cannot be opened, the state stays
    StoppedState and error() is OpenError.
*/
void QAudioSink::start(QIODevice *device) {
    d->close(false);
    d->setState(QtAudio::StoppedState, QtAudio::NoError);
    if (!device || !device->isReadable()) {
        d->setState(QtAudio::StoppedState, QtAudio::OpenError);
        return;
    }
    d->pull = true;
    if (!d->open())
        return;
    d->source = device;
    d->setState(QtAudio::ActiveState, QtAudio::NoError);
    d->tick();
}

/*!
    \overload

    Starts the sink in push mode and returns the device to write samples to, or \c nullptr if the
    output device cannot be opened. The sink owns the device, which is valid until the sink is
    stopped or started again. A write takes what fits, in whole frames.

    \sa bytesFree()
*/
QIODevice *QAudioSink::start() {
    d->close(false);
    d->setState(QtAudio::StoppedState, QtAudio::NoError);
    d->pull = false;
    if (!d->open())
        return nullptr;
    d->push = std::make_unique<SinkPushDevice>(d->stream.get(), [this] {
        if (d->state == QtAudio::IdleState)
            d->setState(QtAudio::ActiveState, QtAudio::NoError);
    });
    d->setState(QtAudio::IdleState, QtAudio::NoError);
    return d->push.get();
}

/*!
    Stops the sink and closes the device. What was queued still plays out. The state becomes
    StoppedState and error() NoError.

    \sa reset()
*/
void QAudioSink::stop() {
    if (d->state == QtAudio::StoppedState)
        return;
    d->close(true);
    d->setState(QtAudio::StoppedState, QtAudio::NoError);
}

/*!
    Stops the sink at once, throwing away what was queued.

    \sa stop()
*/
void QAudioSink::reset() {
    if (d->state == QtAudio::StoppedState)
        return;
    d->close(false);
    d->setState(QtAudio::StoppedState, QtAudio::NoError);
}

/*!
    Pauses playing, keeping what is queued. The state becomes SuspendedState.

    \sa resume()
*/
void QAudioSink::suspend() {
    if (d->state != QtAudio::ActiveState && d->state != QtAudio::IdleState)
        return;
    d->stream->setSuspended(true);
    d->setState(QtAudio::SuspendedState, QtAudio::NoError);
}

/*!
    Goes on playing after suspend().
*/
void QAudioSink::resume() {
    if (d->state != QtAudio::SuspendedState)
        return;
    d->stream->setSuspended(false);
    d->setState(QtAudio::ActiveState, QtAudio::NoError);
    d->tick();
}

/*!
    Sets the size of the buffer between the application and the device to \a bytes, taking effect
    at the next start(). Larger buffers survive longer stalls of the event loop but add latency.
    The default holds 100 ms.

    \sa bufferSize()
*/
void QAudioSink::setBufferSize(qsizetype bytes) { d->bufferSize = std::max<qsizetype>(bytes, 0); }

/*!
    Returns the size of the buffer in bytes: the one in use while the sink runs, otherwise the one
    the next start() will use.
*/
qsizetype QAudioSink::bufferSize() const {
    return d->stream ? qsizetype(d->stream->bufferBytes()) : qsizetype(d->effectiveBufferSize());
}

/*!
    Returns how many bytes can be written now, in push mode, or 0 when the sink is stopped.
*/
qsizetype QAudioSink::bytesFree() const { return d->stream ? qsizetype(d->stream->freeBytes()) : 0; }

/*!
    Returns how much audio has been handed to the device since start(), in microseconds.

    \sa elapsedUSecs()
*/
qint64 QAudioSink::processedUSecs() const {
    if (!d->stream)
        return 0;
    const qint64 frames = d->stream->processedBytes() / std::max(d->format.bytesPerFrame(), 1);
    return frames * 1000000 / std::max(d->format.sampleRate(), 1);
}

/*!
    Returns the time since start(), in microseconds, including time suspended or idle.
*/
qint64 QAudioSink::elapsedUSecs() const {
    return d->state == QtAudio::StoppedState ? 0 : d->elapsed.nsecsElapsed() / 1000;
}

/*!
    Returns the last error.
*/
QtAudio::Error QAudioSink::error() const { return d->error; }

/*!
    Returns the state of the sink.
*/
QtAudio::State QAudioSink::state() const { return d->state; }

/*!
    Sets the volume to \a volume, on the linear scale from 0.0 (silent) to 1.0 (as recorded).

    \sa QtAudio::convertVolume()
*/
void QAudioSink::setVolume(qreal volume) {
    d->volume = std::clamp(volume, 0.0, 1.0);
    if (d->stream)
        d->stream->setVolume(float(d->volume));
}

/*!
    Returns the volume, from 0.0 to 1.0.
*/
qreal QAudioSink::volume() const { return d->volume; }

#include "moc_qaudiosink.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
