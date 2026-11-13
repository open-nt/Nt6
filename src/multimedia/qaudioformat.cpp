// MIT License - see LICENSE
#include <QtMultimedia/qaudioformat.h>

#include <bitset>
#include <cstring>

/*!
    \class QAudioFormat
    \inmodule NtMultimedia
    \brief The QAudioFormat class describes uncompressed audio: its sample rate, channels and sample format.

    Samples are interleaved: a frame holds one sample of each channel. Set the three parts and check
    isValid():

    \code
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);
    qint32 bytes = format.bytesForDuration(20000); // 20 ms: 3840 bytes
    \endcode

    Samples are in the byte order of the machine.

    \differences
    QAudioFormat is not a Q_GADGET, so its enumerations have no meta-object.
    \enddifferences

    \sa QAudioDevice, QAudioSink, QAudioSource
*/

/*!
    \enum QAudioFormat::AudioChannelPosition

    The position of a speaker, used to build a ChannelConfig.

    \value UnknownPosition Not known.
    \value FrontLeft Front left.
    \value FrontRight Front right.
    \value FrontCenter Front center, or the only channel of mono sound.
    \value LFE Low frequency effects (subwoofer).
    \value BackLeft Back left.
    \value BackRight Back right.
    \value FrontLeftOfCenter Between front left and front center.
    \value FrontRightOfCenter Between front right and front center.
    \value BackCenter Back center.
    \value SideLeft Side left.
    \value SideRight Side right.
    \value TopCenter Above the listener.
    \value TopFrontLeft Top front left.
    \value TopFrontCenter Top front center.
    \value TopFrontRight Top front right.
    \value TopBackLeft Top back left.
    \value TopBackCenter Top back center.
    \value TopBackRight Top back right.
    \value LFE2 A second low frequency effects channel.
    \value TopSideLeft Top side left.
    \value TopSideRight Top side right.
    \value BottomFrontCenter Bottom front center.
    \value BottomFrontLeft Bottom front left.
    \value BottomFrontRight Bottom front right.
*/

/*!
    \variable QAudioFormat::NChannelPositions

    The number of values of AudioChannelPosition.
*/

/*!
    \enum QAudioFormat::ChannelConfig

    A set of speaker positions, one bit (\c{1 << position}) for each AudioChannelPosition present.
    channelConfig() builds other sets.

    \value ChannelConfigUnknown Not known.
    \value ChannelConfigMono One channel, front center.
    \value ChannelConfigStereo Front left and right.
    \value ChannelConfig2Dot1 Stereo and LFE.
    \value ChannelConfig3Dot0 Front left, right and center.
    \value ChannelConfig3Dot1 Front left, right and center, and LFE.
    \value ChannelConfigSurround5Dot0 Front left, right and center, and back left and right.
    \value ChannelConfigSurround5Dot1 5.0 and LFE.
    \value ChannelConfigSurround7Dot0 5.0 and side left and right.
    \value ChannelConfigSurround7Dot1 7.0 and LFE.
*/

/*!
    \enum QAudioFormat::SampleFormat

    How a sample is stored.

    \value Unknown Not set.
    \value UInt8 Unsigned 8-bit integers, silence at 128.
    \value Int16 Signed 16-bit integers.
    \value Int32 Signed 32-bit integers.
    \value Float 32-bit floating point, from -1.0 to 1.0.
    \omitvalue NSampleFormats
*/

/*!
    \fn QAudioFormat::QAudioFormat()

    Constructs an invalid format, with nothing set.
*/

/*!
    \fn bool QAudioFormat::isValid() const

    Returns \c true if the sample rate, channel count and sample format are all set; otherwise
    returns \c false.
*/

/*!
    \fn void QAudioFormat::setSampleRate(int sampleRate)

    Sets the number of frames a second to \a sampleRate.
*/

/*!
    \fn int QAudioFormat::sampleRate() const

    Returns the number of frames a second.
*/

/*!
    \fn QAudioFormat::ChannelConfig QAudioFormat::channelConfig() const

    Returns the speaker positions of the channels, or ChannelConfigUnknown if only a channel count
    was set.

    \sa setChannelConfig(), defaultChannelConfigForChannelCount()
*/

/*!
    \fn void QAudioFormat::setChannelCount(int channelCount)

    Sets the number of channels to \a channelCount, and the channel configuration to unknown.

    \sa setChannelConfig()
*/

/*!
    \fn int QAudioFormat::channelCount() const

    Returns the number of channels.
*/

/*!
    \fn void QAudioFormat::setSampleFormat(QAudioFormat::SampleFormat format)

    Sets how samples are stored to \a format.
*/

/*!
    \fn QAudioFormat::SampleFormat QAudioFormat::sampleFormat() const

    Returns how samples are stored.
*/

/*!
    \fn int QAudioFormat::bytesPerFrame() const

    Returns the size of a frame, one sample of every channel, in bytes.
*/

/*!
    \fn int QAudioFormat::bytesPerSample() const

    Returns the size of one sample in bytes, or 0 if the sample format is not set.
*/

/*!
    \fn template <typename... Args> QAudioFormat::ChannelConfig QAudioFormat::channelConfig(Args... channels)

    Returns the channel configuration with the speakers \a channels, each an AudioChannelPosition:

    \code
    auto quad = QAudioFormat::channelConfig(QAudioFormat::FrontLeft, QAudioFormat::FrontRight,
                                            QAudioFormat::BackLeft, QAudioFormat::BackRight);
    \endcode
*/

/*!
    Sets the speaker positions of the channels to \a config, and the channel count to the number of
    positions in it.
*/
void QAudioFormat::setChannelConfig(ChannelConfig config) noexcept {
    m_config = config;
    if (config != ChannelConfigUnknown)
        m_channelCount = int(std::bitset<32>(config).count());
}

/*!
    Returns the index in a frame of the sample for the speaker \a channel, or -1 if the channel
    configuration does not have it. Channels are in the order of AudioChannelPosition.
*/
int QAudioFormat::channelOffset(AudioChannelPosition channel) const noexcept {
    const quint32 config =
        m_config != ChannelConfigUnknown ? m_config : defaultChannelConfigForChannelCount(m_channelCount);
    const quint32 bit = 1u << quint32(channel);
    if (channel == UnknownPosition || !(config & bit))
        return -1;
    return int(std::bitset<32>(config & (bit - 1)).count());
}

/*!
    Returns the number of bytes, in whole frames, that play for \a microseconds.

    \sa durationForBytes(), framesForDuration()
*/
qint32 QAudioFormat::bytesForDuration(qint64 microseconds) const {
    return bytesPerFrame() * framesForDuration(microseconds);
}

/*!
    Returns how long \a bytes play, in microseconds, counting whole frames only.

    \sa bytesForDuration()
*/
qint64 QAudioFormat::durationForBytes(qint32 bytes) const {
    const int frame = bytesPerFrame();
    if (!isValid() || bytes <= 0 || frame <= 0)
        return 0;
    return durationForFrames(bytes / frame);
}

/*!
    Returns the number of bytes in \a frameCount frames.
*/
qint32 QAudioFormat::bytesForFrames(qint32 frameCount) const { return frameCount * bytesPerFrame(); }

/*!
    Returns the number of whole frames in \a byteCount bytes.
*/
qint32 QAudioFormat::framesForBytes(qint32 byteCount) const {
    const int frame = bytesPerFrame();
    return frame > 0 ? byteCount / frame : 0;
}

/*!
    Returns the number of whole frames that play for \a microseconds.
*/
qint32 QAudioFormat::framesForDuration(qint64 microseconds) const {
    if (!isValid() || microseconds <= 0)
        return 0;
    return qint32(microseconds * sampleRate() / 1000000);
}

/*!
    Returns how long \a frameCount frames play, in microseconds.
*/
qint64 QAudioFormat::durationForFrames(qint32 frameCount) const {
    if (!isValid() || frameCount <= 0)
        return 0;
    return qint64(frameCount) * 1000000 / sampleRate();
}

/*!
    Returns the sample at \a sample, which must point to one sample in this format, as a value from
    -1.0 to 1.0.
*/
float QAudioFormat::normalizedSampleValue(const void *sample) const {
    switch (m_format) {
    case UInt8:
        return (float(*static_cast<const quint8 *>(sample)) - 128.0f) / 128.0f;
    case Int16: {
        qint16 v;
        std::memcpy(&v, sample, sizeof v);
        return float(v) / 32768.0f;
    }
    case Int32: {
        qint32 v;
        std::memcpy(&v, sample, sizeof v);
        return float(double(v) / 2147483648.0);
    }
    case Float: {
        float v;
        std::memcpy(&v, sample, sizeof v);
        return v;
    }
    case Unknown:
    case NSampleFormats:
        break;
    }
    return 0.0f;
}

/*!
    Returns the usual channel configuration for \a channelCount channels: mono, stereo, 2.1 for
    three, 5.0 for five, 5.1 for six, 7.0 for seven and 7.1 for eight, and ChannelConfigUnknown for
    others.
*/
QAudioFormat::ChannelConfig QAudioFormat::defaultChannelConfigForChannelCount(int channelCount) {
    switch (channelCount) {
    case 1:
        return ChannelConfigMono;
    case 2:
        return ChannelConfigStereo;
    case 3:
        return ChannelConfig2Dot1;
    case 4:
        return channelConfig(FrontLeft, FrontRight, BackLeft, BackRight);
    case 5:
        return ChannelConfigSurround5Dot0;
    case 6:
        return ChannelConfigSurround5Dot1;
    case 7:
        return ChannelConfigSurround7Dot0;
    case 8:
        return ChannelConfigSurround7Dot1;
    default:
        return ChannelConfigUnknown;
    }
}
