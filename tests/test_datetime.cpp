// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/qtemporarydir.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using namespace Qt::StringLiterals;

// Zones compiled with "zic -b slim" from these rules, so that the daylight-saving changes come from
// the POSIX rule in the footer of each file:
//   Rule EU 1981 max - Mar lastSun 1:00u 1:00 S
//   Rule EU 1996 max - Oct lastSun 1:00u 0 -
//   Zone Test/Berlin 0:53:28 - LMT 1893 Apr
//                    1:00 EU CE%sT
//   Rule AN 2008 max - Apr Sun>=1 2:00s 0 S
//   Rule AN 2008 max - Oct Sun>=1 2:00s 1:00 D
//   Zone Test/South 10:00 - AEST 2008 Apr 6 3:00s
//                   10:00 AN AE%sT
//   Zone Test/Kolkata 5:30 - IST
static const unsigned char kBerlin[] = {
    0x54, 0x5a, 0x69, 0x66, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x5a, 0x69, 0x66, 0x32, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x08,
    0xff, 0xff, 0xff, 0xff, 0x6f, 0xa2, 0x61, 0xf8, 0x01, 0x00, 0x00, 0x0c, 0x88, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x10,
    0x00, 0x04, 0x4c, 0x4d, 0x54, 0x00, 0x43, 0x45, 0x54, 0x00, 0x0a, 0x43, 0x45, 0x54, 0x2d, 0x31, 0x43, 0x45, 0x53,
    0x54, 0x2c, 0x4d, 0x33, 0x2e, 0x35, 0x2e, 0x30, 0x2c, 0x4d, 0x31, 0x30, 0x2e, 0x35, 0x2e, 0x30, 0x2f, 0x33, 0x0a};
static const unsigned char kSouth[] = {
    0x54, 0x5a, 0x69, 0x66, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54, 0x5a, 0x69, 0x66, 0x32, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x05,
    0x00, 0x00, 0x00, 0x00, 0x47, 0xf7, 0xb0, 0x10, 0x00, 0x00, 0x00, 0x8c, 0xa0, 0x00, 0x00, 0x41, 0x45, 0x53, 0x54,
    0x00, 0x0a, 0x41, 0x45, 0x53, 0x54, 0x2d, 0x31, 0x30, 0x41, 0x45, 0x44, 0x54, 0x2c, 0x4d, 0x31, 0x30, 0x2e, 0x31,
    0x2e, 0x30, 0x2c, 0x4d, 0x34, 0x2e, 0x31, 0x2e, 0x30, 0x2f, 0x33, 0x0a};
static const unsigned char kKolkata[] = {
    0x54, 0x5a, 0x69, 0x66, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x54, 0x5a, 0x69, 0x66, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x4d, 0x58, 0x00, 0x00, 0x49,
    0x53, 0x54, 0x00, 0x0a, 0x49, 0x53, 0x54, 0x2d, 0x35, 0x3a, 0x33, 0x30, 0x0a};

static bool writeFile(const QString &path, const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
}

static QByteArray berlinId() { return QByteArray("Test/Berlin"); }

static QDateTime utc(int y, int mo, int d, int h = 0, int mi = 0) {
    return QDateTime(QDate(y, mo, d), QTime(h, mi), Qt::UTC);
}

static void testTimeZoneBasics() {
    const QTimeZone berlin("Test/Berlin"), south("Test/South"), kolkata("Test/Kolkata");
    CHECK(berlin.isValid() && south.isValid() && kolkata.isValid() && !QTimeZone("Test/Missing").isValid() &&
          !QTimeZone().isValid());
    CHECK(!QTimeZone("../etc/passwd").isValid() && !QTimeZone("/etc/passwd").isValid());
    CHECK(berlin.id() == "Test/Berlin" && berlin.timeSpec() == Qt::TimeZone && !berlin.isUtcOrFixedOffset());
    CHECK(berlin.territory() == QLocale::Germany && berlin.country() == QLocale::Germany &&
          berlin.comment() == u"most of Testland"_s);
    CHECK(QTimeZone::isTimeZoneIdAvailable("Test/Berlin") && !QTimeZone::isTimeZoneIdAvailable("Nope/Nope"));
    CHECK((QTimeZone::availableTimeZoneIds() ==
           QList<QByteArray>{"Test/Alias", "Test/Berlin", "Test/Kolkata", "Test/South", "UTC"}));
    CHECK((QTimeZone::availableTimeZoneIds(QLocale::Germany) == QList<QByteArray>{"Test/Berlin"}));
    CHECK((QTimeZone::availableTimeZoneIds(19800) == QList<QByteArray>{"Test/Kolkata"}));
    CHECK(QTimeZone("Test/Alias").offsetFromUtc(utc(2024, 7, 1)) == 7200);

    // Offsets from the footer rule, for any year.
    CHECK(berlin.offsetFromUtc(utc(2024, 1, 15)) == 3600 && berlin.offsetFromUtc(utc(2024, 7, 1)) == 7200);
    CHECK(berlin.offsetFromUtc(utc(2100, 7, 1)) == 7200 && berlin.offsetFromUtc(utc(1850, 7, 1)) == 3208);
    CHECK(berlin.abbreviation(utc(2024, 7, 1)) == u"CEST"_s && berlin.abbreviation(utc(2024, 1, 1)) == u"CET"_s);
    CHECK(berlin.isDaylightTime(utc(2024, 7, 1)) && !berlin.isDaylightTime(utc(2024, 12, 1)) &&
          berlin.hasDaylightTime());
    CHECK(berlin.standardTimeOffset(utc(2024, 7, 1)) == 3600 && berlin.daylightTimeOffset(utc(2024, 7, 1)) == 3600);
    CHECK(berlin.daylightTimeOffset(utc(2024, 1, 1)) == 0);
    // The transitions happen at 01:00 UTC on the last Sundays of March and October.
    CHECK(berlin.offsetFromUtc(utc(2024, 3, 31, 0, 59)) == 3600 &&
          berlin.offsetFromUtc(utc(2024, 3, 31, 1, 0)) == 7200);
    CHECK(berlin.offsetFromUtc(utc(2024, 10, 27, 0, 59)) == 7200 &&
          berlin.offsetFromUtc(utc(2024, 10, 27, 1, 0)) == 3600);
    // The southern rule wraps around the new year.
    CHECK(south.offsetFromUtc(utc(2024, 1, 10)) == 39600 && south.offsetFromUtc(utc(2024, 7, 10)) == 36000);
    CHECK(south.abbreviation(utc(2024, 12, 25)) == u"AEDT"_s && south.offsetFromUtc(utc(2005, 1, 10)) == 36000);
    CHECK(kolkata.offsetFromUtc(utc(2024, 7, 1)) == 19800 && !kolkata.hasDaylightTime() &&
          kolkata.abbreviation(utc(2024, 1, 1)) == u"IST"_s);

    const QTimeZone::OffsetData data = berlin.offsetData(utc(2024, 7, 1, 12));
    CHECK(data.abbreviation == u"CEST"_s && data.offsetFromUtc == 7200 && data.standardTimeOffset == 3600 &&
          data.daylightTimeOffset == 3600);
    CHECK(data.atUtc == utc(2024, 7, 1, 12));
    const QTimeZone::OffsetData next = berlin.nextTransition(utc(2024, 7, 1));
    CHECK(next.atUtc == utc(2024, 10, 27, 1) && next.offsetFromUtc == 3600 && next.abbreviation == u"CET"_s);
    CHECK(berlin.previousTransition(utc(2024, 7, 1)).atUtc == utc(2024, 3, 31, 1));
    CHECK(berlin.nextTransition(utc(2090, 5, 1)).atUtc == utc(2090, 10, 29, 1));
    CHECK(berlin.previousTransition(utc(1890, 1, 1)).atUtc.isValid() == false);
    const QTimeZone::OffsetDataList list = berlin.transitions(utc(2024, 1, 1), utc(2025, 12, 31));
    CHECK(list.size() == 4 && list.at(0).atUtc == utc(2024, 3, 31, 1) && list.at(3).atUtc == utc(2025, 10, 26, 1));
    CHECK(berlin.hasTransitions() && !kolkata.hasTransitions() && !QTimeZone::utc().hasTransitions());
    CHECK(kolkata.nextTransition(utc(2024, 1, 1)).atUtc.isValid() == false);

    CHECK(berlin.displayName(utc(2024, 7, 1)) == u"CEST"_s &&
          berlin.displayName(utc(2024, 7, 1), QTimeZone::OffsetName) == u"UTC+02:00"_s);
    CHECK(berlin.displayName(utc(2024, 7, 1), QTimeZone::LongName) == u"Test/Berlin"_s);
    CHECK(berlin.displayName(QTimeZone::StandardTime, QTimeZone::ShortName) == u"CET"_s);
    CHECK(berlin.displayName(QTimeZone::DaylightTime, QTimeZone::ShortName) == u"CEST"_s);
}

static void testFixedAndSpecialZones() {
    const QTimeZone u = QTimeZone::utc(), f(19800), f2 = QTimeZone::fromSecondsAheadOfUtc(-3600);
    CHECK(u.isValid() && u.id() == "UTC" && u.timeSpec() == Qt::UTC && u.isUtcOrFixedOffset() &&
          u.fixedSecondsAheadOfUtc() == 0);
    CHECK(f.id() == "UTC+05:30" && f.timeSpec() == Qt::OffsetFromUTC && f.fixedSecondsAheadOfUtc() == 19800 &&
          f.offsetFromUtc(utc(2024, 1, 1)) == 19800);
    CHECK(f2.id() == "UTC-01:00" && f.abbreviation(utc(2024, 1, 1)) == u"UTC+05:30"_s &&
          QTimeZone(0) == QTimeZone::utc());
    CHECK(!QTimeZone(17 * 3600).isValid() && QTimeZone("UTC+05:30") == f && QTimeZone("UTC").timeSpec() == Qt::UTC);
    CHECK(!QTimeZone("UTC+25:00").isValid());
    const QTimeZone custom("Mars/Base", 3700, u"Mars Base Time"_s, u"MBT"_s, QLocale::AnyTerritory, u"red"_s);
    CHECK(custom.isValid() && custom.id() == "Mars/Base" && custom.timeSpec() == Qt::TimeZone &&
          custom.abbreviation(utc(2024, 1, 1)) == u"MBT"_s);
    CHECK(custom.displayName(utc(2024, 1, 1), QTimeZone::LongName) == u"Mars Base Time"_s &&
          custom.comment() == u"red"_s && !custom.isUtcOrFixedOffset());
    const QTimeZone posix("EST5EDT,M3.2.0,M11.1.0");
    CHECK(posix.isValid() && posix.offsetFromUtc(utc(2024, 7, 1)) == -14400 &&
          posix.offsetFromUtc(utc(2024, 1, 1)) == -18000);
    CHECK(QTimeZone("<+0330>-3:30").offsetFromUtc(utc(2024, 1, 1)) == 12600);
    const QTimeZone local(QTimeZone::LocalTime);
    CHECK(local.isValid() && local.timeSpec() == Qt::LocalTime);
    CHECK(QTimeZone::systemTimeZoneId() == "Test/Berlin" && QTimeZone::systemTimeZone() == QTimeZone("Test/Berlin"));
    QTimeZone a(berlinId()), b = f;
    a.swap(b);
    CHECK(a == f && b.id() == "Test/Berlin" && a != b && qHash(a) == qHash(QTimeZone(19800)));
    QTimeZone moved = std::move(b);
    CHECK(moved.isValid() && moved.id() == "Test/Berlin");
}

static void testDateTimeWithZones() {
    const QTimeZone berlin("Test/Berlin"), ny("EST5EDT,M3.2.0,M11.1.0");
    QDateTime meeting(QDate(2024, 7, 1), QTime(15, 0), berlin);
    CHECK(meeting.isValid() && meeting.timeSpec() == Qt::TimeZone && meeting.timeZone() == berlin);
    CHECK(meeting.offsetFromUtc() == 7200 && meeting.isDaylightTime() && meeting.timeZoneAbbreviation() == u"CEST"_s);
    CHECK(meeting.toMSecsSinceEpoch() == utc(2024, 7, 1, 13).toMSecsSinceEpoch() && meeting == utc(2024, 7, 1, 13));
    CHECK(meeting.toString(Qt::ISODate) == u"2024-07-01T15:00:00+02:00"_s &&
          meeting.toString(u"HH:mm t"_s) == u"15:00 CEST"_s);
    const QDateTime there = meeting.toTimeZone(ny);
    CHECK(there.time() == QTime(9, 0) && there.timeZoneAbbreviation() == u"EDT"_s && there == meeting);
    CHECK(meeting.toUTC().time() == QTime(13, 0) && meeting.addDays(180).offsetFromUtc() == 3600 &&
          meeting.addSecs(3600).time() == QTime(16, 0));
    // A time in the spring gap moves forward; in the autumn fold the earlier time is used.
    const QDateTime gap(QDate(2024, 3, 31), QTime(2, 30), berlin);
    CHECK(gap.time() == QTime(3, 30) && gap.offsetFromUtc() == 7200 && gap.toUTC().time() == QTime(1, 30));
    const QDateTime fold(QDate(2024, 10, 27), QTime(2, 30), berlin);
    CHECK(fold.offsetFromUtc() == 7200 && fold.toUTC().time() == QTime(0, 30) &&
          fold.addSecs(3600).time() == QTime(2, 30) && fold.addSecs(3600).offsetFromUtc() == 3600);
    CHECK(QDateTime::fromMSecsSinceEpoch(utc(2024, 1, 1, 12).toMSecsSinceEpoch(), berlin).time() == QTime(13, 0));
    CHECK(QDateTime::fromSecsSinceEpoch(utc(2024, 7, 1).toSecsSinceEpoch(), berlin).time() == QTime(2, 0));
    CHECK(QDateTime::currentDateTime(berlin).timeSpec() == Qt::TimeZone &&
          QDateTime::currentDateTime(QTimeZone::utc()).timeSpec() == Qt::UTC);
    CHECK(!QDateTime(QDate(2024, 1, 1), QTime(0, 0), QTimeZone()).isValid() &&
          !QDateTime::fromSecsSinceEpoch(0, QTimeZone()).isValid());

    QDateTime dt(QDate(2024, 7, 1), QTime(12, 0), Qt::UTC);
    dt.setTimeZone(berlin);
    CHECK(dt.time() == QTime(12, 0) && dt.offsetFromUtc() == 7200 && dt.toUTC().time() == QTime(10, 0));
    dt.setTimeZone(QTimeZone(3600));
    CHECK(dt.timeSpec() == Qt::OffsetFromUTC && dt.offsetFromUtc() == 3600 && dt.timeZone() == QTimeZone(3600));
    dt.setTimeZone(QTimeZone::utc());
    CHECK(dt.timeSpec() == Qt::UTC && dt.timeZone() == QTimeZone::utc() && !dt.isDaylightTime() &&
          dt.timeZoneAbbreviation() == u"UTC"_s);
    dt.setTimeZone(QTimeZone(QTimeZone::LocalTime));
    CHECK(dt.timeSpec() == Qt::LocalTime && dt.timeZone().timeSpec() == Qt::LocalTime);
    dt.setTimeZone(berlin);
    dt.setTimeSpec(Qt::UTC);
    CHECK(dt.timeSpec() == Qt::UTC && dt.timeZone() == QTimeZone::utc());
    CHECK(QDateTime(QDate(2024, 1, 1), QTime(0, 0), Qt::OffsetFromUTC, 3600).timeZoneAbbreviation() == u"UTC+01:00"_s);
    QDateTime x = meeting, y = dt;
    x.swap(y);
    CHECK(x == dt && y == meeting && y.timeZone() == berlin);

    CHECK(QDate(2024, 7, 1).startOfDay(berlin) == QDateTime(QDate(2024, 7, 1), QTime(0, 0), berlin));
    CHECK(QDate(2024, 7, 1).endOfDay(berlin).time() == QTime(23, 59, 59, 999));
    // A zone whose gap swallows midnight: the day starts at 01:00.
    const QTimeZone midnightGap("<-04>4<-03>,M9.1.6/24,M4.1.6/24");
    CHECK(QDate(2024, 9, 8).startOfDay(midnightGap).time() == QTime(1, 0));
    CHECK(QDate(2024, 9, 7).endOfDay(midnightGap).time() == QTime(23, 59, 59, 999));
    CHECK(!QDate().startOfDay(berlin).isValid() && !QDate(2024, 1, 1).startOfDay(QTimeZone()).isValid());
}

// ------------------------------------------------------------------ QCalendar

static void testCalendar() {
    const QCalendar greg, julian(QCalendar::System::Julian), milankovic(QCalendar::System::Milankovic);
    const QCalendar jalali(QCalendar::System::Jalali), islamic(QCalendar::System::IslamicCivil);
    CHECK(greg.isValid() && greg.isGregorian() && greg.name() == u"Gregorian"_s &&
          QCalendar(u"persian"_s).name() == u"Jalali"_s);
    CHECK(QCalendar("islamic-civil").isValid() && !QCalendar("Hebrew").isValid() &&
          !QCalendar(QCalendar::System::User).isValid());
    CHECK(QCalendar::availableCalendars().size() == 5);
    CHECK(julian.dateFromParts(2024, 1, 1) == QDate(2024, 1, 14) && julian.partsFromDate(QDate(1, 1, 1)).day == 3);
    CHECK(julian.isLeapYear(1900) && !greg.isLeapYear(1900) && julian.isLeapYear(-1) && !julian.isLeapYear(-2));
    CHECK(milankovic.dateFromParts(2024, 5, 1) == QDate(2024, 5, 1) &&
          milankovic.dateFromParts(2800, 3, 1) == QDate(2800, 2, 29));
    CHECK(!milankovic.isLeapYear(2800) && milankovic.isLeapYear(2000) && milankovic.isLeapYear(2400) &&
          !milankovic.isLeapYear(2500) && milankovic.isLeapYear(2900));
    CHECK(jalali.dateFromParts(1403, 1, 1) == QDate(2024, 3, 20) && jalali.isLeapYear(1403) &&
          !jalali.isLeapYear(1402));
    const QCalendar::YearMonthDay p = jalali.partsFromDate(QDate(2024, 6, 1));
    CHECK(p.isValid() && p.year == 1403 && p.month == 3 && p.day == 12);
    CHECK(jalali.daysInMonth(12, 1403) == 30 && jalali.daysInMonth(12, 1402) == 29 &&
          jalali.daysInMonth(1, 1402) == 31 && jalali.daysInYear(1403) == 366);
    CHECK(islamic.dateFromParts(1445, 9, 1) == QDate(2024, 3, 11) &&
          islamic.dateFromParts(1446, 1, 1) == QDate(2024, 7, 8));
    CHECK(islamic.isLunar() && !islamic.isSolar() &&
          islamic.daysInYear(1445) == 354 + (islamic.isLeapYear(1445) ? 1 : 0));
    CHECK(islamic.maximumDaysInMonth() == 30 && islamic.minimumDaysInMonth() == 29 && greg.maximumMonthsInYear() == 12);
    CHECK(islamic.monthName(QLocale::c(), 9) == u"Ramadan"_s &&
          jalali.monthName(QLocale::c(), 1, 1403, QLocale::ShortFormat) == u"Far"_s);
    CHECK(greg.monthName(QLocale(QLocale::German), 3) == u"März"_s &&
          greg.standaloneMonthName(QLocale(QLocale::Russian), 1) == u"январь"_s);
    CHECK(julian.weekDayName(QLocale::c(), 1) == u"Monday"_s &&
          julian.standaloneWeekDayName(QLocale::c(), 7, QLocale::ShortFormat) == u"Sun"_s);
    CHECK(greg.dayOfWeek(QDate(2024, 7, 1)) == 1 && julian.dayOfWeek(QDate()) == 0);
    CHECK(!greg.isDateValid(2023, 2, 29) && greg.isDateValid(2024, 2, 29) && !greg.isDateValid(0, 1, 1) &&
          !greg.hasYearZero());
    CHECK(greg.daysInMonth(2) == 29 && greg.daysInMonth(13, 2024) == 0 && greg.monthsInYear(2024) == 12 &&
          greg.isProleptic() && !jalali.isProleptic());
    // No year zero: 1 BCE (year -1) is followed by 1 CE.
    CHECK(julian.dateFromParts(-1, 12, 31).addDays(1) == julian.dateFromParts(1, 1, 1));
    CHECK(julian.partsFromDate(julian.dateFromParts(-1, 12, 31)).year == -1);
    for (const QCalendar &cal : {greg, julian, milankovic, jalali, islamic}) {
        // Round trips over a range of days, including negative years.
        bool ok = true;
        for (qint64 jd = 1000000; jd < 3000000 && ok; jd += 997) {
            const QDate d = QDate::fromJulianDay(jd);
            const QCalendar::YearMonthDay ymd = cal.partsFromDate(d);
            ok = cal.dateFromParts(ymd) == d && cal.isDateValid(ymd.year, ymd.month, ymd.day);
        }
        CHECK(ok);
    }
    CHECK(!greg.dateFromParts(2024, 2, 30).isValid() && !greg.partsFromDate(QDate()).isValid() &&
          !QCalendar::YearMonthDay().isValid());

    const QDate d(1403, 12, 30, jalali);
    CHECK(d == QDate(2025, 3, 20) && d.year(jalali) == 1403 && d.month(jalali) == 12 && d.day(jalali) == 30);
    CHECK(d.dayOfYear(jalali) == 366 && d.daysInMonth(jalali) == 30 && d.daysInYear(jalali) == 366);
    CHECK(d.addYears(1, jalali) == QDate(1404, 12, 29, jalali) && d.addMonths(1, jalali) == QDate(1404, 1, 30, jalali));
    CHECK(QDate(2024, 1, 31).addMonths(1, greg) == QDate(2024, 2, 29) &&
          QDate(1, 6, 1, julian).addYears(-1, julian).year(julian) == -1);
    QDate e;
    CHECK(e.setDate(1445, 9, 1, islamic) && e == QDate(2024, 3, 11) && !e.setDate(1445, 13, 1, islamic) &&
          !e.isValid());
    CHECK(QDate().year(julian) == 0 && QDate().addMonths(1, julian).isNull());
}

// ------------------------------------------------------------------ QEasingCurve

static bool near(qreal a, qreal b) { return std::fabs(a - b) < 1e-6; }

static qreal halfway(qreal t) { return t < 0.5 ? 0 : 1; }

static void testEasingCurve() {
    QEasingCurve linear;
    CHECK(linear.type() == QEasingCurve::Linear && near(linear.valueForProgress(0.3), 0.3) &&
          near(linear.valueForProgress(2), 1));
    CHECK(near(QEasingCurve(QEasingCurve::InQuad).valueForProgress(0.5), 0.25) &&
          near(QEasingCurve(QEasingCurve::OutQuad).valueForProgress(0.5), 0.75));
    CHECK(near(QEasingCurve(QEasingCurve::InOutQuad).valueForProgress(0.25), 0.125) &&
          near(QEasingCurve(QEasingCurve::OutInQuad).valueForProgress(0.25), 0.375));
    CHECK(near(QEasingCurve(QEasingCurve::InCubic).valueForProgress(0.5), 0.125) &&
          near(QEasingCurve(QEasingCurve::InQuint).valueForProgress(0.5), 1.0 / 32));
    CHECK(near(QEasingCurve(QEasingCurve::InOutSine).valueForProgress(0.5), 0.5) &&
          near(QEasingCurve(QEasingCurve::OutSine).valueForProgress(1.0 / 3), 0.5));
    CHECK(near(QEasingCurve(QEasingCurve::InExpo).valueForProgress(0.9), 0.5) &&
          near(QEasingCurve(QEasingCurve::OutCirc).valueForProgress(1), 1));
    for (int t = QEasingCurve::Linear; t < QEasingCurve::SineCurve; ++t) {
        const QEasingCurve c(static_cast<QEasingCurve::Type>(t));
        CHECK(near(c.valueForProgress(0), 0) && near(c.valueForProgress(1), 1));
    }
    QEasingCurve back(QEasingCurve::InBack);
    CHECK(back.valueForProgress(0.2) < 0 && near(back.overshoot(), 1.70158));
    back.setOvershoot(0);
    CHECK(near(back.valueForProgress(0.5), 0.125)); // no overshoot: a cubic
    QEasingCurve elastic(QEasingCurve::OutElastic);
    CHECK(elastic.valueForProgress(0.1) > 1 && near(elastic.amplitude(), 1) && near(elastic.period(), 0.3));
    elastic.setPeriod(0.5);
    elastic.setAmplitude(2);
    CHECK(near(elastic.period(), 0.5) && near(elastic.amplitude(), 2));
    QEasingCurve bounce(QEasingCurve::OutBounce);
    CHECK(near(bounce.valueForProgress(4 / 11.0), 1) && bounce.valueForProgress(0.8) < 1);
    bounce.setAmplitude(0.5);
    CHECK(bounce.valueForProgress(0.8) > QEasingCurve(QEasingCurve::OutBounce).valueForProgress(0.8));
    CHECK(near(QEasingCurve(QEasingCurve::SineCurve).valueForProgress(0.5), 1) &&
          near(QEasingCurve(QEasingCurve::SineCurve).valueForProgress(1), 0));
    CHECK(near(QEasingCurve(QEasingCurve::CosineCurve).valueForProgress(0), 0.5));

    QEasingCurve bezier(QEasingCurve::BezierSpline);
    CHECK(near(bezier.valueForProgress(0.4), 0.4));
    bezier.addCubicBezierSegment(QPointF(0.25, 0.25), QPointF(0.5, 0.5), QPointF(0.5, 0.5));
    bezier.addCubicBezierSegment(QPointF(0.5, 1), QPointF(1, 1), QPointF(1, 1));
    CHECK(near(bezier.valueForProgress(0.25), 0.25) && bezier.valueForProgress(0.75) > 0.75 &&
          near(bezier.valueForProgress(1), 1));
    CHECK(bezier.toCubicSpline().size() == 6 && bezier.toCubicSpline().at(2) == QPointF(0.5, 0.5));
    QEasingCurve tcb(QEasingCurve::TCBSpline);
    tcb.addTCBSegment(QPointF(0.4, 0.6), 0, 0, 0);
    tcb.addTCBSegment(QPointF(1, 1), 0, 0, 0);
    CHECK(near(tcb.valueForProgress(0.4), 0.6) && near(tcb.valueForProgress(1), 1) && tcb.toCubicSpline().size() == 6);

    QEasingCurve custom;
    custom.setCustomType(halfway);
    CHECK(custom.type() == QEasingCurve::Custom && custom.customType() == halfway && custom.valueForProgress(0.7) == 1);
    QEasingCurve noCustom;
    noCustom.setType(QEasingCurve::Custom);
    CHECK(noCustom.type() == QEasingCurve::Linear && noCustom.customType() == nullptr);
    CHECK(QEasingCurve(QEasingCurve::InQuad) == QEasingCurve(QEasingCurve::InQuad) &&
          QEasingCurve(QEasingCurve::InQuad) != linear);
    QEasingCurve s1(QEasingCurve::OutBack), s2;
    s1.swap(s2);
    CHECK(s1.type() == QEasingCurve::Linear && s2.type() == QEasingCurve::OutBack);
    QString text;
    QDebug(&text) << QEasingCurve(QEasingCurve::InQuad);
    CHECK(text.startsWith(u"QEasingCurve(type=1"_s));
}

// ------------------------------------------------------------------ QChronoTimer and QTimeLine

template <typename Pred> static bool spinUntil(Pred pred, int timeoutMs = 5000) {
    QElapsedTimer t;
    t.start();
    while (!pred() && t.elapsed() < timeoutMs)
        QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    return pred();
}

static void testChronoTimer() {
    using namespace std::chrono_literals;
    QChronoTimer timer(20ms);
    CHECK(timer.interval() == 20ms && !timer.isActive() && timer.remainingTime() == -1ns && timer.id() == 0);
    int fired = 0;
    QObject::connect(&timer, &QChronoTimer::timeout, [&] { ++fired; });
    timer.setSingleShot(true);
    CHECK(timer.isSingleShot());
    timer.start();
    CHECK(timer.isActive() && timer.id() != 0 && timer.remainingTime() <= 20ms);
    CHECK(spinUntil([&] { return fired == 1; }) && !timer.isActive());
    timer.setSingleShot(false);
    timer.setInterval(5ms);
    timer.start();
    CHECK(spinUntil([&] { return fired >= 4; }));
    timer.stop();
    CHECK(!timer.isActive());
    timer.setTimerType(Qt::PreciseTimer);
    CHECK(timer.timerType() == Qt::PreciseTimer);
    QChronoTimer sub(QChronoTimer(1500us).interval());
    CHECK(sub.interval() == 1500us);
}

static void testTimeLine() {
    QTimeLine line(120);
    CHECK(line.duration() == 120 && line.state() == QTimeLine::NotRunning && line.loopCount() == 1 &&
          line.direction() == QTimeLine::Forward);
    CHECK(line.easingCurve().type() == QEasingCurve::InOutSine && line.updateInterval() == 40);
    line.setFrameRange(0, 10);
    line.setUpdateInterval(10);
    line.setEasingCurve(QEasingCurve::Linear);
    CHECK(line.startFrame() == 0 && line.endFrame() == 10 && line.frameForTime(60) == 5 &&
          near(line.valueForTime(30), 0.25));
    QList<int> frames;
    QList<QTimeLine::State> states;
    int finished = 0;
    QObject::connect(&line, &QTimeLine::frameChanged, [&](int f) { frames << f; });
    QObject::connect(&line, &QTimeLine::stateChanged, [&](QTimeLine::State s) { states << s; });
    QObject::connect(&line, &QTimeLine::finished, [&] { ++finished; });
    line.start();
    CHECK(line.state() == QTimeLine::Running);
    CHECK(spinUntil([&] { return finished == 1; }));
    CHECK(line.state() == QTimeLine::NotRunning && line.currentTime() == 120 && line.currentFrame() == 10 &&
          frames.last() == 10);
    CHECK((states == QList<QTimeLine::State>{QTimeLine::Running, QTimeLine::NotRunning}));

    line.setDirection(QTimeLine::Backward);
    line.setLoopCount(2);
    line.start();
    CHECK(line.currentTime() == 120);
    CHECK(spinUntil([&] { return finished == 2; }) && line.currentTime() == 0 && line.currentFrame() == 0);

    line.setDirection(QTimeLine::Forward);
    line.setCurrentTime(30);
    CHECK(line.currentTime() == 30 && near(line.currentValue(), 0.25));
    line.setCurrentTime(150); // past one loop of two: 30 into the second loop
    CHECK(line.currentTime() == 30);
    line.toggleDirection();
    CHECK(line.direction() == QTimeLine::Backward);
    line.setLoopCount(0);
    line.start();
    line.setPaused(true);
    CHECK(line.state() == QTimeLine::Paused);
    line.setPaused(false);
    CHECK(line.state() == QTimeLine::Running);
    line.stop();
    CHECK(line.state() == QTimeLine::NotRunning && finished == 2);
    line.setDuration(0); // rejected
    CHECK(line.duration() == 120);
}

int main(int argc, char **argv) {
    QTemporaryDir tzdir;
    if (!tzdir.isValid())
        return 1;
    const QString dir = tzdir.path();
    writeFile(dir + u"/Test/Berlin"_s, QByteArray(reinterpret_cast<const char *>(kBerlin), sizeof kBerlin));
    writeFile(dir + u"/Test/Alias"_s, QByteArray(reinterpret_cast<const char *>(kBerlin), sizeof kBerlin));
    writeFile(dir + u"/Test/South"_s, QByteArray(reinterpret_cast<const char *>(kSouth), sizeof kSouth));
    writeFile(dir + u"/Test/Kolkata"_s, QByteArray(reinterpret_cast<const char *>(kKolkata), sizeof kKolkata));
    writeFile(dir + u"/zone.tab"_s,
              "# comment\nDE\t+5230+01322\tTest/Berlin\tmost of Testland\nIN\t+2232+08822\tTest/Kolkata\n");
    writeFile(dir + u"/tzdata.zi"_s, "# version test\nZ Test/Berlin 0:53:28 - LMT 1893 Ap\nZ Test/South 10 - AEST\nZ "
                                     "Test/Kolkata 5:30 - IST\nL Test/Berlin Test/Alias\n");
    setenv("TZDIR", dir.toLocal8Bit().constData(), 1);
    setenv("TZ", "Test/Berlin", 1);
    QCoreApplication app(argc, argv);

    testTimeZoneBasics();
    testFixedAndSpecialZones();
    testDateTimeWithZones();
    testCalendar();
    testEasingCurve();
    testChronoTimer();
    testTimeLine();
    if (g_failures == 0)
        std::puts("all date and time tests passed");
    return g_failures == 0 ? 0 : 1;
}
