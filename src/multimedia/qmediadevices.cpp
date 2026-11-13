// MIT License - see LICENSE
#include <QtMultimedia/qmediadevices.h>

#include "qaudiobackend_p.h"

#include <QtCore/qfilesystemwatcher.h>
#include <QtCore/qtimer.h>

/*!
    \module NtMultimedia
    \title Nt Multimedia C++ Classes
    \brief Classes for playing and recording audio.
    \cmaketarget Nt::Multimedia

    Nt Multimedia has the audio part of Qt Multimedia: QMediaDevices lists the devices,
    QAudioSink and QAudioSource stream raw samples, QSoundEffect plays short sounds with low
    latency, and QMediaPlayer with QAudioOutput plays files. On Linux it uses ALSA, loaded at run
    time, which reaches PipeWire and PulseAudio through ALSA's \c default device.

    See \l{Using Nt Multimedia} for a walk through.
*/

/*!
    \page nt-multimedia.html
    \title Using Nt Multimedia
    \brief Playing sound effects and WAV files, streaming raw audio in and out, and choosing devices.

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

    \snippet multimedia/audioplayer.cpp effect

    \section1 Playing Files

    QMediaPlayer plays a file through a QAudioOutput, which sets the device and volume, and reports
    its position and state:

    \snippet multimedia/audioplayer.cpp player

    \section1 Raw Audio

    QAudioSink plays samples in a QAudioFormat. In pull mode it reads them from a QIODevice; in push
    mode start() returns a device to write them to. QAudioSource records the same way:

    \snippet multimedia/audiodevices.cpp tone

    \section1 Devices

    QMediaDevices lists the inputs and outputs, and tells when they change:

    \snippet multimedia/audiodevices.cpp list

    \section1 What Is Missing

    Nt Multimedia has no video: there is no QCamera, QVideoSink, QVideoFrame, QMediaCaptureSession,
    QMediaRecorder or QImageCapture. QMediaPlayer and QSoundEffect read only WAV files (PCM and
    floating point), and there is no QAudioDecoder or QAudioBuffer. Only Linux has a backend; on
    other platforms there are no devices.
*/

class QMediaDevicesPrivate {
public:
    explicit QMediaDevicesPrivate(QMediaDevices *owner) : q(owner) {
        inputs = ids(QAudioDevice::Input);
        outputs = ids(QAudioDevice::Output);
        const QString dir = QtMultimediaPrivate::audioBackend()->deviceDirectory();
        if (!dir.isEmpty()) {
            watcher.addPath(dir);
            // Devices settle a moment after their node appears.
            settle.setSingleShot(true);
            settle.setInterval(200);
            QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged, &settle,
                             [this](const QString &) { settle.start(); });
            QObject::connect(&settle, &QTimer::timeout, &settle, [this] { check(); });
        }
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
        const bool inputsChanged = newInputs != inputs;
        const bool outputsChanged = newOutputs != outputs;
        inputs = newInputs;
        outputs = newOutputs;
        if (inputsChanged)
            emit q->audioInputsChanged();
        if (outputsChanged)
            emit q->audioOutputsChanged();
    }

    QMediaDevices *q;
    QList<QByteArray> inputs;
    QList<QByteArray> outputs;
    QFileSystemWatcher watcher;
    QTimer settle;
};

/*!
    \class QMediaDevices
    \inmodule NtMultimedia
    \brief The QMediaDevices class lists the audio devices and reports when they change.

    The static functions list the devices at the time of the call. An instance also emits
    audioInputsChanged() and audioOutputsChanged() when a device is plugged in or removed:

    \snippet multimedia/audiodevices.cpp list

    \differences
    There are no video inputs: no \c videoInputs(), \c defaultVideoInput() or
    \c videoInputsChanged(), since there is no QCameraDevice. On Linux changes are noticed by
    watching \c{/dev/snd}, so a change of the default device inside PipeWire or PulseAudio is not
    reported.
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

#include "moc_qmediadevices.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
