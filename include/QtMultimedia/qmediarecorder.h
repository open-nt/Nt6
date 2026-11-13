// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtCore/qurl.h>
#include <QtMultimedia/qmediaformat.h>

#include <memory>

class QMediaCaptureSession;
class QMediaRecorderPrivate;

class QMediaRecorder : public QObject {
    Q_OBJECT
    Q_PROPERTY(QMediaRecorder::RecorderState recorderState READ recorderState NOTIFY recorderStateChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(QUrl outputLocation READ outputLocation WRITE setOutputLocation)
    Q_PROPERTY(QUrl actualLocation READ actualLocation NOTIFY actualLocationChanged)
    Q_PROPERTY(QMediaRecorder::Error error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorChanged)
    Q_PROPERTY(QMediaFormat mediaFormat READ mediaFormat WRITE setMediaFormat NOTIFY mediaFormatChanged)
    Q_PROPERTY(Quality quality READ quality WRITE setQuality NOTIFY qualityChanged)
    Q_PROPERTY(EncodingMode encodingMode READ encodingMode WRITE setEncodingMode NOTIFY encodingModeChanged)
    Q_PROPERTY(QSize videoResolution READ videoResolution WRITE setVideoResolution NOTIFY videoResolutionChanged)
    Q_PROPERTY(qreal videoFrameRate READ videoFrameRate WRITE setVideoFrameRate NOTIFY videoFrameRateChanged)
    Q_PROPERTY(int videoBitRate READ videoBitRate WRITE setVideoBitRate NOTIFY videoBitRateChanged)
    Q_PROPERTY(int audioBitRate READ audioBitRate WRITE setAudioBitRate NOTIFY audioBitRateChanged)
    Q_PROPERTY(int audioChannelCount READ audioChannelCount WRITE setAudioChannelCount NOTIFY audioChannelCountChanged)
    Q_PROPERTY(int audioSampleRate READ audioSampleRate WRITE setAudioSampleRate NOTIFY audioSampleRateChanged)
public:
    enum Quality { VeryLowQuality, LowQuality, NormalQuality, HighQuality, VeryHighQuality };
    Q_ENUM(Quality)

    enum EncodingMode { ConstantQualityEncoding, ConstantBitRateEncoding, AverageBitRateEncoding, TwoPassEncoding };
    Q_ENUM(EncodingMode)

    enum RecorderState { StoppedState, RecordingState, PausedState };
    Q_ENUM(RecorderState)

    enum Error { NoError, ResourceError, FormatError, OutOfSpaceError, LocationNotWritable };
    Q_ENUM(Error)

    explicit QMediaRecorder(QObject *parent = nullptr);
    ~QMediaRecorder() override;

    bool isAvailable() const;

    QUrl outputLocation() const;
    void setOutputLocation(const QUrl &location);
    QUrl actualLocation() const;

    RecorderState recorderState() const;
    Error error() const;
    QString errorString() const;
    qint64 duration() const;

    QMediaFormat mediaFormat() const;
    void setMediaFormat(const QMediaFormat &format);

    EncodingMode encodingMode() const;
    void setEncodingMode(EncodingMode mode);

    Quality quality() const;
    void setQuality(Quality quality);

    QSize videoResolution() const;
    void setVideoResolution(const QSize &size);
    void setVideoResolution(int width, int height) { setVideoResolution(QSize(width, height)); }

    qreal videoFrameRate() const;
    void setVideoFrameRate(qreal frameRate);

    int videoBitRate() const;
    void setVideoBitRate(int bitRate);

    int audioBitRate() const;
    void setAudioBitRate(int bitRate);

    int audioChannelCount() const;
    void setAudioChannelCount(int channels);

    int audioSampleRate() const;
    void setAudioSampleRate(int sampleRate);

    QMediaCaptureSession *captureSession() const;

public slots:
    void record();
    void pause();
    void stop();

signals:
    /*!
        This signal is emitted when the recorder state changes to \a state.
    */
    void recorderStateChanged(QMediaRecorder::RecorderState state) {
        emitSignal(&QMediaRecorder::recorderStateChanged, state);
    }
    /*!
        This signal is emitted while recording, about every 50 ms, with the recorded \a duration in
        milliseconds.
    */
    void durationChanged(qint64 duration) { emitSignal(&QMediaRecorder::durationChanged, duration); }
    /*!
        This signal is emitted when recording starts in the file \a location.
    */
    void actualLocationChanged(const QUrl &location) { emitSignal(&QMediaRecorder::actualLocationChanged, location); }
    /*!
        This signal is emitted when any of the encoder settings change.
    */
    void encoderSettingsChanged() { emitSignal(&QMediaRecorder::encoderSettingsChanged); }
    /*!
        This signal is emitted when an \a error occurs, with \a errorString describing it.
    */
    void errorOccurred(QMediaRecorder::Error error, const QString &errorString) {
        emitSignal(&QMediaRecorder::errorOccurred, error, errorString);
    }
    /*!
        This signal is emitted when error() or errorString() changes.
    */
    void errorChanged() { emitSignal(&QMediaRecorder::errorChanged); }
    /*!
        This signal is emitted when the media format changes.
    */
    void mediaFormatChanged() { emitSignal(&QMediaRecorder::mediaFormatChanged); }
    /*!
        This signal is emitted when the encoding mode changes.
    */
    void encodingModeChanged() { emitSignal(&QMediaRecorder::encodingModeChanged); }
    /*!
        This signal is emitted when the quality changes.
    */
    void qualityChanged() { emitSignal(&QMediaRecorder::qualityChanged); }
    /*!
        This signal is emitted when the video resolution changes.
    */
    void videoResolutionChanged() { emitSignal(&QMediaRecorder::videoResolutionChanged); }
    /*!
        This signal is emitted when the video frame rate changes.
    */
    void videoFrameRateChanged() { emitSignal(&QMediaRecorder::videoFrameRateChanged); }
    /*!
        This signal is emitted when the video bit rate changes.
    */
    void videoBitRateChanged() { emitSignal(&QMediaRecorder::videoBitRateChanged); }
    /*!
        This signal is emitted when the audio bit rate changes.
    */
    void audioBitRateChanged() { emitSignal(&QMediaRecorder::audioBitRateChanged); }
    /*!
        This signal is emitted when the audio channel count changes.
    */
    void audioChannelCountChanged() { emitSignal(&QMediaRecorder::audioChannelCountChanged); }
    /*!
        This signal is emitted when the audio sample rate changes.
    */
    void audioSampleRateChanged() { emitSignal(&QMediaRecorder::audioSampleRateChanged); }

private:
    friend class QMediaCaptureSession;
    friend class QMediaCaptureSessionPrivate;
    friend class QMediaRecorderPrivate;
    std::unique_ptr<QMediaRecorderPrivate> d;
};
