// MIT License - see LICENSE
#include "qgltfdocument.h"

#include <QtCore/qdir.h>
#include <QtCore/qendian.h>
#include <QtCore/qfile.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qjsonarray.h>
#include <QtCore/qjsondocument.h>
#include <QtCore/qjsonobject.h>
#include <QtCore/qjsonvalue.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace QtGltf {

class DocumentData : public QSharedData {
public:
    bool loaded = false;
    Document::Error error = Document::NoError;
    QString errorString;
    QString generator;
    QString copyright;
    QStringList extensionsUsed;
    QList<Scene> scenes;
    int defaultScene = -1;
    QList<Node> nodes;
    QList<int> parents;
    QList<Mesh> meshes;
    QList<Material> materials;
    QList<Texture> textures;
    QList<Sampler> samplers;
    QList<Image> images;
    QList<Camera> cameras;
    QList<Light> lights;
};

namespace {

constexpr quint32 GlbMagic = 0x46546C67;     // "glTF"
constexpr quint32 GlbJsonChunk = 0x4E4F534A; // "JSON"
constexpr quint32 GlbBinChunk = 0x004E4942;  // "BIN\0"

constexpr int Byte = 5120;
constexpr int UnsignedByte = 5121;
constexpr int Short = 5122;
constexpr int UnsignedShort = 5123;
constexpr int UnsignedInt = 5125;
constexpr int Float = 5126;

// The largest element count an accessor may declare; keeps the size arithmetic far from overflow.
constexpr qint64 MaxCount = 0x7fffffff;

// The extensions the loader understands well enough that a file may require them.
const char *const SupportedRequiredExtensions[] = {
    "KHR_lights_punctual",   "KHR_materials_emissive_strength",
    "KHR_materials_ior",     "KHR_materials_transmission",
    "KHR_materials_unlit",   "KHR_materials_volume",
    "KHR_mesh_quantization", "KHR_texture_transform",
};

quint32 readU32(const char *p) { return qFromLittleEndian<quint32>(p); }

int componentSize(int type) {
    switch (type) {
    case Byte:
    case UnsignedByte:
        return 1;
    case Short:
    case UnsignedShort:
        return 2;
    case UnsignedInt:
    case Float:
        return 4;
    default:
        return 0;
    }
}

int componentCount(const QString &type) {
    if (type == QLatin1StringView("SCALAR"))
        return 1;
    if (type == QLatin1StringView("VEC2"))
        return 2;
    if (type == QLatin1StringView("VEC3"))
        return 3;
    if (type == QLatin1StringView("VEC4"))
        return 4;
    if (type == QLatin1StringView("MAT2"))
        return 4;
    if (type == QLatin1StringView("MAT3"))
        return 9;
    if (type == QLatin1StringView("MAT4"))
        return 16;
    return 0;
}

// Reads one little-endian component; normalized integers map to [0, 1] or [-1, 1] as the
// specification says.
double readComponent(const char *p, int type, bool normalized) {
    switch (type) {
    case Byte: {
        const auto v = static_cast<qint8>(*p);
        return normalized ? std::max(v / 127.0, -1.0) : double(v);
    }
    case UnsignedByte: {
        const auto v = static_cast<quint8>(*p);
        return normalized ? v / 255.0 : double(v);
    }
    case Short: {
        const auto v = qFromLittleEndian<qint16>(p);
        return normalized ? std::max(v / 32767.0, -1.0) : double(v);
    }
    case UnsignedShort: {
        const auto v = qFromLittleEndian<quint16>(p);
        return normalized ? v / 65535.0 : double(v);
    }
    case UnsignedInt:
        return double(qFromLittleEndian<quint32>(p));
    case Float: {
        const quint32 bits = qFromLittleEndian<quint32>(p);
        float v = 0;
        std::memcpy(&v, &bits, sizeof v);
        return double(v);
    }
    default:
        return 0;
    }
}

struct BufferView {
    int buffer = -1;
    qsizetype offset = 0;
    qsizetype length = 0;
    qsizetype stride = 0;
};

struct Accessor {
    int bufferView = -1;
    qsizetype offset = 0;
    int componentType = 0;
    bool normalized = false;
    qsizetype count = 0;
    int components = 0;
    qsizetype sparseCount = 0;
    int sparseIndexView = -1;
    qsizetype sparseIndexOffset = 0;
    int sparseIndexType = 0;
    int sparseValueView = -1;
    qsizetype sparseValueOffset = 0;
};

class Parser {
public:
    Parser(DocumentData &out, QString baseDirectory) : m_out(out), m_baseDirectory(std::move(baseDirectory)) {}

    bool parse(const QByteArray &data);

private:
    bool fail(Document::Error error, const QString &message) {
        m_out.error = error;
        m_out.errorString = message;
        return false;
    }
    bool dataError(const QString &what, const QString &message) {
        return fail(Document::DataError, what + QLatin1StringView(": ") + message);
    }

    bool readGlb(const QByteArray &data, QByteArray *json);
    bool readAsset(const QJsonObject &root);
    bool resolveUri(const QString &uri, const QString &what, QByteArray *data, QString *mimeType, QString *fileName);

    bool index(const QJsonObject &o, const char *key, qsizetype count, int *out, const QString &what);
    bool integer(const QJsonObject &o, const char *key, qint64 min, qint64 max, qint64 *out, const QString &what);
    bool floats(const QJsonObject &o, const char *key, int n, float *out, const QString &what);
    static float number(const QJsonObject &o, const char *key, float defaultValue) {
        return float(o.value(key).toDouble(double(defaultValue)));
    }

    bool readBuffers(const QJsonArray &array);
    bool readBufferViews(const QJsonArray &array);
    bool readAccessors(const QJsonArray &array);
    template <typename T> bool readAccessor(int accessor, QList<T> *out, const QString &what);
    bool viewData(int view, qsizetype offset, qsizetype size, const char **data, const QString &what);

    bool readSamplers(const QJsonArray &array);
    bool readImages(const QJsonArray &array);
    bool readTextures(const QJsonArray &array);
    bool readTextureInfo(const QJsonObject &o, const char *key, const char *scaleKey, TextureInfo *info,
                         const QString &what);
    bool readMaterials(const QJsonArray &array);
    bool readMeshes(const QJsonArray &array);
    bool readPrimitive(const QJsonObject &o, Primitive *primitive, bool *keep, const QString &what);
    bool readAttribute(const QJsonObject &attributes, const char *name, int minComponents, int maxComponents,
                       qsizetype vertexCount, QList<float> *values, int *components, const QString &what);
    bool readCameras(const QJsonArray &array);
    bool readLights(const QJsonObject &root);
    bool readNodes(const QJsonArray &array);
    bool readScenes(const QJsonObject &root);

    DocumentData &m_out;
    QString m_baseDirectory;
    QByteArray m_glbBin;
    bool m_hasGlbBin = false;
    QList<QByteArray> m_buffers;
    QList<BufferView> m_views;
    QList<Accessor> m_accessors;
    qsizetype m_textureCount = 0;
    qsizetype m_imageCount = 0;
    qsizetype m_samplerCount = 0;
    qsizetype m_materialCount = 0;
    qsizetype m_meshCount = 0;
    qsizetype m_cameraCount = 0;
    qsizetype m_lightCount = 0;
    qsizetype m_nodeCount = 0;
};

bool Parser::index(const QJsonObject &o, const char *key, qsizetype count, int *out, const QString &what) {
    const QJsonValue v = o.value(key);
    if (v.isUndefined())
        return true;
    const double x = v.toDouble(-1.0);
    if (!v.isDouble() || x < 0 || x >= double(count) || x != std::floor(x))
        return dataError(what, QString::fromLatin1(key) + QLatin1StringView(" is not a valid index"));
    *out = int(x);
    return true;
}

bool Parser::integer(const QJsonObject &o, const char *key, qint64 min, qint64 max, qint64 *out, const QString &what) {
    const QJsonValue v = o.value(key);
    if (v.isUndefined())
        return true;
    const double x = v.toDouble(-1.0);
    if (!v.isDouble() || x < double(min) || x > double(max) || x != std::floor(x))
        return dataError(what, QString::fromLatin1(key) + QLatin1StringView(" is out of range"));
    *out = qint64(x);
    return true;
}

bool Parser::floats(const QJsonObject &o, const char *key, int n, float *out, const QString &what) {
    const QJsonValue v = o.value(key);
    if (v.isUndefined())
        return true;
    const QJsonArray array = v.toArray();
    if (!v.isArray() || array.size() != n)
        return dataError(what, QString::fromLatin1(key) + QLatin1StringView(" must hold %1 numbers").arg(n));
    for (int i = 0; i < n; ++i) {
        const QJsonValue item = array.at(i);
        if (!item.isDouble())
            return dataError(what, QString::fromLatin1(key) + QLatin1StringView(" must hold numbers"));
        out[i] = float(item.toDouble());
    }
    return true;
}

bool Parser::readGlb(const QByteArray &data, QByteArray *json) {
    const char *p = data.constData();
    if (readU32(p + 4) != 2)
        return fail(Document::VersionError, QStringLiteral("GLB container version %1 is not 2").arg(readU32(p + 4)));
    const qsizetype length = qsizetype(readU32(p + 8));
    if (length > data.size() || length < 12)
        return fail(Document::ParseError, QStringLiteral("GLB container is truncated"));
    qsizetype pos = 12;
    bool first = true;
    while (pos + 8 <= length) {
        const qsizetype chunkLength = qsizetype(readU32(p + pos));
        const quint32 type = readU32(p + pos + 4);
        if (chunkLength > length - pos - 8)
            return fail(Document::ParseError, QStringLiteral("GLB chunk at offset %1 is truncated").arg(pos));
        if (first) {
            if (type != GlbJsonChunk)
                return fail(Document::ParseError, QStringLiteral("GLB container does not start with a JSON chunk"));
            *json = data.mid(pos + 8, chunkLength);
            first = false;
        } else if (type == GlbBinChunk && !m_hasGlbBin) {
            m_glbBin = data.mid(pos + 8, chunkLength);
            m_hasGlbBin = true;
        }
        pos += 8 + ((chunkLength + 3) & ~qsizetype(3));
    }
    if (first)
        return fail(Document::ParseError, QStringLiteral("GLB container has no JSON chunk"));
    return true;
}

bool Parser::readAsset(const QJsonObject &root) {
    const QJsonObject asset = root.value("asset").toObject();
    const QString version = asset.value("version").toString();
    if (version.isEmpty())
        return fail(Document::ParseError, QStringLiteral("asset.version is missing"));
    auto parseVersion = [](const QString &text, int *major, int *minor) {
        const QStringList parts = text.split(QLatin1Char('.'));
        bool ok1 = false;
        bool ok2 = false;
        *major = parts.value(0).toInt(&ok1);
        *minor = parts.value(1).toInt(&ok2);
        return parts.size() == 2 && ok1 && ok2;
    };
    int major = 0;
    int minor = 0;
    if (!parseVersion(version, &major, &minor) || major != 2)
        return fail(Document::VersionError, QStringLiteral("glTF version %1 is not supported").arg(version));
    const QString minVersion = asset.value("minVersion").toString();
    if (!minVersion.isEmpty()) {
        if (!parseVersion(minVersion, &major, &minor) || major != 2 || minor > 0)
            return fail(Document::VersionError,
                        QStringLiteral("glTF minimum version %1 is not supported").arg(minVersion));
    }
    m_out.generator = asset.value("generator").toString();
    m_out.copyright = asset.value("copyright").toString();

    for (const auto &v : root.value("extensionsUsed").toArray())
        m_out.extensionsUsed.append(v.toString());
    for (const auto &v : root.value("extensionsRequired").toArray()) {
        const QString name = v.toString();
        const bool supported =
            std::any_of(std::begin(SupportedRequiredExtensions), std::end(SupportedRequiredExtensions),
                        [&](const char *s) { return name == QLatin1StringView(s); });
        if (!supported)
            return fail(Document::ExtensionError, QStringLiteral("required extension %1 is not supported").arg(name));
    }
    return true;
}

bool Parser::resolveUri(const QString &uri, const QString &what, QByteArray *data, QString *mimeType,
                        QString *fileName) {
    if (uri.startsWith(QLatin1StringView("data:"))) {
        const qsizetype comma = uri.indexOf(QLatin1Char(','));
        if (comma < 0)
            return dataError(what, QStringLiteral("malformed data URI"));
        const QString header = uri.mid(5, comma - 5);
        const QByteArray payload = uri.mid(comma + 1).toLatin1();
        if (header.endsWith(QLatin1StringView(";base64"))) {
            const auto result = QByteArray::fromBase64Encoding(payload, QByteArray::AbortOnBase64DecodingErrors);
            if (!result)
                return dataError(what, QStringLiteral("malformed base64 in data URI"));
            *data = result.decoded;
        } else {
            *data = QByteArray::fromPercentEncoding(payload);
        }
        if (mimeType)
            *mimeType = header.section(QLatin1Char(';'), 0, 0);
        return true;
    }
    const QString path = QString::fromUtf8(QByteArray::fromPercentEncoding(uri.toUtf8()));
    // A scheme ("http:", or a drive letter) or an absolute path is not a relative reference.
    const qsizetype colon = path.indexOf(QLatin1Char(':'));
    const qsizetype slash = path.indexOf(QLatin1Char('/'));
    if (path.isEmpty() || QDir::isAbsolutePath(path) || (colon >= 0 && (slash < 0 || colon < slash)))
        return fail(Document::FileError, what + QStringLiteral(": URI %1 is not a relative path").arg(uri));
    *fileName = m_baseDirectory.isEmpty() ? path : QDir(m_baseDirectory).filePath(path);
    return true;
}

bool Parser::readBuffers(const QJsonArray &array) {
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject o = array.at(i).toObject();
        const QString what = QStringLiteral("buffer %1").arg(i);
        qint64 byteLength = -1;
        if (!integer(o, "byteLength", 1, MaxCount, &byteLength, what))
            return false;
        if (byteLength < 0)
            return dataError(what, QStringLiteral("byteLength is missing"));
        QByteArray data;
        const QString uri = o.value("uri").toString();
        if (uri.isEmpty()) {
            if (i != 0 || !m_hasGlbBin)
                return dataError(what, QStringLiteral("has no URI and there is no GLB binary chunk"));
            data = m_glbBin;
        } else {
            QString fileName;
            if (!resolveUri(uri, what, &data, nullptr, &fileName))
                return false;
            if (!fileName.isEmpty()) {
                QFile file(fileName);
                if (!file.open(QIODevice::ReadOnly))
                    return fail(Document::FileError, what + QStringLiteral(": cannot open %1").arg(fileName));
                data = file.readAll();
            }
        }
        if (data.size() < byteLength)
            return dataError(
                what,
                QStringLiteral("holds %1 bytes, fewer than its byteLength of %2").arg(data.size()).arg(byteLength));
        data.truncate(qsizetype(byteLength));
        m_buffers.append(data);
    }
    return true;
}

bool Parser::readBufferViews(const QJsonArray &array) {
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject o = array.at(i).toObject();
        const QString what = QStringLiteral("bufferView %1").arg(i);
        BufferView view;
        qint64 offset = 0;
        qint64 length = -1;
        qint64 stride = 0;
        if (!o.contains("buffer"))
            return dataError(what, QStringLiteral("buffer is missing"));
        if (!index(o, "buffer", m_buffers.size(), &view.buffer, what) ||
            !integer(o, "byteOffset", 0, MaxCount, &offset, what) ||
            !integer(o, "byteLength", 1, MaxCount, &length, what) || !integer(o, "byteStride", 4, 252, &stride, what))
            return false;
        if (length < 0)
            return dataError(what, QStringLiteral("byteLength is missing"));
        if (offset + length > m_buffers.at(view.buffer).size())
            return dataError(what, QStringLiteral("extends past the end of its buffer"));
        view.offset = qsizetype(offset);
        view.length = qsizetype(length);
        view.stride = qsizetype(stride);
        m_views.append(view);
    }
    return true;
}

bool Parser::readAccessors(const QJsonArray &array) {
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject o = array.at(i).toObject();
        const QString what = QStringLiteral("accessor %1").arg(i);
        Accessor a;
        qint64 offset = 0;
        qint64 count = -1;
        qint64 componentType = 0;
        if (!index(o, "bufferView", m_views.size(), &a.bufferView, what) ||
            !integer(o, "byteOffset", 0, MaxCount, &offset, what) || !integer(o, "count", 1, MaxCount, &count, what) ||
            !integer(o, "componentType", Byte, Float, &componentType, what))
            return false;
        a.offset = qsizetype(offset);
        a.count = qsizetype(count);
        a.componentType = int(componentType);
        a.normalized = o.value("normalized").toBool();
        a.components = componentCount(o.value("type").toString());
        if (count < 0)
            return dataError(what, QStringLiteral("count is missing"));
        if (componentSize(a.componentType) == 0)
            return dataError(what, QStringLiteral("componentType %1 is not valid").arg(componentType));
        if (a.components == 0)
            return dataError(what, QStringLiteral("type is not valid"));
        if (a.normalized && (a.componentType == UnsignedInt || a.componentType == Float))
            return dataError(what, QStringLiteral("only 8- and 16-bit integers can be normalized"));

        const QJsonValue sparseValue = o.value("sparse");
        if (!sparseValue.isUndefined()) {
            const QJsonObject sparse = sparseValue.toObject();
            const QJsonObject indices = sparse.value("indices").toObject();
            const QJsonObject values = sparse.value("values").toObject();
            qint64 sparseCount = -1;
            qint64 indexOffset = 0;
            qint64 indexType = 0;
            qint64 valueOffset = 0;
            if (!integer(sparse, "count", 1, a.count, &sparseCount, what) ||
                !index(indices, "bufferView", m_views.size(), &a.sparseIndexView, what) ||
                !integer(indices, "byteOffset", 0, MaxCount, &indexOffset, what) ||
                !integer(indices, "componentType", UnsignedByte, UnsignedInt, &indexType, what) ||
                !index(values, "bufferView", m_views.size(), &a.sparseValueView, what) ||
                !integer(values, "byteOffset", 0, MaxCount, &valueOffset, what))
                return false;
            if (sparseCount < 0 || a.sparseIndexView < 0 || a.sparseValueView < 0 ||
                (indexType != UnsignedByte && indexType != UnsignedShort && indexType != UnsignedInt))
                return dataError(what, QStringLiteral("sparse storage is incomplete"));
            a.sparseCount = qsizetype(sparseCount);
            a.sparseIndexOffset = qsizetype(indexOffset);
            a.sparseIndexType = int(indexType);
            a.sparseValueOffset = qsizetype(valueOffset);
        }
        m_accessors.append(a);
    }
    return true;
}

bool Parser::viewData(int view, qsizetype offset, qsizetype size, const char **data, const QString &what) {
    const BufferView &v = m_views.at(view);
    if (offset > v.length || size > v.length - offset)
        return dataError(what, QStringLiteral("extends past the end of bufferView %1").arg(view));
    *data = m_buffers.at(v.buffer).constData() + v.offset + offset;
    return true;
}

// Decodes an accessor into a flat array of count * components values, applying sparse storage.
template <typename T> bool Parser::readAccessor(int accessor, QList<T> *out, const QString &what) {
    const Accessor &a = m_accessors.at(accessor);
    const QString where = what + QStringLiteral(", accessor %1").arg(accessor);
    const qsizetype componentBytes = componentSize(a.componentType);
    const qsizetype elementBytes = componentBytes * a.components;
    out->fill(T(0), a.count * a.components);
    T *dst = out->data();

    if (a.bufferView >= 0) {
        const qsizetype stride = m_views.at(a.bufferView).stride ? m_views.at(a.bufferView).stride : elementBytes;
        if (stride < elementBytes)
            return dataError(where, QStringLiteral("byteStride is smaller than an element"));
        const char *base = nullptr;
        if (!viewData(a.bufferView, a.offset, stride * (a.count - 1) + elementBytes, &base, where))
            return false;
        for (qsizetype i = 0; i < a.count; ++i) {
            const char *element = base + i * stride;
            for (int c = 0; c < a.components; ++c)
                dst[i * a.components + c] =
                    static_cast<T>(readComponent(element + c * componentBytes, a.componentType, a.normalized));
        }
    }

    if (a.sparseCount > 0) {
        const qsizetype indexBytes = componentSize(a.sparseIndexType);
        const char *indices = nullptr;
        const char *values = nullptr;
        if (!viewData(a.sparseIndexView, a.sparseIndexOffset, indexBytes * a.sparseCount, &indices, where) ||
            !viewData(a.sparseValueView, a.sparseValueOffset, elementBytes * a.sparseCount, &values, where))
            return false;
        for (qsizetype k = 0; k < a.sparseCount; ++k) {
            const auto target = qsizetype(readComponent(indices + k * indexBytes, a.sparseIndexType, false));
            if (target >= a.count)
                return dataError(where, QStringLiteral("sparse index %1 is out of range").arg(target));
            for (int c = 0; c < a.components; ++c)
                dst[target * a.components + c] = static_cast<T>(
                    readComponent(values + k * elementBytes + c * componentBytes, a.componentType, a.normalized));
        }
    }
    return true;
}

bool Parser::readSamplers(const QJsonArray &array) {
    auto filter = [](qint64 v) {
        switch (v) {
        case Sampler::Nearest:
        case Sampler::Linear:
        case Sampler::NearestMipmapNearest:
        case Sampler::LinearMipmapNearest:
        case Sampler::NearestMipmapLinear:
        case Sampler::LinearMipmapLinear:
            return Sampler::Filter(v);
        default:
            return Sampler::DefaultFilter;
        }
    };
    auto wrap = [](qint64 v) {
        switch (v) {
        case Sampler::ClampToEdge:
        case Sampler::MirroredRepeat:
            return Sampler::Wrap(v);
        default:
            return Sampler::Repeat;
        }
    };
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject o = array.at(i).toObject();
        Sampler s;
        s.name = o.value("name").toString();
        s.magFilter = filter(o.value("magFilter").toInteger());
        s.minFilter = filter(o.value("minFilter").toInteger());
        s.wrapS = wrap(o.value("wrapS").toInteger(Sampler::Repeat));
        s.wrapT = wrap(o.value("wrapT").toInteger(Sampler::Repeat));
        m_out.samplers.append(s);
    }
    return true;
}

bool Parser::readImages(const QJsonArray &array) {
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject o = array.at(i).toObject();
        const QString what = QStringLiteral("image %1").arg(i);
        Image image;
        image.name = o.value("name").toString();
        image.mimeType = o.value("mimeType").toString();
        int view = -1;
        if (!index(o, "bufferView", m_views.size(), &view, what))
            return false;
        const QString uri = o.value("uri").toString();
        if (view >= 0) {
            const char *data = nullptr;
            const qsizetype length = m_views.at(view).length;
            if (!viewData(view, 0, length, &data, what))
                return false;
            image.data = QByteArray(data, length);
        } else if (!uri.isEmpty()) {
            QString mimeType;
            if (!resolveUri(uri, what, &image.data, &mimeType, &image.fileName))
                return false;
            if (image.mimeType.isEmpty())
                image.mimeType = mimeType;
        } else {
            return dataError(what, QStringLiteral("has neither a URI nor a bufferView"));
        }
        m_out.images.append(image);
    }
    return true;
}

bool Parser::readTextures(const QJsonArray &array) {
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject o = array.at(i).toObject();
        const QString what = QStringLiteral("texture %1").arg(i);
        Texture t;
        t.name = o.value("name").toString();
        if (!index(o, "source", m_imageCount, &t.source, what) ||
            !index(o, "sampler", m_samplerCount, &t.sampler, what))
            return false;
        // Textures in other container formats name their image in an extension.
        if (t.source < 0) {
            const QJsonObject extensions = o.value("extensions").toObject();
            for (const char *name : {"KHR_texture_basisu", "EXT_texture_webp", "MSFT_texture_dds"}) {
                const QJsonValue ext = extensions.value(name);
                if (ext.isObject() && !index(ext.toObject(), "source", m_imageCount, &t.source, what))
                    return false;
                if (t.source >= 0)
                    break;
            }
        }
        m_out.textures.append(t);
    }
    return true;
}

bool Parser::readTextureInfo(const QJsonObject &o, const char *key, const char *scaleKey, TextureInfo *info,
                             const QString &what) {
    const QJsonValue v = o.value(key);
    if (v.isUndefined())
        return true;
    const QJsonObject t = v.toObject();
    const QString where = what + QLatin1StringView(", ") + QString::fromLatin1(key);
    if (!t.contains("index"))
        return dataError(where, QStringLiteral("index is missing"));
    qint64 texCoord = 0;
    if (!index(t, "index", m_textureCount, &info->index, where) ||
        !integer(t, "texCoord", 0, 1 << 16, &texCoord, where))
        return false;
    info->texCoord = int(texCoord);
    if (scaleKey)
        info->scale = number(t, scaleKey, 1.0f);
    // KHR_texture_transform: the coordinates are scaled, then rotated, then moved; it may also pick the set.
    const QJsonObject transform = t.value("extensions").toObject().value("KHR_texture_transform").toObject();
    if (!transform.isEmpty()) {
        float offset[2] = {0.0f, 0.0f};
        float scale[2] = {1.0f, 1.0f};
        qint64 set = info->texCoord;
        if (!floats(transform, "offset", 2, offset, where) || !floats(transform, "scale", 2, scale, where) ||
            !integer(transform, "texCoord", 0, 1 << 16, &set, where))
            return false;
        info->uvOffset = QVector2D(offset[0], offset[1]);
        info->uvScale = QVector2D(scale[0], scale[1]);
        info->uvRotation = number(transform, "rotation", 0.0f);
        info->texCoord = int(set);
    }
    return true;
}

bool Parser::readMaterials(const QJsonArray &array) {
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject o = array.at(i).toObject();
        const QString what = QStringLiteral("material %1").arg(i);
        Material m;
        m.name = o.value("name").toString();
        const QJsonObject pbr = o.value("pbrMetallicRoughness").toObject();
        float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        float emissive[3] = {0.0f, 0.0f, 0.0f};
        if (!floats(pbr, "baseColorFactor", 4, color, what) || !floats(o, "emissiveFactor", 3, emissive, what) ||
            !readTextureInfo(pbr, "baseColorTexture", nullptr, &m.baseColorTexture, what) ||
            !readTextureInfo(pbr, "metallicRoughnessTexture", nullptr, &m.metallicRoughnessTexture, what) ||
            !readTextureInfo(o, "normalTexture", "scale", &m.normalTexture, what) ||
            !readTextureInfo(o, "occlusionTexture", "strength", &m.occlusionTexture, what) ||
            !readTextureInfo(o, "emissiveTexture", nullptr, &m.emissiveTexture, what))
            return false;
        m.baseColorFactor = QVector4D(color[0], color[1], color[2], color[3]);
        m.emissiveFactor = QVector3D(emissive[0], emissive[1], emissive[2]);
        m.metallicFactor = number(pbr, "metallicFactor", 1.0f);
        m.roughnessFactor = number(pbr, "roughnessFactor", 1.0f);
        const QString alphaMode = o.value("alphaMode").toString(QStringLiteral("OPAQUE"));
        if (alphaMode == QLatin1StringView("OPAQUE"))
            m.alphaMode = Material::Opaque;
        else if (alphaMode == QLatin1StringView("MASK"))
            m.alphaMode = Material::Mask;
        else if (alphaMode == QLatin1StringView("BLEND"))
            m.alphaMode = Material::Blend;
        else
            return dataError(what, QStringLiteral("alphaMode %1 is not valid").arg(alphaMode));
        m.alphaCutoff = number(o, "alphaCutoff", 0.5f);
        m.doubleSided = o.value("doubleSided").toBool();
        const QJsonObject extensions = o.value("extensions").toObject();
        m.emissiveStrength =
            number(extensions.value("KHR_materials_emissive_strength").toObject(), "emissiveStrength", 1.0f);
        m.unlit = extensions.contains("KHR_materials_unlit");
        m.ior = number(extensions.value("KHR_materials_ior").toObject(), "ior", 1.5f);
        const QJsonObject transmission = extensions.value("KHR_materials_transmission").toObject();
        m.transmissionFactor = number(transmission, "transmissionFactor", 0.0f);
        const QJsonObject volume = extensions.value("KHR_materials_volume").toObject();
        float attenuation[3] = {1.0f, 1.0f, 1.0f};
        if (!readTextureInfo(transmission, "transmissionTexture", nullptr, &m.transmissionTexture, what) ||
            !readTextureInfo(volume, "thicknessTexture", nullptr, &m.thicknessTexture, what) ||
            !floats(volume, "attenuationColor", 3, attenuation, what))
            return false;
        m.thicknessFactor = number(volume, "thicknessFactor", 0.0f);
        m.attenuationColor = QVector3D(attenuation[0], attenuation[1], attenuation[2]);
        m.attenuationDistance = number(volume, "attenuationDistance", std::numeric_limits<float>::infinity());
        m_out.materials.append(m);
    }
    return true;
}

bool Parser::readAttribute(const QJsonObject &attributes, const char *name, int minComponents, int maxComponents,
                           qsizetype vertexCount, QList<float> *values, int *components, const QString &what) {
    int accessor = -1;
    if (!index(attributes, name, m_accessors.size(), &accessor, what))
        return false;
    if (accessor < 0)
        return true;
    const Accessor &a = m_accessors.at(accessor);
    const QString where = what + QLatin1StringView(", ") + QString::fromLatin1(name);
    if (a.components < minComponents || a.components > maxComponents)
        return dataError(where, QStringLiteral("has the wrong type"));
    if (vertexCount >= 0 && a.count != vertexCount)
        return dataError(where, QStringLiteral("has %1 elements, not %2").arg(a.count).arg(vertexCount));
    *components = a.components;
    return readAccessor(accessor, values, where);
}

bool Parser::readPrimitive(const QJsonObject &o, Primitive *p, bool *keep, const QString &what) {
    const QJsonObject attributes = o.value("attributes").toObject();
    qint64 mode = 4;
    if (!index(o, "material", m_materialCount, &p->material, what) || !integer(o, "mode", 0, 6, &mode, what))
        return false;

    // Primitives without positions are not rendered, as the specification recommends.
    int positionAccessor = -1;
    if (!index(attributes, "POSITION", m_accessors.size(), &positionAccessor, what))
        return false;
    if (positionAccessor < 0) {
        *keep = false;
        return true;
    }
    *keep = true;
    const qsizetype vertexCount = m_accessors.at(positionAccessor).count;

    QList<float> values;
    int components = 0;
    if (!readAttribute(attributes, "POSITION", 3, 3, vertexCount, &values, &components, what))
        return false;
    p->positions.resize(vertexCount);
    for (qsizetype i = 0; i < vertexCount; ++i)
        p->positions[i] = QVector3D(values[3 * i], values[3 * i + 1], values[3 * i + 2]);

    values.clear();
    if (!readAttribute(attributes, "NORMAL", 3, 3, vertexCount, &values, &components, what))
        return false;
    if (!values.isEmpty()) {
        p->normals.resize(vertexCount);
        for (qsizetype i = 0; i < vertexCount; ++i)
            p->normals[i] = QVector3D(values[3 * i], values[3 * i + 1], values[3 * i + 2]);
    }

    values.clear();
    if (!readAttribute(attributes, "TANGENT", 4, 4, vertexCount, &values, &components, what))
        return false;
    if (!values.isEmpty()) {
        p->tangents.resize(vertexCount);
        for (qsizetype i = 0; i < vertexCount; ++i)
            p->tangents[i] = QVector4D(values[4 * i], values[4 * i + 1], values[4 * i + 2], values[4 * i + 3]);
    }

    const char *const texCoordNames[] = {"TEXCOORD_0", "TEXCOORD_1"};
    QList<QVector2D> *const texCoordLists[] = {&p->texCoords0, &p->texCoords1};
    for (int set = 0; set < 2; ++set) {
        values.clear();
        if (!readAttribute(attributes, texCoordNames[set], 2, 2, vertexCount, &values, &components, what))
            return false;
        if (!values.isEmpty()) {
            texCoordLists[set]->resize(vertexCount);
            for (qsizetype i = 0; i < vertexCount; ++i)
                (*texCoordLists[set])[i] = QVector2D(values[2 * i], values[2 * i + 1]);
        }
    }

    values.clear();
    if (!readAttribute(attributes, "COLOR_0", 3, 4, vertexCount, &values, &components, what))
        return false;
    if (!values.isEmpty()) {
        p->colors.resize(vertexCount);
        for (qsizetype i = 0; i < vertexCount; ++i) {
            const float *v = values.constData() + i * components;
            p->colors[i] = QVector4D(v[0], v[1], v[2], components == 4 ? v[3] : 1.0f);
        }
    }

    // Indices, generated when absent, then strips, fans and loops turned into lists.
    QList<quint32> indices;
    int indexAccessor = -1;
    if (!index(o, "indices", m_accessors.size(), &indexAccessor, what))
        return false;
    if (indexAccessor >= 0) {
        const Accessor &a = m_accessors.at(indexAccessor);
        if (a.components != 1 || a.normalized ||
            (a.componentType != UnsignedByte && a.componentType != UnsignedShort && a.componentType != UnsignedInt))
            return dataError(what, QStringLiteral("indices must be unsigned integer scalars"));
        if (!readAccessor(indexAccessor, &indices, what + QStringLiteral(", indices")))
            return false;
        for (quint32 i : std::as_const(indices)) {
            if (qsizetype(i) >= vertexCount)
                return dataError(what, QStringLiteral("index %1 is out of range").arg(i));
        }
    } else {
        indices.resize(vertexCount);
        for (qsizetype i = 0; i < vertexCount; ++i)
            indices[i] = quint32(i);
    }

    const qsizetype n = indices.size();
    QList<quint32> &out = p->indices;
    auto triangle = [&out](quint32 a, quint32 b, quint32 c) {
        if (a != b && b != c && a != c)
            out.append({a, b, c});
    };
    switch (mode) {
    case 0: // POINTS
        p->mode = Primitive::Points;
        out = indices;
        break;
    case 1: // LINES
        p->mode = Primitive::Lines;
        out = indices.mid(0, n - n % 2);
        break;
    case 2: // LINE_LOOP
    case 3: // LINE_STRIP
        p->mode = Primitive::Lines;
        for (qsizetype i = 0; i + 1 < n; ++i)
            out.append({indices[i], indices[i + 1]});
        if (mode == 2 && n > 2)
            out.append({indices[n - 1], indices[0]});
        break;
    case 4: // TRIANGLES
        p->mode = Primitive::Triangles;
        out = indices.mid(0, n - n % 3);
        break;
    case 5: // TRIANGLE_STRIP
        p->mode = Primitive::Triangles;
        for (qsizetype i = 0; i + 2 < n; ++i) {
            if (i % 2 == 0)
                triangle(indices[i], indices[i + 1], indices[i + 2]);
            else
                triangle(indices[i], indices[i + 2], indices[i + 1]);
        }
        break;
    default: // TRIANGLE_FAN
        p->mode = Primitive::Triangles;
        for (qsizetype i = 0; i + 2 < n; ++i)
            triangle(indices[i + 1], indices[i + 2], indices[0]);
        break;
    }

    if (!p->positions.isEmpty()) {
        p->boundsMin = p->boundsMax = p->positions.first();
        for (const QVector3D &v : std::as_const(p->positions)) {
            p->boundsMin = QVector3D(std::min(p->boundsMin.x(), v.x()), std::min(p->boundsMin.y(), v.y()),
                                     std::min(p->boundsMin.z(), v.z()));
            p->boundsMax = QVector3D(std::max(p->boundsMax.x(), v.x()), std::max(p->boundsMax.y(), v.y()),
                                     std::max(p->boundsMax.z(), v.z()));
        }
    }
    return true;
}

bool Parser::readMeshes(const QJsonArray &array) {
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject o = array.at(i).toObject();
        Mesh mesh;
        mesh.name = o.value("name").toString();
        const QJsonArray primitives = o.value("primitives").toArray();
        for (qsizetype j = 0; j < primitives.size(); ++j) {
            Primitive primitive;
            bool keep = false;
            if (!readPrimitive(primitives.at(j).toObject(), &primitive, &keep,
                               QStringLiteral("mesh %1, primitive %2").arg(i).arg(j)))
                return false;
            if (keep)
                mesh.primitives.append(std::move(primitive));
        }
        m_out.meshes.append(std::move(mesh));
    }
    return true;
}

bool Parser::readCameras(const QJsonArray &array) {
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject o = array.at(i).toObject();
        const QString what = QStringLiteral("camera %1").arg(i);
        Camera c;
        c.name = o.value("name").toString();
        const QString type = o.value("type").toString();
        if (type == QLatin1StringView("perspective")) {
            const QJsonObject p = o.value("perspective").toObject();
            c.type = Camera::Perspective;
            c.yfov = number(p, "yfov", c.yfov);
            c.aspectRatio = number(p, "aspectRatio", 0.0f);
            c.znear = number(p, "znear", c.znear);
            c.zfar = number(p, "zfar", 0.0f);
        } else if (type == QLatin1StringView("orthographic")) {
            const QJsonObject p = o.value("orthographic").toObject();
            c.type = Camera::Orthographic;
            c.xmag = number(p, "xmag", c.xmag);
            c.ymag = number(p, "ymag", c.ymag);
            c.znear = number(p, "znear", 0.0f);
            c.zfar = number(p, "zfar", 0.0f);
        } else {
            return dataError(what, QStringLiteral("type %1 is not valid").arg(type));
        }
        m_out.cameras.append(c);
    }
    return true;
}

bool Parser::readLights(const QJsonObject &root) {
    const QJsonArray array =
        root.value("extensions").toObject().value("KHR_lights_punctual").toObject().value("lights").toArray();
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject o = array.at(i).toObject();
        const QString what = QStringLiteral("light %1").arg(i);
        Light light;
        light.name = o.value("name").toString();
        const QString type = o.value("type").toString();
        if (type == QLatin1StringView("directional"))
            light.type = Light::Directional;
        else if (type == QLatin1StringView("point"))
            light.type = Light::Point;
        else if (type == QLatin1StringView("spot"))
            light.type = Light::Spot;
        else
            return dataError(what, QStringLiteral("type %1 is not valid").arg(type));
        float color[3] = {1.0f, 1.0f, 1.0f};
        if (!floats(o, "color", 3, color, what))
            return false;
        light.color = QVector3D(color[0], color[1], color[2]);
        light.intensity = number(o, "intensity", 1.0f);
        light.range = number(o, "range", 0.0f);
        const QJsonObject spot = o.value("spot").toObject();
        light.innerConeAngle = number(spot, "innerConeAngle", light.innerConeAngle);
        light.outerConeAngle = number(spot, "outerConeAngle", light.outerConeAngle);
        m_out.lights.append(light);
    }
    m_lightCount = array.size();
    return true;
}

bool Parser::readNodes(const QJsonArray &array) {
    m_out.parents.fill(-1, array.size());
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject o = array.at(i).toObject();
        const QString what = QStringLiteral("node %1").arg(i);
        Node node;
        node.name = o.value("name").toString();
        const QJsonObject lightExt = o.value("extensions").toObject().value("KHR_lights_punctual").toObject();
        if (!index(o, "mesh", m_meshCount, &node.mesh, what) ||
            !index(o, "camera", m_cameraCount, &node.camera, what) ||
            !index(lightExt, "light", m_lightCount, &node.light, what))
            return false;

        const QJsonArray children = o.value("children").toArray();
        for (qsizetype j = 0; j < children.size(); ++j) {
            const QJsonValue v = children.at(j);
            const double x = v.toDouble(-1.0);
            if (!v.isDouble() || x < 0 || x >= double(array.size()) || x != std::floor(x))
                return dataError(what, QStringLiteral("child %1 is not a valid index").arg(j));
            const int child = int(x);
            if (child == int(i) || m_out.parents.at(child) != -1)
                return dataError(what, QStringLiteral("node %1 has more than one parent").arg(child));
            m_out.parents[child] = int(i);
            node.children.append(child);
        }

        if (o.contains("matrix")) {
            float m[16] = {};
            if (!floats(o, "matrix", 16, m, what))
                return false;
            // glTF stores the matrix by column; the constructor takes it by row.
            node.transform = QMatrix4x4(m[0], m[4], m[8], m[12], m[1], m[5], m[9], m[13], m[2], m[6], m[10], m[14],
                                        m[3], m[7], m[11], m[15]);
        } else {
            float t[3] = {0.0f, 0.0f, 0.0f};
            float r[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            float s[3] = {1.0f, 1.0f, 1.0f};
            if (!floats(o, "translation", 3, t, what) || !floats(o, "rotation", 4, r, what) ||
                !floats(o, "scale", 3, s, what))
                return false;
            QQuaternion rotation(r[3], r[0], r[1], r[2]);
            if (rotation.length() > 0.0f)
                rotation = rotation.normalized();
            else
                rotation = QQuaternion();
            node.transform.translate(t[0], t[1], t[2]);
            node.transform.rotate(rotation);
            node.transform.scale(s[0], s[1], s[2]);
        }
        m_out.nodes.append(std::move(node));
    }

    // A node can have only one parent, so a cycle is the only way left to break the hierarchy.
    for (qsizetype i = 0; i < m_out.parents.size(); ++i) {
        qsizetype steps = 0;
        for (int p = m_out.parents.at(i); p >= 0; p = m_out.parents.at(p)) {
            if (++steps > m_out.parents.size())
                return dataError(QStringLiteral("node %1").arg(i), QStringLiteral("is part of a cycle"));
        }
    }
    return true;
}

bool Parser::readScenes(const QJsonObject &root) {
    const QJsonArray array = root.value("scenes").toArray();
    for (qsizetype i = 0; i < array.size(); ++i) {
        const QJsonObject o = array.at(i).toObject();
        const QString what = QStringLiteral("scene %1").arg(i);
        Scene scene;
        scene.name = o.value("name").toString();
        const QJsonArray nodes = o.value("nodes").toArray();
        for (qsizetype j = 0; j < nodes.size(); ++j) {
            const QJsonValue v = nodes.at(j);
            const double x = v.toDouble(-1.0);
            if (!v.isDouble() || x < 0 || x >= double(m_nodeCount) || x != std::floor(x))
                return dataError(what, QStringLiteral("node %1 is not a valid index").arg(j));
            if (m_out.parents.at(qsizetype(x)) != -1)
                return dataError(what, QStringLiteral("node %1 is not a root node").arg(qsizetype(x)));
            scene.nodes.append(int(x));
        }
        m_out.scenes.append(scene);
    }
    int defaultScene = array.isEmpty() ? -1 : 0;
    if (!index(root, "scene", array.size(), &defaultScene, QStringLiteral("document")))
        return false;
    m_out.defaultScene = defaultScene;
    return true;
}

bool Parser::parse(const QByteArray &data) {
    QByteArray json = data;
    if (data.size() >= 12 && readU32(data.constData()) == GlbMagic) {
        if (!readGlb(data, &json))
            return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return fail(Document::ParseError,
                    QStringLiteral("JSON error at offset %1: %2").arg(parseError.offset).arg(parseError.errorString()));
    if (!document.isObject())
        return fail(Document::ParseError, QStringLiteral("the document is not a JSON object"));
    const QJsonObject root = document.object();
    if (!readAsset(root))
        return false;

    // The arrays refer to each other by index, so their sizes are known before any is read.
    m_imageCount = root.value("images").toArray().size();
    m_samplerCount = root.value("samplers").toArray().size();
    m_textureCount = root.value("textures").toArray().size();
    m_materialCount = root.value("materials").toArray().size();
    m_meshCount = root.value("meshes").toArray().size();
    m_cameraCount = root.value("cameras").toArray().size();
    m_nodeCount = root.value("nodes").toArray().size();

    return readBuffers(root.value("buffers").toArray()) && readBufferViews(root.value("bufferViews").toArray()) &&
           readAccessors(root.value("accessors").toArray()) && readSamplers(root.value("samplers").toArray()) &&
           readImages(root.value("images").toArray()) && readTextures(root.value("textures").toArray()) &&
           readMaterials(root.value("materials").toArray()) && readMeshes(root.value("meshes").toArray()) &&
           readCameras(root.value("cameras").toArray()) && readLights(root) &&
           readNodes(root.value("nodes").toArray()) && readScenes(root);
}

} // namespace

/*!
    \namespace QtGltf
    \inmodule NtGltf
    \brief The QtGltf namespace holds Nt glTF: a loader for glTF 2.0 scenes.

    \sa {Using Nt glTF}
*/

/*!
    \module NtGltf
    \title Nt glTF C++ Classes
    \brief Loads glTF 2.0 scenes into meshes, materials, textures, cameras and lights ready to render.
    \cmaketarget Nt::Gltf

    Nt glTF reads \c .gltf and \c .glb files into a QtGltf::Document: plain structs that follow the
    glTF 2.0 specification, with vertex data already decoded into float arrays and every primitive
    turned into a list of points, lines or triangles. A renderer copies the arrays into its own
    buffers; the \l{Using Nt Vulkan}{Vulkan classes} of Nt GUI can build ray tracing acceleration
    structures from them. Qt has no such module (Qt Quick 3D loads glTF only into its own scene
    graph).

    See \l{Using Nt glTF} for a walk through and \c{examples/gltf/gltfinfo.cpp} for a complete
    program.
*/

// --- TextureInfo ---------------------------------------------------------------------------------

/*!
    \class QtGltf::TextureInfo
    \inmodule NtGltf
    \brief The TextureInfo struct holds a material's reference to a texture.

    \c KHR_texture_transform is read into uvOffset, uvRotation and uvScale, and its \c texCoord replaces
    texCoord; the coordinates are not changed, so that a renderer applies the transform where it samples.

    \differences
    Texture coordinates beyond the first two sets are not read, as in Primitive.
    \enddifferences

    \sa QtGltf::Material
*/

/*!
    \variable QtGltf::TextureInfo::index
    The index of the texture in QtGltf::Document::textures(), or -1 if the material has no such
    texture.
*/

/*!
    \variable QtGltf::TextureInfo::texCoord
    The texture coordinate set the texture uses: 0 for Primitive::texCoords0, 1 for
    Primitive::texCoords1.
*/

/*!
    \variable QtGltf::TextureInfo::scale
    The scale of a normal texture or the strength of an occlusion texture; 1 for the others.
*/

/*!
    \variable QtGltf::TextureInfo::uvOffset
    The offset that \c KHR_texture_transform adds to the coordinates after scaling and rotating them; (0, 0)
    by default.
*/

/*!
    \variable QtGltf::TextureInfo::uvRotation
    The rotation, in radians counter-clockwise, that \c KHR_texture_transform applies to the scaled coordinates;
    0 by default. The matrix is \c{[cos*sx, sin*sy, ox; -sin*sx, cos*sy, oy]}, with the scale s and the offset o.
*/

/*!
    \variable QtGltf::TextureInfo::uvScale
    The scale that \c KHR_texture_transform applies to the coordinates first; (1, 1) by default.
*/

/*!
    \fn bool QtGltf::TextureInfo::isValid() const
    Returns \c true if the reference names a texture; otherwise returns \c false.
*/

// --- Material ------------------------------------------------------------------------------------

/*!
    \class QtGltf::Material
    \inmodule NtGltf
    \brief The Material struct holds a physically based material with metallic-roughness parameters.

    The factors multiply the matching textures, as the glTF specification describes. Colours are
    linear except the base colour and emissive \e textures, which are sRGB.

    \differences
    Of the material extensions, \c KHR_materials_emissive_strength, \c KHR_materials_unlit and the
    glass ones, \c KHR_materials_ior, \c KHR_materials_transmission and \c KHR_materials_volume,
    are read. Their other fields, such as dispersion, are not.
    \enddifferences
*/

/*!
    \enum QtGltf::Material::AlphaMode
    How the alpha of the base colour is used.
    \value Opaque Alpha is ignored and the surface is opaque.
    \value Mask The surface is opaque where alpha is at least alphaCutoff and absent elsewhere.
    \value Blend Alpha blends the surface with what is behind it.
*/

/*!
    \variable QtGltf::Material::name
    The name of the material; empty if it has none.
*/

/*!
    \variable QtGltf::Material::baseColorFactor
    The linear base colour and alpha; white and opaque by default.
*/

/*!
    \variable QtGltf::Material::baseColorTexture
    The sRGB base colour texture.
*/

/*!
    \variable QtGltf::Material::metallicFactor
    How metallic the surface is, from 0 to 1; 1 by default.
*/

/*!
    \variable QtGltf::Material::roughnessFactor
    How rough the surface is, from 0 to 1; 1 by default.
*/

/*!
    \variable QtGltf::Material::metallicRoughnessTexture
    The texture whose blue channel holds metalness and green channel roughness.
*/

/*!
    \variable QtGltf::Material::normalTexture
    The tangent-space normal map; TextureInfo::scale scales its X and Y.
*/

/*!
    \variable QtGltf::Material::occlusionTexture
    The ambient occlusion texture, read from its red channel; TextureInfo::scale holds its strength.
*/

/*!
    \variable QtGltf::Material::emissiveTexture
    The sRGB emissive texture.
*/

/*!
    \variable QtGltf::Material::emissiveFactor
    The linear emissive colour; black by default.
*/

/*!
    \variable QtGltf::Material::emissiveStrength
    The multiplier of the emissive colour from \c KHR_materials_emissive_strength; 1 by default.
*/

/*!
    \variable QtGltf::Material::alphaMode
    How alpha is used; Opaque by default.
*/

/*!
    \variable QtGltf::Material::alphaCutoff
    The alpha below which a Mask material is absent; 0.5 by default.
*/

/*!
    \variable QtGltf::Material::doubleSided
    \c true if back faces are rendered and lit with the normal reversed; \c false by default.
*/

/*!
    \variable QtGltf::Material::unlit
    \c true if the material uses \c KHR_materials_unlit and shows its base colour without lighting.
*/

/*!
    \variable QtGltf::Material::ior
    The index of refraction from \c KHR_materials_ior; 1.5 by default.
*/

/*!
    \variable QtGltf::Material::transmissionFactor
    The share of light that passes through the surface instead of being diffusely reflected, from
    \c KHR_materials_transmission; 0 by default, so that the material is opaque.
*/

/*!
    \variable QtGltf::Material::transmissionTexture
    The texture whose red channel multiplies transmissionFactor.
*/

/*!
    \variable QtGltf::Material::thicknessFactor
    The thickness of the volume under the surface, in the mesh's units, from \c KHR_materials_volume; 0 by
    default, which makes the surface thin.
*/

/*!
    \variable QtGltf::Material::thicknessTexture
    The texture whose green channel multiplies thicknessFactor.
*/

/*!
    \variable QtGltf::Material::attenuationColor
    The colour that light has after travelling attenuationDistance() through the volume; white by default.
*/

/*!
    \variable QtGltf::Material::attenuationDistance
    How far white light travels in the volume before it has the attenuationColor; infinity by default, which means
    that the volume does not tint.
*/

// --- Sampler -------------------------------------------------------------------------------------

/*!
    \class QtGltf::Sampler
    \inmodule NtGltf
    \brief The Sampler struct holds how a texture is filtered and wrapped.

    The enumerator values are the OpenGL constants that glTF stores.

    \differences
    Unknown filter or wrap values are read as DefaultFilter and Repeat instead of being rejected.
    \enddifferences
*/

/*!
    \enum QtGltf::Sampler::Filter
    A texture filter.
    \value DefaultFilter The file does not say; the renderer chooses.
    \value Nearest The nearest texel.
    \value Linear Linear interpolation of the nearest texels.
    \value NearestMipmapNearest The nearest texel of the nearest mipmap level.
    \value LinearMipmapNearest Linear interpolation within the nearest mipmap level.
    \value NearestMipmapLinear The nearest texel, interpolated between two mipmap levels.
    \value LinearMipmapLinear Linear interpolation within and between mipmap levels.
*/

/*!
    \enum QtGltf::Sampler::Wrap
    How texture coordinates outside [0, 1] are treated.
    \value Repeat The texture repeats.
    \value ClampToEdge The coordinate is clamped to the edge texels.
    \value MirroredRepeat The texture repeats, mirrored every other time.
*/

/*!
    \variable QtGltf::Sampler::name
    The name of the sampler; empty if it has none.
*/

/*!
    \variable QtGltf::Sampler::magFilter
    The magnification filter.
*/

/*!
    \variable QtGltf::Sampler::minFilter
    The minification filter.
*/

/*!
    \variable QtGltf::Sampler::wrapS
    How the U coordinate wraps.
*/

/*!
    \variable QtGltf::Sampler::wrapT
    How the V coordinate wraps.
*/

// --- Texture -------------------------------------------------------------------------------------

/*!
    \class QtGltf::Texture
    \inmodule NtGltf
    \brief The Texture struct pairs an image with a sampler.

    \differences
    When a texture has no \c source, the image of a \c KHR_texture_basisu, \c EXT_texture_webp or
    \c MSFT_texture_dds extension is used; Nt glTF does not decode those formats itself.
    \enddifferences
*/

/*!
    \variable QtGltf::Texture::name
    The name of the texture; empty if it has none.
*/

/*!
    \variable QtGltf::Texture::source
    The index of the image in QtGltf::Document::images(), or -1.
*/

/*!
    \variable QtGltf::Texture::sampler
    The index of the sampler in QtGltf::Document::samplers(), or -1 for repeat wrapping and a filter
    the renderer chooses.
*/

// --- Image ---------------------------------------------------------------------------------------

/*!
    \class QtGltf::Image
    \inmodule NtGltf
    \brief The Image struct holds an encoded texture image, or where to find one.

    An image stored in the file (a GLB chunk or a data URI) is in data; an external image is named
    by fileName and read only when encodedData() or toImage() asks for it, so loading a large scene
    does not decode its textures.

    \differences
    None.
    \enddifferences
*/

/*!
    \variable QtGltf::Image::name
    The name of the image; empty if it has none.
*/

/*!
    \variable QtGltf::Image::mimeType
    The MIME type of the encoded image, such as \c image/png, if the file gives one.
*/

/*!
    \variable QtGltf::Image::fileName
    The path of an external image file, resolved against the document's directory; empty if the
    image is stored in the file.
*/

/*!
    \variable QtGltf::Image::data
    The encoded bytes of an image stored in the file; empty for an external image.
*/

/*!
    Returns the encoded image: data, or the contents of fileName. Returns an empty byte array if the
    file cannot be read.
*/
QByteArray Image::encodedData() const {
    if (!data.isEmpty() || fileName.isEmpty())
        return data;
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return QByteArray();
    return file.readAll();
}

/*!
    Decodes the image with QImage. Returns a null image if it cannot be read or QImage does not
    know its format.

    \sa encodedData()
*/
QImage Image::toImage() const { return QImage::fromData(encodedData()); }

// --- Primitive -----------------------------------------------------------------------------------

/*!
    \class QtGltf::Primitive
    \inmodule NtGltf
    \brief The Primitive struct holds decoded geometry drawn with one material.

    Every attribute list is empty or has one element per position. Quantized and normalized
    attributes are converted to floats, sparse accessors are applied, and indices are always
    present: generated for non-indexed geometry, and converted from strips, fans and loops to
    plain lists. Triangles that a strip or fan makes degenerate are dropped.

    \differences
    Skinning attributes (\c JOINTS_n, \c WEIGHTS_n), morph targets, texture coordinate sets after
    the second, and colour sets after the first are not read.
    \enddifferences
*/

/*!
    \enum QtGltf::Primitive::Mode
    What indices describes.
    \value Points Each index is a point.
    \value Lines Each pair of indices is a line.
    \value Triangles Each three indices are a triangle, counter-clockwise when seen from the front.
*/

/*!
    \variable QtGltf::Primitive::mode
    What indices describes.
*/

/*!
    \variable QtGltf::Primitive::material
    The index of the material in QtGltf::Document::materials(), or -1 for the default material.
*/

/*!
    \variable QtGltf::Primitive::positions
    The vertex positions.
*/

/*!
    \variable QtGltf::Primitive::normals
    The unit vertex normals; empty if the file has none, when flat normals should be used.
*/

/*!
    \variable QtGltf::Primitive::tangents
    The vertex tangents; \c w is 1 or -1 and gives the direction of the bitangent.
*/

/*!
    \variable QtGltf::Primitive::texCoords0
    The first set of texture coordinates, with (0, 0) at the top left of the image.
*/

/*!
    \variable QtGltf::Primitive::texCoords1
    The second set of texture coordinates.
*/

/*!
    \variable QtGltf::Primitive::colors
    The linear vertex colours; alpha is 1 when the file stores RGB.
*/

/*!
    \variable QtGltf::Primitive::indices
    The vertex indices, as mode describes.
*/

/*!
    \variable QtGltf::Primitive::boundsMin
    The smallest coordinates of the positions.
*/

/*!
    \variable QtGltf::Primitive::boundsMax
    The largest coordinates of the positions.
*/

// --- Mesh, Camera, Light, Node, Scene, Instance --------------------------------------------------

/*!
    \class QtGltf::Mesh
    \inmodule NtGltf
    \brief The Mesh struct holds the primitives that one node draws.

    \differences
    Primitives without a \c POSITION attribute are left out, and morph target weights are not read.
    \enddifferences
*/

/*!
    \variable QtGltf::Mesh::name
    The name of the mesh; empty if it has none.
*/

/*!
    \variable QtGltf::Mesh::primitives
    The primitives of the mesh.
*/

/*!
    \class QtGltf::Camera
    \inmodule NtGltf
    \brief The Camera struct holds a perspective or orthographic projection.

    The camera looks down the negative Z axis of its node, with Y up.

    \differences
    None.
    \enddifferences
*/

/*!
    \enum QtGltf::Camera::Type
    The projection.
    \value Perspective A perspective projection, described by yfov and aspectRatio.
    \value Orthographic An orthographic projection, described by xmag and ymag.
*/

/*!
    \variable QtGltf::Camera::name
    The name of the camera; empty if it has none.
*/

/*!
    \variable QtGltf::Camera::type
    The projection.
*/

/*!
    \variable QtGltf::Camera::yfov
    The vertical field of view of a perspective camera, in radians.
*/

/*!
    \variable QtGltf::Camera::aspectRatio
    The aspect ratio of a perspective camera; 0 to use the viewport's.
*/

/*!
    \variable QtGltf::Camera::xmag
    The horizontal half-width of an orthographic camera's view.
*/

/*!
    \variable QtGltf::Camera::ymag
    The vertical half-height of an orthographic camera's view.
*/

/*!
    \variable QtGltf::Camera::znear
    The distance to the near plane.
*/

/*!
    \variable QtGltf::Camera::zfar
    The distance to the far plane; 0 for a perspective camera with no far plane.
*/

/*!
    \class QtGltf::Light
    \inmodule NtGltf
    \brief The Light struct holds a punctual light from \c KHR_lights_punctual.

    Lights shine down the negative Z axis of their node.

    \differences
    None.
    \enddifferences
*/

/*!
    \enum QtGltf::Light::Type
    The kind of light.
    \value Directional Parallel light from infinitely far away; intensity is in lux.
    \value Point Light from a point in all directions; intensity is in candela.
    \value Spot Light from a point in a cone; intensity is in candela.
*/

/*!
    \variable QtGltf::Light::name
    The name of the light; empty if it has none.
*/

/*!
    \variable QtGltf::Light::type
    The kind of light.
*/

/*!
    \variable QtGltf::Light::color
    The linear colour of the light; white by default.
*/

/*!
    \variable QtGltf::Light::intensity
    The brightness of the light, in lux or candela as type describes.
*/

/*!
    \variable QtGltf::Light::range
    The distance beyond which a point or spot light has no effect; 0 for no limit.
*/

/*!
    \variable QtGltf::Light::innerConeAngle
    The angle from the axis of a spot light at which it starts to fade, in radians.
*/

/*!
    \variable QtGltf::Light::outerConeAngle
    The angle from the axis of a spot light at which it has faded out, in radians.
*/

/*!
    \class QtGltf::Node
    \inmodule NtGltf
    \brief The Node struct holds one node of the scene hierarchy.

    \differences
    The transform is kept only as a matrix, not as translation, rotation and scale, since
    animations are not read. Skins and morph weights are not read.
    \enddifferences
*/

/*!
    \variable QtGltf::Node::name
    The name of the node; empty if it has none.
*/

/*!
    \variable QtGltf::Node::mesh
    The index of the mesh the node draws, or -1.
*/

/*!
    \variable QtGltf::Node::camera
    The index of the node's camera, or -1.
*/

/*!
    \variable QtGltf::Node::light
    The index of the node's light in QtGltf::Document::lights(), or -1.
*/

/*!
    \variable QtGltf::Node::children
    The indices of the node's children.
*/

/*!
    \variable QtGltf::Node::transform
    The transform from the node's space to its parent's.
*/

/*!
    \class QtGltf::Scene
    \inmodule NtGltf
    \brief The Scene struct names the root nodes of one scene.

    \differences
    None.
    \enddifferences
*/

/*!
    \variable QtGltf::Scene::name
    The name of the scene; empty if it has none.
*/

/*!
    \variable QtGltf::Scene::nodes
    The indices of the scene's root nodes.
*/

/*!
    \class QtGltf::Instance
    \inmodule NtGltf
    \brief The Instance struct holds a node of a scene and its transform to world space.

    \differences
    None.
    \enddifferences

    \sa QtGltf::Document::instances()
*/

/*!
    \variable QtGltf::Instance::node
    The index of the node.
*/

/*!
    \variable QtGltf::Instance::transform
    The transform from the node's space to world space.
*/

// --- Document ------------------------------------------------------------------------------------

/*!
    \class QtGltf::Document
    \inmodule NtGltf
    \brief The Document class holds a loaded glTF 2.0 scene.

    load() reads a \c .gltf file with its external buffers, or a \c .glb file; loadFromData() reads
    either from memory. The lists then hold the scene as the glTF specification describes it, with
    the same indices:

    \snippet gltf/gltfinfo.cpp load

    instances() walks a scene and gives each node with its transform to world space, which is
    what a ray tracer needs for its top-level acceleration structure:

    \snippet gltf/gltfinfo.cpp instances

    Document is a value type and implicitly shared. Loading checks every index and byte range, so
    a document that loaded can be used without further checks; on failure the document is empty and
    error() says why.

    \differences
    Animations, skins, morph targets and \c KHR_draco_mesh_compression and
    \c EXT_meshopt_compression are not supported; a file that requires an unsupported extension
    fails with ExtensionError. Buffers are read when the document loads; images are read on demand.
    \enddifferences

    \sa {Using Nt glTF}
*/

/*!
    \enum QtGltf::Document::Error
    Why loading failed.
    \value NoError No error.
    \value FileError The file or one of its external buffers could not be read, or a URI is not a
           relative path.
    \value ParseError The file is not JSON or GLB, or has no asset version.
    \value VersionError The file is not glTF 2.0.
    \value ExtensionError The file requires an extension that is not supported.
    \value DataError An index, byte range or value is invalid.
*/

/*!
    Constructs an empty document.
*/
Document::Document() : d(new DocumentData) {}

/*!
    Constructs a copy of \a other.
*/
Document::Document(const Document &other) = default;

/*!
    Move-constructs a document from \a other.
*/
Document::Document(Document &&other) noexcept = default;

/*!
    Assigns \a other to this document and returns a reference to it.
*/
Document &Document::operator=(const Document &other) = default;

/*!
    Move-assigns \a other to this document and returns a reference to it.
*/
Document &Document::operator=(Document &&other) noexcept = default;

/*!
    Destroys the document.
*/
Document::~Document() = default;

/*!
    Loads the \c .gltf or \c .glb file \a fileName, with any external buffers it names. Returns
    \c true on success; otherwise returns \c false and the document is empty.

    \sa error(), errorString()
*/
bool Document::load(const QString &fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        auto *data = new DocumentData;
        data->error = FileError;
        data->errorString = QStringLiteral("cannot open %1").arg(fileName);
        d.reset(data);
        return false;
    }
    return loadFromData(file.readAll(), QFileInfo(fileName).absolutePath());
}

/*!
    Loads glTF JSON or a GLB container from \a data. Relative URIs of external buffers and images
    are resolved against \a baseDirectory, or the current directory if it is empty. Returns
    \c true on success; otherwise returns \c false and the document is empty.
*/
bool Document::loadFromData(const QByteArray &data, const QString &baseDirectory) {
    auto *loaded = new DocumentData;
    Parser parser(*loaded, baseDirectory);
    if (!parser.parse(data)) {
        auto *failed = new DocumentData;
        failed->error = loaded->error;
        failed->errorString = loaded->errorString;
        delete loaded;
        d.reset(failed);
        return false;
    }
    loaded->loaded = true;
    d.reset(loaded);
    return true;
}

/*!
    Returns \c true if a document has been loaded successfully; otherwise returns \c false.
*/
bool Document::isValid() const { return d->loaded; }

/*!
    Returns why the last load failed, or NoError.
*/
Document::Error Document::error() const { return d->error; }

/*!
    Returns a description of why the last load failed, naming the object at fault; empty if it did
    not fail.
*/
QString Document::errorString() const { return d->errorString; }

/*!
    Returns the name of the tool that wrote the file, if it says.
*/
QString Document::generator() const { return d->generator; }

/*!
    Returns the copyright notice of the file, if it has one.
*/
QString Document::copyright() const { return d->copyright; }

/*!
    Returns the names of the extensions the file says it uses.
*/
QStringList Document::extensionsUsed() const { return d->extensionsUsed; }

/*!
    Returns the scenes.
*/
const QList<Scene> &Document::scenes() const { return d->scenes; }

/*!
    Returns the index of the scene to show: the one the file names, or 0 if it names none, or -1
    if there are no scenes.
*/
int Document::defaultScene() const { return d->defaultScene; }

/*!
    Returns the nodes.
*/
const QList<Node> &Document::nodes() const { return d->nodes; }

/*!
    Returns the meshes.
*/
const QList<Mesh> &Document::meshes() const { return d->meshes; }

/*!
    Returns the materials.
*/
const QList<Material> &Document::materials() const { return d->materials; }

/*!
    Returns the textures.
*/
const QList<Texture> &Document::textures() const { return d->textures; }

/*!
    Returns the samplers.
*/
const QList<Sampler> &Document::samplers() const { return d->samplers; }

/*!
    Returns the images.
*/
const QList<Image> &Document::images() const { return d->images; }

/*!
    Returns the cameras.
*/
const QList<Camera> &Document::cameras() const { return d->cameras; }

/*!
    Returns the lights of \c KHR_lights_punctual.
*/
const QList<Light> &Document::lights() const { return d->lights; }

/*!
    Returns the index of the parent of \a node, or -1 if it is a root node or not a valid index.
*/
int Document::parentNode(int node) const {
    if (node < 0 || node >= d->parents.size())
        return -1;
    return d->parents.at(node);
}

/*!
    Returns the transform from the space of \a node to world space, or the identity if \a node is
    not a valid index.

    \sa instances()
*/
QMatrix4x4 Document::worldTransform(int node) const {
    if (node < 0 || node >= d->nodes.size())
        return QMatrix4x4();
    QMatrix4x4 m = d->nodes.at(node).transform;
    for (int p = d->parents.at(node); p >= 0; p = d->parents.at(p))
        m = d->nodes.at(p).transform * m;
    return m;
}

/*!
    Returns every node of \a scene with its transform to world space, parents before children in
    the order of the file. If \a scene is -1, defaultScene() is used; if the document has no scenes,
    all root nodes are. Returns an empty list if \a scene is not a valid index.

    A node that draws a mesh becomes one instance of the mesh; a node with a camera or light places
    it.
*/
QList<Instance> Document::instances(int scene) const {
    QList<int> roots;
    if (scene < 0)
        scene = d->defaultScene;
    if (scene >= 0) {
        if (scene >= d->scenes.size())
            return {};
        roots = d->scenes.at(scene).nodes;
    } else {
        for (qsizetype i = 0; i < d->parents.size(); ++i) {
            if (d->parents.at(i) < 0)
                roots.append(int(i));
        }
    }
    QList<Instance> result;
    QList<Instance> stack;
    for (auto it = roots.crbegin(); it != roots.crend(); ++it)
        stack.append({*it, d->nodes.at(*it).transform});
    while (!stack.isEmpty()) {
        const Instance instance = stack.takeLast();
        result.append(instance);
        const QList<int> &children = d->nodes.at(instance.node).children;
        for (auto it = children.crbegin(); it != children.crend(); ++it)
            stack.append({*it, instance.transform * d->nodes.at(*it).transform});
    }
    return result;
}

} // namespace QtGltf
