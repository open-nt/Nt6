// MIT License - see LICENSE
//
// QVideoWidget on the offscreen platform: frames drawn in each aspect ratio mode, checked through
// pixels, the capture session finding the widget's sink, and full screen.
#include <QtCore>
#include <QtGui/qscreen.h>
#include <QtMultimedia>
#include <QtMultimediaWidgets>
#include <QtWidgets>

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

QRgb pixel(const QImage &image, int x, int y) { return reinterpret_cast<const QRgb *>(image.constScanLine(y))[x]; }

bool isColor(QRgb p, int r, int g, int b) {
    const bool ok = std::abs(qRed(p) - r) <= 2 && std::abs(qGreen(p) - g) <= 2 && std::abs(qBlue(p) - b) <= 2;
    if (!ok)
        std::printf("  pixel is %d,%d,%d, expected %d,%d,%d\n", qRed(p), qGreen(p), qBlue(p), r, g, b);
    return ok;
}

QImage render(QWidget &widget) {
    QImage image(widget.size(), QImage::Format_RGB32);
    image.fill(Qt::white);
    widget.render(&image);
    return image;
}

// A frame of size, red.
QVideoFrame redFrame(const QSize &size) {
    QImage image(size, QImage::Format_RGB32);
    image.fill(QColor(255, 0, 0));
    return QVideoFrame(image);
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    QVideoWidget widget;
    CHECK(widget.videoSink());
    CHECK(widget.aspectRatioMode() == Qt::KeepAspectRatio);
    widget.resize(100, 50);
    widget.show();

    // Before a frame: black.
    QImage image = render(widget);
    CHECK(isColor(pixel(image, 50, 25), 0, 0, 0));

    // A square frame keeps its shape: black bars left and right.
    widget.videoSink()->setVideoFrame(redFrame(QSize(10, 10)));
    CHECK(widget.sizeHint() == QSize(10, 10));
    image = render(widget);
    CHECK(isColor(pixel(image, 50, 25), 255, 0, 0));
    CHECK(isColor(pixel(image, 10, 25), 0, 0, 0));
    CHECK(isColor(pixel(image, 90, 25), 0, 0, 0));
    CHECK(isColor(pixel(image, 30, 2), 255, 0, 0));

    QList<Qt::AspectRatioMode> modes;
    QObject::connect(&widget, &QVideoWidget::aspectRatioModeChanged, [&](Qt::AspectRatioMode m) { modes.append(m); });
    widget.setAspectRatioMode(Qt::IgnoreAspectRatio);
    image = render(widget);
    CHECK(isColor(pixel(image, 10, 25), 255, 0, 0));
    CHECK(isColor(pixel(image, 98, 48), 255, 0, 0));
    widget.setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
    image = render(widget);
    CHECK(isColor(pixel(image, 2, 2), 255, 0, 0));
    widget.setAspectRatioMode(Qt::KeepAspectRatioByExpanding);
    CHECK(modes == (QList<Qt::AspectRatioMode>{Qt::IgnoreAspectRatio, Qt::KeepAspectRatioByExpanding}));

    // A new frame replaces the picture.
    QImage blue(QSize(4, 2), QImage::Format_RGB32);
    blue.fill(QColor(0, 0, 255));
    widget.videoSink()->setVideoFrame(QVideoFrame(blue));
    CHECK(widget.sizeHint() == QSize(4, 2));
    image = render(widget);
    CHECK(isColor(pixel(image, 50, 25), 0, 0, 255));

    // The session sends frames to the widget's sink.
    QMediaCaptureSession session;
    session.setVideoOutput(&widget);
    CHECK(session.videoOutput() == &widget);
    CHECK(session.videoSink() == widget.videoSink());
    QObject plain;
    session.setVideoOutput(&plain);
    CHECK(session.videoSink() == nullptr);

    // Full screen covers the screen, and goes back.
    QList<bool> full;
    QObject::connect(&widget, &QVideoWidget::fullScreenChanged, [&](bool f) { full.append(f); });
    const QRect normal = widget.geometry();
    widget.setFullScreen(true);
    CHECK(widget.isFullScreen());
    if (QScreen *screen = QGuiApplication::primaryScreen())
        CHECK(widget.geometry() == screen->geometry());
    widget.setFullScreen(false);
    CHECK(!widget.isFullScreen());
    CHECK(widget.geometry() == normal);
    CHECK(full == (QList<bool>{true, false}));

    if (g_failures) {
        std::printf("%d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("all multimedia widgets tests passed\n");
    return 0;
}
