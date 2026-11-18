// MIT License - see LICENSE
//
// Aztec Code (ISO/IEC 24778), compact and full-range: the five character modes with latches and
// shifts, Binary Shift for other bytes, bit stuffing, the choice of layers, Reed-Solomon check
// words over GF(16), GF(64), GF(256), GF(1024) or GF(4096), the mode message, the bullseye with its
// orientation marks, the reference grid of full-range symbols and the spiral of data layers.
#include "qbarcode_p.h"

#include <algorithm>
#include <array>
#include <vector>

namespace QtBarcodePrivate {

namespace {

enum Mode { Upper, Lower, Mixed, Punct, Digit, ModeCount };

int modeBits(Mode mode) { return mode == Digit ? 4 : 5; }

// The code of byte c in a mode, or -1 if the mode lacks it.
int charCode(Mode mode, int c) {
    switch (mode) {
    case Upper:
        if (c == ' ')
            return 1;
        return c >= 'A' && c <= 'Z' ? c - 'A' + 2 : -1;
    case Lower:
        if (c == ' ')
            return 1;
        return c >= 'a' && c <= 'z' ? c - 'a' + 2 : -1;
    case Mixed: {
        if (c == ' ')
            return 1;
        if (c >= 1 && c <= 13)
            return c + 1;
        if (c >= 27 && c <= 31)
            return c - 27 + 15;
        static constexpr char kTail[] = "@\\^_`|~\x7f";
        for (int i = 0; kTail[i] != '\0'; ++i) {
            if (kTail[i] == c)
                return 20 + i;
        }
        return -1;
    }
    case Punct: {
        if (c == '\r')
            return 1;
        static constexpr char kPunct[] = "!\"#$%&'()*+,-./:;<=>?[]{}";
        for (int i = 0; kPunct[i] != '\0'; ++i) {
            if (kPunct[i] == c)
                return 6 + i;
        }
        return -1;
    }
    case Digit:
        if (c == ' ')
            return 1;
        if (c >= '0' && c <= '9')
            return c - '0' + 2;
        if (c == ',')
            return 12;
        return c == '.' ? 13 : -1;
    case ModeCount:
        break;
    }
    return -1;
}

struct Code {
    int value;
    int bits;
};

// The codes that latch from one mode to another.
std::vector<Code> latch(Mode from, Mode to) {
    static constexpr Code kUL5{29, 5};  // U/L from Mixed
    static constexpr Code kUL4{14, 4};  // U/L from Digit
    static constexpr Code kUL31{31, 5}; // U/L from Punct
    static constexpr Code kLL{28, 5};
    static constexpr Code kML{29, 5};
    static constexpr Code kDL{30, 5};
    static constexpr Code kPL{30, 5}; // P/L from Mixed
    switch (from) {
    case Upper:
        if (to == Lower)
            return {kLL};
        if (to == Mixed)
            return {kML};
        if (to == Digit)
            return {kDL};
        return {kML, kPL};
    case Lower:
        if (to == Upper)
            return {kDL, kUL4};
        if (to == Mixed)
            return {kML};
        if (to == Digit)
            return {kDL};
        return {kML, kPL};
    case Mixed:
        if (to == Upper)
            return {kUL5};
        if (to == Lower)
            return {kLL};
        if (to == Punct)
            return {kPL};
        return {kUL5, kDL};
    case Punct: {
        std::vector<Code> codes{kUL31};
        if (to != Upper) {
            const std::vector<Code> rest = latch(Upper, to);
            codes.insert(codes.end(), rest.begin(), rest.end());
        }
        return codes;
    }
    case Digit: {
        std::vector<Code> codes{kUL4};
        if (to != Upper) {
            const std::vector<Code> rest = latch(Upper, to);
            codes.insert(codes.end(), rest.begin(), rest.end());
        }
        return codes;
    }
    case ModeCount:
        break;
    }
    return {};
}

int cost(const std::vector<Code> &codes) {
    int bits = 0;
    for (const Code &code : codes)
        bits += code.bits;
    return bits;
}

bool encodable(int c) {
    for (int m = 0; m < ModeCount; ++m) {
        if (charCode(Mode(m), c) >= 0)
            return true;
    }
    return false;
}

class Bits {
public:
    void append(int value, int count) {
        for (int i = count - 1; i >= 0; --i)
            m_bits.push_back(((value >> i) & 1) != 0);
    }
    void append(const Code &code) { append(code.value, code.bits); }
    std::size_t size() const { return m_bits.size(); }
    bool at(std::size_t i) const { return m_bits[i]; }

private:
    std::vector<bool> m_bits;
};

// A greedy high-level encoding: stay in the current mode while it has the characters, shift for a
// single character of another mode, latch for a run, and Binary Shift bytes no mode has.
Bits highLevelEncode(const QByteArray &data) {
    Bits bits;
    Mode mode = Upper;
    const qsizetype n = data.size();
    auto byteAt = [&data](qsizetype i) { return int(quint8(data.at(i))); };
    for (qsizetype i = 0; i < n;) {
        const int c = byteAt(i);
        const int code = charCode(mode, c);
        if (code >= 0) {
            bits.append(code, modeBits(mode));
            ++i;
            continue;
        }
        if (!encodable(c)) {
            qsizetype end = i;
            while (end < n && !encodable(byteAt(end)) && end - i < 2078 + 31)
                ++end;
            if (mode == Punct || mode == Digit) {
                for (const Code &step : latch(mode, Upper))
                    bits.append(step);
                mode = Upper;
            }
            const int length = int(end - i);
            bits.append(31, 5); // B/S
            if (length <= 31) {
                bits.append(length, 5);
            } else {
                bits.append(0, 5);
                bits.append(length - 31, 11);
            }
            for (; i < end; ++i)
                bits.append(byteAt(i), 8);
            continue;
        }
        // Prefer the mode that holds the longest run from here; break ties by the cheaper latch.
        Mode best = Upper;
        int bestRun = -1;
        int bestCost = 0;
        for (int m = 0; m < ModeCount; ++m) {
            const Mode candidate = Mode(m);
            if (candidate == mode || charCode(candidate, c) < 0)
                continue;
            int run = 0;
            while (i + run < n && charCode(candidate, byteAt(i + run)) >= 0)
                ++run;
            const int latchCost = cost(latch(mode, candidate));
            if (run > bestRun || (run == bestRun && latchCost < bestCost)) {
                best = candidate;
                bestRun = run;
                bestCost = latchCost;
            }
        }
        if (bestRun == 1) {
            // A single character: shift where the mode allows it.
            if (best == Punct || (best == Upper && (mode == Lower || mode == Digit))) {
                const bool digit = mode == Digit;
                if (best == Punct)
                    bits.append(0, modeBits(mode)); // P/S
                else
                    bits.append(digit ? 15 : 28, modeBits(mode)); // U/S
                bits.append(charCode(best, c), 5);
                ++i;
                continue;
            }
        }
        for (const Code &step : latch(mode, best))
            bits.append(step);
        mode = best;
    }
    return bits;
}

// Splits the bits into words, making sure no word is all zeros or all ones: such a word takes one
// bit less and a complementary stuffing bit. The last word is padded with ones.
std::vector<int> stuffBits(const Bits &bits, int wordSize) {
    std::vector<int> words;
    const auto n = std::ptrdiff_t(bits.size());
    const int mask = (1 << wordSize) - 2;
    for (std::ptrdiff_t i = 0; i < n; i += wordSize) {
        int word = 0;
        for (int j = 0; j < wordSize; ++j) {
            if (i + j >= n || bits.at(std::size_t(i + j)))
                word |= 1 << (wordSize - 1 - j);
        }
        if ((word & mask) == mask) {
            words.push_back(word & mask);
            --i;
        } else if ((word & mask) == 0) {
            words.push_back(word | 1);
            --i;
        } else {
            words.push_back(word);
        }
    }
    return words;
}

int wordSizeFor(int layers) {
    if (layers <= 2)
        return 6;
    if (layers <= 8)
        return 8;
    return layers <= 22 ? 10 : 12;
}

const GaloisField &fieldFor(int wordSize) {
    static const GaloisField gf16(4, 0x13);
    static const GaloisField gf64(6, 0x43);
    static const GaloisField gf256(8, 0x12D);
    static const GaloisField gf1024(10, 0x409);
    static const GaloisField gf4096(12, 0x1069);
    switch (wordSize) {
    case 4:
        return gf16;
    case 6:
        return gf64;
    case 8:
        return gf256;
    case 10:
        return gf1024;
    default:
        break;
    }
    return gf4096;
}

int totalBits(int layers, bool compact) { return ((compact ? 88 : 112) + 16 * layers) * layers; }

} // namespace

void encodeAztec(QBarcodePrivate *d) {
    if (d->data.isEmpty()) {
        d->fail(QStringLiteral("An Aztec Code needs at least one byte"));
        return;
    }
    const Bits bits = highLevelEncode(d->data);
    static constexpr std::array<int, 4> kEccPercent = {10, 23, 36, 50};
    const std::size_t eccBits = bits.size() * std::size_t(kEccPercent[std::size_t(d->level)]) / 100 + 11;

    // Compact symbols with 1 to 4 layers, then full-range ones with 4 to 32.
    bool compact = true;
    int layers = 0;
    int wordSize = 0;
    std::vector<int> dataWords;
    for (int i = 0;; ++i) {
        compact = i <= 3;
        layers = compact ? i + 1 : i;
        if (layers > 32) {
            d->fail(QStringLiteral("The data is too long for an Aztec Code at this error correction level"));
            return;
        }
        const int total = totalBits(layers, compact);
        wordSize = wordSizeFor(layers);
        const std::size_t usable = std::size_t(total - total % wordSize);
        dataWords = stuffBits(bits, wordSize);
        const std::size_t stuffed = dataWords.size() * std::size_t(wordSize);
        if (stuffed + eccBits > usable)
            continue;
        if (compact && dataWords.size() > 64)
            continue;
        break;
    }

    // The message: padding to fill the layers, the data words and the check words.
    const int total = totalBits(layers, compact);
    const int totalWords = total / wordSize;
    const std::vector<int> ecc = reedSolomon(fieldFor(wordSize), dataWords, totalWords - int(dataWords.size()), 1);
    Bits message;
    message.append(0, total % wordSize);
    for (int word : dataWords)
        message.append(word, wordSize);
    for (int word : ecc)
        message.append(word, wordSize);

    // The mode message: the layers and data words, protected by check words over GF(16).
    Bits mode;
    {
        const int value = compact ? ((layers - 1) << 6) | (int(dataWords.size()) - 1)
                                  : ((layers - 1) << 11) | (int(dataWords.size()) - 1);
        const int words = compact ? 2 : 4;
        std::vector<int> nibbles;
        for (int i = words - 1; i >= 0; --i)
            nibbles.push_back((value >> (4 * i)) & 0xF);
        const std::vector<int> check = reedSolomon(fieldFor(4), nibbles, compact ? 5 : 6, 1);
        for (int nibble : nibbles)
            mode.append(nibble, 4);
        for (int nibble : check)
            mode.append(nibble, 4);
    }

    // Full-range symbols have reference grid lines every 16 modules from the centre; the data
    // layers skip them, which alignment maps from a base grid to the symbol.
    const int baseSize = (compact ? 11 : 14) + 4 * layers;
    int size = baseSize;
    std::vector<int> alignment(std::size_t(baseSize), 0);
    if (compact) {
        for (int i = 0; i < baseSize; ++i)
            alignment[std::size_t(i)] = i;
    } else {
        size = baseSize + 1 + 2 * ((baseSize / 2 - 1) / 15);
        const int originalCentre = baseSize / 2;
        const int centre = size / 2;
        for (int i = 0; i < originalCentre; ++i) {
            const int offset = i + i / 15;
            alignment[std::size_t(originalCentre - i - 1)] = centre - offset - 1;
            alignment[static_cast<std::size_t>(originalCentre) + static_cast<std::size_t>(i)] = centre + offset + 1;
        }
    }

    std::vector<quint8> modules(std::size_t(size) * std::size_t(size), 0);
    auto set = [&modules, size](int x, int y) { modules[std::size_t(y) * std::size_t(size) + std::size_t(x)] = 1; };
    auto map = [&alignment](int i) { return alignment[std::size_t(i)]; };

    // Data layers, outermost first, each a ring two modules wide filled side by side.
    std::size_t rowOffset = 0;
    for (int i = 0; i < layers; ++i) {
        const int rowSize = (layers - i) * 4 + (compact ? 9 : 12);
        const auto side = std::size_t(rowSize) * 2;
        for (int j = 0; j < rowSize; ++j) {
            const auto column = std::size_t(j) * 2;
            for (int k = 0; k < 2; ++k) {
                const auto bit = column + std::size_t(k);
                const int inner = i * 2;
                const int outer = baseSize - 1 - i * 2;
                if (message.at(rowOffset + bit))
                    set(map(inner + k), map(inner + j));
                if (message.at(rowOffset + side + bit))
                    set(map(inner + j), map(outer - k));
                if (message.at(rowOffset + 2 * side + bit))
                    set(map(outer - k), map(outer - j));
                if (message.at(rowOffset + 3 * side + bit))
                    set(map(outer - j), map(inner + k));
            }
        }
        rowOffset += std::size_t(rowSize) * 8;
    }

    const int centre = size / 2;
    // The mode message, around the bullseye.
    if (compact) {
        for (int i = 0; i < 7; ++i) {
            const int offset = centre - 3 + i;
            if (mode.at(std::size_t(i)))
                set(offset, centre - 5);
            if (mode.at(static_cast<std::size_t>(i) + 7))
                set(centre + 5, offset);
            if (mode.at(std::size_t(20 - i)))
                set(offset, centre + 5);
            if (mode.at(std::size_t(27 - i)))
                set(centre - 5, offset);
        }
    } else {
        for (int i = 0; i < 10; ++i) {
            const int offset = centre - 5 + i + i / 5;
            if (mode.at(std::size_t(i)))
                set(offset, centre - 7);
            if (mode.at(static_cast<std::size_t>(i) + 10))
                set(centre + 7, offset);
            if (mode.at(std::size_t(29 - i)))
                set(offset, centre + 7);
            if (mode.at(std::size_t(39 - i)))
                set(centre - 7, offset);
        }
    }

    // The bullseye: dark squares at even distances from the centre, and the orientation marks.
    const int ring = compact ? 5 : 7;
    for (int i = 0; i < ring; i += 2) {
        for (int j = centre - i; j <= centre + i; ++j) {
            set(j, centre - i);
            set(j, centre + i);
            set(centre - i, j);
            set(centre + i, j);
        }
    }
    set(centre - ring, centre - ring);
    set(centre - ring + 1, centre - ring);
    set(centre - ring, centre - ring + 1);
    set(centre + ring, centre - ring);
    set(centre + ring, centre - ring + 1);
    set(centre + ring, centre + ring - 1);

    // The reference grid of a full-range symbol.
    if (!compact) {
        for (int i = 0, j = 0; i < baseSize / 2 - 1; i += 15, j += 16) {
            for (int k = (size / 2) & 1; k < size; k += 2) {
                set(centre - j, k);
                set(centre + j, k);
                set(k, centre - j);
                set(k, centre + j);
            }
        }
    }

    d->setMatrix(size, modules);
    d->version = layers;
}

} // namespace QtBarcodePrivate
