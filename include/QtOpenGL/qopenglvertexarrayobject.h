// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtGui/qopengl.h>

#include <memory>

class QOpenGLVertexArrayObject : public QObject {
    Q_OBJECT
public:
    explicit QOpenGLVertexArrayObject(QObject *parent = nullptr);
    ~QOpenGLVertexArrayObject() override;

    bool create();
    void destroy();
    bool isCreated() const;
    GLuint objectId() const;
    void bind();
    void release();

    class Binder {
    public:
        explicit Binder(QOpenGLVertexArrayObject *v) : m_vao(v) {
            if (m_vao->isCreated() || m_vao->create())
                m_vao->bind();
        }
        ~Binder() { release(); }
        void release() { m_vao->release(); }
        void rebind() { m_vao->bind(); }

    private:
        Q_DISABLE_COPY(Binder)
        QOpenGLVertexArrayObject *m_vao;
    };

private:
    Q_DISABLE_COPY(QOpenGLVertexArrayObject)
    struct Data;
    std::unique_ptr<Data> m_d;
};
