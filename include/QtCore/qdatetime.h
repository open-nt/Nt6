// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>

// Text formats use C-locale English names until QLocale exists.
class QDate {
public:
    constexpr QDate() noexcept = default;
    QDate(int y, int m, int d);

    constexpr bool isNull() const noexcept { return !isValid(); }
    constexpr bool isValid() const noexcept { return jd != kNullJd; }
    int year() const;
    int month() const;
    int day() const;
    int dayOfWeek() const;
    int dayOfYear() const;
    int daysInMonth() const;
    int daysInYear() const;
    int weekNumber(int *yearNumber = nullptr) const;
    bool setDate(int year, int month, int day);
    void getDate(int *year, int *month, int *day) const;
    QDate addDays(qint64 days) const;
    QDate addMonths(int months) const;
    QDate addYears(int years) const;
    qint64 daysTo(QDate d) const;
    constexpr qint64 toJulianDay() const noexcept { return jd; }
    QString toString(Qt::DateFormat format = Qt::TextDate) const;
    QString toString(const QString &format) const;

    static QDate currentDate();
    static QDate fromString(const QString &string, Qt::DateFormat format = Qt::TextDate);
    static QDate fromString(const QString &string, const QString &format);
    static bool isValid(int y, int m, int d);
    static bool isLeapYear(int year);
    static QDate fromJulianDay(qint64 julianDay);

    friend constexpr bool operator==(QDate a, QDate b) noexcept { return a.jd == b.jd; }
    friend constexpr bool operator!=(QDate a, QDate b) noexcept { return a.jd != b.jd; }
    friend constexpr bool operator<(QDate a, QDate b) noexcept { return a.jd < b.jd; }
    friend constexpr bool operator<=(QDate a, QDate b) noexcept { return a.jd <= b.jd; }
    friend constexpr bool operator>(QDate a, QDate b) noexcept { return a.jd > b.jd; }
    friend constexpr bool operator>=(QDate a, QDate b) noexcept { return a.jd >= b.jd; }

private:
    static constexpr qint64 kNullJd = std::numeric_limits<qint64>::min();
    qint64 jd = kNullJd;
};

class QTime {
public:
    constexpr QTime() noexcept = default;
    QTime(int h, int m, int s = 0, int ms = 0);

    constexpr bool isNull() const noexcept { return ms == -1; }
    constexpr bool isValid() const noexcept { return ms >= 0 && ms < 86400000; }
    int hour() const { return isValid() ? ms / 3600000 : -1; }
    int minute() const { return isValid() ? (ms % 3600000) / 60000 : -1; }
    int second() const { return isValid() ? (ms / 1000) % 60 : -1; }
    int msec() const { return isValid() ? ms % 1000 : -1; }
    bool setHMS(int h, int m, int s, int msecs = 0);
    QTime addSecs(int secs) const;
    QTime addMSecs(int msecs) const;
    int secsTo(QTime t) const;
    int msecsTo(QTime t) const;
    constexpr int msecsSinceStartOfDay() const noexcept { return ms == -1 ? 0 : ms; }
    QString toString(Qt::DateFormat format = Qt::TextDate) const;
    QString toString(const QString &format) const;

    static QTime currentTime();
    static QTime fromMSecsSinceStartOfDay(int msecs);
    static QTime fromString(const QString &string, Qt::DateFormat format = Qt::TextDate);
    static QTime fromString(const QString &string, const QString &format);
    static bool isValid(int h, int m, int s, int ms = 0);

    friend constexpr bool operator==(QTime a, QTime b) noexcept { return a.ms == b.ms; }
    friend constexpr bool operator!=(QTime a, QTime b) noexcept { return a.ms != b.ms; }
    friend constexpr bool operator<(QTime a, QTime b) noexcept { return a.ms < b.ms; }
    friend constexpr bool operator<=(QTime a, QTime b) noexcept { return a.ms <= b.ms; }
    friend constexpr bool operator>(QTime a, QTime b) noexcept { return a.ms > b.ms; }
    friend constexpr bool operator>=(QTime a, QTime b) noexcept { return a.ms >= b.ms; }

private:
    int ms = -1;
};

class QDateTime {
public:
    QDateTime() = default;
    QDateTime(QDate date, QTime time, Qt::TimeSpec spec = Qt::LocalTime, int offsetSeconds = 0);

    bool isNull() const noexcept { return m_date.isNull() && m_time.isNull(); }
    bool isValid() const noexcept { return m_date.isValid() && m_time.isValid(); }
    QDate date() const { return m_date; }
    QTime time() const { return m_time; }
    Qt::TimeSpec timeSpec() const { return m_spec; }
    int offsetFromUtc() const;
    qint64 toMSecsSinceEpoch() const;
    qint64 toSecsSinceEpoch() const;
    void setDate(QDate date) { m_date = date; if (!m_time.isValid()) m_time = QTime(0, 0); }
    void setTime(QTime time) { m_time = time; }
    void setTimeSpec(Qt::TimeSpec spec) { m_spec = spec == Qt::OffsetFromUTC ? Qt::OffsetFromUTC : spec; if (spec != Qt::OffsetFromUTC) m_offset = 0; }
    void setOffsetFromUtc(int offsetSeconds);
    void setMSecsSinceEpoch(qint64 msecs);
    void setSecsSinceEpoch(qint64 secs) { setMSecsSinceEpoch(secs * 1000); }

    QDateTime addDays(qint64 days) const;
    QDateTime addMonths(int months) const;
    QDateTime addYears(int years) const;
    QDateTime addSecs(qint64 secs) const { return addMSecs(secs * 1000); }
    QDateTime addMSecs(qint64 msecs) const;
    qint64 daysTo(const QDateTime &o) const { return m_date.daysTo(o.m_date); }
    qint64 secsTo(const QDateTime &o) const { return msecsTo(o) / 1000; }
    qint64 msecsTo(const QDateTime &o) const { return o.toMSecsSinceEpoch() - toMSecsSinceEpoch(); }
    QDateTime toTimeSpec(Qt::TimeSpec spec) const;
    QDateTime toLocalTime() const { return toTimeSpec(Qt::LocalTime); }
    QDateTime toUTC() const { return toTimeSpec(Qt::UTC); }
    QDateTime toOffsetFromUtc(int offsetSeconds) const;
    QString toString(Qt::DateFormat format = Qt::TextDate) const;
    QString toString(const QString &format) const;

    static QDateTime currentDateTime();
    static QDateTime currentDateTimeUtc();
    static qint64 currentMSecsSinceEpoch() noexcept;
    static qint64 currentSecsSinceEpoch() noexcept;
    static QDateTime fromMSecsSinceEpoch(qint64 msecs, Qt::TimeSpec spec = Qt::LocalTime, int offsetFromUtc = 0);
    static QDateTime fromSecsSinceEpoch(qint64 secs, Qt::TimeSpec spec = Qt::LocalTime, int offsetFromUtc = 0);
    static QDateTime fromString(const QString &string, Qt::DateFormat format = Qt::TextDate);
    static QDateTime fromString(const QString &string, const QString &format);

    friend bool operator==(const QDateTime &a, const QDateTime &b) { return a.isValid() == b.isValid() && (!a.isValid() || a.toMSecsSinceEpoch() == b.toMSecsSinceEpoch()); }
    friend bool operator!=(const QDateTime &a, const QDateTime &b) { return !(a == b); }
    friend bool operator<(const QDateTime &a, const QDateTime &b) { return a.toMSecsSinceEpoch() < b.toMSecsSinceEpoch(); }
    friend bool operator<=(const QDateTime &a, const QDateTime &b) { return !(b < a); }
    friend bool operator>(const QDateTime &a, const QDateTime &b) { return b < a; }
    friend bool operator>=(const QDateTime &a, const QDateTime &b) { return !(a < b); }

private:
    QDate m_date;
    QTime m_time;
    Qt::TimeSpec m_spec = Qt::LocalTime;
    int m_offset = 0;
};
