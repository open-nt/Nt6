// MIT License - see LICENSE
//
// QR Code Model 2 (ISO/IEC 18004): mode selection, version selection, Reed-Solomon blocks,
// interleaving, codeword placement and mask selection by penalty score.
#include "qbarcode_p.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

namespace QtBarcodePrivate {

namespace {

// ---------------------------------------------------------------------------------------------
// QR Code

// Error correction codewords per block and number of blocks, indexed by level (L, M, Q, H) and
// version (1 to 40; index 0 is unused).
constexpr std::array<std::array<int, 41>, 4> kEccPerBlock = {{
    {-1, 7,  10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28,
     28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26,
     26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},
    {-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30,
     28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28,
     30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
}};

constexpr std::array<std::array<int, 41>, 4> kBlockCount = {{
    {-1, 1, 1, 1,  1,  1,  2,  2,  2,  2,  4,  4,  4,  4,  4,  6,  6,  6,  6,  7, 8,
     8,  9, 9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
    {-1, 1,  1,  1,  2,  2,  4,  4,  4,  5,  5,  5,  8,  9,  9,  10, 10, 11, 13, 14, 16,
     17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
    {-1, 1,  1,  2,  2,  4,  4,  6,  6,  8,  8,  8,  10, 12, 16, 12, 17, 16, 18, 21, 20,
     23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},
    {-1, 1,  1,  2,  4,  4,  4,  5,  6,  8,  8,  11, 11, 16, 16, 18, 16, 19, 21, 25, 25,
     25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},
}};

// The two format-information bits of each level, in the order of ErrorCorrectionLevel.
constexpr std::array<int, 4> kFormatLevelBits = {1, 0, 3, 2};

constexpr char kAlphanumeric[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

enum class QrMode { Numeric, Alphanumeric, Byte };

// Number of modules of a version that are left for data and error correction, in bits.
int rawDataModules(int version) {
    int result = (16 * version + 128) * version + 64;
    if (version >= 2) {
        const int alignments = version / 7 + 2;
        result -= (25 * alignments - 10) * alignments - 55;
        if (version >= 7)
            result -= 36;
    }
    return result;
}

int dataCodewords(int version, int level) {
    return rawDataModules(version) / 8 - kEccPerBlock[std::size_t(level)][std::size_t(version)] *
                                             kBlockCount[std::size_t(level)][std::size_t(version)];
}

int characterCountBits(QrMode mode, int version) {
    // Rows: numeric, alphanumeric, byte; columns: versions 1-9, 10-26, 27-40.
    static constexpr std::array<std::array<int, 3>, 3> kBits = {{{10, 12, 14}, {9, 11, 13}, {8, 16, 16}}};
    const std::size_t band = version <= 9 ? 0 : (version <= 26 ? 1 : 2);
    return kBits[static_cast<std::size_t>(mode)][band];
}

int alphanumericValue(char c) {
    for (int i = 0; kAlphanumeric[i] != '\0'; ++i) {
        if (kAlphanumeric[i] == c)
            return i;
    }
    return -1;
}

std::vector<int> alignmentPositions(int version) {
    if (version == 1)
        return {};
    const int count = version / 7 + 2;
    const int step = version == 32 ? 26 : (version * 4 + count * 2 + 1) / (count * 2 - 2) * 2;
    std::vector<int> result(static_cast<std::size_t>(count));
    result[0] = 6;
    int position = version * 4 + 17 - 7;
    for (int i = count - 1; i >= 1; --i, position -= step)
        result[std::size_t(i)] = position;
    return result;
}

class BitWriter {
public:
    void append(quint32 value, int bits) {
        for (int i = bits - 1; i >= 0; --i)
            m_bits.push_back(quint8((value >> i) & 1U));
    }
    std::size_t size() const { return m_bits.size(); }
    const std::vector<quint8> &bits() const { return m_bits; }

private:
    std::vector<quint8> m_bits;
};

quint8 gfMultiply(quint8 x, quint8 y) {
    unsigned z = 0;
    for (int i = 7; i >= 0; --i) {
        z = (z << 1) ^ ((z >> 7) * 0x11DU);
        z ^= ((unsigned(y) >> i) & 1U) * unsigned(x);
    }
    return quint8(z);
}

std::vector<quint8> reedSolomonDivisor(int degree) {
    std::vector<quint8> result(std::size_t(degree), 0);
    result.back() = 1;
    quint8 root = 1;
    for (int i = 0; i < degree; ++i) {
        for (std::size_t j = 0; j < result.size(); ++j) {
            result[j] = gfMultiply(result[j], root);
            if (j + 1 < result.size())
                result[j] ^= result[j + 1];
        }
        root = gfMultiply(root, 0x02);
    }
    return result;
}

std::vector<quint8> reedSolomonRemainder(const std::vector<quint8> &data, const std::vector<quint8> &divisor) {
    std::vector<quint8> result(divisor.size(), 0);
    for (quint8 byte : data) {
        const quint8 factor = byte ^ result.front();
        result.erase(result.begin());
        result.push_back(0);
        for (std::size_t i = 0; i < result.size(); ++i)
            result[i] ^= gfMultiply(divisor[i], factor);
    }
    return result;
}

class QrMatrix {
public:
    explicit QrMatrix(int version)
        : m_version(version), m_size(version * 4 + 17), m_dark(std::size_t(m_size * m_size), 0),
          m_function(std::size_t(m_size * m_size), 0) {}

    int size() const { return m_size; }
    bool dark(int x, int y) const { return m_dark[index(x, y)] != 0; }

    void drawFunctionPatterns() {
        for (int i = 0; i < m_size; ++i) {
            setFunction(6, i, i % 2 == 0);
            setFunction(i, 6, i % 2 == 0);
        }
        drawFinder(3, 3);
        drawFinder(m_size - 4, 3);
        drawFinder(3, m_size - 4);
        const std::vector<int> positions = alignmentPositions(m_version);
        const std::size_t count = positions.size();
        for (std::size_t i = 0; i < count; ++i) {
            for (std::size_t j = 0; j < count; ++j) {
                const bool corner = (i == 0 && j == 0) || (i == 0 && j == count - 1) || (i == count - 1 && j == 0);
                if (!corner)
                    drawAlignment(positions[i], positions[j]);
            }
        }
        drawFormatBits(0, 0);
        drawVersion();
    }

    void drawFormatBits(int level, int mask) {
        const int data = kFormatLevelBits[std::size_t(level)] << 3 | mask;
        int remainder = data;
        for (int i = 0; i < 10; ++i)
            remainder = (remainder << 1) ^ ((remainder >> 9) * 0x537);
        const int bits = (data << 10 | remainder) ^ 0x5412;
        auto bit = [bits](int i) { return ((bits >> i) & 1) != 0; };
        for (int i = 0; i <= 5; ++i)
            setFunction(8, i, bit(i));
        setFunction(8, 7, bit(6));
        setFunction(8, 8, bit(7));
        setFunction(7, 8, bit(8));
        for (int i = 9; i < 15; ++i)
            setFunction(14 - i, 8, bit(i));
        for (int i = 0; i < 8; ++i)
            setFunction(m_size - 1 - i, 8, bit(i));
        for (int i = 8; i < 15; ++i)
            setFunction(8, m_size - 15 + i, bit(i));
        setFunction(8, m_size - 8, true);
    }

    void drawCodewords(const std::vector<quint8> &codewords) {
        std::size_t i = 0;
        const std::size_t total = codewords.size() * 8;
        for (int right = m_size - 1; right >= 1; right -= 2) {
            if (right == 6)
                right = 5;
            for (int vertical = 0; vertical < m_size; ++vertical) {
                for (int j = 0; j < 2; ++j) {
                    const int x = right - j;
                    const bool upward = ((right + 1) & 2) == 0;
                    const int y = upward ? m_size - 1 - vertical : vertical;
                    if (m_function[index(x, y)] == 0 && i < total) {
                        m_dark[index(x, y)] = quint8((codewords[i >> 3] >> (7 - int(i & 7))) & 1);
                        ++i;
                    }
                }
            }
        }
    }

    void applyMask(int mask) {
        for (int y = 0; y < m_size; ++y) {
            for (int x = 0; x < m_size; ++x) {
                bool invert = false;
                switch (mask) {
                case 0:
                    invert = (x + y) % 2 == 0;
                    break;
                case 1:
                    invert = y % 2 == 0;
                    break;
                case 2:
                    invert = x % 3 == 0;
                    break;
                case 3:
                    invert = (x + y) % 3 == 0;
                    break;
                case 4:
                    invert = (x / 3 + y / 2) % 2 == 0;
                    break;
                case 5:
                    invert = x * y % 2 + x * y % 3 == 0;
                    break;
                case 6:
                    invert = (x * y % 2 + x * y % 3) % 2 == 0;
                    break;
                default:
                    invert = ((x + y) % 2 + x * y % 3) % 2 == 0;
                    break;
                }
                if (invert && m_function[index(x, y)] == 0)
                    m_dark[index(x, y)] ^= 1;
            }
        }
    }

    // The penalty score of ISO/IEC 18004 section 7.8.3; the mask with the lowest score is used.
    long penalty() const {
        long result = 0;
        for (int line = 0; line < m_size; ++line) {
            result += linePenalty([this, line](int i) { return dark(i, line); });
            result += linePenalty([this, line](int i) { return dark(line, i); });
        }
        for (int y = 0; y + 1 < m_size; ++y) {
            for (int x = 0; x + 1 < m_size; ++x) {
                const bool c = dark(x, y);
                if (c == dark(x + 1, y) && c == dark(x, y + 1) && c == dark(x + 1, y + 1))
                    result += 3;
            }
        }
        long darkCount = 0;
        for (quint8 module : m_dark)
            darkCount += module;
        const long total = long(m_size) * m_size;
        const long k = (std::labs(darkCount * 20 - total * 10) + total - 1) / total - 1;
        result += k * 10;
        return result;
    }

private:
    std::size_t index(int x, int y) const {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_size) + static_cast<std::size_t>(x);
    }

    void setFunction(int x, int y, bool isDark) {
        m_dark[index(x, y)] = isDark ? 1 : 0;
        m_function[index(x, y)] = 1;
    }

    void drawFinder(int cx, int cy) {
        for (int dy = -4; dy <= 4; ++dy) {
            for (int dx = -4; dx <= 4; ++dx) {
                const int distance = std::max(std::abs(dx), std::abs(dy));
                const int x = cx + dx;
                const int y = cy + dy;
                if (x >= 0 && x < m_size && y >= 0 && y < m_size)
                    setFunction(x, y, distance != 2 && distance != 4);
            }
        }
    }

    void drawAlignment(int cx, int cy) {
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx)
                setFunction(cx + dx, cy + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
        }
    }

    void drawVersion() {
        if (m_version < 7)
            return;
        int remainder = m_version;
        for (int i = 0; i < 12; ++i)
            remainder = (remainder << 1) ^ ((remainder >> 11) * 0x1F25);
        const long bits = long(m_version) << 12 | remainder;
        for (int i = 0; i < 18; ++i) {
            const bool bit = ((bits >> i) & 1) != 0;
            const int a = m_size - 11 + i % 3;
            const int b = i / 3;
            setFunction(a, b, bit);
            setFunction(b, a, bit);
        }
    }

    // Rule 1 (runs of five or more) and rule 3 (1:1:3:1:1 finder look-alikes with four light
    // modules on one side; the area outside the symbol counts as light).
    template <typename At> long linePenalty(At at) const {
        long result = 0;
        int run = 0;
        bool previous = false;
        for (int i = 0; i < m_size; ++i) {
            const bool c = at(i);
            if (i > 0 && c == previous) {
                ++run;
            } else {
                if (run >= 5)
                    result += 3 + (run - 5);
                run = 1;
                previous = c;
            }
        }
        if (run >= 5)
            result += 3 + (run - 5);

        static constexpr std::array<bool, 7> kFinder = {true, false, true, true, true, false, true};
        auto moduleAt = [this, &at](int i) { return i >= 0 && i < m_size && at(i); };
        for (int i = 0; i + 7 <= m_size; ++i) {
            bool match = true;
            for (int k = 0; k < 7 && match; ++k)
                match = moduleAt(i + k) == kFinder[std::size_t(k)];
            if (!match)
                continue;
            bool lightBefore = true;
            bool lightAfter = true;
            for (int k = 1; k <= 4; ++k) {
                lightBefore = lightBefore && !moduleAt(i - k);
                lightAfter = lightAfter && !moduleAt(i + 6 + k);
            }
            if (lightBefore)
                result += 40;
            if (lightAfter)
                result += 40;
        }
        return result;
    }

    int m_version;
    int m_size;
    std::vector<quint8> m_dark;
    std::vector<quint8> m_function;
};

} // namespace

void encodeQr(QBarcodePrivate *d) {
    const QByteArray &data = d->data;
    QrMode mode = QrMode::Byte;
    if (allDigits(data))
        mode = QrMode::Numeric;
    else if (std::all_of(data.cbegin(), data.cend(), [](char c) { return alphanumericValue(c) >= 0; }))
        mode = QrMode::Alphanumeric;

    const auto length = std::size_t(data.size());
    std::size_t payloadBits = 0;
    switch (mode) {
    case QrMode::Numeric:
        payloadBits = length / 3 * 10 + std::array<std::size_t, 3>{0, 4, 7}[length % 3];
        break;
    case QrMode::Alphanumeric:
        payloadBits = length / 2 * 11 + length % 2 * 6;
        break;
    case QrMode::Byte:
        payloadBits = length * 8;
        break;
    }

    const int level = int(d->level);
    int version = 1;
    for (; version <= 40; ++version) {
        const int countBits = characterCountBits(mode, version);
        const std::size_t capacity = std::size_t(dataCodewords(version, level)) * 8;
        if (length < (std::size_t(1) << countBits) && 4 + std::size_t(countBits) + payloadBits <= capacity)
            break;
    }
    if (version > 40) {
        d->fail(QStringLiteral("The data is too long for a QR Code at this error correction level"));
        return;
    }

    BitWriter bits;
    switch (mode) {
    case QrMode::Numeric:
        bits.append(1, 4);
        bits.append(quint32(length), characterCountBits(mode, version));
        for (std::size_t i = 0; i < length; i += 3) {
            const std::size_t n = std::min<std::size_t>(3, length - i);
            quint32 value = 0;
            for (std::size_t k = 0; k < n; ++k)
                value = value * 10 + quint32(data.at(static_cast<qsizetype>(i) + static_cast<qsizetype>(k)) - '0');
            bits.append(value, int(n * 3 + 1));
        }
        break;
    case QrMode::Alphanumeric:
        bits.append(2, 4);
        bits.append(quint32(length), characterCountBits(mode, version));
        for (std::size_t i = 0; i < length; i += 2) {
            const auto first = quint32(alphanumericValue(data.at(qsizetype(i))));
            if (i + 1 < length)
                bits.append(first * 45 + quint32(alphanumericValue(data.at(static_cast<qsizetype>(i) + 1))), 11);
            else
                bits.append(first, 6);
        }
        break;
    case QrMode::Byte:
        bits.append(4, 4);
        bits.append(quint32(length), characterCountBits(mode, version));
        for (char c : data)
            bits.append(quint32(quint8(c)), 8);
        break;
    }

    const std::size_t capacityBits = std::size_t(dataCodewords(version, level)) * 8;
    bits.append(0, int(std::min<std::size_t>(4, capacityBits - bits.size())));
    bits.append(0, int((8 - bits.size() % 8) % 8));
    std::vector<quint8> codewords(bits.size() / 8, 0);
    for (std::size_t i = 0; i < bits.size(); ++i)
        codewords[i >> 3] = quint8(codewords[i >> 3] | (bits.bits()[i] << (7 - (i & 7))));
    for (quint8 pad = 0xEC; codewords.size() < capacityBits / 8; pad ^= 0xEC ^ 0x11)
        codewords.push_back(pad);

    // Split into blocks, append the error correction of each, and interleave.
    const int blocks = kBlockCount[std::size_t(level)][std::size_t(version)];
    const int eccLength = kEccPerBlock[std::size_t(level)][std::size_t(version)];
    const int rawCodewords = rawDataModules(version) / 8;
    const int shortBlocks = blocks - rawCodewords % blocks;
    const int shortBlockLength = rawCodewords / blocks;
    const std::vector<quint8> divisor = reedSolomonDivisor(eccLength);
    std::vector<std::vector<quint8>> blockData;
    std::size_t offset = 0;
    for (int i = 0; i < blocks; ++i) {
        const int blockDataLength = shortBlockLength - eccLength + (i < shortBlocks ? 0 : 1);
        const auto dataLength = static_cast<std::size_t>(blockDataLength);
        std::vector<quint8> block(codewords.begin() + std::ptrdiff_t(offset),
                                  codewords.begin() + std::ptrdiff_t(offset + dataLength));
        offset += dataLength;
        const std::vector<quint8> ecc = reedSolomonRemainder(block, divisor);
        if (i < shortBlocks)
            block.push_back(0);
        block.insert(block.end(), ecc.begin(), ecc.end());
        blockData.push_back(std::move(block));
    }
    std::vector<quint8> interleaved;
    for (std::size_t i = 0; i < blockData.front().size(); ++i) {
        for (std::size_t j = 0; j < blockData.size(); ++j) {
            if (i != std::size_t(shortBlockLength - eccLength) || j >= std::size_t(shortBlocks))
                interleaved.push_back(blockData[j][i]);
        }
    }

    QrMatrix base(version);
    base.drawFunctionPatterns();
    base.drawCodewords(interleaved);
    int bestMask = 0;
    long bestPenalty = -1;
    QrMatrix best(version);
    for (int mask = 0; mask < 8; ++mask) {
        QrMatrix candidate = base;
        candidate.applyMask(mask);
        candidate.drawFormatBits(level, mask);
        const long penalty = candidate.penalty();
        if (bestPenalty < 0 || penalty < bestPenalty) {
            bestPenalty = penalty;
            bestMask = mask;
            best = candidate;
        }
    }

    const int size = best.size();
    std::vector<quint8> modules(std::size_t(size) * std::size_t(size), 0);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x)
            modules[std::size_t(y) * std::size_t(size) + std::size_t(x)] = best.dark(x, y) ? 1 : 0;
    }
    d->setMatrix(size, modules);
    d->version = version;
    d->mask = bestMask;
}

} // namespace QtBarcodePrivate
