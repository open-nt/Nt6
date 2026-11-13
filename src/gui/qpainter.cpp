// MIT License - see LICENSE
#include <QtGui/qpainter.h>

#include <QtGui/qpaintdevice.h>
#include <QtGui/qpaintengine.h>

#include "qfontengine_p.h"
#include "qtextlayout_p.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

struct QPainter::Data {
    struct State {
        QPen pen;
        QBrush brush;
        QPoint translation;
        QRegion clip; // device coordinates
        bool clipping = false;
        qreal opacity = 1.0;
        CompositionMode mode = CompositionMode_SourceOver;
        RenderHints hints;
        QFont font;
        QTransform world; // the transformation of the painter's coordinates, translations included
        bool worldEnabled = true;
    };

    QPaintDevice *device = nullptr;
    QPaintEngine *engine = nullptr;
    State state;
    std::vector<State> saved;
    bool active = false;
    QPoint baseOffset;  // where painting at (0, 0) lands on the target device
    QRegion systemClip; // device coordinates; applies when hasSystemClip is set
    bool hasSystemClip = false;
    qreal scale = 1.0; // device pixels per unit of the painter's coordinates

    int dev(int v) const { return scale == 1.0 ? v : qRound(v * scale); }
    QPoint devPoint(const QPoint &p) const { return scale == 1.0 ? p : QPoint(dev(p.x()), dev(p.y())); }
    QRect devRect(const QRect &r) const {
        if (scale == 1.0)
            return r;
        const int left = dev(r.x());
        const int top = dev(r.y());
        return QRect(left, top, std::max(r.width() > 0 ? 1 : 0, dev(r.x() + r.width()) - left),
                     std::max(r.height() > 0 ? 1 : 0, dev(r.y() + r.height()) - top));
    }
    // The smallest rectangle in the painter's coordinates that covers `r`, given in device pixels.
    QRect logicalRect(const QRect &r) const {
        if (scale == 1.0 || r.isNull())
            return r;
        const int left = static_cast<int>(std::floor(r.left() / scale));
        const int top = static_cast<int>(std::floor(r.top() / scale));
        const int right = static_cast<int>(std::ceil((r.right() + 1) / scale));
        const int bottom = static_cast<int>(std::ceil((r.bottom() + 1) / scale));
        return QRect(left, top, right - left, bottom - top);
    }
    QRegion devRegion(const QRegion &region) const {
        if (scale == 1.0)
            return region;
        QRegion out;
        for (const QRect &r : region.rects())
            out += devRect(r);
        return out;
    }
    int devPenWidth(int width) const { return scale == 1.0 || width <= 0 ? width : std::max(1, qRound(width * scale)); }

    QTransform user() const { return state.worldEnabled ? state.world : QTransform(); }
    // True if the coordinates are only moved by whole pixels: integer drawing can keep its exact path.
    bool simple() const {
        const QTransform t = user();
        return t.type() <= QTransform::TxTranslate && t.dx() == std::floor(t.dx()) && t.dy() == std::floor(t.dy()) &&
               std::fabs(t.dx()) < 1e9 && std::fabs(t.dy()) < 1e9;
    }
    void updateTranslation() {
        const QTransform t = user();
        state.translation =
            simple() ? baseOffset + QPoint(static_cast<int>(t.dx()), static_cast<int>(t.dy())) : baseOffset;
    }
    QTransform toDevice() const {
        return user() * QTransform::fromTranslate(baseOffset.x(), baseOffset.y()) * QTransform::fromScale(scale, scale);
    }
    static bool fancy(const QBrush &brush) { return brush.gradient() != nullptr; }
    static bool dashed(const QPen &pen) { return pen.style() != Qt::NoPen && pen.style() != Qt::SolidLine; }

    // Draws `path`, in the painter's coordinates, with `pen` and `brush`.
    void drawPath(const QPainterPath &path, const QPen &pen, const QBrush &brush) {
        const State before = state;
        state.pen = pen;
        state.brush = brush;
        sync(true);
        engine->drawPath(path);
        state = before;
        sync();
    }

    // Fills `deviceRect` (in device pixels) with `brush`, whatever the current pen is.
    void fillDevice(const QRect &deviceRect, const QBrush &brush, CompositionMode mode, qreal opacity) {
        const State before = state;
        state.pen = QPen(Qt::NoPen);
        state.brush = brush;
        state.mode = mode;
        state.opacity = opacity;
        sync();
        engine->drawRects(&deviceRect, 1);
        state = before;
        sync();
    }

    // Hands the state to the engine. For a path, which the engine strokes through the transformation that holds the
    // device pixel ratio already, the pen keeps its width, except for a cosmetic pen; the other drawing functions
    // take device coordinates and get the width in device pixels.
    void sync(bool forPath = false) {
        QPaintEngineState s;
        s.m_pen = state.pen;
        if (scale != 1.0) {
            if (!forPath)
                s.m_pen.setWidth(devPenWidth(state.pen.width()));
            else if (state.pen.isCosmetic() && state.pen.widthF() > 0.0)
                s.m_pen.setWidthF(state.pen.widthF() * scale);
        }
        s.m_brush = state.brush;
        s.m_clip = devRegion(state.clip);
        s.m_clipping = state.clipping;
        if (hasSystemClip) {
            s.m_clip = state.clipping ? s.m_clip.intersected(devRegion(systemClip)) : devRegion(systemClip);
            s.m_clipping = true;
        }
        s.m_opacity = state.opacity;
        s.m_mode = state.mode;
        s.m_transform = toDevice();
        s.m_hints = state.hints;
        engine->m_lastState = s;
        engine->updateState(s);
    }
};

/*!
    \class QPainter
    \brief The QPainter class performs low-level painting on widgets and other paint devices.

    A painter draws shapes and images on a QPaintDevice, such as a QImage or a
    window, using its pen, brush and clipping:

    \code
    QImage image(100, 60, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setPen(QPen(Qt::darkBlue, 2));
    painter.setBrush(Qt::yellow);
    painter.drawRect(10, 10, 80, 40);
    \endcode

    The painter begins when it is constructed with a device, or when begin() is
    called, and ends in the destructor or at end(). Only one painter can
    paint on a device at a time.

    \section1 Paths, Transformations and Antialiasing

    Shapes with real coordinates, paths, polygons, arcs, pies and chords are drawn through the
    transformation of the painter (translate(), scale(), rotate(), shear(), setTransform()), with the cap,
    join and dash styles of the pen and the gradients of the brush. With the Antialiasing render hint their
    edges are smooth:

    \snippet gui/painting.cpp flower

    \image painter/flower.png A flower of Bezier petals, rotated around a center

    Shapes with integer coordinates, drawn without antialiasing and with no transformation other than a
    translation by whole pixels, are drawn on whole pixels as before: a rectangle of width 10 with a pen of
    width 1 covers 11 pixels.

    \differences
    There is no viewport or window. Text follows the transformation in position, not in rotation: it is drawn
    upright at the size of its font. Images are rotated, sheared and mirrored with the transformation. Clipping
    is by rectangles of the device: a clip under a rotation is its bounding rectangle. Text is drawn without kerning or shaping, in the color of the pen. The
    composition modes are SourceOver, Source and Clear. Pens draw with the color of their brush.
    \enddifferences

    \sa QPaintDevice, QPaintEngine, QPen, QBrush, QImage
*/

/*!
    \enum QPainter::RenderHint

    This enum describes optional drawing quality settings. The RenderHints type
    is a typedef for \c{QFlags<RenderHint>}.

    \value Antialiasing Smooth the edges of paths and of shapes with real coordinates or a transformation.
    \value TextAntialiasing Smooth the edges of text; glyphs are always smooth in Nt.
    \value SmoothPixmapTransform Use bilinear filtering when scaling images; Nt picks the nearest pixel.
*/

/*!
    \enum QPainter::CompositionMode

    This enum describes how drawn pixels combine with the pixels below them.

    \value CompositionMode_SourceOver The source is blended over the destination. This is the default.
    \value CompositionMode_Clear The destination pixels become transparent.
    \value CompositionMode_Source The source replaces the destination.
*/

/*!
    Constructs a painter that is not active.

    \sa begin()
*/
QPainter::QPainter() : d(std::make_unique<Data>()) {}

/*!
    Constructs a painter that paints on \a device, as begin() does.
*/
QPainter::QPainter(QPaintDevice *device) : d(std::make_unique<Data>()) { begin(device); }

/*!
    Destroys the painter, ending painting if it is active.
*/
QPainter::~QPainter() {
    if (d->active)
        end();
}

/*!
    Begins painting on \a device and returns \c true on success. It fails and
    returns \c false if the painter is already active, if \a device is null,
    already has a painter, or has no paint engine that can paint on it.

    \sa end(), isActive()
*/
bool QPainter::begin(QPaintDevice *device) {
    if (d->active || !device || device->paintingActive())
        return false;
    QPaintDevice *target = device;
    QPoint redirection;
    QRegion clip;
    bool clipped = false;
    for (QPaintDevice *current = device; current;) {
        QRegion part;
        if (!clipped && current->systemClip(&part)) {
            clip = part;
            clipped = true;
        }
        QPaintDevice *next = current->redirected(&redirection);
        if (next)
            target = next;
        current = next;
    }
    QPaintEngine *engine = target->paintEngine();
    if (!engine || !engine->begin(target))
        return false;
    d->device = device;
    d->engine = engine;
    d->state = Data::State();
    d->state.translation = redirection;
    d->baseOffset = redirection;
    d->systemClip = clip;
    d->hasSystemClip = clipped;
    d->scale = target->devicePixelRatio() > 0 ? target->devicePixelRatio() : 1.0;
    d->saved.clear();
    d->active = true;
    device->m_active = true;
    d->sync();
    return true;
}

/*!
    Ends painting and returns \c true if the painter was active.

    \sa begin()
*/
bool QPainter::end() {
    if (!d->active)
        return false;
    d->engine->end();
    d->device->m_active = false;
    d->active = false;
    d->engine = nullptr;
    d->device = nullptr;
    return true;
}

/*!
    Returns \c true if the painter is painting.
*/
bool QPainter::isActive() const { return d->active; }

/*!
    Returns the device the painter paints on, or \c nullptr.
*/
QPaintDevice *QPainter::device() const { return d->device; }

/*!
    Returns the paint engine that the painter uses, or \c nullptr.
*/
QPaintEngine *QPainter::paintEngine() const { return d->engine; }

/*!
    Sets the pen to \a pen.

    \sa pen(), setBrush()
*/
void QPainter::setPen(const QPen &pen) {
    d->state.pen = pen;
    if (d->active)
        d->sync();
}

/*!
    \overload

    Sets a solid pen of one pixel with the given \a color.
*/
void QPainter::setPen(const QColor &color) { setPen(QPen(color)); }

/*!
    \overload

    Sets a black pen with the given \a style.
*/
void QPainter::setPen(Qt::PenStyle style) { setPen(QPen(style)); }

/*!
    Returns the current pen.
*/
const QPen &QPainter::pen() const { return d->state.pen; }

/*!
    Sets the brush to \a brush.

    \sa brush(), setPen()
*/
void QPainter::setBrush(const QBrush &brush) {
    d->state.brush = brush;
    if (d->active)
        d->sync();
}

/*!
    \overload

    Sets a black brush with the given \a style.
*/
void QPainter::setBrush(Qt::BrushStyle style) { setBrush(QBrush(style)); }

/*!
    Returns the current brush.
*/
const QBrush &QPainter::brush() const { return d->state.brush; }

/*!
    Sets the opacity of everything drawn afterwards to \a opacity, from 0.0
    (transparent) to 1.0 (opaque). Values outside the range are clamped.

    \sa opacity()
*/
void QPainter::setOpacity(qreal opacity) {
    d->state.opacity = opacity < 0.0 ? 0.0 : (opacity > 1.0 ? 1.0 : opacity);
    if (d->active)
        d->sync();
}

/*!
    Returns the opacity.
*/
qreal QPainter::opacity() const { return d->state.opacity; }

/*!
    Sets the composition mode to \a mode.

    \sa compositionMode()
*/
void QPainter::setCompositionMode(CompositionMode mode) {
    d->state.mode = mode;
    if (d->active)
        d->sync();
}

/*!
    Returns the composition mode.
*/
QPainter::CompositionMode QPainter::compositionMode() const { return d->state.mode; }

/*!
    Sets \a hint on if \a on is \c true and off otherwise. The hints are stored and
    have no effect.

    \sa renderHints()
*/
void QPainter::setRenderHint(RenderHint hint, bool on) {
    if (on)
        d->state.hints |= hint;
    else
        d->state.hints &= ~RenderHints(hint);
}

/*!
    Sets the render hints in \a hints on if \a on is \c true and off otherwise.

    \sa setRenderHint()
*/
void QPainter::setRenderHints(RenderHints hints, bool on) {
    if (on)
        d->state.hints |= hints;
    else
        d->state.hints &= ~hints;
}

/*!
    Returns the render hints that are on.
*/
QPainter::RenderHints QPainter::renderHints() const { return d->state.hints; }

/*!
    Sets the font used by drawText() to \a font.

    \sa font()
*/
void QPainter::setFont(const QFont &font) { d->state.font = font; }

/*!
    Returns the current font.

    \sa setFont()
*/
const QFont &QPainter::font() const { return d->state.font; }

/*!
    Saves the state of the painter (pen, brush, opacity, font, translation, clipping
    and composition mode) on a stack.

    \sa restore()
*/
void QPainter::save() { d->saved.push_back(d->state); }

/*!
    Restores the state that save() stored last. Does nothing if nothing was
    saved.

    \sa save()
*/
void QPainter::restore() {
    if (d->saved.empty())
        return;
    d->state = d->saved.back();
    d->saved.pop_back();
    if (d->active)
        d->sync();
}

/*!
    Moves the origin of the coordinate system by \a offset.

    \sa translation(), resetTransform()
*/
void QPainter::translate(const QPoint &offset) { translate(QPointF(offset)); }

/*!
    \overload

    Moves the origin of the coordinate system by \a offset.
*/
void QPainter::translate(const QPointF &offset) {
    d->state.world.translate(offset.x(), offset.y());
    d->updateTranslation();
    if (d->active)
        d->sync();
}

/*!
    \fn void QPainter::translate(qreal dx, qreal dy)
    \overload

    Moves the origin by \a dx and \a dy.
*/

/*!
    Scales the coordinate system by \a sx horizontally and \a sy vertically.
*/
void QPainter::scale(qreal sx, qreal sy) {
    d->state.world.scale(sx, sy);
    d->updateTranslation();
    if (d->active)
        d->sync();
}

/*!
    Rotates the coordinate system clockwise by \a angle degrees.
*/
void QPainter::rotate(qreal angle) {
    d->state.world.rotate(angle);
    d->updateTranslation();
    if (d->active)
        d->sync();
}

/*!
    Shears the coordinate system by \a sh horizontally and \a sv vertically.
*/
void QPainter::shear(qreal sh, qreal sv) {
    d->state.world.shear(sh, sv);
    d->updateTranslation();
    if (d->active)
        d->sync();
}

/*!
    Returns the translation of the transformation, rounded to whole pixels.
*/
QPoint QPainter::translation() const { return QPoint(qRound(d->state.world.dx()), qRound(d->state.world.dy())); }

/*!
    Makes the transformation the identity.
*/
void QPainter::resetTransform() {
    d->state.world = QTransform();
    d->updateTranslation();
    if (d->active)
        d->sync();
}

/*!
    Sets the transformation of the coordinate system to \a transform, or to \a transform followed by the
    current one if \a combine is \c true.
*/
void QPainter::setTransform(const QTransform &transform, bool combine) {
    d->state.world = combine ? transform * d->state.world : transform;
    d->updateTranslation();
    if (d->active)
        d->sync();
}

/*!
    Returns the transformation of the coordinate system.
*/
const QTransform &QPainter::transform() const { return d->state.world; }

/*!
    \fn void QPainter::setWorldTransform(const QTransform &matrix, bool combine)

    Sets the transformation to \a matrix, combined with the current one if \a combine is \c true, as
    setTransform() does.
*/

/*!
    \fn const QTransform &QPainter::worldTransform() const

    Returns the transformation of the coordinate system.
*/

/*!
    \fn QTransform QPainter::combinedTransform() const

    Returns the transformation of the coordinate system; there is no window or viewport in Nt.
*/

/*!
    Returns the transformation from the painter's coordinates to device pixels: the transformation of the
    coordinate system, the redirection of the device, and its device pixel ratio.
*/
QTransform QPainter::deviceTransform() const { return d->toDevice(); }

/*!
    Uses the transformation if \a enabled is \c true, the default, and ignores it otherwise.
*/
void QPainter::setWorldMatrixEnabled(bool enabled) {
    d->state.worldEnabled = enabled;
    d->updateTranslation();
    if (d->active)
        d->sync();
}

/*!
    Returns \c true if the transformation is used; otherwise returns \c false.
*/
bool QPainter::worldMatrixEnabled() const { return d->state.worldEnabled; }

/*!
    Sets the clip to \a rect, given in the current coordinate system, and turns
    clipping on.

    \sa setClipRegion(), setClipping()
*/
void QPainter::setClipRect(const QRect &rect) { setClipRegion(QRegion(rect)); }

/*!
    \fn void QPainter::setClipRect(int x, int y, int width, int height)
    \overload

    Sets the clip to the rectangle at (\a x, \a y) that is \a width by
    \a height pixels.
*/

/*!
    Sets the clip to \a region, given in the current coordinate system, and
    turns clipping on. An empty region clips everything.

    \sa setClipRect(), clipRegion()
*/
void QPainter::setClipRegion(const QRegion &region) {
    if (d->simple()) {
        d->state.clip = region.translated(d->state.translation);
    } else {
        // Clipping is by rectangles of the device: a rotated clip is clipped to its bounding rectangles.
        const QTransform t = d->user() * QTransform::fromTranslate(d->baseOffset.x(), d->baseOffset.y());
        QRegion mapped;
        for (const QRect &r : region.rects())
            mapped += t.mapRect(QRectF(r)).toAlignedRect();
        d->state.clip = mapped;
    }
    d->state.clipping = true;
    if (d->active)
        d->sync();
}

/*!
    Returns the clip region in the current coordinate system, or an empty
    region if clipping is off.

    \sa setClipRegion()
*/
QRegion QPainter::clipRegion() const {
    return d->state.clipping ? d->state.clip.translated(-d->state.translation.x(), -d->state.translation.y())
                             : QRegion();
}

/*!
    Turns clipping on or off according to \a enable.

    \sa hasClipping()
*/
void QPainter::setClipping(bool enable) {
    d->state.clipping = enable;
    if (d->active)
        d->sync();
}

/*!
    Returns \c true if clipping is on.

    \sa setClipping()
*/
bool QPainter::hasClipping() const { return d->state.clipping; }

/*!
    Fills \a rect with \a brush, leaving the pen and the current brush alone.
*/
void QPainter::fillRect(const QRect &rect, const QBrush &brush) {
    if (!d->active || brush.style() == Qt::NoBrush)
        return;
    if (!d->simple() || Data::fancy(brush)) {
        fillRect(QRectF(rect), brush);
        return;
    }
    d->fillDevice(d->devRect(rect.translated(d->state.translation)), brush, d->state.mode, d->state.opacity);
}

/*!
    \overload

    Fills \a rect with the solid \a color.
*/
void QPainter::fillRect(const QRect &rect, const QColor &color) { fillRect(rect, QBrush(color)); }

/*!
    \overload

    Fills \a rect with the solid \a color.
*/
void QPainter::fillRect(const QRect &rect, Qt::GlobalColor color) { fillRect(rect, QBrush(color)); }

/*!
    \overload

    Fills \a rect with a black brush of the given \a style; nothing is drawn for
    Qt::NoBrush.
*/
void QPainter::fillRect(const QRect &rect, Qt::BrushStyle style) { fillRect(rect, QBrush(style)); }

/*!
    \fn void QPainter::fillRect(int x, int y, int width, int height, const QBrush &brush)
    \overload

    Fills the rectangle at (\a x, \a y) of \a width by \a height with \a brush.
*/

/*!
    \fn void QPainter::fillRect(int x, int y, int width, int height, const QColor &color)
    \overload

    Fills the rectangle at (\a x, \a y) of \a width by \a height with \a color.
*/

/*!
    \fn void QPainter::fillRect(int x, int y, int width, int height, Qt::GlobalColor color)
    \overload

    Fills the rectangle at (\a x, \a y) of \a width by \a height with \a color.
*/

/*!
    \fn void QPainter::fillRect(int x, int y, int width, int height, Qt::BrushStyle style)
    \overload

    Fills the rectangle at (\a x, \a y) of \a width by \a height with a black brush of
    the given \a style.
*/

/*!
    Erases \a rect: it becomes transparent, or black on an image without
    alpha.
*/
void QPainter::eraseRect(const QRect &rect) {
    if (!d->active)
        return;
    if (!d->simple()) {
        eraseRect(QRectF(rect));
        return;
    }
    d->fillDevice(d->devRect(rect.translated(d->state.translation)), QBrush(Qt::black), CompositionMode_Clear, 1.0);
}

/*!
    Draws \a point with the current pen.
*/
void QPainter::drawPoint(const QPoint &point) {
    if (!d->active)
        return;
    if (!d->simple()) {
        drawPoint(QPointF(point));
        return;
    }
    const QPoint p = point + d->state.translation;
    if (d->scale == 1.0) {
        d->engine->drawPoints(&p, 1);
        return;
    }
    if (d->state.pen.style() == Qt::NoPen)
        return;
    const int half = (d->state.pen.width() - 1) / 2;
    const int pw = std::max(1, d->state.pen.width());
    d->fillDevice(d->devRect(QRect(p.x() - half, p.y() - half, pw, pw)), QBrush(d->state.pen.color()), d->state.mode,
                  d->state.opacity);
}

/*!
    \fn void QPainter::drawPoint(int x, int y)
    \overload

    Draws the point (\a x, \a y).
*/

/*!
    Draws \a line with the current pen.
*/
void QPainter::drawLine(const QLine &line) {
    if (!d->active)
        return;
    if (!d->simple() || (d->state.hints & Antialiasing) || Data::dashed(d->state.pen) ||
        d->state.pen.capStyle() == Qt::RoundCap) {
        drawLine(QLineF(line));
        return;
    }
    const QLine l = line.translated(d->state.translation);
    if (d->scale == 1.0) {
        d->engine->drawLines(&l, 1);
        return;
    }
    const QPen &pen = d->state.pen;
    if (pen.style() == Qt::NoPen)
        return;
    const int pw = std::max(1, pen.width());
    const int half = (pw - 1) / 2;
    if (l.y1() == l.y2() || l.x1() == l.x2()) {
        // Axis-aligned lines cover whole pixels of the painter's coordinates; keep them exact.
        const QRect covered =
            l.y1() == l.y2()
                ? QRect(std::min(l.x1(), l.x2()) - half, l.y1() - half, std::abs(l.x2() - l.x1()) + pw, pw)
                : QRect(l.x1() - half, std::min(l.y1(), l.y2()) - half, pw, std::abs(l.y2() - l.y1()) + pw);
        d->fillDevice(d->devRect(covered), QBrush(pen.color()), d->state.mode, d->state.opacity);
        return;
    }
    // Aim at the centres of the pixels that the line passes through.
    const auto centre = [&](const QPoint &p) {
        return QPoint(qRound((p.x() + 0.5) * d->scale - 0.5), qRound((p.y() + 0.5) * d->scale - 0.5));
    };
    const QLine device(centre(l.p1()), centre(l.p2()));
    d->engine->drawLines(&device, 1);
}

/*!
    \fn void QPainter::drawLine(const QPoint &p1, const QPoint &p2)
    \overload

    Draws a line from \a p1 to \a p2.
*/

/*!
    \fn void QPainter::drawLine(int x1, int y1, int x2, int y2)
    \overload

    Draws a line from (\a x1, \a y1) to (\a x2, \a y2).
*/

/*!
    Draws \a rect, outlined with the current pen and filled with the current
    brush. With a pen of width 1 the outline covers one pixel more to the
    right and below than the width and height of \a rect.
*/
void QPainter::drawRect(const QRect &rect) {
    if (!d->active)
        return;
    if (!d->simple() || Data::fancy(d->state.brush) || (d->state.hints & Antialiasing) || Data::dashed(d->state.pen) ||
        d->state.pen.joinStyle() == Qt::RoundJoin) {
        drawRect(QRectF(rect));
        return;
    }
    const QRect r = rect.translated(d->state.translation);
    if (d->scale == 1.0 || d->state.pen.style() == Qt::NoPen) {
        const QRect device = d->devRect(r);
        d->engine->drawRects(&device, 1);
        return;
    }
    // The outline of a rectangle lies half a pen width outside it; scale the area it covers and let
    // the engine stroke back inside that.
    const int pw = std::max(1, d->state.pen.width());
    const int half = (pw - 1) / 2;
    const QRect outer = d->devRect(QRect(r.x() - half, r.y() - half, r.width() + pw, r.height() + pw));
    const int devPw = d->devPenWidth(pw);
    const int devHalf = (devPw - 1) / 2;
    const QRect device(outer.x() + devHalf, outer.y() + devHalf, outer.width() - devPw, outer.height() - devPw);
    d->engine->drawRects(&device, 1);
}

/*!
    \fn void QPainter::drawRect(int x, int y, int width, int height)
    \overload

    Draws the rectangle at (\a x, \a y) of \a width by \a height.
*/

/*!
    Draws the ellipse that fits in \a rect, outlined with the current pen and
    filled with the current brush.
*/
void QPainter::drawEllipse(const QRect &rect) {
    if (!d->active)
        return;
    if (!d->simple() || Data::fancy(d->state.brush) || (d->state.hints & Antialiasing) || Data::dashed(d->state.pen)) {
        drawEllipse(QRectF(rect));
        return;
    }
    d->engine->drawEllipse(d->devRect(rect.translated(d->state.translation)));
}

/*!
    \fn void QPainter::drawEllipse(int x, int y, int width, int height)
    \overload

    Draws the ellipse in the rectangle at (\a x, \a y) of \a width by
    \a height.
*/

/*!
    Draws \a image with its top left at \a point.
*/
void QPainter::drawImage(const QPoint &point, const QImage &image) { drawImage(point, image, QRect()); }

/*!
    \fn void QPainter::drawImage(int x, int y, const QImage &image)
    \overload

    Draws \a image with its top left at (\a x, \a y).
*/

/*!
    \fn void QPainter::drawPixmap(const QPoint &point, const QPixmap &pixmap)

    Draws \a pixmap with its top left corner at \a point.
*/

/*!
    \fn void QPainter::drawPixmap(int x, int y, const QPixmap &pixmap)

    \overload

    Draws \a pixmap with its top left corner at \a x, \a y.
*/

/*!
    \fn void QPainter::drawPixmap(const QRect &target, const QPixmap &pixmap, const QRect &source)

    \overload

    Draws the part \a source of \a pixmap, or all of it if \a source is null, scaled to fill \a target.
*/

/*!
    \fn void QPainter::drawPixmap(int x, int y, int width, int height, const QPixmap &pixmap)

    \overload

    Draws \a pixmap scaled to the rectangle at \a x, \a y of \a width by \a height.
*/

/*!
    \overload

    Draws the part \a source of \a image, scaled to fill \a target. A null
    \a source stands for the whole image. Scaling picks the nearest pixel.
*/
void QPainter::drawImage(const QRect &target, const QImage &image, const QRect &source) {
    if (!d->active || image.isNull())
        return;
    if (!d->simple()) {
        drawImage(QRectF(target), image, QRectF(source));
        return;
    }
    d->engine->drawImage(d->devRect(target.translated(d->state.translation)), image, source);
}

/*!
    \overload

    Draws the part \a source of \a image with its top left at \a point, without
    scaling.
*/
void QPainter::drawImage(const QPoint &point, const QImage &image, const QRect &source) {
    const QRect src = source.isNull() ? image.rect() : source;
    const qreal ratio = image.devicePixelRatio();
    const QSize size = ratio == 1.0 ? src.size() : QSize(qRound(src.width() / ratio), qRound(src.height() / ratio));
    drawImage(QRect(point, size), image, src);
}

namespace {

void drawLineOfText(QPaintEngine *engine, QFontEngine &font, const QtPrivate::PlacedLine &line, const QPoint &offset) {
    int pen64 = 0;
    QtPrivate::forEachCodePoint(line.text, [&](char32_t cp) {
        if (cp < 0x20 && cp != '\t')
            return;
        if (cp != '\t') {
            const QGlyph &g = font.glyph(cp);
            if (g.width > 0 && g.height > 0) {
                const QPoint topLeft(line.x + ((pen64 + 32) >> 6) + g.left, line.baseline - g.top);
                engine->drawAlphaMask(topLeft + offset, g.bitmap.data(), g.width, g.height, g.stride);
            }
        }
        pen64 += QtPrivate::advance64(font, cp);
    });
}

} // namespace

/*!
    Draws \a text with the current font and pen. \a position is the left end
    of the baseline of the first line. Each new line in \a text starts a line
    below the one before.

    \sa setFont(), QFontMetrics
*/
void QPainter::drawText(const QPoint &position, const QString &text) {
    if (!d->active || text.isEmpty())
        return;
    if (!d->simple()) {
        drawText(QPointF(position), text);
        return;
    }
    const std::shared_ptr<QFontEngine> font = QFontEngine::create(d->state.font, d->scale);
    const QtPrivate::TextBlock block =
        QtPrivate::layoutAtPoint(*font, d->devPoint(position + d->state.translation), text);
    for (const QtPrivate::PlacedLine &line : block.lines)
        drawLineOfText(d->engine, *font, line, QPoint());
}

/*!
    \fn void QPainter::drawText(int x, int y, const QString &text)
    \overload

    Draws \a text with the left end of the baseline at (\a x, \a y).
*/

/*!
    \overload

    Draws \a text in \a rectangle. The \a flags are Qt::AlignmentFlag values (left, right,
    centered, top, bottom) and Qt::TextFlag values (word wrap, single line, no clipping). Text
    is clipped to the rectangle unless Qt::TextDontClip is set. If \a boundingRect is not
    \c nullptr, it receives the rectangle that the text covers.

    \sa QFontMetrics::boundingRect()
*/
void QPainter::drawText(const QRect &rectangle, int flags, const QString &text, QRect *boundingRect) {
    if (!d->active)
        return;
    if (!d->simple()) {
        // Text is laid out in the bounding rectangle of the transformed one, upright.
        const QTransform t = d->user();
        const QRect mapped = t.mapRect(QRectF(rectangle)).toAlignedRect();
        const QTransform saved = d->state.world;
        d->state.world = QTransform();
        d->updateTranslation();
        drawText(mapped, flags, text, boundingRect);
        d->state.world = saved;
        d->updateTranslation();
        if (boundingRect)
            *boundingRect = t.inverted().mapRect(QRectF(*boundingRect)).toAlignedRect();
        return;
    }
    const std::shared_ptr<QFontEngine> font = QFontEngine::create(d->state.font, d->scale);
    const QRect target = rectangle.translated(d->state.translation);
    const QtPrivate::TextBlock block = QtPrivate::layoutText(*font, d->devRect(target), flags, text);
    if (boundingRect)
        *boundingRect = d->logicalRect(block.bounds).translated(-d->state.translation);
    if (text.isEmpty())
        return;
    const Data::State saved = d->state;
    if (!(flags & Qt::TextDontClip)) {
        d->state.clip = d->state.clipping ? d->state.clip.intersected(target) : QRegion(target);
        d->state.clipping = true;
        d->sync();
    }
    for (const QtPrivate::PlacedLine &line : block.lines)
        drawLineOfText(d->engine, *font, line, QPoint());
    d->state = saved;
    d->sync();
}

/*!
    \fn void QPainter::drawText(int x, int y, int width, int height, int flags, const QString &text, QRect *boundingRect)
    \overload

    Draws \a text in the rectangle at (\a x, \a y) of \a width by \a height.
*/

/*!
    Returns the rectangle that drawText() covers for \a text in \a rectangle with the given
    \a flags, using the current font.
*/
QRect QPainter::boundingRect(const QRect &rectangle, int flags, const QString &text) {
    const std::shared_ptr<QFontEngine> font = QFontEngine::create(d->state.font, d->scale);
    return d->logicalRect(QtPrivate::layoutText(*font, d->devRect(rectangle), flags, text).bounds);
}

/*!
    \fn QRect QPainter::boundingRect(int x, int y, int w, int h, int flags, const QString &text)
    \overload

    Returns the rectangle that drawText() covers for \a text in the rectangle at (\a x, \a y) of
    \a w by \a h.
*/

// ---- Floating-point coordinates, paths and transformations ----

/*!
    \overload

    Fills \a rect with \a brush, leaving the pen and the current brush alone.
*/
void QPainter::fillRect(const QRectF &rect, const QBrush &brush) {
    if (!d->active || brush.style() == Qt::NoBrush)
        return;
    QPainterPath path;
    path.addRect(rect);
    d->drawPath(path, QPen(Qt::NoPen), brush);
}

/*!
    \fn void QPainter::fillRect(const QRectF &rect, const QColor &color)
    \overload

    Fills \a rect with \a color.
*/

/*!
    \fn void QPainter::fillRect(const QRectF &rect, Qt::GlobalColor color)
    \overload

    Fills \a rect with \a color.
*/

/*!
    \overload

    Erases \a rect: it becomes transparent, or black on an image without alpha.
*/
void QPainter::eraseRect(const QRectF &rect) {
    if (!d->active)
        return;
    const CompositionMode mode = d->state.mode;
    d->state.mode = CompositionMode_Clear;
    fillRect(rect, QBrush(Qt::black));
    d->state.mode = mode;
    d->sync();
}

/*!
    Fills \a path with the current brush and strokes it with the current pen, through the transformation.
    With the Antialiasing render hint, edges are smooth.

    \sa fillPath(), strokePath(), setRenderHint()
*/
void QPainter::drawPath(const QPainterPath &path) {
    if (d->active)
        d->drawPath(path, d->state.pen, d->state.brush);
}

/*!
    Fills \a path with \a brush, without an outline.
*/
void QPainter::fillPath(const QPainterPath &path, const QBrush &brush) {
    if (d->active && brush.style() != Qt::NoBrush)
        d->drawPath(path, QPen(Qt::NoPen), brush);
}

/*!
    Draws the outline of \a path with \a pen, without filling it.
*/
void QPainter::strokePath(const QPainterPath &path, const QPen &pen) {
    if (d->active && pen.style() != Qt::NoPen)
        d->drawPath(path, pen, QBrush());
}

/*!
    \overload

    Draws \a point as a dot the width of the pen: round with Qt::RoundCap, square otherwise.
*/
void QPainter::drawPoint(const QPointF &point) { drawPoints(&point, 1); }

/*!
    Draws the \a pointCount points in \a points.
*/
void QPainter::drawPoints(const QPointF *points, int pointCount) {
    if (!d->active || !points || pointCount <= 0)
        return;
    const QPen &pen = d->state.pen;
    if (pen.style() == Qt::NoPen)
        return;
    const qreal w = pen.widthF() > 0 ? pen.widthF() : 1.0;
    // Aliased points cover the pixel whose top left corner is the point, as integer drawing does.
    const qreal shift = (d->state.hints & Antialiasing) ? 0.0 : 0.5;
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    for (int i = 0; i < pointCount; ++i) {
        const QPointF c = points[i] + QPointF(shift, shift);
        if (pen.capStyle() == Qt::RoundCap)
            path.addEllipse(c, w / 2.0, w / 2.0);
        else
            path.addRect(QRectF(c.x() - w / 2.0, c.y() - w / 2.0, w, w));
    }
    d->drawPath(path, QPen(Qt::NoPen), pen.brush());
}

/*!
    \overload

    Draws the \a pointCount points in \a points.
*/
void QPainter::drawPoints(const QPoint *points, int pointCount) {
    if (!points || pointCount <= 0)
        return;
    if (d->simple()) {
        for (int i = 0; i < pointCount; ++i)
            drawPoint(points[i]);
        return;
    }
    QPolygonF p;
    for (int i = 0; i < pointCount; ++i)
        p.append(QPointF(points[i]));
    drawPoints(p.constData(), pointCount);
}

/*!
    \fn void QPainter::drawPoints(const QPolygonF &points)
    \overload
    Draws \a points.
*/

/*!
    \fn void QPainter::drawPoints(const QPolygon &points)
    \overload
    Draws \a points.
*/

/*!
    \overload

    Draws \a line with the current pen, through the transformation.
*/
void QPainter::drawLine(const QLineF &line) { drawLines(&line, 1); }

/*!
    \fn void QPainter::drawLine(const QPointF &p1, const QPointF &p2)
    \overload
    Draws a line from \a p1 to \a p2.
*/

/*!
    Draws the \a lineCount lines in \a lines.
*/
void QPainter::drawLines(const QLineF *lines, int lineCount) {
    if (!d->active || !lines || lineCount <= 0 || d->state.pen.style() == Qt::NoPen)
        return;
    QPainterPath path;
    for (int i = 0; i < lineCount; ++i) {
        path.moveTo(lines[i].p1());
        path.lineTo(lines[i].p2());
    }
    d->drawPath(path, d->state.pen, QBrush());
}

/*!
    \overload

    Draws the \a lineCount lines in \a lines.
*/
void QPainter::drawLines(const QLine *lines, int lineCount) {
    if (!lines)
        return;
    for (int i = 0; i < lineCount; ++i)
        drawLine(lines[i]);
}

/*!
    \fn void QPainter::drawLines(const QList<QLineF> &lines)
    \overload
    Draws \a lines.
*/

/*!
    \fn void QPainter::drawLines(const QList<QLine> &lines)
    \overload
    Draws \a lines.
*/

/*!
    \overload

    Draws \a rect, filled with the brush and outlined with the pen, through the transformation.
*/
void QPainter::drawRect(const QRectF &rect) { drawRects(&rect, 1); }

/*!
    Draws the \a rectCount rectangles in \a rects.
*/
void QPainter::drawRects(const QRectF *rects, int rectCount) {
    if (!d->active || !rects || rectCount <= 0)
        return;
    QPainterPath path;
    for (int i = 0; i < rectCount; ++i)
        path.addRect(rects[i]);
    d->drawPath(path, d->state.pen, d->state.brush);
}

/*!
    \overload

    Draws the \a rectCount rectangles in \a rects.
*/
void QPainter::drawRects(const QRect *rects, int rectCount) {
    if (!rects)
        return;
    for (int i = 0; i < rectCount; ++i)
        drawRect(rects[i]);
}

/*!
    \fn void QPainter::drawRects(const QList<QRectF> &rects)
    \overload
    Draws \a rects.
*/

/*!
    \fn void QPainter::drawRects(const QList<QRect> &rects)
    \overload
    Draws \a rects.
*/

/*!
    Draws \a rect with corners rounded by \a xRadius and \a yRadius, in units or, with \a mode
    Qt::RelativeSize, in percent of half the width and height.
*/
void QPainter::drawRoundedRect(const QRectF &rect, qreal xRadius, qreal yRadius, Qt::SizeMode mode) {
    if (!d->active)
        return;
    QPainterPath path;
    path.addRoundedRect(rect, xRadius, yRadius, mode);
    d->drawPath(path, d->state.pen, d->state.brush);
}

/*!
    \fn void QPainter::drawRoundedRect(int x, int y, int w, int h, qreal xRadius, qreal yRadius, Qt::SizeMode mode)
    \overload
    Draws the rectangle at (\a x, \a y) of \a w by \a h with corners rounded by \a xRadius and \a yRadius
    in the given \a mode.
*/

/*!
    \fn void QPainter::drawRoundedRect(const QRect &rect, qreal xRadius, qreal yRadius, Qt::SizeMode mode)
    \overload
    Draws \a rect with corners rounded by \a xRadius and \a yRadius in the given \a mode.
*/

/*!
    \overload

    Draws the ellipse that fits in \a rect, through the transformation.
*/
void QPainter::drawEllipse(const QRectF &rect) {
    if (!d->active)
        return;
    QPainterPath path;
    path.addEllipse(rect);
    d->drawPath(path, d->state.pen, d->state.brush);
}

/*!
    \fn void QPainter::drawEllipse(const QPointF &center, qreal rx, qreal ry)
    \overload
    Draws the ellipse at \a center with the radii \a rx and \a ry.
*/

/*!
    \fn void QPainter::drawEllipse(const QPoint &center, int rx, int ry)
    \overload
    Draws the ellipse at \a center with the radii \a rx and \a ry.
*/

/*!
    Draws the arc of the ellipse that fits in \a rectangle, from \a startAngle for \a spanAngle, both in
    sixteenths of a degree, counterclockwise from 3 o'clock. The pen draws it; the brush is not used.
*/
void QPainter::drawArc(const QRectF &rectangle, int startAngle, int spanAngle) {
    if (!d->active)
        return;
    QPainterPath path;
    path.arcMoveTo(rectangle, startAngle / 16.0);
    path.arcTo(rectangle, startAngle / 16.0, spanAngle / 16.0);
    d->drawPath(path, d->state.pen, QBrush());
}

/*!
    \fn void QPainter::drawArc(const QRect &rectangle, int startAngle, int spanAngle)
    \overload
    Draws the arc of the ellipse in \a rectangle from \a startAngle for \a spanAngle sixteenths of a degree.
*/

/*!
    \fn void QPainter::drawArc(int x, int y, int width, int height, int startAngle, int spanAngle)
    \overload
    Draws the arc of the ellipse in the rectangle at (\a x, \a y) of \a width by \a height, from
    \a startAngle for \a spanAngle sixteenths of a degree.
*/

/*!
    Draws the pie of the ellipse that fits in \a rectangle, from \a startAngle for \a spanAngle, in
    sixteenths of a degree: the arc and the lines to the center, filled with the brush.
*/
void QPainter::drawPie(const QRectF &rectangle, int startAngle, int spanAngle) {
    if (!d->active)
        return;
    QPainterPath path;
    path.moveTo(rectangle.center());
    path.arcTo(rectangle, startAngle / 16.0, spanAngle / 16.0);
    path.closeSubpath();
    d->drawPath(path, d->state.pen, d->state.brush);
}

/*!
    \fn void QPainter::drawPie(const QRect &rectangle, int startAngle, int spanAngle)
    \overload
    Draws the pie of the ellipse in \a rectangle from \a startAngle for \a spanAngle.
*/

/*!
    \fn void QPainter::drawPie(int x, int y, int width, int height, int startAngle, int spanAngle)
    \overload
    Draws the pie of the ellipse in the rectangle at (\a x, \a y) of \a width by \a height, from
    \a startAngle for \a spanAngle.
*/

/*!
    Draws the chord of the ellipse that fits in \a rectangle, from \a startAngle for \a spanAngle, in
    sixteenths of a degree: the arc closed by a straight line, filled with the brush.
*/
void QPainter::drawChord(const QRectF &rectangle, int startAngle, int spanAngle) {
    if (!d->active)
        return;
    QPainterPath path;
    path.arcMoveTo(rectangle, startAngle / 16.0);
    path.arcTo(rectangle, startAngle / 16.0, spanAngle / 16.0);
    path.closeSubpath();
    d->drawPath(path, d->state.pen, d->state.brush);
}

/*!
    \fn void QPainter::drawChord(const QRect &rectangle, int startAngle, int spanAngle)
    \overload
    Draws the chord of the ellipse in \a rectangle from \a startAngle for \a spanAngle.
*/

/*!
    \fn void QPainter::drawChord(int x, int y, int width, int height, int startAngle, int spanAngle)
    \overload
    Draws the chord of the ellipse in the rectangle at (\a x, \a y) of \a width by \a height, from
    \a startAngle for \a spanAngle.
*/

/*!
    Draws the open line through the \a pointCount points in \a points.
*/
void QPainter::drawPolyline(const QPointF *points, int pointCount) {
    if (!d->active || !points || pointCount < 2)
        return;
    QPainterPath path;
    path.moveTo(points[0]);
    for (int i = 1; i < pointCount; ++i)
        path.lineTo(points[i]);
    d->drawPath(path, d->state.pen, QBrush());
}

/*!
    \overload

    Draws the open line through the \a pointCount points in \a points.
*/
void QPainter::drawPolyline(const QPoint *points, int pointCount) {
    if (!points || pointCount < 2)
        return;
    QPolygonF p;
    for (int i = 0; i < pointCount; ++i)
        p.append(QPointF(points[i]));
    drawPolyline(p.constData(), pointCount);
}

/*!
    \fn void QPainter::drawPolyline(const QPolygonF &polyline)
    \overload
    Draws \a polyline.
*/

/*!
    \fn void QPainter::drawPolyline(const QPolygon &polyline)
    \overload
    Draws \a polyline.
*/

/*!
    Draws the polygon of the \a pointCount points in \a points, closed, filled with the brush by
    \a fillRule and outlined with the pen.
*/
void QPainter::drawPolygon(const QPointF *points, int pointCount, Qt::FillRule fillRule) {
    if (!d->active || !points || pointCount < 2)
        return;
    QPainterPath path;
    path.setFillRule(fillRule);
    path.moveTo(points[0]);
    for (int i = 1; i < pointCount; ++i)
        path.lineTo(points[i]);
    path.closeSubpath();
    d->drawPath(path, d->state.pen, d->state.brush);
}

/*!
    \overload

    Draws the polygon of the \a pointCount points in \a points, filled by \a fillRule.
*/
void QPainter::drawPolygon(const QPoint *points, int pointCount, Qt::FillRule fillRule) {
    if (!points || pointCount < 2)
        return;
    QPolygonF p;
    for (int i = 0; i < pointCount; ++i)
        p.append(QPointF(points[i]));
    drawPolygon(p.constData(), pointCount, fillRule);
}

/*!
    \fn void QPainter::drawPolygon(const QPolygonF &points, Qt::FillRule fillRule)
    \overload
    Draws the polygon \a points, filled by \a fillRule.
*/

/*!
    \fn void QPainter::drawPolygon(const QPolygon &points, Qt::FillRule fillRule)
    \overload
    Draws the polygon \a points, filled by \a fillRule.
*/

/*!
    \fn void QPainter::drawConvexPolygon(const QPointF *points, int pointCount)
    Draws the convex polygon of the \a pointCount points in \a points.
*/

/*!
    \fn void QPainter::drawConvexPolygon(const QPoint *points, int pointCount)
    \overload
    Draws the convex polygon of the \a pointCount points in \a points.
*/

/*!
    \fn void QPainter::drawConvexPolygon(const QPolygonF &polygon)
    \overload
    Draws the convex \a polygon.
*/

/*!
    \fn void QPainter::drawConvexPolygon(const QPolygon &polygon)
    \overload
    Draws the convex \a polygon.
*/

/*!
    \overload

    Draws \a image with its top left at \a point, through the transformation.
*/
void QPainter::drawImage(const QPointF &point, const QImage &image) {
    const qreal ratio = image.devicePixelRatio() > 0 ? image.devicePixelRatio() : 1.0;
    drawImage(QRectF(point, QSizeF(image.width() / ratio, image.height() / ratio)), image, QRectF());
}

/*!
    \overload

    Draws the part \a source of \a image, or all of it if \a source is null, scaled to fill \a target.
    Under a transformation that rotates, shears or mirrors, the image is transformed with it.
*/
void QPainter::drawImage(const QRectF &target, const QImage &image, const QRectF &source) {
    if (!d->active || image.isNull() || target.isEmpty())
        return;
    const QTransform toDevice = d->toDevice();
    // Rotated, sheared and mirrored images need the transformation; mapRect() would draw them upright.
    if (toDevice.type() > QTransform::TxScale || toDevice.m11() < 0 || toDevice.m22() < 0) {
        d->engine->drawTransformedImage(target, image, source.isNull() ? QRectF(image.rect()) : source);
        return;
    }
    const QRect src = source.isNull() ? image.rect() : source.toAlignedRect();
    const QRect device = toDevice.mapRect(target).toRect();
    if (device.isEmpty())
        return;
    d->engine->drawImage(device, image, src);
}

/*!
    \fn void QPainter::drawPixmap(const QPointF &point, const QPixmap &pixmap)
    \overload
    Draws \a pixmap with its top left at \a point.
*/

/*!
    \fn void QPainter::drawPixmap(const QRectF &target, const QPixmap &pixmap, const QRectF &source)
    \overload
    Draws the part \a source of \a pixmap scaled to fill \a target.
*/

/*!
    \overload

    Draws \a text with the left end of its baseline at \a position, mapped by the transformation. The
    glyphs are drawn upright at the size of the font: the transformation moves the text but does not rotate
    or scale it.
*/
void QPainter::drawText(const QPointF &position, const QString &text) {
    if (!d->active || text.isEmpty())
        return;
    const QPointF p = d->user().map(position);
    const QTransform saved = d->state.world;
    d->state.world = QTransform();
    d->updateTranslation();
    drawText(QPoint(qRound(p.x()), qRound(p.y())), text);
    d->state.world = saved;
    d->updateTranslation();
}

/*!
    \overload

    Draws \a text in \a rectangle with \a flags, as the QRect version does. If \a boundingRect is not
    \c nullptr, it receives the rectangle that the text covers.
*/
void QPainter::drawText(const QRectF &rectangle, int flags, const QString &text, QRectF *boundingRect) {
    QRect covered;
    drawText(rectangle.toAlignedRect(), flags, text, boundingRect ? &covered : nullptr);
    if (boundingRect)
        *boundingRect = QRectF(covered);
}
