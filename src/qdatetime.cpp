// MIT License - see LICENSE
#include <QtCore/qdatetime.h>
#include <QtCore/qtimezone.h>

#include "qtimezone_p.h"
#include <QtCore/qstringlist.h>

#include <sys/time.h>

#include <cstdio>
#include <ctime>

namespace {

constexpr qint64 kEpochJd = 2440588;
constexpr qint64 kMsPerDay = 86400000;

constexpr const char *kMonths[] = {"January", "February", "March", "April", "May", "June", "July",
                                   "August", "September", "October", "November", "December"};
constexpr const char *kDays[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

qint64 floorDiv(qint64 a, qint64 b) { return a / b - ((a % b != 0 && (a < 0) != (b < 0)) ? 1 : 0); }
qint64 floorMod(qint64 a, qint64 b) { return a - floorDiv(a, b) * b; }

// There is no year 0: year -1 is astronomical year 0.
int toAstronomical(int year) { return year < 0 ? year + 1 : year; }
int fromAstronomical(qint64 year) { return static_cast<int>(year <= 0 ? year - 1 : year); }

qint64 julianFromCivil(int astroYear, int m, int d) {
    const qint64 a = (14 - m) / 12;
    const qint64 y = qint64(astroYear) + 4800 - a;
    const qint64 mm = m + 12 * a - 3;
    return d + (153 * mm + 2) / 5 + 365 * y + floorDiv(y, 4) - floorDiv(y, 100) + floorDiv(y, 400) - 32045;
}

void civilFromJulian(qint64 jd, int *year, int *month, int *day) {
    const qint64 a = jd + 32044;
    const qint64 b = floorDiv(4 * a + 3, 146097);
    const qint64 c = a - floorDiv(146097 * b, 4);
    const qint64 d = floorDiv(4 * c + 3, 1461);
    const qint64 e = c - floorDiv(1461 * d, 4);
    const qint64 m = floorDiv(5 * e + 2, 153);
    if (day)
        *day = static_cast<int>(e - floorDiv(153 * m + 2, 5) + 1);
    if (month)
        *month = static_cast<int>(m + 3 - 12 * floorDiv(m, 10));
    if (year)
        *year = fromAstronomical(100 * b + d - 4800 + floorDiv(m, 10));
}

int monthLength(int year, int month) {
    static const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && QDate::isLeapYear(year) ? 29 : lengths[month - 1];
}

QString pad(int v, int width) {
    char b[16];
    std::snprintf(b, sizeof b, "%0*d", width, v);
    return QString::fromUtf8(b);
}

QString latin(const char *s) { return QString::fromUtf8(s); }

QString shortName(const char *s) { return QString::fromUtf8(s).left(3); }

QString offsetText(int seconds, bool colon = true) {
    const char sign = seconds < 0 ? '-' : '+';
    const int a = seconds < 0 ? -seconds : seconds;
    char b[16];
    std::snprintf(b, sizeof b, colon ? "%c%02d:%02d" : "%c%02d%02d", sign, a / 3600, (a % 3600) / 60);
    return QString::fromUtf8(b);
}

// Formats with Qt-style tokens; `dt` adds the time zone token.
QString formatTokens(const QString &f, QDate date, QTime time, const QDateTime *dt = nullptr) {
    bool twelve = false;
    for (qsizetype i = 0, quoted = 0; i < f.size() && !twelve; ++i) {
        if (f[i] == QChar(u'\''))
            quoted ^= 1;
        else if (!quoted && (f[i] == QChar(u'a') || f[i] == QChar(u'A')))
            twelve = true;
    }
    QString out;
    const qsizetype n = f.size();
    for (qsizetype i = 0; i < n;) {
        const QChar c = f[i];
        if (c == QChar(u'\'')) {
            ++i;
            if (i < n && f[i] == QChar(u'\'')) {
                out.append(QChar(u'\''));
                ++i;
                continue;
            }
            while (i < n) {
                if (f[i] == QChar(u'\'')) {
                    if (i + 1 < n && f[i + 1] == QChar(u'\'')) {
                        out.append(QChar(u'\''));
                        i += 2;
                        continue;
                    }
                    ++i;
                    break;
                }
                out.append(f[i++]);
            }
            continue;
        }
        qsizetype run = 1;
        while (i + run < n && f[i + run] == c)
            ++run;
        const char16_t u = c.unicode();
        const bool haveDate = date.isValid(), haveTime = time.isValid();
        if (u == u'd' && haveDate) {
            run = qMin<qsizetype>(run, 4);
            if (run == 1) out.append(QString::number(date.day()));
            else if (run == 2) out.append(pad(date.day(), 2));
            else if (run == 3) out.append(shortName(kDays[date.dayOfWeek() - 1]));
            else out.append(latin(kDays[date.dayOfWeek() - 1]));
        } else if (u == u'M' && haveDate) {
            run = qMin<qsizetype>(run, 4);
            if (run == 1) out.append(QString::number(date.month()));
            else if (run == 2) out.append(pad(date.month(), 2));
            else if (run == 3) out.append(shortName(kMonths[date.month() - 1]));
            else out.append(latin(kMonths[date.month() - 1]));
        } else if (u == u'y' && haveDate && run >= 2) {
            if (run >= 4) {
                run = 4;
                out.append(date.year() < 0 ? QChar(u'-') + pad(-date.year(), 4) : pad(date.year(), 4));
            } else {
                run = 2;
                const int yy = date.year() % 100;
                out.append(pad(yy < 0 ? -yy : yy, 2));
            }
        } else if ((u == u'h' || u == u'H') && haveTime) {
            run = qMin<qsizetype>(run, 2);
            int h = time.hour();
            if (u == u'h' && twelve)
                h = h % 12 == 0 ? 12 : h % 12;
            out.append(run == 1 ? QString::number(h) : pad(h, 2));
        } else if (u == u'm' && haveTime) {
            run = qMin<qsizetype>(run, 2);
            out.append(run == 1 ? QString::number(time.minute()) : pad(time.minute(), 2));
        } else if (u == u's' && haveTime) {
            run = qMin<qsizetype>(run, 2);
            out.append(run == 1 ? QString::number(time.second()) : pad(time.second(), 2));
        } else if (u == u'z' && haveTime) {
            if (run >= 3) {
                run = 3;
                out.append(pad(time.msec(), 3));
            } else {
                run = 1;
                QString t = pad(time.msec(), 3);
                while (t.size() > 1 && t.endsWith(QChar(u'0')))
                    t.chop(1);
                out.append(t);
            }
        } else if ((u == u'A' || u == u'a') && haveTime) {
            const bool two = i + 1 < n && (f[i + 1] == QChar(u'P') || f[i + 1] == QChar(u'p'));
            run = two ? 2 : 1;
            const QString m = time.hour() < 12 ? QString("AM") : QString("PM");
            out.append(u == u'A' ? m : m.toLower());
        } else if (u == u't' && dt) {
            run = 1;
            if (dt->timeSpec() == Qt::UTC)
                out.append(QString("UTC"));
            else if (dt->timeSpec() == Qt::TimeZone)
                out.append(dt->timeZoneAbbreviation());
            else
                out.append(QString("UTC") + offsetText(dt->offsetFromUtc()));
        } else {
            for (qsizetype k = 0; k < run; ++k)
                out.append(c);
        }
        i += run;
    }
    return out;
}

bool readInt(const QString &s, qsizetype &pos, int minDigits, int maxDigits, int *out) {
    int v = 0, digits = 0;
    while (pos < s.size() && digits < maxDigits && QtPrivate::isAsciiDigit(s[pos])) {
        v = v * 10 + (s[pos].unicode() - u'0');
        ++pos;
        ++digits;
    }
    *out = v;
    return digits >= minDigits;
}

struct Parsed {
    int year = 1900, month = 1, day = 1;
    int hour = 0, minute = 0, second = 0, ms = 0;
    int pm = -1;
    bool hour12 = false;
};

int matchName(const QString &s, qsizetype &pos, const char *const *names, int count, int length) {
    for (int i = 0; i < count; ++i) {
        const QString name = length ? latin(names[i]).left(length) : latin(names[i]);
        if (s.mid(pos, name.size()).compare(name, Qt::CaseInsensitive) == 0) {
            pos += name.size();
            return i + 1;
        }
    }
    return 0;
}

bool parseTokens(const QString &s, const QString &f, Parsed *p) {
    qsizetype pos = 0;
    const qsizetype n = f.size();
    for (qsizetype i = 0; i < n;) {
        const QChar c = f[i];
        if (c == QChar(u'\'')) {
            ++i;
            while (i < n && f[i] != QChar(u'\'')) {
                if (pos >= s.size() || s[pos] != f[i])
                    return false;
                ++pos;
                ++i;
            }
            ++i;
            continue;
        }
        qsizetype run = 1;
        while (i + run < n && f[i + run] == c)
            ++run;
        const char16_t u = c.unicode();
        if (u == u'd') {
            run = qMin<qsizetype>(run, 4);
            if (run <= 2) {
                if (!readInt(s, pos, static_cast<int>(run), 2, &p->day))
                    return false;
            } else if (!matchName(s, pos, kDays, 7, run == 3 ? 3 : 0)) {
                return false;
            }
        } else if (u == u'M') {
            run = qMin<qsizetype>(run, 4);
            if (run <= 2) {
                if (!readInt(s, pos, static_cast<int>(run), 2, &p->month))
                    return false;
            } else {
                p->month = matchName(s, pos, kMonths, 12, run == 3 ? 3 : 0);
                if (p->month == 0)
                    return false;
            }
        } else if (u == u'y' && run >= 4) {
            run = 4;
            const bool negative = pos < s.size() && s[pos] == QChar(u'-');
            if (negative)
                ++pos;
            if (!readInt(s, pos, 4, 4, &p->year))
                return false;
            if (negative)
                p->year = -p->year;
        } else if (u == u'y' && run >= 2) {
            run = 2;
            if (!readInt(s, pos, 2, 2, &p->year))
                return false;
            p->year += 1900; // same default century as the absent-year case
        } else if (u == u'h' || u == u'H') {
            run = qMin<qsizetype>(run, 2);
            if (!readInt(s, pos, static_cast<int>(run), 2, &p->hour))
                return false;
            p->hour12 = u == u'h';
        } else if (u == u'm') {
            run = qMin<qsizetype>(run, 2);
            if (!readInt(s, pos, static_cast<int>(run), 2, &p->minute))
                return false;
        } else if (u == u's') {
            run = qMin<qsizetype>(run, 2);
            if (!readInt(s, pos, static_cast<int>(run), 2, &p->second))
                return false;
        } else if (u == u'z') {
            run = run >= 3 ? 3 : 1;
            const qsizetype start = pos;
            if (!readInt(s, pos, run == 3 ? 3 : 1, 3, &p->ms))
                return false;
            for (qsizetype k = pos - start; k < 3; ++k)
                p->ms *= 10;
        } else if (u == u'A' || u == u'a') {
            run = i + 1 < n && (f[i + 1] == QChar(u'P') || f[i + 1] == QChar(u'p')) ? 2 : 1;
            const QString m = s.mid(pos, 2).toUpper();
            if (m == QString("AM")) p->pm = 0;
            else if (m == QString("PM")) p->pm = 1;
            else return false;
            pos += 2;
        } else {
            for (qsizetype k = 0; k < run; ++k, ++pos)
                if (pos >= s.size() || s[pos] != c)
                    return false;
        }
        i += run;
    }
    if (pos != s.size())
        return false;
    if (p->pm >= 0 && p->hour12) {
        if (p->hour < 1 || p->hour > 12)
            return false;
        p->hour = p->hour % 12 + (p->pm ? 12 : 0);
    }
    return true;
}

// Parses "HH:mm[:ss[.zzz]]" and returns where it stopped.
qsizetype parseIsoTime(const QString &s, qsizetype pos, QTime *out) {
    int h, m, sec = 0, ms = 0;
    if (!readInt(s, pos, 2, 2, &h) || pos >= s.size() || s[pos] != QChar(u':'))
        return -1;
    ++pos;
    if (!readInt(s, pos, 2, 2, &m))
        return -1;
    if (pos < s.size() && s[pos] == QChar(u':')) {
        ++pos;
        if (!readInt(s, pos, 2, 2, &sec))
            return -1;
        if (pos < s.size() && (s[pos] == QChar(u'.') || s[pos] == QChar(u','))) {
            ++pos;
            int digits = 0, value = 0;
            while (pos < s.size() && QtPrivate::isAsciiDigit(s[pos])) {
                if (digits < 3) {
                    value = value * 10 + (s[pos].unicode() - u'0');
                    ++digits;
                }
                ++pos;
            }
            if (digits == 0)
                return -1;
            for (; digits < 3; ++digits)
                value *= 10;
            ms = value;
        }
    }
    *out = QTime(h, m, sec, ms);
    return out->isValid() ? pos : -1;
}

int systemOffsetAt(time_t seconds) {
#ifdef __ZEPHYR__
    // The C library has no time zones: local time is UTC.
    (void)seconds;
    return 0;
#else
    struct tm t;
    localtime_r(&seconds, &t);
    return static_cast<int>(t.tm_gmtoff);
#endif
}

} // namespace

// QDate

/*!
    \class QDate
    \brief The QDate class provides date functions.

    A QDate represents a calendar date in the proleptic Gregorian calendar,
    as a year, month and day. Year 0 does not exist: the year before 1 is -1.
    A default-constructed QDate is null and invalid; so is a QDate built from
    an impossible date such as February 30.

    \code
    QDate d(2026, 11, 7);
    d.dayOfWeek();            // 6, Saturday
    d.addMonths(3).toString(Qt::ISODate); // "2027-02-07"
    \endcode

    Text formats use English month and day names, since Nt has no QLocale.

    \differences
    There is no QCalendar support, so every function uses the Gregorian
    calendar, and there are no \c startOfDay(), \c endOfDay() or
    \c currentDate() overloads that take a QTimeZone, \c longMonthName(), \c shortMonthName(),
    \c longDayName() or \c shortDayName(). Text formats use C-locale English
    names. The format string passed to toString() and fromString() supports
    the \c d, \c M and \c y tokens, and single-quoted literal text.
    \enddifferences

    \sa QTime, QDateTime
*/

/*!
    Constructs a date with year \a y, month \a m and day \a d. If the date is
    not valid, the QDate is null.

    \sa isValid(), setDate()
*/
QDate::QDate(int y, int m, int d) { setDate(y, m, d); }

/*!
    Returns \c true if \a year is a leap year in the Gregorian calendar;
    otherwise returns \c false. Year 1 BC is year -1 and is a leap year.

    \sa daysInYear()
*/
bool QDate::isLeapYear(int y) {
    const int a = toAstronomical(y);
    return a % 4 == 0 && (a % 100 != 0 || a % 400 == 0);
}

/*!
    \overload

    Returns \c true if the specified date (\a y, \a m, \a d) is valid;
    otherwise returns \c false. A date is valid if the year is not 0, the
    month is 1 to 12, and the day exists in that month.

    \sa setDate()
*/
bool QDate::isValid(int y, int m, int d) {
    return y != 0 && m >= 1 && m <= 12 && d >= 1 && d <= monthLength(y, m);
}

/*!
    Sets the date to \a year, \a month and \a day. Returns \c true if the date
    is valid; otherwise makes the QDate null and returns \c false.

    \sa isValid(), getDate()
*/
bool QDate::setDate(int y, int m, int d) {
    if (!isValid(y, m, d)) {
        jd = kNullJd;
        return false;
    }
    jd = julianFromCivil(toAstronomical(y), m, d);
    return true;
}

/*!
    Stores the year, month and day of the date in \a year, \a month and
    \a day. A pointer may be \c nullptr to skip that value. All three are set
    to 0 if the date is invalid.

    \sa setDate()
*/
void QDate::getDate(int *y, int *m, int *d) const {
    if (isValid()) {
        civilFromJulian(jd, y, m, d);
        return;
    }
    if (y) *y = 0;
    if (m) *m = 0;
    if (d) *d = 0;
}

/*!
    Returns the year of the date, or 0 if the date is invalid. There is no
    year 0, so the year before 1 is -1.

    \sa month(), day()
*/
int QDate::year() const { int v = 0; getDate(&v, nullptr, nullptr); return v; }
/*!
    Returns the month of the date, from 1 (January) to 12 (December), or 0 if
    the date is invalid.

    \sa year(), day()
*/
int QDate::month() const { int v = 0; getDate(nullptr, &v, nullptr); return v; }
/*!
    Returns the day of the month, from 1 to 31, or 0 if the date is invalid.

    \sa year(), month(), dayOfWeek()
*/
int QDate::day() const { int v = 0; getDate(nullptr, nullptr, &v); return v; }
/*!
    Returns the day of the week, from 1 (Monday) to 7 (Sunday), or 0 if the
    date is invalid.

    \sa day(), dayOfYear()
*/
int QDate::dayOfWeek() const { return isValid() ? static_cast<int>(floorMod(jd, 7)) + 1 : 0; }
/*!
    Returns the day of the year, from 1 for 1 January, or 0 if the date is
    invalid.

    \sa day(), daysInYear()
*/
int QDate::dayOfYear() const { return isValid() ? static_cast<int>(jd - QDate(year(), 1, 1).jd) + 1 : 0; }
/*!
    Returns the number of days in the date's month, from 28 to 31, or 0 if
    the date is invalid.

    \sa daysInYear()
*/
int QDate::daysInMonth() const { return isValid() ? monthLength(year(), month()) : 0; }
/*!
    Returns the number of days in the date's year, 365 or 366, or 0 if the
    date is invalid.

    \sa daysInMonth(), isLeapYear()
*/
int QDate::daysInYear() const { return isValid() ? (isLeapYear(year()) ? 366 : 365) : 0; }

/*!
    Returns the ISO 8601 week number, from 1 to 53, or 0 if the date is
    invalid. Weeks start on Monday, and week 1 is the week with the year's
    first Thursday. If \a yearNumber is not \c nullptr, it is set to the year
    that the week belongs to, which can differ from year() near the start and
    end of the year.

    \sa dayOfYear()
*/
int QDate::weekNumber(int *yearNumber) const {
    if (!isValid())
        return 0;
    const QDate thursday = addDays(4 - dayOfWeek());
    if (yearNumber)
        *yearNumber = thursday.year();
    return (thursday.dayOfYear() - 1) / 7 + 1;
}

/*!
    Returns a QDate that is \a days days after this date, or before it if
    \a days is negative. Returns a null QDate if this date is invalid or the
    result is out of range.

    \sa addMonths(), addYears(), daysTo()
*/
QDate QDate::addDays(qint64 days) const {
    qint64 target;
    if (!isValid() || __builtin_add_overflow(jd, days, &target))
        return QDate();
    return fromJulianDay(target);
}

/*!
    Returns a QDate that is \a months months after this date, or before it if
    \a months is negative. If the resulting month is too short for the day, the
    day is clamped to its last day. Returns a null QDate if this date is
    invalid.

    \sa addDays(), addYears()
*/
QDate QDate::addMonths(int months) const {
    if (!isValid())
        return QDate();
    int y, m, d;
    getDate(&y, &m, &d);
    const qint64 total = qint64(toAstronomical(y)) * 12 + (m - 1) + months;
    const int ny = fromAstronomical(floorDiv(total, 12));
    const int nm = static_cast<int>(floorMod(total, 12)) + 1;
    return QDate(ny, nm, qMin(d, monthLength(ny, nm)));
}

/*!
    Returns a QDate that is \a years years after this date, or before it if
    \a years is negative. 29 February is clamped to 28 February if the
    resulting year is not a leap year. Returns a null QDate if this date is
    invalid.

    \sa addDays(), addMonths()
*/
QDate QDate::addYears(int years) const {
    if (!isValid())
        return QDate();
    int y, m, d;
    getDate(&y, &m, &d);
    const int ny = fromAstronomical(qint64(toAstronomical(y)) + years);
    return QDate(ny, m, qMin(d, monthLength(ny, m)));
}

/*!
    Returns the number of days from this date to \a d, which is negative if
    \a d is earlier. Returns 0 if either date is invalid.

    \sa addDays()
*/
qint64 QDate::daysTo(QDate d) const { return isValid() && d.isValid() ? d.jd - jd : 0; }

/*!
    Returns the QDate for the Julian day \a julianDay. Returns a null QDate if
    \a julianDay is outside the range that Qt supports.

    \sa toJulianDay()
*/
QDate QDate::fromJulianDay(qint64 julianDay) {
    QDate d;
    // Same limits as Qt, so that years still fit in an int.
    if (julianDay >= -784350574879LL && julianDay <= 784354017364LL)
        d.jd = julianDay;
    return d;
}

/*!
    Returns the current date, as reported by the system clock in local time.

    \sa QDateTime::currentDateTime(), QTime::currentTime()
*/
QDate QDate::currentDate() { return QDateTime::currentDateTime().date(); }

namespace {

// Returns true if \a dt names a local time that exists, that is, one that survives a round trip
// through the epoch (a time in a daylight-saving gap is moved by mktime()).
bool isExistingLocalTime(const QDateTime &dt) {
    const QDateTime back = QDateTime::fromMSecsSinceEpoch(dt.toMSecsSinceEpoch(), Qt::LocalTime);
    return back.date() == dt.date() && back.time() == dt.time();
}

} // namespace

/*!
    \since 5.14
    Returns the first moment of this date in the time specification \a spec,
    with \a offsetSeconds as the offset when \a spec is Qt::OffsetFromUTC.
    The result is midnight, unless the date starts later in local time
    because a daylight-saving transition skips midnight; then it is the first
    minute of the day that exists. Returns an invalid QDateTime if the date is
    invalid.

    \sa endOfDay()
*/
QDateTime QDate::startOfDay(Qt::TimeSpec spec, int offsetSeconds) const {
    if (!isValid())
        return {};
    QDateTime dt(*this, QTime(0, 0), spec, offsetSeconds);
    if (spec != Qt::LocalTime || isExistingLocalTime(dt))
        return dt;
    // A transition never skips more than a few hours; search the first existing minute.
    for (int minute = 1; minute < 24 * 60; ++minute) {
        const QDateTime candidate(*this, QTime(minute / 60, minute % 60), spec);
        if (isExistingLocalTime(candidate))
            return candidate;
    }
    return dt;
}

/*!
    \since 5.14
    Returns the last moment of this date, one millisecond before the next
    day starts, in the time specification \a spec, with \a offsetSeconds as
    the offset when \a spec is Qt::OffsetFromUTC. Returns an invalid
    QDateTime if the date is invalid.

    \sa startOfDay()
*/
QDateTime QDate::endOfDay(Qt::TimeSpec spec, int offsetSeconds) const {
    if (!isValid())
        return {};
    QDateTime dt(*this, QTime(23, 59, 59, 999), spec, offsetSeconds);
    if (spec != Qt::LocalTime || isExistingLocalTime(dt))
        return dt;
    // The day ends in a daylight-saving gap; take the last minute that exists.
    for (int minute = 24 * 60 - 1; minute > 0; --minute) {
        const QDateTime candidate(*this, QTime(minute / 60, minute % 60, 59, 999), spec);
        if (isExistingLocalTime(candidate))
            return candidate;
    }
    return dt;
}

/*!
    Returns the date as text in the given \a format, or an empty string if
    the date is invalid. TextDate gives \c{ddd MMM d yyyy}, ISODate and
    ISODateWithMs give \c{yyyy-MM-dd} (empty for years outside 0 to 9999),
    and RFC2822Date gives \c{dd MMM yyyy}. Names are English.

    \sa fromString()
*/
QString QDate::toString(Qt::DateFormat format) const {
    if (!isValid())
        return QString();
    switch (format) {
    case Qt::RFC2822Date:
        return toString(QString("dd MMM yyyy"));
    case Qt::ISODate:
    case Qt::ISODateWithMs:
        return year() >= 0 && year() <= 9999 ? toString(QString("yyyy-MM-dd")) : QString();
    default:
        return toString(QString("ddd MMM d yyyy"));
    }
}

/*!
    \overload

    Returns the date as text using \a format, or an empty string if the date
    is invalid. The expressions are \c d and \c dd (day as 1 to 31 or 01 to
    31), \c ddd and \c dddd (abbreviated and full English day name), \c M and
    \c MM (month as 1 to 12 or 01 to 12), \c MMM and \c MMMM (abbreviated and
    full English month name), \c yy (two-digit year) and \c yyyy (four-digit
    year, with a leading minus sign if negative). Text in single quotes is
    copied as is, and two single quotes give one quote. Other characters are
    copied unchanged.

    \sa fromString()
*/
QString QDate::toString(const QString &format) const { return isValid() ? formatTokens(format, *this, QTime()) : QString(); }

/*!
    Parses \a string as a date in the given \a format and returns it. Returns
    a null QDate if \a string does not match or is not a valid date. For
    ISODate and ISODateWithMs the text must start with \c{yyyy-MM-dd}; for
    RFC2822Date a leading weekday is ignored. TextDate expects
    \c{ddd MMM d yyyy}.

    \sa toString()
*/
QDate QDate::fromString(const QString &string, Qt::DateFormat format) {
    switch (format) {
    case Qt::ISODate:
    case Qt::ISODateWithMs:
        if (string.size() < 10 || (string.size() > 10 && QtPrivate::isAsciiDigit(string[10])))
            return QDate();
        return fromString(string.left(10), QString("yyyy-MM-dd"));
    case Qt::RFC2822Date: {
        QString s = string;
        const qsizetype comma = s.indexOf(QChar(u','));
        if (comma >= 0)
            s = s.mid(comma + 1);
        return fromString(s.trimmed(), QString("d MMM yyyy"));
    }
    default:
        return fromString(string, QString("ddd MMM d yyyy"));
    }
}

/*!
    \overload

    Parses \a string using \a format, which uses the same expressions as
    toString(const QString &) const, and returns the date. Returns a null
    QDate if \a string does not match \a format or is not a valid date. An
    absent year is taken as 1900, and a two-digit year is taken as 19xx.

    \sa toString()
*/
QDate QDate::fromString(const QString &string, const QString &format) {
    Parsed p;
    return parseTokens(string, format, &p) ? QDate(p.year, p.month, p.day) : QDate();
}

// QTime

/*!
    \class QTime
    \brief The QTime class provides clock time functions.

    A QTime represents a time of day, with millisecond resolution, from
    00:00:00.000 to 23:59:59.999. It stores no date, time zone or daylight
    saving information. A default-constructed QTime is null; a QTime built
    from out-of-range values is invalid.

    \code
    QTime t(13, 45, 30);
    t.addSecs(3600).toString("hh:mm:ss"); // "14:45:30"
    \endcode

    \differences
    The \c start(), \c restart() and \c elapsed() functions that Qt 5
    provided are not available; use QElapsedTimer instead. Text formats use
    English text and the format string supports the \c h, \c H, \c m, \c s,
    \c z, \c AP and \c ap tokens, and single-quoted literal text. fromString()
    with a Qt::DateFormat accepts only \c{HH:mm[:ss[.zzz]]}, whichever
    format is given.
    \enddifferences

    \sa QDate, QDateTime, QElapsedTimer
*/

/*!
    Constructs a time with hour \a h, minute \a m, seconds \a s and
    milliseconds \a ms. If the time is not valid, the QTime is invalid but not
    null.

    \sa isValid()
*/
QTime::QTime(int h, int m, int s, int msecs) { setHMS(h, m, s, msecs); }

/*!
    \overload

    Returns \c true if the specified time (\a h, \a m, \a s, \a ms) is valid;
    otherwise returns \c false. The hour must be 0 to 23, the minute and
    second 0 to 59, and the milliseconds 0 to 999.
*/
bool QTime::isValid(int h, int m, int s, int msecs) {
    return h >= 0 && h < 24 && m >= 0 && m < 60 && s >= 0 && s < 60 && msecs >= 0 && msecs < 1000;
}

/*!
    Sets the time to hour \a h, minute \a m, seconds \a s and milliseconds
    \a msecs. Returns \c true if the time is valid; otherwise makes the QTime
    invalid and returns \c false.

    \sa isValid()
*/
bool QTime::setHMS(int h, int m, int s, int msecs) {
    if (!isValid(h, m, s, msecs)) {
        ms = -2; // invalid, but not null
        return false;
    }
    ms = (h * 3600 + m * 60 + s) * 1000 + msecs;
    return true;
}

/*!
    Returns a QTime that is \a secs seconds after this time, or before it if
    \a secs is negative. The result wraps around midnight. Returns a null
    QTime if this time is invalid.

    \sa addMSecs(), secsTo()
*/
QTime QTime::addSecs(int secs) const { return addMSecs(static_cast<int>(floorMod(qint64(secs) * 1000, kMsPerDay))); }

/*!
    Returns a QTime that is \a msecs milliseconds after this time, or before
    it if \a msecs is negative. The result wraps around midnight. Returns a
    null QTime if this time is invalid.

    \sa addSecs(), msecsTo()
*/
QTime QTime::addMSecs(int msecs) const {
    if (!isValid())
        return QTime();
    return fromMSecsSinceStartOfDay(static_cast<int>(floorMod(qint64(ms) + msecs, kMsPerDay)));
}

/*!
    Returns the number of whole seconds from this time to \a t, which is
    negative if \a t is earlier. The result does not wrap around midnight.
    Returns 0 if either time is invalid.

    \sa addSecs(), msecsTo()
*/
int QTime::secsTo(QTime t) const { return isValid() && t.isValid() ? t.ms / 1000 - ms / 1000 : 0; }
/*!
    Returns the number of milliseconds from this time to \a t, which is
    negative if \a t is earlier. The result does not wrap around midnight.
    Returns 0 if either time is invalid.

    \sa addMSecs(), secsTo()
*/
int QTime::msecsTo(QTime t) const { return isValid() && t.isValid() ? t.ms - ms : 0; }

/*!
    Returns the time that is \a msecs milliseconds after midnight. Returns an
    invalid QTime if \a msecs is negative or not less than 86400000.

    \sa msecsSinceStartOfDay()
*/
QTime QTime::fromMSecsSinceStartOfDay(int msecs) {
    QTime t;
    t.ms = msecs >= 0 && msecs < kMsPerDay ? msecs : -2;
    return t;
}

/*!
    Returns the current time, as reported by the system clock in local time.

    \sa QDateTime::currentDateTime(), QDate::currentDate()
*/
QTime QTime::currentTime() { return QDateTime::currentDateTime().time(); }

/*!
    Returns the time as text in the given \a format, or an empty string if
    the time is invalid. ISODateWithMs gives \c{HH:mm:ss.zzz}; every other
    format gives \c{HH:mm:ss}.

    \sa fromString()
*/
QString QTime::toString(Qt::DateFormat format) const {
    if (!isValid())
        return QString();
    return toString(format == Qt::ISODateWithMs ? QString("HH:mm:ss.zzz") : QString("HH:mm:ss"));
}

/*!
    \overload

    Returns the time as text using \a format, or an empty string if the time
    is invalid. The expressions are \c h and \c hh (hour as 0 to 23, or 1 to
    12 if the format has an AM/PM expression), \c H and \c HH (hour as 0 to
    23), \c m and \c mm (minute), \c s and \c ss (second), \c z (milliseconds
    without trailing zeros), \c zzz (milliseconds as three digits), \c AP and
    \c A (upper-case AM or PM), \c ap and \c a (lower-case am or pm). Text in
    single quotes is copied as is.

    \sa fromString()
*/
QString QTime::toString(const QString &format) const { return isValid() ? formatTokens(format, QDate(), *this) : QString(); }

/*!
    Parses \a string as \c{HH:mm[:ss[.zzz]]} and returns the time. Returns an
    invalid QTime if \a string does not match. The \a format is ignored.

    \sa toString()
*/
QTime QTime::fromString(const QString &string, Qt::DateFormat format) {
    Q_UNUSED(format)
    QTime t;
    return parseIsoTime(string, 0, &t) == string.size() ? t : QTime();
}

/*!
    \overload

    Parses \a string using \a format, which uses the same expressions as
    toString(const QString &) const, and returns the time. Returns an invalid
    QTime if \a string does not match \a format.

    \sa toString()
*/
QTime QTime::fromString(const QString &string, const QString &format) {
    Parsed p;
    p.year = 1900;
    return parseTokens(string, format, &p) ? QTime(p.hour, p.minute, p.second, p.ms) : QTime();
}

// QDateTime

/*!
    \class QDateTime
    \brief The QDateTime class provides date and time functions.

    A QDateTime combines a QDate and a QTime with a time specification: local
    time, UTC, or a fixed offset from UTC. It converts to and from
    milliseconds since the Unix epoch, 1970-01-01T00:00:00 UTC.

    \code
    QDateTime now = QDateTime::currentDateTimeUtc();
    QDateTime later = now.addDays(7);
    later.toString(Qt::ISODate); // "2026-11-14T10:10:14Z"
    \endcode

    \differences
    Local time follows the system's rules through \c mktime(); zones given as
    a QTimeZone use Nt's reader of the time zone database. A time that falls
    in a daylight-saving gap is moved forward, and in a fold the earlier time
    is chosen. There is no \c fromTime_t() and no TransitionResolution
    argument. Text formats use English text and the format string accepts the
    QDate and QTime expressions plus \c t for the time zone.
    \enddifferences

    \sa QDate, QTime
*/

/*!
    Constructs a datetime from \a date and \a time, interpreted according to
    \a spec. For Qt::OffsetFromUTC, \a offsetSeconds is the offset from UTC in
    seconds; an offset of 0 makes the datetime UTC. If \a date is valid and
    \a time is null, the time is taken as midnight.

    \sa isValid()
*/
QDateTime::QDateTime(QDate date, QTime time, Qt::TimeSpec spec, int offsetSeconds)
    : m_date(date), m_time(date.isValid() && !time.isValid() && time.isNull() ? QTime(0, 0) : time), m_spec(spec) {
    if (spec == Qt::TimeZone)
        m_spec = Qt::LocalTime; // a zone is given with the QTimeZone constructor
    if (spec == Qt::OffsetFromUTC)
        m_offset = offsetSeconds;
    if (spec == Qt::OffsetFromUTC && offsetSeconds == 0)
        m_spec = Qt::UTC;
}

/*!
    Returns the datetime as milliseconds since the Unix epoch,
    1970-01-01T00:00:00 UTC. Returns 0 if the datetime is invalid. The value
    saturates at the limits of \c qint64.

    \sa toSecsSinceEpoch(), setMSecsSinceEpoch()
*/
qint64 QDateTime::toMSecsSinceEpoch() const {
    if (!isValid())
        return 0;
    qint64 fields;
    if (__builtin_mul_overflow(m_date.toJulianDay() - kEpochJd, kMsPerDay, &fields) ||
        __builtin_add_overflow(fields, qint64(m_time.msecsSinceStartOfDay()), &fields))
        fields = m_date.toJulianDay() < kEpochJd ? std::numeric_limits<qint64>::min() : std::numeric_limits<qint64>::max();
    if (m_spec == Qt::UTC)
        return fields;
    if (m_spec == Qt::TimeZone && m_zone) {
        const qint64 local = floorDiv(fields, 1000);
        if (m_offset != kNoOffsetHint && m_zone->offsetsAt(local - m_offset).offset == m_offset)
            return (local - m_offset) * 1000 + floorMod(fields, 1000);
        return m_zone->toUtc(local) * 1000 + floorMod(fields, 1000);
    }
    if (m_spec == Qt::OffsetFromUTC)
        return fields - qint64(m_offset) * 1000;
    struct tm t = {};
    t.tm_year = m_date.year() - 1900;
    t.tm_mon = m_date.month() - 1;
    t.tm_mday = m_date.day();
    t.tm_hour = m_time.hour();
    t.tm_min = m_time.minute();
    t.tm_sec = m_time.second();
    t.tm_isdst = -1;
    return qint64(mktime(&t)) * 1000 + m_time.msec();
}

/*!
    Returns the datetime as seconds since the Unix epoch, rounded down.
    Returns 0 if the datetime is invalid.

    \sa toMSecsSinceEpoch(), setSecsSinceEpoch()
*/
qint64 QDateTime::toSecsSinceEpoch() const { return floorDiv(toMSecsSinceEpoch(), 1000); }

/*!
    Returns the datetime's offset from UTC in seconds. It is 0 for UTC, the
    stored offset for Qt::OffsetFromUTC, and the system's offset at that moment
    for local time. Returns 0 for an invalid local datetime.

    \sa setOffsetFromUtc(), timeSpec()
*/
int QDateTime::offsetFromUtc() const {
    if (m_spec == Qt::UTC)
        return 0;
    if (m_spec == Qt::OffsetFromUTC)
        return m_offset;
    if (m_spec == Qt::TimeZone && m_zone)
        return isValid() ? m_zone->offsetsAt(toSecsSinceEpoch()).offset : 0;
    return isValid() ? systemOffsetAt(static_cast<time_t>(toSecsSinceEpoch())) : 0;
}

/*!
    Sets the time specification to a fixed offset of \a offsetSeconds from UTC,
    keeping the date and time fields as they are. An offset of 0 sets the
    specification to UTC.

    \sa offsetFromUtc(), toOffsetFromUtc()
*/
void QDateTime::setOffsetFromUtc(int offsetSeconds) {
    m_spec = offsetSeconds == 0 ? Qt::UTC : Qt::OffsetFromUTC;
    m_offset = offsetSeconds;
    m_zone.reset();
}

/*!
    Sets the datetime to \a msecs milliseconds since the Unix epoch, shown in
    the datetime's current time specification.

    \sa toMSecsSinceEpoch(), setSecsSinceEpoch()
*/
void QDateTime::setMSecsSinceEpoch(qint64 msecs) {
    qint64 local = msecs;
    if (m_spec == Qt::OffsetFromUTC)
        local += qint64(m_offset) * 1000;
    else if (m_spec == Qt::TimeZone && m_zone) {
        m_offset = m_zone->offsetsAt(floorDiv(msecs, 1000)).offset;
        local += qint64(m_offset) * 1000;
    } else if (m_spec == Qt::LocalTime)
        local += qint64(systemOffsetAt(static_cast<time_t>(floorDiv(msecs, 1000)))) * 1000;
    m_date = QDate::fromJulianDay(kEpochJd + floorDiv(local, kMsPerDay));
    m_time = QTime::fromMSecsSinceStartOfDay(static_cast<int>(floorMod(local, kMsPerDay)));
}

/*!
    Returns a QDateTime that is \a days days after this one, or before it if
    \a days is negative. The time and time specification are kept; the date
    changes as QDate::addDays() does.

    \sa addMonths(), addYears(), addSecs(), daysTo()
*/
QDateTime QDateTime::addDays(qint64 days) const { QDateTime r = *this; r.m_date = m_date.addDays(days); return r; }
/*!
    Returns a QDateTime that is \a months months after this one, or before it
    if \a months is negative. The day is clamped as QDate::addMonths() does.

    \sa addDays(), addYears()
*/
QDateTime QDateTime::addMonths(int months) const { QDateTime r = *this; r.m_date = m_date.addMonths(months); return r; }
/*!
    Returns a QDateTime that is \a years years after this one, or before it if
    \a years is negative. The day is clamped as QDate::addYears() does.

    \sa addDays(), addMonths()
*/
QDateTime QDateTime::addYears(int years) const { QDateTime r = *this; r.m_date = m_date.addYears(years); return r; }

/*!
    Returns a QDateTime that is \a msecs milliseconds after this one, or
    before it if \a msecs is negative, in the same time specification.
    Returns an invalid QDateTime if this one is invalid.

    \sa addSecs(), msecsTo()
*/
QDateTime QDateTime::addMSecs(qint64 msecs) const {
    if (!isValid())
        return QDateTime();
    QDateTime r = *this;
    r.setMSecsSinceEpoch(toMSecsSinceEpoch() + msecs);
    return r;
}

/*!
    Returns a copy of this datetime converted to the time specification
    \a spec, representing the same instant. Converting to Qt::OffsetFromUTC
    keeps the current offset.

    \sa toLocalTime(), toUTC(), toOffsetFromUtc()
*/
QDateTime QDateTime::toTimeSpec(Qt::TimeSpec spec) const {
    if (spec == Qt::OffsetFromUTC)
        return toOffsetFromUtc(m_offset);
    return fromMSecsSinceEpoch(toMSecsSinceEpoch(), spec);
}

/*!
    Returns a copy of this datetime converted to a fixed offset of
    \a offsetSeconds from UTC, representing the same instant.

    \sa toTimeSpec(), setOffsetFromUtc()
*/
QDateTime QDateTime::toOffsetFromUtc(int offsetSeconds) const {
    return fromMSecsSinceEpoch(toMSecsSinceEpoch(), Qt::OffsetFromUTC, offsetSeconds);
}

/*!
    Returns the datetime as text in the given \a format, or an empty string if
    the datetime is invalid. ISODate and ISODateWithMs give
    \c{yyyy-MM-ddTHH:mm:ss}, with milliseconds for ISODateWithMs, followed by
    \c Z for UTC or the offset such as \c{+10:00}. The result is empty for
    years outside 0 to 9999. RFC2822Date gives
    \c{dd MMM yyyy hh:mm:ss +hhmm}. TextDate gives
    \c{ddd MMM d HH:mm:ss yyyy}, followed by \c GMT and the offset for UTC and
    offset datetimes.

    \sa fromString()
*/
QString QDateTime::toString(Qt::DateFormat format) const {
    if (!isValid())
        return QString();
    switch (format) {
    case Qt::ISODate:
    case Qt::ISODateWithMs: {
        if (m_date.year() < 0 || m_date.year() > 9999)
            return QString();
        QString r = m_date.toString(Qt::ISODate) + QChar(u'T') + m_time.toString(format);
        if (m_spec == Qt::UTC)
            r.append(QChar(u'Z'));
        else if (m_spec == Qt::OffsetFromUTC)
            r.append(offsetText(m_offset));
        else if (m_spec == Qt::TimeZone)
            r.append(offsetText(offsetFromUtc()));
        return r;
    }
    case Qt::RFC2822Date:
        return toString(QString("dd MMM yyyy hh:mm:ss ")) + offsetText(offsetFromUtc(), false);
    default: {
        QString r = toString(QString("ddd MMM d HH:mm:ss yyyy"));
        if (m_spec == Qt::UTC)
            r.append(QString(" GMT"));
        else if (m_spec == Qt::OffsetFromUTC)
            r.append(QString(" GMT") + offsetText(m_offset, false));
        return r;
    }
    }
}

/*!
    \overload

    Returns the datetime as text using \a format, or an empty string if the
    datetime is invalid. It accepts the expressions of QDate and QTime, plus
    \c t, which gives \c UTC or \c UTC followed by the offset, such as
    \c{UTC+10:00}.

    \sa fromString()
*/
QString QDateTime::toString(const QString &format) const { return isValid() ? formatTokens(format, m_date, m_time, this) : QString(); }

/*!
    Returns the current time in milliseconds since the Unix epoch, as
    reported by the system clock.

    \sa currentSecsSinceEpoch(), currentDateTime()
*/
qint64 QDateTime::currentMSecsSinceEpoch() noexcept {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return qint64(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

/*!
    Returns the current time in seconds since the Unix epoch, as reported by
    the system clock.

    \sa currentMSecsSinceEpoch()
*/
qint64 QDateTime::currentSecsSinceEpoch() noexcept { return currentMSecsSinceEpoch() / 1000; }

/*!
    Returns the current datetime in local time.

    \sa currentDateTimeUtc(), QDate::currentDate(), QTime::currentTime()
*/
QDateTime QDateTime::currentDateTime() { return fromMSecsSinceEpoch(currentMSecsSinceEpoch()); }

/*!
    Returns the current datetime in UTC.

    \sa currentDateTime()
*/
QDateTime QDateTime::currentDateTimeUtc() { return fromMSecsSinceEpoch(currentMSecsSinceEpoch(), Qt::UTC); }

/*!
    Returns a datetime for \a msecs milliseconds since the Unix epoch, shown
    in the time specification \a spec. For Qt::OffsetFromUTC,
    \a offsetFromUtc is the offset in seconds.

    \sa toMSecsSinceEpoch(), fromSecsSinceEpoch()
*/
QDateTime QDateTime::fromMSecsSinceEpoch(qint64 msecs, Qt::TimeSpec spec, int offsetFromUtc) {
    QDateTime r;
    r.m_spec = spec;
    if (spec == Qt::OffsetFromUTC) {
        r.m_offset = offsetFromUtc;
        if (offsetFromUtc == 0)
            r.m_spec = Qt::UTC;
    }
    r.setMSecsSinceEpoch(msecs);
    return r;
}

/*!
    Returns a datetime for \a secs seconds since the Unix epoch, shown in the
    time specification \a spec. For Qt::OffsetFromUTC, \a offsetFromUtc is the
    offset in seconds.

    \sa toSecsSinceEpoch(), fromMSecsSinceEpoch()
*/
QDateTime QDateTime::fromSecsSinceEpoch(qint64 secs, Qt::TimeSpec spec, int offsetFromUtc) {
    return fromMSecsSinceEpoch(secs * 1000, spec, offsetFromUtc);
}

/*!
    Parses \a string in the given \a format and returns the datetime. Returns
    an invalid QDateTime if \a string does not match. ISO 8601 text may use a
    space instead of \c T, omit the time (which then means midnight), and end
    with \c Z or an offset; without either it is local time. RFC2822Date
    accepts an optional leading weekday, optional seconds, and a zone of
    \c GMT, \c UT, \c UTC, \c Z or \c{+hhmm}. TextDate expects
    \c{ddd MMM d HH:mm:ss yyyy}, optionally followed by \c GMT and an offset.

    \sa toString()
*/
QDateTime QDateTime::fromString(const QString &string, Qt::DateFormat format) {
    if (format == Qt::ISODate || format == Qt::ISODateWithMs) {
        const QDate date = QDate::fromString(string, Qt::ISODate);
        if (!date.isValid())
            return QDateTime();
        if (string.size() == 10)
            return QDateTime(date, QTime(0, 0));
        if (string.size() < 11 || (string[10] != QChar(u'T') && string[10] != QChar(u' ')))
            return QDateTime();
        QTime time;
        qsizetype pos = parseIsoTime(string, 11, &time);
        if (pos < 0)
            return QDateTime();
        if (pos == string.size())
            return QDateTime(date, time);
        if (string[pos] == QChar(u'Z') && pos + 1 == string.size())
            return QDateTime(date, time, Qt::UTC);
        const QChar sign = string[pos];
        if (sign != QChar(u'+') && sign != QChar(u'-'))
            return QDateTime();
        ++pos;
        int h, m = 0;
        if (!readInt(string, pos, 2, 2, &h))
            return QDateTime();
        if (pos < string.size() && string[pos] == QChar(u':'))
            ++pos;
        if (pos < string.size() && !readInt(string, pos, 2, 2, &m))
            return QDateTime();
        if (pos != string.size())
            return QDateTime();
        const int offset = (h * 3600 + m * 60) * (sign == QChar(u'-') ? -1 : 1);
        return QDateTime(date, time, Qt::OffsetFromUTC, offset);
    }
    if (format == Qt::RFC2822Date) {
        QString s = string.trimmed();
        const qsizetype comma = s.indexOf(QChar(u','));
        if (comma >= 0)
            s = s.mid(comma + 1).trimmed();
        const qsizetype sp = s.lastIndexOf(QChar(u' '));
        if (sp < 0)
            return QDateTime();
        QDateTime fields = fromString(s.left(sp), QString("d MMM yyyy H:mm:ss"));
        if (!fields.isValid())
            fields = fromString(s.left(sp), QString("d MMM yyyy H:mm"));
        const QString zone = s.mid(sp + 1);
        if (!fields.isValid() || zone.isEmpty())
            return QDateTime();
        if (zone == QString("GMT") || zone == QString("UT") || zone == QString("UTC") || zone == QString("Z"))
            return QDateTime(fields.date(), fields.time(), Qt::UTC);
        if (zone.size() != 5 || (zone[0] != QChar(u'+') && zone[0] != QChar(u'-')))
            return QDateTime();
        bool ok = false;
        const int v = zone.mid(1).toInt(&ok);
        if (!ok || v % 100 >= 60)
            return QDateTime();
        const int offset = (v / 100 * 3600 + v % 100 * 60) * (zone[0] == QChar(u'-') ? -1 : 1);
        return QDateTime(fields.date(), fields.time(), Qt::OffsetFromUTC, offset);
    }
    QString s = string.trimmed();
    Qt::TimeSpec spec = Qt::LocalTime;
    int offset = 0;
    const qsizetype gmt = s.indexOf(QString(" GMT"));
    if (gmt >= 0) {
        const QString zone = s.mid(gmt + 4);
        s = s.left(gmt);
        spec = Qt::UTC;
        if (zone.size() == 5) {
            const int v = zone.mid(1).toInt();
            offset = (v / 100 * 3600 + v % 100 * 60) * (zone[0] == QChar(u'-') ? -1 : 1);
            spec = Qt::OffsetFromUTC;
        }
    }
    const QDateTime fields = fromString(s, QString("ddd MMM d HH:mm:ss yyyy"));
    return fields.isValid() ? QDateTime(fields.date(), fields.time(), spec, offset) : QDateTime();
}

/*!
    \overload

    Parses \a string using \a format, which uses the expressions of QDate and
    QTime, and returns the datetime in local time. Returns an invalid
    QDateTime if \a string does not match \a format.

    \sa toString()
*/
QDateTime QDateTime::fromString(const QString &string, const QString &format) {
    Parsed p;
    if (!parseTokens(string, format, &p))
        return QDateTime();
    return QDateTime(QDate(p.year, p.month, p.day), QTime(p.hour, p.minute, p.second, p.ms));
}

/*!
    \since 5.2
    Constructs a datetime from \a date and \a time in \a timeZone. A zone for
    local time, UTC or a fixed offset gives the matching time specification;
    any other zone gives Qt::TimeZone. The datetime is invalid if the zone is
    invalid. A time that does not exist in the zone, because of a
    daylight-saving gap, is moved forward by the length of the gap.

    \sa timeZone(), toTimeZone()
*/
QDateTime::QDateTime(QDate date, QTime time, const QTimeZone &timeZone) : QDateTime(date, time) {
    setTimeZone(timeZone);
    if (!timeZone.isValid())
        m_date = QDate();
    else if (m_spec == Qt::TimeZone && isValid())
        setMSecsSinceEpoch(toMSecsSinceEpoch()); // moves a time in a gap forward
}

/*!
    \since 5.2
    Returns the time zone of the datetime: the zone given to it, or a zone for
    local time, UTC or the fixed offset of its time specification.

    \sa setTimeZone(), timeSpec()
*/
QTimeZone QDateTime::timeZone() const {
    switch (m_spec) {
    case Qt::UTC:
        return QTimeZone(QTimeZone::UTC);
    case Qt::OffsetFromUTC:
        return QTimeZone::fromSecondsAheadOfUtc(m_offset);
    case Qt::TimeZone:
        if (m_zone)
            return QTimeZone(m_zone);
        break;
    case Qt::LocalTime:
        break;
    }
    return QTimeZone(QTimeZone::LocalTime);
}

/*!
    \since 5.2
    Sets the time zone of the datetime to \a toZone, keeping the date and time
    fields, so the datetime then refers to a different instant.

    \sa timeZone(), toTimeZone()
*/
void QDateTime::setTimeZone(const QTimeZone &toZone) {
    m_offset = 0;
    m_zone.reset();
    switch (toZone.timeSpec()) {
    case Qt::UTC:
        m_spec = Qt::UTC;
        break;
    case Qt::OffsetFromUTC:
        m_spec = Qt::OffsetFromUTC;
        m_offset = toZone.fixedSecondsAheadOfUtc();
        break;
    case Qt::LocalTime:
        m_spec = Qt::LocalTime;
        break;
    case Qt::TimeZone:
        m_spec = toZone.isValid() ? Qt::TimeZone : Qt::LocalTime;
        m_zone = toZone.d;
        m_offset = kNoOffsetHint;
        break;
    }
}

/*!
    \since 5.2
    Returns a copy of this datetime converted to \a toZone, representing the
    same instant. Returns an invalid datetime if \a toZone is invalid.

    \sa setTimeZone(), toTimeSpec()
*/
QDateTime QDateTime::toTimeZone(const QTimeZone &toZone) const {
    if (!isValid() || !toZone.isValid())
        return QDateTime();
    return fromMSecsSinceEpoch(toMSecsSinceEpoch(), toZone);
}

/*!
    \since 5.2
    Returns \c true if daylight-saving time is in effect at this datetime;
    otherwise returns \c false. UTC and fixed offsets have no daylight time.
*/
bool QDateTime::isDaylightTime() const {
    if (!isValid())
        return false;
    if (m_spec == Qt::TimeZone && m_zone)
        return m_zone->offsetsAt(toSecsSinceEpoch()).dst;
    if (m_spec != Qt::LocalTime)
        return false;
    const time_t secs = static_cast<time_t>(toSecsSinceEpoch());
    struct tm t = {};
    localtime_r(&secs, &t);
    return t.tm_isdst > 0;
}

/*!
    \since 5.2
    Returns the abbreviation of the time zone at this datetime, such as
    \c{"CEST"}, \c{"UTC"} or \c{"UTC+05:30"} for a fixed offset.
*/
QString QDateTime::timeZoneAbbreviation() const {
    if (!isValid())
        return QString();
    switch (m_spec) {
    case Qt::UTC:
        return QString(u"UTC");
    case Qt::OffsetFromUTC:
        return QTimeZonePrivate::offsetName(m_offset);
    case Qt::TimeZone:
        if (m_zone) {
            const auto o = m_zone->offsetsAt(toSecsSinceEpoch());
            return o.abbreviation.empty() ? QTimeZonePrivate::offsetName(o.offset)
                                          : QString::fromStdString(o.abbreviation);
        }
        break;
    case Qt::LocalTime:
        break;
    }
    const time_t secs = static_cast<time_t>(toSecsSinceEpoch());
    struct tm t = {};
    localtime_r(&secs, &t);
#if defined(__GLIBC__) || defined(__APPLE__) || defined(__ANDROID__)
    if (t.tm_zone)
        return QString::fromLocal8Bit(t.tm_zone);
#endif
#ifdef __ZEPHYR__
    return QTimeZonePrivate::offsetName(0);
#else
    return QTimeZonePrivate::offsetName(static_cast<int>(t.tm_gmtoff));
#endif
}

/*!
    \since 5.2
    Returns the current date and time in \a zone.
*/
QDateTime QDateTime::currentDateTime(const QTimeZone &zone) {
    return fromMSecsSinceEpoch(currentMSecsSinceEpoch(), zone);
}

/*!
    \since 5.2
    Returns the datetime \a msecs milliseconds after the Unix epoch, shown in
    \a timeZone. Returns an invalid datetime if the zone is invalid.
*/
QDateTime QDateTime::fromMSecsSinceEpoch(qint64 msecs, const QTimeZone &timeZone) {
    if (!timeZone.isValid())
        return QDateTime();
    QDateTime r(QDate(1970, 1, 1), QTime(0, 0));
    r.setTimeZone(timeZone);
    r.setMSecsSinceEpoch(msecs);
    return r;
}

/*!
    \since 5.8
    Returns the datetime \a secs seconds after the Unix epoch, shown in
    \a timeZone.
*/
QDateTime QDateTime::fromSecsSinceEpoch(qint64 secs, const QTimeZone &timeZone) {
    return fromMSecsSinceEpoch(secs * 1000, timeZone);
}

/*!
    \since 5.14
    Returns the first moment of this date in \a zone. If a daylight-saving
    transition skips midnight, this is the first moment after the gap.
*/
QDateTime QDate::startOfDay(const QTimeZone &zone) const {
    if (!isValid() || !zone.isValid())
        return QDateTime();
    if (zone.timeSpec() == Qt::LocalTime)
        return startOfDay(Qt::LocalTime);
    QDateTime midnight(*this, QTime(0, 0), zone);
    if (midnight.date() == *this && midnight.time() == QTime(0, 0))
        return midnight;
    // Midnight is in a gap: the day starts at the transition.
    const QTimeZone::OffsetData t = zone.previousTransition(midnight.addSecs(1));
    return t.atUtc.isValid() ? t.atUtc.toTimeZone(zone) : midnight;
}

/*!
    \since 5.14
    Returns the last moment of this date in \a zone, one millisecond before
    the next day starts.
*/
QDateTime QDate::endOfDay(const QTimeZone &zone) const {
    if (!isValid() || !zone.isValid())
        return QDateTime();
    if (zone.timeSpec() == Qt::LocalTime)
        return endOfDay(Qt::LocalTime);
    return addDays(1).startOfDay(zone).addMSecs(-1);
}
