// MIT License - see LICENSE
//
// Tests for Nt Material Symbols: the icons compiled into the package, and their colors and sizes.
#include <QtGui/QtGui>
#include <QtMaterialSymbols/QtMaterialSymbols>

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

// How many pixels of an image are painted, and whether all that are fully painted have the color.
int painted(const QImage &image, const QColor &color = QColor()) {
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QColor pixel = image.pixelColor(x, y);
            if (pixel.alpha() == 0)
                continue;
            ++count;
            if (color.isValid() && pixel.alpha() == 255 &&
                (qAbs(pixel.red() - color.red()) > 2 || qAbs(pixel.green() - color.green()) > 2 ||
                 qAbs(pixel.blue() - color.blue()) > 2))
                return -1;
        }
    }
    return count;
}

void testNames() {
    const QStringList names = QMaterialSymbols::names();
    CHECK(names.size() >= 70);
    CHECK(names.contains(QStringLiteral("home")));
    CHECK(names.contains(QStringLiteral("arrow_back")));
    // Every symbol there is can be drawn, outlined and filled.
    for (const QString &name : names) {
        for (const bool filled : {false, true}) {
            const QPixmap drawn = QMaterialSymbols::pixmap(name, 24, QColor(), filled);
            if (drawn.isNull() || painted(drawn.toImage()) < 12) {
                std::printf("FAIL cannot draw %s filled=%d\n", qPrintable(name), int(filled));
                ++g_failures;
            }
        }
    }
    CHECK(QMaterialSymbols::hasSymbol(QStringLiteral("search")));
    CHECK(!QMaterialSymbols::hasSymbol(QStringLiteral("no_such_symbol")));
    CHECK(!QMaterialSymbols::hasSymbol(QStringLiteral("../home")));
    CHECK(!QMaterialSymbols::hasSymbol(QString()));
    CHECK(QMaterialSymbols::svg(QStringLiteral("home")).startsWith("<svg "));
}

void testDrawing() {
    // Black as published, or in a color, the whole symbol.
    const QImage black = QMaterialSymbols::pixmap(QStringLiteral("home"), 48).toImage();
    CHECK(black.width() == 48 && painted(black, QColor(0, 0, 0)) > 100);
    const QColor green(0, 108, 76);
    const QImage tinted = QMaterialSymbols::pixmap(QStringLiteral("home"), 48, green).toImage();
    CHECK(painted(tinted, green) == painted(black));
    // Half transparent: no pixel is more than half there.
    const QImage faint = QMaterialSymbols::pixmap(QStringLiteral("home"), 48, QColor(0, 0, 0, 128)).toImage();
    int most = 0;
    for (int y = 0; y < faint.height(); ++y)
        for (int x = 0; x < faint.width(); ++x)
            most = qMax(most, faint.pixelColor(x, y).alpha());
    CHECK(most > 100 && most <= 130);
    // The filled form of a heart covers more than its outline does.
    CHECK(painted(QMaterialSymbols::pixmap(QStringLiteral("favorite"), 48, QColor(), true).toImage()) >
          painted(QMaterialSymbols::pixmap(QStringLiteral("favorite"), 48).toImage()) + 200);
    // For a screen of two device pixels to a logical one.
    const QPixmap sharp = QMaterialSymbols::pixmap(QStringLiteral("home"), 24, green, false, 2.0);
    CHECK(sharp.width() == 48 && sharp.devicePixelRatio() == 2.0);
    CHECK(QMaterialSymbols::pixmap(QStringLiteral("home"), 0).isNull());

    const QIcon icon = QMaterialSymbols::icon(QStringLiteral("search"), green);
    CHECK(!icon.isNull());
    for (const int extent : {24, 40, 96}) {
        const QImage image = icon.pixmap(QSize(extent, extent)).toImage();
        CHECK(image.width() == extent && painted(image, green) > extent);
    }
    CHECK(QMaterialSymbols::icon(QStringLiteral("no_such_symbol")).isNull());
}

} // namespace

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    testNames();
    testDrawing();
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
