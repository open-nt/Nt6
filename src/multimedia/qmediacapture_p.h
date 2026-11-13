// MIT License - see LICENSE
//
// The private parts of the capture classes, which call each other: a QCamera hands its frames to
// the QMediaCaptureSession it is in, which passes them to the session's QVideoSink and QImageCapture;
// a QMediaRecorder records from the session's QAudioInput.
#pragma once

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qfile.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>
#include <QtMultimedia/qaudioinput.h>
#include <QtMultimedia/qaudiooutput.h>
#include <QtMultimedia/qaudiosink.h>
#include <QtMultimedia/qaudiosource.h>
#include <QtMultimedia/qcamera.h>
#include <QtMultimedia/qimagecapture.h>
#include <QtMultimedia/qmediacapturesession.h>
#include <QtMultimedia/qmediarecorder.h>
#include <QtMultimedia/qvideosink.h>

#include "qvideobackend_p.h"

#include <atomic>
#include <memory>
#include <mutex>

class QMediaCaptureSessionPrivate {
public:
    explicit QMediaCaptureSessionPrivate(QMediaCaptureSession *owner) : q(owner) {}

    // Called by the camera, on the session's thread, with each new frame.
    void deliverFrame(const QVideoFrame &frame);
    // Called when the camera starts or stops.
    void cameraActiveChanged();
    // Plays the audio input on the audio output while the session has both.
    void updateMonitor();
    void applyMonitorVolume();

    QMediaCaptureSession *q;
    QPointer<QAudioInput> audioInput;
    QPointer<QAudioOutput> audioOutput;
    QPointer<QCamera> camera;
    QPointer<QImageCapture> imageCapture;
    QPointer<QMediaRecorder> recorder;
    QPointer<QObject> videoOutput;
    QPointer<QVideoSink> videoSink;

    std::unique_ptr<QAudioSource> monitorSource;
    std::unique_ptr<QAudioSink> monitorSink;
};

class QCameraPrivate {
public:
    explicit QCameraPrivate(QCamera *owner) : q(owner) {}

    void setError(QCamera::Error e, const QString &message);
    bool startStream();
    void stopStream();
    // Takes the newest frame from the stream's thread; runs on the camera's thread.
    void deliver();
    void notifySession();

    QCamera *q;
    QCameraDevice device;
    QCameraFormat format;
    bool active = false;
    QCamera::Error error = QCamera::NoError;
    QString errorString;
    QMediaCaptureSession *session = nullptr;
    std::unique_ptr<QtMultimediaPrivate::CameraStream> stream;

    std::mutex frameMutex;
    QVideoFrame latest;
    std::atomic<bool> deliveryPending{false};
    std::atomic<quint64> generation{0}; // drops frames and errors of streams that have been stopped
};

class QImageCapturePrivate {
public:
    explicit QImageCapturePrivate(QImageCapture *owner) : q(owner) {}

    struct Request {
        int id;
        bool toFile;
        QString fileName;
    };

    void onFrame(const QVideoFrame &frame);
    void updateReady();
    void setError(int id, QImageCapture::Error e, const QString &message);
    int enqueue(bool toFile, const QString &location);

    QImageCapture *q;
    QMediaCaptureSession *session = nullptr;
    QImageCapture::Error error = QImageCapture::NoError;
    QString errorString;
    QImageCapture::FileFormat fileFormat = QImageCapture::UnspecifiedFormat;
    QImageCapture::Quality quality = QImageCapture::NormalQuality;
    QSize resolution;
    bool ready = false;
    int nextId = 1;
    QList<Request> requests;
};

class QMediaRecorderPrivate {
public:
    explicit QMediaRecorderPrivate(QMediaRecorder *owner) : q(owner) {}

    void setState(QMediaRecorder::RecorderState s);
    void setError(QMediaRecorder::Error e, const QString &message);
    QString resolveLocation(QString *message) const;
    void finish();
    void applyVolume();
    qint64 liveDuration() const;

    QMediaRecorder *q;
    QMediaCaptureSession *session = nullptr;
    QUrl outputLocation;
    QUrl actualLocation;
    QMediaRecorder::RecorderState state = QMediaRecorder::StoppedState;
    QMediaRecorder::Error error = QMediaRecorder::NoError;
    QString errorString;
    QMediaFormat mediaFormat;
    QMediaRecorder::EncodingMode encodingMode = QMediaRecorder::ConstantQualityEncoding;
    QMediaRecorder::Quality quality = QMediaRecorder::NormalQuality;
    QSize videoResolution;
    qreal videoFrameRate = 0.0;
    int videoBitRate = -1;
    int audioBitRate = -1;
    int audioChannelCount = -1;
    int audioSampleRate = -1;

    QMediaMetaData metaData;
    QMediaMetaData recordingMetaData; // what the header of the file being recorded holds
    qint64 headerSize = 44;
    std::unique_ptr<QFile> file;
    std::unique_ptr<QAudioSource> source;
    QAudioFormat format;
    qint64 duration = 0;
    QTimer timer;
};
