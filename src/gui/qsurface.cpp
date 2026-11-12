// MIT License - see LICENSE
#include <QtGui/qsurface.h>

/*!
    \class QSurface
    \inmodule NtGui
    \since 6.0
    \brief The QSurface class is an abstraction of renderable surfaces.

    A surface is what a QOpenGLContext renders on: a QWindow or a
    QOffscreenSurface.

    \differences
    There is no \c surfaceHandle(). Only raster and OpenGL surfaces are
    supported.
    \enddifferences

    \sa QWindow, QOffscreenSurface, QOpenGLContext
*/

/*!
    \enum QSurface::SurfaceClass

    This enum describes what a surface is.

    \value Window A QWindow.
    \value Offscreen A QOffscreenSurface.
*/

/*!
    \enum QSurface::SurfaceType

    This enum describes how a surface is rendered.

    \value RasterSurface In software, through a QBackingStore.
    \value OpenGLSurface With OpenGL, through a QOpenGLContext.
    \value RasterGLSurface Either way.
    \value OpenVGSurface With OpenVG, which Nt does not support.
    \value VulkanSurface With Vulkan, which Nt does not support.
    \value MetalSurface With Metal, which Nt does not support.
    \value Direct3DSurface With Direct3D, which Nt does not support.
*/

/*!
    \fn QSurface::QSurface(SurfaceClass type)

    Constructs a surface of the class \a type.
*/

/*!
    \variable QSurface::m_type
    \internal
*/

/*!
    Destroys the surface.
*/
QSurface::~QSurface() = default;

/*!
    \fn QSurface::SurfaceClass QSurface::surfaceClass() const

    Returns the class of the surface.
*/

/*!
    \fn QSurfaceFormat QSurface::format() const

    Returns the format of the surface.
*/

/*!
    \fn QSurface::SurfaceType QSurface::surfaceType() const

    Returns the type of the surface.
*/

/*!
    \fn QSize QSurface::size() const

    Returns the size of the surface in pixels of its coordinate system.
*/

/*!
    Returns \c true if a QOpenGLContext can render on the surface.
*/
bool QSurface::supportsOpenGL() const {
    const SurfaceType type = surfaceType();
    return type == OpenGLSurface || type == RasterGLSurface;
}
