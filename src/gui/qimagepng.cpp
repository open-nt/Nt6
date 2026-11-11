// MIT License - see LICENSE
#include "qimagecodecs_p.h"

#ifdef NT_HAVE_ZLIB
#include <zlib.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace NtImageCodecs {

#ifdef NT_HAVE_ZLIB

namespace {

constexpr uchar kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};

quint32 be32(const uchar *p) {
    return (static_cast<quint32>(p[0]) << 24) | (static_cast<quint32>(p[1]) << 16) | (static_cast<quint32>(p[2]) << 8) |
           static_cast<quint32>(p[3]);
}

void putBe32(QByteArray *out, quint32 v) {
    const char bytes[4] = {static_cast<char>(v >> 24), static_cast<char>(v >> 16), static_cast<char>(v >> 8),
                           static_cast<char>(v)};
    out->append(bytes, 4);
}

quint32 crcOf(const uchar *type, const uchar *data, std::size_t size) {
    uLong crc = crc32(0L, type, 4);
    if (size)
        crc = crc32(crc, data, static_cast<uInt>(size));
    return static_cast<quint32>(crc);
}

void appendChunk(QByteArray *out, const char *type, const uchar *data, std::size_t size) {
    putBe32(out, static_cast<quint32>(size));
    out->append(type, 4);
    if (size)
        out->append(reinterpret_cast<const char *>(data), static_cast<qsizetype>(size));
    putBe32(out, crcOf(reinterpret_cast<const uchar *>(type), data, size));
}

int channelsFor(int colorType) {
    switch (colorType) {
    case 0:
    case 3:
        return 1;
    case 2:
        return 3;
    case 4:
        return 2;
    case 6:
        return 4;
    default:
        return 0;
    }
}

bool validDepth(int colorType, int depth) {
    switch (colorType) {
    case 0:
        return depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16;
    case 3:
        return depth == 1 || depth == 2 || depth == 4 || depth == 8;
    case 2:
    case 4:
    case 6:
        return depth == 8 || depth == 16;
    default:
        return false;
    }
}

struct Pass {
    unsigned xStart, yStart, xStep, yStep;
};

constexpr Pass kAdam7[7] = {{0, 0, 8, 8}, {4, 0, 8, 8}, {0, 4, 4, 8}, {2, 0, 4, 4},
                            {0, 2, 2, 4}, {1, 0, 2, 2}, {0, 1, 1, 2}};
constexpr Pass kSinglePass = {0, 0, 1, 1};

struct PassSize {
    std::size_t width = 0;
    std::size_t height = 0;
    std::size_t rowBytes = 0;
};

PassSize sizeOfPass(const Pass &pass, quint32 width, quint32 height, std::size_t bitsPerPixel) {
    PassSize s;
    if (width <= pass.xStart || height <= pass.yStart)
        return s;
    s.width = (width - pass.xStart + pass.xStep - 1) / pass.xStep;
    s.height = (height - pass.yStart + pass.yStep - 1) / pass.yStep;
    s.rowBytes = (s.width * bitsPerPixel + 7) / 8;
    return s;
}

int paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = std::abs(p - a);
    const int pb = std::abs(p - b);
    const int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc)
        return a;
    return pb <= pc ? b : c;
}

void filterRow(int type, const uchar *cur, const uchar *prev, uchar *dst, std::size_t n, std::size_t bpp) {
    for (std::size_t i = 0; i < n; ++i) {
        const int a = i >= bpp ? cur[i - bpp] : 0;
        const int b = prev[i];
        const int c = i >= bpp ? prev[i - bpp] : 0;
        int predictor = 0;
        switch (type) {
        case 1:
            predictor = a;
            break;
        case 2:
            predictor = b;
            break;
        case 3:
            predictor = (a + b) / 2;
            break;
        case 4:
            predictor = paeth(a, b, c);
            break;
        default:
            break;
        }
        dst[i] = static_cast<uchar>(cur[i] - predictor);
    }
}

bool unfilterRow(int type, uchar *row, const uchar *prev, std::size_t n, std::size_t bpp) {
    switch (type) {
    case 0:
        return true;
    case 1:
        for (std::size_t i = bpp; i < n; ++i)
            row[i] = static_cast<uchar>(row[i] + row[i - bpp]);
        return true;
    case 2:
        for (std::size_t i = 0; i < n; ++i)
            row[i] = static_cast<uchar>(row[i] + prev[i]);
        return true;
    case 3:
        for (std::size_t i = 0; i < n; ++i) {
            const int a = i >= bpp ? row[i - bpp] : 0;
            row[i] = static_cast<uchar>(row[i] + (a + prev[i]) / 2);
        }
        return true;
    case 4:
        for (std::size_t i = 0; i < n; ++i) {
            const int a = i >= bpp ? row[i - bpp] : 0;
            const int c = i >= bpp ? prev[i - bpp] : 0;
            row[i] = static_cast<uchar>(row[i] + paeth(a, prev[i], c));
        }
        return true;
    default:
        return false;
    }
}

// Inflates a zlib stream that must be exactly \a expected bytes. The buffer grows as data comes out, so a
// small file cannot make this allocate a large one.
bool inflateExact(const std::vector<uchar> &in, std::size_t expected, std::vector<uchar> *out) {
    if (in.size() > std::numeric_limits<uInt>::max())
        return false;
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (inflateInit(&zs) != Z_OK)
        return false;
    out->assign(std::min<std::size_t>(expected, 1u << 16), 0);
    zs.next_in = const_cast<Bytef *>(in.data());
    zs.avail_in = static_cast<uInt>(in.size());
    bool ok = false;
    std::size_t done = 0;
    for (;;) {
        if (done == out->size() && done < expected)
            out->resize(std::min(expected, out->size() * 2));
        zs.next_out = out->data() + done;
        zs.avail_out = static_cast<uInt>(std::min<std::size_t>(out->size() - done, std::numeric_limits<uInt>::max()));
        const int rc = inflate(&zs, Z_NO_FLUSH);
        done = zs.total_out;
        if (rc == Z_STREAM_END) {
            ok = done == expected;
            break;
        }
        if (rc != Z_OK && rc != Z_BUF_ERROR)
            break;
        if (zs.avail_in == 0 && zs.avail_out != 0)
            break; // the data ends early
        if (done == expected && zs.avail_out == 0)
            break; // there is more data than the image needs
    }
    inflateEnd(&zs);
    return ok;
}

struct Header {
    quint32 width = 0;
    quint32 height = 0;
    int depth = 0;
    int colorType = 0;
    bool interlaced = false;
};

unsigned scaleTo8(unsigned v, int depth) {
    switch (depth) {
    case 16:
        return (v * 255 + 32767) / 65535;
    case 8:
        return v;
    default:
        return v * 255 / ((1u << depth) - 1);
    }
}

} // namespace

QImage readPng(const uchar *data, std::size_t size) {
    if (size < 8 + 12 + 13 || std::memcmp(data, kSignature, 8) != 0)
        return QImage();

    Header h;
    std::vector<uchar> palette;
    std::vector<uchar> transparency;
    std::vector<uchar> idat;
    bool haveHeader = false;
    std::size_t pos = 8;
    while (pos + 12 <= size) {
        const quint32 length = be32(data + pos);
        const uchar *type = data + pos + 4;
        const uchar *body = data + pos + 8;
        if (length > size - pos - 12)
            break;
        const bool critical = (type[0] & 0x20) == 0;
        if (crcOf(type, body, length) != be32(body + length)) {
            if (critical)
                return QImage();
            pos += 12 + length;
            continue;
        }
        if (!haveHeader && std::memcmp(type, "IHDR", 4) != 0)
            return QImage();
        if (std::memcmp(type, "IHDR", 4) == 0) {
            if (haveHeader || length != 13)
                return QImage();
            h.width = be32(body);
            h.height = be32(body + 4);
            h.depth = body[8];
            h.colorType = body[9];
            h.interlaced = body[12] == 1;
            if (h.width == 0 || h.height == 0 || h.width > 0x7fffffffu || h.height > 0x7fffffffu ||
                !validDepth(h.colorType, h.depth) || body[10] != 0 || body[11] != 0 || body[12] > 1)
                return QImage();
            if (h.width > kMaxPixels / h.height)
                return QImage();
            haveHeader = true;
        } else if (std::memcmp(type, "PLTE", 4) == 0) {
            if (length % 3 != 0 || length == 0 || length > 768 || !idat.empty())
                return QImage();
            palette.assign(body, body + length);
        } else if (std::memcmp(type, "tRNS", 4) == 0) {
            transparency.assign(body, body + length);
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.insert(idat.end(), body, body + length);
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += 12 + length;
    }
    if (!haveHeader || idat.empty())
        return QImage();
    if (h.colorType == 3 && palette.empty())
        return QImage();

    const std::size_t channels = static_cast<std::size_t>(channelsFor(h.colorType));
    const std::size_t bitsPerPixel = channels * static_cast<std::size_t>(h.depth);
    const std::size_t bpp = std::max<std::size_t>(1, bitsPerPixel / 8);

    const Pass *passes = h.interlaced ? kAdam7 : &kSinglePass;
    const int passCount = h.interlaced ? 7 : 1;
    std::size_t expected = 0;
    for (int p = 0; p < passCount; ++p) {
        const PassSize ps = sizeOfPass(passes[p], h.width, h.height, bitsPerPixel);
        expected += ps.height * (1 + ps.rowBytes);
    }
    std::vector<uchar> raw;
    if (!inflateExact(idat, expected, &raw))
        return QImage();

    // Colors that are transparent: a gray or RGB color key, or the alpha of palette entries.
    const bool colorKey =
        (h.colorType == 0 && transparency.size() >= 2) || (h.colorType == 2 && transparency.size() >= 6);
    const bool hasAlpha =
        h.colorType == 4 || h.colorType == 6 || colorKey || (h.colorType == 3 && !transparency.empty());
    unsigned key[3] = {0, 0, 0};
    if (colorKey) {
        const int n = h.colorType == 0 ? 1 : 3;
        for (int i = 0; i < n; ++i) {
            key[i] = (static_cast<unsigned>(transparency[static_cast<std::size_t>(i) * 2]) << 8) |
                     transparency[static_cast<std::size_t>(i) * 2 + 1];
            if (h.depth < 16)
                key[i] &= (1u << h.depth) - 1;
        }
    }

    QImage image(static_cast<int>(h.width), static_cast<int>(h.height),
                 hasAlpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    if (image.isNull())
        return QImage();

    const std::size_t depth = static_cast<std::size_t>(h.depth);
    std::size_t offset = 0;
    for (int p = 0; p < passCount; ++p) {
        const Pass &pass = passes[p];
        const PassSize ps = sizeOfPass(pass, h.width, h.height, bitsPerPixel);
        if (ps.width == 0)
            continue;
        const std::vector<uchar> zeros(ps.rowBytes, 0);
        const uchar *prev = zeros.data();
        for (std::size_t j = 0; j < ps.height; ++j) {
            uchar *row = raw.data() + offset + 1;
            if (!unfilterRow(raw[offset], row, prev, ps.rowBytes, bpp))
                return QImage();
            offset += 1 + ps.rowBytes;
            prev = row;

            QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(static_cast<int>(pass.yStart + j * pass.yStep)));
            auto sample = [&](std::size_t index) -> unsigned {
                if (depth == 8)
                    return row[index];
                if (depth == 16)
                    return (static_cast<unsigned>(row[2 * index]) << 8) | row[2 * index + 1];
                const std::size_t bit = index * depth;
                const unsigned shift = static_cast<unsigned>(8 - depth - (bit & 7));
                return (row[bit >> 3] >> shift) & ((1u << depth) - 1);
            };
            for (std::size_t i = 0; i < ps.width; ++i) {
                unsigned r = 0, g = 0, b = 0, a = 255;
                switch (h.colorType) {
                case 0: {
                    const unsigned v = sample(i);
                    r = g = b = scaleTo8(v, h.depth);
                    if (colorKey && v == key[0])
                        a = 0;
                    break;
                }
                case 2: {
                    const unsigned vr = sample(i * 3);
                    const unsigned vg = sample(i * 3 + 1);
                    const unsigned vb = sample(i * 3 + 2);
                    r = scaleTo8(vr, h.depth);
                    g = scaleTo8(vg, h.depth);
                    b = scaleTo8(vb, h.depth);
                    if (colorKey && vr == key[0] && vg == key[1] && vb == key[2])
                        a = 0;
                    break;
                }
                case 3: {
                    const std::size_t index = sample(i);
                    if (index * 3 + 2 < palette.size()) {
                        r = palette[index * 3];
                        g = palette[index * 3 + 1];
                        b = palette[index * 3 + 2];
                    }
                    if (index < transparency.size())
                        a = transparency[index];
                    break;
                }
                case 4:
                    r = g = b = scaleTo8(sample(i * 2), h.depth);
                    a = scaleTo8(sample(i * 2 + 1), h.depth);
                    break;
                default:
                    r = scaleTo8(sample(i * 4), h.depth);
                    g = scaleTo8(sample(i * 4 + 1), h.depth);
                    b = scaleTo8(sample(i * 4 + 2), h.depth);
                    a = scaleTo8(sample(i * 4 + 3), h.depth);
                    break;
                }
                line[pass.xStart + i * pass.xStep] =
                    qRgba(static_cast<int>(r), static_cast<int>(g), static_cast<int>(b), static_cast<int>(a));
            }
        }
    }
    return image;
}

bool writePng(const QImage &image, QByteArray *out, int quality) {
    if (image.isNull())
        return false;
    const bool alpha = image.hasAlphaChannel();
    const std::size_t channels = alpha ? 4 : 3;
    const std::size_t width = static_cast<std::size_t>(image.width());
    const std::size_t height = static_cast<std::size_t>(image.height());
    const std::size_t rowBytes = width * channels;
    const std::size_t bpp = channels;

    std::vector<uchar> filtered;
    filtered.reserve(height * (rowBytes + 1));
    std::vector<uchar> prev(rowBytes, 0);
    std::vector<uchar> cur(rowBytes);
    std::vector<uchar> candidate(rowBytes);
    std::vector<uchar> best(rowBytes);
    for (std::size_t y = 0; y < height; ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(static_cast<int>(y)));
        for (std::size_t x = 0; x < width; ++x) {
            QRgb px = line[x];
            if (image.format() == QImage::Format_ARGB32_Premultiplied)
                px = qUnpremultiply(px);
            cur[x * channels] = static_cast<uchar>(qRed(px));
            cur[x * channels + 1] = static_cast<uchar>(qGreen(px));
            cur[x * channels + 2] = static_cast<uchar>(qBlue(px));
            if (alpha)
                cur[x * channels + 3] = static_cast<uchar>(qAlpha(px));
        }
        // The filter whose output has the smallest sum of absolute values usually compresses best.
        int bestType = 0;
        std::size_t bestScore = std::numeric_limits<std::size_t>::max();
        for (int type = 0; type <= 4; ++type) {
            filterRow(type, cur.data(), prev.data(), candidate.data(), rowBytes, bpp);
            std::size_t score = 0;
            for (uchar v : candidate)
                score += static_cast<std::size_t>(std::abs(static_cast<signed char>(v)));
            if (score < bestScore) {
                bestScore = score;
                bestType = type;
                best.swap(candidate);
            }
        }
        filtered.push_back(static_cast<uchar>(bestType));
        filtered.insert(filtered.end(), best.begin(), best.end());
        prev.swap(cur);
    }

    int level = Z_DEFAULT_COMPRESSION;
    if (quality >= 0)
        level = 9 - (std::min(quality, 100) * 9 + 50) / 100;
    uLongf compressedSize = compressBound(static_cast<uLong>(filtered.size()));
    std::vector<uchar> compressed(compressedSize);
    if (compress2(compressed.data(), &compressedSize, filtered.data(), static_cast<uLong>(filtered.size()), level) !=
        Z_OK)
        return false;

    out->clear();
    out->append(reinterpret_cast<const char *>(kSignature), 8);
    uchar ihdr[13];
    const quint32 w = static_cast<quint32>(image.width());
    const quint32 hgt = static_cast<quint32>(image.height());
    for (int i = 0; i < 4; ++i) {
        ihdr[i] = static_cast<uchar>(w >> (24 - 8 * i));
        ihdr[4 + i] = static_cast<uchar>(hgt >> (24 - 8 * i));
    }
    ihdr[8] = 8;
    ihdr[9] = alpha ? 6 : 2;
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    appendChunk(out, "IHDR", ihdr, sizeof(ihdr));
    appendChunk(out, "IDAT", compressed.data(), compressedSize);
    appendChunk(out, "IEND", nullptr, 0);
    return true;
}

#else // NT_HAVE_ZLIB

QImage readPng(const uchar *, std::size_t) { return QImage(); }
bool writePng(const QImage &, QByteArray *, int) { return false; }

#endif

} // namespace NtImageCodecs
