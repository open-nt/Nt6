// MIT License - see LICENSE
#pragma once

#include <QtCore/qanystringview.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qlocale.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

#include <limits>

/*!
    \class QCalendar
    \since 5.14
    \brief The QCalendar class describes calendar systems.

    QDate stores a day as a Julian day number, independent of any calendar.
    A QCalendar converts between that day and its year, month and day in a
    calendar system, and names the months:

    \code
    const QCalendar persian(QCalendar::System::Jalali);
    const QDate nowruz = persian.dateFromParts(1403, 1, 1); // 2024-03-20
    const QCalendar::YearMonthDay parts = persian.partsFromDate(QDate(2024, 6, 1));
    int day = QDate(2024, 6, 1).day(persian);                // 12
    \endcode

    As in Qt, the calendars have no year zero: year -1 (1 BCE) is directly
    followed by year 1.

    \differences
    Only the built-in systems exist; custom calendars cannot be registered and
    the \c SystemId API is not provided. The Jalali calendar uses the 33-year
    arithmetic leap rule and the Islamic civil calendar the tabular rule, as
    in Qt. Month names of the Jalali and Islamic calendars are in English.
    QDate::toString() and QLocale have no QCalendar overloads.
    \enddifferences

    \sa QDate, QLocale
*/
class QCalendar {
public:
    /*!
        A value meaning "not specified", for the optional year argument of
        some functions.
    */
    static constexpr int Unspecified = std::numeric_limits<int>::min();

    /*!
        \class QCalendar::YearMonthDay
        \brief The YearMonthDay class holds the year, month and day of a date in a calendar.

        \differences
        None.
        \enddifferences
    */
    struct YearMonthDay {
        /*!
            Constructs an invalid YearMonthDay.
        */
        YearMonthDay() = default;
        /*!
            Constructs a YearMonthDay for year \a y, month \a m and day \a d.
        */
        YearMonthDay(int y, int m = 1, int d = 1) noexcept : year(y), month(m), day(d) {}
        /*!
            Returns \c true if none of the fields is Unspecified; otherwise
            returns \c false. It does not check that the date exists.
        */
        bool isValid() const noexcept { return year != Unspecified && month != Unspecified && day != Unspecified; }
        /*!
            The year.
        */
        int year = Unspecified;
        /*!
            The month, from 1.
        */
        int month = Unspecified;
        /*!
            The day of the month, from 1.
        */
        int day = Unspecified;
    };

    /*!
        \enum QCalendar::System

        \value Gregorian The proleptic Gregorian calendar, which QDate uses by default.
        \value Julian The proleptic Julian calendar.
        \value Milankovic The Revised Julian calendar of Milutin Milanković.
        \value Jalali The Solar Hijri (Persian) calendar.
        \value IslamicCivil The tabular Islamic civil calendar.
        \omitvalue Last
        \omitvalue User
    */
    enum class System { Gregorian, Julian, Milankovic, Jalali, IslamicCivil, Last = IslamicCivil, User = -1 };

    QCalendar();
    explicit QCalendar(System system);
    explicit QCalendar(QAnyStringView name);

    bool isValid() const;
    int daysInMonth(int month, int year = Unspecified) const;
    int daysInYear(int year) const;
    int monthsInYear(int year) const;
    bool isDateValid(int year, int month, int day) const;
    bool isLeapYear(int year) const;
    bool isGregorian() const;
    bool isLunar() const;
    bool isLuniSolar() const;
    bool isSolar() const;
    bool isProleptic() const;
    bool hasYearZero() const;
    int maximumDaysInMonth() const;
    int minimumDaysInMonth() const;
    int maximumMonthsInYear() const;
    QString name() const;

    QDate dateFromParts(int year, int month, int day) const;
    QDate dateFromParts(const YearMonthDay &parts) const;
    YearMonthDay partsFromDate(QDate date) const;
    int dayOfWeek(QDate date) const;

    QString monthName(const QLocale &locale, int month, int year = Unspecified,
                      QLocale::FormatType format = QLocale::LongFormat) const;
    QString standaloneMonthName(const QLocale &locale, int month, int year = Unspecified,
                                QLocale::FormatType format = QLocale::LongFormat) const;
    QString weekDayName(const QLocale &locale, int day, QLocale::FormatType format = QLocale::LongFormat) const;
    QString standaloneWeekDayName(const QLocale &locale, int day,
                                  QLocale::FormatType format = QLocale::LongFormat) const;

    static QStringList availableCalendars();

private:
    System m_system = System::Gregorian;
};
