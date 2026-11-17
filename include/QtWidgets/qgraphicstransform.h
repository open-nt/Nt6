// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qvectornd.h>

class QGraphicsItem;

class QGraphicsTransform : public QObject {
    Q_OBJECT
public:
    explicit QGraphicsTransform(QObject *parent = nullptr);
    ~QGraphicsTransform() override;

    virtual void applyTo(QMatrix4x4 *matrix) const = 0;

protected slots:
    void update();

private:
    friend class QGraphicsItem;
    Q_DISABLE_COPY(QGraphicsTransform)
    QList<QGraphicsItem *> m_items; // the items that it transforms
};

class QGraphicsScale : public QGraphicsTransform {
    Q_OBJECT
    Q_PROPERTY(QVector3D origin READ origin WRITE setOrigin NOTIFY originChanged)
    Q_PROPERTY(qreal xScale READ xScale WRITE setXScale NOTIFY xScaleChanged)
    Q_PROPERTY(qreal yScale READ yScale WRITE setYScale NOTIFY yScaleChanged)
    Q_PROPERTY(qreal zScale READ zScale WRITE setZScale NOTIFY zScaleChanged)
public:
    explicit QGraphicsScale(QObject *parent = nullptr);
    ~QGraphicsScale() override;

    QVector3D origin() const { return m_origin; }
    void setOrigin(const QVector3D &point);
    qreal xScale() const { return m_x; }
    void setXScale(qreal scale);
    qreal yScale() const { return m_y; }
    void setYScale(qreal scale);
    qreal zScale() const { return m_z; }
    void setZScale(qreal scale);

    void applyTo(QMatrix4x4 *matrix) const override;

signals:
    /*! This signal is emitted when the origin changes. */
    void originChanged() { emitSignal(&QGraphicsScale::originChanged); }
    /*! This signal is emitted when the scale across changes. */
    void xScaleChanged() { emitSignal(&QGraphicsScale::xScaleChanged); }
    /*! This signal is emitted when the scale down changes. */
    void yScaleChanged() { emitSignal(&QGraphicsScale::yScaleChanged); }
    /*! This signal is emitted when the scale in depth changes. */
    void zScaleChanged() { emitSignal(&QGraphicsScale::zScaleChanged); }
    /*! This signal is emitted when any of the scales changes. */
    void scaleChanged() { emitSignal(&QGraphicsScale::scaleChanged); }

private:
    QVector3D m_origin;
    qreal m_x = 1;
    qreal m_y = 1;
    qreal m_z = 1;
};

class QGraphicsRotation : public QGraphicsTransform {
    Q_OBJECT
    Q_PROPERTY(QVector3D origin READ origin WRITE setOrigin NOTIFY originChanged)
    Q_PROPERTY(qreal angle READ angle WRITE setAngle NOTIFY angleChanged)
    Q_PROPERTY(QVector3D axis READ axis WRITE setAxis NOTIFY axisChanged)
public:
    explicit QGraphicsRotation(QObject *parent = nullptr);
    ~QGraphicsRotation() override;

    QVector3D origin() const { return m_origin; }
    void setOrigin(const QVector3D &point);
    qreal angle() const { return m_angle; }
    void setAngle(qreal angle);
    QVector3D axis() const { return m_axis; }
    void setAxis(const QVector3D &axis);
    void setAxis(Qt::Axis axis);

    void applyTo(QMatrix4x4 *matrix) const override;

signals:
    /*! This signal is emitted when the origin changes. */
    void originChanged() { emitSignal(&QGraphicsRotation::originChanged); }
    /*! This signal is emitted when the angle changes. */
    void angleChanged() { emitSignal(&QGraphicsRotation::angleChanged); }
    /*! This signal is emitted when the axis changes. */
    void axisChanged() { emitSignal(&QGraphicsRotation::axisChanged); }

private:
    QVector3D m_origin;
    qreal m_angle = 0;
    QVector3D m_axis{0, 0, 1};
};
