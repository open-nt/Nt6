// MIT License - see LICENSE
#include <QLocale>
#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QTime>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);          \
            ++g_failures;                                                        \
        }                                                                        \
    } while (0)

#define CHECK_STR(actual, expected)                                                                     \
    do {                                                                                                \
        const QString a_ = (actual);                                                                    \
        const QString e_ = QString::fromUtf8(expected);                                                 \
        if (a_ != e_) {                                                                                 \
            std::printf("FAIL %s:%d  %s\n  got:      %s\n  expected: %s\n", __FILE__, __LINE__, #actual, \
                        a_.toUtf8().constData(), e_.toUtf8().constData());                              \
            ++g_failures;                                                                               \
        }                                                                                               \
    } while (0)


static void testIdentity() {
    CHECK(QLocale::AnyLanguage == 0 && QLocale::C == 1 && QLocale::English == 73 && QLocale::German == 92);
    CHECK(QLocale::French == 83 && QLocale::Chinese == 58 && QLocale::Japanese == 118);
    CHECK(QLocale::AnyTerritory == 0 && QLocale::UnitedStates == 247 && QLocale::Germany == 91);
    CHECK(QLocale::AnyScript == 0 && QLocale::LatinScript == 7 && QLocale::CyrillicScript == 2);
    CHECK(QLocale::MetricSystem == 0 && QLocale::ImperialUSSystem == 1 && QLocale::ImperialUKSystem == 2);
    CHECK(QLocale::FloatingPointShortest == -128);
    CHECK(QLocale::OmitGroupSeparator == 1 && QLocale::RejectGroupSeparator == 2 && QLocale::OmitLeadingZeroInExponent == 4);
    CHECK(QLocale::RejectLeadingZeroInExponent == 8 && QLocale::IncludeTrailingZeroesAfterDot == 0x10);
    CHECK(QLocale::RejectTrailingZeroesAfterDot == 0x20);
    CHECK(QLocale::LongFormat == 0 && QLocale::ShortFormat == 1 && QLocale::NarrowFormat == 2);

    const QLocale c = QLocale::c();
    CHECK(c.language() == QLocale::C);
    CHECK(c.territory() == QLocale::AnyTerritory);
    CHECK(c.country() == QLocale::AnyTerritory);
    CHECK_STR(c.name(), "C");
    CHECK_STR(c.bcp47Name(), "en");
    CHECK(c == QLocale(QLocale::C));
    CHECK(c == QLocale("C"));
    CHECK(c == QLocale("POSIX"));
    CHECK(c == QLocale("garbage"));
    CHECK(c == QLocale(""));
    CHECK(c == QLocale("xx_YY"));
    CHECK(c == QLocale(QLocale::Greek));
    CHECK(c == QLocale(QLocale::AnyLanguage));

    struct Row {
        const char *name;
        QLocale::Language lang;
        QLocale::Script script;
        QLocale::Territory terr;
        const char *canonical;
        const char *bcp47;
    };
    const Row rows[] = {
        {"en_US", QLocale::English, QLocale::LatinScript, QLocale::UnitedStates, "en_US", "en-US"},
        {"en-GB", QLocale::English, QLocale::LatinScript, QLocale::UnitedKingdom, "en_GB", "en-GB"},
        {"en_AU.UTF-8", QLocale::English, QLocale::LatinScript, QLocale::Australia, "en_AU", "en-AU"},
        {"de_DE@euro", QLocale::German, QLocale::LatinScript, QLocale::Germany, "de_DE", "de-DE"},
        {"fr_FR", QLocale::French, QLocale::LatinScript, QLocale::France, "fr_FR", "fr-FR"},
        {"es_ES", QLocale::Spanish, QLocale::LatinScript, QLocale::Spain, "es_ES", "es-ES"},
        {"it_IT", QLocale::Italian, QLocale::LatinScript, QLocale::Italy, "it_IT", "it-IT"},
        {"pt_BR", QLocale::Portuguese, QLocale::LatinScript, QLocale::Brazil, "pt_BR", "pt-BR"},
        {"nl_NL", QLocale::Dutch, QLocale::LatinScript, QLocale::Netherlands, "nl_NL", "nl-NL"},
        {"sv_SE", QLocale::Swedish, QLocale::LatinScript, QLocale::Sweden, "sv_SE", "sv-SE"},
        {"ru_RU", QLocale::Russian, QLocale::CyrillicScript, QLocale::Russia, "ru_RU", "ru-RU"},
        {"ja_JP", QLocale::Japanese, QLocale::JapaneseScript, QLocale::Japan, "ja_JP", "ja-JP"},
        {"zh_CN", QLocale::Chinese, QLocale::SimplifiedHanScript, QLocale::China, "zh_CN", "zh-CN"},
        {"zh_Hans_CN", QLocale::Chinese, QLocale::SimplifiedHanScript, QLocale::China, "zh_CN", "zh-CN"},
        {"ko_KR", QLocale::Korean, QLocale::KoreanScript, QLocale::SouthKorea, "ko_KR", "ko-KR"},
        {"pl_PL", QLocale::Polish, QLocale::LatinScript, QLocale::Poland, "pl_PL", "pl-PL"},
        {"tr_TR", QLocale::Turkish, QLocale::LatinScript, QLocale::Turkey, "tr_TR", "tr-TR"},
        {"DE_de", QLocale::German, QLocale::LatinScript, QLocale::Germany, "de_DE", "de-DE"},
        {"de", QLocale::German, QLocale::LatinScript, QLocale::Germany, "de_DE", "de-DE"},
        {"en", QLocale::English, QLocale::LatinScript, QLocale::UnitedStates, "en_US", "en-US"},
        {"pt", QLocale::Portuguese, QLocale::LatinScript, QLocale::Brazil, "pt_BR", "pt-BR"},
        {"ja", QLocale::Japanese, QLocale::JapaneseScript, QLocale::Japan, "ja_JP", "ja-JP"},
        {"de_AT", QLocale::German, QLocale::LatinScript, QLocale::Austria, "de_AT", "de-AT"},
        {"en_CA", QLocale::English, QLocale::LatinScript, QLocale::Canada, "en_CA", "en-CA"},
        {"fr_ZZ", QLocale::French, QLocale::LatinScript, QLocale::France, "fr_FR", "fr-FR"},
    };
    for (const Row &r : rows) {
        const QLocale l(QString::fromUtf8(r.name));
        CHECK(l.language() == r.lang);
        CHECK(l.script() == r.script);
        CHECK(l.territory() == r.terr);
        CHECK_STR(l.name(), r.canonical);
        CHECK_STR(l.bcp47Name(), r.bcp47);
        CHECK(l == QLocale(l.language(), l.territory()));
        CHECK(l.numberOptions() == QLocale::NumberOptions());
    }
    CHECK_STR(QLocale("de_DE").name(QLocale::TagSeparator::Dash), "de-DE");
    CHECK_STR(QLocale("de_DE").bcp47Name(QLocale::TagSeparator::Underscore), "de_DE");
    CHECK(QLocale("de_DE") != QLocale("fr_FR"));
    CHECK(QLocale("de_DE") != QLocale("de_AT"));
    QLocale opt("de_DE");
    opt.setNumberOptions(QLocale::OmitGroupSeparator);
    CHECK(opt != QLocale("de_DE"));
    CHECK(opt.numberOptions() == QLocale::OmitGroupSeparator);
    CHECK(qHash(QLocale("de_DE")) == qHash(QLocale("de_DE")));
    CHECK(c.numberOptions().testFlag(QLocale::OmitGroupSeparator));

    CHECK_STR(QLocale::languageToString(QLocale::English), "English");
    CHECK_STR(QLocale::languageToString(QLocale::German), "German");
    CHECK_STR(QLocale::languageToString(QLocale::C), "C");
    CHECK_STR(QLocale::languageToString(QLocale::AnyLanguage), "Default");
    CHECK_STR(QLocale::languageToString(static_cast<QLocale::Language>(400)), "Unknown");
    CHECK_STR(QLocale::territoryToString(QLocale::UnitedStates), "United States");
    CHECK_STR(QLocale::countryToString(QLocale::Germany), "Germany");
    CHECK_STR(QLocale::territoryToString(QLocale::AnyTerritory), "Default");
    CHECK_STR(QLocale::scriptToString(QLocale::LatinScript), "Latin");
    CHECK_STR(QLocale::scriptToString(QLocale::CyrillicScript), "Cyrillic");
    CHECK_STR(QLocale::languageToCode(QLocale::English), "en");
    CHECK_STR(QLocale::languageToCode(QLocale::English, QLocale::ISO639Alpha3), "eng");
    CHECK_STR(QLocale::languageToCode(QLocale::Filipino), "fil");
    CHECK_STR(QLocale::languageToCode(QLocale::Filipino, QLocale::ISO639Part1), "");
    CHECK_STR(QLocale::languageToCode(QLocale::AnyLanguage), "");
    CHECK_STR(QLocale::territoryToCode(QLocale::Germany), "DE");
    CHECK_STR(QLocale::scriptToCode(QLocale::LatinScript), "Latn");
    CHECK(QLocale::codeToLanguage(u"de") == QLocale::German);
    CHECK(QLocale::codeToLanguage(u"DEU") == QLocale::German);
    CHECK(QLocale::codeToLanguage(u"deu", QLocale::ISO639Part1) == QLocale::AnyLanguage);
    CHECK(QLocale::codeToLanguage(u"no") == QLocale::NorwegianBokmal);
    CHECK(QLocale::codeToLanguage(u"qq") == QLocale::AnyLanguage);
    CHECK(QLocale::codeToTerritory(u"us") == QLocale::UnitedStates);
    CHECK(QLocale::codeToTerritory(u"001") == QLocale::World);
    CHECK(QLocale::codeToTerritory(u"QQ") == QLocale::AnyTerritory);
    CHECK(QLocale::codeToScript(u"cyrl") == QLocale::CyrillicScript);
    CHECK(QLocale::codeToScript(u"zzzz") == QLocale::AnyScript);

    CHECK_STR(QLocale("de_DE").nativeLanguageName(), "Deutsch");
    CHECK_STR(QLocale("de_DE").nativeTerritoryName(), "Deutschland");
    CHECK_STR(QLocale("de_DE").nativeCountryName(), "Deutschland");
    CHECK_STR(QLocale("ja_JP").nativeLanguageName(), "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e");

    // Default and system locales.
    const QLocale saved;
    QLocale::setDefault(QLocale("de_DE"));
    CHECK(QLocale() == QLocale("de_DE"));
    CHECK(QLocale().toString(1.5) == QString("1,5"));
    QLocale::setDefault(QLocale("fr_FR"));
    CHECK(QLocale() == QLocale("fr_FR"));
    QLocale::setDefault(saved);
    CHECK(QLocale() == saved);
    const QLocale sys = QLocale::system();
    CHECK(!sys.name().isEmpty());
    CHECK(sys.language() != QLocale::AnyLanguage);
    CHECK(!sys.uiLanguages().isEmpty());
    CHECK(sys == QLocale::system());

    QLocale a("de_DE"), b("fr_FR");
    a.swap(b);
    CHECK(a == QLocale("fr_FR") && b == QLocale("de_DE"));

    const QList<QLocale> all = QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyTerritory);
    CHECK(all.size() == 17);
    CHECK(all.front() == QLocale::c());
    const QList<QLocale> eng = QLocale::matchingLocales(QLocale::English, QLocale::AnyScript, QLocale::AnyTerritory);
    CHECK(eng.size() == 3);
    CHECK(QLocale::matchingLocales(QLocale::German, QLocale::AnyScript, QLocale::Germany).size() == 1);
    CHECK(QLocale::matchingLocales(QLocale::Greek, QLocale::AnyScript, QLocale::AnyTerritory).isEmpty());

    CHECK(QLocale("de_DE").uiLanguages() == QStringList({"de-DE", "de"}));
    CHECK(QLocale::c().uiLanguages() == QStringList({"en"}));
    CHECK(QLocale("de_DE").uiLanguages(QLocale::TagSeparator::Underscore) == QStringList({"de_DE", "de"}));
    CHECK(QLocale("de_DE").collation() == QLocale("de_DE"));
    CHECK(QLocale("de_DE").textDirection() == Qt::LeftToRight);
}

struct IntCase {
    const char *locale;
    qlonglong value;
    const char *expected;
};

static void testIntegers() {
    const IntCase cases[] = {
        {"C", 0, "0"},
        {"C", 1234567, "1234567"},
        {"C", -1234567, "-1234567"},
        {"en_US", 0, "0"},
        {"en_US", 999, "999"},
        {"en_US", 1000, "1,000"},
        {"en_US", 1234567, "1,234,567"},
        {"en_US", -1234567, "-1,234,567"},
        {"en_GB", 1234567, "1,234,567"},
        {"en_AU", 1234567, "1,234,567"},
        {"de_DE", 1234567, "1.234.567"},
        {"de_DE", -1000, "-1.000"},
        {"fr_FR", 1234567, "1\xe2\x80\xaf" "234\xe2\x80\xaf" "567"},
        {"es_ES", 1234567, "1.234.567"},
        {"it_IT", 1234567, "1.234.567"},
        {"pt_BR", 1234567, "1.234.567"},
        {"nl_NL", 1234567, "1.234.567"},
        {"sv_SE", 1234567, "1\xc2\xa0" "234\xc2\xa0" "567"},
        {"sv_SE", -5, "\xe2\x88\x92" "5"},
        {"ru_RU", 1234567, "1\xc2\xa0" "234\xc2\xa0" "567"},
        {"ja_JP", 1234567, "1,234,567"},
        {"zh_CN", 1234567, "1,234,567"},
        {"ko_KR", 1234567, "1,234,567"},
        {"pl_PL", 1234567, "1\xc2\xa0" "234\xc2\xa0" "567"},
        {"tr_TR", 1234567, "1.234.567"},
        {"en_US", std::numeric_limits<qlonglong>::max(), "9,223,372,036,854,775,807"},
        {"en_US", std::numeric_limits<qlonglong>::min(), "-9,223,372,036,854,775,808"},
    };
    for (const IntCase &c : cases)
        CHECK_STR(QLocale(QString::fromUtf8(c.locale)).toString(c.value), c.expected);

    const QLocale en("en_US");
    CHECK_STR(en.toString(static_cast<qulonglong>(18446744073709551615ULL)), "18,446,744,073,709,551,615");
    CHECK_STR(en.toString(12345), "12,345");
    CHECK_STR(en.toString(12345u), "12,345");
    CHECK_STR(en.toString(static_cast<short>(-1234)), "-1,234");
    CHECK_STR(en.toString(static_cast<unsigned short>(65535)), "65,535");
    CHECK_STR(en.toString(1234567L), "1,234,567");
    CHECK_STR(en.toString(1234567UL), "1,234,567");
    QLocale omit("en_US");
    omit.setNumberOptions(QLocale::OmitGroupSeparator);
    CHECK_STR(omit.toString(1234567), "1234567");
    CHECK_STR(omit.toString(1234567.5, 'f', 1), "1234567.5");
}

struct DoubleCase {
    const char *locale;
    double value;
    char format;
    int precision;
    const char *expected;
};

static void testDoubles() {
    const DoubleCase cases[] = {
        {"C", 1234.5, 'f', 2, "1234.50"},
        {"C", 0.0, 'f', 2, "0.00"},
        {"C", -0.5, 'f', 0, "-0"},
        {"C", 1234567.891, 'f', 1, "1234567.9"},
        {"en_US", 1234567.891, 'f', 2, "1,234,567.89"},
        {"en_US", -1234567.891, 'f', 2, "-1,234,567.89"},
        {"en_US", 0.000123, 'f', 6, "0.000123"},
        {"en_US", 1234.5, 'e', 3, "1.234e+03"},
        {"en_US", 1234.5, 'E', 3, "1.234E+03"},
        {"en_US", 1234.5, 'e', 0, "1e+03"},
        {"en_US", 1234.5, 'g', 6, "1,234.5"},
        {"en_US", 1234567.0, 'g', 6, "1.23457e+06"},
        {"en_US", 1234567.0, 'G', 6, "1.23457E+06"},
        {"en_US", 0.0001234, 'g', 6, "0.0001234"},
        {"en_US", 0.00001234, 'g', 6, "1.234e-05"},
        {"en_US", 100.0, 'g', 6, "100"},
        {"en_US", 1e100, 'e', 2, "1.00e+100"},
        {"en_US", 3.14159265358979, 'f', 4, "3.1416"},
        {"en_US", 2.5, 'f', 0, "2"},
        {"en_US", 0.1, 'g', QLocale::FloatingPointShortest, "0.1"},
        {"en_US", 1.0 / 3.0, 'g', QLocale::FloatingPointShortest, "0.3333333333333333"},
        {"en_US", 1234567.0, 'f', QLocale::FloatingPointShortest, "1,234,567"},
        {"en_US", 1234.5, 'f', -1, "1,234.500000"},
        {"en_US", 1234.5, 'x', 2, "1.2e+03"},
        {"de_DE", 1234567.891, 'f', 2, "1.234.567,89"},
        {"de_DE", -0.5, 'f', 1, "-0,5"},
        {"de_DE", 1234.5, 'e', 3, "1,234e+03"},
        {"fr_FR", 1234567.891, 'f', 2, "1\xe2\x80\xaf" "234\xe2\x80\xaf" "567,89"},
        {"es_ES", 1234.5, 'f', 2, "1.234,50"},
        {"it_IT", 1234.5, 'f', 2, "1.234,50"},
        {"pt_BR", 1234.5, 'f', 2, "1.234,50"},
        {"nl_NL", 1234.5, 'f', 2, "1.234,50"},
        {"sv_SE", -1234.5, 'f', 2, "\xe2\x88\x92" "1\xc2\xa0" "234,50"},
        {"ru_RU", 1234.5, 'f', 2, "1\xc2\xa0" "234,50"},
        {"ja_JP", 1234.5, 'f', 2, "1,234.50"},
        {"zh_CN", 1234.5, 'f', 2, "1,234.50"},
        {"ko_KR", 1234.5, 'f', 2, "1,234.50"},
        {"pl_PL", 1234.5, 'f', 2, "1\xc2\xa0" "234,50"},
        {"tr_TR", 1234.5, 'f', 2, "1.234,50"},
        {"en_US", HUGE_VAL, 'f', 2, "inf"},
        {"en_US", -HUGE_VAL, 'f', 2, "-inf"},
        {"en_US", HUGE_VAL, 'E', 2, "INF"},
        {"de_DE", std::nan(""), 'g', 6, "nan"},
        {"sv_SE", -HUGE_VAL, 'g', 6, "\xe2\x88\x92" "inf"},
    };
    for (const DoubleCase &c : cases)
        CHECK_STR(QLocale(QString::fromUtf8(c.locale)).toString(c.value, c.format, c.precision), c.expected);

    CHECK_STR(QLocale("en_US").toString(1234.5f, 'f', 1), "1,234.5");
    CHECK_STR(QLocale("en_US").toString(1234.5), "1,234.5");

    // Options.
    QLocale o("en_US");
    o.setNumberOptions(QLocale::OmitLeadingZeroInExponent);
    CHECK_STR(o.toString(1234.5, 'e', 3), "1.234e+3");
    CHECK_STR(o.toString(1.5e100, 'e', 1), "1.5e+100");
    CHECK_STR(o.toString(1.5e-5, 'e', 1), "1.5e-5");
    o.setNumberOptions(QLocale::IncludeTrailingZeroesAfterDot);
    CHECK_STR(o.toString(1.5, 'g', 6), "1.50000");
    CHECK_STR(o.toString(100.0, 'g', 6), "100.000");
    CHECK_STR(o.toString(0.5, 'g', 3), "0.500");
    CHECK_STR(o.toString(1e10, 'g', 3), "1.00e+10");
    CHECK_STR(o.toString(1.5, 'f', 3), "1.500");
    o.setNumberOptions(QLocale::OmitGroupSeparator | QLocale::OmitLeadingZeroInExponent);
    CHECK_STR(o.toString(1234567.5, 'f', 1), "1234567.5");
    CHECK_STR(o.toString(1.5e5, 'e', 1), "1.5e+5");
}

struct ParseCase {
    const char *locale;
    const char *text;
    bool ok;
    double value;
};

static void testParsing() {
    const ParseCase cases[] = {
        {"C", "1234.5", true, 1234.5},
        {"C", "1,234.5", true, 1234.5},
        {"C", "  12  ", true, 12},
        {"C", "-0.5", true, -0.5},
        {"C", "+0.5", true, 0.5},
        {"C", ".5", true, 0.5},
        {"C", "5.", true, 5},
        {"C", "1e3", true, 1000},
        {"C", "1E3", true, 1000},
        {"C", "1.5e-3", true, 0.0015},
        {"C", "1.5e+03", true, 1500},
        {"C", "", false, 0},
        {"C", " ", false, 0},
        {"C", ".", false, 0},
        {"C", "-", false, 0},
        {"C", "e5", false, 0},
        {"C", "1e", false, 0},
        {"C", "1e+", false, 0},
        {"C", "1,23", false, 0},
        {"C", "12,3456", false, 0},
        {"C", ",123", false, 0},
        {"C", "123,", false, 0},
        {"C", "1,,234", false, 0},
        {"C", "1,234,56", false, 0},
        {"C", "1.2.3", false, 0},
        {"C", "1,234.5,6", false, 0},
        {"C", "abc", false, 0},
        {"C", "12abc", false, 0},
        {"C", "1 2", false, 0},
        {"C", "1e5e5", false, 0},
        {"C", "1e400", false, 0},
        {"C", "--1", false, 0},
        {"en_US", "1,234,567.89", true, 1234567.89},
        {"en_US", "1234567.89", true, 1234567.89},
        {"en_US", "12,34", false, 0},
        {"de_DE", "1.234.567,89", true, 1234567.89},
        {"de_DE", "1234567,89", true, 1234567.89},
        {"de_DE", "1,5", true, 1.5},
        {"de_DE", "1.5", false, 0},
        {"de_DE", "1.23", false, 0},
        {"de_DE", "-1.234,5", true, -1234.5},
        {"de_DE", "1,5e3", true, 1500},
        {"fr_FR", "1\xe2\x80\xaf" "234,5", true, 1234.5},
        {"fr_FR", "1\xc2\xa0" "234,5", true, 1234.5},
        {"fr_FR", "1 234,5", true, 1234.5},
        {"ru_RU", "1\xc2\xa0" "234,5", true, 1234.5},
        {"ru_RU", "1 234,5", true, 1234.5},
        {"sv_SE", "\xe2\x88\x92" "1\xc2\xa0" "234,5", true, -1234.5},
        {"sv_SE", "-1234,5", true, -1234.5},
        {"ja_JP", "1,234.5", true, 1234.5},
        {"pl_PL", "1\xc2\xa0" "234,5", true, 1234.5},
        {"tr_TR", "1.234,5", true, 1234.5},
    };
    for (const ParseCase &c : cases) {
        bool ok = !c.ok;
        const QLocale l(QString::fromUtf8(c.locale));
        const double v = l.toDouble(QString::fromUtf8(c.text), &ok);
        if (ok != c.ok || (ok && std::fabs(v - c.value) > 1e-9 * std::fabs(c.value) + 1e-12) || (!ok && v != 0.0)) {
            std::printf("FAIL parse %s \"%s\": ok=%d v=%g (expected ok=%d v=%g)\n", c.locale, c.text, ok, v, c.ok, c.value);
            ++g_failures;
        }
    }

    const QLocale c = QLocale::c();
    bool ok = false;
    CHECK(std::isinf(c.toDouble(u"inf", &ok)) && ok);
    CHECK(std::isinf(c.toDouble(u"-INF", &ok)) && ok && c.toDouble(u"-inf") < 0);
    CHECK(std::isnan(c.toDouble(u"nan", &ok)) && ok);
    CHECK(std::isnan(c.toDouble(u"NaN", &ok)) && ok);
    CHECK(c.toDouble(u"infx", &ok) == 0 && !ok);
    CHECK(c.toFloat(u"1.5", &ok) == 1.5f && ok);
    CHECK(c.toFloat(u"1e300", &ok) == 0.0f && !ok);
    CHECK(c.toFloat(u"-1e300", &ok) == 0.0f && !ok);
    CHECK(c.toDouble("2.5", &ok) == 2.5 && ok);
    CHECK(c.toDouble(QString("2.5"), &ok) == 2.5 && ok);
    CHECK(c.toDouble(u"7") == 7.0);

    // Integers.
    const QLocale de("de_DE");
    CHECK(de.toInt(u"1.234", &ok) == 1234 && ok);
    CHECK(de.toInt(u"-1.234.567", &ok) == -1234567 && ok);
    CHECK(de.toInt(u"1,5", &ok) == 0 && !ok);
    CHECK(de.toInt(u"1e3", &ok) == 0 && !ok);
    CHECK(de.toInt(u"12.34", &ok) == 0 && !ok);
    CHECK(de.toInt(u"", &ok) == 0 && !ok);
    CHECK(de.toInt(u"2147483647", &ok) == 2147483647 && ok);
    CHECK(de.toInt(u"2147483648", &ok) == 0 && !ok);
    CHECK(de.toInt(u"-2147483648", &ok) == std::numeric_limits<int>::min() && ok);
    CHECK(de.toInt(u"-2147483649", &ok) == 0 && !ok);
    CHECK(de.toUInt(u"4294967295", &ok) == 4294967295u && ok);
    CHECK(de.toUInt(u"4294967296", &ok) == 0 && !ok);
    CHECK(de.toUInt(u"-1", &ok) == 0 && !ok);
    CHECK(de.toShort(u"32767", &ok) == 32767 && ok);
    CHECK(de.toShort(u"32768", &ok) == 0 && !ok);
    CHECK(de.toShort(u"-32768", &ok) == -32768 && ok);
    CHECK(de.toUShort(u"65535", &ok) == 65535 && ok);
    CHECK(de.toUShort(u"65536", &ok) == 0 && !ok);
    CHECK(de.toLongLong(u"9.223.372.036.854.775.807", &ok) == std::numeric_limits<qlonglong>::max() && ok);
    CHECK(de.toLongLong(u"9.223.372.036.854.775.808", &ok) == 0 && !ok);
    CHECK(de.toULongLong(u"18.446.744.073.709.551.615", &ok) == 18446744073709551615ULL && ok);
    CHECK(de.toULongLong(u"18.446.744.073.709.551.616", &ok) == 0 && !ok);
    CHECK(de.toULongLong(u"-5", &ok) == 0 && !ok);
    CHECK(de.toLong(u"1.000", &ok) == 1000 && ok);
    CHECK(de.toULong(u"1.000", &ok) == 1000 && ok);
    CHECK(de.toInt("1.000", &ok) == 1000 && ok);
    CHECK(de.toInt(QString("12"), &ok) == 12 && ok);
    CHECK(de.toInt(u"  42\t") == 42);
    CHECK(de.toInt(u"+42") == 42);
    CHECK(de.toShort("5") == 5 && de.toUShort("5") == 5 && de.toUInt("5") == 5u && de.toLong("5") == 5 && de.toULong("5") == 5);
    CHECK(de.toLongLong("5") == 5 && de.toULongLong("5") == 5 && de.toFloat("5,5") == 5.5f);

    // Options.
    QLocale r("en_US");
    r.setNumberOptions(QLocale::RejectGroupSeparator);
    CHECK(r.toInt(u"1,234", &ok) == 0 && !ok);
    CHECK(r.toInt(u"1234", &ok) == 1234 && ok);
    CHECK(r.toDouble(u"1,234.5", &ok) == 0 && !ok);
    r.setNumberOptions(QLocale::OmitGroupSeparator);
    CHECK(r.toInt(u"1,234", &ok) == 1234 && ok);
    r.setNumberOptions(QLocale::RejectLeadingZeroInExponent);
    CHECK(r.toDouble(u"1e05", &ok) == 0 && !ok);
    CHECK(r.toDouble(u"1e5", &ok) == 1e5 && ok);
    CHECK(r.toDouble(u"1e0", &ok) == 1 && ok);
    CHECK(r.toDouble(u"1e00", &ok) == 0 && !ok);
    r.setNumberOptions(QLocale::RejectTrailingZeroesAfterDot);
    CHECK(r.toDouble(u"1.50", &ok) == 0 && !ok);
    CHECK(r.toDouble(u"1.0", &ok) == 0 && !ok);
    CHECK(r.toDouble(u"1.5", &ok) == 1.5 && ok);
    CHECK(r.toDouble(u"15", &ok) == 15 && ok);
    CHECK(r.toDouble(u"10", &ok) == 10 && ok);
    r.setNumberOptions(QLocale::RejectGroupSeparator | QLocale::RejectTrailingZeroesAfterDot | QLocale::RejectLeadingZeroInExponent);
    CHECK(r.toDouble(u"1.5e5", &ok) == 1.5e5 && ok);

    // Round trips.
    for (const char *name : {"C", "en_US", "de_DE", "fr_FR", "ru_RU", "sv_SE", "pl_PL", "tr_TR", "ja_JP"}) {
        const QLocale l{QString::fromUtf8(name)};
        for (double v : {0.0, 1.0, -1.0, 0.5, 1234.5, -9876543.21, 1e-7, 123456789.0, 1.5e300, 3.141592653589793}) {
            const QString text = l.toString(v, 'g', QLocale::FloatingPointShortest);
            const double back = l.toDouble(text, &ok);
            if (!ok || back != v) {
                std::printf("FAIL roundtrip %s %.17g -> \"%s\" -> %.17g ok=%d\n", name, v, text.toUtf8().constData(), back, ok);
                ++g_failures;
            }
        }
        for (qlonglong v : {0LL, 1LL, -1LL, 999LL, 1000LL, -123456789012LL}) {
            const QString text = l.toString(v);
            CHECK(l.toLongLong(text, &ok) == v && ok);
        }
    }
}

static void testSymbols() {
    CHECK_STR(QLocale("en_US").decimalPoint(), ".");
    CHECK_STR(QLocale("en_US").groupSeparator(), ",");
    CHECK_STR(QLocale("de_DE").decimalPoint(), ",");
    CHECK_STR(QLocale("de_DE").groupSeparator(), ".");
    CHECK_STR(QLocale("en_US").percent(), "%");
    CHECK_STR(QLocale("en_US").zeroDigit(), "0");
    CHECK_STR(QLocale("en_US").negativeSign(), "-");
    CHECK_STR(QLocale("en_US").positiveSign(), "+");
    CHECK_STR(QLocale("en_US").exponential(), "e");
    CHECK_STR(QLocale("sv_SE").negativeSign(), "\xe2\x88\x92");
    CHECK_STR(QLocale::c().decimalPoint(), ".");
    CHECK_STR(QLocale::c().groupSeparator(), ",");

    CHECK(QLocale("en_US").measurementSystem() == QLocale::ImperialUSSystem);
    CHECK(QLocale("en_GB").measurementSystem() == QLocale::ImperialUKSystem);
    CHECK(QLocale("en_AU").measurementSystem() == QLocale::MetricSystem);
    CHECK(QLocale("de_DE").measurementSystem() == QLocale::MetricSystem);
    CHECK(QLocale::c().measurementSystem() == QLocale::MetricSystem);

    CHECK(QLocale("en_US").firstDayOfWeek() == Qt::Sunday);
    CHECK(QLocale("en_GB").firstDayOfWeek() == Qt::Monday);
    CHECK(QLocale("de_DE").firstDayOfWeek() == Qt::Monday);
    CHECK(QLocale("pt_BR").firstDayOfWeek() == Qt::Sunday);
    CHECK(QLocale("ja_JP").firstDayOfWeek() == Qt::Sunday);
    CHECK(QLocale::c().firstDayOfWeek() == Qt::Monday);
    const QList<Qt::DayOfWeek> wd = QLocale("en_US").weekdays();
    CHECK(wd.size() == 5 && wd.front() == Qt::Monday && wd.back() == Qt::Friday);

    CHECK(QLocale("en_US").amText() == QString("AM") && QLocale("en_US").pmText() == QString("PM"));
    CHECK(QLocale("en_AU").amText() == QString("am"));
    CHECK_STR(QLocale("ja_JP").amText(), "\xe5\x8d\x88\xe5\x89\x8d");
    CHECK_STR(QLocale("ko_KR").pmText(), "\xec\x98\xa4\xed\x9b\x84");

    // Upper and lower case.
    CHECK_STR(QLocale("en_US").toUpper("hello i"), "HELLO I");
    CHECK_STR(QLocale("en_US").toLower("HELLO I"), "hello i");
    CHECK_STR(QLocale("tr_TR").toUpper("iI"), "\xc4\xb0I");
    CHECK_STR(QLocale("tr_TR").toLower("I\xc4\xb0"), "\xc4\xb1i");
    CHECK_STR(QLocale("de_DE").toUpper("abc"), "ABC");

    // Lists.
    const QStringList none, one{"a"}, two{"a", "b"}, three{"a", "b", "c"}, four{"a", "b", "c", "d"};
    CHECK_STR(QLocale("en_US").createSeparatedList(none), "");
    CHECK_STR(QLocale("en_US").createSeparatedList(one), "a");
    CHECK_STR(QLocale("en_US").createSeparatedList(two), "a and b");
    CHECK_STR(QLocale("en_US").createSeparatedList(three), "a, b, and c");
    CHECK_STR(QLocale("en_US").createSeparatedList(four), "a, b, c, and d");
    CHECK_STR(QLocale("en_GB").createSeparatedList(three), "a, b and c");
    CHECK_STR(QLocale("de_DE").createSeparatedList(three), "a, b und c");
    CHECK_STR(QLocale("fr_FR").createSeparatedList(three), "a, b et c");
    CHECK_STR(QLocale("es_ES").createSeparatedList(two), "a y b");
    CHECK_STR(QLocale("ja_JP").createSeparatedList(three), "a\xe3\x80\x81" "b\xe3\x80\x81" "c");
    CHECK_STR(QLocale("zh_CN").createSeparatedList(three), "a\xe3\x80\x81" "b\xe5\x92\x8c" "c");

    // Quotes.
    CHECK_STR(QLocale("en_US").quoteString(u"x"), "\xe2\x80\x9c" "x\xe2\x80\x9d");
    CHECK_STR(QLocale("en_US").quoteString(u"x", QLocale::AlternateQuotation), "\xe2\x80\x98" "x\xe2\x80\x99");
    CHECK_STR(QLocale("en_GB").quoteString("x"), "\xe2\x80\x98" "x\xe2\x80\x99");
    CHECK_STR(QLocale("de_DE").quoteString(QString("x")), "\xe2\x80\x9e" "x\xe2\x80\x9c");
    CHECK_STR(QLocale("fr_FR").quoteString(u"x"), "\xc2\xab" "x\xc2\xbb");
    CHECK_STR(QLocale("ja_JP").quoteString(u"x"), "\xe3\x80\x8c" "x\xe3\x80\x8d");

    // Data sizes.
    const QLocale en("en_US");
    CHECK_STR(en.formattedDataSize(0), "0 bytes");
    CHECK_STR(en.formattedDataSize(1), "1 bytes");
    CHECK_STR(en.formattedDataSize(1023), "1,023 bytes");
    CHECK_STR(en.formattedDataSize(1024), "1.00 KiB");
    CHECK_STR(en.formattedDataSize(1536), "1.50 KiB");
    CHECK_STR(en.formattedDataSize(1536, 1), "1.5 KiB");
    CHECK_STR(en.formattedDataSize(1536, 0), "2 KiB");
    CHECK_STR(en.formattedDataSize(1024 * 1024), "1.00 MiB");
    CHECK_STR(en.formattedDataSize(5LL * 1024 * 1024 * 1024), "5.00 GiB");
    CHECK_STR(en.formattedDataSize(1000, 2, QLocale::DataSizeSIFormat), "1.00 kB");
    CHECK_STR(en.formattedDataSize(999, 2, QLocale::DataSizeSIFormat), "999 bytes");
    CHECK_STR(en.formattedDataSize(1500000, 2, QLocale::DataSizeSIFormat), "1.50 MB");
    CHECK_STR(en.formattedDataSize(2048, 2, QLocale::DataSizeTraditionalFormat), "2.00 kB");
    CHECK_STR(en.formattedDataSize(-2048), "-2.00 KiB");
    CHECK_STR(en.formattedDataSize(1LL << 60), "1.00 EiB");
    CHECK_STR(en.formattedDataSize(std::numeric_limits<qint64>::max()), "8.00 EiB");
    CHECK_STR(QLocale("de_DE").formattedDataSize(1536), "1,50 KiB");
    CHECK_STR(QLocale("de_DE").formattedDataSize(1536, 5), "1,500 KiB");
}

static void testCurrency() {
    CHECK_STR(QLocale("en_US").currencySymbol(), "$");
    CHECK_STR(QLocale("en_US").currencySymbol(QLocale::CurrencyIsoCode), "USD");
    CHECK_STR(QLocale("en_US").currencySymbol(QLocale::CurrencyDisplayName), "US Dollar");
    CHECK_STR(QLocale("en_GB").currencySymbol(), "\xc2\xa3");
    CHECK_STR(QLocale("de_DE").currencySymbol(QLocale::CurrencyIsoCode), "EUR");
    CHECK_STR(QLocale::c().currencySymbol(), "");

    CHECK_STR(QLocale("en_US").toCurrencyString(1234.5), "$1,234.50");
    CHECK_STR(QLocale("en_US").toCurrencyString(-1234.5), "-$1,234.50");
    CHECK_STR(QLocale("en_US").toCurrencyString(1234), "$1,234.00");
    CHECK_STR(QLocale("en_US").toCurrencyString(1234u), "$1,234.00");
    CHECK_STR(QLocale("en_US").toCurrencyString(static_cast<qulonglong>(5)), "$5.00");
    CHECK_STR(QLocale("en_US").toCurrencyString(1234.5, "USD"), "USD1,234.50");
    CHECK_STR(QLocale("en_US").toCurrencyString(1234.567, QString(), 1), "$1,234.6");
    CHECK_STR(QLocale("en_GB").toCurrencyString(5.0), "\xc2\xa3" "5.00");
    CHECK_STR(QLocale("de_DE").toCurrencyString(1234.5), "1.234,50\xc2\xa0\xe2\x82\xac");
    CHECK_STR(QLocale("de_DE").toCurrencyString(-1234.5), "-1.234,50\xc2\xa0\xe2\x82\xac");
    CHECK_STR(QLocale("fr_FR").toCurrencyString(1234.5), "1\xe2\x80\xaf" "234,50\xc2\xa0\xe2\x82\xac");
    CHECK_STR(QLocale("pt_BR").toCurrencyString(1234.5), "R$\xc2\xa0" "1.234,50");
    CHECK_STR(QLocale("nl_NL").toCurrencyString(1234.5), "\xe2\x82\xac\xc2\xa0" "1.234,50");
    CHECK_STR(QLocale("ja_JP").toCurrencyString(1234.5), "\xef\xbf\xa5" "1,234");
    CHECK_STR(QLocale("ko_KR").toCurrencyString(1234), "\xe2\x82\xa9" "1,234");
    CHECK_STR(QLocale("tr_TR").toCurrencyString(1234.5), "\xe2\x82\xba" "1.234,50");
    CHECK_STR(QLocale::c().toCurrencyString(1234.5), "1234.50");
}

struct DateCase {
    const char *locale;
    QLocale::FormatType type;
    const char *expected;
};

static void testDates() {
    const QDate d(2026, 11, 7); // a Saturday
    const DateCase longCases[] = {
        {"C", QLocale::LongFormat, "Saturday, 7 November 2026"},
        {"en_US", QLocale::LongFormat, "Saturday, November 7, 2026"},
        {"en_GB", QLocale::LongFormat, "Saturday, 7 November 2026"},
        {"en_AU", QLocale::LongFormat, "Saturday, 7 November 2026"},
        {"de_DE", QLocale::LongFormat, "Samstag, 7. November 2026"},
        {"fr_FR", QLocale::LongFormat, "samedi 7 novembre 2026"},
        {"es_ES", QLocale::LongFormat, "s\xc3\xa1" "bado, 7 de noviembre de 2026"},
        {"it_IT", QLocale::LongFormat, "sabato 7 novembre 2026"},
        {"pt_BR", QLocale::LongFormat, "s\xc3\xa1" "bado, 7 de novembro de 2026"},
        {"nl_NL", QLocale::LongFormat, "zaterdag 7 november 2026"},
        {"sv_SE", QLocale::LongFormat, "l\xc3\xb6rdag 7 november 2026"},
        {"ru_RU", QLocale::LongFormat, "\xd1\x81\xd1\x83\xd0\xb1\xd0\xb1\xd0\xbe\xd1\x82\xd0\xb0, 7 \xd0\xbd\xd0\xbe\xd1\x8f\xd0\xb1\xd1\x80\xd1\x8f 2026 \xd0\xb3."},
        {"ja_JP", QLocale::LongFormat, "2026\xe5\xb9\xb4" "11\xe6\x9c\x88" "7\xe6\x97\xa5\xe5\x9c\x9f\xe6\x9b\x9c\xe6\x97\xa5"},
        {"zh_CN", QLocale::LongFormat, "2026\xe5\xb9\xb4" "11\xe6\x9c\x88" "7\xe6\x97\xa5\xe6\x98\x9f\xe6\x9c\x9f\xe5\x85\xad"},
        {"ko_KR", QLocale::LongFormat, "2026\xeb\x85\x84 11\xec\x9b\x94 7\xec\x9d\xbc \xed\x86\xa0\xec\x9a\x94\xec\x9d\xbc"},
        {"pl_PL", QLocale::LongFormat, "sobota, 7 listopada 2026"},
        {"tr_TR", QLocale::LongFormat, "7 Kas\xc4\xb1m 2026 Cumartesi"},
        {"C", QLocale::ShortFormat, "07/11/2026"},
        {"en_US", QLocale::ShortFormat, "11/7/26"},
        {"en_GB", QLocale::ShortFormat, "07/11/2026"},
        {"en_AU", QLocale::ShortFormat, "7/11/26"},
        {"de_DE", QLocale::ShortFormat, "07.11.26"},
        {"fr_FR", QLocale::ShortFormat, "07/11/2026"},
        {"es_ES", QLocale::ShortFormat, "7/11/26"},
        {"it_IT", QLocale::ShortFormat, "07/11/26"},
        {"pt_BR", QLocale::ShortFormat, "07/11/2026"},
        {"nl_NL", QLocale::ShortFormat, "07-11-26"},
        {"sv_SE", QLocale::ShortFormat, "2026-11-07"},
        {"ru_RU", QLocale::ShortFormat, "07.11.2026"},
        {"ja_JP", QLocale::ShortFormat, "2026/11/07"},
        {"zh_CN", QLocale::ShortFormat, "2026/11/7"},
        {"ko_KR", QLocale::ShortFormat, "26. 11. 7."},
        {"pl_PL", QLocale::ShortFormat, "7.11.2026"},
        {"tr_TR", QLocale::ShortFormat, "7.11.2026"},
        {"en_US", QLocale::NarrowFormat, "11/7/26"},
    };
    for (const DateCase &c : longCases)
        CHECK_STR(QLocale(QString::fromUtf8(c.locale)).toString(d, c.type), c.expected);

    const QTime t(15, 4, 9, 7);
    const DateCase timeCases[] = {
        {"C", QLocale::LongFormat, "15:04:09 "},
        {"C", QLocale::ShortFormat, "15:04:09"},
        {"en_US", QLocale::LongFormat, "3:04:09 PM "},
        {"en_US", QLocale::ShortFormat, "3:04 PM"},
        {"en_GB", QLocale::LongFormat, "15:04:09 "},
        {"en_GB", QLocale::ShortFormat, "15:04"},
        {"en_AU", QLocale::ShortFormat, "3:04 pm"},
        {"de_DE", QLocale::ShortFormat, "15:04"},
        {"es_ES", QLocale::ShortFormat, "15:04"},
        {"ja_JP", QLocale::ShortFormat, "15:04"},
        {"ko_KR", QLocale::ShortFormat, "\xec\x98\xa4\xed\x9b\x84 3:04"},
        {"sv_SE", QLocale::ShortFormat, "15:04"},
    };
    for (const DateCase &c : timeCases)
        CHECK_STR(QLocale(QString::fromUtf8(c.locale)).toString(t, c.type), c.expected);

    // Format strings.
    const QLocale en("en_US");
    const QLocale de("de_DE");
    CHECK_STR(en.toString(d, "d"), "7");
    CHECK_STR(en.toString(d, "dd"), "07");
    CHECK_STR(en.toString(d, "ddd"), "Sat");
    CHECK_STR(en.toString(d, "dddd"), "Saturday");
    CHECK_STR(en.toString(d, "M"), "11");
    CHECK_STR(en.toString(QDate(2026, 3, 1), "M"), "3");
    CHECK_STR(en.toString(QDate(2026, 3, 1), "MM"), "03");
    CHECK_STR(en.toString(d, "MMM"), "Nov");
    CHECK_STR(en.toString(d, "MMMM"), "November");
    CHECK_STR(en.toString(d, "yy"), "26");
    CHECK_STR(en.toString(d, "yyyy"), "2026");
    CHECK_STR(en.toString(QDate(5, 1, 1), "yyyy"), "0005");
    CHECK_STR(en.toString(d, "yyyy-MM-dd"), "2026-11-07");
    CHECK_STR(en.toString(d, QString("dddd, d MMMM")), "Saturday, 7 November");
    CHECK_STR(en.toString(d, QStringView(u"d.M.")), "7.11.");
    CHECK_STR(de.toString(d, "dddd"), "Samstag");
    CHECK_STR(de.toString(d, "ddd"), "Sa.");
    CHECK_STR(de.toString(d, "MMMM"), "November");
    CHECK_STR(de.toString(QDate(2026, 3, 1), "MMM"), "M\xc3\xa4rz");
    CHECK_STR(en.toString(d, "'on' dd 'it''s' yyyy"), "on 07 it's 2026");
    CHECK_STR(en.toString(d, "dd''MM"), "07'11");
    CHECK_STR(en.toString(d, "d 'unterminated"), "7 unterminated");
    CHECK_STR(en.toString(d, "x-y-z"), "x-y-z");
    CHECK_STR(en.toString(d, "yyy y"), "yyy y");
    CHECK_STR(en.toString(d, ""), "");
    CHECK_STR(en.toString(QDate(), "yyyy"), "");
    CHECK_STR(en.toString(QDate(), QLocale::LongFormat), "");

    CHECK_STR(en.toString(t, "h:mm:ss"), "15:04:09");
    CHECK_STR(en.toString(t, "H:mm:ss"), "15:04:09");
    CHECK_STR(en.toString(t, "hh:mm AP"), "03:04 PM");
    CHECK_STR(en.toString(t, "h:mm ap"), "3:04 pm");
    CHECK_STR(en.toString(t, "h:mm a"), "3:04 pm");
    CHECK_STR(en.toString(t, "h:mm A"), "3:04 PM");
    CHECK_STR(en.toString(QTime(0, 5), "h:mm AP"), "12:05 AM");
    CHECK_STR(en.toString(QTime(12, 5), "h:mm AP"), "12:05 PM");
    CHECK_STR(en.toString(QTime(0, 5), "HH:mm"), "00:05");
    CHECK_STR(en.toString(t, "ss.zzz"), "09.007");
    CHECK_STR(en.toString(t, "s.z"), "9.7");
    CHECK_STR(en.toString(t, "m"), "4");
    CHECK_STR(de.toString(t, "h:mm AP"), "3:04 PM");
    CHECK_STR(QLocale("es_ES").toString(t, "h:mm AP"), "3:04 P.\xc2\xa0M.");
    CHECK_STR(QLocale("sv_SE").toString(t, "h:mm ap"), "3:04 em");
    CHECK_STR(en.toString(QTime(), "hh"), "");
    CHECK_STR(en.toString(QTime(), QLocale::ShortFormat), "");

    // Date and time.
    const QDateTime utc(d, t, Qt::UTC);
    CHECK_STR(en.toString(utc, "yyyy-MM-dd hh:mm:ss t"), "2026-11-07 15:04:09 UTC");
    CHECK_STR(en.toString(utc, "dddd MMMM h:mm AP"), "Saturday November 3:04 PM");
    CHECK_STR(de.toString(utc, QString("dddd, d. MMMM yyyy HH:mm")), "Samstag, 7. November 2026 15:04");
    CHECK_STR(en.toString(utc, QLocale::ShortFormat), "11/7/26, 3:04 PM");
    CHECK_STR(en.toString(utc, QLocale::LongFormat), "Saturday, November 7, 2026 at 3:04:09 PM UTC");
    CHECK_STR(de.toString(utc, QLocale::ShortFormat), "07.11.26, 15:04");
    CHECK_STR(QLocale::c().toString(utc, QLocale::LongFormat), "Saturday, 7 November 2026 15:04:09 UTC");
    CHECK_STR(QLocale::c().toString(utc, QLocale::ShortFormat), "07/11/2026 15:04:09");
    QDateTime off(d, t, Qt::OffsetFromUTC, 2 * 3600 + 30 * 60);
    CHECK_STR(en.toString(off, "t"), "UTC+02:30");
    QDateTime offNeg(d, t, Qt::OffsetFromUTC, -(5 * 3600));
    CHECK_STR(en.toString(offNeg, "t"), "UTC-05:00");
    QDateTime off0(d, t, Qt::OffsetFromUTC, 0);
    CHECK_STR(en.toString(off0, "t"), "UTC");
    CHECK_STR(en.toString(QDateTime(), "yyyy"), "");
    CHECK_STR(en.toString(QDateTime(), QLocale::LongFormat), "");
    CHECK(!en.toString(QDateTime(d, t, Qt::LocalTime), "t").isNull());

    // Format helpers.
    CHECK_STR(en.dateFormat(), "dddd, MMMM d, yyyy");
    CHECK_STR(en.dateFormat(QLocale::ShortFormat), "M/d/yy");
    CHECK_STR(en.timeFormat(QLocale::ShortFormat), "h:mm AP");
    CHECK_STR(en.dateTimeFormat(QLocale::ShortFormat), "M/d/yy, h:mm AP");
    CHECK_STR(de.dateTimeFormat(QLocale::ShortFormat), "dd.MM.yy, HH:mm");
}

static void testNames() {
    struct NameRow {
        const char *locale;
        const char *days[7];
        const char *months[12];
    };
    const NameRow names[] = {
        {"en_US",
         {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"},
         {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"}},
        {"de_DE",
         {"Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag", "Sonntag"},
         {"Januar", "Februar", "M\xc3\xa4rz", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"}},
        {"fr_FR",
         {"lundi", "mardi", "mercredi", "jeudi", "vendredi", "samedi", "dimanche"},
         {"janvier", "f\xc3\xa9vrier", "mars", "avril", "mai", "juin", "juillet", "ao\xc3\xbbt", "septembre", "octobre", "novembre", "d\xc3\xa9" "cembre"}},
        {"es_ES",
         {"lunes", "martes", "mi\xc3\xa9rcoles", "jueves", "viernes", "s\xc3\xa1" "bado", "domingo"},
         {"enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre", "octubre", "noviembre", "diciembre"}},
        {"it_IT",
         {"luned\xc3\xac", "marted\xc3\xac", "mercoled\xc3\xac", "gioved\xc3\xac", "venerd\xc3\xac", "sabato", "domenica"},
         {"gennaio", "febbraio", "marzo", "aprile", "maggio", "giugno", "luglio", "agosto", "settembre", "ottobre", "novembre", "dicembre"}},
        {"nl_NL",
         {"maandag", "dinsdag", "woensdag", "donderdag", "vrijdag", "zaterdag", "zondag"},
         {"januari", "februari", "maart", "april", "mei", "juni", "juli", "augustus", "september", "oktober", "november", "december"}},
        {"sv_SE",
         {"m\xc3\xa5ndag", "tisdag", "onsdag", "torsdag", "fredag", "l\xc3\xb6rdag", "s\xc3\xb6ndag"},
         {"januari", "februari", "mars", "april", "maj", "juni", "juli", "augusti", "september", "oktober", "november", "december"}},
    };
    for (const NameRow &r : names) {
        const QLocale l{QString::fromUtf8(r.locale)};
        for (int i = 0; i < 7; ++i) {
            CHECK_STR(l.dayName(i + 1), r.days[i]);
            CHECK_STR(l.dayName(i + 1, QLocale::LongFormat), r.days[i]);
            CHECK_STR(l.standaloneDayName(i + 1), r.days[i]);
            CHECK(!l.dayName(i + 1, QLocale::ShortFormat).isEmpty());
            CHECK(l.dayName(i + 1, QLocale::NarrowFormat).size() >= 1);
        }
        for (int i = 0; i < 12; ++i) {
            CHECK_STR(l.monthName(i + 1), r.months[i]);
            CHECK_STR(l.standaloneMonthName(i + 1), r.months[i]);
            CHECK(!l.monthName(i + 1, QLocale::ShortFormat).isEmpty());
            CHECK(l.monthName(i + 1, QLocale::NarrowFormat).size() >= 1);
        }
        CHECK(l.dayName(0).isEmpty() && l.dayName(8).isEmpty() && l.monthName(0).isEmpty() && l.monthName(13).isEmpty());
        CHECK(l.standaloneDayName(0).isEmpty() && l.standaloneMonthName(13).isEmpty());
    }
    // Every locale has names for every day and month in every form.
    for (const QLocale &l : QLocale::matchingLocales(QLocale::AnyLanguage, QLocale::AnyScript, QLocale::AnyTerritory)) {
        for (QLocale::FormatType ft : {QLocale::LongFormat, QLocale::ShortFormat, QLocale::NarrowFormat}) {
            for (int i = 1; i <= 7; ++i) {
                CHECK(!l.dayName(i, ft).isEmpty());
                CHECK(!l.standaloneDayName(i, ft).isEmpty());
            }
            for (int i = 1; i <= 12; ++i) {
                CHECK(!l.monthName(i, ft).isEmpty());
                CHECK(!l.standaloneMonthName(i, ft).isEmpty());
            }
        }
        CHECK(!l.dateFormat().isEmpty() && !l.dateFormat(QLocale::ShortFormat).isEmpty());
        CHECK(!l.timeFormat().isEmpty() && !l.timeFormat(QLocale::ShortFormat).isEmpty());
        CHECK(!l.amText().isEmpty() && !l.pmText().isEmpty());
        CHECK(!l.decimalPoint().isEmpty() && !l.groupSeparator().isEmpty());
        CHECK(!l.nativeLanguageName().isEmpty());
        CHECK(!l.weekdays().isEmpty());
    }
    const QLocale ru("ru_RU");
    CHECK_STR(ru.monthName(3), "\xd0\xbc\xd0\xb0\xd1\x80\xd1\x82\xd0\xb0");
    CHECK_STR(ru.standaloneMonthName(3), "\xd0\xbc\xd0\xb0\xd1\x80\xd1\x82");
    CHECK_STR(QLocale("pl_PL").monthName(1), "stycznia");
    CHECK_STR(QLocale("pl_PL").standaloneMonthName(1), "stycze\xc5\x84");
    CHECK_STR(QLocale("en_US").dayName(1, QLocale::ShortFormat), "Mon");
    CHECK_STR(QLocale("en_US").dayName(1, QLocale::NarrowFormat), "M");
    CHECK_STR(QLocale("en_US").monthName(9, QLocale::ShortFormat), "Sep");
    CHECK_STR(QLocale("de_DE").dayName(1, QLocale::ShortFormat), "Mo.");
    CHECK_STR(QLocale("de_DE").standaloneDayName(1, QLocale::ShortFormat), "Mo");
    CHECK_STR(QLocale("ja_JP").dayName(1, QLocale::ShortFormat), "\xe6\x9c\x88");
}

static void testStringIndependence() {
    // QString functions stay locale independent, whatever the default is.
    const QLocale saved;
    QLocale::setDefault(QLocale("de_DE"));
    CHECK_STR(QString::number(1234.5, 'f', 1), "1234.5");
    CHECK_STR(QString::number(1234567), "1234567");
    CHECK_STR(QString("%1").arg(1234.5, 0, 'f', 1), "1234.5");
    CHECK_STR(QString("%1").arg(1234567), "1234567");
    CHECK(QString("1234.5").toDouble() == 1234.5);
    bool ok = true;
    CHECK(QString("1234,5").toDouble(&ok) == 0 && !ok);
    CHECK(QString("1,234").toInt(&ok) == 0 && !ok);
    // %L uses the default locale.
    CHECK_STR(QString("%L1").arg(1234567), "1.234.567");
    CHECK_STR(QString("%L1").arg(1234.5, 0, 'f', 1), "1.234,5");
    CHECK_STR(QString("%L1 %2").arg(1234567).arg(1234567), "1.234.567 1234567");
    CHECK_STR(QString("%L1").arg(static_cast<qulonglong>(1234567)), "1.234.567");
    CHECK_STR(QString("%L1").arg(QString("text")), "text");
    CHECK_STR(QString("%L1").arg(1234567, 12), "   1.234.567");
    CHECK_STR(QString("%L1").arg(1234567, -12), "1.234.567   ");
    QLocale::setDefault(QLocale("en_US"));
    CHECK_STR(QString("%L1").arg(1234567), "1,234,567");
    CHECK_STR(QString("%L1").arg(-1234567), "-1,234,567");
    CHECK_STR(QString("%1").arg(1234567), "1234567");
    QLocale::setDefault(QLocale::c());
    CHECK_STR(QString("%L1").arg(1234567), "1234567");
    QLocale::setDefault(saved);
}

int main() {
    testIdentity();
    testIntegers();
    testDoubles();
    testParsing();
    testSymbols();
    testCurrency();
    testDates();
    testNames();
    testStringIndependence();
    if (g_failures == 0)
        std::printf("all locale tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
