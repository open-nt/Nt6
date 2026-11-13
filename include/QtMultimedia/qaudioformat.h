// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

class QAudioFormat {
public:
    enum AudioChannelPosition {
        UnknownPosition,
        FrontLeft,
        FrontRight,
        FrontCenter,
        LFE,
        BackLeft,
        BackRight,
        FrontLeftOfCenter,
        FrontRightOfCenter,
        BackCenter,
        SideLeft,
        SideRight,
        TopCenter,
        TopFrontLeft,
        TopFrontCenter,
        TopFrontRight,
        TopBackLeft,
        TopBackCenter,
        TopBackRight,
        LFE2,
        TopSideLeft,
        TopSideRight,
        BottomFrontCenter,
        BottomFrontLeft,
        BottomFrontRight
    };
    static constexpr int NChannelPositions = BottomFrontRight + 1;

    enum ChannelConfig : quint32 {
        ChannelConfigUnknown = 0,
        ChannelConfigMono = 1u << FrontCenter,
        ChannelConfigStereo = 1u << FrontLeft | 1u << FrontRight,
        ChannelConfig2Dot1 = 1u << FrontLeft | 1u << FrontRight | 1u << LFE,
        ChannelConfig3Dot0 = 1u << FrontLeft | 1u << FrontRight | 1u << FrontCenter,
        ChannelConfig3Dot1 = 1u << FrontLeft | 1u << FrontRight | 1u << FrontCenter | 1u << LFE,
        ChannelConfigSurround5Dot0 =
            1u << FrontLeft | 1u << FrontRight | 1u << FrontCenter | 1u << BackLeft | 1u << BackRight,
        ChannelConfigSurround5Dot1 =
            1u << FrontLeft | 1u << FrontRight | 1u << FrontCenter | 1u << LFE | 1u << BackLeft | 1u << BackRight,
        ChannelConfigSurround7Dot0 = 1u << FrontLeft | 1u << FrontRight | 1u << FrontCenter | 1u << BackLeft |
                                     1u << BackRight | 1u << SideLeft | 1u << SideRight,
        ChannelConfigSurround7Dot1 = 1u << FrontLeft | 1u << FrontRight | 1u << FrontCenter | 1u << LFE |
                                     1u << BackLeft | 1u << BackRight | 1u << SideLeft | 1u << SideRight
    };

    enum SampleFormat : quint16 { Unknown, UInt8, Int16, Int32, Float, NSampleFormats };

    constexpr QAudioFormat() noexcept = default;

    constexpr bool isValid() const noexcept { return m_sampleRate > 0 && m_channelCount > 0 && m_format != Unknown; }

    constexpr void setSampleRate(int sampleRate) noexcept { m_sampleRate = sampleRate; }
    constexpr int sampleRate() const noexcept { return m_sampleRate; }

    void setChannelConfig(ChannelConfig config) noexcept;
    constexpr ChannelConfig channelConfig() const noexcept { return m_config; }
    constexpr void setChannelCount(int channelCount) noexcept {
        m_config = ChannelConfigUnknown;
        m_channelCount = channelCount;
    }
    constexpr int channelCount() const noexcept { return m_channelCount; }
    int channelOffset(AudioChannelPosition channel) const noexcept;

    constexpr void setSampleFormat(SampleFormat format) noexcept { m_format = format; }
    constexpr SampleFormat sampleFormat() const noexcept { return m_format; }

    qint32 bytesForDuration(qint64 microseconds) const;
    qint64 durationForBytes(qint32 bytes) const;
    qint32 bytesForFrames(qint32 frameCount) const;
    qint32 framesForBytes(qint32 byteCount) const;
    qint32 framesForDuration(qint64 microseconds) const;
    qint64 durationForFrames(qint32 frameCount) const;

    constexpr int bytesPerFrame() const { return bytesPerSample() * channelCount(); }
    constexpr int bytesPerSample() const noexcept {
        switch (m_format) {
        case Unknown:
        case NSampleFormats:
            return 0;
        case UInt8:
            return 1;
        case Int16:
            return 2;
        case Int32:
        case Float:
            return 4;
        }
        return 0;
    }

    float normalizedSampleValue(const void *sample) const;

    static ChannelConfig defaultChannelConfigForChannelCount(int channelCount);

    template <typename... Args> static constexpr ChannelConfig channelConfig(Args... channels) {
        // A set of positions need not be an enumerator; ChannelConfig is a quint32 bitmask, as in Qt.
        // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
        return ChannelConfig((0u | ... | (1u << quint32(channels))));
    }

    friend constexpr bool operator==(const QAudioFormat &a, const QAudioFormat &b) noexcept {
        return a.m_sampleRate == b.m_sampleRate && a.m_channelCount == b.m_channelCount && a.m_format == b.m_format &&
               a.m_config == b.m_config;
    }
    friend constexpr bool operator!=(const QAudioFormat &a, const QAudioFormat &b) noexcept { return !(a == b); }

private:
    SampleFormat m_format = Unknown;
    ChannelConfig m_config = ChannelConfigUnknown;
    int m_channelCount = 0;
    int m_sampleRate = 0;
};
