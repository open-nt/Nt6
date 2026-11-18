// MIT License - see LICENSE
//
// Nt Barcode is tested by reading its symbols back. The QR reader below works only from the
// module grid: it finds the function patterns with its own layout code, checks the format and
// version information against their BCH codes, unmasks, de-interleaves the blocks, verifies every
// Reed-Solomon block with syndromes computed from log tables, and decodes the segment. Linear
// symbols are split into bar widths and decoded the same way. Data Matrix and Aztec are checked for
// their fixed patterns and sizes, and every symbology has a fingerprint of a module grid that
// zxing-cpp 2.3 decoded correctly when it was recorded.
#include <QtBarcode>
#include <QtCore>
#include <QtGui/qpainter.h>

#include <array>
#include <cstdio>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

// ---------------------------------------------------------------------------------------------
// GF(256) with log tables (the encoder multiplies bit by bit, so this is a second implementation).

struct Gf {
    std::array<int, 512> exp{};
    std::array<int, 256> log{};
    Gf() {
        int x = 1;
        for (int i = 0; i < 255; ++i) {
            exp[std::size_t(i)] = x;
            log[std::size_t(x)] = i;
            x <<= 1;
            if (x & 0x100)
                x ^= 0x11D;
        }
        for (int i = 255; i < 512; ++i)
            exp[std::size_t(i)] = exp[std::size_t(i - 255)];
    }
    int mul(int a, int b) const {
        return a == 0 || b == 0 ? 0 : exp[std::size_t(log[std::size_t(a)] + log[std::size_t(b)])];
    }
};

const Gf &gf() {
    static const Gf g;
    return g;
}

bool syndromesZero(const std::vector<int> &block, int eccLength) {
    for (int i = 0; i < eccLength; ++i) {
        const int root = gf().exp[std::size_t(i)];
        int s = 0;
        for (int c : block)
            s = gf().mul(s, root) ^ c;
        if (s != 0)
            return false;
    }
    return true;
}

// Polynomial remainder of value * x^shift modulo generator, for the BCH codes.
int bchRemainder(int value, int shift, int generator, int generatorDegree) {
    int r = value << shift;
    for (int bit = generatorDegree + shift - 1; bit >= generatorDegree; --bit) {
        if (r & (1 << bit))
            r ^= generator << (bit - generatorDegree);
    }
    return r;
}

// ---------------------------------------------------------------------------------------------
// QR reader

// Structure tables of ISO/IEC 18004 Table 9 (error correction codewords per block and number of
// blocks for levels L, M, Q, H).
constexpr int kEcc[4][41] = {
    {0,  7,  10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28,
     28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {0,  10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26,
     26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},
    {0,  13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30,
     28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {0,  17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28,
     30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
};
constexpr int kBlocks[4][41] = {
    {0, 1, 1, 1,  1,  1,  2,  2,  2,  2,  4,  4,  4,  4,  4,  6,  6,  6,  6,  7, 8,
     8, 9, 9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
    {0,  1,  1,  1,  2,  2,  4,  4,  4,  5,  5,  5,  8,  9,  9,  10, 10, 11, 13, 14, 16,
     17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
    {0,  1,  1,  2,  2,  4,  4,  6,  6,  8,  8,  8,  10, 12, 16, 12, 17, 16, 18, 21, 20,
     23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},
    {0,  1,  1,  2,  4,  4,  4,  5,  6,  8,  8,  11, 11, 16, 16, 18, 16, 19, 21, 25, 25,
     25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},
};

std::vector<int> alignmentCenters(int version) {
    if (version == 1)
        return {};
    const int n = version / 7 + 2;
    const int step = (version * 8 + n * 3 + 5) / (n * 4 - 4) * 2;
    std::vector<int> result;
    for (int i = 0, p = version * 4 + 10; i < n - 1; ++i, p -= step)
        result.insert(result.begin(), p);
    result.insert(result.begin(), 6);
    return result;
}

bool maskBit(int mask, int i, int j) { // i = row, j = column, as in the standard
    switch (mask) {
    case 0:
        return (i + j) % 2 == 0;
    case 1:
        return i % 2 == 0;
    case 2:
        return j % 3 == 0;
    case 3:
        return (i + j) % 3 == 0;
    case 4:
        return (i / 2 + j / 3) % 2 == 0;
    case 5:
        return (i * j) % 2 + (i * j) % 3 == 0;
    case 6:
        return ((i * j) % 2 + (i * j) % 3) % 2 == 0;
    default:
        return ((i * j) % 3 + (i + j) % 2) % 2 == 0;
    }
}

struct QrResult {
    QString error;
    int version = 0;
    int level = -1;
    int mask = -1;
    QByteArray data;
};

QrResult readQr(const QBarcode &code) {
    QrResult r;
    const int size = code.size().width();
    if (size < 21 || (size - 17) % 4 != 0 || code.size().height() != size) {
        r.error = QStringLiteral("bad size");
        return r;
    }
    const int version = (size - 17) / 4;
    r.version = version;
    auto dark = [&code](int x, int y) { return code.isDark(x, y); };

    // Finder patterns.
    for (auto [cx, cy] : {std::pair{3, 3}, std::pair{size - 4, 3}, std::pair{3, size - 4}}) {
        for (int dy = -3; dy <= 3; ++dy) {
            for (int dx = -3; dx <= 3; ++dx) {
                const int d = std::max(std::abs(dx), std::abs(dy));
                if (dark(cx + dx, cy + dy) != (d != 2)) {
                    r.error = QStringLiteral("finder pattern");
                    return r;
                }
            }
        }
    }

    // Format information, both copies.
    int first = 0;
    int second = 0;
    for (int i = 0; i < 15; ++i) {
        int x = 0;
        int y = 0;
        if (i <= 5) {
            x = 8;
            y = i;
        } else if (i == 6) {
            x = 8;
            y = 7;
        } else if (i == 7) {
            x = 8;
            y = 8;
        } else if (i == 8) {
            x = 7;
            y = 8;
        } else {
            x = 14 - i;
            y = 8;
        }
        first |= int(dark(x, y)) << i;
        second |= int(i < 8 ? dark(size - 1 - i, 8) : dark(8, size - 15 + i)) << i;
    }
    if (first != second || !dark(8, size - 8)) {
        r.error = QStringLiteral("format copies differ");
        return r;
    }
    int format = -1;
    for (int d = 0; d < 32; ++d) {
        if (((d << 10 | bchRemainder(d, 10, 0x537, 10)) ^ 0x5412) == first)
            format = d;
    }
    if (format < 0) {
        r.error = QStringLiteral("format BCH");
        return r;
    }
    r.level = std::array<int, 4>{1, 0, 3, 2}[std::size_t(format >> 3)]; // bits 01=L 00=M 11=Q 10=H
    r.mask = format & 7;

    // Version information.
    if (version >= 7) {
        int a = 0;
        int b = 0;
        for (int i = 0; i < 18; ++i) {
            a |= int(dark(size - 11 + i % 3, i / 3)) << i;
            b |= int(dark(i / 3, size - 11 + i % 3)) << i;
        }
        if (a != b || a != (version << 12 | bchRemainder(version, 12, 0x1F25, 12))) {
            r.error = QStringLiteral("version information");
            return r;
        }
    }

    // Function modules.
    std::vector<bool> function(std::size_t(size * size), false);
    auto mark = [&](int x, int y) { function[std::size_t(y * size + x)] = true; };
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if ((x < 9 && y < 9) || (x >= size - 8 && y < 9) || (x < 9 && y >= size - 8) || x == 6 || y == 6)
                mark(x, y);
            if (version >= 7 &&
                ((y < 6 && x >= size - 11 && x < size - 8) || (x < 6 && y >= size - 11 && y < size - 8)))
                mark(x, y);
        }
    }
    const std::vector<int> centers = alignmentCenters(version);
    for (std::size_t i = 0; i < centers.size(); ++i) {
        for (std::size_t j = 0; j < centers.size(); ++j) {
            const std::size_t last = centers.size() - 1;
            if ((i == 0 && j == 0) || (i == 0 && j == last) || (i == last && j == 0))
                continue;
            for (int dy = -2; dy <= 2; ++dy) {
                for (int dx = -2; dx <= 2; ++dx) {
                    const int x = centers[i] + dx;
                    const int y = centers[j] + dy;
                    if (dark(x, y) != (std::max(std::abs(dx), std::abs(dy)) != 1)) {
                        r.error = QStringLiteral("alignment pattern");
                        return r;
                    }
                    mark(x, y);
                }
            }
        }
    }

    // Codewords, upward and downward in two-module columns from the right.
    std::vector<int> codewords;
    int bits = 0;
    int current = 0;
    for (int right = size - 1; right > 0; right -= 2) {
        if (right == 6)
            --right;
        const bool up = ((size - 1 - right) / 2) % 2 == 0;
        for (int k = 0; k < size; ++k) {
            const int y = up ? size - 1 - k : k;
            for (int x : {right, right - 1}) {
                if (function[std::size_t(y * size + x)])
                    continue;
                current = current << 1 | int(dark(x, y) != maskBit(r.mask, y, x));
                if (++bits == 8) {
                    codewords.push_back(current);
                    bits = 0;
                    current = 0;
                }
            }
        }
    }

    const int total = int(codewords.size());
    const int blocks = kBlocks[r.level][version];
    const int ecc = kEcc[r.level][version];
    const int shortLength = total / blocks;
    const int shortBlocks = blocks - total % blocks;
    std::vector<std::vector<int>> data(static_cast<std::size_t>(blocks));
    std::vector<std::vector<int>> parity(static_cast<std::size_t>(blocks));
    std::size_t k = 0;
    for (int i = 0; i < shortLength - ecc + 1; ++i) {
        for (int b = 0; b < blocks; ++b) {
            if (i < shortLength - ecc || b >= shortBlocks)
                data[std::size_t(b)].push_back(codewords[k++]);
        }
    }
    for (int i = 0; i < ecc; ++i) {
        for (int b = 0; b < blocks; ++b)
            parity[std::size_t(b)].push_back(codewords[k++]);
    }
    std::vector<int> stream;
    for (int b = 0; b < blocks; ++b) {
        std::vector<int> block = data[std::size_t(b)];
        block.insert(block.end(), parity[std::size_t(b)].begin(), parity[std::size_t(b)].end());
        if (!syndromesZero(block, ecc)) {
            r.error = QStringLiteral("Reed-Solomon block %1").arg(b);
            return r;
        }
        stream.insert(stream.end(), data[std::size_t(b)].begin(), data[std::size_t(b)].end());
    }

    // Segment.
    std::size_t pos = 0;
    auto take = [&](int n) {
        int v = 0;
        for (int i = 0; i < n; ++i, ++pos)
            v = v << 1 | ((stream[pos / 8] >> (7 - int(pos % 8))) & 1);
        return v;
    };
    const std::size_t available = stream.size() * 8;
    const int band = version <= 9 ? 0 : (version <= 26 ? 1 : 2);
    while (pos + 4 <= available) {
        const int mode = take(4);
        if (mode == 0)
            break;
        if (mode == 1) {
            int count = take(std::array<int, 3>{10, 12, 14}[std::size_t(band)]);
            for (; count >= 3; count -= 3)
                r.data += QByteArray::number(take(10)).rightJustified(3, '0');
            if (count == 2)
                r.data += QByteArray::number(take(7)).rightJustified(2, '0');
            else if (count == 1)
                r.data += QByteArray::number(take(4));
        } else if (mode == 2) {
            static const char table[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
            int count = take(std::array<int, 3>{9, 11, 13}[std::size_t(band)]);
            for (; count >= 2; count -= 2) {
                const int v = take(11);
                r.data += table[v / 45];
                r.data += table[v % 45];
            }
            if (count == 1)
                r.data += table[take(6)];
        } else if (mode == 4) {
            const int count = take(std::array<int, 3>{8, 16, 16}[std::size_t(band)]);
            for (int i = 0; i < count; ++i)
                r.data += char(take(8));
        } else {
            r.error = QStringLiteral("unexpected mode %1").arg(mode);
            return r;
        }
    }
    return r;
}

void checkQrRoundTrip(const QByteArray &data, QBarcode::ErrorCorrectionLevel level) {
    const QBarcode code(data, QBarcode::QrCode, level);
    CHECK(!code.isNull());
    CHECK(code.errorString().isEmpty());
    const QrResult r = readQr(code);
    if (!r.error.isEmpty())
        std::printf("  QR read error: %s (version %d)\n", qPrintable(r.error), r.version);
    CHECK(r.error.isEmpty());
    CHECK(r.version == code.version());
    CHECK(r.level == int(level));
    CHECK(r.mask == code.maskPattern());
    CHECK(r.data == data);
}

// ---------------------------------------------------------------------------------------------
// Linear readers

std::vector<int> barWidths(const QBarcode &code) {
    std::vector<int> widths;
    const int width = code.size().width();
    const int row = code.size().height() / 2; // below the bearer bar of ITF-14
    for (int x = 0; x < width;) {
        const bool c = code.isDark(x, row);
        int run = 0;
        while (x < width && code.isDark(x, row) == c) {
            ++x;
            ++run;
        }
        widths.push_back(run);
    }
    return widths;
}

constexpr const char *kCode128[107] = {
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

// Returns the decoded text, or a string starting with '!' on error; startCode receives the start symbol.
QByteArray readCode128(const QBarcode &code, int *startCode = nullptr) {
    const std::vector<int> widths = barWidths(code);
    if (widths.size() < 19 || (widths.size() - 7) % 6 != 0)
        return "!widths";
    std::vector<int> values;
    for (std::size_t i = 0; i + 7 < widths.size(); i += 6) {
        QByteArray pattern;
        for (std::size_t k = 0; k < 6; ++k)
            pattern += char('0' + widths[i + k]);
        int value = -1;
        for (int v = 0; v < 106; ++v) {
            if (pattern == kCode128[v])
                value = v;
        }
        if (value < 0)
            return "!pattern";
        values.push_back(value);
    }
    QByteArray stop;
    for (std::size_t k = widths.size() - 7; k < widths.size(); ++k)
        stop += char('0' + widths[k]);
    if (stop != kCode128[106])
        return "!stop";
    int sum = values.front();
    for (std::size_t i = 1; i + 1 < values.size(); ++i)
        sum += values[i] * int(i);
    if (sum % 103 != values.back())
        return "!checksum";
    if (startCode != nullptr)
        *startCode = values.front();
    char set = values.front() == 103 ? 'A' : (values.front() == 104 ? 'B' : 'C');
    QByteArray text;
    for (std::size_t i = 1; i + 1 < values.size(); ++i) {
        const int v = values[i];
        if (set != 'C' && v < 96)
            text += char(set == 'A' && v >= 64 ? v - 64 : v + 32);
        else if (set == 'C' && v < 100)
            text += QByteArray::number(v).rightJustified(2, '0');
        else if (v == 99)
            set = 'C';
        else if (v == 100)
            set = 'B';
        else if (v == 101)
            set = 'A';
        else if (v == 102 && i == 1)
            continue; // FNC1 in the first position marks GS1-128
        else if (v == 102)
            text += '\x1d';
        else
            return "!function code";
    }
    return text;
}

constexpr const char *kEanL[10] = {"0001101", "0011001", "0010011", "0111101", "0100011",
                                   "0110001", "0101111", "0111011", "0110111", "0001011"};

QByteArray modulesAt(const QBarcode &code, int from, int count) {
    QByteArray s;
    for (int x = from; x < from + count; ++x)
        s += code.isDark(x, 0) ? '1' : '0';
    return s;
}

// Decodes an EAN symbol with only the L table: R is its complement and G the reverse of R.
QByteArray readEan(const QBarcode &code) {
    const int width = code.size().width();
    const int digits = width == 95 ? 6 : 4;
    if ((width != 95 && width != 67) || modulesAt(code, 0, 3) != "101" ||
        modulesAt(code, 3 + digits * 7, 5) != "01010" || modulesAt(code, width - 3, 3) != "101")
        return "!guards";
    QByteArray text;
    QByteArray parity;
    for (int half = 0; half < 2; ++half) {
        const int origin = half == 0 ? 3 : 3 + digits * 7 + 5;
        for (int i = 0; i < digits; ++i) {
            const QByteArray m = modulesAt(code, origin + i * 7, 7);
            int found = -1;
            for (int d = 0; d < 10; ++d) {
                QByteArray l = kEanL[d];
                QByteArray rr = l;
                for (char &c : rr)
                    c = c == '1' ? '0' : '1';
                QByteArray reversed;
                for (qsizetype k = rr.size() - 1; k >= 0; --k)
                    reversed += rr.at(k);
                if (half == 0 && m == l) {
                    found = d;
                    parity += 'L';
                } else if (half == 0 && m == reversed) {
                    found = d;
                    parity += 'G';
                } else if (half == 1 && m == rr) {
                    found = d;
                }
            }
            if (found < 0)
                return "!digit";
            text += char('0' + found);
        }
    }
    if (width == 95) {
        static const char *const kParity[10] = {"LLLLLL", "LLGLGG", "LLGGLG", "LLGGGL", "LGLLGG",
                                                "LGGLLG", "LGGGLL", "LGLGLG", "LGLGGL", "LGGLGL"};
        int first = -1;
        for (int d = 0; d < 10; ++d) {
            if (parity == kParity[d])
                first = d;
        }
        if (first < 0)
            return "!parity";
        text.prepend(char('0' + first));
    } else if (parity != "LLLL") {
        return "!parity";
    }
    return text;
}

// ---------------------------------------------------------------------------------------------

void testQr() {
    const QBarcode::ErrorCorrectionLevel levels[] = {QBarcode::LevelL, QBarcode::LevelM, QBarcode::LevelQ,
                                                     QBarcode::LevelH};
    for (auto level : levels) {
        checkQrRoundTrip("", level);
        checkQrRoundTrip("01234567", level);
        checkQrRoundTrip("HELLO WORLD", level);
        checkQrRoundTrip("https://open-nt.org/6.0.0/qbarcode.html?x=1&y=2", level);
    }

    // The worked example of ISO/IEC 18004 Annex I: "01234567" at level M is a version 1 symbol.
    QBarcode example("01234567", QBarcode::QrCode, QBarcode::LevelM);
    CHECK(example.version() == 1);
    CHECK(example.size() == QSize(21, 21));

    // Every version and level, with byte data that just fits in the next lower version.
    QRandomGenerator random(1234);
    for (int length : {20, 60, 120, 250, 400, 600, 900, 1300, 1700, 2200, 2900}) {
        QByteArray data(length, 0);
        for (char &c : data)
            c = char(random.bounded(256));
        for (auto level : levels) {
            if (QBarcode(data, QBarcode::QrCode, level).isNull())
                continue;
            checkQrRoundTrip(data, level);
        }
    }
    int lastVersion = 0;
    for (int length = 1; length <= 2953; length += 7) {
        const QBarcode code(QByteArray(length, 'a'), QBarcode::QrCode, QBarcode::LevelL);
        CHECK(code.version() >= lastVersion);
        if (code.version() != lastVersion) {
            const QrResult r = readQr(code);
            CHECK(r.error.isEmpty());
            CHECK(r.data == QByteArray(length, 'a'));
            lastVersion = code.version();
        }
    }
    CHECK(lastVersion == 40);

    // Capacities of ISO/IEC 18004 Table 7.
    struct Capacity {
        QBarcode::ErrorCorrectionLevel level;
        char fill;
        int version;
        int characters;
    };
    const Capacity capacities[] = {
        {QBarcode::LevelL, 'a', 1, 17},    {QBarcode::LevelM, 'a', 1, 14},    {QBarcode::LevelQ, 'a', 1, 11},
        {QBarcode::LevelH, 'a', 1, 7},     {QBarcode::LevelL, '7', 1, 41},    {QBarcode::LevelH, '7', 1, 17},
        {QBarcode::LevelL, 'A', 1, 25},    {QBarcode::LevelH, 'A', 1, 10},    {QBarcode::LevelM, 'a', 10, 213},
        {QBarcode::LevelH, 'A', 40, 1852}, {QBarcode::LevelH, '7', 40, 3057}, {QBarcode::LevelL, 'a', 40, 2953},
        {QBarcode::LevelL, 'A', 40, 4296}, {QBarcode::LevelL, '7', 40, 7089}, {QBarcode::LevelH, 'a', 40, 1273},
    };
    for (const Capacity &c : capacities) {
        const QBarcode fits(QByteArray(c.characters, c.fill), QBarcode::QrCode, c.level);
        const QBarcode over(QByteArray(c.characters + 1, c.fill), QBarcode::QrCode, c.level);
        CHECK(fits.version() == c.version);
        CHECK(over.version() == (c.version == 40 ? 0 : c.version + 1));
        if (c.version == 40) {
            CHECK(over.isNull());
            CHECK(!over.errorString().isEmpty());
        }
    }

    // A QString is encoded as UTF-8.
    const QString text = QStringLiteral("Grüße ✓");
    const QBarcode utf8(text);
    CHECK(utf8.data() == text.toUtf8());
    CHECK(readQr(utf8).data == text.toUtf8());

    // The mask with the lowest penalty is chosen, so not always the same one.
    QSet<int> masks;
    for (int i = 0; i < 40; ++i)
        masks.insert(QBarcode(QByteArray::number(i * 7919)).maskPattern());
    CHECK(masks.size() > 1);

    QBarcode null;
    CHECK(null.isNull());
    CHECK(null.size() == QSize(0, 0));
    CHECK(null.version() == 0);
    CHECK(null.maskPattern() == -1);
    CHECK(null.toImage().isNull());
    CHECK(null.toSvg().isEmpty());
    CHECK(!null.isDark(0, 0));
}

void testCode128() {
    const QByteArray samples[] = {"A",           "Nt",     "hello, world", "1234567890", "12345",
                                  "ABC12345678", "a1234b", "\tTAB\x01x",   "x123456y",   "lower\x1F"};
    for (const QByteArray &s : samples) {
        const QBarcode code(s, QBarcode::Code128);
        CHECK(!code.isNull());
        CHECK(code.version() == 0);
        CHECK(code.maskPattern() == -1);
        const QByteArray decoded = readCode128(code);
        if (decoded != s)
            std::printf("  Code 128 '%s' read as '%s'\n", s.constData(), decoded.constData());
        CHECK(decoded == s);
        CHECK(code.size().width() % 11 == 2);
    }

    // Digits use code set C: start, five pairs, check, stop.
    int start = 0;
    const QBarcode digits("1234567890", QBarcode::Code128);
    CHECK(readCode128(digits, &start) == "1234567890");
    CHECK(start == 105);
    CHECK(digits.size().width() == 11 * 7 + 13);
    CHECK(digits.size().height() == 32);
    CHECK(digits.isDark(0, 31) == digits.isDark(0, 0));
    CHECK(!digits.isDark(0, 32));
    CHECK(digits.quietZone() == 10);
    readCode128(QBarcode("abc", QBarcode::Code128), &start);
    CHECK(start == 104);
    readCode128(QBarcode("\x02"
                         "AB",
                         QBarcode::Code128),
                &start);
    CHECK(start == 103);

    CHECK(QBarcode("", QBarcode::Code128).isNull());
    CHECK(QBarcode(QStringLiteral("café"), QBarcode::Code128).isNull());
}

void testEan() {
    CHECK(QBarcode::checkDigit("400638133393") == 1);
    CHECK(QBarcode::checkDigit("9638507") == 4);
    CHECK(QBarcode::checkDigit("03600029145") == 2); // UPC-A
    CHECK(QBarcode::checkDigit("") == -1);
    CHECK(QBarcode::checkDigit("12a") == -1);

    const QBarcode ean13("400638133393", QBarcode::Ean13);
    CHECK(!ean13.isNull());
    CHECK(ean13.data() == "4006381333931");
    CHECK(ean13.size() == QSize(95, 69));
    CHECK(readEan(ean13) == "4006381333931");
    CHECK(QBarcode("4006381333931", QBarcode::Ean13).data() == "4006381333931");
    CHECK(QBarcode("4006381333932", QBarcode::Ean13).isNull());
    CHECK(QBarcode("40063813339", QBarcode::Ean13).isNull());
    CHECK(QBarcode("40063813339x", QBarcode::Ean13).isNull());
    for (int first = 0; first < 10; ++first) {
        const QByteArray body = QByteArray::number(first) + "12345678901";
        const QBarcode code(body, QBarcode::Ean13);
        CHECK(readEan(code) == body + char('0' + QBarcode::checkDigit(body)));
    }

    const QBarcode ean8("9638507", QBarcode::Ean8);
    CHECK(ean8.data() == "96385074");
    CHECK(ean8.size() == QSize(67, 55));
    CHECK(readEan(ean8) == "96385074");
    CHECK(ean8.quietZone() == 11);
    CHECK(QBarcode("96385075", QBarcode::Ean8).isNull());
    CHECK(QBarcode("96385075", QBarcode::Ean8).errorString().contains(QStringLiteral("check digit")));
}

void testRendering() {
    const QBarcode code("Nt rendering");
    const int n = code.size().width();

    const QImage image = code.toImage(3);
    CHECK(image.size() == QSize((n + 8) * 3, (n + 8) * 3));
    CHECK(image.pixel(0, 0) == qRgba(255, 255, 255, 255));
    CHECK(image.pixel(12, 12) == qRgba(0, 0, 0, 255)); // top-left corner of the finder pattern
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x)
            CHECK((image.pixel(x * 3 + 13, y * 3 + 13) == qRgba(0, 0, 0, 255)) == code.isDark(x, y));
    }

    const QImage tight = code.toImage(1, 0, QColor(10, 20, 30), QColor(0, 0, 0, 0));
    CHECK(tight.size() == QSize(n, n));
    CHECK(tight.pixel(0, 0) == qRgba(10, 20, 30, 255));
    CHECK(qAlpha(tight.pixel(7, 0)) == 0);

    // paint() at a non-integer scale covers the same modules as toImage().
    QImage painted(n * 5 + 3, n * 5 + 3, QImage::Format_ARGB32);
    painted.fill(Qt::white);
    {
        QPainter painter(&painted);
        code.paint(&painter, QRect(0, 0, painted.width(), painted.height()));
    }
    const double scale = double(painted.width()) / n;
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const QRgb pixel = painted.pixel(int((x + 0.5) * scale), int((y + 0.5) * scale));
            CHECK((qRed(pixel) < 128) == code.isDark(x, y));
        }
    }

    const QByteArray svg = code.toSvg(2);
    CHECK(svg.startsWith("<?xml"));
    CHECK(svg.contains("viewBox=\"0 0 " + QByteArray::number(n + 8) + ' ' + QByteArray::number(n + 8) + '"'));
    CHECK(svg.contains("width=\"" + QByteArray::number((n + 8) * 2) + '"'));
    CHECK(svg.contains("fill=\"#ffffff\""));
    CHECK(svg.contains("fill=\"#000000\""));
    CHECK(svg.trimmed().endsWith("</svg>"));
    // The dark area of the path equals the number of dark modules.
    long darkModules = 0;
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x)
            darkModules += code.isDark(x, y) ? 1 : 0;
    }
    long area = 0;
    const QByteArray path = svg.mid(svg.indexOf(" d=\"") + 4);
    for (const QByteArray &cmd : path.left(path.indexOf('"')).split('z')) {
        const qsizetype hpos = cmd.indexOf('h');
        const qsizetype vpos = cmd.indexOf('v');
        if (hpos > 0 && vpos > hpos)
            area += cmd.mid(hpos + 1, vpos - hpos - 1).toLong() *
                    cmd.mid(vpos + 1, cmd.lastIndexOf('h') - vpos - 1).toLong();
    }
    CHECK(area == darkModules);
    CHECK(!code.toSvg(1, 0, QColor(0, 0, 0), QColor(0, 0, 0, 0)).contains("<rect"));

    const QBarcode bars("4006381333931", QBarcode::Ean13);
    const QImage linear = bars.toImage(2, 0);
    CHECK(linear.size() == QSize(95 * 2, 69 * 2));
    CHECK(linear.pixel(0, 137) == qRgba(0, 0, 0, 255));
    CHECK(linear.pixel(2, 0) == qRgba(255, 255, 255, 255));
    CHECK(bars.toSvg().count('M') == 30); // two bars per guard and per digit
}

// ---------------------------------------------------------------------------------------------
// Code 39 and Interleaved 2 of 5 readers. The tables are written as bit masks (1 = wide element),
// unlike the n/w strings of the encoder.

constexpr char kCode39Chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. *$/+%";
constexpr int kCode39Masks[44] = {0x034, 0x121, 0x061, 0x160, 0x031, 0x130, 0x070, 0x025, 0x124, 0x064, 0x109,
                                  0x049, 0x148, 0x019, 0x118, 0x058, 0x00D, 0x10C, 0x04C, 0x01C, 0x103, 0x043,
                                  0x142, 0x013, 0x112, 0x052, 0x007, 0x106, 0x046, 0x016, 0x181, 0x0C1, 0x1C0,
                                  0x091, 0x190, 0x0D0, 0x085, 0x184, 0x0C4, 0x094, 0x0A8, 0x0A2, 0x08A, 0x02A};
constexpr int kItfMasks[10] = {0x06, 0x11, 0x09, 0x18, 0x05, 0x14, 0x0C, 0x03, 0x12, 0x0A};

QByteArray readCode39(const QBarcode &code) {
    const std::vector<int> widths = barWidths(code);
    if ((widths.size() + 1) % 10 != 0)
        return "!widths";
    QByteArray text;
    for (std::size_t i = 0; i < widths.size(); i += 10) {
        int mask = 0;
        for (std::size_t k = 0; k < 9; ++k) {
            if (widths[i + k] != 1 && widths[i + k] != 3)
                return "!width";
            mask = mask << 1 | (widths[i + k] == 3 ? 1 : 0);
        }
        if (i + 9 < widths.size() && widths[i + 9] != 1)
            return "!gap";
        int found = -1;
        for (int c = 0; c < 44; ++c) {
            if (kCode39Masks[c] == mask)
                found = c;
        }
        if (found < 0)
            return "!pattern";
        text += kCode39Chars[found];
    }
    if (!text.startsWith('*') || !text.endsWith('*'))
        return "!start/stop";
    return text.mid(1, text.size() - 2);
}

QByteArray readItf(const QBarcode &code) {
    const std::vector<int> widths = barWidths(code);
    if (widths.size() < 7 || std::vector<int>(widths.begin(), widths.begin() + 4) != std::vector<int>{1, 1, 1, 1} ||
        std::vector<int>(widths.end() - 3, widths.end()) != std::vector<int>{3, 1, 1} || (widths.size() - 7) % 10 != 0)
        return "!start/stop";
    QByteArray digits;
    for (std::size_t i = 4; i + 3 < widths.size(); i += 10) {
        int bars = 0;
        int spaces = 0;
        for (std::size_t k = 0; k < 5; ++k) {
            bars = bars << 1 | (widths[i + 2 * k] == 3 ? 1 : 0);
            spaces = spaces << 1 | (widths[i + 2 * k + 1] == 3 ? 1 : 0);
        }
        int first = -1;
        int second = -1;
        for (int d = 0; d < 10; ++d) {
            if (kItfMasks[d] == bars)
                first = d;
            if (kItfMasks[d] == spaces)
                second = d;
        }
        if (first < 0 || second < 0)
            return "!pattern";
        digits += char('0' + first);
        digits += char('0' + second);
    }
    return digits;
}

quint64 fingerprint(const QBarcode &code) {
    quint64 h = 1469598103934665603ULL;
    for (int y = 0; y < code.size().height(); ++y) {
        for (int x = 0; x < code.size().width(); ++x) {
            h ^= quint64(code.isDark(x, y) ? 1 : 0);
            h *= 1099511628211ULL;
        }
    }
    return h;
}

void testFingerprints() {
    struct Golden {
        const char *data;
        QBarcode::Symbology symbology;
        QBarcode::ErrorCorrectionLevel level;
        int width;
        int height;
        quint64 fingerprint;
    };
    // Each grid was decoded by zxing-cpp 2.3 to the data when it was recorded.
    const Golden golden[] = {
        {"https://open-nt.org", QBarcode::QrCode, QBarcode::LevelM, 25, 25, 0x1b309cde0ccfbfa9ULL},
        {"https://open-nt.org", QBarcode::DataMatrix, QBarcode::LevelM, 18, 18, 0x5a52350a02ea7cf6ULL},
        {"0123456789012345678901234567890123456789", QBarcode::DataMatrix, QBarcode::LevelM, 20, 20,
         0x76709ae3c8a05526ULL},
        {"Hello, Aztec!", QBarcode::Aztec, QBarcode::LevelM, 19, 19, 0x263fca5a34e88856ULL},
        {"Nt 6 Acorn: the quick brown fox jumps over the lazy dog 0123456789 THE QUICK BROWN FOX JUMPS OVER THE "
         "LAZY DOG",
         QBarcode::Aztec, QBarcode::LevelH, 37, 37, 0x3faf547214b12533ULL},
        {"(01)09501101530003(17)250101(10)ABC123", QBarcode::Gs1Code128, QBarcode::LevelM, 266, 66,
         0x2bd9dd8e9bca5ac7ULL},
        {"NT-6.0.5", QBarcode::Code39, QBarcode::LevelM, 159, 39, 0x7ca03cca4142e671ULL},
        {"1540014128876", QBarcode::Itf14, QBarcode::LevelM, 135, 48, 0x49edea1333e160f3ULL},
        {"03600029145", QBarcode::UpcA, QBarcode::LevelM, 95, 69, 0x78522d01daf14f0dULL},
        {"0425261", QBarcode::UpcE, QBarcode::LevelM, 51, 69, 0xc6eaa9f6aeb1d1dfULL},
    };
    for (const Golden &g : golden) {
        const QBarcode code(g.data, g.symbology, g.level);
        CHECK(code.size() == QSize(g.width, g.height));
        if (fingerprint(code) != g.fingerprint)
            std::printf("  fingerprint of '%s' changed: 0x%016llx\n", g.data,
                        static_cast<unsigned long long>(fingerprint(code)));
        CHECK(fingerprint(code) == g.fingerprint);
    }
}

void testDataMatrix() {
    const QBarcode small("ABC", QBarcode::DataMatrix);
    CHECK(small.size() == QSize(10, 10));
    CHECK(!small.isLinear());
    CHECK(small.version() == 0);
    CHECK(small.maskPattern() == -1);
    CHECK(small.quietZone() == 1);
    CHECK(small.text().isEmpty());
    CHECK(QBarcode("ABCD", QBarcode::DataMatrix).size() == QSize(12, 12));
    CHECK(small.options() == QBarcode::NoOption);
    CHECK(QBarcode("123456", QBarcode::DataMatrix).size() == QSize(10, 10));             // digit pairs
    CHECK(QBarcode(QStringLiteral("éé"), QBarcode::DataMatrix).size() == QSize(14, 14)); // Upper Shift
    // Capacities: Text packs three lower case letters into two codewords, Base 256 takes binary.
    CHECK(QBarcode(QByteArray(2335, 'x'), QBarcode::DataMatrix).size() == QSize(144, 144));
    CHECK(QBarcode(QByteArray(2336, 'x'), QBarcode::DataMatrix).isNull());
    CHECK(QBarcode(QByteArray(2335, 'A'), QBarcode::DataMatrix).size() == QSize(144, 144));
    CHECK(QBarcode(QByteArray(1555, '\xff'), QBarcode::DataMatrix).size() == QSize(144, 144));
    CHECK(QBarcode(QByteArray(1556, '\xff'), QBarcode::DataMatrix).isNull());
    // C40 makes 9 upper case letters take 8 codewords (latch, 3 pairs, unlatch) instead of 9.
    CHECK(QBarcode("ABCDEFGHI", QBarcode::DataMatrix).size() == QSize(14, 14));
    CHECK(QBarcode(QByteArray(3116, '7'), QBarcode::DataMatrix).size() == QSize(144, 144));
    CHECK(QBarcode(QByteArray(3117, '7'), QBarcode::DataMatrix).isNull());
    CHECK(!QBarcode(QByteArray(3117, '7'), QBarcode::DataMatrix).errorString().isEmpty());
    CHECK(QBarcode("", QBarcode::DataMatrix).size() == QSize(10, 10));

    // Finder (solid left and bottom) and clock (alternating top and right) of every data region.
    for (int length : {3, 70, 300}) {
        const QBarcode code(QByteArray(length, 'N'), QBarcode::DataMatrix);
        const int size = code.size().width();
        const int regions = size <= 26 ? 1 : (size <= 52 ? 2 : (size <= 104 ? 4 : 6));
        const int step = size / regions;
        for (int r = 0; r < regions; ++r) {
            for (int c = 0; c < regions; ++c) {
                const int top = r * step;
                const int left = c * step;
                for (int i = 0; i < step; ++i) {
                    CHECK(code.isDark(left, top + i));
                    CHECK(code.isDark(left + i, top + step - 1));
                    CHECK(code.isDark(left + i, top) == (i % 2 == 0));
                    CHECK(code.isDark(left + step - 1, top + i) == (i % 2 == 1));
                }
            }
        }
    }
}

void testAztec() {
    const QBarcode one("A", QBarcode::Aztec);
    CHECK(one.size() == QSize(15, 15));
    CHECK(one.version() == 1);
    CHECK(one.quietZone() == 0);
    // Compact bullseye: dark at even distances from the centre, up to 4.
    for (int y = 3; y <= 11; ++y) {
        for (int x = 3; x <= 11; ++x) {
            const int distance = std::max(std::abs(x - 7), std::abs(y - 7));
            CHECK(one.isDark(x, y) == (distance % 2 == 0));
        }
    }
    // Orientation marks at the corners of the mode message ring.
    CHECK(one.isDark(2, 2) && one.isDark(3, 2) && one.isDark(2, 3));
    CHECK(one.isDark(12, 2) && one.isDark(12, 3));
    CHECK(one.isDark(12, 11));

    // Full range: a 37 x 37 symbol has reference grid lines through the centre and 16 modules away.
    const QBarcode full(QByteArray(120, 'q'), QBarcode::Aztec, QBarcode::LevelH);
    CHECK(full.size().width() > 27);
    const int size = full.size().width();
    const int centre = size / 2;
    for (int offset : {-16, 0, 16}) {
        if (centre + offset < 0 || centre + offset >= size)
            continue;
        for (int k = centre % 2; k < size; k += 2) {
            CHECK(full.isDark(centre + offset, k));
            CHECK(full.isDark(k, centre + offset));
        }
    }

    const QByteArray text = "The quick brown fox jumps over the lazy dog";
    CHECK(QBarcode(text, QBarcode::Aztec, QBarcode::LevelL).size().width() <=
          QBarcode(text, QBarcode::Aztec, QBarcode::LevelH).size().width());
    CHECK(QBarcode("", QBarcode::Aztec).isNull());
    const QBarcode tooLong(QByteArray(5000, '\xff'), QBarcode::Aztec, QBarcode::LevelH);
    CHECK(tooLong.isNull());
    CHECK(!tooLong.errorString().isEmpty());
}

void testMoreLinear() {
    for (const QByteArray &s :
         {QByteArray("CODE39"), QByteArray("NT-6.0.5"), QByteArray("A B$/+%."), QByteArray("0")}) {
        const QBarcode code(s, QBarcode::Code39);
        CHECK(readCode39(code) == s);
        CHECK(code.text() == QString::fromLatin1(s));
    }
    CHECK(QBarcode("lower", QBarcode::Code39).isNull());
    CHECK(QBarcode("A*B", QBarcode::Code39).isNull());
    CHECK(QBarcode("", QBarcode::Code39).isNull());

    const QBarcode itf("123456", QBarcode::Interleaved2Of5);
    CHECK(readItf(itf) == "123456");
    const QBarcode odd("12345", QBarcode::Interleaved2Of5);
    CHECK(odd.data() == "012345");
    CHECK(readItf(odd) == "012345");
    CHECK(QBarcode("12a4", QBarcode::Interleaved2Of5).isNull());

    const QBarcode itf14("1540014128876", QBarcode::Itf14);
    CHECK(itf14.data() == QByteArray("1540014128876") + char('0' + QBarcode::checkDigit("1540014128876")));
    CHECK(readItf(itf14) == itf14.data());
    CHECK(itf14.isDark(0, 0) && itf14.isDark(itf14.size().width() - 1, itf14.size().height() - 1)); // bearer bars
    CHECK(QBarcode(itf14.data(), QBarcode::Itf14).data() == itf14.data());
    QByteArray wrong = itf14.data();
    wrong[13] = char('0' + (wrong.at(13) - '0' + 1) % 10);
    CHECK(QBarcode(wrong, QBarcode::Itf14).isNull());
    CHECK(QBarcode("123", QBarcode::Itf14).isNull());

    // UPC-A is EAN-13 with a leading zero.
    const QBarcode upcA("03600029145", QBarcode::UpcA);
    CHECK(upcA.data() == "036000291452");
    const QBarcode asEan("003600029145", QBarcode::Ean13);
    CHECK(fingerprint(upcA) == fingerprint(asEan));
    CHECK(readEan(upcA) == "0036000291452");
    CHECK(QBarcode("036000291453", QBarcode::UpcA).isNull());

    // UPC-E: the check digit comes from the UPC-A number the digits stand for.
    struct UpcE {
        const char *in;
        const char *out;
    };
    for (const UpcE &u : {UpcE{"0425261", "04252614"}, UpcE{"654320", "06543208"}, UpcE{"1234565", "12345656"},
                          UpcE{"0112233", "01122333"}}) {
        const QBarcode code(u.in, QBarcode::UpcE);
        CHECK(code.data() == u.out);
        CHECK(code.size() == QSize(51, 69));
        CHECK(QBarcode(u.out, QBarcode::UpcE).data() == u.out);
    }
    CHECK(QBarcode("2425261", QBarcode::UpcE).isNull());
    CHECK(QBarcode("04252615", QBarcode::UpcE).isNull());
    CHECK(QBarcode("04252", QBarcode::UpcE).isNull());

    // GS1-128: FNC1 first, and after each variable-length field that is followed by another.
    const QBarcode gs1("(01)09501101530003(17)250101(10)ABC123", QBarcode::Gs1Code128);
    CHECK(readCode128(gs1) == "01095011015300031725010110ABC123");
    CHECK(gs1.text() == QStringLiteral("(01)09501101530003(17)250101(10)ABC123"));
    CHECK(readCode128(QBarcode("(10)LOT42(21)SER9(01)09501101530003", QBarcode::Gs1Code128)) == "10LOT42\x1d"
                                                                                                "21SER9\x1d"
                                                                                                "0109501101530003");
    CHECK(QBarcode("0109501101530003", QBarcode::Gs1Code128).isNull());
    CHECK(QBarcode("(1)23", QBarcode::Gs1Code128).isNull());
    CHECK(QBarcode("(01)123", QBarcode::Gs1Code128).isNull());
    CHECK(QBarcode("(01)09501101530004", QBarcode::Gs1Code128).isNull());
    CHECK(QBarcode("(10)", QBarcode::Gs1Code128).isNull());
    CHECK(QBarcode("(00)106141411234567897", QBarcode::Gs1Code128).text() == QStringLiteral("(00)106141411234567897"));
}

void testText() {
    QBarcode ean("400638133393", QBarcode::Ean13);
    CHECK(ean.text() == QStringLiteral("4006381333931"));
    CHECK(!ean.isTextVisible());
    const QSize plain = ean.imageSize(2);
    QBarcode copy = ean;
    copy.setTextVisible(true);
    CHECK(!ean.isTextVisible()); // implicitly shared, detached on write
    CHECK(copy.isTextVisible());
    CHECK(copy.imageSize(2) == QSize(plain.width(), plain.height() + 20));
    const QImage image = copy.toImage(2);
    CHECK(image.size() == copy.imageSize(2));
    // The guard bars extend into the text band; the data bars do not.
    const int barBottom = (11 + 69) * 2;
    CHECK(image.pixel((11 + 0) * 2, barBottom + 4) == qRgba(0, 0, 0, 255));
    CHECK(image.pixel((11 + 46) * 2, barBottom + 4) == qRgba(0, 0, 0, 255));
    const QByteArray svg = copy.toSvg();
    CHECK(svg.count("<text") == 13);
    CHECK(svg.contains(">4</text>"));

    QBarcode upcE("0425261", QBarcode::UpcE);
    upcE.setTextVisible(true);
    CHECK(upcE.toSvg().count("<text") == 8);
    QBarcode label("Nt <&>", QBarcode::Code128);
    label.setTextVisible(true);
    CHECK(label.toSvg().contains(">Nt &lt;&amp;&gt;</text>"));
    CHECK(label.toImage(1).height() == label.size().height() + 20 + 10);

    // Two-dimensional symbols have no text.
    QBarcode qr("text");
    qr.setTextVisible(true);
    CHECK(qr.text().isEmpty());
    CHECK(qr.imageSize(1) == QSize(qr.size().width() + 8, qr.size().height() + 8));
}

void testDataMatrixExtras() {
    // Rectangular sizes, and the fallback to square when the data is too long for them.
    const QBarcode::Options rectangular = QBarcode::RectangularSymbol;
    const QBarcode rect("ABC", QBarcode::DataMatrix, QBarcode::LevelM, rectangular);
    CHECK(rect.options() == rectangular);
    CHECK(rect.size() == QSize(18, 8));
    CHECK(QBarcode(QByteArray(70, 'A'), QBarcode::DataMatrix, QBarcode::LevelM, rectangular).size() == QSize(48, 16));
    CHECK(QBarcode(QByteArray(200, 'A'), QBarcode::DataMatrix, QBarcode::LevelM, rectangular).size() == QSize(44, 44));
    // Finder and clock of both regions of an 8 x 32 symbol.
    const QBarcode twoRegions(QByteArray(14, '7'), QBarcode::DataMatrix, QBarcode::LevelM, rectangular);
    CHECK(twoRegions.size() == QSize(32, 8));
    for (int left : {0, 16}) {
        for (int i = 0; i < 8; ++i) {
            CHECK(twoRegions.isDark(left, i));
            CHECK(twoRegions.isDark(left + 15, i) == (i % 2 == 1));
        }
        for (int i = 0; i < 16; ++i) {
            CHECK(twoRegions.isDark(left + i, 7));
            CHECK(twoRegions.isDark(left + i, 0) == (i % 2 == 0));
        }
    }

    // GS1 DataMatrix shares the parser of GS1-128.
    const QBarcode gs1("(01)09501101530003(17)250101(10)ABC123", QBarcode::Gs1DataMatrix);
    CHECK(!gs1.isNull());
    CHECK(gs1.text() == QStringLiteral("(01)09501101530003(17)250101(10)ABC123"));
    CHECK(gs1.quietZone() == 1);
    CHECK(QBarcode("(01)09501101530004", QBarcode::Gs1DataMatrix).isNull());
    CHECK(QBarcode("no parentheses", QBarcode::Gs1DataMatrix).isNull());
    CHECK(QBarcode("(01)09501101530003", QBarcode::DataMatrix).text().isEmpty());
}

// The images of the Nt Barcode guide are this test's output; NT_UPDATE_DOC_IMAGES=1 rewrites them.
void testDocumentationImages() {
    struct Image {
        const char *file;
        const char *data;
        QBarcode::Symbology symbology;
        QBarcode::Options options;
    };
    const Image images[] = {
        {"qrcode", "https://open-nt.org", QBarcode::QrCode, QBarcode::NoOption},
        {"datamatrix", "NT6-0005 Acorn", QBarcode::DataMatrix, QBarcode::NoOption},
        {"datamatrix-rectangular", "NT6-0005", QBarcode::DataMatrix, QBarcode::RectangularSymbol},
        {"gs1datamatrix", "(01)09501101530003(17)250101(10)ABC123", QBarcode::Gs1DataMatrix, QBarcode::NoOption},
        {"aztec", "Nt 6 Acorn ticket 0005", QBarcode::Aztec, QBarcode::NoOption},
        {"code128", "NT-6.0.5", QBarcode::Code128, QBarcode::NoOption},
        {"gs1code128", "(01)09501101530003(10)ABC123", QBarcode::Gs1Code128, QBarcode::NoOption},
        {"code39", "NT-605", QBarcode::Code39, QBarcode::NoOption},
        {"interleaved2of5", "12345678", QBarcode::Interleaved2Of5, QBarcode::NoOption},
        {"itf14", "1540014128876", QBarcode::Itf14, QBarcode::NoOption},
        {"ean13", "400638133393", QBarcode::Ean13, QBarcode::NoOption},
        {"ean8", "9638507", QBarcode::Ean8, QBarcode::NoOption},
        {"upca", "03600029145", QBarcode::UpcA, QBarcode::NoOption},
        {"upce", "0425261", QBarcode::UpcE, QBarcode::NoOption},
    };
    const bool update = qEnvironmentVariableIsSet("NT_UPDATE_DOC_IMAGES");
    for (const Image &image : images) {
        QBarcode code(image.data, image.symbology, QBarcode::LevelM, image.options);
        code.setTextVisible(true);
        CHECK(!code.isNull());
        const QByteArray svg = code.toSvg(code.isLinear() ? 3 : 6); // two-dimensional symbols are small
        QFile file(QStringLiteral(NT_SOURCE_DIR "/doc/images/barcode/%1.svg").arg(QString::fromLatin1(image.file)));
        if (update) {
            CHECK(file.open(QIODevice::WriteOnly));
            file.write(svg);
            continue;
        }
        CHECK(file.open(QIODevice::ReadOnly));
        const bool current = file.readAll() == svg;
        if (!current)
            std::printf("  doc/images/barcode/%s.svg is out of date; run with NT_UPDATE_DOC_IMAGES=1\n", image.file);
        CHECK(current);
    }
}

} // namespace

int main() {
    testQr();
    testCode128();
    testEan();
    testRendering();
    testFingerprints();
    testDataMatrix();
    testAztec();
    testMoreLinear();
    testText();
    testDataMatrixExtras();
    testDocumentationImages();
    if (g_failures == 0)
        std::printf("All barcode tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
