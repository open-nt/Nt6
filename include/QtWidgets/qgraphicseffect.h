// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtGui/qbrush.h>
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

class QGraphicsBlurEffect : public QGraphicsEffect {
    Q_OBJECT
    Q_PROPERTY(qreal blurRadius READ blurRadius WRITE setBlurRadius NOTIFY blurRadiusChanged)
    Q_PROPERTY(BlurHints blurHints READ blurHints WRITE setBlurHints NOTIFY blurHintsChanged)
public:
    enum BlurHint { PerformanceHint = 0x00, QualityHint = 0x01, AnimationHint = 0x02 };
    Q_DECLARE_FLAGS(BlurHints, BlurHint)
    Q_FLAG(BlurHints)

    explicit QGraphicsBlurEffect(QObject *parent = nullptr);
    ~QGraphicsBlurEffect() override;

    QRectF boundingRectFor(const QRectF &rect) const override;
    qreal blurRadius() const { return m_blurRadius; }
    BlurHints blurHints() const { return m_hints; }

public slots:
    void setBlurRadius(qreal blurRadius);
    void setBlurHints(QGraphicsBlurEffect::BlurHints hints);

signals:
    /*! This signal is emitted when the blur radius changes to \a blurRadius. */
    void blurRadiusChanged(qreal blurRadius) { emitSignal(&QGraphicsBlurEffect::blurRadiusChanged, blurRadius); }
    /*! This signal is emitted when the blur hints change to \a hints. */
    void blurHintsChanged(QGraphicsBlurEffect::BlurHints hints) {
        emitSignal(&QGraphicsBlurEffect::blurHintsChanged, hints);
    }

private:
    qreal m_blurRadius = 5.0;
    BlurHints m_hints = PerformanceHint;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QGraphicsBlurEffect::BlurHints)

class QGraphicsColorizeEffect : public QGraphicsEffect {
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(qreal strength READ strength WRITE setStrength NOTIFY strengthChanged)
public:
    explicit QGraphicsColorizeEffect(QObject *parent = nullptr);
    ~QGraphicsColorizeEffect() override;

    QColor color() const { return m_color; }
    qreal strength() const { return m_strength; }

public slots:
    void setColor(const QColor &c);
    void setStrength(qreal strength);

signals:
    /*! This signal is emitted when the color changes to \a color. */
    void colorChanged(const QColor &color) { emitSignal(&QGraphicsColorizeEffect::colorChanged, color); }
    /*! This signal is emitted when the strength changes to \a strength. */
    void strengthChanged(qreal strength) { emitSignal(&QGraphicsColorizeEffect::strengthChanged, strength); }

private:
    QColor m_color{0, 0, 192};
    qreal m_strength = 1.0;
};

class QGraphicsOpacityEffect : public QGraphicsEffect {
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(QBrush opacityMask READ opacityMask WRITE setOpacityMask NOTIFY opacityMaskChanged)
public:
    explicit QGraphicsOpacityEffect(QObject *parent = nullptr);
    ~QGraphicsOpacityEffect() override;

    qreal opacity() const { return m_opacity; }
    QBrush opacityMask() const { return m_mask; }

public slots:
    void setOpacity(qreal opacity);
    void setOpacityMask(const QBrush &mask);

signals:
    /*! This signal is emitted when the opacity changes to \a opacity. */
    void opacityChanged(qreal opacity) { emitSignal(&QGraphicsOpacityEffect::opacityChanged, opacity); }
    /*! This signal is emitted when the opacity mask changes to \a mask. */
    void opacityMaskChanged(const QBrush &mask) { emitSignal(&QGraphicsOpacityEffect::opacityMaskChanged, mask); }

private:
    qreal m_opacity = 0.7;
    QBrush m_mask;
};
