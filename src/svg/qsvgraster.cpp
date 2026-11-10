// MIT License - see LICENSE
#include "qsvgraster_p.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <initializer_list>

namespace QtSvgPrivate {

namespace {

const double kPi = 3.14159265358979323846;

int clampInt(double v, int lo, int hi) {
    if (!(v > lo))
        return lo;
    if (v > hi)
        return hi;
    return static_cast<int>(v);
}

// Multiplies the four channels of a pixel by factor / 255, rounding exactly.
std::uint32_t scalePixel(std::uint32_t p, unsigned factor) {
    if (factor >= 255u)
        return p;
    std::uint32_t rb = (p & 0x00ff00ffu) * factor + 0x00800080u;
    rb = ((rb + ((rb >> 8u) & 0x00ff00ffu)) >> 8u) & 0x00ff00ffu;
    std::uint32_t ag = ((p >> 8u) & 0x00ff00ffu) * factor + 0x00800080u;
    ag = (ag + ((ag >> 8u) & 0x00ff00ffu)) & 0xff00ff00u;
    return rb | ag;
}

void blendPixel(std::uint32_t *dst, std::uint32_t src) {
    const unsigned alpha = src >> 24u;
    if (alpha == 255u) {
        *dst = src;
        return;
    }
    if (src == 0u)
        return;
    *dst = src + scalePixel(*dst, 255u - alpha);
}

} // namespace

Mat Mat::operator*(const Mat &o) const {
    return {a * o.a + c * o.b, b * o.a + d * o.b,     a * o.c + c * o.d,
            b * o.c + d * o.d, a * o.e + c * o.f + e, b * o.e + d * o.f + f};
}

bool Mat::invert(Mat *out) const {
    const double det = a * d - b * c;
    if (!std::isfinite(det) || std::fabs(det) < 1e-300)
        return false;
    const double i = 1.0 / det;
    *out = {d * i, -b * i, -c * i, a * i, (c * f - d * e) * i, (b * e - a * f) * i};
    return true;
}

double Mat::scale() const { return std::sqrt(std::fabs(a * d - b * c)); }

const char *scanNumber(const char *p, const char *end, double *out) {
    const char *q = p;
    bool negative = false;
    if (q < end && (*q == '+' || *q == '-'))
        negative = *q++ == '-';
    double mantissa = 0.0;
    int exponent = 0;
    bool digits = false;
    for (; q < end && *q >= '0' && *q <= '9'; ++q) {
        mantissa = mantissa * 10.0 + (*q - '0');
        digits = true;
    }
    if (q < end && *q == '.') {
        const char *dot = q++;
        bool fraction = false;
        for (; q < end && *q >= '0' && *q <= '9'; ++q) {
            mantissa = mantissa * 10.0 + (*q - '0');
            --exponent;
            fraction = true;
        }
        if (!fraction && !digits)
            return nullptr;
        if (!fraction)
            q = dot + 1;
        digits = true;
    }
    if (!digits)
        return nullptr;
    if (q < end && (*q == 'e' || *q == 'E')) {
        const char *x = q + 1;
        bool negativeExponent = false;
        if (x < end && (*x == '+' || *x == '-'))
            negativeExponent = *x++ == '-';
        if (x < end && *x >= '0' && *x <= '9') {
            int value = 0;
            for (; x < end && *x >= '0' && *x <= '9'; ++x)
                value = std::min(value * 10 + (*x - '0'), 10000);
            exponent += negativeExponent ? -value : value;
            q = x;
        }
    }
    const double v = mantissa * std::pow(10.0, exponent);
    if (!std::isfinite(v))
        return nullptr;
    *out = negative ? -v : v;
    return q;
}

void Box::add(Pt p) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y))
        return;
    if (!valid) {
        x0 = x1 = p.x;
        y0 = y1 = p.y;
        valid = true;
        return;
    }
    x0 = std::min(x0, p.x);
    y0 = std::min(y0, p.y);
    x1 = std::max(x1, p.x);
    y1 = std::max(y1, p.y);
}

void Box::add(const Box &other) {
    if (!other.valid)
        return;
    add(Pt{other.x0, other.y0});
    add(Pt{other.x1, other.y1});
}

void Path::moveTo(Pt p) {
    m_verbs.push_back(Move);
    m_points.push_back(p);
    m_start = m_current = p;
}

void Path::lineTo(Pt p) {
    if (m_verbs.empty())
        moveTo(m_current);
    m_verbs.push_back(Line);
    m_points.push_back(p);
    m_current = p;
}

void Path::cubicTo(Pt c1, Pt c2, Pt p) {
    if (m_verbs.empty())
        moveTo(m_current);
    m_verbs.push_back(Cubic);
    m_points.push_back(c1);
    m_points.push_back(c2);
    m_points.push_back(p);
    m_current = p;
}

void Path::quadTo(Pt c, Pt p) {
    const Pt s = m_current;
    cubicTo({s.x + 2.0 / 3.0 * (c.x - s.x), s.y + 2.0 / 3.0 * (c.y - s.y)},
            {p.x + 2.0 / 3.0 * (c.x - p.x), p.y + 2.0 / 3.0 * (c.y - p.y)}, p);
}

void Path::close() {
    if (m_verbs.empty() || m_verbs.back() == Close)
        return;
    m_verbs.push_back(Close);
    m_current = m_start;
}

// The conversion from the endpoint to the center form of an arc is the one in the implementation notes of
// SVG 1.1 (appendix F.6); the arc is then drawn as cubic curves of at most a quarter turn.
void Path::arcTo(double rx, double ry, double rotationDegrees, bool largeArc, bool sweep, Pt p) {
    const Pt s = m_current;
    rx = std::fabs(rx);
    ry = std::fabs(ry);
    if (s.x == p.x && s.y == p.y)
        return;
    if (rx < 1e-12 || ry < 1e-12) {
        lineTo(p);
        return;
    }
    const double phi = rotationDegrees * kPi / 180.0;
    const double cosPhi = std::cos(phi), sinPhi = std::sin(phi);
    const double dx = (s.x - p.x) / 2.0, dy = (s.y - p.y) / 2.0;
    const double x1 = cosPhi * dx + sinPhi * dy;
    const double y1 = -sinPhi * dx + cosPhi * dy;
    const double lambda = (x1 * x1) / (rx * rx) + (y1 * y1) / (ry * ry);
    if (lambda > 1.0) {
        const double grow = std::sqrt(lambda);
        rx *= grow;
        ry *= grow;
    }
    const double num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
    const double den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
    double coef = den > 0.0 ? std::sqrt(std::max(0.0, num / den)) : 0.0;
    if (largeArc == sweep)
        coef = -coef;
    const double cxp = coef * rx * y1 / ry;
    const double cyp = -coef * ry * x1 / rx;
    const double cx = cosPhi * cxp - sinPhi * cyp + (s.x + p.x) / 2.0;
    const double cy = sinPhi * cxp + cosPhi * cyp + (s.y + p.y) / 2.0;
    const double theta1 = std::atan2((y1 - cyp) / ry, (x1 - cxp) / rx);
    double delta = std::atan2((-y1 - cyp) / ry, (-x1 - cxp) / rx) - theta1;
    if (sweep && delta < 0.0)
        delta += 2.0 * kPi;
    else if (!sweep && delta > 0.0)
        delta -= 2.0 * kPi;
    if (!std::isfinite(delta) || !std::isfinite(cx) || !std::isfinite(cy)) {
        lineTo(p);
        return;
    }
    const int pieces = std::max(1, static_cast<int>(std::ceil(std::fabs(delta) / (kPi / 2.0) - 1e-9)));
    const double step = delta / pieces;
    const double k = 4.0 / 3.0 * std::tan(step / 4.0);
    auto point = [&](double angle, double *tx, double *ty) {
        const double ca = std::cos(angle), sa = std::sin(angle);
        *tx = -rx * sa * cosPhi - ry * ca * sinPhi;
        *ty = -rx * sa * sinPhi + ry * ca * cosPhi;
        return Pt{cx + rx * ca * cosPhi - ry * sa * sinPhi, cy + rx * ca * sinPhi + ry * sa * cosPhi};
    };
    double angle = theta1;
    double tx0 = 0.0, ty0 = 0.0;
    Pt from = point(angle, &tx0, &ty0);
    for (int i = 0; i < pieces; ++i) {
        angle += step;
        double tx1 = 0.0, ty1 = 0.0;
        Pt to = point(angle, &tx1, &ty1);
        if (i == pieces - 1)
            to = p;
        cubicTo({from.x + k * tx0, from.y + k * ty0}, {to.x - k * tx1, to.y - k * ty1}, to);
        from = to;
        tx0 = tx1;
        ty0 = ty1;
    }
}

void Path::addRect(double x, double y, double w, double h, double rx, double ry) {
    rx = std::min(std::max(rx, 0.0), w / 2.0);
    ry = std::min(std::max(ry, 0.0), h / 2.0);
    if (rx <= 0.0 || ry <= 0.0) {
        moveTo({x, y});
        lineTo({x + w, y});
        lineTo({x + w, y + h});
        lineTo({x, y + h});
        close();
        return;
    }
    moveTo({x + rx, y});
    lineTo({x + w - rx, y});
    arcTo(rx, ry, 0.0, false, true, {x + w, y + ry});
    lineTo({x + w, y + h - ry});
    arcTo(rx, ry, 0.0, false, true, {x + w - rx, y + h});
    lineTo({x + rx, y + h});
    arcTo(rx, ry, 0.0, false, true, {x, y + h - ry});
    lineTo({x, y + ry});
    arcTo(rx, ry, 0.0, false, true, {x + rx, y});
    close();
}

void Path::addEllipse(double cx, double cy, double rx, double ry) {
    moveTo({cx + rx, cy});
    arcTo(rx, ry, 0.0, false, true, {cx, cy + ry});
    arcTo(rx, ry, 0.0, false, true, {cx - rx, cy});
    arcTo(rx, ry, 0.0, false, true, {cx, cy - ry});
    arcTo(rx, ry, 0.0, false, true, {cx + rx, cy});
    close();
}

namespace {

// Reads the numbers and flags of path data.
class PathScanner {
public:
    PathScanner(const char *data, std::size_t size) : m_p(data), m_end(data + size) {}

    void skipSpace(bool comma) {
        while (m_p < m_end && (std::isspace(static_cast<unsigned char>(*m_p)) || (comma && *m_p == ','))) {
            if (*m_p == ',')
                comma = false;
            ++m_p;
        }
    }
    bool atEnd() {
        skipSpace(false);
        return m_p >= m_end;
    }
    bool atCommand() const { return m_p < m_end && std::isalpha(static_cast<unsigned char>(*m_p)) != 0; }
    char command() { return *m_p++; }

    bool number(double *out) {
        skipSpace(true);
        const char *after = scanNumber(m_p, m_end, out);
        if (!after)
            return false;
        m_p = after;
        return true;
    }

    bool flag(bool *out) {
        skipSpace(true);
        if (m_p >= m_end || (*m_p != '0' && *m_p != '1'))
            return false;
        *out = *m_p++ == '1';
        return true;
    }

private:
    const char *m_p;
    const char *m_end;
};

} // namespace

Path Path::fromSvg(const char *data, std::size_t size) {
    Path path;
    PathScanner in(data, size);
    char command = 0;
    Pt lastControl;
    char lastCommand = 0;
    bool first = true;
    while (!in.atEnd()) {
        if (in.atCommand()) {
            command = in.command();
        } else if (command == 0 || command == 'Z' || command == 'z') {
            break;
        }
        if (first && command != 'M' && command != 'm')
            break;
        first = false;
        const bool relative = command >= 'a';
        const Pt cur = path.currentPoint();
        const Pt base = relative ? cur : Pt{};
        double v[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
        auto read = [&](int count) {
            for (int i = 0; i < count; ++i) {
                if (!in.number(&v[i]))
                    return false;
            }
            return true;
        };
        bool ok = true;
        switch (command) {
        case 'M':
        case 'm':
            ok = read(2);
            if (ok) {
                path.moveTo({base.x + v[0], base.y + v[1]});
                command = relative ? 'l' : 'L'; // further pairs are lines
            }
            break;
        case 'L':
        case 'l':
            ok = read(2);
            if (ok)
                path.lineTo({base.x + v[0], base.y + v[1]});
            break;
        case 'H':
        case 'h':
            ok = read(1);
            if (ok)
                path.lineTo({base.x + v[0], cur.y});
            break;
        case 'V':
        case 'v':
            ok = read(1);
            if (ok)
                path.lineTo({cur.x, base.y + v[0]});
            break;
        case 'C':
        case 'c':
            ok = read(6);
            if (ok) {
                lastControl = {base.x + v[2], base.y + v[3]};
                path.cubicTo({base.x + v[0], base.y + v[1]}, lastControl, {base.x + v[4], base.y + v[5]});
            }
            break;
        case 'S':
        case 's':
            ok = read(4);
            if (ok) {
                const bool smooth = lastCommand == 'C' || lastCommand == 'S';
                const Pt c1 = smooth ? Pt{2.0 * cur.x - lastControl.x, 2.0 * cur.y - lastControl.y} : cur;
                lastControl = {base.x + v[0], base.y + v[1]};
                path.cubicTo(c1, lastControl, {base.x + v[2], base.y + v[3]});
            }
            break;
        case 'Q':
        case 'q':
            ok = read(4);
            if (ok) {
                lastControl = {base.x + v[0], base.y + v[1]};
                path.quadTo(lastControl, {base.x + v[2], base.y + v[3]});
            }
            break;
        case 'T':
        case 't':
            ok = read(2);
            if (ok) {
                const bool smooth = lastCommand == 'Q' || lastCommand == 'T';
                lastControl = smooth ? Pt{2.0 * cur.x - lastControl.x, 2.0 * cur.y - lastControl.y} : cur;
                path.quadTo(lastControl, {base.x + v[0], base.y + v[1]});
            }
            break;
        case 'A':
        case 'a': {
            bool large = false, sweep = false;
            ok = read(3) && in.flag(&large) && in.flag(&sweep) && in.number(&v[3]) && in.number(&v[4]);
            if (ok)
                path.arcTo(v[0], v[1], v[2], large, sweep, {base.x + v[3], base.y + v[4]});
            break;
        }
        case 'Z':
        case 'z':
            path.close();
            break;
        default:
            ok = false;
            break;
        }
        if (!ok)
            break;
        lastCommand = static_cast<char>(std::toupper(static_cast<unsigned char>(command)));
    }
    return path;
}

std::vector<Path::Contour> Path::flatten(double tolerance) const {
    std::vector<Contour> out;
    tolerance = std::max(tolerance, 1e-9);
    std::size_t pi = 0;
    Pt cur;
    // What follows a closed subpath without a move starts a new one at the same point.
    bool restart = false;
    const auto begin = [&] {
        if (restart) {
            out.emplace_back();
            out.back().points.push_back(cur);
            restart = false;
        }
    };
    for (const Verb verb : m_verbs) {
        switch (verb) {
        case Move:
            out.emplace_back();
            cur = m_points[pi++];
            out.back().points.push_back(cur);
            restart = false;
            break;
        case Line:
            begin();
            cur = m_points[pi++];
            out.back().points.push_back(cur);
            break;
        case Cubic: {
            begin();
            const Pt c1 = m_points[pi], c2 = m_points[pi + 1], p = m_points[pi + 2];
            pi += 3;
            const double d1 = std::hypot(cur.x - 2.0 * c1.x + c2.x, cur.y - 2.0 * c1.y + c2.y);
            const double d2 = std::hypot(c1.x - 2.0 * c2.x + p.x, c1.y - 2.0 * c2.y + p.y);
            const int n = clampInt(std::ceil(std::sqrt(0.75 * std::max(d1, d2) / tolerance)), 1, 500);
            for (int i = 1; i <= n; ++i) {
                const double t = static_cast<double>(i) / n, u = 1.0 - t;
                const double w0 = u * u * u, w1 = 3.0 * u * u * t, w2 = 3.0 * u * t * t, w3 = t * t * t;
                out.back().points.push_back(
                    {w0 * cur.x + w1 * c1.x + w2 * c2.x + w3 * p.x, w0 * cur.y + w1 * c1.y + w2 * c2.y + w3 * p.y});
            }
            cur = p;
            break;
        }
        case Close:
            if (!out.empty() && !restart) {
                out.back().closed = true;
                cur = out.back().points.front();
                restart = true;
            }
            break;
        }
    }
    // A move that nothing follows draws nothing, not even caps.
    out.erase(std::remove_if(out.begin(), out.end(), [](const Contour &c) { return c.points.size() < 2 && !c.closed; }),
              out.end());
    return out;
}

void Polygons::add(const Pt *pts, std::size_t count) {
    if (count < 3)
        return;
    double area = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const Pt &p = pts[i], &q = pts[(i + 1) % count];
        area += p.x * q.y - q.x * p.y;
    }
    if (area >= 0.0)
        points.insert(points.end(), pts, pts + count);
    else
        for (std::size_t i = count; i > 0; --i)
            points.push_back(pts[i - 1]);
    ends.push_back(points.size());
}

void fillContours(const std::vector<Path::Contour> &contours, const Mat &m, Polygons *out) {
    for (const Path::Contour &c : contours) {
        if (c.points.size() < 3)
            continue;
        // Kept as drawn: the fill rule needs the direction of each contour.
        for (const Pt &p : c.points)
            out->points.push_back(m.map(p));
        out->ends.push_back(out->points.size());
    }
}

namespace {

class Stroker {
public:
    Stroker(const StrokeStyle &style, const Mat &m, double tolerance, Polygons *out)
        : m_style(style), m_m(m), m_out(out), m_hw(style.width / 2.0) {
        const double ratio = std::min(1.0, std::max(tolerance, 1e-9) / std::max(m_hw, 1e-9));
        m_circleSteps = clampInt(std::ceil(kPi / std::acos(1.0 - ratio)), 8, 720);
    }

    void stroke(const std::vector<Pt> &raw, bool closed) {
        std::vector<Pt> pts;
        pts.reserve(raw.size());
        for (const Pt &p : raw) {
            if (pts.empty() || std::hypot(p.x - pts.back().x, p.y - pts.back().y) > 1e-12)
                pts.push_back(p);
        }
        if (closed && pts.size() > 1 && std::hypot(pts.front().x - pts.back().x, pts.front().y - pts.back().y) <= 1e-12)
            pts.pop_back();
        if (pts.size() < 2) {
            // A dot: only round and square caps show.
            if (pts.empty() || raw.empty())
                return;
            if (m_style.cap == LineCap::Round)
                circle(pts[0]);
            else if (m_style.cap == LineCap::Square)
                polygon({{pts[0].x - m_hw, pts[0].y - m_hw},
                         {pts[0].x + m_hw, pts[0].y - m_hw},
                         {pts[0].x + m_hw, pts[0].y + m_hw},
                         {pts[0].x - m_hw, pts[0].y + m_hw}});
            return;
        }
        const std::size_t n = pts.size();
        const std::size_t segments = closed ? n : n - 1;
        std::vector<Pt> normals(segments);
        for (std::size_t i = 0; i < segments; ++i) {
            const Pt &a = pts[i], &b = pts[(i + 1) % n];
            const double len = std::hypot(b.x - a.x, b.y - a.y);
            normals[i] = {-(b.y - a.y) / len * m_hw, (b.x - a.x) / len * m_hw};
        }
        for (std::size_t i = 0; i < segments; ++i) {
            Pt a = pts[i], b = pts[(i + 1) % n];
            const Pt nrm = normals[i];
            if (!closed && m_style.cap == LineCap::Square) {
                // The direction of the segment, half a width long, is the normal turned back.
                const Pt dir{nrm.y, -nrm.x};
                if (i == 0)
                    a = {a.x - dir.x, a.y - dir.y};
                if (i == segments - 1)
                    b = {b.x + dir.x, b.y + dir.y};
            }
            polygon({{a.x + nrm.x, a.y + nrm.y},
                     {b.x + nrm.x, b.y + nrm.y},
                     {b.x - nrm.x, b.y - nrm.y},
                     {a.x - nrm.x, a.y - nrm.y}});
        }
        for (std::size_t i = closed ? 0 : 1; i < segments; ++i)
            join(pts[i], normals[(i + segments - 1) % segments], normals[i]);
        if (!closed && m_style.cap == LineCap::Round) {
            circle(pts.front());
            circle(pts.back());
        }
    }

private:
    void polygon(std::initializer_list<Pt> pts) {
        m_scratch.clear();
        for (const Pt &p : pts)
            m_scratch.push_back(m_m.map(p));
        m_out->add(m_scratch.data(), m_scratch.size());
    }

    void circle(Pt c) {
        m_scratch.clear();
        for (int i = 0; i < m_circleSteps; ++i) {
            const double a = 2.0 * kPi * i / m_circleSteps;
            m_scratch.push_back(m_m.map({c.x + m_hw * std::cos(a), c.y + m_hw * std::sin(a)}));
        }
        m_out->add(m_scratch.data(), m_scratch.size());
    }

    void join(Pt v, Pt n0, Pt n1) {
        const double cross = n0.x * n1.y - n0.y * n1.x;
        const double dot = n0.x * n1.x + n0.y * n1.y;
        const double hw2 = m_hw * m_hw;
        if (std::fabs(cross) < 1e-12 * hw2 && dot > 0.0)
            return; // straight on
        // The outer side of the turn is where the offsets of the two segments leave a gap.
        const double side = cross > 0.0 ? -1.0 : 1.0;
        const Pt a{v.x + side * n0.x, v.y + side * n0.y};
        const Pt b{v.x + side * n1.x, v.y + side * n1.y};
        // Between the points of a flattened curve the turn is slight, and a bevel is as good as any join.
        const bool slight = dot > 0.985 * hw2;
        if (m_style.join == LineJoin::Round && !slight) {
            circle(v);
            return;
        }
        if (m_style.join == LineJoin::Miter && !slight) {
            // The miter is 1/sin(theta/2) half widths long, where theta is the angle between the segments.
            const double cosHalf2 = (1.0 + dot / hw2) / 2.0; // cos^2 of half the angle between the normals
            if (cosHalf2 > 1e-12 && 1.0 / std::sqrt(cosHalf2) <= m_style.miterLimit) {
                const double mx = n0.x + n1.x, my = n0.y + n1.y;
                const double k = side / (2.0 * cosHalf2);
                polygon({v, a, {v.x + mx * k, v.y + my * k}, b});
                return;
            }
        }
        polygon({v, a, b});
    }

    const StrokeStyle &m_style;
    Mat m_m;
    Polygons *m_out;
    double m_hw;
    int m_circleSteps = 16;
    std::vector<Pt> m_scratch;
};

// Cuts a polyline into the pieces that a dash pattern leaves.
void dashContour(const Path::Contour &contour, const std::vector<double> &pattern, double offset,
                 std::vector<Path::Contour> *out) {
    std::vector<Pt> pts = contour.points;
    if (contour.closed && pts.size() > 1)
        pts.push_back(pts.front());
    if (pts.size() < 2)
        return;
    double total = 0.0;
    for (const double d : pattern)
        total += d;
    std::size_t index = 0;
    double phase = std::fmod(offset, total);
    if (phase < 0.0)
        phase += total;
    while (phase >= pattern[index]) {
        phase -= pattern[index];
        index = (index + 1) % pattern.size();
    }
    double left = pattern[index] - phase;
    bool on = index % 2 == 0;
    Path::Contour piece;
    if (on)
        piece.points.push_back(pts[0]);
    std::size_t budget = 200000; // a pattern much finer than the path is not worth drawing dash by dash
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        Pt a = pts[i];
        const Pt b = pts[i + 1];
        double len = std::hypot(b.x - a.x, b.y - a.y);
        while (len > left && budget > 0) {
            --budget;
            const double t = left / len;
            a = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
            len -= left;
            if (on) {
                piece.points.push_back(a);
                out->push_back(piece);
                piece.points.clear();
            } else {
                piece.points.push_back(a);
            }
            on = !on;
            index = (index + 1) % pattern.size();
            left = pattern[index];
        }
        left -= len;
        if (on)
            piece.points.push_back(b);
    }
    if (on && !piece.points.empty())
        out->push_back(piece);
}

} // namespace

void strokeContours(const std::vector<Path::Contour> &contours, const StrokeStyle &style, const Mat &m,
                    double tolerance, Polygons *out) {
    if (!(style.width > 0.0) || !std::isfinite(style.width))
        return;
    std::vector<double> pattern = style.dashes;
    double total = 0.0;
    bool valid = !pattern.empty();
    for (const double d : pattern) {
        if (!(d >= 0.0) || !std::isfinite(d))
            valid = false;
        total += d;
    }
    if (!valid || total <= 1e-9) {
        pattern.clear();
    } else if (pattern.size() % 2 == 1) {
        const std::vector<double> copy = pattern;
        pattern.insert(pattern.end(), copy.begin(), copy.end());
    }
    Stroker stroker(style, m, tolerance, out);
    if (pattern.empty()) {
        for (const Path::Contour &c : contours)
            stroker.stroke(c.points, c.closed);
        return;
    }
    std::vector<Path::Contour> dashed;
    for (const Path::Contour &c : contours)
        dashContour(c, pattern, style.dashOffset, &dashed);
    for (const Path::Contour &c : dashed)
        stroker.stroke(c.points, false);
}

void Shader::setStops(const std::vector<GradientStop> &stops, double opacity) {
    const std::size_t size = 256;
    table.assign(size, 0u);
    if (stops.empty())
        return;
    std::size_t s = 0;
    for (std::size_t i = 0; i < size; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(size - 1);
        while (s + 1 < stops.size() && stops[s + 1].offset <= t)
            ++s;
        GradientStop c = stops[s];
        if (t < stops[0].offset) {
            c = stops[0];
        } else if (s + 1 < stops.size()) {
            const GradientStop &n = stops[s + 1];
            const double span = n.offset - c.offset;
            const double k = span > 0.0 ? (t - c.offset) / span : 1.0;
            c.r += (n.r - c.r) * k;
            c.g += (n.g - c.g) * k;
            c.b += (n.b - c.b) * k;
            c.a += (n.a - c.a) * k;
        }
        const double a = std::min(1.0, std::max(0.0, c.a * opacity));
        const auto channel = [a](double v) {
            return static_cast<std::uint32_t>(clampInt(v * a * 255.0 + 0.5, 0, 255));
        };
        table[i] = (static_cast<std::uint32_t>(clampInt(a * 255.0 + 0.5, 0, 255)) << 24u) | (channel(c.r) << 16u) |
                   (channel(c.g) << 8u) | channel(c.b);
    }
}

std::uint32_t Shader::at(double x, double y) const {
    if (!gradient)
        return color;
    if (table.empty())
        return 0u;
    const Pt p = toGradient.map({x, y});
    double t = 0.0;
    if (!radial) {
        const double vx = p2.x - p1.x, vy = p2.y - p1.y;
        const double len2 = vx * vx + vy * vy;
        t = len2 > 0.0 ? ((p.x - p1.x) * vx + (p.y - p1.y) * vy) / len2 : 1.0;
    } else if (radius <= 0.0) {
        t = 1.0;
    } else {
        // The circle of the gradient at t has its center on the line from the focal point (t = 0) to the
        // center (t = 1) and the radius t * radius; find the t whose circle passes through p.
        const double dx = p.x - p2.x, dy = p.y - p2.y;
        const double cdx = p1.x - p2.x, cdy = p1.y - p2.y;
        const double a = cdx * cdx + cdy * cdy - radius * radius;
        if (a > -1e-12) {
            t = std::hypot(dx, dy) / radius;
        } else {
            const double b = dx * cdx + dy * cdy;
            const double c = dx * dx + dy * dy;
            t = (b - std::sqrt(std::max(0.0, b * b - a * c))) / a;
        }
    }
    if (!std::isfinite(t))
        t = 0.0;
    switch (spread) {
    case Spread::Pad:
        break;
    case Spread::Repeat:
        t -= std::floor(t);
        break;
    case Spread::Reflect:
        t = std::fabs(t - 2.0 * std::floor((t + 1.0) / 2.0));
        break;
    }
    const int last = static_cast<int>(table.size()) - 1;
    return table[static_cast<std::size_t>(clampInt(t * last + 0.5, 0, last))];
}

// The coverage of each pixel is accumulated exactly from the edges, as signed areas: each edge adds, on every
// row it crosses, the area to its right, and a running sum along the row gives the winding number times the
// covered part of the pixel. The fill rule is applied to that sum.
void Surface::fill(const Polygons &polygons, FillRule rule, const Shader &shader) {
    if (width <= 0 || height <= 0 || polygons.points.empty())
        return;
    Box box;
    for (const Pt &p : polygons.points)
        box.add(p);
    if (!box.valid)
        return;
    const int x0 = clampInt(std::floor(box.x0), 0, width);
    const int x1 = clampInt(std::ceil(box.x1), 0, width);
    const int y0 = clampInt(std::floor(box.y0), 0, height);
    const int y1 = clampInt(std::ceil(box.y1), 0, height);
    if (x1 <= x0 || y1 <= y0)
        return;
    const int w = x1 - x0, h = y1 - y0;
    const std::size_t stride = static_cast<std::size_t>(w) + 2;
    std::vector<float> acc(stride * static_cast<std::size_t>(h), 0.0f);

    // Adds an edge whose x coordinates lie within 0 and w.
    const auto addEdge = [&](Pt p0, Pt p1) {
        if (p0.y == p1.y)
            return;
        const float dir = p0.y < p1.y ? 1.0f : -1.0f;
        if (p0.y > p1.y)
            std::swap(p0, p1);
        if (p1.y <= 0.0 || p0.y >= h)
            return;
        const double dxdy = (p1.x - p0.x) / (p1.y - p0.y);
        double x = p0.x;
        if (p0.y < 0.0)
            x -= p0.y * dxdy;
        const int rowEnd = std::min(h, static_cast<int>(std::ceil(p1.y)));
        for (int y = std::max(0, static_cast<int>(std::floor(p0.y))); y < rowEnd; ++y) {
            float *row = acc.data() + static_cast<std::size_t>(y) * stride;
            const double dy = std::min<double>(y + 1, p1.y) - std::max<double>(y, p0.y);
            const double xnext = std::min<double>(w, std::max(0.0, x + dxdy * dy));
            const float d = static_cast<float>(dy) * dir;
            const double xa = std::min(x, xnext), xb = std::max(x, xnext);
            const double xaFloor = std::floor(xa);
            const int xai = static_cast<int>(xaFloor);
            const double xbCeil = std::ceil(xb);
            const int xbi = static_cast<int>(xbCeil);
            if (xbi <= xai + 1) {
                const float xmf = static_cast<float>(0.5 * (x + xnext) - xaFloor);
                row[xai] += d - d * xmf;
                row[xai + 1] += d * xmf;
            } else {
                const float s = static_cast<float>(1.0 / (xb - xa));
                const float xaf = static_cast<float>(xa - xaFloor);
                const float a0 = 0.5f * s * (1.0f - xaf) * (1.0f - xaf);
                const float xbf = static_cast<float>(xb - xbCeil + 1.0);
                const float am = 0.5f * s * xbf * xbf;
                row[xai] += d * a0;
                if (xbi == xai + 2) {
                    row[xai + 1] += d * (1.0f - a0 - am);
                } else {
                    const float a1 = s * (1.5f - xaf);
                    row[xai + 1] += d * (a1 - a0);
                    for (int xi = xai + 2; xi < xbi - 1; ++xi)
                        row[xi] += d * s;
                    const float a2 = a1 + static_cast<float>(xbi - xai - 3) * s;
                    row[xbi - 1] += d * (1.0f - a2 - am);
                }
                row[xbi] += d * am;
            }
            x = xnext;
        }
    };
    // Splits an edge where it leaves the columns of the buffer. What lies to the left of them still counts,
    // as an edge along the first column; what lies to the right covers nothing that is kept.
    const auto addClipped = [&](Pt p0, Pt p1) {
        p0.x -= x0;
        p1.x -= x0;
        p0.y -= y0;
        p1.y -= y0;
        if (!std::isfinite(p0.x) || !std::isfinite(p0.y) || !std::isfinite(p1.x) || !std::isfinite(p1.y))
            return;
        double cuts[4] = {0.0, 1.0, 1.0, 1.0};
        int count = 1;
        if (p0.x != p1.x) {
            for (const double edge : {0.0, static_cast<double>(w)}) {
                const double t = (edge - p0.x) / (p1.x - p0.x);
                if (t > 0.0 && t < 1.0)
                    cuts[count++] = t;
            }
            if (count == 3 && cuts[1] > cuts[2])
                std::swap(cuts[1], cuts[2]);
        }
        cuts[count] = 1.0;
        for (int i = 0; i < count; ++i) {
            const auto at = [&](double t) {
                return Pt{std::min<double>(w, std::max(0.0, p0.x + (p1.x - p0.x) * t)), p0.y + (p1.y - p0.y) * t};
            };
            addEdge(at(cuts[i]), at(cuts[i + 1]));
        }
    };

    std::size_t begin = 0;
    for (const std::size_t end : polygons.ends) {
        for (std::size_t i = begin; i < end; ++i)
            addClipped(polygons.points[i], polygons.points[i + 1 < end ? i + 1 : begin]);
        begin = end;
    }

    const bool solid = !shader.gradient;
    for (int y = 0; y < h; ++y) {
        const float *row = acc.data() + static_cast<std::size_t>(y) * stride;
        std::uint32_t *dst = pixels.data() + static_cast<std::size_t>(y + y0) * static_cast<std::size_t>(width) +
                             static_cast<std::size_t>(x0);
        float sum = 0.0f;
        for (int x = 0; x < w; ++x) {
            sum += row[x];
            float cover = std::fabs(sum);
            if (rule == FillRule::EvenOdd) {
                cover = std::fmod(cover, 2.0f);
                if (cover > 1.0f)
                    cover = 2.0f - cover;
            } else if (cover > 1.0f) {
                cover = 1.0f;
            }
            const unsigned c = static_cast<unsigned>(std::lround(cover * 255.0f));
            if (c == 0u)
                continue;
            const std::uint32_t src = solid ? shader.color : shader.at(x + x0 + 0.5, y + y0 + 0.5);
            blendPixel(dst + x, scalePixel(src, c));
        }
    }
}

void Surface::composite(const Surface &layer, double opacity, const Surface *mask) {
    if (layer.width != width || layer.height != height)
        return;
    const unsigned factor = static_cast<unsigned>(clampInt(opacity * 255.0 + 0.5, 0, 255));
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        std::uint32_t src = layer.pixels[i];
        if (src == 0u)
            continue;
        unsigned f = factor;
        if (mask) {
            f = (f * (mask->pixels[i] >> 24u) + 127u) / 255u;
        }
        if (f == 0u)
            continue;
        src = scalePixel(src, f);
        blendPixel(&pixels[i], src);
    }
}

} // namespace QtSvgPrivate
