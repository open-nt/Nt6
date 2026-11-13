// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>

#include <memory>

class QAudioInput;
class QAudioOutput;
class QCamera;
class QImageCapture;
class QMediaRecorder;
class QVideoSink;
class QMediaCaptureSessionPrivate;

class QMediaCaptureSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAudioInput *audioInput READ audioInput WRITE setAudioInput NOTIFY audioInputChanged)
    Q_PROPERTY(QAudioOutput *audioOutput READ audioOutput WRITE setAudioOutput NOTIFY audioOutputChanged)
    Q_PROPERTY(QCamera *camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(QImageCapture *imageCapture READ imageCapture WRITE setImageCapture NOTIFY imageCaptureChanged)
    Q_PROPERTY(QMediaRecorder *recorder READ recorder WRITE setRecorder NOTIFY recorderChanged)
    Q_PROPERTY(QObject *videoOutput READ videoOutput WRITE setVideoOutput NOTIFY videoOutputChanged)
public:
    explicit QMediaCaptureSession(QObject *parent = nullptr);
    ~QMediaCaptureSession() override;

    QAudioInput *audioInput() const;
    void setAudioInput(QAudioInput *input);

    QCamera *camera() const;
    void setCamera(QCamera *camera);

    QImageCapture *imageCapture();
    void setImageCapture(QImageCapture *imageCapture);

    QMediaRecorder *recorder();
    void setRecorder(QMediaRecorder *recorder);

    void setVideoOutput(QObject *output);
    QObject *videoOutput() const;

    void setVideoSink(QVideoSink *sink);
    QVideoSink *videoSink() const;

    void setAudioOutput(QAudioOutput *output);
    QAudioOutput *audioOutput() const;

signals:
    /*!
        This signal is emitted when the audio input changes.
    */
    void audioInputChanged() { emitSignal(&QMediaCaptureSession::audioInputChanged); }
    /*!
        This signal is emitted when the camera changes.
    */
    void cameraChanged() { emitSignal(&QMediaCaptureSession::cameraChanged); }
    /*!
        This signal is emitted when the image capture changes.
    */
    void imageCaptureChanged() { emitSignal(&QMediaCaptureSession::imageCaptureChanged); }
    /*!
        This signal is emitted when the recorder changes.
    */
    void recorderChanged() { emitSignal(&QMediaCaptureSession::recorderChanged); }
    /*!
        This signal is emitted when the video output changes.
    */
    void videoOutputChanged() { emitSignal(&QMediaCaptureSession::videoOutputChanged); }
    /*!
        This signal is emitted when the audio output changes.
    */
    void audioOutputChanged() { emitSignal(&QMediaCaptureSession::audioOutputChanged); }

private:
    friend class QMediaCaptureSessionPrivate;
    friend class QCameraPrivate;
    friend class QImageCapturePrivate;
    friend class QMediaRecorderPrivate;
    std::unique_ptr<QMediaCaptureSessionPrivate> d;
};
