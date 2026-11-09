// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qlist.h>
#include <QtCore/qpoint.h>

#include <vector>

class QDebug;

/*!
    \class QEasingCurve
    \since 4.6
    \brief The QEasingCurve class provides easing curves for controlling animation.

    An easing curve maps the progress of an animation, from 0 to 1, to the
    progress of the animated value, so that a movement can start slowly,
    overshoot or bounce. valueForProgress() evaluates the curve:

    \code
    QEasingCurve curve(QEasingCurve::OutBounce);
    for (qreal t = 0; t <= 1; t += 0.1)
        y = start + (end - start) * curve.valueForProgress(t);
    \endcode

    The elastic curves use amplitude() and period(), the bounce curves
    amplitude(), and the back curves overshoot(). BezierSpline and TCBSpline
    curves are built from segments with addCubicBezierSegment() and
    addTCBSegment().

    \differences
    The exponential curves start at exactly 0 and end at exactly 1, without
    the small offset of Qt's formulas. InCurve and OutCurve follow the sine
    ease-in and ease-out shapes. There is no QDataStream support.
    \enddifferences

    \sa QTimeLine
*/
class QEasingCurve {
public:
    /*!
        \enum QEasingCurve::Type

        \value Linear Linear (t) function: velocity is constant.
        \value InQuad Quadratic (t^2) easing in: accelerating from zero velocity.
        \value OutQuad Quadratic easing out: decelerating to zero velocity.
        \value InOutQuad Quadratic easing in and out.
        \value OutInQuad Quadratic easing out and in.
        \value InCubic Cubic (t^3) easing in.
        \value OutCubic Cubic easing out.
        \value InOutCubic Cubic easing in and out.
        \value OutInCubic Cubic easing out and in.
        \value InQuart Quartic (t^4) easing in.
        \value OutQuart Quartic easing out.
        \value InOutQuart Quartic easing in and out.
        \value OutInQuart Quartic easing out and in.
        \value InQuint Quintic (t^5) easing in.
        \value OutQuint Quintic easing out.
        \value InOutQuint Quintic easing in and out.
        \value OutInQuint Quintic easing out and in.
        \value InSine Sinusoidal easing in.
        \value OutSine Sinusoidal easing out.
        \value InOutSine Sinusoidal easing in and out.
        \value OutInSine Sinusoidal easing out and in.
        \value InExpo Exponential (2^t) easing in.
        \value OutExpo Exponential easing out.
        \value InOutExpo Exponential easing in and out.
        \value OutInExpo Exponential easing out and in.
        \value InCirc Circular (sqrt(1-t^2)) easing in.
        \value OutCirc Circular easing out.
        \value InOutCirc Circular easing in and out.
        \value OutInCirc Circular easing out and in.
        \value InElastic Elastic (exponentially decaying sine wave) easing in.
        \value OutElastic Elastic easing out.
        \value InOutElastic Elastic easing in and out.
        \value OutInElastic Elastic easing out and in.
        \value InBack Back (overshooting cubic) easing in.
        \value OutBack Back easing out.
        \value InOutBack Back easing in and out.
        \value OutInBack Back easing out and in.
        \value InBounce Bounce (exponentially decaying parabolic bounce) easing in.
        \value OutBounce Bounce easing out.
        \value InOutBounce Bounce easing in and out.
        \value OutInBounce Bounce easing out and in.
        \value InCurve A curve that starts slowly and speeds up.
        \value OutCurve A curve that starts fast and slows down.
        \value SineCurve A sine wave going from 0 to 1 and back to 0.
        \value CosineCurve A cosine wave starting and ending at 0.5.
        \value BezierSpline A curve made of cubic Bézier segments.
        \value TCBSpline A curve made of Kochanek-Bartels (TCB) spline segments.
        \value Custom A curve given by a function; see setCustomType().
        \omitvalue NCurveTypes
    */
    enum Type {
        Linear,
        InQuad,
        OutQuad,
        InOutQuad,
        OutInQuad,
        InCubic,
        OutCubic,
        InOutCubic,
        OutInCubic,
        InQuart,
        OutQuart,
        InOutQuart,
        OutInQuart,
        InQuint,
        OutQuint,
        InOutQuint,
        OutInQuint,
        InSine,
        OutSine,
        InOutSine,
        OutInSine,
        InExpo,
        OutExpo,
        InOutExpo,
        OutInExpo,
        InCirc,
        OutCirc,
        InOutCirc,
        OutInCirc,
        InElastic,
        OutElastic,
        InOutElastic,
        OutInElastic,
        InBack,
        OutBack,
        InOutBack,
        OutInBack,
        InBounce,
        OutBounce,
        InOutBounce,
        OutInBounce,
        InCurve,
        OutCurve,
        SineCurve,
        CosineCurve,
        BezierSpline,
        TCBSpline,
        Custom,
        NCurveTypes
    };
    /*! \typedef QEasingCurve::EasingFunction

        A pointer to a function that maps progress (0 to 1) to a value, as used by setCustomType(). */
    using EasingFunction = qreal (*)(qreal progress);

    QEasingCurve(Type type = Linear);
    /*!
        Constructs a copy of \a other.
    */
    QEasingCurve(const QEasingCurve &other) = default;
    /*!
        Move-constructs a curve from \a other.
    */
    QEasingCurve(QEasingCurve &&other) noexcept = default;
    /*!
        Destroys the curve.
    */
    ~QEasingCurve() = default;
    /*!
        Assigns \a other to this curve and returns a reference to it.
    */
    QEasingCurve &operator=(const QEasingCurve &other) = default;
    /*!
        Move-assigns \a other to this curve and returns a reference to it.
    */
    QEasingCurve &operator=(QEasingCurve &&other) noexcept = default;
    /*!
        Swaps this curve with \a other.
    */
    void swap(QEasingCurve &other) noexcept { std::swap(*this, other); }

    qreal amplitude() const;
    void setAmplitude(qreal amplitude);
    qreal period() const;
    void setPeriod(qreal period);
    qreal overshoot() const;
    void setOvershoot(qreal overshoot);

    void addCubicBezierSegment(const QPointF &c1, const QPointF &c2, const QPointF &endPoint);
    void addTCBSegment(const QPointF &nextPoint, qreal t, qreal c, qreal b);
    QList<QPointF> toCubicSpline() const;

    Type type() const;
    void setType(Type type);
    void setCustomType(EasingFunction func);
    EasingFunction customType() const;
    qreal valueForProgress(qreal progress) const;

    friend bool operator==(const QEasingCurve &a, const QEasingCurve &b);
    friend bool operator!=(const QEasingCurve &a, const QEasingCurve &b) { return !(a == b); }

private:
    struct TcbPoint {
        QPointF point;
        qreal t, c, b;
    };
    Type m_type = Linear;
    qreal m_amplitude = -1; // negative: the default of the type
    qreal m_period = -1;
    qreal m_overshoot = -1;
    EasingFunction m_custom = nullptr;
    std::vector<QPointF> m_bezier; // control point 1, control point 2, end point, ...
    std::vector<TcbPoint> m_tcb;
};

QDebug operator<<(QDebug debug, const QEasingCurve &item);
