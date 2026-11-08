// MIT License - see LICENSE
#include "qfontengine_p.h"

#ifdef NT_HAVE_FREETYPE
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

namespace {

// Engines are never freed: they hold FreeType faces, and the cache outlives static destruction.
struct EngineCache {
    std::mutex mutex;
    std::map<std::string, std::shared_ptr<QFontEngine>> engines;
};

EngineCache &cache() {
    static EngineCache *instance = new EngineCache;
    return *instance;
}

#ifdef NT_HAVE_FREETYPE

// FreeType and fontconfig are not thread-safe; one lock covers every call into them.
std::mutex &libraryMutex() {
    static std::mutex *m = new std::mutex;
    return *m;
}

FT_Library library() {
    static FT_Library lib = [] {
        FT_Library l = nullptr;
        if (FT_Init_FreeType(&l) != 0)
            return static_cast<FT_Library>(nullptr);
        return l;
    }();
    return lib;
}

FT_Face openFace(const QFont &font, double pixelSize) {
    std::lock_guard<std::mutex> lock(libraryMutex());
    FT_Library lib = library();
    if (!lib || !FcInit())
        return nullptr;
    QString family = font.family();
    if (family.isEmpty() || family.toLower() == QString::fromLatin1("sans serif"))
        family = QString::fromLatin1("sans-serif");
    FcPattern *pattern = FcPatternCreate();
    const QByteArray familyUtf8 = family.toUtf8();
    FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8 *>(familyUtf8.constData()));
    FcPatternAddInteger(pattern, FC_WEIGHT, FcWeightFromOpenType(static_cast<int>(font.weight())));
    FcPatternAddInteger(pattern, FC_SLANT, font.italic() ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
    FcPatternAddDouble(pattern, FC_PIXEL_SIZE, pixelSize);
    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result;
    FcPattern *match = FcFontMatch(nullptr, pattern, &result);
    FT_Face face = nullptr;
    if (match) {
        FcChar8 *file = nullptr;
        int index = 0;
        if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch) {
            FcPatternGetInteger(match, FC_INDEX, 0, &index);
            if (FT_New_Face(lib, reinterpret_cast<const char *>(file), index, &face) != 0)
                face = nullptr;
        }
        FcPatternDestroy(match);
    }
    FcPatternDestroy(pattern);
    if (face && FT_Set_Char_Size(face, 0, static_cast<FT_F26Dot6>(std::lround(pixelSize * 64.0)), 72, 72) != 0) {
        FT_Done_Face(face);
        face = nullptr;
    }
    return face;
}

#endif // NT_HAVE_FREETYPE

} // namespace

double QFontEngine::pixelSizeOf(const QFont &font) {
    if (font.pixelSize() > 0)
        return font.pixelSize();
    const double points = font.pointSizeF() > 0 ? font.pointSizeF() : 10.0;
    return points * 96.0 / 72.0;
}

std::shared_ptr<QFontEngine> QFontEngine::create(const QFont &font, double scale) {
    const double pixels = std::max(1.0, pixelSizeOf(font) * scale);
    const std::string key = font.family().toUtf8().toStdString() + "|" +
                            std::to_string(static_cast<int>(font.weight())) + "|" + (font.italic() ? "i" : "r") + "|" +
                            std::to_string(std::lround(pixels * 64.0));
    EngineCache &c = cache();
    {
        std::lock_guard<std::mutex> lock(c.mutex);
        const auto it = c.engines.find(key);
        if (it != c.engines.end())
            return it->second;
    }
    void *face = nullptr;
#ifdef NT_HAVE_FREETYPE
    face = openFace(font, pixels);
#endif
    std::shared_ptr<QFontEngine> engine(new QFontEngine(face, pixels));
    std::lock_guard<std::mutex> lock(c.mutex);
    return c.engines.emplace(key, engine).first->second;
}

QFontEngine::QFontEngine(void *face, double pixelSize) : m_face(face), m_pixelSize(pixelSize) { measure(); }

QFontEngine::~QFontEngine() {
#ifdef NT_HAVE_FREETYPE
    if (m_face) {
        std::lock_guard<std::mutex> lock(libraryMutex());
        FT_Done_Face(static_cast<FT_Face>(m_face));
    }
#endif
}

void QFontEngine::measure() {
#ifdef NT_HAVE_FREETYPE
    if (m_face) {
        std::lock_guard<std::mutex> lock(libraryMutex());
        auto face = static_cast<FT_Face>(m_face);
        const FT_Size_Metrics &sm = face->size->metrics;
        // Round the exact scaled metrics to the nearest pixel, which is what Qt does, rather than up as the
        // FreeType size metrics do.
        if (FT_IS_SCALABLE(face)) {
            m_ascent = static_cast<int>((FT_MulFix(face->ascender, sm.y_scale) + 32) >> 6);
            m_descent = static_cast<int>((FT_MulFix(-face->descender, sm.y_scale) + 32) >> 6);
        } else {
            m_ascent = static_cast<int>((sm.ascender + 63) >> 6);
            m_descent = static_cast<int>((-sm.descender + 63) >> 6);
        }
        const int line = static_cast<int>((sm.height + 32) >> 6);
        m_leading = std::max(0, line - m_ascent - m_descent);
        const FT_UInt x = FT_Get_Char_Index(face, 'x');
        m_xHeight = m_ascent / 2;
        if (x && FT_Load_Glyph(face, x, FT_LOAD_NO_BITMAP) == 0)
            m_xHeight = static_cast<int>(face->glyph->metrics.height >> 6);
        const FT_UInt n = FT_Get_Char_Index(face, '0');
        m_averageCharWidth = static_cast<int>(m_pixelSize / 2);
        if (n && FT_Load_Glyph(face, n, FT_LOAD_NO_BITMAP) == 0)
            m_averageCharWidth = static_cast<int>((face->glyph->advance.x + 32) >> 6);
        return;
    }
#endif
    m_ascent = static_cast<int>(std::lround(m_pixelSize * 0.8));
    m_descent = static_cast<int>(std::lround(m_pixelSize * 0.2));
    m_leading = 0;
    m_xHeight = static_cast<int>(std::lround(m_pixelSize * 0.5));
    m_averageCharWidth = static_cast<int>(std::lround(m_pixelSize * 0.5));
}

bool QFontEngine::hasGlyph(char32_t codePoint) {
#ifdef NT_HAVE_FREETYPE
    if (m_face) {
        std::lock_guard<std::mutex> lock(libraryMutex());
        return FT_Get_Char_Index(static_cast<FT_Face>(m_face), codePoint) != 0;
    }
#endif
    (void)codePoint;
    return false;
}

const QGlyph &QFontEngine::glyph(char32_t codePoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_glyphs.find(codePoint);
    if (it != m_glyphs.end())
        return it->second;
    QGlyph g;
    g.advance64 = static_cast<int>(std::lround(m_pixelSize * 0.5 * 64.0));
#ifdef NT_HAVE_FREETYPE
    if (m_face) {
        std::lock_guard<std::mutex> libLock(libraryMutex());
        auto face = static_cast<FT_Face>(m_face);
        const FT_UInt index = FT_Get_Char_Index(face, codePoint);
        if (FT_Load_Glyph(face, index, FT_LOAD_TARGET_LIGHT | FT_LOAD_NO_BITMAP) == 0 &&
            FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) == 0) {
            FT_GlyphSlot slot = face->glyph;
            // The unhinted advance (16.16 pixels, here in 26.6), so that text is as wide at every size and
            // scale as the font's design says, rather than rounded to whole pixels by the hinter.
            g.advance64 = static_cast<int>((slot->linearHoriAdvance + 512) >> 10);
            g.left = slot->bitmap_left;
            g.top = slot->bitmap_top;
            g.width = static_cast<int>(slot->bitmap.width);
            g.height = static_cast<int>(slot->bitmap.rows);
            g.stride = g.width;
            if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_GRAY && g.width > 0 && g.height > 0) {
                g.bitmap.resize(static_cast<std::size_t>(g.stride) * static_cast<std::size_t>(g.height));
                const int pitch = slot->bitmap.pitch;
                for (int row = 0; row < g.height; ++row)
                    std::copy_n(slot->bitmap.buffer + static_cast<std::ptrdiff_t>(row) * pitch, g.width,
                                g.bitmap.begin() + static_cast<std::ptrdiff_t>(row) * g.stride);
            } else {
                g.width = g.height = 0;
            }
        }
    }
#endif
    return m_glyphs.emplace(codePoint, std::move(g)).first->second;
}
