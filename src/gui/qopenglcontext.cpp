// MIT License - see LICENSE
#include <QtGui/qopenglcontext.h>

#include "qegl_p.h"
#include "qpa/qplatformwindow_p.h"
#include "qwindow_p.h"

#include <QtCore/qdebug.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qoffscreensurface.h>
#include <QtGui/qopenglfunctions.h>
#include <QtGui/qscreen.h>
#include <QtGui/qwindow.h>

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

namespace egl = QtPrivate::Egl;

namespace {

thread_local QOpenGLContext *t_current = nullptr;

// What OpenGL made for one window or offscreen surface and one context.
struct Target {
    GLuint fbo = 0; // a window without an EGL surface renders into this framebuffer object
    GLuint color = 0;
    GLuint depthStencil = 0;
    QSize size;
    int samples = 0;
    egl::EGLSurface pbuffer = nullptr; // an offscreen surface where contexts need a surface to be current
};

// The EGL window surfaces, shared by all contexts, and the contexts that exist.
struct Registry {
    std::mutex mutex;
    std::map<QSurface *, egl::EGLSurface> windowSurfaces;
    std::vector<QOpenGLContext *> contexts;
};

Registry &registry() {
    static Registry *r = new Registry; // never destroyed: windows can outlive static destruction
    return *r;
}

bool versionAtLeast(const QSurfaceFormat &f, int major, int minor) {
    return f.majorVersion() > major || (f.majorVersion() == major && f.minorVersion() >= minor);
}

} // namespace

struct QOpenGLContext::Data {
    QSurfaceFormat requested = QSurfaceFormat::defaultFormat();
    QSurfaceFormat format;
    QOpenGLContext *share = nullptr;
    QScreen *screen = nullptr;
    egl::EGLContext context = nullptr;
    egl::EGLConfig config = nullptr;
    egl::EGLenum api = egl::OPENGL_API;
    QSurface *surface = nullptr;
    bool versionKnown = false;
    std::unique_ptr<QOpenGLFunctionsPrivate> functionsPrivate;
    std::unique_ptr<QOpenGLFunctions> functions;
    mutable bool extensionsKnown = false;
    mutable QSet<QByteArray> extensions;
    std::map<QSurface *, Target> targets;
    std::vector<GLuint> deadFramebuffers;
    std::vector<GLuint> deadRenderbuffers;
    bool emulatedBound = false;
    int appliedSwapInterval = INT_MIN;
    QSurface *swapIntervalSurface = nullptr;

    bool es() const { return api == egl::OPENGL_ES_API; }
    void queryVersion();
    void freeDead();
    bool bindEmulated(QWindow *window);
    void deleteTarget(Target &t, bool current);
};

void QOpenGLContext::Data::queryVersion() {
    QOpenGLFunctions *f = functions.get();
    const char *version = reinterpret_cast<const char *>(f->glGetString(GL_VERSION));
    if (!version)
        return;
    versionKnown = true;
    const char *p = version;
    const char esPrefix[] = "OpenGL ES ";
    const bool isEs = std::strncmp(p, esPrefix, sizeof esPrefix - 1) == 0;
    if (isEs)
        p += sizeof esPrefix - 1;
    while (*p && (*p < '0' || *p > '9'))
        ++p;
    int major = 0, minor = 0;
    if (std::sscanf(p, "%d.%d", &major, &minor) >= 1) {
        format.setMajorVersion(major);
        format.setMinorVersion(minor);
    }
    format.setRenderableType(isEs ? QSurfaceFormat::OpenGLES : QSurfaceFormat::OpenGL);
    format.setProfile(QSurfaceFormat::NoProfile);
    if (!isEs && versionAtLeast(format, 3, 2)) {
        GLint mask = 0;
        f->glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &mask);
        format.setProfile((mask & GL_CONTEXT_CORE_PROFILE_BIT) ? QSurfaceFormat::CoreProfile
                                                               : QSurfaceFormat::CompatibilityProfile);
    }
}

void QOpenGLContext::Data::freeDead() {
    if (!functions)
        return;
    if (!deadFramebuffers.empty())
        functions->glDeleteFramebuffers(static_cast<GLsizei>(deadFramebuffers.size()), deadFramebuffers.data());
    if (!deadRenderbuffers.empty())
        functions->glDeleteRenderbuffers(static_cast<GLsizei>(deadRenderbuffers.size()), deadRenderbuffers.data());
    deadFramebuffers.clear();
    deadRenderbuffers.clear();
}

void QOpenGLContext::Data::deleteTarget(Target &t, bool current) {
    if (t.fbo) {
        deadFramebuffers.push_back(t.fbo);
        if (t.color)
            deadRenderbuffers.push_back(t.color);
        if (t.depthStencil)
            deadRenderbuffers.push_back(t.depthStencil);
        t.fbo = t.color = t.depthStencil = 0;
    }
    if (current)
        freeDead();
    if (t.pbuffer) {
        const egl::Display &d = egl::display();
        d.egl->DestroySurface(d.display, t.pbuffer);
        t.pbuffer = nullptr;
    }
}

// Makes the framebuffer object of `window` (created or resized to the window) the target of drawing.
bool QOpenGLContext::Data::bindEmulated(QWindow *window) {
    QOpenGLFunctions *f = functions.get();
    Target &t = targets[window];
    const qreal dpr = window->devicePixelRatio() > 0 ? window->devicePixelRatio() : 1.0;
    const QSize size(std::max(1, qRound(window->width() * dpr)), std::max(1, qRound(window->height() * dpr)));
    const bool multisample = !es() || versionAtLeast(format, 3, 0);
    const int samples = multisample ? std::max(0, window->requestedFormat().samples()) : 0;
    if (!t.fbo || t.size != size || t.samples != samples) {
        if (t.fbo)
            deleteTarget(t, true);
        f->glGenFramebuffers(1, &t.fbo);
        f->glGenRenderbuffers(1, &t.color);
        f->glGenRenderbuffers(1, &t.depthStencil);
        using Storage = void(QOPENGLF_APIENTRYP)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
        auto storageMultisample = reinterpret_cast<Storage>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            egl::display().procAddress("glRenderbufferStorageMultisample", !es()));
        const auto storage = [&](GLuint rb, GLenum internal) {
            f->glBindRenderbuffer(GL_RENDERBUFFER, rb);
            if (samples > 0 && storageMultisample)
                storageMultisample(GL_RENDERBUFFER, samples, internal, size.width(), size.height());
            else
                f->glRenderbufferStorage(GL_RENDERBUFFER, internal, size.width(), size.height());
        };
        storage(t.color, GL_RGBA8);
        storage(t.depthStencil, GL_DEPTH24_STENCIL8);
        f->glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
        f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, t.color);
        f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, t.depthStencil);
        f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, t.depthStencil);
        if (f->glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            qWarning("QOpenGLContext: cannot make a framebuffer object for the window");
            return false;
        }
        t.size = size;
        t.samples = samples && storageMultisample ? samples : 0;
        GLint got = 0;
        if (t.samples) {
            f->glBindRenderbuffer(GL_RENDERBUFFER, t.color);
            f->glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &got);
            t.samples = got;
        }
        f->glBindRenderbuffer(GL_RENDERBUFFER, 0);
        f->glClearColor(0, 0, 0, 1);
        f->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }
    f->glBindFramebuffer(GL_FRAMEBUFFER, t.fbo);
    emulatedBound = true;
    QSurfaceFormat actual = format;
    actual.setSamples(t.samples);
    actual.setDepthBufferSize(24);
    actual.setStencilBufferSize(8);
    actual.setRedBufferSize(8);
    actual.setGreenBufferSize(8);
    actual.setBlueBufferSize(8);
    actual.setAlphaBufferSize(8);
    actual.setSwapInterval(window->requestedFormat().swapInterval());
    QtPrivate::GuiAccess::setActualFormat(window, actual);
    return true;
}

/*!
    \class QOpenGLContext
    \inmodule NtGui
    \since 6.0
    \brief The QOpenGLContext class represents a native OpenGL context, which renders with OpenGL on a QSurface.

    Set the format of the context, create it, and make it current on a window
    whose surface type is QSurface::OpenGLSurface, or on a QOffscreenSurface.
    Then call OpenGL through functions(), and show the frame with swapBuffers():

    \code
    QWindow window;
    window.setSurfaceType(QSurface::OpenGLSurface);
    window.show();

    QOpenGLContext context;
    context.setFormat(window.requestedFormat());
    context.create();
    context.makeCurrent(&window);
    QOpenGLFunctions *f = context.functions();
    f->glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
    f->glClear(GL_COLOR_BUFFER_BIT);
    context.swapBuffers(&window);
    \endcode

    QOpenGLWindow does all of this for a window that only renders with
    OpenGL, and QOpenGLPaintDevice lets a QPainter draw with OpenGL.

    Nt reaches OpenGL through EGL, which it loads when the first context is
    created: a program that never uses OpenGL needs no OpenGL library. On the
    \c xcb platform contexts render into the window; on the \c offscreen
    platform, which has no window system, each window renders into a
    framebuffer object, which defaultFramebufferObject() returns while the
    context is current on the window.

    \differences
    Contexts are created through EGL only: there is no GLX, WGL, CGL or
    native handle, no \c QOpenGLContextGroup, no \c globalShareContext() and
    no \c extraFunctions() or versioned function classes. Unless the format
    asks for OpenGL ES, the context is desktop OpenGL when EGL offers it. The
    screen is only remembered.
    \enddifferences

    \sa QOpenGLFunctions, QSurfaceFormat, QOffscreenSurface, QOpenGLWindow
*/

/*!
    \enum QOpenGLContext::OpenGLModuleType

    This enum describes the kind of OpenGL library contexts use by default.

    \value LibGL Desktop OpenGL.
    \value LibGLES OpenGL ES.
*/

/*!
    Constructs a context with the given \a parent. Call create() before using it.
*/
QOpenGLContext::QOpenGLContext(QObject *parent) : QObject(parent), m_d(std::make_unique<Data>()) {
    m_d->screen = QGuiApplication::primaryScreen();
    Registry &r = registry();
    std::lock_guard<std::mutex> lock(r.mutex);
    r.contexts.push_back(this);
}

/*!
    Destroys the context, after emitting aboutToBeDestroyed(). If the context
    is current in this thread, it is first released.
*/
QOpenGLContext::~QOpenGLContext() {
    emit aboutToBeDestroyed();
    {
        Registry &r = registry();
        std::lock_guard<std::mutex> lock(r.mutex);
        r.contexts.erase(std::remove(r.contexts.begin(), r.contexts.end(), this), r.contexts.end());
    }
    if (t_current == this)
        doneCurrent();
    if (m_d->context) {
        const egl::Display &d = egl::display();
        for (auto &entry : m_d->targets)
            if (entry.second.pbuffer)
                d.egl->DestroySurface(d.display, entry.second.pbuffer);
        // The framebuffer objects of the context go with it.
        d.egl->DestroyContext(d.display, m_d->context);
    }
}

/*!
    Sets the \a format the context is created with. Call it before create().

    \sa format(), create()
*/
void QOpenGLContext::setFormat(const QSurfaceFormat &format) { m_d->requested = format; }

/*!
    Makes the context share its textures, buffers and other objects with
    \a shareContext. Call it before create().

    \sa shareContext(), areSharing()
*/
void QOpenGLContext::setShareContext(QOpenGLContext *shareContext) { m_d->share = shareContext; }

/*!
    Sets the \a screen of the context.

    \sa screen()
*/
void QOpenGLContext::setScreen(QScreen *screen) { m_d->screen = screen; }

/*!
    Creates the native context with the format, share context and screen that
    were set, and returns \c true if that worked. If the context exists
    already, it is created anew.

    \sa isValid(), format()
*/
bool QOpenGLContext::create() {
    const egl::Display &d = egl::display();
    if (m_d->context) {
        if (t_current == this)
            doneCurrent();
        d.egl->DestroyContext(d.display, m_d->context);
        m_d->context = nullptr;
        m_d->functions.reset();
        m_d->functionsPrivate.reset();
        m_d->targets.clear();
        m_d->versionKnown = false;
        m_d->extensionsKnown = false;
    }
    if (!d.display) {
        qWarning("QOpenGLContext::create(): %s", qPrintable(d.error));
        return false;
    }
    const QSurfaceFormat &req = m_d->requested;
    if (req.renderableType() == QSurfaceFormat::OpenGL && !d.desktopGL) {
        qWarning("QOpenGLContext::create(): the EGL display has no desktop OpenGL");
        return false;
    }
    const bool es = req.renderableType() == QSurfaceFormat::OpenGLES ||
                    (req.renderableType() != QSurfaceFormat::OpenGL && !d.desktopGL);
    m_d->api = es ? egl::OPENGL_ES_API : egl::OPENGL_API;
    QSurfaceFormat configFormat = req;
    if (!d.windowSurfaces)
        configFormat.setSamples(0); // windows render into framebuffer objects, which have their own samples
    m_d->config = egl::chooseConfig(d, configFormat, m_d->api);
    if (!m_d->config) {
        qWarning("QOpenGLContext::create(): no EGL config matches the format");
        return false;
    }
    std::vector<egl::EGLint> attribs;
    if (es) {
        attribs = {egl::CONTEXT_MAJOR_VERSION, std::max(2, req.majorVersion())};
        if (req.majorVersion() >= 3)
            attribs.insert(attribs.end(), {egl::CONTEXT_MINOR_VERSION, req.minorVersion()});
    } else if (versionAtLeast(req, 3, 2) && req.profile() != QSurfaceFormat::NoProfile) {
        attribs = {egl::CONTEXT_MAJOR_VERSION,
                   req.majorVersion(),
                   egl::CONTEXT_MINOR_VERSION,
                   req.minorVersion(),
                   egl::CONTEXT_OPENGL_PROFILE_MASK,
                   req.profile() == QSurfaceFormat::CoreProfile ? egl::CONTEXT_OPENGL_CORE_PROFILE_BIT
                                                                : egl::CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT};
    } else if (versionAtLeast(req, 3, 0)) {
        attribs = {egl::CONTEXT_MAJOR_VERSION, req.majorVersion(), egl::CONTEXT_MINOR_VERSION, req.minorVersion()};
    }
    if (req.testOption(QSurfaceFormat::DebugContext) && (d.major > 1 || d.minor >= 5))
        attribs.insert(attribs.end(), {egl::CONTEXT_OPENGL_DEBUG, 1});
    attribs.push_back(egl::NONE);
    d.egl->BindAPI(m_d->api);
    egl::EGLContext share = m_d->share ? m_d->share->m_d->context : nullptr;
    m_d->context = d.egl->CreateContext(d.display, m_d->config, share, attribs.data());
    if (!m_d->context) {
        qWarning("QOpenGLContext::create(): cannot create the context (%s)", qPrintable(egl::lastError(d)));
        return false;
    }
    m_d->format = egl::formatOf(d, m_d->config, req);
    m_d->format.setRenderableType(es ? QSurfaceFormat::OpenGLES : QSurfaceFormat::OpenGL);
    if (!d.windowSurfaces)
        m_d->format.setSamples(std::max(0, req.samples()));
    return true;
}

/*!
    Returns \c true if create() made the native context.
*/
bool QOpenGLContext::isValid() const { return m_d->context != nullptr; }

/*!
    Returns the format of the context: the requested format before create(),
    and then the buffers of the native context. Once the context has been made
    current, the version, profile and renderable type are those of the context
    OpenGL gave.

    \sa setFormat()
*/
QSurfaceFormat QOpenGLContext::format() const { return m_d->context ? m_d->format : m_d->requested; }

/*!
    Returns the context set with setShareContext(), or \c nullptr.
*/
QOpenGLContext *QOpenGLContext::shareContext() const { return m_d->share; }

/*!
    Returns the screen of the context: the primary screen unless setScreen()
    set another.
*/
QScreen *QOpenGLContext::screen() const { return m_d->screen; }

/*!
    Returns the name of the framebuffer object that is the window's surface
    while the context is current on a window that has no native OpenGL
    surface, as on the \c offscreen platform; otherwise returns 0, the
    framebuffer of the window. Bind it again after drawing into a framebuffer
    object of your own.

    \sa makeCurrent()
*/
GLuint QOpenGLContext::defaultFramebufferObject() const {
    if (!m_d->surface)
        return 0;
    const auto it = m_d->targets.find(m_d->surface);
    return it != m_d->targets.end() ? it->second.fbo : 0;
}

/*!
    Makes the context current in this thread on \a surface, and returns
    \c true if it succeeded. A window must have the surface type
    QSurface::OpenGLSurface; it is created if it has not been.

    \sa doneCurrent(), currentContext(), swapBuffers()
*/
bool QOpenGLContext::makeCurrent(QSurface *surface) {
    if (!m_d->context || !surface)
        return false;
    if (!surface->supportsOpenGL()) {
        qWarning("QOpenGLContext::makeCurrent(): the surface does not support OpenGL");
        return false;
    }
    const egl::Display &d = egl::display();
    egl::EGLSurface eglSurface = nullptr;
    QWindow *window = surface->surfaceClass() == QSurface::Window ? static_cast<QWindow *>(surface) : nullptr;
    const bool emulated = window && !d.windowSurfaces;
    if (window && !window->handle())
        window->create();
    if (window && !emulated) {
        Registry &r = registry();
        std::lock_guard<std::mutex> lock(r.mutex);
        auto it = r.windowSurfaces.find(surface);
        if (it == r.windowSurfaces.end()) {
            QPlatformWindow *platform = window->handle();
            if (!platform)
                return false;
            // The XCB platform takes a pointer to an xcb_window_t, the X11 platform a pointer to a Window.
            std::uint32_t xcbWindow = static_cast<std::uint32_t>(platform->winId());
            unsigned long xlibWindow = static_cast<unsigned long>(platform->winId());
            void *nativeWindow = d.platform == egl::PLATFORM_X11_KHR ? static_cast<void *>(&xlibWindow)
                                                                     : static_cast<void *>(&xcbWindow);
            egl::EGLSurface created = nullptr;
            if (d.egl->CreatePlatformWindowSurface) {
                const egl::EGLAttrib attribs[] = {egl::NONE};
                created = d.egl->CreatePlatformWindowSurface(d.display, m_d->config, nativeWindow, attribs);
            } else if (d.egl->CreatePlatformWindowSurfaceEXT) {
                const egl::EGLint attribs[] = {egl::NONE};
                created = d.egl->CreatePlatformWindowSurfaceEXT(d.display, m_d->config, nativeWindow, attribs);
            }
            if (!created) {
                qWarning("QOpenGLContext::makeCurrent(): cannot create the window surface (%s)",
                         qPrintable(egl::lastError(d)));
                return false;
            }
            it = r.windowSurfaces.emplace(surface, created).first;
        }
        eglSurface = it->second;
    } else if (!d.surfaceless) {
        Target &t = m_d->targets[surface];
        if (!t.pbuffer) {
            const egl::EGLint attribs[] = {egl::WIDTH, 1, egl::HEIGHT, 1, egl::NONE};
            t.pbuffer = d.egl->CreatePbufferSurface(d.display, m_d->config, attribs);
        }
        eglSurface = t.pbuffer;
    }
    d.egl->BindAPI(m_d->api);
    if (!d.egl->MakeCurrent(d.display, eglSurface, eglSurface, m_d->context)) {
        qWarning("QOpenGLContext::makeCurrent(): %s", qPrintable(egl::lastError(d)));
        return false;
    }
    t_current = this;
    m_d->surface = surface;
    functions();
    if (!m_d->versionKnown)
        m_d->queryVersion();
    m_d->freeDead();
    if (emulated) {
        if (!m_d->bindEmulated(window))
            return false;
    } else if (m_d->emulatedBound) {
        m_d->functions->glBindFramebuffer(GL_FRAMEBUFFER, 0);
        m_d->emulatedBound = false;
    }
    if (window && !emulated) {
        const int interval = window->requestedFormat().swapInterval();
        if (d.egl->SwapInterval && (m_d->swapIntervalSurface != surface || m_d->appliedSwapInterval != interval)) {
            d.egl->SwapInterval(d.display, interval);
            m_d->swapIntervalSurface = surface;
            m_d->appliedSwapInterval = interval;
        }
        QSurfaceFormat actual = egl::formatOf(d, m_d->config, m_d->format);
        actual.setSwapInterval(interval);
        QtPrivate::GuiAccess::setActualFormat(window, actual);
    }
    return true;
}

/*!
    Releases the context from this thread, if it is current here.

    \sa makeCurrent()
*/
void QOpenGLContext::doneCurrent() {
    if (t_current != this)
        return;
    const egl::Display &d = egl::display();
    d.egl->BindAPI(m_d->api);
    d.egl->MakeCurrent(d.display, nullptr, nullptr, nullptr);
    t_current = nullptr;
    m_d->surface = nullptr;
    m_d->emulatedBound = false;
}

/*!
    Shows what was rendered into the window \a surface. The context must be
    current on it. With a swap interval of 1, the default, this waits for the
    display to show the frame; QSurfaceFormat::setSwapInterval() with 0 lets
    rendering run as fast as it can. For an offscreen surface, or a window that
    renders into a framebuffer object, it only flushes OpenGL.

    \sa makeCurrent(), QSurfaceFormat::setSwapInterval()
*/
void QOpenGLContext::swapBuffers(QSurface *surface) {
    if (!m_d->context || !surface)
        return;
    if (t_current != this) {
        qWarning("QOpenGLContext::swapBuffers(): the context is not current");
        return;
    }
    egl::EGLSurface eglSurface = nullptr;
    {
        Registry &r = registry();
        std::lock_guard<std::mutex> lock(r.mutex);
        const auto it = r.windowSurfaces.find(surface);
        if (it != r.windowSurfaces.end())
            eglSurface = it->second;
    }
    const egl::Display &d = egl::display();
    if (eglSurface)
        d.egl->SwapBuffers(d.display, eglSurface);
    else if (m_d->functions)
        m_d->functions->glFlush();
}

/*!
    Returns the address of the OpenGL function \a procName, or \c nullptr if
    there is none.
*/
QFunctionPointer QOpenGLContext::getProcAddress(const char *procName) const {
    return egl::display().procAddress(procName, !m_d->es());
}

/*!
    \overload

    Returns the address of the OpenGL function \a procName, or \c nullptr.
*/
QFunctionPointer QOpenGLContext::getProcAddress(const QByteArray &procName) const {
    return getProcAddress(procName.constData());
}

/*!
    Returns the surface the context is current on, or \c nullptr.
*/
QSurface *QOpenGLContext::surface() const { return m_d->surface; }

/*!
    Returns the context that is current in this thread, or \c nullptr.
*/
QOpenGLContext *QOpenGLContext::currentContext() { return t_current; }

/*!
    Returns \c true if \a first and \a second share their OpenGL objects,
    which is when they are the same context or one was created to share
    with the other, directly or through others.
*/
bool QOpenGLContext::areSharing(QOpenGLContext *first, QOpenGLContext *second) {
    if (!first || !second)
        return false;
    const auto root = [](QOpenGLContext *c) {
        while (c->m_d->share)
            c = c->m_d->share;
        return c;
    };
    return root(first) == root(second);
}

/*!
    Returns the OpenGL ES 2.0 functions of the context. They can be called
    once the context has been made current.
*/
QOpenGLFunctions *QOpenGLContext::functions() const {
    if (!m_d->functions && m_d->context)
        m_d->functions = std::make_unique<QOpenGLFunctions>(const_cast<QOpenGLContext *>(this));
    return m_d->functions.get();
}

/*!
    Returns the names of the OpenGL extensions of the context. The context
    must be current.

    \sa hasExtension()
*/
QSet<QByteArray> QOpenGLContext::extensions() const {
    if (m_d->extensionsKnown || t_current != this)
        return m_d->extensions;
    QOpenGLFunctions *f = m_d->functions.get();
    using GetStringi = const GLubyte *(QOPENGLF_APIENTRYP)(GLenum, GLuint);
    auto getStringi = reinterpret_cast<GetStringi>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        getProcAddress("glGetStringi"));
    if (versionAtLeast(m_d->format, 3, 0) && getStringi) {
        GLint count = 0;
        f->glGetIntegerv(GL_NUM_EXTENSIONS, &count);
        for (GLint i = 0; i < count; ++i)
            if (const GLubyte *name = getStringi(GL_EXTENSIONS, static_cast<GLuint>(i)))
                m_d->extensions.insert(QByteArray(reinterpret_cast<const char *>(name)));
    } else if (const GLubyte *all = f->glGetString(GL_EXTENSIONS)) {
        for (const QByteArray &name : QByteArray(reinterpret_cast<const char *>(all)).split(' '))
            if (!name.isEmpty())
                m_d->extensions.insert(name);
    }
    m_d->extensionsKnown = true;
    return m_d->extensions;
}

/*!
    Returns \c true if the context has the OpenGL extension \a extension. The
    context must be current.

    \sa extensions()
*/
bool QOpenGLContext::hasExtension(const QByteArray &extension) const { return extensions().contains(extension); }

/*!
    Returns \c true if the context is OpenGL ES rather than desktop OpenGL.
*/
bool QOpenGLContext::isOpenGLES() const { return m_d->es(); }

/*!
    Returns the kind of OpenGL that contexts use when their format does not
    ask for one: desktop OpenGL if the EGL display offers it.
*/
QOpenGLContext::OpenGLModuleType QOpenGLContext::openGLModuleType() {
    const egl::Display &d = egl::display();
    return !d.display || d.desktopGL ? LibGL : LibGLES;
}

/*!
    Returns \c true: contexts can be current in threads other than the main
    thread, one thread at a time.
*/
bool QOpenGLContext::supportsThreadedOpenGL() { return true; }

QOpenGLFunctionsPrivate *QtPrivate::GuiAccess::openGLFunctions(QOpenGLContext *context) {
    QOpenGLContext::Data &d = *context->m_d;
    if (!d.functionsPrivate && d.context)
        d.functionsPrivate = std::make_unique<QOpenGLFunctionsPrivate>(context);
    return d.functionsPrivate.get();
}

void QtPrivate::GuiAccess::surfaceAboutToBeDestroyed(QSurface *surface) {
    Registry &r = registry();
    egl::EGLSurface windowSurface = nullptr;
    std::vector<QOpenGLContext *> contexts;
    {
        std::lock_guard<std::mutex> lock(r.mutex);
        const auto it = r.windowSurfaces.find(surface);
        if (it != r.windowSurfaces.end()) {
            windowSurface = it->second;
            r.windowSurfaces.erase(it);
        }
        contexts = r.contexts;
    }
    for (QOpenGLContext *c : contexts) {
        QOpenGLContext::Data &d = *c->m_d;
        const bool current = t_current == c;
        if (d.surface == surface) {
            if (current)
                c->doneCurrent();
            d.surface = nullptr;
        }
        const auto it = d.targets.find(surface);
        if (it != d.targets.end()) {
            d.deleteTarget(it->second, false);
            d.targets.erase(it);
        }
        if (d.swapIntervalSurface == surface)
            d.swapIntervalSurface = nullptr;
    }
    if (windowSurface) {
        const egl::Display &d = egl::display();
        d.egl->DestroySurface(d.display, windowSurface);
    }
}

#include "moc_qopenglcontext.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
