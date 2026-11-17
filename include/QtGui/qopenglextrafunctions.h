// MIT License - see LICENSE
#pragma once

#include <QtGui/qopenglfunctions.h>

// The OpenGL ES 3.0 functions of one context beyond those of OpenGL ES 2.0, resolved when they are first
// asked for. A function the context lacks is null.
struct QOpenGLExtraFunctionsPrivate {
    explicit QOpenGLExtraFunctionsPrivate(QOpenGLContext *context);

    struct Functions {
        void(QOPENGLF_APIENTRYP ReadBuffer)(GLenum src);
        void(QOPENGLF_APIENTRYP DrawRangeElements)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type,
                                                   const void *indices);
        void(QOPENGLF_APIENTRYP TexImage3D)(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                            GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type,
                                            const void *pixels);
        void(QOPENGLF_APIENTRYP TexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                               GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                               const void *pixels);
        void(QOPENGLF_APIENTRYP CopyTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                                   GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
        void(QOPENGLF_APIENTRYP CompressedTexImage3D)(GLenum target, GLint level, GLenum internalformat, GLsizei width,
                                                      GLsizei height, GLsizei depth, GLint border, GLsizei imageSize,
                                                      const void *data);
        void(QOPENGLF_APIENTRYP CompressedTexSubImage3D)(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                                         GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                                         GLenum format, GLsizei imageSize, const void *data);
        void(QOPENGLF_APIENTRYP GenQueries)(GLsizei n, GLuint *ids);
        void(QOPENGLF_APIENTRYP DeleteQueries)(GLsizei n, const GLuint *ids);
        GLboolean(QOPENGLF_APIENTRYP IsQuery)(GLuint id);
        void(QOPENGLF_APIENTRYP BeginQuery)(GLenum target, GLuint id);
        void(QOPENGLF_APIENTRYP EndQuery)(GLenum target);
        void(QOPENGLF_APIENTRYP GetQueryiv)(GLenum target, GLenum pname, GLint *params);
        void(QOPENGLF_APIENTRYP GetQueryObjectuiv)(GLuint id, GLenum pname, GLuint *params);
        GLboolean(QOPENGLF_APIENTRYP UnmapBuffer)(GLenum target);
        void(QOPENGLF_APIENTRYP GetBufferPointerv)(GLenum target, GLenum pname, void **params);
        void(QOPENGLF_APIENTRYP DrawBuffers)(GLsizei n, const GLenum *bufs);
        void(QOPENGLF_APIENTRYP UniformMatrix2x3fv)(GLint location, GLsizei count, GLboolean transpose,
                                                    const GLfloat *value);
        void(QOPENGLF_APIENTRYP UniformMatrix3x2fv)(GLint location, GLsizei count, GLboolean transpose,
                                                    const GLfloat *value);
        void(QOPENGLF_APIENTRYP UniformMatrix2x4fv)(GLint location, GLsizei count, GLboolean transpose,
                                                    const GLfloat *value);
        void(QOPENGLF_APIENTRYP UniformMatrix4x2fv)(GLint location, GLsizei count, GLboolean transpose,
                                                    const GLfloat *value);
        void(QOPENGLF_APIENTRYP UniformMatrix3x4fv)(GLint location, GLsizei count, GLboolean transpose,
                                                    const GLfloat *value);
        void(QOPENGLF_APIENTRYP UniformMatrix4x3fv)(GLint location, GLsizei count, GLboolean transpose,
                                                    const GLfloat *value);
        void(QOPENGLF_APIENTRYP BlitFramebuffer)(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
                                                 GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
        void(QOPENGLF_APIENTRYP RenderbufferStorageMultisample)(GLenum target, GLsizei samples, GLenum internalformat,
                                                                GLsizei width, GLsizei height);
        void(QOPENGLF_APIENTRYP FramebufferTextureLayer)(GLenum target, GLenum attachment, GLuint texture, GLint level,
                                                         GLint layer);
        void *(QOPENGLF_APIENTRYP MapBufferRange)(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
        void(QOPENGLF_APIENTRYP FlushMappedBufferRange)(GLenum target, GLintptr offset, GLsizeiptr length);
        void(QOPENGLF_APIENTRYP BindVertexArray)(GLuint array);
        void(QOPENGLF_APIENTRYP DeleteVertexArrays)(GLsizei n, const GLuint *arrays);
        void(QOPENGLF_APIENTRYP GenVertexArrays)(GLsizei n, GLuint *arrays);
        GLboolean(QOPENGLF_APIENTRYP IsVertexArray)(GLuint array);
        void(QOPENGLF_APIENTRYP GetIntegeri_v)(GLenum target, GLuint index, GLint *data);
        void(QOPENGLF_APIENTRYP BeginTransformFeedback)(GLenum primitiveMode);
        void(QOPENGLF_APIENTRYP EndTransformFeedback)();
        void(QOPENGLF_APIENTRYP BindBufferRange)(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                                                 GLsizeiptr size);
        void(QOPENGLF_APIENTRYP BindBufferBase)(GLenum target, GLuint index, GLuint buffer);
        void(QOPENGLF_APIENTRYP TransformFeedbackVaryings)(GLuint program, GLsizei count, const GLchar *const *varyings,
                                                           GLenum bufferMode);
        void(QOPENGLF_APIENTRYP GetTransformFeedbackVarying)(GLuint program, GLuint index, GLsizei bufSize,
                                                             GLsizei *length, GLsizei *size, GLenum *type,
                                                             GLchar *name);
        void(QOPENGLF_APIENTRYP VertexAttribIPointer)(GLuint index, GLint size, GLenum type, GLsizei stride,
                                                      const void *pointer);
        void(QOPENGLF_APIENTRYP GetVertexAttribIiv)(GLuint index, GLenum pname, GLint *params);
        void(QOPENGLF_APIENTRYP GetVertexAttribIuiv)(GLuint index, GLenum pname, GLuint *params);
        void(QOPENGLF_APIENTRYP VertexAttribI4i)(GLuint index, GLint x, GLint y, GLint z, GLint w);
        void(QOPENGLF_APIENTRYP VertexAttribI4ui)(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
        void(QOPENGLF_APIENTRYP VertexAttribI4iv)(GLuint index, const GLint *v);
        void(QOPENGLF_APIENTRYP VertexAttribI4uiv)(GLuint index, const GLuint *v);
        void(QOPENGLF_APIENTRYP GetUniformuiv)(GLuint program, GLint location, GLuint *params);
        GLint(QOPENGLF_APIENTRYP GetFragDataLocation)(GLuint program, const GLchar *name);
        void(QOPENGLF_APIENTRYP Uniform1ui)(GLint location, GLuint v0);
        void(QOPENGLF_APIENTRYP Uniform2ui)(GLint location, GLuint v0, GLuint v1);
        void(QOPENGLF_APIENTRYP Uniform3ui)(GLint location, GLuint v0, GLuint v1, GLuint v2);
        void(QOPENGLF_APIENTRYP Uniform4ui)(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
        void(QOPENGLF_APIENTRYP Uniform1uiv)(GLint location, GLsizei count, const GLuint *value);
        void(QOPENGLF_APIENTRYP Uniform2uiv)(GLint location, GLsizei count, const GLuint *value);
        void(QOPENGLF_APIENTRYP Uniform3uiv)(GLint location, GLsizei count, const GLuint *value);
        void(QOPENGLF_APIENTRYP Uniform4uiv)(GLint location, GLsizei count, const GLuint *value);
        void(QOPENGLF_APIENTRYP ClearBufferiv)(GLenum buffer, GLint drawbuffer, const GLint *value);
        void(QOPENGLF_APIENTRYP ClearBufferuiv)(GLenum buffer, GLint drawbuffer, const GLuint *value);
        void(QOPENGLF_APIENTRYP ClearBufferfv)(GLenum buffer, GLint drawbuffer, const GLfloat *value);
        void(QOPENGLF_APIENTRYP ClearBufferfi)(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
        const GLubyte *(QOPENGLF_APIENTRYP GetStringi)(GLenum name, GLuint index);
        void(QOPENGLF_APIENTRYP CopyBufferSubData)(GLenum readTarget, GLenum writeTarget, GLintptr readOffset,
                                                   GLintptr writeOffset, GLsizeiptr size);
        void(QOPENGLF_APIENTRYP GetUniformIndices)(GLuint program, GLsizei uniformCount,
                                                   const GLchar *const *uniformNames, GLuint *uniformIndices);
        void(QOPENGLF_APIENTRYP GetActiveUniformsiv)(GLuint program, GLsizei uniformCount, const GLuint *uniformIndices,
                                                     GLenum pname, GLint *params);
        GLuint(QOPENGLF_APIENTRYP GetUniformBlockIndex)(GLuint program, const GLchar *uniformBlockName);
        void(QOPENGLF_APIENTRYP GetActiveUniformBlockiv)(GLuint program, GLuint uniformBlockIndex, GLenum pname,
                                                         GLint *params);
        void(QOPENGLF_APIENTRYP GetActiveUniformBlockName)(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize,
                                                           GLsizei *length, GLchar *uniformBlockName);
        void(QOPENGLF_APIENTRYP UniformBlockBinding)(GLuint program, GLuint uniformBlockIndex,
                                                     GLuint uniformBlockBinding);
        void(QOPENGLF_APIENTRYP DrawArraysInstanced)(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
        void(QOPENGLF_APIENTRYP DrawElementsInstanced)(GLenum mode, GLsizei count, GLenum type, const void *indices,
                                                       GLsizei instancecount);
        GLsync(QOPENGLF_APIENTRYP FenceSync)(GLenum condition, GLbitfield flags);
        GLboolean(QOPENGLF_APIENTRYP IsSync)(GLsync sync);
        void(QOPENGLF_APIENTRYP DeleteSync)(GLsync sync);
        GLenum(QOPENGLF_APIENTRYP ClientWaitSync)(GLsync sync, GLbitfield flags, GLuint64 timeout);
        void(QOPENGLF_APIENTRYP WaitSync)(GLsync sync, GLbitfield flags, GLuint64 timeout);
        void(QOPENGLF_APIENTRYP GetInteger64v)(GLenum pname, GLint64 *data);
        void(QOPENGLF_APIENTRYP GetSynciv)(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values);
        void(QOPENGLF_APIENTRYP GetInteger64i_v)(GLenum target, GLuint index, GLint64 *data);
        void(QOPENGLF_APIENTRYP GetBufferParameteri64v)(GLenum target, GLenum pname, GLint64 *params);
        void(QOPENGLF_APIENTRYP GenSamplers)(GLsizei count, GLuint *samplers);
        void(QOPENGLF_APIENTRYP DeleteSamplers)(GLsizei count, const GLuint *samplers);
        GLboolean(QOPENGLF_APIENTRYP IsSampler)(GLuint sampler);
        void(QOPENGLF_APIENTRYP BindSampler)(GLuint unit, GLuint sampler);
        void(QOPENGLF_APIENTRYP SamplerParameteri)(GLuint sampler, GLenum pname, GLint param);
        void(QOPENGLF_APIENTRYP SamplerParameteriv)(GLuint sampler, GLenum pname, const GLint *param);
        void(QOPENGLF_APIENTRYP SamplerParameterf)(GLuint sampler, GLenum pname, GLfloat param);
        void(QOPENGLF_APIENTRYP SamplerParameterfv)(GLuint sampler, GLenum pname, const GLfloat *param);
        void(QOPENGLF_APIENTRYP GetSamplerParameteriv)(GLuint sampler, GLenum pname, GLint *params);
        void(QOPENGLF_APIENTRYP GetSamplerParameterfv)(GLuint sampler, GLenum pname, GLfloat *params);
        void(QOPENGLF_APIENTRYP VertexAttribDivisor)(GLuint index, GLuint divisor);
        void(QOPENGLF_APIENTRYP BindTransformFeedback)(GLenum target, GLuint id);
        void(QOPENGLF_APIENTRYP DeleteTransformFeedbacks)(GLsizei n, const GLuint *ids);
        void(QOPENGLF_APIENTRYP GenTransformFeedbacks)(GLsizei n, GLuint *ids);
        GLboolean(QOPENGLF_APIENTRYP IsTransformFeedback)(GLuint id);
        void(QOPENGLF_APIENTRYP PauseTransformFeedback)();
        void(QOPENGLF_APIENTRYP ResumeTransformFeedback)();
        void(QOPENGLF_APIENTRYP GetProgramBinary)(GLuint program, GLsizei bufSize, GLsizei *length,
                                                  GLenum *binaryFormat, void *binary);
        void(QOPENGLF_APIENTRYP ProgramBinary)(GLuint program, GLenum binaryFormat, const void *binary, GLsizei length);
        void(QOPENGLF_APIENTRYP ProgramParameteri)(GLuint program, GLenum pname, GLint value);
        void(QOPENGLF_APIENTRYP InvalidateFramebuffer)(GLenum target, GLsizei numAttachments,
                                                       const GLenum *attachments);
        void(QOPENGLF_APIENTRYP InvalidateSubFramebuffer)(GLenum target, GLsizei numAttachments,
                                                          const GLenum *attachments, GLint x, GLint y, GLsizei width,
                                                          GLsizei height);
        void(QOPENGLF_APIENTRYP TexStorage2D)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                                              GLsizei height);
        void(QOPENGLF_APIENTRYP TexStorage3D)(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                                              GLsizei height, GLsizei depth);
        void(QOPENGLF_APIENTRYP GetInternalformativ)(GLenum target, GLenum internalformat, GLenum pname,
                                                     GLsizei bufSize, GLint *params);
    } f;
};

class QOpenGLExtraFunctions : public QOpenGLFunctions {
public:
    QOpenGLExtraFunctions();
    explicit QOpenGLExtraFunctions(QOpenGLContext *context);
    ~QOpenGLExtraFunctions() = default;

    void initializeOpenGLFunctions();

    void glReadBuffer(GLenum src);
    void glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices);
    void glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth,
                      GLint border, GLenum format, GLenum type, const void *pixels);
    void glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,
                         GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
    void glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y,
                             GLsizei width, GLsizei height);
    void glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height,
                                GLsizei depth, GLint border, GLsizei imageSize, const void *data);
    void glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                   GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize,
                                   const void *data);
    void glGenQueries(GLsizei n, GLuint *ids);
    void glDeleteQueries(GLsizei n, const GLuint *ids);
    GLboolean glIsQuery(GLuint id);
    void glBeginQuery(GLenum target, GLuint id);
    void glEndQuery(GLenum target);
    void glGetQueryiv(GLenum target, GLenum pname, GLint *params);
    void glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params);
    GLboolean glUnmapBuffer(GLenum target);
    void glGetBufferPointerv(GLenum target, GLenum pname, void **params);
    void glDrawBuffers(GLsizei n, const GLenum *bufs);
    void glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    void glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1,
                           GLint dstY1, GLbitfield mask, GLenum filter);
    void glRenderbufferStorageMultisample(GLenum target, GLsizei samples, GLenum internalformat, GLsizei width,
                                          GLsizei height);
    void glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer);
    void *glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
    void glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length);
    void glBindVertexArray(GLuint array);
    void glDeleteVertexArrays(GLsizei n, const GLuint *arrays);
    void glGenVertexArrays(GLsizei n, GLuint *arrays);
    GLboolean glIsVertexArray(GLuint array);
    void glGetIntegeri_v(GLenum target, GLuint index, GLint *data);
    void glBeginTransformFeedback(GLenum primitiveMode);
    void glEndTransformFeedback();
    void glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
    void glBindBufferBase(GLenum target, GLuint index, GLuint buffer);
    void glTransformFeedbackVaryings(GLuint program, GLsizei count, const GLchar *const *varyings, GLenum bufferMode);
    void glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLsizei *size,
                                       GLenum *type, GLchar *name);
    void glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);
    void glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params);
    void glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params);
    void glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w);
    void glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w);
    void glVertexAttribI4iv(GLuint index, const GLint *v);
    void glVertexAttribI4uiv(GLuint index, const GLuint *v);
    void glGetUniformuiv(GLuint program, GLint location, GLuint *params);
    GLint glGetFragDataLocation(GLuint program, const GLchar *name);
    void glUniform1ui(GLint location, GLuint v0);
    void glUniform2ui(GLint location, GLuint v0, GLuint v1);
    void glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2);
    void glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
    void glUniform1uiv(GLint location, GLsizei count, const GLuint *value);
    void glUniform2uiv(GLint location, GLsizei count, const GLuint *value);
    void glUniform3uiv(GLint location, GLsizei count, const GLuint *value);
    void glUniform4uiv(GLint location, GLsizei count, const GLuint *value);
    void glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value);
    void glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value);
    void glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value);
    void glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil);
    const GLubyte *glGetStringi(GLenum name, GLuint index);
    void glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset,
                             GLsizeiptr size);
    void glGetUniformIndices(GLuint program, GLsizei uniformCount, const GLchar *const *uniformNames,
                             GLuint *uniformIndices);
    void glGetActiveUniformsiv(GLuint program, GLsizei uniformCount, const GLuint *uniformIndices, GLenum pname,
                               GLint *params);
    GLuint glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName);
    void glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint *params);
    void glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei *length,
                                     GLchar *uniformBlockName);
    void glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
    void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instancecount);
    void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount);
    GLsync glFenceSync(GLenum condition, GLbitfield flags);
    GLboolean glIsSync(GLsync sync);
    void glDeleteSync(GLsync sync);
    GLenum glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout);
    void glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout);
    void glGetInteger64v(GLenum pname, GLint64 *data);
    void glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values);
    void glGetInteger64i_v(GLenum target, GLuint index, GLint64 *data);
    void glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params);
    void glGenSamplers(GLsizei count, GLuint *samplers);
    void glDeleteSamplers(GLsizei count, const GLuint *samplers);
    GLboolean glIsSampler(GLuint sampler);
    void glBindSampler(GLuint unit, GLuint sampler);
    void glSamplerParameteri(GLuint sampler, GLenum pname, GLint param);
    void glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *param);
    void glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param);
    void glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param);
    void glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params);
    void glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params);
    void glVertexAttribDivisor(GLuint index, GLuint divisor);
    void glBindTransformFeedback(GLenum target, GLuint id);
    void glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids);
    void glGenTransformFeedbacks(GLsizei n, GLuint *ids);
    GLboolean glIsTransformFeedback(GLuint id);
    void glPauseTransformFeedback();
    void glResumeTransformFeedback();
    void glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary);
    void glProgramBinary(GLuint program, GLenum binaryFormat, const void *binary, GLsizei length);
    void glProgramParameteri(GLuint program, GLenum pname, GLint value);
    void glInvalidateFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments);
    void glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments, GLint x, GLint y,
                                    GLsizei width, GLsizei height);
    void glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
    void glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height,
                        GLsizei depth);
    void glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname, GLsizei bufSize, GLint *params);

protected:
    QOpenGLExtraFunctionsPrivate *e_ptr = nullptr;
};

inline void QOpenGLExtraFunctions::glReadBuffer(GLenum src) { e_ptr->f.ReadBuffer(src); }
inline void QOpenGLExtraFunctions::glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
                                                       GLenum type, const void *indices) {
    e_ptr->f.DrawRangeElements(mode, start, end, count, type, indices);
}
inline void QOpenGLExtraFunctions::glTexImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                                GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type,
                                                const void *pixels) {
    e_ptr->f.TexImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);
}
inline void QOpenGLExtraFunctions::glTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                                   GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
                                                   GLenum format, GLenum type, const void *pixels) {
    e_ptr->f.TexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}
inline void QOpenGLExtraFunctions::glCopyTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                                       GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height) {
    e_ptr->f.CopyTexSubImage3D(target, level, xoffset, yoffset, zoffset, x, y, width, height);
}
inline void QOpenGLExtraFunctions::glCompressedTexImage3D(GLenum target, GLint level, GLenum internalformat,
                                                          GLsizei width, GLsizei height, GLsizei depth, GLint border,
                                                          GLsizei imageSize, const void *data) {
    e_ptr->f.CompressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize, data);
}
inline void QOpenGLExtraFunctions::glCompressedTexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
                                                             GLint zoffset, GLsizei width, GLsizei height,
                                                             GLsizei depth, GLenum format, GLsizei imageSize,
                                                             const void *data) {
    e_ptr->f.CompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, imageSize,
                                     data);
}
inline void QOpenGLExtraFunctions::glGenQueries(GLsizei n, GLuint *ids) { e_ptr->f.GenQueries(n, ids); }
inline void QOpenGLExtraFunctions::glDeleteQueries(GLsizei n, const GLuint *ids) { e_ptr->f.DeleteQueries(n, ids); }
inline GLboolean QOpenGLExtraFunctions::glIsQuery(GLuint id) { return e_ptr->f.IsQuery(id); }
inline void QOpenGLExtraFunctions::glBeginQuery(GLenum target, GLuint id) { e_ptr->f.BeginQuery(target, id); }
inline void QOpenGLExtraFunctions::glEndQuery(GLenum target) { e_ptr->f.EndQuery(target); }
inline void QOpenGLExtraFunctions::glGetQueryiv(GLenum target, GLenum pname, GLint *params) {
    e_ptr->f.GetQueryiv(target, pname, params);
}
inline void QOpenGLExtraFunctions::glGetQueryObjectuiv(GLuint id, GLenum pname, GLuint *params) {
    e_ptr->f.GetQueryObjectuiv(id, pname, params);
}
inline GLboolean QOpenGLExtraFunctions::glUnmapBuffer(GLenum target) { return e_ptr->f.UnmapBuffer(target); }
inline void QOpenGLExtraFunctions::glGetBufferPointerv(GLenum target, GLenum pname, void **params) {
    e_ptr->f.GetBufferPointerv(target, pname, params);
}
inline void QOpenGLExtraFunctions::glDrawBuffers(GLsizei n, const GLenum *bufs) { e_ptr->f.DrawBuffers(n, bufs); }
inline void QOpenGLExtraFunctions::glUniformMatrix2x3fv(GLint location, GLsizei count, GLboolean transpose,
                                                        const GLfloat *value) {
    e_ptr->f.UniformMatrix2x3fv(location, count, transpose, value);
}
inline void QOpenGLExtraFunctions::glUniformMatrix3x2fv(GLint location, GLsizei count, GLboolean transpose,
                                                        const GLfloat *value) {
    e_ptr->f.UniformMatrix3x2fv(location, count, transpose, value);
}
inline void QOpenGLExtraFunctions::glUniformMatrix2x4fv(GLint location, GLsizei count, GLboolean transpose,
                                                        const GLfloat *value) {
    e_ptr->f.UniformMatrix2x4fv(location, count, transpose, value);
}
inline void QOpenGLExtraFunctions::glUniformMatrix4x2fv(GLint location, GLsizei count, GLboolean transpose,
                                                        const GLfloat *value) {
    e_ptr->f.UniformMatrix4x2fv(location, count, transpose, value);
}
inline void QOpenGLExtraFunctions::glUniformMatrix3x4fv(GLint location, GLsizei count, GLboolean transpose,
                                                        const GLfloat *value) {
    e_ptr->f.UniformMatrix3x4fv(location, count, transpose, value);
}
inline void QOpenGLExtraFunctions::glUniformMatrix4x3fv(GLint location, GLsizei count, GLboolean transpose,
                                                        const GLfloat *value) {
    e_ptr->f.UniformMatrix4x3fv(location, count, transpose, value);
}
inline void QOpenGLExtraFunctions::glBlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0,
                                                     GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask,
                                                     GLenum filter) {
    e_ptr->f.BlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}
inline void QOpenGLExtraFunctions::glRenderbufferStorageMultisample(GLenum target, GLsizei samples,
                                                                    GLenum internalformat, GLsizei width,
                                                                    GLsizei height) {
    e_ptr->f.RenderbufferStorageMultisample(target, samples, internalformat, width, height);
}
inline void QOpenGLExtraFunctions::glFramebufferTextureLayer(GLenum target, GLenum attachment, GLuint texture,
                                                             GLint level, GLint layer) {
    e_ptr->f.FramebufferTextureLayer(target, attachment, texture, level, layer);
}
inline void *QOpenGLExtraFunctions::glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
                                                     GLbitfield access) {
    return e_ptr->f.MapBufferRange(target, offset, length, access);
}
inline void QOpenGLExtraFunctions::glFlushMappedBufferRange(GLenum target, GLintptr offset, GLsizeiptr length) {
    e_ptr->f.FlushMappedBufferRange(target, offset, length);
}
inline void QOpenGLExtraFunctions::glBindVertexArray(GLuint array) { e_ptr->f.BindVertexArray(array); }
inline void QOpenGLExtraFunctions::glDeleteVertexArrays(GLsizei n, const GLuint *arrays) {
    e_ptr->f.DeleteVertexArrays(n, arrays);
}
inline void QOpenGLExtraFunctions::glGenVertexArrays(GLsizei n, GLuint *arrays) { e_ptr->f.GenVertexArrays(n, arrays); }
inline GLboolean QOpenGLExtraFunctions::glIsVertexArray(GLuint array) { return e_ptr->f.IsVertexArray(array); }
inline void QOpenGLExtraFunctions::glGetIntegeri_v(GLenum target, GLuint index, GLint *data) {
    e_ptr->f.GetIntegeri_v(target, index, data);
}
inline void QOpenGLExtraFunctions::glBeginTransformFeedback(GLenum primitiveMode) {
    e_ptr->f.BeginTransformFeedback(primitiveMode);
}
inline void QOpenGLExtraFunctions::glEndTransformFeedback() { e_ptr->f.EndTransformFeedback(); }
inline void QOpenGLExtraFunctions::glBindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset,
                                                     GLsizeiptr size) {
    e_ptr->f.BindBufferRange(target, index, buffer, offset, size);
}
inline void QOpenGLExtraFunctions::glBindBufferBase(GLenum target, GLuint index, GLuint buffer) {
    e_ptr->f.BindBufferBase(target, index, buffer);
}
inline void QOpenGLExtraFunctions::glTransformFeedbackVaryings(GLuint program, GLsizei count,
                                                               const GLchar *const *varyings, GLenum bufferMode) {
    e_ptr->f.TransformFeedbackVaryings(program, count, varyings, bufferMode);
}
inline void QOpenGLExtraFunctions::glGetTransformFeedbackVarying(GLuint program, GLuint index, GLsizei bufSize,
                                                                 GLsizei *length, GLsizei *size, GLenum *type,
                                                                 GLchar *name) {
    e_ptr->f.GetTransformFeedbackVarying(program, index, bufSize, length, size, type, name);
}
inline void QOpenGLExtraFunctions::glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride,
                                                          const void *pointer) {
    e_ptr->f.VertexAttribIPointer(index, size, type, stride, pointer);
}
inline void QOpenGLExtraFunctions::glGetVertexAttribIiv(GLuint index, GLenum pname, GLint *params) {
    e_ptr->f.GetVertexAttribIiv(index, pname, params);
}
inline void QOpenGLExtraFunctions::glGetVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params) {
    e_ptr->f.GetVertexAttribIuiv(index, pname, params);
}
inline void QOpenGLExtraFunctions::glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) {
    e_ptr->f.VertexAttribI4i(index, x, y, z, w);
}
inline void QOpenGLExtraFunctions::glVertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w) {
    e_ptr->f.VertexAttribI4ui(index, x, y, z, w);
}
inline void QOpenGLExtraFunctions::glVertexAttribI4iv(GLuint index, const GLint *v) {
    e_ptr->f.VertexAttribI4iv(index, v);
}
inline void QOpenGLExtraFunctions::glVertexAttribI4uiv(GLuint index, const GLuint *v) {
    e_ptr->f.VertexAttribI4uiv(index, v);
}
inline void QOpenGLExtraFunctions::glGetUniformuiv(GLuint program, GLint location, GLuint *params) {
    e_ptr->f.GetUniformuiv(program, location, params);
}
inline GLint QOpenGLExtraFunctions::glGetFragDataLocation(GLuint program, const GLchar *name) {
    return e_ptr->f.GetFragDataLocation(program, name);
}
inline void QOpenGLExtraFunctions::glUniform1ui(GLint location, GLuint v0) { e_ptr->f.Uniform1ui(location, v0); }
inline void QOpenGLExtraFunctions::glUniform2ui(GLint location, GLuint v0, GLuint v1) {
    e_ptr->f.Uniform2ui(location, v0, v1);
}
inline void QOpenGLExtraFunctions::glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2) {
    e_ptr->f.Uniform3ui(location, v0, v1, v2);
}
inline void QOpenGLExtraFunctions::glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) {
    e_ptr->f.Uniform4ui(location, v0, v1, v2, v3);
}
inline void QOpenGLExtraFunctions::glUniform1uiv(GLint location, GLsizei count, const GLuint *value) {
    e_ptr->f.Uniform1uiv(location, count, value);
}
inline void QOpenGLExtraFunctions::glUniform2uiv(GLint location, GLsizei count, const GLuint *value) {
    e_ptr->f.Uniform2uiv(location, count, value);
}
inline void QOpenGLExtraFunctions::glUniform3uiv(GLint location, GLsizei count, const GLuint *value) {
    e_ptr->f.Uniform3uiv(location, count, value);
}
inline void QOpenGLExtraFunctions::glUniform4uiv(GLint location, GLsizei count, const GLuint *value) {
    e_ptr->f.Uniform4uiv(location, count, value);
}
inline void QOpenGLExtraFunctions::glClearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *value) {
    e_ptr->f.ClearBufferiv(buffer, drawbuffer, value);
}
inline void QOpenGLExtraFunctions::glClearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *value) {
    e_ptr->f.ClearBufferuiv(buffer, drawbuffer, value);
}
inline void QOpenGLExtraFunctions::glClearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *value) {
    e_ptr->f.ClearBufferfv(buffer, drawbuffer, value);
}
inline void QOpenGLExtraFunctions::glClearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) {
    e_ptr->f.ClearBufferfi(buffer, drawbuffer, depth, stencil);
}
inline const GLubyte *QOpenGLExtraFunctions::glGetStringi(GLenum name, GLuint index) {
    return e_ptr->f.GetStringi(name, index);
}
inline void QOpenGLExtraFunctions::glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset,
                                                       GLintptr writeOffset, GLsizeiptr size) {
    e_ptr->f.CopyBufferSubData(readTarget, writeTarget, readOffset, writeOffset, size);
}
inline void QOpenGLExtraFunctions::glGetUniformIndices(GLuint program, GLsizei uniformCount,
                                                       const GLchar *const *uniformNames, GLuint *uniformIndices) {
    e_ptr->f.GetUniformIndices(program, uniformCount, uniformNames, uniformIndices);
}
inline void QOpenGLExtraFunctions::glGetActiveUniformsiv(GLuint program, GLsizei uniformCount,
                                                         const GLuint *uniformIndices, GLenum pname, GLint *params) {
    e_ptr->f.GetActiveUniformsiv(program, uniformCount, uniformIndices, pname, params);
}
inline GLuint QOpenGLExtraFunctions::glGetUniformBlockIndex(GLuint program, const GLchar *uniformBlockName) {
    return e_ptr->f.GetUniformBlockIndex(program, uniformBlockName);
}
inline void QOpenGLExtraFunctions::glGetActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname,
                                                             GLint *params) {
    e_ptr->f.GetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}
inline void QOpenGLExtraFunctions::glGetActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex,
                                                               GLsizei bufSize, GLsizei *length,
                                                               GLchar *uniformBlockName) {
    e_ptr->f.GetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
}
inline void QOpenGLExtraFunctions::glUniformBlockBinding(GLuint program, GLuint uniformBlockIndex,
                                                         GLuint uniformBlockBinding) {
    e_ptr->f.UniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}
inline void QOpenGLExtraFunctions::glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
                                                         GLsizei instancecount) {
    e_ptr->f.DrawArraysInstanced(mode, first, count, instancecount);
}
inline void QOpenGLExtraFunctions::glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type, const void *indices,
                                                           GLsizei instancecount) {
    e_ptr->f.DrawElementsInstanced(mode, count, type, indices, instancecount);
}
inline GLsync QOpenGLExtraFunctions::glFenceSync(GLenum condition, GLbitfield flags) {
    return e_ptr->f.FenceSync(condition, flags);
}
inline GLboolean QOpenGLExtraFunctions::glIsSync(GLsync sync) { return e_ptr->f.IsSync(sync); }
inline void QOpenGLExtraFunctions::glDeleteSync(GLsync sync) { e_ptr->f.DeleteSync(sync); }
inline GLenum QOpenGLExtraFunctions::glClientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    return e_ptr->f.ClientWaitSync(sync, flags, timeout);
}
inline void QOpenGLExtraFunctions::glWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout) {
    e_ptr->f.WaitSync(sync, flags, timeout);
}
inline void QOpenGLExtraFunctions::glGetInteger64v(GLenum pname, GLint64 *data) { e_ptr->f.GetInteger64v(pname, data); }
inline void QOpenGLExtraFunctions::glGetSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length,
                                               GLint *values) {
    e_ptr->f.GetSynciv(sync, pname, bufSize, length, values);
}
inline void QOpenGLExtraFunctions::glGetInteger64i_v(GLenum target, GLuint index, GLint64 *data) {
    e_ptr->f.GetInteger64i_v(target, index, data);
}
inline void QOpenGLExtraFunctions::glGetBufferParameteri64v(GLenum target, GLenum pname, GLint64 *params) {
    e_ptr->f.GetBufferParameteri64v(target, pname, params);
}
inline void QOpenGLExtraFunctions::glGenSamplers(GLsizei count, GLuint *samplers) {
    e_ptr->f.GenSamplers(count, samplers);
}
inline void QOpenGLExtraFunctions::glDeleteSamplers(GLsizei count, const GLuint *samplers) {
    e_ptr->f.DeleteSamplers(count, samplers);
}
inline GLboolean QOpenGLExtraFunctions::glIsSampler(GLuint sampler) { return e_ptr->f.IsSampler(sampler); }
inline void QOpenGLExtraFunctions::glBindSampler(GLuint unit, GLuint sampler) { e_ptr->f.BindSampler(unit, sampler); }
inline void QOpenGLExtraFunctions::glSamplerParameteri(GLuint sampler, GLenum pname, GLint param) {
    e_ptr->f.SamplerParameteri(sampler, pname, param);
}
inline void QOpenGLExtraFunctions::glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *param) {
    e_ptr->f.SamplerParameteriv(sampler, pname, param);
}
inline void QOpenGLExtraFunctions::glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param) {
    e_ptr->f.SamplerParameterf(sampler, pname, param);
}
inline void QOpenGLExtraFunctions::glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param) {
    e_ptr->f.SamplerParameterfv(sampler, pname, param);
}
inline void QOpenGLExtraFunctions::glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params) {
    e_ptr->f.GetSamplerParameteriv(sampler, pname, params);
}
inline void QOpenGLExtraFunctions::glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params) {
    e_ptr->f.GetSamplerParameterfv(sampler, pname, params);
}
inline void QOpenGLExtraFunctions::glVertexAttribDivisor(GLuint index, GLuint divisor) {
    e_ptr->f.VertexAttribDivisor(index, divisor);
}
inline void QOpenGLExtraFunctions::glBindTransformFeedback(GLenum target, GLuint id) {
    e_ptr->f.BindTransformFeedback(target, id);
}
inline void QOpenGLExtraFunctions::glDeleteTransformFeedbacks(GLsizei n, const GLuint *ids) {
    e_ptr->f.DeleteTransformFeedbacks(n, ids);
}
inline void QOpenGLExtraFunctions::glGenTransformFeedbacks(GLsizei n, GLuint *ids) {
    e_ptr->f.GenTransformFeedbacks(n, ids);
}
inline GLboolean QOpenGLExtraFunctions::glIsTransformFeedback(GLuint id) { return e_ptr->f.IsTransformFeedback(id); }
inline void QOpenGLExtraFunctions::glPauseTransformFeedback() { e_ptr->f.PauseTransformFeedback(); }
inline void QOpenGLExtraFunctions::glResumeTransformFeedback() { e_ptr->f.ResumeTransformFeedback(); }
inline void QOpenGLExtraFunctions::glGetProgramBinary(GLuint program, GLsizei bufSize, GLsizei *length,
                                                      GLenum *binaryFormat, void *binary) {
    e_ptr->f.GetProgramBinary(program, bufSize, length, binaryFormat, binary);
}
inline void QOpenGLExtraFunctions::glProgramBinary(GLuint program, GLenum binaryFormat, const void *binary,
                                                   GLsizei length) {
    e_ptr->f.ProgramBinary(program, binaryFormat, binary, length);
}
inline void QOpenGLExtraFunctions::glProgramParameteri(GLuint program, GLenum pname, GLint value) {
    e_ptr->f.ProgramParameteri(program, pname, value);
}
inline void QOpenGLExtraFunctions::glInvalidateFramebuffer(GLenum target, GLsizei numAttachments,
                                                           const GLenum *attachments) {
    e_ptr->f.InvalidateFramebuffer(target, numAttachments, attachments);
}
inline void QOpenGLExtraFunctions::glInvalidateSubFramebuffer(GLenum target, GLsizei numAttachments,
                                                              const GLenum *attachments, GLint x, GLint y,
                                                              GLsizei width, GLsizei height) {
    e_ptr->f.InvalidateSubFramebuffer(target, numAttachments, attachments, x, y, width, height);
}
inline void QOpenGLExtraFunctions::glTexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                                                  GLsizei height) {
    e_ptr->f.TexStorage2D(target, levels, internalformat, width, height);
}
inline void QOpenGLExtraFunctions::glTexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width,
                                                  GLsizei height, GLsizei depth) {
    e_ptr->f.TexStorage3D(target, levels, internalformat, width, height, depth);
}
inline void QOpenGLExtraFunctions::glGetInternalformativ(GLenum target, GLenum internalformat, GLenum pname,
                                                         GLsizei bufSize, GLint *params) {
    e_ptr->f.GetInternalformativ(target, internalformat, pname, bufSize, params);
}
