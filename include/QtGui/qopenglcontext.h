// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qset.h>
#include <QtGui/qopengl.h>
#include <QtGui/qsurfaceformat.h>

#include <memory>

class QOpenGLFunctions;
class QScreen;
class QSurface;

namespace QtPrivate {
struct GuiAccess;
}

class QOpenGLContext : public QObject {
    Q_OBJECT
public:
    enum OpenGLModuleType { LibGL, LibGLES };

    explicit QOpenGLContext(QObject *parent = nullptr);
    ~QOpenGLContext() override;

    void setFormat(const QSurfaceFormat &format);
    void setShareContext(QOpenGLContext *shareContext);
    void setScreen(QScreen *screen);

    bool create();
    bool isValid() const;

    QSurfaceFormat format() const;
    QOpenGLContext *shareContext() const;
    QScreen *screen() const;

    GLuint defaultFramebufferObject() const;

    bool makeCurrent(QSurface *surface);
    void doneCurrent();
    void swapBuffers(QSurface *surface);
    QFunctionPointer getProcAddress(const QByteArray &procName) const;
    QFunctionPointer getProcAddress(const char *procName) const;

    QSurface *surface() const;

    static QOpenGLContext *currentContext();
    static bool areSharing(QOpenGLContext *first, QOpenGLContext *second);

    QOpenGLFunctions *functions() const;
    QSet<QByteArray> extensions() const;
    bool hasExtension(const QByteArray &extension) const;
    bool isOpenGLES() const;

    static OpenGLModuleType openGLModuleType();
    static bool supportsThreadedOpenGL();

signals:
    /*!
        This signal is emitted when the context is about to be destroyed. Make
        it current in a slot to delete the OpenGL objects it has made.
    */
    void aboutToBeDestroyed() { emitSignal(&QOpenGLContext::aboutToBeDestroyed); }

private:
    friend struct QtPrivate::GuiAccess;
    struct Data;
    std::unique_ptr<Data> m_d;
};
