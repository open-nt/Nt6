// MIT License - see LICENSE
#include <QtCore>
#include <QSet>

#include <cmath>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <list>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }
static std::string hx(const QByteArray &b) { return std::string(b.toHex().constData()); }

// QDate

static void testDateValidity() {
    CHECK(QDate().isNull());
    CHECK(!QDate().isValid());
    CHECK(QDate(2024, 2, 29).isValid());
    CHECK(!QDate(2023, 2, 29).isValid());
    CHECK(!QDate(1900, 2, 29).isValid());
    CHECK(QDate(2000, 2, 29).isValid());
    CHECK(!QDate(2000, 13, 1).isValid());
    CHECK(!QDate(2000, 0, 1).isValid());
    CHECK(!QDate(2000, 4, 31).isValid());
    CHECK(!QDate(2000, 1, 0).isValid());
    CHECK(!QDate(0, 1, 1).isValid()); // there is no year 0
    CHECK(QDate(-1, 1, 1).isValid());
    CHECK(QDate::isValid(2024, 12, 31));
    CHECK(!QDate::isValid(2024, 12, 32));
    CHECK(QDate::isLeapYear(2024));
    CHECK(!QDate::isLeapYear(2023));
    CHECK(!QDate::isLeapYear(1900));
    CHECK(QDate::isLeapYear(2000));
    CHECK(QDate::isLeapYear(-1)); // 1 BC is astronomical year 0
    CHECK(QDate::isLeapYear(-5));
    CHECK(!QDate::isLeapYear(-4));
    QDate d;
    CHECK(d.setDate(2020, 5, 17));
    CHECK(d.year() == 2020 && d.month() == 5 && d.day() == 17);
    CHECK(!d.setDate(2020, 5, 32));
    CHECK(d.isNull());
    CHECK(d.year() == 0 && d.month() == 0 && d.day() == 0);
    int y = 7, m = 7, dd = 7;
    QDate(1999, 12, 31).getDate(&y, &m, &dd);
    CHECK(y == 1999 && m == 12 && dd == 31);
}

static void testDateJulianAndWeekday() {
    CHECK(QDate(1970, 1, 1).toJulianDay() == 2440588);
    CHECK(QDate(2000, 1, 1).toJulianDay() == 2451545);
    CHECK(QDate(1900, 1, 1).toJulianDay() == 2415021);
    CHECK(QDate(2100, 3, 1).toJulianDay() == 2488129);
    CHECK(QDate::fromJulianDay(2440588) == QDate(1970, 1, 1));
    CHECK(QDate::fromJulianDay(2451545).year() == 2000);
    for (qint64 jd = 1721426 - 800; jd < 1721426 + 800; ++jd) { // across year -1/1
        const QDate d = QDate::fromJulianDay(jd);
        CHECK(QDate(d.year(), d.month(), d.day()).toJulianDay() == jd);
        CHECK(d.year() != 0);
    }
    for (qint64 jd = 2400000; jd < 2500000; jd += 37) {
        const QDate d = QDate::fromJulianDay(jd);
        CHECK(QDate(d.year(), d.month(), d.day()) == d);
    }
    CHECK(QDate(1970, 1, 1).dayOfWeek() == 4); // Thursday
    CHECK(QDate(2000, 1, 1).dayOfWeek() == 6);
    CHECK(QDate(2024, 2, 29).dayOfWeek() == 4);
    CHECK(QDate(2024, 3, 3).dayOfWeek() == 7);
    CHECK(QDate(2024, 3, 4).dayOfWeek() == 1);
    CHECK(QDate(1969, 12, 31).dayOfWeek() == 3);
    CHECK(QDate().dayOfWeek() == 0);
    CHECK(QDate(2024, 1, 1).dayOfYear() == 1);
    CHECK(QDate(2024, 12, 31).dayOfYear() == 366);
    CHECK(QDate(2023, 12, 31).dayOfYear() == 365);
    CHECK(QDate(2024, 3, 1).dayOfYear() == 61);
    CHECK(QDate(2023, 3, 1).dayOfYear() == 60);
    CHECK(QDate(2024, 2, 1).daysInMonth() == 29);
    CHECK(QDate(2023, 2, 1).daysInMonth() == 28);
    CHECK(QDate(2023, 4, 10).daysInMonth() == 30);
    CHECK(QDate(2023, 12, 10).daysInMonth() == 31);
    CHECK(QDate(2024, 6, 1).daysInYear() == 366);
    CHECK(QDate(2023, 6, 1).daysInYear() == 365);
    CHECK(QDate().daysInMonth() == 0 && QDate().daysInYear() == 0);
}

static void testDateWeekNumber() {
    struct W { int y, m, d, week, year; };
    const W cases[] = {{2021, 1, 3, 53, 2020}, {2021, 1, 4, 1, 2021},   {2024, 12, 30, 1, 2025}, {2016, 1, 1, 53, 2015},
                       {2020, 12, 31, 53, 2020}, {2019, 12, 30, 1, 2020}, {2018, 12, 31, 1, 2019}, {2010, 1, 3, 53, 2009},
                       {2024, 2, 29, 9, 2024},   {2024, 1, 1, 1, 2024},   {2023, 1, 1, 52, 2022},  {2015, 12, 31, 53, 2015}};
    for (const W &c : cases) {
        int yn = 0;
        const int w = QDate(c.y, c.m, c.d).weekNumber(&yn);
        CHECK(w == c.week);
        CHECK(yn == c.year);
    }
    CHECK(QDate(2024, 2, 29).weekNumber() == 9);
    CHECK(QDate().weekNumber() == 0);
}

static void testDateArithmetic() {
    const QDate d(2024, 2, 29);
    CHECK(d.addDays(1) == QDate(2024, 3, 1));
    CHECK(d.addDays(-29) == QDate(2024, 1, 31));
    CHECK(d.addDays(366) == QDate(2025, 3, 1));
    CHECK(d.addDays(0) == d);
    CHECK(QDate().addDays(5).isNull());
    CHECK(QDate(1, 1, 1).addDays(-1) == QDate(-1, 12, 31));
    CHECK(QDate(-1, 12, 31).addDays(1) == QDate(1, 1, 1));

    CHECK(QDate(2024, 1, 31).addMonths(1) == QDate(2024, 2, 29));
    CHECK(QDate(2023, 1, 31).addMonths(1) == QDate(2023, 2, 28));
    CHECK(QDate(2024, 3, 31).addMonths(-1) == QDate(2024, 2, 29));
    CHECK(QDate(2024, 1, 31).addMonths(13) == QDate(2025, 2, 28));
    CHECK(QDate(2024, 12, 15).addMonths(1) == QDate(2025, 1, 15));
    CHECK(QDate(2024, 1, 15).addMonths(-1) == QDate(2023, 12, 15));
    CHECK(QDate(2024, 1, 15).addMonths(-13) == QDate(2022, 12, 15));
    CHECK(QDate(2024, 1, 15).addMonths(0) == QDate(2024, 1, 15));
    CHECK(QDate(2024, 5, 31).addMonths(120) == QDate(2034, 5, 31));
    CHECK(QDate(1, 1, 15).addMonths(-1) == QDate(-1, 12, 15));
    CHECK(QDate(-1, 12, 15).addMonths(1) == QDate(1, 1, 15));
    CHECK(QDate().addMonths(1).isNull());

    CHECK(QDate(2024, 2, 29).addYears(1) == QDate(2025, 2, 28));
    CHECK(QDate(2024, 2, 29).addYears(4) == QDate(2028, 2, 29));
    CHECK(QDate(2024, 2, 29).addYears(-1) == QDate(2023, 2, 28));
    CHECK(QDate(2024, 6, 1).addYears(0) == QDate(2024, 6, 1));
    CHECK(QDate(1, 6, 1).addYears(-1) == QDate(-1, 6, 1));
    CHECK(QDate(-1, 6, 1).addYears(1) == QDate(1, 6, 1));
    CHECK(QDate().addYears(1).isNull());

    CHECK(QDate(2024, 1, 1).daysTo(QDate(2024, 12, 31)) == 365);
    CHECK(QDate(2024, 12, 31).daysTo(QDate(2024, 1, 1)) == -365);
    CHECK(QDate(2024, 1, 1).daysTo(QDate(2024, 1, 1)) == 0);
    CHECK(QDate(2024, 1, 1).daysTo(QDate()) == 0);
    CHECK(QDate().daysTo(QDate(2024, 1, 1)) == 0);
    CHECK(QDate(1970, 1, 1).daysTo(QDate(2000, 1, 1)) == 10957);

    CHECK(QDate(2024, 1, 1) < QDate(2024, 1, 2));
    CHECK(QDate(2024, 1, 2) > QDate(2024, 1, 1));
    CHECK(QDate(2024, 1, 1) <= QDate(2024, 1, 1));
    CHECK(QDate(2024, 1, 1) >= QDate(2024, 1, 1));
    CHECK(QDate(2024, 1, 1) != QDate(2024, 1, 2));
    CHECK(QDate(-5, 1, 1) < QDate(5, 1, 1));
    CHECK(QDate() == QDate());
}

static void testDateText() {
    const QDate d(2024, 2, 29);
    CHECK(d.toString(Qt::ISODate) == "2024-02-29");
    CHECK(d.toString(Qt::TextDate) == "Thu Feb 29 2024");
    CHECK(d.toString() == "Thu Feb 29 2024");
    CHECK(d.toString(Qt::RFC2822Date) == "29 Feb 2024");
    CHECK(d.toString(QString("dddd, MMMM d, yyyy")) == "Thursday, February 29, 2024");
    CHECK(d.toString(QString("ddd MMM d")) == "Thu Feb 29");
    CHECK(d.toString(QString("dd/MM/yy")) == "29/02/24");
    CHECK(d.toString(QString("d/M/yyyy")) == "29/2/2024");
    CHECK(d.toString(QString("yyyy-MM-dd")) == "2024-02-29");
    CHECK(d.toString(QString("'Day' d 'of' MMMM")) == "Day 29 of February");
    CHECK(d.toString(QString("d 'o''clock'")) == "29 o'clock");
    CHECK(QDate(2024, 7, 4).toString(QString("MMM")) == "Jul");
    CHECK(QDate(5, 1, 2).toString(QString("yyyy-MM-dd")) == "0005-01-02");
    CHECK(QDate(-5, 1, 2).toString(QString("yyyy")) == "-0005");
    CHECK(QDate().toString(Qt::ISODate).isEmpty());
    CHECK(QDate().toString(QString("yyyy")).isEmpty());
    CHECK(QDate(12345, 1, 1).toString(Qt::ISODate).isEmpty());

    CHECK(QDate::fromString(QString("2024-02-29"), Qt::ISODate) == d);
    CHECK(!QDate::fromString(QString("2023-02-29"), Qt::ISODate).isValid());
    CHECK(!QDate::fromString(QString("2024-2-29"), Qt::ISODate).isValid());
    CHECK(!QDate::fromString(QString("garbage"), Qt::ISODate).isValid());
    CHECK(!QDate::fromString(QString(""), Qt::ISODate).isValid());
    CHECK(!QDate::fromString(QString("2024-02-291"), Qt::ISODate).isValid());
    CHECK(QDate::fromString(QString("Thu Feb 29 2024")) == d);
    CHECK(QDate::fromString(QString("29 Feb 2024"), Qt::RFC2822Date) == d);
    CHECK(QDate::fromString(QString("5/3/2021"), QString("d/M/yyyy")) == QDate(2021, 3, 5));
    CHECK(QDate::fromString(QString("05.03.2021"), QString("dd.MM.yyyy")) == QDate(2021, 3, 5));
    CHECK(QDate::fromString(QString("Mar 5 2021"), QString("MMM d yyyy")) == QDate(2021, 3, 5));
    CHECK(QDate::fromString(QString("March 5, 2021"), QString("MMMM d, yyyy")) == QDate(2021, 3, 5));
    CHECK(QDate::fromString(QString("Friday, 5 March 2021"), QString("dddd, d MMMM yyyy")) == QDate(2021, 3, 5));
    CHECK(QDate::fromString(QString("21-03-05"), QString("yy-MM-dd")) == QDate(1921, 3, 5));
    CHECK(!QDate::fromString(QString("32/1/2021"), QString("d/M/yyyy")).isValid());
    CHECK(!QDate::fromString(QString("5/3/2021 x"), QString("d/M/yyyy")).isValid());
    CHECK(!QDate::fromString(QString("Foo 5 2021"), QString("MMM d yyyy")).isValid());
    CHECK(QDate::fromString(QString("2024 leap"), QString("yyyy 'leap'")) == QDate(2024, 1, 1));

    for (qint64 jd = 2450000; jd < 2460000; jd += 13) {
        const QDate x = QDate::fromJulianDay(jd);
        CHECK(QDate::fromString(x.toString(Qt::ISODate), Qt::ISODate) == x);
        CHECK(QDate::fromString(x.toString(Qt::TextDate), Qt::TextDate) == x);
        CHECK(QDate::fromString(x.toString(QString("d MMMM yyyy")), QString("d MMMM yyyy")) == x);
    }
    CHECK(QDate::currentDate().isValid());
    CHECK(QDate::currentDate().year() >= 2024);
}

// QTime

static void testTime() {
    CHECK(QTime().isNull());
    CHECK(!QTime().isValid());
    CHECK(QTime(0, 0).isValid());
    CHECK(!QTime(0, 0).isNull());
    CHECK(QTime(23, 59, 59, 999).isValid());
    CHECK(!QTime(24, 0).isValid());
    CHECK(!QTime(0, 60).isValid());
    CHECK(!QTime(0, 0, 60).isValid());
    CHECK(!QTime(0, 0, 0, 1000).isValid());
    CHECK(!QTime(-1, 0).isValid());
    CHECK(QTime::isValid(12, 30, 30, 500));
    CHECK(!QTime::isValid(12, 30, 30, 1000));
    const QTime t(13, 14, 15, 678);
    CHECK(t.hour() == 13 && t.minute() == 14 && t.second() == 15 && t.msec() == 678);
    CHECK(QTime().hour() == -1 && QTime().msec() == -1);
    CHECK(t.msecsSinceStartOfDay() == ((13 * 60 + 14) * 60 + 15) * 1000 + 678);
    CHECK(QTime().msecsSinceStartOfDay() == 0);
    CHECK(QTime(25, 0).msecsSinceStartOfDay() == 0);
    CHECK(QTime::fromMSecsSinceStartOfDay(t.msecsSinceStartOfDay()) == t);
    CHECK(!QTime::fromMSecsSinceStartOfDay(86400000).isValid());
    CHECK(!QTime::fromMSecsSinceStartOfDay(-1).isValid());
    CHECK(QTime::fromMSecsSinceStartOfDay(86399999) == QTime(23, 59, 59, 999));
    QTime s;
    CHECK(s.setHMS(1, 2, 3, 4));
    CHECK(s == QTime(1, 2, 3, 4));
    CHECK(!s.setHMS(1, 2, 3, 4000));
    CHECK(!s.isValid());

    CHECK(QTime(0, 0).addSecs(-1) == QTime(23, 59, 59));
    CHECK(QTime(23, 59, 59).addSecs(1) == QTime(0, 0));
    CHECK(QTime(12, 0).addSecs(86400 * 3 + 5) == QTime(12, 0, 5));
    CHECK(QTime(12, 0).addSecs(-86400 * 3 - 5) == QTime(11, 59, 55));
    CHECK(QTime(0, 0).addMSecs(-1) == QTime(23, 59, 59, 999));
    CHECK(QTime(23, 59, 59, 999).addMSecs(1) == QTime(0, 0));
    CHECK(QTime(10, 0).addMSecs(1500) == QTime(10, 0, 1, 500));
    CHECK(QTime(10, 0).addSecs(0) == QTime(10, 0));
    CHECK(!QTime().addSecs(5).isValid());
    CHECK(QTime(0, 0).addSecs(std::numeric_limits<int>::min()).isValid());
    CHECK(QTime(10, 0).secsTo(QTime(10, 1)) == 60);
    CHECK(QTime(10, 1).secsTo(QTime(10, 0)) == -60);
    CHECK(QTime(10, 0).msecsTo(QTime(10, 0, 1, 250)) == 1250);
    CHECK(QTime(10, 0, 0, 900).secsTo(QTime(10, 0, 1, 100)) == 1);
    CHECK(QTime(10, 0).secsTo(QTime()) == 0);
    CHECK(QTime(1, 0) < QTime(2, 0));
    CHECK(QTime(2, 0) > QTime(1, 0));
    CHECK(QTime(2, 0) >= QTime(2, 0));
    CHECK(QTime(2, 0) <= QTime(2, 0));
    CHECK(QTime(2, 0) != QTime(2, 0, 0, 1));

    const QTime pm(15, 4, 5, 7);
    CHECK(pm.toString(Qt::ISODate) == "15:04:05");
    CHECK(pm.toString(Qt::ISODateWithMs) == "15:04:05.007");
    CHECK(pm.toString() == "15:04:05");
    CHECK(pm.toString(QString("HH:mm:ss.zzz")) == "15:04:05.007");
    CHECK(pm.toString(QString("hh:mm:ss ap")) == "03:04:05 pm");
    CHECK(pm.toString(QString("h:mm AP")) == "3:04 PM");
    CHECK(pm.toString(QString("h:mm A")) == "3:04 PM");
    CHECK(pm.toString(QString("h:mm a")) == "3:04 pm");
    CHECK(QTime(0, 5).toString(QString("h:mm AP")) == "12:05 AM");
    CHECK(QTime(12, 5).toString(QString("h:mm AP")) == "12:05 PM");
    CHECK(QTime(0, 5).toString(QString("H:mm")) == "0:05");
    CHECK(pm.toString(QString("H:m:s")) == "15:4:5");
    CHECK(pm.toString(QString("s.z")) == "5.007");
    CHECK(QTime(0, 0, 5, 70).toString(QString("s.z")) == "5.07");
    CHECK(QTime(0, 0, 5, 500).toString(QString("s.z")) == "5.5");
    CHECK(QTime(0, 0, 5, 0).toString(QString("s.z")) == "5.0");
    CHECK(pm.toString(QString("HH 'h' mm")) == "15 h 04");
    CHECK(QTime().toString(Qt::ISODate).isEmpty());

    CHECK(QTime::fromString(QString("15:04:05"), Qt::ISODate) == QTime(15, 4, 5));
    CHECK(QTime::fromString(QString("15:04:05.007"), Qt::ISODate) == pm);
    CHECK(QTime::fromString(QString("15:04:05.5"), Qt::ISODate) == QTime(15, 4, 5, 500));
    CHECK(QTime::fromString(QString("15:04"), Qt::ISODate) == QTime(15, 4));
    CHECK(!QTime::fromString(QString("25:04:05"), Qt::ISODate).isValid());
    CHECK(!QTime::fromString(QString("15:04:05x"), Qt::ISODate).isValid());
    CHECK(!QTime::fromString(QString("junk"), Qt::ISODate).isValid());
    CHECK(QTime::fromString(QString("03:04:05 pm"), QString("hh:mm:ss ap")) == QTime(15, 4, 5));
    CHECK(QTime::fromString(QString("12:30 AM"), QString("h:mm AP")) == QTime(0, 30));
    CHECK(QTime::fromString(QString("12:30 PM"), QString("h:mm AP")) == QTime(12, 30));
    CHECK(QTime::fromString(QString("1:30 PM"), QString("h:mm AP")) == QTime(13, 30));
    CHECK(QTime::fromString(QString("1:30 pm"), QString("h:mm a")) == QTime(13, 30));
    CHECK(!QTime::fromString(QString("13:30 PM"), QString("h:mm AP")).isValid());
    CHECK(QTime::fromString(QString("15:04:05.007"), QString("HH:mm:ss.zzz")) == pm);
    CHECK(QTime::fromString(QString("5.07"), QString("s.z")) == QTime(0, 0, 5, 70));
    CHECK(QTime::fromString(QString("7-8"), QString("H-m")) == QTime(7, 8));
    CHECK(!QTime::fromString(QString("7-61"), QString("H-m")).isValid());
    CHECK(QTime::currentTime().isValid());
}

// QDateTime

static void useTimeZone(const char *tz) {
    setenv("TZ", tz, 1);
    tzset();
}

static void testDateTimeBasics() {
    CHECK(QDateTime().isNull());
    CHECK(!QDateTime().isValid());
    const QDateTime utc(QDate(2024, 2, 29), QTime(12, 34, 56, 789), Qt::UTC);
    CHECK(utc.isValid() && !utc.isNull());
    CHECK(utc.timeSpec() == Qt::UTC);
    CHECK(utc.date() == QDate(2024, 2, 29));
    CHECK(utc.time() == QTime(12, 34, 56, 789));
    CHECK(utc.toMSecsSinceEpoch() == 1709210096789LL);
    CHECK(utc.toSecsSinceEpoch() == 1709210096LL);
    CHECK(utc.offsetFromUtc() == 0);
    CHECK(!QDateTime(QDate(2024, 2, 30), QTime(1, 2), Qt::UTC).isValid());
    CHECK(!QDateTime(QDate(2024, 2, 29), QTime(25, 2), Qt::UTC).isValid());
    CHECK(QDateTime(QDate(2024, 2, 29), QTime(), Qt::UTC).time() == QTime(0, 0));

    CHECK(QDateTime::fromSecsSinceEpoch(0, Qt::UTC).toString(Qt::ISODate) == "1970-01-01T00:00:00Z");
    CHECK(QDateTime::fromSecsSinceEpoch(-1, Qt::UTC).toString(Qt::ISODate) == "1969-12-31T23:59:59Z");
    CHECK(QDateTime::fromMSecsSinceEpoch(-1, Qt::UTC).toString(Qt::ISODateWithMs) == "1969-12-31T23:59:59.999Z");
    CHECK(QDateTime::fromMSecsSinceEpoch(-1, Qt::UTC).toSecsSinceEpoch() == -1);
    CHECK(QDateTime::fromMSecsSinceEpoch(-1, Qt::UTC).toMSecsSinceEpoch() == -1);
    CHECK(QDateTime::fromSecsSinceEpoch(2147472000, Qt::UTC).date() == QDate(2038, 1, 19));
    CHECK(QDateTime::fromSecsSinceEpoch(-2208988800LL, Qt::UTC).date() == QDate(1900, 1, 1));
    CHECK(QDateTime::fromSecsSinceEpoch(4107542400LL, Qt::UTC).date() == QDate(2100, 3, 1));
    for (qint64 s = -3000000000LL; s < 5000000000LL; s += 7654321) {
        const QDateTime d = QDateTime::fromSecsSinceEpoch(s, Qt::UTC);
        CHECK(d.toSecsSinceEpoch() == s);
    }
    QDateTime set;
    set.setSecsSinceEpoch(86400);
    set.setTimeSpec(Qt::UTC);
    set.setMSecsSinceEpoch(86400 * 1000 + 5);
    CHECK(set.date() == QDate(1970, 1, 2) && set.time() == QTime(0, 0, 0, 5));
}

static void testDateTimeOffsets() {
    const QDateTime utc(QDate(2024, 2, 29), QTime(12, 0), Qt::UTC);
    const QDateTime plus(QDate(2024, 2, 29), QTime(17, 30), Qt::OffsetFromUTC, 5 * 3600 + 1800);
    CHECK(plus.timeSpec() == Qt::OffsetFromUTC);
    CHECK(plus.offsetFromUtc() == 19800);
    CHECK(plus.toMSecsSinceEpoch() == utc.toMSecsSinceEpoch());
    CHECK(plus == utc);
    CHECK(!(plus < utc) && !(plus > utc) && plus <= utc && plus >= utc);
    CHECK(plus.toUTC() == utc);
    CHECK(plus.toUTC().time() == QTime(12, 0));
    CHECK(plus.toUTC().timeSpec() == Qt::UTC);
    CHECK(utc.toOffsetFromUtc(-3600 * 8).time() == QTime(4, 0));
    CHECK(utc.toOffsetFromUtc(-3600 * 8).offsetFromUtc() == -28800);
    CHECK(utc.toOffsetFromUtc(3600 * 13).date() == QDate(2024, 3, 1));
    CHECK(plus.toString(Qt::ISODate) == "2024-02-29T17:30:00+05:30");
    CHECK(utc.toOffsetFromUtc(-3600).toString(Qt::ISODate) == "2024-02-29T11:00:00-01:00");
    CHECK(QDateTime::fromSecsSinceEpoch(0, Qt::OffsetFromUTC, 3600).time() == QTime(1, 0));
    CHECK(QDateTime::fromSecsSinceEpoch(0, Qt::OffsetFromUTC, -3600).date() == QDate(1969, 12, 31));
    CHECK(QDateTime(QDate(2024, 1, 1), QTime(0, 0), Qt::UTC) < QDateTime(QDate(2024, 1, 1), QTime(0, 0, 0, 1), Qt::UTC));
    CHECK(QDateTime(QDate(2024, 1, 1), QTime(0, 0), Qt::OffsetFromUTC, 7200) < QDateTime(QDate(2024, 1, 1), QTime(0, 0), Qt::UTC));
    CHECK(QDateTime() == QDateTime());
    CHECK(QDateTime() != utc);

    QDateTime d = utc;
    d.setOffsetFromUtc(3600);
    CHECK(d.timeSpec() == Qt::OffsetFromUTC && d.offsetFromUtc() == 3600);
    CHECK(d.time() == QTime(12, 0)); // fields are kept, the instant moves
    CHECK(d.toMSecsSinceEpoch() == utc.toMSecsSinceEpoch() - 3600 * 1000);
}

static void testDateTimeLocal() {
    useTimeZone("EST5EDT,M3.2.0,M11.1.0");
    const QDateTime winter(QDate(2024, 1, 15), QTime(12, 0));
    CHECK(winter.timeSpec() == Qt::LocalTime);
    CHECK(winter.offsetFromUtc() == -5 * 3600);
    CHECK(winter.toUTC().time() == QTime(17, 0));
    CHECK(winter.toUTC().timeSpec() == Qt::UTC);
    CHECK(winter.toMSecsSinceEpoch() == QDateTime(QDate(2024, 1, 15), QTime(17, 0), Qt::UTC).toMSecsSinceEpoch());
    const QDateTime summer(QDate(2024, 7, 15), QTime(12, 0));
    CHECK(summer.offsetFromUtc() == -4 * 3600);
    CHECK(summer.toUTC().time() == QTime(16, 0));
    CHECK(summer.toUTC().toLocalTime() == summer);
    CHECK(summer.toUTC().toLocalTime().time() == QTime(12, 0));
    CHECK(summer.toUTC().toLocalTime().timeSpec() == Qt::LocalTime);
    CHECK(QDateTime::fromSecsSinceEpoch(1721044800).time() == QTime(8, 0)); // 2024-07-15T12:00Z
    CHECK(QDateTime::fromSecsSinceEpoch(1705338000).time() == QTime(12, 0)); // 2024-01-15T17:00Z
    CHECK(QDateTime::fromSecsSinceEpoch(1705338000).toSecsSinceEpoch() == 1705338000);

    // Wall-clock arithmetic keeps the time of day across the DST jump, elapsed time does not.
    const QDateTime before(QDate(2024, 3, 9), QTime(12, 0));
    CHECK(before.addDays(1).time() == QTime(12, 0));
    CHECK(before.addDays(1).date() == QDate(2024, 3, 10));
    CHECK(before.addDays(2).time() == QTime(12, 0));
    CHECK(before.addSecs(2 * 86400).time() == QTime(13, 0));
    CHECK(before.addSecs(86400).date() == QDate(2024, 3, 10));
    CHECK(before.secsTo(before.addDays(2)) == 2 * 86400 - 3600);
    CHECK(before.daysTo(before.addDays(2)) == 2);

    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    CHECK(now.isValid() && nowUtc.isValid());
    CHECK(now.timeSpec() == Qt::LocalTime && nowUtc.timeSpec() == Qt::UTC);
    CHECK(std::llabs(now.toMSecsSinceEpoch() - nowUtc.toMSecsSinceEpoch()) < 2000);
    CHECK(std::llabs(QDateTime::currentMSecsSinceEpoch() - nowUtc.toMSecsSinceEpoch()) < 2000);
    CHECK(std::llabs(QDateTime::currentSecsSinceEpoch() - nowUtc.toSecsSinceEpoch()) <= 2);
    CHECK(std::llabs(std::time(nullptr) - now.toSecsSinceEpoch()) <= 2);
    CHECK(now.date().year() >= 2024);

    useTimeZone("UTC");
    CHECK(QDateTime(QDate(2024, 1, 15), QTime(12, 0)).offsetFromUtc() == 0);
    CHECK(QDateTime(QDate(2024, 1, 15), QTime(12, 0)).toMSecsSinceEpoch() ==
          QDateTime(QDate(2024, 1, 15), QTime(12, 0), Qt::UTC).toMSecsSinceEpoch());
    useTimeZone("<+0530>-5:30");
    CHECK(QDateTime(QDate(2024, 1, 15), QTime(12, 0)).offsetFromUtc() == 19800);
    CHECK(QDateTime(QDate(2024, 1, 15), QTime(12, 0)).toUTC().time() == QTime(6, 30));
    useTimeZone("UTC");
}

static void testDateTimeArithmetic() {
    const QDateTime a(QDate(2024, 1, 31), QTime(12, 0), Qt::UTC);
    CHECK(a.addMonths(1).date() == QDate(2024, 2, 29) && a.addMonths(1).time() == QTime(12, 0));
    CHECK(a.addMonths(1).timeSpec() == Qt::UTC);
    CHECK(a.addYears(1).date() == QDate(2025, 1, 31));
    CHECK(QDateTime(QDate(2024, 2, 29), QTime(1, 0), Qt::UTC).addYears(1).date() == QDate(2025, 2, 28));
    CHECK(a.addDays(1).date() == QDate(2024, 2, 1));
    CHECK(a.addDays(-31).date() == QDate(2023, 12, 31));
    CHECK(a.addSecs(12 * 3600).date() == QDate(2024, 2, 1));
    CHECK(a.addSecs(12 * 3600).time() == QTime(0, 0));
    CHECK(a.addSecs(-1).time() == QTime(11, 59, 59));
    CHECK(a.addMSecs(1).time() == QTime(12, 0, 0, 1));
    CHECK(a.addMSecs(-1).time() == QTime(11, 59, 59, 999));
    CHECK(a.addSecs(-12 * 3600 - 1).date() == QDate(2024, 1, 30));
    CHECK(QDateTime().addSecs(5).isNull());
    CHECK(!QDateTime().addDays(5).isValid());

    const QDateTime b = a.addMSecs(1500);
    CHECK(a.msecsTo(b) == 1500);
    CHECK(a.secsTo(b) == 1);
    CHECK(b.msecsTo(a) == -1500);
    CHECK(b.secsTo(a) == -1);
    CHECK(a.secsTo(a.addDays(1)) == 86400);
    CHECK(a.daysTo(a.addSecs(86400 * 3 + 5)) == 3);
    CHECK(a.daysTo(a.addDays(-5)) == -5);
    CHECK(a.addSecs(86400LL * 365 * 100).secsTo(a) == -86400LL * 365 * 100);
}

static void testDateTimeText() {
    const QDateTime utc(QDate(2024, 2, 29), QTime(12, 34, 56, 789), Qt::UTC);
    CHECK(utc.toString(Qt::ISODate) == "2024-02-29T12:34:56Z");
    CHECK(utc.toString(Qt::ISODateWithMs) == "2024-02-29T12:34:56.789Z");
    CHECK(utc.toString(QString("yyyy-MM-dd HH:mm:ss.zzz")) == "2024-02-29 12:34:56.789");
    CHECK(utc.toString(QString("ddd MMM d")) == "Thu Feb 29");
    CHECK(utc.toString(QString("dd/MM/yyyy hh:mm AP")) == "29/02/2024 12:34 PM");
    CHECK(utc.toString(QString("yyyyMMddTHHmmss")) == "20240229T123456");
    CHECK(utc.toString() == "Thu Feb 29 12:34:56 2024 GMT");
    CHECK(utc.toString(Qt::RFC2822Date) == "29 Feb 2024 12:34:56 +0000");
    CHECK(utc.toOffsetFromUtc(3600).toString(Qt::RFC2822Date) == "29 Feb 2024 13:34:56 +0100");
    CHECK(utc.toOffsetFromUtc(-9 * 3600 - 1800).toString(Qt::RFC2822Date) == "29 Feb 2024 03:04:56 -0930");
    CHECK(QDateTime(QDate(2024, 2, 29), QTime(12, 34, 56)).toString(Qt::ISODate) == "2024-02-29T12:34:56");
    CHECK(QDateTime().toString(Qt::ISODate).isEmpty());
    CHECK(QDateTime().toString(QString("yyyy")).isEmpty());

    QDateTime p = QDateTime::fromString(QString("2024-02-29T12:34:56Z"), Qt::ISODate);
    CHECK(p.isValid() && p.timeSpec() == Qt::UTC);
    CHECK(p.toMSecsSinceEpoch() == 1709210096000LL);
    p = QDateTime::fromString(QString("2024-02-29T12:34:56.789Z"), Qt::ISODate);
    CHECK(p.isValid() && p.time() == QTime(12, 34, 56, 789));
    p = QDateTime::fromString(QString("2024-02-29T12:34:56.789"), Qt::ISODateWithMs);
    CHECK(p.isValid() && p.timeSpec() == Qt::LocalTime && p.time().msec() == 789);
    p = QDateTime::fromString(QString("2024-02-29T17:34:56+05:00"), Qt::ISODate);
    CHECK(p.isValid() && p.timeSpec() == Qt::OffsetFromUTC && p.offsetFromUtc() == 18000);
    CHECK(p.toMSecsSinceEpoch() == 1709210096000LL);
    p = QDateTime::fromString(QString("2024-02-29T07:04:56-05:30"), Qt::ISODate);
    CHECK(p.isValid() && p.offsetFromUtc() == -19800 && p.toMSecsSinceEpoch() == 1709210096000LL);
    p = QDateTime::fromString(QString("2024-02-29T07:04:56-0530"), Qt::ISODate);
    CHECK(p.isValid() && p.offsetFromUtc() == -19800);
    p = QDateTime::fromString(QString("2024-02-29"), Qt::ISODate);
    CHECK(p.isValid() && p.time() == QTime(0, 0));
    p = QDateTime::fromString(QString("2024-02-29 12:34"), Qt::ISODate);
    CHECK(p.isValid() && p.time() == QTime(12, 34));
    CHECK(!QDateTime::fromString(QString("2024-02-30T12:34:56"), Qt::ISODate).isValid());
    CHECK(!QDateTime::fromString(QString("2024-02-29T25:34:56"), Qt::ISODate).isValid());
    CHECK(!QDateTime::fromString(QString("2024-02-29T12:34:56Q"), Qt::ISODate).isValid());
    CHECK(!QDateTime::fromString(QString("nonsense"), Qt::ISODate).isValid());
    CHECK(!QDateTime::fromString(QString(""), Qt::ISODate).isValid());
    CHECK(!QDateTime::fromString(QString("2024-02-29T12:34:56ZZ"), Qt::ISODate).isValid());

    p = QDateTime::fromString(QString("2024-02-29 12:34:56.789"), QString("yyyy-MM-dd HH:mm:ss.zzz"));
    CHECK(p.isValid() && p.date() == QDate(2024, 2, 29) && p.time() == QTime(12, 34, 56, 789));
    CHECK(p.timeSpec() == Qt::LocalTime);
    p = QDateTime::fromString(QString("29/02/2024 03:34 PM"), QString("dd/MM/yyyy hh:mm AP"));
    CHECK(p.isValid() && p.time() == QTime(15, 34));
    CHECK(!QDateTime::fromString(QString("29/02/2023 03:34 PM"), QString("dd/MM/yyyy hh:mm AP")).isValid());
    CHECK(!QDateTime::fromString(QString("29/02/2024"), QString("dd/MM/yyyy hh:mm")).isValid());

    p = QDateTime::fromString(QString("Thu Feb 29 12:34:56 2024 GMT"), Qt::TextDate);
    CHECK(p.isValid() && p.timeSpec() == Qt::UTC && p.toMSecsSinceEpoch() == 1709210096000LL);
    p = QDateTime::fromString(QString("Thu Feb 29 12:34:56 2024"));
    CHECK(p.isValid() && p.timeSpec() == Qt::LocalTime && p.time() == QTime(12, 34, 56));
    p = QDateTime::fromString(QString("Thu Feb 29 12:34:56 2024 GMT+0530"));
    CHECK(p.isValid() && p.offsetFromUtc() == 19800);

    p = QDateTime::fromString(QString("29 Feb 2024 12:34:56 +0000"), Qt::RFC2822Date);
    CHECK(p.isValid() && p.toMSecsSinceEpoch() == 1709210096000LL);
    p = QDateTime::fromString(QString("Thu, 29 Feb 2024 17:34:56 +0500"), Qt::RFC2822Date);
    CHECK(p.isValid() && p.toMSecsSinceEpoch() == 1709210096000LL && p.offsetFromUtc() == 18000);
    p = QDateTime::fromString(QString("Thu, 29 Feb 2024 12:34:56 GMT"), Qt::RFC2822Date);
    CHECK(p.isValid() && p.toMSecsSinceEpoch() == 1709210096000LL);
    p = QDateTime::fromString(QString("Thu, 29 Feb 2024 12:34 -0000"), Qt::RFC2822Date);
    CHECK(p.isValid() && p.time() == QTime(12, 34));
    CHECK(!QDateTime::fromString(QString("Thu, 29 Feb 2024"), Qt::RFC2822Date).isValid());

    for (qint64 s = -2000000000LL; s < 4000000000LL; s += 12345679) {
        const QDateTime d = QDateTime::fromMSecsSinceEpoch(s * 1000 + 123, Qt::UTC);
        CHECK(QDateTime::fromString(d.toString(Qt::ISODateWithMs), Qt::ISODateWithMs).toMSecsSinceEpoch() == d.toMSecsSinceEpoch());
        CHECK(QDateTime::fromString(d.toString(Qt::TextDate), Qt::TextDate).toSecsSinceEpoch() == d.toSecsSinceEpoch());
        CHECK(QDateTime::fromString(d.toString(Qt::RFC2822Date), Qt::RFC2822Date).toSecsSinceEpoch() == d.toSecsSinceEpoch());
    }
}

// QUuid

static void testUuid() {
    const QUuid dns = QUuid::fromString(QString("{6ba7b810-9dad-11d1-80b4-00c04fd430c8}"));
    const QUuid url = QUuid::fromString(QString("6ba7b811-9dad-11d1-80b4-00c04fd430c8"));
    const QUuid oid = QUuid::fromString(QString("6ba7b812-9dad-11d1-80b4-00c04fd430c8"));
    const QUuid x500 = QUuid::fromString(QString("6ba7b8149dad11d180b400c04fd430c8"));
    CHECK(!dns.isNull() && !url.isNull() && !oid.isNull() && !x500.isNull());
    CHECK(dns.data1 == 0x6ba7b810 && dns.data2 == 0x9dad && dns.data3 == 0x11d1);
    CHECK(dns.data4[0] == 0x80 && dns.data4[1] == 0xb4 && dns.data4[2] == 0x00 && dns.data4[7] == 0xc8);

    CHECK(QUuid::createUuidV5(dns, QByteArray("www.example.com")).toString(QUuid::WithoutBraces) ==
          "2ed6657d-e927-568b-95e1-2665a8aea6a2");
    CHECK(QUuid::createUuidV5(dns, QString("www.example.com")).toString(QUuid::WithoutBraces) ==
          "2ed6657d-e927-568b-95e1-2665a8aea6a2");
    CHECK(QUuid::createUuidV3(dns, QByteArray("www.example.com")).toString(QUuid::WithoutBraces) ==
          "5df41881-3aed-3515-88a7-2f4a814cf09e");
    CHECK(QUuid::createUuidV5(dns, QByteArray("python.org")).toString(QUuid::WithoutBraces) == "886313e1-3b8a-5372-9b90-0c9aee199e5d");
    CHECK(QUuid::createUuidV3(dns, QByteArray("python.org")).toString(QUuid::WithoutBraces) == "6fa459ea-ee8a-3ca4-894e-db77e160355e");
    CHECK(QUuid::createUuidV3(url, QByteArray("https://example.com/")).toString(QUuid::WithoutBraces) ==
          "b9dcdff8-af4a-365d-8043-0f8361942709");
    CHECK(QUuid::createUuidV5(url, QByteArray("https://example.com/")).toString(QUuid::WithoutBraces) ==
          "dd2c1780-811a-5296-81c5-178a0ef488bc");
    CHECK(QUuid::createUuidV3(oid, QByteArray("1.3.6.1")).toString(QUuid::WithoutBraces) == "dd1a1cef-13d5-368a-ad82-eca71acd4cd1");
    CHECK(QUuid::createUuidV5(oid, QByteArray("1.3.6.1")).toString(QUuid::WithoutBraces) == "1447fa61-5277-5fef-a9b3-fbc6e44f4af3");
    CHECK(QUuid::createUuidV3(x500, QByteArray("cn=foo")).toString(QUuid::WithoutBraces) == "f0acd3e5-2f59-3c82-87de-1941ab9e45ff");
    CHECK(QUuid::createUuidV5(x500, QByteArray("cn=foo")).toString(QUuid::WithoutBraces) == "01f289df-6f87-508f-ba48-73eccbf5208d");
    CHECK(QUuid::createUuidV3(dns, QByteArray()).toString(QUuid::WithoutBraces) == "c87ee674-4ddc-3efe-a74e-dfe25da5d7b3");
    CHECK(QUuid::createUuidV5(dns, QByteArray()).toString(QUuid::WithoutBraces) == "4ebd0208-8328-5d69-8c44-ec50939c0967");
    CHECK(QUuid::createUuidV5(dns, QByteArray("www.example.com")).version() == QUuid::Sha1);
    CHECK(QUuid::createUuidV3(dns, QByteArray("www.example.com")).version() == QUuid::Md5);
    CHECK(QUuid::createUuidV3(dns, QByteArray("a")).variant() == QUuid::DCE);
    CHECK(QUuid::createUuidV5(dns, QByteArray("a")) == QUuid::createUuidV5(dns, QByteArray("a")));
    CHECK(QUuid::createUuidV5(dns, QByteArray("a")) != QUuid::createUuidV5(dns, QByteArray("b")));
    CHECK(QUuid::createUuidV5(dns, QByteArray("a")) != QUuid::createUuidV5(url, QByteArray("a")));

    const QUuid u = QUuid::createUuidV5(dns, QByteArray("www.example.com"));
    CHECK(u.toString() == "{2ed6657d-e927-568b-95e1-2665a8aea6a2}");
    CHECK(u.toString(QUuid::WithBraces) == "{2ed6657d-e927-568b-95e1-2665a8aea6a2}");
    CHECK(u.toString(QUuid::WithoutBraces) == "2ed6657d-e927-568b-95e1-2665a8aea6a2");
    CHECK(u.toString(QUuid::Id128) == "2ed6657de927568b95e12665a8aea6a2");
    CHECK(u.toByteArray() == QByteArray("{2ed6657d-e927-568b-95e1-2665a8aea6a2}"));
    CHECK(u.toByteArray(QUuid::Id128) == QByteArray("2ed6657de927568b95e12665a8aea6a2"));
    CHECK(QUuid(0x2ed6657d, 0xe927, 0x568b, 0x95, 0xe1, 0x26, 0x65, 0xa8, 0xae, 0xa6, 0xa2) == u);

    CHECK(QUuid::fromString(u.toString()) == u);
    CHECK(QUuid::fromString(u.toString(QUuid::WithoutBraces)) == u);
    CHECK(QUuid::fromString(u.toString(QUuid::Id128)) == u);
    CHECK(QUuid::fromString(QString("{2ED6657D-E927-568B-95E1-2665A8AEA6A2}")) == u);
    CHECK(QUuid(QString("{2ed6657d-e927-568b-95e1-2665a8aea6a2}")) == u);
    CHECK(QUuid::fromString("{2ed6657d-e927-568b-95e1-2665a8aea6a2}") == u);
    CHECK(QUuid::fromString(QByteArray("2ed6657d-e927-568b-95e1-2665a8aea6a2")) == u);
    CHECK(QUuid("2ed6657de927568b95e12665a8aea6a2") == u);
    CHECK(u <= u && u >= u);
    CHECK(QUuid::fromString(QString("{2ed6657d-e927-568b-95e1-2665a8aea6a2")).isNull());
    CHECK(QUuid::fromString(QString("2ed6657d-e927-568b-95e1-2665a8aea6a2}")).isNull());
    CHECK(QUuid::fromString(QString("2ed6657d-e927-568b-95e1-2665a8aea6a")).isNull());
    CHECK(QUuid::fromString(QString("2ed6657d-e927-568b-95e1-2665a8aea6a2a")).isNull());
    CHECK(QUuid::fromString(QString("2ed6657de927-568b-95e1-2665a8aea6a2")).isNull());
    CHECK(QUuid::fromString(QString("2ed6657d-e927-568b-95e1-2665a8aea6ag")).isNull());
    CHECK(QUuid::fromString(QString("")).isNull());
    CHECK(QUuid::fromString(QString("not a uuid")).isNull());
    CHECK(QUuid::fromString(QString("{2ed6657de927568b95e12665a8aea6a2}")).isNull());

    const QByteArray raw = u.toRfc4122();
    CHECK(raw.size() == 16);
    CHECK(hx(raw) == "2ed6657de927568b95e12665a8aea6a2");
    CHECK(QUuid::fromRfc4122(raw) == u);
    CHECK(QUuid::fromRfc4122(QByteArray(15, 'x')).isNull());
    CHECK(QUuid::fromRfc4122(QByteArray(17, 'x')).isNull());
    CHECK(QUuid::fromRfc4122(QByteArray()).isNull());

    CHECK(QUuid().isNull());
    CHECK(QUuid().toString() == "{00000000-0000-0000-0000-000000000000}");
    CHECK(QUuid().variant() == QUuid::VarUnknown);
    CHECK(QUuid().version() == QUuid::VerUnknown);
    CHECK(QUuid() == QUuid());
    CHECK(!u.isNull());

    QUuid v;
    v.data4[0] = 0x00;
    v.data1 = 1;
    CHECK(v.variant() == QUuid::NCS);
    v.data4[0] = 0x7f;
    CHECK(v.variant() == QUuid::NCS);
    v.data4[0] = 0x80;
    CHECK(v.variant() == QUuid::DCE);
    v.data4[0] = 0xbf;
    CHECK(v.variant() == QUuid::DCE);
    v.data4[0] = 0xc0;
    CHECK(v.variant() == QUuid::Microsoft);
    v.data4[0] = 0xdf;
    CHECK(v.variant() == QUuid::Microsoft);
    v.data4[0] = 0xe0;
    CHECK(v.variant() == QUuid::Reserved);
    v.data4[0] = 0x80;
    for (int ver = 1; ver <= 5; ++ver) {
        v.data3 = static_cast<ushort>(ver << 12 | 0x123);
        CHECK(v.version() == ver);
    }
    v.data3 = 0x0123;
    CHECK(v.version() == QUuid::VerUnknown);
    v.data3 = 0x6123;
    CHECK(v.version() == QUuid::VerUnknown || static_cast<int>(v.version()) == 6);
    v.data3 = 0x4123;
    v.data4[0] = 0x00;
    CHECK(v.version() == QUuid::VerUnknown); // not an RFC 4122 variant

    CHECK(QUuid(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) < QUuid(2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
    CHECK(QUuid(1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0) > QUuid(1, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0));
    CHECK(QUuid(1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1) > QUuid(1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0));
    CHECK(QUuid(0xffffffff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) > QUuid(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));
    CHECK(QUuid(1, 1, 1, 0xff, 0, 0, 0, 0, 0, 0, 0) > QUuid(1, 1, 1, 0x01, 0, 0, 0, 0, 0, 0, 0));
    CHECK(!(u < u) && !(u > u));
    CHECK(qHash(u) == qHash(QUuid::fromString(u.toString())));
    CHECK(qHash(u, 7) == qHash(u, 7));

    QSet<QUuid> seen;
    for (int i = 0; i < 2000; ++i) {
        const QUuid r = QUuid::createUuid();
        CHECK(r.version() == QUuid::Random);
        CHECK(r.variant() == QUuid::DCE);
        CHECK(!r.isNull());
        CHECK(r.toString().size() == 38);
        CHECK(QUuid::fromString(r.toString()) == r);
        seen.insert(r);
    }
    CHECK(seen.size() == 2000);
    int bits[128] = {};
    for (int i = 0; i < 1000; ++i) {
        const QByteArray b = QUuid::createUuid().toRfc4122();
        for (int k = 0; k < 128; ++k)
            bits[k] += (static_cast<uchar>(b[k / 8]) >> (7 - k % 8)) & 1;
    }
    for (int k = 0; k < 128; ++k) {
        if (k == 48 || k == 49 || k == 50 || k == 51 || k == 64 || k == 65)
            continue; // fixed version and variant bits
        CHECK(bits[k] > 380 && bits[k] < 620);
    }
}

// QRandomGenerator

static void testRandom() {
    QRandomGenerator a(12345), b(12345), c(54321);
    bool differs = false;
    for (int i = 0; i < 100; ++i) {
        const quint32 x = a.generate();
        CHECK(x == b.generate());
        if (x != c.generate())
            differs = true;
    }
    CHECK(differs);
    a.seed(777);
    b.seed(777);
    for (int i = 0; i < 20; ++i)
        CHECK(a.generate64() == b.generate64());
    a.seed(1);
    QRandomGenerator d(1);
    CHECK(a.generate() == d.generate());
    QRandomGenerator copy = a;
    CHECK(copy.generate() == a.generate());
    const quint32 seedData[] = {1, 2, 3, 4};
    QRandomGenerator s1(seedData), s2(seedData, 4);
    CHECK(s1.generate() == s2.generate());
    QRandomGenerator e(5), f(5);
    e.discard(10);
    for (int i = 0; i < 10; ++i)
        f.generate();
    CHECK(e.generate() == f.generate());

    QRandomGenerator g(42);
    int counts[6] = {};
    for (int i = 0; i < 60000; ++i) {
        const quint32 v = g.bounded(6u);
        CHECK(v < 6);
        ++counts[v];
    }
    for (int k = 0; k < 6; ++k)
        CHECK(counts[k] > 9000 && counts[k] < 11000);
    for (int i = 0; i < 10000; ++i) {
        CHECK(g.bounded(1) == 0);
        CHECK(g.bounded(1000) >= 0 && g.bounded(1000) < 1000);
        const int r = g.bounded(-10, 10);
        CHECK(r >= -10 && r < 10);
        const quint32 u = g.bounded(100u, 200u);
        CHECK(u >= 100 && u < 200);
        const qint64 w = g.bounded(qint64(-5000000000LL), qint64(5000000000LL));
        CHECK(w >= -5000000000LL && w < 5000000000LL);
        const quint64 q = g.bounded(quint64(1) << 40);
        CHECK(q < (quint64(1) << 40));
        const double dbl = g.bounded(2.5);
        CHECK(dbl >= 0.0 && dbl < 2.5);
    }
    // The full int range must not overflow.
    bool negative = false, positive = false;
    for (int i = 0; i < 200; ++i) {
        const int full = g.bounded(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        (full < 0 ? negative : positive) = true;
        const qint64 wide = g.bounded(std::numeric_limits<qint64>::min(), std::numeric_limits<qint64>::max());
        (void)wide;
    }
    CHECK(negative && positive);
    bool sawLow = false, sawHigh = false;
    for (int i = 0; i < 2000; ++i) {
        const int r = g.bounded(5, 8);
        sawLow |= r == 5;
        sawHigh |= r == 7;
        CHECK(r >= 5 && r < 8);
    }
    CHECK(sawLow && sawHigh);

    double sum = 0;
    bool anyHighHalf = false;
    for (int i = 0; i < 100000; ++i) {
        const double x = g.generateDouble();
        CHECK(x >= 0.0 && x < 1.0);
        sum += x;
        anyHighHalf |= x > 0.5;
    }
    CHECK(anyHighHalf);
    CHECK(near(sum / 100000, 0.5, 0.01));

    bool hi = false, lo = false;
    for (int i = 0; i < 100; ++i) {
        const quint64 x = g.generate64();
        hi |= (x >> 32) != 0;
        lo |= (x & 0xffffffffu) != 0;
    }
    CHECK(hi && lo);

    quint32 buf[16] = {};
    g.fillRange(buf, 16);
    bool nonZero = false;
    for (quint32 v : buf)
        nonZero |= v != 0;
    CHECK(nonZero);
    quint64 buf64[8] = {};
    g.fillRange(buf64, 8);
    nonZero = false;
    for (quint64 v : buf64)
        nonZero |= (v >> 32) != 0;
    CHECK(nonZero);
    quint32 arr[8] = {};
    g.generate(std::begin(arr), std::end(arr));
    nonZero = false;
    for (quint32 v : arr)
        nonZero |= v != 0;
    CHECK(nonZero);
    QRandomGenerator h1(9), h2(9);
    quint32 r1[4], r2[4];
    h1.fillRange(r1, 4);
    for (int i = 0; i < 4; ++i)
        r2[i] = h2.generate();
    CHECK(r1[0] == r2[0] && r1[3] == r2[3]);

    CHECK(QRandomGenerator::global() != nullptr);
    CHECK(QRandomGenerator::global() == QRandomGenerator::global());
    CHECK(QRandomGenerator::system() != nullptr);
    CHECK(QRandomGenerator::system() == QRandomGenerator::system());
    CHECK(QRandomGenerator::system() != QRandomGenerator::global());
    CHECK(QRandomGenerator::global()->bounded(10) < 10);
    CHECK(QRandomGenerator::system()->bounded(10) < 10);
    CHECK(QRandomGenerator::system()->generateDouble() < 1.0);
    int distinct = 0;
    quint32 last = QRandomGenerator::system()->generate();
    for (int i = 0; i < 50; ++i) {
        const quint32 v = QRandomGenerator::system()->generate();
        distinct += v != last;
        last = v;
    }
    CHECK(distinct > 45);
    QRandomGenerator sec = QRandomGenerator::securelySeeded();
    QRandomGenerator sec2 = QRandomGenerator::securelySeeded();
    CHECK(sec.generate64() != sec2.generate64());
    CHECK(QRandomGenerator::min() == 0 && QRandomGenerator::max() == 0xffffffffu);
    quint32 sys[4];
    QRandomGenerator::system()->fillRange(sys, 4);
    CHECK((sys[0] | sys[1] | sys[2] | sys[3]) != 0);
}

// QCryptographicHash / QMessageAuthenticationCode

struct HashVec { QCryptographicHash::Algorithm alg; int len; const char *empty, *abc, *longMsg, *million; };
static const HashVec kHashVecs[] = {
    {QCryptographicHash::Md4, 16,
     "31d6cfe0d16ae931b73c59d7e0c089c0",
     "a448017aaf21d8525fc10ae87aa6729d",
     "4691a9ec81b1a6bd1ab8557240b245c5",
     "bbce80cc6bb65e5c6745e30d4eeca9a4"},
    {QCryptographicHash::Md5, 16,
     "d41d8cd98f00b204e9800998ecf8427e",
     "900150983cd24fb0d6963f7d28e17f72",
     "8215ef0796a20bcaaae116d3876c664a",
     "7707d6ae4e027c70eea2a935c2296f21"},
    {QCryptographicHash::Sha1, 20,
     "da39a3ee5e6b4b0d3255bfef95601890afd80709",
     "a9993e364706816aba3e25717850c26c9cd0d89d",
     "84983e441c3bd26ebaae4aa1f95129e5e54670f1",
     "34aa973cd4c4daa4f61eeb2bdbad27316534016f"},
    {QCryptographicHash::Sha224, 28,
     "d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f",
     "23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7",
     "75388b16512776cc5dba5da1fd890150b0c6455cb4f58b1952522525",
     "20794655980c91d8bbb4c1ea97618a4bf03f42581948b2ee4ee7ad67"},
    {QCryptographicHash::Sha256, 32,
     "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
     "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
     "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"},
    {QCryptographicHash::Sha384, 48,
     "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b",
     "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7",
     "3391fdddfc8dc7393707a65b1b4709397cf8b1d162af05abfe8f450de5f36bc6b0455a8520bc4e6f5fe95b1fe3c8452b",
     "9d0e1809716474cb086e834e310a4a1ced149e9c00f248527972cec5704c2a5b07b8b3dc38ecc4ebae97ddd87f3d8985"},
    {QCryptographicHash::Sha512, 64,
     "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e",
     "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
     "204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c33596fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445",
     "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973ebde0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b"},
    {QCryptographicHash::Keccak_224, 28,
     "f71837502ba8e10837bdd8d365adb85591895602fc552b48b7390abd",
     "c30411768506ebe1c2871b1ee2e87d38df342317300a9b97a95ec6a8",
     "e51faa2b4655150b931ee8d700dc202f763ca5f962c529eae55012b6",
     "19f9167be2a04c43abd0ed554788101b9c339031acc8e1468531303f"},
    {QCryptographicHash::Keccak_256, 32,
     "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470",
     "4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45",
     "45d3b367a6904e6e8d502ee04999a7c27647f91fa845d456525fd352ae3d7371",
     "fadae6b49f129bbb812be8407b7b2894f34aecf6dbd1f9b0f0c7e9853098fc96"},
    {QCryptographicHash::Keccak_384, 48,
     "2c23146a63a29acf99e73b88f8c24eaa7dc60aa771780ccc006afbfa8fe2479b2dd2b21362337441ac12b515911957ff",
     "f7df1165f033337be098e7d288ad6a2f74409d7a60b49c36642218de161b1f99f8c681e4afaf31a34db29fb763e3c28e",
     "b41e8896428f1bcbb51e17abd6acc98052a3502e0d5bf7fa1af949b4d3c855e7c4dc2c390326b3f3e74c7b1e2b9a3657",
     "0c8324e1ebc182822c5e2a086cac07c2fe00e3bce61d01ba8ad6b71780e2dec5fb89e5ae90cb593e57bc6258fdd94e17"},
    {QCryptographicHash::Keccak_512, 64,
     "0eab42de4c3ceb9235fc91acffe746b29c29a8c366b7c60e4e67c466f36a4304c00fa9caf9d87976ba469bcbe06713b435f091ef2769fb160cdab33d3670680e",
     "18587dc2ea106b9a1563e32b3312421ca164c7f1f07bc922a9c83d77cea3a1e5d0c69910739025372dc14ac9642629379540c17e2a65b19d77aa511a9d00bb96",
     "6aa6d3669597df6d5a007b00d09c20795b5c4218234e1698a944757a488ecdc09965435d97ca32c3cfed7201ff30e070cd947f1fc12b9d9214c467d342bcba5d",
     "5cf53f2e556be5a624425ede23d0e8b2c7814b4ba0e4e09cbbf3c2fac7056f61e048fc341262875ebc58a5183fea651447124370c1ebf4d6c89bc9a7731063bb"},
    {QCryptographicHash::Sha3_224, 28,
     "6b4e03423667dbb73b6e15454f0eb1abd4597f9a1b078e3f5b5a6bc7",
     "e642824c3f8cf24ad09234ee7d3c766fc9a3a5168d0c94ad73b46fdf",
     "8a24108b154ada21c9fd5574494479ba5c7e7ab76ef264ead0fcce33",
     "d69335b93325192e516a912e6d19a15cb51c6ed5c15243e7a7fd653c"},
    {QCryptographicHash::Sha3_256, 32,
     "a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a",
     "3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532",
     "41c0dba2a9d6240849100376a8235e2c82e1b9998a999e21db32dd97496d3376",
     "5c8875ae474a3634ba4fd55ec85bffd661f32aca75c6d699d0cdcb6c115891c1"},
    {QCryptographicHash::Sha3_384, 48,
     "0c63a75b845e4f7d01107d852e4c2485c51a50aaaa94fc61995e71bbee983a2ac3713831264adb47fb6bd1e058d5f004",
     "ec01498288516fc926459f58e2c6ad8df9b473cb0fc08c2596da7cf0e49be4b298d88cea927ac7f539f1edf228376d25",
     "991c665755eb3a4b6bbdfb75c78a492e8c56a22c5c4d7e429bfdbc32b9d4ad5aa04a1f076e62fea19eef51acd0657c22",
     "eee9e24d78c1855337983451df97c8ad9eedf256c6334f8e948d252d5e0e76847aa0774ddb90a842190d2c558b4b8340"},
    {QCryptographicHash::Sha3_512, 64,
     "a69f73cca23a9ac5c8b567dc185a756e97c982164fe25859e0d1dcc1475c80a615b2123af1f5f94c11e3e9402c3ac558f500199d95b6d3e301758586281dcd26",
     "b751850b1a57168a5693cd924b6b096e08f621827444f70d884f5d0240d2712e10e116e9192af3c91a7ec57647e3934057340b4cf408d5a56592f8274eec53f0",
     "04a371e84ecfb5b8b77cb48610fca8182dd457ce6f326a0fd3d7ec2f1e91636dee691fbe0c985302ba1b0d8dc78c086346b533b49c030d99a27daf1139d6e75e",
     "3c3a876da14034ab60627c077bb98f7e120a2a5370212dffb3385a18d4f38859ed311d0a9d5141ce9cc5c66ee689b266a8aa18ace8282a0e0db596c90b0a7b87"},
};

static QByteArray fromHex(const char *h) { return QByteArray::fromHex(QByteArray(h)); }

static void testHashKnownAnswers() {
    for (const HashVec &v : kHashVecs) {
        using H = QCryptographicHash;
        CHECK(H::hashLength(v.alg) == v.len);
        CHECK(H::supportsAlgorithm(v.alg));
        CHECK(H::hash(QByteArray(), v.alg) == fromHex(v.empty));
        CHECK(H::hash(QByteArray("abc"), v.alg) == fromHex(v.abc));
        CHECK(H::hash(QByteArrayView("abc"), v.alg) == fromHex(v.abc));
        CHECK(H::hash(QByteArray("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"), v.alg) == fromHex(v.longMsg));
        CHECK(H::hash(QByteArray(1000000, 'a'), v.alg) == fromHex(v.million));
        CHECK(H::hash(QByteArray(1000000, 'a'), v.alg).size() == v.len);

        H rv(v.alg);
        rv.addData("abc", 3);
        CHECK(rv.resultView().toByteArray() == fromHex(v.abc));
        CHECK(rv.resultView() == QByteArrayView(fromHex(v.abc)));
        CHECK(rv.resultView().size() == v.len);
        H h(v.alg);
        CHECK(h.algorithm() == v.alg);
        CHECK(h.result() == fromHex(v.empty));
        h.addData(QByteArray("abc"));
        CHECK(h.result() == fromHex(v.abc));
        CHECK(h.result() == fromHex(v.abc)); // result() does not consume the state
        h.reset();
        CHECK(h.result() == fromHex(v.empty));
        h.addData(QByteArray("ab"));
        h.addData(QByteArray("c"));
        CHECK(h.result() == fromHex(v.abc));
        h.reset();
        const QByteArray block(1000, 'a');
        for (int i = 0; i < 1000; ++i)
            h.addData(block);
        CHECK(h.result() == fromHex(v.million));
        h.addData(QByteArray("more"));
        CHECK(h.result() != fromHex(v.million));
        h.reset();
        h.addData(QByteArray("abc"));
        CHECK(h.result() == fromHex(v.abc));
        h.addData(QByteArray()); // empty adds change nothing
        h.addData(QByteArrayView());
        CHECK(h.result() == fromHex(v.abc));
    }
    // 15 is the largest value the enum can hold that is not an algorithm.
    CHECK(!QCryptographicHash::supportsAlgorithm(static_cast<QCryptographicHash::Algorithm>(15)));
    CHECK(QCryptographicHash::hashLength(static_cast<QCryptographicHash::Algorithm>(15)) == 0);
}

static void testHashIncremental() {
    QByteArray data(3000, '\0');
    for (int i = 0; i < data.size(); ++i)
        data[i] = static_cast<char>((i * 31 + i / 7) & 0xff);
    const int chunks[] = {1, 2, 3, 7, 55, 56, 57, 63, 64, 65, 71, 72, 73, 103, 104, 105, 127, 128, 129, 135, 136, 137, 143, 144, 145, 199, 200, 201, 1000};
    for (int alg = 0; alg <= QCryptographicHash::Sha3_512; ++alg) {
        const auto method = static_cast<QCryptographicHash::Algorithm>(alg);
        if (!QCryptographicHash::supportsAlgorithm(method))
            continue;
        for (int len : {0, 1, 55, 56, 63, 64, 65, 111, 112, 119, 120, 127, 128, 129, 135, 136, 137, 199, 200, 201, 3000}) {
            const QByteArray msg = data.left(len);
            const QByteArray want = QCryptographicHash::hash(msg, method);
            for (int chunk : chunks) {
                QCryptographicHash h(method);
                for (int off = 0; off < len; off += chunk)
                    h.addData(msg.mid(off, chunk));
                if (h.result() != want) {
                    std::printf("incremental mismatch alg=%d len=%d chunk=%d\n", alg, len, chunk);
                    CHECK(false);
                }
            }
            // uneven chunk sizes in one go
            QCryptographicHash h(method);
            int off = 0, step = 1;
            while (off < len) {
                h.addData(msg.mid(off, step));
                off += step;
                step = step * 2 + 1;
            }
            CHECK(h.result() == want);
        }
    }
    // distinct algorithms and distinct inputs give distinct digests
    CHECK(QCryptographicHash::hash(QByteArray("a"), QCryptographicHash::Sha3_256) !=
          QCryptographicHash::hash(QByteArray("a"), QCryptographicHash::Keccak_256));
    CHECK(QCryptographicHash::hash(QByteArray("a"), QCryptographicHash::Sha256) !=
          QCryptographicHash::hash(QByteArray("b"), QCryptographicHash::Sha256));

    // moved-from hash can be reassigned and used
    QCryptographicHash m1(QCryptographicHash::Sha1);
    m1.addData(QByteArray("abc"));
    QCryptographicHash m2(std::move(m1));
    CHECK(hx(m2.result()) == "a9993e364706816aba3e25717850c26c9cd0d89d");
    m1 = QCryptographicHash(QCryptographicHash::Md5);
    CHECK(hx(m1.result()) == "d41d8cd98f00b204e9800998ecf8427e");
}

static QByteArray naiveHmac(QCryptographicHash::Algorithm method, QByteArray key, const QByteArray &msg) {
    int block = 64;
    switch (method) {
    case QCryptographicHash::Sha384:
    case QCryptographicHash::Sha512: block = 128; break;
    case QCryptographicHash::Keccak_224: case QCryptographicHash::Sha3_224: block = 144; break;
    case QCryptographicHash::Keccak_256: case QCryptographicHash::Sha3_256: block = 136; break;
    case QCryptographicHash::Keccak_384: case QCryptographicHash::Sha3_384: block = 104; break;
    case QCryptographicHash::Keccak_512: case QCryptographicHash::Sha3_512: block = 72; break;
    default: break;
    }
    if (key.size() > block)
        key = QCryptographicHash::hash(key, method);
    QByteArray ipad(block, '\x36'), opad(block, '\x5c');
    for (int i = 0; i < key.size(); ++i) {
        ipad[i] = static_cast<char>(ipad[i] ^ key[i]);
        opad[i] = static_cast<char>(opad[i] ^ key[i]);
    }
    return QCryptographicHash::hash(opad + QCryptographicHash::hash(ipad + msg, method), method);
}

static void testMessageAuthenticationCode() {
    struct MacVec { QCryptographicHash::Algorithm alg; const char *hex[5]; };
    // RFC 2202 / RFC 4231 style cases: 0x0b*20 + "Hi There"; "Jefe"; 0xaa*20 + 0xdd*50;
    // 0xaa*131 with a short and a long message.
    static const MacVec vecs[] = {
        {QCryptographicHash::Md5, {"5ccec34ea9656392457fa1ac27f08fbc", "750c783e6ab0b503eaa86e310a5db738", "2ab8b9a9f7d3894d15ad8383b97044b2", "bfecaf4efff90a3a668f3922fec3762d", "09b8ae7b15adbbb243aca3491b51512b"}},
        {QCryptographicHash::Sha1, {"b617318655057264e28bc0b6fb378c8ef146be00", "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79", "125d7342b9ac11cd91a39af48aa17b4f63f175d3", "90d0dace1c1bdc957339307803160335bde6df2b", "217e44bb08b6e06a2d6c30f3cb9f537f97c63356"}},
        {QCryptographicHash::Sha224, {"896fb1128abbdf196832107cd49df33f47b4b1169912ba4f53684b22", "a30e01098bc6dbbf45690f3a7e9e6d0f8bbea2a39e6148008fd05e44", "7fb3cb3588c6c1f6ffa9694d7d6ad2649365b0c1f65d69d1ec8333ea", "95e9a0db962095adaebe9b2d6f0dbce2d499f112f2d2b7273fa6870e", "3a854166ac5d9f023f54d517d0b39dbd946770db9c2b95c9f6f565d1"}},
        {QCryptographicHash::Sha256, {"b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843", "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe", "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54", "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2"}},
        {QCryptographicHash::Sha384, {"afd03944d84895626b0825f4ab46907f15f9dadbe4101ec682aa034c7cebc59cfaea9ea9076ede7f4af152e8b2fa9cb6", "af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e8e2240ca5e69e2c78b3239ecfab21649", "88062608d3e6ad8a0aa2ace014c8a86f0aa635d947ac9febe83ef4e55966144b2a5ab39dc13814b94e3ab6e101a34f27", "4ece084485813e9088d2c63a041bc5b44f9ef1012a2b588f3cd11f05033ac4c60c2ef6ab4030fe8296248df163f44952", "6617178e941f020d351e2f254e8fd32c602420feb0b8fb9adccebb82461e99c5a678cc31e799176d3860e6110c46523e"}},
        {QCryptographicHash::Sha512, {"87aa7cdea5ef619d4ff0b4241a1d6cb02379f4e2ce4ec2787ad0b30545e17cdedaa833b7d6b8a702038b274eaea3f4e4be9d914eeb61f1702e696c203a126854", "164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737", "fa73b0089d56a284efb0f0756c890be9b1b5dbdd8ee81a3655f83e33b2279d39bf3e848279a722c806b485a47e67c807b946a337bee8942674278859e13292fb", "80b24263c7c1a3ebb71493c1dd7be8b49b46d1f41b4aeec1121b013783f8f3526b56d037e05f2598bd0fd2215d6a1e5295e64f73f63f0aec8b915a985d786598", "e37b6a775dc87dbaa4dfa9f96e5e3ffddebd71f8867289865df5a32d20cdc944b6022cac3c4982b10d5eeb55c3e4de15134676fb6de0446065c97440fa8c6a58"}},
        {QCryptographicHash::Keccak_256, {"9663d10c73ee294054dc9faf95647cb99731d12210ff7075fb3d3395abfb9821", "aa9aed448c7abc8b5e326ffa6a01cdedf7b4b831881468c044ba8dd4566369a1", "95f43e50f8df80a21977d51a8db3ba572dcd71db24687e6f86f47c1139b26260", "b4d0cdee7ec2ba81a88b86918958312300a15622377929a054a9ce3ae1fac2b6", "1fdc8cb4e27d07c10d897dec39c217792a6e64fa9c63a77ce42ad106ef284e02"}},
        {QCryptographicHash::Sha3_256, {"ba85192310dffa96e2a3a40e69774351140bb7185e1202cdcc917589f95e16bb", "c7d4072e788877ae3596bbb0da73b887c9171f93095b294ae857fbe2645e1ba5", "84ec79124a27107865cedd8bd82da9965e5ed8c37b0ac98005a7f39ed58a4207", "ed73a374b96c005235f948032f09674a58c0ce555cfc1f223b02356560312c3b", "65c5b06d4c3de32a7aef8763261e49adb6e2293ec8e7c61e8de61701fc63e123"}},
        {QCryptographicHash::Sha3_512, {"eb3fbd4b2eaab8f5c504bd3a41465aacec15770a7cabac531e482f860b5ec7ba47ccb2c6f2afce8f88d22b6dc61380f23a668fd3888bb80537c0a0b86407689e", "5a4bfeab6166427c7a3647b747292b8384537cdb89afb3bf5665e4c5e709350b287baec921fd7ca0ee7a0c31d022a95e1fc92ba9d77df883960275beb4e62024", "309e99f9ec075ec6c6d475eda1180687fcf1531195802a99b5677449a8625182851cb332afb6a89c411325fbcbcd42afcb7b6e5aab7ea42c660f97fd8584bf03", "00f751a9e50695b090ed6911a4b65524951cdc15a73a5d58bb55215ea2cd839ac79d2b44a39bafab27e83fde9e11f6340b11d991b1b91bf2eee7fc872426c3a4", "38a456a004bd10d32c9ab8336684112862c3db61adcca31829355eaf46fd5c73d06a1f0d13fec9a652fb3811b577b1b1d1b9789f97ae5b83c6f44dfcf1d67eba"}},
    };
    const QByteArray keys[] = {QByteArray(20, '\x0b'), QByteArray("Jefe"), QByteArray(20, '\xaa'), QByteArray(131, '\xaa'),
                               QByteArray(131, '\xaa')};
    const QByteArray msgs[] = {QByteArray("Hi There"), QByteArray("what do ya want for nothing?"), QByteArray(50, '\xdd'),
                               QByteArray("Test Using Larger Than Block-Size Key - Hash Key First"),
                               QByteArray("This is a test using a larger than block-size key and a larger than block-size data. "
                                          "The key needs to be hashed before being used by the HMAC algorithm.")};
    for (const MacVec &v : vecs) {
        for (int i = 0; i < 5; ++i) {
            const QByteArray want = fromHex(v.hex[i]);
            CHECK(QMessageAuthenticationCode::hash(msgs[i], keys[i], v.alg) == want);
            QMessageAuthenticationCode mac(v.alg, keys[i]);
            mac.addData(msgs[i]);
            CHECK(mac.result() == want);
            CHECK(mac.result() == want);
            mac.reset();
            for (int off = 0; off < msgs[i].size(); off += 7)
                mac.addData(msgs[i].mid(off, 7));
            CHECK(mac.result() == want);
        }
    }
    // RFC 2202 test case 3/4 for HMAC-SHA1 and MD5
    CHECK(hx(QMessageAuthenticationCode::hash(QByteArray(50, '\xcd'),
                                              QByteArray::fromHex("0102030405060708090a0b0c0d0e0f10111213141516171819"),
                                              QCryptographicHash::Sha1)) == "4c9007f4026250c6bc8414f9bf50c86c2d7235da");
    CHECK(hx(QMessageAuthenticationCode::hash(QByteArray("Test With Truncation"), QByteArray(20, '\x0c'), QCryptographicHash::Sha1)) ==
          "4c1a03424b55e07fe7f27be1d58bb9324a9a5a04");

    // every algorithm against the definition, with keys around the block size
    const int keyLengths[] = {0, 1, 20, 63, 64, 65, 71, 72, 73, 103, 104, 105, 127, 128, 129, 135, 136, 137, 143, 144, 145, 200, 300};
    for (int alg = 0; alg <= QCryptographicHash::Sha3_512; ++alg) {
        const auto method = static_cast<QCryptographicHash::Algorithm>(alg);
        if (!QCryptographicHash::supportsAlgorithm(method))
            continue;
        for (int kl : keyLengths) {
            QByteArray key(kl, '\0');
            for (int i = 0; i < kl; ++i)
                key[i] = static_cast<char>(i * 7 + 3);
            for (const char *msg : {"", "x", "The quick brown fox jumps over the lazy dog"}) {
                const QByteArray m(msg);
                const QByteArray want = naiveHmac(method, key, m);
                if (QMessageAuthenticationCode::hash(m, key, method) != want) {
                    std::printf("hmac mismatch alg=%d keylen=%d msg=%s\n", alg, kl, msg);
                    CHECK(false);
                }
            }
        }
    }
    QMessageAuthenticationCode mac(QCryptographicHash::Sha256);
    mac.setKey(QByteArray("Jefe"));
    mac.addData(QByteArray("what do ya want for nothing?"));
    CHECK(hx(mac.result()) == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    mac.setKey(QByteArray(131, '\xaa')); // setKey restarts the message
    mac.addData(QByteArray("Test Using Larger Than Block-Size Key - Hash Key First"));
    CHECK(hx(mac.result()) == "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    CHECK(QMessageAuthenticationCode::hash(QByteArray("a"), QByteArray("k"), QCryptographicHash::Sha256) !=
          QMessageAuthenticationCode::hash(QByteArray("a"), QByteArray("l"), QCryptographicHash::Sha256));
    CHECK(QMessageAuthenticationCode::hash(QByteArray("a"), QByteArray("k"), QCryptographicHash::Sha256) !=
          QCryptographicHash::hash(QByteArray("ka"), QCryptographicHash::Sha256));
}

static void testHashDevice() {
    QBuffer buf;
    buf.setData(QByteArray(10000, 'a'));
    CHECK(buf.open(QIODevice::ReadOnly));
    QCryptographicHash h(QCryptographicHash::Sha256);
    CHECK(h.addData(&buf));
    CHECK(h.result() == QCryptographicHash::hash(QByteArray(10000, 'a'), QCryptographicHash::Sha256));
    QCryptographicHash n(QCryptographicHash::Sha1);
    CHECK(!n.addData(static_cast<QIODevice *>(nullptr)));
    QBuffer closed;
    CHECK(!n.addData(&closed));
}

// QVersionNumber

static void testVersionNumber() {
    qsizetype idx = -1;
    QVersionNumber v = QVersionNumber::fromString(QString("1.2.3"), &idx);
    CHECK(v.segments() == QList<int>({1, 2, 3}));
    CHECK(idx == 5);
    CHECK(v.majorVersion() == 1 && v.minorVersion() == 2 && v.microVersion() == 3);
    CHECK(v.segmentCount() == 3);
    CHECK(v.toString() == "1.2.3");
    v = QVersionNumber::fromString(QString("1.2.3-beta"), &idx);
    CHECK(v == QVersionNumber(1, 2, 3) && idx == 5);
    v = QVersionNumber::fromString(QString("1.2.3."), &idx);
    CHECK(v == QVersionNumber(1, 2, 3) && idx == 5);
    v = QVersionNumber::fromString(QString("1..2"), &idx);
    CHECK(v == QVersionNumber(1) && idx == 1);
    v = QVersionNumber::fromString(QString("abc"), &idx);
    CHECK(v.isNull() && idx == 0);
    v = QVersionNumber::fromString(QString(""), &idx);
    CHECK(v.isNull() && idx == 0);
    v = QVersionNumber::fromString(QString(".1"), &idx);
    CHECK(v.isNull() && idx == 0);
    v = QVersionNumber::fromString(QString("10.20b3"), &idx);
    CHECK(v == QVersionNumber(10, 20) && idx == 5);
    v = QVersionNumber::fromString(QString("7"), &idx);
    CHECK(v == QVersionNumber(7) && idx == 1);
    v = QVersionNumber::fromString(QString("1.2.3.4.5.6.7.8.9.10"), &idx);
    CHECK(v.segmentCount() == 10 && v.segmentAt(9) == 10 && idx == 20);
    v = QVersionNumber::fromString(QString("007.01"));
    CHECK(v == QVersionNumber(7, 1));
    v = QVersionNumber::fromString(QString("1.99999999999.3"), &idx);
    CHECK(v == QVersionNumber(1) && idx == 1);
    CHECK(QVersionNumber::fromString(QString("1.5")).toString() == "1.5");
    CHECK(QVersionNumber::fromString(QString("1.5"), nullptr) == QVersionNumber(1, 5));

    CHECK(QVersionNumber().isNull());
    CHECK(QVersionNumber().toString().isEmpty());
    CHECK(QVersionNumber().segmentCount() == 0);
    CHECK(QVersionNumber().majorVersion() == 0);
    CHECK(!QVersionNumber(0).isNull());
    CHECK(QVersionNumber(4).toString() == "4");
    CHECK(QVersionNumber(2, 5).toString() == "2.5");
    CHECK(QVersionNumber(QList<int>({1, 0, 7})).toString() == "1.0.7");
    CHECK(QVersionNumber({1, 2, 3, 4}).segmentCount() == 4);
    CHECK(QVersionNumber(1, 2, 3).segmentAt(5) == 0);

    CHECK(QVersionNumber::compare(QVersionNumber(1, 2), QVersionNumber(1, 2)) == 0);
    CHECK(QVersionNumber::compare(QVersionNumber(1, 2), QVersionNumber(1, 3)) < 0);
    CHECK(QVersionNumber::compare(QVersionNumber(2), QVersionNumber(1, 9, 9)) > 0);
    CHECK(QVersionNumber::compare(QVersionNumber(1, 2), QVersionNumber(1, 2, 1)) < 0);
    CHECK(QVersionNumber::compare(QVersionNumber(1, 2, 1), QVersionNumber(1, 2)) > 0);
    CHECK(QVersionNumber::compare(QVersionNumber(1, 2), QVersionNumber(1, 2, 0)) == 0);
    CHECK(QVersionNumber::compare(QVersionNumber({1, 2, 0, 0}), QVersionNumber(1, 2)) == 0);
    CHECK(QVersionNumber::compare(QVersionNumber({1, 2, 0, 1}), QVersionNumber(1, 2)) > 0);
    CHECK(QVersionNumber::compare(QVersionNumber(1, 2), QVersionNumber({1, 2, 0, 1})) < 0);
    CHECK(QVersionNumber::compare(QVersionNumber(1, 2), QVersionNumber(1, 2, -1)) > 0);
    CHECK(QVersionNumber::compare(QVersionNumber(), QVersionNumber()) == 0);
    CHECK(QVersionNumber::compare(QVersionNumber(), QVersionNumber(0)) == 0);
    CHECK(QVersionNumber::compare(QVersionNumber(), QVersionNumber(1)) < 0);
    CHECK(QVersionNumber(1, 2) == QVersionNumber(1, 2, 0));
    CHECK(QVersionNumber(1, 2) != QVersionNumber(1, 2, 1));
    CHECK(QVersionNumber(1, 2) < QVersionNumber(1, 10));
    CHECK(QVersionNumber(1, 10) > QVersionNumber(1, 9));
    CHECK(QVersionNumber(1, 2) <= QVersionNumber(1, 2));
    CHECK(QVersionNumber(1, 2) >= QVersionNumber(1, 2));
    CHECK(QVersionNumber(5, 15, 2) > QVersionNumber(5, 9, 100));
    CHECK(QVersionNumber(-1) < QVersionNumber(0));
    CHECK(QVersionNumber(std::numeric_limits<int>::max()) > QVersionNumber(std::numeric_limits<int>::min()));
    CHECK(QVersionNumber(std::numeric_limits<int>::min()) < QVersionNumber(std::numeric_limits<int>::max()));
    CHECK(QVersionNumber::compare(QVersionNumber(std::numeric_limits<int>::min()), QVersionNumber(std::numeric_limits<int>::max())) < 0);
    CHECK(QVersionNumber::compare(QVersionNumber(std::numeric_limits<int>::max()), QVersionNumber(std::numeric_limits<int>::min())) > 0);

    CHECK(QVersionNumber::commonPrefix(QVersionNumber(1, 2, 3), QVersionNumber(1, 2, 4)) == QVersionNumber(1, 2));
    CHECK(QVersionNumber::commonPrefix(QVersionNumber(1, 2, 3), QVersionNumber(1, 2, 3)) == QVersionNumber(1, 2, 3));
    CHECK(QVersionNumber::commonPrefix(QVersionNumber(1, 2, 3), QVersionNumber(1, 2)) == QVersionNumber(1, 2));
    CHECK(QVersionNumber::commonPrefix(QVersionNumber(1, 2), QVersionNumber(1, 2, 3)).segments() == QList<int>({1, 2}));
    CHECK(QVersionNumber::commonPrefix(QVersionNumber(2, 2), QVersionNumber(1, 2)).isNull());
    CHECK(QVersionNumber::commonPrefix(QVersionNumber(), QVersionNumber(1)).isNull());
    CHECK(QVersionNumber::commonPrefix(QVersionNumber(1, 2, 3), QVersionNumber(1, 9, 3)).segments() == QList<int>({1}));

    CHECK(QVersionNumber(1, 2).isPrefixOf(QVersionNumber(1, 2, 3)));
    CHECK(QVersionNumber(1, 2).isPrefixOf(QVersionNumber(1, 2)));
    CHECK(!QVersionNumber(1, 2, 3).isPrefixOf(QVersionNumber(1, 2)));
    CHECK(!QVersionNumber(1, 3).isPrefixOf(QVersionNumber(1, 2, 3)));
    CHECK(QVersionNumber().isPrefixOf(QVersionNumber(1)));
    CHECK(QVersionNumber().isPrefixOf(QVersionNumber()));
    CHECK(!QVersionNumber(1).isPrefixOf(QVersionNumber()));

    CHECK(QVersionNumber({1, 2, 0, 0}).normalized() == QVersionNumber(1, 2));
    CHECK(QVersionNumber({1, 2, 0, 0}).normalized().segmentCount() == 2);
    CHECK(QVersionNumber(1, 0, 3).normalized().segmentCount() == 3);
    CHECK(QVersionNumber(0, 0).normalized().segmentCount() == 0);
    CHECK(QVersionNumber(0, 0).normalized().isNull());
    CHECK(QVersionNumber({1, 2, 0, 0}).isNormalized() == false);
    CHECK(QVersionNumber(1, 2).isNormalized());
    CHECK(QVersionNumber().isNormalized());
    CHECK(QVersionNumber(1, 0, 3).isNormalized());
    CHECK(QVersionNumber(1, 2, 0).normalized().toString() == "1.2");
}

// Geometry

static void testPoint() {
    QPoint p(3, -4);
    CHECK(p.x() == 3 && p.y() == -4);
    CHECK(!p.isNull() && QPoint().isNull() && QPoint(0, 0).isNull());
    CHECK(p.manhattanLength() == 7);
    CHECK(p.transposed() == QPoint(-4, 3));
    CHECK(p + QPoint(1, 1) == QPoint(4, -3));
    CHECK(p - QPoint(1, 1) == QPoint(2, -5));
    CHECK(p * 2 == QPoint(6, -8) && 2 * p == QPoint(6, -8));
    CHECK(-p == QPoint(-3, 4) && +p == p);
    CHECK(QPoint(3, 5) * 1.5 == QPoint(5, 8));
    CHECK(QPoint(10, 20) / 4.0 == QPoint(3, 5));
    CHECK(QPoint::dotProduct(QPoint(1, 2), QPoint(3, 4)) == 11);
    p += QPoint(1, 1);
    p -= QPoint(0, 2);
    CHECK(p == QPoint(4, -5));
    p *= 2;
    CHECK(p == QPoint(8, -10));
    p.rx() = 1;
    p.ry() = 2;
    CHECK(p == QPoint(1, 2) && p != QPoint(2, 1));
    p.setX(9);
    p.setY(8);
    CHECK(p == QPoint(9, 8));
    CHECK(QPoint(1, 2).toPointF() == QPointF(1.0, 2.0));

    QPointF f(1.5, -2.5);
    CHECK(near(f.x(), 1.5) && near(f.y(), -2.5));
    CHECK(QPointF().isNull() && !f.isNull());
    CHECK(QPointF(1e-15, -1e-15).isNull());
    CHECK(near(f.manhattanLength(), 4.0));
    CHECK(f.transposed() == QPointF(-2.5, 1.5));
    CHECK(f + QPointF(0.5, 0.5) == QPointF(2.0, -2.0));
    CHECK(f - QPointF(0.5, 0.5) == QPointF(1.0, -3.0));
    CHECK(f * 2 == QPointF(3.0, -5.0) && 2 * f == QPointF(3.0, -5.0));
    CHECK(f / 2 == QPointF(0.75, -1.25));
    CHECK(-f == QPointF(-1.5, 2.5));
    CHECK(f.toPoint() == QPoint(2, -2) || f.toPoint() == QPoint(2, -3));
    CHECK(QPointF(1.2, 2.7).toPoint() == QPoint(1, 3));
    CHECK(QPointF(0.1 + 0.2, 1) == QPointF(0.3, 1));
    CHECK(QPointF(0, 1e-15) == QPointF(0, 0));
    CHECK(QPointF(1, 2) != QPointF(1, 2.1));
    CHECK(near(QPointF::dotProduct(QPointF(1, 2), QPointF(3, 4)), 11));
    f += QPointF(1, 1);
    f *= 2;
    f /= 4;
    CHECK(f == QPointF(1.25, -0.75));
    CHECK(QPointF(QPoint(3, 4)) == QPointF(3, 4));
}

static void testSize() {
    CHECK(!QSize().isValid() && QSize().width() == -1 && QSize().height() == -1);
    CHECK(QSize(0, 0).isValid() && QSize(0, 0).isNull() && QSize(0, 0).isEmpty());
    CHECK(QSize(0, 5).isEmpty() && QSize(0, 5).isValid() && !QSize(0, 5).isNull());
    CHECK(!QSize(1, 1).isEmpty());
    CHECK(!QSize(-1, 5).isValid() && QSize(-1, 5).isEmpty());
    QSize s(100, 50);
    CHECK(s.transposed() == QSize(50, 100));
    s.transpose();
    CHECK(s == QSize(50, 100));
    s.setWidth(10);
    s.setHeight(20);
    CHECK(s == QSize(10, 20));
    s.rwidth() += 1;
    s.rheight() += 2;
    CHECK(s == QSize(11, 22));
    CHECK(QSize(10, 50).expandedTo(QSize(20, 30)) == QSize(20, 50));
    CHECK(QSize(10, 50).boundedTo(QSize(20, 30)) == QSize(10, 30));
    CHECK(QSize(1, 2) + QSize(3, 4) == QSize(4, 6));
    CHECK(QSize(5, 5) - QSize(3, 4) == QSize(2, 1));
    CHECK(QSize(3, 5) * 1.5 == QSize(5, 8) && 1.5 * QSize(3, 5) == QSize(5, 8));
    CHECK(QSize(10, 20) / 4.0 == QSize(3, 5));
    QSize t(1, 1);
    t += QSize(1, 2);
    t -= QSize(0, 1);
    t *= 2.0;
    CHECK(t == QSize(4, 4));

    const QSize base(100, 50);
    CHECK(base.scaled(200, 200, Qt::KeepAspectRatio) == QSize(200, 100));
    CHECK(base.scaled(200, 200, Qt::KeepAspectRatioByExpanding) == QSize(400, 200));
    CHECK(base.scaled(200, 200, Qt::IgnoreAspectRatio) == QSize(200, 200));
    CHECK(base.scaled(QSize(30, 30), Qt::KeepAspectRatio) == QSize(30, 15));
    CHECK(base.scaled(QSize(30, 30), Qt::KeepAspectRatioByExpanding) == QSize(60, 30));
    CHECK(base.scaled(100, 50, Qt::KeepAspectRatio) == QSize(100, 50));
    CHECK(base.scaled(50, 100, Qt::KeepAspectRatio) == QSize(50, 25));
    CHECK(base.scaled(50, 100, Qt::KeepAspectRatioByExpanding) == QSize(200, 100));
    CHECK(QSize(50, 100).scaled(30, 30, Qt::KeepAspectRatio) == QSize(15, 30));
    CHECK(QSize(0, 10).scaled(30, 30, Qt::KeepAspectRatio) == QSize(30, 30));
    CHECK(QSize(10, 0).scaled(30, 30, Qt::KeepAspectRatioByExpanding) == QSize(30, 30));
    CHECK(QSize(3, 7).scaled(1000000, 1000000, Qt::KeepAspectRatio) == QSize(428571, 1000000));
    CHECK(QSize(100000, 100000).scaled(2000000000, 2000000000, Qt::KeepAspectRatio) == QSize(2000000000, 2000000000));
    QSize sc(100, 50);
    sc.scale(10, 10, Qt::KeepAspectRatio);
    CHECK(sc == QSize(10, 5));
    sc.scale(QSize(40, 40), Qt::KeepAspectRatioByExpanding);
    CHECK(sc == QSize(80, 40));

    CHECK(QSizeF().width() == -1.0 && !QSizeF().isValid());
    CHECK(QSizeF(0, 0).isNull() && QSizeF(0, 0).isEmpty() && QSizeF(0, 0).isValid());
    CHECK(QSizeF(0.5, 0.5).isValid() && !QSizeF(0.5, 0.5).isEmpty());
    CHECK(QSizeF(1.5, 2.5).transposed() == QSizeF(2.5, 1.5));
    CHECK(QSizeF(1.5, 2.5).expandedTo(QSizeF(2, 1)) == QSizeF(2, 2.5));
    CHECK(QSizeF(1.5, 2.5).boundedTo(QSizeF(2, 1)) == QSizeF(1.5, 1));
    CHECK(QSizeF(1.5, 2.5) * 2 == QSizeF(3, 5) && 2 * QSizeF(1.5, 2.5) == QSizeF(3, 5));
    CHECK(QSizeF(1.5, 2.5) / 2 == QSizeF(0.75, 1.25));
    CHECK(QSizeF(1, 2) + QSizeF(3, 4) == QSizeF(4, 6));
    CHECK(QSizeF(1, 2) - QSizeF(3, 4) == QSizeF(-2, -2));
    CHECK(QSizeF(100, 50).scaled(QSizeF(30, 30), Qt::KeepAspectRatio) == QSizeF(30, 15));
    CHECK(QSizeF(100, 50).scaled(QSizeF(30, 30), Qt::KeepAspectRatioByExpanding) == QSizeF(60, 30));
    CHECK(QSizeF(100, 50).scaled(QSizeF(30, 30), Qt::IgnoreAspectRatio) == QSizeF(30, 30));
    CHECK(QSizeF(1, 3).scaled(QSizeF(1, 1), Qt::KeepAspectRatio) == QSizeF(1.0 / 3, 1));
    CHECK(QSizeF(2.4, 3.6).toSize() == QSize(2, 4));
    CHECK(QSize(3, 4).toSizeF() == QSizeF(3, 4));
    CHECK(QSizeF(QSize(3, 4)) == QSizeF(3, 4));
    QSizeF sf(1, 2);
    sf += QSizeF(1, 1);
    sf *= 2;
    sf /= 4;
    sf -= QSizeF(0.5, 0.5);
    CHECK(sf == QSizeF(0.5, 1));
    sf.transpose();
    sf.scale(QSizeF(4, 4), Qt::KeepAspectRatio);
    CHECK(sf == QSizeF(4, 2));
}

static void testLine() {
    QLine l(1, 2, 4, 6);
    CHECK(l.dx() == 3 && l.dy() == 4);
    CHECK(l.p1() == QPoint(1, 2) && l.p2() == QPoint(4, 6));
    CHECK(l.x1() == 1 && l.y1() == 2 && l.x2() == 4 && l.y2() == 6);
    CHECK(!l.isNull() && QLine().isNull() && QLine(1, 1, 1, 1).isNull());
    CHECK(QLine(0, 0, 5, 5).center() == QPoint(2, 2));
    CHECK(QLine(0, 0, -5, -5).center() == QPoint(-2, -2));
    CHECK(l.translated(10, 20) == QLine(11, 22, 14, 26));
    CHECK(l.translated(QPoint(1, 1)) == QLine(2, 3, 5, 7));
    l.translate(1, 1);
    CHECK(l == QLine(QPoint(2, 3), QPoint(5, 7)));
    l.setP1(QPoint(0, 0));
    l.setP2(QPoint(3, 4));
    CHECK(l == QLine(0, 0, 3, 4) && l != QLine(0, 0, 3, 5));
    l.setLine(1, 1, 2, 2);
    CHECK(l == QLine(1, 1, 2, 2));
    l.setPoints(QPoint(5, 5), QPoint(6, 6));
    CHECK(l.p1() == QPoint(5, 5));
    CHECK(QLine(0, 0, 2000000000, 2000000000).center() == QPoint(1000000000, 1000000000));

    QLineF f(0, 0, 3, 4);
    CHECK(near(f.length(), 5.0));
    CHECK(near(f.dx(), 3) && near(f.dy(), 4));
    CHECK(f.center() == QPointF(1.5, 2));
    CHECK(near(QLineF(0, 0, 1, 0).angle(), 0.0));
    CHECK(near(QLineF(0, 0, 0, -1).angle(), 90.0));
    CHECK(near(QLineF(0, 0, -1, 0).angle(), 180.0));
    CHECK(near(QLineF(0, 0, 0, 1).angle(), 270.0));
    CHECK(near(QLineF(0, 0, 1, -1).angle(), 45.0));
    CHECK(near(QLineF(0, 0, 1, 1).angle(), 315.0));
    CHECK(near(QLineF(0, 0, -1, -1).angle(), 135.0));
    CHECK(near(QLineF(0, 0, 0, 0).angle(), 0.0));
    QLineF a(0, 0, 1, 0);
    a.setAngle(90);
    CHECK(near(a.x2(), 0, 1e-12) && near(a.y2(), -1, 1e-12));
    a.setLength(5);
    CHECK(near(a.length(), 5) && near(a.angle(), 90));
    CHECK(near(QLineF(0, 0, 1, 0).angleTo(QLineF(0, 0, 0, -1)), 90));
    CHECK(near(QLineF(0, 0, 0, -1).angleTo(QLineF(0, 0, 1, 0)), 270));
    CHECK(near(QLineF(0, 0, 1, 0).angleTo(QLineF(5, 5, 6, 5)), 0));
    CHECK(near(QLineF(0, 0, 0, 0).angleTo(QLineF(0, 0, 1, 0)), 0));
    const QLineF pol = QLineF::fromPolar(2, 90);
    CHECK(near(pol.x2(), 0, 1e-12) && near(pol.y2(), -2, 1e-12));
    CHECK(near(QLineF::fromPolar(3, 0).x2(), 3));
    CHECK(near(QLineF::fromPolar(1, 45).angle(), 45));
    const QLineF unit = f.unitVector();
    CHECK(near(unit.length(), 1) && near(unit.dx(), 0.6) && near(unit.dy(), 0.8));
    CHECK(unit.p1() == f.p1());
    const QLineF zero(2, 2, 2, 2);
    const QLineF zu = zero.unitVector();
    CHECK(std::isfinite(zu.x2()) && std::isfinite(zu.y2()));
    CHECK(zu == zero);
    const QLineF nv = QLineF(0, 0, 3, 0).normalVector();
    CHECK(nv.p1() == QPointF(0, 0) && nv.p2() == QPointF(0, -3));
    CHECK(near(QLineF(0, 0, 3, 0).normalVector().angleTo(QLineF(0, 0, 3, 0)), 270));
    CHECK(QLineF(1, 1, 3, 5).pointAt(0.5) == QPointF(2, 3));
    CHECK(QLineF(1, 1, 3, 5).pointAt(0) == QPointF(1, 1));
    CHECK(QLineF(1, 1, 3, 5).pointAt(1) == QPointF(3, 5));
    CHECK(QLineF(1, 1, 3, 5).pointAt(2) == QPointF(5, 9));
    CHECK(QLineF(1, 1, 3, 5).pointAt(-1) == QPointF(-1, -3));

    QPointF ip;
    CHECK(QLineF(0, 0, 10, 10).intersects(QLineF(0, 10, 10, 0), &ip) == QLineF::BoundedIntersection);
    CHECK(ip == QPointF(5, 5));
    CHECK(QLineF(0, 0, 1, 1).intersects(QLineF(0, 10, 10, 0), &ip) == QLineF::UnboundedIntersection);
    CHECK(ip == QPointF(5, 5));
    CHECK(QLineF(0, 0, 10, 0).intersects(QLineF(0, 1, 10, 1), &ip) == QLineF::NoIntersection);
    CHECK(QLineF(0, 0, 10, 0).intersects(QLineF(20, 0, 30, 0)) == QLineF::NoIntersection);
    CHECK(QLineF(0, 0, 10, 10).intersects(QLineF(0, 0, 10, 0), &ip) == QLineF::BoundedIntersection);
    CHECK(ip == QPointF(0, 0));
    CHECK(QLineF(0, 0, 10, 0).intersects(QLineF(10, -5, 10, 5), &ip) == QLineF::BoundedIntersection);
    CHECK(ip == QPointF(10, 0));
    CHECK(QLineF(0, 0, 10, 0).intersects(QLineF(11, -5, 11, 5), &ip) == QLineF::UnboundedIntersection);
    CHECK(ip == QPointF(11, 0));
    CHECK(QLineF(0, 0, 4, 2).intersects(QLineF(1, 5, 3, -5)) == QLineF::BoundedIntersection);
    CHECK(QLineF(0, 0, 10, 10).intersects(QLineF(0, 10, 4, 6)) == QLineF::UnboundedIntersection);
    CHECK(QLineF(0, 0, 0, 0).intersects(QLineF(0, 10, 4, 6)) == QLineF::NoIntersection);

    QLineF t(0, 0, 1, 1);
    t.translate(1, 2);
    CHECK(t == QLineF(1, 2, 2, 3));
    CHECK(t.translated(QPointF(1, 1)) == QLineF(2, 3, 3, 4));
    CHECK(t.toLine() == QLine(1, 2, 2, 3));
    CHECK(QLineF(QLine(1, 2, 3, 4)) == QLineF(1, 2, 3, 4));
    CHECK(QLineF().isNull() && !t.isNull());
    t.setP1(QPointF(0, 0));
    t.setP2(QPointF(1, 0));
    CHECK(t == QLineF(0, 0, 1, 0) && t != QLineF(0, 0, 2, 0));
    t.setLine(1, 2, 3, 4);
    CHECK(t.p1() == QPointF(1, 2) && t.p2() == QPointF(3, 4));
}

static void testMargins() {
    QMargins m(1, 2, 3, 4);
    CHECK(m.left() == 1 && m.top() == 2 && m.right() == 3 && m.bottom() == 4);
    CHECK(!m.isNull() && QMargins().isNull());
    CHECK(m + QMargins(1, 1, 1, 1) == QMargins(2, 3, 4, 5));
    CHECK(m - QMargins(1, 1, 1, 1) == QMargins(0, 1, 2, 3));
    CHECK(m + 1 == QMargins(2, 3, 4, 5) && 1 + m == QMargins(2, 3, 4, 5));
    CHECK(m - 1 == QMargins(0, 1, 2, 3));
    CHECK(m * 2 == QMargins(2, 4, 6, 8) && 2 * m == QMargins(2, 4, 6, 8));
    CHECK(QMargins(2, 4, 6, 8) / 2 == m);
    CHECK(m * 1.5 == QMargins(2, 3, 5, 6));
    CHECK(QMargins(2, 4, 6, 8) / 2.0 == m);
    CHECK(-m == QMargins(-1, -2, -3, -4) && +m == m);
    m.setLeft(10);
    m.setTop(20);
    m.setRight(30);
    m.setBottom(40);
    CHECK(m == QMargins(10, 20, 30, 40) && m != QMargins());
    m += QMargins(1, 1, 1, 1);
    m -= 1;
    m *= 2;
    m /= 2;
    CHECK(m == QMargins(10, 20, 30, 40));
    m *= 0.5;
    CHECK(m == QMargins(5, 10, 15, 20));

    QMarginsF f(0.5, 1.5, 2.5, 3.5);
    CHECK(near(f.left(), 0.5) && near(f.bottom(), 3.5));
    CHECK(f + QMarginsF(1, 1, 1, 1) == QMarginsF(1.5, 2.5, 3.5, 4.5));
    CHECK(f - QMarginsF(0.5, 0.5, 0.5, 0.5) == QMarginsF(0, 1, 2, 3));
    CHECK(f * 2 == QMarginsF(1, 3, 5, 7) && 2 * f == QMarginsF(1, 3, 5, 7));
    CHECK(f / 2 == QMarginsF(0.25, 0.75, 1.25, 1.75));
    CHECK(f + 1.0 == QMarginsF(1.5, 2.5, 3.5, 4.5));
    CHECK(f - 0.5 == QMarginsF(0, 1, 2, 3));
    CHECK(-f == QMarginsF(-0.5, -1.5, -2.5, -3.5));
    CHECK(QMarginsF().isNull() && !f.isNull());
    CHECK(f.toMargins() == QMargins(1, 2, 3, 4) || f.toMargins() == QMargins(0, 2, 3, 4));
    CHECK(QMarginsF(QMargins(1, 2, 3, 4)) == QMarginsF(1, 2, 3, 4));
}

static void testRect() {
    QRect r(10, 20, 30, 40);
    CHECK(r.left() == 10 && r.top() == 20 && r.right() == 39 && r.bottom() == 59);
    CHECK(r.x() == 10 && r.y() == 20 && r.width() == 30 && r.height() == 40);
    CHECK(r.size() == QSize(30, 40));
    CHECK(r.topLeft() == QPoint(10, 20) && r.bottomRight() == QPoint(39, 59));
    CHECK(r.topRight() == QPoint(39, 20) && r.bottomLeft() == QPoint(10, 59));
    CHECK(r.center() == QPoint(24, 39));
    CHECK(QRect(0, 0, 10, 10).center() == QPoint(4, 4));
    CHECK(QRect(-10, -10, 5, 5).center() == QPoint(-8, -8));
    CHECK(QRect(QPoint(1, 2), QSize(3, 4)) == QRect(1, 2, 3, 4));
    CHECK(QRect(QPoint(1, 2), QPoint(3, 5)) == QRect(1, 2, 3, 4));
    CHECK(QRect(QPoint(1, 2), QPoint(3, 5)).bottomRight() == QPoint(3, 5));

    CHECK(QRect().isNull() && QRect().isEmpty() && !QRect().isValid());
    CHECK(QRect(0, 0, 0, 0).isNull());
    CHECK(QRect(5, 5, 0, 0).isNull());
    CHECK(!QRect(0, 0, 0, 5).isNull() && QRect(0, 0, 0, 5).isEmpty() && !QRect(0, 0, 0, 5).isValid());
    CHECK(QRect(0, 0, 1, 1).isValid() && !QRect(0, 0, 1, 1).isEmpty());
    CHECK(QRect(0, 0, 1, 1).right() == 0 && QRect(0, 0, 1, 1).bottom() == 0);
    CHECK(QRect(0, 0, 1, 1).center() == QPoint(0, 0));
    CHECK(!QRect(0, 0, -5, 5).isValid());

    CHECK(r.contains(QPoint(10, 20)) && r.contains(QPoint(39, 59)));
    CHECK(!r.contains(QPoint(40, 59)) && !r.contains(QPoint(39, 60)) && !r.contains(QPoint(9, 20)));
    CHECK(!r.contains(QPoint(10, 20), true) && r.contains(QPoint(11, 21), true) && !r.contains(QPoint(39, 30), true));
    CHECK(r.contains(11, 21) && !r.contains(0, 0));
    CHECK(r.contains(QRect(10, 20, 30, 40)) && !r.contains(QRect(10, 20, 30, 40), true));
    CHECK(r.contains(QRect(11, 21, 28, 38), true));
    CHECK(r.contains(QRect(15, 25, 5, 5)));
    CHECK(!r.contains(QRect(15, 25, 50, 5)));
    CHECK(!r.contains(QRect()));
    CHECK(!QRect().contains(QPoint(0, 0)));
    CHECK(!QRect(0, 0, 0, 5).contains(QPoint(0, 1)));

    CHECK(QRect(QPoint(10, 10), QPoint(0, 0)).normalized() == QRect(QPoint(0, 0), QPoint(10, 10)));
    CHECK(QRect(QPoint(10, 10), QPoint(0, 0)).normalized().width() == 11);
    CHECK(QRect(QPoint(10, 0), QPoint(0, 10)).normalized() == QRect(QPoint(0, 0), QPoint(10, 10)));
    CHECK(QRect(5, 5, 10, 10).normalized() == QRect(5, 5, 10, 10));
    CHECK(QRect(5, 5, 0, 0).normalized() == QRect(5, 5, 0, 0));
    CHECK(QRect(QPoint(10, 10), QPoint(0, 0)).contains(QPoint(5, 5)));

    CHECK(r.adjusted(1, 2, -3, -4) == QRect(QPoint(11, 22), QPoint(36, 55)));
    QRect adj = r;
    adj.adjust(-1, -1, 1, 1);
    CHECK(adj == QRect(9, 19, 32, 42));
    CHECK(r.marginsAdded(QMargins(1, 2, 3, 4)) == QRect(QPoint(9, 18), QPoint(42, 63)));
    CHECK(r.marginsRemoved(QMargins(1, 2, 3, 4)) == QRect(QPoint(11, 22), QPoint(36, 55)));
    CHECK(r + QMargins(1, 1, 1, 1) == QRect(9, 19, 32, 42));
    CHECK(QMargins(1, 1, 1, 1) + r == QRect(9, 19, 32, 42));
    CHECK(r - QMargins(1, 1, 1, 1) == QRect(11, 21, 28, 38));
    QRect mrect = r;
    mrect += QMargins(1, 1, 1, 1);
    mrect -= QMargins(1, 1, 1, 1);
    CHECK(mrect == r);

    CHECK((QRect(0, 0, 10, 10) & QRect(5, 5, 10, 10)) == QRect(5, 5, 5, 5));
    CHECK(QRect(0, 0, 10, 10).intersected(QRect(5, 5, 10, 10)).bottomRight() == QPoint(9, 9));
    CHECK((QRect(0, 0, 10, 10) & QRect(10, 0, 10, 10)).isNull());
    CHECK((QRect(0, 0, 10, 10) & QRect(20, 20, 5, 5)) == QRect());
    CHECK(!QRect(0, 0, 10, 10).intersects(QRect(10, 0, 10, 10)));
    CHECK(QRect(0, 0, 10, 10).intersects(QRect(9, 9, 10, 10)));
    CHECK(!QRect(0, 0, 10, 10).intersects(QRect()));
    CHECK((QRect(0, 0, 10, 10) & QRect(2, 2, 3, 3)) == QRect(2, 2, 3, 3));
    CHECK((QRect(0, 0, 10, 10) | QRect(20, 20, 5, 5)) == QRect(0, 0, 25, 25));
    CHECK(QRect(0, 0, 10, 10).united(QRect()) == QRect(0, 0, 10, 10));
    CHECK(QRect().united(QRect(1, 2, 3, 4)) == QRect(1, 2, 3, 4));
    CHECK(QRect(0, 0, 10, 10).united(QRect(-5, -5, 5, 5)) == QRect(-5, -5, 15, 15));
    QRect acc(0, 0, 2, 2);
    acc |= QRect(5, 5, 2, 2);
    CHECK(acc == QRect(0, 0, 7, 7));
    acc &= QRect(3, 3, 10, 10);
    CHECK(acc == QRect(3, 3, 4, 4));

    QRect m = r;
    m.translate(5, -5);
    CHECK(m == QRect(15, 15, 30, 40));
    CHECK(r.translated(QPoint(1, 1)) == QRect(11, 21, 30, 40));
    CHECK(r.translated(-10, -20).topLeft() == QPoint(0, 0));
    m.moveTo(0, 0);
    CHECK(m == QRect(0, 0, 30, 40));
    m.moveTo(QPoint(3, 4));
    CHECK(m == QRect(3, 4, 30, 40));
    m.moveLeft(0);
    m.moveTop(0);
    CHECK(m == QRect(0, 0, 30, 40));
    m.moveRight(99);
    m.moveBottom(99);
    CHECK(m == QRect(70, 60, 30, 40) && m.right() == 99 && m.bottom() == 99);
    m.moveTopLeft(QPoint(0, 0));
    m.moveBottomRight(QPoint(49, 59));
    CHECK(m == QRect(20, 20, 30, 40));
    m.moveTopRight(QPoint(29, 0));
    CHECK(m == QRect(0, 0, 30, 40));
    m.moveBottomLeft(QPoint(5, 39));
    CHECK(m == QRect(5, 0, 30, 40));
    m.moveCenter(QPoint(50, 50));
    CHECK(m.center() == QPoint(50, 50) && m.size() == QSize(30, 40));
    m.setWidth(10);
    m.setHeight(20);
    CHECK(m.size() == QSize(10, 20));
    m.setSize(QSize(7, 8));
    CHECK(m.size() == QSize(7, 8));
    m.setRect(1, 2, 3, 4);
    CHECK(m == QRect(1, 2, 3, 4));
    m.setCoords(0, 0, 9, 9);
    CHECK(m == QRect(0, 0, 10, 10));
    m.setLeft(5);
    m.setTop(5);
    CHECK(m == QRect(QPoint(5, 5), QPoint(9, 9)));
    m.setRight(19);
    m.setBottom(19);
    CHECK(m == QRect(5, 5, 15, 15));
    m.setTopLeft(QPoint(0, 0));
    m.setBottomRight(QPoint(1, 1));
    CHECK(m == QRect(0, 0, 2, 2));
    m.setTopRight(QPoint(3, 1));
    m.setBottomLeft(QPoint(1, 3));
    CHECK(m == QRect(QPoint(1, 1), QPoint(3, 3)));
    m.setX(0);
    m.setY(0);
    CHECK(m.topLeft() == QPoint(0, 0));
    int x, y, w, h;
    r.getRect(&x, &y, &w, &h);
    CHECK(x == 10 && y == 20 && w == 30 && h == 40);
    r.getCoords(&x, &y, &w, &h);
    CHECK(x == 10 && y == 20 && w == 39 && h == 59);
    CHECK(r.transposed() == QRect(10, 20, 40, 30));
    CHECK(QRect(1, 2, 3, 4) != QRect(1, 2, 3, 5));
}

static void testRectF() {
    QRectF r(10, 20, 30, 40);
    CHECK(near(r.right(), 40) && near(r.bottom(), 60)); // no off-by-one for QRectF
    CHECK(r.topLeft() == QPointF(10, 20) && r.bottomRight() == QPointF(40, 60));
    CHECK(r.topRight() == QPointF(40, 20) && r.bottomLeft() == QPointF(10, 60));
    CHECK(r.center() == QPointF(25, 40));
    CHECK(r.size() == QSizeF(30, 40));
    CHECK(QRectF(QPointF(1, 2), QSizeF(3, 4)) == QRectF(1, 2, 3, 4));
    CHECK(QRectF(QPointF(1, 2), QPointF(4, 6)) == QRectF(1, 2, 3, 4));
    CHECK(QRectF(QRect(1, 2, 3, 4)) == QRectF(1, 2, 3, 4));
    CHECK(QRectF().isNull() && QRectF().isEmpty() && !QRectF().isValid());
    CHECK(!QRectF(0, 0, 0, 5).isNull() && QRectF(0, 0, 0, 5).isEmpty());
    CHECK(QRectF(0, 0, 0.1, 0.1).isValid());
    CHECK(QRectF(0, 0, -1, 5).isEmpty());

    CHECK(r.contains(QPointF(10, 20)) && r.contains(QPointF(40, 60)) && r.contains(25, 40));
    CHECK(!r.contains(QPointF(40.1, 60)) && !r.contains(QPointF(9.9, 20)));
    CHECK(!QRectF(0, 0, 0, 5).contains(QPointF(0, 1)));
    CHECK(!QRectF(0, 0, 5, 0).contains(QPointF(1, 0)));
    CHECK(!QRectF().contains(QPointF(0, 0)));
    CHECK(QRectF(10, 10, -10, -10).contains(QPointF(5, 5)));
    CHECK(r.contains(QRectF(10, 20, 30, 40)) && r.contains(QRectF(15, 25, 5, 5)));
    CHECK(!r.contains(QRectF(15, 25, 50, 5)) && !r.contains(QRectF()));
    CHECK(!r.contains(QRectF(15, 25, 0, 5)));
    CHECK(!QRectF(0, 0, 0, 5).contains(QRectF(0, 0, 0, 5)));

    CHECK(QRectF(10, 10, -10, -5).normalized() == QRectF(0, 5, 10, 5));
    CHECK(r.normalized() == r);
    CHECK(r.adjusted(1, 2, -3, -4) == QRectF(11, 22, 26, 34));
    QRectF a = r;
    a.adjust(-1, -1, 1, 1);
    CHECK(a == QRectF(9, 19, 32, 42));
    CHECK(r.marginsAdded(QMarginsF(1, 2, 3, 4)) == QRectF(9, 18, 34, 46));
    CHECK(r.marginsRemoved(QMarginsF(1, 2, 3, 4)) == QRectF(11, 22, 26, 34));
    CHECK(r + QMarginsF(1, 1, 1, 1) == QRectF(9, 19, 32, 42));
    CHECK(QMarginsF(1, 1, 1, 1) + r == QRectF(9, 19, 32, 42));
    CHECK(r - QMarginsF(1, 1, 1, 1) == QRectF(11, 21, 28, 38));
    QRectF mr = r;
    mr += QMarginsF(1, 1, 1, 1);
    mr -= QMarginsF(1, 1, 1, 1);
    CHECK(mr == r);

    CHECK((QRectF(0, 0, 10, 10) & QRectF(5, 5, 10, 10)) == QRectF(5, 5, 5, 5));
    CHECK((QRectF(0, 0, 10, 10) & QRectF(10, 0, 10, 10)).isNull());
    CHECK(!QRectF(0, 0, 10, 10).intersects(QRectF(10, 0, 10, 10)));
    CHECK(QRectF(0, 0, 10, 10).intersects(QRectF(9.5, 9.5, 10, 10)));
    CHECK(!QRectF(0, 0, 10, 10).intersects(QRectF(2, 2, 0, 5)));
    CHECK((QRectF(0, 0, 10, 10) & QRectF(2, 2, 0, 5)).isNull());
    CHECK((QRectF(0, 0, 10, 10) | QRectF(20, 20, 5, 5)) == QRectF(0, 0, 25, 25));
    CHECK(QRectF(0, 0, 10, 10).united(QRectF()) == QRectF(0, 0, 10, 10));
    CHECK(QRectF().united(QRectF(1, 2, 3, 4)) == QRectF(1, 2, 3, 4));
    QRectF acc(0, 0, 2, 2);
    acc |= QRectF(5, 5, 2, 2);
    acc &= QRectF(3, 3, 10, 10);
    CHECK(acc == QRectF(3, 3, 4, 4));

    QRectF m = r;
    m.translate(5, -5);
    CHECK(m == QRectF(15, 15, 30, 40));
    CHECK(r.translated(QPointF(1, 1)) == QRectF(11, 21, 30, 40));
    m.moveTo(0.5, 0.5);
    CHECK(m == QRectF(0.5, 0.5, 30, 40));
    m.moveTo(QPointF(1, 1));
    m.moveLeft(2);
    m.moveTop(3);
    CHECK(m == QRectF(2, 3, 30, 40));
    m.moveRight(100);
    m.moveBottom(100);
    CHECK(m == QRectF(70, 60, 30, 40));
    m.moveCenter(QPointF(0, 0));
    CHECK(m == QRectF(-15, -20, 30, 40));
    m.setLeft(-10);
    CHECK(m == QRectF(-10, -20, 25, 40));
    m.setTop(-10);
    CHECK(m == QRectF(-10, -10, 25, 30));
    m.setRight(0);
    m.setBottom(0);
    CHECK(m == QRectF(-10, -10, 10, 10));
    m.setWidth(3);
    m.setHeight(4);
    m.setSize(QSizeF(5, 6));
    CHECK(m == QRectF(-10, -10, 5, 6));
    m.setRect(1, 2, 3, 4);
    CHECK(m == QRectF(1, 2, 3, 4));
    m.setCoords(0, 0, 9, 9);
    CHECK(m == QRectF(0, 0, 9, 9));
    m.setX(2);
    m.setY(2);
    CHECK(m == QRectF(2, 2, 7, 7));
    CHECK(r.transposed() == QRectF(10, 20, 40, 30));

    CHECK(QRectF(1.2, 2.3, 3.4, 4.5).toAlignedRect() == QRect(1, 2, 4, 5));
    CHECK(QRectF(1.0, 2.0, 3.0, 4.0).toAlignedRect() == QRect(1, 2, 3, 4));
    CHECK(QRectF(-1.5, -1.5, 1.0, 1.0).toAlignedRect() == QRect(-2, -2, 2, 2));
    CHECK(QRectF(0.2, 0.2, 0.1, 0.1).toAlignedRect() == QRect(0, 0, 1, 1));
    CHECK(QRectF(1.2, 2.3, 3.4, 4.5).toRect() == QRect(1, 2, 4, 5));
    CHECK(QRectF(1.4, 2.4, 3.0, 4.0).toRect() == QRect(1, 2, 3, 4));
    CHECK(QRectF(10.5, 10.5, 2, 2).toRect().size() == QSize(2, 2));
    CHECK(QRectF(0.1 + 0.2, 0, 1, 1) == QRectF(0.3, 0, 1, 1));
}

// Views

static void testStringView() {
    CHECK(QStringView().isNull() && QStringView().isEmpty() && QStringView().size() == 0);
    CHECK(QStringView(nullptr).isNull());
    CHECK(QStringView(u"").isEmpty() && !QStringView(u"").isNull());
    const QString s("Hello, World");
    QStringView v(s);
    CHECK(v.size() == 12 && v.length() == 12 && !v.isEmpty() && !v.isNull());
    CHECK(v.toString() == s);
    CHECK(v[0] == QChar(u'H') && v.at(4) == QChar(u'o') && v.front() == QChar(u'H') && v.back() == QChar(u'd'));
    CHECK(v.first() == QChar(u'H') && v.last() == QChar(u'd'));
    CHECK(v.data() == s.constData());
    CHECK(v == QStringView(u"Hello, World"));
    CHECK(v != QStringView(u"Hello, world"));
    CHECK(QStringView(u"abc") < QStringView(u"abd"));
    CHECK(QStringView(u"ab") < QStringView(u"abc"));
    CHECK(!(QStringView(u"abc") < QStringView(u"abc")));
    CHECK(v.mid(7) == u"World");
    CHECK(v.mid(0, 5) == u"Hello");
    CHECK(v.mid(7, 100) == u"World");
    CHECK(v.mid(100).isEmpty());
    CHECK(v.mid(12).isEmpty());
    CHECK(v.mid(-3, 5) == u"He");
    CHECK(v.left(5) == u"Hello" && v.left(100) == v && v.left(0).isEmpty());
    CHECK(v.right(5) == u"World" && v.right(100) == v && v.right(0).isEmpty());
    CHECK(v.first(5) == u"Hello" && v.last(5) == u"World");
    CHECK(v.sliced(7) == u"World" && v.sliced(7, 3) == u"Wor");
    CHECK(v.chopped(7) == u"Hello");
    QStringView t = v;
    t.truncate(5);
    CHECK(t == u"Hello");
    t.chop(2);
    CHECK(t == u"Hel");
    CHECK(QStringView(u"  \t padded \n ").trimmed() == u"padded");
    CHECK(QStringView(u"   ").trimmed().isEmpty());
    CHECK(v.startsWith(QStringView(u"Hello")) && !v.startsWith(QStringView(u"hello")));
    CHECK(v.startsWith(QStringView(u"hello"), Qt::CaseInsensitive));
    CHECK(v.endsWith(QStringView(u"World")) && v.endsWith(QStringView(u"WORLD"), Qt::CaseInsensitive));
    CHECK(v.indexOf(QStringView(u"o")) == 4 && v.indexOf(QStringView(u"o"), 5) == 8 && v.indexOf(QStringView(u"xyz")) == -1);
    CHECK(v.lastIndexOf(QStringView(u"o")) == 8);
    CHECK(v.contains(QStringView(u"lo, W")) && !v.contains(QStringView(u"LO"))
          && v.contains(QStringView(u"LO"), Qt::CaseInsensitive));
    CHECK(QStringView(u"42").toInt() == 42);
    bool ok = false;
    CHECK(QStringView(u"ff").toInt(&ok, 16) == 255 && ok);
    QStringView(u"zz").toInt(&ok);
    CHECK(!ok);
    CHECK(near(QStringView(u"2.5").toDouble(), 2.5));
    QString collected;
    for (QChar c : QStringView(u"abc"))
        collected.append(c);
    CHECK(collected == "abc");
    CHECK(QStringView(u"abc").toUtf8() == QByteArray("abc"));
    CHECK(QStringView(QString("h\xc3\xa9llo")).size() == 5);
    CHECK(QStringView(std::u16string_view(u"xyz")).size() == 3);
    CHECK(QStringView(QString()).isNull());
    CHECK(QStringView(u"abc").toString() == "abc");
    CHECK(QStringView().toString().isEmpty());
}

static void testByteArrayView() {
    CHECK(QByteArrayView().isNull() && QByteArrayView().isEmpty());
    CHECK(QByteArrayView("").isEmpty() && !QByteArrayView("").isNull());
    CHECK(QByteArrayView("abc").size() == 3);
    const QByteArray b("Hello, World");
    QByteArrayView v(b);
    CHECK(v.size() == 12 && v.length() == 12 && v.data() == b.constData());
    CHECK(v[0] == 'H' && v.at(4) == 'o' && v.front() == 'H' && v.back() == 'd');
    CHECK(v == QByteArrayView("Hello, World") && v != QByteArrayView("Hello"));
    CHECK(QByteArrayView("abc") < QByteArrayView("abd") && QByteArrayView("ab") < QByteArrayView("abc"));
    CHECK(v.mid(7) == QByteArrayView("World") && v.mid(0, 5) == QByteArrayView("Hello"));
    CHECK(v.mid(100).isEmpty() && v.mid(-3, 5) == QByteArrayView("He"));
    CHECK(v.left(5) == QByteArrayView("Hello") && v.right(5) == QByteArrayView("World"));
    CHECK(v.left(100) == v && v.right(100) == v);
    CHECK(v.first(5) == QByteArrayView("Hello") && v.last(5) == QByteArrayView("World"));
    CHECK(v.sliced(7) == QByteArrayView("World") && v.sliced(7, 3) == QByteArrayView("Wor"));
    CHECK(v.chopped(7) == QByteArrayView("Hello"));
    CHECK(v.startsWith(QByteArrayView("Hello")) && !v.startsWith(QByteArrayView("World")));
    CHECK(v.endsWith(QByteArrayView("World")) && !v.endsWith(QByteArrayView("Hello")));
    CHECK(!QByteArrayView("ab").startsWith(QByteArrayView("abc")) && !QByteArrayView("ab").endsWith(QByteArrayView("abc")));
    CHECK(v.startsWith(QByteArrayView()) && v.endsWith(QByteArrayView()));
    CHECK(v.indexOf(QByteArrayView("o")) == 4 && v.indexOf(QByteArrayView("o"), 5) == 8 && v.indexOf(QByteArrayView("zz")) == -1);
    CHECK(v.contains(QByteArrayView("lo, W")) && !v.contains(QByteArrayView("LO")));
    CHECK(v.toByteArray() == b);
    CHECK(QByteArrayView().toByteArray().isNull());
    int n = 0;
    for (char c : QByteArrayView("abc"))
        n += c;
    CHECK(n == 'a' + 'b' + 'c');
    CHECK(QByteArrayView(std::string_view("xyz")).size() == 3);
    const char raw[] = {'a', '\0', 'b'};
    CHECK(QByteArrayView(raw, 3).size() == 3 && QByteArrayView(raw, 3) == QByteArrayView(QByteArray(raw, 3)));
    CHECK(QByteArrayView(raw, 3) != QByteArrayView(raw, 2));
    CHECK(QByteArrayView(QByteArray()).isNull());
}

static void testSpan() {
    int arr[] = {1, 2, 3, 4, 5};
    QSpan<int> s(arr);
    CHECK(s.size() == 5 && s.size_bytes() == 5 * static_cast<qsizetype>(sizeof(int)) && !s.isEmpty());
    CHECK(s[0] == 1 && s.front() == 1 && s.back() == 5 && s.data() == arr);
    s[1] = 20;
    CHECK(arr[1] == 20);
    int sum = 0;
    for (int v : s)
        sum += v;
    CHECK(sum == 1 + 20 + 3 + 4 + 5);
    CHECK(s.first(2).size() == 2 && s.first(2)[1] == 20);
    CHECK(s.last(2).size() == 2 && s.last(2)[0] == 4);
    CHECK(s.subspan(1).size() == 4 && s.subspan(1, 2).size() == 2 && s.subspan(1, 2)[0] == 20);
    CHECK(s.sliced(3).size() == 2 && s.sliced(1, 3)[2] == 4 && s.chopped(2).size() == 3);
    CHECK(QSpan<int>().isEmpty() && QSpan<int>().data() == nullptr);
    CHECK(QSpan<int>(arr, 0).isEmpty());
    CHECK(QSpan<int>(arr, 3).size() == 3);
    CHECK(QSpan<int>(arr, arr + 4).size() == 4);
    CHECK(QSpan<int>(arr, static_cast<int *>(arr)).size() == 0);
    QSpan<const int> cs = s;
    CHECK(cs.size() == 5 && cs[1] == 20);
    std::array<int, 3> sa = {7, 8, 9};
    QSpan<int> fromArray(sa);
    CHECK(fromArray.size() == 3 && fromArray[2] == 9);
    const std::array<int, 3> csa = {7, 8, 9};
    QSpan<const int> fromConstArray(csa);
    CHECK(fromConstArray.size() == 3);
    QList<int> list = {1, 2, 3, 4};
    QSpan<int> fromList(list);
    CHECK(fromList.size() == 4);
    fromList[0] = 100;
    CHECK(list[0] == 100);
    const QList<int> clist = {5, 6};
    QSpan<const int> fromConstList(clist);
    CHECK(fromConstList.size() == 2 && fromConstList[1] == 6);
    CHECK(*s.rbegin() == 5);
    CHECK(s.cend() - s.cbegin() == 5);
    CHECK(s.count() == 5 && s.length() == 5 && !s.empty());
    QSpan ctad(arr);
    CHECK(ctad.size() == 5);
    const QByteArray ba("xyz");
    QSpan<const char> bs(ba);
    CHECK(bs.size() == 3 && bs[2] == 'z');
}

// Containers

struct Tracked {
    static int alive;
    static int copies;
    int v;
    Tracked(int x = 0) : v(x) { ++alive; }
    Tracked(const Tracked &o) : v(o.v) { ++alive; ++copies; }
    Tracked(Tracked &&o) noexcept : v(o.v) { o.v = -1; ++alive; }
    Tracked &operator=(const Tracked &o) { v = o.v; return *this; }
    Tracked &operator=(Tracked &&o) noexcept { v = o.v; o.v = -1; return *this; }
    ~Tracked() { --alive; }
    bool operator==(const Tracked &o) const { return v == o.v; }
};
int Tracked::alive = 0;
int Tracked::copies = 0;

static void testVarLengthArray() {
    {
        QVarLengthArray<int, 4> a;
        CHECK(a.isEmpty() && a.size() == 0 && a.capacity() == 4);
        const int *inlineData = a.data();
        for (int i = 0; i < 4; ++i)
            a.append(i);
        CHECK(a.size() == 4 && a.data() == inlineData && a.capacity() == 4);
        a.append(4); // spills to the heap
        CHECK(a.size() == 5 && a.data() != inlineData && a.capacity() >= 5);
        for (int i = 0; i < 5; ++i)
            CHECK(a[i] == i);
        for (int i = 5; i < 1000; ++i)
            a.push_back(i);
        CHECK(a.size() == 1000);
        for (int i = 0; i < 1000; ++i)
            CHECK(a.at(i) == i);
        CHECK(a.first() == 0 && a.last() == 999 && a.front() == 0 && a.back() == 999);
        a.squeeze();
        CHECK(a.capacity() == 1000 && a.size() == 1000 && a[999] == 999);
        a.resize(3);
        CHECK(a.size() == 3 && a.capacity() == 1000);
        a.squeeze(); // small enough to move back inline
        CHECK(a.size() == 3 && a.capacity() == 4 && a.data() == inlineData);
        CHECK(a[0] == 0 && a[1] == 1 && a[2] == 2);
        a.resize(6);
        CHECK(a.size() == 6 && a[3] == 0 && a[5] == 0); // value-initialised
        a.resize(8, 7);
        CHECK(a[6] == 7 && a[7] == 7);
        a.clear();
        CHECK(a.isEmpty());
        a.reserve(100);
        CHECK(a.capacity() >= 100 && a.isEmpty());
    }
    {
        QVarLengthArray<int, 8> a = {5, 6, 7};
        CHECK(a.size() == 3 && a[2] == 7);
        a.insert(a.begin() + 1, 100);
        CHECK(a.size() == 4 && a[0] == 5 && a[1] == 100 && a[2] == 6 && a[3] == 7);
        a.insert(0, 1);
        CHECK(a.size() == 5 && a[0] == 1 && a[1] == 5);
        a.prepend(0);
        CHECK(a.size() == 6 && a[0] == 0 && a[1] == 1);
        a.insert(a.end(), 9);
        CHECK(a.last() == 9 && a.size() == 7);
        auto it = a.insert(a.begin() + 2, 55); // crosses the inline capacity
        CHECK(*it == 55 && a.size() == 8 && a[2] == 55);
        auto it2 = a.insert(a.begin(), 44);
        CHECK(*it2 == 44 && a.size() == 9 && a[0] == 44 && a[1] == 0);
        a.remove(0);
        CHECK(a.size() == 8 && a[0] == 0);
        a.remove(1, 2);
        CHECK(a.size() == 6 && a[0] == 0 && a[1] == 5 && a[2] == 100);
        a.erase(a.begin());
        CHECK(a.size() == 5 && a[0] == 5);
        auto e = a.erase(a.begin() + 1, a.begin() + 3);
        CHECK(*e == a[1] && a.size() == 3);
        a.removeLast();
        CHECK(a.size() == 2);
        a.pop_back();
        CHECK(a.size() == 1);
        CHECK(a.indexOf(5) == 0 && a.indexOf(1234) == -1 && a.contains(5) && !a.contains(1234));
        a.append(5);
        a.append(5);
        CHECK(a.lastIndexOf(5) == 2 && a.indexOf(5, 1) == 1 && a.lastIndexOf(5, 1) == 1);
        CHECK(a.value(0) == 5 && a.value(100) == 0 && a.value(100, -1) == -1 && a.value(-1, -2) == -2);
        int buf[] = {1, 2, 3};
        a.append(buf, 3);
        CHECK(a.size() == 6 && a[5] == 3);
        int collected = 0;
        for (int v : a)
            collected += v;
        CHECK(collected == 5 * 3 + 6);
        CHECK(*a.rbegin() == 3 && a.cend() - a.cbegin() == 6 && a.constEnd() - a.constBegin() == 6);
        CHECK(a.count() == 6 && a.length() == 6 && a.constData() == a.data() && !a.empty());
    }
    {
        // appending an element that lives in the array while it reallocates
        QVarLengthArray<int, 2> a;
        a.append(1);
        a.append(2);
        a.append(a[0]);
        CHECK(a.size() == 3 && a[2] == 1);
        for (int i = 0; i < 100; ++i)
            a.append(a[i % a.size()]);
        CHECK(a.size() == 103);
        a.insert(a.begin(), a[1]);
        CHECK(a[0] == 2);
    }
    {
        QVarLengthArray<int, 4> a;
        for (int i = 0; i < 10; ++i)
            a.append(i);
        QVarLengthArray<int, 4> copy(a);
        CHECK(copy == a && copy.data() != a.data());
        copy[0] = 99;
        CHECK(copy != a && a[0] == 0);
        QVarLengthArray<int, 4> moved(std::move(copy));
        CHECK(moved.size() == 10 && moved[0] == 99 && copy.isEmpty());
        QVarLengthArray<int, 4> assigned;
        assigned = a;
        CHECK(assigned == a);
        QVarLengthArray<int, 4> small = {1, 2};
        QVarLengthArray<int, 4> movedSmall(std::move(small));
        CHECK(movedSmall.size() == 2 && movedSmall[1] == 2 && small.isEmpty());
        assigned = std::move(movedSmall);
        CHECK(assigned.size() == 2 && assigned[0] == 1);
        assigned = std::move(moved);
        CHECK(assigned.size() == 10 && assigned[0] == 99);
        assigned = {7, 8, 9};
        CHECK(assigned.size() == 3 && assigned[2] == 9);
        auto &selfRef = assigned;
        assigned = selfRef;
        CHECK(assigned.size() == 3);
        const int src[] = {4, 5, 6, 7, 8, 9};
        QVarLengthArray<int, 2> fromRange(std::begin(src), std::end(src));
        CHECK(fromRange.size() == 6 && fromRange[5] == 9);
        QVarLengthArray<int, 4> sized(5);
        CHECK(sized.size() == 5 && sized[4] == 0);
    }
    {
        QVarLengthArray<std::string, 2> a;
        for (int i = 0; i < 20; ++i)
            a.append(std::string(40, static_cast<char>('a' + i)));
        CHECK(a.size() == 20 && a[19] == std::string(40, 't'));
        a.insert(a.begin() + 3, std::string("mid"));
        CHECK(a[3] == "mid" && a[4] == std::string(40, 'd'));
        QVarLengthArray<std::string, 2> b = a;
        a.clear();
        CHECK(b.size() == 21 && b[0] == std::string(40, 'a'));
        a = std::move(b);
        CHECK(a.size() == 21);
    }
    CHECK(Tracked::alive == 0);
    {
        QVarLengthArray<Tracked, 3> a;
        for (int i = 0; i < 3; ++i)
            a.append(Tracked(i));
        CHECK(Tracked::alive == 3);
        a.append(Tracked(3)); // reallocate: moved, old ones destroyed
        CHECK(Tracked::alive == 4 && a.size() == 4);
        for (int i = 0; i < 4; ++i)
            CHECK(a[i].v == i);
        a.insert(a.begin() + 1, Tracked(50));
        CHECK(Tracked::alive == 5 && a[1].v == 50 && a[2].v == 1 && a[4].v == 3);
        a.remove(0, 2);
        CHECK(Tracked::alive == 3 && a[0].v == 1);
        a.resize(10);
        CHECK(Tracked::alive == 10 && a[9].v == 0);
        a.resize(2);
        CHECK(Tracked::alive == 2);
        a.squeeze();
        CHECK(Tracked::alive == 2 && a.capacity() == 3 && a[0].v == 1 && a[1].v == 2);
        QVarLengthArray<Tracked, 3> b = a;
        CHECK(Tracked::alive == 4);
        QVarLengthArray<Tracked, 3> c(std::move(b));
        CHECK(Tracked::alive == 4 && c.size() == 2 && c[1].v == 2);
        c.clear();
        CHECK(Tracked::alive == 2);
        a.emplace_back(77);
        CHECK(a.back().v == 77 && Tracked::alive == 3);
        a.append(a[0]);
        CHECK(a.back().v == a[0].v);
        a.pop_back();
        a.removeLast();
        CHECK(Tracked::alive == 2);
    }
    CHECK(Tracked::alive == 0);
}

static void testBitArray() {
    QBitArray n;
    CHECK(n.isNull() && n.isEmpty() && n.size() == 0);
    QBitArray e(0);
    CHECK(!e.isNull() && e.isEmpty());
    QBitArray a(10);
    CHECK(a.size() == 10 && a.count(true) == 0 && a.count(false) == 10 && a.count() == 10);
    a.setBit(3);
    a.setBit(9, true);
    a[5] = true;
    CHECK(a.testBit(3) && a.at(9) && a[5] && !a[4]);
    CHECK(a.count(true) == 3);
    CHECK(a.toggleBit(3) == true && !a.testBit(3));
    CHECK(a.toggleBit(3) == false && a.testBit(3));
    a.clearBit(3);
    CHECK(!a.testBit(3) && a.count(true) == 2);
    a[0] = a[5];
    CHECK(a[0]);
    const QBitArray &ca = a;
    CHECK(ca[0] && !ca[1]);
    CHECK(!(!a[0]) && !a[1]);

    QBitArray b(10, true);
    CHECK(b.count(true) == 10);
    b.fill(false);
    CHECK(b.count(true) == 0);
    b.fill(true, 4);
    CHECK(b.size() == 4 && b.count(true) == 4);
    b.fill(false, 1, 3);
    CHECK(b[0] && !b[1] && !b[2] && b[3]);
    b.resize(6);
    CHECK(b.size() == 6 && !b[4] && !b[5] && b[3]);
    b.truncate(2);
    CHECK(b.size() == 2);
    b.truncate(100);
    CHECK(b.size() == 2);
    b.clear();
    CHECK(b.isNull() && b.isEmpty());

    QBitArray x(4), y(4);
    x.setBit(0);
    x.setBit(1);
    y.setBit(1);
    y.setBit(2);
    const QBitArray andv = x & y, orv = x | y, xorv = x ^ y, notv = ~x;
    CHECK(andv.count(true) == 1 && andv[1]);
    CHECK(orv.count(true) == 3 && orv[0] && orv[1] && orv[2] && !orv[3]);
    CHECK(xorv.count(true) == 2 && xorv[0] && !xorv[1] && xorv[2]);
    CHECK(notv.size() == 4 && !notv[0] && !notv[1] && notv[2] && notv[3]);
    QBitArray z = x;
    z &= y;
    CHECK(z == andv);
    z = x;
    z |= y;
    CHECK(z == orv);
    z = x;
    z ^= y;
    CHECK(z == xorv);
    CHECK(x == x && x != y);

    QBitArray s(3), l(5);
    s.fill(true);
    l.setBit(4);
    const QBitArray mixed = s | l;
    CHECK(mixed.size() == 5 && mixed.count(true) == 4 && mixed[4] && mixed[0]);
    const QBitArray andMixed = s & l;
    CHECK(andMixed.size() == 5 && andMixed.count(true) == 0);
    QBitArray p(3), q(3);
    p.swap(q);
    q.setBit(0);
    p.swap(q);
    CHECK(p[0] && !q[0]);
    QBitArray big(1000);
    for (int i = 0; i < 1000; i += 3)
        big.setBit(i);
    CHECK(big.count(true) == 334 && big.testBit(999) && !big.testBit(998));
}

struct Counted {
    static int alive;
    int id;
    explicit Counted(int i) : id(i) { ++alive; }
    ~Counted() { --alive; }
};
int Counted::alive = 0;

static void testCache() {
    {
        QCache<int, Counted> c(5);
        CHECK(c.maxCost() == 5 && c.isEmpty() && c.size() == 0 && c.totalCost() == 0);
        CHECK(c.insert(1, new Counted(1), 2));
        CHECK(c.insert(2, new Counted(2), 2));
        CHECK(c.size() == 2 && c.totalCost() == 4 && Counted::alive == 2);
        CHECK(c.contains(1) && c.contains(2) && !c.contains(3));
        CHECK(c.insert(3, new Counted(3), 2)); // evicts 1, the least recently used
        CHECK(!c.contains(1) && c.contains(2) && c.contains(3));
        CHECK(c.totalCost() == 4 && c.size() == 2 && Counted::alive == 2);
        CHECK(c.object(2) && c.object(2)->id == 2); // 2 is now the most recently used
        CHECK(c.object(1) == nullptr);
        CHECK(c[2]->id == 2);
        CHECK(c.insert(4, new Counted(4), 2)); // evicts 3
        CHECK(c.contains(2) && c.contains(4) && !c.contains(3));
        CHECK(Counted::alive == 2);
        CHECK(c.keys().size() == 2);

        CHECK(!c.insert(9, new Counted(9), 6)); // too expensive: deleted, not stored
        CHECK(!c.contains(9) && Counted::alive == 2 && c.size() == 2);
        CHECK(c.insert(10, new Counted(10), 5)); // exactly maxCost evicts everything else
        CHECK(c.size() == 1 && c.contains(10) && c.totalCost() == 5 && Counted::alive == 1);

        Counted *old = c.object(10);
        CHECK(c.insert(10, new Counted(11), 1)); // replaces and deletes the previous object
        CHECK(c.object(10)->id == 11 && c.size() == 1 && c.totalCost() == 1 && Counted::alive == 1);
        (void)old;

        c.insert(20, new Counted(20), 1);
        c.insert(21, new Counted(21), 1);
        CHECK(c.size() == 3 && c.totalCost() == 3);
        Counted *taken = c.take(20);
        CHECK(taken && taken->id == 20 && !c.contains(20) && c.totalCost() == 2 && Counted::alive == 3);
        delete taken;
        CHECK(Counted::alive == 2);
        CHECK(c.take(12345) == nullptr);
        CHECK(!c.remove(12345));
        CHECK(c.remove(21) && !c.contains(21) && c.totalCost() == 1 && Counted::alive == 1);

        c.insert(30, new Counted(30), 1);
        c.insert(31, new Counted(31), 1);
        c.insert(32, new Counted(32), 1);
        CHECK(c.totalCost() == 4);
        c.setMaxCost(2);
        CHECK(c.maxCost() == 2 && c.totalCost() == 2 && c.size() == 2 && Counted::alive == 2);
        CHECK(c.contains(32) && c.contains(31)); // the most recent survive
        c.setMaxCost(0);
        CHECK(c.isEmpty() && Counted::alive == 0);
        c.setMaxCost(10);
        for (int i = 0; i < 100; ++i)
            c.insert(i, new Counted(i), 1 + i % 3);
        CHECK(c.totalCost() <= 10 && Counted::alive == c.size());
        c.clear();
        CHECK(c.isEmpty() && c.totalCost() == 0 && Counted::alive == 0);
    }
    CHECK(Counted::alive == 0);
    {
        QCache<QString, int> c(3);
        c.insert("a", new int(1));
        c.insert("b", new int(2));
        c.insert("c", new int(3));
        CHECK(c.size() == 3 && c.count() == 3);
        c.insert("d", new int(4));
        CHECK(!c.contains("a") && c.contains("d") && c.size() == 3);
        CHECK(*c.object("b") == 2);
        c.insert("e", new int(5));
        CHECK(c.contains("b") && !c.contains("c"));
        QCache<int, int> defaultCost;
        CHECK(defaultCost.maxCost() == 100);
    }
    {
        // objects at the destruction of the cache are deleted
        {
            QCache<int, Counted> c(10);
            c.insert(1, new Counted(1));
            c.insert(2, new Counted(2));
            CHECK(Counted::alive == 2);
        }
        CHECK(Counted::alive == 0);
    }
}

static void testContiguousCache() {
    QContiguousCache<int> c(3);
    CHECK(c.capacity() == 3 && c.isEmpty() && c.size() == 0 && !c.isFull() && c.available() == 3);
    c.append(1);
    c.append(2);
    c.append(3);
    CHECK(c.isFull() && c.size() == 3 && c.firstIndex() == 0 && c.lastIndex() == 2);
    CHECK(c.at(0) == 1 && c.at(2) == 3 && c[1] == 2);
    c.append(4);
    CHECK(c.size() == 3 && c.firstIndex() == 1 && c.lastIndex() == 3);
    CHECK(!c.containsIndex(0) && c.containsIndex(1) && c.containsIndex(3) && !c.containsIndex(4));
    CHECK(c.at(1) == 2 && c.at(3) == 4 && c.first() == 2 && c.last() == 4);
    c.prepend(0);
    CHECK(c.firstIndex() == 0 && c.lastIndex() == 2 && c.at(0) == 0 && c.at(2) == 3);
    c.insert(1, 100);
    CHECK(c.at(1) == 100 && c.size() == 3);
    c.insert(3, 9); // next to the end: acts as append
    CHECK(c.firstIndex() == 1 && c.lastIndex() == 3 && c.at(3) == 9);
    c.insert(0, 8); // next to the start: acts as prepend
    CHECK(c.firstIndex() == 0 && c.at(0) == 8 && c.lastIndex() == 2);
    c.insert(50, 5); // far away: starts over there
    CHECK(c.size() == 1 && c.firstIndex() == 50 && c.lastIndex() == 50 && c.at(50) == 5);
    c[51] = 6;
    CHECK(c.size() == 2 && c.lastIndex() == 51 && c.at(51) == 6);
    c[52] = 7;
    c[53] = 8;
    CHECK(c.size() == 3 && c.firstIndex() == 51 && c.lastIndex() == 53);
    CHECK(c.takeFirst() == 6 && c.firstIndex() == 52 && c.size() == 2);
    CHECK(c.takeLast() == 8 && c.lastIndex() == 52 && c.size() == 1);
    c.removeFirst();
    CHECK(c.isEmpty());
    c.append(1);
    c.append(2);
    c.removeLast();
    CHECK(c.size() == 1 && c.last() == 1);
    CHECK(c.areIndexesValid());
    c.clear();
    CHECK(c.isEmpty() && c.firstIndex() == 0);
    for (int i = 0; i < 10; ++i)
        c.append(i);
    CHECK(c.firstIndex() == 7 && c.lastIndex() == 9 && c.at(7) == 7 && c.at(9) == 9);
    c.setCapacity(2);
    CHECK(c.capacity() == 2 && c.size() == 2 && c.firstIndex() == 8 && c.at(8) == 8 && c.at(9) == 9);
    c.setCapacity(5);
    CHECK(c.capacity() == 5 && c.size() == 2 && c.available() == 3);
    QContiguousCache<int> copy = c;
    CHECK(copy == c);
    copy.append(1);
    CHECK(copy != c);
    c.normalizeIndexes();
    CHECK(c.areIndexesValid() && c.size() == 2 && c.at(c.firstIndex()) == 8 && c.at(c.lastIndex()) == 9);
    QContiguousCache<int> zero;
    zero.append(1);
    zero.insert(5, 1);
    CHECK(zero.isEmpty() && zero.capacity() == 0);
    QContiguousCache<std::string> sc(2);
    sc.append("x");
    sc.insert(10, "y");
    CHECK(sc.at(10) == "y" && sc.size() == 1);
}

class TimerCounter : public QObject {
public:
    int ticks = 0;
    int lastId = 0;
    int stopAt = 0;
    QBasicTimer timer;
    QEventLoop *loop = nullptr;

protected:
    void timerEvent(QTimerEvent *e) override {
        ++ticks;
        lastId = e->timerId();
        if (stopAt && ticks >= stopAt) {
            timer.stop();
            if (loop)
                loop->quit();
        }
    }
};

static void testBasicTimer() {
    TimerCounter obj;
    CHECK(!obj.timer.isActive() && obj.timer.timerId() == 0);
    QEventLoop loop;
    obj.loop = &loop;
    obj.stopAt = 3;
    obj.timer.start(5, &obj);
    CHECK(obj.timer.isActive() && obj.timer.timerId() > 0);
    const int id = obj.timer.timerId();
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    loop.exec();
    CHECK(obj.ticks == 3);
    CHECK(obj.lastId == id);
    CHECK(!obj.timer.isActive() && obj.timer.timerId() == 0);

    // stopped timers stay quiet
    QEventLoop quiet;
    QTimer::singleShot(60, &quiet, &QEventLoop::quit);
    quiet.exec();
    CHECK(obj.ticks == 3);

    // restart, and chrono interval
    obj.stopAt = 0;
    obj.ticks = 0;
    obj.timer.start(std::chrono::milliseconds(5), &obj);
    const int id2 = obj.timer.timerId();
    CHECK(obj.timer.isActive() && id2 > 0);
    obj.timer.start(5, &obj); // restarting replaces the old timer
    CHECK(obj.timer.isActive());
    QEventLoop wait;
    QTimer::singleShot(120, &wait, &QEventLoop::quit);
    wait.exec();
    CHECK(obj.ticks >= 3);
    CHECK(obj.lastId == obj.timer.timerId());
    obj.timer.stop();
    obj.timer.stop(); // harmless
    const int frozen = obj.ticks;
    QEventLoop after;
    QTimer::singleShot(40, &after, &QEventLoop::quit);
    after.exec();
    CHECK(obj.ticks == frozen);

    // a timer is cancelled when its object goes away
    {
        auto *gone = new TimerCounter;
        gone->timer.start(5, gone);
        CHECK(gone->timer.isActive());
        delete gone;
        QEventLoop l2;
        QTimer::singleShot(30, &l2, &QEventLoop::quit);
        l2.exec();
    }
    // destroying an active QBasicTimer stops it
    {
        TimerCounter owner;
        {
            QBasicTimer local;
            local.start(5, &owner);
            CHECK(local.isActive());
        }
        QEventLoop l3;
        QTimer::singleShot(40, &l3, &QEventLoop::quit);
        l3.exec();
        CHECK(owner.ticks == 0);
    }
    QBasicTimer none;
    none.start(5, nullptr);
    CHECK(!none.isActive());
}

static void testIterators() {
    QList<int> list = {1, 2, 3, 4};
    QListIterator<int> it(list);
    CHECK(it.hasNext() && !it.hasPrevious());
    CHECK(it.peekNext() == 1);
    CHECK(it.next() == 1 && it.next() == 2);
    CHECK(it.peekNext() == 3 && it.peekPrevious() == 2);
    CHECK(it.hasPrevious());
    CHECK(it.previous() == 2);
    CHECK(it.next() == 2);
    it.toBack();
    CHECK(!it.hasNext() && it.hasPrevious() && it.previous() == 4);
    it.toFront();
    CHECK(it.findNext(3) && it.peekNext() == 4);
    CHECK(!it.findNext(99) && !it.hasNext());
    CHECK(it.findPrevious(2) && it.peekNext() == 2);
    CHECK(!it.findPrevious(99) && !it.hasPrevious());
    int sum = 0;
    for (QListIterator<int> i(list); i.hasNext();)
        sum += i.next();
    CHECK(sum == 10);
    QList<int> empty;
    QListIterator<int> ei(empty);
    CHECK(!ei.hasNext() && !ei.hasPrevious());

    QMutableListIterator<int> m(list);
    while (m.hasNext())
        if (m.next() % 2 == 0)
            m.remove();
    CHECK(list == QList<int>({1, 3}));
    m.toFront();
    m.next();
    m.setValue(10);
    CHECK(list == QList<int>({10, 3}) && m.value() == 10);
    m.insert(7);
    CHECK(list == QList<int>({10, 7, 3}) && m.peekNext() == 3 && m.peekPrevious() == 7);
    m.next();
    m.insert(8);
    CHECK(list == QList<int>({10, 7, 3, 8}) && !m.hasNext());
    m.toBack();
    while (m.hasPrevious()) {
        if (m.previous() == 7)
            m.remove();
    }
    CHECK(list == QList<int>({10, 3, 8}) && !m.hasPrevious());
    m.toBack();
    m.previous();
    m.remove(); // removing after previous() leaves the position before the next item
    CHECK(list == QList<int>({10, 3}));
    m.remove(); // nothing to remove: no effect
    CHECK(list.size() == 2);
    m.toFront();
    CHECK(m.findNext(3) && m.peekPrevious() == 3);
    m.setValue(33);
    CHECK(list[1] == 33);
    m.toFront();
    m.next();
    m.setValue(11);
    m.remove();
    CHECK(list == QList<int>({33}));
    CHECK(m.hasNext() && m.next() == 33);
    m.insert(99);
    m.setValue(100); // an inserted item can be modified through the iterator
    CHECK(list == QList<int>({33, 100}));
    m.remove();
    CHECK(list == QList<int>({33}));
    QMutableListIterator<int> inc(list);
    while (inc.hasNext())
        inc.next() += 1;
    CHECK(list[0] == 34);
    QMutableListIterator<int> fp(list);
    fp.toBack();
    CHECK(fp.findPrevious(34) && !fp.hasPrevious());
    QVectorIterator<int> vi(list);
    CHECK(vi.next() == 34);

    QMap<int, QString> map;
    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(3, "three");
    QMapIterator<int, QString> mi(map);
    CHECK(mi.hasNext() && mi.peekNext().key() == 1 && mi.peekNext().value() == "one");
    CHECK(mi.next().key() == 1);
    CHECK(mi.key() == 1 && mi.value() == "one");
    mi.next();
    CHECK(mi.key() == 2 && mi.value() == "two");
    CHECK(mi.hasPrevious() && mi.peekPrevious().key() == 2);
    CHECK(mi.previous().key() == 2);
    CHECK(mi.key() == 2);
    CHECK(mi.findNext(QString("three")) && mi.key() == 3 && !mi.hasNext());
    mi.toBack();
    CHECK(mi.previous().value() == "three");
    mi.toFront();
    CHECK(mi.next().value() == "one");
    mi.toFront();
    QString joined;
    while (mi.hasNext()) {
        mi.next();
        joined += QString::number(mi.key()) + mi.value();
    }
    CHECK(joined == "1one2two3three");
    CHECK(!mi.findNext(QString("zzz")));
    mi.toBack();
    CHECK(mi.findPrevious(QString("one")) && mi.key() == 1);

    QMutableMapIterator<int, QString> mm(map);
    while (mm.hasNext()) {
        mm.next();
        if (mm.key() == 2)
            mm.remove();
        else
            mm.setValue(mm.value() + "!");
    }
    CHECK(map.size() == 2 && map.value(1) == "one!" && map.value(3) == "three!" && !map.contains(2));
    mm.toBack();
    while (mm.hasPrevious()) {
        mm.previous();
        if (mm.key() == 3)
            mm.remove();
    }
    CHECK(map.size() == 1 && map.contains(1));
    mm.toFront();
    mm.next();
    mm.value() = "uno";
    CHECK(map.value(1) == "uno");
    mm.remove();
    CHECK(map.isEmpty());

    QHash<int, int> hash;
    for (int i = 0; i < 20; ++i)
        hash.insert(i, i * i);
    QHashIterator<int, int> hi(hash);
    int total = 0, count = 0;
    while (hi.hasNext()) {
        hi.next();
        total += hi.value() - hi.key() * hi.key();
        ++count;
    }
    CHECK(total == 0 && count == 20);
    hi.toFront();
    CHECK(hi.findNext(49) && hi.key() == 7);
    QMutableHashIterator<int, int> mh(hash);
    while (mh.hasNext()) {
        mh.next();
        if (mh.key() % 2 == 0)
            mh.remove();
        else
            mh.setValue(-mh.value());
    }
    CHECK(hash.size() == 10);
    for (int i = 1; i < 20; i += 2)
        CHECK(hash.value(i) == -i * i);
    mh.toFront();
    int seen = 0;
    while (mh.hasNext()) {
        mh.next();
        ++seen;
        mh.remove(); // every item visited exactly once even when removing
    }
    CHECK(seen == 10 && hash.isEmpty());

    QMultiMap<int, int> mmap;
    mmap.insert(1, 10);
    mmap.insert(1, 11);
    mmap.insert(2, 20);
    QMultiMapIterator<int, int> mmi(mmap);
    int mmCount = 0, mmSum = 0;
    while (mmi.hasNext()) {
        mmi.next();
        ++mmCount;
        mmSum += mmi.value();
    }
    CHECK(mmCount == 3 && mmSum == 41);
    QMutableMultiMapIterator<int, int> mmm(mmap);
    while (mmm.hasNext())
        if (mmm.next().key() == 1)
            mmm.remove();
    CHECK(mmap.size() == 1 && mmap.contains(2));

    QSet<int> set = {1, 2, 3};
    QSetIterator<int> si(set);
    int setSum = 0, setCount = 0;
    while (si.hasNext()) {
        setSum += si.next();
        ++setCount;
    }
    CHECK(setSum == 6 && setCount == 3);
    si.toFront();
    CHECK(si.findNext(2));
    si.toFront();
    CHECK(si.peekNext() >= 1 && si.peekNext() <= 3);
}

namespace {
enum class Opt { None = 0, A = 1, B = 2, C = 8 };
Q_DECLARE_FLAGS(Opts, Opt)

struct Base {
    virtual ~Base() = default;
};
struct Derived : Base {};
} // namespace

static QString debugText(const std::function<void(QDebug &)> &write) {
    QString text;
    {
        QDebug d(&text);
        write(d);
    }
    return text;
}

static void testMoreCoreApi() {
    // QElapsedTimer
    {
        QElapsedTimer a, b;
        a.start();
        b.start();
        CHECK(!(b < a));
        CHECK(a.secsTo(b) == 0 && a.durationTo(b) >= std::chrono::nanoseconds(0));
        CHECK(a.durationElapsed() >= std::chrono::nanoseconds(0));
        QElapsedTimer invalid;
        CHECK(invalid.durationElapsed() == std::chrono::nanoseconds(0));
    }
    // QDeadlineTimer
    {
        const QDeadlineTimer now = QDeadlineTimer::current();
        CHECK(now.hasExpired() && !now.isForever());
        const QDeadlineTimer later = now + 1000;
        CHECK(later > now && later - now == 1000 && now < later && now <= later && later >= now);
        CHECK(later.deadline() - now.deadline() == 1000);
        CHECK(QDeadlineTimer::addNSecs(now, 5000000).deadlineNSecs() - now.deadlineNSecs() == 5000000);
        QDeadlineTimer moved = later;
        moved -= 1000;
        CHECK(moved == now);
        moved += 1;
        CHECK(moved != now);

        const QDeadlineTimer never(QDeadlineTimer::Forever);
        CHECK(never.deadline() == std::numeric_limits<qint64>::max());
        CHECK(never.deadlineNSecs() == std::numeric_limits<qint64>::max());
        CHECK(never > later && (never + 5).isForever() && never == QDeadlineTimer(-1));
        CHECK(never.remainingTimeAsDuration() == std::chrono::nanoseconds::max());
        CHECK(never.timePoint() == QDeadlineTimer::Clock::time_point::max());

        QDeadlineTimer set;
        set.setDeadline(later.deadline(), Qt::PreciseTimer);
        CHECK(set.deadline() == later.deadline() && set.timerType() == Qt::PreciseTimer);
        set.setDeadline(std::numeric_limits<qint64>::max());
        CHECK(set.isForever());
        set.setPreciseDeadline(later.deadlineNSecs() / 1000000000, later.deadlineNSecs() % 1000000000);
        CHECK(set.deadlineNSecs() == later.deadlineNSecs());
        set.setPreciseRemainingTime(-1);
        CHECK(set.isForever());
        set.setTimerType(Qt::VeryCoarseTimer);
        CHECK(set.timerType() == Qt::VeryCoarseTimer);

        const QDeadlineTimer chronoTimer(std::chrono::seconds(60));
        CHECK(!chronoTimer.hasExpired() && chronoTimer.remainingTime() > 59000);
        CHECK(chronoTimer.remainingTimeAsDuration() > std::chrono::seconds(59));
        CHECK(QDeadlineTimer(std::chrono::milliseconds(-5)).hasExpired());
        CHECK(QDeadlineTimer(std::chrono::nanoseconds::max()).isForever());
        CHECK(QDeadlineTimer(later.timePoint()) == later);
        QDeadlineTimer x = now, y = never;
        x.swap(y);
        CHECK(x.isForever() && y == now);
    }
    // QList
    {
        QList<std::string> l;
        l.emplace(0, 3, 'b');
        l.emplace(l.cbegin(), "a");
        std::string moved("c");
        l.insert(2, std::move(moved));
        l.prepend(std::string("z"));
        CHECK((l == QList<std::string>{"z", "a", "bbb", "c"}));
        CHECK(l.constFirst() == "z" && l.constLast() == "c");
        l.insert(l.cbegin() + 1, 2, std::string("y"));
        CHECK(l.size() == 6 && l[1] == "y" && l[2] == "y");
        l.assign(2, std::string("q"));
        CHECK((l == QList<std::string>{"q", "q"}));
        l.assign({"1", "2", "3"});
        CHECK(l.size() == 3 && *l.crbegin() == "3" && *(l.crend() - 1) == "1");
        const std::vector<std::string> src{"x", "y"};
        l.assign(src.begin(), src.end());
        CHECK(l == QList<std::string>(src.begin(), src.end()));
        l += std::string("w");
        l << std::string("v");
        CHECK(l.size() == 4 && l.last() == "v");
        CHECK(l.toList() == l && l.toVector() == l && QList<std::string>::fromList(l) == l &&
              QList<std::string>::fromVector(l) == l);
        l.shrink_to_fit();
        CHECK(l.max_size() > 0);
        const QList<int> a{1, 2}, b{1, 3};
        CHECK(a < b && a <= b && b > a && b >= a && a <= a && a >= a);
    }
    // QVarLengthArray
    {
        QVarLengthArray<int, 4> v(6, 7);
        CHECK(v.size() == 6 && v[5] == 7);
        v.assign({1, 2, 3, 2, 4, 2});
        CHECK(v.removeAll(2) == 3 && v.size() == 3 && v[2] == 4);
        CHECK(v.removeOne(3) && !v.removeOne(42) && v.size() == 2);
        CHECK(v.removeIf([](int i) { return i > 1; }) == 1 && v.size() == 1);
        v.assign(5, 9);
        CHECK(v.size() == 5 && v[4] == 9);
        v.assign(5, v[0]);
        CHECK(v.size() == 5 && v[0] == 9);
        const int src[] = {5, 6};
        v.assign(std::begin(src), std::end(src));
        CHECK(v.size() == 2 && v[1] == 6);
        int moved = 1;
        v.insert(0, std::move(moved));
        v.prepend(0);
        v.insert(v.cend(), 2, 8);
        v += 10;
        v << 11;
        CHECK((v == QVarLengthArray<int, 4>{0, 1, 5, 6, 8, 8, 10, 11}));
        QVarLengthArray<int, 4> w{0, 1, 5, 7};
        CHECK(v < w && w > v && v <= w && w >= v && v <= v);
        CHECK((QVarLengthArray<int, 4>::max_size() > 0));

        QVarLengthArray<std::string, 2> strings{"a", "b", "a", "c"};
        CHECK(strings.removeAll(std::string("a")) == 2 && strings.size() == 2 && strings[0] == "b");
        strings.insert(strings.cbegin(), std::string("z"));
        strings.prepend(std::string("y"));
        CHECK(strings.size() == 4 && strings[0] == "y" && strings[1] == "z");
    }
    // QFlags
    {
        Opts o = Opt::A;
        o |= Opt::C;
        CHECK(o.toInt() == 9 && Opts::fromInt(9) == o);
        CHECK(o.testFlags(Opts(Opt::A) | Opt::C) && !o.testFlags(Opts(Opt::A) | Opt::B));
        CHECK(o.testAnyFlag(Opt::C) && !o.testAnyFlag(Opt::B) && !o.testAnyFlag(Opt::None));
        CHECK(o.testAnyFlags(Opts(Opt::B) | Opt::C) && !o.testAnyFlags(Opt::B));
        CHECK(Opts().testFlags(Opts()) && !o.testFlags(Opts()));
    }
    // QSharedPointer and QWeakPointer
    {
        QSharedPointer<QObject> obj(new QTimer);
        {
            const QSharedPointer<QTimer> timer = obj.objectCast<QTimer>();
            CHECK(timer && timer.use_count() == 2);
        }
        CHECK(!obj.objectCast<QThread>());
        CHECK(qSharedPointerObjectCast<QTimer>(obj).data() == obj.data());
        QSharedPointer<const QObject> constObj = obj;
        CHECK(qSharedPointerConstCast<QObject>(constObj) == obj);
        QWeakPointer<QObject> weak = obj;
        CHECK(weak && !!weak && weak == obj && obj == weak && weak != nullptr && !(nullptr == weak));
        CHECK(weak.lock() == obj && qSharedPointerObjectCast<QTimer>(weak) == obj);
        QWeakPointer<QObject> weak2 = weak;
        CHECK(weak == weak2);
        QSharedPointer<Base> base(new Derived);
        CHECK(qSharedPointerDynamicCast<Derived>(QWeakPointer<Base>(base)));
        CHECK(qHash(obj) == qHash(QSharedPointer<QObject>(obj)) && qHash(obj, 1) != qHash(obj, 2));
        QSet<QSharedPointer<QObject>> set;
        set.insert(obj);
        CHECK(set.contains(obj));
        set.clear();
        constObj.reset();
        QSharedPointer<QObject> other;
        obj.swap(other);
        CHECK(!obj && other && weak == other);
        other.reset();
        CHECK(!weak && weak.isNull() && weak == nullptr && weak == other);
        QSharedPointer<int> counted;
        bool deleted = false;
        counted.reset(new int(1), [&deleted](int *p) {
            deleted = true;
            delete p;
        });
        counted.reset();
        CHECK(deleted);
    }
    // QPointer
    {
        auto *timer = new QTimer;
        QPointer<QTimer> p = timer;
        QPointer<QObject> o = p;
        QPointer<QObject> empty;
        CHECK(o == p && o == timer && timer == o && o != empty && empty == nullptr);
        o.swap(empty);
        CHECK(!o && empty == timer);
        delete timer;
        CHECK(empty.isNull() && p == nullptr);
    }
    // QDebug
    {
        CHECK(debugText([](QDebug &d) { d << QSet<int>{3}; }) == "QSet(3) ");
        CHECK(debugText([](QDebug &d) { d << std::vector<int>{1, 2}; }) == "std::vector(1, 2) ");
        CHECK(debugText([](QDebug &d) { d << std::list<int>{4}; }) == "std::list(4) ");
        CHECK(debugText([](QDebug &d) { d << std::make_pair(1, QString("a")); }) == "std::pair(1, \"a\") ");
        QMultiMap<int, int> mm;
        mm.insert(1, 2);
        CHECK(debugText([&](QDebug &d) { d << mm; }) == "QMultiMap((1, 2)) ");
        QMultiHash<int, int> mh;
        mh.insert(3, 4);
        CHECK(debugText([&](QDebug &d) { d << mh; }) == "QMultiHash((3, 4)) ");
        CHECK(debugText([](QDebug &d) { d << (Opts(Opt::A) | Opt::C); }) == "QFlags(0x1|0x8) ");
        CHECK(debugText([](QDebug &d) { d << Opts(); }) == "QFlags() ");
        const QString s("v\"w");
        CHECK(debugText([&](QDebug &d) { d << QStringView(s); }) == debugText([&](QDebug &d) { d << s; }));
        CHECK(debugText([](QDebug &d) {
                  d.nospace().noquote().verbosity(5);
                  CHECK(d.verbosity() == 5);
                  d.setVerbosity(42);
                  CHECK(d.verbosity() == 7);
                  {
                      QDebugStateSaver saver(d);
                      d.setVerbosity(1);
                  }
                  CHECK(d.verbosity() == 7);
                  d.resetFormat();
                  CHECK(d.verbosity() == 2 && d.autoInsertSpaces());
                  d << QString("q");
              }) == "\"q\" ");
        QString t1, t2;
        {
            QDebug a(&t1), b(&t2);
            a.swap(b);
            a << 1;
            b << 2;
        }
        CHECK(t1 == "2 " && t2 == "1 ");
    }
    // QDate::startOfDay() and endOfDay()
    {
        const QDate date(2024, 3, 15);
        CHECK(date.startOfDay(Qt::UTC) == QDateTime(date, QTime(0, 0), Qt::UTC));
        CHECK(date.endOfDay(Qt::UTC) == QDateTime(date, QTime(23, 59, 59, 999), Qt::UTC));
        CHECK(date.endOfDay(Qt::UTC).addMSecs(1) == date.addDays(1).startOfDay(Qt::UTC));
        const QDateTime offset = date.startOfDay(Qt::OffsetFromUTC, 3600);
        CHECK(offset.offsetFromUtc() == 3600 && offset.time() == QTime(0, 0));
        CHECK(date.startOfDay().date() == date && date.startOfDay().timeSpec() == Qt::LocalTime);
        CHECK(date.startOfDay() < date.endOfDay());
        CHECK(!QDate().startOfDay().isValid() && !QDate().endOfDay(Qt::UTC).isValid());
    }
}

static void testExtras() {
    CHECK(QDate::fromJulianDay(784354017364LL).isValid());
    CHECK(!QDate::fromJulianDay(784354017365LL).isValid());
    CHECK(QDate::fromJulianDay(-784350574879LL).isValid());
    CHECK(!QDate::fromJulianDay(-784350574880LL).isValid());
    CHECK(QDate::fromJulianDay(784354017364LL).year() > 2000000000);
    CHECK(QDate(2000, 1, 1).addDays(std::numeric_limits<qint64>::max()).isNull());
    CHECK(QDate(2000, 1, 1).addDays(std::numeric_limits<qint64>::min()).isNull());
    CHECK(QDate(2024, 1, 1).addDays(1LL << 50).isNull());
    CHECK(QDateTime(QDate::fromJulianDay(700000000000LL), QTime(0, 0), Qt::UTC).toMSecsSinceEpoch() ==
          std::numeric_limits<qint64>::max());
    CHECK(QDateTime(QDate::fromJulianDay(-700000000000LL), QTime(0, 0), Qt::UTC).toMSecsSinceEpoch() ==
          std::numeric_limits<qint64>::min());

    QDateTime d(QDate(2024, 5, 6), QTime(7, 8, 9), Qt::OffsetFromUTC, 3600);
    d.setTimeSpec(Qt::UTC);
    CHECK(d.timeSpec() == Qt::UTC && d.offsetFromUtc() == 0 && d.time() == QTime(7, 8, 9));
    d.setDate(QDate(2025, 1, 2));
    d.setTime(QTime(1, 2, 3));
    CHECK(d.date() == QDate(2025, 1, 2) && d.time() == QTime(1, 2, 3));
    QDateTime nd;
    nd.setDate(QDate(2020, 1, 1));
    CHECK(nd.isValid() && nd.time() == QTime(0, 0));

    const QStringView sv(u"Hello");
    CHECK(sv.startsWith(QChar(u'H')) && !sv.startsWith(QChar(u'h')) && sv.startsWith(QChar(u'h'), Qt::CaseInsensitive));
    CHECK(sv.endsWith(QChar(u'o')) && !sv.endsWith(QChar(u'H')) && !QStringView().startsWith(QChar(u'a')));
    CHECK(sv.indexOf(QChar(u'l')) == 2 && sv.indexOf(QChar(u'l'), 3) == 3 && sv.lastIndexOf(QChar(u'l')) == 3);
    CHECK(sv.contains(QChar(u'e')) && !sv.contains(QChar(u'z')));
    CHECK(sv.compare(QStringView(u"Hello")) == 0 && sv.compare(QStringView(u"Help")) < 0 && sv.compare(QStringView(u"Hell")) > 0);
    CHECK(sv.compare(QStringView(u"HELLO"), Qt::CaseInsensitive) == 0);
    CHECK(sv >= QStringView(u"Hello") && sv <= QStringView(u"Hello") && sv > QStringView(u"Hell") && QStringView(u"A") < sv);
    CHECK(QStringView(u"-5").toLongLong() == -5 && QStringView(u"7").toUInt() == 7 && QStringView(u"9").toULongLong() == 9);
    CHECK(sv.cend() - sv.cbegin() == 5);

    const QByteArrayView bv("Hello");
    CHECK(bv.startsWith('H') && !bv.startsWith('h') && bv.endsWith('o') && !bv.endsWith('H') && !QByteArrayView().startsWith('a'));
    CHECK(bv.indexOf('l') == 2 && bv.indexOf('l', 3) == 3 && bv.lastIndexOf('l') == 3 && bv.lastIndexOf("l", 2) == 2);
    CHECK(bv.lastIndexOf("lo") == 3 && bv.lastIndexOf("zz") == -1 && bv.contains('e') && !bv.contains('z'));
    CHECK(bv.compare(QByteArrayView("Hello")) == 0 && bv.compare(QByteArrayView("Help")) < 0 && bv.compare(QByteArrayView("Hell")) > 0);
    CHECK(bv >= QByteArrayView("Hello") && bv <= QByteArrayView("Hello") && bv > QByteArrayView("Hell"));
    CHECK(bv.cend() - bv.cbegin() == 5);

    QRandomGenerator rg(3), rh(3);
    quint32 words[5];
    quint64 wide[3];
    rg.fillRange(words);
    for (quint32 &w : words)
        CHECK(w == rh.generate());
    rg.fillRange(wide);
    for (quint64 &w : wide)
        CHECK(w == rh.generate64());
    const double lo = rg.bounded(10.0, 20.0);
    CHECK(lo >= 10.0 && lo < 20.0);

    QVarLengthArray<int, 4> va = {1, 2, 3};
    va.insert(1, 2, 9);
    CHECK(va.size() == 5 && va[0] == 1 && va[1] == 9 && va[2] == 9 && va[3] == 2 && va[4] == 3);
    va.replace(0, 5);
    va.removeAt(1);
    va.removeFirst();
    CHECK(va.size() == 3 && va[0] == 9 && va[2] == 3);
    va << 4;
    va += 5;
    va.append({6, 7});
    CHECK(va.size() == 7 && va[6] == 7 && va.crbegin() != va.crend() && *va.crbegin() == 7);
    va.fill(0);
    CHECK(va[0] == 0 && va[6] == 0);
    va.fill(1, 2);
    CHECK(va.size() == 2 && va[1] == 1);
    va.emplace(va.begin(), 8);
    CHECK(va.size() == 3 && va[0] == 8);
    va.shrink_to_fit();
    CHECK(va.capacity() == 4);
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    useTimeZone("UTC");
    testDateValidity();
    testDateJulianAndWeekday();
    testDateWeekNumber();
    testDateArithmetic();
    testDateText();
    testTime();
    testDateTimeBasics();
    testDateTimeOffsets();
    testDateTimeLocal();
    testDateTimeArithmetic();
    testDateTimeText();
    testUuid();
    testRandom();
    testHashKnownAnswers();
    testHashIncremental();
    testMessageAuthenticationCode();
    testHashDevice();
    testVersionNumber();
    testPoint();
    testSize();
    testLine();
    testMargins();
    testRect();
    testRectF();
    testStringView();
    testByteArrayView();
    testSpan();
    testVarLengthArray();
    testBitArray();
    testCache();
    testContiguousCache();
    testBasicTimer();
    testIterators();
    testExtras();
    testMoreCoreApi();

    if (g_failures) {
        std::printf("%d type test check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all type tests passed");
    return 0;
}
