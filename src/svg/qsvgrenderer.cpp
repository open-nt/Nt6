// MIT License - see LICENSE
#include <QtSvg/qsvgrenderer.h>

#include "qsvgdocument_p.h"

#include <QtCore/qfile.h>
#include <QtGui/qimage.h>
#include <QtGui/qpaintdevice.h>
#include <QtGui/qpainter.h>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace QtSvgPrivate;

/*!
    \module NtSvg
    \title Nt SVG C++ Classes
    \brief Classes that draw SVG documents and write them.
    \cmaketarget Nt::Svg

    Nt SVG reads Scalable Vector Graphics and draws them with a QPainter (QSvgRenderer), and writes what a
    painter draws as SVG (QSvgGenerator). See \l{Using Nt SVG}.

    Link with \c Nt::Svg and include \c{<QSvgRenderer>} or \c{<QSvgGenerator>}.
*/

/*!
    \namespace QtSvg
    \brief The QtSvg namespace holds the options of the SVG classes.
    \since 6.7
*/

/*!
    \enum QtSvg::Option

    This enum describes how an SVG document is read and drawn. The Options type is a typedef for
    \c{QFlags<Option>}. Only \c AssumeTrustedSource changes anything in Nt SVG, which draws static documents
    only and reads no files other than the one it is given.

    \value NoOption The default: everything that is supported is drawn.
    \value Tiny12FeaturesOnly Only the features of SVG Tiny 1.2 are used.
    \value AssumeTrustedSource The document comes from a trusted source, so that limits on its cost can be lifted.
    \value DisableSMILAnimations SMIL animations are not played.
    \value DisableCSSAnimations CSS animations are not played.
    \value DisableAnimations No animations are played.
*/

/*!
    \class QSvgRenderer
    \brief The QSvgRenderer class draws the contents of SVG files with a QPainter.

    Load a document from a file, a resource or a byte array, and draw it on any paint device:

    \snippet svg/svg_viewer.cpp render

    The document is drawn into a rectangle: all of the device with render(QPainter *), or the bounds given.
    The view box of the document is stretched to fill the rectangle unless aspectRatioMode() says otherwise.
    defaultSize() is the size the document asks for. One element can be drawn alone, by its \c id, and
    boundsOnElement() tells where it is; a sheet of icons in one file can be used that way.

    Shapes are filled and stroked with antialiasing, whatever the painter can do itself: the renderer draws
    the document at the resolution of the device and gives the result to the painter as an image.

    \section1 What Is Drawn

    The structure elements \c svg, \c g, \c defs, \c symbol, \c use, \c a and \c switch; the shapes \c path,
    \c rect, \c circle, \c ellipse, \c line, \c polyline and \c polygon; \c text with \c tspan;
    \c linearGradient and \c radialGradient; and \c clipPath. Fills and strokes take colors (names, \c{#rgb},
    \c{rgb()}, \c currentColor) and gradients, with opacity, both fill rules, line caps and joins, miter
    limits and dashes. Properties come from attributes, the \c style attribute and \c style elements, whose
    rules may select by element name, class and id. Transformations, group opacity and lengths with units
    are supported.

    \differences
    Documents are static: no SMIL or CSS animation, so animated() is \c false and repaintNeeded() is never
    emitted. Not drawn: \c image, \c mask, \c pattern, \c marker, filters, \c foreignObject, \c textPath and
    fonts in the document. Text is drawn upright with the fonts of the system: it follows the position and
    scale of its transformation but not rotation or shear, takes one color and has no stroke.
    \c preserveAspectRatio of the document is not read; use setAspectRatioMode(). CSS selectors with
    combinators or pseudo-classes are skipped. There is no \c load() from a \c QXmlStreamReader, no
    \c transformForElement() (there is no \c QTransform) and no \c setDefaultOptions(). Of the options,
    only \c QtSvg::AssumeTrustedSource has an effect.
    \enddifferences

    \sa QSvgWidget, QSvgGenerator, QPainter
*/

/*!
    \fn void QSvgRenderer::repaintNeeded()

    This signal is emitted when the document must be drawn again, because an animation moved on. Nt SVG has no
    animations and never emits it.
*/

struct QSvgRenderer::Data {
    Document document;
    bool valid = false;
    bool customViewBox = false;
    QRectF viewBox;
    Qt::AspectRatioMode aspect = Qt::IgnoreAspectRatio;
    QtSvg::Options options;
    int framesPerSecond = 30;
    bool animationEnabled = true;

    QRectF effectiveViewBox() const {
        if (customViewBox)
            return viewBox;
        const Box box = document.viewBox();
        return QRectF(box.x0, box.y0, box.width(), box.height());
    }

    // Draws the document, or one node, so that `source` fills `bounds` on the device of the painter.
    void draw(QPainter *painter, const QRectF &bounds, const QRectF &source, const Node *only, bool fit) const {
        if (!valid || !painter || !painter->isActive() || bounds.isEmpty() || source.isEmpty())
            return;
        // The image covers whole pixels of the painter's coordinates, and has the resolution of the device.
        const double x0 = std::floor(bounds.x()), y0 = std::floor(bounds.y());
        const double x1 = std::ceil(bounds.x() + bounds.width()), y1 = std::ceil(bounds.y() + bounds.height());
        const double limit = 1e8; // far beyond any device, and well within an int
        if (!(std::fabs(x0) < limit && std::fabs(y0) < limit && std::fabs(x1) < limit && std::fabs(y1) < limit))
            return;
        const int left = static_cast<int>(x0), top = static_cast<int>(y0);
        const int right = static_cast<int>(x1), bottom = static_cast<int>(y1);
        const QRect target(left, top, right - left, bottom - top);
        qreal ratio = painter->device() ? painter->device()->devicePixelRatio() : 1.0;
        if (!(ratio > 0.0))
            ratio = 1.0;
        const double pw = std::ceil(target.width() * ratio), ph = std::ceil(target.height() * ratio);
        // An image of more than 64 million pixels is not drawn.
        if (pw < 1.0 || ph < 1.0 || pw > 32768.0 || ph > 32768.0 || pw * ph > 64e6)
            return;
        double sx = bounds.width() / source.width(), sy = bounds.height() / source.height();
        double dx = 0.0, dy = 0.0;
        if (fit && aspect != Qt::IgnoreAspectRatio) {
            const double s = aspect == Qt::KeepAspectRatio ? std::min(sx, sy) : std::max(sx, sy);
            dx = (bounds.width() - source.width() * s) / 2.0;
            dy = (bounds.height() - source.height() * s) / 2.0;
            sx = sy = s;
        }
        const Mat m = Mat::scaling(ratio, ratio) * Mat::translate(bounds.x() - left + dx, bounds.y() - top + dy) *
                      Mat::scaling(sx, sy) * Mat::translate(-source.x(), -source.y());
        Surface surface(static_cast<int>(pw), static_cast<int>(ph));
        document.render(&surface, m, only);
        const QImage image(reinterpret_cast<const uchar *>(surface.pixels.data()), surface.width, surface.height,
                           QImage::Format_ARGB32_Premultiplied);
        painter->drawImage(target, image);
    }
};

/*!
    Constructs a renderer without a document, with the given \a parent.

    \sa load()
*/
QSvgRenderer::QSvgRenderer(QObject *parent) : QObject(parent), d(std::make_unique<Data>()) {}

/*!
    Constructs a renderer with the given \a parent and loads the file \a filename, as load() does.
*/
QSvgRenderer::QSvgRenderer(const QString &filename, QObject *parent) : QSvgRenderer(parent) { load(filename); }

/*!
    Constructs a renderer with the given \a parent and loads the document in \a contents, as load() does.
*/
QSvgRenderer::QSvgRenderer(const QByteArray &contents, QObject *parent) : QSvgRenderer(parent) { load(contents); }

/*!
    Destroys the renderer.
*/
QSvgRenderer::~QSvgRenderer() = default;

/*!
    Returns \c true if a document is loaded; otherwise returns \c false.

    \sa load()
*/
bool QSvgRenderer::isValid() const { return d->valid; }

/*!
    Returns the size the document asks for: its \c width and \c height, or the size of its view box where
    they are missing. Returns an invalid size if no document is loaded.

    \sa viewBoxF()
*/
QSize QSvgRenderer::defaultSize() const {
    if (!d->valid)
        return QSize();
    const auto round = [](double v) { return static_cast<int>(std::lround(std::min(v, 1e9))); };
    return QSize(round(d->document.width()), round(d->document.height()));
}

/*!
    Returns the view box as a rectangle of integers.

    \sa viewBoxF()
*/
QRect QSvgRenderer::viewBox() const {
    const QRectF box = viewBoxF();
    const auto round = [](double v) { return static_cast<int>(std::floor(std::clamp(v, -1e9, 1e9) + 0.5)); };
    return QRect(round(box.x()), round(box.y()), round(box.width()), round(box.height()));
}

/*!
    Returns the view box: the part of the document that render() fits into its bounds. It is the \c viewBox
    of the document unless setViewBox() changed it; a document without one has a view box of its size, or
    the bounds of its contents if it has no size either.

    \sa setViewBox()
*/
QRectF QSvgRenderer::viewBoxF() const { return d->valid ? d->effectiveViewBox() : QRectF(); }

/*!
    \overload
    Sets the view box to \a viewbox.
*/
void QSvgRenderer::setViewBox(const QRect &viewbox) { setViewBox(QRectF(viewbox)); }

/*!
    Sets the view box to \a viewbox, so that render() shows that part of the document. Loading a document
    resets it.

    \sa viewBoxF()
*/
void QSvgRenderer::setViewBox(const QRectF &viewbox) {
    d->viewBox = viewbox;
    d->customViewBox = true;
}

/*!
    Returns how the view box is fitted into the bounds of render(). The default is
    \c Qt::IgnoreAspectRatio: it is stretched to fill them.

    \sa setAspectRatioMode()
*/
Qt::AspectRatioMode QSvgRenderer::aspectRatioMode() const { return d->aspect; }

/*!
    Sets how the view box is fitted into the bounds of render() to \a mode. With \c Qt::KeepAspectRatio the
    document keeps its proportions and is centered in the bounds; with \c Qt::KeepAspectRatioByExpanding it
    covers them, and what does not fit is cut off.

    \sa aspectRatioMode()
*/
void QSvgRenderer::setAspectRatioMode(Qt::AspectRatioMode mode) { d->aspect = mode; }

/*!
    Returns the options of the renderer.

    \sa setOptions()
*/
QtSvg::Options QSvgRenderer::options() const { return d->options; }

/*!
    Sets the options to \a flags. \c QtSvg::AssumeTrustedSource lifts the limit on the number of elements
    that one drawing visits (500000, counting every use of an element), which keeps a hostile document from
    taking forever; the other options are stored and change nothing.

    \sa options()
*/
void QSvgRenderer::setOptions(QtSvg::Options flags) {
    d->options = flags;
    d->document.setBudget(flags.testFlag(QtSvg::AssumeTrustedSource) ? std::numeric_limits<long long>::max()
                                                                     : Document::DefaultBudget);
}

/*!
    Returns \c true if the document has animations. Nt SVG draws static documents and returns \c false.
*/
bool QSvgRenderer::animated() const { return false; }

/*!
    Returns the number of frames per second of an animation. The default is 30.

    \sa setFramesPerSecond()
*/
int QSvgRenderer::framesPerSecond() const { return d->framesPerSecond; }

/*!
    Sets the number of frames per second to \a num. Negative numbers are ignored. The value is stored only,
    as there are no animations.

    \sa framesPerSecond()
*/
void QSvgRenderer::setFramesPerSecond(int num) {
    if (num >= 0)
        d->framesPerSecond = num;
}

/*!
    Returns the current frame of the animation, which is always 0.

    \sa setCurrentFrame()
*/
int QSvgRenderer::currentFrame() const { return 0; }

/*!
    Sets the current frame of the animation to \a frame. Does nothing, as there are no animations.

    \sa currentFrame()
*/
void QSvgRenderer::setCurrentFrame(int frame) { (void)frame; }

/*!
    Returns the number of frames of the animation, which is always 0.
*/
int QSvgRenderer::animationDuration() const { return 0; }

/*!
    Returns \c true if animations are enabled. The default is \c true.

    \sa setAnimationEnabled()
*/
bool QSvgRenderer::isAnimationEnabled() const { return d->animationEnabled; }

/*!
    Enables animations if \a enable is \c true. The value is stored only, as there are no animations.

    \sa isAnimationEnabled()
*/
void QSvgRenderer::setAnimationEnabled(bool enable) { d->animationEnabled = enable; }

/*!
    Returns the bounding rectangle of the element with the given \a id, in the coordinates of its parent:
    the transformation of the element itself is applied, those of its ancestors are not. The stroke is
    part of the bounds. Returns a null rectangle if there is no such element or it has nothing to draw.

    \sa elementExists(), render()
*/
QRectF QSvgRenderer::boundsOnElement(const QString &id) const {
    if (!d->valid)
        return QRectF();
    const Box box = d->document.bounds(d->document.find(id.toUtf8().toStdString()));
    return box.valid ? QRectF(box.x0, box.y0, box.width(), box.height()) : QRectF();
}

/*!
    Returns \c true if the document has an element with the given \a id that can be drawn; otherwise
    returns \c false.

    \sa boundsOnElement()
*/
bool QSvgRenderer::elementExists(const QString &id) const {
    return d->valid && d->document.find(id.toUtf8().toStdString()) != nullptr;
}

/*!
    Loads the SVG file \a filename, which may be a resource (\c{:/icons/logo.svg}), and returns \c true on
    success. If the file cannot be read or is not an SVG document, the renderer becomes invalid and \c false
    is returned.

    \sa isValid()
*/
bool QSvgRenderer::load(const QString &filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        d->valid = false;
        return false;
    }
    return load(file.readAll());
}

/*!
    \overload

    Loads the SVG document in \a contents and returns \c true on success. The document must be well-formed
    XML in UTF-8 with an \c svg element as its root.
*/
bool QSvgRenderer::load(const QByteArray &contents) {
    d->customViewBox = false;
    d->valid = d->document.parse(contents);
    return d->valid;
}

/*!
    Draws the document with \a painter, filling the device it paints on.
*/
void QSvgRenderer::render(QPainter *painter) {
    if (!painter || !painter->device())
        return;
    render(painter, QRectF(0.0, 0.0, painter->device()->width(), painter->device()->height()));
}

/*!
    \overload

    Draws the document with \a painter so that its view box fills \a bounds, as aspectRatioMode() says.
    Nothing is drawn outside \a bounds.
*/
void QSvgRenderer::render(QPainter *painter, const QRectF &bounds) {
    d->draw(painter, bounds, viewBoxF(), nullptr, true);
}

/*!
    \overload

    Draws the element with the id \a elementId, and what is in it, with \a painter. Its bounding rectangle
    (see boundsOnElement()) is stretched to fill \a bounds, or the whole device if \a bounds is empty. An
    element in \c defs can be drawn too.
*/
void QSvgRenderer::render(QPainter *painter, const QString &elementId, const QRectF &bounds) {
    if (!d->valid || !painter || !painter->device())
        return;
    const Node *node = d->document.find(elementId.toUtf8().toStdString());
    const QRectF source = boundsOnElement(elementId);
    if (!node)
        return;
    const QRectF target =
        bounds.isEmpty() ? QRectF(0.0, 0.0, painter->device()->width(), painter->device()->height()) : bounds;
    d->draw(painter, target, source, node, false);
}

#include "moc_qsvgrenderer.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
