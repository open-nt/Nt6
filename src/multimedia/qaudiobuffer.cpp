// MIT License - see LICENSE
#include <QtMultimedia/qaudiobuffer.h>

class QAudioBufferPrivate : public QSharedData {
public:
    QByteArray data;
    QAudioFormat format;
    qint64 startTime = -1;
};

/*!
    \class QAudioBuffer
    \inmodule NtMultimedia
    \brief The QAudioBuffer class holds a block of audio samples and their format.

    QAudioDecoder delivers decoded audio in buffers. Read the samples as frames of the buffer's
    format:

    \snippet multimedia/decoder.cpp buffer

    Copies share their samples; data() makes a copy of its own first.

    \differences
    QAudioFrame has \c channels, \c value(), \c setValue(), \c average() and \c clear(), but not the
    \c left() and \c right() shortcuts.
    \enddifferences

    \sa QAudioDecoder, QAudioFormat
*/

/*!
    \class QAudioFrame
    \inmodule NtMultimedia
    \brief The QAudioFrame class template is one frame of \c N channels of samples of type \c T.

    QAudioBuffer names the common ones: \c U8M, \c S16M, \c S32M and \c F32M for mono, and \c U8S,
    \c S16S, \c S32S and \c F32S for stereo.

    \differences
    There are no \c left() and \c right() shortcuts and no \c positionToIndex().
    \enddifferences
*/

/*!
    \variable QAudioFrame::NChannels

    The number of channels in the frame.
*/

/*!
    \variable QAudioFrame::channels

    The samples of the frame, one for each channel.
*/

/*!
    \fn template <typename T, int N> T QAudioFrame<T, N>::value(QAudioFormat::AudioChannelPosition position) const

    Returns the sample of the channel at \a position, or 0 if the frame has none there.
*/

/*!
    \fn template <typename T, int N> void QAudioFrame<T, N>::setValue(QAudioFormat::AudioChannelPosition position, T value)

    Sets the sample of the channel at \a position to \a value.
*/

/*!
    \fn template <typename T, int N> T QAudioFrame<T, N>::average() const

    Returns the average of the samples of all channels.
*/

/*!
    \fn template <typename T, int N> void QAudioFrame<T, N>::clear()

    Sets every sample to silence.
*/

/*!
    \typedef QAudioBuffer::U8M

    A frame of mono, unsigned 8-bit samples.
*/

/*!
    \typedef QAudioBuffer::S16M

    A frame of mono, signed 16-bit samples.
*/

/*!
    \typedef QAudioBuffer::S32M

    A frame of mono, signed 32-bit samples.
*/

/*!
    \typedef QAudioBuffer::F32M

    A frame of mono, 32-bit floating-point samples.
*/

/*!
    \typedef QAudioBuffer::U8S

    A frame of stereo, unsigned 8-bit samples.
*/

/*!
    \typedef QAudioBuffer::S16S

    A frame of stereo, signed 16-bit samples.
*/

/*!
    \typedef QAudioBuffer::S32S

    A frame of stereo, signed 32-bit samples.
*/

/*!
    \typedef QAudioBuffer::F32S

    A frame of stereo, 32-bit floating-point samples.
*/

/*!
    Constructs an invalid buffer.
*/
QAudioBuffer::QAudioBuffer() noexcept = default;

/*!
    Constructs a buffer of the samples in \a data, in \a format, that starts \a startTime
    microseconds into the stream (-1 if not known).
*/
QAudioBuffer::QAudioBuffer(const QByteArray &data, const QAudioFormat &format, qint64 startTime) {
    if (!format.isValid())
        return;
    d = QExplicitlySharedDataPointer<QAudioBufferPrivate>(new QAudioBufferPrivate);
    d->format = format;
    d->data = data.left(format.bytesPerFrame() > 0 ? data.size() / format.bytesPerFrame() * format.bytesPerFrame() : 0);
    d->startTime = startTime;
}

/*!
    Constructs a buffer of \a numFrames frames of silence in \a format, that starts \a startTime
    microseconds into the stream.
*/
QAudioBuffer::QAudioBuffer(int numFrames, const QAudioFormat &format, qint64 startTime) {
    if (!format.isValid() || numFrames < 0)
        return;
    d = QExplicitlySharedDataPointer<QAudioBufferPrivate>(new QAudioBufferPrivate);
    d->format = format;
    d->data = QByteArray(qsizetype(numFrames) * format.bytesPerFrame(),
                         format.sampleFormat() == QAudioFormat::UInt8 ? char(0x80) : '\0');
    d->startTime = startTime;
}

/*!
    Constructs a copy of \a other, sharing its samples.
*/
QAudioBuffer::QAudioBuffer(const QAudioBuffer &other) noexcept = default;

/*!
    Move-constructs a buffer from \a other, which becomes invalid.
*/
QAudioBuffer::QAudioBuffer(QAudioBuffer &&other) noexcept = default;

/*!
    Destroys the buffer.
*/
QAudioBuffer::~QAudioBuffer() = default;

/*!
    Makes this buffer share the samples of \a other and returns a reference to it.
*/
QAudioBuffer &QAudioBuffer::operator=(const QAudioBuffer &other) = default;

/*!
    Move-assigns \a other to this buffer and returns a reference to it.
*/
QAudioBuffer &QAudioBuffer::operator=(QAudioBuffer &&other) noexcept = default;

/*!
    \fn void QAudioBuffer::swap(QAudioBuffer &other)

    Swaps this buffer with \a other.
*/

/*!
    Returns \c true if the buffer has a valid format; otherwise returns \c false.
*/
bool QAudioBuffer::isValid() const noexcept { return d; }

/*!
    Gives the buffer samples of its own if it shares them.
*/
void QAudioBuffer::detach() {
    if (d)
        d.detach();
}

/*!
    Returns the format of the samples.
*/
QAudioFormat QAudioBuffer::format() const noexcept { return d ? d->format : QAudioFormat(); }

/*!
    Returns the number of frames.
*/
qsizetype QAudioBuffer::frameCount() const noexcept {
    return d && d->format.bytesPerFrame() > 0 ? d->data.size() / d->format.bytesPerFrame() : 0;
}

/*!
    Returns the number of samples: frames times channels.
*/
qsizetype QAudioBuffer::sampleCount() const noexcept { return d ? frameCount() * d->format.channelCount() : 0; }

/*!
    Returns the size of the samples in bytes.
*/
qsizetype QAudioBuffer::byteCount() const noexcept { return d ? d->data.size() : 0; }

/*!
    Returns how long the buffer plays, in microseconds.
*/
qint64 QAudioBuffer::duration() const noexcept { return d ? d->format.durationForFrames(qint32(frameCount())) : 0; }

/*!
    Returns when the buffer starts in the stream, in microseconds, or -1 if not known.
*/
qint64 QAudioBuffer::startTime() const noexcept { return d ? d->startTime : -1; }

/*!
    \fn template <typename T> const T *QAudioBuffer::constData() const

    Returns the samples as an array of \c T, such as \c qint16 or QAudioBuffer::S16S.
*/

/*!
    \fn template <typename T> const T *QAudioBuffer::data() const

    \overload

    Returns the samples as an array of \c T.
*/

/*!
    \fn template <typename T> T *QAudioBuffer::data()

    Returns the samples as a modifiable array of \c T, giving the buffer samples of its own first.
*/

const void *QAudioBuffer::constDataRaw() const noexcept { return d ? d->data.constData() : nullptr; }

void *QAudioBuffer::dataRaw() {
    if (!d)
        return nullptr;
    d.detach();
    return d->data.data();
}
