// MIT License - see LICENSE
#pragma once

#include <QtGui/qfont.h>
#include <QtGui/qpalette.h>
#include <QtWidgets/qgraphicsitem.h>
#include <QtWidgets/qgraphicslayout.h>
#include <QtWidgets/qgraphicslayoutitem.h>

class QGraphicsSceneMoveEvent;
class QGraphicsSceneResizeEvent;

class QGraphicsWidget : public QGraphicsObject, public QGraphicsLayoutItem {
    Q_OBJECT
    Q_PROPERTY(QSizeF size READ size WRITE resize NOTIFY geometryChanged)
    Q_PROPERTY(QRectF geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont)
    Q_PROPERTY(QPalette palette READ palette WRITE setPalette)
    Q_PROPERTY(bool autoFillBackground READ autoFillBackground WRITE setAutoFillBackground)
    Q_INTERFACES(QGraphicsItem QGraphicsLayoutItem)
public:
    enum { Type = 11 };

    explicit QGraphicsWidget(QGraphicsItem *parent = nullptr, Qt::WindowFlags wFlags = Qt::WindowFlags());
    ~QGraphicsWidget() override;

    QGraphicsLayout *layout() const { return m_layout; }
    void setLayout(QGraphicsLayout *layout);
    void adjustSize();

    QSizeF size() const { return QGraphicsLayoutItem::geometry().size(); }
    void resize(const QSizeF &size);
    void resize(qreal w, qreal h) { resize(QSizeF(w, h)); }
    QRectF rect() const { return QRectF(QPointF(0, 0), size()); }
    void setGeometry(const QRectF &rect) override;
    void setGeometry(qreal x, qreal y, qreal w, qreal h) { setGeometry(QRectF(x, y, w, h)); }
    QRectF geometry() const { return QRectF(pos(), size()); }

    void setContentsMargins(qreal left, qreal top, qreal right, qreal bottom);
    void getContentsMargins(qreal *left, qreal *top, qreal *right, qreal *bottom) const override;

    QFont font() const { return m_font; }
    void setFont(const QFont &font);
    QPalette palette() const { return m_palette; }
    void setPalette(const QPalette &palette);
    bool autoFillBackground() const { return m_autoFill; }
    void setAutoFillBackground(bool enabled);

    Qt::WindowFlags windowFlags() const { return m_windowFlags; }
    void setWindowFlags(Qt::WindowFlags wFlags) { m_windowFlags = wFlags; }

    void updateGeometry() override;
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    int type() const override;

signals:
    /*!
        This signal is emitted when the geometry of the widget changes.
    */
    void geometryChanged() { emitSignal(&QGraphicsWidget::geometryChanged); }
    /*!
        This signal is emitted when the layout of the widget changes.
    */
    void layoutChanged() { emitSignal(&QGraphicsWidget::layoutChanged); }

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event);
    virtual void moveEvent(QGraphicsSceneMoveEvent *event);
    bool event(QEvent *event) override;

private:
    friend class QGraphicsLayout;
    void requestLayout(); // posts a layout request, once until it is handled

    QGraphicsLayout *m_layout = nullptr;
    qreal m_margins[4] = {0, 0, 0, 0};
    QFont m_font;
    QPalette m_palette;
    bool m_autoFill = false;
    bool m_layoutRequested = false;
    QPointF m_lastPos; // for the move events
    bool m_settingGeometry = false;
    Qt::WindowFlags m_windowFlags;
};
