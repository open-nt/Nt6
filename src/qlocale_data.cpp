// MIT License - see LICENSE
#include "qlocale_p.h"

namespace {

QLocaleRow englishBase() {
    QLocaleRow r;
    r.language = QLocale::English;
    r.script = QLocale::LatinScript;
    r.territory = QLocale::UnitedStates;
    r.dateLong = "dddd, MMMM d, yyyy";
    r.dateShort = "M/d/yy";
    r.timeLong = "h:mm:ss AP t";
    r.timeShort = "h:mm AP";
    r.dateTimeJoinLong = " 'at' ";
    r.dateTimeJoinShort = ", ";
    r.daysLong = "Monday|Tuesday|Wednesday|Thursday|Friday|Saturday|Sunday";
    r.daysShort = "Mon|Tue|Wed|Thu|Fri|Sat|Sun";
    r.daysNarrow = "M|T|W|T|F|S|S";
    r.monthsLong = "January|February|March|April|May|June|July|August|September|October|November|December";
    r.monthsShort = "Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec";
    r.monthsNarrow = "J|F|M|A|M|J|J|A|S|O|N|D";
    r.currencySymbol = "$";
    r.currencyIso = "USD";
    r.currencyName = "US Dollar";
    r.nativeLanguage = "English";
    r.nativeTerritory = "United States";
    r.firstDay = Qt::Sunday;
    r.measurement = QLocale::ImperialUSSystem;
    return r;
}

std::vector<QLocaleRow> buildRows() {
    std::vector<QLocaleRow> rows;

    // C: English text with the conventions of POSIX.
    {
        QLocaleRow r = englishBase();
        r.language = QLocale::C;
        r.territory = QLocale::AnyTerritory;
        r.dateLong = "dddd, d MMMM yyyy";
        r.dateShort = "dd/MM/yyyy";
        r.timeLong = "HH:mm:ss t";
        r.timeShort = "HH:mm:ss";
        r.dateTimeJoinLong = " ";
        r.dateTimeJoinShort = " ";
        r.currencySymbol = "";
        r.currencyIso = "";
        r.currencyName = "";
        r.nativeLanguage = "American English";
        r.nativeTerritory = "Default";
        r.firstDay = Qt::Monday;
        r.measurement = QLocale::MetricSystem;
        rows.push_back(r);
    }
    // en_US
    rows.push_back(englishBase());
    // en_GB
    {
        QLocaleRow r = englishBase();
        r.territory = QLocale::UnitedKingdom;
        r.dateLong = "dddd, d MMMM yyyy";
        r.dateShort = "dd/MM/yyyy";
        r.timeLong = "HH:mm:ss t";
        r.timeShort = "HH:mm";
        r.am = "am";
        r.pm = "pm";
        r.currencySymbol = "\xc2\xa3";
        r.currencyIso = "GBP";
        r.currencyName = "British Pound";
        r.quoteStart = "\xe2\x80\x98";
        r.quoteEnd = "\xe2\x80\x99";
        r.altQuoteStart = "\xe2\x80\x9c";
        r.altQuoteEnd = "\xe2\x80\x9d";
        r.listEnd = " and ";
        r.nativeTerritory = "United Kingdom";
        r.firstDay = Qt::Monday;
        r.measurement = QLocale::ImperialUKSystem;
        rows.push_back(r);
    }
    // en_AU
    {
        QLocaleRow r = englishBase();
        r.territory = QLocale::Australia;
        r.dateLong = "dddd, d MMMM yyyy";
        r.dateShort = "d/M/yy";
        r.timeLong = "h:mm:ss ap t";
        r.timeShort = "h:mm ap";
        r.am = "am";
        r.pm = "pm";
        r.currencyIso = "AUD";
        r.currencyName = "Australian Dollar";
        r.listEnd = " and ";
        r.nativeTerritory = "Australia";
        r.firstDay = Qt::Monday;
        r.measurement = QLocale::MetricSystem;
        rows.push_back(r);
    }
    // de_DE
    {
        QLocaleRow r;
        r.language = QLocale::German;
        r.territory = QLocale::Germany;
        r.decimal = ",";
        r.group = ".";
        r.dateLong = "dddd, d. MMMM yyyy";
        r.dateShort = "dd.MM.yy";
        r.timeLong = "HH:mm:ss t";
        r.timeShort = "HH:mm";
        r.dateTimeJoinLong = " 'um' ";
        r.dateTimeJoinShort = ", ";
        r.daysLong = "Montag|Dienstag|Mittwoch|Donnerstag|Freitag|Samstag|Sonntag";
        r.daysShort = "Mo.|Di.|Mi.|Do.|Fr.|Sa.|So.";
        r.daysNarrow = "M|D|M|D|F|S|S";
        r.standaloneDaysShort = "Mo|Di|Mi|Do|Fr|Sa|So";
        r.monthsLong = "Januar|Februar|M\xc3\xa4rz|April|Mai|Juni|Juli|August|September|Oktober|November|Dezember";
        r.monthsShort = "Jan.|Feb.|M\xc3\xa4rz|Apr.|Mai|Juni|Juli|Aug.|Sept.|Okt.|Nov.|Dez.";
        r.standaloneMonthsShort = "Jan|Feb|M\xc3\xa4r|Apr|Mai|Jun|Jul|Aug|Sep|Okt|Nov|Dez";
        r.monthsNarrow = "J|F|M|A|M|J|J|A|S|O|N|D";
        r.currencySymbol = "\xe2\x82\xac";
        r.currencyIso = "EUR";
        r.currencyName = "Euro";
        r.currencyPosition = 2;
        r.quoteStart = "\xe2\x80\x9e";
        r.quoteEnd = "\xe2\x80\x9c";
        r.altQuoteStart = "\xe2\x80\x9a";
        r.altQuoteEnd = "\xe2\x80\x98";
        r.listEnd = " und ";
        r.listPair = " und ";
        r.nativeLanguage = "Deutsch";
        r.nativeTerritory = "Deutschland";
        rows.push_back(r);
    }
    // fr_FR
    {
        QLocaleRow r;
        r.language = QLocale::French;
        r.territory = QLocale::France;
        r.decimal = ",";
        r.group = "\xe2\x80\xaf";
        r.dateLong = "dddd d MMMM yyyy";
        r.dateShort = "dd/MM/yyyy";
        r.timeLong = "HH:mm:ss t";
        r.timeShort = "HH:mm";
        r.dateTimeJoinLong = " '\xc3\xa0' ";
        r.dateTimeJoinShort = " ";
        r.daysLong = "lundi|mardi|mercredi|jeudi|vendredi|samedi|dimanche";
        r.daysShort = "lun.|mar.|mer.|jeu.|ven.|sam.|dim.";
        r.daysNarrow = "L|M|M|J|V|S|D";
        r.monthsLong = "janvier|f\xc3\xa9vrier|mars|avril|mai|juin|juillet|ao\xc3\xbbt|septembre|octobre|novembre|d\xc3\xa9" "cembre";
        r.monthsShort = "janv.|f\xc3\xa9vr.|mars|avr.|mai|juin|juil.|ao\xc3\xbbt|sept.|oct.|nov.|d\xc3\xa9" "c.";
        r.monthsNarrow = "J|F|M|A|M|J|J|A|S|O|N|D";
        r.currencySymbol = "\xe2\x82\xac";
        r.currencyIso = "EUR";
        r.currencyName = "Euro";
        r.currencyPosition = 2;
        r.quoteStart = "\xc2\xab";
        r.quoteEnd = "\xc2\xbb";
        r.altQuoteStart = "\xe2\x80\x9c";
        r.altQuoteEnd = "\xe2\x80\x9d";
        r.listSeparator = ", ";
        r.listEnd = " et ";
        r.listPair = " et ";
        r.nativeLanguage = "fran\xc3\xa7" "ais";
        r.nativeTerritory = "France";
        rows.push_back(r);
    }
    // es_ES
    {
        QLocaleRow r;
        r.language = QLocale::Spanish;
        r.territory = QLocale::Spain;
        r.decimal = ",";
        r.group = ".";
        r.dateLong = "dddd, d 'de' MMMM 'de' yyyy";
        r.dateShort = "d/M/yy";
        r.timeLong = "H:mm:ss t";
        r.timeShort = "H:mm";
        r.dateTimeJoinLong = ", ";
        r.dateTimeJoinShort = ", ";
        r.daysLong = "lunes|martes|mi\xc3\xa9rcoles|jueves|viernes|s\xc3\xa1" "bado|domingo";
        r.daysShort = "lun|mar|mi\xc3\xa9|jue|vie|s\xc3\xa1" "b|dom";
        r.daysNarrow = "L|M|X|J|V|S|D";
        r.monthsLong = "enero|febrero|marzo|abril|mayo|junio|julio|agosto|septiembre|octubre|noviembre|diciembre";
        r.monthsShort = "ene|feb|mar|abr|may|jun|jul|ago|sept|oct|nov|dic";
        r.monthsNarrow = "E|F|M|A|M|J|J|A|S|O|N|D";
        r.am = "a.\xc2\xa0m.";
        r.pm = "p.\xc2\xa0m.";
        r.currencySymbol = "\xe2\x82\xac";
        r.currencyIso = "EUR";
        r.currencyName = "Euro";
        r.currencyPosition = 2;
        r.altQuoteStart = "\xc2\xab";
        r.altQuoteEnd = "\xc2\xbb";
        r.listSeparator = ", ";
        r.listEnd = " y ";
        r.listPair = " y ";
        r.nativeLanguage = "espa\xc3\xb1ol";
        r.nativeTerritory = "Espa\xc3\xb1" "a";
        rows.push_back(r);
    }
    // it_IT
    {
        QLocaleRow r;
        r.language = QLocale::Italian;
        r.territory = QLocale::Italy;
        r.decimal = ",";
        r.group = ".";
        r.dateLong = "dddd d MMMM yyyy";
        r.dateShort = "dd/MM/yy";
        r.timeLong = "HH:mm:ss t";
        r.timeShort = "HH:mm";
        r.dateTimeJoinLong = " 'alle' ";
        r.dateTimeJoinShort = ", ";
        r.daysLong = "luned\xc3\xac|marted\xc3\xac|mercoled\xc3\xac|gioved\xc3\xac|venerd\xc3\xac|sabato|domenica";
        r.daysShort = "lun|mar|mer|gio|ven|sab|dom";
        r.daysNarrow = "L|M|M|G|V|S|D";
        r.monthsLong = "gennaio|febbraio|marzo|aprile|maggio|giugno|luglio|agosto|settembre|ottobre|novembre|dicembre";
        r.monthsShort = "gen|feb|mar|apr|mag|giu|lug|ago|set|ott|nov|dic";
        r.monthsNarrow = "G|F|M|A|M|G|L|A|S|O|N|D";
        r.currencySymbol = "\xe2\x82\xac";
        r.currencyIso = "EUR";
        r.currencyName = "Euro";
        r.currencyPosition = 2;
        r.listSeparator = ", ";
        r.listEnd = " e ";
        r.listPair = " e ";
        r.nativeLanguage = "italiano";
        r.nativeTerritory = "Italia";
        rows.push_back(r);
    }
    // pt_BR
    {
        QLocaleRow r;
        r.language = QLocale::Portuguese;
        r.territory = QLocale::Brazil;
        r.decimal = ",";
        r.group = ".";
        r.dateLong = "dddd, d 'de' MMMM 'de' yyyy";
        r.dateShort = "dd/MM/yyyy";
        r.timeLong = "HH:mm:ss t";
        r.timeShort = "HH:mm";
        r.dateTimeJoinLong = " '\xc3\xa0s' ";
        r.dateTimeJoinShort = " ";
        r.daysLong = "segunda-feira|ter\xc3\xa7" "a-feira|quarta-feira|quinta-feira|sexta-feira|s\xc3\xa1" "bado|domingo";
        r.daysShort = "seg.|ter.|qua.|qui.|sex.|s\xc3\xa1" "b.|dom.";
        r.daysNarrow = "S|T|Q|Q|S|S|D";
        r.monthsLong = "janeiro|fevereiro|mar\xc3\xa7o|abril|maio|junho|julho|agosto|setembro|outubro|novembro|dezembro";
        r.monthsShort = "jan.|fev.|mar.|abr.|mai.|jun.|jul.|ago.|set.|out.|nov.|dez.";
        r.monthsNarrow = "J|F|M|A|M|J|J|A|S|O|N|D";
        r.currencySymbol = "R$";
        r.currencyIso = "BRL";
        r.currencyName = "Brazilian Real";
        r.currencyPosition = 1;
        r.listSeparator = ", ";
        r.listEnd = " e ";
        r.listPair = " e ";
        r.nativeLanguage = "portugu\xc3\xaas";
        r.nativeTerritory = "Brasil";
        r.firstDay = Qt::Sunday;
        rows.push_back(r);
    }
    // nl_NL
    {
        QLocaleRow r;
        r.language = QLocale::Dutch;
        r.territory = QLocale::Netherlands;
        r.decimal = ",";
        r.group = ".";
        r.dateLong = "dddd d MMMM yyyy";
        r.dateShort = "dd-MM-yy";
        r.timeLong = "HH:mm:ss t";
        r.timeShort = "HH:mm";
        r.dateTimeJoinLong = " 'om' ";
        r.dateTimeJoinShort = " ";
        r.daysLong = "maandag|dinsdag|woensdag|donderdag|vrijdag|zaterdag|zondag";
        r.daysShort = "ma|di|wo|do|vr|za|zo";
        r.daysNarrow = "M|D|W|D|V|Z|Z";
        r.monthsLong = "januari|februari|maart|april|mei|juni|juli|augustus|september|oktober|november|december";
        r.monthsShort = "jan.|feb.|mrt.|apr.|mei|jun.|jul.|aug.|sep.|okt.|nov.|dec.";
        r.monthsNarrow = "J|F|M|A|M|J|J|A|S|O|N|D";
        r.am = "a.m.";
        r.pm = "p.m.";
        r.currencySymbol = "\xe2\x82\xac";
        r.currencyIso = "EUR";
        r.currencyName = "Euro";
        r.currencyPosition = 1;
        r.listSeparator = ", ";
        r.listEnd = " en ";
        r.listPair = " en ";
        r.nativeLanguage = "Nederlands";
        r.nativeTerritory = "Nederland";
        rows.push_back(r);
    }
    // sv_SE
    {
        QLocaleRow r;
        r.language = QLocale::Swedish;
        r.territory = QLocale::Sweden;
        r.decimal = ",";
        r.group = "\xc2\xa0";
        r.minus = "\xe2\x88\x92";
        r.dateLong = "dddd d MMMM yyyy";
        r.dateShort = "yyyy-MM-dd";
        r.timeLong = "HH:mm:ss t";
        r.timeShort = "HH:mm";
        r.dateTimeJoinLong = " 'kl'. ";
        r.dateTimeJoinShort = " ";
        r.daysLong = "m\xc3\xa5ndag|tisdag|onsdag|torsdag|fredag|l\xc3\xb6rdag|s\xc3\xb6ndag";
        r.daysShort = "m\xc3\xa5n|tis|ons|tors|fre|l\xc3\xb6r|s\xc3\xb6n";
        r.daysNarrow = "M|T|O|T|F|L|S";
        r.monthsLong = "januari|februari|mars|april|maj|juni|juli|augusti|september|oktober|november|december";
        r.monthsShort = "jan.|feb.|mars|apr.|maj|juni|juli|aug.|sep.|okt.|nov.|dec.";
        r.monthsNarrow = "J|F|M|A|M|J|J|A|S|O|N|D";
        r.am = "fm";
        r.pm = "em";
        r.currencySymbol = "kr";
        r.currencyIso = "SEK";
        r.currencyName = "Swedish Krona";
        r.currencyPosition = 2;
        r.quoteStart = "\xe2\x80\x9d";
        r.quoteEnd = "\xe2\x80\x9d";
        r.altQuoteStart = "\xe2\x80\x99";
        r.altQuoteEnd = "\xe2\x80\x99";
        r.listSeparator = ", ";
        r.listEnd = " och ";
        r.listPair = " och ";
        r.nativeLanguage = "svenska";
        r.nativeTerritory = "Sverige";
        rows.push_back(r);
    }
    // ru_RU
    {
        QLocaleRow r;
        r.language = QLocale::Russian;
        r.script = QLocale::CyrillicScript;
        r.territory = QLocale::Russia;
        r.decimal = ",";
        r.group = "\xc2\xa0";
        r.dateLong = "dddd, d MMMM yyyy '\xd0\xb3'.";
        r.dateShort = "dd.MM.yyyy";
        r.timeLong = "HH:mm:ss t";
        r.timeShort = "HH:mm";
        r.dateTimeJoinLong = ", ";
        r.dateTimeJoinShort = ", ";
        r.daysLong = "\xd0\xbf\xd0\xbe\xd0\xbd\xd0\xb5\xd0\xb4\xd0\xb5\xd0\xbb\xd1\x8c\xd0\xbd\xd0\xb8\xd0\xba|\xd0\xb2\xd1\x82\xd0\xbe\xd1\x80\xd0\xbd\xd0\xb8\xd0\xba|"
                     "\xd1\x81\xd1\x80\xd0\xb5\xd0\xb4\xd0\xb0|\xd1\x87\xd0\xb5\xd1\x82\xd0\xb2\xd0\xb5\xd1\x80\xd0\xb3|\xd0\xbf\xd1\x8f\xd1\x82\xd0\xbd\xd0\xb8\xd1\x86\xd0\xb0|"
                     "\xd1\x81\xd1\x83\xd0\xb1\xd0\xb1\xd0\xbe\xd1\x82\xd0\xb0|\xd0\xb2\xd0\xbe\xd1\x81\xd0\xba\xd1\x80\xd0\xb5\xd1\x81\xd0\xb5\xd0\xbd\xd1\x8c\xd0\xb5";
        r.daysShort = "\xd0\xbf\xd0\xbd|\xd0\xb2\xd1\x82|\xd1\x81\xd1\x80|\xd1\x87\xd1\x82|\xd0\xbf\xd1\x82|\xd1\x81\xd0\xb1|\xd0\xb2\xd1\x81";
        r.daysNarrow = "\xd0\x9f|\xd0\x92|\xd0\xa1|\xd0\xa7|\xd0\x9f|\xd0\xa1|\xd0\x92";
        // Months in the genitive, as used in dates.
        r.monthsLong = "\xd1\x8f\xd0\xbd\xd0\xb2\xd0\xb0\xd1\x80\xd1\x8f|\xd1\x84\xd0\xb5\xd0\xb2\xd1\x80\xd0\xb0\xd0\xbb\xd1\x8f|\xd0\xbc\xd0\xb0\xd1\x80\xd1\x82\xd0\xb0|"
                       "\xd0\xb0\xd0\xbf\xd1\x80\xd0\xb5\xd0\xbb\xd1\x8f|\xd0\xbc\xd0\xb0\xd1\x8f|\xd0\xb8\xd1\x8e\xd0\xbd\xd1\x8f|\xd0\xb8\xd1\x8e\xd0\xbb\xd1\x8f|"
                       "\xd0\xb0\xd0\xb2\xd0\xb3\xd1\x83\xd1\x81\xd1\x82\xd0\xb0|\xd1\x81\xd0\xb5\xd0\xbd\xd1\x82\xd1\x8f\xd0\xb1\xd1\x80\xd1\x8f|"
                       "\xd0\xbe\xd0\xba\xd1\x82\xd1\x8f\xd0\xb1\xd1\x80\xd1\x8f|\xd0\xbd\xd0\xbe\xd1\x8f\xd0\xb1\xd1\x80\xd1\x8f|\xd0\xb4\xd0\xb5\xd0\xba\xd0\xb0\xd0\xb1\xd1\x80\xd1\x8f";
        r.standaloneMonthsLong = "\xd1\x8f\xd0\xbd\xd0\xb2\xd0\xb0\xd1\x80\xd1\x8c|\xd1\x84\xd0\xb5\xd0\xb2\xd1\x80\xd0\xb0\xd0\xbb\xd1\x8c|\xd0\xbc\xd0\xb0\xd1\x80\xd1\x82|"
                                 "\xd0\xb0\xd0\xbf\xd1\x80\xd0\xb5\xd0\xbb\xd1\x8c|\xd0\xbc\xd0\xb0\xd0\xb9|\xd0\xb8\xd1\x8e\xd0\xbd\xd1\x8c|\xd0\xb8\xd1\x8e\xd0\xbb\xd1\x8c|"
                                 "\xd0\xb0\xd0\xb2\xd0\xb3\xd1\x83\xd1\x81\xd1\x82|\xd1\x81\xd0\xb5\xd0\xbd\xd1\x82\xd1\x8f\xd0\xb1\xd1\x80\xd1\x8c|"
                                 "\xd0\xbe\xd0\xba\xd1\x82\xd1\x8f\xd0\xb1\xd1\x80\xd1\x8c|\xd0\xbd\xd0\xbe\xd1\x8f\xd0\xb1\xd1\x80\xd1\x8c|\xd0\xb4\xd0\xb5\xd0\xba\xd0\xb0\xd0\xb1\xd1\x80\xd1\x8c";
        r.monthsShort = "\xd1\x8f\xd0\xbd\xd0\xb2.|\xd1\x84\xd0\xb5\xd0\xb2\xd1\x80.|\xd0\xbc\xd0\xb0\xd1\x80.|\xd0\xb0\xd0\xbf\xd1\x80.|\xd0\xbc\xd0\xb0\xd1\x8f|"
                        "\xd0\xb8\xd1\x8e\xd0\xbd.|\xd0\xb8\xd1\x8e\xd0\xbb.|\xd0\xb0\xd0\xb2\xd0\xb3.|\xd1\x81\xd0\xb5\xd0\xbd\xd1\x82.|\xd0\xbe\xd0\xba\xd1\x82.|"
                        "\xd0\xbd\xd0\xbe\xd1\x8f\xd0\xb1.|\xd0\xb4\xd0\xb5\xd0\xba.";
        r.standaloneMonthsShort = "\xd1\x8f\xd0\xbd\xd0\xb2.|\xd1\x84\xd0\xb5\xd0\xb2\xd1\x80.|\xd0\xbc\xd0\xb0\xd1\x80\xd1\x82|\xd0\xb0\xd0\xbf\xd1\x80.|\xd0\xbc\xd0\xb0\xd0\xb9|"
                                  "\xd0\xb8\xd1\x8e\xd0\xbd\xd1\x8c|\xd0\xb8\xd1\x8e\xd0\xbb\xd1\x8c|\xd0\xb0\xd0\xb2\xd0\xb3.|\xd1\x81\xd0\xb5\xd0\xbd\xd1\x82.|\xd0\xbe\xd0\xba\xd1\x82.|"
                                  "\xd0\xbd\xd0\xbe\xd1\x8f\xd0\xb1.|\xd0\xb4\xd0\xb5\xd0\xba.";
        r.monthsNarrow = "\xd0\xaf|\xd0\xa4|\xd0\x9c|\xd0\x90|\xd0\x9c|\xd0\x98|\xd0\x98|\xd0\x90|\xd0\xa1|\xd0\x9e|\xd0\x9d|\xd0\x94";
        r.currencySymbol = "\xe2\x82\xbd";
        r.currencyIso = "RUB";
        r.currencyName = "Russian Ruble";
        r.currencyPosition = 2;
        r.quoteStart = "\xc2\xab";
        r.quoteEnd = "\xc2\xbb";
        r.altQuoteStart = "\xe2\x80\x9e";
        r.altQuoteEnd = "\xe2\x80\x9c";
        r.listSeparator = ", ";
        r.listEnd = " \xd0\xb8 ";
        r.listPair = " \xd0\xb8 ";
        r.nativeLanguage = "\xd1\x80\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9";
        r.nativeTerritory = "\xd0\xa0\xd0\xbe\xd1\x81\xd1\x81\xd0\xb8\xd1\x8f";
        rows.push_back(r);
    }
    // ja_JP
    {
        QLocaleRow r;
        r.language = QLocale::Japanese;
        r.script = QLocale::JapaneseScript;
        r.territory = QLocale::Japan;
        r.dateLong = "yyyy\xe5\xb9\xb4M\xe6\x9c\x88" "d\xe6\x97\xa5" "dddd";
        r.dateShort = "yyyy/MM/dd";
        r.timeLong = "H:mm:ss t";
        r.timeShort = "H:mm";
        r.dateTimeJoinLong = " ";
        r.dateTimeJoinShort = " ";
        r.daysLong = "\xe6\x9c\x88\xe6\x9b\x9c\xe6\x97\xa5|\xe7\x81\xab\xe6\x9b\x9c\xe6\x97\xa5|\xe6\xb0\xb4\xe6\x9b\x9c\xe6\x97\xa5|\xe6\x9c\xa8\xe6\x9b\x9c\xe6\x97\xa5|"
                     "\xe9\x87\x91\xe6\x9b\x9c\xe6\x97\xa5|\xe5\x9c\x9f\xe6\x9b\x9c\xe6\x97\xa5|\xe6\x97\xa5\xe6\x9b\x9c\xe6\x97\xa5";
        r.daysShort = "\xe6\x9c\x88|\xe7\x81\xab|\xe6\xb0\xb4|\xe6\x9c\xa8|\xe9\x87\x91|\xe5\x9c\x9f|\xe6\x97\xa5";
        r.daysNarrow = r.daysShort;
        r.monthsLong = "1\xe6\x9c\x88|2\xe6\x9c\x88|3\xe6\x9c\x88|4\xe6\x9c\x88|5\xe6\x9c\x88|6\xe6\x9c\x88|7\xe6\x9c\x88|8\xe6\x9c\x88|9\xe6\x9c\x88|10\xe6\x9c\x88|11\xe6\x9c\x88|12\xe6\x9c\x88";
        r.monthsShort = r.monthsLong;
        r.monthsNarrow = "1|2|3|4|5|6|7|8|9|10|11|12";
        r.am = "\xe5\x8d\x88\xe5\x89\x8d";
        r.pm = "\xe5\x8d\x88\xe5\xbe\x8c";
        r.currencySymbol = "\xef\xbf\xa5";
        r.currencyIso = "JPY";
        r.currencyName = "Japanese Yen";
        r.currencyDigits = 0;
        r.quoteStart = "\xe3\x80\x8c";
        r.quoteEnd = "\xe3\x80\x8d";
        r.altQuoteStart = "\xe3\x80\x8e";
        r.altQuoteEnd = "\xe3\x80\x8f";
        r.listSeparator = "\xe3\x80\x81";
        r.listEnd = "\xe3\x80\x81";
        r.listPair = "\xe3\x80\x81";
        r.nativeLanguage = "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e";
        r.nativeTerritory = "\xe6\x97\xa5\xe6\x9c\xac";
        r.firstDay = Qt::Sunday;
        rows.push_back(r);
    }
    // zh_CN
    {
        QLocaleRow r;
        r.language = QLocale::Chinese;
        r.script = QLocale::SimplifiedHanScript;
        r.territory = QLocale::China;
        r.dateLong = "yyyy\xe5\xb9\xb4M\xe6\x9c\x88" "d\xe6\x97\xa5" "dddd";
        r.dateShort = "yyyy/M/d";
        r.timeLong = "HH:mm:ss t";
        r.timeShort = "HH:mm";
        r.dateTimeJoinLong = " ";
        r.dateTimeJoinShort = " ";
        r.daysLong = "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x80|\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x8c|\xe6\x98\x9f\xe6\x9c\x9f\xe4\xb8\x89|\xe6\x98\x9f\xe6\x9c\x9f\xe5\x9b\x9b|"
                     "\xe6\x98\x9f\xe6\x9c\x9f\xe4\xba\x94|\xe6\x98\x9f\xe6\x9c\x9f\xe5\x85\xad|\xe6\x98\x9f\xe6\x9c\x9f\xe6\x97\xa5";
        r.daysShort = "\xe5\x91\xa8\xe4\xb8\x80|\xe5\x91\xa8\xe4\xba\x8c|\xe5\x91\xa8\xe4\xb8\x89|\xe5\x91\xa8\xe5\x9b\x9b|\xe5\x91\xa8\xe4\xba\x94|\xe5\x91\xa8\xe5\x85\xad|\xe5\x91\xa8\xe6\x97\xa5";
        r.daysNarrow = "\xe4\xb8\x80|\xe4\xba\x8c|\xe4\xb8\x89|\xe5\x9b\x9b|\xe4\xba\x94|\xe5\x85\xad|\xe6\x97\xa5";
        r.monthsLong = "\xe4\xb8\x80\xe6\x9c\x88|\xe4\xba\x8c\xe6\x9c\x88|\xe4\xb8\x89\xe6\x9c\x88|\xe5\x9b\x9b\xe6\x9c\x88|\xe4\xba\x94\xe6\x9c\x88|\xe5\x85\xad\xe6\x9c\x88|"
                       "\xe4\xb8\x83\xe6\x9c\x88|\xe5\x85\xab\xe6\x9c\x88|\xe4\xb9\x9d\xe6\x9c\x88|\xe5\x8d\x81\xe6\x9c\x88|\xe5\x8d\x81\xe4\xb8\x80\xe6\x9c\x88|\xe5\x8d\x81\xe4\xba\x8c\xe6\x9c\x88";
        r.monthsShort = "1\xe6\x9c\x88|2\xe6\x9c\x88|3\xe6\x9c\x88|4\xe6\x9c\x88|5\xe6\x9c\x88|6\xe6\x9c\x88|7\xe6\x9c\x88|8\xe6\x9c\x88|9\xe6\x9c\x88|10\xe6\x9c\x88|11\xe6\x9c\x88|12\xe6\x9c\x88";
        r.monthsNarrow = "1|2|3|4|5|6|7|8|9|10|11|12";
        r.am = "\xe4\xb8\x8a\xe5\x8d\x88";
        r.pm = "\xe4\xb8\x8b\xe5\x8d\x88";
        r.currencySymbol = "\xc2\xa5";
        r.currencyIso = "CNY";
        r.currencyName = "Chinese Yuan";
        r.listSeparator = "\xe3\x80\x81";
        r.listEnd = "\xe5\x92\x8c";
        r.listPair = "\xe5\x92\x8c";
        r.nativeLanguage = "\xe4\xb8\xad\xe6\x96\x87";
        r.nativeTerritory = "\xe4\xb8\xad\xe5\x9b\xbd";
        rows.push_back(r);
    }
    // ko_KR
    {
        QLocaleRow r;
        r.language = QLocale::Korean;
        r.script = QLocale::KoreanScript;
        r.territory = QLocale::SouthKorea;
        r.dateLong = "yyyy\xeb\x85\x84 M\xec\x9b\x94 d\xec\x9d\xbc dddd";
        r.dateShort = "yy. M. d.";
        r.timeLong = "AP h\xec\x8b\x9c m\xeb\xb6\x84 s\xec\xb4\x88 t";
        r.timeShort = "AP h:mm";
        r.dateTimeJoinLong = " ";
        r.dateTimeJoinShort = " ";
        r.daysLong = "\xec\x9b\x94\xec\x9a\x94\xec\x9d\xbc|\xed\x99\x94\xec\x9a\x94\xec\x9d\xbc|\xec\x88\x98\xec\x9a\x94\xec\x9d\xbc|\xeb\xaa\xa9\xec\x9a\x94\xec\x9d\xbc|"
                     "\xea\xb8\x88\xec\x9a\x94\xec\x9d\xbc|\xed\x86\xa0\xec\x9a\x94\xec\x9d\xbc|\xec\x9d\xbc\xec\x9a\x94\xec\x9d\xbc";
        r.daysShort = "\xec\x9b\x94|\xed\x99\x94|\xec\x88\x98|\xeb\xaa\xa9|\xea\xb8\x88|\xed\x86\xa0|\xec\x9d\xbc";
        r.daysNarrow = r.daysShort;
        r.monthsLong = "1\xec\x9b\x94|2\xec\x9b\x94|3\xec\x9b\x94|4\xec\x9b\x94|5\xec\x9b\x94|6\xec\x9b\x94|7\xec\x9b\x94|8\xec\x9b\x94|9\xec\x9b\x94|10\xec\x9b\x94|11\xec\x9b\x94|12\xec\x9b\x94";
        r.monthsShort = r.monthsLong;
        r.monthsNarrow = r.monthsLong;
        r.am = "\xec\x98\xa4\xec\xa0\x84";
        r.pm = "\xec\x98\xa4\xed\x9b\x84";
        r.currencySymbol = "\xe2\x82\xa9";
        r.currencyIso = "KRW";
        r.currencyName = "South Korean Won";
        r.currencyDigits = 0;
        r.listSeparator = ", ";
        r.listEnd = " \xeb\xb0\x8f ";
        r.listPair = " \xeb\xb0\x8f ";
        r.nativeLanguage = "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4";
        r.nativeTerritory = "\xeb\x8c\x80\xed\x95\x9c\xeb\xaf\xbc\xea\xb5\xad";
        r.firstDay = Qt::Sunday;
        rows.push_back(r);
    }
    // pl_PL
    {
        QLocaleRow r;
        r.language = QLocale::Polish;
        r.territory = QLocale::Poland;
        r.decimal = ",";
        r.group = "\xc2\xa0";
        r.dateLong = "dddd, d MMMM yyyy";
        r.dateShort = "d.MM.yyyy";
        r.timeLong = "HH:mm:ss t";
        r.timeShort = "HH:mm";
        r.dateTimeJoinLong = " ";
        r.dateTimeJoinShort = ", ";
        r.daysLong = "poniedzia\xc5\x82" "ek|wtorek|\xc5\x9b" "roda|czwartek|pi\xc4\x85tek|sobota|niedziela";
        r.daysShort = "pon.|wt.|\xc5\x9br.|czw.|pt.|sob.|niedz.";
        r.daysNarrow = "p|w|\xc5\x9b|c|p|s|n";
        // Months in the genitive, as used in dates.
        r.monthsLong = "stycznia|lutego|marca|kwietnia|maja|czerwca|lipca|sierpnia|wrze\xc5\x9bnia|pa\xc5\xba" "dziernika|listopada|grudnia";
        r.standaloneMonthsLong = "stycze\xc5\x84|luty|marzec|kwiecie\xc5\x84|maj|czerwiec|lipiec|sierpie\xc5\x84|wrzesie\xc5\x84|pa\xc5\xba" "dziernik|listopad|grudzie\xc5\x84";
        r.monthsShort = "sty|lut|mar|kwi|maj|cze|lip|sie|wrz|pa\xc5\xba|lis|gru";
        r.monthsNarrow = "s|l|m|k|m|c|l|s|w|p|l|g";
        r.currencySymbol = "z\xc5\x82";
        r.currencyIso = "PLN";
        r.currencyName = "Polish Zloty";
        r.currencyPosition = 2;
        r.quoteStart = "\xe2\x80\x9e";
        r.quoteEnd = "\xe2\x80\x9d";
        r.altQuoteStart = "\xc2\xab";
        r.altQuoteEnd = "\xc2\xbb";
        r.listSeparator = ", ";
        r.listEnd = " i ";
        r.listPair = " i ";
        r.nativeLanguage = "polski";
        r.nativeTerritory = "Polska";
        rows.push_back(r);
    }
    // tr_TR
    {
        QLocaleRow r;
        r.language = QLocale::Turkish;
        r.territory = QLocale::Turkey;
        r.decimal = ",";
        r.group = ".";
        r.dateLong = "d MMMM yyyy dddd";
        r.dateShort = "d.MM.yyyy";
        r.timeLong = "HH:mm:ss t";
        r.timeShort = "HH:mm";
        r.dateTimeJoinLong = " ";
        r.dateTimeJoinShort = " ";
        r.daysLong = "Pazartesi|Sal\xc4\xb1|\xc3\x87" "ar\xc5\x9f" "amba|Per\xc5\x9f" "embe|Cuma|Cumartesi|Pazar";
        r.daysShort = "Pzt|Sal|\xc3\x87" "ar|Per|Cum|Cmt|Paz";
        r.daysNarrow = "P|S|\xc3\x87|P|C|C|P";
        r.monthsLong = "Ocak|\xc5\x9e" "ubat|Mart|Nisan|May\xc4\xb1s|Haziran|Temmuz|A\xc4\x9f" "ustos|Eyl\xc3\xbcl|Ekim|Kas\xc4\xb1m|Aral\xc4\xb1k";
        r.monthsShort = "Oca|\xc5\x9e" "ub|Mar|Nis|May|Haz|Tem|A\xc4\x9f" "u|Eyl|Eki|Kas|Ara";
        r.monthsNarrow = "O|\xc5\x9e|M|N|M|H|T|A|E|E|K|A";
        r.am = "\xc3\x96\xc3\x96";
        r.pm = "\xc3\x96S";
        r.currencySymbol = "\xe2\x82\xba";
        r.currencyIso = "TRY";
        r.currencyName = "Turkish Lira";
        r.listSeparator = ", ";
        r.listEnd = " ve ";
        r.listPair = " ve ";
        r.nativeLanguage = "T\xc3\xbcrk\xc3\xa7" "e";
        r.nativeTerritory = "T\xc3\xbcrkiye";
        rows.push_back(r);
    }
    return rows;
}

std::vector<QLocaleNameEntry> buildLanguageNames() {
    return {
        {QLocale::AnyLanguage, "", nullptr, "Default"},
        {QLocale::C, "C", nullptr, "C"},
        {QLocale::Afrikaans, "af", "afr", "Afrikaans"},
        {QLocale::Albanian, "sq", "sqi", "Albanian"},
        {QLocale::Arabic, "ar", "ara", "Arabic"},
        {QLocale::Armenian, "hy", "hye", "Armenian"},
        {QLocale::Azerbaijani, "az", "aze", "Azerbaijani"},
        {QLocale::Bangla, "bn", "ben", "Bangla"},
        {QLocale::Basque, "eu", "eus", "Basque"},
        {QLocale::Belarusian, "be", "bel", "Belarusian"},
        {QLocale::Bosnian, "bs", "bos", "Bosnian"},
        {QLocale::Bulgarian, "bg", "bul", "Bulgarian"},
        {QLocale::Catalan, "ca", "cat", "Catalan"},
        {QLocale::Chinese, "zh", "zho", "Chinese"},
        {QLocale::Croatian, "hr", "hrv", "Croatian"},
        {QLocale::Czech, "cs", "ces", "Czech"},
        {QLocale::Danish, "da", "dan", "Danish"},
        {QLocale::Dutch, "nl", "nld", "Dutch"},
        {QLocale::English, "en", "eng", "English"},
        {QLocale::Estonian, "et", "est", "Estonian"},
        {QLocale::Filipino, "fil", "fil", "Filipino"},
        {QLocale::Finnish, "fi", "fin", "Finnish"},
        {QLocale::French, "fr", "fra", "French"},
        {QLocale::Galician, "gl", "glg", "Galician"},
        {QLocale::Georgian, "ka", "kat", "Georgian"},
        {QLocale::German, "de", "deu", "German"},
        {QLocale::Greek, "el", "ell", "Greek"},
        {QLocale::Gujarati, "gu", "guj", "Gujarati"},
        {QLocale::Hebrew, "he", "heb", "Hebrew"},
        {QLocale::Hindi, "hi", "hin", "Hindi"},
        {QLocale::Hungarian, "hu", "hun", "Hungarian"},
        {QLocale::Icelandic, "is", "isl", "Icelandic"},
        {QLocale::Indonesian, "id", "ind", "Indonesian"},
        {QLocale::Irish, "ga", "gle", "Irish"},
        {QLocale::Italian, "it", "ita", "Italian"},
        {QLocale::Japanese, "ja", "jpn", "Japanese"},
        {QLocale::Kazakh, "kk", "kaz", "Kazakh"},
        {QLocale::Khmer, "km", "khm", "Khmer"},
        {QLocale::Korean, "ko", "kor", "Korean"},
        {QLocale::Latvian, "lv", "lav", "Latvian"},
        {QLocale::Lithuanian, "lt", "lit", "Lithuanian"},
        {QLocale::Macedonian, "mk", "mkd", "Macedonian"},
        {QLocale::Malay, "ms", "msa", "Malay"},
        {QLocale::Maltese, "mt", "mlt", "Maltese"},
        {QLocale::Mongolian, "mn", "mon", "Mongolian"},
        {QLocale::NorwegianBokmal, "nb", "nob", "NorwegianBokmal"},
        {QLocale::NorwegianNynorsk, "nn", "nno", "NorwegianNynorsk"},
        {QLocale::Persian, "fa", "fas", "Persian"},
        {QLocale::Polish, "pl", "pol", "Polish"},
        {QLocale::Portuguese, "pt", "por", "Portuguese"},
        {QLocale::Romanian, "ro", "ron", "Romanian"},
        {QLocale::Russian, "ru", "rus", "Russian"},
        {QLocale::Serbian, "sr", "srp", "Serbian"},
        {QLocale::Slovak, "sk", "slk", "Slovak"},
        {QLocale::Slovenian, "sl", "slv", "Slovenian"},
        {QLocale::Spanish, "es", "spa", "Spanish"},
        {QLocale::Swahili, "sw", "swa", "Swahili"},
        {QLocale::Swedish, "sv", "swe", "Swedish"},
        {QLocale::Tamil, "ta", "tam", "Tamil"},
        {QLocale::Thai, "th", "tha", "Thai"},
        {QLocale::Turkish, "tr", "tur", "Turkish"},
        {QLocale::Ukrainian, "uk", "ukr", "Ukrainian"},
        {QLocale::Urdu, "ur", "urd", "Urdu"},
        {QLocale::Uzbek, "uz", "uzb", "Uzbek"},
        {QLocale::Vietnamese, "vi", "vie", "Vietnamese"},
        {QLocale::Welsh, "cy", "cym", "Welsh"},
    };
}

std::vector<QLocaleNameEntry> buildTerritoryNames() {
    return {
        {QLocale::AnyTerritory, "", nullptr, "Default"},
        {QLocale::Argentina, "AR", nullptr, "Argentina"},
        {QLocale::Australia, "AU", nullptr, "Australia"},
        {QLocale::Austria, "AT", nullptr, "Austria"},
        {QLocale::Belgium, "BE", nullptr, "Belgium"},
        {QLocale::Brazil, "BR", nullptr, "Brazil"},
        {QLocale::Canada, "CA", nullptr, "Canada"},
        {QLocale::Chile, "CL", nullptr, "Chile"},
        {QLocale::China, "CN", nullptr, "China"},
        {QLocale::Colombia, "CO", nullptr, "Colombia"},
        {QLocale::Czechia, "CZ", nullptr, "Czechia"},
        {QLocale::Denmark, "DK", nullptr, "Denmark"},
        {QLocale::Finland, "FI", nullptr, "Finland"},
        {QLocale::France, "FR", nullptr, "France"},
        {QLocale::Germany, "DE", nullptr, "Germany"},
        {QLocale::Greece, "GR", nullptr, "Greece"},
        {QLocale::HongKong, "HK", nullptr, "Hong Kong"},
        {QLocale::Hungary, "HU", nullptr, "Hungary"},
        {QLocale::India, "IN", nullptr, "India"},
        {QLocale::Ireland, "IE", nullptr, "Ireland"},
        {QLocale::Israel, "IL", nullptr, "Israel"},
        {QLocale::Italy, "IT", nullptr, "Italy"},
        {QLocale::Japan, "JP", nullptr, "Japan"},
        {QLocale::Mexico, "MX", nullptr, "Mexico"},
        {QLocale::Netherlands, "NL", nullptr, "Netherlands"},
        {QLocale::NewZealand, "NZ", nullptr, "New Zealand"},
        {QLocale::Norway, "NO", nullptr, "Norway"},
        {QLocale::Poland, "PL", nullptr, "Poland"},
        {QLocale::Portugal, "PT", nullptr, "Portugal"},
        {QLocale::Russia, "RU", nullptr, "Russia"},
        {QLocale::Singapore, "SG", nullptr, "Singapore"},
        {QLocale::SouthAfrica, "ZA", nullptr, "South Africa"},
        {QLocale::SouthKorea, "KR", nullptr, "South Korea"},
        {QLocale::Spain, "ES", nullptr, "Spain"},
        {QLocale::Sweden, "SE", nullptr, "Sweden"},
        {QLocale::Switzerland, "CH", nullptr, "Switzerland"},
        {QLocale::Taiwan, "TW", nullptr, "Taiwan"},
        {QLocale::Turkey, "TR", nullptr, "Turkey"},
        {QLocale::Ukraine, "UA", nullptr, "Ukraine"},
        {QLocale::UnitedKingdom, "GB", nullptr, "United Kingdom"},
        {QLocale::UnitedStates, "US", nullptr, "United States"},
        {QLocale::World, "001", nullptr, "World"},
    };
}

std::vector<QLocaleNameEntry> buildScriptNames() {
    return {
        {QLocale::AnyScript, "", nullptr, "Default"},
        {QLocale::ArabicScript, "Arab", nullptr, "Arabic"},
        {QLocale::CyrillicScript, "Cyrl", nullptr, "Cyrillic"},
        {QLocale::DeseretScript, "Dsrt", nullptr, "Deseret"},
        {QLocale::GurmukhiScript, "Guru", nullptr, "Gurmukhi"},
        {QLocale::SimplifiedHanScript, "Hans", nullptr, "Simplified Han"},
        {QLocale::TraditionalHanScript, "Hant", nullptr, "Traditional Han"},
        {QLocale::LatinScript, "Latn", nullptr, "Latin"},
        {QLocale::MongolianScript, "Mong", nullptr, "Mongolian"},
        {QLocale::TifinaghScript, "Tfng", nullptr, "Tifinagh"},
        {QLocale::ArmenianScript, "Armn", nullptr, "Armenian"},
        {QLocale::BanglaScript, "Beng", nullptr, "Bangla"},
        {QLocale::CherokeeScript, "Cher", nullptr, "Cherokee"},
        {QLocale::DevanagariScript, "Deva", nullptr, "Devanagari"},
        {QLocale::EthiopicScript, "Ethi", nullptr, "Ethiopic"},
        {QLocale::GeorgianScript, "Geor", nullptr, "Georgian"},
        {QLocale::GreekScript, "Grek", nullptr, "Greek"},
        {QLocale::GujaratiScript, "Gujr", nullptr, "Gujarati"},
        {QLocale::HebrewScript, "Hebr", nullptr, "Hebrew"},
        {QLocale::JapaneseScript, "Jpan", nullptr, "Japanese"},
        {QLocale::KhmerScript, "Khmr", nullptr, "Khmer"},
        {QLocale::KannadaScript, "Knda", nullptr, "Kannada"},
        {QLocale::KoreanScript, "Kore", nullptr, "Korean"},
        {QLocale::LaoScript, "Laoo", nullptr, "Lao"},
        {QLocale::MalayalamScript, "Mlym", nullptr, "Malayalam"},
        {QLocale::MyanmarScript, "Mymr", nullptr, "Myanmar"},
        {QLocale::OriyaScript, "Orya", nullptr, "Oriya"},
        {QLocale::TamilScript, "Taml", nullptr, "Tamil"},
        {QLocale::TeluguScript, "Telu", nullptr, "Telugu"},
        {QLocale::ThaanaScript, "Thaa", nullptr, "Thaana"},
        {QLocale::ThaiScript, "Thai", nullptr, "Thai"},
        {QLocale::TibetanScript, "Tibt", nullptr, "Tibetan"},
        {QLocale::SinhalaScript, "Sinh", nullptr, "Sinhala"},
        {QLocale::SyriacScript, "Syrc", nullptr, "Syriac"},
        {QLocale::YiScript, "Yiii", nullptr, "Yi"},
        {QLocale::VaiScript, "Vaii", nullptr, "Vai"},
    };
}

} // namespace

namespace QtPrivate {

const std::vector<QLocaleRow> &localeRows() {
    static const std::vector<QLocaleRow> rows = buildRows();
    return rows;
}

const std::vector<QLocaleNameEntry> &languageNames() {
    static const std::vector<QLocaleNameEntry> names = buildLanguageNames();
    return names;
}

const std::vector<QLocaleNameEntry> &territoryNames() {
    static const std::vector<QLocaleNameEntry> names = buildTerritoryNames();
    return names;
}

const std::vector<QLocaleNameEntry> &scriptNames() {
    static const std::vector<QLocaleNameEntry> names = buildScriptNames();
    return names;
}

} // namespace QtPrivate
