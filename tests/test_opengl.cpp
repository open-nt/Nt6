// MIT License - see LICENSE
//
// Tests for OpenGL in Nt: QSurfaceFormat, QOpenGLContext with QOffscreenSurface, QOpenGLFunctions, framebuffer
// objects, QPainter through QOpenGLPaintDevice compared with the raster engine, and QOpenGLWindow on the offscreen
// platform. Everything after the format tests is skipped when there is no OpenGL (no EGL, or no driver).
#include <QtCore/qelapsedtimer.h>
#include <QtGui/QtGui>
#include <QtOpenGL/QtOpenGL>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>

// Under AddressSanitizer: the OpenGL drivers keep their EGL displays, dispatch tables and compiled code to the end
// of the process, and some call through code they generate, which only the slow unwinder can walk. These defaults
// apply to this test alone; the leaks of Nt are still reported.
#if defined(__SANITIZE_ADDRESS__)
// The sanitizer runtime looks these up by name; declared first for -Wmissing-declarations.
extern "C" const char *__asan_default_options();
extern "C" const char *__lsan_default_suppressions();
extern "C" const char *__asan_default_options() { return "fast_unwind_on_malloc=0"; }
extern "C" const char *__lsan_default_suppressions() {
    return "leak:libEGL_mesa\nleak:libgallium\nleak:libGLX_mesa\nleak:libEGL_nvidia\nleak:libnvidia-\n"
           "leak:libGLdispatch\nleak:libdrm\nleak:libdbus-1\nleak:libxcb\nleak:libX11\n";
}
#endif

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

static bool color(QRgb p, int r, int g, int b, int tolerance = 3) {
    const bool ok = std::abs(qRed(p) - r) <= tolerance && std::abs(qGreen(p) - g) <= tolerance &&
                    std::abs(qBlue(p) - b) <= tolerance;
    if (!ok)
        std::printf("  pixel is (%d, %d, %d, %d), expected (%d, %d, %d)\n", qRed(p), qGreen(p), qBlue(p), qAlpha(p), r,
                    g, b);
    return ok;
}

// The share of pixels that differ by more than `tolerance` in a channel.
static double difference(const QImage &a, const QImage &b, int tolerance = 8) {
    if (a.size() != b.size())
        return 1.0;
    long differ = 0;
    for (int y = 0; y < a.height(); ++y)
        for (int x = 0; x < a.width(); ++x) {
            const QRgb p = a.pixel(x, y), q = b.pixel(x, y);
            if (std::abs(qRed(p) - qRed(q)) > tolerance || std::abs(qGreen(p) - qGreen(q)) > tolerance ||
                std::abs(qBlue(p) - qBlue(q)) > tolerance || std::abs(qAlpha(p) - qAlpha(q)) > tolerance)
                ++differ;
        }
    return static_cast<double>(differ) / (a.width() * a.height());
}

// A scene with every kind of drawing: integer primitives, paths with both fill rules, pens with caps and dashes,
// gradients, a transformation, an image, a clip region and text.
static void scene(QPainter &p) {
    p.fillRect(QRect(0, 0, 200, 150), Qt::white);
    p.setPen(QPen(Qt::black, 3));
    p.setBrush(QColor(200, 60, 60));
    p.drawRect(10, 10, 50, 30);
    p.drawLine(70, 10, 120, 40);
    p.drawEllipse(QRect(130, 10, 50, 30));

    QPainterPath star;
    star.moveTo(40, 50);
    for (int i = 1; i < 5; ++i)
        star.lineTo(40 + 30 * std::sin(i * 4 * M_PI / 5), 80 - 30 * std::cos(i * 4 * M_PI / 5));
    star.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(40, 90, 200));
    p.drawPath(star);

    QLinearGradient linear(90, 50, 190, 110);
    linear.setColorAt(0, QColor(250, 200, 40));
    linear.setColorAt(1, QColor(40, 160, 90));
    p.fillRect(QRectF(90, 50, 100, 30), linear);
    QRadialGradient radial(QPointF(140, 100), 20);
    radial.setColorAt(0, Qt::white);
    radial.setColorAt(1, QColor(120, 40, 160));
    p.setBrush(radial);
    p.drawEllipse(QPointF(140, 100), 20, 15);

    p.save();
    p.translate(40, 125);
    p.rotate(20);
    p.setPen(QPen(QColor(20, 20, 20), 6, Qt::DashLine, Qt::FlatCap));
    p.drawLine(QPointF(-30, 0), QPointF(30, 0));
    p.restore();

    QImage tile(8, 8, QImage::Format_ARGB32_Premultiplied);
    tile.fill(QColor(0, 200, 200));
    p.drawImage(QPoint(170, 120), tile);

    p.save();
    p.setClipRegion(QRegion(QRect(80, 115, 20, 20)) + QRegion(QRect(105, 125, 20, 20)));
    p.fillRect(QRect(70, 110, 70, 40), QColor(230, 120, 20));
    p.restore();
}

static QImage rasterScene() {
    QImage image(200, 150, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    scene(p);
    return image;
}

static void testFormat() {
    QSurfaceFormat f;
    CHECK(f.depthBufferSize() == -1 && f.stencilBufferSize() == -1 && f.samples() == -1);
    CHECK(f.majorVersion() == 2 && f.minorVersion() == 0);
    CHECK(f.swapInterval() == 1);
    CHECK(f.renderableType() == QSurfaceFormat::DefaultRenderableType);
    f.setSamples(4);
    f.setVersion(3, 3);
    f.setProfile(QSurfaceFormat::CoreProfile);
    f.setSwapInterval(0);
    f.setOption(QSurfaceFormat::DebugContext);
    CHECK(f.samples() == 4 && f.version() == std::make_pair(3, 3));
    CHECK(f.testOption(QSurfaceFormat::DebugContext) && !f.testOption(QSurfaceFormat::StereoBuffers));
    QSurfaceFormat g = f;
    CHECK(g == f);
    g.setSwapInterval(1);
    CHECK(g != f);
    const QSurfaceFormat before = QSurfaceFormat::defaultFormat();
    QSurfaceFormat::setDefaultFormat(f);
    CHECK(QSurfaceFormat::defaultFormat() == f);
    QSurfaceFormat::setDefaultFormat(before);

    QWindow window;
    CHECK(window.surfaceType() == QSurface::RasterSurface);
    CHECK(window.surfaceClass() == QSurface::Window);
    CHECK(!window.supportsOpenGL());
    window.setSurfaceType(QSurface::OpenGLSurface);
    CHECK(window.supportsOpenGL());
    window.setFormat(f);
    CHECK(window.requestedFormat() == f && window.format() == f);
}

static void testContext(QOffscreenSurface &surface, QOpenGLContext &context) {
    CHECK(context.isValid());
    CHECK(QOpenGLContext::currentContext() == nullptr);
    CHECK(context.makeCurrent(&surface));
    CHECK(QOpenGLContext::currentContext() == &context);
    CHECK(context.surface() == &surface);
    CHECK(context.defaultFramebufferObject() == 0);
    QOpenGLFunctions *f = context.functions();
    CHECK(f != nullptr);
    const char *version = reinterpret_cast<const char *>(f->glGetString(GL_VERSION));
    CHECK(version != nullptr);
    std::printf("OpenGL: %s (%s)\n", version ? version : "?",
                reinterpret_cast<const char *>(f->glGetString(GL_RENDERER)));
    CHECK(context.format().majorVersion() >= 2);
    CHECK(context.getProcAddress("glClear") != nullptr);
    CHECK(!context.extensions().isEmpty());
    CHECK(f->hasOpenGLFeature(QOpenGLFunctions::Shaders));
    QOpenGLFunctions mine;
    mine.initializeOpenGLFunctions();
    mine.glClearColor(0, 0, 0, 1);
    CHECK(mine.glGetError() == GL_NO_ERROR);

    QOpenGLContext shared;
    shared.setShareContext(&context);
    CHECK(shared.create());
    CHECK(QOpenGLContext::areSharing(&context, &shared));
    QOpenGLContext alone;
    CHECK(alone.create());
    CHECK(!QOpenGLContext::areSharing(&context, &alone));
    CHECK(QOpenGLContext::currentContext() == &context);

    context.doneCurrent();
    CHECK(QOpenGLContext::currentContext() == nullptr);
    CHECK(context.makeCurrent(&surface));
}

static void testFramebuffer(QOpenGLContext &context) {
    QOpenGLFunctions *f = context.functions();
    QOpenGLFramebufferObject fbo(64, 32);
    CHECK(fbo.isValid());
    CHECK(fbo.size() == QSize(64, 32));
    CHECK(fbo.texture() != 0 && fbo.handle() != 0);
    CHECK(fbo.attachment() == QOpenGLFramebufferObject::NoAttachment);
    CHECK(fbo.bind());
    CHECK(fbo.isBound());
    f->glViewport(0, 0, 64, 32);
    f->glClearColor(1.0f, 0.5f, 0.0f, 1.0f);
    f->glClear(GL_COLOR_BUFFER_BIT);
    f->glEnable(GL_SCISSOR_TEST);
    f->glScissor(0, 0, 64, 8); // the bottom rows in OpenGL, the last rows of the image
    f->glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    f->glClear(GL_COLOR_BUFFER_BIT);
    f->glDisable(GL_SCISSOR_TEST);
    const QImage image = fbo.toImage();
    CHECK(image.size() == QSize(64, 32));
    CHECK(color(image.pixel(10, 2), 255, 128, 0, 2));
    CHECK(color(image.pixel(10, 30), 0, 0, 255, 2));
    CHECK(color(fbo.toImage(false).pixel(10, 2), 0, 0, 255, 2));
    CHECK(fbo.release());
    CHECK(!fbo.isBound());

    fbo.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    CHECK(fbo.isValid() && fbo.attachment() == QOpenGLFramebufferObject::CombinedDepthStencil);

    QOpenGLFramebufferObjectFormat format;
    format.setSamples(4);
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    QOpenGLFramebufferObject msaa(QSize(16, 16), format);
    CHECK(msaa.isValid());
    if (QOpenGLFramebufferObject::hasOpenGLFramebufferBlit()) {
        CHECK(msaa.format().samples() > 0);
        CHECK(msaa.texture() == 0);
        msaa.bind();
        f->glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        f->glClear(GL_COLOR_BUFFER_BIT);
        CHECK(color(msaa.toImage().pixel(5, 5), 0, 255, 0, 2));
        QOpenGLFramebufferObject copy(QSize(16, 16));
        QOpenGLFramebufferObject::blitFramebuffer(&copy, &msaa);
        CHECK(color(copy.toImage().pixel(8, 8), 0, 255, 0, 2));
        msaa.release();
    }
}

// Paints the scene through OpenGL into a framebuffer object with `samples` and returns it.
static QImage glScene(int samples, bool antialias) {
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(samples);
    QOpenGLFramebufferObject fbo(QSize(200, 150), format);
    fbo.bind();
    QOpenGLContext::currentContext()->functions()->glClearColor(0, 0, 0, 0);
    QOpenGLContext::currentContext()->functions()->glClear(GL_COLOR_BUFFER_BIT);
    QOpenGLPaintDevice device(fbo.size());
    {
        QPainter p(&device);
        CHECK(p.isActive());
        CHECK(p.paintEngine()->type() == QPaintEngine::OpenGL2);
        p.setRenderHint(QPainter::Antialiasing, antialias);
        scene(p);
    }
    const QImage image = fbo.toImage();
    fbo.release();
    return image;
}

static void testPainting() {
    const QImage reference = rasterScene();
    const QImage gl = glScene(0, false);
    const double diff = difference(reference, gl);
    std::printf("OpenGL against raster, aliased: %.2f%% of the pixels differ\n", diff * 100);
    CHECK(diff < 0.02);
    // Places that do not depend on edges.
    CHECK(color(gl.pixel(35, 25), 200, 60, 60));     // the inside of the rectangle
    CHECK(color(gl.pixel(10, 10), 0, 0, 0));         // its pen
    CHECK(color(gl.pixel(40, 58), 40, 90, 200, 2));  // a point of the star
    CHECK(color(gl.pixel(40, 75), 255, 255, 255));   // its middle: paths fill with the odd-even rule
    CHECK(color(gl.pixel(173, 123), 0, 200, 200));   // the image
    CHECK(color(gl.pixel(85, 120), 230, 120, 20));   // inside the clip region
    CHECK(color(gl.pixel(102, 120), 255, 255, 255)); // between its rectangles
    CHECK(color(gl.pixel(110, 130), 230, 120, 20));
    CHECK(color(gl.pixel(90, 65), qRed(reference.pixel(90, 65)), qGreen(reference.pixel(90, 65)),
                qBlue(reference.pixel(90, 65)), 4)); // the linear gradient
    CHECK(color(gl.pixel(140, 100), qRed(reference.pixel(140, 100)), qGreen(reference.pixel(140, 100)),
                qBlue(reference.pixel(140, 100)), 4)); // the radial gradient

    // With samples, edges are antialiased: some pixels of a circle's edge are partly covered.
    const QImage smooth = glScene(4, true);
    const QImage smoothReference = [] {
        QImage image(200, 150, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);
        scene(p);
        return image;
    }();
    const double smoothDiff = difference(smoothReference, smooth, 40);
    std::printf("OpenGL against raster, antialiased: %.2f%% of the pixels differ\n", smoothDiff * 100);
    CHECK(smoothDiff < 0.02);
    if (smooth.size() == QSize(200, 150)) {
        int partial = 0;
        for (int x = 125; x < 165; ++x) {
            const QRgb p = smooth.pixel(x, 88);
            if (qRed(p) > 130 && qRed(p) < 245 && qGreen(p) > 50 && qGreen(p) < 245)
                ++partial;
        }
        CHECK(partial > 0);
    }
}

static void testFillRules() {
    QOpenGLFramebufferObject fbo(QSize(60, 60), QOpenGLFramebufferObject::CombinedDepthStencil);
    fbo.bind();
    QOpenGLPaintDevice device(fbo.size());
    QPainterPath star;
    star.moveTo(30, 2);
    for (int i = 1; i < 5; ++i)
        star.lineTo(30 + 28 * std::sin(i * 4 * M_PI / 5), 30 - 28 * std::cos(i * 4 * M_PI / 5));
    star.closeSubpath();
    for (const Qt::FillRule rule : {Qt::OddEvenFill, Qt::WindingFill}) {
        star.setFillRule(rule);
        {
            QPainter p(&device);
            p.fillRect(QRect(0, 0, 60, 60), Qt::white);
            p.setPen(Qt::NoPen);
            p.setBrush(Qt::black);
            p.drawPath(star);
        }
        const QImage image = fbo.toImage();
        CHECK(color(image.pixel(30, 32), rule == Qt::OddEvenFill ? 255 : 0, rule == Qt::OddEvenFill ? 255 : 0,
                    rule == Qt::OddEvenFill ? 255 : 0));
        CHECK(color(image.pixel(30, 8), 0, 0, 0)); // a point of the star
    }
    fbo.release();
}

// Draws with OpenGL into a 40 by 40 framebuffer and returns the pixels.
static QImage glPaint(const std::function<void(QPainter &)> &draw) {
    QOpenGLFramebufferObject fbo(QSize(40, 40), QOpenGLFramebufferObject::CombinedDepthStencil);
    fbo.bind();
    QOpenGLPaintDevice device(fbo.size());
    {
        QPainter p(&device);
        p.fillRect(QRect(0, 0, 40, 40), Qt::white);
        draw(p);
    }
    QImage image = fbo.toImage();
    fbo.release();
    return image;
}

static void testImages() {
    // Four quadrants: red, green, blue and black.
    QImage source(20, 20, QImage::Format_ARGB32);
    for (int y = 0; y < 20; ++y)
        for (int x = 0; x < 20; ++x)
            source.setPixel(x, y,
                            y < 10 ? (x < 10 ? qRgb(255, 0, 0) : qRgb(0, 255, 0))
                                   : (x < 10 ? qRgb(0, 0, 255) : qRgb(0, 0, 0)));

    // Rotated by 90 degrees, as the raster engine draws it.
    auto rotated = [&](QPainter &p) {
        p.translate(20, 20);
        p.rotate(90);
        p.drawImage(QPointF(-10, -10), source);
    };
    const QImage gl = glPaint(rotated);
    CHECK(color(gl.pixel(25, 15), 255, 0, 0));
    CHECK(color(gl.pixel(25, 25), 0, 255, 0));
    CHECK(color(gl.pixel(15, 15), 0, 0, 255));
    CHECK(color(gl.pixel(15, 25), 0, 0, 0));
    QImage raster(40, 40, QImage::Format_ARGB32_Premultiplied);
    raster.fill(Qt::white);
    {
        QPainter p(&raster);
        rotated(p);
    }
    CHECK(difference(raster, gl) < 0.02);

    // A part of the image, mirrored.
    const QImage mirrored = glPaint([&](QPainter &p) {
        p.translate(40, 0);
        p.scale(-1, 1);
        p.drawImage(QRectF(0, 0, 20, 20), source, QRectF(0, 0, 20, 10)); // the red and green half
    });
    CHECK(color(mirrored.pixel(35, 5), 255, 0, 0) && color(mirrored.pixel(25, 5), 0, 255, 0));

    // The same image is drawn from its cached texture, and an image that changed is uploaded again.
    QImage changing(10, 10, QImage::Format_RGB32);
    changing.fill(Qt::red);
    const QImage first = glPaint([&](QPainter &p) {
        p.drawImage(QPoint(0, 0), changing);
        p.drawImage(QPoint(20, 0), changing);
    });
    CHECK(color(first.pixel(5, 5), 255, 0, 0) && color(first.pixel(25, 5), 255, 0, 0));
    changing.fill(Qt::blue);
    const QImage second = glPaint([&](QPainter &p) { p.drawImage(QPoint(0, 0), changing); });
    CHECK(color(second.pixel(5, 5), 0, 0, 255));
    changing.setPixel(5, 5, qRgb(0, 255, 0));
    const QImage third = glPaint([&](QPainter &p) { p.drawImage(QPoint(0, 0), changing); });
    CHECK(color(third.pixel(5, 5), 0, 255, 0) && color(third.pixel(2, 2), 0, 0, 255));

    // More images than the cache holds are all drawn right: the least recently used textures make room.
    const QImage many = glPaint([&](QPainter &p) {
        for (int i = 0; i < 20; ++i) { // 80 MiB in all
            QImage big(1024, 1024, QImage::Format_RGB32);
            big.fill(QColor(i * 12, 255 - i * 12, 0));
            p.drawImage(QRect(i * 2, 30, 2, 2), big);
        }
    });
    CHECK(color(many.pixel(0, 30), 0, 255, 0) && color(many.pixel(39, 31), 228, 27, 0));
}

class GLWindow : public QOpenGLWindow {
public:
    using QOpenGLWindow::QOpenGLWindow;
    int initialized = 0;
    int resized = 0;
    int painted = 0;
    QSize lastResize;
    QImage grabbed; // the last frame, read where the back buffer is defined

    QSize pixels(int w, int h) const { return QSize(qRound(w * devicePixelRatio()), qRound(h * devicePixelRatio())); }

protected:
    void initializeGL() override {
        ++initialized;
        CHECK(QOpenGLContext::currentContext() == context());
    }
    void resizeGL(int w, int h) override {
        ++resized;
        lastResize = QSize(w, h);
    }
    void paintGL() override {
        ++painted;
        QPainter p(this);
        p.fillRect(QRect(0, 0, width(), height()), QColor(20, 40, 160));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(240, 200, 30));
        p.drawEllipse(QRectF(width() / 4.0, height() / 4.0, width() / 2.0, height() / 2.0));
    }
    void paintOverGL() override { grabbed = grabFramebuffer(); }
};

static bool waitFor(const std::function<bool()> &done, int ms = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < ms)
        QCoreApplication::processEvents();
    return done();
}

static void testWindow() {
    GLWindow window;
    CHECK(window.surfaceType() == QSurface::OpenGLSurface);
    CHECK(!window.isValid());
    QSurfaceFormat format;
    format.setSwapInterval(0);
    window.setFormat(format);
    int swapped = 0;
    QObject::connect(&window, &QOpenGLWindow::frameSwapped, &window, [&swapped] { ++swapped; });
    window.resize(120, 80);
    window.show();
    CHECK(waitFor([&] { return swapped > 0; }));
    CHECK(window.isValid());
    CHECK(window.initialized == 1);
    CHECK(window.resized >= 1 && window.lastResize == QSize(120, 80));
    CHECK(window.painted >= 1);
    CHECK(window.format().swapInterval() == 0);
    CHECK(window.format().stencilBufferSize() >= 8);
    const QImage image = window.grabbed;
    CHECK(image.size() == window.pixels(120, 80));
    if (image.size() == window.pixels(120, 80)) {
        CHECK(color(image.pixel(5, 5), 20, 40, 160));
        CHECK(color(image.pixel(image.width() / 2, image.height() / 2), 240, 200, 30));
    }
    const int before = swapped;
    window.update();
    CHECK(waitFor([&] { return swapped > before; }));
    window.resize(100, 60);
    CHECK(waitFor([&] { return window.lastResize == QSize(100, 60); }));
    CHECK(waitFor([&] { return window.grabbed.size() == window.pixels(100, 60); }));
    CHECK(window.initialized == 1);

    // Partial updates keep what was painted, and blend it over paintUnderGL().
    GLWindow partial(QOpenGLWindow::PartialUpdateBlend);
    CHECK(partial.updateBehavior() == QOpenGLWindow::PartialUpdateBlend);
    int partialSwaps = 0;
    QObject::connect(&partial, &QOpenGLWindow::frameSwapped, &partial, [&partialSwaps] { ++partialSwaps; });
    partial.resize(40, 40);
    partial.show();
    CHECK(waitFor([&] { return partialSwaps > 0; }));
    CHECK(partial.defaultFramebufferObject() != partial.context()->defaultFramebufferObject());
    const QImage kept = partial.grabbed;
    CHECK(!kept.isNull());
    if (!kept.isNull())
        CHECK(color(kept.pixel(kept.width() / 2, kept.height() / 2), 240, 200, 30));
}

// Paints the scene with a context of `format` and compares it with the raster engine.
static void testWithFormat(const char *name, const QSurfaceFormat &format, QOffscreenSurface &surface) {
    QOpenGLContext context;
    context.setFormat(format);
    if (!context.create() || !context.makeCurrent(&surface)) {
        std::printf("SKIP: no %s context\n", name);
        return;
    }
    std::printf("%s: %s\n", name, reinterpret_cast<const char *>(context.functions()->glGetString(GL_VERSION)));
    const double diff = difference(rasterScene(), glScene(0, false));
    std::printf("%s against raster: %.2f%% of the pixels differ\n", name, diff * 100);
    CHECK(diff < 0.02);
    context.doneCurrent();
}

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    testFormat();

    QOffscreenSurface surface;
    surface.create();
    QOpenGLContext context;
    if (!surface.isValid() || !context.create()) {
        std::puts("SKIP: no OpenGL");
        std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
        return g_failures ? 1 : 0;
    }
    testContext(surface, context);
    testFramebuffer(context);
    testPainting();
    testFillRules();
    testImages();
    context.doneCurrent();
    testWindow();

    QSurfaceFormat es;
    es.setRenderableType(QSurfaceFormat::OpenGLES);
    testWithFormat("OpenGL ES", es, surface);
    QSurfaceFormat core;
    core.setVersion(3, 3);
    core.setProfile(QSurfaceFormat::CoreProfile);
    testWithFormat("OpenGL 3.3 core", core, surface);

    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
