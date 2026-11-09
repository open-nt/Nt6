// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>

#include <memory>

class QDateTime;
class QTimeZone;
class QCalendar;
struct QTimeZonePrivate;

// Text formats use C-locale English names until QLocale exists.
class QDate {
public:
    /*!
        Constructs a null date. A null date is invalid.

        \sa isNull(), isValid()
    */
    constexpr QDate() noexcept = default;
    QDate(int y, int m, int d);
    QDate(int y, int m, int d, QCalendar cal);

    /*!
        Returns \c true if the date is null; otherwise returns \c false. A null
        date is invalid.

        \sa isValid()
    */
    constexpr bool isNull() const noexcept { return !isValid(); }
    /*!
        Returns \c true if the date is valid; otherwise returns \c false.

        \sa isNull()
    */
    constexpr bool isValid() const noexcept { return jd != kNullJd; }
    int year() const;
    int month() const;
    int day() const;
    int dayOfWeek() const;
    int dayOfYear() const;
    int daysInMonth() const;
    int daysInYear() const;
    int year(QCalendar cal) const;
    int month(QCalendar cal) const;
    int day(QCalendar cal) const;
    int dayOfYear(QCalendar cal) const;
    int daysInMonth(QCalendar cal) const;
    int daysInYear(QCalendar cal) const;
    bool setDate(int year, int month, int day, QCalendar cal);
    QDate addMonths(int months, QCalendar cal) const;
    QDate addYears(int years, QCalendar cal) const;
    int weekNumber(int *yearNumber = nullptr) const;
    bool setDate(int year, int month, int day);
    void getDate(int *year, int *month, int *day) const;
    QDate addDays(qint64 days) const;
    QDate addMonths(int months) const;
    QDate addYears(int years) const;
    qint64 daysTo(QDate d) const;
    QDateTime startOfDay(Qt::TimeSpec spec = Qt::LocalTime, int offsetSeconds = 0) const;
    QDateTime endOfDay(Qt::TimeSpec spec = Qt::LocalTime, int offsetSeconds = 0) const;
    QDateTime startOfDay(const QTimeZone &zone) const;
    QDateTime endOfDay(const QTimeZone &zone) const;
    /*!
        Returns the date as a Julian day number, or the minimum \c qint64
        value if the date is invalid.

        \sa fromJulianDay()
    */
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
    /*!
        Constructs a null time. A null time is invalid.

        \sa isNull(), isValid()
    */
    constexpr QTime() noexcept = default;
    QTime(int h, int m, int s = 0, int ms = 0);

    /*!
        Returns \c true if the time is null, which means it was default
        constructed and not set; otherwise returns \c false. A null time is
        also invalid.

        \sa isValid()
    */
    constexpr bool isNull() const noexcept { return ms == -1; }
    /*!
        Returns \c true if the time is valid; otherwise returns \c false.

        \sa isNull()
    */
    constexpr bool isValid() const noexcept { return ms >= 0 && ms < 86400000; }
    /*!
        Returns the hour, from 0 to 23, or -1 if the time is invalid.

        \sa minute(), second(), msec()
    */
    int hour() const { return isValid() ? ms / 3600000 : -1; }
    /*!
        Returns the minute, from 0 to 59, or -1 if the time is invalid.

        \sa hour(), second(), msec()
    */
    int minute() const { return isValid() ? (ms % 3600000) / 60000 : -1; }
    /*!
        Returns the second, from 0 to 59, or -1 if the time is invalid.

        \sa hour(), minute(), msec()
    */
    int second() const { return isValid() ? (ms / 1000) % 60 : -1; }
    /*!
        Returns the millisecond, from 0 to 999, or -1 if the time is invalid.

        \sa hour(), minute(), second()
    */
    int msec() const { return isValid() ? ms % 1000 : -1; }
    bool setHMS(int h, int m, int s, int msecs = 0);
    QTime addSecs(int secs) const;
    QTime addMSecs(int msecs) const;
    int secsTo(QTime t) const;
    int msecsTo(QTime t) const;
    /*!
        Returns the number of milliseconds since midnight, or 0 if the time is
        invalid.

        \sa fromMSecsSinceStartOfDay()
    */
    constexpr int msecsSinceStartOfDay() const noexcept { return isValid() ? ms : 0; }
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
    /*!
        Constructs a null datetime, which is invalid.

        \sa isNull(), isValid()
    */
    QDateTime() = default;
    QDateTime(QDate date, QTime time, Qt::TimeSpec spec = Qt::LocalTime, int offsetSeconds = 0);
    QDateTime(QDate date, QTime time, const QTimeZone &timeZone);

    /*!
        Returns \c true if both the date and the time are null; otherwise
        returns \c false.

        \sa isValid()
    */
    bool isNull() const noexcept { return m_date.isNull() && m_time.isNull(); }
    /*!
        Returns \c true if both the date and the time are valid; otherwise
        returns \c false.

        \sa isNull()
    */
    bool isValid() const noexcept { return m_date.isValid() && m_time.isValid(); }
    /*!
        Returns the date part of the datetime.

        \sa time(), setDate()
    */
    QDate date() const { return m_date; }
    /*!
        Returns the time part of the datetime.

        \sa date(), setTime()
    */
    QTime time() const { return m_time; }
    /*!
        Returns the time specification of the datetime.

        \sa setTimeSpec(), toTimeSpec()
    */
    Qt::TimeSpec timeSpec() const { return m_spec; }
    int offsetFromUtc() const;
    qint64 toMSecsSinceEpoch() const;
    qint64 toSecsSinceEpoch() const;
    /*!
        Sets the date part to \a date. If the time is not valid, it is set to
        midnight.

        \sa date(), setTime()
    */
    void setDate(QDate date) {
        m_date = date;
        if (!m_time.isValid())
            m_time = QTime(0, 0);
        if (m_spec == Qt::TimeZone)
            m_offset = kNoOffsetHint;
    }
    /*!
        Sets the time part to \a time.

        \sa time(), setDate()
    */
    void setTime(QTime time) {
        m_time = time;
        if (m_spec == Qt::TimeZone)
            m_offset = kNoOffsetHint;
    }
    /*!
        Sets the time specification to \a spec, keeping the date and time
        fields as they are. The offset is reset to 0 unless \a spec is
        Qt::OffsetFromUTC; use setOffsetFromUtc() to set a fixed offset.

        \sa timeSpec(), toTimeSpec()
    */
    void setTimeSpec(Qt::TimeSpec spec) {
        m_spec = spec == Qt::TimeZone ? Qt::LocalTime : spec;
        if (spec != Qt::OffsetFromUTC)
            m_offset = 0;
        m_zone.reset();
    }
    void setOffsetFromUtc(int offsetSeconds);
    QTimeZone timeZone() const;
    void setTimeZone(const QTimeZone &toZone);
    QDateTime toTimeZone(const QTimeZone &toZone) const;
    bool isDaylightTime() const;
    QString timeZoneAbbreviation() const;
    /*!
        \since 5.0
        Swaps this datetime with \a other.
    */
    void swap(QDateTime &other) noexcept {
        std::swap(m_date, other.m_date);
        std::swap(m_time, other.m_time);
        std::swap(m_spec, other.m_spec);
        std::swap(m_offset, other.m_offset);
        m_zone.swap(other.m_zone);
    }
    void setMSecsSinceEpoch(qint64 msecs);
    /*!
        Sets the datetime to \a secs seconds since the Unix epoch, shown in the
        datetime's current time specification.

        \sa toSecsSinceEpoch(), setMSecsSinceEpoch()
    */
    void setSecsSinceEpoch(qint64 secs) { setMSecsSinceEpoch(secs * 1000); }

    QDateTime addDays(qint64 days) const;
    QDateTime addMonths(int months) const;
    QDateTime addYears(int years) const;
    /*!
        Returns a QDateTime that is \a secs seconds after this one, or before it
        if \a secs is negative.

        \sa addMSecs(), secsTo()
    */
    QDateTime addSecs(qint64 secs) const { return addMSecs(secs * 1000); }
    QDateTime addMSecs(qint64 msecs) const;
    /*!
        Returns the number of days from this datetime's date to the date of
        \a o, which is negative if \a o is earlier. The times are not taken
        into account. Returns 0 if either date is invalid.

        \sa secsTo(), addDays()
    */
    qint64 daysTo(const QDateTime &o) const { return m_date.daysTo(o.m_date); }
    /*!
        Returns the number of whole seconds from this datetime to \a o, which
        is negative if \a o is earlier. The difference is taken between the
        instants the two datetimes represent, so their time specifications
        do not matter.

        \sa msecsTo(), addSecs()
    */
    qint64 secsTo(const QDateTime &o) const { return msecsTo(o) / 1000; }
    /*!
        Returns the number of milliseconds from this datetime to \a o, which
        is negative if \a o is earlier. The difference is taken between the
        instants the two datetimes represent, so their time specifications
        do not matter.

        \sa secsTo(), addMSecs()
    */
    qint64 msecsTo(const QDateTime &o) const { return o.toMSecsSinceEpoch() - toMSecsSinceEpoch(); }
    QDateTime toTimeSpec(Qt::TimeSpec spec) const;
    /*!
        Returns a copy of this datetime converted to local time.

        \sa toUTC(), toTimeSpec()
    */
    QDateTime toLocalTime() const { return toTimeSpec(Qt::LocalTime); }
    /*!
        Returns a copy of this datetime converted to UTC.

        \sa toLocalTime(), toTimeSpec()
    */
    QDateTime toUTC() const { return toTimeSpec(Qt::UTC); }
    QDateTime toOffsetFromUtc(int offsetSeconds) const;
    QString toString(Qt::DateFormat format = Qt::TextDate) const;
    QString toString(const QString &format) const;

    static QDateTime currentDateTime();
    static QDateTime currentDateTime(const QTimeZone &zone);
    static QDateTime currentDateTimeUtc();
    static qint64 currentMSecsSinceEpoch() noexcept;
    static qint64 currentSecsSinceEpoch() noexcept;
    static QDateTime fromMSecsSinceEpoch(qint64 msecs, Qt::TimeSpec spec = Qt::LocalTime, int offsetFromUtc = 0);
    static QDateTime fromSecsSinceEpoch(qint64 secs, Qt::TimeSpec spec = Qt::LocalTime, int offsetFromUtc = 0);
    static QDateTime fromMSecsSinceEpoch(qint64 msecs, const QTimeZone &timeZone);
    static QDateTime fromSecsSinceEpoch(qint64 secs, const QTimeZone &timeZone);
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
    // For Qt::OffsetFromUTC the offset; for Qt::TimeZone the offset last computed, which tells the
    // two occurrences of a local time in a daylight-saving fold apart.
    int m_offset = 0;
    static constexpr int kNoOffsetHint = std::numeric_limits<int>::min();
    std::shared_ptr<const QTimeZonePrivate> m_zone; // for Qt::TimeZone
};
