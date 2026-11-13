// MIT License - see LICENSE
//
// Overview page about OpenGL in Nt for the NDoc documentation. This file holds only a documentation comment and
// is not part of the library.

/*!
    \page nt-opengl.html
    \title Using Nt OpenGL
    \brief How to render with OpenGL in a window or offscreen, paint with QPainter through OpenGL, and control vsync.

    Nt renders with OpenGL through the same classes as Qt. The contexts and their functions are in Nt GUI
    (QOpenGLContext, QOpenGLFunctions, QSurfaceFormat, QOffscreenSurface); the window, the QPainter device and
    framebuffer objects are in Nt OpenGL (QOpenGLWindow, QOpenGLPaintDevice, QOpenGLFramebufferObject):

    \code
    find_package(Nt6 REQUIRED)
    add_executable(viewer main.cpp)
    target_link_libraries(viewer PRIVATE Nt::OpenGL)
    \endcode

    Nt does not link with an OpenGL library. It loads EGL (\c libEGL.so.1) and the OpenGL library when the first
    context is created, so programs that never use OpenGL run where there is none, and building Nt needs no
    OpenGL headers: \c{<QtGui/qopengl.h>} declares the types and constants.

    \section1 A Window That Renders With OpenGL

    Derive from QOpenGLWindow and reimplement initializeGL(), resizeGL() and paintGL(). The window creates its
    context the first time it is shown, makes it current before calling them, and swaps after paintGL(). A
    QPainter on the window draws with OpenGL too:

    \snippet opengl/glwindow.cpp window

    Each frame ends with QOpenGLWindow::frameSwapped(); asking for the next frame from there renders
    continuously.

    \section1 Frame Rate and Vsync

    The swap interval of the format decides whether swapping waits for the display. With 1, the default, frames
    come at the refresh rate of the display, which suits animation. With 0, swapping does not wait, so a
    benchmark measures the rendering rather than the display:

    \code
    QSurfaceFormat format;
    format.setSwapInterval(0); // no vsync
    format.setSamples(4);      // antialiasing for QPainter
    window.setFormat(format);  // or QSurfaceFormat::setDefaultFormat(format) before any window is made
    \endcode

    The \c glwindow example prints its frame rate: \c{glwindow} runs at the display's rate, and
    \c{glwindow --no-vsync} as fast as it can. A compositor can still hold back windows that do not wait for
    vsync; QOpenGLWindow::frameSwapped() tells when each frame was handed over.

    \section1 QPainter on OpenGL

    QOpenGLPaintDevice lets a QPainter draw into the framebuffer that is bound: the window, or a
    QOpenGLFramebufferObject. Everything QPainter draws on a QImage works: paths with both fill rules, pens with
    caps, joins and dashes, linear, radial and conical gradients, transformations, images, text and clipping.
    Paths are flattened and stroked by the same code as in the raster engine, so the pictures match.
    Images are kept as textures, by QImage::cacheKey(), so drawing the same image again does not upload its
    pixels again; an image that has been written to since is uploaded anew. The textures of a context hold up to
    64 MiB, and the least recently drawn images make room for new ones.

    \code
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(4);
    QOpenGLFramebufferObject fbo(QSize(800, 600), format);
    fbo.bind();

    QOpenGLPaintDevice device(fbo.size());
    QPainter painter(&device);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPath(path);
    painter.end();

    QImage picture = fbo.toImage();
    \endcode

    Two things differ from painting on an image:

    \list
    \li Shapes are filled through the stencil buffer, so the target needs one: give framebuffer objects the
        \c CombinedDepthStencil attachment. Windows get one unless their format asks for none.
    \li Antialiasing comes from multisampling. Ask for samples in the format of the window or the framebuffer
        object; without them, edges are aliased whatever the render hints say.
    \endlist

    \section1 Software and OpenGL Compared

    \c{painting --bench} draws the scenes of the \l{QPainter} documentation 800 by 600 pixels large, in
    software into a QImage and with OpenGL into a framebuffer object with four samples (waiting for each frame
    to finish), and prints the time of each. On a laptop with an NVIDIA GeForce RTX 5070 Ti:

    \table
    \header \li Scene \li Raster, ms per frame \li OpenGL, ms per frame
    \row \li Flower (Bezier petals with radial gradients) \li 9.4 \li 1.5
    \row \li Gradients (linear, radial, conical fills) \li 7.5 \li 0.09
    \row \li Pens (caps, dashes, round arcs) \li 0.46 \li 0.05
    \row \li Transforms (rotated rounded rectangles) \li 0.50 \li 0.10
    \row \li Lines (180 antialiased strokes) \li 12.2 \li 0.39
    \endtable

    \c{painting --gl} shows the scenes in a QOpenGLWindow that does not wait for vsync, with the frame rate in
    its title; \c{painting} shows them with the raster engine.

    \section1 OpenGL Calls

    QOpenGLContext::functions() gives the OpenGL ES 2.0 API, which desktop OpenGL has too. Derive from
    QOpenGLFunctions and call initializeOpenGLFunctions() in initializeGL() to call the functions by their
    names. Other functions, such as those of OpenGL 3, come from QOpenGLContext::getProcAddress().

    Without a format that asks otherwise, contexts are desktop OpenGL in a compatibility profile where EGL
    offers it, and OpenGL ES otherwise. Ask for a version and profile, or for OpenGL ES, with
    QSurfaceFormat::setVersion(), QSurfaceFormat::setProfile() and QSurfaceFormat::setRenderableType().

    \section1 Without a Window

    A QOffscreenSurface lets a context be current without a window, to render into framebuffer objects.

    \code
    QOffscreenSurface surface;
    surface.create();
    QOpenGLContext context;
    context.create();
    context.makeCurrent(&surface);
    \endcode

    On the \c offscreen platform, which has no window system, windows render into a framebuffer object that
    QOpenGLContext::defaultFramebufferObject() returns while the context is current on the window, so
    QOpenGLWindow and QOpenGLWindow::grabFramebuffer() work in tests and on servers without a display. That
    platform uses the surfaceless EGL platform of Mesa, or the first EGL device.

    \section1 What Is Not There Yet

    \list
    \li Only EGL: no GLX, WGL or CGL, so Nt OpenGL works on Linux (X11 through XCB, or headless) for now.
    \li No QOpenGLShaderProgram, QOpenGLBuffer, QOpenGLTexture, QOpenGLVertexArrayObject,
        QOpenGLTextureBlitter or QOpenGLExtraFunctions; call OpenGL through QOpenGLFunctions.
    \li No QOpenGLWidget, no Wayland platform, and no Vulkan or QRhi.
    \li QPainter has no \c beginNativePainting().
    \endlist

    \sa QOpenGLWindow, QOpenGLContext, QOpenGLPaintDevice, {Using Nt GUI}
*/
