// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qnamespace.h>

class QSizeF;

class QSize {
public:
    constexpr QSize() noexcept = default;
    constexpr QSize(int w, int h) noexcept : wd(w), ht(h) {}

    constexpr bool isNull() const noexcept { return wd == 0 && ht == 0; }
    constexpr bool isEmpty() const noexcept { return wd < 1 || ht < 1; }
    constexpr bool isValid() const noexcept { return wd >= 0 && ht >= 0; }
    constexpr int width() const noexcept { return wd; }
    constexpr int height() const noexcept { return ht; }
    constexpr void setWidth(int w) noexcept { wd = w; }
    constexpr void setHeight(int h) noexcept { ht = h; }
    constexpr int &rwidth() noexcept { return wd; }
    constexpr int &rheight() noexcept { return ht; }
    constexpr void transpose() noexcept { const int t = wd; wd = ht; ht = t; }
    constexpr QSize transposed() const noexcept { return QSize(ht, wd); }
    constexpr QSize expandedTo(const QSize &o) const noexcept { return QSize(qMax(wd, o.wd), qMax(ht, o.ht)); }
    constexpr QSize boundedTo(const QSize &o) const noexcept { return QSize(qMin(wd, o.wd), qMin(ht, o.ht)); }
    void scale(int w, int h, Qt::AspectRatioMode mode) { *this = scaled(QSize(w, h), mode); }
    void scale(const QSize &s, Qt::AspectRatioMode mode) { *this = scaled(s, mode); }
    QSize scaled(int w, int h, Qt::AspectRatioMode mode) const { return scaled(QSize(w, h), mode); }
    QSize scaled(const QSize &s, Qt::AspectRatioMode mode) const {
        if (mode == Qt::IgnoreAspectRatio || wd == 0 || ht == 0)
            return s;
        const qint64 rw = qint64(s.ht) * qint64(wd) / qint64(ht);
        const bool useHeight = mode == Qt::KeepAspectRatio ? rw <= s.wd : rw >= s.wd;
        if (useHeight)
            return QSize(static_cast<int>(rw), s.ht);
        return QSize(s.wd, static_cast<int>(qint64(s.wd) * qint64(ht) / qint64(wd)));
    }
    constexpr QSizeF toSizeF() const noexcept;

    constexpr QSize &operator+=(const QSize &s) { wd += s.wd; ht += s.ht; return *this; }
    constexpr QSize &operator-=(const QSize &s) { wd -= s.wd; ht -= s.ht; return *this; }
    QSize &operator*=(double f) { wd = qRound(wd * f); ht = qRound(ht * f); return *this; }
    QSize &operator/=(double d) { wd = qRound(wd / d); ht = qRound(ht / d); return *this; }

    friend constexpr bool operator==(const QSize &a, const QSize &b) noexcept { return a.wd == b.wd && a.ht == b.ht; }
    friend constexpr bool operator!=(const QSize &a, const QSize &b) noexcept { return !(a == b); }
    friend constexpr QSize operator+(const QSize &a, const QSize &b) noexcept { return QSize(a.wd + b.wd, a.ht + b.ht); }
    friend constexpr QSize operator-(const QSize &a, const QSize &b) noexcept { return QSize(a.wd - b.wd, a.ht - b.ht); }
    friend QSize operator*(const QSize &s, double f) { return QSize(qRound(s.wd * f), qRound(s.ht * f)); }
    friend QSize operator*(double f, const QSize &s) { return s * f; }
    friend QSize operator/(const QSize &s, double d) { return QSize(qRound(s.wd / d), qRound(s.ht / d)); }

private:
    int wd = -1;
    int ht = -1;
};

class QSizeF {
public:
    constexpr QSizeF() noexcept = default;
    constexpr QSizeF(const QSize &s) noexcept : wd(s.width()), ht(s.height()) {}
    constexpr QSizeF(double w, double h) noexcept : wd(w), ht(h) {}

    bool isNull() const noexcept { return qFuzzyIsNull(wd) && qFuzzyIsNull(ht); }
    constexpr bool isEmpty() const noexcept { return wd <= 0. || ht <= 0.; }
    constexpr bool isValid() const noexcept { return wd >= 0. && ht >= 0.; }
    constexpr double width() const noexcept { return wd; }
    constexpr double height() const noexcept { return ht; }
    constexpr void setWidth(double w) noexcept { wd = w; }
    constexpr void setHeight(double h) noexcept { ht = h; }
    constexpr double &rwidth() noexcept { return wd; }
    constexpr double &rheight() noexcept { return ht; }
    constexpr void transpose() noexcept { const double t = wd; wd = ht; ht = t; }
    constexpr QSizeF transposed() const noexcept { return QSizeF(ht, wd); }
    constexpr QSizeF expandedTo(const QSizeF &o) const noexcept { return QSizeF(qMax(wd, o.wd), qMax(ht, o.ht)); }
    constexpr QSizeF boundedTo(const QSizeF &o) const noexcept { return QSizeF(qMin(wd, o.wd), qMin(ht, o.ht)); }
    void scale(const QSizeF &s, Qt::AspectRatioMode mode) { *this = scaled(s, mode); }
    QSizeF scaled(const QSizeF &s, Qt::AspectRatioMode mode) const {
        if (mode == Qt::IgnoreAspectRatio || qFuzzyIsNull(wd) || qFuzzyIsNull(ht))
            return s;
        const double rw = s.ht * wd / ht;
        const bool useHeight = mode == Qt::KeepAspectRatio ? rw <= s.wd : rw >= s.wd;
        return useHeight ? QSizeF(rw, s.ht) : QSizeF(s.wd, s.wd * ht / wd);
    }
    QSize toSize() const { return QSize(qRound(wd), qRound(ht)); }

    constexpr QSizeF &operator+=(const QSizeF &s) { wd += s.wd; ht += s.ht; return *this; }
    constexpr QSizeF &operator-=(const QSizeF &s) { wd -= s.wd; ht -= s.ht; return *this; }
    constexpr QSizeF &operator*=(double f) { wd *= f; ht *= f; return *this; }
    constexpr QSizeF &operator/=(double d) { wd /= d; ht /= d; return *this; }

    friend bool operator==(const QSizeF &a, const QSizeF &b) {
        return (qFuzzyIsNull(a.wd - b.wd) || qFuzzyCompare(a.wd, b.wd)) &&
               (qFuzzyIsNull(a.ht - b.ht) || qFuzzyCompare(a.ht, b.ht));
    }
    friend bool operator!=(const QSizeF &a, const QSizeF &b) { return !(a == b); }
    friend constexpr QSizeF operator+(const QSizeF &a, const QSizeF &b) { return QSizeF(a.wd + b.wd, a.ht + b.ht); }
    friend constexpr QSizeF operator-(const QSizeF &a, const QSizeF &b) { return QSizeF(a.wd - b.wd, a.ht - b.ht); }
    friend constexpr QSizeF operator*(const QSizeF &s, double f) { return QSizeF(s.wd * f, s.ht * f); }
    friend constexpr QSizeF operator*(double f, const QSizeF &s) { return s * f; }
    friend constexpr QSizeF operator/(const QSizeF &s, double d) { return QSizeF(s.wd / d, s.ht / d); }

private:
    double wd = -1.;
    double ht = -1.;
};

constexpr QSizeF QSize::toSizeF() const noexcept { return QSizeF(*this); }
