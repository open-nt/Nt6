// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtGui/qopengl.h>

#include <memory>

class QOpenGLBuffer {
public:
    enum Type {
        VertexBuffer = 0x8892,     // GL_ARRAY_BUFFER
        IndexBuffer = 0x8893,      // GL_ELEMENT_ARRAY_BUFFER
        PixelPackBuffer = 0x88EB,  // GL_PIXEL_PACK_BUFFER
        PixelUnpackBuffer = 0x88EC // GL_PIXEL_UNPACK_BUFFER
    };

    enum UsagePattern {
        StreamDraw = 0x88E0,
        StreamRead = 0x88E1,
        StreamCopy = 0x88E2,
        StaticDraw = 0x88E4,
        StaticRead = 0x88E5,
        StaticCopy = 0x88E6,
        DynamicDraw = 0x88E8,
        DynamicRead = 0x88E9,
        DynamicCopy = 0x88EA
    };

    enum Access { ReadOnly = 0x88B8, WriteOnly = 0x88B9, ReadWrite = 0x88BA };

    enum RangeAccessFlag {
        RangeRead = 0x0001,
        RangeWrite = 0x0002,
        RangeInvalidate = 0x0004,
        RangeInvalidateBuffer = 0x0008,
        RangeFlushExplicit = 0x0010,
        RangeUnsynchronized = 0x0020
    };
    Q_DECLARE_FLAGS(RangeAccessFlags, RangeAccessFlag)

    QOpenGLBuffer();
    explicit QOpenGLBuffer(QOpenGLBuffer::Type type);
    QOpenGLBuffer(const QOpenGLBuffer &other);
    ~QOpenGLBuffer();

    QOpenGLBuffer &operator=(const QOpenGLBuffer &other);
    void swap(QOpenGLBuffer &other) noexcept { m_d.swap(other.m_d); }

    QOpenGLBuffer::Type type() const;
    QOpenGLBuffer::UsagePattern usagePattern() const;
    void setUsagePattern(QOpenGLBuffer::UsagePattern value);

    bool create();
    bool isCreated() const;
    void destroy();

    bool bind();
    void release();
    static void release(QOpenGLBuffer::Type type);

    GLuint bufferId() const;
    int size() const;

    bool read(int offset, void *data, int count);
    void write(int offset, const void *data, int count);
    void allocate(const void *data, int count);
    void allocate(int count) { allocate(nullptr, count); }

    void *map(QOpenGLBuffer::Access access);
    void *mapRange(int offset, int count, QOpenGLBuffer::RangeAccessFlags access);
    bool unmap();

private:
    struct Data;
    std::shared_ptr<Data> m_d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QOpenGLBuffer::RangeAccessFlags)
