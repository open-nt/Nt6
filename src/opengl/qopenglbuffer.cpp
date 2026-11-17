// MIT License - see LICENSE
#include <QtOpenGL/qopenglbuffer.h>

#include "qopenglhelpers_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qpointer.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>

#include <cstring>

namespace {

using MapBuffer = void *(QOPENGLF_APIENTRYP)(GLenum, GLenum);
using MapBufferRange = void *(QOPENGLF_APIENTRYP)(GLenum, GLintptr, GLsizeiptr, GLbitfield);
using UnmapBuffer = GLboolean(QOPENGLF_APIENTRYP)(GLenum);
using GetBufferSubData = void(QOPENGLF_APIENTRYP)(GLenum, GLintptr, GLsizeiptr, void *);

template <typename F> F resolve(QOpenGLContext *context, const char *name) {
    return reinterpret_cast<F>(context->getProcAddress(name)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

// The context current in this thread if it can use the buffers of `owner`, otherwise null.
QOpenGLContext *usableContext(QOpenGLContext *owner) {
    QOpenGLContext *current = QOpenGLContext::currentContext();
    if (!current || !owner || !QOpenGLContext::areSharing(current, owner))
        return nullptr;
    return current;
}

} // namespace

struct QOpenGLBuffer::Data {
    QOpenGLBuffer::Type type = QOpenGLBuffer::VertexBuffer;
    QOpenGLBuffer::UsagePattern usage = QOpenGLBuffer::StaticDraw;
    GLuint id = 0;
    int size = -1;
    QPointer<QOpenGLContext> context;
    // Resolved by create(), for the API of the context; null where the context has no such function.
    MapBuffer mapBuffer = nullptr;
    MapBufferRange mapBufferRange = nullptr;
    UnmapBuffer unmapBuffer = nullptr;
    GetBufferSubData getBufferSubData = nullptr;

    Data() = default;
    explicit Data(QOpenGLBuffer::Type t) : type(t) {}
    Data(const Data &) = delete;
    Data &operator=(const Data &) = delete;
    ~Data() { destroy(); }

    void resolveFunctions(QOpenGLContext *c);
    void destroy();
};

void QOpenGLBuffer::Data::resolveFunctions(QOpenGLContext *c) {
    const bool three = c->format().majorVersion() >= 3;
    if (c->isOpenGLES()) {
        if (three) {
            mapBufferRange = resolve<MapBufferRange>(c, "glMapBufferRange");
            unmapBuffer = resolve<UnmapBuffer>(c, "glUnmapBuffer");
        } else {
            if (c->hasExtension("GL_EXT_map_buffer_range"))
                mapBufferRange = resolve<MapBufferRange>(c, "glMapBufferRangeEXT");
            if (c->hasExtension("GL_OES_mapbuffer")) {
                mapBuffer = resolve<MapBuffer>(c, "glMapBufferOES");
                unmapBuffer = resolve<UnmapBuffer>(c, "glUnmapBufferOES");
            } else if (mapBufferRange) {
                unmapBuffer = resolve<UnmapBuffer>(c, "glUnmapBufferOES");
            }
        }
    } else {
        mapBuffer = resolve<MapBuffer>(c, "glMapBuffer");
        unmapBuffer = resolve<UnmapBuffer>(c, "glUnmapBuffer");
        getBufferSubData = resolve<GetBufferSubData>(c, "glGetBufferSubData");
        if (three || c->hasExtension("GL_ARB_map_buffer_range"))
            mapBufferRange = resolve<MapBufferRange>(c, "glMapBufferRange");
    }
}

void QOpenGLBuffer::Data::destroy() {
    if (!id)
        return;
    const GLuint name = id;
    if (context && !QtOpenGLPrivate::runInContext(
                       context, true, [name](QOpenGLContext *c) { c->functions()->glDeleteBuffers(1, &name); }))
        qWarning("QOpenGLBuffer::destroy(): the context of the buffer cannot be made current, so the buffer leaks");
    // A context that is gone took the buffer with it.
    id = 0;
    size = -1;
    context = nullptr;
}

/*!
    \class QOpenGLBuffer
    \inmodule NtOpenGL
    \since 6.0
    \brief The QOpenGLBuffer class provides functions for creating and managing OpenGL buffer objects.

    A QOpenGLBuffer holds vertices, indices or pixels in the memory of the
    graphics card. Create it with create() while a context is current, bind
    it, and fill it with allocate():

    \code
    QOpenGLBuffer vertices(QOpenGLBuffer::VertexBuffer);
    vertices.create();
    vertices.bind();
    vertices.allocate(points.constData(), int(points.size() * sizeof(QVector3D)));
    program.enableAttributeArray(0);
    program.setAttributeBuffer(0, GL_FLOAT, 0, 3);
    \endcode

    Copies of a QOpenGLBuffer refer to the same OpenGL buffer, which is
    deleted when the last copy is destroyed, or by destroy(). Its context, or
    one that shares with it, should be current then; otherwise the buffer makes
    its context current on an offscreen surface for the time.

    \differences
    size() returns the size given to the last allocate() rather than asking
    OpenGL. A buffer whose context is current in another thread when it is
    destroyed leaks, with a warning, where Qt deletes it later. map() on OpenGL ES 3.0
    maps the whole buffer with \c glMapBufferRange(); on OpenGL ES 2.0 it needs
    \c GL_OES_mapbuffer, which allows only WriteOnly.
    \enddifferences

    \sa QOpenGLVertexArrayObject, QOpenGLShaderProgram
*/

/*!
    \enum QOpenGLBuffer::Type

    This enum gives the target a buffer binds to.

    \value VertexBuffer Vertex attributes, \c GL_ARRAY_BUFFER.
    \value IndexBuffer Indices of vertices, \c GL_ELEMENT_ARRAY_BUFFER.
    \value PixelPackBuffer Pixels read back from OpenGL, \c GL_PIXEL_PACK_BUFFER.
    \value PixelUnpackBuffer Pixels written to OpenGL, \c GL_PIXEL_UNPACK_BUFFER.
*/

/*!
    \enum QOpenGLBuffer::UsagePattern

    This enum tells OpenGL how the data of a buffer will be used, so that it
    can put the data where that is fastest.

    \value StreamDraw Set once, drawn from a few times.
    \value StreamRead Set once by OpenGL, read back a few times.
    \value StreamCopy Set once by OpenGL, drawn from a few times.
    \value StaticDraw Set once, drawn from many times.
    \value StaticRead Set once by OpenGL, read back many times.
    \value StaticCopy Set once by OpenGL, drawn from many times.
    \value DynamicDraw Set and drawn from many times.
    \value DynamicRead Set by OpenGL and read back many times.
    \value DynamicCopy Set by OpenGL and drawn from many times.
*/

/*!
    \enum QOpenGLBuffer::Access

    This enum gives what map() lets the application do with the memory.

    \value ReadOnly Read it.
    \value WriteOnly Write it.
    \value ReadWrite Read and write it.
*/

/*!
    \enum QOpenGLBuffer::RangeAccessFlag

    This enum gives what mapRange() lets the application do with the memory.

    \value RangeRead Read it.
    \value RangeWrite Write it.
    \value RangeInvalidate The old contents of the range may be discarded.
    \value RangeInvalidateBuffer The old contents of the whole buffer may be discarded.
    \value RangeFlushExplicit Changes are made visible with \c glFlushMappedBufferRange().
    \value RangeUnsynchronized OpenGL does not wait for pending operations on the buffer.
*/

/*!
    Constructs a vertex buffer. Call create() to create the OpenGL buffer.
*/
QOpenGLBuffer::QOpenGLBuffer() : m_d(std::make_shared<Data>()) {}

/*!
    Constructs a buffer of the given \a type. Call create() to create the
    OpenGL buffer.
*/
QOpenGLBuffer::QOpenGLBuffer(QOpenGLBuffer::Type type) : m_d(std::make_shared<Data>(type)) {}

/*!
    Constructs a copy of \a other that refers to the same OpenGL buffer.
*/
QOpenGLBuffer::QOpenGLBuffer(const QOpenGLBuffer &other) = default;

/*!
    Destroys this copy of the buffer. The OpenGL buffer is deleted with the
    last copy, in its context, or one that shares with it.
*/
QOpenGLBuffer::~QOpenGLBuffer() = default;

/*!
    Makes this buffer refer to the OpenGL buffer of \a other, and returns a
    reference to it.
*/
QOpenGLBuffer &QOpenGLBuffer::operator=(const QOpenGLBuffer &other) = default;

/*!
    \fn void QOpenGLBuffer::swap(QOpenGLBuffer &other)

    Swaps this buffer with \a other.
*/

/*!
    Returns the type of the buffer.
*/
QOpenGLBuffer::Type QOpenGLBuffer::type() const { return m_d->type; }

/*!
    Returns the usage pattern that allocate() passes to OpenGL. The default is
    StaticDraw.
*/
QOpenGLBuffer::UsagePattern QOpenGLBuffer::usagePattern() const { return m_d->usage; }

/*!
    Sets the usage pattern that the next allocate() passes to OpenGL to
    \a value.
*/
void QOpenGLBuffer::setUsagePattern(QOpenGLBuffer::UsagePattern value) { m_d->usage = value; }

/*!
    Creates the OpenGL buffer in the current context, and returns \c true if
    it could. A buffer that exists already is kept.

    \sa isCreated(), bind(), destroy()
*/
bool QOpenGLBuffer::create() {
    if (m_d->id)
        return true;
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context) {
        qWarning("QOpenGLBuffer::create(): no context is current");
        return false;
    }
    context->functions()->glGenBuffers(1, &m_d->id);
    if (!m_d->id)
        return false;
    m_d->context = context;
    m_d->resolveFunctions(context);
    return true;
}

/*!
    Returns \c true if the OpenGL buffer has been created.
*/
bool QOpenGLBuffer::isCreated() const { return m_d->id != 0; }

/*!
    Deletes the OpenGL buffer for all copies. If neither the context of the
    buffer nor one that shares with it is current, the context is made current
    on an offscreen surface for the time.
*/
void QOpenGLBuffer::destroy() { m_d->destroy(); }

/*!
    Binds the buffer to its target in the current context, and returns
    \c true if it could: the buffer must have been created in that context or
    one that shares with it.

    \sa release(), create()
*/
bool QOpenGLBuffer::bind() {
    if (!m_d->id) {
        qWarning("QOpenGLBuffer::bind(): the buffer has not been created");
        return false;
    }
    QOpenGLContext *context = usableContext(m_d->context);
    if (!context) {
        qWarning("QOpenGLBuffer::bind(): the buffer does not belong to the current context");
        return false;
    }
    context->functions()->glBindBuffer(static_cast<GLenum>(m_d->type), m_d->id);
    return true;
}

/*!
    Unbinds buffers of the type of this buffer from the current context.

    \sa bind()
*/
void QOpenGLBuffer::release() { release(m_d->type); }

/*!
    Unbinds buffers of the given \a type from the current context.
*/
void QOpenGLBuffer::release(QOpenGLBuffer::Type type) {
    if (QOpenGLContext *context = QOpenGLContext::currentContext())
        context->functions()->glBindBuffer(static_cast<GLenum>(type), 0);
}

/*!
    Returns the name of the OpenGL buffer, or 0 before create().
*/
GLuint QOpenGLBuffer::bufferId() const { return m_d->id; }

/*!
    Returns the size given to the last allocate(), in bytes, or -1 if the
    buffer has not been allocated.
*/
int QOpenGLBuffer::size() const { return m_d->size; }

/*!
    Reads \a count bytes at \a offset in the buffer into \a data, and returns
    \c true if it could. The buffer must be bound. On OpenGL ES, this needs
    \c glMapBufferRange().
*/
bool QOpenGLBuffer::read(int offset, void *data, int count) {
    QOpenGLContext *context = usableContext(m_d->context);
    if (!context || !m_d->id || offset < 0 || count < 0)
        return false;
    const auto target = static_cast<GLenum>(m_d->type);
    if (m_d->getBufferSubData) {
        m_d->getBufferSubData(target, offset, count, data);
        return true;
    }
    if (!m_d->mapBufferRange || !m_d->unmapBuffer)
        return false;
    void *mapped = m_d->mapBufferRange(target, offset, count, GL_MAP_READ_BIT);
    if (!mapped)
        return false;
    std::memcpy(data, mapped, static_cast<std::size_t>(count));
    m_d->unmapBuffer(target);
    return true;
}

/*!
    Replaces \a count bytes at \a offset in the buffer with \a data. The
    buffer must be bound and allocated.

    \sa allocate()
*/
void QOpenGLBuffer::write(int offset, const void *data, int count) {
    if (QOpenGLContext *context = usableContext(m_d->context); context && m_d->id)
        context->functions()->glBufferSubData(static_cast<GLenum>(m_d->type), offset, count, data);
}

/*!
    Gives the buffer \a count bytes, copied from \a data, or left undefined
    if \a data is \c nullptr, with the usage pattern. The buffer must be bound.

    \sa write(), usagePattern()
*/
void QOpenGLBuffer::allocate(const void *data, int count) {
    QOpenGLContext *context = usableContext(m_d->context);
    if (!context || !m_d->id || count < 0)
        return;
    context->functions()->glBufferData(static_cast<GLenum>(m_d->type), count, data, static_cast<GLenum>(m_d->usage));
    m_d->size = count;
}

/*!
    \fn void QOpenGLBuffer::allocate(int count)
    \overload

    Gives the buffer \a count bytes whose contents are undefined.
*/

/*!
    Maps the whole buffer into the memory of the application with
    \a access, and returns where, or \c nullptr if it cannot. The buffer must
    be bound. Call unmap() when done.

    \sa mapRange(), unmap()
*/
void *QOpenGLBuffer::map(QOpenGLBuffer::Access access) {
    QOpenGLContext *context = usableContext(m_d->context);
    if (!context || !m_d->id)
        return nullptr;
    const auto target = static_cast<GLenum>(m_d->type);
    if (m_d->mapBuffer && (!context->isOpenGLES() || access == WriteOnly))
        return m_d->mapBuffer(target, static_cast<GLenum>(access));
    if (!m_d->mapBufferRange)
        return nullptr;
    GLint size = 0;
    context->functions()->glGetBufferParameteriv(target, GL_BUFFER_SIZE, &size);
    if (size <= 0)
        return nullptr;
    GLbitfield flags = 0;
    if (access != WriteOnly)
        flags |= GL_MAP_READ_BIT;
    if (access != ReadOnly)
        flags |= GL_MAP_WRITE_BIT;
    return m_d->mapBufferRange(target, 0, size, flags);
}

/*!
    Maps \a count bytes at \a offset in the buffer into the memory of the
    application with \a access, and returns where, or \c nullptr if it
    cannot. The buffer must be bound, and the context OpenGL ES 3.0, desktop
    OpenGL 3.0, or have \c glMapBufferRange() as an extension. Call unmap()
    when done.

    \sa map(), unmap()
*/
void *QOpenGLBuffer::mapRange(int offset, int count, QOpenGLBuffer::RangeAccessFlags access) {
    if (!usableContext(m_d->context) || !m_d->id || !m_d->mapBufferRange || offset < 0 || count <= 0)
        return nullptr;
    return m_d->mapBufferRange(static_cast<GLenum>(m_d->type), offset, count, static_cast<GLbitfield>(access.toInt()));
}

/*!
    Unmaps the buffer after map() or mapRange(), and returns \c true if its
    contents are intact. The buffer must be bound.
*/
bool QOpenGLBuffer::unmap() {
    if (!usableContext(m_d->context) || !m_d->id || !m_d->unmapBuffer)
        return false;
    return m_d->unmapBuffer(static_cast<GLenum>(m_d->type)) == GL_TRUE;
}
