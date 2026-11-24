// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtCore/qsize.h>
#include <QtGui/qimage.h>
#include <QtWidgets/qgraphicsitem.h>
#include <QtWidgets/qgraphicswidget.h>
#include <QtWidgets/qwidget.h>

class QGraphicsProxyWidget : public QGraphicsWidget {
    Q_OBJECT
public:
    enum { Type = 12 };

    explicit QGraphicsProxyWidget(QGraphicsItem *parent = nullptr, Qt::WindowFlags wFlags = Qt::WindowFlags());
    ~QGraphicsProxyWidget() override;

    void setWidget(QWidget *widget);
    QWidget *widget() const;
    QRectF subWidgetRect(const QWidget *widget) const;
    QGraphicsProxyWidget *createProxyForChildWidget(QWidget *child);

    void setGeometry(const QRectF &rect) override;
    using QGraphicsWidget::setGeometry;

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    int type() const override;

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;
    bool sceneEvent(QEvent *event) override;
    virtual QGraphicsProxyWidget *newProxyWidget(const QWidget *child);
    void grabMouseEvent(QEvent *event) override;
    void ungrabMouseEvent(QEvent *event) override;
    bool focusNextPrevChild(bool next) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void wheelEvent(QGraphicsSceneWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;

private:
    void sendMouse(QGraphicsSceneMouseEvent *event, QEvent::Type type);
    void widgetDirty(const QRegion &region);
    QPointer<QWidget> m_widget;
    QPointer<QWidget> m_grab;
    QImage m_cache;
    bool m_cacheValid = false;
};
