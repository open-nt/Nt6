// MIT License - see LICENSE
#include <QtMultimedia/qtaudio.h>

#include <algorithm>
#include <cmath>

/*!
    \namespace QtAudio
    \inmodule NtMultimedia
    \brief The QtAudio namespace holds the enumerations and functions shared by the audio classes.

    \c QAudio is another name for it, as in Qt before 6.7.

    \differences
    The enumerations have no meta-object, so they cannot be converted to and from strings with
    QMetaEnum.
    \enddifferences

    \sa QAudioSink, QAudioSource
*/

/*!
    \enum QtAudio::Error

    What went wrong with an audio stream.

    \value NoError Nothing.
    \value OpenError The device could not be opened, or does not take the format.
    \value IOError Reading from or writing to the device failed.
    \value UnderrunError Audio data was not supplied fast enough, and the device ran out.
    \value FatalError The device failed and cannot be used again.
*/

/*!
    \enum QtAudio::State

    The state of an audio stream.

    \value ActiveState Audio data is flowing.
    \value SuspendedState The stream is paused by suspend().
    \value StoppedState The stream is closed.
    \value IdleState The stream is open but has no data to play.
*/

/*!
    \enum QtAudio::VolumeScale

    The scales a volume can be measured in, for convertVolume().

    \value LinearVolumeScale The factor the samples are multiplied by, from 0.0 (silent) to 1.0 (full
        volume). QAudioSink, QSoundEffect and QAudioOutput take volumes on this scale.
    \value CubicVolumeScale The cube root of the linear scale, close to how loud a sound seems; a good
        scale for a volume slider.
    \value LogarithmicVolumeScale \c{1 - exp(-linear * ln(100))}, another perceptual scale from 0.0 to 1.0.
    \value DecibelVolumeScale Decibels relative to full volume: 0 dB is full volume and -200 dB silence.
*/

namespace {

const float kLn100 = std::log(100.0f);

// Every scale goes through linear.
float toLinear(float volume, QtAudio::VolumeScale from) {
    switch (from) {
    case QtAudio::LinearVolumeScale:
        return std::max(volume, 0.0f);
    case QtAudio::CubicVolumeScale:
        volume = std::max(volume, 0.0f);
        return volume * volume * volume;
    case QtAudio::LogarithmicVolumeScale:
        volume = std::max(volume, 0.0f);
        return volume > 0.99f ? 1.0f : -std::log(1.0f - volume) / kLn100;
    case QtAudio::DecibelVolumeScale:
        return std::pow(10.0f, volume / 20.0f);
    }
    return volume;
}

} // namespace

/*!
    Returns \a volume, measured on the scale \a from, measured on the scale \a to instead. A
    slider that shows a perceptual scale converts its value to linear for setVolume():

    \code
    sink->setVolume(QtAudio::convertVolume(slider->value() / 100.0f, QtAudio::LogarithmicVolumeScale,
                                           QtAudio::LinearVolumeScale));
    \endcode
*/
float QtAudio::convertVolume(float volume, VolumeScale from, VolumeScale to) {
    if (from == to)
        return volume;
    const float linear = toLinear(volume, from);
    switch (to) {
    case LinearVolumeScale:
        return linear;
    case CubicVolumeScale:
        return std::cbrt(linear);
    case LogarithmicVolumeScale:
        return 1.0f - std::exp(-linear * kLn100);
    case DecibelVolumeScale:
        return linear < 0.001f ? -200.0f : 20.0f * std::log10(linear);
    }
    return linear;
}
