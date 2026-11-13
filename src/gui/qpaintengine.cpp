// MIT License - see LICENSE
#include <QtGui/qpaintengine.h>

/*!
    \class QPaintEngineState
    \brief The QPaintEngineState class describes the state a QPainter hands to its paint engine.

    The painter fills in the state and passes it to QPaintEngine::updateState()
    before it draws. The state is read-only for the engine, and the clip
    region is already in device coordinates.

    \differences
    There are no dirty flags: an engine receives the whole state each time.
    There is no font or background state.
    \enddifferences

    \sa QPaintEngine, QPainter
*/

/*!
    \fn QPen QPaintEngineState::pen() const

    Returns the pen.
*/

/*!
    \fn QBrush QPaintEngineState::brush() const

    Returns the brush.
*/

/*!
    \fn QRegion QPaintEngineState::clipRegion() const

    Returns the clip region in device coordinates. It only applies when
    hasClipping() is \c true.
*/

/*!
    \fn bool QPaintEngineState::hasClipping() const

    Returns \c true if drawing is clipped to clipRegion().
*/

/*!
    \fn qreal QPaintEngineState::opacity() const

    Returns the opacity, from 0.0 to 1.0.
*/

/*!
    \fn QPainter::CompositionMode QPaintEngineState::compositionMode() const

    Returns the composition mode.
*/

/*!
    \fn QTransform QPaintEngineState::transform() const

    Returns the transformation from the coordinates of paths given to QPaintEngine::drawPath() to device
    pixels. The other drawing functions receive device coordinates.
*/

/*!
    \fn QPainter::RenderHints QPaintEngineState::renderHints() const

    Returns the render hints of the painter.
*/

/*!
    \class QPaintEngine
    \brief The QPaintEngine class defines how a QPainter draws on a particular kind of paint device.

    A paint engine does the drawing for a QPainter. Each QPaintDevice returns
    the engine that suits it from QPaintDevice::paintEngine(). Nt provides one
    engine, the raster engine of QImage. The painter converts all coordinates
    to device coordinates before it calls an engine.

    \differences
    The engine interface is smaller than Qt's: rectangles, lines, points, ellipses and images in device
    coordinates, coverage masks for glyphs, and paths and rotated images with the transformation of the state. There are no
    polygons, pixmaps or text runs: QPainter draws polygons as paths.
    \enddifferences

    \sa QPainter, QPaintDevice
*/

/*!
    \enum QPaintEngine::Type

    This enum identifies the kind of an engine.

    \value Raster The software raster engine.
    \value OpenGL2 The OpenGL engine of QOpenGLPaintDevice, in the Nt OpenGL module.
    \value User The first value for engines that applications define.
    \value MaxUser The last value for engines that applications define.
*/

/*!
    Destroys the engine.
*/
QPaintEngine::~QPaintEngine() = default;

/*!
    \fn bool QPaintEngine::begin(QPaintDevice *device)

    Starts painting on \a device and returns \c true on success. The engine must
    not be active already.
*/

/*!
    \fn bool QPaintEngine::end()

    Ends painting and returns \c true on success.
*/

/*!
    \fn void QPaintEngine::updateState(const QPaintEngineState &state)

    Makes \a state the state for the drawing calls that follow.
*/

/*!
    \fn void QPaintEngine::drawRects(const QRect *rects, int rectCount)

    Draws the \a rectCount rectangles at \a rects with the current pen and brush.
*/

/*!
    \fn void QPaintEngine::drawLines(const QLine *lines, int lineCount)

    Draws the \a lineCount lines at \a lines with the current pen.
*/

/*!
    \fn void QPaintEngine::drawPoints(const QPoint *points, int pointCount)

    Draws the \a pointCount points at \a points with the current pen.
*/

/*!
    \fn void QPaintEngine::drawEllipse(const QRect &rect)

    Draws the ellipse inscribed in \a rect with the current pen and brush.
*/

/*!
    \fn void QPaintEngine::drawImage(const QRect &rect, const QImage &image, const QRect &sourceRect)

    Draws \a sourceRect of \a image scaled into \a rect.
*/

/*!
    Draws the part \a sourceRect of \a image scaled to fill \a rect, where \a rect is in the coordinates of
    paths: the transformation of state() maps it to device pixels. QPainter calls this instead of drawImage()
    when the transformation rotates or shears, so that the image is drawn rotated.

    The default implementation draws the image upright into the bounding rectangle of the transformed \a rect
    with drawImage(). Both engines of Nt reimplement it. This function is an Nt addition; Qt's engines take
    every image in the coordinates of the transformation.
*/
void QPaintEngine::drawTransformedImage(const QRectF &rect, const QImage &image, const QRectF &sourceRect) {
    const QRect device = state().transform().mapRect(rect).toRect();
    if (!device.isEmpty())
        drawImage(device, image, sourceRect.toAlignedRect());
}

/*!
    \fn void QPaintEngine::drawAlphaMask(const QPoint &topLeft, const uchar *mask, int width, int height, qsizetype stride)

    Paints the color of the current pen through \a mask, an array of \a height rows of
    \a width coverage values from 0 to 255, \a stride bytes apart, with its top left corner
    at \a topLeft. QPainter::drawText() draws each glyph this way.
*/

/*!
    \fn QPaintEngine::Type QPaintEngine::type() const

    Returns the type of the engine.
*/

/*!
    \fn bool QPaintEngine::isActive() const

    Returns \c true if the engine is painting.
*/

/*!
    \fn void QPaintEngine::setActive(bool active)

    Sets whether the engine is active to \a active.
*/

/*!
    \fn QPaintDevice *QPaintEngine::paintDevice() const

    Returns the device the engine paints on, or \c nullptr.
*/

/*!
    \fn void QPaintEngine::setPaintDevice(QPaintDevice *device)

    Sets the device that paintDevice() returns to \a device. Engines call it
    from begin() and end().
*/

/*!
    \fn QPaintEngine::QPaintEngine()

    Constructs an engine.
*/

/*!
    \fn const QPaintEngineState &QPaintEngine::state() const

    Returns the state that the painter set last.
*/

/*!
    Fills \a path with the brush and strokes it with the pen of the state, through its transformation. The
    raster engine draws it antialiased if the Antialiasing render hint is set. The default implementation
    draws the outline as lines in device coordinates.
*/
void QPaintEngine::drawPath(const QPainterPath &path) {
    const QTransform t = state().transform();
    for (const QPolygonF &poly : path.toSubpathPolygons(t)) {
        std::vector<QLine> lines;
        for (qsizetype i = 1; i < poly.size(); ++i)
            lines.emplace_back(poly.at(i - 1).toPoint(), poly.at(i).toPoint());
        if (!lines.empty())
            drawLines(lines.data(), static_cast<int>(lines.size()));
    }
}
