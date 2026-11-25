// MIT License - see LICENSE
#include <QtMultimedia/qaudioheadset.h>

#include "qaudiobackend_p.h"

#include <QtCore/qtimer.h>

#include <algorithm>
#include <cmath>

namespace {

// Below this, a microphone is taken to be dynamic (a coil, which a bias current would only load), and from it up an
// electret capsule that needs the bias to power its built-in amplifier.
constexpr int kDynamicImpedanceLimit = 1000;
// The gain that each kind is given by setMicrophoneImpedance(): a dynamic capsule gives about 15 dB less signal.
constexpr float kElectretGain = 24.0f;
constexpr float kDynamicGain = 36.0f;

} // namespace

class QAudioHeadsetPrivate {
public:
    explicit QAudioHeadsetPrivate(QAudioHeadset *owner) : q(owner) {}

    QtMultimediaPrivate::AudioHeadsetControl *control() const {
        return QtMultimediaPrivate::audioBackend() ? QtMultimediaPrivate::audioBackend()->headsetControl() : nullptr;
    }

    // Applies next and emits what changed. Returns false when the codec could not be reached.
    bool apply(const QtMultimediaPrivate::AudioHeadsetSettings &next, int nextImpedance) {
        const QtMultimediaPrivate::AudioHeadsetSettings before = settings;
        const int impedanceBefore = impedance;
        if (QtMultimediaPrivate::AudioHeadsetControl *c = control()) {
            QString error;
            if (!c->apply(next, &error)) {
                errorString = error;
                emit q->errorOccurred();
                return false;
            }
            settings = c->settings();
        } else {
            settings = next;
        }
        impedance = nextImpedance;
        if (impedance != impedanceBefore)
            emit q->microphoneImpedanceChanged(impedance);
        if (settings.bias != before.bias)
            emit q->microphoneBiasChanged(QAudioHeadset::MicrophoneBias(settings.bias));
        if (settings.gain != before.gain)
            emit q->microphoneGainChanged(float(settings.gain));
        if (settings.channel != before.channel)
            emit q->microphoneChannelChanged(QAudioHeadset::Channel(settings.channel));
        if (settings.input != before.input)
            emit q->microphoneInputChanged(settings.input);
        poll();
        return true;
    }

    void poll() {
        QtMultimediaPrivate::AudioHeadsetControl *c = control();
        if (!c)
            return;
        const int status = c->status();
        if (status < 0)
            return;
        const bool mic = (status & QtMultimediaPrivate::AudioHeadsetControl::MicrophonePresent) != 0;
        const bool button = mic && (status & QtMultimediaPrivate::AudioHeadsetControl::ButtonPressed) != 0;
        if (mic != microphoneConnected) {
            microphoneConnected = mic;
            emit q->microphoneConnectedChanged(mic);
        }
        if (button != buttonPressed) {
            buttonPressed = button;
            emit q->buttonPressedChanged(button);
        }
    }

    QAudioHeadset *q;
    QtMultimediaPrivate::AudioHeadsetSettings settings;
    int impedance = 0;
    bool microphoneConnected = false;
    bool buttonPressed = false;
    QString errorString;
    QTimer timer;
};

/*!
    \class QAudioHeadset
    \inmodule NtMultimedia
    \brief The QAudioHeadset class sets up the microphone of a headset plugged into the audio codec.
    \since 6.0

    A headset microphone has to be matched to the input it is plugged into. An electret capsule,
    the kind in most computer and phone headsets, has an impedance of a kilohm or more and needs a
    bias voltage to power its built-in amplifier. A dynamic capsule, common in aviation and
    military headsets, is a coil of 5 to 600 ohms, needs no bias, gives a weaker signal, and is
    loaded down by a bias current. Tell QAudioHeadset the impedance of the microphone and it sets
    the bias and the gain to suit:

    \code
    QAudioHeadset headset;
    if (!headset.isAvailable())
        qWarning() << "this board has no headset control";
    headset.setMicrophoneImpedance(150);      // a dynamic microphone: no bias, more gain
    headset.setMicrophoneGain(40.0f);         // and a little more than the default for it
    QObject::connect(&headset, &QAudioHeadset::buttonPressedChanged, [](bool pressed) {
        qDebug() << "headset button" << pressed;
    });

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);                // the channel that microphoneChannel() names
    format.setSampleFormat(QAudioFormat::Int16);
    QAudioSource source(QMediaDevices::defaultAudioInput(), format);
    QIODevice *mic = source.start();
    \endcode

    The settings belong to the codec, so every QAudioHeadset object sees the same ones, and they
    stay in force while streams are opened and closed. They are kept when the codec is set up
    again for a new stream. On Linux the first object starts from what the mixer has.

    While the bias is on, the codec senses the current through the microphone contact: it reports
    whether a microphone is plugged in, with microphoneConnectedChanged(), and whether the button
    of the headset, which shorts the microphone, is held down, with buttonPressedChanged(). The
    object polls the codec for these every pollInterval() milliseconds. With the bias off nothing
    is sensed, and isMicrophoneConnected() is \c false.

    \section1 Supported Hardware

    \list
        \li \b Zephyr, with the Wolfson (Cirrus Logic) WM8962 codec, as on the NXP MIMXRT1170-EVKB
            board: everything. Its gain runs from -23.25 dB to +54 dB: the input PGA up to +24 dB in
            steps of 0.75 dB, then the mixer boost of the PGA in steps of 3 to 7 dB.
        \li \b Linux, through the ALSA mixer of one sound card: the gain is split between the
            card's microphone boost (such as \c{Mic Boost}) and its capture volume (\c Capture), the
            input pin chooses an item of \c{Input Source} or \c{Capture Source}, the bias turns a
            switch with \c Bias in its name, and a jack control (such as \c{Mic Jack}) reports the
            microphone. Each works where the card's driver has the control; most drivers keep the
            bias on whenever the input is in use and have no switch for it. The button is not
            reported. The card is the one with the most of these controls, or the one that the
            environment variable \c NT_AUDIO_HEADSET_CARD names, by number or by id (\c PCH).
            codecName() is the name of the card.
    \endlist

    Elsewhere isAvailable() returns \c false, and the setters only store their values.

    \differences
    Qt has no headset class; QAudioHeadset is an Nt addition. A codec cannot change the impedance
    of its input: setMicrophoneImpedance() chooses the bias and gain that suit a microphone of that
    impedance. Matching a load resistor to the microphone needs a switch on the board, which can be
    driven with QGpioLine. The thresholds at which the codec senses a microphone and a button are
    those of its reset state. Jack detection of the headphones is not reported. On Linux the
    settings are written to the card's mixer, where PipeWire or PulseAudio may change them again
    when the volume of their source is set, and they are not kept for the next login.
    \enddifferences

    \sa QAudioSource, QAudioInput, {Nt on Zephyr}
*/

/*!
    \enum QAudioHeadset::MicrophoneBias

    This enum describes the bias voltage that the codec puts on the microphone contact.

    \value BiasOff No bias, for a dynamic microphone. The codec senses no microphone or button.
    \value BiasLow The lower of the codec's two bias levels.
    \value BiasHigh The higher of the codec's two bias levels, for most electret microphones.
*/

/*!
    \enum QAudioHeadset::Channel

    This enum names the channel of the codec's stereo input that the microphone is routed to.

    \value LeftChannel The left channel: the first sample of each frame.
    \value RightChannel The right channel: the second sample of each frame.
*/

/*!
    Constructs a headset control with the given \a parent. It takes the settings the codec has
    now, and starts polling for the microphone and its button.
*/
QAudioHeadset::QAudioHeadset(QObject *parent) : QObject(parent), d(std::make_unique<QAudioHeadsetPrivate>(this)) {
    if (QtMultimediaPrivate::AudioHeadsetControl *c = d->control()) {
        d->settings = c->settings();
        d->poll();
        d->timer.setInterval(100);
        QObject::connect(&d->timer, &QTimer::timeout, this, [this] { d->poll(); });
        d->timer.start();
    }
}

/*!
    Destroys the object. The codec keeps its settings.
*/
QAudioHeadset::~QAudioHeadset() = default;

/*!
    Returns \c true if the platform can control the headset microphone of its codec.
*/
bool QAudioHeadset::isAvailable() const { return d->control() != nullptr; }

/*!
    Returns the name of the codec, such as \c WM8962 or, on Linux, the name of the sound card
    (\c{HDA Intel PCH}), or an empty string if isAvailable() is
    \c false.
*/
QString QAudioHeadset::codecName() const {
    QtMultimediaPrivate::AudioHeadsetControl *c = d->control();
    return c ? c->codecName() : QString();
}

/*!
    Sets the impedance of the microphone to \a ohms, and the bias and gain to suit it.

    Below 1000 ohms the microphone is taken to be dynamic: the bias is turned off and the gain
    set to +36 dB. From 1000 ohms up it is taken to be an electret capsule: the bias is set to
    BiasHigh and the gain to +24 dB. Zero or less leaves the bias and gain as they are. Call
    setMicrophoneBias() and setMicrophoneGain() afterwards to change them.

    \sa microphoneImpedance()
*/
void QAudioHeadset::setMicrophoneImpedance(int ohms) {
    QtMultimediaPrivate::AudioHeadsetSettings next = d->settings;
    if (ohms > 0) {
        const bool dynamic = ohms < kDynamicImpedanceLimit;
        next.bias = dynamic ? BiasOff : BiasHigh;
        next.gain =
            std::clamp(dynamic ? kDynamicGain : kElectretGain, minimumMicrophoneGain(), maximumMicrophoneGain());
    }
    d->apply(next, std::max(ohms, 0));
}

/*!
    Returns the impedance of the microphone, in ohms, as last set with setMicrophoneImpedance(),
    or 0 if it was never set.
*/
int QAudioHeadset::microphoneImpedance() const { return d->impedance; }

/*!
    Sets the bias voltage on the microphone contact to \a bias.

    \sa setMicrophoneImpedance()
*/
void QAudioHeadset::setMicrophoneBias(MicrophoneBias bias) {
    QtMultimediaPrivate::AudioHeadsetSettings next = d->settings;
    next.bias = std::clamp(int(bias), int(BiasOff), int(BiasHigh));
    d->apply(next, d->impedance);
}

/*!
    Returns the bias voltage on the microphone contact. It is BiasHigh until set.
*/
QAudioHeadset::MicrophoneBias QAudioHeadset::microphoneBias() const { return MicrophoneBias(d->settings.bias); }

/*!
    Sets the analog gain of the microphone to \a decibels, which is clamped to the range from
    minimumMicrophoneGain() to maximumMicrophoneGain() and rounded to a step of the codec.

    The gain is applied in the codec, ahead of its converter, so it raises the signal above the
    converter's noise. QAudioInput::setVolume() and QAudioSource::setVolume() scale the samples
    after conversion.
*/
void QAudioHeadset::setMicrophoneGain(float decibels) {
    QtMultimediaPrivate::AudioHeadsetSettings next = d->settings;
    next.gain = double(std::clamp(decibels, minimumMicrophoneGain(), maximumMicrophoneGain()));
    d->apply(next, d->impedance);
}

/*!
    Returns the analog gain of the microphone, in dB. It is +24 dB until set.
*/
float QAudioHeadset::microphoneGain() const { return float(d->settings.gain); }

/*!
    Returns the lowest gain, in dB, that setMicrophoneGain() can set.
*/
float QAudioHeadset::minimumMicrophoneGain() const {
    QtMultimediaPrivate::AudioHeadsetControl *c = d->control();
    return c ? float(c->minimumGain()) : -100.0f;
}

/*!
    Returns the highest gain, in dB, that setMicrophoneGain() can set.
*/
float QAudioHeadset::maximumMicrophoneGain() const {
    QtMultimediaPrivate::AudioHeadsetControl *c = d->control();
    return c ? float(c->maximumGain()) : 100.0f;
}

/*!
    Sets the channel of the codec's input that the microphone is routed to, to \a channel. The
    gain and the input pin apply to that channel. A QAudioSource with one channel records it. On
    Linux the gain applies to both channels, and the channel is only stored.

    \sa setMicrophoneInput()
*/
void QAudioHeadset::setMicrophoneChannel(Channel channel) {
    QtMultimediaPrivate::AudioHeadsetSettings next = d->settings;
    next.channel = channel == LeftChannel ? 0 : 1;
    d->apply(next, d->impedance);
}

/*!
    Returns the channel of the codec's input that the microphone is routed to. It is
    RightChannel until set.
*/
QAudioHeadset::Channel QAudioHeadset::microphoneChannel() const { return Channel(d->settings.channel); }

/*!
    Sets the input pin of the codec that the microphone is wired to, to \a input, numbered from 1.
    On the WM8962 these are the pins IN1 to IN4 of the microphoneChannel() side: IN1L to IN4L or
    IN1R to IN4R. Other numbers are ignored.
*/
void QAudioHeadset::setMicrophoneInput(int input) {
    if (input < 1 || input > 4)
        return;
    QtMultimediaPrivate::AudioHeadsetSettings next = d->settings;
    next.input = input;
    d->apply(next, d->impedance);
}

/*!
    Returns the input pin of the codec that the microphone is wired to, numbered from 1. It is 3
    until set: IN3R, with the default RightChannel.
*/
int QAudioHeadset::microphoneInput() const { return d->settings.input; }

/*!
    Returns \c true if the codec senses a microphone on the contact. It needs the bias on.

    \sa microphoneConnectedChanged()
*/
bool QAudioHeadset::isMicrophoneConnected() const { return d->microphoneConnected; }

/*!
    Returns \c true while the button of the headset is held down.

    \sa buttonPressedChanged()
*/
bool QAudioHeadset::isButtonPressed() const { return d->buttonPressed; }

/*!
    Sets how often the codec is asked about the microphone and its button, to every \a msecs
    milliseconds. Zero or less stops asking. The default is 100.
*/
void QAudioHeadset::setPollInterval(int msecs) {
    if (msecs <= 0) {
        d->timer.stop();
        d->timer.setInterval(0);
        return;
    }
    d->timer.setInterval(msecs);
    if (isAvailable())
        d->timer.start();
}

/*!
    Returns how often, in milliseconds, the codec is asked about the microphone and its button,
    or 0 if it is not.
*/
int QAudioHeadset::pollInterval() const { return d->timer.isActive() ? d->timer.interval() : 0; }

/*!
    Returns a description of the last error, or an empty string.

    \sa errorOccurred()
*/
QString QAudioHeadset::errorString() const { return d->errorString; }

#include "moc_qaudioheadset.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
