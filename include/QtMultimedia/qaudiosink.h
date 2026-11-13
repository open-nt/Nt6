// MIT License - see LICENSE
#pragma once

#include <QtCore/qiodevice.h>
#include <QtCore/qobject.h>
#include <QtMultimedia/qaudiodevice.h>
#include <QtMultimedia/qaudioformat.h>
#include <QtMultimedia/qtaudio.h>

#include <memory>

class QAudioSinkPrivate;

class QAudioSink : public QObject {
    Q_OBJECT
public:
    explicit QAudioSink(const QAudioFormat &format = QAudioFormat(), QObject *parent = nullptr);
    explicit QAudioSink(const QAudioDevice &audioDevice, const QAudioFormat &format = QAudioFormat(),
                        QObject *parent = nullptr);
    ~QAudioSink() override;

    bool isNull() const;
    QAudioFormat format() const;

    void start(QIODevice *device);
    QIODevice *start();
    void stop();
    void reset();
    void suspend();
    void resume();

    void setBufferSize(qsizetype bytes);
    qsizetype bufferSize() const;
    qsizetype bytesFree() const;

    qint64 processedUSecs() const;
    qint64 elapsedUSecs() const;

    QtAudio::Error error() const;
    QtAudio::State state() const;

    void setVolume(qreal volume);
    qreal volume() const;

signals:
    /*!
        This signal is emitted when the state of the stream changes to \a state.

        \sa state()
    */
    void stateChanged(QtAudio::State state) { emitSignal(&QAudioSink::stateChanged, state); }

private:
    friend class QAudioSinkPrivate;
    std::unique_ptr<QAudioSinkPrivate> d;
};
