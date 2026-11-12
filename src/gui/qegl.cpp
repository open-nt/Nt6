// MIT License - see LICENSE
#include "qegl_p.h"

#include "qpa/qplatformintegration_p.h"
#include "qwindow_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qlibrary.h>

#include <cstdlib>
#include <cstring>
#include <mutex>

namespace QtPrivate::Egl {

namespace {

template <typename F> void resolve(QLibrary *lib, const char *name, F *out) {
    *out = reinterpret_cast<F>(lib->resolve(name)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

template <typename F> void resolveProc(const Api &api, const char *name, F *out) {
    if (!*out && api.GetProcAddress)
        *out = reinterpret_cast<F>(api.GetProcAddress(name)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

QLibrary *openLibrary(std::initializer_list<std::pair<const char *, int>> names) {
    for (const auto &[name, version] : names) {
        auto *lib =
            version >= 0 ? new QLibrary(QString::fromLatin1(name), version) : new QLibrary(QString::fromLatin1(name));
        if (lib->load())
            return lib; // kept loaded for the life of the process
        delete lib;
    }
    return nullptr;
}

const Api *loadApi() {
    static Api api{};
    static bool loaded = false;
    static std::once_flag once;
    std::call_once(once, [] {
        static QLibrary *lib = openLibrary({{"EGL", 1}, {"EGL", -1}, {"libEGL", -1}}); // kept for the process
        if (!lib)
            return;
        resolve(lib, "eglGetError", &api.GetError);
        resolve(lib, "eglGetPlatformDisplay", &api.GetPlatformDisplay);
        resolve(lib, "eglGetDisplay", &api.GetDisplay);
        resolve(lib, "eglInitialize", &api.Initialize);
        resolve(lib, "eglQueryString", &api.QueryString);
        resolve(lib, "eglGetConfigs", &api.GetConfigs);
        resolve(lib, "eglGetConfigAttrib", &api.GetConfigAttrib);
        resolve(lib, "eglBindAPI", &api.BindAPI);
        resolve(lib, "eglCreateContext", &api.CreateContext);
        resolve(lib, "eglDestroyContext", &api.DestroyContext);
        resolve(lib, "eglCreatePlatformWindowSurface", &api.CreatePlatformWindowSurface);
        resolve(lib, "eglCreatePbufferSurface", &api.CreatePbufferSurface);
        resolve(lib, "eglDestroySurface", &api.DestroySurface);
        resolve(lib, "eglMakeCurrent", &api.MakeCurrent);
        resolve(lib, "eglSwapBuffers", &api.SwapBuffers);
        resolve(lib, "eglSwapInterval", &api.SwapInterval);
        resolve(lib, "eglQuerySurface", &api.QuerySurface);
        resolve(lib, "eglGetProcAddress", &api.GetProcAddress);
        resolveProc(api, "eglGetPlatformDisplayEXT", &api.GetPlatformDisplayEXT);
        resolveProc(api, "eglCreatePlatformWindowSurfaceEXT", &api.CreatePlatformWindowSurfaceEXT);
        loaded = api.GetError && api.Initialize && api.QueryString && api.GetConfigs && api.GetConfigAttrib &&
                 api.BindAPI && api.CreateContext && api.DestroyContext && api.DestroySurface && api.MakeCurrent &&
                 api.SwapBuffers && api.GetProcAddress && (api.GetPlatformDisplay || api.GetPlatformDisplayEXT);
    });
    return loaded ? &api : nullptr;
}

bool hasToken(const char *list, const char *token) {
    if (!list)
        return false;
    const std::size_t n = std::strlen(token);
    for (const char *p = list; (p = std::strstr(p, token)) != nullptr; p += n) {
        const bool startOk = p == list || p[-1] == ' ';
        const bool endOk = p[n] == '\0' || p[n] == ' ';
        if (startOk && endOk)
            return true;
    }
    return false;
}

EGLDisplay platformDisplay(const Api &egl, unsigned platform, void *native, int screen) {
    EGLAttrib attribs[3] = {NONE, 0, 0};
    EGLint intAttribs[3] = {NONE, 0, 0};
    if (screen >= 0 && (platform == PLATFORM_XCB_EXT || platform == PLATFORM_X11_KHR)) {
        const EGLint key = platform == PLATFORM_XCB_EXT ? PLATFORM_XCB_SCREEN_EXT : PLATFORM_X11_SCREEN_KHR;
        attribs[0] = key;
        attribs[1] = screen;
        attribs[2] = NONE;
        intAttribs[0] = key;
        intAttribs[1] = screen;
        intAttribs[2] = NONE;
    }
    if (egl.GetPlatformDisplay)
        return egl.GetPlatformDisplay(platform, native, attribs);
    return egl.GetPlatformDisplayEXT(platform, native, intAttribs);
}

// The first EGL device, for a display without a window system when the Mesa surfaceless platform is missing.
void *firstDevice(const Api &egl) {
    using QueryDevices = EGLBoolean(NT_EGLAPIENTRY *)(EGLint, void **, EGLint *);
    auto query = reinterpret_cast<QueryDevices>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        egl.GetProcAddress("eglQueryDevicesEXT"));
    void *device = nullptr;
    EGLint count = 0;
    if (query && query(1, &device, &count) && count > 0)
        return device;
    return nullptr;
}

// An Xlib display for the X11 platform of EGL, when the EGL library has no XCB platform.
void *openXlibDisplay() {
    static QLibrary *x11 = openLibrary({{"X11", 6}, {"X11", -1}});
    if (!x11)
        return nullptr;
    using OpenDisplay = void *(*)(const char *);
    auto open = reinterpret_cast<OpenDisplay>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        x11->resolve("XOpenDisplay"));
    return open ? open(nullptr) : nullptr;
}

void initialize(Display *d) {
    QPlatformIntegration *integration = QtPrivate::GuiAccess::integration();
    if (!integration) {
        d->error = QStringLiteral("OpenGL needs a QGuiApplication");
        return;
    }
    const QEglNativePlatform native = integration->eglPlatform();
    if (native.platform == 0) {
        d->error = QStringLiteral("the platform has no OpenGL");
        return;
    }
    const Api *egl = loadApi();
    if (!egl) {
        d->error = QStringLiteral("the EGL library (libEGL.so.1) was not found");
        return;
    }
    d->egl = egl;
    d->windowSurfaces = native.windowSurfaces;
    d->visualId = native.visualId;

    // The native platform first; then, only if it fails, Xlib for X11 or the first device for a headless display.
    const auto tryDisplay = [&](unsigned platform, void *nativeDisplay) {
        EGLDisplay display = platformDisplay(*egl, platform, nativeDisplay, native.screen);
        EGLint major = 0, minor = 0;
        if (!display || !egl->Initialize(display, &major, &minor))
            return false;
        d->display = display;
        d->platform = platform;
        d->major = major;
        d->minor = minor;
        return true;
    };
    if (!tryDisplay(native.platform, native.display)) {
        if (native.platform == PLATFORM_XCB_EXT) {
            if (void *xlib = openXlibDisplay())
                tryDisplay(PLATFORM_X11_KHR, xlib);
        } else if (native.platform == PLATFORM_SURFACELESS_MESA) {
            if (void *device = firstDevice(*egl))
                tryDisplay(PLATFORM_DEVICE_EXT, device);
        }
    }
    if (!d->display) {
        d->error = QStringLiteral("no EGL display for the platform (EGL error 0x%1)").arg(egl->GetError(), 0, 16);
        return;
    }
    d->extensions = QByteArray(egl->QueryString(d->display, EXTENSIONS));
    d->desktopGL = hasToken(egl->QueryString(d->display, CLIENT_APIS), "OpenGL");
    d->surfaceless = d->hasExtension("EGL_KHR_surfaceless_context");
    if (d->platform == PLATFORM_DEVICE_EXT || d->platform == PLATFORM_SURFACELESS_MESA)
        d->windowSurfaces = false;
}

EGLint attrib(const Display &d, EGLConfig config, EGLint name) {
    EGLint value = 0;
    d.egl->GetConfigAttrib(d.display, config, name, &value);
    return value;
}

} // namespace

bool Display::hasExtension(const char *name) const { return hasToken(extensions.constData(), name); }

QFunctionPointer Display::procAddress(const char *name, bool desktop) const {
    if (!egl)
        return nullptr;
    // Without EGL_KHR_get_all_proc_addresses, eglGetProcAddress() only knows extensions: look in the library.
    static QLibrary *gl = openLibrary({{"OpenGL", 0}, {"GL", 1}, {"GL", -1}, {"opengl32", -1}});
    static QLibrary *gles = openLibrary({{"GLESv2", 2}, {"GLESv2", -1}, {"libGLESv2", -1}});
    QLibrary *lib = desktop ? gl : gles;
    if (lib)
        if (QFunctionPointer f = lib->resolve(name))
            return f;
    return egl->GetProcAddress(name);
}

const Display &display() {
    static Display d;
    static std::once_flag once;
    std::call_once(once, [] { initialize(&d); });
    return d;
}

EGLConfig chooseConfig(const Display &d, const QSurfaceFormat &format, EGLenum api) {
    if (!d.display)
        return nullptr;
    EGLint count = 0;
    if (!d.egl->GetConfigs(d.display, nullptr, 0, &count) || count <= 0)
        return nullptr;
    std::vector<EGLConfig> configs(static_cast<std::size_t>(count));
    d.egl->GetConfigs(d.display, configs.data(), count, &count);
    const EGLint renderable = api == OPENGL_API            ? OPENGL_BIT
                              : format.majorVersion() >= 3 ? OPENGL_ES3_BIT
                                                           : OPENGL_ES2_BIT;
    const auto wanted = [](int requested, int preferred) { return requested > 0 ? requested : preferred; };
    EGLConfig best = nullptr;
    long bestScore = 0;
    for (EGLConfig c : configs) {
        if (!(attrib(d, c, RENDERABLE_TYPE) & renderable) || attrib(d, c, COLOR_BUFFER_TYPE) != RGB_BUFFER)
            continue;
        if (d.windowSurfaces) {
            if (!(attrib(d, c, SURFACE_TYPE) & WINDOW_BIT))
                continue;
            if (d.visualId != 0 && static_cast<unsigned>(attrib(d, c, NATIVE_VISUAL_ID)) != d.visualId)
                continue;
        }
        const int r = attrib(d, c, RED_SIZE), g = attrib(d, c, GREEN_SIZE), b = attrib(d, c, BLUE_SIZE);
        const int a = attrib(d, c, ALPHA_SIZE), depth = attrib(d, c, DEPTH_SIZE);
        const int stencil = attrib(d, c, STENCIL_SIZE), samples = attrib(d, c, SAMPLES);
        if ((format.alphaBufferSize() > 0 && a < format.alphaBufferSize()) ||
            (format.depthBufferSize() > 0 && depth < format.depthBufferSize()) ||
            (format.stencilBufferSize() > 0 && stencil < format.stencilBufferSize()))
            continue;
        long score = 0;
        score += 100L *
                 (std::abs(r - wanted(format.redBufferSize(), 8)) + std::abs(g - wanted(format.greenBufferSize(), 8)) +
                  std::abs(b - wanted(format.blueBufferSize(), 8)));
        if (format.alphaBufferSize() >= 0)
            score += 100L * std::abs(a - format.alphaBufferSize());
        score += 10L * std::abs(depth - wanted(format.depthBufferSize(), 24));
        score += 10L * std::abs(stencil - wanted(format.stencilBufferSize(), 8));
        const int wantSamples = std::max(0, format.samples());
        score += samples >= wantSamples ? 1000L * (samples - wantSamples) : 100000L * (wantSamples - samples);
        if (attrib(d, c, CONFIG_CAVEAT) == SLOW_CONFIG)
            score += 1000000L;
        if (!best || score < bestScore) {
            best = c;
            bestScore = score;
        }
    }
    return best;
}

QSurfaceFormat formatOf(const Display &d, EGLConfig config, const QSurfaceFormat &base) {
    QSurfaceFormat f = base;
    if (!config)
        return f;
    f.setRedBufferSize(attrib(d, config, RED_SIZE));
    f.setGreenBufferSize(attrib(d, config, GREEN_SIZE));
    f.setBlueBufferSize(attrib(d, config, BLUE_SIZE));
    f.setAlphaBufferSize(attrib(d, config, ALPHA_SIZE));
    f.setDepthBufferSize(attrib(d, config, DEPTH_SIZE));
    f.setStencilBufferSize(attrib(d, config, STENCIL_SIZE));
    f.setSamples(attrib(d, config, SAMPLES));
    return f;
}

QString lastError(const Display &d) {
    if (!d.egl)
        return d.error;
    return QStringLiteral("EGL error 0x%1").arg(d.egl->GetError(), 0, 16);
}

} // namespace QtPrivate::Egl
