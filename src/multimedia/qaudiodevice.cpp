// MIT License - see LICENSE
#include "qaudiobackend_p.h"

#include <algorithm>

/*!
    \class QAudioDevice
    \inmodule NtMultimedia
    \brief The QAudioDevice class describes an audio input or output device.

    QMediaDevices lists the devices; pass one to QAudioSink, QAudioSource, QSoundEffect or
    QAudioOutput to use it instead of the default:

    \snippet multimedia/audiodevices.cpp list

    A default-constructed QAudioDevice is null. Copies share their data.

    \differences
    The capabilities that ALSA does not report are given as the range Nt can convert: 8000 to 192000
    Hz, 1 to 8 channels and every sample format, since the \c default device and its plug plugin
    convert. QAudioDevice is not a Q_GADGET, so it has no properties.
    \enddifferences

    \sa QMediaDevices, QAudioFormat
*/

/*!
    \enum QAudioDevice::Mode

    Which way audio goes through a device.

    \value Null A null device.
    \value Input A device that records, such as a microphone.
    \value Output A device that plays, such as speakers or headphones.
*/

/*!
    Constructs a null device.
*/
QAudioDevice::QAudioDevice() noexcept = default;

/*!
    Constructs a copy of \a other, sharing its data.
*/
QAudioDevice::QAudioDevice(const QAudioDevice &other) = default;

/*!
    Move-constructs a device from \a other, which becomes null.
*/
QAudioDevice::QAudioDevice(QAudioDevice &&other) noexcept = default;

/*!
    \internal
*/
QAudioDevice::QAudioDevice(QAudioDevicePrivate *data) : d(data) {}

/*!
    Destroys the device.
*/
QAudioDevice::~QAudioDevice() = default;

/*!
    Makes this device a copy of \a other and returns a reference to it.
*/
QAudioDevice &QAudioDevice::operator=(const QAudioDevice &other) = default;

/*!
    Move-assigns \a other to this device and returns a reference to it.
*/
QAudioDevice &QAudioDevice::operator=(QAudioDevice &&other) noexcept = default;

/*!
    \fn void QAudioDevice::swap(QAudioDevice &other)

    Swaps this device with \a other.
*/

/*!
    \fn bool QAudioDevice::operator!=(const QAudioDevice &other) const

    Returns \c true if this device is not the same as \a other; otherwise returns \c false.
*/

/*!
    \fn const QAudioDevicePrivate *QAudioDevice::handle() const
    \internal
*/

/*!
    Returns \c true if this device and \a other are the same device: they have the same id and mode.
*/
bool QAudioDevice::operator==(const QAudioDevice &other) const {
    if (d == other.d)
        return true;
    if (!d || !other.d)
        return false;
    return d->id == other.d->id && d->mode == other.d->mode;
}

/*!
    Returns \c true if this is a null device; otherwise returns \c false.
*/
bool QAudioDevice::isNull() const { return !d; }

/*!
    Returns the id of the device, which identifies it to the platform: on Linux the ALSA device name,
    such as \c default or \c{plughw:CARD=PCH,DEV=0}.
*/
QByteArray QAudioDevice::id() const { return d ? d->id : QByteArray(); }

/*!
    Returns a description of the device for the user.
*/
QString QAudioDevice::description() const { return d ? d->description : QString(); }

/*!
    Returns \c true if this is the default device of its mode; otherwise returns \c false.
*/
bool QAudioDevice::isDefault() const { return d && d->isDefault; }

/*!
    Returns whether the device records or plays, or Null for a null device.
*/
QAudioDevice::Mode QAudioDevice::mode() const { return d ? d->mode : Null; }

/*!
    Returns \c true if the device can be opened with \a format; otherwise returns \c false.

    \sa preferredFormat()
*/
bool QAudioDevice::isFormatSupported(const QAudioFormat &format) const {
    if (!d || !format.isValid())
        return false;
    return format.sampleRate() >= d->minimumSampleRate && format.sampleRate() <= d->maximumSampleRate &&
           format.channelCount() >= d->minimumChannelCount && format.channelCount() <= d->maximumChannelCount &&
           d->sampleFormats.contains(format.sampleFormat());
}

/*!
    Returns the format the device works in best.
*/
QAudioFormat QAudioDevice::preferredFormat() const { return d ? d->preferredFormat : QAudioFormat(); }

/*!
    Returns the lowest sample rate the device takes.
*/
int QAudioDevice::minimumSampleRate() const { return d ? d->minimumSampleRate : 0; }

/*!
    Returns the highest sample rate the device takes.
*/
int QAudioDevice::maximumSampleRate() const { return d ? d->maximumSampleRate : 0; }

/*!
    Returns the smallest number of channels the device takes.
*/
int QAudioDevice::minimumChannelCount() const { return d ? d->minimumChannelCount : 0; }

/*!
    Returns the largest number of channels the device takes.
*/
int QAudioDevice::maximumChannelCount() const { return d ? d->maximumChannelCount : 0; }

/*!
    Returns the sample formats the device takes.
*/
QList<QAudioFormat::SampleFormat> QAudioDevice::supportedSampleFormats() const {
    return d ? d->sampleFormats : QList<QAudioFormat::SampleFormat>();
}

/*!
    Returns the speaker positions of the device's channels.
*/
QAudioFormat::ChannelConfig QAudioDevice::channelConfiguration() const {
    return d ? d->channelConfiguration : QAudioFormat::ChannelConfigUnknown;
}
