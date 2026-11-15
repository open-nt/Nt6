// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtGui/qcolor.h>

class QGraphicsEffect : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
public:
    explicit QGraphicsEffect(QObject *parent = nullptr);
    ~QGraphicsEffect() override;

    virtual QRectF boundingRectFor(const QRectF &sourceRect) const;
    QRectF boundingRect() const;
    bool isEnabled() const;

public slots:
    void setEnabled(bool enable);
    void update();

signals:
    /*! This signal is emitted when the effect is turned on or off; \a enabled is the new state. */
    void enabledChanged(bool enabled) { emitSignal(&QGraphicsEffect::enabledChanged, enabled); }

private:
    friend class QGraphicsItem;
    friend struct QGraphicsEffectAccess;
    bool m_enabled = true;
    class QGraphicsItem *m_item = nullptr;
};

class QGraphicsDropShadowEffect : public QGraphicsEffect {
    Q_OBJECT
    Q_PROPERTY(QPointF offset READ offset WRITE setOffset NOTIFY offsetChanged)
    Q_PROPERTY(qreal xOffset READ xOffset WRITE setXOffset NOTIFY offsetChanged)
    Q_PROPERTY(qreal yOffset READ yOffset WRITE setYOffset NOTIFY offsetChanged)
    Q_PROPERTY(qreal blurRadius READ blurRadius WRITE setBlurRadius NOTIFY blurRadiusChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
public:
    explicit QGraphicsDropShadowEffect(QObject *parent = nullptr);
    ~QGraphicsDropShadowEffect() override;

    QRectF boundingRectFor(const QRectF &rect) const override;
    /*! Returns the offset of the shadow from the item, in item coordinates. */
    QPointF offset() const { return m_offset; }
    /*! Returns the horizontal offset of the shadow. */
    qreal xOffset() const { return m_offset.x(); }
    /*! Returns the vertical offset of the shadow. */
    qreal yOffset() const { return m_offset.y(); }
    /*! Returns the blur radius of the shadow in device pixels. */
    qreal blurRadius() const { return m_blurRadius; }
    /*! Returns the color of the shadow. */
    QColor color() const { return m_color; }

public slots:
    void setOffset(const QPointF &ofs);
    /*! \overload Sets the offset to (\a dx, \a dy). */
    void setOffset(qreal dx, qreal dy) { setOffset(QPointF(dx, dy)); }
    /*! \overload Sets both offsets to \a d. */
    void setOffset(qreal d) { setOffset(QPointF(d, d)); }
    /*! Sets the horizontal offset to \a dx. */
    void setXOffset(qreal dx) { setOffset(QPointF(dx, m_offset.y())); }
    /*! Sets the vertical offset to \a dy. */
    void setYOffset(qreal dy) { setOffset(QPointF(m_offset.x(), dy)); }
    void setBlurRadius(qreal blurRadius);
    void setColor(const QColor &color);

signals:
    /*! This signal is emitted when the offset changes to \a offset. */
    void offsetChanged(const QPointF &offset) { emitSignal(&QGraphicsDropShadowEffect::offsetChanged, offset); }
    /*! This signal is emitted when the blur radius changes to \a blurRadius. */
    void blurRadiusChanged(qreal blurRadius) { emitSignal(&QGraphicsDropShadowEffect::blurRadiusChanged, blurRadius); }
    /*! This signal is emitted when the color changes to \a color. */
    void colorChanged(const QColor &color) { emitSignal(&QGraphicsDropShadowEffect::colorChanged, color); }

private:
    QPointF m_offset{8, 8};
    qreal m_blurRadius = 1.0;
    QColor m_color{63, 63, 63, 180};
};
