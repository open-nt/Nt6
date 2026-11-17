// MIT License - see LICENSE
#include <QtOpenGLWidgets/qopenglwidget.h>

#include <QtCore/qdebug.h>
#include <QtCore/qpointer.h>
#include <QtCore/qtenvironmentvariables.h>
#include <QtGui/qevent.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpaintdevicewindow.h>
#include <QtGui/qoffscreensurface.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>
#include <QtGui/qpainter.h>
#include <QtOpenGL/qopenglframebufferobject.h>
#include <QtOpenGL/qopenglpaintdevice.h>

#include <algorithm>

#include "../gui/qwindow_p.h"

namespace {

// The device a QPainter on the widget paints on inside paintGL(): the framebuffer object of the widget.
class QOpenGLWidgetPaintDevice : public QOpenGLPaintDevice {
public:
    explicit QOpenGLWidgetPaintDevice(QOpenGLWidget *widget) : m_widget(widget) {}
    void ensureActiveTarget() override { m_widget->makeCurrent(); }

private:
    QOpenGLWidget *m_widget;
};

} // namespace

struct QOpenGLWidget::Data {
    QOpenGLWidget::UpdateBehavior behavior = QOpenGLWidget::NoPartialUpdate;
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    GLenum textureFormat = 0;
    std::unique_ptr<QOffscreenSurface> surface;
    std::unique_ptr<QOpenGLContext> context;
    std::unique_ptr<QOpenGLFramebufferObject> fbo;
    std::unique_ptr<QOpenGLWidgetPaintDevice> device;
    QImage frame; // what paintGL() rendered last, read back
    bool initialized = false;
    bool failed = false; // there is no OpenGL: do not try again on every paint
    bool inPaintGL = false;
    bool resizePending = true;
    // Top-level widget on a platform with native OpenGL windows: frames go from the
    // framebuffer object to the window with a blit and a buffer swap, not read back.
    QPointer<QPaintDeviceWindow> hooked;
    bool directFailed = false;
    std::unique_ptr<QOpenGLFramebufferObject> resolve; // single-sampled copy, when a blit needs one

    QSize pixelSize(const QOpenGLWidget *w) const {
        const qreal dpr = w->devicePixelRatio() > 0 ? w->devicePixelRatio() : 1.0;
        return QSize(std::max(1, qRound(w->width() * dpr)), std::max(1, qRound(w->height() * dpr)));
    }
};

/*!
    \module NtOpenGLWidgets
    \title Nt OpenGL Widgets C++ Classes
    \brief A widget that renders with OpenGL.
    \cmaketarget Nt::OpenGLWidgets

    Nt OpenGL Widgets has QOpenGLWidget, which renders with OpenGL inside a
    user interface made with Nt Widgets. The contexts and functions it uses
    are in Nt GUI, and the buffers, shaders and framebuffer objects in Nt
    OpenGL. See \l{Using Nt OpenGL}.

    Link with \c Nt::OpenGLWidgets, which brings \c Nt::OpenGL and
    \c Nt::Widgets with it, and include \c{<QOpenGLWidget>}.
*/

/*!
    \class QOpenGLWidget
    \inmodule NtOpenGLWidgets
    \since 6.0
    \brief The QOpenGLWidget class is a widget for rendering OpenGL graphics.

    Reimplement initializeGL(), resizeGL() and paintGL(). The widget creates
    its context with format() the first time it is painted, makes it current
    with its framebuffer object bound before calling them, and puts what
    paintGL() rendered on the widget. Call update() to have another frame
    rendered:

    \code
    class View : public QOpenGLWidget, protected QOpenGLExtraFunctions {
    protected:
        void initializeGL() override {
            initializeOpenGLFunctions();
            glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
            m_vao.create();
        }

        void paintGL() override {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            QOpenGLVertexArrayObject::Binder binder(&m_vao);
            // ... draw the scene ...

            QPainter painter(this); // a HUD over the scene, drawn with OpenGL
            painter.drawText(rect(), Qt::AlignTop | Qt::AlignLeft, tr("Hello"));
        }

    private:
        QOpenGLVertexArrayObject m_vao;
    };
    \endcode

    paintGL() renders into a framebuffer object, not the window: bind
    defaultFramebufferObject() again after drawing into a framebuffer object
    of your own. A QPainter on the widget inside paintGL() draws with OpenGL
    into the same framebuffer. Ask for multisampling, a depth buffer size or
    a core profile with setFormat() before the widget is first shown, or for
    all widgets with QSurfaceFormat::setDefaultFormat(). The framebuffer
    object always has a depth and a stencil buffer.

    grabFramebuffer() renders a frame and returns it as an image, whether the
    widget shows or not, which suits screenshots and video streams.

    To free OpenGL resources, call makeCurrent() in the destructor of the
    subclass, delete them, then call doneCurrent().

    \differences
    The widget renders into a framebuffer object in a context of its own,
    current on an offscreen surface. A widget that is a window of its own,
    with no child widgets, on a platform with native OpenGL windows (xcb),
    shows each frame by blitting the framebuffer object into the window and
    swapping buffers, as QOpenGLWindow does; its window has no backing store.
    Otherwise, as inside a window of other widgets, each frame is copied from
    OpenGL into the backing store with \c glReadPixels(), which costs a read
    back per frame. Setting \c NT_OPENGLWIDGET_READBACK=1 always reads back. Contexts of different
    widgets do not share, and Qt::AA_ShareOpenGLContexts is not used. The
    framebuffer keeps its contents between frames with both update behaviors.
    Unless format() has an alpha buffer, the frame is opaque. There is no
    stereoscopic rendering (\c TargetBuffer), \c metric() or
    \c paintEngine() reimplementation.
    \enddifferences

    \sa QOpenGLWindow, QOpenGLExtraFunctions, QOpenGLFramebufferObject
*/

/*!
    \enum QOpenGLWidget::UpdateBehavior

    This enum describes what the framebuffer holds when paintGL() is called.

    \value NoPartialUpdate paintGL() redraws the whole widget. This is the default.
    \value PartialUpdate The framebuffer keeps what the previous paintGL() rendered.
*/

/*!
    Constructs a widget with the given \a parent and window flags \a f. Its
    context is created the first time it is painted or grabbed.
*/
QOpenGLWidget::QOpenGLWidget(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f), m_d(std::make_unique<Data>()) {}

/*!
    Destroys the widget, its framebuffer object and its context.
    QOpenGLContext::aboutToBeDestroyed() is emitted as the context goes.
*/
QOpenGLWidget::~QOpenGLWidget() {
    if (m_d->hooked)
        QtPrivate::GuiAccess::setPresentHook(m_d->hooked, {});
    if (m_d->initialized) {
        makeCurrent();
        m_d->device.reset();
        m_d->resolve.reset();
        m_d->fbo.reset();
        doneCurrent();
    }
    m_d->initialized = false;
    m_d->context.reset();
    m_d->surface.reset();
}

/*!
    Sets the update behavior of the widget to \a updateBehavior.
*/
void QOpenGLWidget::setUpdateBehavior(UpdateBehavior updateBehavior) { m_d->behavior = updateBehavior; }

/*!
    Returns the update behavior of the widget. The default is NoPartialUpdate.
*/
QOpenGLWidget::UpdateBehavior QOpenGLWidget::updateBehavior() const { return m_d->behavior; }

/*!
    Sets the format of the context and framebuffer of the widget to
    \a format. Call it before the widget is first shown or grabbed; it has no
    effect afterwards.

    \sa format(), QSurfaceFormat::setDefaultFormat()
*/
void QOpenGLWidget::setFormat(const QSurfaceFormat &format) {
    if (m_d->initialized) {
        qWarning("QOpenGLWidget::setFormat(): the widget has created its context already");
        return;
    }
    m_d->format = format;
}

/*!
    Returns the format of the widget: the one asked for before its context
    exists, and the context's actual format afterwards. The samples are those
    of the framebuffer object.
*/
QSurfaceFormat QOpenGLWidget::format() const {
    if (!m_d->context)
        return m_d->format;
    QSurfaceFormat actual = m_d->context->format();
    actual.setSamples(m_d->fbo ? m_d->fbo->format().samples() : m_d->format.samples());
    return actual;
}

/*!
    Returns the internal format of the color buffer of the framebuffer object,
    or 0 if it is chosen automatically.
*/
GLenum QOpenGLWidget::textureFormat() const { return m_d->textureFormat; }

/*!
    Sets the internal format of the color buffer of the framebuffer object to
    \a texFormat, such as \c GL_RGBA16F. Call it before the widget is first
    shown or grabbed. The default, 0, chooses \c GL_RGBA8.
*/
void QOpenGLWidget::setTextureFormat(GLenum texFormat) {
    if (m_d->initialized) {
        qWarning("QOpenGLWidget::setTextureFormat(): the widget has created its framebuffer already");
        return;
    }
    m_d->textureFormat = texFormat;
}

/*!
    Returns \c true once the widget has created its context and framebuffer
    object, and called initializeGL().
*/
bool QOpenGLWidget::isValid() const { return m_d->initialized && m_d->context && m_d->context->isValid(); }

/*!
    Makes the context of the widget current and binds its framebuffer object.
    The widget calls it before initializeGL(), resizeGL() and paintGL(); call
    it to use OpenGL elsewhere. It does nothing before the widget is
    initialized.

    \sa doneCurrent(), context()
*/
void QOpenGLWidget::makeCurrent() {
    if (!m_d->initialized || !m_d->context)
        return;
    // Showing frames in the window itself, the context stays current on the window:
    // moving it between a pbuffer and the window every frame leaves some drivers'
    // windows black (NVIDIA's EGL on X11 with PRIME offload).
    QSurface *target = m_d->surface.get();
    if (m_d->hooked && !m_d->directFailed && m_d->hooked == windowHandle())
        target = m_d->hooked.data();
    if (QOpenGLContext::currentContext() != m_d->context.get() || m_d->context->surface() != target) {
        if (!m_d->context->makeCurrent(target) && target != m_d->surface.get()) {
            qWarning("QOpenGLWidget: cannot render into the window; reading frames back instead");
            m_d->directFailed = true;
            QtPrivate::GuiAccess::setPresentHook(m_d->hooked, {});
            m_d->context->makeCurrent(m_d->surface.get());
        }
    }
    if (m_d->fbo)
        m_d->fbo->bind();
}

/*!
    Releases the context of the widget.

    \sa makeCurrent()
*/
void QOpenGLWidget::doneCurrent() {
    if (m_d->context)
        m_d->context->doneCurrent();
}

/*!
    Returns the context of the widget, or \c nullptr before it is initialized.
*/
QOpenGLContext *QOpenGLWidget::context() const { return m_d->initialized ? m_d->context.get() : nullptr; }

/*!
    Returns the framebuffer object that paintGL() renders into, or 0 before
    the widget is initialized. Bind it again after drawing into a framebuffer
    object of your own. It changes when the widget is resized.
*/
GLuint QOpenGLWidget::defaultFramebufferObject() const { return m_d->fbo ? m_d->fbo->handle() : 0; }

/*!
    Renders a frame with paintGL() and returns it, at the size of the widget
    in pixels, with the device pixel ratio of the widget. The widget is
    initialized first if needed, so a widget that is not shown can be grabbed.
    Returns a null image if there is no OpenGL.
*/
QImage QOpenGLWidget::grabFramebuffer() {
    if (!render())
        return QImage();
    return m_d->frame;
}

/*!
    Called once, with the context current and the framebuffer object bound,
    before the first resizeGL() and paintGL(): set up shaders, buffers and
    textures here. The default does nothing.
*/
void QOpenGLWidget::initializeGL() {}

/*!
    Called with the context current when the widget gets the size \a w by
    \a h, in the coordinates of the widget, and before the first frame. The
    default does nothing.
*/
void QOpenGLWidget::resizeGL(int w, int h) {
    (void)w;
    (void)h;
}

/*!
    Called with the context current, the framebuffer object bound and the
    viewport covering it, to render a frame. A QPainter on the widget draws
    with OpenGL. The default does nothing.

    \sa update()
*/
void QOpenGLWidget::paintGL() {}

/*!
    \reimp
    Renders a frame with paintGL() and puts it on the widget. The \a e is not
    used.
*/
void QOpenGLWidget::paintEvent(QPaintEvent *e) {
    (void)e;
    hookWindow(); // later frames of a top-level widget skip this path
    if (!render())
        return;
    emit aboutToCompose();
    {
        QPainter painter(this);
        painter.drawImage(QRectF(rect()), m_d->frame, QRectF(m_d->frame.rect()));
    }
    emit frameSwapped();
}

/*!
    \reimp
    Resizes the framebuffer object and calls resizeGL() if the widget is
    initialized; \a e goes to the base class.
*/
void QOpenGLWidget::resizeEvent(QResizeEvent *e) {
    QWidget::resizeEvent(e);
    if (!m_d->initialized)
        return;
    emit aboutToResize();
    makeCurrent();
    ensureFramebuffer();
    m_d->resizePending = false;
    resizeGL(width(), height());
    emit resized();
}

/*!
    \reimp
    Passes \a e to the base class.
*/
bool QOpenGLWidget::event(QEvent *e) { return QWidget::event(e); }

/*!
    \reimp
    Inside paintGL(), a painter on the widget paints on its framebuffer
    object with OpenGL; otherwise on the window, at the offset \a p.
*/
QPaintDevice *QOpenGLWidget::redirected(QPoint *p) const {
    if (!m_d->inPaintGL)
        return QWidget::redirected(p);
    if (!m_d->device)
        m_d->device = std::make_unique<QOpenGLWidgetPaintDevice>(const_cast<QOpenGLWidget *>(this));
    m_d->device->setSize(m_d->pixelSize(this));
    m_d->device->setDevicePixelRatio(devicePixelRatio() > 0 ? devicePixelRatio() : 1.0);
    return m_d->device.get();
}

/*!
    \reimp
    Inside paintGL() there is no clip in \a region, which covers the window,
    not the framebuffer object.
*/
bool QOpenGLWidget::systemClip(QRegion *region) const {
    if (m_d->inPaintGL)
        return false;
    return QWidget::systemClip(region);
}

bool QOpenGLWidget::initialize() {
    if (m_d->initialized)
        return true;
    if (m_d->failed)
        return false;
    auto surface = std::make_unique<QOffscreenSurface>();
    surface->setFormat(m_d->format);
    surface->create();
    auto context = std::make_unique<QOpenGLContext>();
    context->setFormat(m_d->format);
    // Showing frames in the window itself (see hookWindow()), the context is made
    // current on the window first, as QOpenGLWindow does: bound first to a pbuffer,
    // NVIDIA's EGL on X11 (PRIME offload) shows nothing in the window afterwards.
    QSurface *first = surface.get();
    if (m_d->hooked && !m_d->directFailed && m_d->hooked == windowHandle())
        first = m_d->hooked.data();
    if (!surface->isValid() || !context->create() ||
        (!context->makeCurrent(first) && !context->makeCurrent(surface.get()))) {
        qWarning("QOpenGLWidget: there is no OpenGL, so the widget stays empty");
        m_d->failed = true;
        return false;
    }
    m_d->surface = std::move(surface);
    m_d->context = std::move(context);
    m_d->initialized = true;
    ensureFramebuffer();
    if (!m_d->fbo || !m_d->fbo->isValid()) {
        qWarning("QOpenGLWidget: cannot create the framebuffer object, so the widget stays empty");
        m_d->fbo.reset();
        m_d->context->doneCurrent();
        m_d->context.reset();
        m_d->surface.reset();
        m_d->initialized = false;
        m_d->failed = true;
        return false;
    }
    const QSize px = m_d->pixelSize(this);
    m_d->context->functions()->glViewport(0, 0, px.width(), px.height());
    initializeGL();
    m_d->resizePending = true;
    return true;
}

void QOpenGLWidget::ensureFramebuffer() {
    const QSize px = m_d->pixelSize(this);
    if (m_d->fbo && m_d->fbo->size() == px)
        return;
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(std::max(0, m_d->format.samples()));
    if (m_d->textureFormat)
        format.setInternalTextureFormat(m_d->textureFormat);
    m_d->fbo = std::make_unique<QOpenGLFramebufferObject>(px, format);
    m_d->fbo->bind();
    // A new framebuffer starts transparent black, without changing the clear values of the application.
    QOpenGLFunctions *f = m_d->context->functions();
    GLfloat color[4] = {0, 0, 0, 0};
    f->glGetFloatv(GL_COLOR_CLEAR_VALUE, color);
    f->glClearColor(0, 0, 0, 0);
    f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    f->glClearColor(color[0], color[1], color[2], color[3]);
    m_d->resizePending = true;
}

bool QOpenGLWidget::render() {
    if (!renderFrame())
        return false;
    readBack();
    return true;
}

// Renders a frame with paintGL() into the framebuffer object; the context stays current on it.
bool QOpenGLWidget::renderFrame() {
    if (!initialize())
        return false;
    makeCurrent();
    ensureFramebuffer();
    if (m_d->resizePending) {
        m_d->resizePending = false;
        resizeGL(width(), height());
    }
    const QSize px = m_d->pixelSize(this);
    m_d->fbo->bind();
    m_d->context->functions()->glViewport(0, 0, px.width(), px.height());
    m_d->inPaintGL = true;
    paintGL();
    m_d->inPaintGL = false;
    makeCurrent(); // paintGL() may have released the context or bound another framebuffer
    return true;
}

// Copies the last frame from OpenGL into m_d->frame, for the backing store and grabFramebuffer().
void QOpenGLWidget::readBack() {
    QImage frame = m_d->fbo->toImage(true);
    if (!m_d->format.hasAlpha())
        frame = frame.convertToFormat(QImage::Format_RGB32);
    frame.setDevicePixelRatio(devicePixelRatio() > 0 ? devicePixelRatio() : 1.0);
    m_d->frame = frame;
}

// A widget that is a window of its own, with no child widgets, owns every pixel of
// it: it can show its frames with OpenGL, so its window does not need a backing
// store. Only where windows have native OpenGL surfaces (xcb); NT_OPENGLWIDGET_READBACK=1
// keeps the read back everywhere.
void QOpenGLWidget::hookWindow() {
    if (m_d->directFailed || !isWindow())
        return;
    auto *window = qobject_cast<QPaintDeviceWindow *>(windowHandle());
    if (!window || window == m_d->hooked)
        return;
    if (QGuiApplication::platformName() != QLatin1String("xcb") ||
        qEnvironmentVariableIntValue("NT_OPENGLWIDGET_READBACK")) {
        m_d->directFailed = true;
        return;
    }
    if (m_d->hooked)
        QtPrivate::GuiAccess::setPresentHook(m_d->hooked, {});
    window->setSurfaceType(QSurface::RasterGLSurface);
    m_d->hooked = window;
    QtPrivate::GuiAccess::setPresentHook(window, [this] { return presentDirect(); });
    window->requestUpdate(); // the next frame goes the direct way
}

// Renders a frame and shows it in the window: blit to the window's framebuffer, swap.
// Returns false, and the window paints its backing store as before, when that cannot be done.
bool QOpenGLWidget::presentDirect() {
    QWindow *window = windowHandle();
    if (m_d->directFailed || !window || !isWindow() || !isVisible())
        return false;
    for (QObject *child : children()) {
        auto *w = qobject_cast<QWidget *>(child);
        if (w && !w->isWindow() && w->isVisible())
            return false; // child widgets are painted in the backing store
    }
    if (!renderFrame())
        return false;
    emit aboutToCompose();
    QOpenGLContext *context = m_d->context.get();
    if (m_d->directFailed || context->surface() != window)
        return false; // makeCurrent() could not use the window; the backing store shows this frame
    const QSize px = m_d->pixelSize(this);
    QOpenGLFramebufferObject *source = m_d->fbo.get();
    if (source->format().samples() > 0) {
        // A multisampled framebuffer is resolved into a single-sampled one first: a
        // multisampled blit needs the same format at both ends, and the window's need not match.
        if (!m_d->resolve || m_d->resolve->size() != source->size())
            m_d->resolve = std::make_unique<QOpenGLFramebufferObject>(source->size());
        QOpenGLFramebufferObject::blitFramebuffer(m_d->resolve.get(), source);
        source = m_d->resolve.get();
    }
    // A blit, not a textured quad: it leaves the application's OpenGL state alone.
    QOpenGLFramebufferObject::blitFramebuffer(nullptr, QRect(QPoint(0, 0), px), source,
                                              QRect(QPoint(0, 0), source->size()), GL_COLOR_BUFFER_BIT,
                                              source->size() == px ? GL_NEAREST : GL_LINEAR);
    context->swapBuffers(window);
    m_d->fbo->bind();
    emit frameSwapped();
    return true;
}

#include "moc_qopenglwidget.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
