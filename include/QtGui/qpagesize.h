// MIT License - see LICENSE
#pragma once

#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>

#include <utility>

class QPageSize {
public:
    enum PageSizeId {
        Letter,
        Legal,
        Executive,
        A0,
        A1,
        A2,
        A3,
        A4,
        A5,
        A6,
        A7,
        A8,
        A9,
        B0,
        B1,
        B10,
        B2,
        B3,
        B4,
        B5,
        B6,
        B7,
        B8,
        B9,
        C5E,
        Comm10E,
        DLE,
        Folio,
        Ledger,
        Tabloid,
        Custom,
        A10,

        LastPageSize = A10,
        NPageSize = LastPageSize,
        NPaperSize = LastPageSize,
        AnsiA = Letter,
        AnsiB = Ledger,
        EnvelopeC5 = C5E,
        EnvelopeDL = DLE,
        Envelope10 = Comm10E
    };

    enum Unit { Millimeter, Point, Inch, Pica, Didot, Cicero };

    enum SizeMatchPolicy { FuzzyMatch, FuzzyOrientationMatch, ExactMatch };

    QPageSize() = default;
    QPageSize(QPageSize::PageSizeId pageSizeId); // NOLINT(google-explicit-constructor): implicit, as in Qt
    explicit QPageSize(const QSize &pointSize, const QString &name = QString(),
                       QPageSize::SizeMatchPolicy matchPolicy = QPageSize::FuzzyMatch);
    explicit QPageSize(const QSizeF &size, QPageSize::Unit units, const QString &name = QString(),
                       QPageSize::SizeMatchPolicy matchPolicy = QPageSize::FuzzyMatch);

    void swap(QPageSize &other) noexcept { std::swap(*this, other); }

    /*!
        Returns \c true if \a lhs and \a rhs are the same page size, with the same name and units.
    */
    friend bool operator==(const QPageSize &lhs, const QPageSize &rhs) noexcept {
        return lhs.m_valid == rhs.m_valid && lhs.m_id == rhs.m_id && lhs.m_name == rhs.m_name &&
               lhs.m_definition == rhs.m_definition && lhs.m_units == rhs.m_units;
    }
    /*!
        Returns \c true if \a lhs and \a rhs differ.
    */
    friend bool operator!=(const QPageSize &lhs, const QPageSize &rhs) noexcept { return !(lhs == rhs); }

    bool isEquivalentTo(const QPageSize &other) const;
    bool isValid() const { return m_valid; }

    QString key() const;
    QString name() const;
    QPageSize::PageSizeId id() const { return m_id; }

    QSizeF definitionSize() const { return m_definition; }
    QPageSize::Unit definitionUnits() const { return m_units; }

    QSizeF size(QPageSize::Unit units) const;
    QSize sizePoints() const;
    QSize sizePixels(int resolution) const;

    QRectF rect(QPageSize::Unit units) const;
    QRect rectPoints() const;
    QRect rectPixels(int resolution) const;

    static QString key(QPageSize::PageSizeId pageSizeId);
    static QString name(QPageSize::PageSizeId pageSizeId);

    static QPageSize::PageSizeId id(const QSize &pointSize,
                                    QPageSize::SizeMatchPolicy matchPolicy = QPageSize::FuzzyMatch);
    static QPageSize::PageSizeId id(const QSizeF &size, QPageSize::Unit units,
                                    QPageSize::SizeMatchPolicy matchPolicy = QPageSize::FuzzyMatch);

    static QSizeF definitionSize(QPageSize::PageSizeId pageSizeId);
    static QPageSize::Unit definitionUnits(QPageSize::PageSizeId pageSizeId);

    static QSizeF size(QPageSize::PageSizeId pageSizeId, QPageSize::Unit units);
    static QSize sizePoints(QPageSize::PageSizeId pageSizeId);
    static QSize sizePixels(QPageSize::PageSizeId pageSizeId, int resolution);

private:
    bool m_valid = false;
    PageSizeId m_id = Custom;
    QString m_name; // the name given for a custom size; standard sizes use name(id)
    QSizeF m_definition;
    Unit m_units = Point;
};
