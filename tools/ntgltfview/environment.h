// MIT License - see LICENSE
//
// The environment image of ntgltfview: an equirectangular panorama, from a Radiance .hdr file or from any image Nt can
// decode, as half-float RGBA for the GPU.
#pragma once

#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QString>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Environment {

// The half-float bits of `value`, rounded to nearest; too large a value becomes the largest half.
inline uint16_t toHalf(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const uint32_t sign = (bits >> 16) & 0x8000u;
    if (std::isnan(value))
        return static_cast<uint16_t>(sign | 0x7e00u);
    float magnitude = std::fabs(value);
    if (magnitude > 65504.0f)
        magnitude = 65504.0f;
    std::memcpy(&bits, &magnitude, sizeof(bits));
    const int exponent = static_cast<int>((bits >> 23) & 0xffu) - 127 + 15;
    uint32_t mantissa = bits & 0x7fffffu;
    if (exponent <= 0) {
        if (exponent < -10)
            return static_cast<uint16_t>(sign);
        mantissa |= 0x800000u;
        const int shift = 14 - exponent;
        const uint32_t half = mantissa >> shift;
        return static_cast<uint16_t>(sign | (half + ((mantissa >> (shift - 1)) & 1u)));
    }
    const uint32_t half = (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13);
    return static_cast<uint16_t>(sign | (half + ((mantissa >> 12) & 1u)));
}

// A Radiance RGBE file: the header, then scanlines that are run-length encoded or flat.
inline bool decodeRadiance(const QByteArray &file, int *width, int *height, std::vector<float> *rgb, QString *error) {
    const char *data = file.constData();
    const qsizetype size = file.size();
    if (!file.startsWith("#?")) {
        *error = QStringLiteral("not a Radiance file");
        return false;
    }
    qsizetype pos = 0;
    // Header lines, up to the empty one; then the resolution line.
    const auto line = [&]() {
        const qsizetype start = pos;
        while (pos < size && data[pos] != '\n')
            ++pos;
        const QByteArray text(data + start, pos - start);
        if (pos < size)
            ++pos;
        return text;
    };
    bool formatOk = false;
    for (;;) {
        if (pos >= size) {
            *error = QStringLiteral("the header is cut off");
            return false;
        }
        const QByteArray header = line();
        if (header.isEmpty())
            break;
        if (header.startsWith("FORMAT=32-bit_rle_rgbe"))
            formatOk = true;
    }
    int w = 0;
    int h = 0;
    char yAxis = 0;
    char xAxis = 0;
    if (!formatOk || std::sscanf(line().constData(), "%c%*c %d %c%*c %d", &yAxis, &h, &xAxis, &w) != 4 ||
        yAxis != '-' || xAxis != '+' || w <= 0 || h <= 0 || w > 16384 || h > 16384) {
        *error = QStringLiteral("only -Y +X files in 32-bit RGBE are read");
        return false;
    }
    rgb->assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3, 0.0f);
    std::vector<uint8_t> scan(static_cast<std::size_t>(w) * 4);
    for (int y = 0; y < h; ++y) {
        if (pos + 4 > size) {
            *error = QStringLiteral("the pixels are cut off");
            return false;
        }
        const auto *p = reinterpret_cast<const uint8_t *>(data + pos);
        if (w >= 8 && w < 32768 && p[0] == 2 && p[1] == 2 && (p[2] & 0x80) == 0 && ((p[2] << 8) | p[3]) == w) {
            pos += 4;
            for (int channel = 0; channel < 4; ++channel) {
                int x = 0;
                while (x < w) {
                    if (pos >= size) {
                        *error = QStringLiteral("the pixels are cut off");
                        return false;
                    }
                    int count = static_cast<uint8_t>(data[pos++]);
                    if (count > 128) {
                        count -= 128;
                        if (pos >= size || x + count > w) {
                            *error = QStringLiteral("a run is out of range");
                            return false;
                        }
                        const uint8_t value = static_cast<uint8_t>(data[pos++]);
                        while (count-- > 0)
                            scan[static_cast<std::size_t>(x++) * 4 + static_cast<std::size_t>(channel)] = value;
                    } else {
                        if (count == 0 || pos + count > size || x + count > w) {
                            *error = QStringLiteral("a run is out of range");
                            return false;
                        }
                        while (count-- > 0)
                            scan[static_cast<std::size_t>(x++) * 4 + static_cast<std::size_t>(channel)] =
                                static_cast<uint8_t>(data[pos++]);
                    }
                }
            }
        } else {
            if (pos + static_cast<qsizetype>(w) * 4 > size) {
                *error = QStringLiteral("the pixels are cut off");
                return false;
            }
            std::memcpy(scan.data(), data + pos, scan.size());
            pos += static_cast<qsizetype>(scan.size());
        }
        for (int x = 0; x < w; ++x) {
            const uint8_t *e = &scan[static_cast<std::size_t>(x) * 4];
            float *out =
                &(*rgb)[(static_cast<std::size_t>(y) * static_cast<std::size_t>(w) + static_cast<std::size_t>(x)) * 3];
            if (e[3] != 0) {
                const float scale = std::ldexp(1.0f, static_cast<int>(e[3]) - 136);
                out[0] = static_cast<float>(e[0]) * scale;
                out[1] = static_cast<float>(e[1]) * scale;
                out[2] = static_cast<float>(e[2]) * scale;
            }
        }
    }
    *width = w;
    *height = h;
    return true;
}

// Loads the panorama in `path` as half-float RGBA, linear. Images other than .hdr are taken as sRGB.
inline bool load(const QString &path, int *width, int *height, std::vector<uint16_t> *halfRgba, QString *error) {
    std::vector<float> rgb;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("cannot open %1").arg(path);
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.startsWith("#?")) {
        if (!decodeRadiance(bytes, width, height, &rgb, error))
            return false;
    } else {
        const QImage image = QImage::fromData(bytes).convertToFormat(QImage::Format_ARGB32);
        if (image.isNull()) {
            *error = QStringLiteral("%1 is not an image Nt can decode").arg(path);
            return false;
        }
        *width = image.width();
        *height = image.height();
        rgb.resize(static_cast<std::size_t>(*width) * static_cast<std::size_t>(*height) * 3);
        for (int y = 0; y < *height; ++y) {
            const auto *row = reinterpret_cast<const uint32_t *>(image.constScanLine(y));
            for (int x = 0; x < *width; ++x) {
                const uint32_t argb = row[x];
                const float channels[3] = {static_cast<float>((argb >> 16) & 0xffu),
                                           static_cast<float>((argb >> 8) & 0xffu), static_cast<float>(argb & 0xffu)};
                for (int c = 0; c < 3; ++c)
                    rgb[(static_cast<std::size_t>(y) * static_cast<std::size_t>(*width) + static_cast<std::size_t>(x)) *
                            3 +
                        static_cast<std::size_t>(c)] = std::pow(channels[c] / 255.0f, 2.2f);
            }
        }
    }
    const std::size_t pixels = static_cast<std::size_t>(*width) * static_cast<std::size_t>(*height);
    halfRgba->resize(pixels * 4);
    for (std::size_t i = 0; i < pixels; ++i) {
        (*halfRgba)[i * 4 + 0] = toHalf(rgb[i * 3 + 0]);
        (*halfRgba)[i * 4 + 1] = toHalf(rgb[i * 3 + 1]);
        (*halfRgba)[i * 4 + 2] = toHalf(rgb[i * 3 + 2]);
        (*halfRgba)[i * 4 + 3] = toHalf(1.0f);
    }
    return true;
}

} // namespace Environment
