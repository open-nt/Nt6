// MIT License - see LICENSE
#include <QtCore/qdatetime.h>
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
    const bool twelve = f.contains(QChar(u'a'), Qt::CaseInsensitive) && (f.contains(QString("AP")) || f.contains(QString("ap")));
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
    while (pos < s.size() && digits < maxDigits && s[pos].isDigit()) {
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
                if (!readInt(s, pos, run, 2, &p->day))
                    return false;
            } else if (!matchName(s, pos, kDays, 7, run == 3 ? 3 : 0)) {
                return false;
            }
        } else if (u == u'M') {
            run = qMin<qsizetype>(run, 4);
            if (run <= 2) {
                if (!readInt(s, pos, run, 2, &p->month))
                    return false;
            } else if (!(p->month = matchName(s, pos, kMonths, 12, run == 3 ? 3 : 0))) {
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
            if (!readInt(s, pos, run, 2, &p->hour))
                return false;
            p->hour12 = u == u'h';
        } else if (u == u'm') {
            run = qMin<qsizetype>(run, 2);
            if (!readInt(s, pos, run, 2, &p->minute))
                return false;
        } else if (u == u's') {
            run = qMin<qsizetype>(run, 2);
            if (!readInt(s, pos, run, 2, &p->second))
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
            while (pos < s.size() && s[pos].isDigit()) {
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
    struct tm t;
    localtime_r(&seconds, &t);
    return static_cast<int>(t.tm_gmtoff);
}

} // namespace

// QDate

QDate::QDate(int y, int m, int d) { setDate(y, m, d); }

bool QDate::isLeapYear(int y) {
    const int a = toAstronomical(y);
    return a % 4 == 0 && (a % 100 != 0 || a % 400 == 0);
}

bool QDate::isValid(int y, int m, int d) {
    return y != 0 && m >= 1 && m <= 12 && d >= 1 && d <= monthLength(y, m);
}

bool QDate::setDate(int y, int m, int d) {
    if (!isValid(y, m, d)) {
        jd = kNullJd;
        return false;
    }
    jd = julianFromCivil(toAstronomical(y), m, d);
    return true;
}

void QDate::getDate(int *y, int *m, int *d) const {
    if (isValid()) {
        civilFromJulian(jd, y, m, d);
        return;
    }
    if (y) *y = 0;
    if (m) *m = 0;
    if (d) *d = 0;
}

int QDate::year() const { int v = 0; getDate(&v, nullptr, nullptr); return v; }
int QDate::month() const { int v = 0; getDate(nullptr, &v, nullptr); return v; }
int QDate::day() const { int v = 0; getDate(nullptr, nullptr, &v); return v; }
int QDate::dayOfWeek() const { return isValid() ? static_cast<int>(floorMod(jd, 7)) + 1 : 0; }
int QDate::dayOfYear() const { return isValid() ? static_cast<int>(jd - QDate(year(), 1, 1).jd) + 1 : 0; }
int QDate::daysInMonth() const { return isValid() ? monthLength(year(), month()) : 0; }
int QDate::daysInYear() const { return isValid() ? (isLeapYear(year()) ? 366 : 365) : 0; }

int QDate::weekNumber(int *yearNumber) const {
    if (!isValid())
        return 0;
    const QDate thursday = addDays(4 - dayOfWeek());
    if (yearNumber)
        *yearNumber = thursday.year();
    return (thursday.dayOfYear() - 1) / 7 + 1;
}

QDate QDate::addDays(qint64 days) const { return isValid() ? fromJulianDay(jd + days) : QDate(); }

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

QDate QDate::addYears(int years) const {
    if (!isValid())
        return QDate();
    int y, m, d;
    getDate(&y, &m, &d);
    const int ny = fromAstronomical(qint64(toAstronomical(y)) + years);
    return QDate(ny, m, qMin(d, monthLength(ny, m)));
}

qint64 QDate::daysTo(QDate d) const { return isValid() && d.isValid() ? d.jd - jd : 0; }

QDate QDate::fromJulianDay(qint64 julianDay) {
    QDate d;
    d.jd = julianDay;
    return d;
}

QDate QDate::currentDate() { return QDateTime::currentDateTime().date(); }

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

QString QDate::toString(const QString &format) const { return isValid() ? formatTokens(format, *this, QTime()) : QString(); }

QDate QDate::fromString(const QString &string, Qt::DateFormat format) {
    switch (format) {
    case Qt::ISODate:
    case Qt::ISODateWithMs:
        return fromString(string.left(10), QString("yyyy-MM-dd"));
    case Qt::RFC2822Date:
        return fromString(string, QString("d MMM yyyy"));
    default:
        return fromString(string, QString("ddd MMM d yyyy"));
    }
}

QDate QDate::fromString(const QString &string, const QString &format) {
    Parsed p;
    return parseTokens(string, format, &p) ? QDate(p.year, p.month, p.day) : QDate();
}

// QTime

QTime::QTime(int h, int m, int s, int msecs) { setHMS(h, m, s, msecs); }

bool QTime::isValid(int h, int m, int s, int msecs) {
    return h >= 0 && h < 24 && m >= 0 && m < 60 && s >= 0 && s < 60 && msecs >= 0 && msecs < 1000;
}

bool QTime::setHMS(int h, int m, int s, int msecs) {
    if (!isValid(h, m, s, msecs)) {
        ms = -2; // invalid, but not null
        return false;
    }
    ms = (h * 3600 + m * 60 + s) * 1000 + msecs;
    return true;
}

QTime QTime::addSecs(int secs) const { return addMSecs(static_cast<int>(floorMod(qint64(secs) * 1000, kMsPerDay))); }

QTime QTime::addMSecs(int msecs) const {
    if (!isValid())
        return QTime();
    return fromMSecsSinceStartOfDay(static_cast<int>(floorMod(qint64(ms) + msecs, kMsPerDay)));
}

int QTime::secsTo(QTime t) const { return isValid() && t.isValid() ? t.ms / 1000 - ms / 1000 : 0; }
int QTime::msecsTo(QTime t) const { return isValid() && t.isValid() ? t.ms - ms : 0; }

QTime QTime::fromMSecsSinceStartOfDay(int msecs) {
    QTime t;
    t.ms = msecs >= 0 && msecs < kMsPerDay ? msecs : -2;
    return t;
}

QTime QTime::currentTime() { return QDateTime::currentDateTime().time(); }

QString QTime::toString(Qt::DateFormat format) const {
    if (!isValid())
        return QString();
    return toString(format == Qt::ISODateWithMs ? QString("HH:mm:ss.zzz") : QString("HH:mm:ss"));
}

QString QTime::toString(const QString &format) const { return isValid() ? formatTokens(format, QDate(), *this) : QString(); }

QTime QTime::fromString(const QString &string, Qt::DateFormat format) {
    Q_UNUSED(format)
    QTime t;
    return parseIsoTime(string, 0, &t) == string.size() ? t : QTime();
}

QTime QTime::fromString(const QString &string, const QString &format) {
    Parsed p;
    p.year = 1900;
    return parseTokens(string, format, &p) ? QTime(p.hour, p.minute, p.second, p.ms) : QTime();
}

// QDateTime

QDateTime::QDateTime(QDate date, QTime time, Qt::TimeSpec spec, int offsetSeconds)
    : m_date(date), m_time(date.isValid() && !time.isValid() && time.isNull() ? QTime(0, 0) : time), m_spec(spec) {
    if (spec == Qt::OffsetFromUTC)
        m_offset = offsetSeconds;
    if (spec == Qt::OffsetFromUTC && offsetSeconds == 0)
        m_spec = Qt::UTC;
}

qint64 QDateTime::toMSecsSinceEpoch() const {
    if (!isValid())
        return 0;
    const qint64 fields = (m_date.toJulianDay() - kEpochJd) * kMsPerDay + m_time.msecsSinceStartOfDay();
    if (m_spec == Qt::UTC)
        return fields;
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

qint64 QDateTime::toSecsSinceEpoch() const { return floorDiv(toMSecsSinceEpoch(), 1000); }

int QDateTime::offsetFromUtc() const {
    if (m_spec == Qt::UTC)
        return 0;
    if (m_spec == Qt::OffsetFromUTC)
        return m_offset;
    return isValid() ? systemOffsetAt(static_cast<time_t>(toSecsSinceEpoch())) : 0;
}

void QDateTime::setOffsetFromUtc(int offsetSeconds) {
    m_spec = offsetSeconds == 0 ? Qt::UTC : Qt::OffsetFromUTC;
    m_offset = offsetSeconds;
}

void QDateTime::setMSecsSinceEpoch(qint64 msecs) {
    qint64 local = msecs;
    if (m_spec == Qt::OffsetFromUTC)
        local += qint64(m_offset) * 1000;
    else if (m_spec == Qt::LocalTime)
        local += qint64(systemOffsetAt(static_cast<time_t>(floorDiv(msecs, 1000)))) * 1000;
    m_date = QDate::fromJulianDay(kEpochJd + floorDiv(local, kMsPerDay));
    m_time = QTime::fromMSecsSinceStartOfDay(static_cast<int>(floorMod(local, kMsPerDay)));
}

QDateTime QDateTime::addDays(qint64 days) const { QDateTime r = *this; r.m_date = m_date.addDays(days); return r; }
QDateTime QDateTime::addMonths(int months) const { QDateTime r = *this; r.m_date = m_date.addMonths(months); return r; }
QDateTime QDateTime::addYears(int years) const { QDateTime r = *this; r.m_date = m_date.addYears(years); return r; }

QDateTime QDateTime::addMSecs(qint64 msecs) const {
    if (!isValid())
        return QDateTime();
    QDateTime r = *this;
    r.setMSecsSinceEpoch(toMSecsSinceEpoch() + msecs);
    return r;
}

QDateTime QDateTime::toTimeSpec(Qt::TimeSpec spec) const {
    if (spec == Qt::OffsetFromUTC)
        return toOffsetFromUtc(m_offset);
    return fromMSecsSinceEpoch(toMSecsSinceEpoch(), spec);
}

QDateTime QDateTime::toOffsetFromUtc(int offsetSeconds) const {
    return fromMSecsSinceEpoch(toMSecsSinceEpoch(), Qt::OffsetFromUTC, offsetSeconds);
}

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

QString QDateTime::toString(const QString &format) const { return isValid() ? formatTokens(format, m_date, m_time, this) : QString(); }

qint64 QDateTime::currentMSecsSinceEpoch() noexcept {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return qint64(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

qint64 QDateTime::currentSecsSinceEpoch() noexcept { return currentMSecsSinceEpoch() / 1000; }

QDateTime QDateTime::currentDateTime() { return fromMSecsSinceEpoch(currentMSecsSinceEpoch()); }

QDateTime QDateTime::currentDateTimeUtc() { return fromMSecsSinceEpoch(currentMSecsSinceEpoch(), Qt::UTC); }

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

QDateTime QDateTime::fromSecsSinceEpoch(qint64 secs, Qt::TimeSpec spec, int offsetFromUtc) {
    return fromMSecsSinceEpoch(secs * 1000, spec, offsetFromUtc);
}

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
        const QDateTime fields = fromString(s.left(sp), QString("d MMM yyyy hh:mm:ss"));
        const QString zone = s.mid(sp + 1);
        if (!fields.isValid() || zone.size() != 5)
            return QDateTime();
        const int v = zone.mid(1).toInt();
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

QDateTime QDateTime::fromString(const QString &string, const QString &format) {
    Parsed p;
    if (!parseTokens(string, format, &p))
        return QDateTime();
    return QDateTime(QDate(p.year, p.month, p.day), QTime(p.hour, p.minute, p.second, p.ms));
}
