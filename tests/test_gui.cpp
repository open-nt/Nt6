// MIT License - see LICENSE
//
// Tests for Nt GUI on the offscreen platform: value types, painting, events and the window
// life cycle. No display is needed.
#include <QtCore>
#include <QtGui/qbackingstore.h>
#include <QtGui/qbrush.h>
#include <QtGui/qcolor.h>
#include <QtGui/qevent.h>
#include <QtGui/qfont.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qimage.h>
#include <QtGui/qpaintdevicewindow.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpen.h>
#include <QtGui/qrasterwindow.h>
#include <QtGui/qregion.h>
#include <QtGui/qscreen.h>
#include <QtGui/qstylehints.h>
#include <QtGui/qwindow.h>

#include "qpa/qdesktopportal_p.h"
#include "qpa/qplatformintegration_p.h"
#include "qpa/qwindowsysteminterface_p.h"
#include "qwindow_p.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static void testRgb() {
    const QRgb c = qRgba(10, 20, 30, 40);
    CHECK(qRed(c) == 10 && qGreen(c) == 20 && qBlue(c) == 30 && qAlpha(c) == 40);
    CHECK(qRgb(1, 2, 3) == 0xff010203u);
    CHECK(qGray(255, 255, 255) == 255 && qGray(0, 0, 0) == 0);
    CHECK(qPremultiply(0xff102030u) == 0xff102030u);
    CHECK(qPremultiply(0x80ff0000u) == 0x80800000u);
    CHECK(qUnpremultiply(qPremultiply(0x80ff0000u)) == 0x80ff0000u);
    CHECK(qUnpremultiply(0u) == 0u);
}

static void testColor() {
    QColor invalid;
    CHECK(!invalid.isValid());
    CHECK(invalid.name().isEmpty());

    QColor c(255, 128, 0);
    CHECK(c.isValid() && c.red() == 255 && c.green() == 128 && c.blue() == 0 && c.alpha() == 255);
    CHECK(c.name() == QString("#ff8000"));
    CHECK(c.name(QColor::HexArgb) == QString("#ffff8000"));
    CHECK(QColor(300, -5, 7).red() == 255 && QColor(300, -5, 7).green() == 0);

    CHECK(QColor("#336699") == QColor(0x33, 0x66, 0x99));
    CHECK(QColor("#369") == QColor(0x33, 0x66, 0x99));
    CHECK(QColor("#80ff0000").alpha() == 0x80);
    CHECK(QColor("Red") == QColor(Qt::red));
    CHECK(QColor("white") == QColor(255, 255, 255));
    CHECK(!QColor("nonsense").isValid());
    CHECK(!QColor("#12").isValid());
    CHECK(!QColor("#gggggg").isValid());
    CHECK(QColor::isValidColor("#abcdef") && !QColor::isValidColor("#abcde"));

    CHECK(QColor(Qt::transparent).alpha() == 0);
    CHECK(QColor(Qt::black) == QColor(0, 0, 0));
    CHECK(QColor(Qt::darkGray) == QColor(128, 128, 128));

    CHECK(QColor::fromRgba(0x80102030u).alpha() == 0x80);
    CHECK(QColor(0x80102030u).alpha() == 255);
    CHECK(QColor::fromRgbF(1.0, 0.0, 0.5, 1.0).blue() == 128);
    QColor f;
    f.setRedF(1.0);
    CHECK(f.isValid() && f.red() == 255);
    f.setAlpha(10);
    CHECK(f.alpha() == 10 && qFuzzyCompare(f.alphaF() + 1.0, 10 / 255.0 + 1.0));
    int r = 0, g = 0, b = 0, a = 0;
    c.getRgb(&r, &g, &b, &a);
    CHECK(r == 255 && g == 128 && b == 0 && a == 255);

    CHECK(QColor(255, 0, 0).hue() == 0 && QColor(0, 255, 0).hue() == 120 && QColor(0, 0, 255).hue() == 240);
    CHECK(QColor(128, 128, 128).hue() == -1);
    CHECK(QColor(255, 0, 0).saturation() == 255 && QColor(255, 0, 0).value() == 255);
    CHECK(QColor::fromHsv(120, 255, 255) == QColor(0, 255, 0));
    CHECK(QColor::fromHsv(0, 0, 100) == QColor(100, 100, 100));
    CHECK(QColor(100, 100, 100).lighter(200).value() == 200);
    CHECK(QColor(200, 200, 200).darker(200).value() == 100);
    CHECK(QColor(100, 100, 100).lighter(100) == QColor(100, 100, 100));
    CHECK(QColor(Qt::black).lighter().value() > 0);
}

static void testRegion() {
    QRegion empty;
    CHECK(empty.isEmpty() && empty.boundingRect().isNull() && empty.rectCount() == 0);
    QRegion a(0, 0, 10, 10);
    CHECK(!a.isEmpty() && a.boundingRect() == QRect(0, 0, 10, 10));
    CHECK(a.contains(QPoint(5, 5)) && !a.contains(QPoint(10, 10)));
    QRegion b = a + QRect(20, 20, 5, 5);
    CHECK(b.rectCount() == 2 && b.boundingRect() == QRect(0, 0, 25, 25));
    CHECK(b.contains(QPoint(22, 22)) && !b.contains(QPoint(15, 15)));
    CHECK(b.intersects(QRect(24, 24, 10, 10)) && !b.intersects(QRect(12, 12, 5, 5)));
    CHECK((b & QRect(0, 0, 3, 3)).boundingRect() == QRect(0, 0, 3, 3));
    CHECK((b & QRect(100, 100, 3, 3)).isEmpty());
    CHECK(b.translated(1, 2).boundingRect() == QRect(1, 2, 25, 25));
    CHECK(a.united(QRect(2, 2, 3, 3)).rectCount() == 1);
    CHECK(QRegion(QRect(0, 0, 0, 5)).isEmpty());
    QRegion c = a;
    c += QRegion(QRect(5, 5, 10, 10));
    CHECK(c.boundingRect() == QRect(0, 0, 15, 15));
    CHECK(a == QRegion(QRect(0, 0, 10, 10)) && a != c);
    CHECK(a.intersected(QRegion(QRect(5, 5, 10, 10))).boundingRect() == QRect(5, 5, 5, 5));
}

static void testPenBrush() {
    QPen pen;
    CHECK(pen.style() == Qt::SolidLine && pen.width() == 1 && pen.color() == QColor(Qt::black));
    pen.setWidth(3);
    pen.setColor(Qt::red);
    pen.setStyle(Qt::NoPen);
    CHECK(pen.width() == 3 && pen.color() == QColor(Qt::red) && pen.style() == Qt::NoPen);
    CHECK(QPen(Qt::SolidLine) == QPen(Qt::SolidLine) && QPen(QColor(Qt::red)) != QPen());
    CHECK(QPen(QBrush(Qt::blue), 2.5).widthF() == 2.5);

    QBrush brush;
    CHECK(brush.style() == Qt::NoBrush);
    QBrush solid(QColor(1, 2, 3));
    CHECK(solid.style() == Qt::SolidPattern && solid.color() == QColor(1, 2, 3));
    solid.setColor(Qt::green);
    solid.setStyle(Qt::NoBrush);
    CHECK(solid.color() == QColor(Qt::green) && solid.style() == Qt::NoBrush);
    CHECK(QBrush(Qt::red) == QBrush(QColor(255, 0, 0)) && QBrush(Qt::red) != QBrush(Qt::blue));
}

static void testImage() {
    QImage null;
    CHECK(null.isNull() && null.width() == 0 && null.format() == QImage::Format_Invalid && null.bits() == nullptr);
    CHECK(QImage(0, 5, QImage::Format_RGB32).isNull());
    CHECK(QImage(5, 5, QImage::Format_Invalid).isNull());

    QImage img(4, 3, QImage::Format_ARGB32);
    CHECK(!img.isNull() && img.width() == 4 && img.height() == 3 && img.size() == QSize(4, 3));
    CHECK(img.rect() == QRect(0, 0, 4, 3) && img.depth() == 32 && img.bytesPerLine() == 16);
    CHECK(img.sizeInBytes() == 48 && img.hasAlphaChannel());
    CHECK(img.pixel(0, 0) == 0u); // new images are zeroed
    img.fill(QColor(10, 20, 30, 40));
    CHECK(img.pixel(3, 2) == qRgba(10, 20, 30, 40));
    CHECK(img.pixelColor(1, 1) == QColor(10, 20, 30, 40));
    img.setPixel(1, 1, 0xff00ff00u);
    CHECK(img.pixel(1, 1) == 0xff00ff00u);
    img.setPixelColor(2, 2, QColor(Qt::blue));
    CHECK(img.pixel(2, 2) == 0xff0000ffu);
    CHECK(img.pixel(10, 10) == 0u && !img.pixelColor(-1, 0).isValid() && !img.valid(4, 0) && img.valid(3, 2));
    img.setPixel(99, 99, 1); // out of range is ignored
    CHECK(img.scanLine(5) == nullptr && img.constScanLine(-1) == nullptr);

    QImage pre(2, 1, QImage::Format_ARGB32_Premultiplied);
    pre.setPixel(0, 0, 0x80ff0000u);
    CHECK(pre.constBits() != nullptr);
    uint stored;
    std::memcpy(&stored, pre.constScanLine(0), sizeof stored);
    CHECK(stored == 0x80800000u);
    CHECK(pre.pixel(0, 0) == 0x80ff0000u);

    QImage rgb(2, 1, QImage::Format_RGB32);
    rgb.setPixel(0, 0, 0x12345678u);
    CHECK(rgb.pixel(0, 0) == 0xff345678u && !rgb.hasAlphaChannel());
    CHECK(rgb.convertToFormat(QImage::Format_ARGB32).pixel(0, 0) == 0xff345678u);
    CHECK(pre.convertToFormat(QImage::Format_ARGB32).pixel(0, 0) == 0x80ff0000u);
    CHECK(pre.convertToFormat(QImage::Format_RGB32).pixel(0, 0) == 0xffff0000u);
    CHECK(pre.convertToFormat(QImage::Format_Invalid).isNull() && null.convertToFormat(QImage::Format_RGB32).isNull());

    // Implicit sharing
    QImage a(2, 2, QImage::Format_RGB32);
    a.fill(Qt::red);
    QImage b = a;
    CHECK(a == b && !a.isDetached());
    b.setPixel(0, 0, 0xff0000ffu);
    CHECK(a.pixel(0, 0) == 0xffff0000u && b.pixel(0, 0) == 0xff0000ffu && a != b);
    CHECK(a.isDetached() && b.isDetached());
    QImage c = a;
    c.detach();
    CHECK(c.isDetached() && c == a);

    // copy()
    QImage big(5, 5, QImage::Format_RGB32);
    big.fill(Qt::white);
    big.setPixel(2, 2, 0xff000000u);
    QImage sub = big.copy(QRect(1, 1, 3, 3));
    CHECK(sub.size() == QSize(3, 3) && sub.pixel(1, 1) == 0xff000000u && sub.pixel(0, 0) == 0xffffffffu);
    QImage outside = big.copy(QRect(3, 3, 4, 4));
    CHECK(outside.size() == QSize(4, 4) && outside.pixel(0, 0) == 0xffffffffu && outside.pixel(3, 3) == 0xff000000u);
    CHECK(big.copy().size() == big.size() && big.copy(0, 0, 2, 2).size() == QSize(2, 2) && null.copy().isNull());

    // External memory
    std::vector<uchar> mem(2 * 2 * 4, 0);
    QImage wrap(mem.data(), 2, 2, QImage::Format_RGB32);
    wrap.fill(Qt::green);
    CHECK(mem[1] == 0xff && mem[2] == 0x00); // memory changed in place: 0xff00ff00 little endian => 00 ff 00 ff
    QImage copyOfWrap = wrap;
    copyOfWrap.setPixel(0, 0, 0xff0000ffu); // detaches: the memory stays as it is
    CHECK(wrap.pixel(0, 0) == 0xff00ff00u);
    const std::vector<uchar> constMem(16, 0xff);
    QImage ro(constMem.data(), 2, 2, QImage::Format_RGB32);
    CHECK(!ro.isDetached());
    ro.setPixel(0, 0, 0xff112233u);
    CHECK(ro.isDetached() && constMem[0] == 0xff);
    CHECK(QImage(static_cast<uchar *>(nullptr), 2, 2, QImage::Format_RGB32).isNull());
    CHECK(QImage(mem.data(), 2, 2, 4, QImage::Format_RGB32).isNull()); // bytesPerLine too small
    CHECK(QImage(mem.data(), 2, 2, 9, QImage::Format_RGB32).isNull()); // not a multiple of 4

    QImage moved(std::move(a));
    CHECK(a.isNull() && !moved.isNull()); // NOLINT(bugprone-use-after-move)
    QImage assigned;
    assigned = std::move(moved);
    CHECK(!assigned.isNull());
    QImage s1(1, 1, QImage::Format_RGB32), s2(2, 2, QImage::Format_RGB32);
    s1.swap(s2);
    CHECK(s1.width() == 2 && s2.width() == 1);
    CHECK(s1.width() == 2 && s1.height() == 2 && s1.QPaintDevice::width() == 2);
}

static QImage newImage(int w, int h, QImage::Format f = QImage::Format_ARGB32_Premultiplied) {
    QImage img(w, h, f);
    img.fill(Qt::white);
    return img;
}

static void testPainterBasics() {
    QImage img = newImage(20, 20);
    QPainter p;
    CHECK(!p.isActive() && !p.end() && p.device() == nullptr);
    CHECK(!p.begin(nullptr));
    CHECK(p.begin(&img));
    CHECK(p.isActive() && p.device() == &img && p.paintEngine() != nullptr && img.paintingActive());
    CHECK(!p.begin(&img)); // already active
    QPainter other;
    CHECK(!other.begin(&img)); // device is busy
    CHECK(p.end() && !img.paintingActive() && !p.isActive());

    QImage nullImage;
    QPainter bad(&nullImage);
    CHECK(!bad.isActive());

    // Drawing on an inactive painter is harmless.
    QPainter idle;
    idle.fillRect(0, 0, 5, 5, Qt::red);
    idle.drawLine(0, 0, 1, 1);
    idle.drawRect(0, 0, 1, 1);
    idle.drawEllipse(0, 0, 3, 3);
    idle.drawPoint(1, 1);
    idle.drawImage(0, 0, img);
    idle.eraseRect(QRect(0, 0, 1, 1));
    CHECK(img.pixel(0, 0) == 0xffffffffu);

    CHECK(p.begin(&img));
    QPainter::RenderHints hints = p.renderHints();
    CHECK(hints == QPainter::RenderHints());
    p.setRenderHint(QPainter::Antialiasing);
    CHECK(p.renderHints().testFlag(QPainter::Antialiasing));
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHints(QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    CHECK(p.renderHints().testFlag(QPainter::SmoothPixmapTransform));
    p.setRenderHints(QPainter::TextAntialiasing, false);
    CHECK(!p.renderHints().testFlag(QPainter::TextAntialiasing));
    p.setOpacity(2.0);
    CHECK(p.opacity() == 1.0);
    p.setOpacity(-1.0);
    CHECK(p.opacity() == 0.0);
    p.setOpacity(0.5);
    CHECK(p.opacity() == 0.5);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    CHECK(p.compositionMode() == QPainter::CompositionMode_Source);
    p.restore(); // nothing saved
    p.setPen(Qt::NoPen);
    CHECK(p.pen().style() == Qt::NoPen);
    p.setPen(QColor(Qt::red));
    CHECK(p.pen().color() == QColor(Qt::red) && p.pen().style() == Qt::SolidLine);
    p.setBrush(Qt::SolidPattern);
    CHECK(p.brush().style() == Qt::SolidPattern);
    p.setBrush(QBrush(Qt::blue));
    CHECK(p.brush().color() == QColor(Qt::blue));
    p.end();
}

static void testPainterShapes() {
    // fillRect fills exactly the rectangle and does not change the pen or brush.
    QImage img = newImage(20, 20);
    {
        QPainter p(&img);
        p.setPen(QPen(Qt::green));
        p.setBrush(Qt::NoBrush);
        p.fillRect(QRect(2, 3, 4, 5), QColor(Qt::red));
        CHECK(p.pen().color() == QColor(Qt::green) && p.brush().style() == Qt::NoBrush);
    }
    CHECK(img.pixel(2, 3) == 0xffff0000u && img.pixel(5, 7) == 0xffff0000u);
    CHECK(img.pixel(6, 3) == 0xffffffffu && img.pixel(2, 8) == 0xffffffffu && img.pixel(1, 3) == 0xffffffffu);
    {
        QPainter p(&img);
        p.fillRect(10, 10, 2, 2, QBrush(Qt::blue));
        p.fillRect(14, 14, 2, 2, Qt::NoBrush); // nothing
        p.fillRect(16, 16, 2, 2, QColor(0, 255, 0));
        p.fillRect(0, 0, 1, 1, Qt::yellow);
        p.fillRect(QRect(0, 19, 1, 1), Qt::cyan);
    }
    CHECK(img.pixel(10, 10) == 0xff0000ffu && img.pixel(14, 14) == 0xffffffffu && img.pixel(17, 17) == 0xff00ff00u);
    CHECK(img.pixel(0, 0) == 0xffffff00u && img.pixel(0, 19) == 0xff00ffffu);

    // drawRect with a one pixel pen outlines w+1 by h+1 pixels; the brush fills the inside.
    img = newImage(20, 20);
    {
        QPainter p(&img);
        p.setPen(QPen(Qt::black));
        p.setBrush(Qt::red);
        p.drawRect(2, 2, 6, 6);
    }
    CHECK(img.pixel(2, 2) == 0xff000000u && img.pixel(8, 8) == 0xff000000u && img.pixel(8, 2) == 0xff000000u);
    CHECK(img.pixel(5, 2) == 0xff000000u && img.pixel(2, 5) == 0xff000000u && img.pixel(8, 5) == 0xff000000u);
    CHECK(img.pixel(3, 3) == 0xffff0000u && img.pixel(7, 7) == 0xffff0000u && img.pixel(9, 9) == 0xffffffffu);
    // No pen: the rectangle is exactly w by h.
    img = newImage(20, 20);
    {
        QPainter p(&img);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::red);
        p.drawRect(2, 2, 6, 6);
    }
    CHECK(img.pixel(2, 2) == 0xffff0000u && img.pixel(7, 7) == 0xffff0000u && img.pixel(8, 8) == 0xffffffffu);
    // Wide pen
    img = newImage(30, 30);
    {
        QPainter p(&img);
        p.setPen(QPen(QBrush(Qt::black), 3));
        p.setBrush(Qt::NoBrush);
        p.drawRect(5, 5, 20, 20);
    }
    CHECK(img.pixel(4, 4) == 0xff000000u && img.pixel(5, 12) == 0xff000000u && img.pixel(6, 12) == 0xff000000u &&
          img.pixel(7, 12) == 0xffffffffu && img.pixel(12, 12) == 0xffffffffu);
    // Small rectangle with a wide pen is a solid block.
    img = newImage(20, 20);
    {
        QPainter p(&img);
        p.setPen(QPen(QBrush(Qt::black), 5));
        p.drawRect(8, 8, 1, 1);
    }
    CHECK(img.pixel(8, 8) == 0xff000000u && img.pixel(9, 9) == 0xff000000u);

    // Lines
    img = newImage(20, 20);
    {
        QPainter p(&img);
        p.setPen(QPen(Qt::black));
        p.drawLine(0, 0, 19, 19);
        p.drawLine(QPoint(0, 5), QPoint(10, 5));
        p.drawLine(QLine(15, 0, 15, 10));
        p.drawLine(19, 0, 10, 9);
        p.drawPoint(3, 17);
        p.drawPoint(QPoint(4, 17));
    }
    CHECK(img.pixel(0, 0) == 0xff000000u && img.pixel(10, 10) == 0xff000000u && img.pixel(19, 19) == 0xff000000u);
    CHECK(img.pixel(0, 5) == 0xff000000u && img.pixel(10, 5) == 0xff000000u && img.pixel(11, 5) == 0xffffffffu);
    CHECK(img.pixel(15, 0) == 0xff000000u && img.pixel(15, 10) == 0xff000000u && img.pixel(15, 11) == 0xffffffffu);
    CHECK(img.pixel(19, 0) == 0xff000000u && img.pixel(10, 9) == 0xff000000u);
    CHECK(img.pixel(3, 17) == 0xff000000u && img.pixel(4, 17) == 0xff000000u && img.pixel(5, 17) == 0xffffffffu);
    // Reversed and wide lines; a line with the pen off draws nothing
    img = newImage(20, 20);
    {
        QPainter p(&img);
        p.setPen(QPen(QBrush(Qt::black), 3));
        p.drawLine(10, 2, 10, 15);
        p.drawLine(2, 10, 15, 10);
        p.drawLine(18, 18, 12, 12);
        p.setPen(Qt::NoPen);
        p.drawLine(0, 0, 5, 5);
        p.drawPoint(1, 1);
    }
    CHECK(img.pixel(9, 5) == 0xff000000u && img.pixel(11, 5) == 0xff000000u && img.pixel(12, 5) == 0xffffffffu);
    CHECK(img.pixel(5, 9) == 0xff000000u && img.pixel(5, 11) == 0xff000000u);
    CHECK(img.pixel(15, 15) == 0xff000000u && img.pixel(0, 0) == 0xffffffffu && img.pixel(1, 1) == 0xffffffffu);

    // Ellipses
    img = newImage(30, 30);
    {
        QPainter p(&img);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::red);
        p.drawEllipse(QRect(5, 5, 20, 20));
    }
    CHECK(img.pixel(15, 15) == 0xffff0000u && img.pixel(15, 6) == 0xffff0000u && img.pixel(5, 15) == 0xffff0000u);
    CHECK(img.pixel(5, 5) == 0xffffffffu && img.pixel(24, 24) == 0xffffffffu && img.pixel(2, 15) == 0xffffffffu);
    img = newImage(30, 30);
    {
        QPainter p(&img);
        p.setPen(QPen(Qt::black));
        p.setBrush(Qt::red);
        p.drawEllipse(5, 5, 20, 20);
    }
    CHECK(img.pixel(15, 15) == 0xffff0000u && img.pixel(15, 5) == 0xff000000u && img.pixel(5, 15) == 0xff000000u);
    CHECK(img.pixel(24, 15) == 0xff000000u && img.pixel(15, 24) == 0xff000000u);
    img = newImage(30, 30);
    {
        QPainter p(&img);
        p.setPen(QPen(QBrush(Qt::black), 6));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(5, 5, 10, 10); // the pen fills the whole ellipse
        p.setPen(Qt::NoPen);
        p.drawEllipse(20, 20, 5, 5); // nothing to draw
        p.drawEllipse(QRect(0, 0, 0, 5));
    }
    CHECK(img.pixel(10, 10) == 0xff000000u && img.pixel(22, 22) == 0xffffffffu);
}

static void testPainterState() {
    // translate and save/restore
    QImage img = newImage(20, 20);
    {
        QPainter p(&img);
        p.translate(5, 5);
        CHECK(p.translation() == QPoint(5, 5));
        p.save();
        p.translate(QPoint(2, 2));
        p.setBrush(Qt::red);
        p.setPen(Qt::NoPen);
        p.drawRect(0, 0, 2, 2);
        CHECK(p.translation() == QPoint(7, 7));
        p.restore();
        CHECK(p.translation() == QPoint(5, 5) && p.brush().style() == Qt::NoBrush);
        p.fillRect(0, 0, 1, 1, Qt::blue);
        p.resetTransform();
        CHECK(p.translation() == QPoint());
        p.fillRect(0, 0, 1, 1, Qt::green);
    }
    CHECK(img.pixel(7, 7) == 0xffff0000u && img.pixel(8, 8) == 0xffff0000u && img.pixel(5, 5) == 0xff0000ffu);
    CHECK(img.pixel(0, 0) == 0xff00ff00u);

    // clipping
    img = newImage(20, 20);
    {
        QPainter p(&img);
        CHECK(!p.hasClipping() && p.clipRegion().isEmpty());
        p.setClipRect(5, 5, 5, 5);
        CHECK(p.hasClipping() && p.clipRegion().boundingRect() == QRect(5, 5, 5, 5));
        p.fillRect(0, 0, 20, 20, Qt::red);
        p.setClipping(false);
        p.fillRect(0, 0, 1, 1, Qt::blue);
        p.setClipping(true);
        p.fillRect(19, 19, 1, 1, Qt::blue); // clipped out
        QRegion two(QRect(12, 12, 4, 4));
        two += QRect(14, 14, 4, 4); // overlapping rectangles must not blend twice
        p.setClipRegion(two);
        p.setOpacity(0.5);
        p.fillRect(0, 0, 20, 20, Qt::black);
        p.setOpacity(1.0);
        p.translate(1, 1);
        p.setClipRect(0, 0, 1, 1); // in translated coordinates
        CHECK(p.clipRegion().boundingRect() == QRect(0, 0, 1, 1));
        p.fillRect(0, 0, 5, 5, Qt::yellow);
        p.setClipRegion(QRegion()); // empty clips everything
        p.fillRect(0, 0, 20, 20, Qt::magenta);
    }
    CHECK(img.pixel(5, 5) == 0xffff0000u && img.pixel(9, 9) == 0xffff0000u && img.pixel(10, 10) == 0xffffffffu);
    CHECK(img.pixel(0, 0) == 0xff0000ffu && img.pixel(19, 19) == 0xffffffffu);
    CHECK(img.pixel(14, 14) == img.pixel(12, 12)); // the overlap was blended once
    CHECK(qRed(img.pixel(14, 14)) > 100 && qRed(img.pixel(14, 14)) < 150);
    CHECK(img.pixel(1, 1) == 0xffffff00u && img.pixel(2, 2) == 0xffffffffu);

    // opacity and alpha
    img = newImage(4, 4);
    {
        QPainter p(&img);
        p.setOpacity(0.5);
        p.fillRect(0, 0, 2, 2, Qt::black);
        p.setOpacity(1.0);
        p.fillRect(2, 0, 2, 2, QColor(0, 0, 0, 128));
        p.fillRect(0, 2, 2, 2, QColor(255, 0, 0, 0)); // fully transparent: no change
    }
    const int half = qRed(img.pixel(0, 0));
    CHECK(half > 120 && half < 135);
    CHECK(img.pixel(0, 0) == img.pixel(2, 0));
    CHECK(img.pixel(0, 2) == 0xffffffffu);
    // All three formats blend alike
    QImage argb(2, 2, QImage::Format_ARGB32), rgb(2, 2, QImage::Format_RGB32);
    argb.fill(Qt::white);
    rgb.fill(Qt::white);
    for (QImage *i : {&argb, &rgb}) {
        QPainter p(i);
        p.fillRect(0, 0, 2, 2, QColor(0, 0, 0, 128));
    }
    CHECK(qRed(argb.pixel(0, 0)) == half || qAbs(qRed(argb.pixel(0, 0)) - half) <= 1);
    CHECK(rgb.pixel(0, 0) == argb.pixel(0, 0));
    // translucent over transparent
    QImage clear(2, 2, QImage::Format_ARGB32);
    clear.fill(0u);
    {
        QPainter p(&clear);
        p.fillRect(0, 0, 2, 2, QColor(255, 0, 0, 128));
    }
    CHECK(qAlpha(clear.pixel(0, 0)) == 128 && qRed(clear.pixel(0, 0)) == 255);

    // composition modes
    img = newImage(4, 4);
    {
        QPainter p(&img);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(0, 0, 2, 2, QColor(255, 0, 0, 100));
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        p.eraseRect(QRect(2, 2, 2, 2));
        CHECK(p.compositionMode() == QPainter::CompositionMode_SourceOver);
    }
    CHECK(qAlpha(img.pixel(0, 0)) == 100 && qRed(img.pixel(0, 0)) == 255);
    CHECK(img.pixel(3, 3) == 0u && img.pixel(1, 3) == 0xffffffffu);
    QImage opaque = newImage(2, 2, QImage::Format_RGB32);
    {
        QPainter p(&opaque);
        p.eraseRect(QRect(0, 0, 1, 1));
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(1, 1, 1, 1, QColor(0, 255, 0, 10));
    }
    CHECK(opaque.pixel(0, 0) == 0xff000000u && opaque.pixel(1, 1) == 0xff00ff00u);
}

static void testPainterImages() {
    QImage src(2, 2, QImage::Format_ARGB32_Premultiplied);
    src.setPixel(0, 0, 0xffff0000u);
    src.setPixel(1, 0, 0xff00ff00u);
    src.setPixel(0, 1, 0xff0000ffu);
    src.setPixel(1, 1, 0x00000000u);
    QImage dst = newImage(8, 8);
    {
        QPainter p(&dst);
        p.drawImage(QPoint(1, 1), src);
        p.drawImage(5, 5, src);
        p.drawImage(QPoint(4, 0), src, QRect(1, 0, 1, 1));
        p.drawImage(QRect(0, 4, 4, 4), src);                    // scaled up by two
        p.drawImage(QRect(6, 0, 1, 1), QImage());               // null image
        p.drawImage(QRect(6, 0, 2, 2), src, QRect(1, 1, 5, 5)); // source is clipped to the image
    }
    CHECK(dst.pixel(1, 1) == 0xffff0000u && dst.pixel(2, 1) == 0xff00ff00u && dst.pixel(1, 2) == 0xff0000ffu);
    CHECK(dst.pixel(2, 2) == 0xffffffffu); // the transparent source pixel leaves the background
    CHECK(dst.pixel(5, 5) == 0xffff0000u && dst.pixel(4, 0) == 0xff00ff00u);
    CHECK(dst.pixel(0, 4) == 0xffff0000u && dst.pixel(1, 5) == 0xffff0000u && dst.pixel(2, 4) == 0xff00ff00u &&
          dst.pixel(0, 6) == 0xff0000ffu && dst.pixel(3, 7) == 0xffffffffu);

    // Other source formats, opacity, source rectangles
    QImage rgb(1, 1, QImage::Format_RGB32);
    rgb.setPixel(0, 0, 0x00123456u);
    QImage argb(1, 1, QImage::Format_ARGB32);
    argb.setPixel(0, 0, 0x80ff0000u);
    QImage d2 = newImage(4, 4, QImage::Format_RGB32);
    {
        QPainter p(&d2);
        p.drawImage(QPoint(0, 0), rgb);
        p.drawImage(QPoint(1, 0), argb);
        p.setOpacity(0.5);
        p.drawImage(QPoint(2, 0), rgb);
        p.setOpacity(1.0);
        p.drawImage(QPoint(0, 2), src, QRect(1, 0, 1, 2));
    }
    CHECK(d2.pixel(0, 0) == 0xff123456u);
    CHECK(qRed(d2.pixel(1, 0)) == 255 && qGreen(d2.pixel(1, 0)) > 120 && qGreen(d2.pixel(1, 0)) < 135);
    CHECK(qRed(d2.pixel(2, 0)) > 130 && qRed(d2.pixel(2, 0)) < 145);
    CHECK(d2.pixel(0, 2) == 0xff00ff00u && d2.pixel(0, 3) == 0xffffffffu);
    // Drawing a window-sized image through clip
    QImage d3 = newImage(4, 4);
    {
        QPainter p(&d3);
        p.setClipRect(1, 1, 2, 2);
        p.drawImage(QPoint(0, 0), newImage(4, 4, QImage::Format_RGB32).copy());
        QImage black(4, 4, QImage::Format_RGB32);
        black.fill(Qt::black);
        p.drawImage(QPoint(0, 0), black);
    }
    CHECK(d3.pixel(1, 1) == 0xff000000u && d3.pixel(0, 0) == 0xffffffffu && d3.pixel(3, 3) == 0xffffffffu);
}

static void testEvents() {
    QMouseEvent m(QEvent::MouseButtonPress, QPointF(1.4, 2.6), QPointF(11, 12), Qt::LeftButton,
                  Qt::LeftButton | Qt::RightButton, Qt::ControlModifier);
    CHECK(m.type() == QEvent::MouseButtonPress && m.button() == Qt::LeftButton);
    CHECK(m.buttons().testFlag(Qt::RightButton) && m.modifiers() == Qt::ControlModifier);
    CHECK(m.position() == QPointF(1.4, 2.6) && m.globalPosition() == QPointF(11, 12));
    CHECK(m.pos() == QPoint(1, 3) && m.globalPos() == QPoint(11, 12) && m.x() == 1 && m.y() == 3);
    m.setModifiers(Qt::NoModifier);
    CHECK(m.modifiers() == Qt::NoModifier);

    QWheelEvent w(QPointF(3, 4), QPointF(5, 6), QPoint(0, 2), QPoint(0, 120), Qt::NoButton, Qt::ShiftModifier);
    CHECK(w.type() == QEvent::Wheel && w.angleDelta() == QPoint(0, 120) && w.pixelDelta() == QPoint(0, 2));
    CHECK(w.position() == QPointF(3, 4) && w.globalPosition() == QPointF(5, 6) && w.buttons() == Qt::NoButton);

    QKeyEvent k(QEvent::KeyPress, Qt::Key_A, Qt::ShiftModifier, "A", true, 2);
    CHECK(k.key() == Qt::Key_A && k.text() == QString("A") && k.isAutoRepeat() && k.count() == 2);
    QKeyEvent k2(QEvent::KeyRelease, Qt::Key_Escape, Qt::NoModifier);
    CHECK(k2.text().isEmpty() && !k2.isAutoRepeat() && k2.count() == 1 && k2.type() == QEvent::KeyRelease);

    QFocusEvent fi(QEvent::FocusIn), fo(QEvent::FocusOut);
    CHECK(fi.gotFocus() && !fi.lostFocus() && fo.lostFocus() && !fo.gotFocus());

    QEnterEvent en(QPointF(1.6, 2), QPointF(3, 4));
    CHECK(en.type() == QEvent::Enter && en.pos() == QPoint(2, 2) && en.globalPos() == QPoint(3, 4));
    CHECK(en.position() == QPointF(1.6, 2) && en.globalPosition() == QPointF(3, 4));

    QPaintEvent pe(QRect(1, 2, 3, 4));
    CHECK(pe.type() == QEvent::Paint && pe.rect() == QRect(1, 2, 3, 4) && pe.region().rectCount() == 1);
    QPaintEvent pr(QRegion(0, 0, 5, 5));
    CHECK(pr.rect() == QRect(0, 0, 5, 5));
    QExposeEvent ee(QRegion(0, 0, 2, 2));
    CHECK(ee.type() == QEvent::Expose && ee.region().boundingRect() == QRect(0, 0, 2, 2));
    QResizeEvent re(QSize(5, 6), QSize(1, 2));
    CHECK(re.type() == QEvent::Resize && re.size() == QSize(5, 6) && re.oldSize() == QSize(1, 2));
    QMoveEvent mv(QPoint(5, 6), QPoint(1, 2));
    CHECK(mv.type() == QEvent::Move && mv.pos() == QPoint(5, 6) && mv.oldPos() == QPoint(1, 2));
    CHECK(QShowEvent().type() == QEvent::Show && QHideEvent().type() == QEvent::Hide);
    QCloseEvent ce;
    CHECK(ce.type() == QEvent::Close && ce.isAccepted());
    ce.ignore();
    CHECK(!ce.isAccepted());
}

namespace {

// Records what a window receives.
class RecordingWindow : public QWindow {
public:
    QStringList log;
    bool acceptClose = true;
    QPoint lastMouse;
    int lastKey = 0;
    QString lastText;
    QPoint lastWheel;

protected:
    void exposeEvent(QExposeEvent *e) override { log << (e->region().isEmpty() ? "unexposed" : "expose"); }
    void resizeEvent(QResizeEvent *e) override {
        log << QString("resize %1x%2").arg(e->size().width()).arg(e->size().height());
    }
    void moveEvent(QMoveEvent *e) override { log << QString("move %1,%2").arg(e->pos().x()).arg(e->pos().y()); }
    void showEvent(QShowEvent *) override { log << "show"; }
    void hideEvent(QHideEvent *) override { log << "hide"; }
    void focusInEvent(QFocusEvent *) override { log << "focusIn"; }
    void focusOutEvent(QFocusEvent *) override { log << "focusOut"; }
    void closeEvent(QCloseEvent *e) override {
        log << "close";
        e->setAccepted(acceptClose);
    }
    void keyPressEvent(QKeyEvent *e) override {
        log << "keyPress";
        lastKey = e->key();
        lastText = e->text();
    }
    void keyReleaseEvent(QKeyEvent *e) override {
        log << "keyRelease";
        lastKey = e->key();
    }
    void mousePressEvent(QMouseEvent *e) override {
        log << "mousePress";
        lastMouse = e->pos();
    }
    void mouseReleaseEvent(QMouseEvent *) override { log << "mouseRelease"; }
    void mouseDoubleClickEvent(QMouseEvent *) override { log << "mouseDoubleClick"; }
    void mouseMoveEvent(QMouseEvent *e) override {
        log << "mouseMove";
        lastMouse = e->pos();
    }
    void wheelEvent(QWheelEvent *e) override {
        log << "wheel";
        lastWheel = e->angleDelta();
    }
    bool event(QEvent *e) override {
        if (e->type() == QEvent::Enter)
            log << "enter";
        else if (e->type() == QEvent::Leave)
            log << "leave";
        return QWindow::event(e);
    }
};

class PaintWindow : public QRasterWindow {
public:
    int paints = 0;
    QRegion lastRegion;
    QColor color{Qt::red};
    QImage snapshot;

protected:
    void paintEvent(QPaintEvent *e) override {
        ++paints;
        lastRegion = e->region();
        QPainter p(this);
        CHECK(p.isActive() && p.device() == this);
        p.fillRect(rect(), color);
        p.setPen(Qt::NoPen);
        p.setBrush(Qt::blue);
        p.drawRect(0, 0, 10, 10);
        p.end();
        QPoint offset;
        if (auto *image = dynamic_cast<QImage *>(redirected(&offset)))
            snapshot = *image;
    }

private:
    QRect rect() const { return QRect(0, 0, width(), height()); }
};

} // namespace

static void testStyleHints() {
    QStyleHints *hints = QGuiApplication::styleHints();
    CHECK(hints != nullptr && hints == qGuiApp->styleHints());
    CHECK(hints->colorScheme() == Qt::ColorScheme::Unknown); // the offscreen platform has no desktop
    std::vector<Qt::ColorScheme> seen;
    QObject::connect(hints, &QStyleHints::colorSchemeChanged, [&](Qt::ColorScheme scheme) { seen.push_back(scheme); });

    hints->setColorScheme(Qt::ColorScheme::Dark);
    CHECK(hints->colorScheme() == Qt::ColorScheme::Dark && seen.size() == 1 && seen[0] == Qt::ColorScheme::Dark);
    CHECK(hints->property("colorScheme").isValid());
    hints->setColorScheme(Qt::ColorScheme::Dark); // no change, no signal
    CHECK(seen.size() == 1);

    // What the desktop reports applies unless the application chose a scheme
    QWindowSystemInterface::handleColorSchemeChange(Qt::ColorScheme::Light);
    CHECK(hints->colorScheme() == Qt::ColorScheme::Dark && seen.size() == 1);
    hints->unsetColorScheme();
    CHECK(hints->colorScheme() == Qt::ColorScheme::Light && seen.size() == 2 && seen[1] == Qt::ColorScheme::Light);
    QWindowSystemInterface::handleColorSchemeChange(Qt::ColorScheme::Dark);
    CHECK(hints->colorScheme() == Qt::ColorScheme::Dark && seen.size() == 3);
    hints->setColorScheme(Qt::ColorScheme::Light);
    CHECK(seen.size() == 4 && seen[3] == Qt::ColorScheme::Light);
    hints->unsetColorScheme();
    CHECK(hints->colorScheme() == Qt::ColorScheme::Dark && seen.size() == 5);
    QWindowSystemInterface::handleColorSchemeChange(Qt::ColorScheme::Unknown);
    CHECK(hints->colorScheme() == Qt::ColorScheme::Unknown && seen.size() == 6);

    // The portal reports 1 for dark and 2 for light; 0 means no preference
    CHECK(QDesktopPortalSettings::schemeFromValue(1) == Qt::ColorScheme::Dark);
    CHECK(QDesktopPortalSettings::schemeFromValue(2) == Qt::ColorScheme::Light);
    CHECK(QDesktopPortalSettings::schemeFromValue(0) == Qt::ColorScheme::Unknown);
    CHECK(QDesktopPortalSettings::schemeFromValue(7) == Qt::ColorScheme::Unknown);
}

static void testScreens() {
    CHECK(QGuiApplication::platformName() == QString("offscreen"));
    QScreen *screen = QGuiApplication::primaryScreen();
    CHECK(screen != nullptr && QGuiApplication::screens().size() == 1 && QGuiApplication::screens().first() == screen);
    CHECK(screen->name() == QString("offscreen") && screen->depth() == 32 && screen->size() == QSize(800, 600));
    CHECK(screen->geometry() == QRect(0, 0, 800, 600) && screen->availableGeometry() == screen->geometry());
    CHECK(screen->logicalDotsPerInch() == 96.0 && screen->logicalDotsPerInchX() == 96.0 &&
          screen->logicalDotsPerInchY() == 96.0);
    CHECK(screen->devicePixelRatio() == 1.0);
    CHECK(screen->physicalSize().width() > 200 && screen->physicalSize().width() < 250);
    CHECK(screen->physicalDotsPerInch() > 90 && screen->physicalDotsPerInch() < 100);
    CHECK(screen->property("name").toString() == QString("offscreen"));
    CHECK(screen->property("depth").toInt() == 32);
}

static void testWindowLifecycle() {
    RecordingWindow w;
    CHECK(QGuiApplication::allWindows().contains(&w) && QGuiApplication::topLevelWindows().contains(&w));
    CHECK(!w.isVisible() && !w.isExposed() && !w.isActive() && w.winId() == 0 && w.handle() == nullptr);
    CHECK(w.screen() == QGuiApplication::primaryScreen() && w.isTopLevel());
    CHECK(w.geometry().isEmpty());

    int visibleChanges = 0;
    bool lastVisible = false;
    QObject::connect(&w, &QWindow::visibleChanged, [&](bool v) {
        ++visibleChanges;
        lastVisible = v;
    });
    QString title;
    QObject::connect(&w, &QWindow::titleChanged, [&](const QString &t) { title = t; });
    bool activeSeen = false;
    QObject::connect(&w, &QWindow::activeChanged, [&](bool a) { activeSeen = a; });

    w.setTitle("Hello");
    CHECK(w.title() == QString("Hello") && title == QString("Hello"));
    w.setTitle("Hello"); // unchanged
    w.setGeometry(10, 20, 300, 200);
    CHECK(w.geometry() == QRect(10, 20, 300, 200) && w.position() == QPoint(10, 20) && w.size() == QSize(300, 200));
    CHECK(w.x() == 10 && w.y() == 20 && w.width() == 300 && w.height() == 200);
    CHECK(w.log == QStringList({"move 10,20", "resize 300x200"}));
    w.log.clear();

    w.show();
    CHECK(w.isVisible() && w.isExposed() && w.winId() != 0 && w.handle() != nullptr);
    CHECK(visibleChanges == 1 && lastVisible);
    CHECK(w.log == QStringList({"show", "expose", "focusIn"}));
    CHECK(w.isActive() && activeSeen && QGuiApplication::focusWindow() == &w);
    w.show(); // already visible
    CHECK(visibleChanges == 1);

    w.log.clear();
    w.resize(320, 240);
    CHECK(w.log == QStringList({"resize 320x240", "expose"}));
    w.log.clear();
    w.setPosition(5, 6);
    CHECK(w.log == QStringList({"move 5,6", "expose"}) || w.log == QStringList({"move 5,6"}));
    w.log.clear();
    w.setX(7);
    w.setY(8);
    w.setWidth(100);
    w.setHeight(50);
    CHECK(w.geometry() == QRect(7, 8, 100, 50));
    w.setPosition(QPoint(7, 8)); // no change, no events
    w.log.clear();
    w.setPosition(QPoint(7, 8));
    w.resize(QSize(100, 50));
    CHECK(w.log.isEmpty());
    CHECK(w.mapToGlobal(QPoint(1, 1)) == QPoint(8, 9) && w.mapFromGlobal(QPoint(8, 9)) == QPoint(1, 1));

    int xs = 0, ys = 0, ws = 0, hs = 0;
    QObject::connect(&w, &QWindow::xChanged, [&](int) { ++xs; });
    QObject::connect(&w, &QWindow::yChanged, [&](int) { ++ys; });
    QObject::connect(&w, &QWindow::widthChanged, [&](int) { ++ws; });
    QObject::connect(&w, &QWindow::heightChanged, [&](int) { ++hs; });
    w.setGeometry(QRect(7, 9, 101, 50));
    CHECK(xs == 0 && ys == 1 && ws == 1 && hs == 0);

    w.log.clear();
    w.hide();
    CHECK(!w.isVisible() && !w.isExposed() && visibleChanges == 2 && !lastVisible);
    CHECK(w.log == QStringList({"hide", "unexposed", "focusOut"}) || w.log == QStringList({"hide", "focusOut"}) ||
          w.log == QStringList({"hide", "unexposed"}) || w.log == QStringList({"hide"}));
    CHECK(QGuiApplication::focusWindow() == nullptr && !w.isActive());

    w.show();
    w.log.clear();
    CHECK(w.close());
    CHECK(w.log.first() == QString("close") && !w.isVisible() && w.handle() == nullptr);
    w.log.clear();
    w.show(); // the platform window is created again
    CHECK(w.isVisible() && w.handle() != nullptr && w.isExposed());
    w.acceptClose = false;
    w.log.clear();
    CHECK(!w.close() && w.isVisible() && w.log == QStringList({"close"}));
    w.acceptClose = true;
    w.destroy();
    CHECK(!w.isVisible() && w.handle() == nullptr && w.winId() == 0);
    CHECK(w.close()); // nothing to close

    // A window that has no size gets the default one.
    RecordingWindow d;
    d.show();
    CHECK(d.size() == QSize(160, 160) && d.isExposed());
    d.hide();
    d.create();
    CHECK(d.winId() != 0 && d.handle() != nullptr);
    d.requestActivate();
    d.raise();
}

static void testInput() {
    RecordingWindow w;
    w.resize(100, 100);
    w.show();
    w.log.clear();

    QWindowSystemInterface::handleEnterEvent(&w, QPointF(1, 1), QPointF(1, 1));
    QWindowSystemInterface::handleMouseEvent(&w, QPointF(10, 20), QPointF(110, 120), Qt::LeftButton, Qt::LeftButton,
                                             QEvent::MouseButtonPress, Qt::ShiftModifier);
    CHECK(w.lastMouse == QPoint(10, 20));
    CHECK(QGuiApplication::mouseButtons() == Qt::LeftButton &&
          QGuiApplication::keyboardModifiers() == Qt::ShiftModifier);
    QWindowSystemInterface::handleMouseEvent(&w, QPointF(11, 21), QPointF(111, 121), Qt::LeftButton, Qt::NoButton,
                                             QEvent::MouseMove, Qt::NoModifier);
    CHECK(w.lastMouse == QPoint(11, 21));
    QWindowSystemInterface::handleMouseEvent(&w, QPointF(11, 21), QPointF(111, 121), Qt::LeftButton, Qt::LeftButton,
                                             QEvent::MouseButtonDblClick, Qt::NoModifier);
    QWindowSystemInterface::handleMouseEvent(&w, QPointF(11, 21), QPointF(111, 121), Qt::NoButton, Qt::LeftButton,
                                             QEvent::MouseButtonRelease, Qt::NoModifier);
    CHECK(QGuiApplication::mouseButtons() == Qt::NoButton);
    QWindowSystemInterface::handleWheelEvent(&w, QPointF(5, 5), QPointF(5, 5), QPoint(), QPoint(0, -120),
                                             Qt::ControlModifier);
    CHECK(w.lastWheel == QPoint(0, -120) && QGuiApplication::keyboardModifiers() == Qt::ControlModifier);
    QWindowSystemInterface::handleKeyEvent(&w, QEvent::KeyPress, Qt::Key_B, Qt::NoModifier, "b");
    CHECK(w.lastKey == Qt::Key_B && w.lastText == QString("b"));
    QWindowSystemInterface::handleKeyEvent(&w, QEvent::KeyRelease, Qt::Key_B, Qt::NoModifier);
    QWindowSystemInterface::handleLeaveEvent(&w);
    CHECK(w.log == QStringList({"enter", "mousePress", "mouseMove", "mouseDoubleClick", "mouseRelease", "wheel",
                                "keyPress", "keyRelease", "leave"}));

    // A close request from the window system closes the window; the window can veto it.
    w.log.clear();
    w.acceptClose = false;
    QWindowSystemInterface::handleCloseEvent(&w);
    CHECK(w.isVisible() && w.log == QStringList({"close"}));
    w.acceptClose = true;
    QWindowSystemInterface::handleCloseEvent(&w);
    CHECK(!w.isVisible());
}

static void testFocus() {
    RecordingWindow a, b;
    a.resize(10, 10);
    b.resize(10, 10);
    int focusChanges = 0;
    QWindow *last = nullptr;
    QObject::connect(qGuiApp, &QGuiApplication::focusWindowChanged, [&](QWindow *w) {
        ++focusChanges;
        last = w;
    });
    a.show();
    CHECK(QGuiApplication::focusWindow() == &a && focusChanges == 1 && last == &a);
    b.show();
    CHECK(QGuiApplication::focusWindow() == &b && !a.isActive() && b.isActive() && focusChanges == 2);
    CHECK(a.log.contains("focusOut") && b.log.contains("focusIn"));
    a.hide(); // not the focus window: focus stays
    CHECK(QGuiApplication::focusWindow() == &b);
    b.hide();
    CHECK(QGuiApplication::focusWindow() == nullptr && last == nullptr);
    a.show();
    {
        RecordingWindow temp;
        temp.show();
        CHECK(QGuiApplication::focusWindow() == &temp);
    } // destroyed while it has the focus
    CHECK(QGuiApplication::focusWindow() == nullptr);
    QWindowSystemInterface::handleWindowActivated(&a);
    QWindowSystemInterface::handleWindowActivated(&a); // no change
    QObject::disconnect(qGuiApp, nullptr, nullptr, nullptr);
}

static void testPaintWindow() {
    PaintWindow w;
    w.resize(40, 30);
    CHECK(w.paintEngine() == nullptr); // no backing store before the first paint
    w.show();                          // the expose paints at once
    CHECK(w.paints == 1 && w.lastRegion.boundingRect() == QRect(0, 0, 40, 30));
    CHECK(w.snapshot.size() == QSize(40, 30));
    CHECK(w.snapshot.pixel(20, 20) == 0xffff0000u && w.snapshot.pixel(5, 5) == 0xff0000ffu);
    CHECK(w.paintEngine() != nullptr);
    CHECK(w.width() == 40 && w.height() == 30 && w.QPaintDevice::width() == 40 && w.QPaintDevice::height() == 30);
    CHECK(w.depth() == 32 && w.devicePixelRatio() == 1.0 && w.logicalDpiX() == 96);

    // update() paints from the event loop, once however often it is called
    w.color = Qt::green;
    w.update(QRect(0, 0, 5, 5));
    w.update(QRegion(10, 10, 5, 5));
    CHECK(w.paints == 1);
    QCoreApplication::sendPostedEvents(&w);
    CHECK(w.paints == 2);
    CHECK(w.lastRegion.boundingRect() == QRect(0, 0, 15, 15));
    CHECK(w.snapshot.pixel(20, 20) == 0xff00ff00u);
    w.update();
    QCoreApplication::sendPostedEvents(&w);
    CHECK(w.paints == 3 && w.lastRegion.boundingRect() == QRect(0, 0, 40, 30));

    // A resize repaints with a backing store of the new size
    w.resize(60, 20);
    QCoreApplication::sendPostedEvents(&w);
    CHECK(w.paints >= 4 && w.snapshot.size() == QSize(60, 20));

    // A hidden window does not paint
    w.hide();
    const int before = w.paints;
    w.update();
    QCoreApplication::sendPostedEvents(&w);
    CHECK(w.paints == before);

    // Painting from the event loop
    PaintWindow v;
    v.resize(20, 20);
    v.show();
    QEventLoop loop;
    v.color = Qt::yellow;
    v.update();
    QTimer::singleShot(0, [&] { loop.quit(); });
    loop.exec();
    CHECK(v.snapshot.pixel(5, 5) == 0xff0000ffu && v.snapshot.pixel(15, 15) == 0xffffff00u);
}

static void testBackingStore() {
    QWindow w;
    w.resize(30, 20);
    w.show();
    QBackingStore store(&w);
    CHECK(store.window() == &w && store.size() == QSize());
    store.resize(QSize(30, 20));
    CHECK(store.size() == QSize(30, 20));
    auto *image = dynamic_cast<QImage *>(store.paintDevice());
    CHECK(image != nullptr && image->size() == QSize(30, 20));
    store.beginPaint(QRegion(0, 0, 30, 20));
    {
        QPainter p(store.paintDevice());
        CHECK(p.isActive());
        p.fillRect(0, 0, 30, 20, Qt::magenta);
    }
    store.endPaint();
    store.flush(QRegion(0, 0, 30, 20));
    store.flush(QRegion(0, 0, 1, 1), &w, QPoint(1, 1));
    CHECK(image->pixel(3, 3) == 0xffff00ffu);
    store.resize(QSize(30, 20)); // same size: contents stay
    CHECK(dynamic_cast<QImage *>(store.paintDevice())->pixel(3, 3) == 0xffff00ffu);
    store.resize(QSize(10, 10));
    CHECK(dynamic_cast<QImage *>(store.paintDevice())->size() == QSize(10, 10));
}

static void testQuitOnLastWindowClosed() {
    CHECK(QGuiApplication::quitOnLastWindowClosed());
    int lastClosed = 0;
    QObject::connect(qGuiApp, &QGuiApplication::lastWindowClosed, [&] { ++lastClosed; });
    QWindow a, b;
    a.show();
    b.show();
    a.hide();
    CHECK(lastClosed == 0);
    b.hide();
    CHECK(lastClosed == 1);

    // quit() from a closing last window stops a running loop
    QGuiApplication::setQuitOnLastWindowClosed(false);
    CHECK(!QGuiApplication::quitOnLastWindowClosed());
    QWindow c;
    c.show();
    QEventLoop loop;
    bool timedOut = false;
    QTimer::singleShot(0, [&] { c.close(); });
    QTimer::singleShot(100, [&] {
        timedOut = true;
        loop.quit();
    });
    loop.exec();
    CHECK(timedOut); // quitting is switched off
    QGuiApplication::setQuitOnLastWindowClosed(true);
    QWindow d;
    d.show();
    timedOut = false;
    QEventLoop loop2;
    QTimer::singleShot(0, [&] { d.close(); });
    QTimer::singleShot(2000, [&] {
        timedOut = true;
        loop2.quit();
    });
    loop2.exec();
    CHECK(!timedOut);
    QObject::disconnect(qGuiApp, nullptr, nullptr, nullptr);
}

static void testPlatformRegistry() {
    const QStringList names = QtPrivate::availablePlatforms();
    CHECK(names.contains("offscreen"));
    int argc = 0;
    CHECK(QtPrivate::createPlatformIntegration("no-such-platform", argc, nullptr) == nullptr);
}

static void testMetaObject() {
    QWindow w;
    CHECK(w.metaObject()->indexOfProperty("title") >= 0 && w.metaObject()->indexOfProperty("visible") >= 0);
    CHECK(w.setProperty("title", "via property") && w.title() == QString("via property"));
    CHECK(w.setProperty("width", 123) && w.width() == 123);
    CHECK(w.property("visible").toBool() == false);
    CHECK(qGuiApp->property("quitOnLastWindowClosed").toBool());
    QRasterWindow rw;
    CHECK(QByteArray(rw.metaObject()->className()) == QByteArray("QRasterWindow"));
    CHECK(QByteArray(rw.metaObject()->superClass()->className()) == QByteArray("QPaintDeviceWindow"));
    CHECK(QByteArray(rw.metaObject()->superClass()->superClass()->className()) == QByteArray("QWindow"));
}

static bool haveRealFonts() { return QFontMetrics(QFont("sans-serif", 10)).inFont(QChar('A')); }

static void testFontValues() {
    QFont f;
    CHECK(f.family() == QString("Sans Serif") && f.pointSize() == 10 && f.pixelSize() == -1);
    CHECK(f.weight() == QFont::Normal && !f.bold() && !f.italic());
    f.setFamily("Foo");
    f.setPointSizeF(12.5);
    CHECK(f.family() == QString("Foo") && f.pointSizeF() == 12.5 && f.pointSize() == 13);
    f.setPixelSize(20);
    CHECK(f.pixelSize() == 20 && f.pointSize() == -1 && f.pointSizeF() == -1.0);
    f.setPointSize(9);
    CHECK(f.pixelSize() == -1 && f.pointSize() == 9);
    f.setPointSize(0);  // ignored
    f.setPixelSize(-3); // ignored
    CHECK(f.pointSize() == 9 && f.pixelSize() == -1);
    f.setBold(true);
    f.setItalic(true);
    CHECK(f.bold() && f.italic() && f.weight() == QFont::Bold);
    f.setBold(false);
    CHECK(!f.bold() && f.weight() == QFont::Normal);
    f.setWeight(QFont::DemiBold);
    CHECK(f.weight() == QFont::DemiBold && !f.bold());
    f.setWeight(QFont::Black);
    CHECK(f.bold());

    QFont g("Bar", 14, QFont::Bold, true);
    CHECK(g.family() == QString("Bar") && g.pointSize() == 14 && g.bold() && g.italic());
    CHECK(QFont("x", 10, 75).weight() == QFont::Bold && QFont("x", 10, 50).weight() == QFont::Normal);
    CHECK(QFont("x", 10, 25).weight() == QFont::Light && QFont("x", 10, 0).weight() == QFont::Thin);
    CHECK(QFont("x", 10, 63).weight() == QFont::DemiBold && QFont("x", 10, 87).weight() == QFont::Black);
    CHECK(QFont("x", 10, 81).weight() == QFont::ExtraBold && QFont("x", 10, 12).weight() == QFont::ExtraLight);
    CHECK(QFont("x", 10, 57).weight() == QFont::Medium && QFont("x", 10, 600).weight() == QFont::DemiBold);
    CHECK(QFont("x").pointSize() == -1);
    CHECK(QFont("a", 10) == QFont("a", 10) && QFont("a", 10) != QFont("a", 11) && QFont("a", 10) != QFont("b", 10));

    const QFont original = QGuiApplication::font();
    QGuiApplication::setFont(QFont("Zed", 17));
    CHECK(QFont().family() == QString("Zed") && QFont().pointSize() == 17);
    QGuiApplication::setFont(original);
    CHECK(QFont() == original);
}

// Number of pixels that differ from white, and their bounding box.
static int inkCount(const QImage &img, QRect *box = nullptr) {
    int count = 0;
    QRect r;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            if (img.pixel(x, y) != 0xffffffffu) {
                ++count;
                r = r.isNull() ? QRect(x, y, 1, 1) : r.united(QRect(x, y, 1, 1));
            }
    if (box)
        *box = r;
    return count;
}

static void testFontMetrics() {
    const QFont font("sans-serif", 12);
    QFontMetrics fm(font);
    CHECK(fm.ascent() > 0 && fm.descent() >= 0 && fm.height() == fm.ascent() + fm.descent());
    CHECK(fm.leading() >= 0 && fm.lineSpacing() == fm.height() + fm.leading());
    CHECK(fm.xHeight() > 0 && fm.xHeight() < fm.ascent() && fm.averageCharWidth() > 0);
    CHECK(fm.inFont(QChar('A')) && !fm.inFont(QChar(0xfdd0)));
    CHECK(fm.height() > 12 && fm.height() < 30); // 12 points are 16 pixels

    CHECK(fm.horizontalAdvance("") == 0);
    CHECK(fm.horizontalAdvance("iii") < fm.horizontalAdvance("WWW"));
    CHECK(fm.horizontalAdvance("abc", 2) == fm.horizontalAdvance("ab"));
    CHECK(fm.horizontalAdvance("abc", 0) == 0);
    CHECK(qAbs(fm.horizontalAdvance("ab") - fm.horizontalAdvance(QChar('a')) - fm.horizontalAdvance(QChar('b'))) <= 1);
    CHECK(fm.horizontalAdvance("a\tb") > fm.horizontalAdvance("a b")); // a tab is wide
    CHECK(fm.horizontalAdvance("a\x01"
                               "b") == fm.horizontalAdvance("ab")); // control characters have no width
    QFontMetrics copy(fm);
    QFontMetrics assigned(QFont("sans-serif", 30));
    assigned = copy;
    CHECK(copy.height() == fm.height() && assigned.height() == fm.height());

    QFont big = font;
    big.setPointSize(24);
    CHECK(QFontMetrics(big).height() > fm.height() &&
          QFontMetrics(big).horizontalAdvance("Hello") > fm.horizontalAdvance("Hello"));
    QFont px;
    px.setPixelSize(40);
    CHECK(QFontMetrics(px).ascent() + QFontMetrics(px).descent() >= 40);
    QFont bold = font;
    bold.setBold(true);
    CHECK(QFontMetrics(bold).horizontalAdvance("Hello world") >= fm.horizontalAdvance("Hello world"));
    // an unknown family falls back to a real font
    CHECK(QFontMetrics(QFont("No Such Family 12345", 12)).height() > 0);
    CHECK(QFontMetrics(QFont("", 12)).height() > 0);

    const QRect br = fm.boundingRect("Hello");
    CHECK(br.x() == 0 && br.y() == -fm.ascent() && br.height() == fm.height() &&
          br.width() == fm.horizontalAdvance("Hello"));
    const QSize two = fm.size(0, "ab\ncdef");
    CHECK(two.width() == fm.horizontalAdvance("cdef") && two.height() == 2 * fm.lineSpacing() - fm.leading());
    CHECK(fm.size(Qt::TextSingleLine, "ab\ncd").height() == fm.height());
    CHECK(fm.size(0, "").height() == fm.height() && fm.size(0, "").width() == 0);

    const QString longText = "the quick brown fox jumps over the lazy dog";
    const QRect wrapped = fm.boundingRect(QRect(0, 0, 80, 0), Qt::TextWordWrap, longText);
    CHECK(wrapped.width() <= 80 && wrapped.height() >= 3 * fm.height());
    const QRect unwrapped = fm.boundingRect(QRect(0, 0, 80, 0), 0, longText);
    CHECK(unwrapped.height() == fm.height() && unwrapped.width() > 80);
    const QRect longWord = fm.boundingRect(0, 0, 40, 0, Qt::TextWordWrap, "Supercalifragilisticexpialidocious");
    CHECK(longWord.height() == fm.height()); // an unbreakable word stays on one line
    const QRect anywhere =
        fm.boundingRect(QRect(0, 0, 40, 0), Qt::TextWrapAnywhere, "Supercalifragilisticexpialidocious");
    CHECK(anywhere.width() <= 40 && anywhere.height() > fm.height());
    const QRect right = fm.boundingRect(QRect(0, 0, 200, 50), Qt::AlignRight | Qt::AlignBottom, "Hi");
    CHECK(right.right() == 199 && right.bottom() == 49);
    const QRect centered = fm.boundingRect(QRect(0, 0, 200, 50), Qt::AlignCenter, "Hi");
    CHECK(qAbs(centered.center().x() - 100) <= 2 && qAbs(centered.center().y() - 25) <= 2);

    // eliding
    const QString name = "A very long file name for a test.txt";
    CHECK(fm.elidedText(name, Qt::ElideRight, 10000) == name);
    CHECK(fm.elidedText(name, Qt::ElideNone, 10) == name);
    for (Qt::TextElideMode mode : {Qt::ElideRight, Qt::ElideLeft, Qt::ElideMiddle}) {
        const QString e = fm.elidedText(name, mode, 120);
        CHECK(fm.horizontalAdvance(e) <= 120 && e.size() < name.size() && e.size() > 3);
        if (mode == Qt::ElideRight)
            CHECK(e.startsWith("A very") && !e.endsWith("txt"));
        if (mode == Qt::ElideLeft)
            CHECK(e.endsWith("test.txt") && !e.startsWith("A very"));
        if (mode == Qt::ElideMiddle)
            CHECK(e.startsWith("A") && e.endsWith("txt"));
    }
    CHECK(fm.elidedText(name, Qt::ElideRight, 2).isEmpty());
}

static void testDrawText() {
    const QFont font("sans-serif", 12);
    const QFontMetrics fm(font);
    QImage img(160, 60, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::white);
    {
        QPainter p(&img);
        CHECK(p.font() == QFont());
        p.setFont(font);
        CHECK(p.font() == font);
        p.setPen(Qt::black);
        p.drawText(10, 30, "Hello");
    }
    QRect box;
    const int ink = inkCount(img, &box);
    CHECK(ink > 40);
    CHECK(box.left() >= 8 && box.right() <= 10 + fm.horizontalAdvance("Hello") + 2);
    CHECK(box.top() >= 30 - fm.ascent() && box.bottom() <= 30 + fm.descent());
    CHECK(box.top() < 30 && box.height() >= fm.xHeight()); // the H reaches above the baseline
    int darkest = 255;
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x)
            darkest = std::min(darkest, qRed(img.pixel(x, y)));
    CHECK(darkest < 40); // the stems of the letters are solid, and the edges are smooth
    bool partial = false;
    for (int y = 0; y < img.height() && !partial; ++y)
        for (int x = 0; x < img.width(); ++x)
            if (qRed(img.pixel(x, y)) > 60 && qRed(img.pixel(x, y)) < 200)
                partial = true;
    CHECK(partial);

    // color, opacity and translation
    QImage colored(160, 60, QImage::Format_RGB32);
    colored.fill(Qt::white);
    {
        QPainter p(&colored);
        p.setFont(font);
        p.setPen(QColor(255, 0, 0));
        p.translate(20, 0);
        p.drawText(QPoint(0, 30), "Hello");
    }
    QRect cbox;
    CHECK(inkCount(colored, &cbox) > 40 && qAbs(cbox.left() - 20) <= 3 && cbox.top() >= 30 - fm.ascent());
    bool reddish = true;
    for (int y = 0; y < colored.height(); ++y)
        for (int x = 0; x < colored.width(); ++x) {
            const QRgb c = colored.pixel(x, y);
            if (qGreen(c) != qBlue(c) || qRed(c) < qGreen(c))
                reddish = false;
        }
    CHECK(reddish);
    QImage faint(160, 60, QImage::Format_ARGB32);
    faint.fill(Qt::white);
    {
        QPainter p(&faint);
        p.setFont(font);
        p.setOpacity(0.25);
        p.drawText(10, 30, "Hello");
    }
    int faintest = 255;
    for (int y = 0; y < faint.height(); ++y)
        for (int x = 0; x < faint.width(); ++x)
            faintest = std::min(faintest, qRed(faint.pixel(x, y)));
    CHECK(faintest > 150 && faintest < 220);

    // new lines
    QImage lines(160, 100, QImage::Format_ARGB32_Premultiplied);
    lines.fill(Qt::white);
    {
        QPainter p(&lines);
        p.setFont(font);
        p.drawText(10, 20, "one\ntwo");
    }
    QRect lbox;
    inkCount(lines, &lbox);
    CHECK(lbox.bottom() > 20 + fm.lineSpacing() - fm.ascent());

    // empty text and spaces leave no trace; control characters are not drawn
    QImage blank(60, 40, QImage::Format_ARGB32_Premultiplied);
    blank.fill(Qt::white);
    {
        QPainter p(&blank);
        p.setFont(font);
        p.drawText(5, 20, "");
        p.drawText(5, 20, "   ");
        p.drawText(5, 20, "\x01\x02");
        p.drawText(QRect(0, 0, 50, 30), Qt::AlignCenter, "");
    }
    CHECK(inkCount(blank) == 0);
    {
        QPainter p(&blank);
        p.setFont(font);
        p.drawText(5, 20, QString::fromUtf8("\xf0\x9f\x98\x80 \xc3\xa9\xe2\x82\xac")); // no crash on any code point
    }

    // rectangles: alignment and clipping
    QImage rects(200, 60, QImage::Format_ARGB32_Premultiplied);
    rects.fill(Qt::white);
    QRect shown;
    {
        QPainter p(&rects);
        p.setFont(font);
        p.drawText(QRect(0, 0, 200, 60), Qt::AlignRight | Qt::AlignBottom, "Hi", &shown);
    }
    QRect rbox;
    inkCount(rects, &rbox);
    CHECK(shown.right() == 199 && shown.bottom() == 59);
    CHECK(rbox.right() <= 199 && rbox.right() >= 190 && rbox.bottom() <= 59 && rbox.bottom() >= 40);
    rects.fill(Qt::white);
    {
        QPainter p(&rects);
        p.setFont(font);
        p.drawText(0, 0, 200, 60, Qt::AlignCenter, "Hi");
    }
    inkCount(rects, &rbox);
    CHECK(qAbs(rbox.center().x() - 100) <= 6 && qAbs(rbox.center().y() - 30) <= 8);

    const QString wide = "This text is much wider than the rectangle";
    QImage clipped(300, 40, QImage::Format_ARGB32_Premultiplied);
    clipped.fill(Qt::white);
    {
        QPainter p(&clipped);
        p.setFont(font);
        p.drawText(QRect(0, 0, 50, 40), Qt::AlignLeft | Qt::AlignVCenter, wide);
        CHECK(!p.hasClipping()); // the clip set for the call is gone again
    }
    inkCount(clipped, &rbox);
    CHECK(rbox.right() < 50);
    clipped.fill(Qt::white);
    {
        QPainter p(&clipped);
        p.setFont(font);
        p.drawText(QRect(0, 0, 50, 40), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip, wide);
    }
    inkCount(clipped, &rbox);
    CHECK(rbox.right() > 150);
    clipped.fill(Qt::white);
    {
        QPainter p(&clipped);
        p.setFont(font);
        p.setClipRect(20, 0, 100, 40); // the clip of the painter and the rectangle both apply
        p.drawText(QRect(0, 0, 60, 40), Qt::AlignLeft | Qt::AlignVCenter, wide);
        CHECK(p.hasClipping() && p.clipRegion().boundingRect() == QRect(20, 0, 100, 40));
    }
    inkCount(clipped, &rbox);
    CHECK(rbox.left() >= 20 && rbox.right() < 60);

    QImage wrapped(100, 120, QImage::Format_ARGB32_Premultiplied);
    wrapped.fill(Qt::white);
    QRect wrapBox;
    {
        QPainter p(&wrapped);
        p.setFont(font);
        p.drawText(QRect(0, 0, 100, 120), Qt::TextWordWrap, "the quick brown fox jumps over the lazy dog", &wrapBox);
        CHECK(p.boundingRect(QRect(0, 0, 100, 120), Qt::TextWordWrap, "the quick brown fox jumps over the lazy dog") ==
              wrapBox);
        CHECK(p.boundingRect(0, 0, 100, 120, Qt::TextWordWrap, "x") ==
              QRect(0, 0, fm.horizontalAdvance("x"), fm.height()));
    }
    inkCount(wrapped, &rbox);
    CHECK(wrapBox.width() <= 100 && wrapBox.height() >= 3 * fm.height() && rbox.bottom() >= 2 * fm.lineSpacing());
    CHECK(rbox.right() < 100);

    // an inactive painter draws nothing, and text uses the saved font after restore()
    QPainter idle;
    idle.drawText(1, 1, "x");
    idle.drawText(QRect(0, 0, 5, 5), 0, "x");
    QImage restored(100, 40, QImage::Format_ARGB32_Premultiplied);
    restored.fill(Qt::white);
    {
        QPainter p(&restored);
        p.setFont(font);
        p.save();
        QFont large = font;
        large.setPointSize(30);
        p.setFont(large);
        p.restore();
        CHECK(p.font() == font);
    }
    // 8 bit coverage at the edges of the image and with a clip region that has several parts
    QImage edge(30, 30, QImage::Format_RGB32);
    edge.fill(Qt::white);
    {
        QPainter p(&edge);
        QFont f2 = font;
        f2.setPixelSize(40);
        p.setFont(f2);
        p.drawText(-10, 10, "Wg");
        p.drawText(20, 40, "Wg");
        p.drawText(-50, -50, "Wg");
    }
    CHECK(inkCount(edge) > 0);
}

int main(int argc, char **argv) {
    // The tests need no display.
    ::setenv("QT_QPA_PLATFORM", "offscreen", 1);
    ::setenv("QT_SCALE_FACTOR", "1", 1);
    QGuiApplication app(argc, argv);
    testRgb();
    testColor();
    testRegion();
    testPenBrush();
    testImage();
    testPainterBasics();
    testPainterShapes();
    testPainterState();
    testPainterImages();
    testEvents();
    testScreens();
    testStyleHints();
    testWindowLifecycle();
    testInput();
    testFocus();
    testPaintWindow();
    testBackingStore();
    testQuitOnLastWindowClosed();
    testPlatformRegistry();
    testMetaObject();
    if (haveRealFonts()) {
        testFontValues();
        testFontMetrics();
        testDrawText();
    } else {
        std::puts("SKIP: no fonts for the text tests");
    }
    if (g_failures == 0)
        std::puts("all GUI tests passed");
    return g_failures == 0 ? 0 : 1;
}
