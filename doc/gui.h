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
    here on a screen with twice the usual pixels and the dark color scheme of the desktop, shows every widget
    that Nt has on the pages of a tab widget:

    \image hello-widgets-window.png The hello_widgets example: the Nt logo, and a page of tabs with a live example of every widget

    See \l{Using Nt Widgets}{Using Nt Widgets} for the widgets, layouts and styles.

    \section1 What Is Not There Yet

    Shapes are drawn without antialiasing, and text is drawn glyph by glyph
    without shaping, kerning or font fallback, so complex scripts and emoji do
    not display. Widgets and layouts are in \l{Using Nt Widgets}{Nt Widgets}. There is no clipboard,
    drag and drop, cursors or input methods. The \c xcb platform shows one screen,
    the whole X desktop, and copies the window contents to the server with
    \c PutImage. Each class lists its own limits in its \e Differences from Qt
    section.

    \sa QGuiApplication, QWindow, QRasterWindow, QPainter, QImage, QStyleHints, QScreen
*/
