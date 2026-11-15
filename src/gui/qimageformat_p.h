// MIT License - see LICENSE
#pragma once

// How each QImage::Format stores a pixel in its 32 bits, converted to and from 0xAARRGGBB (straight or
// premultiplied alpha). Shared by QImage, the raster paint engine and the OpenGL paint engine.

#include <QtGui/qimage.h>
#include <QtGui/qrgb.h>

#include <cstring>

namespace QtImageFormat {

// The R, G, B, A byte order of Format_RGBX8888 and Format_RGBA8888, read as a native 32-bit word, from
// 0xAARRGGBB and back.
inline QRgb argbToBytes(QRgb argb) {
    const unsigned char bytes[4] = {static_cast<unsigned char>(qRed(argb)), static_cast<unsigned char>(qGreen(argb)),
                                    static_cast<unsigned char>(qBlue(argb)), static_cast<unsigned char>(qAlpha(argb))};
    QRgb word = 0;
    static_assert(sizeof(word) == sizeof(bytes), "QRgb is 32 bits");
    std::memcpy(&word, bytes, sizeof(word));
    return word;
}

inline QRgb bytesToArgb(QRgb word) {
    unsigned char bytes[4];
    std::memcpy(bytes, &word, sizeof(word));
    return qRgba(bytes[0], bytes[1], bytes[2], bytes[3]);
}

// Returns the stored pixel as straight 0xAARRGGBB.
inline QRgb toArgb(QRgb stored, QImage::Format format) {
    switch (format) {
    case QImage::Format_RGB32:
        return stored | 0xff000000u;
    case QImage::Format_ARGB32_Premultiplied:
        return qUnpremultiply(stored);
    case QImage::Format_RGBX8888:
        return bytesToArgb(stored) | 0xff000000u;
    case QImage::Format_RGBA8888:
        return bytesToArgb(stored);
    case QImage::Format_RGBA8888_Premultiplied:
        return qUnpremultiply(bytesToArgb(stored));
    default:
        return stored;
    }
}

// Returns the straight 0xAARRGGBB pixel as stored.
inline QRgb fromArgb(QRgb argb, QImage::Format format) {
    switch (format) {
    case QImage::Format_RGB32:
        return argb | 0xff000000u;
    case QImage::Format_ARGB32_Premultiplied:
        return qPremultiply(argb);
    case QImage::Format_RGBX8888:
        return argbToBytes(argb | 0xff000000u);
    case QImage::Format_RGBA8888:
        return argbToBytes(argb);
    case QImage::Format_RGBA8888_Premultiplied:
        return argbToBytes(qPremultiply(argb));
    default:
        return argb;
    }
}

// Returns the stored pixel as premultiplied 0xAARRGGBB.
inline QRgb toPremultiplied(QRgb stored, QImage::Format format) {
    switch (format) {
    case QImage::Format_RGB32:
        return stored | 0xff000000u;
    case QImage::Format_ARGB32:
        return qPremultiply(stored);
    case QImage::Format_RGBX8888:
        return bytesToArgb(stored) | 0xff000000u;
    case QImage::Format_RGBA8888:
        return qPremultiply(bytesToArgb(stored));
    case QImage::Format_RGBA8888_Premultiplied:
        return bytesToArgb(stored);
    default:
        return stored;
    }
}

// Returns the premultiplied 0xAARRGGBB pixel as stored.
inline QRgb fromPremultiplied(QRgb premultiplied, QImage::Format format) {
    switch (format) {
    case QImage::Format_RGB32:
        return qUnpremultiply(premultiplied) | 0xff000000u;
    case QImage::Format_ARGB32:
        return qUnpremultiply(premultiplied);
    case QImage::Format_RGBX8888:
        return argbToBytes(qUnpremultiply(premultiplied) | 0xff000000u);
    case QImage::Format_RGBA8888:
        return argbToBytes(qUnpremultiply(premultiplied));
    case QImage::Format_RGBA8888_Premultiplied:
        return argbToBytes(premultiplied);
    default:
        return premultiplied;
    }
}

// Returns true if the format has no alpha channel (alpha always reads as 255).
inline bool isOpaque(QImage::Format format) {
    return format == QImage::Format_RGB32 || format == QImage::Format_RGBX8888;
}

// The stored value of an opaque black pixel for opaque formats, or of transparent black for the others.
inline QRgb clearValue(QImage::Format format) { return isOpaque(format) ? fromArgb(0xff000000u, format) : 0u; }

} // namespace QtImageFormat
