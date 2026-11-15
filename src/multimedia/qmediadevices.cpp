// MIT License - see LICENSE
#include <QtMultimedia/qmediadevices.h>

#include "qaudiobackend_p.h"
#include "qvideobackend_p.h"

#include <QtCore/qfilesystemwatcher.h>
#include <QtCore/qtimer.h>

/*!
    \module NtMultimedia
    \title Nt Multimedia C++ Classes
    \brief Classes for audio and cameras.
    \cmaketarget Nt::Multimedia

    Nt Multimedia has the audio and camera parts of Qt Multimedia: QMediaDevices lists the devices,
    QAudioSink and QAudioSource stream raw samples, QSoundEffect plays short sounds with low
    latency, QMediaPlayer with QAudioOutput plays files, QAudioDecoder decodes them, and
    QMediaCaptureSession connects a QCamera, a QAudioInput, a QVideoSink, a QImageCapture and a
    QMediaRecorder. On Linux it uses ALSA, loaded at run time, which reaches PipeWire and PulseAudio
    through ALSA's \c default device, and Video4Linux2 for cameras.

    See \l{Using Nt Multimedia} for a walk through.
*/

/*!
    \page nt-multimedia.html
    \title Using Nt Multimedia
    \brief Playing and recording WAV files, sound effects, raw audio, cameras and pictures, and choosing devices.

    Link \c Nt::Multimedia (or \c Nt6::Multimedia, or \c Qt6::Multimedia, from an installed package)
    and include the classes you use:

    \code
    find_package(Nt6 6 REQUIRED)
    add_executable(app main.cpp)
    target_link_libraries(app PRIVATE Nt6::Multimedia)
    \endcode

    Every class needs a running event loop: streams are fed and drained from a timer on the thread
    that owns them, while a worker thread of their own talks to the device.

    \section1 Sound Effects

    QSoundEffect loads a WAV file once and plays it with little latency, as often as it is asked to:

    \code
    auto *effect = new QSoundEffect(qApp);
    effect->setSource(QUrl::fromLocalFile(file));
    effect->setLoopCount(times);
    effect->setVolume(0.8f);
    QObject::connect(effect, &QSoundEffect::playingChanged, effect, [effect] {
        if (!effect->isPlaying())
            QCoreApplication::quit();
    });
    effect->play();
    \endcode

    \section1 Playing Files

    QMediaPlayer plays a file through a QAudioOutput, which sets the device and volume, and reports
    its position and state:

    \code
    auto *player = new QMediaPlayer(qApp);
    auto *output = new QAudioOutput(qApp);
    output->setVolume(0.5f);
    player->setAudioOutput(output);
    QObject::connect(player, &QMediaPlayer::positionChanged, [player](qint64 position) {
        std::printf("\r%lld / %lld ms", static_cast<long long>(position), static_cast<long long>(player->duration()));
        std::fflush(stdout);
    });
    QObject::connect(player, &QMediaPlayer::mediaStatusChanged, [](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia)
            QCoreApplication::quit();
    });
    player->setSource(QUrl::fromLocalFile(file));
    player->play();
    \endcode

    QMediaPlayer::setPlaybackRate() plays faster or slower; the pitch changes with the speed.

    \section1 Raw Audio

    QAudioSink plays samples in a QAudioFormat. In pull mode it reads them from a QIODevice; in push
    mode start() returns a device to write them to. QAudioSource records the same way:

    \code
    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    QByteArray samples(format.bytesForDuration(1000000), Qt::Uninitialized);
    auto *data = reinterpret_cast<qint16 *>(samples.data());
    for (int i = 0; i < format.sampleRate(); ++i)
        data[i] = qint16(8000 * std::sin(2 * M_PI * 440 * i / format.sampleRate()));

    QBuffer buffer(&samples);
    buffer.open(QIODevice::ReadOnly);
    QAudioSink sink(format);
    QObject::connect(&sink, &QAudioSink::stateChanged, [&](QtAudio::State state) {
        if (state == QtAudio::IdleState) { // played to the end
            sink.stop();
            if (!watch)
                QCoreApplication::quit();
        }
    });
    sink.start(&buffer);
    \endcode

    \section1 Devices

    QMediaDevices lists the inputs and outputs, and tells when they change:

    \code
    for (const QAudioDevice &device : QMediaDevices::audioOutputs()) {
        std::printf("output %s%s: %s\n", device.isDefault() ? "(default) " : "", device.id().constData(),
                    qPrintable(device.description()));
    }
    for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
        std::printf("input  %s%s: %s\n", device.isDefault() ? "(default) " : "", device.id().constData(),
                    qPrintable(device.description()));
    }
    \endcode

    \section1 Cameras

    A QCamera in a QMediaCaptureSession streams frames to a QVideoSink. Each QVideoFrame can be mapped
    to read its planes, or turned into a QImage:

    \code
    QMediaCaptureSession session;
    QCamera camera;
    QVideoSink sink;
    QImageCapture imageCapture;
    session.setCamera(&camera);
    session.setVideoSink(&sink);
    session.setImageCapture(&imageCapture);
    QObject::connect(&camera, &QCamera::errorOccurred, [](QCamera::Error, const QString &message) {
        std::printf("camera error: %s\n", qPrintable(message));
        QCoreApplication::exit(1);
    });
    camera.start();
    \endcode

    \code
    int frames = 0;
    QObject::connect(&sink, &QVideoSink::videoFrameChanged, [&frames](const QVideoFrame &frame) {
        if (frames++ == 0) {
            QVideoFrame mapped = frame;
            mapped.map(QVideoFrame::ReadOnly);
            std::printf("first frame: %dx%d %s, %d plane(s), %d bytes a line\n", frame.width(), frame.height(),
                        qPrintable(QVideoFrameFormat::pixelFormatToString(frame.pixelFormat())), mapped.planeCount(),
                        mapped.bytesPerLine(0));
            mapped.unmap();
        }
    });
    \endcode

    QMediaDevices::videoInputs() lists the cameras and QCameraDevice::videoFormats() what each can
    stream; QCamera::setCameraFormat() chooses one.

    \section1 Showing Video

    QVideoWidget, in Nt Multimedia Widgets (\c Nt::MultimediaWidgets), shows the frames in a window:

    \code
    auto *video = new QVideoWidget;
    QMediaCaptureSession session;
    QCamera camera;
    session.setCamera(&camera);
    session.setVideoOutput(video);
    camera.start();
    \endcode

    \section1 Taking Pictures

    A QImageCapture in the same session takes the next frame as a picture, and can save it as a JPEG
    or PNG file:

    \code
    QObject::connect(&imageCapture, &QImageCapture::imageSaved,
                     [](int, const QString &fileName) { std::printf("saved %s\n", qPrintable(fileName)); });
    QTimer::singleShot(seconds * 1000, &imageCapture,
                       [&imageCapture] { imageCapture.captureToFile(QDir::currentPath()); });
    \endcode

    \section1 Recording Audio

    A QMediaRecorder records the session's QAudioInput to a WAV file:

    \code
    QMediaCaptureSession session;
    QAudioInput input;
    QMediaRecorder recorder;
    session.setAudioInput(&input);
    session.setRecorder(&recorder);
    recorder.setOutputLocation(QUrl::fromLocalFile(args.at(1)));
    QObject::connect(&recorder, &QMediaRecorder::durationChanged, [](qint64 ms) {
        std::printf("\r%lld ms", static_cast<long long>(ms));
        std::fflush(stdout);
    });
    recorder.record();
    \endcode

    QMediaRecorder::setMetaData() writes a title, artist, date and other text tags into the file;
    QMediaPlayer::metaData() reads them back.

    \section1 Hearing the Microphone

    A session with both a QAudioInput and a QAudioOutput plays the input back on the output, with
    about a tenth of a second of delay:

    \code
    QAudioInput microphone;
    QAudioOutput speakers;
    auto *monitor = new QCheckBox(QStringLiteral("Hear the microphone"));
    QObject::connect(monitor, &QCheckBox::toggled, &session, [&](bool on) {
        session.setAudioInput(on ? &microphone : nullptr);
        session.setAudioOutput(on ? &speakers : nullptr);
    });
    \endcode

    \section1 Decoding Audio

    QAudioDecoder reads a WAV file into QAudioBuffer blocks, converted to the sample format and
    channel count asked for:

    \code
    QAudioDecoder decoder;
    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Float);
    decoder.setAudioFormat(format);
    decoder.setSource(QUrl::fromLocalFile(app.arguments().at(1)));
    float peak = 0.0f;
    qint64 frames = 0;
    QObject::connect(&decoder, &QAudioDecoder::bufferReady, [&] {
        const QAudioBuffer buffer = decoder.read();
        const auto *samples = buffer.constData<QAudioBuffer::F32M>();
        for (qsizetype i = 0; i < buffer.frameCount(); ++i)
            peak = std::max(peak, std::fabs(samples[i].channels[0]));
        frames += buffer.frameCount();
    });
    QObject::connect(&decoder, &QAudioDecoder::finished, [&] {
        std::printf("%lld frames, %lld ms, peak %.3f\n", static_cast<long long>(frames),
                    static_cast<long long>(decoder.duration()), double(peak));
        QCoreApplication::quit();
    });
    QObject::connect(&decoder, qOverload<QAudioDecoder::Error>(&QAudioDecoder::error), [&](QAudioDecoder::Error) {
        std::printf("cannot decode: %s\n", qPrintable(decoder.errorString()));
        QCoreApplication::exit(1);
    });
    decoder.start();
    \endcode

    \section1 What Is Missing

    Nt reads and writes only WAV audio, and encodes and decodes no video: QMediaPlayer, QSoundEffect
    and QAudioDecoder play WAV files, QMediaRecorder records audio to WAV, and video can only be
    captured from a camera, not played or recorded. QVideoWidget draws frames without the GPU, and
    there is no QML video item. Cameras have no focus, zoom or exposure controls. Only Linux
    has a backend (ALSA and Video4Linux2); on other platforms there are no devices.
*/

class QMediaDevicesPrivate {
public:
    explicit QMediaDevicesPrivate(QMediaDevices *owner) : q(owner) {
        inputs = ids(QAudioDevice::Input);
        outputs = ids(QAudioDevice::Output);
        cameras = cameraIds();
        // Devices settle a moment after their node appears.
        settle.setSingleShot(true);
        settle.setInterval(200);
        QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged, &settle,
                         [this](const QString &) { settle.start(); });
        QObject::connect(&settle, &QTimer::timeout, &settle, [this] { check(); });
        for (const QString &dir : {QtMultimediaPrivate::audioBackend()->deviceDirectory(),
                                   QtMultimediaPrivate::videoBackend()->deviceDirectory()}) {
            if (!dir.isEmpty() && !watcher.directories().contains(dir))
                watcher.addPath(dir);
        }
    }

    static QList<QByteArray> cameraIds() {
        QList<QByteArray> list;
        for (const QCameraDevice &device : QtMultimediaPrivate::videoBackend()->videoInputs())
            list.append(device.id());
        return list;
    }

    static QList<QByteArray> ids(QAudioDevice::Mode mode) {
        QList<QByteArray> list;
        for (const QAudioDevice &device : QtMultimediaPrivate::audioBackend()->devices(mode))
            list.append(device.id());
        return list;
    }

    void check() {
        const QList<QByteArray> newInputs = ids(QAudioDevice::Input);
        const QList<QByteArray> newOutputs = ids(QAudioDevice::Output);
        const QList<QByteArray> newCameras = cameraIds();
        const bool inputsChanged = newInputs != inputs;
        const bool outputsChanged = newOutputs != outputs;
        const bool camerasChanged = newCameras != cameras;
        inputs = newInputs;
        outputs = newOutputs;
        cameras = newCameras;
        if (inputsChanged)
            emit q->audioInputsChanged();
        if (outputsChanged)
            emit q->audioOutputsChanged();
        if (camerasChanged)
            emit q->videoInputsChanged();
    }

    QMediaDevices *q;
    QList<QByteArray> inputs;
    QList<QByteArray> outputs;
    QList<QByteArray> cameras;
    QFileSystemWatcher watcher;
    QTimer settle;
};

/*!
    \class QMediaDevices
    \inmodule NtMultimedia
    \brief The QMediaDevices class lists the audio devices and cameras, and reports when they change.

    The static functions list the devices at the time of the call. An instance also emits
    audioInputsChanged(), audioOutputsChanged() and videoInputsChanged() when a device is plugged in
    or removed:

    \code
    for (const QAudioDevice &device : QMediaDevices::audioOutputs()) {
        std::printf("output %s%s: %s\n", device.isDefault() ? "(default) " : "", device.id().constData(),
                    qPrintable(device.description()));
    }
    for (const QAudioDevice &device : QMediaDevices::audioInputs()) {
        std::printf("input  %s%s: %s\n", device.isDefault() ? "(default) " : "", device.id().constData(),
                    qPrintable(device.description()));
    }
    \endcode

    \differences
    On Linux changes are noticed by watching \c{/dev/snd} and \c{/dev}, so a change of the default
    device inside PipeWire or PulseAudio is not reported.
    \enddifferences

    \sa QAudioDevice
*/

/*!
    Constructs an object that reports device changes, with the given \a parent.
*/
QMediaDevices::QMediaDevices(QObject *parent) : QObject(parent), d(std::make_unique<QMediaDevicesPrivate>(this)) {}

/*!
    Destroys the object.
*/
QMediaDevices::~QMediaDevices() = default;

/*!
    Returns the audio input devices, the default one first.
*/
QList<QAudioDevice> QMediaDevices::audioInputs() {
    return QtMultimediaPrivate::audioBackend()->devices(QAudioDevice::Input);
}

/*!
    Returns the audio output devices, the default one first.
*/
QList<QAudioDevice> QMediaDevices::audioOutputs() {
    return QtMultimediaPrivate::audioBackend()->devices(QAudioDevice::Output);
}

namespace {

QAudioDevice defaultOf(const QList<QAudioDevice> &devices) {
    for (const QAudioDevice &device : devices)
        if (device.isDefault())
            return device;
    return devices.isEmpty() ? QAudioDevice() : devices.first();
}

} // namespace

/*!
    Returns the default audio input, or a null device if there is none.
*/
QAudioDevice QMediaDevices::defaultAudioInput() { return defaultOf(audioInputs()); }

/*!
    Returns the default audio output, or a null device if there is none.
*/
QAudioDevice QMediaDevices::defaultAudioOutput() { return defaultOf(audioOutputs()); }

/*!
    Returns the cameras, the default one first.
*/
QList<QCameraDevice> QMediaDevices::videoInputs() { return QtMultimediaPrivate::videoBackend()->videoInputs(); }

/*!
    Returns the default camera, or a null device if there is none.
*/
QCameraDevice QMediaDevices::defaultVideoInput() {
    const QList<QCameraDevice> cameras = videoInputs();
    for (const QCameraDevice &camera : cameras)
        if (camera.isDefault())
            return camera;
    return cameras.isEmpty() ? QCameraDevice() : cameras.first();
}

#include "moc_qmediadevices.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
