// MIT License - see LICENSE
#include <QtCore/qlocale.h>

#include <QtCore/qhash.h>

#include "qlocale_data_p.h"
#include "qunicode_p.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <mutex>
#include <string>

namespace {

struct CodeName {
    const char *code;
    const char *name;
};

// Indexed by QLocale::Language.
const CodeName languages[] = {
    {"", "Default"},      {"C", "C"},          {"af", "Afrikaans"}, {"sq", "Albanian"},   {"ar", "Arabic"},    {"hy", "Armenian"},
    {"eu", "Basque"},     {"be", "Belarusian"}, {"bn", "Bangla"},   {"bs", "Bosnian"},    {"bg", "Bulgarian"}, {"ca", "Catalan"},
    {"zh", "Chinese"},    {"hr", "Croatian"},  {"cs", "Czech"},     {"da", "Danish"},     {"nl", "Dutch"},     {"en", "English"},
    {"eo", "Esperanto"},  {"et", "Estonian"},  {"fil", "Filipino"}, {"fi", "Finnish"},    {"fr", "French"},    {"gl", "Galician"},
    {"ka", "Georgian"},   {"de", "German"},    {"el", "Greek"},     {"he", "Hebrew"},     {"hi", "Hindi"},     {"hu", "Hungarian"},
    {"is", "Icelandic"},  {"id", "Indonesian"}, {"ga", "Irish"},    {"it", "Italian"},    {"ja", "Japanese"},  {"kk", "Kazakh"},
    {"ko", "Korean"},     {"la", "Latin"},     {"lv", "Latvian"},   {"lt", "Lithuanian"}, {"mk", "Macedonian"}, {"ms", "Malay"},
    {"mt", "Maltese"},    {"nb", "Norwegian Bokm\xc3\xa5l"}, {"nn", "Norwegian Nynorsk"}, {"fa", "Persian"}, {"pl", "Polish"},
    {"pt", "Portuguese"}, {"ro", "Romanian"},  {"ru", "Russian"},   {"sr", "Serbian"},    {"sk", "Slovak"},    {"sl", "Slovenian"},
    {"es", "Spanish"},    {"sw", "Swahili"},   {"sv", "Swedish"},   {"ta", "Tamil"},      {"th", "Thai"},      {"tr", "Turkish"},
    {"uk", "Ukrainian"},  {"ur", "Urdu"},      {"vi", "Vietnamese"}, {"cy", "Welsh"},
};
static_assert(std::size(languages) == QLocale::LastLanguage + 1, "language table out of sync");

// Indexed by QLocale::Territory.
const CodeName territories[] = {
    {"", "Default"},       {"AR", "Argentina"},     {"AU", "Australia"},     {"AT", "Austria"},       {"BE", "Belgium"},
    {"BR", "Brazil"},      {"BG", "Bulgaria"},      {"CA", "Canada"},        {"CL", "Chile"},         {"CN", "China"},
    {"CO", "Colombia"},    {"HR", "Croatia"},       {"CZ", "Czechia"},       {"DK", "Denmark"},       {"EG", "Egypt"},
    {"EE", "Estonia"},     {"FI", "Finland"},       {"FR", "France"},        {"DE", "Germany"},       {"GR", "Greece"},
    {"HK", "Hong Kong"},   {"HU", "Hungary"},       {"IS", "Iceland"},       {"IN", "India"},         {"ID", "Indonesia"},
    {"IR", "Iran"},        {"IE", "Ireland"},       {"IL", "Israel"},        {"IT", "Italy"},         {"JP", "Japan"},
    {"LV", "Latvia"},      {"LT", "Lithuania"},     {"LU", "Luxembourg"},    {"MY", "Malaysia"},      {"MX", "Mexico"},
    {"NL", "Netherlands"}, {"NZ", "New Zealand"},   {"NO", "Norway"},        {"PK", "Pakistan"},      {"PE", "Peru"},
    {"PH", "Philippines"}, {"PL", "Poland"},        {"PT", "Portugal"},      {"RO", "Romania"},       {"RU", "Russia"},
    {"SA", "Saudi Arabia"}, {"RS", "Serbia"},       {"SG", "Singapore"},     {"SK", "Slovakia"},      {"SI", "Slovenia"},
    {"ZA", "South Africa"}, {"KR", "South Korea"},  {"ES", "Spain"},         {"SE", "Sweden"},        {"CH", "Switzerland"},
    {"TW", "Taiwan"},      {"TH", "Thailand"},      {"TR", "Turkey"},        {"UA", "Ukraine"},       {"GB", "United Kingdom"},
    {"US", "United States"}, {"VN", "Vietnam"},     {"001", "world"},
};
static_assert(std::size(territories) == QLocale::LastTerritory + 1, "territory table out of sync");

// Indexed by QLocale::Script.
const CodeName scripts[] = {
    {"", "Default"},     {"Arab", "Arabic"},  {"Armn", "Armenian"}, {"Beng", "Bangla"}, {"Cyrl", "Cyrillic"},
    {"Deva", "Devanagari"}, {"Geor", "Georgian"}, {"Grek", "Greek"}, {"Hebr", "Hebrew"}, {"Jpan", "Japanese"},
    {"Kore", "Korean"},  {"Latn", "Latin"},   {"Hans", "Simplified Han"}, {"Taml", "Tamil"}, {"Thai", "Thai"},
    {"Hant", "Traditional Han"},
};
static_assert(std::size(scripts) == QLocale::LastScript + 1, "script table out of sync");

const QLocaleData *cData() { return &localeTable[0]; }

const QLocaleData *findData(QLocale::Language lang, QLocale::Script script, QLocale::Territory terr) {
    if (lang == QLocale::C)
        return cData();
    const QLocaleData *byLanguage = nullptr;
    const QLocaleData *byScript = nullptr;
    for (const QLocaleData &d : localeTable) {
        if (d.language != lang)
            continue;
        if ((script == QLocale::AnyScript || d.script == script) && (terr == QLocale::AnyTerritory || d.territory == terr))
            return &d;
        if (script != QLocale::AnyScript && d.script == script && !byScript)
            byScript = &d;
        if (d.defaultForLanguage && !byLanguage)
            byLanguage = &d;
    }
    if (byScript)
        return byScript;
    if (byLanguage)
        return byLanguage;
    if (lang == QLocale::AnyLanguage && terr != QLocale::AnyTerritory) {
        for (const QLocaleData &d : localeTable)
            if (d.territory == terr)
                return &d;
    }
    return cData();
}

QString str(const char16_t *s) { return s ? QString(s) : QString(); }

QString nth(const char16_t *list, int index) {
    if (!list || index < 0)
        return QString();
    const char16_t *p = list;
    for (int i = 0; i < index; ++i) {
        while (*p && *p != u';')
            ++p;
        if (!*p)
            return QString();
        ++p;
    }
    const char16_t *e = p;
    while (*e && *e != u';')
        ++e;
    return QString(p, e - p);
}

const QLocaleData *dataForName(QStringView name) {
    QString n = name.toString();
    // Drop the encoding and the modifier: "de_DE.UTF-8@euro".
    for (const QChar stop : {QChar(u'.'), QChar(u'@')}) {
        const qsizetype k = n.indexOf(stop);
        if (k >= 0)
            n.truncate(k);
    }
    if (n.isEmpty() || n == u"C" || n == u"POSIX")
        return cData();
    n.replace(QChar(u'-'), QChar(u'_'));
    const QStringList parts = n.split(QChar(u'_'), Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return cData();
    const QLocale::Language lang = QLocale::codeToLanguage(parts.at(0));
    if (lang == QLocale::AnyLanguage)
        return cData();
    QLocale::Script script = QLocale::AnyScript;
    QLocale::Territory terr = QLocale::AnyTerritory;
    for (qsizetype k = 1; k < parts.size(); ++k) {
        const QString &p = parts.at(k);
        if (p.size() == 4 && script == QLocale::AnyScript)
            script = QLocale::codeToScript(p);
        else if ((p.size() == 2 || p.size() == 3) && terr == QLocale::AnyTerritory)
            terr = QLocale::codeToTerritory(p);
    }
    return findData(lang, script, terr);
}

std::mutex defaultMutex;
const QLocaleData *defaultData = nullptr;
QLocale::NumberOptions defaultOptions;
bool defaultSet = false;

QLocale::NumberOptions optionsFor(const QLocaleData *d) {
    return d == cData() ? QLocale::NumberOptions(QLocale::OmitGroupSeparator) : QLocale::NumberOptions(QLocale::DefaultNumberOptions);
}

// --- numbers

QString groupDigits(const QString &digits, const QLocaleData *d, QLocale::NumberOptions options) {
    if (options.testFlag(QLocale::OmitGroupSeparator) || digits.size() < 3 + d->minGrouping)
        return digits;
    const QString sep = str(d->group);
    QString r;
    const qsizetype n = digits.size();
    for (qsizetype i = 0; i < n; ++i) {
        if (i > 0 && (n - i) % 3 == 0)
            r.append(sep);
        r.append(digits.at(i));
    }
    return r;
}

// Rewrites a number in C notation ("-1234.5e+07") with the symbols of the locale.
QString localize(const std::string &c, const QLocaleData *d, QLocale::NumberOptions options, bool groupInteger) {
    QString out;
    std::size_t i = 0;
    if (i < c.size() && (c[i] == '-' || c[i] == '+')) {
        out.append(c[i] == '-' ? str(d->minus) : QString(u"+"));
        ++i;
    }
    std::size_t intEnd = i;
    while (intEnd < c.size() && c[intEnd] >= '0' && c[intEnd] <= '9')
        ++intEnd;
    if (intEnd == i) { // inf, nan
        out.append(QString::fromLatin1(c.data() + i, static_cast<qsizetype>(c.size() - i)));
        return out;
    }
    const QString integer = QString::fromLatin1(c.data() + i, static_cast<qsizetype>(intEnd - i));
    out.append(groupInteger ? groupDigits(integer, d, options) : integer);
    i = intEnd;
    if (i < c.size() && c[i] == '.') {
        out.append(str(d->decimal));
        ++i;
        while (i < c.size() && c[i] >= '0' && c[i] <= '9')
            out.append(QChar(c[i++]));
    }
    if (i < c.size() && (c[i] == 'e' || c[i] == 'E')) {
        out.append(QChar(c[i++]));
        if (i < c.size() && (c[i] == '-' || c[i] == '+')) {
            out.append(c[i] == '-' ? str(d->minus) : QString(u"+"));
            ++i;
        }
        std::string exp = c.substr(i);
        if (options.testFlag(QLocale::OmitLeadingZeroInExponent))
            while (exp.size() > 1 && exp[0] == '0')
                exp.erase(0, 1);
        out.append(QString::fromLatin1(exp.data(), static_cast<qsizetype>(exp.size())));
    }
    return out;
}

std::string printDouble(double v, char format, int precision, QLocale::NumberOptions options) {
    char buf[512];
    const char f = (format == 'f' || format == 'F' || format == 'e' || format == 'E' || format == 'g' || format == 'G') ? format : 'g';
    if (precision == QLocale::FloatingPointShortest) {
        // The fewest significant digits that read back as the same value.
        int digits = 1;
        for (; digits < 17; ++digits) {
            std::snprintf(buf, sizeof buf, "%.*e", digits - 1, v);
            if (std::strtod(buf, nullptr) == v)
                break;
        }
        if (!std::isfinite(v))
            std::snprintf(buf, sizeof buf, f == 'E' || f == 'G' ? "%G" : "%g", v);
        else if (f == 'e' || f == 'E')
            std::snprintf(buf, sizeof buf, f == 'e' ? "%.*e" : "%.*E", digits - 1, v);
        else if (f == 'f' || f == 'F') {
            const int exponent = v == 0 ? 0 : static_cast<int>(std::floor(std::log10(std::fabs(v))));
            std::snprintf(buf, sizeof buf, "%.*f", std::max(0, digits - 1 - exponent), v);
        } else {
            std::snprintf(buf, sizeof buf, f == 'g' ? "%.*g" : "%.*G", digits, v);
        }
        return buf;
    }
    if (precision < 0)
        precision = 6;
    if ((f == 'g' || f == 'G') && options.testFlag(QLocale::IncludeTrailingZeroesAfterDot))
        std::snprintf(buf, sizeof buf, f == 'g' ? "%#.*g" : "%#.*G", precision, v);
    else {
        const char fmt[] = {'%', '.', '*', f, '\0'};
        std::snprintf(buf, sizeof buf, fmt, precision, v);
    }
    std::string r = buf;
    if (!r.empty() && r.back() == '.')
        r.pop_back();
    return r;
}

bool isGroupChar(QChar c, const QString &group) {
    if (group.size() == 1 && c == group.at(0))
        return true;
    // A space stands for a no-break space separator, which is hard to type.
    return c == u' ' && (group == u" " || group == u" ");
}

// Converts a localized number to C notation. Returns false if it is malformed.
bool delocalize(QStringView s, const QLocaleData *d, QLocale::NumberOptions options, bool allowFraction, std::string *out) {
    const QString decimal = str(d->decimal), group = str(d->group), minus = str(d->minus);
    qsizetype b = 0, e = s.size();
    while (b < e && s.at(b).isSpace())
        ++b;
    while (e > b && s.at(e - 1).isSpace())
        --e;
    if (b == e)
        return false;
    std::string r;
    qsizetype groupDigitsSince = -1; // digits since the last group separator, or -1 if none yet
    qsizetype leadingDigits = 0;     // digits before the first separator
    bool inFraction = false, inExponent = false, sawDigit = false;
    for (qsizetype i = b; i < e; ++i) {
        const QChar c = s.at(i);
        if (c >= u'0' && c <= u'9') {
            r.push_back(static_cast<char>(c.unicode()));
            sawDigit = true;
            if (!inFraction && !inExponent) {
                if (groupDigitsSince >= 0)
                    ++groupDigitsSince;
                else
                    ++leadingDigits;
            }
            continue;
        }
        const bool atStart = r.empty() || (inExponent && (r.back() == 'e'));
        if ((c == u'-' || (minus.size() == 1 && c == minus.at(0))) && atStart) {
            r.push_back('-');
            continue;
        }
        if (c == u'+' && atStart) {
            r.push_back('+');
            continue;
        }
        if (!inFraction && !inExponent && sawDigit && isGroupChar(c, group) && !(decimal.size() == 1 && c == decimal.at(0))) {
            if (options.testFlag(QLocale::RejectGroupSeparator))
                return false;
            if ((groupDigitsSince < 0 && (leadingDigits < 1 || leadingDigits > 3)) || (groupDigitsSince >= 0 && groupDigitsSince != 3))
                return false;
            groupDigitsSince = 0;
            continue;
        }
        if (allowFraction && !inFraction && !inExponent && decimal.size() == 1 && c == decimal.at(0)) {
            if (groupDigitsSince >= 0 && groupDigitsSince != 3)
                return false;
            groupDigitsSince = -2;
            inFraction = true;
            r.push_back('.');
            continue;
        }
        if (allowFraction && !inExponent && sawDigit && (c == u'e' || c == u'E')) {
            if (groupDigitsSince >= 0 && groupDigitsSince != 3)
                return false;
            if (options.testFlag(QLocale::RejectTrailingZeroesAfterDot) && inFraction && r.back() == '0')
                return false;
            groupDigitsSince = -2;
            inExponent = true;
            r.push_back('e');
            if (options.testFlag(QLocale::RejectLeadingZeroInExponent)) {
                qsizetype k = i + 1;
                if (k < e && (s.at(k) == u'+' || s.at(k) == u'-' || (minus.size() == 1 && s.at(k) == minus.at(0))))
                    ++k;
                if (k + 1 < e && s.at(k) == u'0')
                    return false;
            }
            continue;
        }
        return false;
    }
    if (groupDigitsSince >= 0 && groupDigitsSince != 3)
        return false;
    if (!sawDigit)
        return false;
    if (options.testFlag(QLocale::RejectTrailingZeroesAfterDot) && inFraction && !inExponent && r.back() == '0')
        return false;
    *out = r;
    return true;
}

template <typename T>
T toSigned(QStringView s, const QLocaleData *d, QLocale::NumberOptions options, bool *ok) {
    std::string c;
    bool good = delocalize(s, d, options, false, &c);
    long long v = 0;
    if (good) {
        errno = 0;
        char *end = nullptr;
        v = std::strtoll(c.c_str(), &end, 10);
        good = errno == 0 && end && *end == '\0' && v >= std::numeric_limits<T>::min() && v <= std::numeric_limits<T>::max();
    }
    if (ok)
        *ok = good;
    return good ? static_cast<T>(v) : T(0);
}

template <typename T>
T toUnsigned(QStringView s, const QLocaleData *d, QLocale::NumberOptions options, bool *ok) {
    std::string c;
    bool good = delocalize(s, d, options, false, &c) && !c.empty() && c[0] != '-';
    unsigned long long v = 0;
    if (good) {
        errno = 0;
        char *end = nullptr;
        v = std::strtoull(c.c_str(), &end, 10);
        good = errno == 0 && end && *end == '\0' && v <= std::numeric_limits<T>::max();
    }
    if (ok)
        *ok = good;
    return good ? static_cast<T>(v) : T(0);
}

// --- dates and times

struct Token {
    enum Kind { Literal, Field } kind = Literal;
    QChar letter;
    int count = 0;
    QString text;
};

QList<Token> tokenize(const QString &format) {
    QList<Token> tokens;
    const qsizetype n = format.size();
    auto literal = [&](const QString &t) {
        if (!tokens.isEmpty() && tokens.last().kind == Token::Literal)
            tokens.last().text.append(t);
        else {
            Token tok;
            tok.text = t;
            tokens.append(tok);
        }
    };
    for (qsizetype i = 0; i < n;) {
        const QChar c = format.at(i);
        if (c == u'\'') {
            if (i + 1 < n && format.at(i + 1) == u'\'') {
                literal(QString(u"'"));
                i += 2;
                continue;
            }
            QString text;
            ++i;
            while (i < n) {
                if (format.at(i) == u'\'') {
                    if (i + 1 < n && format.at(i + 1) == u'\'') {
                        text.append(QChar(u'\''));
                        i += 2;
                        continue;
                    }
                    ++i;
                    break;
                }
                text.append(format.at(i++));
            }
            literal(text);
            continue;
        }
        qsizetype run = 1;
        while (i + run < n && format.at(i + run) == c)
            ++run;
        int take = 0;
        switch (c.unicode()) {
        case u'd':
        case u'M':
            take = static_cast<int>(std::min<qsizetype>(run, 4));
            break;
        case u'y':
            take = run >= 4 ? 4 : (run >= 2 ? 2 : 0);
            break;
        case u'h':
        case u'H':
        case u'm':
        case u's':
            take = static_cast<int>(std::min<qsizetype>(run, 2));
            break;
        case u'z':
            take = run >= 3 ? 3 : 1;
            break;
        case u't':
            take = 1;
            break;
        case u'A':
        case u'a':
            take = (i + 1 < n && (format.at(i + 1) == u'P' || format.at(i + 1) == u'p')) ? 2 : 1;
            break;
        default:
            break;
        }
        if (take == 0) {
            literal(QString(c));
            ++i;
            continue;
        }
        Token tok;
        tok.kind = Token::Field;
        tok.letter = c;
        tok.count = take;
        tokens.append(tok);
        i += take;
    }
    return tokens;
}

QString pad(int v, int width) {
    QString s = QString::number(std::abs(v));
    while (s.size() < width)
        s.prepend(QChar(u'0'));
    return v < 0 ? QString(u"-") + s : s;
}

bool hasAmPm(const QList<Token> &tokens) {
    return std::any_of(tokens.begin(), tokens.end(), [](const Token &t) { return t.kind == Token::Field && (t.letter == u'A' || t.letter == u'a'); });
}

QString zoneText(const QDateTime *dt, QTime time) {
    if (dt)
        return dt->toString(QString(u"t"));
    return QDateTime(QDate::currentDate(), time.isValid() ? time : QTime(0, 0)).toString(QString(u"t"));
}

QString formatDateTime(const QLocale &loc, const QLocaleData *d, const QString &format, QDate date, QTime time, const QDateTime *dt) {
    const QList<Token> tokens = tokenize(format);
    const bool ampm = hasAmPm(tokens);
    QString out;
    for (const Token &t : tokens) {
        if (t.kind == Token::Literal) {
            out.append(t.text);
            continue;
        }
        const char16_t l = t.letter.unicode();
        const bool isDate = l == u'd' || l == u'M' || l == u'y';
        if (isDate && !date.isValid()) {
            continue;
        }
        if (!isDate && !time.isValid()) {
            continue;
        }
        switch (l) {
        case u'd':
            if (t.count <= 2)
                out.append(pad(date.day(), t.count));
            else
                out.append(loc.dayName(date.dayOfWeek(), t.count == 3 ? QLocale::ShortFormat : QLocale::LongFormat));
            break;
        case u'M':
            if (t.count <= 2)
                out.append(pad(date.month(), t.count));
            else
                out.append(loc.monthName(date.month(), t.count == 3 ? QLocale::ShortFormat : QLocale::LongFormat));
            break;
        case u'y':
            if (t.count == 2)
                out.append(pad(((date.year() % 100) + 100) % 100, 2));
            else
                out.append(pad(date.year(), 4));
            break;
        case u'h': {
            int h = time.hour();
            if (ampm) {
                h %= 12;
                if (h == 0)
                    h = 12;
            }
            out.append(pad(h, t.count));
            break;
        }
        case u'H':
            out.append(pad(time.hour(), t.count));
            break;
        case u'm':
            out.append(pad(time.minute(), t.count));
            break;
        case u's':
            out.append(pad(time.second(), t.count));
            break;
        case u'z':
            if (t.count == 3) {
                out.append(pad(time.msec(), 3));
            } else {
                QString z = pad(time.msec(), 3);
                while (z.size() > 1 && z.endsWith(QChar(u'0')))
                    z.chop(1);
                out.append(z);
            }
            break;
        case u'A':
        case u'a': {
            const QString text = time.hour() < 12 ? str(d->am) : str(d->pm);
            out.append(l == u'A' ? loc.toUpper(text) : loc.toLower(text));
            break;
        }
        case u't':
            out.append(zoneText(dt, time));
            break;
        default:
            break;
        }
    }
    return out.trimmed();
}

bool readNumber(const QString &s, qsizetype &pos, int minDigits, int maxDigits, int *out, bool allowSign = false) {
    bool negative = false;
    qsizetype p = pos;
    if (allowSign && p < s.size() && s.at(p) == u'-') {
        negative = true;
        ++p;
    }
    int v = 0, digits = 0;
    while (p < s.size() && digits < maxDigits && s.at(p) >= u'0' && s.at(p) <= u'9') {
        v = v * 10 + (s.at(p).unicode() - u'0');
        ++p;
        ++digits;
    }
    if (digits < minDigits)
        return false;
    pos = p;
    *out = negative ? -v : v;
    return true;
}

// Matches the longest of the names in \a names (as returned by name(i)) at pos, case-insensitively.
template <typename NameFn>
int readName(const QString &s, qsizetype &pos, int count, NameFn name) {
    int best = -1;
    qsizetype bestLen = 0;
    for (int i = 1; i <= count; ++i) {
        const QString n = name(i);
        if (n.isEmpty() || n.size() <= bestLen || pos + n.size() > s.size())
            continue;
        if (s.mid(pos, n.size()).compare(n, Qt::CaseInsensitive) == 0) {
            best = i;
            bestLen = n.size();
        }
    }
    if (best > 0)
        pos += bestLen;
    return best;
}

bool parseDateTime(const QLocale &loc, const QLocaleData *d, const QString &string, const QString &format, QDate *date, QTime *time,
                   int *offset, bool *hasZone) {
    const QList<Token> tokens = tokenize(format);
    const bool ampm = hasAmPm(tokens);
    const QString s = string.trimmed();
    qsizetype pos = 0;
    int year = 1900, month = 1, day = 1, hour = 0, minute = 0, second = 0, msec = 0;
    int pm = -1;
    for (const Token &t : tokens) {
        if (t.kind == Token::Literal) {
            const QString lit = t.text;
            for (qsizetype k = 0; k < lit.size(); ++k) {
                if (lit.at(k).isSpace()) {
                    // Any run of white space matches a run of white space.
                    if (pos >= s.size() || !s.at(pos).isSpace())
                        return false;
                    while (pos < s.size() && s.at(pos).isSpace())
                        ++pos;
                    while (k + 1 < lit.size() && lit.at(k + 1).isSpace())
                        ++k;
                    continue;
                }
                if (pos >= s.size() || s.at(pos) != lit.at(k))
                    return false;
                ++pos;
            }
            continue;
        }
        const char16_t l = t.letter.unicode();
        bool ok = true;
        switch (l) {
        case u'd':
            if (t.count <= 2)
                ok = readNumber(s, pos, t.count, 2, &day);
            else
                ok = readName(s, pos, 7, [&](int i) { return loc.dayName(i, t.count == 3 ? QLocale::ShortFormat : QLocale::LongFormat); }) > 0 ||
                     readName(s, pos, 7, [&](int i) { return loc.standaloneDayName(i, t.count == 3 ? QLocale::ShortFormat : QLocale::LongFormat); }) > 0;
            break;
        case u'M':
            if (t.count <= 2) {
                ok = readNumber(s, pos, t.count, 2, &month);
            } else {
                const QLocale::FormatType ft = t.count == 3 ? QLocale::ShortFormat : QLocale::LongFormat;
                int m = readName(s, pos, 12, [&](int i) { return loc.monthName(i, ft); });
                if (m < 0)
                    m = readName(s, pos, 12, [&](int i) { return loc.standaloneMonthName(i, ft); });
                ok = m > 0;
                if (ok)
                    month = m;
            }
            break;
        case u'y':
            if (t.count == 2) {
                ok = readNumber(s, pos, 2, 2, &year);
                year += 1900;
            } else {
                ok = readNumber(s, pos, 4, 4, &year, true);
            }
            break;
        case u'h':
        case u'H':
            ok = readNumber(s, pos, t.count, 2, &hour);
            break;
        case u'm':
            ok = readNumber(s, pos, t.count, 2, &minute);
            break;
        case u's':
            ok = readNumber(s, pos, t.count, 2, &second);
            break;
        case u'z': {
            const qsizetype start = pos;
            ok = readNumber(s, pos, t.count == 3 ? 3 : 1, 3, &msec);
            if (ok && t.count == 1) {
                for (qsizetype digits = pos - start; digits < 3; ++digits)
                    msec *= 10;
            }
            break;
        }
        case u'A':
        case u'a': {
            const QString am = str(d->am), pmText = str(d->pm);
            if (s.mid(pos, am.size()).compare(am, Qt::CaseInsensitive) == 0) {
                pm = 0;
                pos += am.size();
            } else if (s.mid(pos, pmText.size()).compare(pmText, Qt::CaseInsensitive) == 0) {
                pm = 1;
                pos += pmText.size();
            } else {
                ok = false;
            }
            break;
        }
        case u't': {
            if (s.mid(pos, 3) == u"UTC" || s.mid(pos, 3) == u"GMT") {
                pos += 3;
                *hasZone = true;
                *offset = 0;
                if (pos < s.size() && (s.at(pos) == u'+' || s.at(pos) == u'-')) {
                    const int sign = s.at(pos) == u'-' ? -1 : 1;
                    ++pos;
                    int oh = 0, om = 0;
                    ok = readNumber(s, pos, 1, 2, &oh);
                    if (ok && pos < s.size() && s.at(pos) == u':') {
                        ++pos;
                        ok = readNumber(s, pos, 2, 2, &om);
                    }
                    *offset = sign * (oh * 3600 + om * 60);
                }
            } else if (s.mid(pos, 1) == u"Z") {
                ++pos;
                *hasZone = true;
                *offset = 0;
            } else {
                while (pos < s.size() && !s.at(pos).isSpace())
                    ++pos;
            }
            break;
        }
        default:
            break;
        }
        if (!ok)
            return false;
    }
    if (pos != s.size())
        return false;
    if (ampm && pm >= 0) {
        if (hour < 1 || hour > 12)
            return false;
        hour = hour % 12 + (pm ? 12 : 0);
    }
    if (date)
        *date = QDate(year, month, day);
    if (time)
        *time = QTime(hour, minute, second, msec);
    return (!date || date->isValid()) && (!time || time->isValid());
}

} // namespace

/*!
    Constructs a QLocale object initialized with the default locale: the
    locale set with setDefault(), or else the system locale.

    \sa setDefault(), system()
*/
QLocale::QLocale() {
    std::lock_guard<std::mutex> lock(defaultMutex);
    if (defaultSet) {
        d = defaultData;
        m_options = defaultOptions;
    } else {
        const QLocale sys = system();
        d = sys.d;
        m_options = sys.m_options;
    }
}

/*!
    Constructs a locale from the name \a name, which has the format
    \c{language[_script][_territory]} (dashes may be used instead of
    underscores), such as \c{"de"}, \c{"de_CH"} or \c{"zh_Hant_TW"}. An
    encoding or modifier suffix, as in \c{"de_DE.UTF-8"}, is ignored. If the
    name is not recognized or Nt has no data for it, the C locale is used.
*/
QLocale::QLocale(const QString &name) : QLocale(QStringView(name)) {}

/*!
    \overload
    \since 6.3
    Constructs a locale from the name viewed by \a name.
*/
QLocale::QLocale(QStringView name) : d(dataForName(name)), m_options(optionsFor(d)) {}

/*!
    Constructs a locale for \a language and \a territory. If there is no data
    for the territory, the main territory of the language is used; if there
    is no data for the language, the C locale is used.
*/
QLocale::QLocale(Language language, Territory territory) : d(findData(language, AnyScript, territory)), m_options(optionsFor(d)) {}

/*!
    Constructs a locale for \a language written in \a script in \a territory.
*/
QLocale::QLocale(Language language, Script script, Territory territory)
    : d(findData(language, script, territory)), m_options(optionsFor(d)) {}

QLocale::QLocale(const QLocaleData *data, NumberOptions options) : d(data), m_options(options) {}

/*!
    Returns the language of the locale.
*/
QLocale::Language QLocale::language() const { return d->language; }

/*!
    Returns the script of the locale.
*/
QLocale::Script QLocale::script() const { return d->script; }

/*!
    \since 6.2
    Returns the territory of the locale.
*/
QLocale::Territory QLocale::territory() const { return d->territory; }

/*!
    Returns the name of the locale as \c{language_territory}, such as
    \c{"de_DE"}, using \a separator between the parts, or \c{"C"}.

    \sa bcp47Name()
*/
QString QLocale::name(TagSeparator separator) const {
    QString n = QString::fromLatin1(d->name);
    if (separator != TagSeparator::Underscore)
        n.replace(QChar(u'_'), QChar(static_cast<char>(separator)));
    return n;
}

/*!
    Returns the BCP 47 name of the locale, using \a separator between the
    parts. The territory is left out when it is the main territory of the
    language, so the name of en_US is \c{"en"} and that of en_GB is
    \c{"en-GB"}.

    \sa name(), uiLanguages()
*/
QString QLocale::bcp47Name(TagSeparator separator) const {
    if (d == cData())
        return QString(u"en");
    QString n = QString::fromLatin1(languages[d->language].code);
    if (!d->defaultForLanguage) {
        n.append(QChar(static_cast<char>(separator)));
        n.append(QString::fromLatin1(territories[d->territory].code));
    }
    return n;
}

/*!
    \since 4.8
    Returns the name of the language in that language, such as
    \c{"Deutsch"}.
*/
QString QLocale::nativeLanguageName() const { return str(d->nativeLanguage); }

/*!
    \since 6.2
    Returns the name of the territory in the language of the locale, such as
    \c{"Deutschland"}.
*/
QString QLocale::nativeTerritoryName() const { return str(d->nativeTerritory); }

/*!
    Returns the decimal separator of the locale.

    \sa groupSeparator(), toString()
*/
QString QLocale::decimalPoint() const { return str(d->decimal); }

/*!
    Returns the digit group separator of the locale.

    \sa decimalPoint(), numberOptions()
*/
QString QLocale::groupSeparator() const { return str(d->group); }

/*!
    Returns the percent sign of the locale.
*/
QString QLocale::percent() const { return QString(u"%"); }

/*!
    Returns the zero digit of the locale. It is always \c{"0"} in Nt.
*/
QString QLocale::zeroDigit() const { return QString(u"0"); }

/*!
    Returns the negative sign of the locale. Some locales, such as Swedish,
    use U+2212 MINUS SIGN.
*/
QString QLocale::negativeSign() const { return str(d->minus); }

/*!
    Returns the positive sign of the locale.
*/
QString QLocale::positiveSign() const { return QString(u"+"); }

/*!
    Returns the exponent separator of the locale, which is always \c{"e"}
    in Nt.
*/
QString QLocale::exponential() const { return QString(u"e"); }

/*!
    Returns the integer \a i formatted for the locale, with group separators
    unless numberOptions() contains OmitGroupSeparator.

    \sa toLongLong(), toCurrencyString()
*/
QString QLocale::toString(qlonglong i) const {
    const qulonglong magnitude = i < 0 ? qulonglong(0) - static_cast<qulonglong>(i) : static_cast<qulonglong>(i);
    const QString digits = groupDigits(QString::number(magnitude), d, m_options);
    return i < 0 ? str(d->minus) + digits : digits;
}

/*!
    \overload
    Returns the unsigned integer \a i formatted for the locale.
*/
QString QLocale::toString(qulonglong i) const { return groupDigits(QString::number(i), d, m_options); }

/*!
    \overload
    Returns the floating-point number \a f formatted for the locale. \a format
    is one of \c{'e'}, \c{'E'}, \c{'f'}, \c{'g'} and \c{'G'}, as for
    QString::number(), and \a precision is the number of digits after the
    decimal point (\c{'e'}, \c{'f'}) or of significant digits (\c{'g'}), or
    FloatingPointShortest.

    \sa toDouble()
*/
QString QLocale::toString(double f, char format, int precision) const {
    const std::string c = printDouble(f, format, precision, m_options);
    return localize(c, d, m_options, true);
}

/*!
    \overload
    Returns \a date formatted with the format string \a format, using the
    month and day names of the locale. The format uses the expressions of
    QDate::toString(): \c d, \c dd, \c ddd, \c dddd, \c M, \c MM, \c MMM,
    \c MMMM, \c yy, \c yyyy and quoted text.
*/
QString QLocale::toString(QDate date, const QString &format) const { return formatDateTime(*this, d, format, date, QTime(), nullptr); }

/*!
    \overload
    Returns \a time formatted with the format string \a format: \c h, \c hh,
    \c H, \c HH, \c m, \c mm, \c s, \c ss, \c z, \c zzz, \c AP, \c ap, \c t
    and quoted text. AM and PM are the texts of the locale.
*/
QString QLocale::toString(QTime time, const QString &format) const { return formatDateTime(*this, d, format, QDate(), time, nullptr); }

/*!
    \overload
    Returns \a dateTime formatted with the format string \a format, which may
    use the date and the time expressions.
*/
QString QLocale::toString(const QDateTime &dateTime, const QString &format) const {
    return formatDateTime(*this, d, format, dateTime.date(), dateTime.time(), &dateTime);
}

/*!
    \overload
    Returns \a date in the date format \a format of the locale.

    \sa dateFormat()
*/
QString QLocale::toString(QDate date, FormatType format) const { return toString(date, dateFormat(format)); }

/*!
    \overload
    Returns \a time in the time format \a format of the locale.

    \sa timeFormat()
*/
QString QLocale::toString(QTime time, FormatType format) const { return toString(time, timeFormat(format)); }

/*!
    \overload
    Returns \a dateTime in the date and time format \a format of the locale.

    \sa dateTimeFormat()
*/
QString QLocale::toString(const QDateTime &dateTime, FormatType format) const { return toString(dateTime, dateTimeFormat(format)); }

/*!
    Returns the number in \a s as a \c short, or 0 if it is not a valid
    number for the locale or out of range. If \a ok is not \c nullptr, it is
    set to whether the conversion succeeded. Group separators are accepted
    in the right places unless numberOptions() contains RejectGroupSeparator;
    leading and trailing white space is ignored.
*/
short QLocale::toShort(QStringView s, bool *ok) const { return toSigned<short>(s, d, m_options, ok); }

/*!
    Returns the number in \a s as an unsigned \c short; sets \a ok as
    toShort() does.
*/
ushort QLocale::toUShort(QStringView s, bool *ok) const { return toUnsigned<ushort>(s, d, m_options, ok); }

/*!
    Returns the number in \a s as an \c int; sets \a ok as toShort() does.
*/
int QLocale::toInt(QStringView s, bool *ok) const { return toSigned<int>(s, d, m_options, ok); }

/*!
    Returns the number in \a s as an unsigned \c int; sets \a ok as toShort()
    does.
*/
uint QLocale::toUInt(QStringView s, bool *ok) const { return toUnsigned<uint>(s, d, m_options, ok); }

/*!
    Returns the number in \a s as a \c long; sets \a ok as toShort() does.
*/
long QLocale::toLong(QStringView s, bool *ok) const { return toSigned<long>(s, d, m_options, ok); }

/*!
    Returns the number in \a s as an unsigned \c long; sets \a ok as
    toShort() does.
*/
ulong QLocale::toULong(QStringView s, bool *ok) const { return toUnsigned<ulong>(s, d, m_options, ok); }

/*!
    Returns the number in \a s as a qlonglong; sets \a ok as toShort() does.
*/
qlonglong QLocale::toLongLong(QStringView s, bool *ok) const { return toSigned<qlonglong>(s, d, m_options, ok); }

/*!
    Returns the number in \a s as a qulonglong; sets \a ok as toShort() does.
*/
qulonglong QLocale::toULongLong(QStringView s, bool *ok) const { return toUnsigned<qulonglong>(s, d, m_options, ok); }

/*!
    Returns the number in \a s as a \c float, or 0 if it is not valid or out
    of the range of \c float; sets \a ok as toDouble() does.
*/
float QLocale::toFloat(QStringView s, bool *ok) const {
    bool good = false;
    const double v = toDouble(s, &good);
    if (good && std::isfinite(v) && std::fabs(v) > static_cast<double>(std::numeric_limits<float>::max()))
        good = false;
    if (ok)
        *ok = good;
    return good ? static_cast<float>(v) : 0.0f;
}

/*!
    Returns the floating-point number in \a s, written with the decimal
    separator of the locale, or 0 if it is not valid. If \a ok is not
    \c nullptr, it is set to whether the conversion succeeded.

    \sa toString(), toInt()
*/
double QLocale::toDouble(QStringView s, bool *ok) const {
    std::string c;
    bool good = delocalize(s, d, m_options, true, &c);
    double v = 0;
    if (good)
        v = QString::fromLatin1(c.data(), static_cast<qsizetype>(c.size())).toDouble(&good);
    if (!good) {
        // "inf" and "nan" are accepted as in the C locale.
        const QString t = s.toString().trimmed().toLower();
        if (t == u"inf" || t == u"+inf" || t == u"-inf" || t == u"nan") {
            good = true;
            v = t == u"nan" ? std::numeric_limits<double>::quiet_NaN()
                            : (t.startsWith(QChar(u'-')) ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity());
        }
    }
    if (ok)
        *ok = good;
    return good ? v : 0.0;
}

/*!
    Returns the date in \a string, which must be in the date format \a format
    of the locale, or an invalid date.

    \sa toString(), dateFormat()
*/
QDate QLocale::toDate(const QString &string, FormatType format) const { return toDate(string, dateFormat(format)); }

/*!
    \overload
    Returns the date in \a string, which must follow the format string
    \a format, or an invalid date. Month and day names are those of the
    locale and are matched regardless of case. Missing fields default to
    1900-01-01.
*/
QDate QLocale::toDate(const QString &string, const QString &format) const {
    QDate date;
    int offset = 0;
    bool zone = false;
    return parseDateTime(*this, d, string, format, &date, nullptr, &offset, &zone) ? date : QDate();
}

/*!
    Returns the time in \a string, which must be in the time format \a format
    of the locale, or an invalid time.
*/
QTime QLocale::toTime(const QString &string, FormatType format) const { return toTime(string, timeFormat(format)); }

/*!
    \overload
    Returns the time in \a string, which must follow the format string
    \a format, or an invalid time.
*/
QTime QLocale::toTime(const QString &string, const QString &format) const {
    QTime time;
    int offset = 0;
    bool zone = false;
    return parseDateTime(*this, d, string, format, nullptr, &time, &offset, &zone) ? time : QTime();
}

/*!
    Returns the date and time in \a string, which must be in the date and
    time format \a format of the locale, or an invalid QDateTime.
*/
QDateTime QLocale::toDateTime(const QString &string, FormatType format) const { return toDateTime(string, dateTimeFormat(format)); }

/*!
    \overload
    Returns the date and time in \a string, which must follow the format
    string \a format, or an invalid QDateTime. A \c t field accepts
    \c{UTC}, \c{UTC+hh:mm} and \c{Z}; any other zone name is skipped and the
    result is in local time.
*/
QDateTime QLocale::toDateTime(const QString &string, const QString &format) const {
    QDate date;
    QTime time;
    int offset = 0;
    bool zone = false;
    if (!parseDateTime(*this, d, string, format, &date, &time, &offset, &zone))
        return QDateTime();
    if (!zone)
        return QDateTime(date, time);
    return offset == 0 ? QDateTime(date, time, Qt::UTC) : QDateTime(date, time, Qt::OffsetFromUTC, offset);
}

/*!
    Returns the date format of the locale for \a format, such as
    \c{"dddd, MMMM d, yyyy"} for the long format of en_US.

    \sa timeFormat(), toString()
*/
QString QLocale::dateFormat(FormatType format) const { return str(format == LongFormat ? d->longDate : d->shortDate); }

/*!
    Returns the time format of the locale for \a format.

    \sa dateFormat()
*/
QString QLocale::timeFormat(FormatType format) const { return str(format == LongFormat ? d->longTime : d->shortTime); }

/*!
    Returns the date and time format of the locale for \a format: the date
    format and the time format separated by a space.
*/
QString QLocale::dateTimeFormat(FormatType format) const { return dateFormat(format) + QChar(u' ') + timeFormat(format); }

/*!
    Returns the name of \a month (1 to 12) in the form \a format, as used in
    dates. In some languages, such as Russian or Polish, this is a different
    grammatical form from standaloneMonthName(). Returns an empty string for
    an invalid month.
*/
QString QLocale::monthName(int month, FormatType format) const {
    if (month < 1 || month > 12)
        return QString();
    if (format == LongFormat)
        return nth(d->months, month - 1);
    const QString shortName = nth(d->shortMonths, month - 1);
    if (format == ShortFormat)
        return shortName;
    return standaloneMonthName(month, NarrowFormat);
}

/*!
    Returns the name of \a month (1 to 12) in the form \a format, as used on
    its own, for example in a calendar heading.
*/
QString QLocale::standaloneMonthName(int month, FormatType format) const {
    if (month < 1 || month > 12)
        return QString();
    if (format == LongFormat)
        return nth(d->standaloneMonths ? d->standaloneMonths : d->months, month - 1);
    if (format == ShortFormat)
        return nth(d->standaloneShortMonths ? d->standaloneShortMonths : d->shortMonths, month - 1);
    const QString name = nth(d->standaloneMonths ? d->standaloneMonths : d->months, month - 1);
    if (d->language == Japanese || d->language == Korean || d->language == Chinese)
        return QString::number(month);
    return name.isEmpty() ? name : toUpper(name.left(1));
}

/*!
    Returns the name of \a day (1 is Monday, 7 is Sunday) in the form
    \a format. Returns an empty string for an invalid day.
*/
QString QLocale::dayName(int day, FormatType format) const {
    if (day < 1 || day > 7)
        return QString();
    if (format == LongFormat)
        return nth(d->days, day - 1);
    if (format == ShortFormat)
        return nth(d->shortDays, day - 1);
    return standaloneDayName(day, NarrowFormat);
}

/*!
    Returns the name of \a day (1 is Monday, 7 is Sunday) in the form
    \a format, as used on its own.
*/
QString QLocale::standaloneDayName(int day, FormatType format) const {
    if (day < 1 || day > 7)
        return QString();
    if (format != NarrowFormat)
        return dayName(day, format);
    const QString shortName = nth(d->shortDays, day - 1);
    return shortName.isEmpty() ? shortName : toUpper(shortName.left(1));
}

/*!
    Returns the text the locale uses for times before noon, such as
    \c{"AM"}.
*/
QString QLocale::amText() const { return str(d->am); }

/*!
    Returns the text the locale uses for times after noon, such as
    \c{"PM"}.
*/
QString QLocale::pmText() const { return str(d->pm); }

/*!
    \since 4.8
    Returns the first day of the week in the locale, such as Qt::Sunday in
    the United States and Qt::Monday in most of Europe.
*/
Qt::DayOfWeek QLocale::firstDayOfWeek() const { return d->firstDay; }

/*!
    \since 4.8
    Returns the working days of the week in the locale, Monday to Friday for
    all locales that Nt knows.
*/
QList<Qt::DayOfWeek> QLocale::weekdays() const { return {Qt::Monday, Qt::Tuesday, Qt::Wednesday, Qt::Thursday, Qt::Friday}; }

/*!
    Returns the measurement system of the locale.
*/
QLocale::MeasurementSystem QLocale::measurementSystem() const { return d->measurement; }

/*!
    \since 4.7
    Returns the direction in which the language of the locale is written.
    All locales with data in Nt are written left to right.
*/
Qt::LayoutDirection QLocale::textDirection() const {
    return d->script == ArabicScript || d->script == HebrewScript ? Qt::RightToLeft : Qt::LeftToRight;
}

/*!
    \since 4.8
    Returns \a str converted to uppercase, using the simple case mappings of
    Unicode.
*/
QString QLocale::toUpper(const QString &str) const {
    std::u16string out;
    out.reserve(static_cast<std::size_t>(str.size()));
    const char16_t *s = str.utf16();
    for (qsizetype i = 0; i < str.size();) {
        int len = 1;
        char32_t c = QtPrivate::Unicode::codePointAt(s, str.size(), i, &len);
        c = QtPrivate::Unicode::toUpper(c);
        if (c >= 0x10000) {
            out.push_back(static_cast<char16_t>(0xd800 + ((c - 0x10000) >> 10)));
            out.push_back(static_cast<char16_t>(0xdc00 + ((c - 0x10000) & 0x3ff)));
        } else {
            out.push_back(static_cast<char16_t>(c));
        }
        i += len;
    }
    return QString(out);
}

/*!
    \since 4.8
    Returns \a str converted to lowercase, using the simple case mappings of
    Unicode.
*/
QString QLocale::toLower(const QString &str) const {
    std::u16string out;
    out.reserve(static_cast<std::size_t>(str.size()));
    const char16_t *s = str.utf16();
    for (qsizetype i = 0; i < str.size();) {
        int len = 1;
        char32_t c = QtPrivate::Unicode::codePointAt(s, str.size(), i, &len);
        c = QtPrivate::Unicode::toLower(c);
        if (c >= 0x10000) {
            out.push_back(static_cast<char16_t>(0xd800 + ((c - 0x10000) >> 10)));
            out.push_back(static_cast<char16_t>(0xdc00 + ((c - 0x10000) & 0x3ff)));
        } else {
            out.push_back(static_cast<char16_t>(c));
        }
        i += len;
    }
    return QString(out);
}

/*!
    \since 4.8
    Returns the currency symbol of the locale in the form \a format. The C
    locale has no currency, so all forms are empty.
*/
QString QLocale::currencySymbol(CurrencySymbolFormat format) const {
    switch (format) {
    case CurrencyIsoCode:
        return str(d->currencyIso);
    case CurrencyDisplayName:
        return str(d->currencyName);
    case CurrencySymbol:
        break;
    }
    return str(d->currencySymbol);
}

namespace {
QString applyCurrency(const QLocaleData *d, const QString &amount, bool negative, const QString &symbol) {
    QString r = str(d->currencyFormat);
    r.replace(QString(u"%1"), amount);
    r.replace(QString(u"%2"), symbol);
    r = r.trimmed();
    if (symbol.isEmpty())
        r = amount;
    return negative ? str(d->minus) + r : r;
}
} // namespace

/*!
    \since 4.8
    Returns \a value as a currency amount in the locale, with \a symbol or,
    if it is empty, the currency symbol of the locale. Integers are written
    without decimals.

    \sa currencySymbol()
*/
QString QLocale::toCurrencyString(qlonglong value, const QString &symbol) const {
    const qulonglong magnitude = value < 0 ? qulonglong(0) - static_cast<qulonglong>(value) : static_cast<qulonglong>(value);
    return applyCurrency(d, toString(magnitude), value < 0, symbol.isEmpty() ? currencySymbol() : symbol);
}

/*!
    \overload
    Returns the unsigned \a value as a currency amount with \a symbol.
*/
QString QLocale::toCurrencyString(qulonglong value, const QString &symbol) const {
    return applyCurrency(d, toString(value), false, symbol.isEmpty() ? currencySymbol() : symbol);
}

/*!
    \overload
    \since 5.7
    Returns \a value as a currency amount with \a symbol and \a precision
    digits after the decimal point. A negative \a precision uses the number
    of digits of the currency, such as 2 for the euro and 0 for the yen.
*/
QString QLocale::toCurrencyString(double value, const QString &symbol, int precision) const {
    const int digits = precision < 0 ? d->currencyDigits : precision;
    QLocale plain = *this;
    plain.m_options &= ~NumberOptions(OmitGroupSeparator);
    if (d == cData())
        plain.m_options |= OmitGroupSeparator;
    const bool negative = std::signbit(value) && value != 0;
    return applyCurrency(d, plain.toString(std::fabs(value), 'f', digits), negative, symbol.isEmpty() ? currencySymbol() : symbol);
}

/*!
    \since 5.10
    Returns \a bytes as a size with a unit, such as \c{"1.50 MiB"}, with
    \a precision digits after the decimal point. \a format selects powers of
    1024 or 1000 and the IEC or SI prefixes. Sizes below the first unit are
    written in bytes. The unit names are not translated.
*/
QString QLocale::formattedDataSize(qint64 bytes, int precision, DataSizeFormats format) const {
    const bool base1000 = format.testFlag(DataSizeBase1000);
    const bool si = format.testFlag(DataSizeSIQuantifiers);
    const double base = base1000 ? 1000.0 : 1024.0;
    const double magnitude = std::fabs(static_cast<double>(bytes));
    int power = 0;
    double value = magnitude;
    while (value >= base && power < 6) {
        value /= base;
        ++power;
    }
    if (power == 0)
        return toString(bytes) + QString(u" bytes");
    static const char *const iec[] = {"KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
    static const char *const siUnits[] = {"kB", "MB", "GB", "TB", "PB", "EB"};
    const QString unit = QString::fromLatin1(si ? siUnits[power - 1] : iec[power - 1]);
    return toString(bytes < 0 ? -value : value, 'f', precision) + QChar(u' ') + unit;
}

/*!
    \since 4.8
    Returns \a str enclosed in the quotation marks of the locale, in the
    style \a style.
*/
QString QLocale::quoteString(QStringView str, QuotationStyle style) const {
    if (style == AlternateQuotation)
        return QString(d->altQuoteStart) + str.toString() + QString(d->altQuoteEnd);
    return QString(d->quoteStart) + str.toString() + QString(d->quoteEnd);
}

/*!
    \since 4.8
    Returns the strings of \a list joined as the locale writes a list, such
    as \c{"a, b, and c"} in American English.
*/
QString QLocale::createSeparatedList(const QStringList &list) const {
    const qsizetype n = list.size();
    if (n == 0)
        return QString();
    if (n == 1)
        return list.at(0);
    if (n == 2)
        return list.at(0) + str(d->listTwo) + list.at(1);
    QString r = list.at(0);
    for (qsizetype i = 1; i < n - 1; ++i)
        r += str(d->listMiddle) + list.at(i);
    return r + str(d->listEnd) + list.at(n - 1);
}

/*!
    \since 4.8
    Returns the languages to use for translations in this locale, most
    preferred first, with \a separator between the parts of each name: the
    full name with the territory, then the language alone.

    \sa bcp47Name(), QTranslator::load()
*/
QStringList QLocale::uiLanguages(TagSeparator separator) const {
    if (d == cData())
        return {QString(u"C")};
    const QString lang = QString::fromLatin1(languages[d->language].code);
    const QString full = lang + QChar(static_cast<char>(separator)) + QString::fromLatin1(territories[d->territory].code);
    return {full, lang};
}

/*!
    \since 5.13
    Returns the locale to use for collation, which is this locale.

    \sa QCollator
*/
QLocale QLocale::collation() const { return *this; }

/*!
    Sets the number options of this locale to \a options.

    \sa numberOptions()
*/
void QLocale::setNumberOptions(NumberOptions options) { m_options = options; }

/*!
    Returns the number options of this locale. They are OmitGroupSeparator
    for the C locale and DefaultNumberOptions for the others.

    \sa setNumberOptions()
*/
QLocale::NumberOptions QLocale::numberOptions() const { return m_options; }

/*!
    Returns the "C" locale, which formats numbers like QString::number()
    and uses English names.
*/
QLocale QLocale::c() { return QLocale(cData(), OmitGroupSeparator); }

/*!
    Returns the system locale, taken from the first of the \c LC_ALL,
    \c LC_NUMERIC and \c LANG environment variables that is set.

    \sa setDefault()
*/
QLocale QLocale::system() {
    for (const char *var : {"LC_ALL", "LC_NUMERIC", "LANG"}) {
        const char *value = std::getenv(var);
        if (value && *value)
            return QLocale(QString::fromLatin1(value));
    }
    return c();
}

/*!
    Sets the default locale, which default-constructed QLocale objects use,
    to \a locale. It does not change locales that already exist.

    \sa system()
*/
void QLocale::setDefault(const QLocale &locale) {
    std::lock_guard<std::mutex> lock(defaultMutex);
    defaultData = locale.d;
    defaultOptions = locale.m_options;
    defaultSet = true;
}

/*!
    \since 4.8
    Returns the locales with data that match \a language, \a script and
    \a territory. AnyLanguage, AnyScript and AnyTerritory match everything.
*/
QList<QLocale> QLocale::matchingLocales(Language language, Script script, Territory territory) {
    QList<QLocale> r;
    if (language == C)
        return {c()};
    for (const QLocaleData &data : localeTable) {
        if (&data == cData() && language != AnyLanguage)
            continue;
        if ((language == AnyLanguage || data.language == language) && (script == AnyScript || data.script == script) &&
            (territory == AnyTerritory || data.territory == territory))
            r.append(QLocale(&data, optionsFor(&data)));
    }
    return r;
}

/*!
    Returns the English name of \a language.
*/
QString QLocale::languageToString(Language language) {
    return language <= LastLanguage ? QString::fromUtf8(languages[language].name) : QString();
}

/*!
    \since 6.2
    Returns the English name of \a territory.
*/
QString QLocale::territoryToString(Territory territory) {
    return territory <= LastTerritory ? QString::fromUtf8(territories[territory].name) : QString();
}

/*!
    Returns the English name of \a script.
*/
QString QLocale::scriptToString(Script script) { return script <= LastScript ? QString::fromUtf8(scripts[script].name) : QString(); }

/*!
    \since 6.1
    Returns the ISO 639 code of \a language, such as \c{"de"}.

    \sa codeToLanguage()
*/
QString QLocale::languageToCode(Language language) { return language <= LastLanguage ? QString::fromLatin1(languages[language].code) : QString(); }

/*!
    \since 6.2
    Returns the ISO 3166 code of \a territory, such as \c{"DE"}.

    \sa codeToTerritory()
*/
QString QLocale::territoryToCode(Territory territory) {
    return territory <= LastTerritory ? QString::fromLatin1(territories[territory].code) : QString();
}

/*!
    \since 6.1
    Returns the ISO 15924 code of \a script, such as \c{"Latn"}.

    \sa codeToScript()
*/
QString QLocale::scriptToCode(Script script) { return script <= LastScript ? QString::fromLatin1(scripts[script].code) : QString(); }

/*!
    \since 6.1
    Returns the language with the ISO 639 code \a languageCode (case does not
    matter), or AnyLanguage if it is not known.
*/
QLocale::Language QLocale::codeToLanguage(QStringView languageCode) noexcept {
    const QString code = languageCode.toString().toLower();
    if (languageCode == u"C")
        return C;
    if (code == u"no" || code == u"nob")
        return NorwegianBokmal;
    for (int i = 2; i <= LastLanguage; ++i)
        if (code == QString::fromLatin1(languages[i].code))
            return static_cast<Language>(i);
    return AnyLanguage;
}

/*!
    \since 6.2
    Returns the territory with the ISO 3166 code \a territoryCode (case does
    not matter), or AnyTerritory if it is not known.
*/
QLocale::Territory QLocale::codeToTerritory(QStringView territoryCode) noexcept {
    const QString code = territoryCode.toString().toUpper();
    for (int i = 1; i <= LastTerritory; ++i)
        if (code == QString::fromLatin1(territories[i].code))
            return static_cast<Territory>(i);
    return AnyTerritory;
}

/*!
    \since 6.1
    Returns the script with the ISO 15924 code \a scriptCode (case does not
    matter), or AnyScript if it is not known.
*/
QLocale::Script QLocale::codeToScript(QStringView scriptCode) noexcept {
    const QString code = scriptCode.toString().toLower();
    for (int i = 1; i <= LastScript; ++i)
        if (code == QString::fromLatin1(scripts[i].code).toLower())
            return static_cast<Script>(i);
    return AnyScript;
}

std::size_t qHash(const QLocale &key, std::size_t seed) noexcept {
    return qHash(key.name(), seed) ^ static_cast<std::size_t>(key.numberOptions().toInt());
}
