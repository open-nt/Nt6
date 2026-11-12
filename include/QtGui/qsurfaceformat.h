// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qglobal.h>

#include <utility>

class QSurfaceFormat {
public:
    enum FormatOption {
        StereoBuffers = 0x0001,
        DebugContext = 0x0002,
        DeprecatedFunctions = 0x0004,
        ResetNotification = 0x0008,
        ProtectedContent = 0x0010
    };
    Q_DECLARE_FLAGS(FormatOptions, FormatOption)

    enum SwapBehavior { DefaultSwapBehavior, SingleBuffer, DoubleBuffer, TripleBuffer };
    enum RenderableType { DefaultRenderableType = 0x0, OpenGL = 0x1, OpenGLES = 0x2, OpenVG = 0x4 };
    enum OpenGLContextProfile { NoProfile, CoreProfile, CompatibilityProfile };

    QSurfaceFormat() = default;
    QSurfaceFormat(FormatOptions options); // NOLINT(google-explicit-constructor): implicit, as in Qt

    void setDepthBufferSize(int size) { m_depth = size; }
    int depthBufferSize() const { return m_depth; }
    void setStencilBufferSize(int size) { m_stencil = size; }
    int stencilBufferSize() const { return m_stencil; }
    void setRedBufferSize(int size) { m_red = size; }
    int redBufferSize() const { return m_red; }
    void setGreenBufferSize(int size) { m_green = size; }
    int greenBufferSize() const { return m_green; }
    void setBlueBufferSize(int size) { m_blue = size; }
    int blueBufferSize() const { return m_blue; }
    void setAlphaBufferSize(int size) { m_alpha = size; }
    int alphaBufferSize() const { return m_alpha; }
    void setSamples(int numSamples) { m_samples = numSamples; }
    int samples() const { return m_samples; }
    void setSwapBehavior(SwapBehavior behavior) { m_swapBehavior = behavior; }
    SwapBehavior swapBehavior() const { return m_swapBehavior; }
    bool hasAlpha() const { return m_alpha > 0; }

    void setProfile(OpenGLContextProfile profile) { m_profile = profile; }
    OpenGLContextProfile profile() const { return m_profile; }
    void setRenderableType(RenderableType type) { m_renderableType = type; }
    RenderableType renderableType() const { return m_renderableType; }
    void setMajorVersion(int majorVersion) { m_major = majorVersion; }
    int majorVersion() const { return m_major; }
    void setMinorVersion(int minorVersion) { m_minor = minorVersion; }
    int minorVersion() const { return m_minor; }
    std::pair<int, int> version() const { return {m_major, m_minor}; }
    void setVersion(int major, int minor) {
        m_major = major;
        m_minor = minor;
    }
    bool stereo() const { return testOption(StereoBuffers); }
    void setStereo(bool enable) { setOption(StereoBuffers, enable); }

    void setOptions(QSurfaceFormat::FormatOptions options) { m_options = options; }
    void setOption(FormatOption option, bool on = true);
    bool testOption(FormatOption option) const { return m_options.testFlag(option); }
    QSurfaceFormat::FormatOptions options() const { return m_options; }

    int swapInterval() const { return m_swapInterval; }
    void setSwapInterval(int interval) { m_swapInterval = interval; }

    static void setDefaultFormat(const QSurfaceFormat &format);
    static QSurfaceFormat defaultFormat();

    friend bool operator==(const QSurfaceFormat &a, const QSurfaceFormat &b);
    friend bool operator!=(const QSurfaceFormat &a, const QSurfaceFormat &b) { return !(a == b); }

private:
    int m_depth = -1;
    int m_stencil = -1;
    int m_red = -1;
    int m_green = -1;
    int m_blue = -1;
    int m_alpha = -1;
    int m_samples = -1;
    SwapBehavior m_swapBehavior = DefaultSwapBehavior;
    OpenGLContextProfile m_profile = NoProfile;
    RenderableType m_renderableType = DefaultRenderableType;
    int m_major = 2;
    int m_minor = 0;
    FormatOptions m_options;
    int m_swapInterval = 1;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QSurfaceFormat::FormatOptions)
