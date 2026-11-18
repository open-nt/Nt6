// MIT License - see LICENSE
//
// Linear symbologies: Code 128 (ISO/IEC 15417) and GS1-128, EAN-13, EAN-8, UPC-A and UPC-E
// (ISO/IEC 15420), Code 39 (ISO/IEC 16388) and Interleaved 2 of 5 with ITF-14 (ISO/IEC 16390).
// Each encoder produces one row of modules (1 = bar) and the human-readable text layout.
#include "qbarcode_p.h"

#include <algorithm>
#include <array>
#include <vector>

namespace QtBarcodePrivate {

namespace {

// ---------------------------------------------------------------------------------------------
// Code 128

// Bar and space widths of the 107 symbols: values 0 to 102, the three start codes and the stop
// code (which alone has seven elements).
constexpr std::array<const char *, 107> kCode128 = {
    "212222", "222122", "222221", "121223", "121322", "131222", "122213", "122312",  "132212", "221213", "221312",
    "231212", "112232", "122132", "122231", "113222", "123122", "123221", "223211",  "221132", "221231", "213212",
    "223112", "312131", "311222", "321122", "321221", "312212", "322112", "322211",  "212123", "212321", "232121",
    "111323", "131123", "131321", "112313", "132113", "132311", "211313", "231113",  "231311", "112133", "112331",
    "132131", "113123", "113321", "133121", "313121", "211331", "231131", "213113",  "213311", "213131", "311123",
    "311321", "331121", "312113", "312311", "332111", "314111", "221411", "431111",  "111224", "111422", "121124",
    "121421", "141122", "141221", "112214", "112412", "122114", "122411", "142112",  "142211", "241211", "221114",
    "413111", "241112", "134111", "111242", "121142", "121241", "114212", "124112",  "124211", "411212", "421112",
    "421211", "212141", "214121", "412121", "111143", "111341", "131141", "114113",  "114311", "411113", "411311",
    "113141", "114131", "311141", "411131", "211412", "211214", "211232", "2331112",
};

constexpr int kFnc1Value = 102;
constexpr int kCodeC = 99;
constexpr int kCodeB = 100;
constexpr int kCodeA = 101;
constexpr int kStartA = 103;
constexpr int kStartB = 104;
constexpr int kStartC = 105;
constexpr int kStop = 106;

enum class CodeSet { A, B, C };

bool isDigit(int c) { return c >= '0' && c <= '9'; }

int digitRun(const std::vector<int> &chars, std::size_t from) {
    int run = 0;
    for (std::size_t i = from; i < chars.size() && isDigit(chars[i]); ++i)
        ++run;
    return run;
}

// Code set C pays off for at least four digits at the start or end of the data and six elsewhere,
// or for data that is an even number of digits.
bool worthCodeC(const std::vector<int> &chars, std::size_t from, bool atStart) {
    const int run = digitRun(chars, from);
    const bool atEnd = from + std::size_t(run) == chars.size();
    return run >= (atStart || atEnd ? 4 : 6) || (atStart && atEnd && run >= 2 && run % 2 == 0);
}

QByteArray widthsToBars(const char *widths, int narrow = 1, int wide = 1) {
    QByteArray bars;
    bool bar = true;
    for (const char *w = widths; *w != '\0'; ++w, bar = !bar) {
        int width = *w - '0';
        if (*w == 'n')
            width = narrow;
        else if (*w == 'w')
            width = wide;
        bars.append(qsizetype(width), char(bar ? 1 : 0));
    }
    return bars;
}

QByteArray code128Bars(const std::vector<int> &chars) {
    std::size_t first = 0;
    while (first < chars.size() && chars[first] == kFnc1)
        ++first;

    std::vector<int> values;
    CodeSet set = CodeSet::B;
    if (first < chars.size() && worthCodeC(chars, first, true)) {
        set = CodeSet::C;
        values.push_back(kStartC);
    } else if (first < chars.size() && chars[first] < 32) {
        set = CodeSet::A;
        values.push_back(kStartA);
    } else {
        values.push_back(kStartB);
    }

    for (std::size_t i = 0; i < chars.size();) {
        const int c = chars[i];
        if (c == kFnc1) {
            values.push_back(kFnc1Value);
            ++i;
            continue;
        }
        if (set == CodeSet::C) {
            if (digitRun(chars, i) >= 2) {
                values.push_back((c - '0') * 10 + (chars[i + 1] - '0'));
                i += 2;
                continue;
            }
            set = c < 32 ? CodeSet::A : CodeSet::B;
            values.push_back(set == CodeSet::A ? kCodeA : kCodeB);
            continue;
        }
        if (worthCodeC(chars, i, false)) {
            // Keep an odd leading digit in the current set so that code set C gets pairs.
            if (digitRun(chars, i) % 2 != 0) {
                values.push_back(c - 32);
                ++i;
            }
            set = CodeSet::C;
            values.push_back(kCodeC);
            continue;
        }
        if (c < 32 && set == CodeSet::B) {
            set = CodeSet::A;
            values.push_back(kCodeA);
        } else if (c >= 96 && set == CodeSet::A) {
            set = CodeSet::B;
            values.push_back(kCodeB);
        }
        values.push_back(c < 32 ? c + 64 : c - 32);
        ++i;
    }

    int checksum = values.front();
    for (std::size_t i = 1; i < values.size(); ++i)
        checksum += values[i] * int(i);
    values.push_back(checksum % 103);
    values.push_back(kStop);

    QByteArray bars;
    for (int value : values)
        bars += widthsToBars(kCode128[std::size_t(value)]);
    return bars;
}

int linearHeight(const QByteArray &bars) { return std::max(32, int(bars.size()) / 4); }

void setCentredText(QBarcodePrivate *d, const QString &text) {
    d->text = text;
    d->textItems = {QBarcodeTextItem{d->width / 2.0, text, false}};
}

// GS1 Application Identifiers whose length is fixed, by their first two digits, with the length
// of the AI and value together (GS1 General Specifications, figure 7.8.5-2).
struct FixedLength {
    const char *prefix;
    int length;
};
constexpr std::array<FixedLength, 22> kFixedLength = {{
    {"00", 20}, {"01", 16}, {"02", 16}, {"03", 16}, {"04", 18}, {"11", 8},  {"12", 8}, {"13", 8},
    {"14", 8},  {"15", 8},  {"16", 8},  {"17", 8},  {"18", 8},  {"19", 8},  {"20", 4}, {"31", 10},
    {"32", 10}, {"33", 10}, {"34", 10}, {"35", 10}, {"36", 10}, {"41", 16},
}};

int fixedLength(const QByteArray &ai) {
    for (const FixedLength &f : kFixedLength) {
        if (ai.startsWith(f.prefix))
            return f.length;
    }
    return 0;
}

// ---------------------------------------------------------------------------------------------
// EAN and UPC

constexpr std::array<const char *, 10> kEanL = {"0001101", "0011001", "0010011", "0111101", "0100011",
                                                "0110001", "0101111", "0111011", "0110111", "0001011"};
constexpr std::array<const char *, 10> kEanG = {"0100111", "0110011", "0011011", "0100001", "0011101",
                                                "0111001", "0000101", "0010001", "0001001", "0010111"};
constexpr std::array<const char *, 10> kEanParity = {"LLLLLL", "LLGLGG", "LLGGLG", "LLGGGL", "LGLLGG",
                                                     "LGGLLG", "LGGGLL", "LGLGLG", "LGLGGL", "LGGLGL"};
// UPC-E parity of the six digits for number system 0, by check digit; a 1 bit is G (even) parity,
// most significant bit first. Number system 1 uses the complement.
constexpr std::array<int, 10> kUpcEParity = {0x38, 0x34, 0x32, 0x31, 0x2C, 0x26, 0x23, 0x2A, 0x29, 0x25};

void appendPattern(QByteArray &bars, const char *pattern, bool invert = false) {
    for (const char *p = pattern; *p != '\0'; ++p)
        bars.append(char(((*p == '1') != invert) ? 1 : 0));
}

QByteArray ean13Bars(const QByteArray &digits) {
    auto digit = [&digits](int i) { return std::size_t(digits.at(i) - '0'); };
    QByteArray bars;
    appendPattern(bars, "101");
    const char *parity = kEanParity[digit(0)];
    for (int i = 1; i <= 6; ++i)
        appendPattern(bars, parity[i - 1] == 'L' ? kEanL[digit(i)] : kEanG[digit(i)]);
    appendPattern(bars, "01010");
    for (int i = 7; i <= 12; ++i)
        appendPattern(bars, kEanL[digit(i)], true);
    appendPattern(bars, "101");
    return bars;
}

// The UPC-A number (without check digit) that a number system and six UPC-E digits stand for.
QByteArray upcEToUpcA(char numberSystem, const QByteArray &six) {
    const char last = six.at(5);
    QByteArray result(1, numberSystem);
    if (last <= '2')
        result += six.left(2) + last + "0000" + six.mid(2, 3);
    else if (last == '3')
        result += six.left(3) + "00000" + six.mid(3, 2);
    else if (last == '4')
        result += six.left(4) + "00000" + six.at(4);
    else
        result += six.left(5) + "0000" + last;
    return result;
}

QByteArray guardMask(int width, std::initializer_list<std::pair<int, int>> ranges) {
    QByteArray mask(width, 0);
    for (const auto &[begin, end] : ranges) {
        for (int x = begin; x < end; ++x)
            mask[x] = 1;
    }
    return mask;
}

QString digitAt(const QByteArray &digits, int i) { return QString(QChar::fromLatin1(digits.at(i))); }

// ---------------------------------------------------------------------------------------------
// Code 39 and Interleaved 2 of 5

constexpr char kCode39Chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. *$/+%";
// Element widths (bar, space, ... bar) of each character above; w is wide and n narrow.
constexpr std::array<const char *, 44> kCode39 = {
    "nnnwwnwnn", "wnnwnnnnw", "nnwwnnnnw", "wnwwnnnnn", "nnnwwnnnw", "wnnwwnnnn", "nnwwwnnnn", "nnnwnnwnw", "wnnwnnwnn",
    "nnwwnnwnn", "wnnnnwnnw", "nnwnnwnnw", "wnwnnwnnn", "nnnnwwnnw", "wnnnwwnnn", "nnwnwwnnn", "nnnnnwwnw", "wnnnnwwnn",
    "nnwnnwwnn", "nnnnwwwnn", "wnnnnnnww", "nnwnnnnww", "wnwnnnnwn", "nnnnwnnww", "wnnnwnnwn", "nnwnwnnwn", "nnnnnnwww",
    "wnnnnnwwn", "nnwnnnwwn", "nnnnwnwwn", "wwnnnnnnw", "nwwnnnnnw", "wwwnnnnnn", "nwnnwnnnw", "wwnnwnnnn", "nwwnwnnnn",
    "nwnnnnwnw", "wwnnnnwnn", "nwwnnnwnn", "nwnnwnwnn", "nwnwnwnnn", "nwnwnnnwn", "nwnnnwnwn", "nnnwnwnwn",
};

constexpr std::array<const char *, 10> kItf = {"nnwwn", "wnnnw", "nwnnw", "wwnnn", "nnwnw",
                                               "wnwnn", "nwwnn", "nnnww", "wnnwn", "nwnwn"};

constexpr int kWide = 3;

} // namespace

void encodeCode128(QBarcodePrivate *d) {
    const QByteArray &data = d->data;
    if (data.isEmpty()) {
        d->fail(QStringLiteral("Code 128 needs at least one character"));
        return;
    }
    std::vector<int> chars;
    QString text;
    for (char c : data) {
        if (quint8(c) > 127) {
            d->fail(QStringLiteral("Code 128 encodes only ASCII characters"));
            return;
        }
        chars.push_back(c);
        text += QChar::fromLatin1(c < 32 ? ' ' : c);
    }
    const QByteArray bars = code128Bars(chars);
    d->setBars(bars, linearHeight(bars));
    setCentredText(d, text);
}

QString parseGs1(const QByteArray &data, std::vector<int> *chars, QString *text) {
    QString syntax = QStringLiteral("GS1 data is written as (AI)value, for example (01)09501101530003");
    if (!data.startsWith('('))
        return syntax;
    chars->push_back(kFnc1);
    qsizetype i = 0;
    while (i < data.size()) {
        const qsizetype close = data.indexOf(')', i);
        if (data.at(i) != '(' || close < 0)
            return syntax;
        const QByteArray ai = data.mid(i + 1, close - i - 1);
        qsizetype next = data.indexOf('(', close);
        if (next < 0)
            next = data.size();
        const QByteArray value = data.mid(close + 1, next - close - 1);
        if (ai.size() < 2 || ai.size() > 4 || !allDigits(ai))
            return QStringLiteral("\"%1\" is not a GS1 Application Identifier").arg(QString::fromLatin1(ai));
        if (value.isEmpty() || std::any_of(value.cbegin(), value.cend(), [](char c) { return c < 32 || c > 126; }))
            return QStringLiteral("The value of AI (%1) is empty or not printable ASCII").arg(QString::fromLatin1(ai));
        const int length = fixedLength(ai);
        if (length > 0 && ai.size() + value.size() != length) {
            return QStringLiteral("The value of AI (%1) must have %2 characters")
                .arg(QString::fromLatin1(ai))
                .arg(length - ai.size());
        }
        if ((ai == "00" || ai == "01" || ai == "02") &&
            (!allDigits(value) || mod10CheckDigit(value.left(value.size() - 1)) != value.back() - '0'))
            return QStringLiteral("The check digit of AI (%1) is wrong").arg(QString::fromLatin1(ai));
        for (char c : ai + value)
            chars->push_back(c);
        // A variable-length field ends with FNC1 unless it is the last one.
        if (length == 0 && next < data.size())
            chars->push_back(kFnc1);
        *text += QLatin1Char('(') + QString::fromLatin1(ai) + QLatin1Char(')') + QString::fromLatin1(value);
        i = next;
    }
    return QString();
}

void encodeGs1Code128(QBarcodePrivate *d) {
    std::vector<int> chars;
    QString text;
    const QString error = parseGs1(d->data, &chars, &text);
    if (!error.isEmpty()) {
        d->fail(error);
        return;
    }
    const QByteArray bars = code128Bars(chars);
    d->setBars(bars, linearHeight(bars));
    setCentredText(d, text);
}

void encodeEanUpc(QBarcodePrivate *d) {
    const QByteArray input = d->data;
    QString name;
    int length = 0;
    switch (d->symbology) {
    case QBarcode::Ean13:
        name = QStringLiteral("EAN-13");
        length = 13;
        break;
    case QBarcode::Ean8:
        name = QStringLiteral("EAN-8");
        length = 8;
        break;
    case QBarcode::UpcA:
        name = QStringLiteral("UPC-A");
        length = 12;
        break;
    default:
        name = QStringLiteral("UPC-E");
        length = 8;
        break;
    }
    if (!allDigits(input)) {
        d->fail(QStringLiteral("%1 encodes only digits").arg(name));
        return;
    }

    QByteArray body;   // the digits without the check digit
    QByteArray parent; // the number the check digit is computed from
    if (d->symbology == QBarcode::UpcE) {
        if (input.size() < 6 || input.size() > 8) {
            d->fail(QStringLiteral("UPC-E needs 6, 7 or 8 digits"));
            return;
        }
        body = input.size() == 6 ? "0" + input : input.left(7);
        if (body.at(0) != '0' && body.at(0) != '1') {
            d->fail(QStringLiteral("The UPC-E number system must be 0 or 1"));
            return;
        }
        parent = upcEToUpcA(body.at(0), body.mid(1));
    } else {
        if (input.size() != length && input.size() != length - 1) {
            d->fail(QStringLiteral("%1 needs %2 or %3 digits").arg(name).arg(length - 1).arg(length));
            return;
        }
        body = input.left(length - 1);
        parent = body;
    }
    const int check = mod10CheckDigit(parent);
    if (input.size() == length && input.back() - '0' != check) {
        d->fail(QStringLiteral("The %1 check digit is wrong").arg(name));
        return;
    }
    const QByteArray digits = body + char('0' + check);
    d->data = digits;
    d->text = QString::fromLatin1(digits);
    auto digit = [&digits](int i) { return std::size_t(digits.at(i) - '0'); };
    auto centre = [](int origin, int slot) { return origin + 7 * slot + 3.5; };

    QByteArray bars;
    QList<QBarcodeTextItem> items;
    switch (d->symbology) {
    case QBarcode::Ean13:
        bars = ean13Bars(digits);
        d->guards = guardMask(95, {{0, 3}, {45, 50}, {92, 95}});
        items.append({-5.0, digitAt(digits, 0), false});
        for (int i = 1; i <= 6; ++i)
            items.append({centre(3, i - 1), digitAt(digits, i), false});
        for (int i = 7; i <= 12; ++i)
            items.append({centre(50, i - 7), digitAt(digits, i), false});
        break;
    case QBarcode::UpcA:
        bars = ean13Bars("0" + digits);
        d->guards = guardMask(95, {{0, 10}, {45, 50}, {85, 95}});
        items.append({-5.0, digitAt(digits, 0), true});
        for (int i = 1; i <= 5; ++i)
            items.append({centre(3, i), digitAt(digits, i), false});
        for (int i = 6; i <= 10; ++i)
            items.append({centre(50, i - 6), digitAt(digits, i), false});
        items.append({100.0, digitAt(digits, 11), true});
        break;
    case QBarcode::Ean8:
        appendPattern(bars, "101");
        for (int i = 0; i < 4; ++i)
            appendPattern(bars, kEanL[digit(i)]);
        appendPattern(bars, "01010");
        for (int i = 4; i < 8; ++i)
            appendPattern(bars, kEanL[digit(i)], true);
        appendPattern(bars, "101");
        d->guards = guardMask(67, {{0, 3}, {31, 36}, {64, 67}});
        for (int i = 0; i < 4; ++i)
            items.append({centre(3, i), digitAt(digits, i), false});
        for (int i = 4; i < 8; ++i)
            items.append({centre(36, i - 4), digitAt(digits, i), false});
        break;
    default: {
        int parity = kUpcEParity[std::size_t(check)];
        if (digits.at(0) == '1')
            parity ^= 0x3F;
        appendPattern(bars, "101");
        for (int i = 0; i < 6; ++i) {
            const std::size_t value = digit(i + 1);
            appendPattern(bars, ((parity >> (5 - i)) & 1) != 0 ? kEanG[value] : kEanL[value]);
        }
        appendPattern(bars, "010101");
        d->guards = guardMask(51, {{0, 3}, {45, 51}});
        items.append({-5.0, digitAt(digits, 0), true});
        for (int i = 1; i <= 6; ++i)
            items.append({centre(3, i - 1), digitAt(digits, i), false});
        items.append({56.0, digitAt(digits, 7), true});
        break;
    }
    }
    const int height = d->symbology == QBarcode::Ean8 ? 55 : 69;
    d->setBars(bars, height);
    d->textItems = items;
}

void encodeCode39(QBarcodePrivate *d) {
    const QByteArray &data = d->data;
    if (data.isEmpty()) {
        d->fail(QStringLiteral("Code 39 needs at least one character"));
        return;
    }
    auto index = [](char c) {
        for (int i = 0; kCode39Chars[i] != '\0'; ++i) {
            if (kCode39Chars[i] == c)
                return i;
        }
        return -1;
    };
    QByteArray bars;
    const QByteArray framed = '*' + data + '*';
    for (qsizetype i = 0; i < framed.size(); ++i) {
        const char c = framed.at(i);
        const int k = index(c);
        if (k < 0 || (c == '*' && i != 0 && i != framed.size() - 1)) {
            d->fail(QStringLiteral("Code 39 encodes only 0-9, A-Z, space and -.$/+%"));
            return;
        }
        if (i > 0)
            bars.append(char(0));
        bars += widthsToBars(kCode39[std::size_t(k)], 1, kWide);
    }
    d->setBars(bars, linearHeight(bars));
    setCentredText(d, QString::fromLatin1(data));
}

void encodeInterleaved2Of5(QBarcodePrivate *d) {
    QByteArray digits = d->data;
    const bool itf14 = d->symbology == QBarcode::Itf14;
    if (digits.isEmpty() || !allDigits(digits)) {
        d->fail(itf14 ? QStringLiteral("ITF-14 encodes only digits")
                      : QStringLiteral("Interleaved 2 of 5 encodes only digits"));
        return;
    }
    if (itf14) {
        if (digits.size() != 13 && digits.size() != 14) {
            d->fail(QStringLiteral("ITF-14 needs 13 or 14 digits"));
            return;
        }
        const int check = mod10CheckDigit(digits.left(13));
        if (digits.size() == 14 && digits.back() - '0' != check) {
            d->fail(QStringLiteral("The ITF-14 check digit is wrong"));
            return;
        }
        digits = digits.left(13) + char('0' + check);
    } else if (digits.size() % 2 != 0) {
        digits.prepend('0');
    }
    d->data = digits;

    QByteArray bars;
    bars += widthsToBars("1111");
    for (qsizetype i = 0; i < digits.size(); i += 2) {
        const char *first = kItf[std::size_t(digits.at(i) - '0')];
        const char *second = kItf[std::size_t(digits.at(i + 1) - '0')];
        const char pair[] = {first[0], second[0], first[1], second[1], first[2], second[2],
                             first[3], second[3], first[4], second[4], '\0'};
        bars += widthsToBars(pair, 1, kWide);
    }
    bars += widthsToBars("w11", 1, kWide);
    if (itf14) {
        constexpr int kBearer = 4;
        d->setBars(bars, 40 + 2 * kBearer);
        d->bearer = kBearer;
    } else {
        d->setBars(bars, linearHeight(bars));
    }
    setCentredText(d, QString::fromLatin1(digits));
}

} // namespace QtBarcodePrivate
