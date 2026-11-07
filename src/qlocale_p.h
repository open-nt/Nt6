// MIT License - see LICENSE
#pragma once

#include <QtCore/qlocale.h>

#include <vector>

// One entry of the locale table. Strings are UTF-8; the name lists are separated by '|', the days
// start with Monday, and a null standalone list means the same list as the format one.
struct QLocaleRow {
    QLocale::Language language = QLocale::C;
    QLocale::Script script = QLocale::LatinScript;
    QLocale::Territory territory = QLocale::AnyTerritory;

    const char *decimal = ".";
    const char *group = ",";
    const char *minus = "-";
    const char *plus = "+";
    const char *exponent = "e";
    const char *percent = "%";

    const char *dateLong = "";
    const char *dateShort = "";
    const char *timeLong = "";
    const char *timeShort = "";
    const char *dateTimeJoinLong = " ";
    const char *dateTimeJoinShort = " ";

    const char *daysLong = "";
    const char *daysShort = "";
    const char *daysNarrow = "";
    const char *monthsLong = "";
    const char *monthsShort = "";
    const char *monthsNarrow = "";
    const char *standaloneDaysLong = nullptr;
    const char *standaloneDaysShort = nullptr;
    const char *standaloneDaysNarrow = nullptr;
    const char *standaloneMonthsLong = nullptr;
    const char *standaloneMonthsShort = nullptr;
    const char *standaloneMonthsNarrow = nullptr;

    const char *am = "AM";
    const char *pm = "PM";

    const char *currencySymbol = "";
    const char *currencyIso = "";
    const char *currencyName = "";
    int currencyDigits = 2;
    // 0: symbol before the amount; 1: symbol before the amount, with a no-break space;
    // 2: symbol after the amount, with a no-break space.
    int currencyPosition = 0;

    const char *quoteStart = "\xe2\x80\x9c";
    const char *quoteEnd = "\xe2\x80\x9d";
    const char *altQuoteStart = "\xe2\x80\x98";
    const char *altQuoteEnd = "\xe2\x80\x99";

    const char *listSeparator = ", ";
    const char *listEnd = ", and ";
    const char *listPair = " and ";

    const char *nativeLanguage = "";
    const char *nativeTerritory = "";

    int firstDay = Qt::Monday;
    int weekendStart = Qt::Saturday;
    int weekendEnd = Qt::Sunday;
    QLocale::MeasurementSystem measurement = QLocale::MetricSystem;
};

struct QLocaleNameEntry {
    int value;
    const char *code;  // ISO 639-1 for languages, ISO 3166 alpha-2 for territories, ISO 15924 for scripts
    const char *code3; // ISO 639-2/3 code for languages, otherwise null
    const char *name;
};

namespace QtPrivate {

const std::vector<QLocaleRow> &localeRows();
const std::vector<QLocaleNameEntry> &languageNames();
const std::vector<QLocaleNameEntry> &territoryNames();
const std::vector<QLocaleNameEntry> &scriptNames();

} // namespace QtPrivate
