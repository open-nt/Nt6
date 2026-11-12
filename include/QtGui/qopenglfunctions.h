// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtGui/qopengl.h>

class QOpenGLContext;

// The OpenGL ES 2.0 functions of one context, resolved when the context is made current for the first time.
struct QOpenGLFunctionsPrivate {
    explicit QOpenGLFunctionsPrivate(QOpenGLContext *context);

    struct Functions {
        void(QOPENGLF_APIENTRYP ActiveTexture)(GLenum texture);
        void(QOPENGLF_APIENTRYP AttachShader)(GLuint program, GLuint shader);
        void(QOPENGLF_APIENTRYP BindAttribLocation)(GLuint program, GLuint index, const GLchar *name);
        void(QOPENGLF_APIENTRYP BindBuffer)(GLenum target, GLuint buffer);
        void(QOPENGLF_APIENTRYP BindFramebuffer)(GLenum target, GLuint framebuffer);
        void(QOPENGLF_APIENTRYP BindRenderbuffer)(GLenum target, GLuint renderbuffer);
        void(QOPENGLF_APIENTRYP BindTexture)(GLenum target, GLuint texture);
        void(QOPENGLF_APIENTRYP BlendColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
        void(QOPENGLF_APIENTRYP BlendEquation)(GLenum mode);
        void(QOPENGLF_APIENTRYP BlendEquationSeparate)(GLenum modeRGB, GLenum modeAlpha);
        void(QOPENGLF_APIENTRYP BlendFunc)(GLenum sfactor, GLenum dfactor);
        void(QOPENGLF_APIENTRYP BlendFuncSeparate)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha,
                                                   GLenum dfactorAlpha);
        void(QOPENGLF_APIENTRYP BufferData)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
        void(QOPENGLF_APIENTRYP BufferSubData)(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
        GLenum(QOPENGLF_APIENTRYP CheckFramebufferStatus)(GLenum target);
        void(QOPENGLF_APIENTRYP Clear)(GLbitfield mask);
        void(QOPENGLF_APIENTRYP ClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
        void(QOPENGLF_APIENTRYP ClearDepthf)(GLfloat d);
        void(QOPENGLF_APIENTRYP ClearStencil)(GLint s);
        void(QOPENGLF_APIENTRYP ColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
        void(QOPENGLF_APIENTRYP CompileShader)(GLuint shader);
        void(QOPENGLF_APIENTRYP CompressedTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                                                      GLsizei height, GLint border, GLsizei imageSize,
                                                      const void *data);
        void(QOPENGLF_APIENTRYP CompressedTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                                         GLsizei width, GLsizei height, GLenum format,
                                                         GLsizei imageSize, const void *data);
        void(QOPENGLF_APIENTRYP CopyTexImage2D)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
                                                GLsizei width, GLsizei height, GLint border);
        void(QOPENGLF_APIENTRYP CopyTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x,
                                                   GLint y, GLsizei width, GLsizei height);
        GLuint(QOPENGLF_APIENTRYP CreateProgram)();
        GLuint(QOPENGLF_APIENTRYP CreateShader)(GLenum type);
        void(QOPENGLF_APIENTRYP CullFace)(GLenum mode);
        void(QOPENGLF_APIENTRYP DeleteBuffers)(GLsizei n, const GLuint *buffers);
        void(QOPENGLF_APIENTRYP DeleteFramebuffers)(GLsizei n, const GLuint *framebuffers);
        void(QOPENGLF_APIENTRYP DeleteProgram)(GLuint program);
        void(QOPENGLF_APIENTRYP DeleteRenderbuffers)(GLsizei n, const GLuint *renderbuffers);
        void(QOPENGLF_APIENTRYP DeleteShader)(GLuint shader);
        void(QOPENGLF_APIENTRYP DeleteTextures)(GLsizei n, const GLuint *textures);
        void(QOPENGLF_APIENTRYP DepthFunc)(GLenum func);
        void(QOPENGLF_APIENTRYP DepthMask)(GLboolean flag);
        void(QOPENGLF_APIENTRYP DepthRangef)(GLfloat n, GLfloat f);
        void(QOPENGLF_APIENTRYP DetachShader)(GLuint program, GLuint shader);
        void(QOPENGLF_APIENTRYP Disable)(GLenum cap);
        void(QOPENGLF_APIENTRYP DisableVertexAttribArray)(GLuint index);
        void(QOPENGLF_APIENTRYP DrawArrays)(GLenum mode, GLint first, GLsizei count);
        void(QOPENGLF_APIENTRYP DrawElements)(GLenum mode, GLsizei count, GLenum type, const void *indices);
        void(QOPENGLF_APIENTRYP Enable)(GLenum cap);
        void(QOPENGLF_APIENTRYP EnableVertexAttribArray)(GLuint index);
        void(QOPENGLF_APIENTRYP Finish)();
        void(QOPENGLF_APIENTRYP Flush)();
        void(QOPENGLF_APIENTRYP FramebufferRenderbuffer)(GLenum target, GLenum attachment, GLenum renderbuffertarget,
                                                         GLuint renderbuffer);
        void(QOPENGLF_APIENTRYP FramebufferTexture2D)(GLenum target, GLenum attachment, GLenum textarget,
                                                      GLuint texture, GLint level);
        void(QOPENGLF_APIENTRYP FrontFace)(GLenum mode);
        void(QOPENGLF_APIENTRYP GenBuffers)(GLsizei n, GLuint *buffers);
        void(QOPENGLF_APIENTRYP GenerateMipmap)(GLenum target);
        void(QOPENGLF_APIENTRYP GenFramebuffers)(GLsizei n, GLuint *framebuffers);
        void(QOPENGLF_APIENTRYP GenRenderbuffers)(GLsizei n, GLuint *renderbuffers);
        void(QOPENGLF_APIENTRYP GenTextures)(GLsizei n, GLuint *textures);
        void(QOPENGLF_APIENTRYP GetActiveAttrib)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length,
                                                 GLint *size, GLenum *type, GLchar *name);
        void(QOPENGLF_APIENTRYP GetActiveUniform)(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length,
                                                  GLint *size, GLenum *type, GLchar *name);
        void(QOPENGLF_APIENTRYP GetAttachedShaders)(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders);
        GLint(QOPENGLF_APIENTRYP GetAttribLocation)(GLuint program, const GLchar *name);
        void(QOPENGLF_APIENTRYP GetBooleanv)(GLenum pname, GLboolean *data);
        void(QOPENGLF_APIENTRYP GetBufferParameteriv)(GLenum target, GLenum pname, GLint *params);
        GLenum(QOPENGLF_APIENTRYP GetError)();
        void(QOPENGLF_APIENTRYP GetFloatv)(GLenum pname, GLfloat *data);
        void(QOPENGLF_APIENTRYP GetFramebufferAttachmentParameteriv)(GLenum target, GLenum attachment, GLenum pname,
                                                                     GLint *params);
        void(QOPENGLF_APIENTRYP GetIntegerv)(GLenum pname, GLint *data);
        void(QOPENGLF_APIENTRYP GetProgramiv)(GLuint program, GLenum pname, GLint *params);
        void(QOPENGLF_APIENTRYP GetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
        void(QOPENGLF_APIENTRYP GetRenderbufferParameteriv)(GLenum target, GLenum pname, GLint *params);
        void(QOPENGLF_APIENTRYP GetShaderiv)(GLuint shader, GLenum pname, GLint *params);
        void(QOPENGLF_APIENTRYP GetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
        void(QOPENGLF_APIENTRYP GetShaderPrecisionFormat)(GLenum shadertype, GLenum precisiontype, GLint *range,
                                                          GLint *precision);
        void(QOPENGLF_APIENTRYP GetShaderSource)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source);
        const GLubyte *(QOPENGLF_APIENTRYP GetString)(GLenum name);
        void(QOPENGLF_APIENTRYP GetTexParameterfv)(GLenum target, GLenum pname, GLfloat *params);
        void(QOPENGLF_APIENTRYP GetTexParameteriv)(GLenum target, GLenum pname, GLint *params);
        void(QOPENGLF_APIENTRYP GetUniformfv)(GLuint program, GLint location, GLfloat *params);
        void(QOPENGLF_APIENTRYP GetUniformiv)(GLuint program, GLint location, GLint *params);
        GLint(QOPENGLF_APIENTRYP GetUniformLocation)(GLuint program, const GLchar *name);
        void(QOPENGLF_APIENTRYP GetVertexAttribfv)(GLuint index, GLenum pname, GLfloat *params);
        void(QOPENGLF_APIENTRYP GetVertexAttribiv)(GLuint index, GLenum pname, GLint *params);
        void(QOPENGLF_APIENTRYP GetVertexAttribPointerv)(GLuint index, GLenum pname, void **pointer);
        void(QOPENGLF_APIENTRYP Hint)(GLenum target, GLenum mode);
        GLboolean(QOPENGLF_APIENTRYP IsBuffer)(GLuint buffer);
        GLboolean(QOPENGLF_APIENTRYP IsEnabled)(GLenum cap);
        GLboolean(QOPENGLF_APIENTRYP IsFramebuffer)(GLuint framebuffer);
        GLboolean(QOPENGLF_APIENTRYP IsProgram)(GLuint program);
        GLboolean(QOPENGLF_APIENTRYP IsRenderbuffer)(GLuint renderbuffer);
        GLboolean(QOPENGLF_APIENTRYP IsShader)(GLuint shader);
        GLboolean(QOPENGLF_APIENTRYP IsTexture)(GLuint texture);
        void(QOPENGLF_APIENTRYP LineWidth)(GLfloat width);
        void(QOPENGLF_APIENTRYP LinkProgram)(GLuint program);
        void(QOPENGLF_APIENTRYP PixelStorei)(GLenum pname, GLint param);
        void(QOPENGLF_APIENTRYP PolygonOffset)(GLfloat factor, GLfloat units);
        void(QOPENGLF_APIENTRYP ReadPixels)(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                                            void *pixels);
        void(QOPENGLF_APIENTRYP ReleaseShaderCompiler)();
        void(QOPENGLF_APIENTRYP RenderbufferStorage)(GLenum target, GLenum internalformat, GLsizei width,
                                                     GLsizei height);
        void(QOPENGLF_APIENTRYP SampleCoverage)(GLfloat value, GLboolean invert);
        void(QOPENGLF_APIENTRYP Scissor)(GLint x, GLint y, GLsizei width, GLsizei height);
        void(QOPENGLF_APIENTRYP ShaderBinary)(GLsizei count, const GLuint *shaders, GLenum binaryFormat,
                                              const void *binary, GLsizei length);
        void(QOPENGLF_APIENTRYP ShaderSource)(GLuint shader, GLsizei count, const GLchar *const *string,
                                              const GLint *length);
        void(QOPENGLF_APIENTRYP StencilFunc)(GLenum func, GLint ref, GLuint mask);
        void(QOPENGLF_APIENTRYP StencilFuncSeparate)(GLenum face, GLenum func, GLint ref, GLuint mask);
        void(QOPENGLF_APIENTRYP StencilMask)(GLuint mask);
        void(QOPENGLF_APIENTRYP StencilMaskSeparate)(GLenum face, GLuint mask);
        void(QOPENGLF_APIENTRYP StencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
        void(QOPENGLF_APIENTRYP StencilOpSeparate)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
        void(QOPENGLF_APIENTRYP TexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                            GLsizei height, GLint border, GLenum format, GLenum type,
                                            const void *pixels);
        void(QOPENGLF_APIENTRYP TexParameterf)(GLenum target, GLenum pname, GLfloat param);
        void(QOPENGLF_APIENTRYP TexParameterfv)(GLenum target, GLenum pname, const GLfloat *params);
        void(QOPENGLF_APIENTRYP TexParameteri)(GLenum target, GLenum pname, GLint param);
        void(QOPENGLF_APIENTRYP TexParameteriv)(GLenum target, GLenum pname, const GLint *params);
        void(QOPENGLF_APIENTRYP TexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                                               GLsizei height, GLenum format, GLenum type, const void *pixels);
        void(QOPENGLF_APIENTRYP Uniform1f)(GLint location, GLfloat v0);
        void(QOPENGLF_APIENTRYP Uniform1fv)(GLint location, GLsizei count, const GLfloat *value);
        void(QOPENGLF_APIENTRYP Uniform1i)(GLint location, GLint v0);
        void(QOPENGLF_APIENTRYP Uniform1iv)(GLint location, GLsizei count, const GLint *value);
        void(QOPENGLF_APIENTRYP Uniform2f)(GLint location, GLfloat v0, GLfloat v1);
        void(QOPENGLF_APIENTRYP Uniform2fv)(GLint location, GLsizei count, const GLfloat *value);
        void(QOPENGLF_APIENTRYP Uniform2i)(GLint location, GLint v0, GLint v1);
        void(QOPENGLF_APIENTRYP Uniform2iv)(GLint location, GLsizei count, const GLint *value);
        void(QOPENGLF_APIENTRYP Uniform3f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
        void(QOPENGLF_APIENTRYP Uniform3fv)(GLint location, GLsizei count, const GLfloat *value);
        void(QOPENGLF_APIENTRYP Uniform3i)(GLint location, GLint v0, GLint v1, GLint v2);
        void(QOPENGLF_APIENTRYP Uniform3iv)(GLint location, GLsizei count, const GLint *value);
        void(QOPENGLF_APIENTRYP Uniform4f)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
        void(QOPENGLF_APIENTRYP Uniform4fv)(GLint location, GLsizei count, const GLfloat *value);
        void(QOPENGLF_APIENTRYP Uniform4i)(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
        void(QOPENGLF_APIENTRYP Uniform4iv)(GLint location, GLsizei count, const GLint *value);
        void(QOPENGLF_APIENTRYP UniformMatrix2fv)(GLint location, GLsizei count, GLboolean transpose,
                                                  const GLfloat *value);
        void(QOPENGLF_APIENTRYP UniformMatrix3fv)(GLint location, GLsizei count, GLboolean transpose,
                                                  const GLfloat *value);
        void(QOPENGLF_APIENTRYP UniformMatrix4fv)(GLint location, GLsizei count, GLboolean transpose,
                                                  const GLfloat *value);
        void(QOPENGLF_APIENTRYP UseProgram)(GLuint program);
        void(QOPENGLF_APIENTRYP ValidateProgram)(GLuint program);
        void(QOPENGLF_APIENTRYP VertexAttrib1f)(GLuint index, GLfloat x);
        void(QOPENGLF_APIENTRYP VertexAttrib1fv)(GLuint index, const GLfloat *v);
        void(QOPENGLF_APIENTRYP VertexAttrib2f)(GLuint index, GLfloat x, GLfloat y);
        void(QOPENGLF_APIENTRYP VertexAttrib2fv)(GLuint index, const GLfloat *v);
        void(QOPENGLF_APIENTRYP VertexAttrib3f)(GLuint index, GLfloat x, GLfloat y, GLfloat z);
        void(QOPENGLF_APIENTRYP VertexAttrib3fv)(GLuint index, const GLfloat *v);
        void(QOPENGLF_APIENTRYP VertexAttrib4f)(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
        void(QOPENGLF_APIENTRYP VertexAttrib4fv)(GLuint index, const GLfloat *v);
        void(QOPENGLF_APIENTRYP VertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                                     GLsizei stride, const void *pointer);
        void(QOPENGLF_APIENTRYP Viewport)(GLint x, GLint y, GLsizei width, GLsizei height);
    } f;
};

class QOpenGLFunctions {
public:
    enum OpenGLFeature {
        Multitexture = 0x0001,
        Shaders = 0x0002,
        Buffers = 0x0004,
        Framebuffers = 0x0008,
        BlendColor = 0x0010,
        BlendEquation = 0x0020,
        BlendEquationSeparate = 0x0040,
        BlendFuncSeparate = 0x0080,
        BlendSubtract = 0x0100,
        CompressedTextures = 0x0200,
        Multisample = 0x0400,
        StencilSeparate = 0x0800,
        NPOTTextures = 0x1000,
        NPOTTextureRepeat = 0x2000,
        FixedFunctionPipeline = 0x4000,
        TextureRGFormats = 0x8000,
        MultipleRenderTargets = 0x10000,
        BlendEquationAdvanced = 0x20000
    };
    Q_DECLARE_FLAGS(OpenGLFeatures, OpenGLFeature)

    QOpenGLFunctions();
    explicit QOpenGLFunctions(QOpenGLContext *context);
    ~QOpenGLFunctions() = default;

    OpenGLFeatures openGLFeatures() const;
    bool hasOpenGLFeature(QOpenGLFunctions::OpenGLFeature feature) const;
    void initializeOpenGLFunctions();

    void glActiveTexture(GLenum texture);
    void glAttachShader(GLuint program, GLuint shader);
    void glBindAttribLocation(GLuint program, GLuint index, const GLchar *name);
    void glBindBuffer(GLenum target, GLuint buffer);
    void glBindFramebuffer(GLenum target, GLuint framebuffer);
    void glBindRenderbuffer(GLenum target, GLuint renderbuffer);
    void glBindTexture(GLenum target, GLuint texture);
    void glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void glBlendEquation(GLenum mode);
    void glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha);
    void glBlendFunc(GLenum sfactor, GLenum dfactor);
    void glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
    void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
    void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
    GLenum glCheckFramebufferStatus(GLenum target);
    void glClear(GLbitfield mask);
    void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    void glClearDepthf(GLfloat d);
    void glClearStencil(GLint s);
    void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
    void glCompileShader(GLuint shader);
    void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height,
                                GLint border, GLsizei imageSize, const void *data);
    void glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                                   GLsizei height, GLenum format, GLsizei imageSize, const void *data);
    void glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width,
                          GLsizei height, GLint border);
    void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width,
                             GLsizei height);
    GLuint glCreateProgram();
    GLuint glCreateShader(GLenum type);
    void glCullFace(GLenum mode);
    void glDeleteBuffers(GLsizei n, const GLuint *buffers);
    void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers);
    void glDeleteProgram(GLuint program);
    void glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers);
    void glDeleteShader(GLuint shader);
    void glDeleteTextures(GLsizei n, const GLuint *textures);
    void glDepthFunc(GLenum func);
    void glDepthMask(GLboolean flag);
    void glDepthRangef(GLfloat n, GLfloat f);
    void glDetachShader(GLuint program, GLuint shader);
    void glDisable(GLenum cap);
    void glDisableVertexAttribArray(GLuint index);
    void glDrawArrays(GLenum mode, GLint first, GLsizei count);
    void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);
    void glEnable(GLenum cap);
    void glEnableVertexAttribArray(GLuint index);
    void glFinish();
    void glFlush();
    void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
    void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
    void glFrontFace(GLenum mode);
    void glGenBuffers(GLsizei n, GLuint *buffers);
    void glGenerateMipmap(GLenum target);
    void glGenFramebuffers(GLsizei n, GLuint *framebuffers);
    void glGenRenderbuffers(GLsizei n, GLuint *renderbuffers);
    void glGenTextures(GLsizei n, GLuint *textures);
    void glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type,
                           GLchar *name);
    void glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type,
                            GLchar *name);
    void glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders);
    GLint glGetAttribLocation(GLuint program, const GLchar *name);
    void glGetBooleanv(GLenum pname, GLboolean *data);
    void glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params);
    GLenum glGetError();
    void glGetFloatv(GLenum pname, GLfloat *data);
    void glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname, GLint *params);
    void glGetIntegerv(GLenum pname, GLint *data);
    void glGetProgramiv(GLuint program, GLenum pname, GLint *params);
    void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
    void glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params);
    void glGetShaderiv(GLuint shader, GLenum pname, GLint *params);
    void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
    void glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision);
    void glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source);
    const GLubyte *glGetString(GLenum name);
    void glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params);
    void glGetTexParameteriv(GLenum target, GLenum pname, GLint *params);
    void glGetUniformfv(GLuint program, GLint location, GLfloat *params);
    void glGetUniformiv(GLuint program, GLint location, GLint *params);
    GLint glGetUniformLocation(GLuint program, const GLchar *name);
    void glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params);
    void glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params);
    void glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer);
    void glHint(GLenum target, GLenum mode);
    GLboolean glIsBuffer(GLuint buffer);
    GLboolean glIsEnabled(GLenum cap);
    GLboolean glIsFramebuffer(GLuint framebuffer);
    GLboolean glIsProgram(GLuint program);
    GLboolean glIsRenderbuffer(GLuint renderbuffer);
    GLboolean glIsShader(GLuint shader);
    GLboolean glIsTexture(GLuint texture);
    void glLineWidth(GLfloat width);
    void glLinkProgram(GLuint program);
    void glPixelStorei(GLenum pname, GLint param);
    void glPolygonOffset(GLfloat factor, GLfloat units);
    void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels);
    void glReleaseShaderCompiler();
    void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
    void glSampleCoverage(GLfloat value, GLboolean invert);
    void glScissor(GLint x, GLint y, GLsizei width, GLsizei height);
    void glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryFormat, const void *binary, GLsizei length);
    void glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length);
    void glStencilFunc(GLenum func, GLint ref, GLuint mask);
    void glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask);
    void glStencilMask(GLuint mask);
    void glStencilMaskSeparate(GLenum face, GLuint mask);
    void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass);
    void glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
    void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border,
                      GLenum format, GLenum type, const void *pixels);
    void glTexParameterf(GLenum target, GLenum pname, GLfloat param);
    void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params);
    void glTexParameteri(GLenum target, GLenum pname, GLint param);
    void glTexParameteriv(GLenum target, GLenum pname, const GLint *params);
    void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                         GLenum format, GLenum type, const void *pixels);
    void glUniform1f(GLint location, GLfloat v0);
    void glUniform1fv(GLint location, GLsizei count, const GLfloat *value);
    void glUniform1i(GLint location, GLint v0);
    void glUniform1iv(GLint location, GLsizei count, const GLint *value);
    void glUniform2f(GLint location, GLfloat v0, GLfloat v1);
    void glUniform2fv(GLint location, GLsizei count, const GLfloat *value);
    void glUniform2i(GLint location, GLint v0, GLint v1);
    void glUniform2iv(GLint location, GLsizei count, const GLint *value);
    void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    void glUniform3fv(GLint location, GLsizei count, const GLfloat *value);
    void glUniform3i(GLint location, GLint v0, GLint v1, GLint v2);
    void glUniform3iv(GLint location, GLsizei count, const GLint *value);
    void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
    void glUniform4fv(GLint location, GLsizei count, const GLfloat *value);
    void glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3);
    void glUniform4iv(GLint location, GLsizei count, const GLint *value);
    void glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void glUseProgram(GLuint program);
    void glValidateProgram(GLuint program);
    void glVertexAttrib1f(GLuint index, GLfloat x);
    void glVertexAttrib1fv(GLuint index, const GLfloat *v);
    void glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y);
    void glVertexAttrib2fv(GLuint index, const GLfloat *v);
    void glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z);
    void glVertexAttrib3fv(GLuint index, const GLfloat *v);
    void glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
    void glVertexAttrib4fv(GLuint index, const GLfloat *v);
    void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
                               const void *pointer);
    void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);

protected:
    QOpenGLFunctionsPrivate *d_ptr = nullptr;
    static bool isInitialized(const QOpenGLFunctionsPrivate *d) { return d != nullptr; }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QOpenGLFunctions::OpenGLFeatures)

inline void QOpenGLFunctions::glActiveTexture(GLenum texture) { d_ptr->f.ActiveTexture(texture); }
inline void QOpenGLFunctions::glAttachShader(GLuint program, GLuint shader) { d_ptr->f.AttachShader(program, shader); }
inline void QOpenGLFunctions::glBindAttribLocation(GLuint program, GLuint index, const GLchar *name) {
    d_ptr->f.BindAttribLocation(program, index, name);
}
inline void QOpenGLFunctions::glBindBuffer(GLenum target, GLuint buffer) { d_ptr->f.BindBuffer(target, buffer); }
inline void QOpenGLFunctions::glBindFramebuffer(GLenum target, GLuint framebuffer) {
    d_ptr->f.BindFramebuffer(target, framebuffer);
}
inline void QOpenGLFunctions::glBindRenderbuffer(GLenum target, GLuint renderbuffer) {
    d_ptr->f.BindRenderbuffer(target, renderbuffer);
}
inline void QOpenGLFunctions::glBindTexture(GLenum target, GLuint texture) { d_ptr->f.BindTexture(target, texture); }
inline void QOpenGLFunctions::glBlendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    d_ptr->f.BlendColor(red, green, blue, alpha);
}
inline void QOpenGLFunctions::glBlendEquation(GLenum mode) { d_ptr->f.BlendEquation(mode); }
inline void QOpenGLFunctions::glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
    d_ptr->f.BlendEquationSeparate(modeRGB, modeAlpha);
}
inline void QOpenGLFunctions::glBlendFunc(GLenum sfactor, GLenum dfactor) { d_ptr->f.BlendFunc(sfactor, dfactor); }
inline void QOpenGLFunctions::glBlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha,
                                                  GLenum dfactorAlpha) {
    d_ptr->f.BlendFuncSeparate(sfactorRGB, dfactorRGB, sfactorAlpha, dfactorAlpha);
}
inline void QOpenGLFunctions::glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage) {
    d_ptr->f.BufferData(target, size, data, usage);
}
inline void QOpenGLFunctions::glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) {
    d_ptr->f.BufferSubData(target, offset, size, data);
}
inline GLenum QOpenGLFunctions::glCheckFramebufferStatus(GLenum target) {
    return d_ptr->f.CheckFramebufferStatus(target);
}
inline void QOpenGLFunctions::glClear(GLbitfield mask) { d_ptr->f.Clear(mask); }
inline void QOpenGLFunctions::glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
    d_ptr->f.ClearColor(red, green, blue, alpha);
}
inline void QOpenGLFunctions::glClearDepthf(GLfloat d) { d_ptr->f.ClearDepthf(d); }
inline void QOpenGLFunctions::glClearStencil(GLint s) { d_ptr->f.ClearStencil(s); }
inline void QOpenGLFunctions::glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) {
    d_ptr->f.ColorMask(red, green, blue, alpha);
}
inline void QOpenGLFunctions::glCompileShader(GLuint shader) { d_ptr->f.CompileShader(shader); }
inline void QOpenGLFunctions::glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                                                     GLsizei height, GLint border, GLsizei imageSize,
                                                     const void *data) {
    d_ptr->f.CompressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}
inline void QOpenGLFunctions::glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                                        GLsizei width, GLsizei height, GLenum format, GLsizei imageSize,
                                                        const void *data) {
    d_ptr->f.CompressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize, data);
}
inline void QOpenGLFunctions::glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
                                               GLsizei width, GLsizei height, GLint border) {
    d_ptr->f.CopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}
inline void QOpenGLFunctions::glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x,
                                                  GLint y, GLsizei width, GLsizei height) {
    d_ptr->f.CopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}
inline GLuint QOpenGLFunctions::glCreateProgram() { return d_ptr->f.CreateProgram(); }
inline GLuint QOpenGLFunctions::glCreateShader(GLenum type) { return d_ptr->f.CreateShader(type); }
inline void QOpenGLFunctions::glCullFace(GLenum mode) { d_ptr->f.CullFace(mode); }
inline void QOpenGLFunctions::glDeleteBuffers(GLsizei n, const GLuint *buffers) { d_ptr->f.DeleteBuffers(n, buffers); }
inline void QOpenGLFunctions::glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers) {
    d_ptr->f.DeleteFramebuffers(n, framebuffers);
}
inline void QOpenGLFunctions::glDeleteProgram(GLuint program) { d_ptr->f.DeleteProgram(program); }
inline void QOpenGLFunctions::glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers) {
    d_ptr->f.DeleteRenderbuffers(n, renderbuffers);
}
inline void QOpenGLFunctions::glDeleteShader(GLuint shader) { d_ptr->f.DeleteShader(shader); }
inline void QOpenGLFunctions::glDeleteTextures(GLsizei n, const GLuint *textures) {
    d_ptr->f.DeleteTextures(n, textures);
}
inline void QOpenGLFunctions::glDepthFunc(GLenum func) { d_ptr->f.DepthFunc(func); }
inline void QOpenGLFunctions::glDepthMask(GLboolean flag) { d_ptr->f.DepthMask(flag); }
inline void QOpenGLFunctions::glDepthRangef(GLfloat n, GLfloat f) { d_ptr->f.DepthRangef(n, f); }
inline void QOpenGLFunctions::glDetachShader(GLuint program, GLuint shader) { d_ptr->f.DetachShader(program, shader); }
inline void QOpenGLFunctions::glDisable(GLenum cap) { d_ptr->f.Disable(cap); }
inline void QOpenGLFunctions::glDisableVertexAttribArray(GLuint index) { d_ptr->f.DisableVertexAttribArray(index); }
inline void QOpenGLFunctions::glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    d_ptr->f.DrawArrays(mode, first, count);
}
inline void QOpenGLFunctions::glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices) {
    d_ptr->f.DrawElements(mode, count, type, indices);
}
inline void QOpenGLFunctions::glEnable(GLenum cap) { d_ptr->f.Enable(cap); }
inline void QOpenGLFunctions::glEnableVertexAttribArray(GLuint index) { d_ptr->f.EnableVertexAttribArray(index); }
inline void QOpenGLFunctions::glFinish() { d_ptr->f.Finish(); }
inline void QOpenGLFunctions::glFlush() { d_ptr->f.Flush(); }
inline void QOpenGLFunctions::glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget,
                                                        GLuint renderbuffer) {
    d_ptr->f.FramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}
inline void QOpenGLFunctions::glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                                                     GLint level) {
    d_ptr->f.FramebufferTexture2D(target, attachment, textarget, texture, level);
}
inline void QOpenGLFunctions::glFrontFace(GLenum mode) { d_ptr->f.FrontFace(mode); }
inline void QOpenGLFunctions::glGenBuffers(GLsizei n, GLuint *buffers) { d_ptr->f.GenBuffers(n, buffers); }
inline void QOpenGLFunctions::glGenerateMipmap(GLenum target) { d_ptr->f.GenerateMipmap(target); }
inline void QOpenGLFunctions::glGenFramebuffers(GLsizei n, GLuint *framebuffers) {
    d_ptr->f.GenFramebuffers(n, framebuffers);
}
inline void QOpenGLFunctions::glGenRenderbuffers(GLsizei n, GLuint *renderbuffers) {
    d_ptr->f.GenRenderbuffers(n, renderbuffers);
}
inline void QOpenGLFunctions::glGenTextures(GLsizei n, GLuint *textures) { d_ptr->f.GenTextures(n, textures); }
inline void QOpenGLFunctions::glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length,
                                                GLint *size, GLenum *type, GLchar *name) {
    d_ptr->f.GetActiveAttrib(program, index, bufSize, length, size, type, name);
}
inline void QOpenGLFunctions::glGetActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length,
                                                 GLint *size, GLenum *type, GLchar *name) {
    d_ptr->f.GetActiveUniform(program, index, bufSize, length, size, type, name);
}
inline void QOpenGLFunctions::glGetAttachedShaders(GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders) {
    d_ptr->f.GetAttachedShaders(program, maxCount, count, shaders);
}
inline GLint QOpenGLFunctions::glGetAttribLocation(GLuint program, const GLchar *name) {
    return d_ptr->f.GetAttribLocation(program, name);
}
inline void QOpenGLFunctions::glGetBooleanv(GLenum pname, GLboolean *data) { d_ptr->f.GetBooleanv(pname, data); }
inline void QOpenGLFunctions::glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    d_ptr->f.GetBufferParameteriv(target, pname, params);
}
inline GLenum QOpenGLFunctions::glGetError() { return d_ptr->f.GetError(); }
inline void QOpenGLFunctions::glGetFloatv(GLenum pname, GLfloat *data) { d_ptr->f.GetFloatv(pname, data); }
inline void QOpenGLFunctions::glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment, GLenum pname,
                                                                    GLint *params) {
    d_ptr->f.GetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}
inline void QOpenGLFunctions::glGetIntegerv(GLenum pname, GLint *data) { d_ptr->f.GetIntegerv(pname, data); }
inline void QOpenGLFunctions::glGetProgramiv(GLuint program, GLenum pname, GLint *params) {
    d_ptr->f.GetProgramiv(program, pname, params);
}
inline void QOpenGLFunctions::glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    d_ptr->f.GetProgramInfoLog(program, bufSize, length, infoLog);
}
inline void QOpenGLFunctions::glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params) {
    d_ptr->f.GetRenderbufferParameteriv(target, pname, params);
}
inline void QOpenGLFunctions::glGetShaderiv(GLuint shader, GLenum pname, GLint *params) {
    d_ptr->f.GetShaderiv(shader, pname, params);
}
inline void QOpenGLFunctions::glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) {
    d_ptr->f.GetShaderInfoLog(shader, bufSize, length, infoLog);
}
inline void QOpenGLFunctions::glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, GLint *range,
                                                         GLint *precision) {
    d_ptr->f.GetShaderPrecisionFormat(shadertype, precisiontype, range, precision);
}
inline void QOpenGLFunctions::glGetShaderSource(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source) {
    d_ptr->f.GetShaderSource(shader, bufSize, length, source);
}
inline const GLubyte *QOpenGLFunctions::glGetString(GLenum name) { return d_ptr->f.GetString(name); }
inline void QOpenGLFunctions::glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params) {
    d_ptr->f.GetTexParameterfv(target, pname, params);
}
inline void QOpenGLFunctions::glGetTexParameteriv(GLenum target, GLenum pname, GLint *params) {
    d_ptr->f.GetTexParameteriv(target, pname, params);
}
inline void QOpenGLFunctions::glGetUniformfv(GLuint program, GLint location, GLfloat *params) {
    d_ptr->f.GetUniformfv(program, location, params);
}
inline void QOpenGLFunctions::glGetUniformiv(GLuint program, GLint location, GLint *params) {
    d_ptr->f.GetUniformiv(program, location, params);
}
inline GLint QOpenGLFunctions::glGetUniformLocation(GLuint program, const GLchar *name) {
    return d_ptr->f.GetUniformLocation(program, name);
}
inline void QOpenGLFunctions::glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params) {
    d_ptr->f.GetVertexAttribfv(index, pname, params);
}
inline void QOpenGLFunctions::glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params) {
    d_ptr->f.GetVertexAttribiv(index, pname, params);
}
inline void QOpenGLFunctions::glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer) {
    d_ptr->f.GetVertexAttribPointerv(index, pname, pointer);
}
inline void QOpenGLFunctions::glHint(GLenum target, GLenum mode) { d_ptr->f.Hint(target, mode); }
inline GLboolean QOpenGLFunctions::glIsBuffer(GLuint buffer) { return d_ptr->f.IsBuffer(buffer); }
inline GLboolean QOpenGLFunctions::glIsEnabled(GLenum cap) { return d_ptr->f.IsEnabled(cap); }
inline GLboolean QOpenGLFunctions::glIsFramebuffer(GLuint framebuffer) { return d_ptr->f.IsFramebuffer(framebuffer); }
inline GLboolean QOpenGLFunctions::glIsProgram(GLuint program) { return d_ptr->f.IsProgram(program); }
inline GLboolean QOpenGLFunctions::glIsRenderbuffer(GLuint renderbuffer) {
    return d_ptr->f.IsRenderbuffer(renderbuffer);
}
inline GLboolean QOpenGLFunctions::glIsShader(GLuint shader) { return d_ptr->f.IsShader(shader); }
inline GLboolean QOpenGLFunctions::glIsTexture(GLuint texture) { return d_ptr->f.IsTexture(texture); }
inline void QOpenGLFunctions::glLineWidth(GLfloat width) { d_ptr->f.LineWidth(width); }
inline void QOpenGLFunctions::glLinkProgram(GLuint program) { d_ptr->f.LinkProgram(program); }
inline void QOpenGLFunctions::glPixelStorei(GLenum pname, GLint param) { d_ptr->f.PixelStorei(pname, param); }
inline void QOpenGLFunctions::glPolygonOffset(GLfloat factor, GLfloat units) { d_ptr->f.PolygonOffset(factor, units); }
inline void QOpenGLFunctions::glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
                                           void *pixels) {
    d_ptr->f.ReadPixels(x, y, width, height, format, type, pixels);
}
inline void QOpenGLFunctions::glReleaseShaderCompiler() { d_ptr->f.ReleaseShaderCompiler(); }
inline void QOpenGLFunctions::glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width,
                                                    GLsizei height) {
    d_ptr->f.RenderbufferStorage(target, internalformat, width, height);
}
inline void QOpenGLFunctions::glSampleCoverage(GLfloat value, GLboolean invert) {
    d_ptr->f.SampleCoverage(value, invert);
}
inline void QOpenGLFunctions::glScissor(GLint x, GLint y, GLsizei width, GLsizei height) {
    d_ptr->f.Scissor(x, y, width, height);
}
inline void QOpenGLFunctions::glShaderBinary(GLsizei count, const GLuint *shaders, GLenum binaryFormat,
                                             const void *binary, GLsizei length) {
    d_ptr->f.ShaderBinary(count, shaders, binaryFormat, binary, length);
}
inline void QOpenGLFunctions::glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string,
                                             const GLint *length) {
    d_ptr->f.ShaderSource(shader, count, string, length);
}
inline void QOpenGLFunctions::glStencilFunc(GLenum func, GLint ref, GLuint mask) {
    d_ptr->f.StencilFunc(func, ref, mask);
}
inline void QOpenGLFunctions::glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
    d_ptr->f.StencilFuncSeparate(face, func, ref, mask);
}
inline void QOpenGLFunctions::glStencilMask(GLuint mask) { d_ptr->f.StencilMask(mask); }
inline void QOpenGLFunctions::glStencilMaskSeparate(GLenum face, GLuint mask) {
    d_ptr->f.StencilMaskSeparate(face, mask);
}
inline void QOpenGLFunctions::glStencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
    d_ptr->f.StencilOp(fail, zfail, zpass);
}
inline void QOpenGLFunctions::glStencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
    d_ptr->f.StencilOpSeparate(face, sfail, dpfail, dppass);
}
inline void QOpenGLFunctions::glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                           GLsizei height, GLint border, GLenum format, GLenum type,
                                           const void *pixels) {
    d_ptr->f.TexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}
inline void QOpenGLFunctions::glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    d_ptr->f.TexParameterf(target, pname, param);
}
inline void QOpenGLFunctions::glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params) {
    d_ptr->f.TexParameterfv(target, pname, params);
}
inline void QOpenGLFunctions::glTexParameteri(GLenum target, GLenum pname, GLint param) {
    d_ptr->f.TexParameteri(target, pname, param);
}
inline void QOpenGLFunctions::glTexParameteriv(GLenum target, GLenum pname, const GLint *params) {
    d_ptr->f.TexParameteriv(target, pname, params);
}
inline void QOpenGLFunctions::glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                                              GLsizei height, GLenum format, GLenum type, const void *pixels) {
    d_ptr->f.TexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}
inline void QOpenGLFunctions::glUniform1f(GLint location, GLfloat v0) { d_ptr->f.Uniform1f(location, v0); }
inline void QOpenGLFunctions::glUniform1fv(GLint location, GLsizei count, const GLfloat *value) {
    d_ptr->f.Uniform1fv(location, count, value);
}
inline void QOpenGLFunctions::glUniform1i(GLint location, GLint v0) { d_ptr->f.Uniform1i(location, v0); }
inline void QOpenGLFunctions::glUniform1iv(GLint location, GLsizei count, const GLint *value) {
    d_ptr->f.Uniform1iv(location, count, value);
}
inline void QOpenGLFunctions::glUniform2f(GLint location, GLfloat v0, GLfloat v1) {
    d_ptr->f.Uniform2f(location, v0, v1);
}
inline void QOpenGLFunctions::glUniform2fv(GLint location, GLsizei count, const GLfloat *value) {
    d_ptr->f.Uniform2fv(location, count, value);
}
inline void QOpenGLFunctions::glUniform2i(GLint location, GLint v0, GLint v1) { d_ptr->f.Uniform2i(location, v0, v1); }
inline void QOpenGLFunctions::glUniform2iv(GLint location, GLsizei count, const GLint *value) {
    d_ptr->f.Uniform2iv(location, count, value);
}
inline void QOpenGLFunctions::glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2) {
    d_ptr->f.Uniform3f(location, v0, v1, v2);
}
inline void QOpenGLFunctions::glUniform3fv(GLint location, GLsizei count, const GLfloat *value) {
    d_ptr->f.Uniform3fv(location, count, value);
}
inline void QOpenGLFunctions::glUniform3i(GLint location, GLint v0, GLint v1, GLint v2) {
    d_ptr->f.Uniform3i(location, v0, v1, v2);
}
inline void QOpenGLFunctions::glUniform3iv(GLint location, GLsizei count, const GLint *value) {
    d_ptr->f.Uniform3iv(location, count, value);
}
inline void QOpenGLFunctions::glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) {
    d_ptr->f.Uniform4f(location, v0, v1, v2, v3);
}
inline void QOpenGLFunctions::glUniform4fv(GLint location, GLsizei count, const GLfloat *value) {
    d_ptr->f.Uniform4fv(location, count, value);
}
inline void QOpenGLFunctions::glUniform4i(GLint location, GLint v0, GLint v1, GLint v2, GLint v3) {
    d_ptr->f.Uniform4i(location, v0, v1, v2, v3);
}
inline void QOpenGLFunctions::glUniform4iv(GLint location, GLsizei count, const GLint *value) {
    d_ptr->f.Uniform4iv(location, count, value);
}
inline void QOpenGLFunctions::glUniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose,
                                                 const GLfloat *value) {
    d_ptr->f.UniformMatrix2fv(location, count, transpose, value);
}
inline void QOpenGLFunctions::glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose,
                                                 const GLfloat *value) {
    d_ptr->f.UniformMatrix3fv(location, count, transpose, value);
}
inline void QOpenGLFunctions::glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                                                 const GLfloat *value) {
    d_ptr->f.UniformMatrix4fv(location, count, transpose, value);
}
inline void QOpenGLFunctions::glUseProgram(GLuint program) { d_ptr->f.UseProgram(program); }
inline void QOpenGLFunctions::glValidateProgram(GLuint program) { d_ptr->f.ValidateProgram(program); }
inline void QOpenGLFunctions::glVertexAttrib1f(GLuint index, GLfloat x) { d_ptr->f.VertexAttrib1f(index, x); }
inline void QOpenGLFunctions::glVertexAttrib1fv(GLuint index, const GLfloat *v) { d_ptr->f.VertexAttrib1fv(index, v); }
inline void QOpenGLFunctions::glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) {
    d_ptr->f.VertexAttrib2f(index, x, y);
}
inline void QOpenGLFunctions::glVertexAttrib2fv(GLuint index, const GLfloat *v) { d_ptr->f.VertexAttrib2fv(index, v); }
inline void QOpenGLFunctions::glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) {
    d_ptr->f.VertexAttrib3f(index, x, y, z);
}
inline void QOpenGLFunctions::glVertexAttrib3fv(GLuint index, const GLfloat *v) { d_ptr->f.VertexAttrib3fv(index, v); }
inline void QOpenGLFunctions::glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    d_ptr->f.VertexAttrib4f(index, x, y, z, w);
}
inline void QOpenGLFunctions::glVertexAttrib4fv(GLuint index, const GLfloat *v) { d_ptr->f.VertexAttrib4fv(index, v); }
inline void QOpenGLFunctions::glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                                    GLsizei stride, const void *pointer) {
    d_ptr->f.VertexAttribPointer(index, size, type, normalized, stride, pointer);
}
inline void QOpenGLFunctions::glViewport(GLint x, GLint y, GLsizei width, GLsizei height) {
    d_ptr->f.Viewport(x, y, width, height);
}
