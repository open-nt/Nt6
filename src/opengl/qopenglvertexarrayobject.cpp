// MIT License - see LICENSE
#include <QtOpenGL/qopenglvertexarrayobject.h>

#include "qopenglhelpers_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qpointer.h>
#include <QtGui/qopenglcontext.h>

namespace {

using GenVertexArrays = void(QOPENGLF_APIENTRYP)(GLsizei, GLuint *);
using DeleteVertexArrays = void(QOPENGLF_APIENTRYP)(GLsizei, const GLuint *);
using BindVertexArray = void(QOPENGLF_APIENTRYP)(GLuint);

template <typename F> F resolve(QOpenGLContext *context, const char *name) {
    return reinterpret_cast<F>(context->getProcAddress(name)); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
}

} // namespace

struct QOpenGLVertexArrayObject::Data {
    GLuint id = 0;
    QPointer<QOpenGLContext> context;
    GenVertexArrays gen = nullptr;
    DeleteVertexArrays del = nullptr;
    BindVertexArray bindArray = nullptr;

    // The current context if it is the one the object was made in: vertex array objects are not shared.
    QOpenGLContext *usableContext() const {
        QOpenGLContext *current = QOpenGLContext::currentContext();
        return current && current == context ? current : nullptr;
    }
};

/*!
    \class QOpenGLVertexArrayObject
    \inmodule NtOpenGL
    \since 6.0
    \brief The QOpenGLVertexArrayObject class wraps an OpenGL Vertex Array Object.

    A vertex array object remembers the vertex attribute setup and the bound
    index buffer, so that drawing a mesh needs only binding its object. Set it
    up once with the object bound:

    \code
    m_vao.create();
    QOpenGLVertexArrayObject::Binder binder(&m_vao);
    m_vertices.create();
    m_vertices.bind();
    m_vertices.allocate(data, size);
    m_program.enableAttributeArray(0);
    m_program.setAttributeBuffer(0, GL_FLOAT, 0, 3);
    \endcode

    and draw with it bound again:

    \code
    QOpenGLVertexArrayObject::Binder binder(&m_vao);
    glDrawArrays(GL_TRIANGLES, 0, m_count);
    \endcode

    Vertex array objects are part of OpenGL ES 3.0 and desktop OpenGL 3.0. On
    OpenGL ES 2.0 they need \c GL_OES_vertex_array_object, and on older
    desktop OpenGL \c GL_ARB_vertex_array_object. A core profile draws only
    with one bound.

    \differences
    \c GL_APPLE_vertex_array_object is not used. There is no
    \c QOpenGLVertexArrayObject(QOpenGLVertexArrayObjectPrivate &, QObject *)
    constructor.
    \enddifferences

    \sa QOpenGLBuffer
*/

/*!
    \class QOpenGLVertexArrayObject::Binder
    \inmodule NtOpenGL
    \since 6.0
    \brief The QOpenGLVertexArrayObject::Binder class binds a vertex array object for the life of a scope.

    The binder creates the object if it has not been, binds it, and releases
    it when the binder is destroyed.

    \differences
    None.
    \enddifferences
*/

/*!
    \fn QOpenGLVertexArrayObject::Binder::Binder(QOpenGLVertexArrayObject *v)

    Creates the vertex array object \a v if it has not been, and binds it.
*/

/*!
    \fn QOpenGLVertexArrayObject::Binder::~Binder()

    Releases the vertex array object.
*/

/*!
    \fn void QOpenGLVertexArrayObject::Binder::release()

    Releases the vertex array object before the binder is destroyed.
*/

/*!
    \fn void QOpenGLVertexArrayObject::Binder::rebind()

    Binds the vertex array object again.
*/

/*!
    Constructs a vertex array object with the given \a parent. Call create()
    to create the OpenGL object.
*/
QOpenGLVertexArrayObject::QOpenGLVertexArrayObject(QObject *parent) : QObject(parent), m_d(std::make_unique<Data>()) {}

/*!
    Destroys the vertex array object and the OpenGL object.
*/
QOpenGLVertexArrayObject::~QOpenGLVertexArrayObject() { destroy(); }

/*!
    Creates the OpenGL object in the current context, and returns \c true if
    it could. An object that exists already is kept.

    \sa isCreated(), bind(), destroy()
*/
bool QOpenGLVertexArrayObject::create() {
    if (m_d->id)
        return true;
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context) {
        qWarning("QOpenGLVertexArrayObject::create(): no context is current");
        return false;
    }
    const bool three = context->format().majorVersion() >= 3;
    if (three || (!context->isOpenGLES() && context->hasExtension("GL_ARB_vertex_array_object"))) {
        m_d->gen = resolve<GenVertexArrays>(context, "glGenVertexArrays");
        m_d->del = resolve<DeleteVertexArrays>(context, "glDeleteVertexArrays");
        m_d->bindArray = resolve<BindVertexArray>(context, "glBindVertexArray");
    } else if (context->isOpenGLES() && context->hasExtension("GL_OES_vertex_array_object")) {
        m_d->gen = resolve<GenVertexArrays>(context, "glGenVertexArraysOES");
        m_d->del = resolve<DeleteVertexArrays>(context, "glDeleteVertexArraysOES");
        m_d->bindArray = resolve<BindVertexArray>(context, "glBindVertexArrayOES");
    }
    if (!m_d->gen || !m_d->del || !m_d->bindArray) {
        m_d->gen = nullptr;
        m_d->del = nullptr;
        m_d->bindArray = nullptr;
        return false;
    }
    m_d->gen(1, &m_d->id);
    if (!m_d->id)
        return false;
    m_d->context = context;
    return true;
}

/*!
    Deletes the OpenGL object. If its context is not current, it is made
    current on an offscreen surface for the time.
*/
void QOpenGLVertexArrayObject::destroy() {
    if (!m_d->id)
        return;
    const GLuint name = m_d->id;
    const DeleteVertexArrays del = m_d->del;
    if (m_d->context &&
        !QtOpenGLPrivate::runInContext(m_d->context, false, [name, del](QOpenGLContext *) { del(1, &name); }))
        qWarning("QOpenGLVertexArrayObject::destroy(): the context of the object cannot be made current, so it leaks");
    m_d->id = 0;
    m_d->context = nullptr;
}

/*!
    Returns \c true if the OpenGL object has been created.
*/
bool QOpenGLVertexArrayObject::isCreated() const { return m_d->id != 0; }

/*!
    Returns the name of the OpenGL object, or 0 before create().
*/
GLuint QOpenGLVertexArrayObject::objectId() const { return m_d->id; }

/*!
    Binds the object in its context, which must be current. Vertex attribute
    and index buffer setup is then stored in it.

    \sa release()
*/
void QOpenGLVertexArrayObject::bind() {
    if (m_d->id && m_d->usableContext())
        m_d->bindArray(m_d->id);
}

/*!
    Unbinds the object, binding vertex array object 0.
*/
void QOpenGLVertexArrayObject::release() {
    if (m_d->id && m_d->usableContext())
        m_d->bindArray(0);
}

#include "moc_qopenglvertexarrayobject.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
