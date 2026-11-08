// MIT License - see LICENSE
#pragma once

#include <QtCore/qstringlist.h>
#include <QtGui/qfont.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

// A rendered glyph: 8-bit coverage, and where it sits relative to the pen position on the baseline.
struct QGlyph {
    int advance64 = 0; // 26.6 fixed point, pixels
    int left = 0;      // pixels from the pen to the left edge of the bitmap
    int top = 0;       // pixels from the baseline up to the top edge of the bitmap
    int width = 0;
    int height = 0;
    int stride = 0;
    std::vector<unsigned char> bitmap;
};

// One font at one pixel size, found through fontconfig and rasterized by FreeType. Without those
// libraries the engine has the metrics of a monospace font and draws nothing. Engines are shared
// and thread-safe.
class QFontEngine {
public:
    // The engine for `font`; the same one for equal fonts.
    // `scale` multiplies the pixel size, for painting on a device with more pixels per unit.
    static std::shared_ptr<QFontEngine> create(const QFont &font, double scale = 1.0);
    ~QFontEngine();
    QFontEngine(const QFontEngine &) = delete;
    QFontEngine &operator=(const QFontEngine &) = delete;

    int ascent() const { return m_ascent; }
    int descent() const { return m_descent; }
    int leading() const { return m_leading; }
    int xHeight() const { return m_xHeight; }
    int averageCharWidth() const { return m_averageCharWidth; }
    // Whether the font has a glyph for `codePoint`.
    bool hasGlyph(char32_t codePoint);
    // The glyph for `codePoint`, or the font's replacement glyph. Valid as long as the engine lives.
    const QGlyph &glyph(char32_t codePoint);

    // Pixel size that a QFont stands for.
    static double pixelSizeOf(const QFont &font);
    // The font families that fontconfig knows, sorted; empty without fontconfig.
    static QStringList families();

private:
    QFontEngine(void *face, double pixelSize);
    void measure();

    std::mutex m_mutex;
    void *m_face = nullptr; // FT_Face
    double m_pixelSize;
    int m_ascent = 0;
    int m_descent = 0;
    int m_leading = 0;
    int m_xHeight = 0;
    int m_averageCharWidth = 0;
    std::unordered_map<char32_t, QGlyph> m_glyphs;
};
