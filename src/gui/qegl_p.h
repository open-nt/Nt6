// MIT License - see LICENSE
#pragma once

// EGL, loaded at run time: Nt GUI does not link with an OpenGL library, so programs that never create an OpenGL
// context do not need one installed.

#include <QtCore/qbytearray.h>
#include <QtCore/qglobal.h>
#include <QtCore/qstring.h>
#include <QtGui/qsurfaceformat.h>

#include <cstdint>

#if defined(_WIN32)
#define NT_EGLAPIENTRY __stdcall
#else
#define NT_EGLAPIENTRY
#endif

namespace QtPrivate::Egl {

using EGLDisplay = void *;
using EGLConfig = void *;
using EGLContext = void *;
using EGLSurface = void *;
using EGLint = std::int32_t;
using EGLBoolean = unsigned int;
using EGLenum = unsigned int;
using EGLAttrib = std::intptr_t;

enum : EGLint {
    SUCCESS = 0x3000,
    ALPHA_SIZE = 0x3021,
    BLUE_SIZE = 0x3022,
    GREEN_SIZE = 0x3023,
    RED_SIZE = 0x3024,
    DEPTH_SIZE = 0x3025,
    STENCIL_SIZE = 0x3026,
    CONFIG_CAVEAT = 0x3027,
    NATIVE_VISUAL_ID = 0x302E,
    SAMPLES = 0x3031,
    SAMPLE_BUFFERS = 0x3032,
    SURFACE_TYPE = 0x3033,
    NONE = 0x3038,
    COLOR_BUFFER_TYPE = 0x303F,
    RENDERABLE_TYPE = 0x3040,
    SLOW_CONFIG = 0x3050,
    VENDOR = 0x3053,
    VERSION = 0x3054,
    EXTENSIONS = 0x3055,
    HEIGHT = 0x3056,
    WIDTH = 0x3057,
    BACK_BUFFER = 0x3084,
    SINGLE_BUFFER = 0x3085,
    RENDER_BUFFER = 0x3086,
    CLIENT_APIS = 0x308D,
    RGB_BUFFER = 0x308E,
    CONTEXT_MAJOR_VERSION = 0x3098,
    CONTEXT_MINOR_VERSION = 0x30FB,
    CONTEXT_FLAGS_KHR = 0x30FC,
    CONTEXT_OPENGL_PROFILE_MASK = 0x30FD,
    CONTEXT_OPENGL_DEBUG = 0x31B0,
    PLATFORM_X11_KHR = 0x31D5,
    PLATFORM_X11_SCREEN_KHR = 0x31D6,
    PLATFORM_XCB_EXT = 0x31DC,
    PLATFORM_SURFACELESS_MESA = 0x31DD,
    PLATFORM_XCB_SCREEN_EXT = 0x31DE,
    PLATFORM_DEVICE_EXT = 0x313F,
};
enum : EGLint { PBUFFER_BIT = 0x0001, WINDOW_BIT = 0x0004 };
enum : EGLint { OPENGL_ES2_BIT = 0x0004, OPENGL_BIT = 0x0008, OPENGL_ES3_BIT = 0x0040 };
enum : EGLint { CONTEXT_OPENGL_CORE_PROFILE_BIT = 0x1, CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT = 0x2 };
enum : EGLenum { OPENGL_ES_API = 0x30A0, OPENGL_API = 0x30A2 };

// The EGL functions Nt uses.
struct Api {
    EGLint(NT_EGLAPIENTRY *GetError)();
    EGLDisplay(NT_EGLAPIENTRY *GetPlatformDisplay)(EGLenum, void *, const EGLAttrib *);
    EGLDisplay(NT_EGLAPIENTRY *GetPlatformDisplayEXT)(EGLenum, void *, const EGLint *);
    EGLDisplay(NT_EGLAPIENTRY *GetDisplay)(void *);
    EGLBoolean(NT_EGLAPIENTRY *Initialize)(EGLDisplay, EGLint *, EGLint *);
    const char *(NT_EGLAPIENTRY *QueryString)(EGLDisplay, EGLint);
    EGLBoolean(NT_EGLAPIENTRY *GetConfigs)(EGLDisplay, EGLConfig *, EGLint, EGLint *);
    EGLBoolean(NT_EGLAPIENTRY *GetConfigAttrib)(EGLDisplay, EGLConfig, EGLint, EGLint *);
    EGLBoolean(NT_EGLAPIENTRY *BindAPI)(EGLenum);
    EGLContext(NT_EGLAPIENTRY *CreateContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
    EGLBoolean(NT_EGLAPIENTRY *DestroyContext)(EGLDisplay, EGLContext);
    EGLSurface(NT_EGLAPIENTRY *CreatePlatformWindowSurface)(EGLDisplay, EGLConfig, void *, const EGLAttrib *);
    EGLSurface(NT_EGLAPIENTRY *CreatePlatformWindowSurfaceEXT)(EGLDisplay, EGLConfig, void *, const EGLint *);
    EGLSurface(NT_EGLAPIENTRY *CreatePbufferSurface)(EGLDisplay, EGLConfig, const EGLint *);
    EGLBoolean(NT_EGLAPIENTRY *DestroySurface)(EGLDisplay, EGLSurface);
    EGLBoolean(NT_EGLAPIENTRY *MakeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
    EGLBoolean(NT_EGLAPIENTRY *SwapBuffers)(EGLDisplay, EGLSurface);
    EGLBoolean(NT_EGLAPIENTRY *SwapInterval)(EGLDisplay, EGLint);
    EGLBoolean(NT_EGLAPIENTRY *QuerySurface)(EGLDisplay, EGLSurface, EGLint, EGLint *);
    QFunctionPointer(NT_EGLAPIENTRY *GetProcAddress)(const char *);
};

// The EGL display of the application's platform, initialized when OpenGL is first used.
struct Display {
    const Api *egl = nullptr;
    EGLDisplay display = nullptr;
    int major = 0;
    int minor = 0;
    QByteArray extensions;    // of the display
    bool desktopGL = false;   // the display offers OpenGL, not only OpenGL ES
    bool surfaceless = false; // contexts can be made current without a surface
    bool windowSurfaces = false;
    unsigned visualId = 0;
    unsigned platform = 0;
    QString error; // why there is no display

    bool hasExtension(const char *name) const;
    // Returns the EGL function or the OpenGL function `name`, or null.
    QFunctionPointer procAddress(const char *name, bool desktopGL) const;
};

// The display, or one whose `display` is null with the reason in `error`.
const Display &display();

// The config that matches `format` best for a context of `api` (OPENGL_API or OPENGL_ES_API); null if none.
EGLConfig chooseConfig(const Display &d, const QSurfaceFormat &format, EGLenum api);
// The format of the buffers of `config`, with the rest taken from `base`.
QSurfaceFormat formatOf(const Display &d, EGLConfig config, const QSurfaceFormat &base);
// A readable message for the last EGL error.
QString lastError(const Display &d);

} // namespace QtPrivate::Egl
