// MIT License - see LICENSE
#include <QtCore/qdebug.h>
#include <QtCore/qlocale.h>

#include "numparse_p.h"
#include "qlocale_p.h"

#include <cerrno>
#include <charconv>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <limits>
#include <mutex>
#include <string>
#include <vector>

namespace {

const QLocaleRow &cRow() { return QtPrivate::localeRows().front(); }

QString utf8(const char *s) { return QString::fromUtf8(s); }

QString nth(const char *list, int index) {
    const char *p = list;
    for (int i = 0; i < index; ++i) {
        p = std::strchr(p, '|');
        if (!p)
            return QString();
        ++p;
    }
    const char *e = std::strchr(p, '|');
    return QString::fromUtf8(p, e ? static_cast<qsizetype>(e - p) : static_cast<qsizetype>(std::strlen(p)));
}

const QLocaleRow *rowForLanguage(QLocale::Language language) {
    for (const QLocaleRow &r : QtPrivate::localeRows()) {
        if (r.language == language)
            return &r;
    }
    return nullptr;
}

const QLocaleRow *rowFor(QLocale::Language language, QLocale::Territory territory) {
    if (territory != QLocale::AnyTerritory) {
        for (const QLocaleRow &r : QtPrivate::localeRows()) {
            if (r.language == language && r.territory == territory)
                return &r;
        }
    }
    return rowForLanguage(language);
}

const QLocaleNameEntry *findByValue(const std::vector<QLocaleNameEntry> &list, int value) {
    for (const QLocaleNameEntry &e : list) {
        if (e.value == value)
            return &e;
    }
    return nullptr;
}

bool equalsIgnoreCase(QStringView a, const char *b) {
    const qsizetype n = static_cast<qsizetype>(std::strlen(b));
    if (a.size() != n)
        return false;
    for (qsizetype i = 0; i < n; ++i) {
        const char16_t c = a[i].unicode();
        const char16_t d = static_cast<unsigned char>(b[i]);
        const char16_t lc = (c >= u'A' && c <= u'Z') ? static_cast<char16_t>(c + 32) : c;
        const char16_t ld = (d >= u'A' && d <= u'Z') ? static_cast<char16_t>(d + 32) : d;
        if (lc != ld)
            return false;
    }
    return true;
}

struct ParsedName {
    QLocale::Language language = QLocale::AnyLanguage;
    QLocale::Script script = QLocale::AnyScript;
    QLocale::Territory territory = QLocale::AnyTerritory;
    bool valid = false;
};

// Reads names such as "de", "de_DE", "de-DE.UTF-8", "zh_Hans_CN" and "sr@latin"; C and POSIX are the C locale.
ParsedName parseName(const QString &name) {
    ParsedName r;
    QString n = name;
    const qsizetype at = n.indexOf(QChar(u'@'));
    if (at >= 0)
        n.truncate(at);
    const qsizetype dot = n.indexOf(QChar(u'.'));
    if (dot >= 0)
        n.truncate(dot);
    if (n.isEmpty())
        return r;
    if (n == QString("C") || n == QString("POSIX")) {
        r.language = QLocale::C;
        r.valid = true;
        return r;
    }
    QString normalized = n;
    normalized.replace(QChar(u'-'), QChar(u'_'));
    const QStringList parts = normalized.split(QChar(u'_'));
    if (parts.isEmpty() || parts.size() > 3)
        return r;
    const QLocale::Language lang = QLocale::codeToLanguage(parts[0]);
    if (lang == QLocale::AnyLanguage)
        return r;
    r.language = lang;
    r.valid = true;
    for (qsizetype i = 1; i < parts.size(); ++i) {
        const QString &p = parts[i];
        if (p.size() == 4) {
            r.script = QLocale::codeToScript(p);
        } else {
            r.territory = QLocale::codeToTerritory(p);
        }
    }
    return r;
}

std::mutex &defaultMutex() {
    static std::mutex m;
    return m;
}

QLocale *&defaultStorage() {
    static QLocale *l = nullptr;
    return l;
}

QString environmentLocaleName() {
    const char *names[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
    for (const char *n : names) {
        const char *v = std::getenv(n);
        if (v && *v)
            return QString::fromUtf8(v);
    }
    return QString();
}

QStringList environmentLanguages() {
    const char *v = std::getenv("LANGUAGE");
    if (!v || !*v)
        return QStringList();
    return QString::fromUtf8(v).split(QChar(u':'), Qt::SkipEmptyParts);
}

// --- number formatting ---

QString groupInteger(const std::string &digits, const QString &sep) {
    QString out;
    const std::size_t n = digits.size();
    for (std::size_t i = 0; i < n; ++i) {
        if (i > 0 && (n - i) % 3 == 0)
            out.append(sep);
        out.append(QChar(static_cast<char16_t>(digits[i])));
    }
    return out;
}

// Turns a number written in the C locale (as QtPrivate::formatDouble() writes it) into the text of the locale.
QString localizeNumber(const QLocaleRow &r, QLocale::NumberOptions options, const std::string &c, bool upperExponent) {
    QString out;
    std::size_t i = 0;
    if (i < c.size() && (c[i] == '-' || c[i] == '+')) {
        out.append(utf8(c[i] == '-' ? r.minus : r.plus));
        ++i;
    }
    if (c.compare(i, std::string::npos, "inf") == 0 || c.compare(i, std::string::npos, "INF") == 0 || c.compare(i, std::string::npos, "nan") == 0
        || c.compare(i, std::string::npos, "NAN") == 0) {
        out.append(QString::fromLatin1(c.data() + i, static_cast<qsizetype>(c.size() - i)));
        return out;
    }
    std::string digits;
    while (i < c.size() && c[i] >= '0' && c[i] <= '9')
        digits.push_back(c[i++]);
    if (options.testFlag(QLocale::OmitGroupSeparator))
        out.append(QString::fromLatin1(digits.data(), static_cast<qsizetype>(digits.size())));
    else
        out.append(groupInteger(digits, utf8(r.group)));
    if (i < c.size() && c[i] == '.') {
        out.append(utf8(r.decimal));
        ++i;
        while (i < c.size() && c[i] >= '0' && c[i] <= '9')
            out.append(QChar(static_cast<char16_t>(c[i++])));
    }
    if (i < c.size() && (c[i] == 'e' || c[i] == 'E')) {
        QString e = utf8(r.exponent);
        out.append(upperExponent || c[i] == 'E' ? e.toUpper() : e);
        ++i;
        if (i < c.size() && (c[i] == '-' || c[i] == '+')) {
            out.append(utf8(c[i] == '-' ? r.minus : r.plus));
            ++i;
        }
        std::string expDigits(c, i);
        if (options.testFlag(QLocale::OmitLeadingZeroInExponent)) {
            while (expDigits.size() > 1 && expDigits[0] == '0')
                expDigits.erase(0, 1);
        }
        out.append(QString::fromLatin1(expDigits.data(), static_cast<qsizetype>(expDigits.size())));
    }
    return out;
}

// Pads the significant digits of a 'g' result with zeroes up to the precision, as C's '#' flag does.
std::string includeTrailingZeroes(std::string s, int precision) {
    if (precision == 0)
        precision = 1;
    const std::size_t ePos = s.find_first_of("eE");
    std::string mant = s.substr(0, ePos);
    const std::string exp = ePos == std::string::npos ? std::string() : s.substr(ePos);
    int significant = 0;
    bool leading = true;
    for (char ch : mant) {
        if (ch >= '0' && ch <= '9') {
            if (ch != '0')
                leading = false;
            if (!leading)
                ++significant;
        }
    }
    if (significant == 0)
        significant = 1;
    if (mant.find('.') == std::string::npos)
        mant.push_back('.');
    for (; significant < precision; ++significant)
        mant.push_back('0');
    return mant + exp;
}

std::string formatCDouble(double v, char format, int precision, bool includeZeroes) {
    char fmt = format;
    const bool upper = fmt == 'E' || fmt == 'G' || fmt == 'F';
    char lower = upper ? static_cast<char>(fmt + 32) : fmt;
    if (lower != 'f' && lower != 'e' && lower != 'g')
        lower = 'g';
    std::chars_format cf = lower == 'f' ? std::chars_format::fixed : lower == 'e' ? std::chars_format::scientific : std::chars_format::general;
    char buf[1200];
    std::to_chars_result r;
    if (precision == QLocale::FloatingPointShortest)
        r = std::to_chars(buf, buf + sizeof buf, v, cf);
    else
        r = std::to_chars(buf, buf + sizeof buf, v, cf, precision < 0 ? 6 : (precision > 400 ? 400 : precision));
    std::string s(buf, static_cast<std::size_t>(r.ptr - buf));
    if (includeZeroes && lower == 'g' && precision != QLocale::FloatingPointShortest)
        s = includeTrailingZeroes(s, precision < 0 ? 6 : precision);
    if (upper) {
        for (char &ch : s) {
            if (ch >= 'a' && ch <= 'z')
                ch = static_cast<char>(ch - 32);
        }
    }
    return s;
}

// --- number parsing ---

bool isSpaceChar(QChar c) { return c.isSpace() || c.unicode() == 0xa0 || c.unicode() == 0x202f; }

// Converts the text of the locale to a number in the C locale. Returns false if it is not a valid number.
bool localizedToC(const QLocaleRow &r, QLocale::NumberOptions options, QStringView in, bool floating, std::string &out) {
    qsizetype b = 0;
    qsizetype e = in.size();
    while (b < e && isSpaceChar(in[b]))
        ++b;
    while (e > b && isSpaceChar(in[e - 1]))
        --e;
    const QStringView s = in.mid(b, e - b);
    if (s.isEmpty())
        return false;

    const QString minus = utf8(r.minus);
    const QString plus = utf8(r.plus);
    const QString group = utf8(r.group);
    const QString decimal = utf8(r.decimal);
    const QString exponent = utf8(r.exponent);
    const bool spaceGroup = group.size() == 1 && (group[0].unicode() == 0xa0 || group[0].unicode() == 0x202f || group[0].unicode() == u' ');

    qsizetype pos = 0;
    std::string result;
    auto startsWith = [&](const QString &token) { return !token.isEmpty() && s.mid(pos, token.size()) == QStringView(token); };
    auto readSign = [&](std::string &dest) {
        if (startsWith(minus)) {
            dest.push_back('-');
            pos += minus.size();
            return true;
        }
        if (s[pos] == QChar(u'-')) {
            dest.push_back('-');
            ++pos;
            return true;
        }
        if (startsWith(plus)) {
            pos += plus.size();
            return true;
        }
        if (s[pos] == QChar(u'+')) {
            ++pos;
            return true;
        }
        return false;
    };
    readSign(result);
    if (pos >= s.size())
        return false;

    if (floating) {
        const QStringView rest = s.mid(pos);
        if (equalsIgnoreCase(rest, "inf") || equalsIgnoreCase(rest, "infinity")) {
            out = result + "inf";
            return true;
        }
        if (equalsIgnoreCase(rest, "nan")) {
            out = "nan";
            return true;
        }
    }

    std::string intDigits;
    std::vector<qsizetype> segments;
    qsizetype segment = 0;
    bool sawGroup = false;
    std::string frac;
    std::string exp;
    bool sawDecimal = false;
    while (pos < s.size()) {
        const QChar ch = s[pos];
        if (ch.unicode() >= u'0' && ch.unicode() <= u'9') {
            intDigits.push_back(static_cast<char>(ch.unicode()));
            ++segment;
            ++pos;
            continue;
        }
        if (startsWith(group) || (spaceGroup && isSpaceChar(ch))) {
            if (options.testFlag(QLocale::RejectGroupSeparator))
                return false;
            sawGroup = true;
            segments.push_back(segment);
            segment = 0;
            pos += startsWith(group) ? group.size() : 1;
            continue;
        }
        break;
    }
    segments.push_back(segment);
    if (sawGroup) {
        for (std::size_t i = 0; i < segments.size(); ++i) {
            if (i == 0 ? (segments[i] < 1 || segments[i] > 3) : segments[i] != 3)
                return false;
        }
    }
    if (pos < s.size() && startsWith(decimal)) {
        if (!floating)
            return false;
        sawDecimal = true;
        pos += decimal.size();
        while (pos < s.size() && s[pos].unicode() >= u'0' && s[pos].unicode() <= u'9')
            frac.push_back(static_cast<char>(s[pos++].unicode()));
    }
    if (intDigits.empty() && frac.empty())
        return false;
    if (pos < s.size()) {
        const bool isExp = startsWith(exponent) || s[pos] == QChar(u'e') || s[pos] == QChar(u'E') || startsWith(exponent.toUpper());
        if (!isExp || !floating)
            return false;
        pos += startsWith(exponent) ? exponent.size() : (startsWith(exponent.toUpper()) ? exponent.size() : 1);
        if (pos >= s.size())
            return false;
        readSign(exp);
        std::string expDigits;
        while (pos < s.size() && s[pos].unicode() >= u'0' && s[pos].unicode() <= u'9')
            expDigits.push_back(static_cast<char>(s[pos++].unicode()));
        if (expDigits.empty() || pos != s.size())
            return false;
        if (options.testFlag(QLocale::RejectLeadingZeroInExponent) && expDigits.size() > 1 && expDigits[0] == '0')
            return false;
        exp += expDigits;
    }
    if (sawDecimal && options.testFlag(QLocale::RejectTrailingZeroesAfterDot) && !frac.empty() && frac.back() == '0')
        return false;
    result += intDigits.empty() ? std::string("0") : intDigits;
    if (!frac.empty())
        result += "." + frac;
    if (!exp.empty())
        result += "e" + exp;
    out = result;
    return true;
}

// --- date and time formatting ---

bool formatUsesAmPm(QStringView f) {
    bool quoted = false;
    for (QChar c : f) {
        if (c == QChar(u'\''))
            quoted = !quoted;
        else if (!quoted && (c == QChar(u'A') || c == QChar(u'a')))
            return true;
    }
    return false;
}

QString pad(int v, int width) {
    QString s = QString::number(v < 0 ? -v : v);
    while (s.size() < width)
        s.prepend(QChar(u'0'));
    return v < 0 ? QString("-") + s : s;
}

QString timeZoneText(const QDateTime &dt) {
    switch (dt.timeSpec()) {
    case Qt::UTC:
        return QString("UTC");
    case Qt::OffsetFromUTC: {
        const int off = dt.offsetFromUtc();
        if (off == 0)
            return QString("UTC");
        const int a = off < 0 ? -off : off;
        return QString("UTC") + QString(off < 0 ? "-" : "+") + pad(a / 3600, 2) + QString(":") + pad((a / 60) % 60, 2);
    }
    default: {
        const std::time_t t = static_cast<std::time_t>(dt.toSecsSinceEpoch());
        std::tm tmv{};
        if (!localtime_r(&t, &tmv))
            return QString();
        char buf[64];
        const std::size_t n = std::strftime(buf, sizeof buf, "%Z", &tmv);
        return QString::fromUtf8(buf, static_cast<qsizetype>(n));
    }
    }
}

QString formatDateTime(const QLocale &loc, const QDate *date, const QTime *time, const QDateTime *dateTime, QStringView f) {
    const bool ampm = formatUsesAmPm(f);
    QString out;
    const qsizetype n = f.size();
    qsizetype i = 0;
    while (i < n) {
        const QChar c = f[i];
        if (c == QChar(u'\'')) {
            if (i + 1 < n && f[i + 1] == QChar(u'\'')) {
                out.append(QChar(u'\''));
                i += 2;
                continue;
            }
            ++i;
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
        auto literal = [&] {
            out.append(f.mid(i, run).toString());
            i += run;
        };
        if (date && u == u'd') {
            const qsizetype k = qMin<qsizetype>(run, 4);
            if (k == 1)
                out.append(pad(date->day(), 1));
            else if (k == 2)
                out.append(pad(date->day(), 2));
            else
                out.append(loc.dayName(date->dayOfWeek(), k == 3 ? QLocale::ShortFormat : QLocale::LongFormat));
            i += k;
        } else if (date && u == u'M') {
            const qsizetype k = qMin<qsizetype>(run, 4);
            if (k == 1)
                out.append(pad(date->month(), 1));
            else if (k == 2)
                out.append(pad(date->month(), 2));
            else
                out.append(loc.monthName(date->month(), k == 3 ? QLocale::ShortFormat : QLocale::LongFormat));
            i += k;
        } else if (date && u == u'y' && (run == 2 || run == 4)) {
            if (run == 2)
                out.append(pad(((date->year() % 100) + 100) % 100, 2));
            else
                out.append(pad(date->year(), 4));
            i += run;
        } else if (time && (u == u'h' || u == u'H') && run <= 2) {
            int h = time->hour();
            if (u == u'h' && ampm) {
                h %= 12;
                if (h == 0)
                    h = 12;
            }
            out.append(pad(h, static_cast<int>(run)));
            i += run;
        } else if (time && u == u'm' && run <= 2) {
            out.append(pad(time->minute(), static_cast<int>(run)));
            i += run;
        } else if (time && u == u's' && run <= 2) {
            out.append(pad(time->second(), static_cast<int>(run)));
            i += run;
        } else if (time && u == u'z') {
            const qsizetype k = qMin<qsizetype>(run, 3);
            out.append(k == 1 ? pad(time->msec(), 1) : pad(time->msec(), 3));
            i += k;
        } else if (time && (u == u'A' || u == u'a')) {
            const bool upper = u == u'A';
            i += 1;
            if (i < n && f[i] == QChar(upper ? u'P' : u'p'))
                ++i;
            QString text = time->hour() < 12 ? loc.amText() : loc.pmText();
            out.append(upper ? text.toUpper() : text.toLower());
        } else if (u == u't' && (dateTime || time)) {
            if (dateTime)
                out.append(timeZoneText(*dateTime));
            i += run;
        } else {
            literal();
        }
    }
    return out;
}

// --- tables of names ---

QString namedList(const QLocaleRow &r, bool standalone, bool days, QLocale::FormatType type, int index) {
    const char *list = nullptr;
    if (days) {
        const char *fmt[] = {r.daysLong, r.daysShort, r.daysNarrow};
        const char *sa[] = {r.standaloneDaysLong, r.standaloneDaysShort, r.standaloneDaysNarrow};
        list = (standalone && sa[type]) ? sa[type] : fmt[type];
    } else {
        const char *fmt[] = {r.monthsLong, r.monthsShort, r.monthsNarrow};
        const char *sa[] = {r.standaloneMonthsLong, r.standaloneMonthsShort, r.standaloneMonthsNarrow};
        list = (standalone && sa[type]) ? sa[type] : fmt[type];
    }
    return nth(list, index);
}

template <typename Int>
bool fitsIn(qlonglong v) {
    return v >= static_cast<qlonglong>(std::numeric_limits<Int>::min()) && v <= static_cast<qlonglong>(std::numeric_limits<Int>::max());
}

} // namespace

/*!
    Constructs a QLocale object initialized with the default locale. If
    setDefault() was not called, this is the system locale.

    \sa system(), setDefault()
*/
QLocale::QLocale() {
    std::lock_guard<std::mutex> lock(defaultMutex());
    QLocale *&d = defaultStorage();
    if (d) {
        *this = *d;
    } else {
        *this = system();
    }
}

/*!
    \overload

    Constructs a QLocale object for the locale called \a name. The name has
    a language code, optionally followed by a script and a territory code,
    separated by \c{'_'} or \c{'-'}, such as \c{"de"}, \c{"de_DE"} or
    \c{"zh_Hans_CN"}. A trailing \c{.encoding} or \c{@modifier} is ignored.
    \c{"C"} and \c{"POSIX"} name the C locale. A name that does not match
    gives the C locale too.

    \sa name()
*/
QLocale::QLocale(const QString &name) {
    const ParsedName p = parseName(name);
    if (!p.valid || p.language == C) {
        *this = QLocale(C);
        return;
    }
    *this = QLocale(p.language, p.script, p.territory);
}

/*!
    \overload

    Constructs a QLocale object for \a language and \a territory. If there
    is no data for \a language, the result is the C locale. If \a territory is
    AnyTerritory, the main territory of the language is used. If there is no
    data for the combination, the data of the language is used with the
    requested territory.
*/
QLocale::QLocale(Language language, Territory territory) : QLocale(language, AnyScript, territory) {}

/*!
    \overload

    Constructs a QLocale object for \a language, \a script and \a territory.
    The script only selects the locale if data exists for it; Nt has one script
    for each language, so \a script is otherwise ignored.
*/
QLocale::QLocale(Language language, Script script, Territory territory) {
    Q_UNUSED(script)
    const QLocaleRow *row = (language == C || language == AnyLanguage) ? nullptr : rowFor(language, territory);
    if (!row) {
        m_row = &cRow();
        m_language = C;
        m_script = LatinScript;
        m_territory = AnyTerritory;
        m_options = OmitGroupSeparator;
        return;
    }
    m_row = row;
    m_language = language;
    m_script = row->script;
    m_territory = territory != AnyTerritory ? territory : row->territory;
}

/*!
    Returns the language of the locale.

    \sa script(), territory(), languageToString()
*/
QLocale::Language QLocale::language() const { return m_language; }

/*!
    Returns the script of the locale.

    \sa language(), territory(), scriptToString()
*/
QLocale::Script QLocale::script() const { return m_script; }

/*!
    Returns the territory of the locale, or AnyTerritory for the C locale.

    \sa language(), script(), territoryToString()
*/
QLocale::Territory QLocale::territory() const { return m_territory; }

/*!
    Returns the language and territory of the locale as a string of the form
    \c{language_territory}, where \a separator is the character between them
    (an underscore by default). The language is an ISO 639 code in lower
    case and the territory an ISO 3166 code in upper case. The name of the C
    locale is \c{"C"}.

    \sa bcp47Name(), QLocale()
*/
QString QLocale::name(TagSeparator separator) const {
    if (m_language == C)
        return QString("C");
    QString r = languageToCode(m_language);
    if (m_territory != AnyTerritory)
        r += QChar(static_cast<char16_t>(separator)) + territoryToCode(m_territory);
    return r;
}

/*!
    Returns the BCP 47 language tag of the locale: the language, the script
    when it is not the usual one of the language and the territory, separated
    by \a separator (a dash by default). The tag of the C locale is
    \c{"en"}.

    \sa name(), uiLanguages()
*/
QString QLocale::bcp47Name(TagSeparator separator) const {
    if (m_language == C)
        return QString("en");
    QString r = languageToCode(m_language);
    if (m_territory != AnyTerritory)
        r += QChar(static_cast<char16_t>(separator)) + territoryToCode(m_territory);
    return r;
}

/*!
    Returns the name of the language of the locale in that language, for
    example \c{"Deutsch"} for German.

    \sa nativeTerritoryName(), languageToString()
*/
QString QLocale::nativeLanguageName() const { return utf8(m_row->nativeLanguage); }

/*!
    Returns the name of the territory of the locale in the language of the
    locale. For a territory other than the one of the locale data, the name is
    that of the territory of the data.

    \sa nativeLanguageName(), territoryToString()
*/
QString QLocale::nativeTerritoryName() const { return utf8(m_row->nativeTerritory); }

/*!
    Returns the English name of \a language, such as \c{"German"}.

    \sa languageToCode()
*/
QString QLocale::languageToString(Language language) {
    const QLocaleNameEntry *e = findByValue(QtPrivate::languageNames(), language);
    return e ? utf8(e->name) : QString("Unknown");
}

/*!
    Returns the English name of \a territory, such as \c{"United States"}.

    \sa territoryToCode()
*/
QString QLocale::territoryToString(Territory territory) {
    const QLocaleNameEntry *e = findByValue(QtPrivate::territoryNames(), territory);
    return e ? utf8(e->name) : QString("Unknown");
}

/*!
    Returns the English name of \a script, such as \c{"Latin"}.

    \sa scriptToCode()
*/
QString QLocale::scriptToString(Script script) {
    const QLocaleNameEntry *e = findByValue(QtPrivate::scriptNames(), script);
    return e ? utf8(e->name) : QString("Unknown");
}

/*!
    Returns the ISO 639 code of \a language, of the kinds selected by
    \a codeTypes: the two-letter code if that is allowed and the language has
    one, otherwise the three-letter code. The code of the C locale is
    \c{"C"}. Returns an empty string for AnyLanguage and for a language
    without a code of the requested kind.

    \sa codeToLanguage()
*/
QString QLocale::languageToCode(Language language, LanguageCodeTypes codeTypes) {
    const QLocaleNameEntry *e = findByValue(QtPrivate::languageNames(), language);
    if (!e || !*e->code)
        return QString();
    if (language == C)
        return QString("C");
    const bool any = codeTypes.testFlag(AnyLanguageCode);
    if ((any || (codeTypes & ISO639Part1)) && std::strlen(e->code) == 2)
        return utf8(e->code);
    if ((any || (codeTypes & ISO639Alpha3)) && e->code3)
        return utf8(e->code3);
    return QString();
}

/*!
    Returns the ISO 3166 alpha-2 code of \a territory, such as \c{"US"}, or
    \c{"001"} for World. Returns an empty string for AnyTerritory.

    \sa codeToTerritory()
*/
QString QLocale::territoryToCode(Territory territory) {
    const QLocaleNameEntry *e = findByValue(QtPrivate::territoryNames(), territory);
    return e ? utf8(e->code) : QString();
}

/*!
    Returns the ISO 15924 code of \a script, such as \c{"Latn"}. Returns an
    empty string for AnyScript.

    \sa codeToScript()
*/
QString QLocale::scriptToCode(Script script) {
    const QLocaleNameEntry *e = findByValue(QtPrivate::scriptNames(), script);
    return e ? utf8(e->code) : QString();
}

/*!
    Returns the language that has the ISO 639 code \a languageCode of one of
    the kinds in \a codeTypes. The comparison ignores case. Returns
    AnyLanguage if there is no such language. \c{"no"} is taken as
    Norwegian Bokmal.

    \sa languageToCode()
*/
QLocale::Language QLocale::codeToLanguage(QStringView languageCode, LanguageCodeTypes codeTypes) {
    if (languageCode.isEmpty())
        return AnyLanguage;
    const bool any = codeTypes.testFlag(AnyLanguageCode);
    if (equalsIgnoreCase(languageCode, "C") && (any || (codeTypes & ISO639Part1)))
        return C;
    if (equalsIgnoreCase(languageCode, "no") && (any || (codeTypes & ISO639Part1)))
        return NorwegianBokmal;
    for (const QLocaleNameEntry &e : QtPrivate::languageNames()) {
        if (e.value == AnyLanguage || e.value == C)
            continue;
        if ((any || (codeTypes & ISO639Part1)) && std::strlen(e.code) == 2 && equalsIgnoreCase(languageCode, e.code))
            return static_cast<Language>(e.value);
        if ((any || (codeTypes & ISO639Alpha3)) && e.code3 && equalsIgnoreCase(languageCode, e.code3))
            return static_cast<Language>(e.value);
    }
    return AnyLanguage;
}

/*!
    Returns the territory that has the ISO 3166 alpha-2 code
    \a territoryCode, ignoring case, or AnyTerritory if there is none.

    \sa territoryToCode()
*/
QLocale::Territory QLocale::codeToTerritory(QStringView territoryCode) {
    if (territoryCode.isEmpty())
        return AnyTerritory;
    for (const QLocaleNameEntry &e : QtPrivate::territoryNames()) {
        if (e.value != AnyTerritory && equalsIgnoreCase(territoryCode, e.code))
            return static_cast<Territory>(e.value);
    }
    return AnyTerritory;
}

/*!
    Returns the script that has the ISO 15924 code \a scriptCode, ignoring
    case, or AnyScript if there is none.

    \sa scriptToCode()
*/
QLocale::Script QLocale::codeToScript(QStringView scriptCode) {
    if (scriptCode.isEmpty())
        return AnyScript;
    for (const QLocaleNameEntry &e : QtPrivate::scriptNames()) {
        if (e.value != AnyScript && equalsIgnoreCase(scriptCode, e.code))
            return static_cast<Script>(e.value);
    }
    return AnyScript;
}

/*!
    Sets the default locale, the one \c{QLocale()} constructs, to
    \a locale. It does not change the C locale used by QString::number() and
    QString::arg() without a \c L flag. The default is initially the system
    locale.

    \sa system(), c()
*/
void QLocale::setDefault(const QLocale &locale) {
    std::lock_guard<std::mutex> lock(defaultMutex());
    QLocale *&d = defaultStorage();
    if (d)
        *d = locale;
    else
        d = new QLocale(locale);
}

/*!
    Returns a QLocale object initialized to the C locale. It has English
    names and formats, a dot as the decimal point, and the
    OmitGroupSeparator option set, so that numbers are written without group
    separators.

    \sa system(), QLocale()
*/
QLocale QLocale::c() { return QLocale(C); }

/*!
    Returns a QLocale object for the locale the environment selects: the
    value of \c LC_ALL, else of \c LC_MESSAGES, else of \c LANG. If none is
    set, or it names the C or POSIX locale or a language without data in Nt,
    the result is the C locale. The environment is read once.

    \sa c(), setDefault()
*/
QLocale QLocale::system() {
    static const QLocale sys = [] {
        const QString name = environmentLocaleName();
        return name.isEmpty() ? QLocale(C) : QLocale(name);
    }();
    return sys;
}

/*!
    Returns the locales that match \a language, \a script and \a territory,
    where AnyLanguage, AnyScript and AnyTerritory match every value. The
    list only holds the locales with data (see the class description).
*/
QList<QLocale> QLocale::matchingLocales(Language language, Script script, Territory territory) {
    QList<QLocale> result;
    for (const QLocaleRow &r : QtPrivate::localeRows()) {
        if (language != AnyLanguage && r.language != language)
            continue;
        if (script != AnyScript && r.script != script)
            continue;
        if (territory != AnyTerritory && r.territory != territory)
            continue;
        result.append(r.language == C ? QLocale(C) : QLocale(r.language, r.territory));
    }
    return result;
}

/*!
    Sets the options that control how numbers are written and read to
    \a options.

    \sa numberOptions()
*/
void QLocale::setNumberOptions(NumberOptions options) { m_options = options; }

/*!
    Returns the options that control how numbers are written and read. The
    C locale starts with OmitGroupSeparator and the others with
    DefaultNumberOptions.

    \sa setNumberOptions()
*/
QLocale::NumberOptions QLocale::numberOptions() const { return m_options; }

/*!
    Returns a string that represents the integer \a i in the format of the
    locale, with the group separator unless OmitGroupSeparator is set.

    \sa toLongLong()
*/
QString QLocale::toString(qlonglong i) const {
    std::string c = i < 0 ? "-" : "";
    const qulonglong mag = i < 0 ? 0 - static_cast<qulonglong>(i) : static_cast<qulonglong>(i);
    c += QtPrivate::intToString(mag, false, 10);
    return localizeNumber(*m_row, m_options, c, false);
}

/*!
    \overload

    Returns a string that represents the unsigned integer \a i.
*/
QString QLocale::toString(qulonglong i) const { return localizeNumber(*m_row, m_options, QtPrivate::intToString(i, false, 10), false); }

/*!
    \overload

    Returns a string that represents the number \a f in the format of the
    locale. \a format is \c 'f' for a decimal number, \c 'e' (or \c 'E')
    for scientific notation, or \c 'g' (or \c 'G') for the shorter of the
    two; any other value is taken as \c 'g'. \a precision is the number of
    digits after the decimal point for \c 'f' and \c 'e', and the number
    of significant digits for \c 'g'. A negative \a precision other than
    FloatingPointShortest means 6, and FloatingPointShortest produces the
    fewest digits that read back as the same number. Infinity and not a number
    are written \c inf and \c nan, with the locale's sign.
*/
QString QLocale::toString(double f, char format, int precision) const {
    if (std::isnan(f))
        return QString((format == 'E' || format == 'G' || format == 'F') ? "NAN" : "nan");
    const bool upper = format == 'E' || format == 'G' || format == 'F';
    if (std::isinf(f)) {
        const QString sign = f < 0 ? utf8(m_row->minus) : QString();
        return sign + QString(upper ? "INF" : "inf");
    }
    const std::string c = formatCDouble(f, format, precision, m_options.testFlag(IncludeTrailingZeroesAfterDot));
    return localizeNumber(*m_row, m_options, c, upper);
}

/*!
    \overload

    Returns \a date formatted with the date format of \a format. An invalid
    date gives an empty string.

    \sa dateFormat(), QDate::toString()
*/
QString QLocale::toString(QDate date, FormatType format) const {
    if (!date.isValid())
        return QString();
    return formatDateTime(*this, &date, nullptr, nullptr, QStringView(dateFormat(format)));
}

/*!
    \overload

    Returns \a time formatted with the time format of \a format. An invalid
    time gives an empty string.

    \sa timeFormat(), QTime::toString()
*/
QString QLocale::toString(QTime time, FormatType format) const {
    if (!time.isValid())
        return QString();
    return formatDateTime(*this, nullptr, &time, nullptr, QStringView(timeFormat(format)));
}

/*!
    \overload

    Returns \a dateTime formatted with the date and time format of
    \a format. An invalid date and time gives an empty string.

    \sa dateTimeFormat(), QDateTime::toString()
*/
QString QLocale::toString(const QDateTime &dateTime, FormatType format) const {
    if (!dateTime.isValid())
        return QString();
    const QDate d = dateTime.date();
    const QTime t = dateTime.time();
    return formatDateTime(*this, &d, &t, &dateTime, QStringView(dateTimeFormat(format)));
}

/*!
    \overload

    Returns \a date formatted according to \a format. In the format, these
    expressions are replaced: \c d (day without a leading zero), \c dd (two
    digits), \c ddd (abbreviated day name), \c dddd (day name), \c M, \c MM,
    \c MMM, \c MMMM (the same for the month) and \c yy and \c yyyy (the year in
    two or four digits). Text in single quotes is copied as it is, with
    \c{''} meaning a quote; other characters are copied. The names are those
    of the locale. An invalid date gives an empty string.
*/
QString QLocale::toString(QDate date, QStringView format) const {
    if (!date.isValid())
        return QString();
    return formatDateTime(*this, &date, nullptr, nullptr, format);
}

/*!
    \overload

    Returns \a time formatted according to \a format. In the format, these
    expressions are replaced: \c h (hour, in 12 hours if the format has an
    AM/PM expression), \c hh, \c H (hour in 24 hours), \c HH, \c m, \c mm,
    \c s, \c ss, \c z (milliseconds), \c zzz (three digits), \c AP or \c A
    (AM or PM of the locale, in upper case) and \c ap or \c a (in lower
    case). Text in single quotes is copied as it is. An invalid time gives an
    empty string.
*/
QString QLocale::toString(QTime time, QStringView format) const {
    if (!time.isValid())
        return QString();
    return formatDateTime(*this, nullptr, &time, nullptr, format);
}

/*!
    \overload

    Returns \a dateTime formatted according to \a format, which can have the
    expressions for dates and for times, and \c t for the time zone: \c UTC,
    \c{UTC+hh:mm} for a fixed offset, or the abbreviation of the local zone. An
    invalid date and time gives an empty string.
*/
QString QLocale::toString(const QDateTime &dateTime, QStringView format) const {
    if (!dateTime.isValid())
        return QString();
    const QDate d = dateTime.date();
    const QTime t = dateTime.time();
    return formatDateTime(*this, &d, &t, &dateTime, format);
}

/*!
    Returns the \c qlonglong value of the string \a s read in the format of
    the locale. The text may have a sign, group separators in their places
    and surrounding white space, but no decimal point or exponent. If \a ok
    is not \c nullptr it is set to \c true if the conversion succeeded and to
    \c false otherwise, and 0 is returned on failure, including overflow.

    \sa toString()
*/
qlonglong QLocale::toLongLong(QStringView s, bool *ok) const {
    std::string c;
    long long v = 0;
    const bool good = localizedToC(*m_row, m_options, s, false, c) && QtPrivate::parseSigned(c, 10, v);
    if (ok)
        *ok = good;
    return good ? v : 0;
}

/*!
    Returns the \c qulonglong value of the string \a s read in the format of
    the locale. A negative number is an error. \a ok is set as for
    toLongLong().
*/
qulonglong QLocale::toULongLong(QStringView s, bool *ok) const {
    std::string c;
    unsigned long long v = 0;
    const bool good = localizedToC(*m_row, m_options, s, false, c) && QtPrivate::parseUnsigned(c, 10, v);
    if (ok)
        *ok = good;
    return good ? v : 0;
}

/*!
    Returns the \c short value of the string \a s; see toLongLong().
*/
short QLocale::toShort(QStringView s, bool *ok) const {
    bool good = false;
    const qlonglong v = toLongLong(s, &good);
    good = good && fitsIn<short>(v);
    if (ok)
        *ok = good;
    return good ? static_cast<short>(v) : short(0);
}

/*!
    Returns the \c{unsigned short} value of the string \a s; see
    toULongLong().
*/
ushort QLocale::toUShort(QStringView s, bool *ok) const {
    bool good = false;
    const qulonglong v = toULongLong(s, &good);
    good = good && v <= std::numeric_limits<ushort>::max();
    if (ok)
        *ok = good;
    return good ? static_cast<ushort>(v) : ushort(0);
}

/*!
    Returns the \c int value of the string \a s; see toLongLong().
*/
int QLocale::toInt(QStringView s, bool *ok) const {
    bool good = false;
    const qlonglong v = toLongLong(s, &good);
    good = good && fitsIn<int>(v);
    if (ok)
        *ok = good;
    return good ? static_cast<int>(v) : 0;
}

/*!
    Returns the \c{unsigned int} value of the string \a s; see
    toULongLong().
*/
uint QLocale::toUInt(QStringView s, bool *ok) const {
    bool good = false;
    const qulonglong v = toULongLong(s, &good);
    good = good && v <= std::numeric_limits<uint>::max();
    if (ok)
        *ok = good;
    return good ? static_cast<uint>(v) : 0u;
}

/*!
    Returns the \c long value of the string \a s; see toLongLong().
*/
long QLocale::toLong(QStringView s, bool *ok) const {
    bool good = false;
    const qlonglong v = toLongLong(s, &good);
    good = good && fitsIn<long>(v);
    if (ok)
        *ok = good;
    return good ? static_cast<long>(v) : 0L;
}

/*!
    Returns the \c{unsigned long} value of the string \a s; see
    toULongLong().
*/
ulong QLocale::toULong(QStringView s, bool *ok) const {
    bool good = false;
    const qulonglong v = toULongLong(s, &good);
    good = good && v <= std::numeric_limits<ulong>::max();
    if (ok)
        *ok = good;
    return good ? static_cast<ulong>(v) : 0UL;
}

/*!
    Returns the \c double value of the string \a s read in the format of the
    locale. The text may have a sign, group separators in their places in the
    integer part, the decimal point of the locale, an exponent, and
    surrounding white space; \c inf and \c nan are accepted in any case. \a ok
    is set as for toLongLong(). A value out of the range of \c double is an
    error.

    \sa toString()
*/
double QLocale::toDouble(QStringView s, bool *ok) const {
    std::string c;
    double v = 0;
    bool good = localizedToC(*m_row, m_options, s, true, c);
    if (good) {
        if (c == "nan") {
            v = std::nan("");
        } else if (c == "inf") {
            v = HUGE_VAL;
        } else if (c == "-inf") {
            v = -HUGE_VAL;
        } else {
            good = QtPrivate::parseDouble(c, v);
        }
    }
    if (ok)
        *ok = good;
    return good ? v : 0.0;
}

/*!
    Returns the \c float value of the string \a s; see toDouble(). A value out
    of the range of \c float is an error.
*/
float QLocale::toFloat(QStringView s, bool *ok) const {
    bool good = false;
    const double v = toDouble(s, &good);
    good = good && (std::isinf(v) || std::isnan(v) || std::fabs(v) <= static_cast<double>(std::numeric_limits<float>::max()));
    if (ok)
        *ok = good;
    return good ? static_cast<float>(v) : 0.0f;
}

/*!
    Returns the decimal point of the locale.

    \sa groupSeparator()
*/
QString QLocale::decimalPoint() const { return utf8(m_row->decimal); }

/*!
    Returns the group separator of the locale.

    \sa decimalPoint()
*/
QString QLocale::groupSeparator() const { return utf8(m_row->group); }

/*!
    Returns the percent sign of the locale.
*/
QString QLocale::percent() const { return utf8(m_row->percent); }

/*!
    Returns the digit zero of the locale, which is always \c{"0"} in Nt.
*/
QString QLocale::zeroDigit() const { return QString("0"); }

/*!
    Returns the negative sign of the locale. It is \c{"-"} except for
    Swedish, where it is the minus sign U+2212.

    \sa positiveSign()
*/
QString QLocale::negativeSign() const { return utf8(m_row->minus); }

/*!
    Returns the positive sign of the locale.

    \sa negativeSign()
*/
QString QLocale::positiveSign() const { return utf8(m_row->plus); }

/*!
    Returns the letter that introduces the exponent of a number in the
    locale.
*/
QString QLocale::exponential() const { return utf8(m_row->exponent); }

/*!
    Returns the format of dates of the locale for \a format. It is a format
    string as QLocale::toString(QDate, QStringView) takes. NarrowFormat gives
    the short format.

    \sa timeFormat(), dateTimeFormat()
*/
QString QLocale::dateFormat(FormatType format) const { return utf8(format == LongFormat ? m_row->dateLong : m_row->dateShort); }

/*!
    Returns the format of times of the locale for \a format. NarrowFormat
    gives the short format.

    \sa dateFormat(), dateTimeFormat()
*/
QString QLocale::timeFormat(FormatType format) const { return utf8(format == LongFormat ? m_row->timeLong : m_row->timeShort); }

/*!
    Returns the format of dates with times of the locale for \a format, which
    joins the date format and the time format.

    \sa dateFormat(), timeFormat()
*/
QString QLocale::dateTimeFormat(FormatType format) const {
    const bool isLong = format == LongFormat;
    return dateFormat(format) + utf8(isLong ? m_row->dateTimeJoinLong : m_row->dateTimeJoinShort) + timeFormat(format);
}

/*!
    Returns the name of the day of the week \a day, where 1 is Monday and 7 is
    Sunday, in the form \a type, as it is used inside a date. Returns an empty
    string if \a day is not between 1 and 7.

    \sa standaloneDayName(), monthName()
*/
QString QLocale::dayName(int day, FormatType type) const {
    if (day < 1 || day > 7)
        return QString();
    return namedList(*m_row, false, true, type, day - 1);
}

/*!
    Returns the name of the day of the week \a day in the form \a type, as it
    is used on its own, for instance in a calendar header.

    \sa dayName()
*/
QString QLocale::standaloneDayName(int day, FormatType type) const {
    if (day < 1 || day > 7)
        return QString();
    return namedList(*m_row, true, true, type, day - 1);
}

/*!
    Returns the name of the month \a month, where 1 is January, in the form
    \a type, as it is used inside a date. Returns an empty string if \a month
    is not between 1 and 12.

    \sa standaloneMonthName(), dayName()
*/
QString QLocale::monthName(int month, FormatType type) const {
    if (month < 1 || month > 12)
        return QString();
    return namedList(*m_row, false, false, type, month - 1);
}

/*!
    Returns the name of the month \a month in the form \a type, as it is used
    on its own, for instance in a calendar header.

    \sa monthName()
*/
QString QLocale::standaloneMonthName(int month, FormatType type) const {
    if (month < 1 || month > 12)
        return QString();
    return namedList(*m_row, true, false, type, month - 1);
}

/*!
    Returns the text for before noon (AM) of the locale.

    \sa pmText()
*/
QString QLocale::amText() const { return utf8(m_row->am); }

/*!
    Returns the text for after noon (PM) of the locale.

    \sa amText()
*/
QString QLocale::pmText() const { return utf8(m_row->pm); }

/*!
    Returns the first day of the week of the locale.

    \sa weekdays()
*/
Qt::DayOfWeek QLocale::firstDayOfWeek() const { return static_cast<Qt::DayOfWeek>(m_row->firstDay); }

/*!
    Returns the days of the week that are not in the weekend of the locale,
    from Monday to Sunday.

    \sa firstDayOfWeek()
*/
QList<Qt::DayOfWeek> QLocale::weekdays() const {
    QList<Qt::DayOfWeek> days;
    const int start = m_row->weekendStart;
    const int end = m_row->weekendEnd;
    for (int d = Qt::Monday; d <= Qt::Sunday; ++d) {
        const bool weekend = end >= start ? (d >= start && d <= end) : (d >= start || d <= end);
        if (!weekend)
            days.append(static_cast<Qt::DayOfWeek>(d));
    }
    return days;
}

/*!
    Returns the system of measurement units of the locale.
*/
QLocale::MeasurementSystem QLocale::measurementSystem() const { return m_row->measurement; }

/*!
    Returns the direction in which the language of the locale is written.
*/
Qt::LayoutDirection QLocale::textDirection() const {
    switch (m_language) {
    case Arabic:
    case Hebrew:
    case Persian:
    case Urdu:
        return Qt::RightToLeft;
    default:
        return Qt::LeftToRight;
    }
}

/*!
    Returns \a str in upper case according to the rules of the language of
    the locale. For Turkish and Azerbaijani, \c i becomes U+0130; otherwise this
    is QString::toUpper().

    \sa toLower()
*/
QString QLocale::toUpper(const QString &str) const {
    if (m_language == Turkish || m_language == Azerbaijani) {
        QString copy = str;
        for (QChar &c : copy) {
            if (c.unicode() == u'i')
                c = QChar(char16_t(0x130));
            else if (c.unicode() == 0x131)
                c = QChar(char16_t(u'I'));
        }
        return copy.toUpper();
    }
    return str.toUpper();
}

/*!
    Returns \a str in lower case according to the rules of the language of
    the locale. For Turkish and Azerbaijani, \c I becomes U+0131 and U+0130
    becomes \c i; otherwise this is QString::toLower().

    \sa toUpper()
*/
QString QLocale::toLower(const QString &str) const {
    if (m_language == Turkish || m_language == Azerbaijani) {
        QString copy = str;
        for (QChar &c : copy) {
            if (c.unicode() == u'I')
                c = QChar(char16_t(0x131));
            else if (c.unicode() == 0x130)
                c = QChar(char16_t(u'i'));
        }
        return copy.toLower();
    }
    return str.toLower();
}

/*!
    Returns \a bytes as a size with a unit, such as \c{"1.50 KiB"}, written
    with the number format of the locale. \a precision is the number of
    digits after the decimal point (at most 3 for each power of the base,
    and none for plain bytes) and \a format selects the base and the units.
    Sizes below one unit are written as \c{"n bytes"}.

    \sa toString()
*/
QString QLocale::formattedDataSize(qint64 bytes, int precision, DataSizeFormats format) const {
    int power = 0;
    qulonglong base = (format & DataSizeBase1000) ? 1000 : 1024;
    const qulonglong mag = bytes < 0 ? 0 - static_cast<qulonglong>(bytes) : static_cast<qulonglong>(bytes);
    qulonglong threshold = base;
    while (power < 6 && mag >= threshold) {
        ++power;
        threshold = power < 6 ? threshold * base : threshold;
    }
    if (power == 0)
        return toString(bytes) + QString(" bytes");
    double value = static_cast<double>(bytes);
    for (int i = 0; i < power; ++i)
        value /= static_cast<double>(base);
    static const char *const si[] = {"", "kB", "MB", "GB", "TB", "PB", "EB"};
    static const char *const iec[] = {"", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    const char *unit = (format & DataSizeSIQuantifiers) ? si[power] : iec[power];
    return toString(value, 'f', qMin(precision, 3 * power)) + QChar(u' ') + QString(unit);
}

/*!
    Returns the strings of \a list joined in the way the locale writes a list
    of items: \c{"a, b, and c"} in American English. A list of one string
    gives that string and an empty list an empty string.
*/
QString QLocale::createSeparatedList(const QStringList &list) const {
    const qsizetype n = list.size();
    if (n == 0)
        return QString();
    if (n == 1)
        return list[0];
    if (n == 2)
        return list[0] + utf8(m_row->listPair) + list[1];
    QString r = list[0];
    for (qsizetype i = 1; i < n - 1; ++i)
        r += utf8(m_row->listSeparator) + list[i];
    return r + utf8(m_row->listEnd) + list[n - 1];
}

/*!
    Returns the currency of the locale as the ISO 4217 code, the symbol or
    the English name, as \a format selects. The C locale has no currency and
    returns an empty string.

    \sa toCurrencyString()
*/
QString QLocale::currencySymbol(CurrencySymbolFormat format) const {
    switch (format) {
    case CurrencyIsoCode:
        return utf8(m_row->currencyIso);
    case CurrencyDisplayName:
        return utf8(m_row->currencyName);
    default:
        return utf8(m_row->currencySymbol);
    }
}

/*!
    Returns \a value written as an amount of money in the currency of the
    locale, with the currency symbol \a symbol, or the symbol of the
    locale's currency if \a symbol is null. The amount has as many decimal
    digits as the currency has.

    \sa currencySymbol()
*/
QString QLocale::toCurrencyString(qlonglong value, const QString &symbol) const { return toCurrencyString(static_cast<double>(value), symbol, -1); }

/*!
    \overload

    Returns the unsigned \a value as an amount of money.
*/
QString QLocale::toCurrencyString(qulonglong value, const QString &symbol) const { return toCurrencyString(static_cast<double>(value), symbol, -1); }

/*!
    \overload

    Returns \a value as an amount of money with \a precision digits after the
    decimal point; if \a precision is negative, the number of digits of the
    currency (two for the C locale) is used.
*/
QString QLocale::toCurrencyString(double value, const QString &symbol, int precision) const {
    const QString sym = symbol.isNull() ? currencySymbol() : symbol;
    const int digits = precision < 0 ? m_row->currencyDigits : precision;
    const bool negative = std::signbit(value) && value != 0.0;
    const QString number = toString(negative ? -value : value, 'f', digits);
    const QString sign = negative ? negativeSign() : QString();
    if (sym.isEmpty())
        return sign + number;
    const QString nbsp(QChar(char16_t(0xa0)));
    switch (m_row->currencyPosition) {
    case 1:
        return sign + sym + nbsp + number;
    case 2:
        return sign + number + nbsp + sym;
    default:
        return sign + sym + number;
    }
}

/*!
    Returns \a str enclosed in the quotation marks of the locale, of the kind
    \a style.
*/
QString QLocale::quoteString(QStringView str, QuotationStyle style) const {
    const bool alt = style == AlternateQuotation;
    return utf8(alt ? m_row->altQuoteStart : m_row->quoteStart) + str.toString() + utf8(alt ? m_row->altQuoteEnd : m_row->quoteEnd);
}

/*!
    Returns the list of languages for the user interface that the locale
    prefers, most preferred first, as BCP 47 tags with \a separator between
    their parts. For the system locale this starts with the languages of
    the \c LANGUAGE environment variable. It always includes the tag of
    the locale itself and its language alone, so that a translation for
    \c{de} is found for \c{de_AT}.

    \sa bcp47Name()
*/
QStringList QLocale::uiLanguages(TagSeparator separator) const {
    QStringList result;
    auto add = [&result](const QString &tag) {
        if (!tag.isEmpty() && !result.contains(tag))
            result.append(tag);
    };
    if (*this == system()) {
        for (const QString &name : environmentLanguages()) {
            const ParsedName p = parseName(name);
            if (p.valid && p.language != C)
                add(QLocale(name).bcp47Name(separator));
        }
    }
    add(bcp47Name(separator));
    if (m_language != C)
        add(languageToCode(m_language));
    return result;
}

/*!
    Returns the locale to use for collation, which is this locale.

    \sa QCollator
*/
QLocale QLocale::collation() const { return *this; }

bool QLocale::equals(const QLocale &other) const {
    return m_row == other.m_row && m_language == other.m_language && m_script == other.m_script && m_territory == other.m_territory
           && m_options == other.m_options;
}

/*!
    \relates QLocale

    Returns the hash value for \a key, using \a seed to seed the calculation.
*/
std::size_t qHash(const QLocale &key, std::size_t seed) noexcept {
    std::size_t h = seed;
    h = h * 31 + static_cast<std::size_t>(key.language());
    h = h * 31 + static_cast<std::size_t>(key.script());
    h = h * 31 + static_cast<std::size_t>(key.territory());
    h = h * 31 + static_cast<std::size_t>(static_cast<int>(key.numberOptions()));
    return h;
}

/*!
    \relates QLocale

    Writes \a l to the debug stream \a dbg.
*/
QDebug operator<<(QDebug dbg, const QLocale &l) {
    dbg.nospace() << "QLocale(" << QLocale::languageToString(l.language()) << ", " << QLocale::scriptToString(l.script()) << ", "
                  << QLocale::territoryToString(l.territory()) << ')';
    return dbg.maybeSpace();
}
