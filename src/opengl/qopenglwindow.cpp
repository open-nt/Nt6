// MIT License - see LICENSE
#include <QtOpenGL/qopenglwindow.h>

#include "qopenglhelpers_p.h"
#include "qopenglpaintengine_p.h"

#include <QtCore/qdebug.h>
#include <QtGui/qevent.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>
#include <QtOpenGL/qopenglframebufferobject.h>
#include <QtOpenGL/qopenglpaintdevice.h>

// The device a QPainter on the window paints on: the framebuffer of the window, or of its partial updates.
class QOpenGLWindowPaintDevice : public QOpenGLPaintDevice {
public:
    explicit QOpenGLWindowPaintDevice(QOpenGLWindow *window) : m_window(window) {}
    void ensureActiveTarget() override { m_window->makeCurrent(); }

private:
    QOpenGLWindow *m_window;
};

struct QOpenGLWindow::Data {
    QOpenGLWindow::UpdateBehavior behavior = QOpenGLWindow::NoPartialUpdate;
    QOpenGLContext *shareContext = nullptr;
    std::unique_ptr<QOpenGLContext> context;
    bool initialized = false;
    QSize lastSize;
    std::unique_ptr<QOpenGLFramebufferObject> fbo;     // partial updates paint here
    std::unique_ptr<QOpenGLFramebufferObject> resolve; // the colors of a multisampled `fbo`, as a texture
    std::unique_ptr<QOpenGLWindowPaintDevice> device;
};

/*!
    \module NtOpenGL
    \title Nt OpenGL C++ Classes
    \brief Windows that render with OpenGL, QPainter on OpenGL, and framebuffer objects.
    \cmaketarget Nt::OpenGL

    Nt OpenGL renders with OpenGL in windows and offscreen: QOpenGLWindow is a
    window that renders with OpenGL, QOpenGLPaintDevice lets QPainter draw
    with OpenGL, and QOpenGLFramebufferObject is an offscreen target. The
    contexts, functions and formats they use are in Nt GUI: QOpenGLContext,
    QOpenGLFunctions, QSurfaceFormat and QOffscreenSurface. See
    \l{Using Nt OpenGL}.

    Link with \c Nt::OpenGL and include \c{<QtOpenGL>} or the headers of the
    classes, such as \c{<QOpenGLWindow>}.
*/

/*!
    \class QOpenGLWindow
    \inmodule NtOpenGL
    \since 6.0
    \brief The QOpenGLWindow class is a convenience subclass of QWindow to perform OpenGL painting.

    Reimplement initializeGL(), resizeGL() and paintGL(). The window creates
    its context with requestedFormat(), makes it current before calling them,
    and swaps the buffers after paintGL(), emitting frameSwapped(). Call
    update() to have another frame painted:

    \snippet opengl/glwindow.cpp window

    Inside paintGL(), a QPainter on the window draws with OpenGL; see
    QOpenGLPaintDevice. Ask for samples in the format to have it antialias.

    With a swap interval of 1, the default, each swap waits for the display,
    so connecting frameSwapped() to update() renders at its refresh rate. Set
    the swap interval to 0 with QSurfaceFormat::setSwapInterval() to render
    as fast as possible, to measure:

    \code
    QSurfaceFormat format;
    format.setSwapInterval(0);
    format.setSamples(4);
    window.setFormat(format);
    QObject::connect(&window, &QOpenGLWindow::frameSwapped, &window, [&] { window.update(); });
    \endcode

    With PartialUpdateBlit or PartialUpdateBlend, paintGL() paints into a
    framebuffer object that keeps its contents between frames, which is then
    copied or blended onto the window.

    \differences
    Windows have no parent window, so \c parent is ignored. On a platform
    without a window system, such as \c offscreen, the window renders into a
    framebuffer object (see QOpenGLContext::defaultFramebufferObject()).
    \enddifferences

    \sa QOpenGLContext, QOpenGLPaintDevice, QSurfaceFormat
*/

/*!
    \enum QOpenGLWindow::UpdateBehavior

    This enum describes what the window keeps between frames.

    \value NoPartialUpdate Nothing: paintGL() paints the whole window each frame.
    \value PartialUpdateBlit paintGL() paints into a framebuffer object that keeps its contents, which is copied
           onto the window.
    \value PartialUpdateBlend As PartialUpdateBlit, but the contents are blended over what paintUnderGL() painted.
*/

/*!
    Constructs a window with \a updateBehavior. The \a parent is ignored.
*/
QOpenGLWindow::QOpenGLWindow(UpdateBehavior updateBehavior, QWindow *parent)
    : QOpenGLWindow(nullptr, updateBehavior, parent) {}

/*!
    Constructs a window with \a updateBehavior whose context shares with
    \a shareContext. The \a parent is ignored.
*/
QOpenGLWindow::QOpenGLWindow(QOpenGLContext *shareContext, UpdateBehavior updateBehavior, QWindow *parent)
    : m_d(std::make_unique<Data>()) {
    (void)parent;
    m_d->behavior = updateBehavior;
    m_d->shareContext = shareContext;
    setSurfaceType(QSurface::OpenGLSurface);
}

/*!
    Destroys the window and its context.
*/
QOpenGLWindow::~QOpenGLWindow() {
    if (m_d->context && m_d->context->isValid()) {
        makeCurrent();
        m_d->fbo.reset();
        m_d->resolve.reset();
        m_d->device.reset();
        doneCurrent();
    }
    m_d->context.reset();
}

/*!
    Returns the update behavior given to the constructor.
*/
QOpenGLWindow::UpdateBehavior QOpenGLWindow::updateBehavior() const { return m_d->behavior; }

/*!
    Returns \c true once the window has created its context, which it does the
    first time it is exposed.
*/
bool QOpenGLWindow::isValid() const { return m_d->context && m_d->context->isValid(); }

/*!
    Makes the context of the window current on it, and binds the framebuffer
    paintGL() paints into. The window calls it before initializeGL(),
    resizeGL() and paintGL(); call it to use OpenGL elsewhere.

    \sa doneCurrent(), context()
*/
void QOpenGLWindow::makeCurrent() {
    if (!isValid())
        return;
    m_d->context->makeCurrent(this);
    if (m_d->fbo)
        m_d->fbo->bind();
}

/*!
    Releases the context of the window.
*/
void QOpenGLWindow::doneCurrent() {
    if (isValid())
        m_d->context->doneCurrent();
}

/*!
    Returns the context of the window, or \c nullptr before it is first exposed.
*/
QOpenGLContext *QOpenGLWindow::context() const { return m_d->context.get(); }

/*!
    Returns the context the window's context shares with, or \c nullptr.
*/
QOpenGLContext *QOpenGLWindow::shareContext() const { return m_d->shareContext; }

/*!
    Returns the framebuffer object paintGL() paints into: that of partial
    updates, or the default framebuffer of the context. Bind it again after
    drawing into a framebuffer object of your own.
*/
GLuint QOpenGLWindow::defaultFramebufferObject() const {
    if (m_d->fbo)
        return m_d->fbo->handle();
    return m_d->context ? m_d->context->defaultFramebufferObject() : 0;
}

/*!
    Returns what the window's framebuffer holds, read back with
    \c glReadPixels(). Without partial updates, the back buffer is undefined
    after a swap on most platforms, so call it from paintGL() or
    paintOverGL() there.
*/
QImage QOpenGLWindow::grabFramebuffer() {
    if (!isValid())
        return QImage();
    makeCurrent();
    const qreal dpr = devicePixelRatio() > 0 ? devicePixelRatio() : 1.0;
    QImage image;
    if (m_d->fbo) {
        image = m_d->fbo->toImage();
    } else {
        const QSize px(qRound(width() * dpr), qRound(height() * dpr));
        const GLuint fbo = m_d->context->defaultFramebufferObject();
        image = QtOpenGLPrivate::readFramebuffer(m_d->context.get(), fbo, px, fbo != 0 && format().samples() > 0, true);
    }
    image.setDevicePixelRatio(dpr);
    return image;
}

/*!
    Called once, with the context current, before the first resizeGL() and
    paintGL(): set up shaders, buffers and textures here. The default does
    nothing.
*/
void QOpenGLWindow::initializeGL() {}

/*!
    Called with the context current when the window gets the size \a w by
    \a h, in the coordinates of the window, and before the first frame. The
    default does nothing.
*/
void QOpenGLWindow::resizeGL(int w, int h) {
    (void)w;
    (void)h;
}

/*!
    Called with the context current to render a frame. The framebuffer and
    viewport are set; a QPainter on the window draws with OpenGL. The default
    does nothing.

    \sa update(), paintUnderGL(), paintOverGL()
*/
void QOpenGLWindow::paintGL() {}

/*!
    Called before paintGL() with the default framebuffer bound, even with
    partial updates, where it paints under the contents that paintGL() keeps.
    The default does nothing.
*/
void QOpenGLWindow::paintUnderGL() {}

/*!
    Called after paintGL(), and after the contents of partial updates were put
    on the window, with the default framebuffer bound. The default does
    nothing.
*/
void QOpenGLWindow::paintOverGL() {}

/*!
    \reimp

    Calls paintGL(). The \a event is not used.
*/
void QOpenGLWindow::paintEvent(QPaintEvent *event) {
    (void)event;
    paintGL();
}

/*!
    \reimp

    Calls resizeGL() if the window has its context, and asks for a frame. The
    \a event is not used.
*/
void QOpenGLWindow::resizeEvent(QResizeEvent *event) {
    (void)event;
    if (isValid() && m_d->initialized) {
        makeCurrent();
        const qreal dpr = devicePixelRatio() > 0 ? devicePixelRatio() : 1.0;
        m_d->lastSize = QSize(qRound(width() * dpr), qRound(height() * dpr));
        resizeGL(width(), height());
    }
    update();
}

/*!
    \reimp

    Renders a frame when the window becomes exposed. The \a event is not used.
*/
void QOpenGLWindow::exposeEvent(QExposeEvent *event) {
    (void)event;
    if (isExposed())
        render();
}

/*!
    \reimp

    Renders a frame on an update request; other events, \a event, go to the
    base class.
*/
bool QOpenGLWindow::event(QEvent *event) {
    if (event->type() == QEvent::UpdateRequest) {
        QWindow::event(event); // NOLINT(bugprone-parent-virtual-call): skip the backing store of the base class
        render();
        return true;
    }
    return QPaintDeviceWindow::event(event);
}

/*!
    \reimp
*/
int QOpenGLWindow::metric(PaintDeviceMetric metric) const { return QPaintDeviceWindow::metric(metric); }

/*!
    \reimp
*/
QPaintDevice *QOpenGLWindow::redirected(QPoint *) const {
    if (!m_d->device)
        m_d->device = std::make_unique<QOpenGLWindowPaintDevice>(const_cast<QOpenGLWindow *>(this));
    const qreal dpr = devicePixelRatio() > 0 ? devicePixelRatio() : 1.0;
    m_d->device->setSize(QSize(qRound(width() * dpr), qRound(height() * dpr)));
    m_d->device->setDevicePixelRatio(dpr);
    return m_d->device.get();
}

void QOpenGLWindow::render() {
    if (!isExposed() || width() <= 0 || height() <= 0)
        return;
    if (!m_d->context) {
        create();
        auto context = std::make_unique<QOpenGLContext>();
        context->setShareContext(m_d->shareContext);
        context->setScreen(screen());
        context->setFormat(requestedFormat());
        context->create();
        m_d->context = std::move(context);
    }
    if (!isValid() || !m_d->context->makeCurrent(this))
        return;
    QOpenGLFunctions *f = m_d->context->functions();
    const qreal dpr = devicePixelRatio() > 0 ? devicePixelRatio() : 1.0;
    const QSize px(qRound(width() * dpr), qRound(height() * dpr));
    const bool partial = m_d->behavior != NoPartialUpdate;
    if (!m_d->initialized) {
        m_d->initialized = true;
        initializeGL();
    }
    if (partial && (!m_d->fbo || m_d->fbo->size() != px)) {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        format.setSamples(std::max(0, requestedFormat().samples()));
        m_d->fbo = std::make_unique<QOpenGLFramebufferObject>(px, format);
        m_d->resolve.reset();
        if (m_d->fbo->format().samples() > 0)
            m_d->resolve = std::make_unique<QOpenGLFramebufferObject>(px);
        m_d->fbo->bind();
        f->glClearColor(0, 0, 0, 0);
        f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
    if (m_d->lastSize != px) {
        m_d->lastSize = px;
        makeCurrent();
        resizeGL(width(), height());
    }
    const GLuint defaultFbo = m_d->context->defaultFramebufferObject();
    f->glBindFramebuffer(GL_FRAMEBUFFER, defaultFbo);
    f->glViewport(0, 0, px.width(), px.height());
    paintUnderGL();
    if (partial)
        m_d->fbo->bind();
    f->glViewport(0, 0, px.width(), px.height());
    QPaintEvent paint(QRect(QPoint(0, 0), size()));
    paintEvent(&paint);
    if (partial) {
        GLuint texture = m_d->fbo->texture();
        if (m_d->resolve) {
            QOpenGLFramebufferObject::blitFramebuffer(m_d->resolve.get(), m_d->fbo.get());
            texture = m_d->resolve->texture();
        }
        f->glBindFramebuffer(GL_FRAMEBUFFER, defaultFbo);
        QOpenGL2PaintEngine::drawTexture(m_d->context.get(), texture, px, m_d->behavior == PartialUpdateBlend);
    }
    f->glBindFramebuffer(GL_FRAMEBUFFER, defaultFbo);
    f->glViewport(0, 0, px.width(), px.height());
    paintOverGL();
    m_d->context->swapBuffers(this);
    emit frameSwapped();
}

#include "moc_qopenglwindow.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
