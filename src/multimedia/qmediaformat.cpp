// MIT License - see LICENSE
#include <QtMultimedia/qmediaformat.h>

#include <QtCore/qmimedatabase.h>

/*!
    \class QMediaFormat
    \inmodule NtMultimedia
    \brief The QMediaFormat class describes a media file format and the codecs in it.

    QMediaRecorder records in a media format; isSupported() tells whether Nt can read or write one:

    \code
    QMediaFormat format(QMediaFormat::Wave);
    bool canRecord = format.isSupported(QMediaFormat::Encode); // true
    \endcode

    \differences
    Nt reads and writes only WAV files with uncompressed audio, so the only supported file format is
    Wave, with the Wave audio codec and no video codec.
    \enddifferences

    \sa QMediaRecorder
*/

/*!
    \enum QMediaFormat::FileFormat

    \value UnspecifiedFormat Not set; recording chooses Wave.
    \value WMV Windows Media Video.
    \value AVI Audio Video Interleave.
    \value Matroska Matroska.
    \value MPEG4 MPEG-4 video.
    \value Ogg Ogg.
    \value QuickTime QuickTime.
    \value WebM WebM.
    \value Mpeg4Audio MPEG-4 audio.
    \value AAC Advanced Audio Coding.
    \value WMA Windows Media Audio.
    \value MP3 MP3.
    \value FLAC FLAC.
    \value Wave WAV, the only format Nt supports.
    \omitvalue LastFileFormat
*/

/*!
    \enum QMediaFormat::AudioCodec

    \value Unspecified Not set.
    \value MP3 MP3.
    \value AAC AAC.
    \value AC3 Dolby Digital.
    \value EAC3 Dolby Digital Plus.
    \value FLAC FLAC.
    \value DolbyTrueHD Dolby TrueHD.
    \value Opus Opus.
    \value Vorbis Vorbis.
    \value Wave Uncompressed samples, the only codec Nt supports.
    \value WMA Windows Media Audio.
    \value ALAC Apple Lossless.
    \omitvalue LastAudioCodec
*/

/*!
    \enum QMediaFormat::VideoCodec

    Nt encodes and decodes no video codec.

    \value Unspecified Not set.
    \value MPEG1 MPEG-1.
    \value MPEG2 MPEG-2.
    \value MPEG4 MPEG-4 Part 2.
    \value H264 H.264.
    \value H265 H.265.
    \value VP8 VP8.
    \value VP9 VP9.
    \value AV1 AV1.
    \value Theora Theora.
    \value WMV Windows Media Video.
    \value MotionJPEG Motion JPEG.
    \omitvalue LastVideoCodec
*/

/*!
    \enum QMediaFormat::ConversionMode

    \value Encode Writing.
    \value Decode Reading.
*/

/*!
    \enum QMediaFormat::ResolveFlags

    \value NoFlags Audio only.
    \value RequiresVideo The format must hold video. Nt cannot satisfy this.
*/

/*!
    \fn QMediaFormat::QMediaFormat(QMediaFormat::FileFormat format)

    Constructs a media format with the file format \a format and no codecs.
*/

/*!
    \fn QMediaFormat::FileFormat QMediaFormat::fileFormat() const

    Returns the file format.
*/

/*!
    \fn void QMediaFormat::setFileFormat(QMediaFormat::FileFormat f)

    Sets the file format to \a f.
*/

/*!
    \fn void QMediaFormat::setVideoCodec(QMediaFormat::VideoCodec codec)

    Sets the video codec to \a codec.
*/

/*!
    \fn QMediaFormat::VideoCodec QMediaFormat::videoCodec() const

    Returns the video codec.
*/

/*!
    \fn void QMediaFormat::setAudioCodec(QMediaFormat::AudioCodec codec)

    Sets the audio codec to \a codec.
*/

/*!
    \fn QMediaFormat::AudioCodec QMediaFormat::audioCodec() const

    Returns the audio codec.
*/

/*!
    \fn bool QMediaFormat::operator==(const QMediaFormat &other) const

    Returns \c true if the file format and codecs equal those of \a other; otherwise returns \c false.
*/

/*!
    \fn bool QMediaFormat::operator!=(const QMediaFormat &other) const

    Returns \c true if this format differs from \a other; otherwise returns \c false.
*/

/*!
    Returns \c true if Nt can read (\a mode Decode) or write (\a mode Encode) files in this format:
    a Wave or unspecified file format with the Wave or no audio codec, and no video codec.
*/
bool QMediaFormat::isSupported(ConversionMode mode) const {
    (void)mode;
    return (m_format == Wave || m_format == UnspecifiedFormat) &&
           (m_audio == AudioCodec::Wave || m_audio == AudioCodec::Unspecified) && m_video == VideoCodec::Unspecified;
}

/*!
    Returns the MIME type of the file format.
*/
QMimeType QMediaFormat::mimeType() const {
    static const char *const names[] = {"video/x-ms-wmv", "video/x-msvideo", "video/x-matroska", "video/mp4",
                                        "audio/ogg",      "video/quicktime", "video/webm",       "audio/mp4",
                                        "audio/aac",      "audio/x-ms-wma",  "audio/mpeg",       "audio/flac",
                                        "audio/x-wav"};
    if (m_format < 0 || m_format > LastFileFormat)
        return QMimeType();
    return QMimeDatabase().mimeTypeForName(QString::fromLatin1(names[m_format]));
}

/*!
    Returns the file formats Nt can read or write, by \a m: Wave.
*/
QList<QMediaFormat::FileFormat> QMediaFormat::supportedFileFormats(ConversionMode m) {
    (void)m;
    return {Wave};
}

/*!
    Returns the video codecs Nt can read or write, by \a m: none.
*/
QList<QMediaFormat::VideoCodec> QMediaFormat::supportedVideoCodecs(ConversionMode m) {
    (void)m;
    return {};
}

/*!
    Returns the audio codecs Nt can read or write in the file format, by \a m: Wave, for a Wave or
    unspecified file format.
*/
QList<QMediaFormat::AudioCodec> QMediaFormat::supportedAudioCodecs(ConversionMode m) {
    (void)m;
    if (m_format == Wave || m_format == UnspecifiedFormat)
        return {AudioCodec::Wave};
    return {};
}

/*!
    Returns the short name of \a fileFormat, such as \c Wave.
*/
QString QMediaFormat::fileFormatName(FileFormat fileFormat) {
    static const char *const names[] = {"WMV",          "AVI", "Matroska", "MPEG-4", "Ogg",  "QuickTime", "WebM",
                                        "MPEG-4 Audio", "AAC", "WMA",      "MP3",    "FLAC", "Wave"};
    if (fileFormat < 0 || fileFormat > LastFileFormat)
        return QStringLiteral("Unspecified");
    return QString::fromLatin1(names[fileFormat]);
}

/*!
    Returns the short name of \a codec, such as \c Wave.
*/
QString QMediaFormat::audioCodecName(AudioCodec codec) {
    static const char *const names[] = {"MP3",  "AAC",    "AC3",  "EAC3", "FLAC", "Dolby TrueHD",
                                        "Opus", "Vorbis", "Wave", "WMA",  "ALAC"};
    const int i = int(codec);
    if (i < 0 || i > int(AudioCodec::LastAudioCodec))
        return QStringLiteral("Unspecified");
    return QString::fromLatin1(names[i]);
}

/*!
    Returns the short name of \a codec, such as \c H264.
*/
QString QMediaFormat::videoCodecName(VideoCodec codec) {
    static const char *const names[] = {"MPEG1", "MPEG2", "MPEG4",  "H264", "H265",      "VP8",
                                        "VP9",   "AV1",   "Theora", "WMV",  "MotionJPEG"};
    const int i = int(codec);
    if (i < 0 || i > int(VideoCodec::LastVideoCodec))
        return QStringLiteral("Unspecified");
    return QString::fromLatin1(names[i]);
}

/*!
    Returns a description of \a fileFormat for the user.
*/
QString QMediaFormat::fileFormatDescription(FileFormat fileFormat) {
    static const char *const names[] = {"Windows Media Video",
                                        "Audio Video Interleave",
                                        "Matroska Multimedia Container",
                                        "MPEG-4 Video Container",
                                        "Ogg",
                                        "QuickTime Container",
                                        "WebM",
                                        "MPEG-4 Audio",
                                        "AAC",
                                        "Windows Media Audio",
                                        "MP3",
                                        "Free Lossless Audio Codec (FLAC)",
                                        "Wave File"};
    if (fileFormat < 0 || fileFormat > LastFileFormat)
        return QStringLiteral("Unspecified File Format");
    return QString::fromLatin1(names[fileFormat]);
}

/*!
    Returns a description of \a codec for the user.
*/
QString QMediaFormat::audioCodecDescription(AudioCodec codec) {
    static const char *const names[] = {"MP3",
                                        "Advanced Audio Codec (AAC)",
                                        "Dolby Digital (AC3)",
                                        "Dolby Digital Plus (E-AC3)",
                                        "Free Lossless Audio Codec (FLAC)",
                                        "Dolby True HD",
                                        "Opus",
                                        "Vorbis",
                                        "Wave",
                                        "Windows Media Audio",
                                        "Apple Lossless Audio Codec (ALAC)"};
    const int i = int(codec);
    if (i < 0 || i > int(AudioCodec::LastAudioCodec))
        return QStringLiteral("Unspecified Audio Codec");
    return QString::fromLatin1(names[i]);
}

/*!
    Returns a description of \a codec for the user.
*/
QString QMediaFormat::videoCodecDescription(VideoCodec codec) {
    static const char *const names[] = {"MPEG-1 Video", "MPEG-2 Video", "MPEG-4 Video", "H.264",  "H.265",
                                        "VP8",          "VP9",          "AV1",          "Theora", "Windows Media Video",
                                        "MotionJPEG"};
    const int i = int(codec);
    if (i < 0 || i > int(VideoCodec::LastVideoCodec))
        return QStringLiteral("Unspecified Video Codec");
    return QString::fromLatin1(names[i]);
}

/*!
    Fills in what is unspecified, or unsupported, with what Nt can write: the Wave file format and
    audio codec, and no video codec. With \a flags RequiresVideo no format qualifies, and the format
    is set to Wave all the same.
*/
void QMediaFormat::resolveForEncoding(ResolveFlags flags) {
    (void)flags;
    m_format = Wave;
    m_audio = AudioCodec::Wave;
    m_video = VideoCodec::Unspecified;
}
