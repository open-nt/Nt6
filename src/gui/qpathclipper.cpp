// MIT License - see LICENSE
//
// Set operations on the areas of paths (QPainterPath::united(), intersected(), subtracted() and simplified(),
// and the same on QPolygonF and QPolygon), and QPainterPathStroker.
//
// The clipper works on the paths with their curves divided into lines. Every point is snapped to a fine grid (a
// power of two, so that snapping is exact and equal points compare equal), every edge is cut where any other edge
// crosses or touches it, and points that end up next to each other are made one. That gives a planar graph of
// segments; for each segment it is known exactly how the winding number of each path changes across it, since
// every edge of a path adds one from its right to its left. The faces of the graph are traced, the winding numbers
// of one face of each connected part are found by casting a ray against the paths, and those of the other faces
// follow across the segments. A face is inside the result or not by the fill rules and the operation, the segments
// between faces inside and outside are kept, turned to have the inside on their left, and are joined into loops.
// As every face gets one pair of winding numbers, the loops always close.
#include <QtGui/qpainterpath.h>
#include <QtGui/qpen.h>
#include <QtGui/qpolygon.h>

#include "qrasterpaintengine_p.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

enum class Op { Union, Intersect, Subtract, Simplify };

using Key = std::pair<double, double>;

Key keyOf(const QPointF &p) { return {p.x(), p.y()}; }

qreal cross(const QPointF &a, const QPointF &b) { return a.x() * b.y() - a.y() * b.x(); }
qreal dot(const QPointF &a, const QPointF &b) { return a.x() * b.x() + a.y() * b.y(); }

QPointF snap(const QPointF &p, double grid) {
    return QPointF(std::nearbyint(p.x() / grid) * grid, std::nearbyint(p.y() / grid) * grid);
}

struct Edge {
    QPointF a, b;
};

// The edges of the closed polygons of a path, with its fill rule, and bands across them that find the edges a
// horizontal line meets.
class Area {
public:
    Area(const QPainterPath &path, double grid, double y0, double y1)
        : m_windingFill(path.fillRule() == Qt::WindingFill), m_y0(y0) {
        for (const QPolygonF &poly : path.toFillPolygons()) {
            for (qsizetype i = 0; i + 1 < poly.size(); ++i) {
                const QPointF a = snap(poly.at(i), grid), b = snap(poly.at(i + 1), grid);
                if (a != b)
                    m_edges.push_back(Edge{a, b});
            }
        }
        const std::size_t count = m_edges.size();
        m_bands.resize(std::clamp<std::size_t>(count / 4, 1, 4096));
        m_bandHeight = std::max((y1 - y0) / static_cast<double>(m_bands.size()), grid);
        for (std::size_t i = 0; i < count; ++i) {
            const Edge &e = m_edges[i];
            const std::size_t first = band(std::min(e.a.y(), e.b.y())), last = band(std::max(e.a.y(), e.b.y()));
            for (std::size_t k = first; k <= last; ++k)
                m_bands[k].push_back(i);
        }
    }

    const std::vector<Edge> &edges() const { return m_edges; }

    // The winding number at `p`, by a ray to the right: an edge that goes down counts one, one that goes up minus
    // one.
    int winding(const QPointF &p) const {
        int winding = 0;
        for (const std::size_t i : m_bands[band(p.y())]) {
            const Edge &e = m_edges[i];
            if ((e.a.y() <= p.y()) == (e.b.y() <= p.y()))
                continue;
            const qreal x = e.a.x() + (p.y() - e.a.y()) * (e.b.x() - e.a.x()) / (e.b.y() - e.a.y());
            if (x > p.x())
                winding += e.b.y() > e.a.y() ? 1 : -1;
        }
        return winding;
    }

    // Whether a winding number is inside, by the fill rule.
    bool inside(int winding) const { return m_windingFill ? winding != 0 : (winding % 2) != 0; }

private:
    std::size_t band(double y) const {
        const double k = std::floor((y - m_y0) / m_bandHeight);
        if (!(k > 0.0))
            return 0;
        return std::min(static_cast<std::size_t>(k), m_bands.size() - 1);
    }

    std::vector<Edge> m_edges;
    std::vector<std::vector<std::size_t>> m_bands;
    bool m_windingFill = false;
    double m_y0 = 0.0;
    double m_bandHeight = 1.0;
};

// An edge to be cut, with the points where it is cut.
struct Piece {
    QPointF a, b;
    bool second = false; // an edge of the second path
    bool changed = true; // new, or moved, since crossings were last looked for
    std::vector<QPointF> cuts;
    double x0 = 0.0, x1 = 0.0, y0 = 0.0, y1 = 0.0;
};

// Records where the edges `p` and `q` cross, touch or overlap, as cuts in each.
void cutAtCrossing(Piece &p, Piece &q, double grid) {
    const QPointF r = p.b - p.a, s = q.b - q.a;
    const qreal rr = dot(r, r), ss = dot(s, s);
    const auto onP = [&](const QPointF &x) { return x != p.a && x != p.b; };
    const auto onQ = [&](const QPointF &x) { return x != q.a && x != q.b; };
    // Whether a point is on the line of an edge, to within the grid.
    const auto nearLine = [&](const QPointF &x, const QPointF &a, const QPointF &d, qreal dd) {
        return std::fabs(cross(d, x - a)) <= 2.0 * grid * std::sqrt(dd);
    };
    // Whether a point is on an edge, between its ends, to within the grid.
    const auto between = [&](const QPointF &x, const QPointF &a, const QPointF &d, qreal dd) {
        const qreal t = dot(x - a, d) / dd;
        return t > 0.0 && t < 1.0 && nearLine(x, a, d, dd);
    };
    // On one line, judged against the line of the longer edge: each is cut where the other ends. Snapping can leave
    // such edges at a slight angle, which must not count as a crossing.
    const bool collinear = rr >= ss ? nearLine(q.a, p.a, r, rr) && nearLine(q.b, p.a, r, rr)
                                    : nearLine(p.a, q.a, s, ss) && nearLine(p.b, q.a, s, ss);
    if (collinear) {
        for (const QPointF &x : {q.a, q.b})
            if (onP(x) && dot(x - p.a, r) > 0.0 && dot(x - p.b, r) < 0.0)
                p.cuts.push_back(x);
        for (const QPointF &x : {p.a, p.b})
            if (onQ(x) && dot(x - q.a, s) > 0.0 && dot(x - q.b, s) < 0.0)
                q.cuts.push_back(x);
        return;
    }
    // Edges that share an end, and are not on one line, meet nowhere else.
    if (p.a == q.a || p.a == q.b || p.b == q.a || p.b == q.b)
        return;
    // An end of one on the other: they meet exactly there. Found apart from the crossing, which is imprecise when
    // the edges meet at a slight angle.
    bool touching = false;
    for (const QPointF &x : {q.a, q.b})
        if (between(x, p.a, r, rr)) {
            p.cuts.push_back(x);
            touching = true;
        }
    for (const QPointF &x : {p.a, p.b})
        if (between(x, q.a, s, ss)) {
            q.cuts.push_back(x);
            touching = true;
        }
    if (touching)
        return;
    const qreal denom = cross(r, s);
    if (std::fabs(denom) <= 1e-12 * std::sqrt(rr * ss))
        return; // parallel, apart
    const QPointF qp = q.a - p.a;
    const qreal t = cross(qp, s) / denom, u = cross(qp, r) / denom;
    if (t <= 0.0 || t >= 1.0 || u <= 0.0 || u >= 1.0)
        return;
    const QPointF x = snap(p.a + r * t, grid);
    if (onP(x))
        p.cuts.push_back(x);
    if (onQ(x))
        q.cuts.push_back(x);
}

bool keep(Op op, bool inA, bool inB) {
    switch (op) {
    case Op::Union:
        return inA || inB;
    case Op::Intersect:
        return inA && inB;
    case Op::Subtract:
        return inA && !inB;
    case Op::Simplify:
        return inA;
    }
    return false;
}

// The area that `op` makes of the areas of `a` and `b`, as closed polygons of lines.
QPainterPath clip(const QPainterPath &a, const QPainterPath &b, Op op) {
    QPainterPath result;
    QRectF bounds = a.controlPointRect();
    if (op != Op::Simplify)
        bounds = bounds.united(b.controlPointRect());
    const double extent = std::max({std::fabs(bounds.left()), std::fabs(bounds.right()), std::fabs(bounds.top()),
                                    std::fabs(bounds.bottom()), 1.0});
    if (!std::isfinite(extent))
        return result;
    // About 36 bits below the largest coordinate: far finer than a pixel, far coarser than rounding errors.
    const double grid = std::ldexp(1.0, std::ilogb(extent) - 36);
    const double y0 = bounds.top() - 4.0 * grid, y1 = bounds.bottom() + 4.0 * grid;
    const Area areaA(a, grid, y0, y1);
    const Area areaB(op == Op::Simplify ? QPainterPath() : b, grid, y0, y1);

    std::vector<Piece> pieces;
    pieces.reserve(areaA.edges().size() + areaB.edges().size());
    for (const Area *area : {&areaA, &areaB})
        for (const Edge &e : area->edges()) {
            Piece p;
            p.a = e.a;
            p.b = e.b;
            p.second = area == &areaB;
            pieces.push_back(std::move(p));
        }
    if (pieces.empty())
        return result;

    // Crossings are worked out pair by pair, so where three edges meet the points can come out a grid step or two
    // apart. Points that close are made one, the ends of the edges first, so that they stay where they are.
    struct BucketHash {
        std::size_t operator()(const std::pair<long long, long long> &k) const {
            return std::hash<long long>()(k.first) ^ (std::hash<long long>()(k.second) * 0x9e3779b97f4a7c15ULL);
        }
    };
    std::unordered_map<std::pair<long long, long long>, std::vector<QPointF>, BucketHash> buckets;
    const double bucketSize = 8.0 * grid, together = 4.0 * grid;
    const auto merged = [&](const QPointF &p) {
        const long long bx = std::llround(std::floor(p.x() / bucketSize));
        const long long by = std::llround(std::floor(p.y() / bucketSize));
        for (long long dx = -1; dx <= 1; ++dx)
            for (long long dy = -1; dy <= 1; ++dy) {
                const auto it = buckets.find({bx + dx, by + dy});
                if (it == buckets.end())
                    continue;
                for (const QPointF &q : it->second)
                    if (std::fabs(q.x() - p.x()) <= together && std::fabs(q.y() - p.y()) <= together)
                        return q;
            }
        buckets[{bx, by}].push_back(p);
        return p;
    };

    // The edges are cut where they cross or touch, into pieces that meet only at their ends. Points are moved a
    // little by snapping and merging, which can make pieces cross that did not; those that changed are looked at
    // again until none do.
    for (int round = 0; round < 8; ++round) {
        for (Piece &p : pieces) {
            p.x0 = std::min(p.a.x(), p.b.x()) - 2.0 * grid;
            p.x1 = std::max(p.a.x(), p.b.x()) + 2.0 * grid;
            p.y0 = std::min(p.a.y(), p.b.y()) - 2.0 * grid;
            p.y1 = std::max(p.a.y(), p.b.y()) + 2.0 * grid;
        }
        // Every pair whose boxes overlap, found by sweeping across x.
        std::sort(pieces.begin(), pieces.end(), [](const Piece &l, const Piece &r) { return l.x0 < r.x0; });
        for (std::size_t i = 0; i < pieces.size(); ++i)
            for (std::size_t j = i + 1; j < pieces.size() && pieces[j].x0 <= pieces[i].x1; ++j)
                if ((pieces[i].changed || pieces[j].changed) && pieces[j].y0 <= pieces[i].y1 &&
                    pieces[i].y0 <= pieces[j].y1)
                    cutAtCrossing(pieces[i], pieces[j], grid);
        for (Piece &p : pieces) {
            const QPointF from = merged(p.a), to = merged(p.b);
            p.changed = from != p.a || to != p.b;
            p.a = from;
            p.b = to;
        }
        std::vector<Piece> cut;
        cut.reserve(pieces.size());
        bool changed = false;
        for (Piece &p : pieces) {
            const QPointF d = p.b - p.a;
            std::sort(p.cuts.begin(), p.cuts.end(),
                      [&](const QPointF &l, const QPointF &r) { return dot(l - p.a, d) < dot(r - p.a, d); });
            p.cuts.push_back(p.b);
            const bool split = p.cuts.size() > 1;
            QPointF from = p.a;
            for (const QPointF &c : p.cuts) {
                const QPointF to = merged(c);
                if (to == from)
                    continue;
                Piece piece;
                piece.a = from;
                piece.b = to;
                piece.second = p.second;
                piece.changed = p.changed || split;
                changed = changed || piece.changed;
                cut.push_back(std::move(piece));
                from = to;
            }
        }
        pieces.swap(cut);
        if (!changed)
            break;
    }

    // The segments, each once whichever way it goes, with how much the winding numbers of the two paths go up from
    // its right to its left, going from its lesser point to its greater one.
    struct Segment {
        Key from, to;
        int a = 0, b = 0;
    };
    std::vector<Segment> segments;
    segments.reserve(pieces.size());
    for (const Piece &p : pieces) {
        const Key from = keyOf(p.a), to = keyOf(p.b);
        const int sign = from < to ? 1 : -1;
        segments.push_back(Segment{std::min(from, to), std::max(from, to), p.second ? 0 : sign, p.second ? sign : 0});
    }
    std::sort(segments.begin(), segments.end(),
              [](const Segment &l, const Segment &r) { return std::tie(l.from, l.to) < std::tie(r.from, r.to); });
    std::size_t count = 0;
    for (std::size_t i = 0; i < segments.size();) {
        Segment s = segments[i];
        for (++i; i < segments.size() && segments[i].from == s.from && segments[i].to == s.to; ++i) {
            s.a += segments[i].a;
            s.b += segments[i].b;
        }
        if (s.a != 0 || s.b != 0) // a segment that changes nothing is no boundary between faces
            segments[count++] = s;
    }
    segments.resize(count);
    if (segments.empty())
        return result;

    // The graph: points, and two half-edges for each segment, the first from its lesser point.
    std::vector<Key> keys;
    keys.reserve(segments.size() * 2);
    for (const Segment &s : segments) {
        keys.push_back(s.from);
        keys.push_back(s.to);
    }
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    const auto vertexOf = [&](const Key &k) {
        return static_cast<std::size_t>(std::lower_bound(keys.begin(), keys.end(), k) - keys.begin());
    };
    const std::size_t halves = segments.size() * 2;
    std::vector<std::size_t> origin(halves);
    std::vector<double> angle(halves);
    std::vector<std::vector<std::size_t>> leaving(keys.size());
    for (std::size_t i = 0; i < segments.size(); ++i) {
        const std::size_t u = vertexOf(segments[i].from), v = vertexOf(segments[i].to);
        const double dx = keys[v].first - keys[u].first, dy = keys[v].second - keys[u].second;
        origin[2 * i] = u;
        origin[2 * i + 1] = v;
        angle[2 * i] = std::atan2(dy, dx);
        angle[2 * i + 1] = std::atan2(-dy, -dx);
        leaving[u].push_back(2 * i);
        leaving[v].push_back(2 * i + 1);
    }
    // Around each point, counterclockwise.
    std::vector<std::size_t> position(halves);
    for (std::vector<std::size_t> &around : leaving) {
        std::sort(around.begin(), around.end(), [&](std::size_t l, std::size_t r) { return angle[l] < angle[r]; });
        for (std::size_t k = 0; k < around.size(); ++k)
            position[around[k]] = k;
    }
    const auto changeA = [&](std::size_t h) { return h % 2 == 0 ? segments[h / 2].a : -segments[h / 2].a; };
    const auto changeB = [&](std::size_t h) { return h % 2 == 0 ? segments[h / 2].b : -segments[h / 2].b; };
    // The half-edge after `h` around the face on its left: the sharpest turn to the left at its end, which is the
    // next one clockwise from the way back.
    const auto next = [&](std::size_t h) {
        const std::size_t back = h ^ 1;
        const std::vector<std::size_t> &around = leaving[origin[back]];
        return around[(position[back] + around.size() - 1) % around.size()];
    };

    // The faces, as the cycles of half-edges around them.
    constexpr std::size_t none = static_cast<std::size_t>(-1);
    std::vector<std::size_t> face(halves, none);
    std::vector<std::size_t> faceStart;
    for (std::size_t h = 0; h < halves; ++h) {
        if (face[h] != none)
            continue;
        const std::size_t f = faceStart.size();
        faceStart.push_back(h);
        for (std::size_t e = h; face[e] == none; e = next(e))
            face[e] = f;
    }

    // The winding numbers of the faces. The leftmost point of each connected part has the face around the part on
    // the left of its last half-edge counterclockwise; a ray from just left of the point gives its winding numbers,
    // and they change across each segment by what the segment carries.
    std::vector<std::size_t> part(keys.size());
    std::iota(part.begin(), part.end(), std::size_t(0));
    const auto root = [&](std::size_t v) {
        while (part[v] != v)
            v = part[v] = part[part[v]];
        return v;
    };
    for (std::size_t i = 0; i < halves; i += 2)
        part[root(origin[i])] = root(origin[i + 1]);
    std::vector<int> windingA(faceStart.size(), 0), windingB(faceStart.size(), 0);
    std::vector<bool> known(faceStart.size(), false), seeded(keys.size(), false);
    std::vector<std::size_t> queue;
    for (std::size_t v = 0; v < keys.size(); ++v) { // the keys are sorted by x, then y
        if (seeded[root(v)] || leaving[v].empty())
            continue;
        seeded[root(v)] = true;
        const std::size_t outside = face[leaving[v].back()];
        if (known[outside])
            continue;
        const QPointF probe(keys[v].first - 64.0 * grid, keys[v].second);
        windingA[outside] = areaA.winding(probe);
        windingB[outside] = areaB.winding(probe);
        known[outside] = true;
        queue.assign(1, outside);
        while (!queue.empty()) {
            const std::size_t f = queue.back();
            queue.pop_back();
            std::size_t e = faceStart[f];
            do {
                const std::size_t other = face[e ^ 1];
                if (!known[other]) {
                    windingA[other] = windingA[f] - changeA(e);
                    windingB[other] = windingB[f] - changeB(e);
                    known[other] = true;
                    queue.push_back(other);
                }
                e = next(e);
            } while (e != faceStart[f]);
        }
    }
    std::vector<bool> in(faceStart.size());
    for (std::size_t f = 0; f < faceStart.size(); ++f)
        in[f] = keep(op, areaA.inside(windingA[f]), areaB.inside(windingB[f]));

    // The half-edges with the result inside on their left and outside on their right, joined into loops: at each
    // point the loop takes the sharpest turn to the left, which keeps shapes that only touch at a corner apart.
    std::vector<bool> kept(halves), used(halves, false);
    for (std::size_t h = 0; h < halves; ++h)
        kept[h] = in[face[h]] && !in[face[h ^ 1]];
    for (std::size_t start = 0; start < halves; ++start) {
        if (!kept[start] || used[start])
            continue;
        QPolygonF loop;
        std::size_t h = start;
        do {
            used[h] = true;
            loop.append(QPointF(keys[origin[h]].first, keys[origin[h]].second));
            const std::size_t back = h ^ 1;
            const std::vector<std::size_t> &around = leaving[origin[back]];
            std::size_t k = position[back];
            do
                k = (k + around.size() - 1) % around.size();
            while (!kept[around[k]] && around[k] != back);
            h = around[k];
        } while (kept[h] && !used[h]);
        // Points between two edges going the same way are dropped.
        QPolygonF clean;
        const qsizetype n = loop.size();
        for (qsizetype i = 0; i < n; ++i) {
            const QPointF &prev = loop.at((i + n - 1) % n), &here = loop.at(i), &following = loop.at((i + 1) % n);
            const QPointF d0 = here - prev, d1 = following - here;
            if (std::fabs(cross(d0, d1)) <= 1e-12 * std::sqrt(dot(d0, d0) * dot(d1, d1)) && dot(d0, d1) > 0.0)
                continue;
            clean.append(here);
        }
        if (clean.size() < 3)
            continue;
        qreal area = 0.0;
        for (qsizetype i = 0; i < clean.size(); ++i)
            area += cross(clean.at(i), clean.at((i + 1) % clean.size()));
        if (std::fabs(area) <= grid * grid * static_cast<double>(clean.size()))
            continue;
        result.moveTo(clean.first());
        for (qsizetype i = 1; i < clean.size(); ++i)
            result.lineTo(clean.at(i));
        result.closeSubpath();
    }
    return result;
}

// Whether the control points of the paths are apart, with a gap between them: then so are their areas.
bool apart(const QPainterPath &a, const QPainterPath &b) {
    const QRectF r = a.controlPointRect(), s = b.controlPointRect();
    return r.right() < s.left() || s.right() < r.left() || r.bottom() < s.top() || s.bottom() < r.top();
}

QPainterPath pathOf(const QPolygonF &polygon) {
    QPainterPath path;
    path.addPolygon(polygon);
    path.closeSubpath();
    return path;
}

} // namespace

// ================================================================================================================

/*!
    \since 4.3

    Returns a path of the area that is inside this path, inside \a p, or inside both.

    Each path fills by its own fill rule, and a subpath that is not closed counts as closed. The result is
    made of closed polygons, with curves divided into lines, and uses Qt::OddEvenFill. Where the areas do
    not overlap and the fill rules are the same, the result is the two paths together, curves and all.

    \code
    QPainterPath a, b;
    a.addRect(0, 0, 20, 20);
    b.addEllipse(10, 10, 20, 20);
    painter.fillPath(a.united(b), Qt::blue);
    \endcode

    \sa intersected(), subtracted(), simplified()
*/
QPainterPath QPainterPath::united(const QPainterPath &p) const {
    if (p.isEmpty())
        return *this;
    if (isEmpty())
        return p;
    if (fillRule() == p.fillRule() && apart(*this, p)) {
        QPainterPath result = *this;
        result.addPath(p);
        return result;
    }
    return clip(*this, p, Op::Union);
}

/*!
    \since 4.3

    Returns a path of the area that is inside both this path and \a p, as closed polygons using
    Qt::OddEvenFill. The result is empty if the paths do not overlap.

    \sa united(), subtracted(), intersects()
*/
QPainterPath QPainterPath::intersected(const QPainterPath &p) const {
    if (isEmpty() || p.isEmpty() || apart(*this, p))
        return QPainterPath();
    return clip(*this, p, Op::Intersect);
}

/*!
    \since 4.3

    Returns a path of the area that is inside this path but not inside \a p, as closed polygons using
    Qt::OddEvenFill. If the paths do not overlap, the result is this path.

    \sa united(), intersected()
*/
QPainterPath QPainterPath::subtracted(const QPainterPath &p) const {
    if (isEmpty())
        return QPainterPath();
    if (p.isEmpty() || apart(*this, p))
        return *this;
    return clip(*this, p, Op::Subtract);
}

/*!
    \since 4.3

    Returns the area of this path, by its fill rule, as closed polygons that neither cross nor overlap.
    Subpaths that intersect are merged, and consecutive lines in one direction become one. The result uses
    Qt::OddEvenFill, and its curves are divided into lines.

    QPainterPathStroker::createStroke() gives outlines that overlap; this makes one outline of them:

    \code
    const QPainterPath outline = QPainterPathStroker(pen).createStroke(path).simplified();
    \endcode
*/
QPainterPath QPainterPath::simplified() const {
    if (isEmpty())
        return QPainterPath();
    return clip(*this, QPainterPath(), Op::Simplify);
}

/*!
    \since 4.3
    \overload

    Returns \c true if the area of \a p is inside the area of this path; otherwise returns \c false.
*/
bool QPainterPath::contains(const QPainterPath &p) const {
    if (p.isEmpty())
        return false;
    if (isEmpty() || apart(*this, p))
        return false;
    return clip(p, *this, Op::Subtract).isEmpty();
}

/*!
    \since 4.3
    \overload

    Returns \c true if the areas of this path and \a p overlap; otherwise returns \c false.
*/
bool QPainterPath::intersects(const QPainterPath &p) const { return !intersected(p).isEmpty(); }

/*!
    \fn QPainterPath QPainterPath::operator&(const QPainterPath &other) const
    \since 4.5
    Returns the intersection of this path and \a other.
    \sa intersected()
*/
/*!
    \fn QPainterPath QPainterPath::operator|(const QPainterPath &other) const
    \since 4.5
    Returns the union of this path and \a other.
    \sa united()
*/
/*!
    \fn QPainterPath QPainterPath::operator+(const QPainterPath &other) const
    \since 4.5
    Returns the union of this path and \a other, as operator|() does.
    \sa united(), addPath()
*/
/*!
    \fn QPainterPath QPainterPath::operator-(const QPainterPath &other) const
    \since 4.5
    Returns this path with \a other subtracted.
    \sa subtracted()
*/
/*!
    \fn QPainterPath &QPainterPath::operator&=(const QPainterPath &other)
    \since 4.5
    Makes this path its intersection with \a other and returns a reference to it.
*/
/*!
    \fn QPainterPath &QPainterPath::operator|=(const QPainterPath &other)
    \since 4.5
    Makes this path its union with \a other and returns a reference to it.
*/
/*!
    \fn QPainterPath &QPainterPath::operator+=(const QPainterPath &other)
    \since 4.5
    Makes this path its union with \a other and returns a reference to it.
*/
/*!
    \fn QPainterPath &QPainterPath::operator-=(const QPainterPath &other)
    \since 4.5
    Subtracts \a other from this path and returns a reference to it.
*/

// ================================================================================================================

/*!
    \since 4.3
    Returns the area inside this polygon or \a r, or both, as a closed polygon. Several separate areas come
    one after the other in the one polygon.
    \sa QPainterPath::united()
*/
QPolygonF QPolygonF::united(const QPolygonF &r) const { return pathOf(*this).united(pathOf(r)).toFillPolygon(); }

/*!
    \since 4.3
    Returns the area inside both this polygon and \a r, as a closed polygon.
    \sa QPainterPath::intersected()
*/
QPolygonF QPolygonF::intersected(const QPolygonF &r) const {
    return pathOf(*this).intersected(pathOf(r)).toFillPolygon();
}

/*!
    \since 4.3
    Returns the area inside this polygon but not inside \a r, as a closed polygon.
    \sa QPainterPath::subtracted()
*/
QPolygonF QPolygonF::subtracted(const QPolygonF &r) const {
    return pathOf(*this).subtracted(pathOf(r)).toFillPolygon();
}

/*!
    \since 5.10
    Returns \c true if the areas of this polygon and \a p, both closed, overlap; otherwise returns \c false.
*/
bool QPolygonF::intersects(const QPolygonF &p) const { return pathOf(*this).intersects(pathOf(p)); }

/*!
    \since 4.3
    Returns the area inside this polygon or \a r, or both, as a closed polygon with its points rounded to
    integers.
*/
QPolygon QPolygon::united(const QPolygon &r) const { return toPolygonF().united(r.toPolygonF()).toPolygon(); }

/*!
    \since 4.3
    Returns the area inside both this polygon and \a r, as a closed polygon with its points rounded to integers.
*/
QPolygon QPolygon::intersected(const QPolygon &r) const {
    return toPolygonF().intersected(r.toPolygonF()).toPolygon();
}

/*!
    \since 4.3
    Returns the area inside this polygon but not inside \a r, as a closed polygon with its points rounded to
    integers.
*/
QPolygon QPolygon::subtracted(const QPolygon &r) const {
    return toPolygonF().subtracted(r.toPolygonF()).toPolygon();
}

/*!
    \since 5.10
    Returns \c true if the areas of this polygon and \a p, both closed, overlap; otherwise returns \c false.
*/
bool QPolygon::intersects(const QPolygon &p) const { return toPolygonF().intersects(p.toPolygonF()); }

// ================================================================================================================

/*!
    \class QPainterPathStroker
    \since 4.1
    \brief The QPainterPathStroker class makes the outline of the stroke of a path, as a path to fill.

    The stroke is what QPainter::strokePath() paints with a pen of the same width, cap style, join style,
    miter limit and dashes. Filling the outline gives the same shape, so it can be hit-tested with
    QPainterPath::contains(), combined with other paths, or filled with any brush:

    \code
    QPainterPathStroker stroker;
    stroker.setWidth(6);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    const QPainterPath outline = stroker.createStroke(curve);
    if (outline.contains(mousePosition))
        select();
    \endcode

    The outline is made of many closed polygons that overlap, filled with Qt::WindingFill;
    QPainterPath::simplified() merges them into one.

    \differences
    The outline is made of lines, with curves divided into lines within curveThreshold() of them. Each
    segment, join and cap is its own polygon, as the raster engine draws them. The class can be copied.
    \enddifferences

    \sa QPen, QPainter::strokePath(), QPainterPath::simplified()
*/

/*!
    \fn QPainterPathStroker::QPainterPathStroker()
    Constructs a stroker one unit wide with Qt::SquareCap, Qt::BevelJoin, a miter limit of 2 and no dashes.
*/

/*!
    \since 4.3
    Constructs a stroker with the width, cap style, join style, miter limit, dash pattern and dash offset
    of \a pen.
*/
QPainterPathStroker::QPainterPathStroker(const QPen &pen) {
    setWidth(pen.widthF());
    setCapStyle(pen.capStyle());
    setJoinStyle(pen.joinStyle());
    setMiterLimit(pen.miterLimit());
    setDashOffset(pen.dashOffset());
    if (pen.style() == Qt::CustomDashLine)
        setDashPattern(pen.dashPattern());
    else
        setDashPattern(pen.style());
}

/*!
    \fn void QPainterPathStroker::setWidth(qreal width)
    Sets the width of the stroke to \a width; a width that is not positive is taken as 1.
*/
/*!
    \fn qreal QPainterPathStroker::width() const
    Returns the width of the stroke.
*/
/*!
    \fn void QPainterPathStroker::setCapStyle(Qt::PenCapStyle style)
    Sets how the ends of open subpaths and dashes are drawn to \a style.
*/
/*!
    \fn Qt::PenCapStyle QPainterPathStroker::capStyle() const
    Returns the cap style; Qt::SquareCap by default.
*/
/*!
    \fn void QPainterPathStroker::setJoinStyle(Qt::PenJoinStyle style)
    Sets how corners are drawn to \a style.
*/
/*!
    \fn Qt::PenJoinStyle QPainterPathStroker::joinStyle() const
    Returns the join style; Qt::BevelJoin by default.
*/
/*!
    \fn void QPainterPathStroker::setMiterLimit(qreal limit)
    Sets how far, in half widths, a miter join may reach before it is drawn as a bevel to \a limit.
*/
/*!
    \fn qreal QPainterPathStroker::miterLimit() const
    Returns the miter limit; 2 by default.
*/
/*!
    \fn void QPainterPathStroker::setCurveThreshold(qreal threshold)
    Sets to \a threshold how far, at most, the lines that replace a curve may be from it.
*/
/*!
    \fn qreal QPainterPathStroker::curveThreshold() const
    Returns how far the lines that replace a curve may be from it; 0.25 by default.
*/
/*!
    \fn QList<qreal> QPainterPathStroker::dashPattern() const
    Returns the dashes and gaps, alternating, in units of the width; empty for a solid stroke.
*/
/*!
    \fn void QPainterPathStroker::setDashOffset(qreal offset)
    Sets how far into the dash pattern, in units of the width, the stroke starts to \a offset.
*/
/*!
    \fn qreal QPainterPathStroker::dashOffset() const
    Returns how far into the dash pattern the stroke starts.
*/

/*!
    Sets the dash pattern to that of the pen style \a style; Qt::SolidLine, and any style without dashes,
    makes the stroke solid.
*/
void QPainterPathStroker::setDashPattern(Qt::PenStyle style) { m_dashes = QPen(style).dashPattern(); }

/*!
    \overload
    Sets the dashes and gaps, alternating, to \a dashPattern, in units of the width. An empty pattern makes
    the stroke solid.
*/
void QPainterPathStroker::setDashPattern(const QList<qreal> &dashPattern) { m_dashes = dashPattern; }

/*!
    Returns the outline of the stroke of \a path, filled with Qt::WindingFill.
*/
QPainterPath QPainterPathStroker::createStroke(const QPainterPath &path) const {
    namespace raster = QtRasterPrivate;
    QPainterPath result;
    result.setFillRule(Qt::WindingFill);
    if (path.isEmpty())
        return result;
    const double tolerance = std::max(m_curveThreshold > 0.0 ? m_curveThreshold : 0.25, 1e-4);
    const std::vector<raster::Path::Contour> contours = raster::toRasterPath(path).flatten(tolerance);
    raster::StrokeStyle style;
    style.width = m_width;
    style.cap = m_cap == Qt::RoundCap ? raster::LineCap::Round
                : m_cap == Qt::SquareCap ? raster::LineCap::Square
                                         : raster::LineCap::Butt;
    style.join = m_join == Qt::RoundJoin ? raster::LineJoin::Round
                 : m_join == Qt::BevelJoin ? raster::LineJoin::Bevel
                                           : raster::LineJoin::Miter;
    style.miterLimit = m_miterLimit;
    // Dashes are in units of the width, as for a pen, but no shorter than for a pen one unit wide.
    const double unit = std::max(1.0, m_width);
    for (const qreal d : m_dashes)
        style.dashes.push_back(std::max(1e-3, d) * unit);
    style.dashOffset = m_dashOffset * unit;
    raster::Polygons polygons;
    raster::strokeContours(contours, style, raster::Mat(), tolerance, &polygons);
    std::size_t begin = 0;
    for (const std::size_t end : polygons.ends) {
        if (end - begin >= 3) {
            result.moveTo(polygons.points[begin].x, polygons.points[begin].y);
            for (std::size_t i = begin + 1; i < end; ++i)
                result.lineTo(polygons.points[i].x, polygons.points[i].y);
            result.closeSubpath();
        }
        begin = end;
    }
    return result;
}
