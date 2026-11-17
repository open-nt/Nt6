// MIT License - see LICENSE
#include <QtGui/qpagesize.h>

#include <cmath>

namespace {

struct Standard {
    const char *key;
    const char *name;
    double width; // in `units`, portrait
    double height;
    QPageSize::PageSizeId id;
    QPageSize::Unit units;
};

// The standard sizes, in the order of PageSizeId.
constexpr Standard kStandards[] = {
    {"Letter", "Letter / ANSI A", 8.5, 11, QPageSize::Letter, QPageSize::Inch},
    {"Legal", "Legal", 8.5, 14, QPageSize::Legal, QPageSize::Inch},
    {"Executive.7x10", "Executive (7.25 x 10.5 in)", 7.25, 10.5, QPageSize::Executive, QPageSize::Inch},
    {"A0", "A0", 841, 1189, QPageSize::A0, QPageSize::Millimeter},
    {"A1", "A1", 594, 841, QPageSize::A1, QPageSize::Millimeter},
    {"A2", "A2", 420, 594, QPageSize::A2, QPageSize::Millimeter},
    {"A3", "A3", 297, 420, QPageSize::A3, QPageSize::Millimeter},
    {"A4", "A4", 210, 297, QPageSize::A4, QPageSize::Millimeter},
    {"A5", "A5", 148, 210, QPageSize::A5, QPageSize::Millimeter},
    {"A6", "A6", 105, 148, QPageSize::A6, QPageSize::Millimeter},
    {"A7", "A7", 74, 105, QPageSize::A7, QPageSize::Millimeter},
    {"A8", "A8", 52, 74, QPageSize::A8, QPageSize::Millimeter},
    {"A9", "A9", 37, 52, QPageSize::A9, QPageSize::Millimeter},
    {"ISOB0", "B0", 1000, 1414, QPageSize::B0, QPageSize::Millimeter},
    {"ISOB1", "B1", 707, 1000, QPageSize::B1, QPageSize::Millimeter},
    {"ISOB10", "B10", 31, 44, QPageSize::B10, QPageSize::Millimeter},
    {"ISOB2", "B2", 500, 707, QPageSize::B2, QPageSize::Millimeter},
    {"ISOB3", "B3", 353, 500, QPageSize::B3, QPageSize::Millimeter},
    {"ISOB4", "B4", 250, 353, QPageSize::B4, QPageSize::Millimeter},
    {"ISOB5", "B5", 176, 250, QPageSize::B5, QPageSize::Millimeter},
    {"ISOB6", "B6", 125, 176, QPageSize::B6, QPageSize::Millimeter},
    {"ISOB7", "B7", 88, 125, QPageSize::B7, QPageSize::Millimeter},
    {"ISOB8", "B8", 62, 88, QPageSize::B8, QPageSize::Millimeter},
    {"ISOB9", "B9", 44, 62, QPageSize::B9, QPageSize::Millimeter},
    {"EnvC5", "Envelope C5", 162, 229, QPageSize::C5E, QPageSize::Millimeter},
    {"Env10", "Envelope US 10", 4.125, 9.5, QPageSize::Comm10E, QPageSize::Inch},
    {"EnvDL", "Envelope DL", 110, 220, QPageSize::DLE, QPageSize::Millimeter},
    {"Folio", "Folio (8.27 x 13 in)", 210, 330, QPageSize::Folio, QPageSize::Millimeter},
    {"Ledger", "Ledger / ANSI B", 17, 11, QPageSize::Ledger, QPageSize::Inch},
    {"Tabloid", "Tabloid", 11, 17, QPageSize::Tabloid, QPageSize::Inch},
    {"Custom", "Custom", 0, 0, QPageSize::Custom, QPageSize::Point},
    {"A10", "A10", 26, 37, QPageSize::A10, QPageSize::Millimeter},
};

const Standard *standard(QPageSize::PageSizeId id) {
    const auto index = static_cast<std::size_t>(id);
    return index < std::size(kStandards) && id != QPageSize::Custom ? &kStandards[index] : nullptr;
}

// Points in one unit.
double pointsPer(QPageSize::Unit units) {
    switch (units) {
    case QPageSize::Millimeter:
        return 72.0 / 25.4;
    case QPageSize::Point:
        return 1.0;
    case QPageSize::Inch:
        return 72.0;
    case QPageSize::Pica:
        return 12.0;
    case QPageSize::Didot:
        return 0.375 * 72.0 / 25.4;
    case QPageSize::Cicero:
        return 12.0 * 0.375 * 72.0 / 25.4;
    }
    return 1.0;
}

QSizeF convert(const QSizeF &size, QPageSize::Unit from, QPageSize::Unit to) {
    if (from == to)
        return size;
    const double factor = pointsPer(from) / pointsPer(to);
    // Two decimals, as Qt keeps sizes in other units.
    const auto round2 = [](double v) { return std::round(v * 100.0) / 100.0; };
    return QSizeF(round2(size.width() * factor), round2(size.height() * factor));
}

QSize toPoints(const QSizeF &size, QPageSize::Unit units) {
    const double factor = pointsPer(units);
    return QSize(qRound(size.width() * factor), qRound(size.height() * factor));
}

} // namespace

/*!
    \class QPageSize
    \inmodule NtGui
    \since 6.0
    \brief The QPageSize class describes the size and name of a defined page size.

    A page size is one of the standard sizes, such as A4 or Letter, or a
    custom size. It is kept in the units it was defined in and can be asked
    for in any unit, in points, or in pixels at a resolution. Sizes are always
    portrait; QPageLayout turns them for landscape pages:

    \code
    const QPageSize a4(QPageSize::A4);
    a4.size(QPageSize::Millimeter); // 210 x 297
    a4.sizePoints();                // 595 x 842
    a4.sizePixels(300);             // 2480 x 3508
    \endcode

    \differences
    Only the standard sizes up to A10 in Qt's list are known (the ISO A and B
    series, US Letter, Legal, Executive, Ledger, Tabloid, Folio and three
    envelopes); the other sizes of Qt's PageSizeId are missing. There is no
    windowsId(), constructor from a Windows paper id, or QDebug output.
    \enddifferences

    \sa QPageLayout, QPdfWriter
*/

/*!
    \enum QPageSize::PageSizeId

    This enum names the standard page sizes.

    \value Letter US Letter, 8.5 x 11 inches.
    \value Legal US Legal, 8.5 x 14 inches.
    \value Executive Executive, 7.25 x 10.5 inches.
    \value A0 841 x 1189 mm.
    \value A1 594 x 841 mm.
    \value A2 420 x 594 mm.
    \value A3 297 x 420 mm.
    \value A4 210 x 297 mm.
    \value A5 148 x 210 mm.
    \value A6 105 x 148 mm.
    \value A7 74 x 105 mm.
    \value A8 52 x 74 mm.
    \value A9 37 x 52 mm.
    \value A10 26 x 37 mm.
    \value B0 1000 x 1414 mm.
    \value B1 707 x 1000 mm.
    \value B2 500 x 707 mm.
    \value B3 353 x 500 mm.
    \value B4 250 x 353 mm.
    \value B5 176 x 250 mm.
    \value B6 125 x 176 mm.
    \value B7 88 x 125 mm.
    \value B8 62 x 88 mm.
    \value B9 44 x 62 mm.
    \value B10 31 x 44 mm.
    \value C5E C5 envelope, 162 x 229 mm.
    \value Comm10E US Common #10 envelope, 4.125 x 9.5 inches.
    \value DLE DL envelope, 110 x 220 mm.
    \value Folio Folio, 210 x 330 mm.
    \value Ledger US Ledger, 17 x 11 inches.
    \value Tabloid US Tabloid, 11 x 17 inches.
    \value Custom A size that is none of the standard ones.
    \value AnsiA The same as Letter.
    \value AnsiB The same as Ledger.
    \value EnvelopeC5 The same as C5E.
    \value EnvelopeDL The same as DLE.
    \value Envelope10 The same as Comm10E.
    \omitvalue LastPageSize
    \omitvalue NPageSize
    \omitvalue NPaperSize
*/

/*!
    \enum QPageSize::Unit

    This enum gives the units that sizes are measured in.

    \value Millimeter Millimeters.
    \value Point PostScript points, 1/72 of an inch.
    \value Inch Inches.
    \value Pica Picas, 12 points.
    \value Didot Didot points, 0.375 mm.
    \value Cicero Ciceros, 12 Didot points.
*/

/*!
    \enum QPageSize::SizeMatchPolicy

    This enum tells how a size is matched with the standard sizes.

    \value FuzzyMatch A standard size within 3 points in each direction, in the same orientation, is used.
    \value FuzzyOrientationMatch As FuzzyMatch, but a landscape size also matches the portrait standard size.
    \value ExactMatch Only a standard size of exactly the same size in points is used.
*/

/*!
    \fn QPageSize::QPageSize()

    Constructs a page size that is not valid.
*/

/*!
    Constructs the standard page size \a pageSizeId. Custom gives a page size
    that is not valid.
*/
QPageSize::QPageSize(QPageSize::PageSizeId pageSizeId) {
    if (const Standard *s = standard(pageSizeId)) {
        m_valid = true;
        m_id = pageSizeId;
        m_definition = QSizeF(s->width, s->height);
        m_units = s->units;
    }
}

/*!
    Constructs a page size of \a pointSize points. If it matches a standard
    size under \a matchPolicy, that size is used; otherwise the size is custom,
    with the given \a name or one made from the size.
*/
QPageSize::QPageSize(const QSize &pointSize, const QString &name, QPageSize::SizeMatchPolicy matchPolicy)
    : QPageSize(QSizeF(pointSize), Point, name, matchPolicy) {}

/*!
    Constructs a page size of \a size in \a units. If it matches a standard
    size under \a matchPolicy, that size is used; otherwise the size is custom,
    with the given \a name or one made from the size.
*/
QPageSize::QPageSize(const QSizeF &size, QPageSize::Unit units, const QString &name,
                     QPageSize::SizeMatchPolicy matchPolicy) {
    if (size.width() <= 0 || size.height() <= 0)
        return;
    const PageSizeId match = id(size, units, matchPolicy);
    if (match != Custom) {
        *this = QPageSize(match);
        if (!name.isEmpty())
            m_name = name;
        return;
    }
    m_valid = true;
    m_id = Custom;
    m_definition = size;
    m_units = units;
    m_name = name;
}

/*!
    \fn void QPageSize::swap(QPageSize &other)

    Swaps this page size with \a other.
*/

/*!
    Returns \c true if this page size has the same size in points as
    \a other, whatever their names and units.
*/
bool QPageSize::isEquivalentTo(const QPageSize &other) const {
    return m_valid && other.m_valid && sizePoints() == other.sizePoints();
}

/*!
    \fn bool QPageSize::isValid() const

    Returns \c true if the page size has a size.
*/

/*!
    Returns a key that identifies the page size, such as \c A4 or, for a
    custom size, \c{Custom.210x297mm}.
*/
QString QPageSize::key() const {
    if (!m_valid)
        return QString();
    if (m_id != Custom)
        return key(m_id);
    static const char *const suffixes[] = {"mm", "pt", "in", "pc", "DD", "CC"};
    return QStringLiteral("Custom.%1x%2%3")
        .arg(m_definition.width())
        .arg(m_definition.height())
        .arg(QLatin1String(suffixes[m_units]));
}

/*!
    Returns the name of the page size for people, such as \c A4: the name it
    was given, or the standard name.
*/
QString QPageSize::name() const {
    if (!m_valid)
        return QString();
    if (!m_name.isEmpty())
        return m_name;
    if (m_id != Custom)
        return name(m_id);
    const QSizeF s = m_definition;
    static const char *const units[] = {"mm", "pt", "in", "pc", "DD", "CC"};
    return QStringLiteral("Custom (%1%3 x %2%3)").arg(s.width()).arg(s.height()).arg(QLatin1String(units[m_units]));
}

/*!
    \fn QPageSize::PageSizeId QPageSize::id() const

    Returns the standard size this is, or Custom.
*/

/*!
    \fn QSizeF QPageSize::definitionSize() const

    Returns the size in the units it was defined in.

    \sa definitionUnits()
*/

/*!
    \fn QPageSize::Unit QPageSize::definitionUnits() const

    Returns the units the size was defined in.
*/

/*!
    Returns the size in \a units, rounded to two decimals unless they are the
    units of the definition.
*/
QSizeF QPageSize::size(QPageSize::Unit units) const {
    return m_valid ? convert(m_definition, m_units, units) : QSizeF();
}

/*!
    Returns the size in whole points.
*/
QSize QPageSize::sizePoints() const { return m_valid ? toPoints(m_definition, m_units) : QSize(); }

/*!
    Returns the size in pixels at \a resolution dots per inch.
*/
QSize QPageSize::sizePixels(int resolution) const {
    if (!m_valid || resolution <= 0)
        return QSize();
    const double factor = pointsPer(m_units) * resolution / 72.0;
    return QSize(qRound(m_definition.width() * factor), qRound(m_definition.height() * factor));
}

/*!
    Returns the page as a rectangle at the origin, in \a units.
*/
QRectF QPageSize::rect(QPageSize::Unit units) const { return QRectF(QPointF(0, 0), size(units)); }

/*!
    Returns the page as a rectangle at the origin, in points.
*/
QRect QPageSize::rectPoints() const { return QRect(QPoint(0, 0), sizePoints()); }

/*!
    Returns the page as a rectangle at the origin, in pixels at
    \a resolution dots per inch.
*/
QRect QPageSize::rectPixels(int resolution) const { return QRect(QPoint(0, 0), sizePixels(resolution)); }

/*!
    Returns the key of the standard size \a pageSizeId, such as \c A4.
*/
QString QPageSize::key(QPageSize::PageSizeId pageSizeId) {
    const auto index = static_cast<std::size_t>(pageSizeId);
    return index < std::size(kStandards) ? QString::fromLatin1(kStandards[index].key) : QString();
}

/*!
    Returns the name of the standard size \a pageSizeId for people.
*/
QString QPageSize::name(QPageSize::PageSizeId pageSizeId) {
    const auto index = static_cast<std::size_t>(pageSizeId);
    return index < std::size(kStandards) ? QString::fromLatin1(kStandards[index].name) : QString();
}

/*!
    Returns the standard size of \a pointSize points under \a matchPolicy, or
    Custom if there is none.
*/
QPageSize::PageSizeId QPageSize::id(const QSize &pointSize, QPageSize::SizeMatchPolicy matchPolicy) {
    return id(QSizeF(pointSize), Point, matchPolicy);
}

/*!
    Returns the standard size of \a size in \a units under \a matchPolicy, or
    Custom if there is none.
*/
QPageSize::PageSizeId QPageSize::id(const QSizeF &size, QPageSize::Unit units, QPageSize::SizeMatchPolicy matchPolicy) {
    // Sizes defined in the same units compare exactly; others compare in points.
    for (const Standard &s : kStandards)
        if (s.id != Custom && s.units == units && s.width == size.width() && s.height == size.height())
            return s.id;
    const QSize points = toPoints(size, units);
    const auto close = [&](const QSize &a, const QSize &b) {
        if (matchPolicy == ExactMatch)
            return a == b;
        return std::abs(a.width() - b.width()) <= 3 && std::abs(a.height() - b.height()) <= 3;
    };
    for (const Standard &s : kStandards) {
        if (s.id == Custom)
            continue;
        const QSize standardPoints = toPoints(QSizeF(s.width, s.height), s.units);
        if (close(points, standardPoints) ||
            (matchPolicy == FuzzyOrientationMatch && close(points.transposed(), standardPoints)))
            return s.id;
    }
    return Custom;
}

/*!
    Returns the size of \a pageSizeId in the units it is defined in.

    \sa definitionUnits()
*/
QSizeF QPageSize::definitionSize(QPageSize::PageSizeId pageSizeId) {
    const Standard *s = standard(pageSizeId);
    return s ? QSizeF(s->width, s->height) : QSizeF();
}

/*!
    Returns the units \a pageSizeId is defined in: millimeters for the ISO
    sizes, inches for the US ones.
*/
QPageSize::Unit QPageSize::definitionUnits(QPageSize::PageSizeId pageSizeId) {
    const Standard *s = standard(pageSizeId);
    return s ? s->units : Point;
}

/*!
    Returns the size of \a pageSizeId in \a units.
*/
QSizeF QPageSize::size(QPageSize::PageSizeId pageSizeId, QPageSize::Unit units) {
    return QPageSize(pageSizeId).size(units);
}

/*!
    Returns the size of \a pageSizeId in whole points.
*/
QSize QPageSize::sizePoints(QPageSize::PageSizeId pageSizeId) { return QPageSize(pageSizeId).sizePoints(); }

/*!
    Returns the size of \a pageSizeId in pixels at \a resolution dots per inch.
*/
QSize QPageSize::sizePixels(QPageSize::PageSizeId pageSizeId, int resolution) {
    return QPageSize(pageSizeId).sizePixels(resolution);
}
