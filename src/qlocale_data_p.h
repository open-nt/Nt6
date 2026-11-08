// MIT License - see LICENSE
#pragma once

// Locale data for QLocale. Lists of names are separated by ';'. Day lists start with Monday.
// Currency formats use %1 for the amount and %2 for the symbol.

#include <QtCore/qlocale.h>

struct QLocaleData {
    QLocale::Language language;
    QLocale::Script script;
    QLocale::Territory territory;
    const char *name;
    bool defaultForLanguage; // the territory is implied by the language in BCP 47 names
    // Language
    const char16_t *nativeLanguage;
    const char16_t *quoteStart, *quoteEnd, *altQuoteStart, *altQuoteEnd;
    const char16_t *listTwo, *listMiddle, *listEnd;
    const char16_t *am, *pm;
    const char16_t *months, *shortMonths, *standaloneMonths, *standaloneShortMonths;
    const char16_t *days, *shortDays;
    // Territory
    const char16_t *nativeTerritory;
    const char16_t *decimal, *group, *minus;
    int minGrouping;
    const char16_t *longDate, *shortDate, *longTime, *shortTime;
    Qt::DayOfWeek firstDay;
    QLocale::MeasurementSystem measurement;
    const char16_t *currencyIso, *currencySymbol, *currencyName;
    int currencyDigits;
    const char16_t *currencyFormat;
};

// clang-format off
#define NT_EN_NAMES u"January;February;March;April;May;June;July;August;September;October;November;December", \
    u"Jan;Feb;Mar;Apr;May;Jun;Jul;Aug;Sep;Oct;Nov;Dec", nullptr, nullptr, \
    u"Monday;Tuesday;Wednesday;Thursday;Friday;Saturday;Sunday", u"Mon;Tue;Wed;Thu;Fri;Sat;Sun"
#define NT_LANG_EN(listEnd) u"English", u"“", u"”", u"‘", u"’", u" and ", u", ", listEnd, u"AM", u"PM", NT_EN_NAMES
#define NT_LANG_DE(qs, qe, aqs, aqe) u"Deutsch", qs, qe, aqs, aqe, u" und ", u", ", u" und ", u"AM", u"PM", \
    u"Januar;Februar;März;April;Mai;Juni;Juli;August;September;Oktober;November;Dezember", \
    u"Jan.;Feb.;März;Apr.;Mai;Juni;Juli;Aug.;Sept.;Okt.;Nov.;Dez.", nullptr, \
    u"Jan;Feb;Mär;Apr;Mai;Jun;Jul;Aug;Sep;Okt;Nov;Dez", \
    u"Montag;Dienstag;Mittwoch;Donnerstag;Freitag;Samstag;Sonntag", u"Mo.;Di.;Mi.;Do.;Fr.;Sa.;So."
#define NT_LANG_FR u"français", u"«", u"»", u"“", u"”", u" et ", u", ", u" et ", u"AM", u"PM", \
    u"janvier;février;mars;avril;mai;juin;juillet;août;septembre;octobre;novembre;décembre", \
    u"janv.;févr.;mars;avr.;mai;juin;juil.;août;sept.;oct.;nov.;déc.", nullptr, nullptr, \
    u"lundi;mardi;mercredi;jeudi;vendredi;samedi;dimanche", u"lun.;mar.;mer.;jeu.;ven.;sam.;dim."
#define NT_LANG_ES u"español", u"«", u"»", u"“", u"”", u" y ", u", ", u" y ", u"a. m.", u"p. m.", \
    u"enero;febrero;marzo;abril;mayo;junio;julio;agosto;septiembre;octubre;noviembre;diciembre", \
    u"ene;feb;mar;abr;may;jun;jul;ago;sept;oct;nov;dic", nullptr, nullptr, \
    u"lunes;martes;miércoles;jueves;viernes;sábado;domingo", u"lun;mar;mié;jue;vie;sáb;dom"
#define NT_LANG_IT u"italiano", u"«", u"»", u"“", u"”", u" e ", u", ", u" e ", u"AM", u"PM", \
    u"gennaio;febbraio;marzo;aprile;maggio;giugno;luglio;agosto;settembre;ottobre;novembre;dicembre", \
    u"gen;feb;mar;apr;mag;giu;lug;ago;set;ott;nov;dic", nullptr, nullptr, \
    u"lunedì;martedì;mercoledì;giovedì;venerdì;sabato;domenica", u"lun;mar;mer;gio;ven;sab;dom"
#define NT_LANG_NL u"Nederlands", u"“", u"”", u"‘", u"’", u" en ", u", ", u" en ", u"a.m.", u"p.m.", \
    u"januari;februari;maart;april;mei;juni;juli;augustus;september;oktober;november;december", \
    u"jan;feb;mrt;apr;mei;jun;jul;aug;sep;okt;nov;dec", nullptr, nullptr, \
    u"maandag;dinsdag;woensdag;donderdag;vrijdag;zaterdag;zondag", u"ma;di;wo;do;vr;za;zo"
#define NT_LANG_PT u"português", u"“", u"”", u"‘", u"’", u" e ", u", ", u" e ", u"AM", u"PM", \
    u"janeiro;fevereiro;março;abril;maio;junho;julho;agosto;setembro;outubro;novembro;dezembro", \
    u"jan.;fev.;mar.;abr.;mai.;jun.;jul.;ago.;set.;out.;nov.;dez.", nullptr, nullptr, \
    u"segunda-feira;terça-feira;quarta-feira;quinta-feira;sexta-feira;sábado;domingo", \
    u"seg.;ter.;qua.;qui.;sex.;sáb.;dom."
#define NT_LANG_SV u"svenska", u"”", u"”", u"’", u"’", u" och ", u", ", u" och ", u"fm", u"em", \
    u"januari;februari;mars;april;maj;juni;juli;augusti;september;oktober;november;december", \
    u"jan.;feb.;mars;apr.;maj;juni;juli;aug.;sep.;okt.;nov.;dec.", nullptr, nullptr, \
    u"måndag;tisdag;onsdag;torsdag;fredag;lördag;söndag", u"mån;tis;ons;tors;fre;lör;sön"
#define NT_LANG_NB u"norsk bokmål", u"«", u"»", u"‘", u"’", u" og ", u", ", u" og ", u"a.m.", u"p.m.", \
    u"januar;februar;mars;april;mai;juni;juli;august;september;oktober;november;desember", \
    u"jan.;feb.;mar.;apr.;mai;jun.;jul.;aug.;sep.;okt.;nov.;des.", nullptr, nullptr, \
    u"mandag;tirsdag;onsdag;torsdag;fredag;lørdag;søndag", u"man.;tir.;ons.;tor.;fre.;lør.;søn."
#define NT_LANG_DA u"dansk", u"“", u"”", u"‘", u"’", u" og ", u", ", u" og ", u"AM", u"PM", \
    u"januar;februar;marts;april;maj;juni;juli;august;september;oktober;november;december", \
    u"jan.;feb.;mar.;apr.;maj;jun.;jul.;aug.;sep.;okt.;nov.;dec.", nullptr, nullptr, \
    u"mandag;tirsdag;onsdag;torsdag;fredag;lørdag;søndag", u"man.;tirs.;ons.;tors.;fre.;lør.;søn."
#define NT_LANG_FI u"suomi", u"”", u"”", u"’", u"’", u" ja ", u", ", u" ja ", u"ap.", u"ip.", \
    u"tammikuuta;helmikuuta;maaliskuuta;huhtikuuta;toukokuuta;kesäkuuta;heinäkuuta;elokuuta;syyskuuta;lokakuuta;marraskuuta;joulukuuta", \
    u"tammik.;helmik.;maalisk.;huhtik.;toukok.;kesäk.;heinäk.;elok.;syysk.;lokak.;marrask.;jouluk.", \
    u"tammikuu;helmikuu;maaliskuu;huhtikuu;toukokuu;kesäkuu;heinäkuu;elokuu;syyskuu;lokakuu;marraskuu;joulukuu", \
    u"tammi;helmi;maalis;huhti;touko;kesä;heinä;elo;syys;loka;marras;joulu", \
    u"maanantaina;tiistaina;keskiviikkona;torstaina;perjantaina;lauantaina;sunnuntaina", u"ma;ti;ke;to;pe;la;su"
#define NT_LANG_PL u"polski", u"„", u"”", u"«", u"»", u" i ", u", ", u" i ", u"AM", u"PM", \
    u"stycznia;lutego;marca;kwietnia;maja;czerwca;lipca;sierpnia;września;października;listopada;grudnia", \
    u"sty;lut;mar;kwi;maj;cze;lip;sie;wrz;paź;lis;gru", \
    u"styczeń;luty;marzec;kwiecień;maj;czerwiec;lipiec;sierpień;wrzesień;październik;listopad;grudzień", nullptr, \
    u"poniedziałek;wtorek;środa;czwartek;piątek;sobota;niedziela", u"pon.;wt.;śr.;czw.;pt.;sob.;niedz."
#define NT_LANG_CS u"čeština", u"„", u"“", u"‚", u"‘", u" a ", u", ", u" a ", u"dop.", u"odp.", \
    u"ledna;února;března;dubna;května;června;července;srpna;září;října;listopadu;prosince", \
    u"led;úno;bře;dub;kvě;čvn;čvc;srp;zář;říj;lis;pro", \
    u"leden;únor;březen;duben;květen;červen;červenec;srpen;září;říjen;listopad;prosinec", nullptr, \
    u"pondělí;úterý;středa;čtvrtek;pátek;sobota;neděle", u"po;út;st;čt;pá;so;ne"
#define NT_LANG_RU u"русский", u"«", u"»", u"„", u"“", u" и ", u", ", u" и ", u"AM", u"PM", \
    u"января;февраля;марта;апреля;мая;июня;июля;августа;сентября;октября;ноября;декабря", \
    u"янв.;февр.;мар.;апр.;мая;июн.;июл.;авг.;сент.;окт.;нояб.;дек.", \
    u"январь;февраль;март;апрель;май;июнь;июль;август;сентябрь;октябрь;ноябрь;декабрь", \
    u"янв.;февр.;март;апр.;май;июнь;июль;авг.;сент.;окт.;нояб.;дек.", \
    u"понедельник;вторник;среда;четверг;пятница;суббота;воскресенье", \
    u"пн;вт;ср;чт;пт;сб;вс"
#define NT_LANG_UK u"українська", u"«", u"»", u"„", u"“", u" і ", u", ", u" і ", u"дп", u"пп", \
    u"січня;лютого;березня;квітня;травня;червня;липня;серпня;вересня;жовтня;листопада;грудня", \
    u"січ.;лют.;бер.;квіт.;трав.;черв.;лип.;серп.;вер.;жовт.;лист.;груд.", \
    u"січень;лютий;березень;квітень;травень;червень;липень;серпень;вересень;жовтень;листопад;грудень", nullptr, \
    u"понеділок;вівторок;середа;четвер;пʼятниця;субота;неділя", \
    u"пн;вт;ср;чт;пт;сб;нд"
#define NT_LANG_EL u"Ελληνικά", u"«", u"»", u"“", u"”", u" και ", u", ", u" και ", u"π.μ.", u"μ.μ.", \
    u"Ιανουαρίου;Φεβρουαρίου;Μαρτίου;Απριλίου;Μαΐου;Ιουνίου;Ιουλίου;Αυγούστου;Σεπτεμβρίου;Οκτωβρίου;Νοεμβρίου;Δεκεμβρίου", \
    u"Ιαν;Φεβ;Μαρ;Απρ;Μαΐ;Ιουν;Ιουλ;Αυγ;Σεπ;Οκτ;Νοε;Δεκ", \
    u"Ιανουάριος;Φεβρουάριος;Μάρτιος;Απρίλιος;Μάιος;Ιούνιος;Ιούλιος;Αύγουστος;Σεπτέμβριος;Οκτώβριος;Νοέμβριος;Δεκέμβριος", nullptr, \
    u"Δευτέρα;Τρίτη;Τετάρτη;Πέμπτη;Παρασκευή;Σάββατο;Κυριακή", \
    u"Δευ;Τρί;Τετ;Πέμ;Παρ;Σάβ;Κυρ"
#define NT_LANG_TR u"Türkçe", u"“", u"”", u"‘", u"’", u" ve ", u", ", u" ve ", u"ÖÖ", u"ÖS", \
    u"Ocak;Şubat;Mart;Nisan;Mayıs;Haziran;Temmuz;Ağustos;Eylül;Ekim;Kasım;Aralık", \
    u"Oca;Şub;Mar;Nis;May;Haz;Tem;Ağu;Eyl;Eki;Kas;Ara", nullptr, nullptr, \
    u"Pazartesi;Salı;Çarşamba;Perşembe;Cuma;Cumartesi;Pazar", u"Pzt;Sal;Çar;Per;Cum;Cmt;Paz"
#define NT_CJK_MONTHS(suffix) u"1" suffix ";2" suffix ";3" suffix ";4" suffix ";5" suffix ";6" suffix ";7" suffix ";8" suffix ";9" suffix ";10" suffix ";11" suffix ";12" suffix
#define NT_LANG_JA u"日本語", u"「", u"」", u"『", u"』", u"、", u"、", u"、", u"午前", u"午後", \
    NT_CJK_MONTHS(u"月"), NT_CJK_MONTHS(u"月"), nullptr, nullptr, \
    u"月曜日;火曜日;水曜日;木曜日;金曜日;土曜日;日曜日", \
    u"月;火;水;木;金;土;日"
#define NT_ZH_LONG_MONTHS u"一月;二月;三月;四月;五月;六月;七月;八月;九月;十月;十一月;十二月"
#define NT_ZH_DAYS u"星期一;星期二;星期三;星期四;星期五;星期六;星期日"
#define NT_LANG_ZH_CN u"中文", u"“", u"”", u"‘", u"’", u"和", u"、", u"和", u"上午", u"下午", \
    NT_ZH_LONG_MONTHS, NT_CJK_MONTHS(u"月"), nullptr, nullptr, NT_ZH_DAYS, \
    u"周一;周二;周三;周四;周五;周六;周日"
#define NT_LANG_ZH_TW u"中文", u"「", u"」", u"『", u"』", u"和", u"、", u"和", u"上午", u"下午", \
    NT_CJK_MONTHS(u"月"), NT_CJK_MONTHS(u"月"), nullptr, nullptr, NT_ZH_DAYS, \
    u"週一;週二;週三;週四;週五;週六;週日"
#define NT_LANG_KO u"한국어", u"“", u"”", u"‘", u"’", u" 및 ", u", ", u" 및 ", u"오전", u"오후", \
    NT_CJK_MONTHS(u"월"), NT_CJK_MONTHS(u"월"), nullptr, nullptr, \
    u"월요일;화요일;수요일;목요일;금요일;토요일;일요일", \
    u"월;화;수;목;금;토;일"

#define NT_NBSP u" "
#define NT_NNBSP u" "
#define NT_MINUS u"−"

using L = QLocale;
inline constexpr QLocaleData localeTable[] = {
    {L::C, L::AnyScript, L::AnyTerritory, "C", true,
     u"C", u"“", u"”", u"‘", u"’", u", ", u", ", u", ", u"AM", u"PM", NT_EN_NAMES,
     u"", u".", u",", u"-", 1, u"dddd, d MMMM yyyy", u"d MMM yyyy", u"HH:mm:ss t", u"HH:mm:ss", Qt::Monday, L::MetricSystem,
     u"", u"", u"", 2, u"%1%2"},
    {L::English, L::LatinScript, L::UnitedStates, "en_US", true, NT_LANG_EN(u", and "),
     u"United States", u".", u",", u"-", 1, u"dddd, MMMM d, yyyy", u"M/d/yy", u"h:mm:ss AP t", u"h:mm AP", Qt::Sunday, L::ImperialUSSystem,
     u"USD", u"$", u"US Dollar", 2, u"%2%1"},
    {L::English, L::LatinScript, L::UnitedKingdom, "en_GB", false, NT_LANG_EN(u" and "),
     u"United Kingdom", u".", u",", u"-", 1, u"dddd d MMMM yyyy", u"dd/MM/yyyy", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::ImperialUKSystem,
     u"GBP", u"£", u"British Pound", 2, u"%2%1"},
    {L::English, L::LatinScript, L::Australia, "en_AU", false, NT_LANG_EN(u" and "),
     u"Australia", u".", u",", u"-", 1, u"dddd d MMMM yyyy", u"d/M/yy", u"h:mm:ss AP t", u"h:mm AP", Qt::Monday, L::MetricSystem,
     u"AUD", u"$", u"Australian Dollar", 2, u"%2%1"},
    {L::English, L::LatinScript, L::Canada, "en_CA", false, NT_LANG_EN(u" and "),
     u"Canada", u".", u",", u"-", 1, u"dddd, MMMM d, yyyy", u"yyyy-MM-dd", u"h:mm:ss AP t", u"h:mm AP", Qt::Sunday, L::MetricSystem,
     u"CAD", u"$", u"Canadian Dollar", 2, u"%2%1"},
    {L::German, L::LatinScript, L::Germany, "de_DE", true, NT_LANG_DE(u"„", u"“", u"‚", u"‘"),
     u"Deutschland", u",", u".", u"-", 1, u"dddd, d. MMMM yyyy", u"dd.MM.yy", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"EUR", u"€", u"Euro", 2, u"%1" NT_NBSP u"%2"},
    {L::German, L::LatinScript, L::Austria, "de_AT", false, NT_LANG_DE(u"„", u"“", u"‚", u"‘"),
     u"Österreich", u",", NT_NBSP, u"-", 1, u"dddd, d. MMMM yyyy", u"dd.MM.yy", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"EUR", u"€", u"Euro", 2, u"%2" NT_NBSP u"%1"},
    {L::German, L::LatinScript, L::Switzerland, "de_CH", false, NT_LANG_DE(u"«", u"»", u"‹", u"›"),
     u"Schweiz", u".", u"’", u"-", 1, u"dddd, d. MMMM yyyy", u"dd.MM.yy", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"CHF", u"CHF", u"Schweizer Franken", 2, u"%2" NT_NBSP u"%1"},
    {L::French, L::LatinScript, L::France, "fr_FR", true, NT_LANG_FR,
     u"France", u",", NT_NNBSP, u"-", 1, u"dddd d MMMM yyyy", u"dd/MM/yyyy", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"EUR", u"€", u"euro", 2, u"%1" NT_NBSP u"%2"},
    {L::French, L::LatinScript, L::Canada, "fr_CA", false, NT_LANG_FR,
     u"Canada", u",", NT_NBSP, u"-", 1, u"dddd d MMMM yyyy", u"yyyy-MM-dd", u"HH:mm:ss t", u"HH:mm", Qt::Sunday, L::MetricSystem,
     u"CAD", u"$", u"dollar canadien", 2, u"%1" NT_NBSP u"%2"},
    {L::French, L::LatinScript, L::Switzerland, "fr_CH", false, NT_LANG_FR,
     u"Suisse", u",", NT_NNBSP, u"-", 1, u"dddd, d MMMM yyyy", u"dd.MM.yy", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"CHF", u"CHF", u"franc suisse", 2, u"%1" NT_NBSP u"%2"},
    {L::Spanish, L::LatinScript, L::Spain, "es_ES", true, NT_LANG_ES,
     u"España", u",", u".", u"-", 2, u"dddd, d 'de' MMMM 'de' yyyy", u"d/M/yy", u"H:mm:ss t", u"H:mm", Qt::Monday, L::MetricSystem,
     u"EUR", u"€", u"euro", 2, u"%1" NT_NBSP u"%2"},
    {L::Spanish, L::LatinScript, L::Mexico, "es_MX", false, NT_LANG_ES,
     u"México", u".", u",", u"-", 1, u"dddd, d 'de' MMMM 'de' yyyy", u"dd/MM/yy", u"H:mm:ss t", u"H:mm", Qt::Sunday, L::MetricSystem,
     u"MXN", u"$", u"peso mexicano", 2, u"%2%1"},
    {L::Italian, L::LatinScript, L::Italy, "it_IT", true, NT_LANG_IT,
     u"Italia", u",", u".", u"-", 1, u"dddd d MMMM yyyy", u"dd/MM/yy", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"EUR", u"€", u"euro", 2, u"%1" NT_NBSP u"%2"},
    {L::Dutch, L::LatinScript, L::Netherlands, "nl_NL", true, NT_LANG_NL,
     u"Nederland", u",", u".", u"-", 1, u"dddd d MMMM yyyy", u"dd-MM-yyyy", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"EUR", u"€", u"Euro", 2, u"%2" NT_NBSP u"%1"},
    {L::Dutch, L::LatinScript, L::Belgium, "nl_BE", false, NT_LANG_NL,
     u"België", u",", u".", u"-", 1, u"dddd d MMMM yyyy", u"d/MM/yy", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"EUR", u"€", u"Euro", 2, u"%2" NT_NBSP u"%1"},
    {L::Portuguese, L::LatinScript, L::Brazil, "pt_BR", true, NT_LANG_PT,
     u"Brasil", u",", u".", u"-", 1, u"dddd, d 'de' MMMM 'de' yyyy", u"dd/MM/yyyy", u"HH:mm:ss t", u"HH:mm", Qt::Sunday, L::MetricSystem,
     u"BRL", u"R$", u"Real brasileiro", 2, u"%2" NT_NBSP u"%1"},
    {L::Portuguese, L::LatinScript, L::Portugal, "pt_PT", false, NT_LANG_PT,
     u"Portugal", u",", NT_NBSP, u"-", 2, u"dddd, d 'de' MMMM 'de' yyyy", u"dd/MM/yy", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"EUR", u"€", u"euro", 2, u"%1" NT_NBSP u"%2"},
    {L::Swedish, L::LatinScript, L::Sweden, "sv_SE", true, NT_LANG_SV,
     u"Sverige", u",", NT_NBSP, NT_MINUS, 1, u"dddd d MMMM yyyy", u"yyyy-MM-dd", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"SEK", u"kr", u"svensk krona", 2, u"%1" NT_NBSP u"%2"},
    {L::NorwegianBokmal, L::LatinScript, L::Norway, "nb_NO", true, NT_LANG_NB,
     u"Norge", u",", NT_NBSP, NT_MINUS, 1, u"dddd d. MMMM yyyy", u"dd.MM.yyyy", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"NOK", u"kr", u"norske kroner", 2, u"%2" NT_NBSP u"%1"},
    {L::Danish, L::LatinScript, L::Denmark, "da_DK", true, NT_LANG_DA,
     u"Danmark", u",", u".", u"-", 1, u"dddd 'den' d. MMMM yyyy", u"dd.MM.yyyy", u"HH.mm.ss t", u"HH.mm", Qt::Monday, L::MetricSystem,
     u"DKK", u"kr.", u"dansk krone", 2, u"%1" NT_NBSP u"%2"},
    {L::Finnish, L::LatinScript, L::Finland, "fi_FI", true, NT_LANG_FI,
     u"Suomi", u",", NT_NBSP, NT_MINUS, 1, u"dddd d. MMMM yyyy", u"d.M.yyyy", u"H.mm.ss t", u"H.mm", Qt::Monday, L::MetricSystem,
     u"EUR", u"€", u"euro", 2, u"%1" NT_NBSP u"%2"},
    {L::Polish, L::LatinScript, L::Poland, "pl_PL", true, NT_LANG_PL,
     u"Polska", u",", NT_NBSP, u"-", 2, u"dddd, d MMMM yyyy", u"dd.MM.yyyy", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"PLN", u"zł", u"złoty polski", 2, u"%1" NT_NBSP u"%2"},
    {L::Czech, L::LatinScript, L::Czechia, "cs_CZ", true, NT_LANG_CS,
     u"Česko", u",", NT_NBSP, u"-", 1, u"dddd d. MMMM yyyy", u"dd.MM.yy", u"H:mm:ss t", u"H:mm", Qt::Monday, L::MetricSystem,
     u"CZK", u"Kč", u"česká koruna", 2, u"%1" NT_NBSP u"%2"},
    {L::Russian, L::CyrillicScript, L::Russia, "ru_RU", true, NT_LANG_RU,
     u"Россия", u",", NT_NBSP, u"-", 1, u"dddd, d MMMM yyyy 'г'.", u"dd.MM.yyyy", u"HH:mm:ss t", u"HH:mm",
     Qt::Monday, L::MetricSystem, u"RUB", u"₽", u"российский рубль", 2,
     u"%1" NT_NBSP u"%2"},
    {L::Ukrainian, L::CyrillicScript, L::Ukraine, "uk_UA", true, NT_LANG_UK,
     u"Україна", u",", NT_NBSP, u"-", 1, u"dddd, d MMMM yyyy 'р'.", u"dd.MM.yy", u"HH:mm:ss t", u"HH:mm",
     Qt::Monday, L::MetricSystem, u"UAH", u"₴", u"українська гривня", 2,
     u"%1" NT_NBSP u"%2"},
    {L::Greek, L::GreekScript, L::Greece, "el_GR", true, NT_LANG_EL,
     u"Ελλάδα", u",", u".", u"-", 1, u"dddd d MMMM yyyy", u"d/M/yy", u"h:mm:ss AP t", u"h:mm AP", Qt::Monday,
     L::MetricSystem, u"EUR", u"€", u"Ευρώ", 2, u"%1" NT_NBSP u"%2"},
    {L::Turkish, L::LatinScript, L::Turkey, "tr_TR", true, NT_LANG_TR,
     u"Türkiye", u",", u".", u"-", 1, u"d MMMM yyyy dddd", u"d.MM.yyyy", u"HH:mm:ss t", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"TRY", u"₺", u"Türk Lirası", 2, u"%2%1"},
    {L::Japanese, L::JapaneseScript, L::Japan, "ja_JP", true, NT_LANG_JA,
     u"日本", u".", u",", u"-", 1, u"yyyy年M月d日dddd", u"yyyy/MM/dd", u"H時mm分ss秒 t", u"H:mm", Qt::Sunday,
     L::MetricSystem, u"JPY", u"￥", u"日本円", 0, u"%2%1"},
    {L::Chinese, L::SimplifiedHanScript, L::China, "zh_CN", true, NT_LANG_ZH_CN,
     u"中国", u".", u",", u"-", 1, u"yyyy年M月d日dddd", u"yyyy/M/d", u"t HH:mm:ss", u"HH:mm", Qt::Monday, L::MetricSystem,
     u"CNY", u"¥", u"人民币", 2, u"%2%1"},
    {L::Chinese, L::TraditionalHanScript, L::Taiwan, "zh_TW", false, NT_LANG_ZH_TW,
     u"台灣", u".", u",", u"-", 1, u"yyyy年M月d日 dddd", u"yyyy/M/d", u"AP h:mm:ss t", u"AP h:mm", Qt::Sunday,
     L::MetricSystem, u"TWD", u"$", u"新台幣", 2, u"%2%1"},
    {L::Korean, L::KoreanScript, L::SouthKorea, "ko_KR", true, NT_LANG_KO,
     u"대한민국", u".", u",", u"-", 1, u"yyyy년 M월 d일 dddd", u"yy. M. d.", u"AP h시 m분 s초 t", u"AP h:mm",
     Qt::Sunday, L::MetricSystem, u"KRW", u"₩", u"대한민국 원", 0, u"%2%1"},
};
// clang-format on

#undef NT_EN_NAMES
#undef NT_LANG_EN
#undef NT_LANG_DE
#undef NT_LANG_FR
#undef NT_LANG_ES
#undef NT_LANG_IT
#undef NT_LANG_NL
#undef NT_LANG_PT
#undef NT_LANG_SV
#undef NT_LANG_NB
#undef NT_LANG_DA
#undef NT_LANG_FI
#undef NT_LANG_PL
#undef NT_LANG_CS
#undef NT_LANG_RU
#undef NT_LANG_UK
#undef NT_LANG_EL
#undef NT_LANG_TR
#undef NT_CJK_MONTHS
#undef NT_LANG_JA
#undef NT_ZH_LONG_MONTHS
#undef NT_ZH_DAYS
#undef NT_LANG_ZH_CN
#undef NT_LANG_ZH_TW
#undef NT_LANG_KO
#undef NT_NBSP
#undef NT_NNBSP
#undef NT_MINUS
