// MIT License - see LICENSE
#include <QtMultimedia/qmediarecorder.h>

#include "qmediacapture_p.h"
#include "qwavedecoder_p.h"

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qstandardpaths.h>

void QMediaRecorderPrivate::setState(QMediaRecorder::RecorderState s) {
    if (s == state)
        return;
    state = s;
    emit q->recorderStateChanged(s);
}

void QMediaRecorderPrivate::setError(QMediaRecorder::Error e, const QString &message) {
    error = e;
    errorString = message;
    emit q->errorChanged();
    if (e != QMediaRecorder::NoError)
        emit q->errorOccurred(e, message);
}

QString QMediaRecorderPrivate::resolveLocation(QString *message) const {
    QString path = outputLocation.isLocalFile() ? outputLocation.toLocalFile() : outputLocation.path();
    if (!outputLocation.isEmpty() && !outputLocation.isLocalFile() && !outputLocation.scheme().isEmpty()) {
        *message = QStringLiteral("only local files can be recorded to");
        return QString();
    }
    QString dir;
    if (path.isEmpty()) {
        dir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
        if (dir.isEmpty())
            dir = QDir::currentPath();
    } else if (QFileInfo(path).isDir()) {
        dir = path;
    } else {
        if (QFileInfo(path).suffix().isEmpty())
            path += QStringLiteral(".wav");
        return path;
    }
    for (int n = 1; n < 100000; ++n) {
        const QString candidate = QDir(dir).filePath(QStringLiteral("record_%1.wav").arg(n, 4, 10, QLatin1Char('0')));
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
    *message = QStringLiteral("no free file name in %1").arg(dir);
    return QString();
}

qint64 QMediaRecorderPrivate::liveDuration() const { return source ? source->processedUSecs() / 1000 : duration; }

void QMediaRecorderPrivate::applyVolume() {
    if (!source)
        return;
    QAudioInput *input = session ? session->audioInput() : nullptr;
    source->setVolume(!input || input->isMuted() ? 0.0 : qreal(input->volume()));
}

// Stops recording and writes the final sizes into the header.
void QMediaRecorderPrivate::finish() {
    timer.stop();
    if (source) {
        QObject::disconnect(source.get(), nullptr, q, nullptr);
        source->stop();
        source.reset();
    }
    if (file) {
        file->flush();
        const qint64 bytes = std::max<qint64>(file->size() - headerSize, 0);
        duration = format.isValid() && format.bytesPerFrame() > 0
                       ? bytes / format.bytesPerFrame() * 1000 / format.sampleRate()
                       : 0;
        file->seek(0);
        file->write(QtMultimediaPrivate::waveHeader(format, bytes, recordingMetaData));
        file->close();
        file.reset();
    }
}

/*!
    \class QMediaRecorder
    \inmodule NtMultimedia
    \brief The QMediaRecorder class records the audio of a QMediaCaptureSession to a file.

    Put a recorder and a QAudioInput in a session, choose where to write, and record():

    \snippet multimedia/recorder.cpp record

    Without an output location, the recorder writes \c{record_0001.wav} and onwards in the music
    folder. actualLocation() is the file it writes.

    \differences
    Only audio is recorded, to WAV files with 16-bit samples; a camera in the session is not
    recorded. The video settings, bit rates, quality and encoding mode are stored but have no
    effect. Only text metadata is written.
    \enddifferences

    \sa QMediaCaptureSession, QAudioInput, QMediaFormat
*/

/*!
    \enum QMediaRecorder::Quality

    \value VeryLowQuality Very low quality.
    \value LowQuality Low quality.
    \value NormalQuality Normal quality.
    \value HighQuality High quality.
    \value VeryHighQuality Very high quality.
*/

/*!
    \enum QMediaRecorder::EncodingMode

    \value ConstantQualityEncoding Encode at a constant quality.
    \value ConstantBitRateEncoding Encode at a constant bit rate.
    \value AverageBitRateEncoding Encode at an average bit rate.
    \value TwoPassEncoding Encode in two passes.
*/

/*!
    \enum QMediaRecorder::RecorderState

    \value StoppedState Not recording.
    \value RecordingState Recording.
    \value PausedState Paused; record() goes on in the same file.
*/

/*!
    \enum QMediaRecorder::Error

    \value NoError Nothing went wrong.
    \value ResourceError There is nothing to record, or the input device cannot be opened.
    \value FormatError The media format cannot be written.
    \value OutOfSpaceError There is no room for the file. Nt does not report this.
    \value LocationNotWritable The output file cannot be written.
*/

/*!
    Constructs a recorder with the given \a parent.
*/
QMediaRecorder::QMediaRecorder(QObject *parent) : QObject(parent), d(std::make_unique<QMediaRecorderPrivate>(this)) {
    d->timer.setInterval(50);
    connect(&d->timer, &QTimer::timeout, this, [this] {
        const qint64 now = d->liveDuration();
        if (now != d->duration) {
            d->duration = now;
            emit durationChanged(now);
        }
    });
}

/*!
    Destroys the recorder, finishing the file it records.
*/
QMediaRecorder::~QMediaRecorder() {
    d->finish();
    if (d->session)
        d->session->setRecorder(nullptr);
}

/*!
    Returns \c true: Nt can always record audio.
*/
bool QMediaRecorder::isAvailable() const { return true; }

/*!
    Returns the location set with setOutputLocation().
*/
QUrl QMediaRecorder::outputLocation() const { return d->outputLocation; }

/*!
    Sets where to record to \a location: a local file, or a folder to make a file in. It takes
    effect at the next record().
*/
void QMediaRecorder::setOutputLocation(const QUrl &location) { d->outputLocation = location; }

/*!
    Returns the file being recorded, or last recorded.
*/
QUrl QMediaRecorder::actualLocation() const { return d->actualLocation; }

/*!
    Returns whether the recorder records.
*/
QMediaRecorder::RecorderState QMediaRecorder::recorderState() const { return d->state; }

/*!
    Returns the last error.
*/
QMediaRecorder::Error QMediaRecorder::error() const { return d->error; }

/*!
    Returns a description of the last error, or an empty string.
*/
QString QMediaRecorder::errorString() const { return d->errorString; }

/*!
    Returns how much has been recorded, in milliseconds.
*/
qint64 QMediaRecorder::duration() const { return d->state == StoppedState ? d->duration : d->liveDuration(); }

/*!
    Returns the media format to record in.
*/
QMediaFormat QMediaRecorder::mediaFormat() const { return d->mediaFormat; }

/*!
    Sets the media format to record in to \a format. Only Wave can be recorded.
*/
void QMediaRecorder::setMediaFormat(const QMediaFormat &format) {
    if (format == d->mediaFormat)
        return;
    d->mediaFormat = format;
    emit mediaFormatChanged();
    emit encoderSettingsChanged();
}

/*!
    Returns the encoding mode.
*/
QMediaRecorder::EncodingMode QMediaRecorder::encodingMode() const { return d->encodingMode; }

/*!
    Sets the encoding mode to \a mode. WAV files are not encoded, so it has no effect.
*/
void QMediaRecorder::setEncodingMode(EncodingMode mode) {
    if (mode == d->encodingMode)
        return;
    d->encodingMode = mode;
    emit encodingModeChanged();
    emit encoderSettingsChanged();
}

/*!
    Returns the quality.
*/
QMediaRecorder::Quality QMediaRecorder::quality() const { return d->quality; }

/*!
    Sets the quality to \a quality. WAV files are not encoded, so it has no effect.
*/
void QMediaRecorder::setQuality(Quality quality) {
    if (quality == d->quality)
        return;
    d->quality = quality;
    emit qualityChanged();
    emit encoderSettingsChanged();
}

/*!
    Returns the video resolution.
*/
QSize QMediaRecorder::videoResolution() const { return d->videoResolution; }

/*!
    Sets the video resolution to \a size. Nt records no video, so it has no effect.
*/
void QMediaRecorder::setVideoResolution(const QSize &size) {
    if (size == d->videoResolution)
        return;
    d->videoResolution = size;
    emit videoResolutionChanged();
    emit encoderSettingsChanged();
}

/*!
    \fn void QMediaRecorder::setVideoResolution(int width, int height)

    \overload

    Sets the video resolution to \a width by \a height.
*/

/*!
    Returns the video frame rate.
*/
qreal QMediaRecorder::videoFrameRate() const { return d->videoFrameRate; }

/*!
    Sets the video frame rate to \a frameRate. Nt records no video, so it has no effect.
*/
void QMediaRecorder::setVideoFrameRate(qreal frameRate) {
    if (frameRate == d->videoFrameRate)
        return;
    d->videoFrameRate = frameRate;
    emit videoFrameRateChanged();
    emit encoderSettingsChanged();
}

/*!
    Returns the video bit rate.
*/
int QMediaRecorder::videoBitRate() const { return d->videoBitRate; }

/*!
    Sets the video bit rate to \a bitRate. Nt records no video, so it has no effect.
*/
void QMediaRecorder::setVideoBitRate(int bitRate) {
    if (bitRate == d->videoBitRate)
        return;
    d->videoBitRate = bitRate;
    emit videoBitRateChanged();
    emit encoderSettingsChanged();
}

/*!
    Returns the audio bit rate.
*/
int QMediaRecorder::audioBitRate() const { return d->audioBitRate; }

/*!
    Sets the audio bit rate to \a bitRate. WAV files are not encoded, so it has no effect.
*/
void QMediaRecorder::setAudioBitRate(int bitRate) {
    if (bitRate == d->audioBitRate)
        return;
    d->audioBitRate = bitRate;
    emit audioBitRateChanged();
    emit encoderSettingsChanged();
}

/*!
    Returns the number of channels to record, or -1 for the input device's preferred number.
*/
int QMediaRecorder::audioChannelCount() const { return d->audioChannelCount; }

/*!
    Sets the number of channels to record to \a channels, or -1 for the device's preferred number.
*/
void QMediaRecorder::setAudioChannelCount(int channels) {
    if (channels == d->audioChannelCount)
        return;
    d->audioChannelCount = channels;
    emit audioChannelCountChanged();
    emit encoderSettingsChanged();
}

/*!
    Returns the sample rate to record at, or -1 for the input device's preferred rate.
*/
int QMediaRecorder::audioSampleRate() const { return d->audioSampleRate; }

/*!
    Sets the sample rate to record at to \a sampleRate, or -1 for the device's preferred rate.
*/
void QMediaRecorder::setAudioSampleRate(int sampleRate) {
    if (sampleRate == d->audioSampleRate)
        return;
    d->audioSampleRate = sampleRate;
    emit audioSampleRateChanged();
    emit encoderSettingsChanged();
}

/*!
    Returns the metadata to write into the next recording.
*/
QMediaMetaData QMediaRecorder::metaData() const { return d->metaData; }

/*!
    Sets the metadata to write into the recording to \a metaData. The text tags (title, artist,
    comment, description, genre, date, language, copyright, album, track number and composer) are
    written into the WAV file's \c LIST/INFO chunk when recording starts.
*/
void QMediaRecorder::setMetaData(const QMediaMetaData &metaData) {
    if (metaData == d->metaData)
        return;
    d->metaData = metaData;
    emit metaDataChanged();
}

/*!
    Adds the values of \a metaData to the metadata to write, replacing those of the same keys.
*/
void QMediaRecorder::addMetaData(const QMediaMetaData &metaData) {
    QMediaMetaData merged = d->metaData;
    for (QMediaMetaData::Key key : metaData.keys())
        merged.insert(key, metaData.value(key));
    setMetaData(merged);
}

/*!
    Returns the session the recorder is in, or \c nullptr.
*/
QMediaCaptureSession *QMediaRecorder::captureSession() const { return d->session; }

/*!
    Starts recording the session's audio input, or goes on after pause(). If recording cannot start,
    errorOccurred() is emitted and the recorder stays stopped.
*/
void QMediaRecorder::record() {
    if (d->state == RecordingState)
        return;
    if (d->state == PausedState) {
        d->source->resume();
        d->timer.start();
        d->setState(RecordingState);
        return;
    }
    QAudioInput *input = d->session ? d->session->audioInput() : nullptr;
    if (!input || input->device().isNull()) {
        d->setError(ResourceError, QStringLiteral("there is no audio input to record"));
        return;
    }
    if (!d->mediaFormat.isSupported(QMediaFormat::Encode)) {
        d->setError(FormatError, QStringLiteral("only WAV files with uncompressed audio can be recorded"));
        return;
    }
    QString message;
    const QString path = d->resolveLocation(&message);
    if (path.isEmpty()) {
        d->setError(LocationNotWritable, message);
        return;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    auto file = std::make_unique<QFile>(path);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        d->setError(LocationNotWritable, QStringLiteral("cannot write %1").arg(path));
        return;
    }
    QAudioFormat format = input->device().preferredFormat();
    if (d->audioChannelCount > 0)
        format.setChannelCount(d->audioChannelCount);
    if (d->audioSampleRate > 0)
        format.setSampleRate(d->audioSampleRate);
    if (format.sampleRate() <= 0)
        format.setSampleRate(48000);
    if (format.channelCount() <= 0)
        format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    d->recordingMetaData = d->metaData;
    const QByteArray header = QtMultimediaPrivate::waveHeader(format, 0, d->recordingMetaData);
    d->headerSize = header.size();
    file->write(header);

    auto source = std::make_unique<QAudioSource>(input->device(), format);
    d->format = format;
    d->file = std::move(file);
    d->source = std::move(source);
    d->applyVolume();
    connect(input, &QAudioInput::volumeChanged, d->source.get(), [this](float) { d->applyVolume(); });
    connect(input, &QAudioInput::mutedChanged, d->source.get(), [this](bool) { d->applyVolume(); });
    connect(d->source.get(), &QAudioSource::stateChanged, this, [this](QtAudio::State s) {
        if (s == QtAudio::StoppedState && d->source && d->source->error() != QtAudio::NoError) {
            QTimer::singleShot(0, this, [this] {
                if (d->state == StoppedState)
                    return;
                d->finish();
                d->setState(StoppedState);
                d->setError(ResourceError, QStringLiteral("the audio input failed"));
            });
        }
    });
    d->source->start(d->file.get());
    if (d->source->state() == QtAudio::StoppedState) {
        d->source.reset();
        d->file->close();
        d->file->remove();
        d->file.reset();
        d->setError(ResourceError, QStringLiteral("the audio input cannot be opened"));
        return;
    }
    if (d->error != NoError)
        d->setError(NoError, QString());
    d->duration = 0;
    const QUrl location = QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath());
    if (location != d->actualLocation) {
        d->actualLocation = location;
        emit actualLocationChanged(location);
    }
    d->timer.start();
    d->setState(RecordingState);
}

/*!
    Pauses recording; record() goes on in the same file.
*/
void QMediaRecorder::pause() {
    if (d->state != RecordingState)
        return;
    d->source->suspend();
    d->timer.stop();
    d->setState(PausedState);
}

/*!
    Stops recording and finishes the file.
*/
void QMediaRecorder::stop() {
    if (d->state == StoppedState)
        return;
    d->finish();
    emit durationChanged(d->duration);
    d->setState(StoppedState);
}

#include "moc_qmediarecorder.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
