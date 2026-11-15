// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtGui/qimage.h>
#include <QtMultimedia/qvideoframe.h>
#include <QtWidgets/qgraphicsitem.h>

class QVideoSink;

class QGraphicsVideoItem : public QGraphicsObject {
    Q_OBJECT
    Q_PROPERTY(Qt::AspectRatioMode aspectRatioMode READ aspectRatioMode WRITE setAspectRatioMode)
    Q_PROPERTY(QPointF offset READ offset WRITE setOffset)
    Q_PROPERTY(QSizeF size READ size WRITE setSize)
    Q_PROPERTY(QSizeF nativeSize READ nativeSize NOTIFY nativeSizeChanged)
    Q_PROPERTY(QVideoSink *videoSink READ videoSink CONSTANT)
public:
    /*! The type of the item, returned by type(). \omitvalue Type */
    enum { Type = 14 };

    explicit QGraphicsVideoItem(QGraphicsItem *parent = nullptr);
    ~QGraphicsVideoItem() override;

    QVideoSink *videoSink() const;
    Qt::AspectRatioMode aspectRatioMode() const;
    void setAspectRatioMode(Qt::AspectRatioMode mode);
    QPointF offset() const;
    void setOffset(const QPointF &offset);
    QSizeF size() const;
    void setSize(const QSizeF &size);
    QSizeF nativeSize() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    int type() const override;

signals:
    /*!
        This signal is emitted when the size of the frames changes to \a size.
    */
    void nativeSizeChanged(const QSizeF &size) { emitSignal(&QGraphicsVideoItem::nativeSizeChanged, size); }

private:
    void frameChanged(const QVideoFrame &frame);
    QRectF videoRect() const;

    QVideoSink *m_sink;
    QVideoFrame m_frame;
    QImage m_image; // m_frame converted, made when it is first painted
    Qt::AspectRatioMode m_mode = Qt::KeepAspectRatio;
    QPointF m_offset;
    QSizeF m_size = QSizeF(320, 240);
    QSizeF m_nativeSize;
};
