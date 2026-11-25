// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

class QAudioHeadsetPrivate;

class QAudioHeadset : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable CONSTANT)
    Q_PROPERTY(QString codecName READ codecName CONSTANT)
    Q_PROPERTY(
        int microphoneImpedance READ microphoneImpedance WRITE setMicrophoneImpedance NOTIFY microphoneImpedanceChanged)
    Q_PROPERTY(MicrophoneBias microphoneBias READ microphoneBias WRITE setMicrophoneBias NOTIFY microphoneBiasChanged)
    Q_PROPERTY(float microphoneGain READ microphoneGain WRITE setMicrophoneGain NOTIFY microphoneGainChanged)
    Q_PROPERTY(
        Channel microphoneChannel READ microphoneChannel WRITE setMicrophoneChannel NOTIFY microphoneChannelChanged)
    Q_PROPERTY(int microphoneInput READ microphoneInput WRITE setMicrophoneInput NOTIFY microphoneInputChanged)
    Q_PROPERTY(bool microphoneConnected READ isMicrophoneConnected NOTIFY microphoneConnectedChanged)
    Q_PROPERTY(bool buttonPressed READ isButtonPressed NOTIFY buttonPressedChanged)
public:
    enum MicrophoneBias { BiasOff, BiasLow, BiasHigh };
    Q_ENUM(MicrophoneBias)
    enum Channel { LeftChannel, RightChannel };
    Q_ENUM(Channel)

    explicit QAudioHeadset(QObject *parent = nullptr);
    ~QAudioHeadset() override;

    bool isAvailable() const;
    QString codecName() const;

    void setMicrophoneImpedance(int ohms);
    int microphoneImpedance() const;
    void setMicrophoneBias(MicrophoneBias bias);
    MicrophoneBias microphoneBias() const;
    void setMicrophoneGain(float decibels);
    float microphoneGain() const;
    float minimumMicrophoneGain() const;
    float maximumMicrophoneGain() const;
    void setMicrophoneChannel(Channel channel);
    Channel microphoneChannel() const;
    void setMicrophoneInput(int input);
    int microphoneInput() const;

    bool isMicrophoneConnected() const;
    bool isButtonPressed() const;
    void setPollInterval(int msecs);
    int pollInterval() const;

    QString errorString() const;

signals:
    /*!
        This signal is emitted when the microphone impedance changes to \a ohms.
    */
    void microphoneImpedanceChanged(int ohms) { emitSignal(&QAudioHeadset::microphoneImpedanceChanged, ohms); }
    /*!
        This signal is emitted when the microphone bias changes to \a bias.
    */
    void microphoneBiasChanged(QAudioHeadset::MicrophoneBias bias) {
        emitSignal(&QAudioHeadset::microphoneBiasChanged, bias);
    }
    /*!
        This signal is emitted when the microphone gain changes to \a decibels.
    */
    void microphoneGainChanged(float decibels) { emitSignal(&QAudioHeadset::microphoneGainChanged, decibels); }
    /*!
        This signal is emitted when the codec channel of the microphone changes to \a channel.
    */
    void microphoneChannelChanged(QAudioHeadset::Channel channel) {
        emitSignal(&QAudioHeadset::microphoneChannelChanged, channel);
    }
    /*!
        This signal is emitted when the codec input of the microphone changes to \a input.
    */
    void microphoneInputChanged(int input) { emitSignal(&QAudioHeadset::microphoneInputChanged, input); }
    /*!
        This signal is emitted when the codec senses that a microphone is plugged in or out;
        \a connected tells which.

        \sa isMicrophoneConnected()
    */
    void microphoneConnectedChanged(bool connected) {
        emitSignal(&QAudioHeadset::microphoneConnectedChanged, connected);
    }
    /*!
        This signal is emitted when the button of the headset is pressed or released; \a pressed
        tells which.

        \sa isButtonPressed()
    */
    void buttonPressedChanged(bool pressed) { emitSignal(&QAudioHeadset::buttonPressedChanged, pressed); }
    /*!
        This signal is emitted when the codec cannot be reached; errorString() says why.
    */
    void errorOccurred() { emitSignal(&QAudioHeadset::errorOccurred); }

private:
    std::unique_ptr<QAudioHeadsetPrivate> d;
};
