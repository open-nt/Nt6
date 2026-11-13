// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtMultimedia/qvideoframe.h>

class QVideoSink : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString subtitleText READ subtitleText WRITE setSubtitleText NOTIFY subtitleTextChanged)
    Q_PROPERTY(QSize videoSize READ videoSize NOTIFY videoSizeChanged)
public:
    explicit QVideoSink(QObject *parent = nullptr);
    ~QVideoSink() override;

    QSize videoSize() const;

    QString subtitleText() const;
    void setSubtitleText(const QString &subtitle);

    void setVideoFrame(const QVideoFrame &frame);
    QVideoFrame videoFrame() const;

signals:
    /*!
        This signal is emitted when a new \a frame is set.
    */
    void videoFrameChanged(const QVideoFrame &frame) { emitSignal(&QVideoSink::videoFrameChanged, frame); }
    /*!
        This signal is emitted when the subtitle text changes to \a subtitleText.
    */
    void subtitleTextChanged(const QString &subtitleText) {
        emitSignal(&QVideoSink::subtitleTextChanged, subtitleText);
    }
    /*!
        This signal is emitted when the size of the frames changes.
    */
    void videoSizeChanged() { emitSignal(&QVideoSink::videoSizeChanged); }

private:
    QVideoFrame m_frame;
    QString m_subtitle;
};
