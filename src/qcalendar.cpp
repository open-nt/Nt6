// MIT License - see LICENSE
#include <QtCore/qcalendar.h>

#include <algorithm>

namespace {

using System = QCalendar::System;

qint64 floorDiv(qint64 a, qint64 b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0); }
qint64 floorMod(qint64 a, qint64 b) { return a - floorDiv(a, b) * b; }

// Calendars have no year zero; arithmetic uses astronomical years, where 0 is 1 BCE.
qint64 astronomical(int year) { return year < 0 ? qint64(year) + 1 : year; }
int fromAstronomical(qint64 a) { return static_cast<int>(a <= 0 ? a - 1 : a); }

bool jalaliLeap(qint64 a) { return floorMod(a * 8 + 29, 33) < 8; }

bool leapAstronomical(System s, qint64 a) {
    switch (s) {
    case System::Gregorian:
        return floorMod(a, 4) == 0 && (floorMod(a, 100) != 0 || floorMod(a, 400) == 0);
    case System::Julian:
        return floorMod(a, 4) == 0;
    case System::Milankovic: {
        const qint64 r = floorMod(a, 900);
        return floorMod(a, 4) == 0 && (floorMod(a, 100) != 0 || r == 200 || r == 600);
    }
    case System::Jalali:
        return jalaliLeap(a);
    case System::IslamicCivil:
        return floorMod(14 + 11 * a, 30) < 11;
    case System::User:
        break;
    }
    return false;
}

// The number of leap years from year 1 to year n (negative for n < 0, counting years n+1..0).
qint64 leapsUpTo(System s, qint64 n) {
    switch (s) {
    case System::Gregorian:
        return floorDiv(n, 4) - floorDiv(n, 100) + floorDiv(n, 400);
    case System::Julian:
        return floorDiv(n, 4);
    case System::Milankovic:
        return floorDiv(n, 4) - floorDiv(n, 100) + floorDiv(n - 200, 900) + 1 + floorDiv(n - 600, 900) + 1;
    case System::Jalali: {
        qint64 count = 8 * floorDiv(n, 33);
        const qint64 rest = floorMod(n, 33);
        for (qint64 k = 1; k <= rest; ++k)
            if (jalaliLeap(k))
                ++count;
        return count;
    }
    case System::IslamicCivil:
        return floorDiv(3 + 11 * (n + 1), 30);
    case System::User:
        break;
    }
    return 0;
}

int monthLength(System s, int month, bool leap) {
    switch (s) {
    case System::Jalali:
        return month <= 6 ? 31 : (month <= 11 ? 30 : (leap ? 30 : 29));
    case System::IslamicCivil:
        return month == 12 ? (leap ? 30 : 29) : (month % 2 == 1 ? 30 : 29);
    default: {
        static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        return month == 2 && leap ? 29 : days[month - 1];
    }
    }
}

int yearLength(System s) { return s == System::IslamicCivil ? 354 : 365; }

// The Julian day number of the day before the first day of astronomical year a.
qint64 yearBase(System s, qint64 a) {
    qint64 epoch = 0;
    switch (s) {
    case System::Gregorian:
    case System::Milankovic:
        epoch = 1721425; // 1 January 1 (Gregorian) is JD 1721426
        break;
    case System::Julian:
        epoch = 1721423; // 1 January 1 (Julian) is JD 1721424
        break;
    case System::Jalali:
        epoch = 1948319; // 1 Farvardin 1 AP is JD 1948320 (Nowruz 1403 = 2024-03-20)
        break;
    case System::IslamicCivil:
        epoch = 1948439; // 1 Muharram 1 AH is JD 1948440 (16 July 622 Julian)
        break;
    case System::User:
        break;
    }
    return epoch + yearLength(s) * (a - 1) + leapsUpTo(s, a - 1);
}

} // namespace

/*!
    Constructs a Gregorian calendar.
*/
QCalendar::QCalendar() = default;

/*!
    Constructs a calendar of the system \a system. The calendar is invalid
    for System::User.
*/
QCalendar::QCalendar(System system) : m_system(system) {}

/*!
    Constructs the calendar called \a name, ignoring case: \c{"Gregorian"},
    \c{"Julian"}, \c{"Milankovic"}, \c{"Jalali"} (or \c{"Persian"}) and
    \c{"Islamic Civil"} (or \c{"islamic-civil"}, \c{"islamicc"}). The calendar
    is invalid if the name is not known.

    \sa availableCalendars()
*/
QCalendar::QCalendar(QAnyStringView name) : m_system(System::User) {
    const QString n = name.toString().toLower();
    if (n == u"gregorian" || n == u"gregory")
        m_system = System::Gregorian;
    else if (n == u"julian")
        m_system = System::Julian;
    else if (n == u"milankovic")
        m_system = System::Milankovic;
    else if (n == u"jalali" || n == u"persian")
        m_system = System::Jalali;
    else if (n == u"islamic civil" || n == u"islamic-civil" || n == u"islamicc")
        m_system = System::IslamicCivil;
}

/*!
    Returns \c true if the calendar is one of the supported systems;
    otherwise returns \c false.
*/
bool QCalendar::isValid() const { return m_system != System::User; }

/*!
    Returns the number of days in \a month of \a year, or 0 if the month does
    not exist. If \a year is Unspecified, the largest length the month can
    have is returned.
*/
int QCalendar::daysInMonth(int month, int year) const {
    if (!isValid() || month < 1 || month > 12 || year == 0)
        return 0;
    if (year == Unspecified)
        return monthLength(m_system, month, true);
    return monthLength(m_system, month, isLeapYear(year));
}

/*!
    Returns the number of days in \a year, or 0 for an invalid year.
*/
int QCalendar::daysInYear(int year) const {
    if (!isValid() || year == 0 || year == Unspecified)
        return 0;
    return yearLength(m_system) + (isLeapYear(year) ? 1 : 0);
}

/*!
    Returns the number of months in \a year: 12 for every supported system,
    or 0 for an invalid year.
*/
int QCalendar::monthsInYear(int year) const { return isValid() && year != 0 && year != Unspecified ? 12 : 0; }

/*!
    Returns \c true if \a day of \a month of \a year exists in the calendar;
    otherwise returns \c false.
*/
bool QCalendar::isDateValid(int year, int month, int day) const {
    return isValid() && year != 0 && year != Unspecified && month >= 1 && month <= 12 && day >= 1 &&
           day <= daysInMonth(month, year);
}

/*!
    Returns \c true if \a year is a leap year of the calendar; otherwise
    returns \c false.
*/
bool QCalendar::isLeapYear(int year) const {
    if (!isValid() || year == 0 || year == Unspecified)
        return false;
    return leapAstronomical(m_system, astronomical(year));
}

/*!
    Returns \c true if the calendar is the Gregorian calendar; otherwise
    returns \c false.
*/
bool QCalendar::isGregorian() const { return m_system == System::Gregorian; }

/*!
    Returns \c true if the months of the calendar follow the moon, which is
    the case for the Islamic civil calendar; otherwise returns \c false.
*/
bool QCalendar::isLunar() const { return m_system == System::IslamicCivil; }

/*!
    Returns \c false; none of the supported calendars is lunisolar.
*/
bool QCalendar::isLuniSolar() const { return false; }

/*!
    Returns \c true if the years of the calendar follow the sun; otherwise
    returns \c false.
*/
bool QCalendar::isSolar() const { return isValid() && m_system != System::IslamicCivil; }

/*!
    Returns \c true if the calendar is extended to dates before it was
    introduced, as the Gregorian, Julian and Milanković calendars are;
    otherwise returns \c false.
*/
bool QCalendar::isProleptic() const {
    return m_system == System::Gregorian || m_system == System::Julian || m_system == System::Milankovic;
}

/*!
    Returns \c false; the supported calendars have no year zero.
*/
bool QCalendar::hasYearZero() const { return false; }

/*!
    Returns the largest number of days a month of the calendar can have.
*/
int QCalendar::maximumDaysInMonth() const { return m_system == System::IslamicCivil ? 30 : 31; }

/*!
    Returns the smallest number of days a month of the calendar can have.
*/
int QCalendar::minimumDaysInMonth() const {
    return m_system == System::IslamicCivil || m_system == System::Jalali ? 29 : 28;
}

/*!
    Returns the largest number of months a year of the calendar can have, 12.
*/
int QCalendar::maximumMonthsInYear() const { return 12; }

/*!
    Returns the name of the calendar, such as \c{"Gregorian"}, or an empty
    string if it is invalid.
*/
QString QCalendar::name() const {
    switch (m_system) {
    case System::Gregorian:
        return QString(u"Gregorian");
    case System::Julian:
        return QString(u"Julian");
    case System::Milankovic:
        return QString(u"Milankovic");
    case System::Jalali:
        return QString(u"Jalali");
    case System::IslamicCivil:
        return QString(u"Islamic Civil");
    case System::User:
        break;
    }
    return QString();
}

/*!
    Returns the date of \a day of \a month of \a year in the calendar, or an
    invalid date if it does not exist.

    \sa partsFromDate()
*/
QDate QCalendar::dateFromParts(int year, int month, int day) const {
    if (!isDateValid(year, month, day))
        return QDate();
    const qint64 a = astronomical(year);
    const bool leap = leapAstronomical(m_system, a);
    qint64 jd = yearBase(m_system, a);
    for (int m = 1; m < month; ++m)
        jd += monthLength(m_system, m, leap);
    return QDate::fromJulianDay(jd + day);
}

/*!
    \overload
    Returns the date of \a parts in the calendar.
*/
QDate QCalendar::dateFromParts(const YearMonthDay &parts) const {
    return dateFromParts(parts.year, parts.month, parts.day);
}

/*!
    Returns the year, month and day of \a date in the calendar, or an invalid
    YearMonthDay if the date or the calendar is invalid.

    \sa dateFromParts()
*/
QCalendar::YearMonthDay QCalendar::partsFromDate(QDate date) const {
    if (!isValid() || !date.isValid())
        return YearMonthDay();
    const qint64 jd = date.toJulianDay();
    // Estimate the year from the mean year length, then correct it.
    const double mean = m_system == System::IslamicCivil ? 354.36667 : 365.2425;
    qint64 a = static_cast<qint64>(static_cast<double>(jd - yearBase(m_system, 1)) / mean) + 1;
    while (yearBase(m_system, a) >= jd)
        --a;
    while (yearBase(m_system, a + 1) < jd)
        ++a;
    int dayOfYear = static_cast<int>(jd - yearBase(m_system, a));
    const bool leap = leapAstronomical(m_system, a);
    int month = 1;
    while (month < 12 && dayOfYear > monthLength(m_system, month, leap)) {
        dayOfYear -= monthLength(m_system, month, leap);
        ++month;
    }
    return YearMonthDay(fromAstronomical(a), month, dayOfYear);
}

/*!
    Returns the day of the week of \a date, from 1 (Monday) to 7 (Sunday),
    or 0 for an invalid date. The week is the same in every supported
    calendar.
*/
int QCalendar::dayOfWeek(QDate date) const { return isValid() && date.isValid() ? date.dayOfWeek() : 0; }

namespace {
const char *const jalaliMonths[] = {"Farvardin", "Ordibehesht", "Khordad", "Tir", "Mordad", "Shahrivar",
                                    "Mehr",      "Aban",        "Azar",    "Dey", "Bahman", "Esfand"};
const char *const islamicMonths[] = {"Muharram",
                                     "Safar",
                                     "Rabi\xca\xbb I",
                                     "Rabi\xca\xbb II",
                                     "Jumada I",
                                     "Jumada II",
                                     "Rajab",
                                     "Sha\u02bbban",
                                     "Ramadan",
                                     "Shawwal",
                                     "Dhu\u02bbl-Qi\u02bbdah",
                                     "Dhu\xca\xbbl-Hijjah"};
} // namespace

/*!
    Returns the name of \a month in the form \a format. The Gregorian, Julian
    and Milanković calendars use the month names of \a locale; the Jalali and
    Islamic calendars use English names. \a year is accepted for
    compatibility.
*/
QString QCalendar::monthName(const QLocale &locale, int month, int year, QLocale::FormatType format) const {
    Q_UNUSED(year)
    if (!isValid() || month < 1 || month > 12)
        return QString();
    if (m_system != System::Jalali && m_system != System::IslamicCivil)
        return locale.monthName(month, format);
    QString name =
        QString::fromUtf8(m_system == System::Jalali ? jalaliMonths[month - 1] : islamicMonths[month - 1]);
    if (format == QLocale::NarrowFormat)
        return name.left(1);
    if (format == QLocale::ShortFormat)
        return name.left(3);
    return name;
}

/*!
    Returns the name of \a month in the form \a format, as used on its own.
    See monthName() for \a locale and \a year.
*/
QString QCalendar::standaloneMonthName(const QLocale &locale, int month, int year, QLocale::FormatType format) const {
    if (m_system != System::Jalali && m_system != System::IslamicCivil && isValid())
        return month >= 1 && month <= 12 ? locale.standaloneMonthName(month, format) : QString();
    return monthName(locale, month, year, format);
}

/*!
    Returns the name of \a day (1 is Monday) in the form \a format in
    \a locale.
*/
QString QCalendar::weekDayName(const QLocale &locale, int day, QLocale::FormatType format) const {
    return isValid() ? locale.dayName(day, format) : QString();
}

/*!
    Returns the name of \a day (1 is Monday) in the form \a format in
    \a locale, as used on its own.
*/
QString QCalendar::standaloneWeekDayName(const QLocale &locale, int day, QLocale::FormatType format) const {
    return isValid() ? locale.standaloneDayName(day, format) : QString();
}

/*!
    Returns the names of the supported calendars.
*/
QStringList QCalendar::availableCalendars() {
    return {QString(u"Gregorian"), QString(u"Julian"), QString(u"Milankovic"), QString(u"Jalali"),
            QString(u"Islamic Civil")};
}

// ------------------------------------------------------------------ QDate overloads

/*!
    \overload
    \since 5.14
    Constructs the date of day \a d of month \a m of year \a y in the
    calendar \a cal. The date is null if it does not exist.
*/
QDate::QDate(int y, int m, int d, QCalendar cal) { *this = cal.dateFromParts(y, m, d); }

/*!
    \overload
    \since 5.14
    Returns the year of the date in the calendar \a cal, or 0 if it is
    invalid.
*/
int QDate::year(QCalendar cal) const {
    const QCalendar::YearMonthDay p = cal.partsFromDate(*this);
    return p.isValid() ? p.year : 0;
}

/*!
    \overload
    \since 5.14
    Returns the month of the date in the calendar \a cal, or 0.
*/
int QDate::month(QCalendar cal) const {
    const QCalendar::YearMonthDay p = cal.partsFromDate(*this);
    return p.isValid() ? p.month : 0;
}

/*!
    \overload
    \since 5.14
    Returns the day of the month of the date in the calendar \a cal, or 0.
*/
int QDate::day(QCalendar cal) const {
    const QCalendar::YearMonthDay p = cal.partsFromDate(*this);
    return p.isValid() ? p.day : 0;
}

/*!
    \overload
    \since 5.14
    Returns the day of the year of the date in the calendar \a cal, from 1,
    or 0.
*/
int QDate::dayOfYear(QCalendar cal) const {
    const QCalendar::YearMonthDay p = cal.partsFromDate(*this);
    if (!p.isValid())
        return 0;
    return static_cast<int>(toJulianDay() - cal.dateFromParts(p.year, 1, 1).toJulianDay()) + 1;
}

/*!
    \overload
    \since 5.14
    Returns the number of days in the month of the date in the calendar
    \a cal, or 0.
*/
int QDate::daysInMonth(QCalendar cal) const {
    const QCalendar::YearMonthDay p = cal.partsFromDate(*this);
    return p.isValid() ? cal.daysInMonth(p.month, p.year) : 0;
}

/*!
    \overload
    \since 5.14
    Returns the number of days in the year of the date in the calendar
    \a cal, or 0.
*/
int QDate::daysInYear(QCalendar cal) const {
    const QCalendar::YearMonthDay p = cal.partsFromDate(*this);
    return p.isValid() ? cal.daysInYear(p.year) : 0;
}

/*!
    \overload
    \since 5.14
    Sets the date to day \a day of month \a month of year \a year in the
    calendar \a cal and returns \c true, or makes it null and returns
    \c false if that date does not exist.
*/
bool QDate::setDate(int year, int month, int day, QCalendar cal) {
    *this = cal.dateFromParts(year, month, day);
    return isValid();
}

/*!
    \overload
    \since 5.14
    Returns the date \a months months later (earlier if negative) in the
    calendar \a cal. If the day does not exist in the new month, the last day
    of that month is used.
*/
QDate QDate::addMonths(int months, QCalendar cal) const {
    const QCalendar::YearMonthDay p = cal.partsFromDate(*this);
    if (!p.isValid())
        return QDate();
    const qint64 index = astronomical(p.year) * 12 + (p.month - 1) + months;
    const int year = fromAstronomical(floorDiv(index, 12));
    const int month = static_cast<int>(floorMod(index, 12)) + 1;
    return cal.dateFromParts(year, month, std::min(p.day, cal.daysInMonth(month, year)));
}

/*!
    \overload
    \since 5.14
    Returns the date \a years years later (earlier if negative) in the
    calendar \a cal, skipping the nonexistent year zero. If the day does not
    exist in the new year, the last day of the month is used.
*/
QDate QDate::addYears(int years, QCalendar cal) const {
    const QCalendar::YearMonthDay p = cal.partsFromDate(*this);
    if (!p.isValid())
        return QDate();
    const int year = fromAstronomical(astronomical(p.year) + years);
    return cal.dateFromParts(year, p.month, std::min(p.day, cal.daysInMonth(p.month, year)));
}
