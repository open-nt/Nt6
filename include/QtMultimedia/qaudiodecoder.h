// MIT License - see LICENSE
#pragma once

#include <QtCore/qiodevice.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qurl.h>
#include <QtMultimedia/qaudiobuffer.h>
#include <QtMultimedia/qaudioformat.h>

#include <memory>

class QAudioDecoderPrivate;

class QAudioDecoder : public QObject {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool isDecoding READ isDecoding NOTIFY isDecodingChanged)
    Q_PROPERTY(QString error READ errorString)
    Q_PROPERTY(bool bufferAvailable READ bufferAvailable NOTIFY bufferAvailableChanged)
public:
    enum Error { NoError, ResourceError, FormatError, AccessDeniedError, NotSupportedError };
    Q_ENUM(Error)

    explicit QAudioDecoder(QObject *parent = nullptr);
    ~QAudioDecoder() override;

    bool isSupported() const;
    bool isDecoding() const;

    QUrl source() const;
    void setSource(const QUrl &fileName);

    QIODevice *sourceDevice() const;
    void setSourceDevice(QIODevice *device);

    QAudioFormat audioFormat() const;
    void setAudioFormat(const QAudioFormat &format);

    Error error() const;
    QString errorString() const;

    QAudioBuffer read() const;
    bool bufferAvailable() const;

    qint64 position() const;
    qint64 duration() const;

public slots:
    void start();
    void stop();

signals:
    /*!
        This signal is emitted when a buffer becomes available, or the last one is read; \a available
        is the new state.
    */
    void bufferAvailableChanged(bool available) { emitSignal(&QAudioDecoder::bufferAvailableChanged, available); }
    /*!
        This signal is emitted when a decoded buffer is ready to read().
    */
    void bufferReady() { emitSignal(&QAudioDecoder::bufferReady); }
    /*!
        This signal is emitted when the whole source has been decoded and read.
    */
    void finished() { emitSignal(&QAudioDecoder::finished); }
    /*!
        This signal is emitted when decoding starts or stops; \a decoding is the new state.
    */
    void isDecodingChanged(bool decoding) { emitSignal(&QAudioDecoder::isDecodingChanged, decoding); }
    /*!
        This signal is emitted when the format of the decoded buffers changes to \a format.
    */
    void formatChanged(const QAudioFormat &format) { emitSignal(&QAudioDecoder::formatChanged, format); }
    /*!
        This signal is emitted when an error occurs; \a error says which.
    */
    void error(QAudioDecoder::Error error) {
        emitSignal(static_cast<void (QAudioDecoder::*)(QAudioDecoder::Error)>(&QAudioDecoder::error), error);
    }
    /*!
        This signal is emitted when the source changes.
    */
    void sourceChanged() { emitSignal(&QAudioDecoder::sourceChanged); }
    /*!
        This signal is emitted when the position of the last buffer read changes to \a position
        milliseconds.
    */
    void positionChanged(qint64 position) { emitSignal(&QAudioDecoder::positionChanged, position); }
    /*!
        This signal is emitted when the duration of the source changes to \a duration milliseconds.
    */
    void durationChanged(qint64 duration) { emitSignal(&QAudioDecoder::durationChanged, duration); }

private:
    friend class QAudioDecoderPrivate;
    std::unique_ptr<QAudioDecoderPrivate> d;
};
