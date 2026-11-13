// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qmimetype.h>
#include <QtCore/qstring.h>

class QMediaFormat {
public:
    enum FileFormat {
        UnspecifiedFormat = -1,
        WMV,
        AVI,
        Matroska,
        MPEG4,
        Ogg,
        QuickTime,
        WebM,
        Mpeg4Audio,
        AAC,
        WMA,
        MP3,
        FLAC,
        Wave,
        LastFileFormat = Wave
    };

    enum class AudioCodec {
        Unspecified = -1,
        MP3,
        AAC,
        AC3,
        EAC3,
        FLAC,
        DolbyTrueHD,
        Opus,
        Vorbis,
        Wave,
        WMA,
        ALAC,
        LastAudioCodec = ALAC
    };

    enum class VideoCodec {
        Unspecified = -1,
        MPEG1,
        MPEG2,
        MPEG4,
        H264,
        H265,
        VP8,
        VP9,
        AV1,
        Theora,
        WMV,
        MotionJPEG,
        LastVideoCodec = MotionJPEG
    };

    enum ConversionMode { Encode, Decode };
    enum ResolveFlags { NoFlags, RequiresVideo };

    QMediaFormat(FileFormat format = UnspecifiedFormat) : m_format(format) {}

    FileFormat fileFormat() const { return m_format; }
    void setFileFormat(FileFormat f) { m_format = f; }
    void setVideoCodec(VideoCodec codec) { m_video = codec; }
    VideoCodec videoCodec() const { return m_video; }
    void setAudioCodec(AudioCodec codec) { m_audio = codec; }
    AudioCodec audioCodec() const { return m_audio; }

    bool isSupported(ConversionMode mode) const;
    QMimeType mimeType() const;

    QList<FileFormat> supportedFileFormats(ConversionMode m);
    QList<VideoCodec> supportedVideoCodecs(ConversionMode m);
    QList<AudioCodec> supportedAudioCodecs(ConversionMode m);

    static QString fileFormatName(FileFormat fileFormat);
    static QString audioCodecName(AudioCodec codec);
    static QString videoCodecName(VideoCodec codec);
    static QString fileFormatDescription(FileFormat fileFormat);
    static QString audioCodecDescription(AudioCodec codec);
    static QString videoCodecDescription(VideoCodec codec);

    void resolveForEncoding(ResolveFlags flags);

    bool operator==(const QMediaFormat &other) const {
        return m_format == other.m_format && m_audio == other.m_audio && m_video == other.m_video;
    }
    bool operator!=(const QMediaFormat &other) const { return !(*this == other); }

private:
    FileFormat m_format;
    AudioCodec m_audio = AudioCodec::Unspecified;
    VideoCodec m_video = VideoCodec::Unspecified;
};
