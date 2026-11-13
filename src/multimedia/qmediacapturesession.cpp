// MIT License - see LICENSE
#include <QtMultimedia/qmediacapturesession.h>

#include "qmediacapture_p.h"

void QMediaCaptureSessionPrivate::deliverFrame(const QVideoFrame &frame) {
    if (videoSink)
        videoSink->setVideoFrame(frame);
    if (imageCapture)
        imageCapture->d->onFrame(frame);
}

void QMediaCaptureSessionPrivate::cameraActiveChanged() {
    if (imageCapture)
        imageCapture->d->updateReady();
}

void QMediaCaptureSessionPrivate::updateMonitor() {
    monitorSource.reset();
    monitorSink.reset();
    if (!audioInput || !audioOutput || audioInput->device().isNull() || audioOutput->device().isNull())
        return;
    QAudioFormat format = audioInput->device().preferredFormat();
    if (format.sampleRate() <= 0)
        format.setSampleRate(48000);
    if (format.channelCount() <= 0)
        format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    // Small buffers, so that what is heard follows what is said closely.
    auto sink = std::make_unique<QAudioSink>(audioOutput->device(), format);
    sink->setBufferSize(format.bytesForDuration(60000));
    QIODevice *out = sink->start();
    auto source = std::make_unique<QAudioSource>(audioInput->device(), format);
    source->setBufferSize(format.bytesForDuration(40000));
    QIODevice *in = source->start();
    if (!out || !in)
        return;
    QObject::connect(in, &QIODevice::readyRead, sink.get(), [in, out] {
        const QByteArray samples = in->readAll();
        out->write(samples); // what does not fit is dropped, rather than adding delay
    });
    monitorSource = std::move(source);
    monitorSink = std::move(sink);
    applyMonitorVolume();
}

void QMediaCaptureSessionPrivate::applyMonitorVolume() {
    if (monitorSource && audioInput)
        monitorSource->setVolume(audioInput->isMuted() ? 0.0 : qreal(audioInput->volume()));
    if (monitorSink && audioOutput)
        monitorSink->setVolume(audioOutput->isMuted() ? 0.0 : qreal(audioOutput->volume()));
}

/*!
    \class QMediaCaptureSession
    \inmodule NtMultimedia
    \brief The QMediaCaptureSession class connects a camera and a microphone to what shows, captures
    or records them.

    A session ties a QCamera to a QVideoSink that receives its frames and a QImageCapture that takes
    pictures, and a QAudioInput to a QMediaRecorder that records it:

    \snippet multimedia/camera.cpp camera

    A camera, image capture or recorder is in one session at a time; putting it in another takes it
    out of the first. The session does not own them.

    \differences
    setVideoOutput() sends frames to a QVideoSink, or to the QVideoSink child of an object such as
    QVideoWidget; there is no QML video item. The recorder records audio only.
    \enddifferences

    \sa QCamera, QImageCapture, QMediaRecorder, QAudioInput, QVideoSink
*/

/*!
    Constructs an empty session with the given \a parent.
*/
QMediaCaptureSession::QMediaCaptureSession(QObject *parent)
    : QObject(parent), d(std::make_unique<QMediaCaptureSessionPrivate>(this)) {}

/*!
    Destroys the session. The objects in it are left alone, out of any session.
*/
QMediaCaptureSession::~QMediaCaptureSession() {
    if (d->camera)
        d->camera->d->session = nullptr;
    if (d->imageCapture)
        d->imageCapture->d->session = nullptr;
    if (d->recorder) {
        d->recorder->stop();
        d->recorder->d->session = nullptr;
    }
}

/*!
    Returns the audio input, or \c nullptr.
*/
QAudioInput *QMediaCaptureSession::audioInput() const { return d->audioInput.data(); }

/*!
    Sets the audio input that the recorder records, and that is played back on the audio output, to
    \a input. The recorder uses it from its next QMediaRecorder::record().
*/
void QMediaCaptureSession::setAudioInput(QAudioInput *input) {
    if (input == d->audioInput)
        return;
    if (d->audioInput)
        disconnect(d->audioInput.data(), nullptr, this, nullptr);
    d->audioInput = input;
    if (input) {
        connect(input, &QAudioInput::deviceChanged, this, [this] { d->updateMonitor(); });
        connect(input, &QAudioInput::volumeChanged, this, [this](float) { d->applyMonitorVolume(); });
        connect(input, &QAudioInput::mutedChanged, this, [this](bool) { d->applyMonitorVolume(); });
        connect(input, &QObject::destroyed, this, [this] {
            d->audioInput = nullptr; // the QAudioInput part is already gone
            d->updateMonitor();
            emit audioInputChanged();
        });
    }
    d->updateMonitor();
    emit audioInputChanged();
}

/*!
    Returns the camera, or \c nullptr.
*/
QCamera *QMediaCaptureSession::camera() const { return d->camera.data(); }

/*!
    Sets the camera whose frames go to the video sink and the image capture to \a camera, taking it
    out of any other session.
*/
void QMediaCaptureSession::setCamera(QCamera *camera) {
    if (camera == d->camera)
        return;
    if (d->camera)
        d->camera->d->session = nullptr;
    if (camera && camera->d->session && camera->d->session != this)
        camera->d->session->setCamera(nullptr);
    d->camera = camera;
    if (camera)
        camera->d->session = this;
    emit cameraChanged();
    d->cameraActiveChanged();
}

/*!
    Returns the image capture, or \c nullptr.
*/
QImageCapture *QMediaCaptureSession::imageCapture() { return d->imageCapture.data(); }

/*!
    Sets the image capture that takes pictures from the camera to \a imageCapture, taking it out of
    any other session.
*/
void QMediaCaptureSession::setImageCapture(QImageCapture *imageCapture) {
    if (imageCapture == d->imageCapture)
        return;
    QImageCapture *old = d->imageCapture.data();
    if (old)
        old->d->session = nullptr;
    if (imageCapture && imageCapture->d->session && imageCapture->d->session != this)
        imageCapture->d->session->setImageCapture(nullptr);
    d->imageCapture = imageCapture;
    if (imageCapture)
        imageCapture->d->session = this;
    if (old)
        old->d->updateReady();
    emit imageCaptureChanged();
    d->cameraActiveChanged();
}

/*!
    Returns the recorder, or \c nullptr.
*/
QMediaRecorder *QMediaCaptureSession::recorder() { return d->recorder.data(); }

/*!
    Sets the recorder that records the audio input to \a recorder, taking it out of any other
    session. A recorder that is taken out of the session stops.
*/
void QMediaCaptureSession::setRecorder(QMediaRecorder *recorder) {
    if (recorder == d->recorder)
        return;
    if (d->recorder) {
        d->recorder->stop();
        d->recorder->d->session = nullptr;
    }
    if (recorder && recorder->d->session && recorder->d->session != this)
        recorder->d->session->setRecorder(nullptr);
    d->recorder = recorder;
    if (recorder)
        recorder->d->session = this;
    emit recorderChanged();
}

/*!
    Sets the object that shows the camera's frames to \a output: a QVideoSink, or an object that has
    one as a direct child, such as QVideoWidget. Other objects are stored but receive nothing.

    \sa setVideoSink()
*/
void QMediaCaptureSession::setVideoOutput(QObject *output) {
    if (output == d->videoOutput)
        return;
    d->videoOutput = output;
    QVideoSink *sink = qobject_cast<QVideoSink *>(output);
    if (!sink && output) // a QVideoWidget, or any object that keeps its sink as a child
        sink = output->findChild<QVideoSink *>(QString(), Qt::FindDirectChildrenOnly);
    d->videoSink = sink;
    emit videoOutputChanged();
}

/*!
    Returns the video output, or \c nullptr.
*/
QObject *QMediaCaptureSession::videoOutput() const { return d->videoOutput.data(); }

/*!
    Sets the sink that receives the camera's frames to \a sink.
*/
void QMediaCaptureSession::setVideoSink(QVideoSink *sink) {
    if (sink == d->videoSink && sink == d->videoOutput)
        return;
    d->videoSink = sink;
    d->videoOutput = sink;
    emit videoOutputChanged();
}

/*!
    Returns the video sink, or \c nullptr.
*/
QVideoSink *QMediaCaptureSession::videoSink() const { return d->videoSink.data(); }

/*!
    Sets the output that the audio input is played back on to \a output. While the session has both
    an input and an output, what the input records is heard on the output, with the volume and mute
    of each, about 100 ms later.
*/
void QMediaCaptureSession::setAudioOutput(QAudioOutput *output) {
    if (output == d->audioOutput)
        return;
    if (d->audioOutput)
        disconnect(d->audioOutput.data(), nullptr, this, nullptr);
    d->audioOutput = output;
    if (output) {
        connect(output, &QAudioOutput::deviceChanged, this, [this] { d->updateMonitor(); });
        connect(output, &QAudioOutput::volumeChanged, this, [this](float) { d->applyMonitorVolume(); });
        connect(output, &QAudioOutput::mutedChanged, this, [this](bool) { d->applyMonitorVolume(); });
        connect(output, &QObject::destroyed, this, [this] {
            d->audioOutput = nullptr;
            d->updateMonitor();
            emit audioOutputChanged();
        });
    }
    d->updateMonitor();
    emit audioOutputChanged();
}

/*!
    Returns the audio output, or \c nullptr.
*/
QAudioOutput *QMediaCaptureSession::audioOutput() const { return d->audioOutput.data(); }

#include "moc_qmediacapturesession.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
