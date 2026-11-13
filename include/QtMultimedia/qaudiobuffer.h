// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qshareddata.h>
#include <QtMultimedia/qaudioformat.h>

#include <cstddef>
#include <type_traits>

template <typename T, int N> struct QAudioFrame {
    static constexpr int NChannels = N;
    T channels[static_cast<std::size_t>(N)];

    T value(QAudioFormat::AudioChannelPosition position) const {
        const int i = index(position);
        return i < 0 ? T{} : channels[i];
    }
    void setValue(QAudioFormat::AudioChannelPosition position, T value) {
        const int i = index(position);
        if (i >= 0)
            channels[i] = value;
    }
    T average() const {
        double sum = 0;
        for (const T &c : channels)
            sum += double(c);
        return T(sum / N);
    }
    void clear() {
        for (T &c : channels)
            c = std::is_same_v<T, unsigned char> ? T(128) : T(0);
    }

private:
    static int index(QAudioFormat::AudioChannelPosition position) {
        QAudioFormat format;
        format.setChannelCount(N);
        return format.channelOffset(position);
    }
};

class QAudioBufferPrivate;

class QAudioBuffer {
public:
    using U8M = QAudioFrame<unsigned char, 1>;
    using S16M = QAudioFrame<short, 1>;
    using S32M = QAudioFrame<int, 1>;
    using F32M = QAudioFrame<float, 1>;
    using U8S = QAudioFrame<unsigned char, 2>;
    using S16S = QAudioFrame<short, 2>;
    using S32S = QAudioFrame<int, 2>;
    using F32S = QAudioFrame<float, 2>;

    QAudioBuffer() noexcept;
    QAudioBuffer(const QByteArray &data, const QAudioFormat &format, qint64 startTime = -1);
    QAudioBuffer(int numFrames, const QAudioFormat &format, qint64 startTime = -1);
    QAudioBuffer(const QAudioBuffer &other) noexcept;
    QAudioBuffer(QAudioBuffer &&other) noexcept;
    ~QAudioBuffer();
    QAudioBuffer &operator=(const QAudioBuffer &other);
    QAudioBuffer &operator=(QAudioBuffer &&other) noexcept;
    void swap(QAudioBuffer &other) noexcept { d.swap(other.d); }

    bool isValid() const noexcept;
    void detach();

    QAudioFormat format() const noexcept;
    qsizetype frameCount() const noexcept;
    qsizetype sampleCount() const noexcept;
    qsizetype byteCount() const noexcept;
    qint64 duration() const noexcept;
    qint64 startTime() const noexcept;

    template <typename T> const T *constData() const { return static_cast<const T *>(constDataRaw()); }
    template <typename T> const T *data() const { return static_cast<const T *>(constDataRaw()); }
    template <typename T> T *data() { return static_cast<T *>(dataRaw()); }

private:
    const void *constDataRaw() const noexcept;
    void *dataRaw();

    QExplicitlySharedDataPointer<QAudioBufferPrivate> d;
};
