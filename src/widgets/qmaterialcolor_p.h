// MIT License - see LICENSE
#pragma once

#include <QtGui/qcolor.h>

namespace QtPrivate {

// A colour as HCT describes it: the hue and chroma of the CAM16 colour appearance model, and the lightness L* of
// CIELAB as the tone. Colours whose tones differ by a given amount have a predictable contrast, which is why Material
// Design makes its palettes in this space. Written from the published CAM16 and CIELAB formulas, for the viewing
// conditions that Material uses (white D65, a mid grey surround).
struct Hct {
    double hue = 0;    // degrees, 0 to 360
    double chroma = 0; // 0 is grey
    double tone = 0;   // 0 (black) to 100 (white)

    static Hct fromColor(const QColor &color);
    // The sRGB colour of that hue and tone with the chroma, or with the highest chroma that sRGB has there.
    static QColor toColor(double hue, double chroma, double tone);
};

} // namespace QtPrivate
