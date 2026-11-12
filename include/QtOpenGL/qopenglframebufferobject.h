// MIT License - see LICENSE
#pragma once

#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtGui/qimage.h>
#include <QtGui/qopengl.h>

#include <memory>

class QOpenGLFramebufferObjectFormat;

class QOpenGLFramebufferObject {
public:
    enum Attachment { NoAttachment, CombinedDepthStencil, Depth };
    enum FramebufferRestorePolicy {
        DontRestoreFramebufferBinding,
        RestoreFramebufferBindingToDefault,
        RestoreFrameBufferBinding
    };

    explicit QOpenGLFramebufferObject(const QSize &size, GLenum target = GL_TEXTURE_2D);
    QOpenGLFramebufferObject(int width, int height, GLenum target = GL_TEXTURE_2D);
    QOpenGLFramebufferObject(const QSize &size, Attachment attachment, GLenum target = GL_TEXTURE_2D,
                             GLenum internalFormat = 0);
    QOpenGLFramebufferObject(int width, int height, Attachment attachment, GLenum target = GL_TEXTURE_2D,
                             GLenum internalFormat = 0);
    QOpenGLFramebufferObject(const QSize &size, const QOpenGLFramebufferObjectFormat &format);
    QOpenGLFramebufferObject(int width, int height, const QOpenGLFramebufferObjectFormat &format);
    virtual ~QOpenGLFramebufferObject();

    QOpenGLFramebufferObjectFormat format() const;

    bool isValid() const;
    bool isBound() const;
    bool bind();
    bool release();

    int width() const { return size().width(); }
    int height() const { return size().height(); }
    QSize size() const;

    GLuint texture() const;
    GLuint takeTexture();
    QImage toImage(bool flipped = true) const;

    Attachment attachment() const;
    void setAttachment(Attachment attachment);

    GLuint handle() const;

    static bool bindDefault();
    static bool hasOpenGLFramebufferObjects();
    static bool hasOpenGLFramebufferBlit();
    static void blitFramebuffer(QOpenGLFramebufferObject *target, const QRect &targetRect,
                                QOpenGLFramebufferObject *source, const QRect &sourceRect,
                                GLbitfield buffers = GL_COLOR_BUFFER_BIT, GLenum filter = GL_NEAREST);
    static void blitFramebuffer(QOpenGLFramebufferObject *target, QOpenGLFramebufferObject *source,
                                GLbitfield buffers = GL_COLOR_BUFFER_BIT, GLenum filter = GL_NEAREST);
    static void blitFramebuffer(QOpenGLFramebufferObject *target, const QRect &targetRect,
                                QOpenGLFramebufferObject *source, const QRect &sourceRect, GLbitfield buffers,
                                GLenum filter, int readColorAttachmentIndex, int drawColorAttachmentIndex,
                                FramebufferRestorePolicy restorePolicy = RestoreFrameBufferBinding);

private:
    Q_DISABLE_COPY(QOpenGLFramebufferObject)
    struct Data;
    std::unique_ptr<Data> m_d;
};

class QOpenGLFramebufferObjectFormat {
public:
    QOpenGLFramebufferObjectFormat() = default;

    void setSamples(int samples) { m_samples = samples; }
    int samples() const { return m_samples; }
    void setMipmap(bool enabled) { m_mipmap = enabled; }
    bool mipmap() const { return m_mipmap; }
    void setAttachment(QOpenGLFramebufferObject::Attachment attachment) { m_attachment = attachment; }
    QOpenGLFramebufferObject::Attachment attachment() const { return m_attachment; }
    void setTextureTarget(GLenum target) { m_target = target; }
    GLenum textureTarget() const { return m_target; }
    void setInternalTextureFormat(GLenum internalTextureFormat) { m_internalFormat = internalTextureFormat; }
    GLenum internalTextureFormat() const { return m_internalFormat; }

    bool operator==(const QOpenGLFramebufferObjectFormat &other) const {
        return m_samples == other.m_samples && m_mipmap == other.m_mipmap && m_attachment == other.m_attachment &&
               m_target == other.m_target && m_internalFormat == other.m_internalFormat;
    }
    bool operator!=(const QOpenGLFramebufferObjectFormat &other) const { return !(*this == other); }

private:
    int m_samples = 0;
    bool m_mipmap = false;
    QOpenGLFramebufferObject::Attachment m_attachment = QOpenGLFramebufferObject::NoAttachment;
    GLenum m_target = GL_TEXTURE_2D;
    GLenum m_internalFormat = 0; // GL_RGBA8 on desktop OpenGL and OpenGL ES 3, GL_RGBA on OpenGL ES 2
};
