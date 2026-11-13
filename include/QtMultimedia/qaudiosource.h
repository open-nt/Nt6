// MIT License - see LICENSE
#pragma once

#include <QtCore/qiodevice.h>
#include <QtCore/qobject.h>
#include <QtMultimedia/qaudiodevice.h>
#include <QtMultimedia/qaudioformat.h>
#include <QtMultimedia/qtaudio.h>

#include <memory>

class QAudioSourcePrivate;

class QAudioSource : public QObject {
    Q_OBJECT
public:
    explicit QAudioSource(const QAudioFormat &format = QAudioFormat(), QObject *parent = nullptr);
    explicit QAudioSource(const QAudioDevice &audioDevice, const QAudioFormat &format = QAudioFormat(),
                          QObject *parent = nullptr);
    ~QAudioSource() override;

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
    qsizetype bytesAvailable() const;

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
    void stateChanged(QtAudio::State state) { emitSignal(&QAudioSource::stateChanged, state); }

private:
    friend class QAudioSourcePrivate;
    std::unique_ptr<QAudioSourcePrivate> d;
};
