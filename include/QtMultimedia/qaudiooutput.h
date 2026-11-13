// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtMultimedia/qaudiodevice.h>

class QAudioOutput : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAudioDevice device READ device WRITE setDevice NOTIFY deviceChanged)
    Q_PROPERTY(float volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY mutedChanged)
public:
    explicit QAudioOutput(QObject *parent = nullptr);
    explicit QAudioOutput(const QAudioDevice &device, QObject *parent = nullptr);
    ~QAudioOutput() override;

    QAudioDevice device() const;
    float volume() const;
    bool isMuted() const;

public slots:
    void setDevice(const QAudioDevice &device);
    void setVolume(float volume);
    void setMuted(bool muted);

signals:
    /*!
        This signal is emitted when the output device changes.

        \sa device()
    */
    void deviceChanged() { emitSignal(&QAudioOutput::deviceChanged); }
    /*!
        This signal is emitted when the volume changes to \a volume.
    */
    void volumeChanged(float volume) { emitSignal(&QAudioOutput::volumeChanged, volume); }
    /*!
        This signal is emitted when the output is muted or unmuted; \a muted is the new state.
    */
    void mutedChanged(bool muted) { emitSignal(&QAudioOutput::mutedChanged, muted); }

private:
    QAudioDevice m_device;
    float m_volume = 1.0f;
    bool m_muted = false;
};
