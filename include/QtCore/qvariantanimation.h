// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractanimation.h>
#include <QtCore/qeasingcurve.h>
#include <QtCore/qlist.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qvariant.h>

#include <functional>
#include <utility>

class QVariantAnimation : public QAbstractAnimation {
    Q_OBJECT
    Q_PROPERTY(QVariant startValue READ startValue WRITE setStartValue)
    Q_PROPERTY(QVariant endValue READ endValue WRITE setEndValue)
    Q_PROPERTY(QVariant currentValue READ currentValue NOTIFY valueChanged)
    Q_PROPERTY(int duration READ duration WRITE setDuration)
    Q_PROPERTY(QEasingCurve easingCurve READ easingCurve WRITE setEasingCurve)
public:
    /*!
        A step, from 0 at the start to 1 at the end, and the value that the animation has there.
    */
    using KeyValue = std::pair<qreal, QVariant>;
    /*!
        A list of key values, ordered by their steps.
    */
    using KeyValues = QList<KeyValue>;

    explicit QVariantAnimation(QObject *parent = nullptr);
    ~QVariantAnimation() override;

    QVariant startValue() const;
    void setStartValue(const QVariant &value);

    QVariant endValue() const;
    void setEndValue(const QVariant &value);

    QVariant keyValueAt(qreal step) const;
    void setKeyValueAt(qreal step, const QVariant &value);

    KeyValues keyValues() const;
    void setKeyValues(const KeyValues &values);

    QVariant currentValue() const;

    int duration() const override;
    void setDuration(int msecs);

    QEasingCurve easingCurve() const;
    void setEasingCurve(const QEasingCurve &easing);

    /*! \internal The function that interpolates values of the meta-type \a typeId. */
    using Interpolator = std::function<QVariant(const QVariant &, const QVariant &, qreal)>;
    /*! \internal Registers \a interpolator for the meta-type \a typeId; an empty one removes it. */
    static void registerInterpolator(int typeId, Interpolator interpolator);

signals:
    /*!
        This signal is emitted when the current value of the animation has changed, with the new
        value in \a value.

        \sa currentValue(), updateCurrentValue()
    */
    void valueChanged(const QVariant &value) { emitSignal(&QVariantAnimation::valueChanged, value); }

protected:
    bool event(QEvent *event) override;

    void updateCurrentTime(int currentTime) override;
    void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState) override;

    virtual void updateCurrentValue(const QVariant &value);
    virtual QVariant interpolated(const QVariant &from, const QVariant &to, qreal progress) const;

private:
    friend class QPropertyAnimation;
    void recalculate(bool force);

    KeyValues m_keyValues;
    QVariant m_defaultStart; // stands in for a start value that was not set
    QVariant m_currentValue;
    QEasingCurve m_easing;
    int m_duration = 250;
};

// Makes QVariantAnimation interpolate values of the type T with the function, which gets the two values around
// the current step and the progress between them, from 0 to 1. A null function takes the type out again.
template <typename T>
void qRegisterAnimationInterpolator(QVariant (*func)(const T &from, const T &to, qreal progress)) {
    if (!func) {
        QVariantAnimation::registerInterpolator(QMetaType::fromType<T>().id(), nullptr);
        return;
    }
    QVariantAnimation::registerInterpolator(QMetaType::fromType<T>().id(),
                                            [func](const QVariant &from, const QVariant &to, qreal progress) {
                                                return func(from.value<T>(), to.value<T>(), progress);
                                            });
}
