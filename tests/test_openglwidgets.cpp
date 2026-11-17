// MIT License - see LICENSE
//
// Tests for QOpenGLWidget on the offscreen platform: OpenGL asked about before the application exists, initialization, resizing, grabbing, a QPainter inside paintGL(),
// multisampling, an OpenGL 3.3 core profile scene drawn with QOpenGLExtraFunctions and a vertex array object, and the
// frame put on the widget's window. Skipped when there is no OpenGL.
#include <QtCore/qelapsedtimer.h>
#include <QtGui/QtGui>
#include <QtOpenGL/QtOpenGL>
#include <QtOpenGLWidgets/QtOpenGLWidgets>
#include <QtWidgets/QtWidgets>

#include <cstdio>
#include <cstdlib>
#include <functional>

// See tests/test_opengl.cpp: the OpenGL drivers keep their own allocations to the end of the process.
#if defined(__SANITIZE_ADDRESS__)
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

static bool waitFor(const std::function<bool()> &done, int ms = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < ms)
        QCoreApplication::processEvents();
    return done();
}

// Clears to blue and paints a yellow square in the top left quarter with QPainter.
class Widget : public QOpenGLWidget, protected QOpenGLFunctions {
public:
    using QOpenGLWidget::QOpenGLWidget;
    int initialized = 0;
    int resizes = 0;
    int paints = 0;
    QSize lastResize;
    GLint boundInPaint = -1;

protected:
    void initializeGL() override {
        ++initialized;
        CHECK(QOpenGLContext::currentContext() == context());
        initializeOpenGLFunctions();
        glClearColor(20 / 255.0f, 40 / 255.0f, 160 / 255.0f, 1.0f);
    }
    void resizeGL(int w, int h) override {
        ++resizes;
        lastResize = QSize(w, h);
    }
    void paintGL() override {
        ++paints;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &boundInPaint);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        QPainter p(this);
        p.fillRect(QRect(0, 0, width() / 2, height() / 2), QColor(240, 200, 30));
    }
};

static void testWidget() {
    Widget w;
    CHECK(!w.isValid());
    CHECK(w.context() == nullptr);
    CHECK(w.defaultFramebufferObject() == 0);
    CHECK(w.updateBehavior() == QOpenGLWidget::NoPartialUpdate);
    w.setUpdateBehavior(QOpenGLWidget::PartialUpdate);
    CHECK(w.updateBehavior() == QOpenGLWidget::PartialUpdate);
    CHECK(w.textureFormat() == 0);
    w.setTextureFormat(GL_RGBA8);
    CHECK(w.textureFormat() == GL_RGBA8);
    w.resize(64, 48);

    // A widget that is not shown can be grabbed.
    const QImage image = w.grabFramebuffer();
    CHECK(w.isValid());
    CHECK(w.context() != nullptr && w.defaultFramebufferObject() != 0);
    CHECK(w.initialized == 1 && w.resizes == 1 && w.paints == 1);
    CHECK(w.lastResize == QSize(64, 48));
    CHECK(static_cast<GLuint>(w.boundInPaint) == w.defaultFramebufferObject());
    CHECK(image.size() == QSize(64, 48));
    CHECK(image.format() == QImage::Format_RGB32);
    if (image.size() == QSize(64, 48)) {
        CHECK(color(image.pixel(5, 5), 240, 200, 30));  // the painter's square, at the top left
        CHECK(color(image.pixel(50, 40), 20, 40, 160)); // the clear color
        CHECK(color(image.pixel(50, 5), 20, 40, 160));  // right of the square
        CHECK(color(image.pixel(5, 40), 20, 40, 160));  // below the square
        CHECK(qAlpha(image.pixel(50, 40)) == 255);
    }

    w.makeCurrent();
    CHECK(QOpenGLContext::currentContext() == w.context());
    GLint bound = 0;
    w.context()->functions()->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound);
    CHECK(static_cast<GLuint>(bound) == w.defaultFramebufferObject());
    w.doneCurrent();
    CHECK(QOpenGLContext::currentContext() == nullptr);

    // A resize makes a new framebuffer and calls resizeGL().
    int aboutToResize = 0, resized = 0;
    QObject::connect(&w, &QOpenGLWidget::aboutToResize, &w, [&aboutToResize] { ++aboutToResize; });
    QObject::connect(&w, &QOpenGLWidget::resized, &w, [&resized] { ++resized; });
    w.resize(32, 30);
    const QImage smaller = w.grabFramebuffer();
    CHECK(smaller.size() == QSize(32, 30));
    CHECK(w.lastResize == QSize(32, 30));
    CHECK(w.resizes == 2);
    CHECK(aboutToResize == resized);
    CHECK(w.initialized == 1);

    // The format cannot change once the context exists.
    QSurfaceFormat other;
    other.setSamples(8);
    w.setFormat(other);
    CHECK(w.format().samples() != 8);

    // The context goes with the widget.
    auto *heap = new Widget;
    heap->resize(8, 8);
    CHECK(!heap->grabFramebuffer().isNull());
    bool destroyed = false;
    QObject::connect(heap->context(), &QOpenGLContext::aboutToBeDestroyed, heap->context(),
                     [&destroyed] { destroyed = true; });
    delete heap;
    CHECK(destroyed);
}

static void testShown() {
    QWidget window;
    window.resize(100, 80);
    auto *gl = new Widget(&window);
    gl->setGeometry(20, 10, 60, 40);
    int composed = 0, swapped = 0;
    QObject::connect(gl, &QOpenGLWidget::aboutToCompose, gl, [&composed] { ++composed; });
    QObject::connect(gl, &QOpenGLWidget::frameSwapped, gl, [&swapped] { ++swapped; });
    window.show();
    CHECK(waitFor([&] { return swapped > 0; }));
    CHECK(composed == swapped);
    CHECK(gl->isValid() && gl->initialized == 1);
    const int before = gl->paints;
    gl->update();
    CHECK(waitFor([&] { return gl->paints > before; }));

    // What the widget showed, rendered with the rest of the window.
    QImage shot(window.size(), QImage::Format_ARGB32_Premultiplied);
    shot.fill(Qt::black);
    window.render(&shot);
    CHECK(color(shot.pixel(25, 15), 240, 200, 30)); // the square, at the top left of the widget
    CHECK(color(shot.pixel(75, 45), 20, 40, 160));  // the clear color, at the bottom right of the widget
    CHECK(qBlue(shot.pixel(5, 5)) != 160);          // outside the widget
}

static void testMultisampled() {
    Widget w;
    QSurfaceFormat format;
    format.setSamples(4);
    w.setFormat(format);
    CHECK(w.format().samples() == 4);
    w.resize(40, 40);
    const QImage image = w.grabFramebuffer();
    CHECK(image.size() == QSize(40, 40));
    if (QOpenGLFramebufferObject::hasOpenGLFramebufferBlit())
        CHECK(w.format().samples() > 0);
    if (!image.isNull()) {
        CHECK(color(image.pixel(5, 5), 240, 200, 30));
        CHECK(color(image.pixel(35, 35), 20, 40, 160));
    }
}

// What ptz-mount does: an OpenGL 3.3 core profile, QOpenGLExtraFunctions, a full screen triangle from an empty
// vertex array object, an index-buffered quad, and a QPainter overlay on top.
class CoreWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions {
public:
    QOpenGLShaderProgram background;
    QOpenGLShaderProgram mesh;
    QOpenGLVertexArrayObject emptyVao;
    QOpenGLVertexArrayObject quadVao;
    QOpenGLBuffer vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer ibo{QOpenGLBuffer::IndexBuffer};
    bool ready = false;

    ~CoreWidget() override {
        makeCurrent();
        emptyVao.destroy();
        quadVao.destroy();
        vbo.destroy();
        ibo.destroy();
        background.removeAllShaders();
        mesh.removeAllShaders();
        doneCurrent();
    }

protected:
    void initializeGL() override {
        initializeOpenGLFunctions();
        ready = background.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                                   "#version 330 core\n"
                                                   "void main() {\n"
                                                   "    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
                                                   "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
                                                   "}\n") &&
                background.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                                   "#version 330 core\n"
                                                   "out vec4 color;\n"
                                                   "void main() { color = vec4(0.0, 0.5, 0.0, 1.0); }\n") &&
                background.link();
        ready = ready &&
                mesh.addShaderFromSourceCode(QOpenGLShader::Vertex,
                                             "#version 330 core\n"
                                             "layout(location = 0) in vec2 aPos;\n"
                                             "void main() { gl_Position = vec4(aPos, 0.0, 1.0); }\n") &&
                mesh.addShaderFromSourceCode(QOpenGLShader::Fragment,
                                             "#version 330 core\n"
                                             "out vec4 color;\n"
                                             "void main() { color = vec4(1.0, 0.0, 0.0, 1.0); }\n") &&
                mesh.link();
        CHECK(ready);
        CHECK(emptyVao.create());
        // A quad over the right half of the widget.
        const GLfloat quad[] = {0.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f};
        const GLuint order[] = {0, 1, 2, 0, 2, 3};
        quadVao.create();
        QOpenGLVertexArrayObject::Binder binder(&quadVao);
        vbo.create();
        vbo.bind();
        vbo.allocate(quad, static_cast<int>(sizeof quad));
        ibo.create();
        ibo.bind();
        ibo.allocate(order, static_cast<int>(sizeof order));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    }
    void paintGL() override {
        if (!ready)
            return;
        glDisable(GL_DEPTH_TEST);
        background.bind();
        {
            QOpenGLVertexArrayObject::Binder binder(&emptyVao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        background.release();
        mesh.bind();
        {
            QOpenGLVertexArrayObject::Binder binder(&quadVao);
            glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, 1);
        }
        mesh.release();
        glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
        QPainter p(this);
        p.fillRect(QRect(2, 2, 6, 6), Qt::white);
    }
};

static void testCoreProfile() {
    CoreWidget w;
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setSamples(4);
    w.setFormat(format);
    w.resize(64, 32);
    const QImage image = w.grabFramebuffer();
    if (!w.isValid() || w.format().majorVersion() < 3 || !w.ready) {
        std::puts("SKIP: no OpenGL 3.3 core profile");
        return;
    }
    CHECK(w.format().profile() == QSurfaceFormat::CoreProfile);
    CHECK(image.size() == QSize(64, 32));
    if (image.size() == QSize(64, 32)) {
        CHECK(color(image.pixel(16, 20), 0, 128, 0, 4)); // the background triangle, left half
        CHECK(color(image.pixel(48, 20), 255, 0, 0));    // the instanced quad, right half
        CHECK(color(image.pixel(4, 4), 255, 255, 255));  // the painter, at the top left
    }
    CHECK(w.context()->functions()->glGetError() == GL_NO_ERROR);
}

int main(int argc, char *argv[]) {
    // As ptz-mount does: the kind of OpenGL is asked for, to choose the default format, before the application
    // exists. That must not keep OpenGL from working once it does.
    const QOpenGLContext::OpenGLModuleType before = QOpenGLContext::openGLModuleType();
    CHECK(before == QOpenGLContext::LibGL);
    QApplication app(argc, argv);
    {
        QOffscreenSurface surface;
        surface.create();
        QOpenGLContext probe;
        if (!surface.isValid() || !probe.create()) {
            std::puts("SKIP: no OpenGL");
            std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
            return g_failures ? 1 : 0;
        }
    }
    testWidget();
    testShown();
    testMultisampled();
    testCoreProfile();
    std::printf("%s\n", g_failures ? "FAILED" : "PASSED");
    return g_failures ? 1 : 0;
}
