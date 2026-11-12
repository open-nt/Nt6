// MIT License - see LICENSE
#include <QtGui/qopenglfunctions.h>

#include <QtCore/qdebug.h>
#include <QtGui/qopenglcontext.h>

#include "qwindow_p.h"

namespace {

template <typename F> void resolve(QOpenGLContext *context, const char *name, F *out) {
    *out = reinterpret_cast<F>(context->getProcAddress(name)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

// Desktop OpenGL before 4.1 has the double versions of these two, and none of the shader compiler functions of
// OpenGL ES.
using ClearDepth = void(QOPENGLF_APIENTRYP)(GLdouble);
using DepthRange = void(QOPENGLF_APIENTRYP)(GLdouble, GLdouble);
ClearDepth s_clearDepth = nullptr;
DepthRange s_depthRange = nullptr;
void QOPENGLF_APIENTRY clearDepthf(GLfloat d) { s_clearDepth(static_cast<GLdouble>(d)); }
void QOPENGLF_APIENTRY depthRangef(GLfloat n, GLfloat f) {
    s_depthRange(static_cast<GLdouble>(n), static_cast<GLdouble>(f));
}
void QOPENGLF_APIENTRY releaseShaderCompiler() {}
void QOPENGLF_APIENTRY shaderBinary(GLsizei, const GLuint *, GLenum, const void *, GLsizei) {}
void QOPENGLF_APIENTRY getShaderPrecisionFormat(GLenum, GLenum, GLint *range, GLint *precision) {
    // What OpenGL ES requires of highp floats.
    range[0] = 127;
    range[1] = 127;
    *precision = 23;
}

} // namespace

/*!
    \class QOpenGLFunctionsPrivate
    \internal
    \brief The function pointers of one context, behind QOpenGLFunctions.
*/

/*!
    \class QOpenGLFunctionsPrivate::Functions
    \internal
*/

/*!
    \internal
    Resolves the functions of \a context.
*/
QOpenGLFunctionsPrivate::QOpenGLFunctionsPrivate(QOpenGLContext *context) : f{} {
    resolve(context, "glActiveTexture", &f.ActiveTexture);
    resolve(context, "glAttachShader", &f.AttachShader);
    resolve(context, "glBindAttribLocation", &f.BindAttribLocation);
    resolve(context, "glBindBuffer", &f.BindBuffer);
    resolve(context, "glBindFramebuffer", &f.BindFramebuffer);
    resolve(context, "glBindRenderbuffer", &f.BindRenderbuffer);
    resolve(context, "glBindTexture", &f.BindTexture);
    resolve(context, "glBlendColor", &f.BlendColor);
    resolve(context, "glBlendEquation", &f.BlendEquation);
    resolve(context, "glBlendEquationSeparate", &f.BlendEquationSeparate);
    resolve(context, "glBlendFunc", &f.BlendFunc);
    resolve(context, "glBlendFuncSeparate", &f.BlendFuncSeparate);
    resolve(context, "glBufferData", &f.BufferData);
    resolve(context, "glBufferSubData", &f.BufferSubData);
    resolve(context, "glCheckFramebufferStatus", &f.CheckFramebufferStatus);
    resolve(context, "glClear", &f.Clear);
    resolve(context, "glClearColor", &f.ClearColor);
    resolve(context, "glClearDepthf", &f.ClearDepthf);
    resolve(context, "glClearStencil", &f.ClearStencil);
    resolve(context, "glColorMask", &f.ColorMask);
    resolve(context, "glCompileShader", &f.CompileShader);
    resolve(context, "glCompressedTexImage2D", &f.CompressedTexImage2D);
    resolve(context, "glCompressedTexSubImage2D", &f.CompressedTexSubImage2D);
    resolve(context, "glCopyTexImage2D", &f.CopyTexImage2D);
    resolve(context, "glCopyTexSubImage2D", &f.CopyTexSubImage2D);
    resolve(context, "glCreateProgram", &f.CreateProgram);
    resolve(context, "glCreateShader", &f.CreateShader);
    resolve(context, "glCullFace", &f.CullFace);
    resolve(context, "glDeleteBuffers", &f.DeleteBuffers);
    resolve(context, "glDeleteFramebuffers", &f.DeleteFramebuffers);
    resolve(context, "glDeleteProgram", &f.DeleteProgram);
    resolve(context, "glDeleteRenderbuffers", &f.DeleteRenderbuffers);
    resolve(context, "glDeleteShader", &f.DeleteShader);
    resolve(context, "glDeleteTextures", &f.DeleteTextures);
    resolve(context, "glDepthFunc", &f.DepthFunc);
    resolve(context, "glDepthMask", &f.DepthMask);
    resolve(context, "glDepthRangef", &f.DepthRangef);
    resolve(context, "glDetachShader", &f.DetachShader);
    resolve(context, "glDisable", &f.Disable);
    resolve(context, "glDisableVertexAttribArray", &f.DisableVertexAttribArray);
    resolve(context, "glDrawArrays", &f.DrawArrays);
    resolve(context, "glDrawElements", &f.DrawElements);
    resolve(context, "glEnable", &f.Enable);
    resolve(context, "glEnableVertexAttribArray", &f.EnableVertexAttribArray);
    resolve(context, "glFinish", &f.Finish);
    resolve(context, "glFlush", &f.Flush);
    resolve(context, "glFramebufferRenderbuffer", &f.FramebufferRenderbuffer);
    resolve(context, "glFramebufferTexture2D", &f.FramebufferTexture2D);
    resolve(context, "glFrontFace", &f.FrontFace);
    resolve(context, "glGenBuffers", &f.GenBuffers);
    resolve(context, "glGenerateMipmap", &f.GenerateMipmap);
    resolve(context, "glGenFramebuffers", &f.GenFramebuffers);
    resolve(context, "glGenRenderbuffers", &f.GenRenderbuffers);
    resolve(context, "glGenTextures", &f.GenTextures);
    resolve(context, "glGetActiveAttrib", &f.GetActiveAttrib);
    resolve(context, "glGetActiveUniform", &f.GetActiveUniform);
    resolve(context, "glGetAttachedShaders", &f.GetAttachedShaders);
    resolve(context, "glGetAttribLocation", &f.GetAttribLocation);
    resolve(context, "glGetBooleanv", &f.GetBooleanv);
    resolve(context, "glGetBufferParameteriv", &f.GetBufferParameteriv);
    resolve(context, "glGetError", &f.GetError);
    resolve(context, "glGetFloatv", &f.GetFloatv);
    resolve(context, "glGetFramebufferAttachmentParameteriv", &f.GetFramebufferAttachmentParameteriv);
    resolve(context, "glGetIntegerv", &f.GetIntegerv);
    resolve(context, "glGetProgramiv", &f.GetProgramiv);
    resolve(context, "glGetProgramInfoLog", &f.GetProgramInfoLog);
    resolve(context, "glGetRenderbufferParameteriv", &f.GetRenderbufferParameteriv);
    resolve(context, "glGetShaderiv", &f.GetShaderiv);
    resolve(context, "glGetShaderInfoLog", &f.GetShaderInfoLog);
    resolve(context, "glGetShaderPrecisionFormat", &f.GetShaderPrecisionFormat);
    resolve(context, "glGetShaderSource", &f.GetShaderSource);
    resolve(context, "glGetString", &f.GetString);
    resolve(context, "glGetTexParameterfv", &f.GetTexParameterfv);
    resolve(context, "glGetTexParameteriv", &f.GetTexParameteriv);
    resolve(context, "glGetUniformfv", &f.GetUniformfv);
    resolve(context, "glGetUniformiv", &f.GetUniformiv);
    resolve(context, "glGetUniformLocation", &f.GetUniformLocation);
    resolve(context, "glGetVertexAttribfv", &f.GetVertexAttribfv);
    resolve(context, "glGetVertexAttribiv", &f.GetVertexAttribiv);
    resolve(context, "glGetVertexAttribPointerv", &f.GetVertexAttribPointerv);
    resolve(context, "glHint", &f.Hint);
    resolve(context, "glIsBuffer", &f.IsBuffer);
    resolve(context, "glIsEnabled", &f.IsEnabled);
    resolve(context, "glIsFramebuffer", &f.IsFramebuffer);
    resolve(context, "glIsProgram", &f.IsProgram);
    resolve(context, "glIsRenderbuffer", &f.IsRenderbuffer);
    resolve(context, "glIsShader", &f.IsShader);
    resolve(context, "glIsTexture", &f.IsTexture);
    resolve(context, "glLineWidth", &f.LineWidth);
    resolve(context, "glLinkProgram", &f.LinkProgram);
    resolve(context, "glPixelStorei", &f.PixelStorei);
    resolve(context, "glPolygonOffset", &f.PolygonOffset);
    resolve(context, "glReadPixels", &f.ReadPixels);
    resolve(context, "glReleaseShaderCompiler", &f.ReleaseShaderCompiler);
    resolve(context, "glRenderbufferStorage", &f.RenderbufferStorage);
    resolve(context, "glSampleCoverage", &f.SampleCoverage);
    resolve(context, "glScissor", &f.Scissor);
    resolve(context, "glShaderBinary", &f.ShaderBinary);
    resolve(context, "glShaderSource", &f.ShaderSource);
    resolve(context, "glStencilFunc", &f.StencilFunc);
    resolve(context, "glStencilFuncSeparate", &f.StencilFuncSeparate);
    resolve(context, "glStencilMask", &f.StencilMask);
    resolve(context, "glStencilMaskSeparate", &f.StencilMaskSeparate);
    resolve(context, "glStencilOp", &f.StencilOp);
    resolve(context, "glStencilOpSeparate", &f.StencilOpSeparate);
    resolve(context, "glTexImage2D", &f.TexImage2D);
    resolve(context, "glTexParameterf", &f.TexParameterf);
    resolve(context, "glTexParameterfv", &f.TexParameterfv);
    resolve(context, "glTexParameteri", &f.TexParameteri);
    resolve(context, "glTexParameteriv", &f.TexParameteriv);
    resolve(context, "glTexSubImage2D", &f.TexSubImage2D);
    resolve(context, "glUniform1f", &f.Uniform1f);
    resolve(context, "glUniform1fv", &f.Uniform1fv);
    resolve(context, "glUniform1i", &f.Uniform1i);
    resolve(context, "glUniform1iv", &f.Uniform1iv);
    resolve(context, "glUniform2f", &f.Uniform2f);
    resolve(context, "glUniform2fv", &f.Uniform2fv);
    resolve(context, "glUniform2i", &f.Uniform2i);
    resolve(context, "glUniform2iv", &f.Uniform2iv);
    resolve(context, "glUniform3f", &f.Uniform3f);
    resolve(context, "glUniform3fv", &f.Uniform3fv);
    resolve(context, "glUniform3i", &f.Uniform3i);
    resolve(context, "glUniform3iv", &f.Uniform3iv);
    resolve(context, "glUniform4f", &f.Uniform4f);
    resolve(context, "glUniform4fv", &f.Uniform4fv);
    resolve(context, "glUniform4i", &f.Uniform4i);
    resolve(context, "glUniform4iv", &f.Uniform4iv);
    resolve(context, "glUniformMatrix2fv", &f.UniformMatrix2fv);
    resolve(context, "glUniformMatrix3fv", &f.UniformMatrix3fv);
    resolve(context, "glUniformMatrix4fv", &f.UniformMatrix4fv);
    resolve(context, "glUseProgram", &f.UseProgram);
    resolve(context, "glValidateProgram", &f.ValidateProgram);
    resolve(context, "glVertexAttrib1f", &f.VertexAttrib1f);
    resolve(context, "glVertexAttrib1fv", &f.VertexAttrib1fv);
    resolve(context, "glVertexAttrib2f", &f.VertexAttrib2f);
    resolve(context, "glVertexAttrib2fv", &f.VertexAttrib2fv);
    resolve(context, "glVertexAttrib3f", &f.VertexAttrib3f);
    resolve(context, "glVertexAttrib3fv", &f.VertexAttrib3fv);
    resolve(context, "glVertexAttrib4f", &f.VertexAttrib4f);
    resolve(context, "glVertexAttrib4fv", &f.VertexAttrib4fv);
    resolve(context, "glVertexAttribPointer", &f.VertexAttribPointer);
    resolve(context, "glViewport", &f.Viewport);
    if (!context->isOpenGLES()) {
        if (!f.ClearDepthf) {
            resolve(context, "glClearDepth", &s_clearDepth);
            if (s_clearDepth)
                f.ClearDepthf = clearDepthf;
        }
        if (!f.DepthRangef) {
            resolve(context, "glDepthRange", &s_depthRange);
            if (s_depthRange)
                f.DepthRangef = depthRangef;
        }
        if (!f.ReleaseShaderCompiler)
            f.ReleaseShaderCompiler = releaseShaderCompiler;
        if (!f.ShaderBinary)
            f.ShaderBinary = shaderBinary;
        if (!f.GetShaderPrecisionFormat)
            f.GetShaderPrecisionFormat = getShaderPrecisionFormat;
    }
}

/*!
    \class QOpenGLFunctions
    \inmodule NtGui
    \since 6.0
    \brief The QOpenGLFunctions class provides the OpenGL ES 2.0 API on desktop OpenGL and OpenGL ES alike.

    Each function calls the OpenGL function of the same name in the current
    context. Get the functions of a context with QOpenGLContext::functions(),
    or derive from the class and call initializeOpenGLFunctions() once a
    context is current:

    \code
    class Renderer : protected QOpenGLFunctions {
    public:
        void initialize() { initializeOpenGLFunctions(); }
        void render() {
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    };
    \endcode

    The types and constants come from \c <QtGui/qopengl.h>, which Nt
    provides: the system's OpenGL headers are not needed.

    \differences
    The functions are resolved through EGL. \c <QtGui/qopengl.h> declares
    the types and constants of OpenGL ES 3.0 and a few of desktop OpenGL
    rather than including the system's headers. On desktop OpenGL before 4.1,
    \c glReleaseShaderCompiler() and \c glShaderBinary() do nothing and
    \c glGetShaderPrecisionFormat() reports single precision.
    \enddifferences

    \sa QOpenGLContext
*/

/*!
    \enum QOpenGLFunctions::OpenGLFeature

    This enum describes the features of OpenGL ES 2.0 that a context may have.

    \value Multitexture glActiveTexture() is available.
    \value Shaders Shader functions are available.
    \value Buffers Vertex and index buffer functions are available.
    \value Framebuffers Framebuffer object functions are available.
    \value BlendColor glBlendColor() is available.
    \value BlendEquation glBlendEquation() is available.
    \value BlendEquationSeparate glBlendEquationSeparate() is available.
    \value BlendFuncSeparate glBlendFuncSeparate() is available.
    \value BlendSubtract Blend subtraction is available.
    \value CompressedTextures Compressed texture functions are available.
    \value Multisample glSampleCoverage() is available.
    \value StencilSeparate Separate stencil functions are available.
    \value NPOTTextures Textures whose sizes are not powers of two are available.
    \value NPOTTextureRepeat Such textures can also repeat.
    \value FixedFunctionPipeline The fixed function pipeline is available.
    \value TextureRGFormats The \c GL_RED and \c GL_RG texture formats are available.
    \value MultipleRenderTargets Framebuffer objects can have several color attachments.
    \value BlendEquationAdvanced The advanced blend equations are available.
*/

/*!
    Constructs a function object that is not initialized yet. Call
    initializeOpenGLFunctions() when a context is current.
*/
QOpenGLFunctions::QOpenGLFunctions() = default;

/*!
    Constructs a function object for \a context, which must have been created.
*/
QOpenGLFunctions::QOpenGLFunctions(QOpenGLContext *context)
    : d_ptr(context ? QtPrivate::GuiAccess::openGLFunctions(context) : nullptr) {}

/*!
    \fn QOpenGLFunctions::~QOpenGLFunctions()

    Destroys the function object.
*/

/*!
    \fn bool QOpenGLFunctions::isInitialized(const QOpenGLFunctionsPrivate *d)

    Returns \c true if \a d has been initialized.
*/

/*!
    \variable QOpenGLFunctions::d_ptr
    \internal
*/

/*!
    Initializes the functions for the current context.
*/
void QOpenGLFunctions::initializeOpenGLFunctions() {
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context) {
        qWarning("QOpenGLFunctions::initializeOpenGLFunctions(): no context is current");
        return;
    }
    d_ptr = QtPrivate::GuiAccess::openGLFunctions(context);
}

/*!
    Returns the features of the current context.

    \sa hasOpenGLFeature()
*/
QOpenGLFunctions::OpenGLFeatures QOpenGLFunctions::openGLFeatures() const {
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context)
        return {};
    OpenGLFeatures features = Multitexture | Shaders | Buffers | Framebuffers | BlendColor | BlendEquation |
                              BlendEquationSeparate | BlendFuncSeparate | BlendSubtract | CompressedTextures |
                              Multisample | StencilSeparate | NPOTTextures;
    const QSurfaceFormat f = context->format();
    const bool three = f.majorVersion() >= 3;
    if (!context->isOpenGLES() || three || context->hasExtension("GL_OES_texture_npot"))
        features |= NPOTTextureRepeat;
    if (three || context->hasExtension("GL_EXT_texture_rg") || context->hasExtension("GL_ARB_texture_rg"))
        features |= TextureRGFormats;
    if (three)
        features |= MultipleRenderTargets;
    if (!context->isOpenGLES() && f.profile() != QSurfaceFormat::CoreProfile)
        features |= FixedFunctionPipeline;
    if (context->hasExtension("GL_KHR_blend_equation_advanced") ||
        context->hasExtension("GL_NV_blend_equation_advanced"))
        features |= BlendEquationAdvanced;
    return features;
}

/*!
    Returns \c true if the current context has \a feature.

    \sa openGLFeatures()
*/
bool QOpenGLFunctions::hasOpenGLFeature(QOpenGLFunctions::OpenGLFeature feature) const {
    return openGLFeatures().testFlag(feature);
}

/*!
    \fn void QOpenGLFunctions::glActiveTexture(GLenum texture)

    Calls the OpenGL function \c glActiveTexture with \a texture.
*/

/*!
    \fn void QOpenGLFunctions::glAttachShader(GLuint program, GLuint shader)

    Calls the OpenGL function \c glAttachShader with \a program and \a shader.
*/

/*!
    \fn void QOpenGLFunctions::glBindAttribLocation(GLuint program, GLuint index, const GLchar *name)

    Calls the OpenGL function \c glBindAttribLocation with \a program, \a index and \a name.
*/

/*!
    \fn void QOpenGLFunctions::glBindBuffer(GLenum target, GLuint buffer)

    Calls the OpenGL function \c glBindBuffer with \a target and \a buffer.
*/

/*!
    \fn void QOpenGLFunctions::glBindFramebuffer(GLenum target, GLuint framebuffer)

    Calls the OpenGL function \c glBindFramebuffer with \a target and \a framebuffer.
*/

/*!
    \fn void QOpenGLFunctions::glBindRenderbuffer(GLenum target, GLuint renderbuffer)

    Calls the OpenGL function \c glBindRenderbuffer with \a target and \a renderbuffer.
*/

/*!
    \fn void QOpenGLFunctions::glBindTexture(GLenum target, GLuint texture)

    Calls the OpenGL function \c glBindTexture with \a target and \a texture.
*/

/*!
    \fn void QOpenGLFunctions::glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)

    Calls the OpenGL function \c glBlendColor with \a red, \a green, \a blue and \a alpha.
*/

/*!
    \fn void QOpenGLFunctions::glBlendEquation(GLenum mode)

    Calls the OpenGL function \c glBlendEquation with \a mode.
*/

/*!
    \fn void QOpenGLFunctions::glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)

    Calls the OpenGL function \c glBlendEquationSeparate with \a modeRGB and \a modeAlpha.
*/

/*!
    \fn void QOpenGLFunctions::glBlendFunc(GLenum sfactor, GLenum dfactor)

    Calls the OpenGL function \c glBlendFunc with \a sfactor and \a dfactor.
*/

/*!
    \fn void QOpenGLFunctions::glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha)

    Calls the OpenGL function \c glBlendFuncSeparate with \a sfactorRGB, \a dfactorRGB, \a sfactorAlpha and \a dfactorAlpha.
*/

/*!
    \fn void QOpenGLFunctions::glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)

    Calls the OpenGL function \c glBufferData with \a target, \a size, \a data and \a usage.
*/

/*!
    \fn void QOpenGLFunctions::glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)

    Calls the OpenGL function \c glBufferSubData with \a target, \a offset, \a size and \a data.
*/

/*!
    \fn GLenum QOpenGLFunctions::glCheckFramebufferStatus(GLenum target)

    Returns what the OpenGL function \c glCheckFramebufferStatus returns when it is called with \a target.
*/

/*!
    \fn void QOpenGLFunctions::glClear(GLbitfield mask)

    Calls the OpenGL function \c glClear with \a mask.
*/

/*!
    \fn void QOpenGLFunctions::glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)

    Calls the OpenGL function \c glClearColor with \a red, \a green, \a blue and \a alpha.
*/

/*!
    \fn void QOpenGLFunctions::glClearDepthf(GLfloat d)

    Calls the OpenGL function \c glClearDepthf with \a d.
*/

/*!
    \fn void QOpenGLFunctions::glClearStencil(GLint s)

    Calls the OpenGL function \c glClearStencil with \a s.
*/

/*!
    \fn void QOpenGLFunctions::glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)

    Calls the OpenGL function \c glColorMask with \a red, \a green, \a blue and \a alpha.
*/

/*!
    \fn void QOpenGLFunctions::glCompileShader(GLuint shader)

    Calls the OpenGL function \c glCompileShader with \a shader.
*/

/*!
    \fn void QOpenGLFunctions::glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data)

    Calls the OpenGL function \c glCompressedTexImage2D with \a target, \a level, \a internalformat, \a width, \a height, \a border, \a imageSize and \a data.
*/

/*!
    \fn void QOpenGLFunctions::glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data)

    Calls the OpenGL function \c glCompressedTexSubImage2D with \a target, \a level, \a xoffset, \a yoffset, \a width, \a height, \a format, \a imageSize and \a data.
*/

/*!
    \fn void QOpenGLFunctions::glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)

    Calls the OpenGL function \c glCopyTexImage2D with \a target, \a level, \a internalformat, \a x, \a y, \a width, \a height and \a border.
*/

/*!
    \fn void QOpenGLFunctions::glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)

    Calls the OpenGL function \c glCopyTexSubImage2D with \a target, \a level, \a xoffset, \a yoffset, \a x, \a y, \a width and \a height.
*/

/*!
    \fn GLuint QOpenGLFunctions::glCreateProgram()

    Returns what the OpenGL function \c glCreateProgram returns when it is called.
*/

/*!
    \fn GLuint QOpenGLFunctions::glCreateShader(GLenum type)

    Returns what the OpenGL function \c glCreateShader returns when it is called with \a type.
*/

/*!
    \fn void QOpenGLFunctions::glCullFace(GLenum mode)

    Calls the OpenGL function \c glCullFace with \a mode.
*/

/*!
    \fn void QOpenGLFunctions::glDeleteBuffers(GLsizei n, const GLuint *buffers)

    Calls the OpenGL function \c glDeleteBuffers with \a n and \a buffers.
*/

/*!
    \fn void QOpenGLFunctions::glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)

    Calls the OpenGL function \c glDeleteFramebuffers with \a n and \a framebuffers.
*/

/*!
    \fn void QOpenGLFunctions::glDeleteProgram(GLuint program)

    Calls the OpenGL function \c glDeleteProgram with \a program.
*/

/*!
    \fn void QOpenGLFunctions::glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)

    Calls the OpenGL function \c glDeleteRenderbuffers with \a n and \a renderbuffers.
*/

/*!
    \fn void QOpenGLFunctions::glDeleteShader(GLuint shader)

    Calls the OpenGL function \c glDeleteShader with \a shader.
*/

/*!
    \fn void QOpenGLFunctions::glDeleteTextures(GLsizei n, const GLuint *textures)

    Calls the OpenGL function \c glDeleteTextures with \a n and \a textures.
*/

/*!
    \fn void QOpenGLFunctions::glDepthFunc(GLenum func)

    Calls the OpenGL function \c glDepthFunc with \a func.
*/

/*!
    \fn void QOpenGLFunctions::glDepthMask(GLboolean flag)

    Calls the OpenGL function \c glDepthMask with \a flag.
*/

/*!
    \fn void QOpenGLFunctions::glDepthRangef(GLfloat n, GLfloat f)

    Calls the OpenGL function \c glDepthRangef with \a n and \a f.
*/

/*!
    \fn void QOpenGLFunctions::glDetachShader(GLuint program, GLuint shader)

    Calls the OpenGL function \c glDetachShader with \a program and \a shader.
*/

/*!
    \fn void QOpenGLFunctions::glDisable(GLenum cap)

    Calls the OpenGL function \c glDisable with \a cap.
*/

/*!
    \fn void QOpenGLFunctions::glDisableVertexAttribArray(GLuint index)

    Calls the OpenGL function \c glDisableVertexAttribArray with \a index.
*/

/*!
    \fn void QOpenGLFunctions::glDrawArrays(GLenum mode, GLint first, GLsizei count)

    Calls the OpenGL function \c glDrawArrays with \a mode, \a first and \a count.
*/

/*!
    \fn void QOpenGLFunctions::glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)

    Calls the OpenGL function \c glDrawElements with \a mode, \a count, \a type and \a indices.
*/

/*!
    \fn void QOpenGLFunctions::glEnable(GLenum cap)

    Calls the OpenGL function \c glEnable with \a cap.
*/

/*!
    \fn void QOpenGLFunctions::glEnableVertexAttribArray(GLuint index)

    Calls the OpenGL function \c glEnableVertexAttribArray with \a index.
*/

/*!
    \fn void QOpenGLFunctions::glFinish()

    Calls the OpenGL function \c glFinish.
*/

/*!
    \fn void QOpenGLFunctions::glFlush()

    Calls the OpenGL function \c glFlush.
*/

/*!
    \fn void QOpenGLFunctions::glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)

    Calls the OpenGL function \c glFramebufferRenderbuffer with \a target, \a attachment, \a renderbuffertarget and \a renderbuffer.
*/

/*!
    \fn void QOpenGLFunctions::glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)

    Calls the OpenGL function \c glFramebufferTexture2D with \a target, \a attachment, \a textarget, \a texture and \a level.
*/

/*!
    \fn void QOpenGLFunctions::glFrontFace(GLenum mode)

    Calls the OpenGL function \c glFrontFace with \a mode.
*/

/*!
    \fn void QOpenGLFunctions::glGenBuffers(GLsizei n, GLuint *buffers)

    Calls the OpenGL function \c glGenBuffers with \a n and \a buffers.
*/

/*!
    \fn void QOpenGLFunctions::glGenerateMipmap(GLenum target)

    Calls the OpenGL function \c glGenerateMipmap with \a target.
*/

/*!
    \fn void QOpenGLFunctions::glGenFramebuffers(GLsizei n, GLuint *framebuffers)

    Calls the OpenGL function \c glGenFramebuffers with \a n and \a framebuffers.
*/

/*!
    \fn void QOpenGLFunctions::glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)

    Calls the OpenGL function \c glGenRenderbuffers with \a n and \a renderbuffers.
*/

/*!
    \fn void QOpenGLFunctions::glGenTextures(GLsizei n, GLuint *textures)

    Calls the OpenGL function \c glGenTextures with \a n and \a textures.
*/

/*!
    \fn void QOpenGLFunctions::glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)

    Calls the OpenGL function \c glGetActiveAttrib with \a program, \a index, \a bufSize, \a length, \a size, \a type and \a name.
*/

/*!
    \fn void QOpenGLFunctions::glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name)

    Calls the OpenGL function \c glGetActiveUniform with \a program, \a index, \a bufSize, \a length, \a size, \a type and \a name.
*/

/*!
    \fn void QOpenGLFunctions::glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders)

    Calls the OpenGL function \c glGetAttachedShaders with \a program, \a maxCount, \a count and \a shaders.
*/

/*!
    \fn GLint QOpenGLFunctions::glGetAttribLocation(GLuint program, const GLchar *name)

    Returns what the OpenGL function \c glGetAttribLocation returns when it is called with \a program and \a name.
*/

/*!
    \fn void QOpenGLFunctions::glGetBooleanv(GLenum pname, GLboolean *data)

    Calls the OpenGL function \c glGetBooleanv with \a pname and \a data.
*/

/*!
    \fn void QOpenGLFunctions::glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)

    Calls the OpenGL function \c glGetBufferParameteriv with \a target, \a pname and \a params.
*/

/*!
    \fn GLenum QOpenGLFunctions::glGetError()

    Returns what the OpenGL function \c glGetError returns when it is called.
*/

/*!
    \fn void QOpenGLFunctions::glGetFloatv(GLenum pname, GLfloat *data)

    Calls the OpenGL function \c glGetFloatv with \a pname and \a data.
*/

/*!
    \fn void QOpenGLFunctions::glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params)

    Calls the OpenGL function \c glGetFramebufferAttachmentParameteriv with \a target, \a attachment, \a pname and \a params.
*/

/*!
    \fn void QOpenGLFunctions::glGetIntegerv(GLenum pname, GLint *data)

    Calls the OpenGL function \c glGetIntegerv with \a pname and \a data.
*/

/*!
    \fn void QOpenGLFunctions::glGetProgramiv(GLuint program, GLenum pname, GLint *params)

    Calls the OpenGL function \c glGetProgramiv with \a program, \a pname and \a params.
*/

/*!
    \fn void QOpenGLFunctions::glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog)

    Calls the OpenGL function \c glGetProgramInfoLog with \a program, \a bufSize, \a length and \a infoLog.
*/

/*!
    \fn void QOpenGLFunctions::glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params)

    Calls the OpenGL function \c glGetRenderbufferParameteriv with \a target, \a pname and \a params.
*/

/*!
    \fn void QOpenGLFunctions::glGetShaderiv(GLuint shader, GLenum pname, GLint *params)

    Calls the OpenGL function \c glGetShaderiv with \a shader, \a pname and \a params.
*/

/*!
    \fn void QOpenGLFunctions::glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog)

    Calls the OpenGL function \c glGetShaderInfoLog with \a shader, \a bufSize, \a length and \a infoLog.
*/

/*!
    \fn void QOpenGLFunctions::glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision)

    Calls the OpenGL function \c glGetShaderPrecisionFormat with \a shadertype, \a precisiontype, \a range and \a precision.
*/

/*!
    \fn void QOpenGLFunctions::glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source)

    Calls the OpenGL function \c glGetShaderSource with \a shader, \a bufSize, \a length and \a source.
*/

/*!
    \fn const GLubyte *QOpenGLFunctions::glGetString(GLenum name)

    Returns what the OpenGL function \c glGetString returns when it is called with \a name.
*/

/*!
    \fn void QOpenGLFunctions::glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)

    Calls the OpenGL function \c glGetTexParameterfv with \a target, \a pname and \a params.
*/

/*!
    \fn void QOpenGLFunctions::glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)

    Calls the OpenGL function \c glGetTexParameteriv with \a target, \a pname and \a params.
*/

/*!
    \fn void QOpenGLFunctions::glGetUniformfv(GLuint program, GLint location, GLfloat *params)

    Calls the OpenGL function \c glGetUniformfv with \a program, \a location and \a params.
*/

/*!
    \fn void QOpenGLFunctions::glGetUniformiv(GLuint program, GLint location, GLint *params)

    Calls the OpenGL function \c glGetUniformiv with \a program, \a location and \a params.
*/

/*!
    \fn GLint QOpenGLFunctions::glGetUniformLocation(GLuint program, const GLchar *name)

    Returns what the OpenGL function \c glGetUniformLocation returns when it is called with \a program and \a name.
*/

/*!
    \fn void QOpenGLFunctions::glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params)

    Calls the OpenGL function \c glGetVertexAttribfv with \a index, \a pname and \a params.
*/

/*!
    \fn void QOpenGLFunctions::glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params)

    Calls the OpenGL function \c glGetVertexAttribiv with \a index, \a pname and \a params.
*/

/*!
    \fn void QOpenGLFunctions::glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer)

    Calls the OpenGL function \c glGetVertexAttribPointerv with \a index, \a pname and \a pointer.
*/

/*!
    \fn void QOpenGLFunctions::glHint(GLenum target, GLenum mode)

    Calls the OpenGL function \c glHint with \a target and \a mode.
*/

/*!
    \fn GLboolean QOpenGLFunctions::glIsBuffer(GLuint buffer)

    Returns what the OpenGL function \c glIsBuffer returns when it is called with \a buffer.
*/

/*!
    \fn GLboolean QOpenGLFunctions::glIsEnabled(GLenum cap)

    Returns what the OpenGL function \c glIsEnabled returns when it is called with \a cap.
*/

/*!
    \fn GLboolean QOpenGLFunctions::glIsFramebuffer(GLuint framebuffer)

    Returns what the OpenGL function \c glIsFramebuffer returns when it is called with \a framebuffer.
*/

/*!
    \fn GLboolean QOpenGLFunctions::glIsProgram(GLuint program)

    Returns what the OpenGL function \c glIsProgram returns when it is called with \a program.
*/

/*!
    \fn GLboolean QOpenGLFunctions::glIsRenderbuffer(GLuint renderbuffer)

    Returns what the OpenGL function \c glIsRenderbuffer returns when it is called with \a renderbuffer.
*/

/*!
    \fn GLboolean QOpenGLFunctions::glIsShader(GLuint shader)

    Returns what the OpenGL function \c glIsShader returns when it is called with \a shader.
*/

/*!
    \fn GLboolean QOpenGLFunctions::glIsTexture(GLuint texture)

    Returns what the OpenGL function \c glIsTexture returns when it is called with \a texture.
*/

/*!
    \fn void QOpenGLFunctions::glLineWidth(GLfloat width)

    Calls the OpenGL function \c glLineWidth with \a width.
*/

/*!
    \fn void QOpenGLFunctions::glLinkProgram(GLuint program)

    Calls the OpenGL function \c glLinkProgram with \a program.
*/

/*!
    \fn void QOpenGLFunctions::glPixelStorei(GLenum pname, GLint param)

    Calls the OpenGL function \c glPixelStorei with \a pname and \a param.
*/

/*!
    \fn void QOpenGLFunctions::glPolygonOffset(GLfloat factor, GLfloat units)

    Calls the OpenGL function \c glPolygonOffset with \a factor and \a units.
*/

/*!
    \fn void QOpenGLFunctions::glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)

    Calls the OpenGL function \c glReadPixels with \a x, \a y, \a width, \a height, \a format, \a type and \a pixels.
*/

/*!
    \fn void QOpenGLFunctions::glReleaseShaderCompiler()

    Calls the OpenGL function \c glReleaseShaderCompiler.
*/

/*!
    \fn void QOpenGLFunctions::glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)

    Calls the OpenGL function \c glRenderbufferStorage with \a target, \a internalformat, \a width and \a height.
*/

/*!
    \fn void QOpenGLFunctions::glSampleCoverage(GLfloat value, GLboolean invert)

    Calls the OpenGL function \c glSampleCoverage with \a value and \a invert.
*/

/*!
    \fn void QOpenGLFunctions::glScissor(GLint x, GLint y, GLsizei width, GLsizei height)

    Calls the OpenGL function \c glScissor with \a x, \a y, \a width and \a height.
*/

/*!
    \fn void QOpenGLFunctions::glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryFormat, const void *binary, GLsizei length)

    Calls the OpenGL function \c glShaderBinary with \a count, \a shaders, \a binaryFormat, \a binary and \a length.
*/

/*!
    \fn void QOpenGLFunctions::glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length)

    Calls the OpenGL function \c glShaderSource with \a shader, \a count, \a string and \a length.
*/

/*!
    \fn void QOpenGLFunctions::glStencilFunc(GLenum func, GLint ref, GLuint mask)

    Calls the OpenGL function \c glStencilFunc with \a func, \a ref and \a mask.
*/

/*!
    \fn void QOpenGLFunctions::glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)

    Calls the OpenGL function \c glStencilFuncSeparate with \a face, \a func, \a ref and \a mask.
*/

/*!
    \fn void QOpenGLFunctions::glStencilMask(GLuint mask)

    Calls the OpenGL function \c glStencilMask with \a mask.
*/

/*!
    \fn void QOpenGLFunctions::glStencilMaskSeparate(GLenum face, GLuint mask)

    Calls the OpenGL function \c glStencilMaskSeparate with \a face and \a mask.
*/

/*!
    \fn void QOpenGLFunctions::glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)

    Calls the OpenGL function \c glStencilOp with \a fail, \a zfail and \a zpass.
*/

/*!
    \fn void QOpenGLFunctions::glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass)

    Calls the OpenGL function \c glStencilOpSeparate with \a face, \a sfail, \a dpfail and \a dppass.
*/

/*!
    \fn void QOpenGLFunctions::glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels)

    Calls the OpenGL function \c glTexImage2D with \a target, \a level, \a internalformat, \a width, \a height, \a border, \a format, \a type and \a pixels.
*/

/*!
    \fn void QOpenGLFunctions::glTexParameterf(GLenum target, GLenum pname, GLfloat param)

    Calls the OpenGL function \c glTexParameterf with \a target, \a pname and \a param.
*/

/*!
    \fn void QOpenGLFunctions::glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)

    Calls the OpenGL function \c glTexParameterfv with \a target, \a pname and \a params.
*/

/*!
    \fn void QOpenGLFunctions::glTexParameteri(GLenum target, GLenum pname, GLint param)

    Calls the OpenGL function \c glTexParameteri with \a target, \a pname and \a param.
*/

/*!
    \fn void QOpenGLFunctions::glTexParameteriv(GLenum target, GLenum pname, const GLint *params)

    Calls the OpenGL function \c glTexParameteriv with \a target, \a pname and \a params.
*/

/*!
    \fn void QOpenGLFunctions::glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels)

    Calls the OpenGL function \c glTexSubImage2D with \a target, \a level, \a xoffset, \a yoffset, \a width, \a height, \a format, \a type and \a pixels.
*/

/*!
    \fn void QOpenGLFunctions::glUniform1f(GLint location, GLfloat v0)

    Calls the OpenGL function \c glUniform1f with \a location and \a v0.
*/

/*!
    \fn void QOpenGLFunctions::glUniform1fv(GLint location, GLsizei count, const GLfloat *value)

    Calls the OpenGL function \c glUniform1fv with \a location, \a count and \a value.
*/

/*!
    \fn void QOpenGLFunctions::glUniform1i(GLint location, GLint v0)

    Calls the OpenGL function \c glUniform1i with \a location and \a v0.
*/

/*!
    \fn void QOpenGLFunctions::glUniform1iv(GLint location, GLsizei count, const GLint *value)

    Calls the OpenGL function \c glUniform1iv with \a location, \a count and \a value.
*/

/*!
    \fn void QOpenGLFunctions::glUniform2f(GLint location, GLfloat v0, GLfloat v1)

    Calls the OpenGL function \c glUniform2f with \a location, \a v0 and \a v1.
*/

/*!
    \fn void QOpenGLFunctions::glUniform2fv(GLint location, GLsizei count, const GLfloat *value)

    Calls the OpenGL function \c glUniform2fv with \a location, \a count and \a value.
*/

/*!
    \fn void QOpenGLFunctions::glUniform2i(GLint location, GLint v0, GLint v1)

    Calls the OpenGL function \c glUniform2i with \a location, \a v0 and \a v1.
*/

/*!
    \fn void QOpenGLFunctions::glUniform2iv(GLint location, GLsizei count, const GLint *value)

    Calls the OpenGL function \c glUniform2iv with \a location, \a count and \a value.
*/

/*!
    \fn void QOpenGLFunctions::glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2)

    Calls the OpenGL function \c glUniform3f with \a location, \a v0, \a v1 and \a v2.
*/

/*!
    \fn void QOpenGLFunctions::glUniform3fv(GLint location, GLsizei count, const GLfloat *value)

    Calls the OpenGL function \c glUniform3fv with \a location, \a count and \a value.
*/

/*!
    \fn void QOpenGLFunctions::glUniform3i(GLint location, GLint v0, GLint v1, GLint v2)

    Calls the OpenGL function \c glUniform3i with \a location, \a v0, \a v1 and \a v2.
*/

/*!
    \fn void QOpenGLFunctions::glUniform3iv(GLint location, GLsizei count, const GLint *value)

    Calls the OpenGL function \c glUniform3iv with \a location, \a count and \a value.
*/

/*!
    \fn void QOpenGLFunctions::glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)

    Calls the OpenGL function \c glUniform4f with \a location, \a v0, \a v1, \a v2 and \a v3.
*/

/*!
    \fn void QOpenGLFunctions::glUniform4fv(GLint location, GLsizei count, const GLfloat *value)

    Calls the OpenGL function \c glUniform4fv with \a location, \a count and \a value.
*/

/*!
    \fn void QOpenGLFunctions::glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3)

    Calls the OpenGL function \c glUniform4i with \a location, \a v0, \a v1, \a v2 and \a v3.
*/

/*!
    \fn void QOpenGLFunctions::glUniform4iv(GLint location, GLsizei count, const GLint *value)

    Calls the OpenGL function \c glUniform4iv with \a location, \a count and \a value.
*/

/*!
    \fn void QOpenGLFunctions::glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)

    Calls the OpenGL function \c glUniformMatrix2fv with \a location, \a count, \a transpose and \a value.
*/

/*!
    \fn void QOpenGLFunctions::glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)

    Calls the OpenGL function \c glUniformMatrix3fv with \a location, \a count, \a transpose and \a value.
*/

/*!
    \fn void QOpenGLFunctions::glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)

    Calls the OpenGL function \c glUniformMatrix4fv with \a location, \a count, \a transpose and \a value.
*/

/*!
    \fn void QOpenGLFunctions::glUseProgram(GLuint program)

    Calls the OpenGL function \c glUseProgram with \a program.
*/

/*!
    \fn void QOpenGLFunctions::glValidateProgram(GLuint program)

    Calls the OpenGL function \c glValidateProgram with \a program.
*/

/*!
    \fn void QOpenGLFunctions::glVertexAttrib1f(GLuint index, GLfloat x)

    Calls the OpenGL function \c glVertexAttrib1f with \a index and \a x.
*/

/*!
    \fn void QOpenGLFunctions::glVertexAttrib1fv(GLuint index, const GLfloat *v)

    Calls the OpenGL function \c glVertexAttrib1fv with \a index and \a v.
*/

/*!
    \fn void QOpenGLFunctions::glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y)

    Calls the OpenGL function \c glVertexAttrib2f with \a index, \a x and \a y.
*/

/*!
    \fn void QOpenGLFunctions::glVertexAttrib2fv(GLuint index, const GLfloat *v)

    Calls the OpenGL function \c glVertexAttrib2fv with \a index and \a v.
*/

/*!
    \fn void QOpenGLFunctions::glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z)

    Calls the OpenGL function \c glVertexAttrib3f with \a index, \a x, \a y and \a z.
*/

/*!
    \fn void QOpenGLFunctions::glVertexAttrib3fv(GLuint index, const GLfloat *v)

    Calls the OpenGL function \c glVertexAttrib3fv with \a index and \a v.
*/

/*!
    \fn void QOpenGLFunctions::glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)

    Calls the OpenGL function \c glVertexAttrib4f with \a index, \a x, \a y, \a z and \a w.
*/

/*!
    \fn void QOpenGLFunctions::glVertexAttrib4fv(GLuint index, const GLfloat *v)

    Calls the OpenGL function \c glVertexAttrib4fv with \a index and \a v.
*/

/*!
    \fn void QOpenGLFunctions::glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer)

    Calls the OpenGL function \c glVertexAttribPointer with \a index, \a size, \a type, \a normalized, \a stride and \a pointer.
*/

/*!
    \fn void QOpenGLFunctions::glViewport(GLint x, GLint y, GLsizei width, GLsizei height)

    Calls the OpenGL function \c glViewport with \a x, \a y, \a width and \a height.
*/
