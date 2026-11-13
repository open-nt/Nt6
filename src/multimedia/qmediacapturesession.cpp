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
    Only QVideoSink works as a video output; setVideoOutput() accepts other objects but sends them no
    frames. setAudioOutput() is stored, but the input is not played back. The recorder records audio
    only.
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
    Sets the audio input that the recorder records to \a input. It takes effect at the next
    QMediaRecorder::record().
*/
void QMediaCaptureSession::setAudioInput(QAudioInput *input) {
    if (input == d->audioInput)
        return;
    d->audioInput = input;
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
    Sets the object that shows the camera's frames to \a output. A QVideoSink receives them; other
    objects are stored but receive nothing.

    \sa setVideoSink()
*/
void QMediaCaptureSession::setVideoOutput(QObject *output) {
    if (output == d->videoOutput)
        return;
    d->videoOutput = output;
    d->videoSink = qobject_cast<QVideoSink *>(output);
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
    Sets the audio output to \a output. Nt stores it but does not play the input back.
*/
void QMediaCaptureSession::setAudioOutput(QAudioOutput *output) {
    if (output == d->audioOutput)
        return;
    d->audioOutput = output;
    emit audioOutputChanged();
}

/*!
    Returns the audio output, or \c nullptr.
*/
QAudioOutput *QMediaCaptureSession::audioOutput() const { return d->audioOutput.data(); }

#include "moc_qmediacapturesession.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
