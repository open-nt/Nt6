// MIT License - see LICENSE
//
// Data Matrix ECC 200 (ISO/IEC 16022) in the square and rectangular sizes: ASCII, C40, Text, X12
// and Base 256 encodation, mixed so that the data takes the fewest codewords; FNC1 for GS1
// DataMatrix; pad codewords; Reed-Solomon blocks over GF(256) with the polynomial 0x12D;
// interleaving; the module placement of Annex F; and the finder and clock patterns of each data
// region.
#include "qbarcode_p.h"

#include <array>
#include <climits>
#include <vector>

namespace QtBarcodePrivate {

namespace {

struct SymbolSize {
    int rows; // modules, including the finder and clock patterns
    int columns;
    int regionRows; // data modules of one data region
    int regionColumns;
    int regionsDown; // data regions
    int regionsAcross;
    int dataWords; // data codewords
    int eccWords;  // error correction codewords
    int blocks;    // interleaved Reed-Solomon blocks
};

constexpr std::array<SymbolSize, 24> kSquare = {{
    {10, 10, 8, 8, 1, 1, 3, 5, 1},          {12, 12, 10, 10, 1, 1, 5, 7, 1},
    {14, 14, 12, 12, 1, 1, 8, 10, 1},       {16, 16, 14, 14, 1, 1, 12, 12, 1},
    {18, 18, 16, 16, 1, 1, 18, 14, 1},      {20, 20, 18, 18, 1, 1, 22, 18, 1},
    {22, 22, 20, 20, 1, 1, 30, 20, 1},      {24, 24, 22, 22, 1, 1, 36, 24, 1},
    {26, 26, 24, 24, 1, 1, 44, 28, 1},      {32, 32, 14, 14, 2, 2, 62, 36, 1},
    {36, 36, 16, 16, 2, 2, 86, 42, 1},      {40, 40, 18, 18, 2, 2, 114, 48, 1},
    {44, 44, 20, 20, 2, 2, 144, 56, 1},     {48, 48, 22, 22, 2, 2, 174, 68, 1},
    {52, 52, 24, 24, 2, 2, 204, 84, 2},     {64, 64, 14, 14, 4, 4, 280, 112, 2},
    {72, 72, 16, 16, 4, 4, 368, 144, 4},    {80, 80, 18, 18, 4, 4, 456, 192, 4},
    {88, 88, 20, 20, 4, 4, 576, 224, 4},    {96, 96, 22, 22, 4, 4, 696, 272, 4},
    {104, 104, 24, 24, 4, 4, 816, 336, 6},  {120, 120, 18, 18, 6, 6, 1050, 408, 6},
    {132, 132, 20, 20, 6, 6, 1304, 496, 8}, {144, 144, 22, 22, 6, 6, 1558, 620, 10},
}};

constexpr std::array<SymbolSize, 6> kRectangular = {{
    {8, 18, 6, 16, 1, 1, 5, 7, 1},
    {8, 32, 6, 14, 1, 2, 10, 11, 1},
    {12, 26, 10, 24, 1, 1, 16, 14, 1},
    {12, 36, 10, 16, 1, 2, 22, 18, 1},
    {16, 36, 14, 16, 1, 2, 32, 24, 1},
    {16, 48, 14, 22, 1, 2, 49, 28, 1},
}};

constexpr int kPad = 129;
constexpr int kLatchC40 = 230;
constexpr int kLatchBase256 = 231;
constexpr int kFnc1Word = 232;
constexpr int kUpperShift = 235;
constexpr int kLatchX12 = 238;
constexpr int kLatchText = 239;
constexpr int kUnlatch = 254;
constexpr int kMaxBase256 = 1555;

enum class Encodation { Ascii, C40, Text, X12, Base256 };

bool isDigit(int c) { return c >= '0' && c <= '9'; }

// Appends the C40 or Text values of c: one for the basic set, two for a shifted character, and two
// more (Shift 2, Upper Shift) in front of a byte above 127.
void c40Values(bool text, int c, std::vector<int> *values) {
    if (c >= 128) {
        values->push_back(1);
        values->push_back(30);
        c -= 128;
    }
    const bool basicUpper = !text && c >= 'A' && c <= 'Z';
    const bool basicLower = text && c >= 'a' && c <= 'z';
    if (c == ' ') {
        values->push_back(3);
    } else if (isDigit(c)) {
        values->push_back(c - '0' + 4);
    } else if (basicUpper || basicLower) {
        values->push_back(c - (text ? 'a' : 'A') + 14);
    } else if (c < 32) {
        values->insert(values->end(), {0, c});
    } else if (c <= 47) {
        values->insert(values->end(), {1, c - 33});
    } else if (c <= 64) {
        values->insert(values->end(), {1, c - 58 + 15});
    } else if (c <= 90) {
        values->insert(values->end(), {2, c - 'A' + 1}); // upper case in Text
    } else if (c <= 95) {
        values->insert(values->end(), {1, c - 91 + 22});
    } else if (c == 96) {
        values->insert(values->end(), {2, 0});
    } else if (c <= 122) {
        values->insert(values->end(), {2, c - 'a' + 1}); // lower case in C40
    } else {
        values->insert(values->end(), {2, c - 123 + 27});
    }
}

int c40Count(bool text, int c) {
    if (c >= 128)
        return 2 + c40Count(text, c - 128);
    const bool basic = c == ' ' || isDigit(c) || (!text && c >= 'A' && c <= 'Z') || (text && c >= 'a' && c <= 'z');
    return basic ? 1 : 2;
}

int x12Value(int c) {
    if (c == '\r')
        return 0;
    if (c == '*')
        return 1;
    if (c == '>')
        return 2;
    if (c == ' ')
        return 3;
    if (isDigit(c))
        return c - '0' + 4;
    return c >= 'A' && c <= 'Z' ? c - 'A' + 14 : -1;
}

void appendTriples(const std::vector<int> &values, std::vector<int> *words) {
    for (std::size_t i = 0; i + 2 < values.size(); i += 3) {
        const int v = 1600 * values[i] + 40 * values[i + 1] + values[i + 2] + 1;
        words->push_back(v / 256);
        words->push_back(v % 256);
    }
}

struct Segment {
    Encodation encodation = Encodation::Ascii;
    std::size_t from = 0;
    std::size_t to = 0;
};

// The cheapest split of the characters into segments, each in one encodation and returning to
// ASCII at its end, found by a shortest path over the character positions. C40, Text and X12
// segments hold whole triples of values (a C40 or Text segment at the end may pad with Shift 1).
std::vector<Segment> plan(const std::vector<int> &chars) {
    const std::size_t n = chars.size();
    std::vector<int> best(n + 1, INT_MAX);
    std::vector<Segment> how(n + 1);
    best[0] = 0;
    auto relax = [&](std::size_t from, std::size_t to, int cost, Encodation encodation) {
        if (best[from] + cost < best[to]) {
            best[to] = best[from] + cost;
            how[to] = Segment{encodation, from, to};
        }
    };
    for (std::size_t i = 0; i < n; ++i) {
        if (best[i] == INT_MAX)
            continue;
        const int c = chars[i];
        if (c != kFnc1 && isDigit(c) && i + 1 < n && isDigit(chars[i + 1]))
            relax(i, i + 2, 1, Encodation::Ascii);
        relax(i, i + 1, c >= 128 ? 2 : 1, Encodation::Ascii);

        for (const bool text : {false, true}) {
            int values = 0;
            for (std::size_t j = i; j < n && chars[j] != kFnc1; ++j) {
                values += c40Count(text, chars[j]);
                const Encodation encodation = text ? Encodation::Text : Encodation::C40;
                if (values % 3 == 0)
                    relax(i, j + 1, 2 + values / 3 * 2, encodation);
                else if (j + 1 == n && values % 3 == 2)
                    relax(i, j + 1, 2 + (values + 1) / 3 * 2, encodation);
            }
        }
        int values = 0;
        for (std::size_t j = i; j < n && chars[j] != kFnc1 && x12Value(chars[j]) >= 0; ++j) {
            if (++values % 3 == 0)
                relax(i, j + 1, 2 + values / 3 * 2, Encodation::X12);
        }
        for (std::size_t j = i; j < n && chars[j] != kFnc1 && j - i < kMaxBase256; ++j) {
            const auto length = int(j - i + 1);
            relax(i, j + 1, 1 + (length <= 249 ? 1 : 2) + length, Encodation::Base256);
        }
    }
    std::vector<Segment> segments;
    for (std::size_t at = n; at > 0; at = how[at].from)
        segments.insert(segments.begin(), how[at]);
    return segments;
}

// Base 256 codewords are scrambled by their position in the data (counted from 1).
int randomize255(int value, std::size_t position) {
    const int pseudoRandom = int((149 * position) % 255) + 1;
    const int result = value + pseudoRandom;
    return result <= 255 ? result : result - 256;
}

// Encodes the characters; trailingUnlatch is set when the last codeword is an Unlatch that may be
// left out if the data then fills the symbol exactly.
std::vector<int> encodeChars(const std::vector<int> &chars, bool *trailingUnlatch) {
    std::vector<int> words;
    const std::vector<Segment> segments = plan(chars);
    *trailingUnlatch = false;
    for (const Segment &segment : segments) {
        *trailingUnlatch = false;
        switch (segment.encodation) {
        case Encodation::Ascii:
            for (std::size_t i = segment.from; i < segment.to; ++i) {
                const int c = chars[i];
                if (segment.to - segment.from == 2) {
                    words.push_back(130 + (c - '0') * 10 + (chars[i + 1] - '0'));
                    break;
                }
                if (c == kFnc1) {
                    words.push_back(kFnc1Word);
                } else if (c >= 128) {
                    words.push_back(kUpperShift);
                    words.push_back(c - 128 + 1);
                } else {
                    words.push_back(c + 1);
                }
            }
            break;
        case Encodation::C40:
        case Encodation::Text: {
            const bool text = segment.encodation == Encodation::Text;
            std::vector<int> values;
            for (std::size_t i = segment.from; i < segment.to; ++i)
                c40Values(text, chars[i], &values);
            if (values.size() % 3 == 2)
                values.push_back(0); // Shift 1 as padding
            words.push_back(text ? kLatchText : kLatchC40);
            appendTriples(values, &words);
            words.push_back(kUnlatch);
            *trailingUnlatch = true;
            break;
        }
        case Encodation::X12: {
            std::vector<int> values;
            for (std::size_t i = segment.from; i < segment.to; ++i)
                values.push_back(x12Value(chars[i]));
            words.push_back(kLatchX12);
            appendTriples(values, &words);
            words.push_back(kUnlatch);
            *trailingUnlatch = true;
            break;
        }
        case Encodation::Base256: {
            words.push_back(kLatchBase256);
            const auto length = int(segment.to - segment.from);
            std::vector<int> field;
            if (length <= 249) {
                field.push_back(length);
            } else {
                field.push_back(length / 250 + 249);
                field.push_back(length % 250);
            }
            for (std::size_t i = segment.from; i < segment.to; ++i)
                field.push_back(chars[i]);
            for (int value : field)
                words.push_back(randomize255(value, words.size() + 1));
            break;
        }
        }
    }
    return words;
}

// The placement of Annex F: fills a rows x columns grid with the bits of the codewords. Each cell
// holds codeword * 8 + bit (bit 0 is the most significant), or -1 for the fixed corner modules.
class Placement {
public:
    Placement(int rows, int columns)
        : m_rows(rows), m_columns(columns), m_cells(std::size_t(rows) * std::size_t(columns), kEmpty) {}

    void run() {
        int codeword = 0;
        int row = 4;
        int column = 0;
        do {
            if (row == m_rows && column == 0)
                corner1(codeword++);
            if (row == m_rows - 2 && column == 0 && m_columns % 4 != 0)
                corner2(codeword++);
            if (row == m_rows - 2 && column == 0 && m_columns % 8 == 4)
                corner3(codeword++);
            if (row == m_rows + 4 && column == 2 && m_columns % 8 == 0)
                corner4(codeword++);
            do {
                if (row < m_rows && column >= 0 && cell(row, column) == kEmpty)
                    utah(row, column, codeword++);
                row -= 2;
                column += 2;
            } while (row >= 0 && column < m_columns);
            row += 1;
            column += 3;
            do {
                if (row >= 0 && column < m_columns && cell(row, column) == kEmpty)
                    utah(row, column, codeword++);
                row += 2;
                column -= 2;
            } while (row < m_rows && column >= 0);
            row += 3;
            column += 1;
        } while (row < m_rows || column < m_columns);
        // An unfilled bottom-right corner gets a fixed pattern.
        if (cell(m_rows - 1, m_columns - 1) == kEmpty) {
            cell(m_rows - 1, m_columns - 1) = kDark;
            cell(m_rows - 2, m_columns - 2) = kDark;
            cell(m_rows - 1, m_columns - 2) = kLight;
            cell(m_rows - 2, m_columns - 1) = kLight;
        }
    }

    bool dark(int row, int column, const std::vector<int> &codewords) const {
        const int value = m_cells[index(row, column)];
        if (value == kDark)
            return true;
        if (value < 0)
            return false;
        const int word = codewords[std::size_t(value / 8)];
        return ((word >> (7 - value % 8)) & 1) != 0;
    }

private:
    static constexpr int kEmpty = -3;
    static constexpr int kLight = -2;
    static constexpr int kDark = -1;

    std::size_t index(int row, int column) const {
        return std::size_t(row) * std::size_t(m_columns) + std::size_t(column);
    }
    int &cell(int row, int column) { return m_cells[index(row, column)]; }

    void module(int row, int column, int codeword, int bit) {
        if (row < 0) {
            row += m_rows;
            column += 4 - ((m_rows + 4) % 8);
        }
        if (column < 0) {
            column += m_columns;
            row += 4 - ((m_columns + 4) % 8);
        }
        if (row < 0 || column < 0 || row >= m_rows || column >= m_columns)
            return; // cannot happen for the sizes of the standard
        cell(row, column) = codeword * 8 + bit;
    }

    void utah(int row, int column, int codeword) {
        module(row - 2, column - 2, codeword, 0);
        module(row - 2, column - 1, codeword, 1);
        module(row - 1, column - 2, codeword, 2);
        module(row - 1, column - 1, codeword, 3);
        module(row - 1, column, codeword, 4);
        module(row, column - 2, codeword, 5);
        module(row, column - 1, codeword, 6);
        module(row, column, codeword, 7);
    }

    void corner1(int codeword) {
        module(m_rows - 1, 0, codeword, 0);
        module(m_rows - 1, 1, codeword, 1);
        module(m_rows - 1, 2, codeword, 2);
        module(0, m_columns - 2, codeword, 3);
        module(0, m_columns - 1, codeword, 4);
        module(1, m_columns - 1, codeword, 5);
        module(2, m_columns - 1, codeword, 6);
        module(3, m_columns - 1, codeword, 7);
    }

    void corner2(int codeword) {
        module(m_rows - 3, 0, codeword, 0);
        module(m_rows - 2, 0, codeword, 1);
        module(m_rows - 1, 0, codeword, 2);
        module(0, m_columns - 4, codeword, 3);
        module(0, m_columns - 3, codeword, 4);
        module(0, m_columns - 2, codeword, 5);
        module(0, m_columns - 1, codeword, 6);
        module(1, m_columns - 1, codeword, 7);
    }

    void corner3(int codeword) {
        module(m_rows - 3, 0, codeword, 0);
        module(m_rows - 2, 0, codeword, 1);
        module(m_rows - 1, 0, codeword, 2);
        module(0, m_columns - 2, codeword, 3);
        module(0, m_columns - 1, codeword, 4);
        module(1, m_columns - 1, codeword, 5);
        module(2, m_columns - 1, codeword, 6);
        module(3, m_columns - 1, codeword, 7);
    }

    void corner4(int codeword) {
        module(m_rows - 1, 0, codeword, 0);
        module(m_rows - 1, m_columns - 1, codeword, 1);
        module(0, m_columns - 3, codeword, 2);
        module(0, m_columns - 2, codeword, 3);
        module(0, m_columns - 1, codeword, 4);
        module(1, m_columns - 3, codeword, 5);
        module(1, m_columns - 2, codeword, 6);
        module(1, m_columns - 1, codeword, 7);
    }

    int m_rows;
    int m_columns;
    std::vector<int> m_cells;
};

} // namespace

void encodeDataMatrix(QBarcodePrivate *d) {
    std::vector<int> chars;
    if (d->symbology == QBarcode::Gs1DataMatrix) {
        const QString error = parseGs1(d->data, &chars, &d->text);
        if (!error.isEmpty()) {
            d->fail(error);
            return;
        }
    } else {
        for (char c : d->data)
            chars.push_back(quint8(c));
    }
    bool trailingUnlatch = false;
    std::vector<int> words = encodeChars(chars, &trailingUnlatch);

    // The smallest symbol that holds the data: rectangular sizes first when asked for, then square.
    // An Unlatch at the very end may be dropped when the rest fills the symbol exactly.
    const std::size_t needed = words.size() - (trailingUnlatch ? 1 : 0);
    const SymbolSize *chosen = nullptr;
    if (d->options.testFlag(QBarcode::RectangularSymbol)) {
        for (const SymbolSize &size : kRectangular) {
            if (std::size_t(size.dataWords) >= needed) {
                chosen = &size;
                break;
            }
        }
    }
    for (std::size_t i = 0; chosen == nullptr && i < kSquare.size(); ++i) {
        if (std::size_t(kSquare[i].dataWords) >= needed)
            chosen = &kSquare[i];
    }
    if (chosen == nullptr) {
        d->fail(QStringLiteral("The data is too long for a Data Matrix symbol"));
        return;
    }
    if (std::size_t(chosen->dataWords) < words.size())
        words.pop_back();

    // Pad: 129, then 129 scrambled by the position of the pad (counted from 1).
    if (words.size() < std::size_t(chosen->dataWords))
        words.push_back(kPad);
    while (words.size() < std::size_t(chosen->dataWords)) {
        const int position = int(words.size()) + 1;
        int pad = kPad + (149 * position) % 253 + 1;
        if (pad > 254)
            pad -= 254;
        words.push_back(pad);
    }

    // Each block takes every blocks-th data codeword; its check words are interleaved the same way.
    static const GaloisField field(8, 0x12D);
    const int blocks = chosen->blocks;
    const int eccPerBlock = chosen->eccWords / blocks;
    std::vector<int> codewords = words;
    const int totalWords = chosen->dataWords + chosen->eccWords;
    codewords.resize(static_cast<std::size_t>(totalWords), 0);
    for (int b = 0; b < blocks; ++b) {
        std::vector<int> block;
        for (int i = b; i < chosen->dataWords; i += blocks)
            block.push_back(words[std::size_t(i)]);
        const std::vector<int> ecc = reedSolomon(field, block, eccPerBlock, 1);
        for (int i = 0; i < eccPerBlock; ++i) {
            const int position = chosen->dataWords + b + i * blocks;
            codewords[static_cast<std::size_t>(position)] = ecc[std::size_t(i)];
        }
    }

    Placement placement(chosen->regionRows * chosen->regionsDown, chosen->regionColumns * chosen->regionsAcross);
    placement.run();

    const int rows = chosen->rows;
    const int columns = chosen->columns;
    const int regionRows = chosen->regionRows;
    const int regionColumns = chosen->regionColumns;
    std::vector<quint8> modules(std::size_t(rows) * std::size_t(columns), 0);
    auto set = [&modules, columns](int row, int column, bool dark) {
        modules[std::size_t(row) * std::size_t(columns) + std::size_t(column)] = dark ? 1 : 0;
    };
    for (int ry = 0; ry < chosen->regionsDown; ++ry) {
        for (int rx = 0; rx < chosen->regionsAcross; ++rx) {
            const int top = ry * (regionRows + 2);
            const int left = rx * (regionColumns + 2);
            // Solid finder on the left and bottom, alternating clock track on the top and right.
            for (int i = 0; i < regionRows + 2; ++i) {
                set(top + i, left, true);
                set(top + i, left + regionColumns + 1, i % 2 == 1);
            }
            for (int i = 0; i < regionColumns + 2; ++i) {
                set(top + regionRows + 1, left + i, true);
                set(top, left + i, i % 2 == 0);
            }
            set(top + regionRows + 1, left + regionColumns + 1, true);
            for (int y = 0; y < regionRows; ++y) {
                for (int x = 0; x < regionColumns; ++x) {
                    set(top + 1 + y, left + 1 + x,
                        placement.dark(ry * regionRows + y, rx * regionColumns + x, codewords));
                }
            }
        }
    }
    d->setMatrix(columns, rows, modules);
}

} // namespace QtBarcodePrivate
