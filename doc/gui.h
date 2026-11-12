// MIT License - see LICENSE
//
// Overview page about Nt GUI for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page nt-gui.html
    \title Using Nt GUI
    \brief How to open a window, paint on it and handle input with Nt GUI.

    Nt GUI follows the layering of Qt GUI. An application creates a
    QGuiApplication, which loads a \e{platform integration}. The platform
    creates the native windows, delivers input, and puts the pixels of a window on
    the screen. Application code only sees QWindow, QScreen, the events in
    \c qevent.h and QPainter.

    \section1 A Window You Paint On

    Subclass QRasterWindow and paint in paintEvent() with a QPainter that
    paints on the window itself. Call update() to ask for a repaint; the
    request is coalesced and delivered from the event loop.

    \snippet gui/hello_window.cpp window

    The \c main() function creates the application object, then the window:

    \code
    int main(int argc, char *argv[])
    {
        QGuiApplication app(argc, argv);
        HelloWindow window;
        window.show();
        return app.exec();
    }
    \endcode

    The \c hello_window example is this program. Link it with \c Nt::Gui:

    \code
    find_package(Nt6 REQUIRED)
    add_executable(hello_window main.cpp)
    target_link_libraries(hello_window PRIVATE Nt::Gui)
    \endcode

    \section1 Platforms

    Nt GUI has two platforms:

    \list
    \li \c xcb talks to an X11 server, including XWayland on a Wayland desktop.
        It needs the XCB library at build time; without it, Nt GUI builds with the
        \c offscreen platform only.
    \li \c offscreen has no display. Windows keep their pixels in memory. The
        tests of Nt use it, and so can yours.
    \endlist

    The platform is the value of the \c{-platform} command line option, or else of
    the \c QT_QPA_PLATFORM environment variable. Without either, Nt GUI uses
    \c xcb when \c DISPLAY is set, and \c offscreen otherwise, with a message.

    \section1 Painting Without a Window

    QImage is a paint device, so a QPainter draws on an image as it does on a
    window. This makes drawing code testable without a display:

    \code
    QImage image(100, 100, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setPen(QPen(Qt::black, 2));
    painter.drawEllipse(10, 10, 80, 80);
    \endcode

    \section1 Paths, Gradients and Transformations

    QPainter draws paths of lines and Bezier curves (QPainterPath), polygons, arcs, pies and chords, with
    real coordinates, through a transformation (translate(), scale(), rotate(), shear(), QTransform). Pens
    have cap and join styles and dashes; brushes have linear, radial and conical gradients. With the
    Antialiasing render hint the edges are smooth:

    \snippet gui/painting.cpp flower

    \image painter/flower.png A flower of Bezier petals with radial gradients

    The rasterizer is software: shapes become polygons, whose coverage of each pixel is computed exactly
    from their edges. Integer shapes drawn without antialiasing or a transformation stay on whole pixels,
    as before.

    The \c painting example shows the scenes of these pages in a window, animated, with the frame rate in
    its title. \c{painting --bench} draws each scene on an 800 by 600 image and prints the time per frame,
    and \c{painting --write dir} writes the pictures.

    \section1 Image Files

    QImage and QPixmap load and save PNG, BMP and JPEG files. The format of a file
    is found from its contents when it is loaded and from the suffix of its name
    when it is saved (or from the \a format argument):

    \code
    QImage image("photo.jpg");
    if (!image.isNull())
        image.save("photo.png");
    QPixmap icon(":/icons/open.png");
    \endcode

    PNG needs zlib and JPEG needs libjpeg; a build without one of them cannot read
    or write that format. Files are limited to 256 MiB of pixels.

    \section1 Drawing Text

    A QFont names a family, size and weight. QPainter::drawText() draws a string
    at a baseline position or inside a rectangle with alignment and word
    wrapping, and QFontMetrics measures it:

    \code
    QPainter painter(this);
    painter.setFont(QFont("Sans Serif", 12));
    painter.drawText(QRect(10, 10, 200, 60), Qt::AlignCenter | Qt::TextWordWrap, "Some text");
    QFontMetrics fm(painter.font());
    int width = fm.horizontalAdvance("Some text");
    \endcode

    Nt looks fonts up with fontconfig and rasterizes them with FreeType; both are
    needed when Nt is built, or text has fixed metrics and does not draw. Sizes in points
    are converted to pixels at 96 dots per inch.

    \section1 Rich Text

    A QTextDocument holds text with formats: paragraphs with their alignment and margins, and parts of
    their text with a font, a size, a color or a link. It reads a subset of HTML, breaks its lines for a
    width, and draws itself with a painter:

    \code
    QTextDocument document;
    document.setDefaultFont(painter.font());
    document.setHtml("<h3>Status</h3><p>Disk <b>87&nbsp;%</b> full, see <a href=\"disk\">details</a>.</p>");
    document.setTextWidth(240);
    document.drawContents(&painter);
    QString link = document.documentLayout()->anchorAt(mousePosition);
    \endcode

    QTextLayout does the same for one paragraph, line by line, for code that places its lines itself.
    QLabel and QTextEdit of Nt Widgets show rich text through a QTextDocument.

    \section1 Screens, Scale and Color Scheme

    A screen has a device pixel ratio, QScreen::devicePixelRatio(): the number of pixels of the screen that one unit
    of window coordinates covers. A window, its events and its QPainter work in those units, and the painter
    scales what it draws, so an application written for an ordinary screen is the right size on a high-resolution
    one. On the \c xcb platform the ratio comes from the \c Xft.dpi resource of the desktop (192 dots per inch is a ratio
    of 2), or from the \c QT_SCALE_FACTOR environment variable. A QImage has a ratio of its own, set with
    QImage::setDevicePixelRatio(), for painting at the resolution of the screen.

    The color scheme of the desktop, light or dark, is QStyleHints::colorScheme(), which QGuiApplication::styleHints()
    returns. On the \c xcb platform it is read from the desktop portal and followed as it changes; connect to
    QStyleHints::colorSchemeChanged() to react, or call QStyleHints::setColorScheme() to choose one yourself.

    \section1 Widgets on Nt GUI

    Nt Widgets is built on the classes of this page: the widgets of a window are painted by a QPainter into the
    backing store of one QWindow, and the events of the window are sent to the widgets. The \c hello_widgets example,
    here on a screen with twice the usual pixels in the dark and the light color scheme, shows every widget
    that Nt has on the pages of a tab widget:

    \image hello-widgets-window.png The hello_widgets example in the dark and the light color scheme: the Nt logo, and a page of tabs with a live example of every widget

    See \l{Using Nt Widgets}{Using Nt Widgets} for the widgets, layouts and styles.

    \section1 What Is Not There Yet

    Windows paint in software unless they render with OpenGL, through EGL (see \l{Using Nt OpenGL}); there is
    no Vulkan, Metal, Direct3D or QRhi back end. Text is drawn glyph by glyph, upright, without shaping, kerning or font
    fallback, so complex scripts and emoji do not display. Widgets and layouts are in \l{Using Nt Widgets}{Nt Widgets}. There is no clipboard,
    drag and drop or input methods; a QCursor has one of the standard shapes. The \c xcb platform shows one screen,
    the whole X desktop, and copies the window contents to the server with
    \c PutImage. Each class lists its own limits in its \e Differences from Qt
    section.

    \sa QGuiApplication, QWindow, QRasterWindow, QPainter, QImage, QStyleHints, QScreen, {Using Nt OpenGL}
*/
