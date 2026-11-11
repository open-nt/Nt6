// MIT License - see LICENSE
#include "qimagecodecs_p.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace NtImageCodecs {

namespace {

constexpr quint32 kBiRgb = 0;
constexpr quint32 kBiBitfields = 3;

quint32 le16(const uchar *p) { return static_cast<quint32>(p[0]) | (static_cast<quint32>(p[1]) << 8); }
quint32 le32(const uchar *p) { return le16(p) | (le16(p + 2) << 16); }

void putLe16(QByteArray *out, quint32 v) {
    const char bytes[2] = {static_cast<char>(v), static_cast<char>(v >> 8)};
    out->append(bytes, 2);
}

void putLe32(QByteArray *out, quint32 v) {
    putLe16(out, v & 0xffff);
    putLe16(out, v >> 16);
}

// Extracts the component of \a pixel under \a mask and scales it to 8 bits.
unsigned component(quint32 pixel, quint32 mask) {
    if (mask == 0)
        return 0;
    unsigned shift = 0;
    while (((mask >> shift) & 1) == 0)
        ++shift;
    const quint32 max = mask >> shift;
    const quint32 value = (pixel & mask) >> shift;
    return max == 255 ? value : static_cast<unsigned>((static_cast<quint64>(value) * 255 + max / 2) / max);
}

} // namespace

QImage readBmp(const uchar *data, std::size_t size) {
    if (size < 14 + 12 || data[0] != 'B' || data[1] != 'M')
        return QImage();
    const quint32 pixelOffset = le32(data + 10);
    const uchar *dib = data + 14;
    const quint32 headerSize = le32(dib);
    if (headerSize < 12 || size < 14 + static_cast<std::size_t>(headerSize))
        return QImage();

    qint64 width = 0;
    qint64 height = 0;
    quint32 bits = 0;
    quint32 compression = kBiRgb;
    quint32 colors = 0;
    quint32 masks[4] = {0, 0, 0, 0};
    std::size_t paletteEntrySize = 4;
    if (headerSize == 12) {
        width = static_cast<qint64>(le16(dib + 4));
        height = static_cast<qint64>(le16(dib + 6));
        bits = le16(dib + 10);
        paletteEntrySize = 3;
    } else {
        if (headerSize < 40)
            return QImage();
        width = static_cast<qint32>(le32(dib + 4));
        height = static_cast<qint32>(le32(dib + 8));
        bits = le16(dib + 14);
        compression = le32(dib + 16);
        colors = le32(dib + 32);
    }
    const bool topDown = height < 0;
    if (topDown)
        height = -height;
    if (width <= 0 || height <= 0 || width > 0x7fffffff || height > 0x7fffffff)
        return QImage();
    if (static_cast<std::size_t>(width) > kMaxPixels / static_cast<std::size_t>(height))
        return QImage();
    if (bits != 1 && bits != 4 && bits != 8 && bits != 16 && bits != 24 && bits != 32)
        return QImage();
    if (compression != kBiRgb && !(compression == kBiBitfields && (bits == 16 || bits == 32)))
        return QImage();

    std::size_t paletteStart = 14 + static_cast<std::size_t>(headerSize);
    if (compression == kBiBitfields) {
        if (headerSize >= 52) {
            for (int i = 0; i < 3; ++i)
                masks[i] = le32(dib + 40 + 4 * i);
            if (headerSize >= 56)
                masks[3] = le32(dib + 52);
        } else {
            // With a 40-byte header the masks follow it.
            if (size < paletteStart + 12)
                return QImage();
            for (int i = 0; i < 3; ++i)
                masks[i] = le32(data + paletteStart + 4 * static_cast<std::size_t>(i));
            paletteStart += 12;
        }
    } else if (bits == 16) {
        masks[0] = 0x7c00;
        masks[1] = 0x03e0;
        masks[2] = 0x001f;
    } else if (bits == 32) {
        masks[0] = 0x00ff0000;
        masks[1] = 0x0000ff00;
        masks[2] = 0x000000ff;
        if (headerSize >= 56 && le32(dib + 52) != 0)
            masks[3] = le32(dib + 52);
    }
    const bool hasAlpha = masks[3] != 0;

    std::vector<QRgb> palette;
    if (bits <= 8) {
        const quint32 count = colors != 0 ? colors : (1u << bits);
        if (count > 256 || paletteStart + static_cast<std::size_t>(count) * paletteEntrySize > size)
            return QImage();
        for (quint32 i = 0; i < count; ++i) {
            const uchar *e = data + paletteStart + static_cast<std::size_t>(i) * paletteEntrySize;
            palette.push_back(qRgb(e[2], e[1], e[0]));
        }
    }

    const std::size_t rowBytes = ((static_cast<std::size_t>(width) * bits + 31) / 32) * 4;
    if (pixelOffset > size || (size - pixelOffset) / rowBytes < static_cast<std::size_t>(height))
        return QImage();

    QImage image(static_cast<int>(width), static_cast<int>(height),
                 hasAlpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
    if (image.isNull())
        return QImage();
    for (int y = 0; y < image.height(); ++y) {
        const int srcRow = topDown ? y : image.height() - 1 - y;
        const uchar *src = data + pixelOffset + static_cast<std::size_t>(srcRow) * rowBytes;
        QRgb *dst = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const std::size_t ux = static_cast<std::size_t>(x);
            switch (bits) {
            case 1:
            case 4:
            case 8: {
                const std::size_t bit = ux * bits;
                const unsigned index = (src[bit >> 3] >> (8 - bits - (bit & 7))) & ((1u << bits) - 1);
                dst[x] = index < palette.size() ? palette[index] : qRgb(0, 0, 0);
                break;
            }
            case 24:
                dst[x] = qRgb(src[ux * 3 + 2], src[ux * 3 + 1], src[ux * 3]);
                break;
            default: {
                const quint32 px = bits == 16 ? le16(src + ux * 2) : le32(src + ux * 4);
                dst[x] = qRgba(static_cast<int>(component(px, masks[0])), static_cast<int>(component(px, masks[1])),
                               static_cast<int>(component(px, masks[2])),
                               hasAlpha ? static_cast<int>(component(px, masks[3])) : 255);
                break;
            }
            }
        }
    }
    return image;
}

bool writeBmp(const QImage &image, QByteArray *out) {
    if (image.isNull())
        return false;
    const bool alpha = image.hasAlphaChannel();
    const std::size_t width = static_cast<std::size_t>(image.width());
    const std::size_t height = static_cast<std::size_t>(image.height());
    const std::size_t bytesPerPixel = alpha ? 4 : 3;
    const std::size_t rowBytes = ((width * bytesPerPixel * 8 + 31) / 32) * 4;
    const std::size_t headerSize = alpha ? 108 : 40;
    const std::size_t offset = 14 + headerSize;
    const std::size_t fileSize = offset + rowBytes * height;
    if (fileSize > 0xffffffffu)
        return false;

    out->clear();
    out->reserve(static_cast<qsizetype>(fileSize));
    out->append("BM", 2);
    putLe32(out, static_cast<quint32>(fileSize));
    putLe32(out, 0);
    putLe32(out, static_cast<quint32>(offset));
    putLe32(out, static_cast<quint32>(headerSize));
    putLe32(out, static_cast<quint32>(width));
    putLe32(out, static_cast<quint32>(height));
    putLe16(out, 1);
    putLe16(out, static_cast<quint32>(bytesPerPixel * 8));
    putLe32(out, alpha ? kBiBitfields : kBiRgb);
    putLe32(out, static_cast<quint32>(rowBytes * height));
    putLe32(out, 2835); // 72 dpi
    putLe32(out, 2835);
    putLe32(out, 0);
    putLe32(out, 0);
    if (alpha) {
        putLe32(out, 0x00ff0000);
        putLe32(out, 0x0000ff00);
        putLe32(out, 0x000000ff);
        putLe32(out, 0xff000000);
        putLe32(out, 0x73524742); // "sRGB"
        for (int i = 0; i < 9 + 3; ++i)
            putLe32(out, 0); // the color space endpoints and gamma
    }

    std::vector<char> row(rowBytes, 0);
    for (int y = image.height() - 1; y >= 0; --y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
        for (std::size_t x = 0; x < width; ++x) {
            QRgb px = line[x];
            if (image.format() == QImage::Format_ARGB32_Premultiplied)
                px = qUnpremultiply(px);
            char *dst = row.data() + x * bytesPerPixel;
            dst[0] = static_cast<char>(qBlue(px));
            dst[1] = static_cast<char>(qGreen(px));
            dst[2] = static_cast<char>(qRed(px));
            if (alpha)
                dst[3] = static_cast<char>(qAlpha(px));
        }
        out->append(row.data(), static_cast<qsizetype>(rowBytes));
    }
    return true;
}

} // namespace NtImageCodecs
