// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtMultimedia/qaudiodevice.h>

class QAudioInput : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAudioDevice device READ device WRITE setDevice NOTIFY deviceChanged)
    Q_PROPERTY(float volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY mutedChanged)
public:
    explicit QAudioInput(QObject *parent = nullptr);
    explicit QAudioInput(const QAudioDevice &deviceInfo, QObject *parent = nullptr);
    ~QAudioInput() override;

    QAudioDevice device() const;
    float volume() const;
    bool isMuted() const;

public slots:
    void setDevice(const QAudioDevice &device);
    void setVolume(float volume);
    void setMuted(bool muted);

signals:
    /*!
        This signal is emitted when the input device changes.
    */
    void deviceChanged() { emitSignal(&QAudioInput::deviceChanged); }
    /*!
        This signal is emitted when the volume changes to \a volume.
    */
    void volumeChanged(float volume) { emitSignal(&QAudioInput::volumeChanged, volume); }
    /*!
        This signal is emitted when the input is muted or unmuted; \a muted is the new state.
    */
    void mutedChanged(bool muted) { emitSignal(&QAudioInput::mutedChanged, muted); }

private:
    QAudioDevice m_device;
    float m_volume = 1.0f;
    bool m_muted = false;
};
