// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtMultimedia/qaudiodevice.h>
#ifndef QT_NO_CAMERA
#include <QtMultimedia/qcameradevice.h>
#endif

#include <memory>

class QMediaDevicesPrivate;

class QMediaDevices : public QObject {
    Q_OBJECT
    Q_PROPERTY(QList<QAudioDevice> audioInputs READ audioInputs NOTIFY audioInputsChanged)
    Q_PROPERTY(QList<QAudioDevice> audioOutputs READ audioOutputs NOTIFY audioOutputsChanged)
    Q_PROPERTY(QAudioDevice defaultAudioInput READ defaultAudioInput NOTIFY audioInputsChanged)
    Q_PROPERTY(QAudioDevice defaultAudioOutput READ defaultAudioOutput NOTIFY audioOutputsChanged)
#ifndef QT_NO_CAMERA
    Q_PROPERTY(QList<QCameraDevice> videoInputs READ videoInputs NOTIFY videoInputsChanged)
#endif
public:
    explicit QMediaDevices(QObject *parent = nullptr);
    ~QMediaDevices() override;

    static QList<QAudioDevice> audioInputs();
    static QList<QAudioDevice> audioOutputs();
    static QAudioDevice defaultAudioInput();
    static QAudioDevice defaultAudioOutput();
#ifndef QT_NO_CAMERA
    static QList<QCameraDevice> videoInputs();
    static QCameraDevice defaultVideoInput();
#endif

signals:
    /*!
        This signal is emitted when an audio input is added or removed, or the default one changes.
    */
    void audioInputsChanged() { emitSignal(&QMediaDevices::audioInputsChanged); }
    /*!
        This signal is emitted when an audio output is added or removed, or the default one changes.
    */
    void audioOutputsChanged() { emitSignal(&QMediaDevices::audioOutputsChanged); }
#ifndef QT_NO_CAMERA
    /*!
        This signal is emitted when a camera is added or removed.
    */
    void videoInputsChanged() { emitSignal(&QMediaDevices::videoInputsChanged); }
#endif

private:
    friend class QMediaDevicesPrivate;
    std::unique_ptr<QMediaDevicesPrivate> d;
};
