// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtGui/qcolor.h>
#include <QtGui/qgenericmatrix.h>
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qopengl.h>
#include <QtGui/qtransform.h>
#include <QtGui/qvectornd.h>

class QOpenGLContext;
class QOpenGLFunctions;

class QOpenGLShader : public QObject {
    Q_OBJECT
public:
    enum ShaderTypeBit {
        Vertex = 0x0001,
        Fragment = 0x0002,
        Geometry = 0x0004,
        TessellationControl = 0x0008,
        TessellationEvaluation = 0x0010,
        Compute = 0x0020
    };
    Q_DECLARE_FLAGS(ShaderType, ShaderTypeBit)

    explicit QOpenGLShader(QOpenGLShader::ShaderType type, QObject *parent = nullptr);
    ~QOpenGLShader() override;

    QOpenGLShader::ShaderType shaderType() const;
    bool compileSourceCode(const char *source);
    bool compileSourceCode(const QByteArray &source);
    bool compileSourceCode(const QString &source);
    bool compileSourceFile(const QString &fileName);
    QByteArray sourceCode() const;
    bool isCompiled() const;
    QString log() const;
    GLuint shaderId() const;

    static bool hasOpenGLShaders(ShaderType type, QOpenGLContext *context = nullptr);

private:
    ShaderType m_type;
    QOpenGLFunctions *m_gl = nullptr;
    GLuint m_id = 0;
    bool m_compiled = false;
    QByteArray m_source;
    QString m_log;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QOpenGLShader::ShaderType)

class QOpenGLShaderProgram : public QObject {
    Q_OBJECT
public:
    explicit QOpenGLShaderProgram(QObject *parent = nullptr);
    ~QOpenGLShaderProgram() override;

    bool addShader(QOpenGLShader *shader);
    void removeShader(QOpenGLShader *shader);
    QList<QOpenGLShader *> shaders() const;
    bool addShaderFromSourceCode(QOpenGLShader::ShaderType type, const char *source);
    bool addShaderFromSourceCode(QOpenGLShader::ShaderType type, const QByteArray &source);
    bool addShaderFromSourceCode(QOpenGLShader::ShaderType type, const QString &source);
    bool addShaderFromSourceFile(QOpenGLShader::ShaderType type, const QString &fileName);
    void removeAllShaders();

    bool create();
    virtual bool link();
    bool isLinked() const;
    QString log() const;
    bool bind();
    void release();
    GLuint programId() const;

    void bindAttributeLocation(const char *name, int location);
    /*! \overload */
    void bindAttributeLocation(const QByteArray &name, int location) {
        bindAttributeLocation(name.constData(), location);
    }
    /*! \overload */
    void bindAttributeLocation(const QString &name, int location) { bindAttributeLocation(name.toLatin1(), location); }
    int attributeLocation(const char *name) const;
    /*! \overload */
    int attributeLocation(const QByteArray &name) const { return attributeLocation(name.constData()); }
    /*! \overload */
    int attributeLocation(const QString &name) const { return attributeLocation(name.toLatin1()); }
    int uniformLocation(const char *name) const;
    /*! \overload */
    int uniformLocation(const QByteArray &name) const { return uniformLocation(name.constData()); }
    /*! \overload */
    int uniformLocation(const QString &name) const { return uniformLocation(name.toLatin1()); }

    void enableAttributeArray(int location);
    /*! \overload Enables the array of the attribute called \a name. */
    void enableAttributeArray(const char *name) { enableAttributeArray(attributeLocation(name)); }
    void disableAttributeArray(int location);
    /*! \overload Disables the array of the attribute called \a name. */
    void disableAttributeArray(const char *name) { disableAttributeArray(attributeLocation(name)); }
    void setAttributeBuffer(int location, GLenum type, int offset, int tupleSize, int stride = 0);
    /*! \overload Sets the buffer layout of the attribute called \a name; see the other overload for \a type,
        \a offset, \a tupleSize and \a stride. */
    void setAttributeBuffer(const char *name, GLenum type, int offset, int tupleSize, int stride = 0) {
        setAttributeBuffer(attributeLocation(name), type, offset, tupleSize, stride);
    }

    void setUniformValue(int location, GLfloat value);
    void setUniformValue(int location, GLint value);
    void setUniformValue(int location, GLuint value);
    void setUniformValue(int location, GLfloat x, GLfloat y);
    void setUniformValue(int location, GLfloat x, GLfloat y, GLfloat z);
    void setUniformValue(int location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
    void setUniformValue(int location, const QVector2D &value);
    void setUniformValue(int location, const QVector3D &value);
    void setUniformValue(int location, const QVector4D &value);
    void setUniformValue(int location, const QColor &color);
    void setUniformValue(int location, const QMatrix3x3 &value);
    void setUniformValue(int location, const QMatrix4x4 &value);
    void setUniformValue(int location, const QTransform &value);
    /*! \overload Sets the uniform called \a name to \a value. */
    template <typename V> void setUniformValue(const char *name, const V &value) {
        setUniformValue(uniformLocation(name), value);
    }

    static bool hasOpenGLShaderPrograms(QOpenGLContext *context = nullptr);

private:
    QOpenGLFunctions *functions();
    QOpenGLFunctions *m_gl = nullptr;
    GLuint m_id = 0;
    bool m_linked = false;
    QList<QOpenGLShader *> m_shaders;
    QList<QOpenGLShader *> m_owned;
    QString m_log;
};
