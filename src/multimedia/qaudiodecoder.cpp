// MIT License - see LICENSE
#include <QtMultimedia/qaudiodecoder.h>

#include "qwavedecoder_p.h"

#include <QtCore/qfile.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <vector>

using QtMultimediaPrivate::WaveInfo;
using QtMultimediaPrivate::WaveReader;

namespace {

constexpr int kFramesPerBuffer = 4096;
constexpr std::size_t kQueuedBuffers = 4;

// Converts blocks of frames from one format to another: the sample format, the channels (mono is
// spread to every channel, and everything is averaged down to mono) and, by linear interpolation,
// the sample rate. The last frame of each block is carried over, so blocks join without a seam.
class Converter {
public:
    void reset(const QAudioFormat &from, const QAudioFormat &to) {
        m_from = from;
        m_to = to;
        m_carry.clear();
        m_t = 0.0;
    }

    QByteArray convert(const QByteArray &in) {
        if (m_from == m_to)
            return in;
        const int inCh = m_from.channelCount();
        const int ch = m_to.channelCount();
        if (inCh <= 0 || ch <= 0 || m_from.bytesPerFrame() <= 0)
            return QByteArray();
        const qsizetype frames = in.size() / m_from.bytesPerFrame();
        // The input as float frames with the output's channels, after the carried frame.
        std::vector<float> f(m_carry);
        f.reserve(m_carry.size() + std::size_t(frames * ch));
        for (qsizetype i = 0; i < frames; ++i) {
            const char *src = in.constData() + i * m_from.bytesPerFrame();
            float mono = 0.0f;
            if (ch == 1 || inCh == 1) {
                for (int c = 0; c < inCh; ++c)
                    mono += m_from.normalizedSampleValue(src + c * m_from.bytesPerSample());
                mono /= float(inCh);
            }
            for (int c = 0; c < ch; ++c)
                f.push_back(ch == 1 || inCh == 1 ? mono
                            : c < inCh           ? m_from.normalizedSampleValue(src + c * m_from.bytesPerSample())
                                                 : 0.0f);
        }
        const auto total = qsizetype(f.size()) / ch;
        QByteArray out;
        if (m_from.sampleRate() == m_to.sampleRate()) {
            out.resize(total * m_to.bytesPerFrame());
            for (qsizetype i = 0; i < total * ch; ++i)
                QtMultimediaPrivate::writeNormalizedSample(m_to, out.data() + i * m_to.bytesPerSample(),
                                                           f[std::size_t(i)]);
            m_carry.clear();
            return out;
        }
        const double step = double(m_from.sampleRate()) / m_to.sampleRate();
        std::vector<char> sample(std::size_t(m_to.bytesPerSample()));
        while (m_t + 1.0 < double(total)) {
            const auto i = qsizetype(m_t);
            const auto frac = float(m_t - double(i));
            for (int c = 0; c < ch; ++c) {
                const float a = f[std::size_t(i * ch + c)];
                const float b = f[std::size_t((i + 1) * ch + c)];
                QtMultimediaPrivate::writeNormalizedSample(m_to, sample.data(), a + (b - a) * frac);
                out.append(sample.data(), qsizetype(sample.size()));
            }
            m_t += step;
        }
        if (total > 0) {
            m_carry.assign(f.end() - ch, f.end());
            m_t -= double(total - 1);
        }
        return out;
    }

private:
    QAudioFormat m_from;
    QAudioFormat m_to;
    std::vector<float> m_carry;
    double m_t = 0.0;
};

} // namespace

class QAudioDecoderPrivate {
public:
    explicit QAudioDecoderPrivate(QAudioDecoder *owner) : q(owner) {
        timer.setInterval(0);
        QObject::connect(&timer, &QTimer::timeout, q, [this] { step(); });
    }

    void setError(QAudioDecoder::Error e, const QString &message) {
        error = e;
        errorString = message;
        emit q->error(e);
        halt();
    }

    void halt() {
        timer.stop();
        reader.reset();
        file.reset();
        if (decoding) {
            decoding = false;
            emit q->isDecodingChanged(false);
        }
    }

    // Decodes the next buffer, while fewer than a few wait to be read.
    void step() {
        if (!reader || queue.size() >= kQueuedBuffers) {
            timer.stop();
            return;
        }
        QByteArray raw(qsizetype(kFramesPerBuffer) * wave.format.bytesPerFrame(), Qt::Uninitialized);
        const qint64 n = reader->read(raw.data(), raw.size());
        if (n <= 0) {
            reader.reset();
            file.reset();
            timer.stop();
            if (queue.empty())
                finish();
            return;
        }
        raw.resize(qsizetype(n));
        const qint64 start = wave.format.durationForFrames(qint32(decodedFrames));
        decodedFrames += n / std::max(wave.format.bytesPerFrame(), 1);
        queue.emplace_back(converter.convert(raw), output, start);
        if (queue.size() == 1)
            emit q->bufferAvailableChanged(true);
        emit q->bufferReady();
    }

    void finish() {
        if (!decoding)
            return;
        decoding = false;
        emit q->isDecodingChanged(false);
        emit q->finished();
    }

    QAudioDecoder *q;
    QUrl source;
    QPointer<QIODevice> sourceDevice;
    std::unique_ptr<QFile> file;
    std::unique_ptr<WaveReader> reader;
    WaveInfo wave;
    QAudioFormat requested;
    QAudioFormat output;
    QAudioDecoder::Error error = QAudioDecoder::NoError;
    QString errorString;
    bool decoding = false;
    qint64 decodedFrames = 0;
    qint64 position = -1;
    qint64 duration = -1;
    std::deque<QAudioBuffer> queue;
    Converter converter;
    QTimer timer;
};

/*!
    \class QAudioDecoder
    \inmodule NtMultimedia
    \brief The QAudioDecoder class decodes an audio file into buffers of samples.

    Set a source and start(); read() each buffer as bufferReady() announces it:

    \snippet multimedia/decoder.cpp decode

    The decoder works from the event loop, keeping a few buffers ahead of what has been read.

    \differences
    Only WAV files can be decoded: 8, 16, 24 or 32-bit integer or 32 or 64-bit floating-point
    samples, up to 8 channels. setAudioFormat() converts the sample rate by linear interpolation,
    with no filtering, so downsampling can alias; mono is spread to every channel, and more channels
    are averaged down to mono.
    \enddifferences

    \sa QAudioBuffer, QMediaFormat
*/

/*!
    \enum QAudioDecoder::Error

    \value NoError Nothing went wrong.
    \value ResourceError The source could not be opened.
    \value FormatError The source is not a WAV file Nt can decode.
    \value AccessDeniedError Access to the source was denied. Nt does not report this.
    \value NotSupportedError Decoding is not supported. Nt does not report this.
*/

/*!
    Constructs a decoder with the given \a parent.
*/
QAudioDecoder::QAudioDecoder(QObject *parent) : QObject(parent), d(std::make_unique<QAudioDecoderPrivate>(this)) {}

/*!
    Destroys the decoder.
*/
QAudioDecoder::~QAudioDecoder() = default;

/*!
    Returns \c true: Nt can always decode WAV files.
*/
bool QAudioDecoder::isSupported() const { return true; }

/*!
    Returns \c true between start() and the end of decoding; otherwise returns \c false.
*/
bool QAudioDecoder::isDecoding() const { return d->decoding; }

/*!
    Returns the file to decode.
*/
QUrl QAudioDecoder::source() const { return d->source; }

/*!
    Sets the file to decode to \a fileName: a local file or a resource (\c{qrc:/} or \c{:/}). It stops
    decoding, and replaces a source device.
*/
void QAudioDecoder::setSource(const QUrl &fileName) {
    stop();
    d->sourceDevice = nullptr;
    d->source = fileName;
    emit sourceChanged();
}

/*!
    Returns the device to decode from, or \c nullptr.
*/
QIODevice *QAudioDecoder::sourceDevice() const { return d->sourceDevice.data(); }

/*!
    Sets the device to decode from to \a device, which must be open for reading. It stops decoding,
    and replaces a source file.
*/
void QAudioDecoder::setSourceDevice(QIODevice *device) {
    stop();
    d->source = QUrl();
    d->sourceDevice = device;
    emit sourceChanged();
}

/*!
    Returns the format the buffers are converted to, or an invalid format to keep the file's format.
*/
QAudioFormat QAudioDecoder::audioFormat() const { return d->requested; }

/*!
    Sets the format to convert the buffers to to \a format, from the next start(). An invalid format
    keeps the file's format.
*/
void QAudioDecoder::setAudioFormat(const QAudioFormat &format) {
    if (format == d->requested)
        return;
    d->requested = format;
    emit formatChanged(format);
}

/*!
    Returns the last error.
*/
QAudioDecoder::Error QAudioDecoder::error() const { return d->error; }

/*!
    Returns a description of the last error, or an empty string.
*/
QString QAudioDecoder::errorString() const { return d->errorString; }

/*!
    Returns the next decoded buffer and removes it, or an invalid buffer if none is ready.
*/
QAudioBuffer QAudioDecoder::read() const {
    if (d->queue.empty())
        return QAudioBuffer();
    QAudioBuffer buffer = d->queue.front();
    d->queue.pop_front();
    const qint64 end = (buffer.startTime() + buffer.duration()) / 1000;
    if (end != d->position) {
        d->position = end;
        emit const_cast<QAudioDecoder *>(this)->positionChanged(end);
    }
    if (d->queue.empty())
        emit const_cast<QAudioDecoder *>(this)->bufferAvailableChanged(false);
    if (d->reader)
        d->timer.start();
    else if (d->queue.empty())
        d->finish();
    return buffer;
}

/*!
    Returns \c true if a decoded buffer is ready to read(); otherwise returns \c false.
*/
bool QAudioDecoder::bufferAvailable() const { return !d->queue.empty(); }

/*!
    Returns the end of the last buffer read, in milliseconds, or -1.
*/
qint64 QAudioDecoder::position() const { return d->position; }

/*!
    Returns the length of the source in milliseconds, or -1 before decoding starts.
*/
qint64 QAudioDecoder::duration() const { return d->duration; }

/*!
    Starts decoding. Errors are reported with the error() signal.
*/
void QAudioDecoder::start() {
    if (d->decoding)
        return;
    d->queue.clear();
    d->error = NoError;
    d->errorString.clear();
    d->decodedFrames = 0;
    d->position = -1;
    d->decoding = true;
    emit isDecodingChanged(true);
    QIODevice *device = d->sourceDevice.data();
    if (!device) {
        QString name;
        if (d->source.isLocalFile())
            name = d->source.toLocalFile();
        else if (d->source.scheme() == QLatin1String("qrc"))
            name = QLatin1Char(':') + d->source.path();
        else if (d->source.scheme().isEmpty())
            name = d->source.path();
        d->file = std::make_unique<QFile>(name);
        if (name.isEmpty() || !d->file->open(QIODevice::ReadOnly)) {
            d->setError(ResourceError, QStringLiteral("cannot open %1").arg(d->source.toString()));
            return;
        }
        device = d->file.get();
    }
    QString message;
    if (!device->isReadable() || !QtMultimediaPrivate::parseWave(device, &d->wave, &message)) {
        d->setError(FormatError, message.isEmpty() ? QStringLiteral("the source cannot be read") : message);
        return;
    }
    d->output = d->requested.isValid() ? d->requested : d->wave.format;
    d->converter.reset(d->wave.format, d->output);
    const qint64 duration = d->wave.durationMs();
    if (duration != d->duration) {
        d->duration = duration;
        emit durationChanged(duration);
    }
    d->reader = std::make_unique<WaveReader>(device, d->wave);
    d->timer.start();
}

/*!
    Stops decoding and throws away buffers that have not been read.
*/
void QAudioDecoder::stop() {
    const bool hadBuffers = !d->queue.empty();
    d->queue.clear();
    d->halt();
    if (hadBuffers)
        emit bufferAvailableChanged(false);
    if (d->position != -1) {
        d->position = -1;
        emit positionChanged(-1);
    }
}

#include "moc_qaudiodecoder.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
