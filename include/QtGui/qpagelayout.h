// MIT License - see LICENSE
#pragma once

#include <QtCore/qmargins.h>
#include <QtCore/qrect.h>
#include <QtGui/qpagesize.h>

#include <utility>

class QPageLayout {
public:
    enum Unit { Millimeter, Point, Inch, Pica, Didot, Cicero };
    enum Orientation { Portrait, Landscape };
    enum Mode { StandardMode, FullPageMode };
    enum class OutOfBoundsPolicy { Reject, Clamp };

    QPageLayout() = default;
    QPageLayout(const QPageSize &pageSize, QPageLayout::Orientation orientation, const QMarginsF &margins,
                QPageLayout::Unit units = QPageLayout::Point, const QMarginsF &minMargins = QMarginsF(0, 0, 0, 0));

    void swap(QPageLayout &other) noexcept { std::swap(*this, other); }

    /*!
        Returns \c true if \a lhs and \a rhs have the same page size, orientation, units, margins and mode.
    */
    friend bool operator==(const QPageLayout &lhs, const QPageLayout &rhs) {
        return lhs.m_pageSize == rhs.m_pageSize && lhs.m_orientation == rhs.m_orientation &&
               lhs.m_units == rhs.m_units && lhs.m_margins == rhs.m_margins && lhs.m_minMargins == rhs.m_minMargins &&
               lhs.m_mode == rhs.m_mode;
    }
    /*!
        Returns \c true if \a lhs and \a rhs differ.
    */
    friend bool operator!=(const QPageLayout &lhs, const QPageLayout &rhs) { return !(lhs == rhs); }

    bool isEquivalentTo(const QPageLayout &other) const;
    bool isValid() const { return m_pageSize.isValid(); }

    void setMode(QPageLayout::Mode mode) { m_mode = mode; }
    QPageLayout::Mode mode() const { return m_mode; }

    void setPageSize(const QPageSize &pageSize, const QMarginsF &minMargins = QMarginsF(0, 0, 0, 0));
    QPageSize pageSize() const { return m_pageSize; }

    void setOrientation(QPageLayout::Orientation orientation);
    QPageLayout::Orientation orientation() const { return m_orientation; }

    void setUnits(QPageLayout::Unit units);
    QPageLayout::Unit units() const { return m_units; }

    bool setMargins(const QMarginsF &margins,
                    QPageLayout::OutOfBoundsPolicy outOfBoundsPolicy = OutOfBoundsPolicy::Reject);
    bool setLeftMargin(qreal leftMargin, QPageLayout::OutOfBoundsPolicy outOfBoundsPolicy = OutOfBoundsPolicy::Reject);
    bool setRightMargin(qreal rightMargin,
                        QPageLayout::OutOfBoundsPolicy outOfBoundsPolicy = OutOfBoundsPolicy::Reject);
    bool setTopMargin(qreal topMargin, QPageLayout::OutOfBoundsPolicy outOfBoundsPolicy = OutOfBoundsPolicy::Reject);
    bool setBottomMargin(qreal bottomMargin,
                         QPageLayout::OutOfBoundsPolicy outOfBoundsPolicy = OutOfBoundsPolicy::Reject);

    QMarginsF margins() const { return m_margins; }
    QMarginsF margins(QPageLayout::Unit units) const;
    QMargins marginsPoints() const;
    QMargins marginsPixels(int resolution) const;

    void setMinimumMargins(const QMarginsF &minMargins);
    QMarginsF minimumMargins() const { return m_minMargins; }
    QMarginsF maximumMargins() const;

    QRectF fullRect() const;
    QRectF fullRect(QPageLayout::Unit units) const;
    QRect fullRectPoints() const;
    QRect fullRectPixels(int resolution) const;

    QRectF paintRect() const;
    QRectF paintRect(QPageLayout::Unit units) const;
    QRect paintRectPoints() const;
    QRect paintRectPixels(int resolution) const;

private:
    QSizeF orientedSize(QPageLayout::Unit units) const;
    QMarginsF clamped(const QMarginsF &margins) const;
    bool accept(const QMarginsF &margins, QPageLayout::OutOfBoundsPolicy policy);

    QPageSize m_pageSize;
    Orientation m_orientation = Portrait;
    Unit m_units = Point;
    Mode m_mode = StandardMode;
    QMarginsF m_margins;
    QMarginsF m_minMargins;
};
