// MIT License - see LICENSE
#include <rhi/qshader.h>

#include <QtCore/qdatastream.h>
#include <QtCore/qiodevice.h>

#include <algorithm>

/*!
    \class QShaderVersion
    \brief The QShaderVersion class describes the version of the language that a shader is written in.

    For GLSL it is the number of the language, such as 450, or 300 with the GlslEs flag. For SPIR-V it is the version
    of the binary format, such as 100 for 1.0.

    \sa QShaderKey, QShader
*/

/*!
    \enum QShaderVersion::Flag

    Details of the version.

    \value GlslEs The shader is OpenGL ES GLSL.
*/

/*!
    \fn QShaderVersion::QShaderVersion()

    Constructs version 100 without flags.
*/

/*!
    \fn QShaderVersion::QShaderVersion(int v, Flags f)

    Constructs the version \a v with the flags \a f.
*/

/*!
    \fn int QShaderVersion::version() const

    Returns the version number.
*/

/*!
    \fn void QShaderVersion::setVersion(int v)

    Sets the version number to \a v.
*/

/*!
    \fn QShaderVersion::Flags QShaderVersion::flags() const

    Returns the flags.
*/

/*!
    \fn void QShaderVersion::setFlags(Flags f)

    Sets the flags to \a f.
*/

/*!
    \class QShaderCode
    \brief The QShaderCode class holds the code of one variant of a shader and the name of its entry point.

    \sa QShader
*/

/*!
    \fn QShaderCode::QShaderCode()

    Constructs an empty code with the entry point \c main.
*/

/*!
    \fn QShaderCode::QShaderCode(const QByteArray &code, const QByteArray &entryPoint)

    Constructs a code from \a code, with the given \a entryPoint.
*/

/*!
    \fn QByteArray QShaderCode::shader() const

    Returns the code: SPIR-V binary, or GLSL text.
*/

/*!
    \fn void QShaderCode::setShader(const QByteArray &code)

    Sets the code to \a code.
*/

/*!
    \fn QByteArray QShaderCode::entryPoint() const

    Returns the name of the entry point.
*/

/*!
    \fn void QShaderCode::setEntryPoint(const QByteArray &entryPoint)

    Sets the name of the entry point to \a entryPoint.
*/

/*!
    \class QShaderKey
    \brief The QShaderKey class identifies one variant of a shader: its source form, its version and its variant.

    \sa QShader
*/

/*!
    \fn QShaderKey::QShaderKey()

    Constructs a key for standard SPIR-V of version 100.
*/

/*!
    \fn QShaderKey::QShaderKey(QShader::Source s, const QShaderVersion &v, QShader::Variant var)

    Constructs a key for the source form \a s, the version \a v and the variant \a var.
*/

/*!
    \fn QShader::Source QShaderKey::source() const

    Returns the source form.
*/

/*!
    \fn void QShaderKey::setSource(QShader::Source s)

    Sets the source form to \a s.
*/

/*!
    \fn QShaderVersion QShaderKey::sourceVersion() const

    Returns the version.
*/

/*!
    \fn void QShaderKey::setSourceVersion(const QShaderVersion &v)

    Sets the version to \a v.
*/

/*!
    \fn QShader::Variant QShaderKey::sourceVariant() const

    Returns the variant.
*/

/*!
    \fn void QShaderKey::setSourceVariant(QShader::Variant v)

    Sets the variant to \a v.
*/

/*!
    \class QShader
    \brief The QShader class holds the variants of one shader of one stage.

    A shader is made once for every target that the application runs on: SPIR-V for Vulkan, GLSL for OpenGL. QRhi picks the
    variant that its backend reads. \c ntsb makes the files from GLSL:

    \code
    QShader vertex = QShader::fromSerialized(readFile(":/shaders/color.vert.nsb"));
    \endcode

    \differences
    The file format is Nt's \c .nsb, not Qt's \c .qsb, so \c ntsb must make the files. There is no QShaderDescription:
    the reflection of the inputs, the uniform blocks and the images is not stored, the application gives QRhi the
    bindings itself. Only the standard variant is used, and there are no native resource binding maps.
    \enddifferences

    \sa QRhiShaderStage, QRhiGraphicsPipeline
*/

/*!
    \enum QShader::Stage

    The stage of the pipeline that a shader is for.

    \value VertexStage The vertex shader.
    \value TessellationControlStage The tessellation control shader.
    \value TessellationEvaluationStage The tessellation evaluation shader.
    \value GeometryStage The geometry shader.
    \value FragmentStage The fragment shader.
    \value ComputeStage The compute shader.
*/

/*!
    \enum QShader::Source

    The form of the code of a variant.

    \value SpirvShader SPIR-V.
    \value GlslShader GLSL text.
    \value HlslShader HLSL text; not used by Nt's backends.
    \value DxbcShader DXBC; not used by Nt's backends.
    \value MslShader Metal source; not used by Nt's backends.
    \value DxilShader DXIL; not used by Nt's backends.
    \value MetalLibShader Metal library; not used by Nt's backends.
    \value WgslShader WGSL; not used by Nt's backends.
*/

/*!
    \enum QShader::Variant

    A variant of the code for special uses. Nt's backends read only StandardShader.

    \value StandardShader The code as written.
    \value BatchableVertexShader A vertex shader that can be batched.
    \value UInt16IndexedVertexAsComputeShader A vertex shader as a compute shader for 16-bit indexes.
    \value UInt32IndexedVertexAsComputeShader A vertex shader as a compute shader for 32-bit indexes.
    \value NonIndexedVertexAsComputeShader A vertex shader as a compute shader without indexes.
*/

struct QShader::Private {
    Stage stage = VertexStage;
    QList<QShaderKey> keys;
    QList<QShaderCode> codes;
};

/*!
    Constructs an invalid shader that has no variants.
*/
QShader::QShader() : d(std::make_shared<Private>()) {}

/*!
    Constructs a shader that shares the variants of \a other.
*/
QShader::QShader(const QShader &other) = default;

/*!
    Makes this shader share the variants of \a other.
*/
QShader &QShader::operator=(const QShader &other) = default;

/*!
    Destroys the shader.
*/
QShader::~QShader() = default;

void QShader::detach() {
    if (d.use_count() > 1)
        d = std::make_shared<Private>(*d);
}

/*!
    Returns \c true if the shader has at least one variant.
*/
bool QShader::isValid() const { return !d->keys.isEmpty(); }

/*!
    Returns the stage that the shader is for.
*/
QShader::Stage QShader::stage() const { return d->stage; }

/*!
    Sets the stage that the shader is for to \a stage.
*/
void QShader::setStage(Stage stage) {
    detach();
    d->stage = stage;
}

/*!
    Returns the keys of the variants, in order.
*/
QList<QShaderKey> QShader::availableShaders() const {
    QList<QShaderKey> keys = d->keys;
    std::sort(keys.begin(), keys.end());
    return keys;
}

/*!
    Returns the code of the variant \a key, or an empty code if there is none.
*/
QShaderCode QShader::shader(const QShaderKey &key) const {
    const qsizetype index = d->keys.indexOf(key);
    return index >= 0 ? d->codes.at(index) : QShaderCode();
}

/*!
    Adds the variant \a key with the code \a shader, or replaces it.
*/
void QShader::setShader(const QShaderKey &key, const QShaderCode &shader) {
    detach();
    const qsizetype index = d->keys.indexOf(key);
    if (index >= 0) {
        d->codes[index] = shader;
    } else {
        d->keys.append(key);
        d->codes.append(shader);
    }
}

/*!
    Removes the variant \a key.
*/
void QShader::removeShader(const QShaderKey &key) {
    detach();
    const qsizetype index = d->keys.indexOf(key);
    if (index >= 0) {
        d->keys.removeAt(index);
        d->codes.removeAt(index);
    }
}

namespace {
constexpr quint32 kMagic = 0x3142534e; // "NSB1"
constexpr quint32 kFormat = 1;
} // namespace

/*!
    Returns the shader in the \c .nsb format: a magic number, the stage and, for each variant, its key and code.
*/
QByteArray QShader::serialized() const {
    QByteArray bytes;
    QDataStream out(&bytes, QIODevice::WriteOnly);
    out.setByteOrder(QDataStream::LittleEndian);
    out << kMagic << kFormat << qint32(d->stage) << qint32(d->keys.size());
    for (qsizetype i = 0; i < d->keys.size(); ++i) {
        const QShaderKey &key = d->keys.at(i);
        out << qint32(key.source()) << qint32(key.sourceVersion().version())
            << qint32(key.sourceVersion().flags().toInt()) << qint32(key.sourceVariant()) << d->codes.at(i).entryPoint()
            << d->codes.at(i).shader();
    }
    return bytes;
}

/*!
    Reads a shader from \a data in the \c .nsb format. Returns an invalid shader if \a data is not valid, for example if
    it is a Qt \c .qsb file.
*/
QShader QShader::fromSerialized(const QByteArray &data) {
    QShader shader;
    QDataStream in(data);
    in.setByteOrder(QDataStream::LittleEndian);
    quint32 magic = 0, format = 0;
    qint32 stage = 0, count = 0;
    in >> magic >> format >> stage >> count;
    if (in.status() != QDataStream::Ok || magic != kMagic || format != kFormat || stage < VertexStage ||
        stage > ComputeStage || count < 0 || count > 1024)
        return shader;
    QShader result;
    result.d->stage = static_cast<Stage>(stage);
    for (qint32 i = 0; i < count; ++i) {
        qint32 source = 0, version = 0, flags = 0, variant = 0;
        QByteArray entry, code;
        in >> source >> version >> flags >> variant >> entry >> code;
        if (in.status() != QDataStream::Ok || source < SpirvShader || source > WgslShader || variant < StandardShader ||
            variant > NonIndexedVertexAsComputeShader)
            return shader;
        result.d->keys.append(QShaderKey(static_cast<Source>(source),
                                         QShaderVersion(version, QShaderVersion::Flags(QShaderVersion::Flag(flags))),
                                         static_cast<Variant>(variant)));
        result.d->codes.append(QShaderCode(code, entry));
    }
    return result;
}

bool operator==(const QShader &a, const QShader &b) noexcept {
    if (a.d == b.d)
        return true;
    if (a.d->stage != b.d->stage || a.d->keys.size() != b.d->keys.size())
        return false;
    for (qsizetype i = 0; i < a.d->keys.size(); ++i) {
        const qsizetype j = b.d->keys.indexOf(a.d->keys.at(i));
        if (j < 0 || a.d->codes.at(i).shader() != b.d->codes.at(j).shader() ||
            a.d->codes.at(i).entryPoint() != b.d->codes.at(j).entryPoint())
            return false;
    }
    return true;
}
