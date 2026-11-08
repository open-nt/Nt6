// MIT License - see LICENSE
#pragma once

#include <QtCore/qdatetime.h>
#include <QtCore/qflags.h>
#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qstringview.h>

struct QLocaleData;

/*!
    \class QLocale
    \brief The QLocale class converts between numbers, dates and their string representations in various languages.

    A QLocale is initialized with a language, an optional script and a
    territory, or with a name such as \c{"de_DE"}. It formats and parses
    numbers, dates and times, currency amounts and lists the way that locale
    writes them:

    \code
    QLocale german(QLocale::German, QLocale::Germany);
    QString s = german.toString(1234567.891, 'f', 2);   // "1.234.567,89"
    double d = german.toDouble(u"1.234,5"_s);           // 1234.5
    QString date = german.toString(QDate(2024, 5, 1));  // "Mittwoch, 1. Mai 2024"
    \endcode

    The default-constructed QLocale is the default locale, which is the
    system locale (taken from the \c LC_ALL, \c LC_NUMERIC or \c LANG
    environment variables) unless setDefault() was called. QLocale::c() is
    the "C" locale, which formats numbers like QString::number().

    \differences
    Nt has no CLDR database. It has data for about thirty common locales
    (English, German, French, Spanish, Italian, Dutch, Portuguese, Swedish,
    Norwegian Bokmål, Danish, Finnish, Polish, Czech, Russian, Ukrainian,
    Greek, Turkish, Japanese, Chinese and Korean, in their main
    territories); any other locale behaves as the C locale. The Language,
    Territory and Script enums list only a selection of values, whose
    numeric values differ from Qt's. All locales use the ASCII digits and the
    exponent character \c{e}. Day periods, era and calendar systems other
    than Gregorian, the \c LanguageCodeType arguments, \c{toCurrencyString()}
    for \c float, \c timeZone and the time-zone format tokens other than
    \c{t} are not supported.
    \enddifferences

    \sa QString::arg(), QCollator, QDate, QDateTime
*/
class QLocale {
public:
    /*!
        \enum QLocale::Language

        The language of a locale. Locale data exists only for some of them;
        see the class description.

        \value AnyLanguage Any language; used when matching locales.
        \value C The "C" locale.
        \value Afrikaans Afrikaans.
        \value Albanian Albanian.
        \value Arabic Arabic.
        \value Armenian Armenian.
        \value Basque Basque.
        \value Belarusian Belarusian.
        \value Bengali Bengali.
        \value Bosnian Bosnian.
        \value Bulgarian Bulgarian.
        \value Catalan Catalan.
        \value Chinese Chinese.
        \value Croatian Croatian.
        \value Czech Czech.
        \value Danish Danish.
        \value Dutch Dutch.
        \value English English.
        \value Esperanto Esperanto.
        \value Estonian Estonian.
        \value Filipino Filipino.
        \value Finnish Finnish.
        \value French French.
        \value Galician Galician.
        \value Georgian Georgian.
        \value German German.
        \value Greek Greek.
        \value Hebrew Hebrew.
        \value Hindi Hindi.
        \value Hungarian Hungarian.
        \value Icelandic Icelandic.
        \value Indonesian Indonesian.
        \value Irish Irish.
        \value Italian Italian.
        \value Japanese Japanese.
        \value Kazakh Kazakh.
        \value Korean Korean.
        \value Latin Latin.
        \value Latvian Latvian.
        \value Lithuanian Lithuanian.
        \value Macedonian Macedonian.
        \value Malay Malay.
        \value Maltese Maltese.
        \value NorwegianBokmal Norwegian Bokmål.
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
        \value Vietnamese Vietnamese.
        \value Welsh Welsh.
        \value Norwegian Same as NorwegianBokmal.
        \omitvalue LastLanguage
    */
    enum Language : ushort {
        AnyLanguage = 0, C = 1, Afrikaans, Albanian, Arabic, Armenian, Basque, Belarusian, Bengali, Bosnian, Bulgarian, Catalan,
        Chinese, Croatian, Czech, Danish, Dutch, English, Esperanto, Estonian, Filipino, Finnish, French, Galician, Georgian, German,
        Greek, Hebrew, Hindi, Hungarian, Icelandic, Indonesian, Irish, Italian, Japanese, Kazakh, Korean, Latin, Latvian, Lithuanian,
        Macedonian, Malay, Maltese, NorwegianBokmal, NorwegianNynorsk, Persian, Polish, Portuguese, Romanian, Russian, Serbian, Slovak,
        Slovenian, Spanish, Swahili, Swedish, Tamil, Thai, Turkish, Ukrainian, Urdu, Vietnamese, Welsh,
        Norwegian = NorwegianBokmal, LastLanguage = Welsh
    };

    /*!
        \enum QLocale::Script

        The script in which a language is written.

        \value AnyScript Any script; used when matching locales.
        \value ArabicScript Arabic.
        \value ArmenianScript Armenian.
        \value BengaliScript Bengali.
        \value CyrillicScript Cyrillic.
        \value DevanagariScript Devanagari.
        \value GeorgianScript Georgian.
        \value GreekScript Greek.
        \value HebrewScript Hebrew.
        \value JapaneseScript Japanese.
        \value KoreanScript Korean.
        \value LatinScript Latin.
        \value SimplifiedHanScript Simplified Han.
        \value TamilScript Tamil.
        \value ThaiScript Thai.
        \value TraditionalHanScript Traditional Han.
        \value SimplifiedChineseScript Same as SimplifiedHanScript.
        \value TraditionalChineseScript Same as TraditionalHanScript.
        \omitvalue LastScript
    */
    enum Script : ushort {
        AnyScript = 0, ArabicScript, ArmenianScript, BengaliScript, CyrillicScript, DevanagariScript, GeorgianScript, GreekScript,
        HebrewScript, JapaneseScript, KoreanScript, LatinScript, SimplifiedHanScript, TamilScript, ThaiScript, TraditionalHanScript,
        SimplifiedChineseScript = SimplifiedHanScript, TraditionalChineseScript = TraditionalHanScript, LastScript = TraditionalHanScript
    };

    /*!
        \enum QLocale::Territory
        \since 6.2

        The territory (country or region) of a locale.

        \value AnyTerritory Any territory; used when matching locales.
        \value Argentina Argentina.
        \value Australia Australia.
        \value Austria Austria.
        \value Belgium Belgium.
        \value Brazil Brazil.
        \value Bulgaria Bulgaria.
        \value Canada Canada.
        \value Chile Chile.
        \value China China.
        \value Colombia Colombia.
        \value Croatia Croatia.
        \value Czechia Czechia.
        \value Denmark Denmark.
        \value Egypt Egypt.
        \value Estonia Estonia.
        \value Finland Finland.
        \value France France.
        \value Germany Germany.
        \value Greece Greece.
        \value HongKong Hong Kong.
        \value Hungary Hungary.
        \value Iceland Iceland.
        \value India India.
        \value Indonesia Indonesia.
        \value Iran Iran.
        \value Ireland Ireland.
        \value Israel Israel.
        \value Italy Italy.
        \value Japan Japan.
        \value Latvia Latvia.
        \value Lithuania Lithuania.
        \value Luxembourg Luxembourg.
        \value Malaysia Malaysia.
        \value Mexico Mexico.
        \value Netherlands Netherlands.
        \value NewZealand New Zealand.
        \value Norway Norway.
        \value Pakistan Pakistan.
        \value Peru Peru.
        \value Philippines Philippines.
        \value Poland Poland.
        \value Portugal Portugal.
        \value Romania Romania.
        \value Russia Russia.
        \value SaudiArabia Saudi Arabia.
        \value Serbia Serbia.
        \value Singapore Singapore.
        \value Slovakia Slovakia.
        \value Slovenia Slovenia.
        \value SouthAfrica South Africa.
        \value SouthKorea South Korea.
        \value Spain Spain.
        \value Sweden Sweden.
        \value Switzerland Switzerland.
        \value Taiwan Taiwan.
        \value Thailand Thailand.
        \value Turkey Turkey.
        \value Ukraine Ukraine.
        \value UnitedKingdom United Kingdom.
        \value UnitedStates United States.
        \value Vietnam Vietnam.
        \value World The world, for languages without a main territory.
        \value AnyCountry Same as AnyTerritory.
        \value CzechRepublic Same as Czechia.
        \omitvalue LastTerritory
        \omitvalue LastCountry
    */
    enum Territory : ushort {
        AnyTerritory = 0, Argentina, Australia, Austria, Belgium, Brazil, Bulgaria, Canada, Chile, China, Colombia, Croatia, Czechia,
        Denmark, Egypt, Estonia, Finland, France, Germany, Greece, HongKong, Hungary, Iceland, India, Indonesia, Iran, Ireland, Israel,
        Italy, Japan, Latvia, Lithuania, Luxembourg, Malaysia, Mexico, Netherlands, NewZealand, Norway, Pakistan, Peru, Philippines,
        Poland, Portugal, Romania, Russia, SaudiArabia, Serbia, Singapore, Slovakia, Slovenia, SouthAfrica, SouthKorea, Spain, Sweden,
        Switzerland, Taiwan, Thailand, Turkey, Ukraine, UnitedKingdom, UnitedStates, Vietnam, World,
        AnyCountry = AnyTerritory, CzechRepublic = Czechia, LastTerritory = World, LastCountry = World
    };
    /*! \typedef QLocale::Country

        Same as Territory, the name used before Qt 6.2. */
    using Country = Territory;

    /*!
        \enum QLocale::MeasurementSystem

        \value MetricSystem The metric system.
        \value ImperialUSSystem The imperial system, as used in the United States.
        \value ImperialUKSystem The imperial system, as used in the United Kingdom.
        \value ImperialSystem Same as ImperialUSSystem.
    */
    enum MeasurementSystem { MetricSystem, ImperialUSSystem, ImperialUKSystem, ImperialSystem = ImperialUSSystem };

    /*!
        \enum QLocale::FormatType

        \value LongFormat The long form, such as "Wednesday" or "Wednesday, 1 May 2024".
        \value ShortFormat The short form, such as "Wed" or "01/05/2024".
        \value NarrowFormat The narrowest form, such as "W". For date and time
               formats it is the same as ShortFormat.
    */
    enum FormatType { LongFormat, ShortFormat, NarrowFormat };

    /*!
        \enum QLocale::NumberOption

        \value DefaultNumberOptions Group separators are written and accepted.
               This is the default for all locales except C.
        \value OmitGroupSeparator Numbers are written without group separators.
               This is the default of the C locale.
        \value RejectGroupSeparator Group separators are not accepted when parsing.
        \value OmitLeadingZeroInExponent The exponent is written without a
               leading zero, as in "1e+5" instead of "1e+05".
        \value RejectLeadingZeroInExponent An exponent with a leading zero is
               not accepted when parsing.
        \value IncludeTrailingZeroesAfterDot With the 'g' format, trailing
               zeros up to the precision are written.
        \value RejectTrailingZeroesAfterDot Trailing zeros after the decimal
               point are not accepted when parsing.
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
        \since 5.7

        \value FloatingPointShortest As a precision, writes the shortest
               representation that reads back as the same value.
    */
    enum FloatingPointPrecisionOption { FloatingPointShortest = -128 };

    /*!
        \enum QLocale::CurrencySymbolFormat

        \value CurrencyIsoCode The ISO 4217 code, such as "EUR".
        \value CurrencySymbol The symbol, such as "€".
        \value CurrencyDisplayName The name, such as "Euro".
    */
    enum CurrencySymbolFormat { CurrencyIsoCode, CurrencySymbol, CurrencyDisplayName };

    /*!
        \enum QLocale::DataSizeFormat
        \since 5.10

        \value DataSizeBase1000 Use powers of 1000 instead of 1024.
        \value DataSizeSIQuantifiers Use the SI prefixes (k, M, G) instead of
               the IEC ones (Ki, Mi, Gi).
        \value DataSizeIecFormat Powers of 1024 with IEC prefixes, such as "1.00 KiB".
        \value DataSizeTraditionalFormat Powers of 1024 with SI prefixes, such as "1.00 kB".
        \value DataSizeSIFormat Powers of 1000 with SI prefixes.
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
        \enum QLocale::QuotationStyle
        \since 4.8

        \value StandardQuotation The usual quotation marks of the locale.
        \value AlternateQuotation The marks used for a quotation inside a quotation.
    */
    enum QuotationStyle { StandardQuotation, AlternateQuotation };

    /*!
        \enum QLocale::TagSeparator
        \since 6.7

        \value Dash The separator \c{-}, as in BCP 47 names.
        \value Underscore The separator \c{_}, as in POSIX locale names.
    */
    enum class TagSeparator : char { Dash = '-', Underscore = '_' };

    QLocale();
    explicit QLocale(const QString &name);
    explicit QLocale(QStringView name);
    QLocale(Language language, Territory territory = AnyTerritory);
    QLocale(Language language, Script script, Territory territory);
    QLocale(const QLocale &other) noexcept = default;
    QLocale &operator=(const QLocale &other) noexcept = default;
    ~QLocale() = default;
    /*!
        Swaps this locale with \a other.
    */
    void swap(QLocale &other) noexcept {
        std::swap(d, other.d);
        std::swap(m_options, other.m_options);
    }

    Language language() const;
    Script script() const;
    Territory territory() const;
    /*!
        Same as territory().
    */
    Country country() const { return territory(); }
    QString name(TagSeparator separator = TagSeparator::Underscore) const;
    QString bcp47Name(TagSeparator separator = TagSeparator::Dash) const;
    QString nativeLanguageName() const;
    QString nativeTerritoryName() const;
    /*!
        Same as nativeTerritoryName().
    */
    QString nativeCountryName() const { return nativeTerritoryName(); }

    QString decimalPoint() const;
    QString groupSeparator() const;
    QString percent() const;
    QString zeroDigit() const;
    QString negativeSign() const;
    QString positiveSign() const;
    QString exponential() const;

    QString toString(qlonglong i) const;
    QString toString(qulonglong i) const;
    /*! \overload Returns \a i formatted as a number. */
    QString toString(long i) const { return toString(static_cast<qlonglong>(i)); }
    /*! \overload Returns \a i formatted as a number. */
    QString toString(ulong i) const { return toString(static_cast<qulonglong>(i)); }
    /*! \overload Returns \a i formatted as a number. */
    QString toString(int i) const { return toString(static_cast<qlonglong>(i)); }
    /*! \overload Returns \a i formatted as a number. */
    QString toString(uint i) const { return toString(static_cast<qulonglong>(i)); }
    /*! \overload Returns \a i formatted as a number. */
    QString toString(short i) const { return toString(static_cast<qlonglong>(i)); }
    /*! \overload Returns \a i formatted as a number. */
    QString toString(ushort i) const { return toString(static_cast<qulonglong>(i)); }
    QString toString(double f, char format = 'g', int precision = 6) const;
    /*! \overload Returns \a f formatted with \a format and \a precision as for the \c double overload. */
    QString toString(float f, char format = 'g', int precision = 6) const { return toString(static_cast<double>(f), format, precision); }
    QString toString(QDate date, const QString &format) const;
    QString toString(QTime time, const QString &format) const;
    QString toString(const QDateTime &dateTime, const QString &format) const;
    QString toString(QDate date, FormatType format = LongFormat) const;
    QString toString(QTime time, FormatType format = LongFormat) const;
    QString toString(const QDateTime &dateTime, FormatType format = LongFormat) const;

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

    QDate toDate(const QString &string, FormatType format = LongFormat) const;
    QDate toDate(const QString &string, const QString &format) const;
    QTime toTime(const QString &string, FormatType format = LongFormat) const;
    QTime toTime(const QString &string, const QString &format) const;
    QDateTime toDateTime(const QString &string, FormatType format = LongFormat) const;
    QDateTime toDateTime(const QString &string, const QString &format) const;

    QString dateFormat(FormatType format = LongFormat) const;
    QString timeFormat(FormatType format = LongFormat) const;
    QString dateTimeFormat(FormatType format = LongFormat) const;
    QString monthName(int month, FormatType format = LongFormat) const;
    QString standaloneMonthName(int month, FormatType format = LongFormat) const;
    QString dayName(int day, FormatType format = LongFormat) const;
    QString standaloneDayName(int day, FormatType format = LongFormat) const;
    QString amText() const;
    QString pmText() const;
    Qt::DayOfWeek firstDayOfWeek() const;
    QList<Qt::DayOfWeek> weekdays() const;
    MeasurementSystem measurementSystem() const;
    Qt::LayoutDirection textDirection() const;

    QString toUpper(const QString &str) const;
    QString toLower(const QString &str) const;
    QString currencySymbol(CurrencySymbolFormat format = CurrencySymbol) const;
    QString toCurrencyString(qlonglong value, const QString &symbol = QString()) const;
    QString toCurrencyString(qulonglong value, const QString &symbol = QString()) const;
    /*! \overload Returns \a value as a currency amount with \a symbol. */
    QString toCurrencyString(int value, const QString &symbol = QString()) const { return toCurrencyString(static_cast<qlonglong>(value), symbol); }
    /*! \overload Returns \a value as a currency amount with \a symbol. */
    QString toCurrencyString(uint value, const QString &symbol = QString()) const { return toCurrencyString(static_cast<qulonglong>(value), symbol); }
    QString toCurrencyString(double value, const QString &symbol = QString(), int precision = -1) const;
    QString formattedDataSize(qint64 bytes, int precision = 2, DataSizeFormats format = DataSizeIecFormat) const;
    QString quoteString(QStringView str, QuotationStyle style = StandardQuotation) const;
    QString createSeparatedList(const QStringList &list) const;
    QStringList uiLanguages(TagSeparator separator = TagSeparator::Dash) const;
    QLocale collation() const;

    void setNumberOptions(NumberOptions options);
    NumberOptions numberOptions() const;

    static QLocale c();
    static QLocale system();
    static void setDefault(const QLocale &locale);
    static QList<QLocale> matchingLocales(Language language, Script script, Territory territory);
    static QString languageToString(Language language);
    static QString territoryToString(Territory territory);
    /*!
        Same as territoryToString(\a country).
    */
    static QString countryToString(Country country) { return territoryToString(country); }
    static QString scriptToString(Script script);
    static QString languageToCode(Language language);
    static QString territoryToCode(Territory territory);
    static QString scriptToCode(Script script);
    static Language codeToLanguage(QStringView languageCode) noexcept;
    static Territory codeToTerritory(QStringView territoryCode) noexcept;
    static Script codeToScript(QStringView scriptCode) noexcept;

    friend bool operator==(const QLocale &a, const QLocale &b) noexcept { return a.d == b.d && a.m_options == b.m_options; }
    friend bool operator!=(const QLocale &a, const QLocale &b) noexcept { return !(a == b); }

private:
    explicit QLocale(const QLocaleData *data, NumberOptions options);
    const QLocaleData *d;
    NumberOptions m_options;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QLocale::NumberOptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(QLocale::DataSizeFormats)

std::size_t qHash(const QLocale &key, std::size_t seed = 0) noexcept;
