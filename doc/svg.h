// MIT License - see LICENSE
//
// Overview page about Nt SVG for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page nt-svg.html
    \title Using Nt SVG
    \brief How to draw SVG files, show them in a widget, and write what a painter draws as SVG.

    Nt SVG reads Scalable Vector Graphics and draws them with a QPainter, and writes them from one. It has
    two libraries: \c Nt::Svg, with QSvgRenderer and QSvgGenerator, needs only Nt GUI; \c Nt::SvgWidgets adds
    QSvgWidget for applications that use Nt Widgets.

    \code
    find_package(Nt6 REQUIRED)
    add_executable(viewer main.cpp)
    target_link_libraries(viewer PRIVATE Nt::SvgWidgets)   # or Nt::Svg, without widgets
    \endcode

    \section1 Drawing a Document

    A QSvgRenderer loads a document from a file, a resource or a QByteArray and draws it on any paint
    device: an image, a window or a widget. The document has no pixels of its own, so it is drawn at the
    size it is given, and stays sharp:

    \snippet svg/svg_viewer.cpp render

    render() stretches the view box of the document over the whole device, or over the rectangle passed to
    it. QSvgRenderer::setAspectRatioMode() keeps the proportions instead, and QSvgRenderer::defaultSize()
    is the size that the document asks for.

    QPainter in Nt GUI draws with whole pixels and has no paths. The renderer does not depend on it for
    the shapes: it fills and strokes them itself, antialiased, at the resolution of the device, and gives
    the painter the finished picture. On a screen with a device pixel ratio of 2, a document drawn into a
    widget has twice the pixels.

    \section1 One File, Many Pictures

    An element with an \c id can be drawn by itself, so one file can hold a set of icons:

    \code
    QSvgRenderer icons(":/icons.svg");
    if (icons.elementExists("save"))
        icons.render(&painter, "save", QRectF(4, 4, 16, 16));
    \endcode

    The bounds of the element, from QSvgRenderer::boundsOnElement(), are stretched over the rectangle.
    Elements inside \c defs can be drawn this way too.

    \section1 In a Widget

    QSvgWidget shows a document scaled to the size of the widget; its size hint is the default size of
    the document. Its renderer() gives access to the QSvgRenderer:

    \snippet svg/svg_viewer.cpp widget

    \section1 Writing SVG

    QSvgGenerator is a paint device. What a QPainter draws on it is written as an SVG document, to a file
    or to any QIODevice, when the painter ends:

    \snippet svg/svg_viewer.cpp generate

    A document that the generator writes is drawn by the renderer as the same painting looks on a QImage.
    Text and images are written as embedded PNG images, which the renderer does not draw; see
    QSvgGenerator for the details.

    The \c svg_viewer example shows a file (\c{svg_viewer picture.svg}), and writes one
    (\c{svg_viewer --write out.svg}).

    \section1 What Is Supported

    Shapes, paths, groups, \c use, transformations, solid colors and gradients, strokes with caps, joins
    and dashes, opacity, clip paths, plain text and style sheets with simple selectors. Not supported:
    animation, embedded images, masks, patterns, markers and filters. QSvgRenderer has the full list.

    \section1 Documents from Elsewhere

    A document is data, and may come from anywhere. Loading reads nothing but the bytes it is given: no
    files, no network, no entities of a document type. Nesting and references are followed to a fixed depth,
    references that go around in circles end, and one drawing visits at most 500000 elements, so a small
    file cannot ask for an endless amount of work; \c QtSvg::AssumeTrustedSource lifts that last limit.
    A drawing of more than 64 million pixels is refused.

    \sa QSvgRenderer, QSvgWidget, QSvgGenerator, {Using Nt GUI}, {Using Nt Widgets}
*/
