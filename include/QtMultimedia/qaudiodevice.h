// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>
#include <QtMultimedia/qaudioformat.h>

class QAudioDevicePrivate;

class QAudioDevice {
public:
    enum Mode { Null, Input, Output };

    QAudioDevice() noexcept;
    QAudioDevice(const QAudioDevice &other);
    QAudioDevice(QAudioDevice &&other) noexcept;
    ~QAudioDevice();
    QAudioDevice &operator=(const QAudioDevice &other);
    QAudioDevice &operator=(QAudioDevice &&other) noexcept;
    void swap(QAudioDevice &other) noexcept { d.swap(other.d); }

    bool operator==(const QAudioDevice &other) const;
    bool operator!=(const QAudioDevice &other) const { return !(*this == other); }

    bool isNull() const;

    QByteArray id() const;
    QString description() const;
    bool isDefault() const;
    Mode mode() const;

    bool isFormatSupported(const QAudioFormat &format) const;
    QAudioFormat preferredFormat() const;

    int minimumSampleRate() const;
    int maximumSampleRate() const;
    int minimumChannelCount() const;
    int maximumChannelCount() const;
    QList<QAudioFormat::SampleFormat> supportedSampleFormats() const;
    QAudioFormat::ChannelConfig channelConfiguration() const;

    explicit QAudioDevice(QAudioDevicePrivate *data);
    const QAudioDevicePrivate *handle() const { return d.data(); }

private:
    QExplicitlySharedDataPointer<QAudioDevicePrivate> d;
};
