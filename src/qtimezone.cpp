// MIT License - see LICENSE
#include <QtCore/qtimezone.h>

#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qhash.h>

#include "qtimezone_p.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <mutex>
#include <set>
#include <unistd.h>

namespace {

constexpr qint64 kEpochJd = 2440588; // 1970-01-01

qint64 floorDiv(qint64 a, qint64 b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0); }

qint32 readI32(const uchar *p) {
    return static_cast<qint32>((static_cast<quint32>(p[0]) << 24) | (static_cast<quint32>(p[1]) << 16) |
                               (static_cast<quint32>(p[2]) << 8) | p[3]);
}
qint64 readI64(const uchar *p) {
    quint64 v = 0;
    for (int i = 0; i < 8; ++i)
        v = (v << 8) | p[i];
    return static_cast<qint64>(v);
}

// Days since the epoch of the given day of a year under a POSIX rule date.
qint64 ruleDay(int year, const QTimeZonePrivate::RuleDate &rd) {
    const QDate jan1(year, 1, 1);
    QDate date;
    switch (rd.form) {
    case QTimeZonePrivate::RuleDate::Form::MonthWeekDay: {
        const QDate first(year, rd.month, 1);
        const int firstDow = first.dayOfWeek() % 7; // 0 = Sunday, as in POSIX
        int day = 1 + (rd.weekday - firstDow + 7) % 7 + (rd.week - 1) * 7;
        while (day > first.daysInMonth())
            day -= 7;
        date = QDate(year, rd.month, day);
        break;
    }
    case QTimeZonePrivate::RuleDate::Form::JulianNoLeap:
        date = jan1.addDays(rd.day - 1 + (QDate::isLeapYear(year) && rd.day >= 60 ? 1 : 0));
        break;
    case QTimeZonePrivate::RuleDate::Form::ZeroBasedDay:
        date = jan1.addDays(rd.day);
        break;
    }
    return date.toJulianDay() - kEpochJd;
}

int yearOf(qint64 secs) { return QDate::fromJulianDay(kEpochJd + floorDiv(secs, 86400)).year(); }

// The UTC instants (seconds) at which daylight time starts and ends in a year.
std::pair<qint64, qint64> ruleTransitions(const QTimeZonePrivate::Rule &r, int year) {
    const qint64 start = ruleDay(year, r.start) * 86400 + r.start.time - r.standard.offset;
    const qint64 end = ruleDay(year, r.end) * 86400 + r.end.time - r.daylight.offset;
    return {start, end};
}

QTimeZonePrivate::Offsets typeOffsets(const QTimeZonePrivate::Type &t, int standard) {
    return {t.offset, standard, t.dst, t.abbreviation};
}

QTimeZonePrivate::Offsets ruleOffsets(const QTimeZonePrivate::Rule &r, qint64 utc) {
    if (!r.hasDst)
        return typeOffsets(r.standard, r.standard.offset);
    const int year = yearOf(utc + r.standard.offset);
    const auto [start, end] = ruleTransitions(r, year);
    const bool dst = start < end ? (utc >= start && utc < end) : (utc < end || utc >= start);
    return dst ? typeOffsets(r.daylight, r.standard.offset) : typeOffsets(r.standard, r.standard.offset);
}

// --- POSIX TZ rules

struct RuleParser {
    const std::string &s;
    std::size_t i = 0;

    bool name(std::string *out) {
        if (i < s.size() && s[i] == '<') {
            const std::size_t close = s.find('>', i);
            if (close == std::string::npos)
                return false;
            *out = s.substr(i + 1, close - i - 1);
            i = close + 1;
            return out->size() >= 3;
        }
        const std::size_t start = i;
        while (i < s.size() && ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z')))
            ++i;
        *out = s.substr(start, i - start);
        return out->size() >= 3;
    }
    bool number(int *out, int maxDigits) {
        const std::size_t start = i;
        int v = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9' && i - start < static_cast<std::size_t>(maxDigits))
            v = v * 10 + (s[i++] - '0');
        *out = v;
        return i > start;
    }
    // [+-]hh[:mm[:ss]], returned in seconds.
    bool time(int *out, int maxHours) {
        int sign = 1;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
            sign = s[i] == '-' ? -1 : 1;
            ++i;
        }
        int h = 0, m = 0, sec = 0;
        if (!number(&h, 3) || h > maxHours)
            return false;
        if (i < s.size() && s[i] == ':') {
            ++i;
            if (!number(&m, 2) || m > 59)
                return false;
            if (i < s.size() && s[i] == ':') {
                ++i;
                if (!number(&sec, 2) || sec > 59)
                    return false;
            }
        }
        *out = sign * (h * 3600 + m * 60 + sec);
        return true;
    }
    // Consumes the character c if it is next.
    bool accept(char c) {
        if (i >= s.size() || s[i] != c)
            return false;
        ++i;
        return true;
    }
    bool date(QTimeZonePrivate::RuleDate *d) {
        if (i < s.size() && s[i] == 'M') {
            ++i;
            d->form = QTimeZonePrivate::RuleDate::Form::MonthWeekDay;
            if (!number(&d->month, 2) || d->month < 1 || d->month > 12 || !accept('.'))
                return false;
            if (!number(&d->week, 1) || d->week < 1 || d->week > 5 || !accept('.'))
                return false;
            if (!number(&d->weekday, 1) || d->weekday > 6)
                return false;
        } else if (i < s.size() && s[i] == 'J') {
            ++i;
            d->form = QTimeZonePrivate::RuleDate::Form::JulianNoLeap;
            if (!number(&d->day, 3) || d->day < 1 || d->day > 365)
                return false;
        } else {
            d->form = QTimeZonePrivate::RuleDate::Form::ZeroBasedDay;
            if (!number(&d->day, 3) || d->day > 365)
                return false;
        }
        d->time = 7200;
        if (i < s.size() && s[i] == '/') {
            ++i;
            if (!time(&d->time, 167))
                return false;
        }
        return true;
    }
};

std::mutex cacheMutex;
std::map<QString, std::shared_ptr<const QTimeZonePrivate>> &cache() {
    static std::map<QString, std::shared_ptr<const QTimeZonePrivate>> c;
    return c;
}

struct ZoneTabEntry {
    QString code;
    QString comment;
};

// zone.tab, keyed by zone ID.
std::map<QByteArray, ZoneTabEntry> readZoneTab() {
    std::map<QByteArray, ZoneTabEntry> r;
    QFile f(QTimeZonePrivate::zoneDirectory() + QString(u"/zone.tab"));
    if (!f.open(QIODevice::ReadOnly))
        return r;
    for (const QByteArray &line : f.readAll().split('\n')) {
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        const QList<QByteArray> fields = line.split('\t');
        if (fields.size() < 3)
            continue;
        r[fields.at(2)] = {QString::fromUtf8(fields.at(0)),
                           fields.size() > 3 ? QString::fromUtf8(fields.at(3)) : QString()};
    }
    return r;
}

bool validId(const QByteArray &id) {
    if (id.isEmpty() || id.startsWith('/') || id.contains("..") || id.size() > 128)
        return false;
    for (char c : id) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '/' ||
                        c == '_' || c == '-' || c == '+' || c == '.';
        if (!ok)
            return false;
    }
    return true;
}

// Parses "UTC", "GMT" and "UTC+hh[:mm[:ss]]" IDs into an offset.
bool parseOffsetId(const QByteArray &id, int *offset) {
    if (id == "UTC" || id == "GMT" || id == "UTC+00:00" || id == "UTC-00:00") {
        *offset = 0;
        return true;
    }
    if (!id.startsWith("UTC") || id.size() < 5 || (id.at(3) != '+' && id.at(3) != '-'))
        return false;
    const int sign = id.at(3) == '-' ? -1 : 1;
    const QList<QByteArray> parts = id.mid(4).split(':');
    if (parts.isEmpty() || parts.size() > 3)
        return false;
    int values[3] = {0, 0, 0};
    for (qsizetype k = 0; k < parts.size(); ++k) {
        bool ok = false;
        values[k] = parts.at(k).toInt(&ok);
        if (!ok || parts.at(k).isEmpty() || parts.at(k).size() > 2 || values[k] < 0 || (k > 0 && values[k] > 59))
            return false;
    }
    *offset = sign * (values[0] * 3600 + values[1] * 60 + values[2]);
    return *offset >= QTimeZone::MinUtcOffsetSecs && *offset <= QTimeZone::MaxUtcOffsetSecs;
}

std::shared_ptr<QTimeZonePrivate> parseTzif(const QByteArray &data) {
    const auto *p = reinterpret_cast<const uchar *>(data.constData());
    const qsizetype n = data.size();
    if (n < 44 || std::memcmp(p, "TZif", 4) != 0)
        return nullptr;
    auto counts = [&](qsizetype at, qint32 c[6]) {
        for (int k = 0; k < 6; ++k)
            c[k] = readI32(p + at + 20 + 4 * k);
    };
    qint32 c[6]; // isutcnt, isstdcnt, leapcnt, timecnt, typecnt, charcnt
    counts(0, c);
    const char version = static_cast<char>(p[4]);
    qsizetype at = 44;
    int timeSize = 4;
    if (version >= '2') {
        // Skip the 32-bit block and use the 64-bit one.
        const qsizetype v1 = qsizetype(c[3]) * 5 + qsizetype(c[4]) * 6 + c[5] + qsizetype(c[2]) * 8 + c[1] + c[0];
        at = 44 + v1;
        if (at + 44 > n || std::memcmp(p + at, "TZif", 4) != 0)
            return nullptr;
        counts(at, c);
        at += 44;
        timeSize = 8;
    }
    for (int k = 0; k < 6; ++k)
        if (c[k] < 0)
            return nullptr;
    const qsizetype need =
        qsizetype(c[3]) * (timeSize + 1) + qsizetype(c[4]) * 6 + c[5] + qsizetype(c[2]) * (timeSize + 4) + c[1] + c[0];
    if (c[4] == 0 || at + need > n)
        return nullptr;
    auto z = std::make_shared<QTimeZonePrivate>();
    z->kind = QTimeZonePrivate::Kind::Tzif;
    z->transitions.resize(static_cast<std::size_t>(c[3]));
    for (qint32 k = 0; k < c[3]; ++k)
        z->transitions[static_cast<std::size_t>(k)] = timeSize == 8 ? readI64(p + at + k * 8) : readI32(p + at + k * 4);
    at += qsizetype(c[3]) * timeSize;
    z->transitionTypes.assign(p + at, p + at + c[3]);
    at += c[3];
    const qsizetype typesAt = at;
    const qsizetype charsAt = at + qsizetype(c[4]) * 6;
    for (qint32 k = 0; k < c[4]; ++k) {
        const uchar *t = p + typesAt + k * 6;
        QTimeZonePrivate::Type type;
        type.offset = readI32(t);
        type.dst = t[4] != 0;
        const int idx = t[5];
        if (idx < c[5])
            type.abbreviation = std::string(reinterpret_cast<const char *>(p + charsAt + idx));
        z->types.push_back(type);
    }
    for (unsigned char &t : z->transitionTypes)
        if (t >= z->types.size())
            t = 0;
    at += need - qsizetype(c[3]) * (timeSize + 1);
    if (version >= '2' && at < n && p[at] == '\n') {
        const qsizetype end = data.indexOf('\n', at + 1);
        if (end > at) {
            const std::string footer(data.constData() + at + 1, static_cast<std::size_t>(end - at - 1));
            QTimeZonePrivate::parseRule(footer, &z->rule);
        }
    }
    return z;
}

QDateTime utcDateTime(qint64 secs) { return QDateTime::fromMSecsSinceEpoch(secs * 1000, Qt::UTC); }

QTimeZone::OffsetData toOffsetData(qint64 secs, const QTimeZonePrivate::Offsets &o) {
    QTimeZone::OffsetData d;
    d.abbreviation = QString::fromStdString(o.abbreviation);
    d.atUtc = utcDateTime(secs);
    d.offsetFromUtc = o.offset;
    d.standardTimeOffset = o.standardOffset;
    d.daylightTimeOffset = o.offset - o.standardOffset;
    return d;
}

} // namespace

// ------------------------------------------------------------------ QTimeZonePrivate

bool QTimeZonePrivate::parseRule(const std::string &text, Rule *rule) {
    Rule r;
    RuleParser p{text};
    if (text.empty() || !p.name(&r.standard.abbreviation))
        return false;
    int stdPosix = 0;
    if (!p.time(&stdPosix, 24))
        return false;
    r.standard.offset = -stdPosix; // POSIX offsets are positive west of Greenwich
    if (p.i < text.size()) {
        if (!p.name(&r.daylight.abbreviation))
            return false;
        r.hasDst = true;
        r.daylight.dst = true;
        r.daylight.offset = r.standard.offset + 3600;
        if (p.i < text.size() && text[p.i] != ',') {
            int dstPosix = 0;
            if (!p.time(&dstPosix, 24))
                return false;
            r.daylight.offset = -dstPosix;
        }
        if (p.i < text.size()) {
            if (!p.accept(',') || !p.date(&r.start) || !p.accept(',') || !p.date(&r.end))
                return false;
        } else {
            // No dates: the US rules, which POSIX uses as the default.
            r.start = {RuleDate::Form::MonthWeekDay, 3, 2, 0, 0, 7200};
            r.end = {RuleDate::Form::MonthWeekDay, 11, 1, 0, 0, 7200};
        }
    }
    if (p.i != text.size())
        return false;
    r.valid = true;
    *rule = r;
    return true;
}

QTimeZonePrivate::Offsets QTimeZonePrivate::offsetsAt(qint64 utcSecs) const {
    if (kind == Kind::Utc)
        return {0, 0, false, "UTC"};
    if (kind == Kind::Fixed) {
        const std::string abbr =
            abbreviationOverride.isEmpty() ? offsetName(fixedOffset).toStdString() : abbreviationOverride.toStdString();
        return {fixedOffset, fixedOffset, false, abbr};
    }
    if (transitions.empty()) {
        if (rule.valid)
            return ruleOffsets(rule, utcSecs);
        return typeOffsets(types.empty() ? Type() : types[0], types.empty() ? 0 : types[0].offset);
    }
    if (utcSecs < transitions.front()) {
        const Type &t = types[0];
        return typeOffsets(t, t.offset);
    }
    if (utcSecs >= transitions.back() && rule.valid)
        return ruleOffsets(rule, utcSecs);
    const auto it = std::upper_bound(transitions.begin(), transitions.end(), utcSecs);
    std::size_t idx = static_cast<std::size_t>(it - transitions.begin()) - 1;
    const Type &t = types[transitionTypes[idx]];
    int standard = t.offset;
    if (t.dst) {
        standard = t.offset - 3600;
        for (std::size_t k = idx + 1; k-- > 0;) {
            const Type &prev = types[transitionTypes[k]];
            if (!prev.dst) {
                standard = prev.offset;
                break;
            }
        }
    }
    return typeOffsets(t, standard);
}

qint64 QTimeZonePrivate::toUtc(qint64 localSecs) const {
    const int before = offsetsAt(localSecs - 18 * 3600).offset;
    const int after = offsetsAt(localSecs + 18 * 3600).offset;
    const int here = offsetsAt(localSecs).offset;
    std::vector<int> candidates{before, after, here};
    std::sort(candidates.begin(), candidates.end(), std::greater<int>()); // larger offset = earlier instant
    for (int o : candidates)
        if (offsetsAt(localSecs - o).offset == o)
            return localSecs - o;
    return localSecs - before; // in a gap: use the offset before it, moving the time forward
}

std::vector<std::pair<qint64, QTimeZonePrivate::Offsets>> QTimeZonePrivate::transitionsBetween(qint64 fromSecs,
                                                                                               qint64 toSecs) const {
    std::vector<std::pair<qint64, Offsets>> r;
    if (kind == Kind::Utc || kind == Kind::Fixed || fromSecs > toSecs)
        return r;
    for (qint64 t : transitions)
        if (t >= fromSecs && t <= toSecs)
            r.emplace_back(t, offsetsAt(t));
    if (rule.valid && rule.hasDst) {
        const qint64 last = transitions.empty() ? std::numeric_limits<qint64>::min() : transitions.back();
        const int firstYear =
            std::max(yearOf(fromSecs) - 1, transitions.empty() ? yearOf(fromSecs) - 1 : yearOf(last) - 1);
        const int lastYear = std::min(yearOf(toSecs) + 1, firstYear + 2000);
        std::vector<qint64> generated;
        for (int y = firstYear; y <= lastYear; ++y) {
            const auto [start, end] = ruleTransitions(rule, y);
            for (qint64 t : {start, end})
                if (t > last && t >= fromSecs && t <= toSecs)
                    generated.push_back(t);
        }
        std::sort(generated.begin(), generated.end());
        for (qint64 t : generated)
            r.emplace_back(t, offsetsAt(t));
    }
    return r;
}

bool QTimeZonePrivate::hasDaylightTime() const {
    if (rule.valid && rule.hasDst)
        return true;
    return std::any_of(types.begin(), types.end(), [](const Type &t) { return t.dst; });
}

QString QTimeZonePrivate::offsetName(int offsetSeconds) {
    if (offsetSeconds == 0)
        return QString(u"UTC");
    const int a = std::abs(offsetSeconds);
    QString r = QString::asprintf("UTC%c%02d:%02d", offsetSeconds < 0 ? '-' : '+', a / 3600, (a / 60) % 60);
    if (a % 60)
        r += QString::asprintf(":%02d", a % 60);
    return r;
}

QString QTimeZonePrivate::zoneDirectory() {
    const char *dir = std::getenv("TZDIR");
    return dir && *dir ? QString::fromLocal8Bit(dir) : QString(u"/usr/share/zoneinfo");
}

std::shared_ptr<const QTimeZonePrivate> QTimeZonePrivate::utc() {
    static const std::shared_ptr<const QTimeZonePrivate> z = [] {
        auto u = std::make_shared<QTimeZonePrivate>();
        u->kind = Kind::Utc;
        u->id = "UTC";
        return u;
    }();
    return z;
}

std::shared_ptr<const QTimeZonePrivate> QTimeZonePrivate::fixed(int offsetSeconds) {
    if (offsetSeconds == 0)
        return utc();
    auto z = std::make_shared<QTimeZonePrivate>();
    z->kind = Kind::Fixed;
    z->fixedOffset = offsetSeconds;
    z->id = offsetName(offsetSeconds).toLatin1();
    return z;
}

QByteArray QTimeZonePrivate::systemId() {
    const QString dir = zoneDirectory();
    if (const char *tz = std::getenv("TZ"); tz && *tz) {
        QByteArray id(*tz == ':' ? tz + 1 : tz);
        if (id.startsWith(dir.toLocal8Bit() + '/'))
            id = id.mid(dir.toLocal8Bit().size() + 1);
        return id;
    }
    QFile etc(QString(u"/etc/timezone"));
    if (etc.open(QIODevice::ReadOnly)) {
        const QByteArray id = etc.readAll().trimmed();
        if (validId(id) && QFileInfo(dir + QChar(u'/') + QString::fromLatin1(id)).isFile())
            return id;
    }
    char buf[1024];
    const ssize_t len = ::readlink("/etc/localtime", buf, sizeof buf - 1);
    if (len > 0) {
        const QByteArray target(buf, static_cast<qsizetype>(len));
        const qsizetype k = target.indexOf("zoneinfo/");
        if (k >= 0)
            return target.mid(k + 9);
    }
    return QByteArray("UTC");
}

std::shared_ptr<const QTimeZonePrivate> QTimeZonePrivate::load(const QByteArray &id) {
    int offset = 0;
    if (parseOffsetId(id, &offset)) {
        if (id == "UTC" || id == "GMT")
            return offset == 0 && id == "UTC" ? utc() : fixed(offset);
        return fixed(offset);
    }
    const QString dir = zoneDirectory();
    const QString key = dir + QChar(u'\n') + QString::fromLatin1(id);
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        const auto it = cache().find(key);
        if (it != cache().end())
            return it->second;
    }
    std::shared_ptr<QTimeZonePrivate> z;
    if (validId(id)) {
        QFile f(dir + QChar(u'/') + QString::fromLatin1(id));
        if (f.open(QIODevice::ReadOnly))
            z = parseTzif(f.readAll());
    }
    if (!z) {
        // A POSIX TZ rule can stand for a zone, as in the TZ environment variable.
        Rule r;
        if (!id.isEmpty() && parseRule(id.toStdString(), &r)) {
            z = std::make_shared<QTimeZonePrivate>();
            z->kind = Kind::Tzif;
            z->rule = r;
            z->types.push_back(r.standard);
        }
    }
    if (!z)
        return nullptr;
    z->id = id;
    static const std::map<QByteArray, ZoneTabEntry> zoneTab = readZoneTab();
    const auto tab = zoneTab.find(id);
    if (tab != zoneTab.end()) {
        z->territory = QLocale::codeToTerritory(tab->second.code);
        z->comment = tab->second.comment;
    }
    std::lock_guard<std::mutex> lock(cacheMutex);
    cache()[key] = z;
    return z;
}

std::shared_ptr<const QTimeZonePrivate> QTimeZonePrivate::local() {
    auto system = load(systemId());
    auto z = std::make_shared<QTimeZonePrivate>(system ? *system : *utc());
    if (z->kind == Kind::Utc) {
        z->kind = Kind::Tzif;
        z->types.push_back(Type{0, false, "UTC"});
    }
    z->kind = Kind::Local;
    return z;
}

// ------------------------------------------------------------------ QTimeZone

/*!
    Constructs an invalid time zone.

    \sa isValid()
*/
QTimeZone::QTimeZone() noexcept = default;

QTimeZone::QTimeZone(std::shared_ptr<const QTimeZonePrivate> data) noexcept : d(std::move(data)) {}

/*!
    \since 6.5
    Constructs the time zone \a spec: local time or UTC.
*/
QTimeZone::QTimeZone(Initialization spec) noexcept {
    try {
        d = spec == UTC ? QTimeZonePrivate::utc() : QTimeZonePrivate::local();
    } catch (...) {
        d.reset();
    }
}

/*!
    Constructs the time zone with the IANA ID \a ianaId, such as
    \c{"Europe/Berlin"}. The IDs \c{"UTC"} and \c{"UTC+hh:mm"} give UTC and
    fixed offsets, and a POSIX TZ rule such as \c{"CET-1CEST,M3.5.0,M10.5.0/3"}
    is accepted too. The zone is invalid if the ID is not available.

    \sa isTimeZoneIdAvailable(), availableTimeZoneIds()
*/
QTimeZone::QTimeZone(const QByteArray &ianaId) : d(QTimeZonePrivate::load(ianaId)) {}

/*!
    Constructs a time zone with the fixed offset \a offsetSeconds from UTC.
    Its ID is \c{"UTC+hh:mm"}, or \c{"UTC"} for an offset of 0. The zone is
    invalid if the offset is outside MinUtcOffsetSecs to MaxUtcOffsetSecs.

    \sa fromSecondsAheadOfUtc()
*/
QTimeZone::QTimeZone(int offsetSeconds) {
    if (offsetSeconds >= MinUtcOffsetSecs && offsetSeconds <= MaxUtcOffsetSecs)
        d = QTimeZonePrivate::fixed(offsetSeconds);
}

/*!
    Constructs a custom time zone with the ID \a zoneId and the fixed offset
    \a offsetSeconds, the display name \a name, the abbreviation
    \a abbreviation, the territory \a territory and the comment \a comment.
*/
QTimeZone::QTimeZone(const QByteArray &zoneId, int offsetSeconds, const QString &name, const QString &abbreviation,
                     QLocale::Territory territory, const QString &comment) {
    if (zoneId.isEmpty() || offsetSeconds < MinUtcOffsetSecs || offsetSeconds > MaxUtcOffsetSecs)
        return;
    auto z = std::make_shared<QTimeZonePrivate>();
    z->kind = QTimeZonePrivate::Kind::Fixed;
    z->id = zoneId;
    z->fixedOffset = offsetSeconds;
    z->name = name.isEmpty() ? QString::fromLatin1(zoneId) : name;
    z->abbreviationOverride = abbreviation;
    z->territory = territory;
    z->comment = comment;
    d = std::move(z);
}

/*!
    Constructs a copy of \a other.
*/
QTimeZone::QTimeZone(const QTimeZone &other) noexcept = default;

/*!
    Move-constructs a time zone from \a other, which becomes invalid.
*/
QTimeZone::QTimeZone(QTimeZone &&other) noexcept = default;

/*!
    Destroys the time zone.
*/
QTimeZone::~QTimeZone() = default;

/*!
    Assigns \a other to this time zone and returns a reference to it.
*/
QTimeZone &QTimeZone::operator=(const QTimeZone &other) = default;

/*!
    Move-assigns \a other to this time zone and returns a reference to it.
*/
QTimeZone &QTimeZone::operator=(QTimeZone &&other) noexcept = default;

/*!
    Returns \c true if the time zone is valid; otherwise returns \c false.
*/
bool QTimeZone::isValid() const { return d != nullptr; }

/*!
    Returns the ID of the time zone: the IANA ID, \c{"UTC"}, \c{"UTC+hh:mm"}
    for a fixed offset, or the ID of the system zone for local time. Returns
    an empty byte array for an invalid zone.
*/
QByteArray QTimeZone::id() const { return d ? d->id : QByteArray(); }

/*!
    \since 6.2
    Returns the territory of the zone, as listed in the \c zone.tab file of
    the time zone database, or QLocale::AnyTerritory.
*/
QLocale::Territory QTimeZone::territory() const { return d ? d->territory : QLocale::AnyTerritory; }

/*!
    Returns the comment of the zone from \c zone.tab, such as
    \c{"most of Spain"}, or an empty string.
*/
QString QTimeZone::comment() const { return d ? d->comment : QString(); }

/*!
    Returns the name of the zone at \a atDateTime in the form \a nameType. The
    \a locale is accepted for compatibility; names are not localized.

    \sa abbreviation()
*/
QString QTimeZone::displayName(const QDateTime &atDateTime, NameType nameType, const QLocale &locale) const {
    Q_UNUSED(locale)
    if (!d)
        return QString();
    const auto o =
        d->offsetsAt(atDateTime.isValid() ? atDateTime.toSecsSinceEpoch() : QDateTime::currentSecsSinceEpoch());
    switch (nameType) {
    case OffsetName:
        return QTimeZonePrivate::offsetName(o.offset);
    case LongName:
        if (!d->name.isEmpty())
            return d->name;
        return d->kind == QTimeZonePrivate::Kind::Tzif || d->kind == QTimeZonePrivate::Kind::Local
                   ? QString::fromLatin1(d->id)
                   : QTimeZonePrivate::offsetName(o.offset);
    case ShortName:
    case DefaultName:
        break;
    }
    const QString abbr = QString::fromStdString(o.abbreviation);
    return abbr.isEmpty() ? QTimeZonePrivate::offsetName(o.offset) : abbr;
}

/*!
    \overload
    Returns the name of the standard, daylight or generic time of the zone,
    \a timeType, in the form \a nameType, as currently in effect. The
    \a locale is accepted for compatibility.
*/
QString QTimeZone::displayName(TimeType timeType, NameType nameType, const QLocale &locale) const {
    if (!d)
        return QString();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (timeType == GenericTime || !hasDaylightTime())
        return displayName(utcDateTime(now), nameType, locale);
    // Find an instant in the coming year with the requested kind of time.
    for (qint64 t = now; t < now + 366 * 86400; t += 7 * 86400)
        if (d->offsetsAt(t).dst == (timeType == DaylightTime))
            return displayName(utcDateTime(t), nameType, locale);
    return displayName(utcDateTime(now), nameType, locale);
}

/*!
    Returns the abbreviation of the zone at \a atDateTime, such as
    \c{"CEST"}.
*/
QString QTimeZone::abbreviation(const QDateTime &atDateTime) const { return displayName(atDateTime, ShortName); }

/*!
    Returns the offset from UTC of the zone at \a atDateTime, in seconds,
    including any daylight-saving offset.

    \sa standardTimeOffset(), daylightTimeOffset()
*/
int QTimeZone::offsetFromUtc(const QDateTime &atDateTime) const {
    return d ? d->offsetsAt(atDateTime.toSecsSinceEpoch()).offset : 0;
}

/*!
    Returns the offset of standard time from UTC at \a atDateTime, in seconds.
*/
int QTimeZone::standardTimeOffset(const QDateTime &atDateTime) const {
    return d ? d->offsetsAt(atDateTime.toSecsSinceEpoch()).standardOffset : 0;
}

/*!
    Returns the daylight-saving offset at \a atDateTime in seconds, or 0 if
    standard time is in effect.
*/
int QTimeZone::daylightTimeOffset(const QDateTime &atDateTime) const {
    if (!d)
        return 0;
    const auto o = d->offsetsAt(atDateTime.toSecsSinceEpoch());
    return o.offset - o.standardOffset;
}

/*!
    Returns \c true if the zone has or had daylight-saving time; otherwise
    returns \c false.
*/
bool QTimeZone::hasDaylightTime() const { return d && d->hasDaylightTime(); }

/*!
    Returns \c true if daylight-saving time is in effect at \a atDateTime;
    otherwise returns \c false.
*/
bool QTimeZone::isDaylightTime(const QDateTime &atDateTime) const {
    return d && d->offsetsAt(atDateTime.toSecsSinceEpoch()).dst;
}

/*!
    Returns the offsets of the zone at \a forDateTime.
*/
QTimeZone::OffsetData QTimeZone::offsetData(const QDateTime &forDateTime) const {
    if (!d || !forDateTime.isValid())
        return OffsetData();
    const qint64 secs = forDateTime.toSecsSinceEpoch();
    return toOffsetData(secs, d->offsetsAt(secs));
}

/*!
    Returns \c true if the zone has transitions, that is, if its offset ever
    changes; otherwise returns \c false.
*/
bool QTimeZone::hasTransitions() const {
    return d && d->hasTransitions() && (!d->transitions.empty() || d->hasDaylightTime());
}

/*!
    Returns the first transition after \a afterDateTime, or an OffsetData
    with an invalid atUtc if there is none.

    \sa previousTransition(), transitions()
*/
QTimeZone::OffsetData QTimeZone::nextTransition(const QDateTime &afterDateTime) const {
    if (!d || !afterDateTime.isValid())
        return OffsetData();
    const qint64 after = afterDateTime.toSecsSinceEpoch();
    const auto it = std::upper_bound(d->transitions.begin(), d->transitions.end(), after);
    if (it != d->transitions.end())
        return toOffsetData(*it, d->offsetsAt(*it));
    const auto found = d->transitionsBetween(after + 1, after + 2 * 366 * 86400);
    return found.empty() ? OffsetData() : toOffsetData(found.front().first, found.front().second);
}

/*!
    Returns the last transition before \a beforeDateTime, or an OffsetData
    with an invalid atUtc if there is none.

    \sa nextTransition()
*/
QTimeZone::OffsetData QTimeZone::previousTransition(const QDateTime &beforeDateTime) const {
    if (!d || !beforeDateTime.isValid())
        return OffsetData();
    const qint64 before = beforeDateTime.toSecsSinceEpoch();
    const auto generated = d->transitionsBetween(before - 2 * 366 * 86400, before - 1);
    if (!generated.empty())
        return toOffsetData(generated.back().first, generated.back().second);
    const auto it = std::lower_bound(d->transitions.begin(), d->transitions.end(), before);
    if (it == d->transitions.begin())
        return OffsetData();
    const qint64 t = *(it - 1);
    return toOffsetData(t, d->offsetsAt(t));
}

/*!
    Returns the transitions between \a fromDateTime and \a toDateTime, both
    included.
*/
QTimeZone::OffsetDataList QTimeZone::transitions(const QDateTime &fromDateTime, const QDateTime &toDateTime) const {
    OffsetDataList r;
    if (!d || !fromDateTime.isValid() || !toDateTime.isValid())
        return r;
    for (const auto &t : d->transitionsBetween(fromDateTime.toSecsSinceEpoch(), toDateTime.toSecsSinceEpoch()))
        r.append(toOffsetData(t.first, t.second));
    return r;
}

/*!
    \since 6.5
    Returns the time specification of the zone: Qt::LocalTime, Qt::UTC,
    Qt::OffsetFromUTC for a fixed offset, or Qt::TimeZone.
*/
Qt::TimeSpec QTimeZone::timeSpec() const noexcept {
    if (!d)
        return Qt::TimeZone;
    switch (d->kind) {
    case QTimeZonePrivate::Kind::Local:
        return Qt::LocalTime;
    case QTimeZonePrivate::Kind::Utc:
        return Qt::UTC;
    case QTimeZonePrivate::Kind::Fixed:
        return d->name.isEmpty() ? Qt::OffsetFromUTC : Qt::TimeZone;
    case QTimeZonePrivate::Kind::Tzif:
        break;
    }
    return Qt::TimeZone;
}

/*!
    \since 6.8
    Returns \c true if \a alias is another name of this zone, such as \c{"Asia/Calcutta"} for
    \c{"Asia/Kolkata"}: the name itself, or a name that the time zone database links to the same zone.

    \sa id()
*/
bool QTimeZone::hasAlternativeName(QByteArrayView alias) const {
    if (!isValid() || alias.isEmpty())
        return false;
    const QByteArray name = alias.toByteArray();
    if (name == id())
        return true;
    const QTimeZone other(name);
    if (!other.isValid())
        return false;
    if (d->kind == QTimeZonePrivate::Kind::Tzif && other.d->kind == QTimeZonePrivate::Kind::Tzif) {
        // Aliases are links to the same file, or copies of it.
        const QString dir = QTimeZonePrivate::zoneDirectory() + QLatin1Char('/');
        const QFileInfo a(dir + QString::fromUtf8(id())), b(dir + QString::fromUtf8(name));
        if (a.exists() && b.exists() && a.canonicalFilePath() == b.canonicalFilePath())
            return true;
        QFile fa(a.filePath()), fb(b.filePath());
        return fa.open(QIODevice::ReadOnly) && fb.open(QIODevice::ReadOnly) && fa.size() == fb.size() &&
               fa.readAll() == fb.readAll();
    }
    return other.id() == id();
}

/*!
    \since 6.6
    Returns a time zone, with the time specification Qt::TimeZone, that stands for the same times as this time
    representation: the system's zone for local time, and a zone with a fixed offset for UTC or a fixed offset.
    A zone that has that time specification already is returned as it is.

    \sa timeSpec(), systemTimeZone()
*/
QTimeZone QTimeZone::asBackendZone() const {
    switch (timeSpec()) {
    case Qt::LocalTime:
        return systemTimeZone();
    case Qt::UTC:
        return QTimeZone(QByteArray("UTC"), 0, QStringLiteral("UTC"), QStringLiteral("UTC"));
    case Qt::OffsetFromUTC: {
        const QString name = QString::fromLatin1(id());
        return QTimeZone(id(), fixedSecondsAheadOfUtc(), name, name);
    }
    case Qt::TimeZone:
        break;
    }
    return *this;
}

/*!
    \since 6.5
    Returns the offset of a UTC or fixed-offset zone, or 0 for other zones.
*/
int QTimeZone::fixedSecondsAheadOfUtc() const noexcept { return timeSpec() == Qt::OffsetFromUTC ? d->fixedOffset : 0; }

/*!
    \since 6.5
    Returns \c true if the zone is UTC or a fixed offset; otherwise returns
    \c false.
*/
bool QTimeZone::isUtcOrFixedOffset() const noexcept {
    const Qt::TimeSpec spec = timeSpec();
    return d && (spec == Qt::UTC || spec == Qt::OffsetFromUTC);
}

/*!
    \since 6.5
    Returns a zone with the fixed offset \a offset from UTC, in seconds.
*/
QTimeZone QTimeZone::fromSecondsAheadOfUtc(int offset) { return QTimeZone(offset); }

/*!
    Returns the UTC time zone.
*/
QTimeZone QTimeZone::utc() { return QTimeZone(UTC); }

/*!
    Returns the time zone of the system, as a zone with the system's IANA ID.

    \sa systemTimeZoneId()
*/
QTimeZone QTimeZone::systemTimeZone() {
    QTimeZone z(systemTimeZoneId());
    return z.isValid() ? z : utc();
}

/*!
    Returns the IANA ID of the system time zone: from the \c TZ environment
    variable, \c /etc/timezone or the target of \c /etc/localtime, or
    \c{"UTC"}.
*/
QByteArray QTimeZone::systemTimeZoneId() { return QTimeZonePrivate::systemId(); }

/*!
    Returns \c true if a zone with the ID \a ianaId can be created; otherwise
    returns \c false.
*/
bool QTimeZone::isTimeZoneIdAvailable(const QByteArray &ianaId) { return QTimeZonePrivate::load(ianaId) != nullptr; }

/*!
    Returns the IDs of all zones of the time zone database, sorted, including
    \c{"UTC"}.
*/
QList<QByteArray> QTimeZone::availableTimeZoneIds() {
    std::set<QByteArray> ids{QByteArray("UTC")};
    const QString dir = QTimeZonePrivate::zoneDirectory();
    QFile zi(dir + QString(u"/tzdata.zi"));
    if (zi.open(QIODevice::ReadOnly)) {
        for (const QByteArray &line : zi.readAll().split('\n')) {
            const QList<QByteArray> f = line.simplified().split(' ');
            if (f.size() >= 2 && f.at(0) == "Z")
                ids.insert(f.at(1));
            else if (f.size() >= 3 && f.at(0) == "L")
                ids.insert(f.at(2));
        }
    } else {
        QFile tab(dir + QString(u"/zone1970.tab"));
        if (tab.open(QIODevice::ReadOnly)) {
            for (const QByteArray &line : tab.readAll().split('\n')) {
                const QList<QByteArray> f = line.split('\t');
                if (!line.startsWith('#') && f.size() >= 3)
                    ids.insert(f.at(2));
            }
        }
    }
    QList<QByteArray> r;
    for (const QByteArray &id : ids)
        if (id == "UTC" || QFileInfo(dir + QChar(u'/') + QString::fromLatin1(id)).isFile())
            r.append(id);
    return r;
}

/*!
    \overload
    Returns the IDs of the zones listed for \a territory in \c zone.tab.
*/
QList<QByteArray> QTimeZone::availableTimeZoneIds(QLocale::Territory territory) {
    QList<QByteArray> r;
    const QString code = QLocale::territoryToCode(territory);
    for (const auto &entry : readZoneTab())
        if (entry.second.code == code)
            r.append(entry.first);
    std::sort(r.begin(), r.end());
    return r;
}

/*!
    \overload
    Returns the IDs of the zones whose standard time is currently
    \a offsetSeconds ahead of UTC.
*/
QList<QByteArray> QTimeZone::availableTimeZoneIds(int offsetSeconds) {
    QList<QByteArray> r;
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    for (const QByteArray &id : availableTimeZoneIds()) {
        const auto z = QTimeZonePrivate::load(id);
        if (z && z->offsetsAt(now).standardOffset == offsetSeconds)
            r.append(id);
    }
    return r;
}

bool operator==(const QTimeZone &a, const QTimeZone &b) noexcept {
    if (a.d == b.d)
        return true;
    if (!a.d || !b.d)
        return false;
    return a.d->kind == b.d->kind && a.d->id == b.d->id && a.d->fixedOffset == b.d->fixedOffset &&
           a.d->name == b.d->name;
}

std::size_t qHash(const QTimeZone &key, std::size_t seed) noexcept { return qHash(key.id(), seed); }
