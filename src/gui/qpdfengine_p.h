// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtCore/qstring.h>
#include <QtGui/qpagelayout.h>
#include <QtGui/qpagedpaintdevice.h>
#include <QtGui/qpaintengine.h>
#include <QtGui/qtransform.h>

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

class QFile;
class QFontEngine;
class QIODevice;

namespace QtPrivate {

// What QPdfWriter tells its engine.
struct PdfSettings {
    QString fileName;
    QIODevice *device = nullptr;
    QString title;
    QString creator;
    int resolution = 1200;
    QPagedPaintDevice::PdfVersion version = QPagedPaintDevice::PdfVersion_1_4;
};

// One glyph of a line of text: its code point and its pen position on the baseline, in device pixels.
struct PdfGlyph {
    char32_t codePoint;
    QPointF origin;
};

// The paint engine of QPdfWriter. It keeps the document in memory and writes it when the painter ends. Device
// coordinates are pixels at the writer's resolution, from the top left of the paint rectangle of the page.
class PdfEngine : public QPaintEngine {
public:
    PdfEngine(const PdfSettings *settings, const QPagedPaintDevice *pages);
    ~PdfEngine() override;

    bool begin(QPaintDevice *device) override;
    bool end() override;
    void updateState(const QPaintEngineState &state) override;
    void drawRects(const QRect *rects, int rectCount) override;
    void drawLines(const QLine *lines, int lineCount) override;
    void drawPoints(const QPoint *points, int pointCount) override;
    void drawEllipse(const QRect &rect) override;
    void drawImage(const QRect &rect, const QImage &image, const QRect &sourceRect) override;
    void drawTransformedImage(const QRectF &rect, const QImage &image, const QRectF &sourceRect) override;
    void drawAlphaMask(const QPoint &topLeft, const uchar *mask, int width, int height, qsizetype stride) override;
    void drawPath(const QPainterPath &path) override;
    Type type() const override { return Pdf; }

    // Fills the outlines of `glyphs` of `font` with the color of the pen, so text stays sharp at any zoom.
    void drawGlyphs(QFontEngine &font, const std::vector<PdfGlyph> &glyphs);
    // Ends the page and starts the next one, with the page layout of the device then. False if not painting.
    bool newPage();

private:
    int addObject(std::string body);
    int reserveObject();
    void setObject(int id, std::string body);
    std::string streamObject(const std::string &dictionary, const std::string &data, bool compress = true) const;

    void startPage();
    void finishPage();

    // Opens a "q" block with the clip and the transparency of `fillAlpha` and `strokeAlpha`; false if the clip
    // leaves nothing to draw, and then nothing is written.
    bool open(double fillAlpha, double strokeAlpha);
    void close() { m_content += "Q\n"; }
    std::string alphaState(double fillAlpha, double strokeAlpha);
    std::string imageObject(const QImage &image);
    std::string glyphObject(QFontEngine &font, char32_t codePoint);
    std::string gradientPattern(const QBrush &brush, const QTransform &userToDevice);
    void writeStroke(const QPen &pen, double width);
    static std::string pathOperators(const QPainterPath &path);

    const PdfSettings *m_settings;
    const QPagedPaintDevice *m_pages;
    QPaintEngineState m_state;

    std::vector<std::string> m_objects; // object n is m_objects[n - 1]; empty while reserved
    std::vector<int> m_pageIds;
    int m_pagesId = 0;
    int m_resourcesId = 0;
    std::string m_content;
    QPageLayout m_layout;
    QTransform m_base; // device pixels to the default space of the page, in points with y up

    std::map<std::string, int> m_alphaStates;                         // "ca CA" -> object
    std::map<std::tuple<qint64, int, int, int, int>, int> m_images;   // cache key and source rectangle -> object
    std::map<std::pair<const QFontEngine *, char32_t>, int> m_glyphs; // 0 for glyphs without an outline
    std::vector<int> m_imageIds;
    std::vector<int> m_glyphIds;
    std::vector<int> m_patternIds;
};

} // namespace QtPrivate
