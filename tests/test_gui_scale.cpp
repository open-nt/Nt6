// MIT License - see LICENSE
//
// Tests of drawing on a screen with a device pixel ratio of 2, on the offscreen platform: the
// painter scales its coordinates, and windows and widgets keep their size in window coordinates
// while their pixels double.
#include <QtCore>
#include <QtGui/qguiapplication.h>
#include <QtGui/qimage.h>
#include <QtGui/qpainter.h>
#include <QtGui/qrasterwindow.h>
#include <QtGui/qscreen.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qwidget.h>

#include "platforms/offscreen/qoffscreenintegration_p.h"

#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

const QRgb kRed = 0xffff0000u;
const QRgb kWhite = 0xffffffffu;

QImage scaledImage(int w, int h) {
    QImage image(w * 2, h * 2, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(2.0);
    image.fill(Qt::white);
    return image;
}

int inkCount(const QImage &image) {
    int n = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixel(x, y) != kWhite)
                ++n;
    return n;
}

class RedWindow : public QRasterWindow {
public:
    int paints = 0;

protected:
    void paintEvent(QPaintEvent *) override {
        ++paints;
        QPainter p(this);
        p.fillRect(0, 0, width(), height(), Qt::white);
        p.fillRect(10, 5, 10, 10, Qt::red);
    }
};

void testScreen() {
    QScreen *screen = QGuiApplication::primaryScreen();
    CHECK(screen->devicePixelRatio() == 2.0);

    RedWindow w;
    w.resize(40, 30);
    w.show();
    CHECK(w.devicePixelRatio() == 2.0);
    CHECK(w.size() == QSize(40, 30));
}

void testImage() {
    QImage image(10, 6, QImage::Format_ARGB32_Premultiplied);
    CHECK(image.devicePixelRatio() == 1.0);
    image.setDevicePixelRatio(0.5); // below 1 is ignored
    CHECK(image.devicePixelRatio() == 1.0);
    image.setDevicePixelRatio(2.0);
    CHECK(image.devicePixelRatio() == 2.0 && image.width() == 10 && image.height() == 6);
    QImage shared = image;
    shared.setDevicePixelRatio(3.0); // detaches; the first image keeps its ratio
    CHECK(image.devicePixelRatio() == 2.0 && shared.devicePixelRatio() == 3.0);
    CHECK(image.copy(1, 1, 4, 4).devicePixelRatio() == 2.0);
    CHECK(image.convertToFormat(QImage::Format_ARGB32).devicePixelRatio() == 2.0);
}

void testPrimitives() {
    // Rectangles
    QImage image = scaledImage(10, 10);
    {
        QPainter p(&image);
        p.fillRect(1, 2, 3, 2, Qt::red);
    }
    CHECK(image.pixel(2, 4) == kRed && image.pixel(7, 7) == kRed);
    CHECK(image.pixel(1, 4) == kWhite && image.pixel(8, 4) == kWhite);
    CHECK(image.pixel(2, 3) == kWhite && image.pixel(2, 8) == kWhite);
    CHECK(inkCount(image) == 6 * 4);

    // The outline of a rectangle covers one more pixel to the right and below, now two device pixels
    image.fill(Qt::white);
    {
        QPainter p(&image);
        p.setPen(Qt::red);
        p.setBrush(Qt::NoBrush);
        p.drawRect(1, 1, 5, 4);
    }
    CHECK(image.pixel(2, 2) == kRed && image.pixel(13, 2) == kRed && image.pixel(2, 11) == kRed);
    CHECK(image.pixel(13, 11) == kRed && image.pixel(1, 2) == kWhite && image.pixel(14, 2) == kWhite);
    CHECK(image.pixel(6, 6) == kWhite);
    CHECK(inkCount(image) == 12 * 10 - 8 * 6);

    // Lines, points and the pen width
    image.fill(Qt::white);
    {
        QPainter p(&image);
        p.setPen(Qt::red);
        p.drawLine(0, 3, 4, 3);
        p.drawPoint(7, 7);
    }
    CHECK(image.pixel(0, 6) == kRed && image.pixel(9, 7) == kRed && image.pixel(10, 6) == kWhite &&
          image.pixel(0, 5) == kWhite && image.pixel(0, 8) == kWhite);
    CHECK(image.pixel(14, 14) == kRed && image.pixel(15, 15) == kRed && image.pixel(13, 14) == kWhite);
    CHECK(inkCount(image) == 10 * 2 + 4);

    // Translation and clipping work in the painter's coordinates
    image.fill(Qt::white);
    {
        QPainter p(&image);
        p.translate(2, 1);
        p.setClipRect(QRect(0, 0, 2, 2));
        p.fillRect(0, 0, 9, 9, Qt::red);
        CHECK(p.translation() == QPoint(2, 1));
        CHECK(p.clipRegion().boundingRect() == QRect(0, 0, 2, 2));
    }
    CHECK(image.pixel(4, 2) == kRed && image.pixel(7, 5) == kRed);
    CHECK(image.pixel(3, 2) == kWhite && image.pixel(8, 5) == kWhite && image.pixel(4, 6) == kWhite);
    CHECK(inkCount(image) == 16);

    // An ellipse fills its rectangle
    image.fill(Qt::white);
    {
        QPainter p(&image);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::red);
        p.drawEllipse(2, 2, 6, 6);
    }
    CHECK(image.pixel(10, 10) == kRed && image.pixel(4, 4) == kWhite && image.pixel(4, 10) == kRed);
    CHECK(image.pixel(3, 10) == kWhite && image.pixel(10, 3) == kWhite);
}

void testImages() {
    QImage source(2, 2, QImage::Format_ARGB32_Premultiplied);
    source.fill(Qt::red);
    QImage target = scaledImage(10, 10);
    {
        QPainter p(&target);
        p.drawImage(1, 1, source); // 2 by 2 units: 4 by 4 pixels
    }
    CHECK(target.pixel(2, 2) == kRed && target.pixel(5, 5) == kRed && target.pixel(6, 6) == kWhite);
    CHECK(inkCount(target) == 16);

    // An image with the ratio of the target is drawn at its own size
    QImage sharp(4, 4, QImage::Format_ARGB32_Premultiplied);
    sharp.setDevicePixelRatio(2.0);
    sharp.fill(Qt::red);
    target.fill(Qt::white);
    {
        QPainter p(&target);
        p.drawImage(QPoint(0, 0), sharp);
    }
    CHECK(inkCount(target) == 16 && target.pixel(3, 3) == kRed && target.pixel(4, 4) == kWhite);
}

void testText() {
    if (!QFontMetrics(QFont("sans-serif", 10)).inFont(QChar('A'))) {
        std::puts("SKIP: no fonts for the text tests");
        return;
    }
    QFont font("sans-serif", 10);
    QImage one(60, 30, QImage::Format_ARGB32_Premultiplied);
    one.fill(Qt::white);
    QImage two = scaledImage(60, 30);
    QRect bound1, bound2;
    {
        QPainter p(&one);
        p.setFont(font);
        p.drawText(QRect(0, 0, 60, 30), Qt::AlignCenter, "Wg", &bound1);
    }
    {
        QPainter p(&two);
        p.setFont(font);
        p.drawText(QRect(0, 0, 60, 30), Qt::AlignCenter, "Wg", &bound2);
        CHECK(p.boundingRect(QRect(0, 0, 60, 30), Qt::AlignCenter, "Wg") == bound2);
    }
    // The same text covers about the same area of the window, with up to four times as many pixels (a hinted font
    // may cover fewer at the smaller size, or more)
    CHECK(std::abs(bound1.width() - bound2.width()) <= 2 && std::abs(bound1.height() - bound2.height()) <= 2);
    CHECK(inkCount(two) > 2 * inkCount(one));
    CHECK(QRect(0, 0, 60, 30).contains(bound2));
}

void testWindow() {
    RedWindow w;
    w.resize(40, 30);
    w.show();
    QCoreApplication::sendPostedEvents(&w);
    CHECK(w.paints >= 1);
    const QImage image = QOffscreenBackingStore::imageOf(&w);
    CHECK(image.size() == QSize(80, 60) && image.devicePixelRatio() == 2.0);
    CHECK(image.pixel(20, 10) == kRed && image.pixel(39, 29) == kRed);
    CHECK(image.pixel(19, 10) == kWhite && image.pixel(40, 10) == kWhite && image.pixel(20, 30) == kWhite);
}

void testWidgets() {
    QWidget top;
    QPushButton button("OK", &top);
    button.setGeometry(4, 4, 60, 24);
    top.resize(100, 40);
    const QSize hint = button.sizeHint();
    CHECK(hint.width() > 0 && hint.width() < 100 && hint.height() > 10 && hint.height() < 40);
    top.show();
    for (int i = 0; i < 3; ++i) {
        QCoreApplication::sendPostedEvents(&top);
        QCoreApplication::sendPostedEvents(top.windowHandle());
    }
    CHECK(top.size() == QSize(100, 40) && button.geometry() == QRect(4, 4, 60, 24));
    CHECK(top.devicePixelRatio() == 2.0 && button.devicePixelRatio() == 2.0);
    const QImage image = QOffscreenBackingStore::imageOf(top.windowHandle());
    CHECK(image.size() == QSize(200, 80));
    // The button is drawn into the doubled rectangle; the background around it is the window colour
    CHECK(image.pixel(8, 8) != image.pixel(2, 2));
    CHECK(image.pixel(2, 2) == image.pixel(190, 70));
    // A click at the window coordinates of the button reaches it
    int clicks = 0;
    QObject::connect(&button, &QAbstractButton::clicked, [&] { ++clicks; });
    button.click();
    CHECK(clicks == 1);
}

} // namespace

int main(int argc, char **argv) {
    ::setenv("QT_QPA_PLATFORM", "offscreen", 1);
    ::setenv("QT_SCALE_FACTOR", "2", 1);
    QApplication app(argc, argv);
    testScreen();
    testImage();
    testPrimitives();
    testImages();
    testText();
    testWindow();
    testWidgets();
    if (g_failures == 0)
        std::puts("all scale tests passed");
    return g_failures == 0 ? 0 : 1;
}
