// MIT License - see LICENSE
#include <QtCore/qeasingcurve.h>

#include <QtCore/qdebug.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr qreal kPi = 3.14159265358979323846;
constexpr qreal kDefaultAmplitude = 1.0;
constexpr qreal kDefaultPeriod = 0.3;
constexpr qreal kDefaultOvershoot = 1.70158;

// The "in" shapes; the out, in-out and out-in variants are derived from them.
enum class Shape { Quad, Cubic, Quart, Quint, Sine, Expo, Circ, Elastic, Back, Bounce };

qreal elasticIn(qreal t, qreal a, qreal p) {
    if (t <= 0)
        return 0;
    if (t >= 1)
        return 1;
    qreal s;
    if (a < 1) {
        a = 1;
        s = p / 4;
    } else {
        s = p / (2 * kPi) * std::asin(1 / a);
    }
    t -= 1;
    return -(a * std::pow(2.0, 10 * t) * std::sin((t - s) * (2 * kPi) / p));
}

// Bounces on the way out; the amplitude scales the height of the bounces.
qreal bounceOut(qreal t, qreal a) {
    if (t >= 1)
        return 1;
    if (t < 4 / 11.0)
        return 7.5625 * t * t;
    if (t < 8 / 11.0) {
        t -= 6 / 11.0;
        return -a * (1 - (7.5625 * t * t + .75)) + 1;
    }
    if (t < 10 / 11.0) {
        t -= 9 / 11.0;
        return -a * (1 - (7.5625 * t * t + .9375)) + 1;
    }
    t -= 21 / 22.0;
    return -a * (1 - (7.5625 * t * t + .984375)) + 1;
}

qreal shapeIn(Shape s, qreal t, qreal a, qreal p, qreal o) {
    switch (s) {
    case Shape::Quad:
        return t * t;
    case Shape::Cubic:
        return t * t * t;
    case Shape::Quart:
        return t * t * t * t;
    case Shape::Quint:
        return t * t * t * t * t;
    case Shape::Sine:
        return 1 - std::cos(t * kPi / 2);
    case Shape::Expo:
        return t <= 0 ? 0 : (t >= 1 ? 1 : std::pow(2.0, 10 * (t - 1)));
    case Shape::Circ:
        return 1 - std::sqrt(std::max<qreal>(0, 1 - t * t));
    case Shape::Elastic:
        return elasticIn(t, a, p);
    case Shape::Back:
        return t * t * ((o + 1) * t - o);
    case Shape::Bounce:
        return 1 - bounceOut(1 - t, a);
    }
    return t;
}

qreal shapeOut(Shape s, qreal t, qreal a, qreal p, qreal o) {
    if (s == Shape::Bounce)
        return bounceOut(t, a);
    return 1 - shapeIn(s, 1 - t, a, p, o);
}

qreal splineValue(const std::vector<QPointF> &bezier, qreal x) {
    // Segments start at (0, 0); find the one that contains x.
    QPointF start(0, 0);
    for (std::size_t k = 0; k + 2 < bezier.size(); k += 3) {
        const QPointF &c1 = bezier[k], &c2 = bezier[k + 1], &end = bezier[k + 2];
        if (x <= end.x() || k + 3 >= bezier.size()) {
            auto coord = [](qreal p0, qreal p1, qreal p2, qreal p3, qreal t) {
                const qreal u = 1 - t;
                return u * u * u * p0 + 3 * u * u * t * p1 + 3 * u * t * t * p2 + t * t * t * p3;
            };
            // Solve x(t) = x by bisection; x(t) is monotonic for a valid easing spline.
            qreal lo = 0, hi = 1, t = 0.5;
            for (int i = 0; i < 60; ++i) {
                t = (lo + hi) / 2;
                if (coord(start.x(), c1.x(), c2.x(), end.x(), t) < x)
                    lo = t;
                else
                    hi = t;
            }
            return coord(start.y(), c1.y(), c2.y(), end.y(), t);
        }
        start = end;
    }
    return x;
}

} // namespace

/*!
    Constructs an easing curve of the given \a type.
*/
QEasingCurve::QEasingCurve(Type type) { setType(type); }

/*!
    Returns the amplitude, which the elastic and bounce curves use. The
    default is 1.0.

    \sa setAmplitude()
*/
qreal QEasingCurve::amplitude() const { return m_amplitude < 0 ? kDefaultAmplitude : m_amplitude; }

/*!
    Sets the amplitude to \a amplitude. It controls the height of the bounces
    of the bounce curves and of the oscillations of the elastic curves.

    \sa amplitude()
*/
void QEasingCurve::setAmplitude(qreal amplitude) { m_amplitude = amplitude; }

/*!
    Returns the period of the elastic curves. The default is 0.3.

    \sa setPeriod()
*/
qreal QEasingCurve::period() const { return m_period < 0 ? kDefaultPeriod : m_period; }

/*!
    Sets the period of the elastic curves to \a period. A small period gives
    a high frequency.

    \sa period()
*/
void QEasingCurve::setPeriod(qreal period) { m_period = period; }

/*!
    Returns the overshoot of the back curves. The default is 1.70158, an
    overshoot of 10 percent.

    \sa setOvershoot()
*/
qreal QEasingCurve::overshoot() const { return m_overshoot < 0 ? kDefaultOvershoot : m_overshoot; }

/*!
    Sets the overshoot of the back curves to \a overshoot.

    \sa overshoot()
*/
void QEasingCurve::setOvershoot(qreal overshoot) { m_overshoot = overshoot; }

/*!
    \since 5.0
    Adds a cubic Bézier segment with the control points \a c1 and \a c2 that
    ends at \a endPoint to a BezierSpline curve. The first segment starts at
    (0, 0), and the last should end at (1, 1).

    \sa toCubicSpline()
*/
void QEasingCurve::addCubicBezierSegment(const QPointF &c1, const QPointF &c2, const QPointF &endPoint) {
    m_bezier.push_back(c1);
    m_bezier.push_back(c2);
    m_bezier.push_back(endPoint);
}

/*!
    \since 5.0
    Adds a segment ending at \a nextPoint to a TCBSpline curve, with the
    tension \a t, continuity \a c and bias \a b at that point. The curve
    starts at (0, 0), and the last point should be (1, 1).
*/
void QEasingCurve::addTCBSegment(const QPointF &nextPoint, qreal t, qreal c, qreal b) {
    m_tcb.push_back({nextPoint, t, c, b});
}

/*!
    \since 5.0
    Returns the control and end points of the cubic Bézier segments of a
    BezierSpline or TCBSpline curve, three points per segment. A TCB spline
    is converted to Bézier segments.
*/
QList<QPointF> QEasingCurve::toCubicSpline() const {
    if (m_type == TCBSpline && !m_tcb.empty()) {
        std::vector<QPointF> pts{QPointF(0, 0)};
        std::vector<const TcbPoint *> params{nullptr};
        for (const TcbPoint &p : m_tcb) {
            pts.push_back(p.point);
            params.push_back(&p);
        }
        // Kochanek-Bartels tangents: incoming and outgoing at each point.
        auto tangents = [&](std::size_t i, QPointF *in, QPointF *out) {
            const qreal t = params[i] ? params[i]->t : 0, c = params[i] ? params[i]->c : 0,
                        b = params[i] ? params[i]->b : 0;
            const QPointF prev = i > 0 ? pts[i] - pts[i - 1] : QPointF(0, 0);
            const QPointF next = i + 1 < pts.size() ? pts[i + 1] - pts[i] : QPointF(0, 0);
            const QPointF p = i > 0 ? prev : next, n = i + 1 < pts.size() ? next : prev;
            *out = p * ((1 - t) * (1 + c) * (1 + b) / 2) + n * ((1 - t) * (1 - c) * (1 - b) / 2);
            *in = p * ((1 - t) * (1 - c) * (1 + b) / 2) + n * ((1 - t) * (1 + c) * (1 - b) / 2);
        };
        QList<QPointF> r;
        for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
            QPointF in0, out0, in1, out1;
            tangents(i, &in0, &out0);
            tangents(i + 1, &in1, &out1);
            r << pts[i] + out0 / 3 << pts[i + 1] - in1 / 3 << pts[i + 1];
        }
        return r;
    }
    return QList<QPointF>(m_bezier.begin(), m_bezier.end());
}

/*!
    Returns the type of the curve.

    \sa setType()
*/
QEasingCurve::Type QEasingCurve::type() const { return m_type; }

/*!
    Sets the type of the curve to \a type. Setting Custom without a function
    has no effect; use setCustomType().
*/
void QEasingCurve::setType(Type type) {
    if (type < Linear || type >= NCurveTypes || (type == Custom && !m_custom))
        return;
    m_type = type;
}

/*!
    Makes this a Custom curve that calls \a func to map progress to a value.

    \sa customType()
*/
void QEasingCurve::setCustomType(EasingFunction func) {
    m_custom = func;
    if (func)
        m_type = Custom;
}

/*!
    Returns the function of a Custom curve, or \c nullptr.
*/
QEasingCurve::EasingFunction QEasingCurve::customType() const { return m_type == Custom ? m_custom : nullptr; }

/*!
    Returns the value of the curve for \a progress, which is clamped to the
    range 0 to 1. The result is 0 at progress 0 and 1 at progress 1 for all
    curves except SineCurve and CosineCurve, and may leave that range in
    between (for the elastic and back curves).
*/
qreal QEasingCurve::valueForProgress(qreal progress) const {
    const qreal t = std::clamp<qreal>(progress, 0, 1);
    const qreal a = amplitude(), p = period(), o = overshoot();
    if (m_type == Linear)
        return t;
    if (m_type >= InQuad && m_type <= OutInBounce) {
        const auto shape = static_cast<Shape>((m_type - InQuad) / 4);
        switch ((m_type - InQuad) % 4) {
        case 0:
            return shapeIn(shape, t, a, p, o);
        case 1:
            return shapeOut(shape, t, a, p, o);
        case 2: {
            // The back curves overshoot more in the in-out form, as in Penner's equations.
            const qreal oo = shape == Shape::Back ? o * 1.525 : o;
            return t < 0.5 ? shapeIn(shape, 2 * t, a, p, oo) / 2 : 0.5 + shapeOut(shape, 2 * t - 1, a, p, oo) / 2;
        }
        default:
            return t < 0.5 ? shapeOut(shape, 2 * t, a, p, o) / 2 : 0.5 + shapeIn(shape, 2 * t - 1, a, p, o) / 2;
        }
    }
    switch (m_type) {
    case InCurve:
        return shapeIn(Shape::Sine, t, a, p, o);
    case OutCurve:
        return shapeOut(Shape::Sine, t, a, p, o);
    case SineCurve:
        return (std::sin(t * 2 * kPi - kPi / 2) + 1) / 2;
    case CosineCurve:
        return (std::cos(t * 2 * kPi - kPi / 2) + 1) / 2;
    case BezierSpline:
        return m_bezier.empty() ? t : splineValue(m_bezier, t);
    case TCBSpline: {
        if (m_tcb.empty())
            return t;
        const QList<QPointF> spline = toCubicSpline();
        return splineValue(std::vector<QPointF>(spline.begin(), spline.end()), t);
    }
    case Custom:
        return m_custom ? m_custom(t) : t;
    default:
        return t;
    }
}

bool operator==(const QEasingCurve &a, const QEasingCurve &b) {
    if (a.m_type != b.m_type || a.amplitude() != b.amplitude() || a.period() != b.period() ||
        a.overshoot() != b.overshoot())
        return false;
    if (a.m_type == QEasingCurve::Custom && a.m_custom != b.m_custom)
        return false;
    if (a.m_type == QEasingCurve::BezierSpline && a.m_bezier != b.m_bezier)
        return false;
    if (a.m_type == QEasingCurve::TCBSpline && a.toCubicSpline() != b.toCubicSpline())
        return false;
    return true;
}

// Writes the type and parameters of the curve to the debug stream.
QDebug operator<<(QDebug debug, const QEasingCurve &item) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "QEasingCurve(type=" << static_cast<int>(item.type()) << ", amplitude=" << item.amplitude()
                    << ", period=" << item.period() << ", overshoot=" << item.overshoot() << ")";
    return debug;
}
