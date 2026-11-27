// MIT License - see LICENSE
#include "qfontengine_p.h"

#ifdef NT_HAVE_FREETYPE
#ifndef __ANDROID__
#define NT_HAVE_FONTCONFIG 1
#include <fontconfig/fontconfig.h>
#endif
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#endif

#include <QtGui/qpainterpath.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>
#include <string>
#include <tuple>
#include <vector>
#ifndef NT_HAVE_FONTCONFIG
#include <dirent.h>
#endif

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

// Fonts the application added from files or memory, looked up by family ahead of fontconfig.
struct AppFont {
    int id;
    QByteArray data; // the face reads it; kept for as long as the font is registered
    QStringList families;
    int weight;
    bool italic;
};

struct AppFonts {
    std::mutex mutex;
    std::vector<AppFont> fonts;
    int nextId = 0;
};

AppFonts &appFonts() {
    static AppFonts *instance = new AppFonts;
    return *instance;
}

// Changes whenever the application fonts change, so that engines made before are not reused for a family that now
// resolves to another face.
std::atomic<int> g_appFontGeneration{0};

#ifdef NT_HAVE_FREETYPE

// The generic fontconfig family a style hint stands for, or an empty string.
const char *genericFamily(QFont::StyleHint hint) {
    switch (hint) {
    case QFont::Times:
        return "serif";
    case QFont::Courier:
    case QFont::Monospace:
        return "monospace";
    case QFont::Helvetica:
    case QFont::System:
        return "sans-serif";
    case QFont::Cursive:
        return "cursive";
    case QFont::OldEnglish:
    case QFont::Fantasy:
        return "fantasy";
    case QFont::AnyStyle:
        break;
    }
    return "";
}

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

// The application font for one of `families` that comes closest to the weight and slant of `font`, or null. The
// app-font mutex must be held.
const AppFont *findAppFont(const QStringList &families, const QFont &font) {
    for (const QString &family : families) {
        const QString wanted = family.toLower();
        const AppFont *best = nullptr;
        int bestScore = 0;
        for (const AppFont &f : appFonts().fonts) {
            bool match = false;
            for (const QString &name : f.families)
                match = match || name.toLower() == wanted;
            if (!match)
                continue;
            const int score =
                std::abs(f.weight - static_cast<int>(font.weight())) + (f.italic != font.italic() ? 1000 : 0);
            if (!best || score < bestScore) {
                best = &f;
                bestScore = score;
            }
        }
        if (best)
            return best;
    }
    return nullptr;
}

#ifndef NT_HAVE_FONTCONFIG
// Without fontconfig (Android), the fonts of the system are the files in its font directories.
struct SystemFont {
    QString family;
    std::string path;
    int index;
    int weight;
    bool italic;
};

int weightFromFileName(const std::string &name) {
    static const struct {
        const char *token;
        int weight;
    } tokens[] = {{"Thin", 100},     {"ExtraLight", 200}, {"Light", 300}, {"Medium", 500},
                  {"SemiBold", 600}, {"ExtraBold", 800},  {"Bold", 700},  {"Black", 900}};
    for (const auto &t : tokens)
        if (name.find(t.token) != std::string::npos)
            return t.weight;
    return 400;
}

const std::vector<SystemFont> &systemFonts() {
    static const std::vector<SystemFont> fonts = [] {
        std::vector<SystemFont> list;
        FT_Library lib = nullptr;
        if (FT_Init_FreeType(&lib) != 0)
            return list;
        for (const char *dir : {"/system/fonts", "/system/product/fonts", "/product/fonts", "/data/fonts/files"}) {
            DIR *d = ::opendir(dir);
            if (!d)
                continue;
            while (const dirent *entry = ::readdir(d)) {
                const std::string file = entry->d_name;
                const auto dot = file.rfind('.');
                if (dot == std::string::npos)
                    continue;
                const std::string ext = file.substr(dot);
                if (ext != ".ttf" && ext != ".otf" && ext != ".ttc")
                    continue;
                const std::string path = std::string(dir) + "/" + file;
                FT_Face face = nullptr;
                if (FT_New_Face(lib, path.c_str(), 0, &face) != 0 || !face)
                    continue;
                const long faces = face->num_faces;
                for (long i = 0; i < faces; ++i) {
                    FT_Face f = face;
                    if (i > 0 && FT_New_Face(lib, path.c_str(), i, &f) != 0)
                        continue;
                    if (f->family_name)
                        list.push_back({QString::fromUtf8(f->family_name), path, static_cast<int>(i),
                                        weightFromFileName(file), (f->style_flags & FT_STYLE_FLAG_ITALIC) != 0});
                    if (f != face)
                        FT_Done_Face(f);
                }
                FT_Done_Face(face);
            }
            ::closedir(d);
        }
        FT_Done_FreeType(lib);
        return list;
    }();
    return fonts;
}

// The system font that comes closest to one of `families`, or to the generic family of the font's style hint.
FT_Face openSystemFace(FT_Library lib, const QStringList &families, const QFont &font) {
    QStringList wanted;
    for (const QString &f : families) {
        const QString lower = f.toLower();
        if (lower == QString::fromLatin1("sans serif") || lower == QString::fromLatin1("sans-serif"))
            wanted << QString::fromLatin1("roboto");
        else if (lower == QString::fromLatin1("serif"))
            wanted << QString::fromLatin1("noto serif");
        else if (lower == QString::fromLatin1("monospace"))
            wanted << QString::fromLatin1("droid sans mono") << QString::fromLatin1("cutive mono");
        else
            wanted << lower;
    }
    const std::string generic = genericFamily(font.styleHint());
    wanted << QString::fromLatin1(generic == "serif"       ? "noto serif"
                                  : generic == "monospace" ? "droid sans mono"
                                                           : "roboto");
    wanted << QString::fromLatin1("roboto") << QString::fromLatin1("noto sans");
    for (const QString &name : wanted) {
        const SystemFont *best = nullptr;
        int bestScore = 0;
        for (const SystemFont &f : systemFonts()) {
            if (f.family.toLower() != name)
                continue;
            const int score =
                std::abs(f.weight - static_cast<int>(font.weight())) + (f.italic != font.italic() ? 1000 : 0);
            if (!best || score < bestScore) {
                best = &f;
                bestScore = score;
            }
        }
        FT_Face face = nullptr;
        if (best && FT_New_Face(lib, best->path.c_str(), best->index, &face) == 0)
            return face;
    }
    return nullptr;
}
#endif

FT_Face openFace(const QFont &font, double pixelSize) {
    std::lock_guard<std::mutex> lock(libraryMutex());
    FT_Library lib = library();
    if (!lib)
        return nullptr;
    QStringList families = font.families();
    FT_Face face = nullptr;
    {
        std::lock_guard<std::mutex> appLock(appFonts().mutex);
        if (const AppFont *app = findAppFont(families, font)) {
            if (FT_New_Memory_Face(lib, reinterpret_cast<const FT_Byte *>(app->data.constData()),
                                   static_cast<FT_Long>(app->data.size()), 0, &face) != 0)
                face = nullptr;
        }
    }
#ifdef NT_HAVE_FONTCONFIG
    if (!face && FcInit()) {
        FcPattern *pattern = FcPatternCreate();
        bool any = false;
        for (const QString &family : families) {
            if (family.isEmpty())
                continue;
            const QString name =
                family.toLower() == QString::fromLatin1("sans serif") ? QString::fromLatin1("sans-serif") : family;
            const QByteArray utf8 = name.toUtf8();
            FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8 *>(utf8.constData()));
            any = true;
        }
        const char *generic = genericFamily(font.styleHint());
        if (*generic != '\0') {
            FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8 *>(generic));
            any = true;
        }
        if (!any)
            FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8 *>("sans-serif"));
        FcPatternAddInteger(pattern, FC_WEIGHT, FcWeightFromOpenType(static_cast<int>(font.weight())));
        FcPatternAddInteger(pattern, FC_SLANT, font.italic() ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
        if (font.stretch() > 0)
            FcPatternAddInteger(pattern, FC_WIDTH, font.stretch());
        FcPatternAddDouble(pattern, FC_PIXEL_SIZE, pixelSize);
        FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
        FcDefaultSubstitute(pattern);
        FcResult result;
        FcPattern *match = FcFontMatch(nullptr, pattern, &result);
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
    }
#else
    if (!face)
        face = openSystemFace(lib, families, font);
#endif
    if (face && FT_Set_Char_Size(face, 0, static_cast<FT_F26Dot6>(std::lround(pixelSize * 64.0)), 72, 72) != 0) {
        FT_Done_Face(face);
        face = nullptr;
    }
    return face;
}

// Where a fallback font is: a file and the number of the face in it. An empty path: there is none.
struct FaceFile {
    std::string path;
    int index = 0;
};

#ifdef NT_HAVE_FONTCONFIG
// The best scalable font of the system that has `codePoint`. The library mutex must be held.
FaceFile searchFallback(char32_t codePoint, int weight, bool italic, QFont::StyleHint hint) {
    FaceFile found;
    if (!FcInit())
        return found;
    FcPattern *pattern = FcPatternCreate();
    const char *generic = genericFamily(hint);
    FcPatternAddString(pattern, FC_FAMILY,
                       reinterpret_cast<const FcChar8 *>(*generic != '\0' ? generic : "sans-serif"));
    FcPatternAddInteger(pattern, FC_WEIGHT, FcWeightFromOpenType(weight));
    FcPatternAddInteger(pattern, FC_SLANT, italic ? FC_SLANT_ITALIC : FC_SLANT_ROMAN);
    FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
    FcCharSet *wanted = FcCharSetCreate();
    FcCharSetAddChar(wanted, codePoint);
    FcPatternAddCharSet(pattern, FC_CHARSET, wanted);
    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result;
    if (FcFontSet *fonts = FcFontSort(nullptr, pattern, FcFalse, nullptr, &result)) {
        for (int i = 0; i < fonts->nfont && found.path.empty(); ++i) {
            FcCharSet *has = nullptr;
            FcBool scalable = FcFalse;
            FcChar8 *file = nullptr;
            // A font of bitmaps, as color emoji fonts are, cannot be drawn.
            if (FcPatternGetCharSet(fonts->fonts[i], FC_CHARSET, 0, &has) != FcResultMatch ||
                !FcCharSetHasChar(has, codePoint) ||
                FcPatternGetBool(fonts->fonts[i], FC_SCALABLE, 0, &scalable) != FcResultMatch || !scalable ||
                FcPatternGetString(fonts->fonts[i], FC_FILE, 0, &file) != FcResultMatch)
                continue;
            found.path = reinterpret_cast<const char *>(file);
            FcPatternGetInteger(fonts->fonts[i], FC_INDEX, 0, &found.index);
        }
        FcFontSetDestroy(fonts);
    }
    FcCharSetDestroy(wanted);
    FcPatternDestroy(pattern);
    return found;
}
#else
// The scalable system font that has `codePoint`: of the families that a reader expects first, then of any, and of those
// the one closest in weight and slant. The library mutex must be held.
FaceFile searchFallback(char32_t codePoint, int weight, bool italic, QFont::StyleHint) {
    const auto rank = [&](const SystemFont &f) {
        const QString family = f.family.toLower();
        const int first = family == QString::fromLatin1("roboto")                       ? 0
                          : family.startsWith(QString::fromLatin1("noto sans symbols")) ? 1
                          : family.startsWith(QString::fromLatin1("noto sans"))         ? 2
                                                                                        : 3;
        return first * 10000 + std::abs(f.weight - weight) + (f.italic != italic ? 1000 : 0);
    };
    std::vector<const SystemFont *> order;
    for (const SystemFont &f : systemFonts())
        order.push_back(&f);
    std::stable_sort(order.begin(), order.end(),
                     [&](const SystemFont *a, const SystemFont *b) { return rank(*a) < rank(*b); });
    for (const SystemFont *f : order) {
        FT_Face face = nullptr;
        if (FT_New_Face(library(), f->path.c_str(), f->index, &face) != 0)
            continue;
        const bool has = FT_IS_SCALABLE(face) && FT_Get_Char_Index(face, codePoint) != 0;
        FT_Done_Face(face);
        if (has)
            return {f->path, f->index};
    }
    return {};
}
#endif

// searchFallback(), asked once for each character and kind of font. The library mutex must be held.
FaceFile fallbackFile(char32_t codePoint, int weight, bool italic, QFont::StyleHint hint) {
    static auto *known = new std::map<std::tuple<char32_t, int, bool, int>, FaceFile>;
    const auto key = std::make_tuple(codePoint, weight, italic, static_cast<int>(hint));
    const auto it = known->find(key);
    if (it != known->end())
        return it->second;
    return known->emplace(key, searchFallback(codePoint, weight, italic, hint)).first->second;
}

// Whether a character is worth looking for in other fonts: not a control or a mark that only steers how text is laid
// out, which has nothing to show.
bool showsSomething(char32_t c) {
    return c > 0x20 && !(c >= 0x7f && c <= 0xa0) && !(c >= 0x200b && c <= 0x200f) && !(c >= 0x2028 && c <= 0x202e) &&
           !(c >= 0x2060 && c <= 0x206f) && !(c >= 0xfe00 && c <= 0xfe0f) && c != 0xfeff &&
           !(c >= 0xd800 && c <= 0xdfff) && c < 0xe0000;
}

// Collects an outline from FreeType into a QPainterPath.
struct OutlineSink {
    QPainterPath *path;
    double x;
    double y;
    bool open = false;
    QPointF point(const FT_Vector *v) const {
        return QPointF(x + static_cast<double>(v->x) / 64.0, y - static_cast<double>(v->y) / 64.0);
    }
};

int outlineMoveTo(const FT_Vector *to, void *user) {
    auto *sink = static_cast<OutlineSink *>(user);
    if (sink->open)
        sink->path->closeSubpath();
    sink->path->moveTo(sink->point(to));
    sink->open = true;
    return 0;
}

int outlineLineTo(const FT_Vector *to, void *user) {
    auto *sink = static_cast<OutlineSink *>(user);
    sink->path->lineTo(sink->point(to));
    return 0;
}

int outlineConicTo(const FT_Vector *control, const FT_Vector *to, void *user) {
    auto *sink = static_cast<OutlineSink *>(user);
    sink->path->quadTo(sink->point(control), sink->point(to));
    return 0;
}

int outlineCubicTo(const FT_Vector *c1, const FT_Vector *c2, const FT_Vector *to, void *user) {
    auto *sink = static_cast<OutlineSink *>(user);
    sink->path->cubicTo(sink->point(c1), sink->point(c2), sink->point(to));
    return 0;
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
    const std::string key = font.families().join(QLatin1Char(',')).toUtf8().toStdString() + "|" +
                            std::to_string(static_cast<int>(font.weight())) + "|" + (font.italic() ? "i" : "r") + "|" +
                            std::to_string(std::lround(pixels * 64.0)) + "|" + std::to_string(font.stretch()) + "|" +
                            std::to_string(static_cast<int>(font.styleHint())) + "|" +
                            std::to_string(static_cast<int>(font.letterSpacingType())) + ":" +
                            std::to_string(font.letterSpacing()) + ":" + std::to_string(font.wordSpacing() * scale) +
                            "|" + std::to_string(g_appFontGeneration.load());
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
    engine->m_weight = static_cast<int>(font.weight());
    engine->m_italic = font.italic();
    engine->m_styleHint = static_cast<int>(font.styleHint());
    if (font.letterSpacingType() == QFont::AbsoluteSpacing)
        engine->m_letterExtra64 = static_cast<int>(std::lround(font.letterSpacing() * scale * 64.0));
    else
        engine->m_letterPercent = font.letterSpacing();
    engine->m_wordExtra64 = static_cast<int>(std::lround(font.wordSpacing() * scale * 64.0));
    std::lock_guard<std::mutex> lock(c.mutex);
    return c.engines.emplace(key, engine).first->second;
}

QFontEngine::QFontEngine(void *face, double pixelSize) : m_face(face), m_pixelSize(pixelSize) { measure(); }

int QFontEngine::spacedAdvance64(char32_t codePoint, int glyphAdvance64) const {
    int advance = glyphAdvance64;
    if (m_letterPercent != 100.0)
        advance = static_cast<int>(std::lround(advance * m_letterPercent / 100.0));
    advance += m_letterExtra64;
    if (codePoint == ' ')
        advance += m_wordExtra64;
    return advance;
}

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
        m_fixedPitch = FT_IS_FIXED_WIDTH(face);
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

QFontEngine *QFontEngine::fallbackFor(char32_t codePoint) {
#ifdef NT_HAVE_FREETYPE
    if (!m_face || m_isFallback || !showsSomething(codePoint))
        return nullptr;
    std::lock_guard<std::mutex> lock(m_fallbackMutex);
    const auto known = m_fallbackByCodePoint.find(codePoint);
    if (known != m_fallbackByCodePoint.end())
        return known->second;
    QFontEngine *use = nullptr;
    FT_Face opened = nullptr;
    {
        std::lock_guard<std::mutex> libLock(libraryMutex());
        if (FT_Get_Char_Index(static_cast<FT_Face>(m_face), codePoint) == 0) {
            // A font that was found for another character may have this one too, as the rest of a script does.
            for (const auto &other : m_fallbacks) {
                if (FT_Get_Char_Index(static_cast<FT_Face>(other->m_face), codePoint) != 0) {
                    use = other.get();
                    break;
                }
            }
            if (!use) {
                const FaceFile file =
                    fallbackFile(codePoint, m_weight, m_italic, static_cast<QFont::StyleHint>(m_styleHint));
                if (!file.path.empty() && FT_New_Face(library(), file.path.c_str(), file.index, &opened) == 0 &&
                    (FT_Get_Char_Index(opened, codePoint) == 0 ||
                     FT_Set_Char_Size(opened, 0, static_cast<FT_F26Dot6>(std::lround(m_pixelSize * 64.0)), 72, 72) !=
                         0)) {
                    FT_Done_Face(opened);
                    opened = nullptr;
                }
            }
        }
    }
    if (opened) {
        // The constructor takes the library's lock itself.
        std::unique_ptr<QFontEngine> engine(new QFontEngine(opened, m_pixelSize));
        engine->m_isFallback = true;
        m_fallbacks.push_back(std::move(engine));
        use = m_fallbacks.back().get();
    }
    m_fallbackByCodePoint.emplace(codePoint, use);
    return use;
#else
    (void)codePoint;
    return nullptr;
#endif
}

const QGlyph &QFontEngine::glyph(char32_t codePoint) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto cached = m_glyphs.find(codePoint);
        if (cached != m_glyphs.end())
            return cached->second;
    }
    if (QFontEngine *other = fallbackFor(codePoint))
        return other->glyph(codePoint);
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

QStringList QFontEngine::families() {
    QStringList out;
#ifdef NT_HAVE_FREETYPE
    std::lock_guard<std::mutex> lock(libraryMutex());
#ifdef NT_HAVE_FONTCONFIG
    if (!FcInit())
        return out;
    FcPattern *pattern = FcPatternCreate();
    FcObjectSet *objects = FcObjectSetBuild(FC_FAMILY, static_cast<char *>(nullptr));
    FcFontSet *set = FcFontList(nullptr, pattern, objects);
    if (set) {
        for (int i = 0; i < set->nfont; ++i) {
            FcChar8 *family = nullptr;
            if (FcPatternGetString(set->fonts[i], FC_FAMILY, 0, &family) == FcResultMatch && family) {
                const QString name = QString::fromUtf8(reinterpret_cast<const char *>(family));
                if (!out.contains(name))
                    out.append(name);
            }
        }
        FcFontSetDestroy(set);
    }
    FcObjectSetDestroy(objects);
    FcPatternDestroy(pattern);
#else
    for (const SystemFont &f : systemFonts()) {
        if (!out.contains(f.family))
            out.append(f.family);
    }
#endif
    {
        std::lock_guard<std::mutex> appLock(appFonts().mutex);
        for (const AppFont &f : appFonts().fonts) {
            for (const QString &name : f.families) {
                if (!out.contains(name))
                    out.append(name);
            }
        }
    }
    std::sort(out.begin(), out.end(), [](const QString &a, const QString &b) { return a.toLower() < b.toLower(); });
#endif
    return out;
}

int QFontEngine::addApplicationFont(const QByteArray &data) {
#ifdef NT_HAVE_FREETYPE
    if (data.isEmpty())
        return -1;
    AppFont font{0, data, QStringList(), 400, false};
    {
        std::lock_guard<std::mutex> lock(libraryMutex());
        FT_Library lib = library();
        if (!lib)
            return -1;
        FT_Face face = nullptr;
        // Reads the faces of the file (a collection has several) for their family names.
        FT_Long faces = 1;
        for (FT_Long i = 0; i < faces; ++i) {
            if (FT_New_Memory_Face(lib, reinterpret_cast<const FT_Byte *>(font.data.constData()),
                                   static_cast<FT_Long>(font.data.size()), i, &face) != 0)
                break;
            faces = face->num_faces;
            if (face->family_name) {
                const QString name = QString::fromUtf8(face->family_name);
                if (!font.families.contains(name))
                    font.families.append(name);
            }
            if (i == 0) {
                font.weight = (face->style_flags & FT_STYLE_FLAG_BOLD) ? 700 : 400;
                font.italic = (face->style_flags & FT_STYLE_FLAG_ITALIC) != 0;
            }
            FT_Done_Face(face);
        }
    }
    if (font.families.isEmpty())
        return -1;
    std::lock_guard<std::mutex> lock(appFonts().mutex);
    font.id = appFonts().nextId++;
    appFonts().fonts.push_back(font);
    ++g_appFontGeneration;
    return font.id;
#else
    (void)data;
    return -1;
#endif
}

QStringList QFontEngine::applicationFontFamilies(int id) {
    std::lock_guard<std::mutex> lock(appFonts().mutex);
    for (const AppFont &f : appFonts().fonts) {
        if (f.id == id)
            return f.families;
    }
    return QStringList();
}

bool QFontEngine::removeApplicationFont(int id) {
    std::lock_guard<std::mutex> lock(appFonts().mutex);
    auto &fonts = appFonts().fonts;
    const auto it = std::find_if(fonts.begin(), fonts.end(), [id](const AppFont &f) { return f.id == id; });
    if (it == fonts.end())
        return false;
    fonts.erase(it);
    ++g_appFontGeneration;
    return true;
}

bool QFontEngine::removeAllApplicationFonts() {
    std::lock_guard<std::mutex> lock(appFonts().mutex);
    if (appFonts().fonts.empty())
        return false;
    appFonts().fonts.clear();
    ++g_appFontGeneration;
    return true;
}

int QFontEngine::addOutline(char32_t codePoint, double originX, double originY, QPainterPath *path) {
    if (QFontEngine *other = fallbackFor(codePoint))
        return other->addOutline(codePoint, originX, originY, path);
    const int advance = glyph(codePoint).advance64;
#ifdef NT_HAVE_FREETYPE
    if (m_face && path) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::lock_guard<std::mutex> libLock(libraryMutex());
        auto face = static_cast<FT_Face>(m_face);
        const FT_UInt index = FT_Get_Char_Index(face, codePoint);
        if (FT_Load_Glyph(face, index, FT_LOAD_NO_BITMAP | FT_LOAD_NO_HINTING) == 0 &&
            face->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
            OutlineSink sink{path, originX, originY};
            FT_Outline_Funcs funcs{};
            funcs.move_to = outlineMoveTo;
            funcs.line_to = outlineLineTo;
            funcs.conic_to = outlineConicTo;
            funcs.cubic_to = outlineCubicTo;
            FT_Outline_Decompose(&face->glyph->outline, &funcs, &sink);
            if (sink.open)
                path->closeSubpath();
        }
    }
#else
    (void)originX;
    (void)originY;
    (void)path;
#endif
    return advance;
}
