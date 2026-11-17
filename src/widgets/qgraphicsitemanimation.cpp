// MIT License - see LICENSE
#include <QtWidgets/qgraphicsitemanimation.h>

#include <QtCore/qtimeline.h>
#include <QtWidgets/qgraphicsitem.h>

#include <algorithm>

/*!
    \class QGraphicsItemAnimation
    \inmodule NtWidgets
    \since 6.0
    \brief The QGraphicsItemAnimation class moves, turns, scales and shears a graphics item with a QTimeLine.

    Give the animation an item, a time line, and values at steps from 0 to 1; between the steps the values are
    interpolated:

    \code
    auto *timer = new QTimeLine(1000, this);
    auto *animation = new QGraphicsItemAnimation(this);
    animation->setItem(ball);
    animation->setTimeLine(timer);
    animation->setPosAt(0, QPointF(0, 0));
    animation->setPosAt(1, QPointF(200, 0));
    animation->setRotationAt(1, 360);
    timer->start();
    \endcode

    \differences
    The step is the value of the time line, so its easing curve applies.
    \enddifferences

    \sa QTimeLine, QGraphicsTransform
*/

/*!
    Constructs an animation with the given \a parent.
*/
QGraphicsItemAnimation::QGraphicsItemAnimation(QObject *parent) : QObject(parent) {}

/*!
    Destroys the animation.
*/
QGraphicsItemAnimation::~QGraphicsItemAnimation() = default;

/*!
    \fn QGraphicsItem *QGraphicsItemAnimation::item() const

    Returns the item that the animation changes.
*/

/*!
    Sets the item that the animation changes to \a item.
*/
void QGraphicsItemAnimation::setItem(QGraphicsItem *item) { m_item = item; }

/*!
    Returns the time line that drives the animation.
*/
QTimeLine *QGraphicsItemAnimation::timeLine() const { return m_timeLine.data(); }

/*!
    Sets the time line that drives the animation to \a timeLine: each of its values is a step.
*/
void QGraphicsItemAnimation::setTimeLine(QTimeLine *timeLine) {
    if (m_connection)
        QObject::disconnect(m_connection);
    m_timeLine = timeLine;
    if (timeLine)
        m_connection = QObject::connect(timeLine, &QTimeLine::valueChanged, this, [this](qreal v) { setStep(v); });
}

void QGraphicsItemAnimation::insert(Keys &keys, qreal step, const QPointF &value) {
    if (step < 0 || step > 1)
        return;
    const auto at = std::lower_bound(keys.begin(), keys.end(), step,
                                     [](const QPair<qreal, QPointF> &k, qreal s) { return k.first < s; });
    if (at != keys.end() && qFuzzyCompare(at->first + 1, step + 1))
        at->second = value;
    else
        keys.insert(at, qMakePair(step, value));
}

QPointF QGraphicsItemAnimation::valueAt(const Keys &keys, qreal step, const QPointF &fallback) {
    if (keys.isEmpty())
        return fallback;
    if (step <= keys.first().first)
        return keys.first().second;
    if (step >= keys.last().first)
        return keys.last().second;
    for (qsizetype i = 1; i < keys.size(); ++i) {
        if (step <= keys.at(i).first) {
            const auto &a = keys.at(i - 1), &b = keys.at(i);
            const qreal t = (step - a.first) / (b.first - a.first);
            return a.second + (b.second - a.second) * t;
        }
    }
    return keys.last().second;
}

/*!
    Returns the position at \a step.
*/
QPointF QGraphicsItemAnimation::posAt(qreal step) const {
    return valueAt(m_pos, step, m_item ? m_item->pos() : QPointF());
}

/*!
    Returns the positions that were set, with their steps.
*/
QList<QPair<qreal, QPointF>> QGraphicsItemAnimation::posList() const { return m_pos; }

/*!
    Sets the position at \a step, from 0 to 1, to \a pos.
*/
void QGraphicsItemAnimation::setPosAt(qreal step, const QPointF &pos) { insert(m_pos, step, pos); }

/*!
    Returns the transform at \a step: the rotation, the translation, the scale and the shear.
*/
QTransform QGraphicsItemAnimation::transformAt(qreal step) const {
    QTransform t;
    t.rotate(rotationAt(step));
    t.translate(xTranslationAt(step), yTranslationAt(step));
    t.scale(horizontalScaleAt(step), verticalScaleAt(step));
    t.shear(horizontalShearAt(step), verticalShearAt(step));
    return t;
}

/*!
    Returns the rotation at \a step, in degrees.
*/
qreal QGraphicsItemAnimation::rotationAt(qreal step) const { return valueAt(m_rotation, step, QPointF()).x(); }

/*!
    Returns the rotations that were set, with their steps.
*/
QList<QPair<qreal, qreal>> QGraphicsItemAnimation::rotationList() const {
    QList<QPair<qreal, qreal>> list;
    for (const auto &k : m_rotation)
        list.append(qMakePair(k.first, k.second.x()));
    return list;
}

/*!
    Sets the rotation at \a step to \a angle degrees.
*/
void QGraphicsItemAnimation::setRotationAt(qreal step, qreal angle) { insert(m_rotation, step, QPointF(angle, 0)); }

/*!
    Returns the translation across at \a step.
*/
qreal QGraphicsItemAnimation::xTranslationAt(qreal step) const { return valueAt(m_translation, step, QPointF()).x(); }

/*!
    Returns the translation down at \a step.
*/
qreal QGraphicsItemAnimation::yTranslationAt(qreal step) const { return valueAt(m_translation, step, QPointF()).y(); }

/*!
    Returns the translations that were set, with their steps.
*/
QList<QPair<qreal, QPointF>> QGraphicsItemAnimation::translationList() const { return m_translation; }

/*!
    Sets the translation at \a step to \a dx across and \a dy down.
*/
void QGraphicsItemAnimation::setTranslationAt(qreal step, qreal dx, qreal dy) {
    insert(m_translation, step, QPointF(dx, dy));
}

/*!
    Returns the scale down at \a step.
*/
qreal QGraphicsItemAnimation::verticalScaleAt(qreal step) const { return valueAt(m_scale, step, QPointF(1, 1)).y(); }

/*!
    Returns the scale across at \a step.
*/
qreal QGraphicsItemAnimation::horizontalScaleAt(qreal step) const {
    return valueAt(m_scale, step, QPointF(1, 1)).x();
}

/*!
    Returns the scales that were set, with their steps.
*/
QList<QPair<qreal, QPointF>> QGraphicsItemAnimation::scaleList() const { return m_scale; }

/*!
    Sets the scale at \a step to \a sx across and \a sy down.
*/
void QGraphicsItemAnimation::setScaleAt(qreal step, qreal sx, qreal sy) { insert(m_scale, step, QPointF(sx, sy)); }

/*!
    Returns the shear down at \a step.
*/
qreal QGraphicsItemAnimation::verticalShearAt(qreal step) const { return valueAt(m_shear, step, QPointF()).y(); }

/*!
    Returns the shear across at \a step.
*/
qreal QGraphicsItemAnimation::horizontalShearAt(qreal step) const { return valueAt(m_shear, step, QPointF()).x(); }

/*!
    Returns the shears that were set, with their steps.
*/
QList<QPair<qreal, QPointF>> QGraphicsItemAnimation::shearList() const { return m_shear; }

/*!
    Sets the shear at \a step to \a sh across and \a sv down.
*/
void QGraphicsItemAnimation::setShearAt(qreal step, qreal sh, qreal sv) { insert(m_shear, step, QPointF(sh, sv)); }

/*!
    Removes all values that were set.
*/
void QGraphicsItemAnimation::clear() {
    m_pos.clear();
    m_rotation.clear();
    m_translation.clear();
    m_scale.clear();
    m_shear.clear();
}

/*!
    Gives the item its position and transform at \a step, from 0 to 1, between beforeAnimationStep() and
    afterAnimationStep().
*/
void QGraphicsItemAnimation::setStep(qreal step) {
    step = std::clamp<qreal>(step, 0, 1);
    beforeAnimationStep(step);
    if (m_item) {
        if (!m_pos.isEmpty())
            m_item->setPos(posAt(step));
        if (!m_rotation.isEmpty() || !m_translation.isEmpty() || !m_scale.isEmpty() || !m_shear.isEmpty())
            m_item->setTransform(transformAt(step));
    }
    afterAnimationStep(step);
}

/*!
    Is called with \a step before the item is changed for it. The default implementation does nothing.
*/
void QGraphicsItemAnimation::beforeAnimationStep(qreal step) { (void)step; }

/*!
    Is called with \a step after the item was changed for it. The default implementation does nothing.
*/
void QGraphicsItemAnimation::afterAnimationStep(qreal step) { (void)step; }

#include "moc_qgraphicsitemanimation.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
