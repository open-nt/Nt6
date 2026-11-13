// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtMultimedia/qaudiodevice.h>

#include <memory>

class QMediaDevicesPrivate;

class QMediaDevices : public QObject {
    Q_OBJECT
    Q_PROPERTY(QList<QAudioDevice> audioInputs READ audioInputs NOTIFY audioInputsChanged)
    Q_PROPERTY(QList<QAudioDevice> audioOutputs READ audioOutputs NOTIFY audioOutputsChanged)
    Q_PROPERTY(QAudioDevice defaultAudioInput READ defaultAudioInput NOTIFY audioInputsChanged)
    Q_PROPERTY(QAudioDevice defaultAudioOutput READ defaultAudioOutput NOTIFY audioOutputsChanged)
public:
    explicit QMediaDevices(QObject *parent = nullptr);
    ~QMediaDevices() override;

    static QList<QAudioDevice> audioInputs();
    static QList<QAudioDevice> audioOutputs();
    static QAudioDevice defaultAudioInput();
    static QAudioDevice defaultAudioOutput();

signals:
    /*!
        This signal is emitted when an audio input is added or removed, or the default one changes.
    */
    void audioInputsChanged() { emitSignal(&QMediaDevices::audioInputsChanged); }
    /*!
        This signal is emitted when an audio output is added or removed, or the default one changes.
    */
    void audioOutputsChanged() { emitSignal(&QMediaDevices::audioOutputsChanged); }

private:
    friend class QMediaDevicesPrivate;
    std::unique_ptr<QMediaDevicesPrivate> d;
};
