// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qpair.h>
#include <QtCore/qpoint.h>
#include <QtCore/qpointer.h>
#include <QtGui/qtransform.h>

class QGraphicsItem;
class QTimeLine;

class QGraphicsItemAnimation : public QObject {
    Q_OBJECT
public:
    explicit QGraphicsItemAnimation(QObject *parent = nullptr);
    ~QGraphicsItemAnimation() override;

    QGraphicsItem *item() const { return m_item; }
    void setItem(QGraphicsItem *item);
    QTimeLine *timeLine() const;
    void setTimeLine(QTimeLine *timeLine);

    QPointF posAt(qreal step) const;
    QList<QPair<qreal, QPointF>> posList() const;
    void setPosAt(qreal step, const QPointF &pos);
    QTransform transformAt(qreal step) const;
    qreal rotationAt(qreal step) const;
    QList<QPair<qreal, qreal>> rotationList() const;
    void setRotationAt(qreal step, qreal angle);
    qreal xTranslationAt(qreal step) const;
    qreal yTranslationAt(qreal step) const;
    QList<QPair<qreal, QPointF>> translationList() const;
    void setTranslationAt(qreal step, qreal dx, qreal dy);
    qreal verticalScaleAt(qreal step) const;
    qreal horizontalScaleAt(qreal step) const;
    QList<QPair<qreal, QPointF>> scaleList() const;
    void setScaleAt(qreal step, qreal sx, qreal sy);
    qreal verticalShearAt(qreal step) const;
    qreal horizontalShearAt(qreal step) const;
    QList<QPair<qreal, QPointF>> shearList() const;
    void setShearAt(qreal step, qreal sh, qreal sv);
    void clear();

public slots:
    void setStep(qreal step);

protected:
    virtual void beforeAnimationStep(qreal step);
    virtual void afterAnimationStep(qreal step);

private:
    Q_DISABLE_COPY(QGraphicsItemAnimation)
    using Keys = QList<QPair<qreal, QPointF>>;
    static void insert(Keys &keys, qreal step, const QPointF &value);
    static QPointF valueAt(const Keys &keys, qreal step, const QPointF &fallback);

    QGraphicsItem *m_item = nullptr;
    QPointer<QTimeLine> m_timeLine;
    QMetaObject::Connection m_connection;
    Keys m_pos;
    Keys m_rotation; // the angle in x
    Keys m_translation;
    Keys m_scale;
    Keys m_shear;
};
