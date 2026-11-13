// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtGui/qimage.h>
#include <QtMultimedia/qvideoframe.h>
#include <QtWidgets/qwidget.h>

class QVideoSink;

class QVideoWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool fullScreen READ isFullScreen WRITE setFullScreen NOTIFY fullScreenChanged)
    Q_PROPERTY(
        Qt::AspectRatioMode aspectRatioMode READ aspectRatioMode WRITE setAspectRatioMode NOTIFY aspectRatioModeChanged)
public:
    explicit QVideoWidget(QWidget *parent = nullptr);
    ~QVideoWidget() override;

    QVideoSink *videoSink() const;

    bool isFullScreen() const { return m_fullScreen; }
    Qt::AspectRatioMode aspectRatioMode() const { return m_mode; }

    QSize sizeHint() const override;

public slots:
    void setFullScreen(bool fullScreen);
    void setAspectRatioMode(Qt::AspectRatioMode mode);

signals:
    /*!
        This signal is emitted when the widget enters or leaves full screen; \a fullScreen is the new
        state.
    */
    void fullScreenChanged(bool fullScreen) { emitSignal(&QVideoWidget::fullScreenChanged, fullScreen); }
    /*!
        This signal is emitted when the aspect ratio mode changes to \a mode.
    */
    void aspectRatioModeChanged(Qt::AspectRatioMode mode) { emitSignal(&QVideoWidget::aspectRatioModeChanged, mode); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void frameChanged(const QVideoFrame &frame);

    QVideoSink *m_sink;
    QVideoFrame m_frame;
    QImage m_image; // m_frame converted, made when it is first painted
    Qt::AspectRatioMode m_mode = Qt::KeepAspectRatio;
    bool m_fullScreen = false;
    QRect m_normalGeometry;
};
