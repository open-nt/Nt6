// MIT License - see LICENSE
#include <QtOpenGL/qopenglpaintdevice.h>

#include "qopenglpaintengine_p.h"

#include <QtGui/qopenglcontext.h>

struct QOpenGLPaintDevice::Data {
    QOpenGLContext *context = QOpenGLContext::currentContext();
    QSize size;
    qreal devicePixelRatio = 1.0;
    qreal dpmx = 96.0 / 0.0254; // 96 dots per inch
    qreal dpmy = 96.0 / 0.0254;
    bool flipped = false;
    std::unique_ptr<QOpenGL2PaintEngine> engine;
};

/*!
    \class QOpenGLPaintDevice
    \inmodule NtOpenGL
    \since 6.0
    \brief The QOpenGLPaintDevice class enables painting to an OpenGL context using QPainter.

    A QPainter on the device draws with OpenGL into the framebuffer that is
    bound in the current context when painting begins: the window, or a
    QOpenGLFramebufferObject. Paths, gradients, pens, transformations, images
    and text work as on a QImage, through the same flattening and stroking
    code, so both look the same:

    \code
    context.makeCurrent(&window);
    QOpenGLPaintDevice device(window.size() * window.devicePixelRatio());
    device.setDevicePixelRatio(window.devicePixelRatio());
    QPainter painter(&device);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QLinearGradient(0, 0, 100, 100));
    painter.drawEllipse(QRectF(10, 10, 200, 120));
    \endcode

    Shapes are filled with the stencil buffer, so the target needs one (8
    bits) for shapes that are not convex and for clip regions that are not a
    single rectangle. Antialiasing comes from multisampling: ask for samples
    with QSurfaceFormat::setSamples() or
    QOpenGLFramebufferObjectFormat::setSamples(). Without samples, edges are
    aliased whatever the render hints say.

    \differences
    Without multisampling there is no antialiasing. Images are uploaded as
    textures each time they are drawn. The composition modes are those of Nt's
    QPainter. There is no \c beginNativePainting(): the painter leaves
    blending, stencil and scissor tests off, and no program, buffer or texture
    bound, when it ends.
    \enddifferences

    \sa QOpenGLWindow, QOpenGLFramebufferObject, QOpenGLContext
*/

/*!
    Constructs a paint device of no size for the current context.
*/
QOpenGLPaintDevice::QOpenGLPaintDevice() : m_d(std::make_unique<Data>()) {}

/*!
    Constructs a paint device of \a size, in pixels, for the current context.
*/
QOpenGLPaintDevice::QOpenGLPaintDevice(const QSize &size) : m_d(std::make_unique<Data>()) { m_d->size = size; }

/*!
    Constructs a paint device of \a width by \a height pixels for the current context.
*/
QOpenGLPaintDevice::QOpenGLPaintDevice(int width, int height) : QOpenGLPaintDevice(QSize(width, height)) {}

/*!
    Destroys the paint device.
*/
QOpenGLPaintDevice::~QOpenGLPaintDevice() = default;

/*!
    \reimp
*/
QPaintEngine *QOpenGLPaintDevice::paintEngine() const {
    if (!m_d->engine)
        m_d->engine = std::make_unique<QOpenGL2PaintEngine>();
    return m_d->engine.get();
}

/*!
    Returns the context that was current when the device was constructed.
*/
QOpenGLContext *QOpenGLPaintDevice::context() const { return m_d->context; }

/*!
    Returns the size of the device in pixels.
*/
QSize QOpenGLPaintDevice::size() const { return m_d->size; }

/*!
    Sets the size of the device to \a size, in pixels. Change it when the
    target changes size.
*/
void QOpenGLPaintDevice::setSize(const QSize &size) { m_d->size = size; }

/*!
    Sets the device pixel ratio to \a devicePixelRatio: a painter scales its
    coordinates by it.
*/
void QOpenGLPaintDevice::setDevicePixelRatio(qreal devicePixelRatio) {
    m_d->devicePixelRatio = devicePixelRatio > 0 ? devicePixelRatio : 1.0;
}

/*!
    Returns the horizontal resolution in dots per meter.
*/
qreal QOpenGLPaintDevice::dotsPerMeterX() const { return m_d->dpmx; }

/*!
    Returns the vertical resolution in dots per meter.
*/
qreal QOpenGLPaintDevice::dotsPerMeterY() const { return m_d->dpmy; }

/*!
    Sets the horizontal resolution to \a dpmx dots per meter.
*/
void QOpenGLPaintDevice::setDotsPerMeterX(qreal dpmx) { m_d->dpmx = dpmx; }

/*!
    Sets the vertical resolution to \a dpmy dots per meter.
*/
void QOpenGLPaintDevice::setDotsPerMeterY(qreal dpmy) { m_d->dpmy = dpmy; }

/*!
    Paints upside down if \a flipped is \c true: the first row is then at the
    bottom of the framebuffer, as in OpenGL, rather than at the top.
*/
void QOpenGLPaintDevice::setPaintFlipped(bool flipped) { m_d->flipped = flipped; }

/*!
    Returns \c true if painting is upside down.
*/
bool QOpenGLPaintDevice::paintFlipped() const { return m_d->flipped; }

/*!
    Called when a painter begins on the device: make the right context current
    and bind the framebuffer to paint into. The default does nothing.
*/
void QOpenGLPaintDevice::ensureActiveTarget() {}

/*!
    \reimp
*/
int QOpenGLPaintDevice::metric(QPaintDevice::PaintDeviceMetric metric) const {
    switch (metric) {
    case PdmWidth:
        return m_d->size.width();
    case PdmHeight:
        return m_d->size.height();
    case PdmDepth:
        return 32;
    case PdmNumColors:
        return 0;
    case PdmWidthMM:
        return qRound(m_d->size.width() * 1000 / m_d->dpmx);
    case PdmHeightMM:
        return qRound(m_d->size.height() * 1000 / m_d->dpmy);
    case PdmDpiX:
        return qRound(m_d->dpmx * 0.0254);
    case PdmDpiY:
        return qRound(m_d->dpmy * 0.0254);
    case PdmDevicePixelRatio:
        return static_cast<int>(m_d->devicePixelRatio);
    case PdmDevicePixelRatioScaled:
        return qRound(m_d->devicePixelRatio * 65536.0);
    }
    return QPaintDevice::metric(metric);
}
