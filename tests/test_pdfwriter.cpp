// MIT License - see LICENSE
//
// Tests for QPageSize, QPageLayout, QPagedPaintDevice and QPdfWriter. The written PDF is checked for its structure
// (header, cross-reference table, pages, metadata) and, where poppler's pdftoppm is installed, rendered and
// compared pixel by pixel with what was painted.
#include <QtCore/qbuffer.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qprocess.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qtemporarydir.h>
#include <QtGui/QtGui>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef NT_HAVE_ZLIB
#include <zlib.h>
#endif

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static bool near(double a, double b, double tolerance = 0.01) { return std::fabs(a - b) <= tolerance; }

static void testPageSize() {
    CHECK(!QPageSize().isValid());
    const QPageSize a4(QPageSize::A4);
    CHECK(a4.isValid() && a4.id() == QPageSize::A4);
    CHECK(a4.name() == QLatin1String("A4") && a4.key() == QLatin1String("A4"));
    CHECK(a4.definitionUnits() == QPageSize::Millimeter && a4.definitionSize() == QSizeF(210, 297));
    CHECK(a4.sizePoints() == QSize(595, 842));
    CHECK(a4.sizePixels(300) == QSize(2480, 3508));
    CHECK(near(a4.size(QPageSize::Inch).width(), 8.27) && near(a4.size(QPageSize::Inch).height(), 11.69));
    CHECK(a4.rectPoints() == QRect(0, 0, 595, 842));
    CHECK(QPageSize(QPageSize::Letter).sizePoints() == QSize(612, 792));
    CHECK(QPageSize::sizePoints(QPageSize::A3) == QSize(842, 1191));
    CHECK(QPageSize::definitionUnits(QPageSize::Legal) == QPageSize::Inch);
    CHECK(QPageSize::name(QPageSize::Ledger).contains(QLatin1String("Ledger")));
    CHECK(QPageSize::AnsiA == QPageSize::Letter && QPageSize::EnvelopeDL == QPageSize::DLE);
    CHECK(!QPageSize(QPageSize::Custom).isValid());

    // Matching standard sizes.
    CHECK(QPageSize::id(QSize(595, 842)) == QPageSize::A4);
    CHECK(QPageSize::id(QSize(597, 840)) == QPageSize::A4);
    CHECK(QPageSize::id(QSize(597, 840), QPageSize::ExactMatch) == QPageSize::Custom);
    CHECK(QPageSize::id(QSize(842, 595)) == QPageSize::Custom);
    CHECK(QPageSize::id(QSize(842, 595), QPageSize::FuzzyOrientationMatch) == QPageSize::A4);
    CHECK(QPageSize::id(QSizeF(210, 297), QPageSize::Millimeter) == QPageSize::A4);
    CHECK(QPageSize(QSizeF(210, 297), QPageSize::Millimeter).id() == QPageSize::A4);
    const QPageSize custom(QSizeF(100, 50), QPageSize::Millimeter, QStringLiteral("Label"));
    CHECK(custom.isValid() && custom.id() == QPageSize::Custom && custom.name() == QLatin1String("Label"));
    CHECK(custom.sizePoints() == QSize(283, 142));
    CHECK(custom.key().startsWith(QLatin1String("Custom.")));
    CHECK(custom != a4 && a4 == QPageSize(QPageSize::A4));
    CHECK(QPageSize(QSize(595, 842)).isEquivalentTo(a4));
}

static void testPageLayout() {
    CHECK(!QPageLayout().isValid());
    QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Landscape, QMarginsF(15, 10, 15, 20),
                       QPageLayout::Millimeter);
    CHECK(layout.isValid() && layout.orientation() == QPageLayout::Landscape);
    CHECK(layout.units() == QPageLayout::Millimeter && layout.mode() == QPageLayout::StandardMode);
    CHECK(layout.fullRect() == QRectF(0, 0, 297, 210));
    CHECK(layout.paintRect() == QRectF(15, 10, 267, 180));
    CHECK(layout.fullRectPoints() == QRect(0, 0, 842, 595));
    CHECK(layout.fullRectPixels(300) == QRect(0, 0, 3508, 2480));
    const QRect paint = layout.paintRectPixels(300);
    CHECK(std::abs(paint.x() - 177) <= 1 && std::abs(paint.y() - 118) <= 1);
    CHECK(std::abs(paint.width() - 3154) <= 1 && std::abs(paint.height() - 2126) <= 1);
    CHECK(near(layout.margins(QPageLayout::Point).left(), 42.52));
    CHECK(layout.marginsPoints() == QMargins(43, 28, 43, 57));

    // Margins out of range are refused, or clamped.
    CHECK(!layout.setMargins(QMarginsF(-1, 0, 0, 0)));
    CHECK(layout.margins() == QMarginsF(15, 10, 15, 20));
    CHECK(layout.setMargins(QMarginsF(-1, 0, 0, 0), QPageLayout::OutOfBoundsPolicy::Clamp));
    CHECK(layout.margins() == QMarginsF(0, 0, 0, 0));
    CHECK(layout.setLeftMargin(30) && layout.margins().left() == 30);
    CHECK(!layout.setTopMargin(500));
    layout.setMinimumMargins(QMarginsF(5, 5, 5, 5));
    CHECK(layout.margins() == QMarginsF(30, 5, 5, 5));
    CHECK(layout.maximumMargins() == QMarginsF(292, 205, 292, 205));

    // Units convert the margins.
    layout.setUnits(QPageLayout::Point);
    CHECK(near(layout.margins().left(), 85.04) && near(layout.fullRect().width(), 841.89));
    layout.setMode(QPageLayout::FullPageMode);
    CHECK(layout.paintRect() == layout.fullRect());
    layout.setOrientation(QPageLayout::Portrait);
    CHECK(layout.fullRectPoints() == QRect(0, 0, 595, 842));
    const QPageLayout same = layout;
    CHECK(same == layout && !(same != layout) && same.isEquivalentTo(layout));
    layout.setPageSize(QPageSize(QPageSize::A5));
    CHECK(layout.pageSize().id() == QPageSize::A5);
}

#ifdef NT_HAVE_ZLIB
static std::string inflate(const std::string &data) {
    std::string out(data.size() * 20 + 4096, '\0');
    uLongf size = static_cast<uLongf>(out.size());
    while (uncompress(reinterpret_cast<Bytef *>(out.data()), &size, reinterpret_cast<const Bytef *>(data.data()),
                      static_cast<uLong>(data.size())) == Z_BUF_ERROR) {
        out.resize(out.size() * 4);
        size = static_cast<uLongf>(out.size());
    }
    out.resize(size);
    return out;
}
#endif

// The contents of every stream in `pdf`, inflated where compressed.
static std::string allStreams(const std::string &pdf) {
    std::string out;
    std::size_t pos = 0;
    while ((pos = pdf.find("stream\n", pos)) != std::string::npos) {
        const std::size_t dict = pdf.rfind("<<", pos);
        const std::size_t start = pos + 7;
        const std::size_t end = pdf.find("\nendstream", start);
        if (end == std::string::npos)
            break;
        const std::string data = pdf.substr(start, end - start);
        const bool flate = pdf.substr(dict, pos - dict).find("/FlateDecode") != std::string::npos;
#ifdef NT_HAVE_ZLIB
        out += flate ? inflate(data) : data;
#else
        out += flate ? std::string() : data;
#endif
        out += "\n";
        pos = end + 10; // past "endstream", whose end would match again
    }
    return out;
}

// Checks the cross-reference table: every offset points at its object.
static bool xrefValid(const std::string &pdf) {
    const std::size_t startxref = pdf.rfind("startxref\n");
    if (startxref == std::string::npos)
        return false;
    const std::size_t xref = std::stoul(pdf.substr(startxref + 10));
    if (pdf.compare(xref, 5, "xref\n") != 0)
        return false;
    std::size_t line = pdf.find('\n', xref + 5) + 1;
    const int count = std::stoi(pdf.substr(pdf.find(' ', xref + 5) + 1));
    line += 20; // object 0
    for (int i = 1; i < count; ++i, line += 20) {
        const std::size_t offset = std::stoul(pdf.substr(line, 10));
        const std::string expected = std::to_string(i) + " 0 obj";
        if (pdf.compare(offset, expected.size(), expected) != 0)
            return false;
    }
    return true;
}

static int occurrences(const std::string &text, const std::string &what) {
    int n = 0;
    for (std::size_t pos = text.find(what); pos != std::string::npos; pos = text.find(what, pos + 1))
        ++n;
    return n;
}

// Renders page `page` of `file` at `dpi` with pdftoppm, if it is installed.
static QImage render(const QString &file, int page, int dpi) {
    const QString tool = QStandardPaths::findExecutable(QStringLiteral("pdftoppm"));
    if (tool.isEmpty())
        return QImage();
    const QString prefix = file + QStringLiteral("-render");
    QProcess process;
    process.start(tool, {QStringLiteral("-png"), QStringLiteral("-r"), QString::number(dpi), QStringLiteral("-f"),
                         QString::number(page), QStringLiteral("-l"), QString::number(page),
                         QStringLiteral("-singlefile"), file, prefix});
    if (!process.waitForFinished(30000) || process.exitCode() != 0)
        return QImage();
    return QImage(prefix + QStringLiteral(".png"));
}

static bool color(QRgb p, int r, int g, int b, int tolerance = 12) {
    const bool ok = std::abs(qRed(p) - r) <= tolerance && std::abs(qGreen(p) - g) <= tolerance &&
                    std::abs(qBlue(p) - b) <= tolerance;
    if (!ok)
        std::printf("  pixel is (%d, %d, %d), expected (%d, %d, %d)\n", qRed(p), qGreen(p), qBlue(p), r, g, b);
    return ok;
}

static void testWriter(const QString &dir) {
    const QString file = dir + QStringLiteral("/test.pdf");
    QPdfWriter writer(file);
    CHECK(writer.resolution() == 1200);
    CHECK(writer.pdfVersion() == QPagedPaintDevice::PdfVersion_1_4);
    CHECK(writer.pageLayout().pageSize().id() == QPageSize::A4);
    CHECK(!writer.newPage()); // not painting yet
    // The layout bohemian's PDF export uses: A4 landscape, margins in millimeters.
    CHECK(writer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Landscape, QMarginsF(15, 15, 15, 25),
                                           QPageLayout::Millimeter)));
    writer.setResolution(300);
    writer.setTitle(QStringLiteral("Domain Model – Test")); // not ASCII: written as UTF-16
    writer.setCreator(QStringLiteral("nt_pdfwriter_tests"));
    CHECK(writer.title().endsWith(QLatin1String("Test")) && writer.creator() == QLatin1String("nt_pdfwriter_tests"));
    CHECK(writer.logicalDpiX() == 300 && writer.physicalDpiY() == 300);
    const QRect paint = writer.pageLayout().paintRectPixels(300);
    CHECK(writer.width() == paint.width() && writer.height() == paint.height());
    CHECK(writer.widthMM() == 267 && writer.heightMM() == 170);

    QImage image(4, 4, QImage::Format_ARGB32);
    image.fill(qRgba(0, 0, 255, 255));
    image.setPixel(0, 0, qRgba(0, 0, 0, 0));
    {
        QPainter painter;
        CHECK(painter.begin(&writer));
        // Page 1: a red square at the top left, a green one at the bottom right, an outlined ellipse, an image,
        // a gradient and text.
        painter.fillRect(QRect(0, 0, 600, 600), QColor(255, 0, 0));
        painter.fillRect(QRect(writer.width() - 600, writer.height() - 600, 600, 600), QColor(0, 160, 0));
        painter.setPen(QPen(Qt::black, 10));
        painter.setBrush(QColor(255, 255, 0));
        painter.drawEllipse(QRectF(1000, 200, 800, 400));
        painter.drawImage(QRect(2000, 200, 400, 400), image);
        QLinearGradient gradient(QPointF(0, 1000), QPointF(1000, 1000));
        gradient.setColorAt(0, Qt::white);
        gradient.setColorAt(0.5, QColor(128, 128, 128));
        gradient.setColorAt(1, Qt::black);
        painter.fillRect(QRectF(0, 900, 1000, 200), gradient);
        painter.setPen(QPen(Qt::blue, 4, Qt::DashLine));
        painter.drawLine(QPointF(0, 1300), QPointF(1500, 1300));
        painter.setOpacity(0.5);
        painter.fillRect(QRect(1600, 900, 300, 300), Qt::black);
        painter.setOpacity(1.0);
        painter.setPen(Qt::black);
        painter.setFont(QFont(QStringLiteral("Helvetica"), 24));
        // A point is a point: 24 pt at 300 dpi is 100 pixels, so the line spacing is about that.
        const QFontMetrics metrics = painter.fontMetrics();
        CHECK(metrics.height() > 80 && metrics.height() < 140);
        CHECK(QFontMetrics(painter.font(), &writer).height() == metrics.height());
        CHECK(QFontMetrics(painter.font()).height() < metrics.height() / 2);
        painter.drawText(QRect(0, 1500, 2000, 200), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("Hello PDF"));

        // Page 2, in portrait, clipped.
        CHECK(writer.setPageOrientation(QPageLayout::Portrait));
        CHECK(writer.newPage());
        CHECK(writer.width() < writer.height());
        painter.setClipRect(QRect(0, 0, 300, 300));
        painter.fillRect(QRect(0, 0, 1000, 1000), QColor(0, 0, 200));
        painter.setClipping(false);
        QPainterPath triangle;
        triangle.moveTo(500, 500);
        triangle.lineTo(900, 500);
        triangle.lineTo(700, 900);
        triangle.closeSubpath();
        painter.save();
        painter.rotate(5);
        painter.fillPath(triangle, QColor(200, 0, 200));
        painter.restore();
        painter.end();
    }

    QFile in(file);
    CHECK(in.open(QIODevice::ReadOnly));
    const QByteArray bytes = in.readAll();
    const std::string pdf(bytes.constData(), static_cast<std::size_t>(bytes.size()));
    CHECK(pdf.rfind("%PDF-1.4\n", 0) == 0);
    CHECK(pdf.size() > 1000 && pdf.substr(pdf.size() - 6) == "%%EOF\n");
    CHECK(xrefValid(pdf));
    CHECK(occurrences(pdf, "/Type /Page ") == 2);
    CHECK(pdf.find("/Count 2") != std::string::npos);
    CHECK(pdf.find("/MediaBox [0 0 841.89 595.28]") != std::string::npos); // landscape A4
    CHECK(pdf.find("/MediaBox [0 0 595.28 841.89]") != std::string::npos); // portrait A4
    CHECK(pdf.find("/Creator (nt_pdfwriter_tests)") != std::string::npos);
    CHECK(pdf.find("/Title <FEFF") != std::string::npos);
    CHECK(pdf.find("/ShadingType 2") != std::string::npos);
    CHECK(pdf.find("/SMask") != std::string::npos); // the image has a transparent pixel
    CHECK(pdf.find("/ca 0.5") != std::string::npos);
    CHECK(pdf.find("/Subtype /Form") != std::string::npos); // glyph outlines
#ifdef NT_HAVE_ZLIB
    CHECK(pdf.find("/FlateDecode") != std::string::npos);
    const std::string streams = allStreams(pdf);
    CHECK(streams.find("1 0 0 rg") != std::string::npos); // red
    CHECK(streams.find(" re\n") != std::string::npos);
    CHECK(streams.find("W n") != std::string::npos);   // the clip
    CHECK(streams.find("] 0 d") != std::string::npos); // the dashed line
#endif

    // Rendered by a PDF reader: the squares are in their corners of the paint rectangle, which starts at the
    // margins (15 mm left, 15 mm top).
    const QImage page1 = render(file, 1, 72);
    if (page1.isNull()) {
        std::puts("SKIP: pdftoppm is not installed; the PDF was not rendered");
        return;
    }
    CHECK(std::abs(page1.width() - 842) <= 1 && std::abs(page1.height() - 595) <= 1);
    const int left = qRound(15 * 72 / 25.4), top = left; // 43 points
    const int square = 600 * 72 / 300;                   // 144 points
    CHECK(color(page1.pixel(left + 5, top + 5), 255, 0, 0));
    CHECK(color(page1.pixel(left + square / 2, top + square / 2), 255, 0, 0));
    CHECK(color(page1.pixel(left - 5, top - 5), 255, 255, 255)); // the margin
    const QRect paintPoints(left, top, 842 - 2 * left, 595 - top - qRound(25 * 72 / 25.4));
    CHECK(color(page1.pixel(paintPoints.right() - 10, paintPoints.bottom() - 10), 0, 160, 0));
    CHECK(color(page1.pixel(left + 1400 * 72 / 300, top + 400 * 72 / 300), 255, 255, 0));        // inside the ellipse
    CHECK(color(page1.pixel(left + 2200 * 72 / 300, top + 400 * 72 / 300), 0, 0, 255));          // the image
    CHECK(color(page1.pixel(left + 50 * 72 / 300, top + 1000 * 72 / 300), 245, 245, 245, 20));   // gradient start
    CHECK(color(page1.pixel(left + 950 * 72 / 300, top + 1000 * 72 / 300), 10, 10, 10, 20));     // gradient end
    CHECK(color(page1.pixel(left + 1750 * 72 / 300, top + 1050 * 72 / 300), 128, 128, 128, 20)); // 50% black
    // The text left some dark pixels where it was drawn.
    int dark = 0;
    for (int y = top + 1500 * 72 / 300; y < top + 1700 * 72 / 300; ++y)
        for (int x = left; x < left + 600 * 72 / 300; ++x)
            dark += qGray(page1.pixel(x, y)) < 100 ? 1 : 0;
    CHECK(dark > 50);

    const QImage page2 = render(file, 2, 72);
    CHECK(std::abs(page2.width() - 595) <= 1 && std::abs(page2.height() - 842) <= 1);
    if (!page2.isNull()) {
        CHECK(color(page2.pixel(left + 30, top + 30), 0, 0, 200));                                // inside the clip
        CHECK(color(page2.pixel(left + 100, top + 100), 255, 255, 255));                          // outside the clip
        CHECK(color(page2.pixel(left + 700 * 72 / 300 - 15, top + 620 * 72 / 300), 200, 0, 200)); // the triangle
    }
}

static void testDevice() {
    QBuffer buffer;
    QPdfWriter writer(&buffer);
    writer.setResolution(72);
    writer.setPageSize(QPageSize(QPageSize::Letter));
    CHECK(writer.pageLayout().pageSize().id() == QPageSize::Letter);
    CHECK(writer.setPageMargins(QMarginsF(1, 1, 1, 1), QPageLayout::Inch));
    CHECK(writer.width() == 612 - 144 && writer.height() == 792 - 144);
    CHECK(!writer.setPageMargins(QMarginsF(20, 0, 0, 0), QPageLayout::Inch));
    writer.setPdfVersion(QPagedPaintDevice::PdfVersion_1_6);
    {
        QPainter painter(&writer);
        painter.drawRect(10, 10, 100, 100);
    }
    CHECK(buffer.data().startsWith("%PDF-1.6"));
    CHECK(buffer.data().contains("/Count 1"));
}

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    testPageSize();
    testPageLayout();
    // NT_PDF_TEST_DIR keeps the PDF for a look at it.
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString keep = qEnvironmentVariable("NT_PDF_TEST_DIR");
    testWriter(keep.isEmpty() ? dir.path() : keep);
    testDevice();
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
