// MIT License - see LICENSE
#include "qopenglhelpers_p.h"

#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>

#include <cstring>
#include <vector>

namespace QtOpenGLPrivate {

namespace {

bool atLeastThree(QOpenGLContext *context) { return context->format().majorVersion() >= 3; }

} // namespace

BlitFramebuffer blitFunction(QOpenGLContext *context) {
    if (!atLeastThree(context) && !context->hasExtension("GL_ARB_framebuffer_object") &&
        !context->hasExtension("GL_EXT_framebuffer_blit"))
        return nullptr;
    return reinterpret_cast<BlitFramebuffer>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
        context->getProcAddress("glBlitFramebuffer"));
}

bool hasMultisampleRenderbuffers(QOpenGLContext *context) {
    return (atLeastThree(context) || context->hasExtension("GL_ARB_framebuffer_object")) &&
           context->getProcAddress("glRenderbufferStorageMultisample") != nullptr;
}

void renderbufferStorage(QOpenGLContext *context, GLuint renderbuffer, int samples, GLenum internalFormat,
                         const QSize &size) {
    QOpenGLFunctions *f = context->functions();
    f->glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    using Storage = void(QOPENGLF_APIENTRYP)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
    auto multisample = samples > 0 && hasMultisampleRenderbuffers(context)
                           ? reinterpret_cast<Storage>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                 context->getProcAddress("glRenderbufferStorageMultisample"))
                           : nullptr;
    if (multisample)
        multisample(GL_RENDERBUFFER, samples, internalFormat, size.width(), size.height());
    else
        f->glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, size.width(), size.height());
    f->glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

QImage readFramebuffer(QOpenGLContext *context, GLuint fbo, const QSize &size, bool multisampled, bool flipped) {
    if (size.isEmpty())
        return QImage();
    QOpenGLFunctions *f = context->functions();
    GLint previous = 0;
    f->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    GLuint resolveFbo = 0, resolveRb = 0;
    GLuint readFrom = fbo;
    if (multisampled) {
        BlitFramebuffer blit = blitFunction(context);
        if (blit) {
            f->glGenFramebuffers(1, &resolveFbo);
            f->glGenRenderbuffers(1, &resolveRb);
            renderbufferStorage(context, resolveRb, 0, GL_RGBA8, size);
            f->glBindFramebuffer(GL_FRAMEBUFFER, resolveFbo);
            f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, resolveRb);
            f->glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
            f->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFbo);
            blit(0, 0, size.width(), size.height(), 0, 0, size.width(), size.height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
            readFrom = resolveFbo;
        }
    }
    f->glBindFramebuffer(GL_FRAMEBUFFER, readFrom);
    const std::size_t w = static_cast<std::size_t>(size.width()), h = static_cast<std::size_t>(size.height());
    std::vector<unsigned char> rgba(w * h * 4);
    f->glPixelStorei(GL_PACK_ALIGNMENT, 4);
    f->glReadPixels(0, 0, size.width(), size.height(), GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    for (std::size_t y = 0; y < h; ++y) {
        const unsigned char *src = rgba.data() + (flipped ? h - 1 - y : y) * w * 4;
        auto *dst = reinterpret_cast<QRgb *>( // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
            image.scanLine(static_cast<int>(y)));
        for (std::size_t x = 0; x < w; ++x, src += 4)
            dst[x] = (static_cast<QRgb>(src[3]) << 24) | (static_cast<QRgb>(src[0]) << 16) |
                     (static_cast<QRgb>(src[1]) << 8) | static_cast<QRgb>(src[2]);
    }
    f->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
    if (resolveFbo) {
        f->glDeleteFramebuffers(1, &resolveFbo);
        f->glDeleteRenderbuffers(1, &resolveRb);
    }
    return image;
}

} // namespace QtOpenGLPrivate
