// MIT License - see LICENSE
#include <QtOpenGL/qopenglframebufferobject.h>

#include "qopenglhelpers_p.h"

#include <QtCore/qdebug.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>
#include <QtGui/qsurface.h>

#include <cstring>
#include <vector>

struct QOpenGLFramebufferObject::Data {
    QOpenGLContext *context = nullptr;
    QOpenGLFramebufferObjectFormat format;
    QSize size;
    GLuint fbo = 0;
    GLuint texture = 0;
    GLuint color = 0; // the multisample color renderbuffer
    GLuint depth = 0;
    GLuint stencil = 0;
    bool valid = false;

    void create();
    void attach(QOpenGLFramebufferObject::Attachment attachment);
    void releaseAttachments();
};

void QOpenGLFramebufferObject::Data::attach(QOpenGLFramebufferObject::Attachment attachment) {
    QOpenGLFunctions *f = context->functions();
    releaseAttachments();
    const int samples = std::max(0, format.samples());
    if (attachment == QOpenGLFramebufferObject::CombinedDepthStencil) {
        f->glGenRenderbuffers(1, &depth);
        QtOpenGLPrivate::renderbufferStorage(context, depth, samples, GL_DEPTH24_STENCIL8, size);
        f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth);
    } else if (attachment == QOpenGLFramebufferObject::Depth) {
        f->glGenRenderbuffers(1, &depth);
        QtOpenGLPrivate::renderbufferStorage(context, depth, samples, GL_DEPTH_COMPONENT24, size);
        f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    }
    format.setAttachment(attachment);
}

void QOpenGLFramebufferObject::Data::releaseAttachments() {
    QOpenGLFunctions *f = context->functions();
    if (depth) {
        f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
        f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
        f->glDeleteRenderbuffers(1, &depth);
        depth = 0;
    }
    if (stencil) {
        f->glDeleteRenderbuffers(1, &stencil);
        stencil = 0;
    }
}

void QOpenGLFramebufferObject::Data::create() {
    context = QOpenGLContext::currentContext();
    if (!context) {
        qWarning("QOpenGLFramebufferObject: no OpenGL context is current");
        return;
    }
    if (size.width() <= 0 || size.height() <= 0)
        return;
    QOpenGLFunctions *f = context->functions();
    GLint previous = 0;
    f->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    const bool es2 = context->isOpenGLES() && context->format().majorVersion() < 3;
    if (format.samples() > 0 && !QtOpenGLPrivate::hasMultisampleRenderbuffers(context))
        format.setSamples(0);
    const GLenum internal = format.internalTextureFormat() ? format.internalTextureFormat()
                            : es2                          ? GLenum(GL_RGBA)
                                                           : GLenum(GL_RGBA8);
    format.setInternalTextureFormat(internal);
    f->glGenFramebuffers(1, &fbo);
    f->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    if (format.samples() > 0) {
        f->glGenRenderbuffers(1, &color);
        QtOpenGLPrivate::renderbufferStorage(context, color, format.samples(), es2 ? GL_RGBA4 : GL_RGBA8, size);
        f->glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
        GLint got = 0;
        f->glBindRenderbuffer(GL_RENDERBUFFER, color);
        f->glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &got);
        f->glBindRenderbuffer(GL_RENDERBUFFER, 0);
        format.setSamples(got);
    } else {
        GLint previousTexture = 0;
        f->glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        f->glGenTextures(1, &texture);
        f->glBindTexture(format.textureTarget(), texture);
        f->glTexImage2D(format.textureTarget(), 0, static_cast<GLint>(internal), size.width(), size.height(), 0,
                        GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        f->glTexParameteri(format.textureTarget(), GL_TEXTURE_MIN_FILTER,
                           format.mipmap() ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        f->glTexParameteri(format.textureTarget(), GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        f->glTexParameteri(format.textureTarget(), GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        f->glTexParameteri(format.textureTarget(), GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (format.mipmap())
            f->glGenerateMipmap(format.textureTarget());
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, format.textureTarget(), texture, 0);
        f->glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    }
    attach(format.attachment());
    valid = f->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!valid)
        qWarning("QOpenGLFramebufferObject: the framebuffer object is not complete");
    f->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
}

/*!
    \class QOpenGLFramebufferObject
    \inmodule NtOpenGL
    \since 6.0
    \brief The QOpenGLFramebufferObject class encapsulates an OpenGL framebuffer object.

    A framebuffer object is an offscreen target for OpenGL, with a texture or,
    when it is multisampled, a renderbuffer for its colors. It is created in
    the context that is current, and works in contexts that share with it.

    Paint on one with QPainter through a QOpenGLPaintDevice, and read the
    result back with toImage():

    \code
    QOpenGLFramebufferObjectFormat format;
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(4);
    QOpenGLFramebufferObject fbo(QSize(400, 300), format);
    fbo.bind();
    QOpenGLPaintDevice device(fbo.size());
    QPainter painter(&device);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawEllipse(QRectF(10, 10, 380, 280));
    painter.end();
    QImage image = fbo.toImage();
    \endcode

    QPainter needs a stencil buffer to fill shapes that are not convex, so
    give the object the \c CombinedDepthStencil attachment.

    \differences
    There is one color attachment: \c addColorAttachment(), \c textures() and
    \c sizes() are missing, and the color attachment index of
    blitFramebuffer() must be 0. Rectangle textures and \c GL_TEXTURE_2D_MULTISAMPLE
    are not supported. The object must be destroyed while a context that shares
    with its context is current; otherwise its OpenGL objects are left to the
    context.
    \enddifferences

    \sa QOpenGLFramebufferObjectFormat, QOpenGLPaintDevice, QOpenGLContext
*/

/*!
    \enum QOpenGLFramebufferObject::Attachment

    This enum describes the buffers attached to the framebuffer object besides
    its colors.

    \value NoAttachment None.
    \value CombinedDepthStencil A depth and a stencil buffer, in one renderbuffer.
    \value Depth A depth buffer.
*/

/*!
    \enum QOpenGLFramebufferObject::FramebufferRestorePolicy

    This enum describes which framebuffer blitFramebuffer() leaves bound.

    \value DontRestoreFramebufferBinding The framebuffer that was drawn to stays bound.
    \value RestoreFramebufferBindingToDefault The default framebuffer of the context is bound.
    \value RestoreFrameBufferBinding The framebuffer that was bound before is bound again.
*/

/*!
    Constructs a framebuffer object of \a size, with a texture of \a target
    and no depth or stencil buffer, in the current context.
*/
QOpenGLFramebufferObject::QOpenGLFramebufferObject(const QSize &size, GLenum target) : m_d(std::make_unique<Data>()) {
    m_d->size = size;
    m_d->format.setTextureTarget(target);
    m_d->create();
}

/*!
    \overload

    Constructs a framebuffer object of \a width by \a height, with a texture of \a target.
*/
QOpenGLFramebufferObject::QOpenGLFramebufferObject(int width, int height, GLenum target)
    : QOpenGLFramebufferObject(QSize(width, height), target) {}

/*!
    \overload

    Constructs a framebuffer object of \a size with \a attachment and a
    texture of \a target in \a internalFormat (0 for the default).
*/
QOpenGLFramebufferObject::QOpenGLFramebufferObject(const QSize &size, Attachment attachment, GLenum target,
                                                   GLenum internalFormat)
    : m_d(std::make_unique<Data>()) {
    m_d->size = size;
    m_d->format.setAttachment(attachment);
    m_d->format.setTextureTarget(target);
    m_d->format.setInternalTextureFormat(internalFormat);
    m_d->create();
}

/*!
    \overload

    Constructs a framebuffer object of \a width by \a height with
    \a attachment and a texture of \a target in \a internalFormat.
*/
QOpenGLFramebufferObject::QOpenGLFramebufferObject(int width, int height, Attachment attachment, GLenum target,
                                                   GLenum internalFormat)
    : QOpenGLFramebufferObject(QSize(width, height), attachment, target, internalFormat) {}

/*!
    \overload

    Constructs a framebuffer object of \a size with \a format.
*/
QOpenGLFramebufferObject::QOpenGLFramebufferObject(const QSize &size, const QOpenGLFramebufferObjectFormat &format)
    : m_d(std::make_unique<Data>()) {
    m_d->size = size;
    m_d->format = format;
    m_d->create();
}

/*!
    \overload

    Constructs a framebuffer object of \a width by \a height with \a format.
*/
QOpenGLFramebufferObject::QOpenGLFramebufferObject(int width, int height, const QOpenGLFramebufferObjectFormat &format)
    : QOpenGLFramebufferObject(QSize(width, height), format) {}

/*!
    Destroys the framebuffer object and its buffers.
*/
QOpenGLFramebufferObject::~QOpenGLFramebufferObject() {
    QOpenGLContext *current = QOpenGLContext::currentContext();
    if (!m_d->context || !current || !QOpenGLContext::areSharing(current, m_d->context))
        return;
    QOpenGLFunctions *f = current->functions();
    if (isBound())
        f->glBindFramebuffer(GL_FRAMEBUFFER, current->defaultFramebufferObject());
    if (m_d->texture)
        f->glDeleteTextures(1, &m_d->texture);
    const GLuint renderbuffers[] = {m_d->color, m_d->depth, m_d->stencil};
    for (GLuint rb : renderbuffers)
        if (rb)
            f->glDeleteRenderbuffers(1, &rb);
    if (m_d->fbo)
        f->glDeleteFramebuffers(1, &m_d->fbo);
}

/*!
    Returns the format of the framebuffer object, with the number of samples
    and the internal format it got.
*/
QOpenGLFramebufferObjectFormat QOpenGLFramebufferObject::format() const { return m_d->format; }

/*!
    Returns \c true if the framebuffer object was created and is complete.
*/
bool QOpenGLFramebufferObject::isValid() const { return m_d->valid; }

/*!
    Returns \c true if the framebuffer object is bound in the current context.
*/
bool QOpenGLFramebufferObject::isBound() const {
    QOpenGLContext *current = QOpenGLContext::currentContext();
    if (!current || !m_d->fbo)
        return false;
    GLint bound = 0;
    current->functions()->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound);
    return static_cast<GLuint>(bound) == m_d->fbo;
}

/*!
    Makes the framebuffer object the target of drawing in the current context,
    and returns \c true if it is valid.

    \sa release()
*/
bool QOpenGLFramebufferObject::bind() {
    QOpenGLContext *current = QOpenGLContext::currentContext();
    if (!m_d->valid || !current)
        return false;
    current->functions()->glBindFramebuffer(GL_FRAMEBUFFER, m_d->fbo);
    return true;
}

/*!
    Binds the default framebuffer of the current context again, and returns
    \c true if there is a current context.

    \sa bind(), QOpenGLContext::defaultFramebufferObject()
*/
bool QOpenGLFramebufferObject::release() { return bindDefault(); }

/*!
    Returns the size of the framebuffer object in pixels.
*/
QSize QOpenGLFramebufferObject::size() const { return m_d->size; }

/*!
    \fn int QOpenGLFramebufferObject::width() const

    Returns the width of the framebuffer object in pixels.
*/

/*!
    \fn int QOpenGLFramebufferObject::height() const

    Returns the height of the framebuffer object in pixels.
*/

/*!
    Returns the texture of the colors, or 0 if the object is multisampled.

    \sa takeTexture()
*/
GLuint QOpenGLFramebufferObject::texture() const { return m_d->texture; }

/*!
    Returns the texture of the colors and gives it to the caller, who must
    delete it. The framebuffer object is not valid afterwards.
*/
GLuint QOpenGLFramebufferObject::takeTexture() {
    const GLuint texture = m_d->texture;
    if (texture && m_d->context) {
        QOpenGLFunctions *f = m_d->context->functions();
        GLint previous = 0;
        f->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
        f->glBindFramebuffer(GL_FRAMEBUFFER, m_d->fbo);
        f->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_d->format.textureTarget(), 0, 0);
        f->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
        m_d->texture = 0;
        m_d->valid = false;
    }
    return texture;
}

/*!
    Returns the contents of the framebuffer object as a premultiplied image.
    OpenGL keeps rows from the bottom up; if \a flipped is \c true, the
    default, the image has its first row at the top, as it is painted. A
    multisampled object is resolved first.
*/
QImage QOpenGLFramebufferObject::toImage(bool flipped) const {
    QOpenGLContext *current = QOpenGLContext::currentContext();
    if (!m_d->valid || !current)
        return QImage();
    return QtOpenGLPrivate::readFramebuffer(current, m_d->fbo, m_d->size, m_d->format.samples() > 0, flipped);
}

/*!
    Returns the attachment of the framebuffer object.
*/
QOpenGLFramebufferObject::Attachment QOpenGLFramebufferObject::attachment() const { return m_d->format.attachment(); }

/*!
    Replaces the depth and stencil buffers with \a attachment.
*/
void QOpenGLFramebufferObject::setAttachment(Attachment attachment) {
    if (!m_d->context || attachment == m_d->format.attachment())
        return;
    QOpenGLFunctions *f = m_d->context->functions();
    GLint previous = 0;
    f->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    f->glBindFramebuffer(GL_FRAMEBUFFER, m_d->fbo);
    m_d->attach(attachment);
    m_d->valid = f->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    f->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
}

/*!
    Returns the OpenGL name of the framebuffer object.
*/
GLuint QOpenGLFramebufferObject::handle() const { return m_d->fbo; }

/*!
    Binds the default framebuffer of the current context and returns \c true
    if there is a current context.
*/
bool QOpenGLFramebufferObject::bindDefault() {
    QOpenGLContext *current = QOpenGLContext::currentContext();
    if (!current)
        return false;
    current->functions()->glBindFramebuffer(GL_FRAMEBUFFER, current->defaultFramebufferObject());
    return true;
}

/*!
    Returns \c true: every context Nt creates has framebuffer objects.
*/
bool QOpenGLFramebufferObject::hasOpenGLFramebufferObjects() { return true; }

/*!
    Returns \c true if the current context can copy between framebuffers with
    blitFramebuffer(), which needs desktop OpenGL 3.0 or OpenGL ES 3.0.
*/
bool QOpenGLFramebufferObject::hasOpenGLFramebufferBlit() {
    QOpenGLContext *current = QOpenGLContext::currentContext();
    return current && QtOpenGLPrivate::blitFunction(current) != nullptr;
}

/*!
    Copies \a sourceRect of \a source to \a targetRect of \a target, with
    \a filter where the sizes differ. The \a buffers are a combination of
    \c GL_COLOR_BUFFER_BIT, \c GL_DEPTH_BUFFER_BIT and \c GL_STENCIL_BUFFER_BIT.
    A null framebuffer object means the default framebuffer of the current
    context. Rectangles are in OpenGL's coordinates, from the bottom left.
*/
void QOpenGLFramebufferObject::blitFramebuffer(QOpenGLFramebufferObject *target, const QRect &targetRect,
                                               QOpenGLFramebufferObject *source, const QRect &sourceRect,
                                               GLbitfield buffers, GLenum filter) {
    blitFramebuffer(target, targetRect, source, sourceRect, buffers, filter, 0, 0, RestoreFrameBufferBinding);
}

/*!
    \overload

    Copies all of \a source to all of \a target, with \a filter, for \a buffers.
*/
void QOpenGLFramebufferObject::blitFramebuffer(QOpenGLFramebufferObject *target, QOpenGLFramebufferObject *source,
                                               GLbitfield buffers, GLenum filter) {
    QOpenGLContext *current = QOpenGLContext::currentContext();
    if (!current)
        return;
    const QSize fallback = current->surface() ? current->surface()->size() : QSize();
    const QSize targetSize = target ? target->size() : fallback;
    const QSize sourceSize = source ? source->size() : fallback;
    blitFramebuffer(target, QRect(QPoint(0, 0), targetSize), source, QRect(QPoint(0, 0), sourceSize), buffers, filter);
}

/*!
    \overload

    Copies \a sourceRect of \a source to \a targetRect of \a target for
    \a buffers with \a filter, and binds the framebuffer that
    \a restorePolicy names afterwards. The color attachment indexes
    \a readColorAttachmentIndex and \a drawColorAttachmentIndex must be 0.
*/
void QOpenGLFramebufferObject::blitFramebuffer(QOpenGLFramebufferObject *target, const QRect &targetRect,
                                               QOpenGLFramebufferObject *source, const QRect &sourceRect,
                                               GLbitfield buffers, GLenum filter, int readColorAttachmentIndex,
                                               int drawColorAttachmentIndex, FramebufferRestorePolicy restorePolicy) {
    QOpenGLContext *current = QOpenGLContext::currentContext();
    if (!current)
        return;
    if (readColorAttachmentIndex != 0 || drawColorAttachmentIndex != 0) {
        qWarning("QOpenGLFramebufferObject::blitFramebuffer(): only color attachment 0 exists");
        return;
    }
    auto blit = QtOpenGLPrivate::blitFunction(current);
    if (!blit) {
        qWarning("QOpenGLFramebufferObject::blitFramebuffer(): the context cannot blit");
        return;
    }
    QOpenGLFunctions *f = current->functions();
    GLint previous = 0;
    f->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    const GLuint defaultFbo = current->defaultFramebufferObject();
    f->glBindFramebuffer(GL_READ_FRAMEBUFFER, source ? source->handle() : defaultFbo);
    f->glBindFramebuffer(GL_DRAW_FRAMEBUFFER, target ? target->handle() : defaultFbo);
    blit(sourceRect.x(), sourceRect.y(), sourceRect.x() + sourceRect.width(), sourceRect.y() + sourceRect.height(),
         targetRect.x(), targetRect.y(), targetRect.x() + targetRect.width(), targetRect.y() + targetRect.height(),
         buffers, filter);
    switch (restorePolicy) {
    case DontRestoreFramebufferBinding:
        f->glBindFramebuffer(GL_FRAMEBUFFER, target ? target->handle() : defaultFbo);
        break;
    case RestoreFramebufferBindingToDefault:
        f->glBindFramebuffer(GL_FRAMEBUFFER, defaultFbo);
        break;
    case RestoreFrameBufferBinding:
        f->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
        break;
    }
}

/*!
    \class QOpenGLFramebufferObjectFormat
    \inmodule NtOpenGL
    \since 6.0
    \brief The QOpenGLFramebufferObjectFormat class specifies the format of an OpenGL framebuffer object.

    \differences
    The texture target must be \c GL_TEXTURE_2D.
    \enddifferences

    \sa QOpenGLFramebufferObject
*/

/*!
    \fn QOpenGLFramebufferObjectFormat::QOpenGLFramebufferObjectFormat()

    Constructs a format with no samples, no mipmaps, no attachment, a
    \c GL_TEXTURE_2D texture and the default internal format.
*/

/*!
    \fn void QOpenGLFramebufferObjectFormat::setSamples(int samples)

    Sets the number of samples per pixel to \a samples. With more than 0, the
    object is multisampled and has no texture.
*/

/*!
    \fn int QOpenGLFramebufferObjectFormat::samples() const

    Returns the number of samples per pixel.
*/

/*!
    \fn void QOpenGLFramebufferObjectFormat::setMipmap(bool enabled)

    Gives the texture mipmaps if \a enabled is \c true.
*/

/*!
    \fn bool QOpenGLFramebufferObjectFormat::mipmap() const

    Returns \c true if the texture has mipmaps.
*/

/*!
    \fn void QOpenGLFramebufferObjectFormat::setAttachment(QOpenGLFramebufferObject::Attachment attachment)

    Sets the depth and stencil \a attachment.
*/

/*!
    \fn QOpenGLFramebufferObject::Attachment QOpenGLFramebufferObjectFormat::attachment() const

    Returns the depth and stencil attachment.
*/

/*!
    \fn void QOpenGLFramebufferObjectFormat::setTextureTarget(GLenum target)

    Sets the texture target to \a target.
*/

/*!
    \fn GLenum QOpenGLFramebufferObjectFormat::textureTarget() const

    Returns the texture target.
*/

/*!
    \fn void QOpenGLFramebufferObjectFormat::setInternalTextureFormat(GLenum internalTextureFormat)

    Sets the internal format of the colors to \a internalTextureFormat.
*/

/*!
    \fn GLenum QOpenGLFramebufferObjectFormat::internalTextureFormat() const

    Returns the internal format of the colors, or 0 for the default.
*/

/*!
    \fn bool QOpenGLFramebufferObjectFormat::operator==(const QOpenGLFramebufferObjectFormat &other) const

    Returns \c true if this format equals \a other.
*/

/*!
    \fn bool QOpenGLFramebufferObjectFormat::operator!=(const QOpenGLFramebufferObjectFormat &other) const

    Returns \c true if this format differs from \a other.
*/
