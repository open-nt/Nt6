// MIT License - see LICENSE
#pragma once

#include <QtCore/qsize.h>
#include <QtGui/qimage.h>
#include <QtGui/qopengl.h>

#include <functional>

class QOpenGLContext;

namespace QtOpenGLPrivate {

using BlitFramebuffer = void(QOPENGLF_APIENTRYP)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield,
                                                 GLenum);

// glBlitFramebuffer() of desktop OpenGL 3.0 and OpenGL ES 3.0, or null.
BlitFramebuffer blitFunction(QOpenGLContext *context);
// Whether renderbuffers can be multisampled.
bool hasMultisampleRenderbuffers(QOpenGLContext *context);
// Allocates the storage of `renderbuffer`, multisampled if `samples` is above 0 and the context can.
void renderbufferStorage(QOpenGLContext *context, GLuint renderbuffer, int samples, GLenum internalFormat,
                         const QSize &size);
// Reads the colors of `fbo` (0 for the default framebuffer) into a premultiplied image, resolving a multisampled
// framebuffer first. The binding of the framebuffer is kept.
QImage readFramebuffer(QOpenGLContext *context, GLuint fbo, const QSize &size, bool multisampled, bool flipped);
// Runs `f` with a context that can use the objects of `owner` current in this thread: the current context if it is
// `owner` (or, with `shared`, shares with it), otherwise `owner`, made current on an offscreen surface for the time.
// Returns false, without running `f`, if neither is possible.
bool runInContext(QOpenGLContext *owner, bool shared, const std::function<void(QOpenGLContext *)> &f);

} // namespace QtOpenGLPrivate
