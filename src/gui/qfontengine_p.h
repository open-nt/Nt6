// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
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
    // The glyph for `codePoint`: the font's, or else that of a system font that has the character, or else the font's
    // replacement glyph. Valid as long as the engine lives.
    const QGlyph &glyph(char32_t codePoint);

    // Pixel size that a QFont stands for.
    static double pixelSizeOf(const QFont &font);
    // The font families that fontconfig knows and the application fonts add, sorted; empty without fontconfig.
    static QStringList families();
    // Registers the TrueType or OpenType font in `data` as an application font, whose families create() then finds
    // ahead of fontconfig's. Returns its id, or -1 if the data is not a font or there is no FreeType.
    static int addApplicationFont(const QByteArray &data);
    static QStringList applicationFontFamilies(int id);
    static bool removeApplicationFont(int id);
    static bool removeAllApplicationFonts();
    // Whether every glyph of the font has the same advance.
    bool isFixedPitch() const { return m_fixedPitch; }
    // Appends the outline of `codePoint`, with its pen position at `origin` (y down), to `path`. Returns the advance
    // in 26.6 pixels. Without FreeType nothing is appended.
    int addOutline(char32_t codePoint, double originX, double originY, class QPainterPath *path);
    // The advance of `glyph`, the glyph of `codePoint`, with the letter and word spacing of the font, in 26.6 pixels.
    int spacedAdvance64(char32_t codePoint, int glyphAdvance64) const;

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
    bool m_fixedPitch = false;
    double m_letterPercent = 100.0; // QFont::PercentageSpacing
    int m_letterExtra64 = 0;        // QFont::AbsoluteSpacing, 26.6 pixels
    int m_wordExtra64 = 0;          // added to spaces, 26.6 pixels
    std::unordered_map<char32_t, QGlyph> m_glyphs;

    // The engine of another font that has `codePoint`, if this one's font does not and one of the system's does;
    // nullptr to use this one.
    QFontEngine *fallbackFor(char32_t codePoint);
    int m_weight = 400; // of the font asked for, which a fallback should come close to,
    bool m_italic = false;
    int m_styleHint = 0;
    bool m_isFallback = false; // and one that is a fallback has none of its own
    std::mutex m_fallbackMutex;
    std::unordered_map<char32_t, QFontEngine *> m_fallbackByCodePoint;
    std::vector<std::unique_ptr<QFontEngine>> m_fallbacks;
};
