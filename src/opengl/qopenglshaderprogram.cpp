// MIT License - see LICENSE
#include <QtOpenGL/qopenglshaderprogram.h>

#include <QtCore/qalgorithms.h>
#include <QtCore/qdebug.h>
#include <QtCore/qfile.h>
#include <QtGui/qopenglcontext.h>
#include <QtGui/qopenglfunctions.h>

#include <vector>

namespace {

QOpenGLFunctions *currentFunctions() {
    QOpenGLContext *context = QOpenGLContext::currentContext();
    return context ? context->functions() : nullptr;
}

// Desktop OpenGL has no precision qualifiers before GLSL 1.30; defining them away lets shaders written for
// OpenGL ES 2 compile there too, as Qt does.
QByteArray prepared(const QByteArray &source) {
    QOpenGLContext *context = QOpenGLContext::currentContext();
    if (!context || context->isOpenGLES())
        return source;
    const QByteArray defines("#define lowp\n#define mediump\n#define highp\n");
    if (source.trimmed().startsWith("#version")) {
        const qsizetype eol = source.indexOf('\n');
        if (eol < 0)
            return source;
        return source.left(eol + 1) + defines + "#line 2\n" + source.mid(eol + 1);
    }
    return defines + "#line 1\n" + source;
}

GLenum glShaderType(QOpenGLShader::ShaderType type) {
    if (type & QOpenGLShader::Vertex)
        return GL_VERTEX_SHADER;
    if (type & QOpenGLShader::Fragment)
        return GL_FRAGMENT_SHADER;
    return 0;
}

} // namespace

/*!
    \class QOpenGLShader
    \brief The QOpenGLShader class compiles one GLSL shader.

    Make it while the OpenGL context it belongs to is current. Most code adds shaders with
    QOpenGLShaderProgram::addShaderFromSourceCode() instead.

    \differences
    Only vertex and fragment shaders are compiled; the other types fail. There is no binary shader support.
    \enddifferences

    \sa QOpenGLShaderProgram
*/

/*!
    \enum QOpenGLShader::ShaderTypeBit

    The stage a shader runs at.

    \value Vertex A vertex shader.
    \value Fragment A fragment shader.
    \value Geometry A geometry shader; not supported by Nt.
    \value TessellationControl A tessellation control shader; not supported by Nt.
    \value TessellationEvaluation A tessellation evaluation shader; not supported by Nt.
    \value Compute A compute shader; not supported by Nt.
*/

/*!
    Constructs a shader of \a type in the current context, with the given \a parent.
*/
QOpenGLShader::QOpenGLShader(QOpenGLShader::ShaderType type, QObject *parent)
    : QObject(parent), m_type(type), m_gl(currentFunctions()) {
    const GLenum glType = glShaderType(type);
    if (m_gl && glType)
        m_id = m_gl->glCreateShader(glType);
}

/*!
    Destroys the shader. Its context must be current.
*/
QOpenGLShader::~QOpenGLShader() {
    if (m_gl && m_id)
        m_gl->glDeleteShader(m_id);
}

/*!
    Returns the type of the shader.
*/
QOpenGLShader::ShaderType QOpenGLShader::shaderType() const { return m_type; }

/*!
    Compiles \a source. Returns \c true on success; log() says why it failed otherwise.
*/
bool QOpenGLShader::compileSourceCode(const char *source) { return compileSourceCode(QByteArray(source)); }

/*!
    \overload
*/
bool QOpenGLShader::compileSourceCode(const QByteArray &source) {
    m_source = source;
    m_compiled = false;
    m_log.clear();
    if (!m_gl || !m_id) {
        m_log = QStringLiteral("no OpenGL context, or an unsupported shader type");
        return false;
    }
    const QByteArray code = prepared(source);
    const char *text = code.constData();
    const GLint length = static_cast<GLint>(code.size());
    m_gl->glShaderSource(m_id, 1, &text, &length);
    m_gl->glCompileShader(m_id);
    GLint status = 0;
    m_gl->glGetShaderiv(m_id, GL_COMPILE_STATUS, &status);
    GLint logLength = 0;
    m_gl->glGetShaderiv(m_id, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1) {
        std::vector<char> buffer(static_cast<std::size_t>(logLength));
        m_gl->glGetShaderInfoLog(m_id, logLength, nullptr, buffer.data());
        m_log = QString::fromUtf8(buffer.data());
    }
    m_compiled = status != 0;
    if (!m_compiled)
        qWarning("QOpenGLShader::compile: %s", qPrintable(m_log));
    return m_compiled;
}

/*!
    \overload
*/
bool QOpenGLShader::compileSourceCode(const QString &source) { return compileSourceCode(source.toUtf8()); }

/*!
    Compiles the source in the file \a fileName, which may be a resource.
*/
bool QOpenGLShader::compileSourceFile(const QString &fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        m_log = QStringLiteral("cannot open ") + fileName;
        return false;
    }
    return compileSourceCode(file.readAll());
}

/*!
    Returns the source that was last compiled.
*/
QByteArray QOpenGLShader::sourceCode() const { return m_source; }

/*!
    Returns \c true if the shader compiled.
*/
bool QOpenGLShader::isCompiled() const { return m_compiled; }

/*!
    Returns the log of the last compilation.
*/
QString QOpenGLShader::log() const { return m_log; }

/*!
    Returns the OpenGL name of the shader, or 0.
*/
GLuint QOpenGLShader::shaderId() const { return m_id; }

/*!
    Returns \c true if \a type can be compiled in \a context (the current one if \c nullptr): vertex and fragment
    shaders, when there is a context.
*/
bool QOpenGLShader::hasOpenGLShaders(ShaderType type, QOpenGLContext *context) {
    if (!context)
        context = QOpenGLContext::currentContext();
    return context && (type & ~ShaderType(Vertex | Fragment)) == ShaderType();
}

/*!
    \class QOpenGLShaderProgram
    \brief The QOpenGLShaderProgram class links GLSL shaders into a program and sets its inputs.

    Add shaders, link(), then bind() the program and set its attributes and uniforms while drawing:

    \code
    QOpenGLShaderProgram program;
    program.addShaderFromSourceCode(QOpenGLShader::Vertex, vertexSource);
    program.addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentSource);
    program.link();
    program.bind();
    program.setUniformValue("uMvp", projection * view * model);
    \endcode

    On desktop OpenGL the precision qualifiers of OpenGL ES (\c lowp, \c mediump, \c highp) are defined away, so
    the same shaders work on both.

    \differences
    Only vertex and fragment shaders, float, integer, vector, color and matrix uniforms, and buffer attributes are
    supported: there are no uniform or attribute arrays set from client memory, no geometry or tessellation
    settings and no program binaries. The context that is current when the program is first used is the one it
    belongs to.
    \enddifferences

    \sa QOpenGLShader
*/

/*!
    Constructs an empty program with the given \a parent.
*/
QOpenGLShaderProgram::QOpenGLShaderProgram(QObject *parent) : QObject(parent) {}

/*!
    Destroys the program and the shaders it made. Its context must be current.
*/
QOpenGLShaderProgram::~QOpenGLShaderProgram() {
    if (m_gl && m_id)
        m_gl->glDeleteProgram(m_id);
    qDeleteAll(m_owned);
}

QOpenGLFunctions *QOpenGLShaderProgram::functions() {
    if (!m_gl)
        m_gl = currentFunctions();
    return m_gl;
}

/*!
    Creates the OpenGL program in the current context. Returns \c false if there is no context. The other functions
    call it when needed.
*/
bool QOpenGLShaderProgram::create() {
    if (m_id)
        return true;
    if (!functions())
        return false;
    m_id = m_gl->glCreateProgram();
    return m_id != 0;
}

/*!
    Adds the compiled \a shader to the program. Returns \c false if it did not compile.
*/
bool QOpenGLShaderProgram::addShader(QOpenGLShader *shader) {
    if (!shader || !shader->isCompiled() || !create())
        return false;
    if (m_shaders.contains(shader))
        return true;
    m_gl->glAttachShader(m_id, shader->shaderId());
    m_shaders.append(shader);
    m_linked = false;
    return true;
}

/*!
    Takes \a shader out of the program.
*/
void QOpenGLShaderProgram::removeShader(QOpenGLShader *shader) {
    if (!shader || !m_shaders.contains(shader))
        return;
    m_shaders.removeAll(shader);
    if (m_owned.removeAll(shader))
        delete shader;
    m_linked = false;
}

/*!
    Returns the shaders of the program.
*/
QList<QOpenGLShader *> QOpenGLShaderProgram::shaders() const { return m_shaders; }

/*!
    Compiles \a source as a shader of \a type and adds it. Returns \c false, with the reason in log(), if it does
    not compile.
*/
bool QOpenGLShaderProgram::addShaderFromSourceCode(QOpenGLShader::ShaderType type, const char *source) {
    return addShaderFromSourceCode(type, QByteArray(source));
}

/*!
    \overload
*/
bool QOpenGLShaderProgram::addShaderFromSourceCode(QOpenGLShader::ShaderType type, const QByteArray &source) {
    if (!create())
        return false;
    auto *shader = new QOpenGLShader(type, this);
    if (!shader->compileSourceCode(source)) {
        m_log = shader->log();
        delete shader;
        return false;
    }
    m_owned.append(shader);
    return addShader(shader);
}

/*!
    \overload
*/
bool QOpenGLShaderProgram::addShaderFromSourceCode(QOpenGLShader::ShaderType type, const QString &source) {
    return addShaderFromSourceCode(type, source.toUtf8());
}

/*!
    Compiles the file \a fileName as a shader of \a type and adds it.
*/
bool QOpenGLShaderProgram::addShaderFromSourceFile(QOpenGLShader::ShaderType type, const QString &fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        m_log = QStringLiteral("cannot open ") + fileName;
        return false;
    }
    return addShaderFromSourceCode(type, file.readAll());
}

/*!
    Takes every shader out of the program and deletes those it made.
*/
void QOpenGLShaderProgram::removeAllShaders() {
    m_shaders.clear();
    qDeleteAll(m_owned);
    m_owned.clear();
    m_linked = false;
}

/*!
    Links the shaders. Returns \c true on success; log() says why it failed otherwise.
*/
bool QOpenGLShaderProgram::link() {
    if (!create())
        return false;
    m_gl->glLinkProgram(m_id);
    GLint status = 0;
    m_gl->glGetProgramiv(m_id, GL_LINK_STATUS, &status);
    GLint logLength = 0;
    m_gl->glGetProgramiv(m_id, GL_INFO_LOG_LENGTH, &logLength);
    m_log.clear();
    if (logLength > 1) {
        std::vector<char> buffer(static_cast<std::size_t>(logLength));
        m_gl->glGetProgramInfoLog(m_id, logLength, nullptr, buffer.data());
        m_log = QString::fromUtf8(buffer.data());
    }
    m_linked = status != 0;
    if (!m_linked)
        qWarning("QOpenGLShaderProgram::link: %s", qPrintable(m_log));
    return m_linked;
}

/*!
    Returns \c true if the program is linked.
*/
bool QOpenGLShaderProgram::isLinked() const { return m_linked; }

/*!
    Returns the log of the last compilation or link.
*/
QString QOpenGLShaderProgram::log() const { return m_log; }

/*!
    Makes the program the current one, linking it first if needed. Returns \c false if it is not linked.
*/
bool QOpenGLShaderProgram::bind() {
    if (!m_linked && !link())
        return false;
    m_gl->glUseProgram(m_id);
    return true;
}

/*!
    Makes no program current.
*/
void QOpenGLShaderProgram::release() {
    if (m_gl)
        m_gl->glUseProgram(0);
}

/*!
    Returns the OpenGL name of the program, or 0.
*/
GLuint QOpenGLShaderProgram::programId() const { return m_id; }

/*!
    Gives the attribute called \a name the \a location; takes effect at the next link().
*/
void QOpenGLShaderProgram::bindAttributeLocation(const char *name, int location) {
    if (create())
        m_gl->glBindAttribLocation(m_id, static_cast<GLuint>(location), name);
}

/*!
    Returns the location of the attribute called \a name, or -1.
*/
int QOpenGLShaderProgram::attributeLocation(const char *name) const {
    return m_linked && m_gl ? m_gl->glGetAttribLocation(m_id, name) : -1;
}

/*!
    Returns the location of the uniform called \a name, or -1.
*/
int QOpenGLShaderProgram::uniformLocation(const char *name) const {
    return m_linked && m_gl ? m_gl->glGetUniformLocation(m_id, name) : -1;
}

/*!
    Enables the vertex array of the attribute at \a location.
*/
void QOpenGLShaderProgram::enableAttributeArray(int location) {
    if (location >= 0 && functions())
        m_gl->glEnableVertexAttribArray(static_cast<GLuint>(location));
}

/*!
    Disables the vertex array of the attribute at \a location.
*/
void QOpenGLShaderProgram::disableAttributeArray(int location) {
    if (location >= 0 && functions())
        m_gl->glDisableVertexAttribArray(static_cast<GLuint>(location));
}

/*!
    Reads the attribute at \a location from the bound vertex buffer: \a tupleSize values of \a type, starting
    \a offset bytes in, \a stride bytes apart (0 for packed).
*/
void QOpenGLShaderProgram::setAttributeBuffer(int location, GLenum type, int offset, int tupleSize, int stride) {
    if (location < 0 || !functions())
        return;
    // With a vertex buffer bound, OpenGL takes the byte offset into it in place of a pointer.
    const auto *start = reinterpret_cast<const void *>( // NOLINT(performance-no-int-to-ptr): an offset, as GL requires
        static_cast<qintptr>(offset));
    m_gl->glVertexAttribPointer(static_cast<GLuint>(location), tupleSize, type, GL_FALSE, stride, start);
}

/*!
    Sets the uniform at \a location to \a value.
*/
void QOpenGLShaderProgram::setUniformValue(int location, GLfloat value) {
    if (location >= 0 && functions())
        m_gl->glUniform1f(location, value);
}

/*!
    \overload
*/
void QOpenGLShaderProgram::setUniformValue(int location, GLint value) {
    if (location >= 0 && functions())
        m_gl->glUniform1i(location, value);
}

/*!
    \overload
*/
void QOpenGLShaderProgram::setUniformValue(int location, GLuint value) {
    setUniformValue(location, static_cast<GLint>(value));
}

/*!
    \overload

    Sets the vec2 uniform at \a location to (\a x, \a y).
*/
void QOpenGLShaderProgram::setUniformValue(int location, GLfloat x, GLfloat y) {
    if (location >= 0 && functions())
        m_gl->glUniform2f(location, x, y);
}

/*!
    \overload

    Sets the vec3 uniform at \a location to (\a x, \a y, \a z).
*/
void QOpenGLShaderProgram::setUniformValue(int location, GLfloat x, GLfloat y, GLfloat z) {
    if (location >= 0 && functions())
        m_gl->glUniform3f(location, x, y, z);
}

/*!
    \overload

    Sets the vec4 uniform at \a location to (\a x, \a y, \a z, \a w).
*/
void QOpenGLShaderProgram::setUniformValue(int location, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    if (location >= 0 && functions())
        m_gl->glUniform4f(location, x, y, z, w);
}

/*!
    \overload
*/
void QOpenGLShaderProgram::setUniformValue(int location, const QVector2D &value) {
    setUniformValue(location, value.x(), value.y());
}

/*!
    \overload
*/
void QOpenGLShaderProgram::setUniformValue(int location, const QVector3D &value) {
    setUniformValue(location, value.x(), value.y(), value.z());
}

/*!
    \overload
*/
void QOpenGLShaderProgram::setUniformValue(int location, const QVector4D &value) {
    setUniformValue(location, value.x(), value.y(), value.z(), value.w());
}

/*!
    \overload

    Sets the vec4 uniform at \a location to the red, green, blue and alpha of \a color, from 0 to 1.
*/
void QOpenGLShaderProgram::setUniformValue(int location, const QColor &color) {
    setUniformValue(location, static_cast<GLfloat>(color.redF()), static_cast<GLfloat>(color.greenF()),
                    static_cast<GLfloat>(color.blueF()), static_cast<GLfloat>(color.alphaF()));
}

/*!
    \overload
*/
void QOpenGLShaderProgram::setUniformValue(int location, const QMatrix3x3 &value) {
    if (location >= 0 && functions())
        m_gl->glUniformMatrix3fv(location, 1, GL_FALSE, value.constData());
}

/*!
    \overload
*/
void QOpenGLShaderProgram::setUniformValue(int location, const QMatrix4x4 &value) {
    if (location >= 0 && functions())
        m_gl->glUniformMatrix4fv(location, 1, GL_FALSE, value.constData());
}

/*!
    \overload

    Sets the mat3 uniform at \a location to \a value.
*/
void QOpenGLShaderProgram::setUniformValue(int location, const QTransform &value) {
    const float m[9] = {
        static_cast<float>(value.m11()), static_cast<float>(value.m12()), static_cast<float>(value.m13()),
        static_cast<float>(value.m21()), static_cast<float>(value.m22()), static_cast<float>(value.m23()),
        static_cast<float>(value.m31()), static_cast<float>(value.m32()), static_cast<float>(value.m33())};
    if (location >= 0 && functions())
        m_gl->glUniformMatrix3fv(location, 1, GL_FALSE, m);
}

/*!
    Returns \c true if \a context (the current one if \c nullptr) can run shader programs.
*/
bool QOpenGLShaderProgram::hasOpenGLShaderPrograms(QOpenGLContext *context) {
    return context || QOpenGLContext::currentContext();
}

#include "moc_qopenglshaderprogram.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
