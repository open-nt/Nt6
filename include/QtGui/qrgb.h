// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

// A pixel as an unsigned 32-bit value, 0xAARRGGBB, with the alpha channel in the top byte.
// qRgb(), qRgba(), qRed(), qGreen(), qBlue() and qAlpha() build and take apart such values.
using QRgb = unsigned int;

// Returns the red component of \a rgb, in the range 0 to 255.
constexpr inline int qRed(QRgb rgb) { return static_cast<int>((rgb >> 16) & 0xffu); }
// Returns the green component of \a rgb, in the range 0 to 255.
constexpr inline int qGreen(QRgb rgb) { return static_cast<int>((rgb >> 8) & 0xffu); }
// Returns the blue component of \a rgb, in the range 0 to 255.
constexpr inline int qBlue(QRgb rgb) { return static_cast<int>(rgb & 0xffu); }
// Returns the alpha component of \a rgb, in the range 0 (transparent) to 255
// (opaque).
constexpr inline int qAlpha(QRgb rgb) { return static_cast<int>(rgb >> 24); }
// Returns the opaque pixel value for the components \a r, \a g and \a b, each
// in the range 0 to 255.
constexpr inline QRgb qRgb(int r, int g, int b) {
    return 0xff000000u | (static_cast<QRgb>(r & 0xff) << 16) | (static_cast<QRgb>(g & 0xff) << 8) |
           static_cast<QRgb>(b & 0xff);
}
// Returns the pixel value for the components \a r, \a g, \a b and \a a, each
// in the range 0 to 255.
constexpr inline QRgb qRgba(int r, int g, int b, int a) {
    return (static_cast<QRgb>(a & 0xff) << 24) | (static_cast<QRgb>(r & 0xff) << 16) |
           (static_cast<QRgb>(g & 0xff) << 8) | static_cast<QRgb>(b & 0xff);
}
// Returns the gray value, from 0 to 255, of the color \a r, \a g, \a b.
constexpr inline int qGray(int r, int g, int b) { return (r * 11 + g * 16 + b * 5) / 32; }
// Returns the gray value of \a rgb, ignoring its alpha.
constexpr inline int qGray(QRgb rgb) { return qGray(qRed(rgb), qGreen(rgb), qBlue(rgb)); }
// Returns \a rgb with its color components multiplied by its alpha.
constexpr inline QRgb qPremultiply(QRgb rgb) {
    const QRgb a = rgb >> 24;
    if (a == 255u)
        return rgb;
    const QRgb r = (((rgb >> 16) & 0xffu) * a + 127u) / 255u;
    const QRgb g = (((rgb >> 8) & 0xffu) * a + 127u) / 255u;
    const QRgb b = ((rgb & 0xffu) * a + 127u) / 255u;
    return (a << 24) | (r << 16) | (g << 8) | b;
}
// Returns \a rgb with its color components divided by its alpha; the inverse
// of qPremultiply().
constexpr inline QRgb qUnpremultiply(QRgb rgb) {
    const QRgb a = rgb >> 24;
    if (a == 255u || a == 0u)
        return a == 0u ? 0u : rgb;
    const QRgb r = (((rgb >> 16) & 0xffu) * 255u + a / 2u) / a;
    const QRgb g = (((rgb >> 8) & 0xffu) * 255u + a / 2u) / a;
    const QRgb b = ((rgb & 0xffu) * 255u + a / 2u) / a;
    return (a << 24) | ((r > 255u ? 255u : r) << 16) | ((g > 255u ? 255u : g) << 8) | (b > 255u ? 255u : b);
}
