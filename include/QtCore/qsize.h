// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qmargins.h>
#include <QtCore/qnamespace.h>

class QSizeF;

/*!
    \class QSize
    \brief The QSize class defines the size of a two-dimensional object using integer point precision.

    A size is specified by a width() and a height(), both stored as \c int.
    A default-constructed size is invalid, with a width and height of -1.
    Use QSizeF for floating-point dimensions.

    \differences
    None.
    \enddifferences

    \sa QSizeF, QPoint, QRect
*/
class QSize {
public:
    /*!
        Constructs a size with an invalid width and height, both -1.

        \sa isValid()
    */
    constexpr QSize() noexcept = default;
    /*!
        Constructs a size with the given \a w and \a h.
    */
    constexpr QSize(int w, int h) noexcept : wd(w), ht(h) {}

    /*!
        Returns \c true if both the width and height are 0; otherwise
        returns \c false.

        \sa isEmpty(), isValid()
    */
    constexpr bool isNull() const noexcept { return wd == 0 && ht == 0; }
    /*!
        Returns \c true if the width or the height is less than 1;
        otherwise returns \c false.

        \sa isNull(), isValid()
    */
    constexpr bool isEmpty() const noexcept { return wd < 1 || ht < 1; }
    /*!
        Returns \c true if both the width and the height are equal to or
        greater than 0; otherwise returns \c false.

        \sa isNull(), isEmpty()
    */
    constexpr bool isValid() const noexcept { return wd >= 0 && ht >= 0; }
    /*!
        Returns the width.

        \sa height(), setWidth(), rwidth()
    */
    constexpr int width() const noexcept { return wd; }
    /*!
        Returns the height.

        \sa width(), setHeight(), rheight()
    */
    constexpr int height() const noexcept { return ht; }
    /*!
        Sets the width to the given \a w.

        \sa width(), setHeight()
    */
    constexpr void setWidth(int w) noexcept { wd = w; }
    /*!
        Sets the height to the given \a h.

        \sa height(), setWidth()
    */
    constexpr void setHeight(int h) noexcept { ht = h; }
    /*!
        Returns a reference to the width, allowing it to be modified
        directly.

        \sa width(), setWidth()
    */
    constexpr int &rwidth() noexcept { return wd; }
    /*!
        Returns a reference to the height, allowing it to be modified
        directly.

        \sa height(), setHeight()
    */
    constexpr int &rheight() noexcept { return ht; }
    /*!
        Swaps the width and height values.

        \sa transposed()
    */
    constexpr void transpose() noexcept { const int t = wd; wd = ht; ht = t; }
    /*!
        Returns a size with the width and height values swapped.

        \sa transpose()
    */
    constexpr QSize transposed() const noexcept { return QSize(ht, wd); }
    /*!
        Returns a size holding the maximum width and height of this size and
        the \a o size.

        \sa boundedTo()
    */
    constexpr QSize expandedTo(const QSize &o) const noexcept { return QSize(qMax(wd, o.wd), qMax(ht, o.ht)); }
    /*!
        Returns a size holding the minimum width and height of this size and
        the \a o size.

        \sa expandedTo()
    */
    constexpr QSize boundedTo(const QSize &o) const noexcept { return QSize(qMin(wd, o.wd), qMin(ht, o.ht)); }
    /*!
        \overload

        Scales this size to a rectangle with the given \a w and \a h,
        according to the given \a mode.
    */
    void scale(int w, int h, Qt::AspectRatioMode mode) { *this = scaled(QSize(w, h), mode); }
    /*!
        Scales this size to a rectangle with the given size \a s, according
        to the given \a mode.

        \sa scaled()
    */
    void scale(const QSize &s, Qt::AspectRatioMode mode) { *this = scaled(s, mode); }
    /*!
        \overload

        Returns this size scaled to a rectangle with the given \a w and
        \a h, according to the given \a mode.
    */
    QSize scaled(int w, int h, Qt::AspectRatioMode mode) const { return scaled(QSize(w, h), mode); }
    /*!
        Returns this size scaled to a rectangle with the given size \a s,
        according to the given \a mode.

        If \a mode is Qt::IgnoreAspectRatio, or this size has a zero width or
        height, \a s is returned unchanged. With Qt::KeepAspectRatio the
        result fits inside \a s; with Qt::KeepAspectRatioByExpanding it
        covers \a s.

        \sa scale()
    */
    QSize scaled(const QSize &s, Qt::AspectRatioMode mode) const {
        if (mode == Qt::IgnoreAspectRatio || wd == 0 || ht == 0)
            return s;
        const qint64 rw = qint64(s.ht) * qint64(wd) / qint64(ht);
        const bool useHeight = mode == Qt::KeepAspectRatio ? rw <= s.wd : rw >= s.wd;
        if (useHeight)
            return QSize(static_cast<int>(rw), s.ht);
        return QSize(s.wd, static_cast<int>(qint64(s.wd) * qint64(ht) / qint64(wd)));
    }
    /*!
        Returns this size as a QSizeF.

        \sa QSizeF::toSize()
    */
    constexpr QSizeF toSizeF() const noexcept;

    /*!
        Adds the given \a s to this size and returns a reference to this
        size.
    */
    constexpr QSize &operator+=(const QSize &s) { wd += s.wd; ht += s.ht; return *this; }
    /*!
        Subtracts the given \a s from this size and returns a reference to
        this size.
    */
    constexpr QSize &operator-=(const QSize &s) { wd -= s.wd; ht -= s.ht; return *this; }
    /*!
        Multiplies both the width and height by the given factor \a f,
        rounds each to the nearest integer, and returns a reference to this
        size.
    */
    QSize &operator*=(double f) { wd = qRound(wd * f); ht = qRound(ht * f); return *this; }
    /*!
        Divides both the width and height by the given divisor \a d, rounds
        each to the nearest integer, and returns a reference to this size.
    */
    QSize &operator/=(double d) { wd = qRound(wd / d); ht = qRound(ht / d); return *this; }

    friend constexpr bool operator==(const QSize &a, const QSize &b) noexcept { return a.wd == b.wd && a.ht == b.ht; }
    friend constexpr bool operator!=(const QSize &a, const QSize &b) noexcept { return !(a == b); }
    friend constexpr QSize operator+(const QSize &a, const QSize &b) noexcept { return QSize(a.wd + b.wd, a.ht + b.ht); }
    friend constexpr QSize operator-(const QSize &a, const QSize &b) noexcept { return QSize(a.wd - b.wd, a.ht - b.ht); }
    friend QSize operator*(const QSize &s, double f) { return QSize(qRound(s.wd * f), qRound(s.ht * f)); }
    friend QSize operator*(double f, const QSize &s) { return s * f; }
    friend QSize operator/(const QSize &s, double d) { return QSize(qRound(s.wd / d), qRound(s.ht / d)); }

    /*!
        \since 5.14
        Returns the size grown by the margins \a m: wider by the left and right
        margins and taller by the top and bottom ones.

        \sa shrunkBy()
    */
    constexpr QSize grownBy(QMargins m) const noexcept {
        return QSize(wd + m.left() + m.right(), ht + m.top() + m.bottom());
    }
    /*!
        \since 5.14
        Returns the size shrunk by the margins \a m.

        \sa grownBy()
    */
    constexpr QSize shrunkBy(QMargins m) const noexcept {
        return QSize(wd - m.left() - m.right(), ht - m.top() - m.bottom());
    }

private:
    int wd = -1;
    int ht = -1;
};

/*!
    \class QSizeF
    \brief The QSizeF class defines the size of a two-dimensional object using floating point precision.

    A size is specified by a width() and a height(), both stored as
    \c double. A default-constructed size is invalid, with a width and
    height of -1. Use QSize for integer dimensions.

    Equality comparison is fuzzy: two dimensions are considered equal if
    they are equal within the precision of \c double.

    \differences
    Dimensions are always \c double, which is what \c qreal is in Nt.
    \enddifferences

    \sa QSize, QPointF, QRectF
*/
class QSizeF {
public:
    /*!
        Constructs an invalid size with a width and height of -1.0.

        \sa isValid()
    */
    constexpr QSizeF() noexcept = default;
    /*!
        Constructs a size with floating point accuracy from the given
        integer size \a s.

        \sa toSize(), QSize::toSizeF()
    */
    constexpr QSizeF(const QSize &s) noexcept : wd(s.width()), ht(s.height()) {}
    /*!
        Constructs a size with the given \a w and \a h.
    */
    constexpr QSizeF(double w, double h) noexcept : wd(w), ht(h) {}

    /*!
        Returns \c true if both the width and height are fuzzily equal to
        0; otherwise returns \c false.

        \sa isEmpty(), isValid()
    */
    bool isNull() const noexcept { return qFuzzyIsNull(wd) && qFuzzyIsNull(ht); }
    /*!
        Returns \c true if the width or the height is less than or equal to
        0; otherwise returns \c false.

        \sa isNull(), isValid()
    */
    constexpr bool isEmpty() const noexcept { return wd <= 0. || ht <= 0.; }
    /*!
        Returns \c true if both the width and the height are equal to or
        greater than 0; otherwise returns \c false.

        \sa isNull(), isEmpty()
    */
    constexpr bool isValid() const noexcept { return wd >= 0. && ht >= 0.; }
    /*!
        Returns the width.

        \sa height(), setWidth(), rwidth()
    */
    constexpr double width() const noexcept { return wd; }
    /*!
        Returns the height.

        \sa width(), setHeight(), rheight()
    */
    constexpr double height() const noexcept { return ht; }
    /*!
        Sets the width to the given \a w.

        \sa width(), setHeight()
    */
    constexpr void setWidth(double w) noexcept { wd = w; }
    /*!
        Sets the height to the given \a h.

        \sa height(), setWidth()
    */
    constexpr void setHeight(double h) noexcept { ht = h; }
    /*!
        Returns a reference to the width, allowing it to be modified
        directly.

        \sa width(), setWidth()
    */
    constexpr double &rwidth() noexcept { return wd; }
    /*!
        Returns a reference to the height, allowing it to be modified
        directly.

        \sa height(), setHeight()
    */
    constexpr double &rheight() noexcept { return ht; }
    /*!
        Swaps the width and height values.

        \sa transposed()
    */
    constexpr void transpose() noexcept { const double t = wd; wd = ht; ht = t; }
    /*!
        Returns a size with the width and height values swapped.

        \sa transpose()
    */
    constexpr QSizeF transposed() const noexcept { return QSizeF(ht, wd); }
    /*!
        Returns a size holding the maximum width and height of this size and
        the \a o size.

        \sa boundedTo()
    */
    constexpr QSizeF expandedTo(const QSizeF &o) const noexcept { return QSizeF(qMax(wd, o.wd), qMax(ht, o.ht)); }
    /*!
        Returns a size holding the minimum width and height of this size and
        the \a o size.

        \sa expandedTo()
    */
    constexpr QSizeF boundedTo(const QSizeF &o) const noexcept { return QSizeF(qMin(wd, o.wd), qMin(ht, o.ht)); }
    /*!
        Scales this size to a rectangle with the given size \a s, according
        to the given \a mode.

        \sa scaled()
    */
    void scale(const QSizeF &s, Qt::AspectRatioMode mode) { *this = scaled(s, mode); }
    /*!
        Returns this size scaled to a rectangle with the given size \a s,
        according to the given \a mode.

        If \a mode is Qt::IgnoreAspectRatio, or this size has a zero width or
        height, \a s is returned unchanged. With Qt::KeepAspectRatio the
        result fits inside \a s; with Qt::KeepAspectRatioByExpanding it
        covers \a s.

        \sa scale()
    */
    QSizeF scaled(const QSizeF &s, Qt::AspectRatioMode mode) const {
        if (mode == Qt::IgnoreAspectRatio || qFuzzyIsNull(wd) || qFuzzyIsNull(ht))
            return s;
        const double rw = s.ht * wd / ht;
        const bool useHeight = mode == Qt::KeepAspectRatio ? rw <= s.wd : rw >= s.wd;
        return useHeight ? QSizeF(rw, s.ht) : QSizeF(s.wd, s.wd * ht / wd);
    }
    /*!
        Returns a QSize with the width and height rounded to the nearest
        integer.

        \sa QSize::toSizeF()
    */
    QSize toSize() const { return QSize(qRound(wd), qRound(ht)); }

    /*!
        Adds the given \a s to this size and returns a reference to this
        size.
    */
    constexpr QSizeF &operator+=(const QSizeF &s) { wd += s.wd; ht += s.ht; return *this; }
    /*!
        Subtracts the given \a s from this size and returns a reference to
        this size.
    */
    constexpr QSizeF &operator-=(const QSizeF &s) { wd -= s.wd; ht -= s.ht; return *this; }
    /*!
        Multiplies both the width and height by the given factor \a f and
        returns a reference to this size.
    */
    constexpr QSizeF &operator*=(double f) { wd *= f; ht *= f; return *this; }
    /*!
        Divides both the width and height by the given divisor \a d and
        returns a reference to this size.
    */
    constexpr QSizeF &operator/=(double d) { wd /= d; ht /= d; return *this; }

    friend bool operator==(const QSizeF &a, const QSizeF &b) {
        return (a.wd == 0. || b.wd == 0. ? qFuzzyIsNull(a.wd - b.wd) : qFuzzyCompare(a.wd, b.wd)) &&
               (a.ht == 0. || b.ht == 0. ? qFuzzyIsNull(a.ht - b.ht) : qFuzzyCompare(a.ht, b.ht));
    }
    friend bool operator!=(const QSizeF &a, const QSizeF &b) { return !(a == b); }
    friend constexpr QSizeF operator+(const QSizeF &a, const QSizeF &b) { return QSizeF(a.wd + b.wd, a.ht + b.ht); }
    friend constexpr QSizeF operator-(const QSizeF &a, const QSizeF &b) { return QSizeF(a.wd - b.wd, a.ht - b.ht); }
    friend constexpr QSizeF operator*(const QSizeF &s, double f) { return QSizeF(s.wd * f, s.ht * f); }
    friend constexpr QSizeF operator*(double f, const QSizeF &s) { return s * f; }
    friend constexpr QSizeF operator/(const QSizeF &s, double d) { return QSizeF(s.wd / d, s.ht / d); }

    /*!
        \since 5.14
        Returns the size grown by the margins \a m.

        \sa shrunkBy()
    */
    constexpr QSizeF grownBy(const QMarginsF &m) const noexcept {
        return QSizeF(wd + m.left() + m.right(), ht + m.top() + m.bottom());
    }
    /*!
        \since 5.14
        Returns the size shrunk by the margins \a m.

        \sa grownBy()
    */
    constexpr QSizeF shrunkBy(const QMarginsF &m) const noexcept {
        return QSizeF(wd - m.left() - m.right(), ht - m.top() - m.bottom());
    }
    /*!
        \overload
        Scales the size to a rectangle of width \a w and height \a h, as
        \a mode says.
    */
    void scale(double w, double h, Qt::AspectRatioMode mode) { *this = scaled(QSizeF(w, h), mode); }
    /*!
        \overload
        Returns the size scaled to a rectangle of width \a w and height \a h,
        as \a mode says.
    */
    QSizeF scaled(double w, double h, Qt::AspectRatioMode mode) const { return scaled(QSizeF(w, h), mode); }

private:
    double wd = -1.;
    double ht = -1.;
};

constexpr QSizeF QSize::toSizeF() const noexcept { return QSizeF(*this); }
