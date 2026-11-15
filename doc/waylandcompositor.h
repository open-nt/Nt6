// MIT License - see LICENSE
//
// Overview page about Nt Wayland Compositor for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page nt-waylandcompositor.html
    \title Nt Wayland Compositor
    \brief Hosting Wayland clients from an Nt application, with the API of Qt Wayland Compositor.

    Nt Wayland Compositor makes an application a Wayland display server: other programs, written with any
    toolkit, connect to it, and the application decides where their windows go, what they look like on screen and
    which of them gets the input. It is the module for a kiosk or an operator console that embeds the windows
    of other processes, such as a vehicle HMI that hosts its display-extension applications. The library is
    \c Nt::WaylandCompositor and it needs Nt GUI:

    \code
    find_package(Nt6 REQUIRED)
    add_executable(console main.cpp)
    target_link_libraries(console PRIVATE Nt::WaylandCompositor Nt::Widgets)
    \endcode

    \section1 How It Fits Together

    QWaylandCompositor listens on a socket in \c XDG_RUNTIME_DIR (\c wayland-0 by default, or the name given to
    QWaylandCompositor::setSocketName()). Each program that connects is a QWaylandClient; each window-sized
    rectangle it draws is a QWaylandSurface. A surface by itself has no place on screen: the xdg-shell protocol,
    implemented by QWaylandXdgShell, gives it a role, a QWaylandXdgToplevel (a window) or a QWaylandXdgPopup (a
    menu or a tool tip), with a title, an application id and the states the compositor grants it.

    The pixels arrive as shared-memory buffers. After every commit QWaylandSurface::redraw() is emitted and
    QWaylandSurface::currentBuffer() holds a QWaylandBufferRef whose image() is a QImage over the client's memory,
    ready for QPainter:

    \code
    QWaylandCompositor compositor;
    QWaylandOutput output(&compositor, nullptr);
    output.addMode(QWaylandOutputMode(QSize(1280, 720), 60000), true);
    QWaylandXdgShell shell(&compositor);

    QObject::connect(&shell, &QWaylandXdgShell::toplevelCreated,
                     [&](QWaylandXdgToplevel *toplevel, QWaylandXdgSurface *xdgSurface) {
        toplevel->sendFullscreen(QSize(1280, 720));
        QWaylandSurface *surface = xdgSurface->surface();
        QObject::connect(surface, &QWaylandSurface::redraw, [surface] {
            QImage frame = surface->currentBuffer().image();
            // paint `frame` where the window belongs, then let the client draw the next one:
            surface->sendFrameCallbacks();
        });
    });
    compositor.create();
    \endcode

    Input goes the other way through a QWaylandSeat: the application tells the seat which QWaylandView the pointer
    is in and which surface has the keyboard focus, and sends it the presses, releases, moves and keys of its own
    devices; the seat turns them into protocol events for the right client.

    \section1 Window Management Is the Application's

    Nothing in the module decides stacking, placement or focus. That is the point for a console: the application
    knows which client belongs in which area of the screen and which one is in front, and nothing else, no
    desktop window manager, can override it. QWaylandXdgToplevel::sendConfigure() tells a client the size and
    states it gets, QWaylandXdgToplevel::sendClose() asks it to close, and the client's own requests
    (QWaylandXdgToplevel::setMaximized(), QWaylandXdgToplevel::startMove() and the rest) are signals the
    application may act on or ignore.

    \section1 What Is Not There Yet

    The module speaks the core protocol (version 6 of \c wl_compositor, \c wl_shm, \c wl_output, \c wl_seat with
    pointer, keyboard and touch) and xdg-shell version 6, in Nt's own implementation of the wire protocol: there
    is no libwayland. Not yet offered: subsurfaces, \c linux-dmabuf and other GPU buffers, xdg-decoration, the
    clipboard and drag and drop, text input, and the QML types of Qt Wayland Compositor. Keymaps are built with
    xkbcommon when Nt was built with it (the CMake option \c NT_USE_XKBCOMMON, on by default); without it clients
    get key codes but no keymap. The module is built on Linux only (\c NT_USE_WAYLAND).

    \sa QWaylandCompositor, QWaylandSurface, QWaylandSeat, QWaylandXdgShell
*/
