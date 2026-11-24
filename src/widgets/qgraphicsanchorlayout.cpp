// MIT License - see LICENSE
#include <QtWidgets/qgraphicsanchorlayout.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr qreal kMaxSize = 16777215;

bool isHorizontalEdge(Qt::AnchorPoint edge) {
    return edge == Qt::AnchorLeft || edge == Qt::AnchorHorizontalCenter || edge == Qt::AnchorRight;
}

// The position of an edge is start * 1 + size * share, in the order of the line: 0, 0.5 and 1.
qreal edgeShare(Qt::AnchorPoint edge) {
    switch (edge) {
    case Qt::AnchorHorizontalCenter:
    case Qt::AnchorVerticalCenter:
        return 0.5;
    case Qt::AnchorRight:
    case Qt::AnchorBottom:
        return 1;
    default:
        return 0;
    }
}

// Solves the dense system `matrix * x = rhs`, in place, by Gaussian elimination with partial pivoting.
std::vector<qreal> solveLinear(std::vector<std::vector<qreal>> matrix, std::vector<qreal> rhs) {
    const std::size_t n = rhs.size();
    for (std::size_t column = 0; column < n; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < n; ++row)
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column]))
                pivot = row;
        std::swap(matrix[column], matrix[pivot]);
        std::swap(rhs[column], rhs[pivot]);
        const qreal diagonal = matrix[column][column];
        if (std::abs(diagonal) < 1e-12)
            continue;
        for (std::size_t row = column + 1; row < n; ++row) {
            const qreal factor = matrix[row][column] / diagonal;
            if (factor == 0)
                continue;
            for (std::size_t k = column; k < n; ++k)
                matrix[row][k] -= factor * matrix[column][k];
            rhs[row] -= factor * rhs[column];
        }
    }
    std::vector<qreal> x(n, 0);
    for (std::size_t i = n; i-- > 0;) {
        qreal sum = rhs[i];
        for (std::size_t k = i + 1; k < n; ++k)
            sum -= matrix[i][k] * x[k];
        x[i] = std::abs(matrix[i][i]) < 1e-12 ? 0 : sum / matrix[i][i];
    }
    return x;
}

} // namespace

/*!
    \class QGraphicsAnchor
    \brief The QGraphicsAnchor class represents an anchor between two items in a QGraphicsAnchorLayout.

    An anchor fixes the distance between an edge of one item and an edge of another. QGraphicsAnchorLayout::addAnchor()
    makes it and gives it to you to change the distance:

    \code
    QGraphicsAnchor *anchor = layout->addAnchor(a, Qt::AnchorRight, b, Qt::AnchorLeft);
    anchor->setSpacing(12);
    \endcode

    \differences
    The size policy is kept but the layout does not use it: an anchor is always an exact distance.
    \enddifferences

    \sa QGraphicsAnchorLayout
*/

QGraphicsAnchor::QGraphicsAnchor(QGraphicsAnchorLayout *parent) : QObject(nullptr), m_layout(parent) {}

/*!
    Destroys the anchor.
*/
QGraphicsAnchor::~QGraphicsAnchor() = default;

/*!
    Sets the distance between the two edges to \a spacing. It may be negative, for items that overlap.

    \sa spacing(), unsetSpacing()
*/
void QGraphicsAnchor::setSpacing(qreal spacing) {
    m_spacing = spacing;
    m_spacingSet = true;
    m_layout->invalidate();
}

/*!
    Brings back the default distance: 0 between an item and the layout, and the spacing of the layout between two items.

    \sa setSpacing()
*/
void QGraphicsAnchor::unsetSpacing() {
    m_spacingSet = false;
    m_layout->invalidate();
}

/*!
    Returns the distance between the two edges.
*/
qreal QGraphicsAnchor::spacing() const {
    return m_spacingSet ? m_spacing
                        : m_layout->anchorSpacing(this, isHorizontalEdge(m_firstEdge) ? Qt::Horizontal : Qt::Vertical);
}

/*!
    Sets the size policy of the anchor to \a policy; only kept.
*/
void QGraphicsAnchor::setSizePolicy(QSizePolicy::Policy policy) { m_policy = policy; }

/*!
    \fn QSizePolicy::Policy QGraphicsAnchor::sizePolicy() const

    Returns the size policy of the anchor, QSizePolicy::Fixed by default.
*/

/*!
    \class QGraphicsAnchorLayout
    \brief The QGraphicsAnchorLayout class places the items of a graphics widget by anchors between their edges.

    Each anchor fixes the distance between an edge of an item and an edge of another item, or of the layout itself.
    The layout stretches the items between its anchors:

    \code
    auto *layout = new QGraphicsAnchorLayout;
    layout->addAnchors(layout, a, Qt::Horizontal);        // a fills the width
    layout->addAnchor(layout, Qt::AnchorTop, a, Qt::AnchorTop);
    layout->addAnchor(a, Qt::AnchorBottom, b, Qt::AnchorTop);
    layout->addAnchors(layout, b, Qt::Horizontal);
    layout->addAnchor(b, Qt::AnchorBottom, layout, Qt::AnchorBottom);
    widget->setLayout(layout);
    \endcode

    \differences
    The layout is solved by least squares, not by a simplex solver: the anchors are exact, and the items get the sizes
    closest to their preferred sizes. The minimum and maximum sizes of the items are applied afterwards, so with
    conflicting anchors an item may be moved from its anchor. Anchors between two parallel edges of the same item, and
    between horizontal and vertical edges, are not accepted. The size policies of the anchors and the items are
    not used.
    \enddifferences

    \sa QGraphicsAnchor, QGraphicsGridLayout, QGraphicsLinearLayout
*/

/*!
    Constructs an empty layout with the given \a parent.
*/
QGraphicsAnchorLayout::QGraphicsAnchorLayout(QGraphicsLayoutItem *parent) : QGraphicsLayout(parent) {}

/*!
    Destroys the layout and the anchors. The items that the layout owns are deleted too.
*/
QGraphicsAnchorLayout::~QGraphicsAnchorLayout() {
    for (QGraphicsAnchor *anchor : m_anchors)
        delete anchor;
    for (QGraphicsLayoutItem *item : std::as_const(m_items)) {
        item->setParentLayoutItem(nullptr);
        if (item->ownedByLayout() || item->isLayout())
            delete item;
    }
}

/*!
    Anchors the \a firstEdge of \a firstItem to the \a secondEdge of \a secondItem, and returns the anchor. Either item
    may be the layout itself. The two edges must be on the same axis, both horizontal (left, center, right) or both
    vertical; otherwise nothing happens and \c nullptr is returned. An anchor that already exists between the edges is
    returned.

    \sa anchor(), addAnchors()
*/
QGraphicsAnchor *QGraphicsAnchorLayout::addAnchor(QGraphicsLayoutItem *firstItem, Qt::AnchorPoint firstEdge,
                                                  QGraphicsLayoutItem *secondItem, Qt::AnchorPoint secondEdge) {
    if (!firstItem || !secondItem || (firstItem == secondItem && firstEdge == secondEdge) ||
        isHorizontalEdge(firstEdge) != isHorizontalEdge(secondEdge))
        return nullptr;
    if (QGraphicsAnchor *existing = anchor(firstItem, firstEdge, secondItem, secondEdge))
        return existing;
    for (QGraphicsLayoutItem *item : {firstItem, secondItem}) {
        if (item != this && !m_items.contains(item)) {
            m_items.append(item);
            addChildLayoutItem(item);
        }
    }
    auto *created = new QGraphicsAnchor(this);
    created->m_first = firstItem;
    created->m_firstEdge = firstEdge;
    created->m_second = secondItem;
    created->m_secondEdge = secondEdge;
    m_anchors.push_back(created);
    invalidate();
    return created;
}

/*!
    Returns the anchor between the \a firstEdge of \a firstItem and the \a secondEdge of \a secondItem, in either order,
    or \c nullptr if there is none.
*/
QGraphicsAnchor *QGraphicsAnchorLayout::anchor(QGraphicsLayoutItem *firstItem, Qt::AnchorPoint firstEdge,
                                               QGraphicsLayoutItem *secondItem, Qt::AnchorPoint secondEdge) {
    for (QGraphicsAnchor *a : m_anchors) {
        if ((a->m_first == firstItem && a->m_firstEdge == firstEdge && a->m_second == secondItem &&
             a->m_secondEdge == secondEdge) ||
            (a->m_first == secondItem && a->m_firstEdge == secondEdge && a->m_second == firstItem &&
             a->m_secondEdge == firstEdge))
            return a;
    }
    return nullptr;
}

/*!
    Anchors the \a firstCorner of \a firstItem to the \a secondCorner of \a secondItem: one anchor for each axis.
*/
void QGraphicsAnchorLayout::addCornerAnchors(QGraphicsLayoutItem *firstItem, Qt::Corner firstCorner,
                                             QGraphicsLayoutItem *secondItem, Qt::Corner secondCorner) {
    const auto horizontal = [](Qt::Corner c) {
        return c == Qt::TopLeftCorner || c == Qt::BottomLeftCorner ? Qt::AnchorLeft : Qt::AnchorRight;
    };
    const auto vertical = [](Qt::Corner c) {
        return c == Qt::TopLeftCorner || c == Qt::TopRightCorner ? Qt::AnchorTop : Qt::AnchorBottom;
    };
    addAnchor(firstItem, horizontal(firstCorner), secondItem, horizontal(secondCorner));
    addAnchor(firstItem, vertical(firstCorner), secondItem, vertical(secondCorner));
}

/*!
    Anchors the left edges and the right edges of the two items together if \a orientations has Qt::Horizontal, and the
    top edges and the bottom edges if it has Qt::Vertical, so that \a secondItem has the size of \a firstItem on that axis.
*/
void QGraphicsAnchorLayout::addAnchors(QGraphicsLayoutItem *firstItem, QGraphicsLayoutItem *secondItem,
                                       Qt::Orientations orientations) {
    if (orientations & Qt::Horizontal) {
        addAnchor(firstItem, Qt::AnchorLeft, secondItem, Qt::AnchorLeft);
        addAnchor(firstItem, Qt::AnchorRight, secondItem, Qt::AnchorRight);
    }
    if (orientations & Qt::Vertical) {
        addAnchor(firstItem, Qt::AnchorTop, secondItem, Qt::AnchorTop);
        addAnchor(firstItem, Qt::AnchorBottom, secondItem, Qt::AnchorBottom);
    }
}

/*!
    Sets the default distance of the anchors between items on the horizontal axis to \a spacing.
*/
void QGraphicsAnchorLayout::setHorizontalSpacing(qreal spacing) {
    m_horizontalSpacing = spacing;
    invalidate();
}

/*!
    Sets the default distance of the anchors between items on the vertical axis to \a spacing.
*/
void QGraphicsAnchorLayout::setVerticalSpacing(qreal spacing) {
    m_verticalSpacing = spacing;
    invalidate();
}

/*!
    Sets the default distance on both axes to \a spacing.
*/
void QGraphicsAnchorLayout::setSpacing(qreal spacing) {
    m_horizontalSpacing = m_verticalSpacing = spacing;
    invalidate();
}

/*!
    \fn qreal QGraphicsAnchorLayout::horizontalSpacing() const

    Returns the default distance of the anchors between items on the horizontal axis, 6 by default.
*/

/*!
    \fn qreal QGraphicsAnchorLayout::verticalSpacing() const

    Returns the default distance of the anchors between items on the vertical axis, 6 by default.
*/

qreal QGraphicsAnchorLayout::anchorSpacing(const QGraphicsAnchor *anchor, Qt::Orientation orientation) const {
    if (anchor->m_first == this || anchor->m_second == this)
        return 0;
    return orientation == Qt::Horizontal ? m_horizontalSpacing : m_verticalSpacing;
}

/*!
    \reimp

    Removes the item at \a index and the anchors that end at it.
*/
void QGraphicsAnchorLayout::removeAt(int index) {
    if (index < 0 || index >= count())
        return;
    QGraphicsLayoutItem *item = m_items.at(index);
    for (std::size_t i = m_anchors.size(); i-- > 0;) {
        if (m_anchors[i]->m_first == item || m_anchors[i]->m_second == item) {
            delete m_anchors[i];
            m_anchors.erase(m_anchors.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
    item->setParentLayoutItem(nullptr);
    m_items.removeAt(index);
    invalidate();
}

/*!
    \reimp
*/
int QGraphicsAnchorLayout::count() const { return static_cast<int>(m_items.size()); }

/*!
    \reimp
*/
QGraphicsLayoutItem *QGraphicsAnchorLayout::itemAt(int index) const {
    return index >= 0 && index < m_items.size() ? m_items.at(index) : nullptr;
}

/*!
    \reimp
*/
void QGraphicsAnchorLayout::invalidate() { QGraphicsLayout::invalidate(); }

// Finds the starts and sizes of the items on both axes: the exact distances of the anchors, with the sizes as near
// as possible to those of `which`. With `fixed`, the layout has that size; without it the layout gets the size that
// the anchors ask for. result[k] is the rectangle of item k in the layout, from the origin of the layout.
void QGraphicsAnchorLayout::solve(Qt::SizeHint which, const QSizeF *fixed, std::vector<QRectF> *result,
                                  QSizeF *layoutSize) const {
    const std::size_t items = static_cast<std::size_t>(m_items.size());
    result->assign(items, QRectF());
    std::vector<qreal> start[2], size[2];
    for (int axis = 0; axis < 2; ++axis) {
        const Qt::Orientation orientation = axis == 0 ? Qt::Horizontal : Qt::Vertical;
        // Variables: start and size of the layout (0, 1), then of each item.
        const std::size_t variables = 2 * (items + 1);
        std::vector<std::pair<std::vector<std::pair<std::size_t, qreal>>, qreal>> rows;
        rows.push_back({{{0, 1}}, 0}); // the layout starts at 0
        if (fixed)
            rows.push_back({{{1, 1}}, axis == 0 ? fixed->width() : fixed->height()});
        const auto indexOf = [this](const QGraphicsLayoutItem *item) -> std::size_t {
            return item == this ? 0 : static_cast<std::size_t>(m_items.indexOf(const_cast<QGraphicsLayoutItem *>(item))) + 1;
        };
        for (const QGraphicsAnchor *a : m_anchors) {
            if ((isHorizontalEdge(a->m_firstEdge) ? Qt::Horizontal : Qt::Vertical) != orientation)
                continue;
            const std::size_t i = indexOf(a->m_first), j = indexOf(a->m_second);
            const qreal pi = edgeShare(a->m_firstEdge), pj = edgeShare(a->m_secondEdge);
            // position(j) - position(i) = spacing
            std::vector<std::pair<std::size_t, qreal>> terms;
            terms.push_back({2 * j, 1});
            terms.push_back({2 * j + 1, pj});
            terms.push_back({2 * i, -1});
            terms.push_back({2 * i + 1, -pi});
            rows.push_back({terms, a->spacing()});
        }
        const std::size_t m = rows.size();
        const std::size_t n = variables + m;
        std::vector<std::vector<qreal>> matrix(n, std::vector<qreal>(n, 0));
        std::vector<qreal> rhs(n, 0);
        constexpr qreal kWeak = 1e-6;
        for (std::size_t k = 0; k <= items; ++k) {
            matrix[2 * k][2 * k] = 2 * kWeak;
            matrix[2 * k + 1][2 * k + 1] = 2 * (k == 0 ? kWeak : 1);
            if (k > 0) {
                const QSizeF hint = m_items.at(static_cast<qsizetype>(k - 1))->effectiveSizeHint(which);
                rhs[2 * k + 1] = 2 * (axis == 0 ? hint.width() : hint.height());
            }
        }
        for (std::size_t r = 0; r < m; ++r) {
            for (const auto &[variable, coefficient] : rows[r].first) {
                matrix[variables + r][variable] += coefficient;
                matrix[variable][variables + r] += coefficient;
            }
            rhs[variables + r] = rows[r].second;
            matrix[variables + r][variables + r] = -1e-9;
        }
        const std::vector<qreal> x = solveLinear(matrix, rhs);
        start[axis].resize(items);
        size[axis].resize(items);
        for (std::size_t k = 0; k < items; ++k) {
            const QGraphicsLayoutItem *item = m_items.at(static_cast<qsizetype>(k));
            const QSizeF minimum = item->effectiveSizeHint(Qt::MinimumSize);
            const QSizeF maximum = item->effectiveSizeHint(Qt::MaximumSize);
            const qreal lo = axis == 0 ? minimum.width() : minimum.height();
            const qreal hi = axis == 0 ? maximum.width() : maximum.height();
            start[axis][k] = x[2 * (k + 1)];
            size[axis][k] = std::clamp(x[2 * (k + 1) + 1], lo, std::max(lo, hi));
        }
        if (layoutSize) {
            if (axis == 0)
                layoutSize->setWidth(std::max<qreal>(0, x[1]));
            else
                layoutSize->setHeight(std::max<qreal>(0, x[1]));
        }
    }
    // The weak terms that keep the system solvable leave errors of about 1e-7; drop them.
    const auto snap = [](qreal v) { return std::round(v * 1e4) / 1e4; };
    for (std::size_t k = 0; k < items; ++k)
        (*result)[k] = QRectF(snap(start[0][k]), snap(start[1][k]), snap(size[0][k]), snap(size[1][k]));
    if (layoutSize)
        *layoutSize = QSizeF(snap(layoutSize->width()), snap(layoutSize->height()));
}

/*!
    \reimp
*/
QSizeF QGraphicsAnchorLayout::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const {
    (void)constraint;
    qreal left = 0, top = 0, right = 0, bottom = 0;
    getContentsMargins(&left, &top, &right, &bottom);
    if (which == Qt::MaximumSize)
        return QSizeF(kMaxSize, kMaxSize);
    if (which == Qt::MinimumDescent)
        return QSizeF(-1, -1);
    std::vector<QRectF> rectangles;
    QSizeF size;
    solve(which, nullptr, &rectangles, &size);
    // The extent that the items need, when they reach out of the layout.
    qreal width = size.width(), height = size.height();
    for (const QRectF &r : rectangles) {
        width = std::max(width, r.right());
        height = std::max(height, r.bottom());
    }
    return QSizeF(width + left + right, height + top + bottom);
}

/*!
    \reimp
*/
void QGraphicsAnchorLayout::setGeometry(const QRectF &rect) {
    QGraphicsLayout::setGeometry(rect);
    qreal left = 0, top = 0, right = 0, bottom = 0;
    getContentsMargins(&left, &top, &right, &bottom);
    const QRectF inner = geometry().adjusted(left, top, -right, -bottom);
    std::vector<QRectF> rectangles;
    const QSizeF innerSize = inner.size();
    solve(Qt::PreferredSize, &innerSize, &rectangles, nullptr);
    for (std::size_t k = 0; k < rectangles.size(); ++k)
        m_items.at(static_cast<qsizetype>(k))->setGeometry(rectangles[k].translated(inner.topLeft()));
}
