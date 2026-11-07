// MIT License - see LICENSE
#pragma once

#include <QtCore/qdatetime.h>
#include <QtCore/qflags.h>
#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qstringview.h>

#include <type_traits>

class QDebug;
struct QLocaleRow;

/*!
    \class QLocale
    \brief The QLocale class converts between numbers and dates and their string representations in various languages.

    A QLocale describes the language and territory conventions that apply to
    the text of numbers, dates, times, currencies and lists: the decimal point
    and group separator, the names of months and days, and the order of the
    fields of a date. Create one from a name, \c{QLocale("de_DE")}, or from
    a language and a territory, \c{QLocale(QLocale::German, QLocale::Germany)}.
    QLocale::c() is the locale of the C language, QLocale::system() the locale
    the environment selects, and \c{QLocale()} the default locale, which is
    the system locale unless setDefault() changed it.

    \code
    QLocale de(QLocale::German, QLocale::Germany);
    de.toString(1234567.891, 'f', 2);        // "1.234.567,89"
    de.toString(QDate(2026, 11, 7), QLocale::LongFormat); // "Samstag, 7. November 2026"
    de.toDouble("1.234,5");                  // 1234.5
    \endcode

    QString::number(), QString::arg() and QString::toDouble() do not use a
    locale: they always use the C locale, as in Qt. Use QLocale::toString()
    and QLocale::toDouble() for text read or written by people. A \c{%L1}
    place marker in QString::arg() uses the default locale.

    \section1 Locale data

    Nt does not include the Unicode CLDR. The data is a compact table written
    by hand, with a language, a territory, number symbols, date and time
    formats, names of the months and days, and list, currency and quotation
    conventions for these locales: \c C, \c en_US, \c en_GB, \c en_AU,
    \c de_DE, \c fr_FR, \c es_ES, \c it_IT, \c pt_BR, \c nl_NL, \c sv_SE,
    \c ru_RU, \c ja_JP, \c zh_CN, \c ko_KR, \c pl_PL and \c tr_TR. A locale for
    another territory of one of these languages, such as \c{de_AT}, keeps its
    own language and territory but takes everything else from the table entry
    of the language. A locale for any other language is the C locale. The
    values follow CLDR as Qt 6 uses it, but they are not guaranteed to be the
    same as the ones of a given Qt release.

    \differences
    Only the locales above have data (see \e{Locale data}); every other
    language gives the C locale, so \c language() is QLocale::C for it. The
    Language, Territory and Script enumerations list a subset of the values of
    Qt, with the numeric values Qt 6 gives them. There is no QCalendar, so
    the date functions take no calendar argument and use the Gregorian
    calendar, and there is no QTimeZone; the \c t format of a QDateTime shows
    \c UTC, a UTC offset or the abbreviation of the local zone. Text cannot be
    parsed into dates (\c toDate(), \c toTime() and \c toDateTime() are not
    provided), the native digits of a script are not used (the zero digit is
    always \c{'0'}), grouping is always in threes, the \c{y} and \c{yyy} date
    format codes are not provided, and QLocale is not a QVariant type and has
    no QDataStream operators. system() is derived from the environment
    variables \c LC_ALL, \c LC_MESSAGES and \c LANG on every platform, and
    \c LANGUAGE is read for uiLanguages(); the operating system settings are
    not queried. The number of digits of a currency is that of the locale's
    own currency, and \c toCurrencyString() does not know other currencies.
    \enddifferences

    \sa QString::number(), QCollator, QDate::toString()
*/
class QLocale {
public:
    /*!
        \enum QLocale::Language

        This enum names the languages of a locale, with the values of Qt 6. A
        subset of the languages of Qt is listed. Only \c C, \c English,
        \c German, \c French, \c Spanish, \c Italian, \c Portuguese, \c Dutch,
        \c Swedish, \c Russian, \c Japanese, \c Chinese, \c Korean, \c Polish
        and \c Turkish have locale data.

        \value AnyLanguage No language; matches any language.
        \value C The C locale, with English text and the conventions of POSIX.
        \value Afrikaans Afrikaans.
        \value Albanian Albanian.
        \value Arabic Arabic.
        \value Armenian Armenian.
        \value Azerbaijani Azerbaijani.
        \value Basque Basque.
        \value Belarusian Belarusian.
        \value Bangla Bangla, also called Bengali.
        \value Bosnian Bosnian.
        \value Bulgarian Bulgarian.
        \value Catalan Catalan.
        \value Chinese Chinese.
        \value Croatian Croatian.
        \value Czech Czech.
        \value Danish Danish.
        \value Dutch Dutch.
        \value English English.
        \value Estonian Estonian.
        \value Filipino Filipino.
        \value Finnish Finnish.
        \value French French.
        \value Galician Galician.
        \value Georgian Georgian.
        \value German German.
        \value Greek Greek.
        \value Gujarati Gujarati.
        \value Hebrew Hebrew.
        \value Hindi Hindi.
        \value Hungarian Hungarian.
        \value Icelandic Icelandic.
        \value Indonesian Indonesian.
        \value Irish Irish.
        \value Italian Italian.
        \value Japanese Japanese.
        \value Kazakh Kazakh.
        \value Khmer Khmer.
        \value Korean Korean.
        \value Latvian Latvian.
        \value Lithuanian Lithuanian.
        \value Macedonian Macedonian.
        \value Malay Malay.
        \value Maltese Maltese.
        \value Mongolian Mongolian.
        \value NorwegianBokmal Norwegian Bokmal.
        \value NorwegianNynorsk Norwegian Nynorsk.
        \value Persian Persian.
        \value Polish Polish.
        \value Portuguese Portuguese.
        \value Romanian Romanian.
        \value Russian Russian.
        \value Serbian Serbian.
        \value Slovak Slovak.
        \value Slovenian Slovenian.
        \value Spanish Spanish.
        \value Swahili Swahili.
        \value Swedish Swedish.
        \value Tamil Tamil.
        \value Thai Thai.
        \value Turkish Turkish.
        \value Ukrainian Ukrainian.
        \value Urdu Urdu.
        \value Uzbek Uzbek.
        \value Vietnamese Vietnamese.
        \value Welsh Welsh.
        \omitvalue LastLanguage
    */
    enum Language {
        AnyLanguage = 0,
        C = 1,
        Afrikaans = 4,
        Albanian = 9,
        Arabic = 14,
        Armenian = 17,
        Azerbaijani = 25,
        Bangla = 30,
        Basque = 33,
        Belarusian = 35,
        Bosnian = 42,
        Bulgarian = 45,
        Catalan = 48,
        Chinese = 58,
        Croatian = 64,
        Czech = 65,
        Danish = 66,
        Dutch = 70,
        English = 73,
        Estonian = 76,
        Filipino = 81,
        Finnish = 82,
        French = 83,
        Galician = 88,
        Georgian = 91,
        German = 92,
        Greek = 94,
        Gujarati = 96,
        Hebrew = 101,
        Hindi = 103,
        Hungarian = 105,
        Icelandic = 106,
        Indonesian = 110,
        Irish = 116,
        Italian = 117,
        Japanese = 118,
        Kazakh = 131,
        Khmer = 133,
        Korean = 140,
        Latvian = 152,
        Lithuanian = 157,
        Macedonian = 166,
        Malay = 173,
        Maltese = 174,
        Mongolian = 188,
        NorwegianBokmal = 206,
        NorwegianNynorsk = 207,
        Persian = 221,
        Polish = 223,
        Portuguese = 224,
        Romanian = 228,
        Russian = 232,
        Serbian = 245,
        Slovak = 258,
        Slovenian = 259,
        Spanish = 262,
        Swahili = 265,
        Swedish = 267,
        Tamil = 275,
        Thai = 281,
        Turkish = 289,
        Ukrainian = 293,
        Urdu = 295,
        Uzbek = 297,
        Vietnamese = 300,
        Welsh = 306,
        LastLanguage = Welsh
    };

    /*!
        \enum QLocale::Script

        This enum names the scripts a language can be written in, with the
        values of Qt 6. The scripts up to \c VaiScript are listed.

        \value AnyScript No script; matches any script.
        \value ArabicScript Arabic.
        \value CyrillicScript Cyrillic.
        \value DeseretScript Deseret.
        \value GurmukhiScript Gurmukhi.
        \value SimplifiedHanScript Han, simplified.
        \value TraditionalHanScript Han, traditional.
        \value LatinScript Latin.
        \value MongolianScript Mongolian.
        \value TifinaghScript Tifinagh.
        \value ArmenianScript Armenian.
        \value BanglaScript Bangla.
        \value CherokeeScript Cherokee.
        \value DevanagariScript Devanagari.
        \value EthiopicScript Ethiopic.
        \value GeorgianScript Georgian.
        \value GreekScript Greek.
        \value GujaratiScript Gujarati.
        \value HebrewScript Hebrew.
        \value JapaneseScript Japanese.
        \value KhmerScript Khmer.
        \value KannadaScript Kannada.
        \value KoreanScript Korean.
        \value LaoScript Lao.
        \value MalayalamScript Malayalam.
        \value MyanmarScript Myanmar.
        \value OriyaScript Oriya.
        \value TamilScript Tamil.
        \value TeluguScript Telugu.
        \value ThaanaScript Thaana.
        \value ThaiScript Thai.
        \value TibetanScript Tibetan.
        \value SinhalaScript Sinhala.
        \value SyriacScript Syriac.
        \value YiScript Yi.
        \value VaiScript Vai.
        \omitvalue LastScript
    */
    enum Script {
        AnyScript = 0,
        ArabicScript = 1,
        CyrillicScript = 2,
        DeseretScript = 3,
        GurmukhiScript = 4,
        SimplifiedHanScript = 5,
        TraditionalHanScript = 6,
        LatinScript = 7,
        MongolianScript = 8,
        TifinaghScript = 9,
        ArmenianScript = 10,
        BanglaScript = 11,
        CherokeeScript = 12,
        DevanagariScript = 13,
        EthiopicScript = 14,
        GeorgianScript = 15,
        GreekScript = 16,
        GujaratiScript = 17,
        HebrewScript = 18,
        JapaneseScript = 19,
        KhmerScript = 20,
        KannadaScript = 21,
        KoreanScript = 22,
        LaoScript = 23,
        MalayalamScript = 24,
        MyanmarScript = 25,
        OriyaScript = 26,
        TamilScript = 27,
        TeluguScript = 28,
        ThaanaScript = 29,
        ThaiScript = 30,
        TibetanScript = 31,
        SinhalaScript = 32,
        SyriacScript = 33,
        YiScript = 34,
        VaiScript = 35,
        LastScript = VaiScript
    };

    /*!
        \enum QLocale::Territory

        This enum names the territories (countries and regions) of a locale,
        with the values of Qt 6. A subset of the territories of Qt is listed.

        \value AnyTerritory No territory; matches any territory.
        \value Argentina Argentina.
        \value Australia Australia.
        \value Austria Austria.
        \value Belgium Belgium.
        \value Brazil Brazil.
        \value Canada Canada.
        \value Chile Chile.
        \value China China.
        \value Colombia Colombia.
        \value Czechia Czechia.
        \value Denmark Denmark.
        \value Finland Finland.
        \value France France.
        \value Germany Germany.
        \value Greece Greece.
        \value HongKong Hong Kong.
        \value Hungary Hungary.
        \value India India.
        \value Ireland Ireland.
        \value Israel Israel.
        \value Italy Italy.
        \value Japan Japan.
        \value Mexico Mexico.
        \value Netherlands The Netherlands.
        \value NewZealand New Zealand.
        \value Norway Norway.
        \value Poland Poland.
        \value Portugal Portugal.
        \value Russia Russia.
        \value Singapore Singapore.
        \value SouthAfrica South Africa.
        \value SouthKorea South Korea.
        \value Spain Spain.
        \value Sweden Sweden.
        \value Switzerland Switzerland.
        \value Taiwan Taiwan.
        \value Turkey Turkey.
        \value Ukraine Ukraine.
        \value UnitedKingdom The United Kingdom.
        \value UnitedStates The United States.
        \value World The world, as in the locale \c{es_419}.
        \omitvalue LastTerritory
    */
    enum Territory {
        AnyTerritory = 0,
        Argentina = 11,
        Australia = 15,
        Austria = 16,
        Belgium = 23,
        Brazil = 32,
        Canada = 41,
        Chile = 49,
        China = 50,
        Colombia = 54,
        Czechia = 64,
        Denmark = 65,
        Finland = 83,
        France = 84,
        Germany = 91,
        Greece = 94,
        HongKong = 107,
        Hungary = 108,
        India = 110,
        Ireland = 114,
        Israel = 116,
        Italy = 117,
        Japan = 120,
        Mexico = 152,
        Netherlands = 165,
        NewZealand = 167,
        Norway = 175,
        Poland = 187,
        Portugal = 188,
        Russia = 193,
        Singapore = 210,
        SouthAfrica = 216,
        SouthKorea = 218,
        Spain = 220,
        Sweden = 225,
        Switzerland = 226,
        Taiwan = 228,
        Turkey = 239,
        Ukraine = 244,
        UnitedKingdom = 246,
        UnitedStates = 247,
        World = 258,
        LastTerritory = World
    };
    /*!
        The old name of Territory, kept for compatibility.
    */
    using Country = Territory;

    /*!
        \enum QLocale::FormatType

        This enum selects the length of the format of a date, a time or a name.

        \value LongFormat A long, complete form such as \c{"Saturday"} or \c{"November"}.
        \value ShortFormat A short form such as \c{"Sat"} or \c{"Nov"}.
        \value NarrowFormat A narrow form, usually one letter, such as \c{"S"} or \c{"N"}; it is not unique.
    */
    enum FormatType { LongFormat, ShortFormat, NarrowFormat };

    /*!
        \enum QLocale::NumberOption

        This enum sets how numbers are written and read. The NumberOptions type
        is a typedef for \c{QFlags<NumberOption>}.

        \value DefaultNumberOptions Group separators are written and accepted, and the exponent keeps its leading zero.
        \value OmitGroupSeparator Group separators are left out when writing numbers.
        \value RejectGroupSeparator Reading a number fails if it has group separators.
        \value OmitLeadingZeroInExponent The exponent is written without a leading zero, \c{1e+5} rather than \c{1e+05}.
        \value RejectLeadingZeroInExponent Reading a number fails if its exponent has a leading zero.
        \value IncludeTrailingZeroesAfterDot Trailing zeroes after the decimal point are kept when a number is written with the \c 'g' or \c 'G' format.
        \value RejectTrailingZeroesAfterDot Reading a number fails if it has trailing zeroes after the decimal point.
    */
    enum NumberOption {
        DefaultNumberOptions = 0x0,
        OmitGroupSeparator = 0x01,
        RejectGroupSeparator = 0x02,
        OmitLeadingZeroInExponent = 0x04,
        RejectLeadingZeroInExponent = 0x08,
        IncludeTrailingZeroesAfterDot = 0x10,
        RejectTrailingZeroesAfterDot = 0x20
    };
    Q_DECLARE_FLAGS(NumberOptions, NumberOption)

    /*!
        \enum QLocale::FloatingPointPrecisionOption

        This enum is a value for the precision argument of the number-to-string
        functions.

        \value FloatingPointShortest Use the fewest digits that read back as the same number.
    */
    enum FloatingPointPrecisionOption { FloatingPointShortest = -128 };

    /*!
        \enum QLocale::MeasurementSystem

        This enum names the system of units a locale uses.

        \value MetricSystem The metric system.
        \value ImperialUSSystem The system of the United States.
        \value ImperialUKSystem The system of the United Kingdom.
        \value ImperialSystem The same as ImperialUSSystem.
    */
    enum MeasurementSystem { MetricSystem, ImperialUSSystem, ImperialUKSystem, ImperialSystem = ImperialUSSystem };

    /*!
        \enum QLocale::DataSizeFormat

        This enum selects the units formattedDataSize() uses. The
        DataSizeFormats type is a typedef for \c{QFlags<DataSizeFormat>}.

        \value DataSizeBase1000 Use a base of 1000 instead of 1024.
        \value DataSizeSIQuantifiers Use the SI quantifiers (\c kB, \c MB) rather than the IEC ones (\c KiB, \c MiB).
        \value DataSizeIecFormat A base of 1024 with the IEC quantifiers.
        \value DataSizeTraditionalFormat A base of 1024 with the SI quantifiers.
        \value DataSizeSIFormat A base of 1000 with the SI quantifiers.
    */
    enum DataSizeFormat {
        DataSizeBase1000 = 1,
        DataSizeSIQuantifiers = 2,
        DataSizeIecFormat = 0,
        DataSizeTraditionalFormat = DataSizeSIQuantifiers,
        DataSizeSIFormat = DataSizeBase1000 | DataSizeSIQuantifiers
    };
    Q_DECLARE_FLAGS(DataSizeFormats, DataSizeFormat)

    /*!
        \enum QLocale::CurrencySymbolFormat

        This enum selects what currencySymbol() returns.

        \value CurrencyIsoCode The ISO 4217 code, such as \c USD.
        \value CurrencySymbol The symbol, such as \c $.
        \value CurrencyDisplayName The English name of the currency, such as \c{US Dollar}.
    */
    enum CurrencySymbolFormat { CurrencyIsoCode, CurrencySymbol, CurrencyDisplayName };

    /*!
        \enum QLocale::QuotationStyle

        This enum selects the quotation marks quoteString() uses.

        \value StandardQuotation The marks of the first level of quotation.
        \value AlternateQuotation The marks used inside a standard quotation.
    */
    enum QuotationStyle { StandardQuotation, AlternateQuotation };

    /*!
        \enum QLocale::LanguageCodeType

        This enum selects the kind of language code languageToCode() returns
        and codeToLanguage() accepts. The LanguageCodeTypes type is a typedef
        for \c{QFlags<LanguageCodeType>}.

        \value ISO639Part1 The two-letter code of ISO 639-1.
        \value ISO639Part2B The three-letter bibliographic code of ISO 639-2.
        \value ISO639Part2T The three-letter terminology code of ISO 639-2.
        \value ISO639Part3 The three-letter code of ISO 639-3.
        \value LegacyLanguageCode The old ISO 639 code Qt used for some languages.
        \value ISO639Part2 Either three-letter code of ISO 639-2.
        \value ISO639Alpha2 The same as ISO639Part1.
        \value ISO639Alpha3 Any three-letter code.
        \value ISO639 Any ISO 639 code.
        \value AnyLanguageCode Any code.
    */
    enum LanguageCodeType {
        ISO639Part1 = 1 << 0,
        ISO639Part2B = 1 << 1,
        ISO639Part2T = 1 << 2,
        ISO639Part3 = 1 << 3,
        LegacyLanguageCode = 1 << 15,
        ISO639Part2 = ISO639Part2B | ISO639Part2T,
        ISO639Alpha2 = ISO639Part1,
        ISO639Alpha3 = ISO639Part2 | ISO639Part3,
        ISO639 = ISO639Alpha2 | ISO639Alpha3,
        AnyLanguageCode = -1
    };
    Q_DECLARE_FLAGS(LanguageCodeTypes, LanguageCodeType)

    /*!
        \enum QLocale::TagSeparator

        This enum is the character that separates the parts of a locale name.

        \value Dash A hyphen, as in \c{en-US}.
        \value Underscore An underscore, as in \c{en_US}.
    */
    enum class TagSeparator : char { Dash = '-', Underscore = '_' };

    QLocale();
    QLocale(const QString &name);
    QLocale(Language language, Territory territory = AnyTerritory);
    QLocale(Language language, Script script, Territory territory);
    QLocale(const QLocale &other) = default;
    QLocale &operator=(const QLocale &other) = default;
    /*!
        Destroys the locale.
    */
    ~QLocale() = default;
    /*!
        Swaps this locale with \a other. This operation is very fast and never
        fails.
    */
    void swap(QLocale &other) noexcept {
        QLocale t(*this);
        *this = other;
        other = t;
    }

    Language language() const;
    Script script() const;
    Territory territory() const;
    /*!
        Returns the territory of the locale. This is the same as territory(),
        provided for compatibility.
    */
    Territory country() const { return territory(); }
    QString name(TagSeparator separator = TagSeparator::Underscore) const;
    QString bcp47Name(TagSeparator separator = TagSeparator::Dash) const;
    QString nativeLanguageName() const;
    QString nativeTerritoryName() const;
    /*!
        Returns the native name of the territory of the locale. This is the
        same as nativeTerritoryName(), provided for compatibility.
    */
    QString nativeCountryName() const { return nativeTerritoryName(); }

    static QString languageToString(Language language);
    static QString territoryToString(Territory territory);
    static QString scriptToString(Script script);
    /*!
        Returns the English name of \a territory. This is the same as
        territoryToString(), provided for compatibility.
    */
    static QString countryToString(Territory territory) { return territoryToString(territory); }
    static QString languageToCode(Language language, LanguageCodeTypes codeTypes = AnyLanguageCode);
    static QString territoryToCode(Territory territory);
    static QString scriptToCode(Script script);
    static Language codeToLanguage(QStringView languageCode, LanguageCodeTypes codeTypes = AnyLanguageCode);
    static Territory codeToTerritory(QStringView territoryCode);
    static Script codeToScript(QStringView scriptCode);

    static void setDefault(const QLocale &locale);
    static QLocale c();
    static QLocale system();
    static QList<QLocale> matchingLocales(Language language, Script script, Territory territory);

    void setNumberOptions(NumberOptions options);
    NumberOptions numberOptions() const;

    QString toString(qlonglong i) const;
    QString toString(qulonglong i) const;
    /*!
        \overload

        Returns a string that represents \a i in the format of the locale.
    */
    QString toString(long i) const { return toString(static_cast<qlonglong>(i)); }
    /*!
        \overload

        Returns a string that represents \a i in the format of the locale.
    */
    QString toString(unsigned long i) const { return toString(static_cast<qulonglong>(i)); }
    /*!
        \overload

        Returns a string that represents \a i in the format of the locale.
    */
    QString toString(short i) const { return toString(static_cast<qlonglong>(i)); }
    /*!
        \overload

        Returns a string that represents \a i in the format of the locale.
    */
    QString toString(unsigned short i) const { return toString(static_cast<qulonglong>(i)); }
    /*!
        \overload

        Returns a string that represents \a i in the format of the locale.
    */
    QString toString(int i) const { return toString(static_cast<qlonglong>(i)); }
    /*!
        \overload

        Returns a string that represents \a i in the format of the locale.
    */
    QString toString(unsigned int i) const { return toString(static_cast<qulonglong>(i)); }
    QString toString(double f, char format = 'g', int precision = 6) const;
    /*!
        \overload

        Returns a string that represents \a f in the format of the locale,
        written as described for the \c double overload.
    */
    QString toString(float f, char format = 'g', int precision = 6) const { return toString(static_cast<double>(f), format, precision); }
    QString toString(QDate date, FormatType format = LongFormat) const;
    QString toString(QTime time, FormatType format = LongFormat) const;
    QString toString(const QDateTime &dateTime, FormatType format = LongFormat) const;
    QString toString(QDate date, QStringView format) const;
    QString toString(QTime time, QStringView format) const;
    QString toString(const QDateTime &dateTime, QStringView format) const;
    /*!
        \overload

        Returns \a date formatted with the format string \a format, which is
        a UTF-8 string.
    */
    QString toString(QDate date, const char *format) const { return toString(date, QString::fromUtf8(format)); }
    /*!
        \overload

        Returns \a time formatted with the format string \a format, which is
        a UTF-8 string.
    */
    QString toString(QTime time, const char *format) const { return toString(time, QString::fromUtf8(format)); }
    /*!
        \overload

        Returns \a dateTime formatted with the format string \a format, which
        is a UTF-8 string.
    */
    QString toString(const QDateTime &dateTime, const char *format) const { return toString(dateTime, QString::fromUtf8(format)); }

    short toShort(QStringView s, bool *ok = nullptr) const;
    ushort toUShort(QStringView s, bool *ok = nullptr) const;
    int toInt(QStringView s, bool *ok = nullptr) const;
    uint toUInt(QStringView s, bool *ok = nullptr) const;
    long toLong(QStringView s, bool *ok = nullptr) const;
    ulong toULong(QStringView s, bool *ok = nullptr) const;
    qlonglong toLongLong(QStringView s, bool *ok = nullptr) const;
    qulonglong toULongLong(QStringView s, bool *ok = nullptr) const;
    float toFloat(QStringView s, bool *ok = nullptr) const;
    double toDouble(QStringView s, bool *ok = nullptr) const;
    /*!
        \overload

        Returns the UTF-8 string \a s converted to a \c short, as described
        for the QStringView overload.
    */
    short toShort(const char *s, bool *ok = nullptr) const { return toShort(QString::fromUtf8(s), ok); }
    /*!
        \overload

        Returns the UTF-8 string \a s converted to an \c{unsigned short}.
    */
    ushort toUShort(const char *s, bool *ok = nullptr) const { return toUShort(QString::fromUtf8(s), ok); }
    /*!
        \overload

        Returns the UTF-8 string \a s converted to an \c int.
    */
    int toInt(const char *s, bool *ok = nullptr) const { return toInt(QString::fromUtf8(s), ok); }
    /*!
        \overload

        Returns the UTF-8 string \a s converted to an \c{unsigned int}.
    */
    uint toUInt(const char *s, bool *ok = nullptr) const { return toUInt(QString::fromUtf8(s), ok); }
    /*!
        \overload

        Returns the UTF-8 string \a s converted to a \c long.
    */
    long toLong(const char *s, bool *ok = nullptr) const { return toLong(QString::fromUtf8(s), ok); }
    /*!
        \overload

        Returns the UTF-8 string \a s converted to an \c{unsigned long}.
    */
    ulong toULong(const char *s, bool *ok = nullptr) const { return toULong(QString::fromUtf8(s), ok); }
    /*!
        \overload

        Returns the UTF-8 string \a s converted to a \c{long long}.
    */
    qlonglong toLongLong(const char *s, bool *ok = nullptr) const { return toLongLong(QString::fromUtf8(s), ok); }
    /*!
        \overload

        Returns the UTF-8 string \a s converted to an \c{unsigned long long}.
    */
    qulonglong toULongLong(const char *s, bool *ok = nullptr) const { return toULongLong(QString::fromUtf8(s), ok); }
    /*!
        \overload

        Returns the UTF-8 string \a s converted to a \c float.
    */
    float toFloat(const char *s, bool *ok = nullptr) const { return toFloat(QString::fromUtf8(s), ok); }
    /*!
        \overload

        Returns the UTF-8 string \a s converted to a \c double.
    */
    double toDouble(const char *s, bool *ok = nullptr) const { return toDouble(QString::fromUtf8(s), ok); }

    QString decimalPoint() const;
    QString groupSeparator() const;
    QString percent() const;
    QString zeroDigit() const;
    QString negativeSign() const;
    QString positiveSign() const;
    QString exponential() const;

    QString dateFormat(FormatType format = LongFormat) const;
    QString timeFormat(FormatType format = LongFormat) const;
    QString dateTimeFormat(FormatType format = LongFormat) const;
    QString dayName(int day, FormatType type = LongFormat) const;
    QString standaloneDayName(int day, FormatType type = LongFormat) const;
    QString monthName(int month, FormatType type = LongFormat) const;
    QString standaloneMonthName(int month, FormatType type = LongFormat) const;
    QString amText() const;
    QString pmText() const;
    Qt::DayOfWeek firstDayOfWeek() const;
    QList<Qt::DayOfWeek> weekdays() const;
    MeasurementSystem measurementSystem() const;
    Qt::LayoutDirection textDirection() const;
    QString toUpper(const QString &str) const;
    QString toLower(const QString &str) const;
    QString formattedDataSize(qint64 bytes, int precision = 2, DataSizeFormats format = DataSizeIecFormat) const;
    QString createSeparatedList(const QStringList &list) const;
    QString currencySymbol(CurrencySymbolFormat format = CurrencySymbol) const;
    QString toCurrencyString(qlonglong value, const QString &symbol = QString()) const;
    QString toCurrencyString(qulonglong value, const QString &symbol = QString()) const;
    QString toCurrencyString(double value, const QString &symbol = QString(), int precision = -1) const;
    /*!
        \overload

        Returns \a value formatted as a currency amount with the locale's
        currency symbol.
    */
    QString toCurrencyString(int value, const QString &symbol = QString()) const { return toCurrencyString(static_cast<qlonglong>(value), symbol); }
    /*!
        \overload

        Returns \a value formatted as a currency amount with the locale's
        currency symbol.
    */
    QString toCurrencyString(uint value, const QString &symbol = QString()) const { return toCurrencyString(static_cast<qulonglong>(value), symbol); }
    QString quoteString(QStringView str, QuotationStyle style = StandardQuotation) const;
    /*!
        \overload

        Returns the UTF-8 string \a str enclosed in the quotation marks of
        \a style.
    */
    QString quoteString(const char *str, QuotationStyle style = StandardQuotation) const { return quoteString(QString::fromUtf8(str), style); }
    QStringList uiLanguages(TagSeparator separator = TagSeparator::Dash) const;
    QLocale collation() const;

    friend bool operator==(const QLocale &lhs, const QLocale &rhs) { return lhs.equals(rhs); }
    friend bool operator!=(const QLocale &lhs, const QLocale &rhs) { return !lhs.equals(rhs); }

private:
    bool equals(const QLocale &other) const;

    const QLocaleRow *m_row = nullptr;
    Language m_language = C;
    Script m_script = LatinScript;
    Territory m_territory = AnyTerritory;
    NumberOptions m_options;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QLocale::NumberOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QLocale::DataSizeFormats)
Q_DECLARE_OPERATORS_FOR_FLAGS(QLocale::LanguageCodeTypes)

std::size_t qHash(const QLocale &key, std::size_t seed = 0) noexcept;
QDebug operator<<(QDebug dbg, const QLocale &l);
