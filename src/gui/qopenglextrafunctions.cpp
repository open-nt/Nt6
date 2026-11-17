// MIT License - see LICENSE
#include <QtGui/qopenglextrafunctions.h>

#include <QtCore/qdebug.h>
#include <QtGui/qopenglcontext.h>

#include "qwindow_p.h"

namespace {

template <typename F> void resolve(QOpenGLContext *context, const char *name, F *out) {
    *out = reinterpret_cast<F>(context->getProcAddress(name)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

// Invalidating is a hint, so desktop OpenGL before 4.3 may ignore it.
void QOPENGLF_APIENTRY invalidateFramebuffer(GLenum, GLsizei, const GLenum *) {}
void QOPENGLF_APIENTRY invalidateSubFramebuffer(GLenum, GLsizei, const GLenum *, GLint, GLint, GLsizei, GLsizei) {}

} // namespace

/*!
    \class QOpenGLExtraFunctionsPrivate
    \internal
    \brief The OpenGL ES 3.0 function pointers of one context, behind QOpenGLExtraFunctions.
*/

/*!
    \class QOpenGLExtraFunctionsPrivate::Functions
    \internal
*/

/*!
    \internal
    Resolves the functions of \a context.
*/
QOpenGLExtraFunctionsPrivate::QOpenGLExtraFunctionsPrivate(QOpenGLContext *context) : f{} {
    resolve(context, "glReadBuffer", &f.ReadBuffer);
    resolve(context, "glDrawRangeElements", &f.DrawRangeElements);
    resolve(context, "glTexImage3D", &f.TexImage3D);
    resolve(context, "glTexSubImage3D", &f.TexSubImage3D);
    resolve(context, "glCopyTexSubImage3D", &f.CopyTexSubImage3D);
    resolve(context, "glCompressedTexImage3D", &f.CompressedTexImage3D);
    resolve(context, "glCompressedTexSubImage3D", &f.CompressedTexSubImage3D);
    resolve(context, "glGenQueries", &f.GenQueries);
    resolve(context, "glDeleteQueries", &f.DeleteQueries);
    resolve(context, "glIsQuery", &f.IsQuery);
    resolve(context, "glBeginQuery", &f.BeginQuery);
    resolve(context, "glEndQuery", &f.EndQuery);
    resolve(context, "glGetQueryiv", &f.GetQueryiv);
    resolve(context, "glGetQueryObjectuiv", &f.GetQueryObjectuiv);
    resolve(context, "glUnmapBuffer", &f.UnmapBuffer);
    resolve(context, "glGetBufferPointerv", &f.GetBufferPointerv);
    resolve(context, "glDrawBuffers", &f.DrawBuffers);
    resolve(context, "glUniformMatrix2x3fv", &f.UniformMatrix2x3fv);
    resolve(context, "glUniformMatrix3x2fv", &f.UniformMatrix3x2fv);
    resolve(context, "glUniformMatrix2x4fv", &f.UniformMatrix2x4fv);
    resolve(context, "glUniformMatrix4x2fv", &f.UniformMatrix4x2fv);
    resolve(context, "glUniformMatrix3x4fv", &f.UniformMatrix3x4fv);
    resolve(context, "glUniformMatrix4x3fv", &f.UniformMatrix4x3fv);
    resolve(context, "glBlitFramebuffer", &f.BlitFramebuffer);
    resolve(context, "glRenderbufferStorageMultisample", &f.RenderbufferStorageMultisample);
    resolve(context, "glFramebufferTextureLayer", &f.FramebufferTextureLayer);
    resolve(context, "glMapBufferRange", &f.MapBufferRange);
    resolve(context, "glFlushMappedBufferRange", &f.FlushMappedBufferRange);
    resolve(context, "glBindVertexArray", &f.BindVertexArray);
    resolve(context, "glDeleteVertexArrays", &f.DeleteVertexArrays);
    resolve(context, "glGenVertexArrays", &f.GenVertexArrays);
    resolve(context, "glIsVertexArray", &f.IsVertexArray);
    resolve(context, "glGetIntegeri_v", &f.GetIntegeri_v);
    resolve(context, "glBeginTransformFeedback", &f.BeginTransformFeedback);
    resolve(context, "glEndTransformFeedback", &f.EndTransformFeedback);
    resolve(context, "glBindBufferRange", &f.BindBufferRange);
    resolve(context, "glBindBufferBase", &f.BindBufferBase);
    resolve(context, "glTransformFeedbackVaryings", &f.TransformFeedbackVaryings);
    resolve(context, "glGetTransformFeedbackVarying", &f.GetTransformFeedbackVarying);
    resolve(context, "glVertexAttribIPointer", &f.VertexAttribIPointer);
    resolve(context, "glGetVertexAttribIiv", &f.GetVertexAttribIiv);
    resolve(context, "glGetVertexAttribIuiv", &f.GetVertexAttribIuiv);
    resolve(context, "glVertexAttribI4i", &f.VertexAttribI4i);
    resolve(context, "glVertexAttribI4ui", &f.VertexAttribI4ui);
    resolve(context, "glVertexAttribI4iv", &f.VertexAttribI4iv);
    resolve(context, "glVertexAttribI4uiv", &f.VertexAttribI4uiv);
    resolve(context, "glGetUniformuiv", &f.GetUniformuiv);
    resolve(context, "glGetFragDataLocation", &f.GetFragDataLocation);
    resolve(context, "glUniform1ui", &f.Uniform1ui);
    resolve(context, "glUniform2ui", &f.Uniform2ui);
    resolve(context, "glUniform3ui", &f.Uniform3ui);
    resolve(context, "glUniform4ui", &f.Uniform4ui);
    resolve(context, "glUniform1uiv", &f.Uniform1uiv);
    resolve(context, "glUniform2uiv", &f.Uniform2uiv);
    resolve(context, "glUniform3uiv", &f.Uniform3uiv);
    resolve(context, "glUniform4uiv", &f.Uniform4uiv);
    resolve(context, "glClearBufferiv", &f.ClearBufferiv);
    resolve(context, "glClearBufferuiv", &f.ClearBufferuiv);
    resolve(context, "glClearBufferfv", &f.ClearBufferfv);
    resolve(context, "glClearBufferfi", &f.ClearBufferfi);
    resolve(context, "glGetStringi", &f.GetStringi);
    resolve(context, "glCopyBufferSubData", &f.CopyBufferSubData);
    resolve(context, "glGetUniformIndices", &f.GetUniformIndices);
    resolve(context, "glGetActiveUniformsiv", &f.GetActiveUniformsiv);
    resolve(context, "glGetUniformBlockIndex", &f.GetUniformBlockIndex);
    resolve(context, "glGetActiveUniformBlockiv", &f.GetActiveUniformBlockiv);
    resolve(context, "glGetActiveUniformBlockName", &f.GetActiveUniformBlockName);
    resolve(context, "glUniformBlockBinding", &f.UniformBlockBinding);
    resolve(context, "glDrawArraysInstanced", &f.DrawArraysInstanced);
    resolve(context, "glDrawElementsInstanced", &f.DrawElementsInstanced);
    resolve(context, "glFenceSync", &f.FenceSync);
    resolve(context, "glIsSync", &f.IsSync);
    resolve(context, "glDeleteSync", &f.DeleteSync);
    resolve(context, "glClientWaitSync", &f.ClientWaitSync);
    resolve(context, "glWaitSync", &f.WaitSync);
    resolve(context, "glGetInteger64v", &f.GetInteger64v);
    resolve(context, "glGetSynciv", &f.GetSynciv);
    resolve(context, "glGetInteger64i_v", &f.GetInteger64i_v);
    resolve(context, "glGetBufferParameteri64v", &f.GetBufferParameteri64v);
    resolve(context, "glGenSamplers", &f.GenSamplers);
    resolve(context, "glDeleteSamplers", &f.DeleteSamplers);
    resolve(context, "glIsSampler", &f.IsSampler);
    resolve(context, "glBindSampler", &f.BindSampler);
    resolve(context, "glSamplerParameteri", &f.SamplerParameteri);
    resolve(context, "glSamplerParameteriv", &f.SamplerParameteriv);
    resolve(context, "glSamplerParameterf", &f.SamplerParameterf);
    resolve(context, "glSamplerParameterfv", &f.SamplerParameterfv);
    resolve(context, "glGetSamplerParameteriv", &f.GetSamplerParameteriv);
    resolve(context, "glGetSamplerParameterfv", &f.GetSamplerParameterfv);
    resolve(context, "glVertexAttribDivisor", &f.VertexAttribDivisor);
    resolve(context, "glBindTransformFeedback", &f.BindTransformFeedback);
    resolve(context, "glDeleteTransformFeedbacks", &f.DeleteTransformFeedbacks);
    resolve(context, "glGenTransformFeedbacks", &f.GenTransformFeedbacks);
    resolve(context, "glIsTransformFeedback", &f.IsTransformFeedback);
    resolve(context, "glPauseTransformFeedback", &f.PauseTransformFeedback);
    resolve(context, "glResumeTransformFeedback", &f.ResumeTransformFeedback);
    resolve(context, "glGetProgramBinary", &f.GetProgramBinary);
    resolve(context, "glProgramBinary", &f.ProgramBinary);
    resolve(context, "glProgramParameteri", &f.ProgramParameteri);
    resolve(context, "glInvalidateFramebuffer", &f.InvalidateFramebuffer);
    resolve(context, "glInvalidateSubFramebuffer", &f.InvalidateSubFramebuffer);
    resolve(context, "glTexStorage2D", &f.TexStorage2D);
    resolve(context, "glTexStorage3D", &f.TexStorage3D);
    resolve(context, "glGetInternalformativ", &f.GetInternalformativ);
    if (!context->isOpenGLES()) {
        if (!f.InvalidateFramebuffer)
            f.InvalidateFramebuffer = invalidateFramebuffer;
        if (!f.InvalidateSubFramebuffer)
            f.InvalidateSubFramebuffer = invalidateSubFramebuffer;
    }
}

/*!
    \class QOpenGLExtraFunctions
    \inmodule NtGui
    \since 6.0
    \brief The QOpenGLExtraFunctions class provides the OpenGL ES 3.0 API on desktop OpenGL and OpenGL ES alike.

    QOpenGLExtraFunctions adds the functions of OpenGL ES 3.0 to those of
    QOpenGLFunctions: vertex array objects, instanced drawing, framebuffer
    blits and multisampled renderbuffers, several render targets, mapped
    buffer ranges, uniform buffers, 3D and array textures, samplers, queries,
    sync objects and transform feedback. They are present on OpenGL ES 3.0 and
    desktop OpenGL 3.3. Get the functions of a context with
    QOpenGLContext::extraFunctions(), or derive from the class and call
    initializeOpenGLFunctions() once a context is current:

    \code
    class Renderer : protected QOpenGLExtraFunctions {
    public:
        void initialize() {
            initializeOpenGLFunctions();
            glGenVertexArrays(1, &m_vao);
        }
        void render(GLsizei count, GLsizei instances) {
            glBindVertexArray(m_vao);
            glDrawElementsInstanced(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr, instances);
            glBindVertexArray(0);
        }

    private:
        GLuint m_vao = 0;
    };
    \endcode

    \differences
    Only the functions of OpenGL ES 3.0 are present; those that OpenGL ES 3.1
    and 3.2 added, such as compute shaders and indirect drawing, are not.
    Calling a function that the context lacks, on OpenGL ES 2.0 or desktop
    OpenGL before 3.0, crashes, as in Qt. On desktop OpenGL before 4.3,
    \c glInvalidateFramebuffer() and \c glInvalidateSubFramebuffer() do
    nothing.
    \enddifferences

    \sa QOpenGLFunctions, QOpenGLContext::extraFunctions()
*/

/*!
    Constructs a function object that is not initialized yet. Call
    initializeOpenGLFunctions() when a context is current.
*/
QOpenGLExtraFunctions::QOpenGLExtraFunctions() = default;

/*!
    Constructs a function object for \a context, which must have been created.
*/
QOpenGLExtraFunctions::QOpenGLExtraFunctions(QOpenGLContext *context)
    : QOpenGLFunctions(context), e_ptr(context ? QtPrivate::GuiAccess::openGLExtraFunctions(context) : nullptr) {}

/*!
    \fn QOpenGLExtraFunctions::~QOpenGLExtraFunctions()

    Destroys the function object.
*/

/*!
    \variable QOpenGLExtraFunctions::e_ptr
    \internal
*/

/*!
    Initializes the functions, those of OpenGL ES 2.0 too, for the current
    context.
*/
void QOpenGLExtraFunctions::initializeOpenGLFunctions() {
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context) {
        qWarning("QOpenGLExtraFunctions::initializeOpenGLFunctions(): no context is current");
        return;
    }
    QOpenGLFunctions::initializeOpenGLFunctions();
    e_ptr = QtPrivate::GuiAccess::openGLExtraFunctions(context);
}

/*!
    \fn void QOpenGLExtraFunctions::glReadBuffer(GLenum src)

    Calls the OpenGL ES 3.0 function \c glReadBuffer with \a src.
*/

/*!
    \fn void QOpenGLExtraFunctions::glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices)

    Calls the OpenGL ES 3.0 function \c glDrawRangeElements with \a mode, \a start, \a end, \a count, \a type and \a indices.
*/

/*!
    \fn void QOpenGLExtraFunctions::glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels)

    Calls the OpenGL ES 3.0 function \c glTexImage3D with \a target, \a level, \a internalformat, \a width, \a height, \a depth, \a border, \a format, \a type and \a pixels.
*/

/*!
    \fn void QOpenGLExtraFunctions::glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels)

    Calls the OpenGL ES 3.0 function \c glTexSubImage3D with \a target, \a level, \a xoffset, \a yoffset, \a zoffset, \a width, \a height, \a depth, \a format, \a type and \a pixels.
*/

/*!
    \fn void QOpenGLExtraFunctions::glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)

    Calls the OpenGL ES 3.0 function \c glCopyTexSubImage3D with \a target, \a level, \a xoffset, \a yoffset, \a zoffset, \a x, \a y, \a width and \a height.
*/

/*!
    \fn void QOpenGLExtraFunctions::glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data)

    Calls the OpenGL ES 3.0 function \c glCompressedTexImage3D with \a target, \a level, \a internalformat, \a width, \a height, \a depth, \a border, \a imageSize and \a data.
*/

/*!
    \fn void QOpenGLExtraFunctions::glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data)

    Calls the OpenGL ES 3.0 function \c glCompressedTexSubImage3D with \a target, \a level, \a xoffset, \a yoffset, \a zoffset, \a width, \a height, \a depth, \a format, \a imageSize and \a data.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGenQueries(GLsizei n, GLuint *ids)

    Calls the OpenGL ES 3.0 function \c glGenQueries with \a n and \a ids.
*/

/*!
    \fn void QOpenGLExtraFunctions::glDeleteQueries(GLsizei n, const GLuint *ids)

    Calls the OpenGL ES 3.0 function \c glDeleteQueries with \a n and \a ids.
*/

/*!
    \fn GLboolean QOpenGLExtraFunctions::glIsQuery(GLuint id)

    Calls the OpenGL ES 3.0 function \c glIsQuery with \a id and returns its result.
*/

/*!
    \fn void QOpenGLExtraFunctions::glBeginQuery(GLenum target, GLuint id)

    Calls the OpenGL ES 3.0 function \c glBeginQuery with \a target and \a id.
*/

/*!
    \fn void QOpenGLExtraFunctions::glEndQuery(GLenum target)

    Calls the OpenGL ES 3.0 function \c glEndQuery with \a target.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetQueryiv(GLenum target, GLenum pname, GLint *params)

    Calls the OpenGL ES 3.0 function \c glGetQueryiv with \a target, \a pname and \a params.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)

    Calls the OpenGL ES 3.0 function \c glGetQueryObjectuiv with \a id, \a pname and \a params.
*/

/*!
    \fn GLboolean QOpenGLExtraFunctions::glUnmapBuffer(GLenum target)

    Calls the OpenGL ES 3.0 function \c glUnmapBuffer with \a target and returns its result.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetBufferPointerv(GLenum target, GLenum pname, void **params)

    Calls the OpenGL ES 3.0 function \c glGetBufferPointerv with \a target, \a pname and \a params.
*/

/*!
    \fn void QOpenGLExtraFunctions::glDrawBuffers(GLsizei n, const GLenum *bufs)

    Calls the OpenGL ES 3.0 function \c glDrawBuffers with \a n and \a bufs.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)

    Calls the OpenGL ES 3.0 function \c glUniformMatrix2x3fv with \a location, \a count, \a transpose and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)

    Calls the OpenGL ES 3.0 function \c glUniformMatrix3x2fv with \a location, \a count, \a transpose and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)

    Calls the OpenGL ES 3.0 function \c glUniformMatrix2x4fv with \a location, \a count, \a transpose and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)

    Calls the OpenGL ES 3.0 function \c glUniformMatrix4x2fv with \a location, \a count, \a transpose and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)

    Calls the OpenGL ES 3.0 function \c glUniformMatrix3x4fv with \a location, \a count, \a transpose and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)

    Calls the OpenGL ES 3.0 function \c glUniformMatrix4x3fv with \a location, \a count, \a transpose and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)

    Calls the OpenGL ES 3.0 function \c glBlitFramebuffer with \a srcX0, \a srcY0, \a srcX1, \a srcY1, \a dstX0, \a dstY0, \a dstX1, \a dstY1, \a mask and \a filter.
*/

/*!
    \fn void QOpenGLExtraFunctions::glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height)

    Calls the OpenGL ES 3.0 function \c glRenderbufferStorageMultisample with \a target, \a samples, \a internalformat, \a width and \a height.
*/

/*!
    \fn void QOpenGLExtraFunctions::glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer)

    Calls the OpenGL ES 3.0 function \c glFramebufferTextureLayer with \a target, \a attachment, \a texture, \a level and \a layer.
*/

/*!
    \fn void *QOpenGLExtraFunctions::glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access)

    Calls the OpenGL ES 3.0 function \c glMapBufferRange with \a target, \a offset, \a length and \a access and returns its result.
*/

/*!
    \fn void QOpenGLExtraFunctions::glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length)

    Calls the OpenGL ES 3.0 function \c glFlushMappedBufferRange with \a target, \a offset and \a length.
*/

/*!
    \fn void QOpenGLExtraFunctions::glBindVertexArray(GLuint array)

    Calls the OpenGL ES 3.0 function \c glBindVertexArray with \a array.
*/

/*!
    \fn void QOpenGLExtraFunctions::glDeleteVertexArrays(GLsizei n, const GLuint *arrays)

    Calls the OpenGL ES 3.0 function \c glDeleteVertexArrays with \a n and \a arrays.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGenVertexArrays(GLsizei n, GLuint *arrays)

    Calls the OpenGL ES 3.0 function \c glGenVertexArrays with \a n and \a arrays.
*/

/*!
    \fn GLboolean QOpenGLExtraFunctions::glIsVertexArray(GLuint array)

    Calls the OpenGL ES 3.0 function \c glIsVertexArray with \a array and returns its result.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetIntegeri_v(GLenum target, GLuint index, GLint *data)

    Calls the OpenGL ES 3.0 function \c glGetIntegeri_v with \a target, \a index and \a data.
*/

/*!
    \fn void QOpenGLExtraFunctions::glBeginTransformFeedback(GLenum primitiveMode)

    Calls the OpenGL ES 3.0 function \c glBeginTransformFeedback with \a primitiveMode.
*/

/*!
    \fn void QOpenGLExtraFunctions::glEndTransformFeedback()

    Calls the OpenGL ES 3.0 function \c glEndTransformFeedback.
*/

/*!
    \fn void QOpenGLExtraFunctions::glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)

    Calls the OpenGL ES 3.0 function \c glBindBufferRange with \a target, \a index, \a buffer, \a offset and \a size.
*/

/*!
    \fn void QOpenGLExtraFunctions::glBindBufferBase(GLenum target, GLuint index, GLuint buffer)

    Calls the OpenGL ES 3.0 function \c glBindBufferBase with \a target, \a index and \a buffer.
*/

/*!
    \fn void QOpenGLExtraFunctions::glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar *const *varyings, GLenum bufferMode)

    Calls the OpenGL ES 3.0 function \c glTransformFeedbackVaryings with \a program, \a count, \a varyings and \a bufferMode.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size, GLenum *type, GLchar *name)

    Calls the OpenGL ES 3.0 function \c glGetTransformFeedbackVarying with \a program, \a index, \a bufSize, \a length, \a size, \a type and \a name.
*/

/*!
    \fn void QOpenGLExtraFunctions::glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer)

    Calls the OpenGL ES 3.0 function \c glVertexAttribIPointer with \a index, \a size, \a type, \a stride and \a pointer.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params)

    Calls the OpenGL ES 3.0 function \c glGetVertexAttribIiv with \a index, \a pname and \a params.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params)

    Calls the OpenGL ES 3.0 function \c glGetVertexAttribIuiv with \a index, \a pname and \a params.
*/

/*!
    \fn void QOpenGLExtraFunctions::glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w)

    Calls the OpenGL ES 3.0 function \c glVertexAttribI4i with \a index, \a x, \a y, \a z and \a w.
*/

/*!
    \fn void QOpenGLExtraFunctions::glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)

    Calls the OpenGL ES 3.0 function \c glVertexAttribI4ui with \a index, \a x, \a y, \a z and \a w.
*/

/*!
    \fn void QOpenGLExtraFunctions::glVertexAttribI4iv(GLuint index, const GLint *v)

    Calls the OpenGL ES 3.0 function \c glVertexAttribI4iv with \a index and \a v.
*/

/*!
    \fn void QOpenGLExtraFunctions::glVertexAttribI4uiv(GLuint index, const GLuint *v)

    Calls the OpenGL ES 3.0 function \c glVertexAttribI4uiv with \a index and \a v.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetUniformuiv(GLuint program, GLint location, GLuint *params)

    Calls the OpenGL ES 3.0 function \c glGetUniformuiv with \a program, \a location and \a params.
*/

/*!
    \fn GLint QOpenGLExtraFunctions::glGetFragDataLocation(GLuint program, const GLchar *name)

    Calls the OpenGL ES 3.0 function \c glGetFragDataLocation with \a program and \a name and returns its result.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniform1ui(GLint location, GLuint v0)

    Calls the OpenGL ES 3.0 function \c glUniform1ui with \a location and \a v0.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniform2ui(GLint location, GLuint v0, GLuint v1)

    Calls the OpenGL ES 3.0 function \c glUniform2ui with \a location, \a v0 and \a v1.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2)

    Calls the OpenGL ES 3.0 function \c glUniform3ui with \a location, \a v0, \a v1 and \a v2.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)

    Calls the OpenGL ES 3.0 function \c glUniform4ui with \a location, \a v0, \a v1, \a v2 and \a v3.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniform1uiv(GLint location, GLsizei count, const GLuint *value)

    Calls the OpenGL ES 3.0 function \c glUniform1uiv with \a location, \a count and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniform2uiv(GLint location, GLsizei count, const GLuint *value)

    Calls the OpenGL ES 3.0 function \c glUniform2uiv with \a location, \a count and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniform3uiv(GLint location, GLsizei count, const GLuint *value)

    Calls the OpenGL ES 3.0 function \c glUniform3uiv with \a location, \a count and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniform4uiv(GLint location, GLsizei count, const GLuint *value)

    Calls the OpenGL ES 3.0 function \c glUniform4uiv with \a location, \a count and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value)

    Calls the OpenGL ES 3.0 function \c glClearBufferiv with \a buffer, \a drawbuffer and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value)

    Calls the OpenGL ES 3.0 function \c glClearBufferuiv with \a buffer, \a drawbuffer and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value)

    Calls the OpenGL ES 3.0 function \c glClearBufferfv with \a buffer, \a drawbuffer and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)

    Calls the OpenGL ES 3.0 function \c glClearBufferfi with \a buffer, \a drawbuffer, \a depth and \a stencil.
*/

/*!
    \fn const GLubyte *QOpenGLExtraFunctions::glGetStringi(GLenum name, GLuint index)

    Calls the OpenGL ES 3.0 function \c glGetStringi with \a name and \a index and returns its result.
*/

/*!
    \fn void QOpenGLExtraFunctions::glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size)

    Calls the OpenGL ES 3.0 function \c glCopyBufferSubData with \a readTarget, \a writeTarget, \a readOffset, \a writeOffset and \a size.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar *const *uniformNames, GLuint *uniformIndices)

    Calls the OpenGL ES 3.0 function \c glGetUniformIndices with \a program, \a uniformCount, \a uniformNames and \a uniformIndices.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname, GLint *params)

    Calls the OpenGL ES 3.0 function \c glGetActiveUniformsiv with \a program, \a uniformCount, \a uniformIndices, \a pname and \a params.
*/

/*!
    \fn GLuint QOpenGLExtraFunctions::glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName)

    Calls the OpenGL ES 3.0 function \c glGetUniformBlockIndex with \a program and \a uniformBlockName and returns its result.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params)

    Calls the OpenGL ES 3.0 function \c glGetActiveUniformBlockiv with \a program, \a uniformBlockIndex, \a pname and \a params.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length, GLchar *uniformBlockName)

    Calls the OpenGL ES 3.0 function \c glGetActiveUniformBlockName with \a program, \a uniformBlockIndex, \a bufSize, \a length and \a uniformBlockName.
*/

/*!
    \fn void QOpenGLExtraFunctions::glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)

    Calls the OpenGL ES 3.0 function \c glUniformBlockBinding with \a program, \a uniformBlockIndex and \a uniformBlockBinding.
*/

/*!
    \fn void QOpenGLExtraFunctions::glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount)

    Calls the OpenGL ES 3.0 function \c glDrawArraysInstanced with \a mode, \a first, \a count and \a instancecount.
*/

/*!
    \fn void QOpenGLExtraFunctions::glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount)

    Calls the OpenGL ES 3.0 function \c glDrawElementsInstanced with \a mode, \a count, \a type, \a indices and \a instancecount.
*/

/*!
    \fn GLsync QOpenGLExtraFunctions::glFenceSync(GLenum condition, GLbitfield flags)

    Calls the OpenGL ES 3.0 function \c glFenceSync with \a condition and \a flags and returns its result.
*/

/*!
    \fn GLboolean QOpenGLExtraFunctions::glIsSync(GLsync sync)

    Calls the OpenGL ES 3.0 function \c glIsSync with \a sync and returns its result.
*/

/*!
    \fn void QOpenGLExtraFunctions::glDeleteSync(GLsync sync)

    Calls the OpenGL ES 3.0 function \c glDeleteSync with \a sync.
*/

/*!
    \fn GLenum QOpenGLExtraFunctions::glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)

    Calls the OpenGL ES 3.0 function \c glClientWaitSync with \a sync, \a flags and \a timeout and returns its result.
*/

/*!
    \fn void QOpenGLExtraFunctions::glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)

    Calls the OpenGL ES 3.0 function \c glWaitSync with \a sync, \a flags and \a timeout.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetInteger64v(GLenum pname, GLint64 *data)

    Calls the OpenGL ES 3.0 function \c glGetInteger64v with \a pname and \a data.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values)

    Calls the OpenGL ES 3.0 function \c glGetSynciv with \a sync, \a pname, \a bufSize, \a length and \a values.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetInteger64i_v(GLenum target, GLuint index, GLint64 *data)

    Calls the OpenGL ES 3.0 function \c glGetInteger64i_v with \a target, \a index and \a data.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params)

    Calls the OpenGL ES 3.0 function \c glGetBufferParameteri64v with \a target, \a pname and \a params.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGenSamplers(GLsizei count, GLuint *samplers)

    Calls the OpenGL ES 3.0 function \c glGenSamplers with \a count and \a samplers.
*/

/*!
    \fn void QOpenGLExtraFunctions::glDeleteSamplers(GLsizei count, const GLuint *samplers)

    Calls the OpenGL ES 3.0 function \c glDeleteSamplers with \a count and \a samplers.
*/

/*!
    \fn GLboolean QOpenGLExtraFunctions::glIsSampler(GLuint sampler)

    Calls the OpenGL ES 3.0 function \c glIsSampler with \a sampler and returns its result.
*/

/*!
    \fn void QOpenGLExtraFunctions::glBindSampler(GLuint unit, GLuint sampler)

    Calls the OpenGL ES 3.0 function \c glBindSampler with \a unit and \a sampler.
*/

/*!
    \fn void QOpenGLExtraFunctions::glSamplerParameteri(GLuint sampler, GLenum pname, GLint param)

    Calls the OpenGL ES 3.0 function \c glSamplerParameteri with \a sampler, \a pname and \a param.
*/

/*!
    \fn void QOpenGLExtraFunctions::glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *param)

    Calls the OpenGL ES 3.0 function \c glSamplerParameteriv with \a sampler, \a pname and \a param.
*/

/*!
    \fn void QOpenGLExtraFunctions::glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param)

    Calls the OpenGL ES 3.0 function \c glSamplerParameterf with \a sampler, \a pname and \a param.
*/

/*!
    \fn void QOpenGLExtraFunctions::glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param)

    Calls the OpenGL ES 3.0 function \c glSamplerParameterfv with \a sampler, \a pname and \a param.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params)

    Calls the OpenGL ES 3.0 function \c glGetSamplerParameteriv with \a sampler, \a pname and \a params.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params)

    Calls the OpenGL ES 3.0 function \c glGetSamplerParameterfv with \a sampler, \a pname and \a params.
*/

/*!
    \fn void QOpenGLExtraFunctions::glVertexAttribDivisor(GLuint index, GLuint divisor)

    Calls the OpenGL ES 3.0 function \c glVertexAttribDivisor with \a index and \a divisor.
*/

/*!
    \fn void QOpenGLExtraFunctions::glBindTransformFeedback(GLenum target, GLuint id)

    Calls the OpenGL ES 3.0 function \c glBindTransformFeedback with \a target and \a id.
*/

/*!
    \fn void QOpenGLExtraFunctions::glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids)

    Calls the OpenGL ES 3.0 function \c glDeleteTransformFeedbacks with \a n and \a ids.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGenTransformFeedbacks(GLsizei n, GLuint *ids)

    Calls the OpenGL ES 3.0 function \c glGenTransformFeedbacks with \a n and \a ids.
*/

/*!
    \fn GLboolean QOpenGLExtraFunctions::glIsTransformFeedback(GLuint id)

    Calls the OpenGL ES 3.0 function \c glIsTransformFeedback with \a id and returns its result.
*/

/*!
    \fn void QOpenGLExtraFunctions::glPauseTransformFeedback()

    Calls the OpenGL ES 3.0 function \c glPauseTransformFeedback.
*/

/*!
    \fn void QOpenGLExtraFunctions::glResumeTransformFeedback()

    Calls the OpenGL ES 3.0 function \c glResumeTransformFeedback.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary)

    Calls the OpenGL ES 3.0 function \c glGetProgramBinary with \a program, \a bufSize, \a length, \a binaryFormat and \a binary.
*/

/*!
    \fn void QOpenGLExtraFunctions::glProgramBinary(GLuint program, GLenum binaryFormat, const void *binary, GLsizei length)

    Calls the OpenGL ES 3.0 function \c glProgramBinary with \a program, \a binaryFormat, \a binary and \a length.
*/

/*!
    \fn void QOpenGLExtraFunctions::glProgramParameteri(GLuint program, GLenum pname, GLint value)

    Calls the OpenGL ES 3.0 function \c glProgramParameteri with \a program, \a pname and \a value.
*/

/*!
    \fn void QOpenGLExtraFunctions::glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments)

    Calls the OpenGL ES 3.0 function \c glInvalidateFramebuffer with \a target, \a numAttachments and \a attachments.
*/

/*!
    \fn void QOpenGLExtraFunctions::glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y, GLsizei width, GLsizei height)

    Calls the OpenGL ES 3.0 function \c glInvalidateSubFramebuffer with \a target, \a numAttachments, \a attachments, \a x, \a y, \a width and \a height.
*/

/*!
    \fn void QOpenGLExtraFunctions::glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height)

    Calls the OpenGL ES 3.0 function \c glTexStorage2D with \a target, \a levels, \a internalformat, \a width and \a height.
*/

/*!
    \fn void QOpenGLExtraFunctions::glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth)

    Calls the OpenGL ES 3.0 function \c glTexStorage3D with \a target, \a levels, \a internalformat, \a width, \a height and \a depth.
*/

/*!
    \fn void QOpenGLExtraFunctions::glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint *params)

    Calls the OpenGL ES 3.0 function \c glGetInternalformativ with \a target, \a internalformat, \a pname, \a bufSize and \a params.
*/
