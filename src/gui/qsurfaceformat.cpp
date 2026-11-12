// MIT License - see LICENSE
#include <QtGui/qsurfaceformat.h>

#include <mutex>

namespace {

std::mutex s_defaultMutex;

QSurfaceFormat &defaultStorage() {
    static QSurfaceFormat format;
    return format;
}

} // namespace

/*!
    \class QSurfaceFormat
    \inmodule NtGui
    \since 6.0
    \brief The QSurfaceFormat class represents the format of a QSurface.

    The format describes the buffers an OpenGL surface asks for (the sizes of
    the color, depth and stencil buffers and the number of samples for
    multisampling), the OpenGL version and profile of the context, and the
    swap interval:

    \code
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    format.setSwapInterval(0); // do not wait for the display
    QSurfaceFormat::setDefaultFormat(format);
    \endcode

    A size of -1, the default, means no particular size. Set the format on a
    window with QWindow::setFormat() and on a context with
    QOpenGLContext::setFormat(), or for all of them with setDefaultFormat()
    before they are made.

    \differences
    There is no color space. OpenVG is not supported, and the swap behavior is
    only remembered: windows are double buffered.
    \enddifferences

    \sa QOpenGLContext, QWindow::setFormat()
*/

/*!
    \enum QSurfaceFormat::FormatOption

    This enum describes options of a context.

    \value StereoBuffers Stereo buffers, for left and right images.
    \value DebugContext A context that reports debug messages.
    \value DeprecatedFunctions A context where the functions deprecated by OpenGL 3.0 remain.
    \value ResetNotification A context that reports resets of the graphics hardware.
    \value ProtectedContent Content that cannot be read back.
*/

/*!
    \enum QSurfaceFormat::SwapBehavior

    This enum describes how many buffers a surface swaps.

    \value DefaultSwapBehavior Whatever the platform does.
    \value SingleBuffer One buffer.
    \value DoubleBuffer Two buffers.
    \value TripleBuffer Three buffers.
*/

/*!
    \enum QSurfaceFormat::RenderableType

    This enum describes the rendering API of a context.

    \value DefaultRenderableType Desktop OpenGL if the system has it, otherwise OpenGL ES.
    \value OpenGL Desktop OpenGL.
    \value OpenGLES OpenGL ES.
    \value OpenVG OpenVG, which Nt does not support.
*/

/*!
    \enum QSurfaceFormat::OpenGLContextProfile

    This enum describes the profile of a desktop OpenGL context of version 3.2 or later.

    \value NoProfile No profile: the version is before 3.2, or it does not matter.
    \value CoreProfile Only the functions that OpenGL 3.0 did not deprecate.
    \value CompatibilityProfile All functions, including the deprecated ones.
*/

/*!
    \fn QSurfaceFormat::QSurfaceFormat()

    Constructs a format with the defaults: no particular buffer sizes,
    OpenGL 2.0, no profile, and a swap interval of 1.
*/

/*!
    Constructs a default format with the \a options set.
*/
QSurfaceFormat::QSurfaceFormat(FormatOptions options) : m_options(options) {}

/*!
    Sets \a option on if \a on is \c true, and off otherwise.

    \sa testOption(), setOptions()
*/
void QSurfaceFormat::setOption(FormatOption option, bool on) { m_options.setFlag(option, on); }

/*!
    \fn void QSurfaceFormat::setDepthBufferSize(int size)
    Sets the number of bits of the depth buffer to \a size.
*/
/*!
    \fn int QSurfaceFormat::depthBufferSize() const
    Returns the number of bits of the depth buffer, or -1 for any.
*/
/*!
    \fn void QSurfaceFormat::setStencilBufferSize(int size)
    Sets the number of bits of the stencil buffer to \a size.
*/
/*!
    \fn int QSurfaceFormat::stencilBufferSize() const
    Returns the number of bits of the stencil buffer, or -1 for any.
*/
/*!
    \fn void QSurfaceFormat::setRedBufferSize(int size)
    Sets the number of bits of red to \a size.
*/
/*!
    \fn int QSurfaceFormat::redBufferSize() const
    Returns the number of bits of red, or -1 for any.
*/
/*!
    \fn void QSurfaceFormat::setGreenBufferSize(int size)
    Sets the number of bits of green to \a size.
*/
/*!
    \fn int QSurfaceFormat::greenBufferSize() const
    Returns the number of bits of green, or -1 for any.
*/
/*!
    \fn void QSurfaceFormat::setBlueBufferSize(int size)
    Sets the number of bits of blue to \a size.
*/
/*!
    \fn int QSurfaceFormat::blueBufferSize() const
    Returns the number of bits of blue, or -1 for any.
*/
/*!
    \fn void QSurfaceFormat::setAlphaBufferSize(int size)
    Sets the number of bits of alpha to \a size.
*/
/*!
    \fn int QSurfaceFormat::alphaBufferSize() const
    Returns the number of bits of alpha, or -1 for any.
*/
/*!
    \fn void QSurfaceFormat::setSamples(int numSamples)
    Sets the number of samples per pixel for multisampling to \a numSamples.
    With more than one, QPainter on an OpenGL surface draws antialiased.
*/
/*!
    \fn int QSurfaceFormat::samples() const
    Returns the number of samples per pixel, or -1 for no multisampling.
*/
/*!
    \fn void QSurfaceFormat::setSwapBehavior(SwapBehavior behavior)
    Sets the swap behavior to \a behavior.
*/
/*!
    \fn QSurfaceFormat::SwapBehavior QSurfaceFormat::swapBehavior() const
    Returns the swap behavior.
*/
/*!
    \fn bool QSurfaceFormat::hasAlpha() const
    Returns \c true if the format has an alpha buffer.
*/
/*!
    \fn void QSurfaceFormat::setProfile(OpenGLContextProfile profile)
    Sets the OpenGL profile to \a profile.
*/
/*!
    \fn QSurfaceFormat::OpenGLContextProfile QSurfaceFormat::profile() const
    Returns the OpenGL profile.
*/
/*!
    \fn void QSurfaceFormat::setRenderableType(RenderableType type)
    Sets the rendering API to \a type.
*/
/*!
    \fn QSurfaceFormat::RenderableType QSurfaceFormat::renderableType() const
    Returns the rendering API.
*/
/*!
    \fn void QSurfaceFormat::setMajorVersion(int majorVersion)
    Sets the major OpenGL version to \a majorVersion.
*/
/*!
    \fn int QSurfaceFormat::majorVersion() const
    Returns the major OpenGL version.
*/
/*!
    \fn void QSurfaceFormat::setMinorVersion(int minorVersion)
    Sets the minor OpenGL version to \a minorVersion.
*/
/*!
    \fn int QSurfaceFormat::minorVersion() const
    Returns the minor OpenGL version.
*/
/*!
    \fn std::pair<int, int> QSurfaceFormat::version() const
    Returns the major and minor OpenGL version.
*/
/*!
    \fn void QSurfaceFormat::setVersion(int major, int minor)
    Sets the OpenGL version to \a major and \a minor.
*/
/*!
    \fn bool QSurfaceFormat::stereo() const
    Returns \c true if the format has stereo buffers.
*/
/*!
    \fn void QSurfaceFormat::setStereo(bool enable)
    Asks for stereo buffers if \a enable is \c true.
*/
/*!
    \fn void QSurfaceFormat::setOptions(QSurfaceFormat::FormatOptions options)
    Sets the options to \a options.
*/
/*!
    \fn bool QSurfaceFormat::testOption(FormatOption option) const
    Returns \c true if \a option is set.
*/
/*!
    \fn QSurfaceFormat::FormatOptions QSurfaceFormat::options() const
    Returns the options.
*/
/*!
    \fn int QSurfaceFormat::swapInterval() const
    Returns the swap interval: the number of display refreshes
    QOpenGLContext::swapBuffers() waits for. The default is 1, which
    synchronizes with the display (vsync).
*/
/*!
    \fn void QSurfaceFormat::setSwapInterval(int interval)
    Sets the swap interval to \a interval. With 0, swapping does not wait for
    the display, so rendering is not capped by its refresh rate.
*/

/*!
    Sets the format that windows, offscreen surfaces and contexts start with
    to \a format.

    \sa defaultFormat()
*/
void QSurfaceFormat::setDefaultFormat(const QSurfaceFormat &format) {
    std::lock_guard<std::mutex> lock(s_defaultMutex);
    defaultStorage() = format;
}

/*!
    Returns the format set with setDefaultFormat(), or a default format.
*/
QSurfaceFormat QSurfaceFormat::defaultFormat() {
    std::lock_guard<std::mutex> lock(s_defaultMutex);
    return defaultStorage();
}

bool operator==(const QSurfaceFormat &a, const QSurfaceFormat &b) {
    return a.m_depth == b.m_depth && a.m_stencil == b.m_stencil && a.m_red == b.m_red && a.m_green == b.m_green &&
           a.m_blue == b.m_blue && a.m_alpha == b.m_alpha && a.m_samples == b.m_samples &&
           a.m_swapBehavior == b.m_swapBehavior && a.m_profile == b.m_profile &&
           a.m_renderableType == b.m_renderableType && a.m_major == b.m_major && a.m_minor == b.m_minor &&
           a.m_options.toInt() == b.m_options.toInt() && a.m_swapInterval == b.m_swapInterval;
}
