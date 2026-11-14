// MIT License - see LICENSE
//
// Nt glTF is tested with documents built in memory: JSON whose buffers are base64 data URIs, the
// same scene packed as GLB with an embedded PNG, and files with external buffers and images in a
// temporary directory. The error tests break one thing at a time in an otherwise valid document.
#include <QtCore>
#include <QtGltf>

#include <cmath>
#include <cstdio>
#include <cstring>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using namespace QtGltf;

namespace {

bool fuzzyEqual(float a, float b) { return std::fabs(a - b) < 1e-5f; }
bool fuzzyEqual(const QVector3D &a, const QVector3D &b) {
    return fuzzyEqual(a.x(), b.x()) && fuzzyEqual(a.y(), b.y()) && fuzzyEqual(a.z(), b.z());
}

// Little-endian binary buffer builder.
class Bytes {
public:
    Bytes &f32(float v) {
        quint32 bits = 0;
        std::memcpy(&bits, &v, sizeof bits);
        return u32(bits);
    }
    Bytes &u32(quint32 v) {
        for (int i = 0; i < 4; ++i)
            data.append(char((v >> (8 * i)) & 0xff));
        return *this;
    }
    Bytes &u16(quint16 v) {
        data.append(char(v & 0xff));
        data.append(char(v >> 8));
        return *this;
    }
    Bytes &u8(quint8 v) {
        data.append(char(v));
        return *this;
    }
    Bytes &pad() {
        while (data.size() % 4)
            data.append('\0');
        return *this;
    }
    QByteArray data;
};

QString dataUri(const QByteArray &bytes) {
    return QStringLiteral("data:application/octet-stream;base64,") + QString::fromLatin1(bytes.toBase64());
}

// One triangle: three float positions (36 bytes) then three unsigned short indices.
QByteArray triangleBuffer() {
    Bytes b;
    b.f32(0).f32(0).f32(0);
    b.f32(1).f32(0).f32(0);
    b.f32(0).f32(1).f32(0);
    b.u16(0).u16(1).u16(2).pad();
    return b.data;
}

QByteArray triangleJson(const QString &bufferEntry, const QString &extra = QString()) {
    return QStringLiteral(R"({
        "asset": {"version": "2.0", "generator": "nt test"},
        "buffers": [%1],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 36},
            {"buffer": 0, "byteOffset": 36, "byteLength": 6}
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"}
        ],
        "meshes": [{"name": "tri", "primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}],
        "nodes": [{"mesh": 0}],
        "scenes": [{"nodes": [0]}]
        %2
    })")
        .arg(bufferEntry, extra)
        .toUtf8();
}

QByteArray dataUriTriangle() {
    return triangleJson(QStringLiteral(R"({"byteLength": 44, "uri": "%1"})").arg(dataUri(triangleBuffer())));
}

QByteArray glb(const QByteArray &json, const QByteArray &bin) {
    QByteArray j = json;
    while (j.size() % 4)
        j.append(' ');
    QByteArray b = bin;
    while (b.size() % 4)
        b.append('\0');
    Bytes out;
    out.u32(0x46546C67).u32(2).u32(quint32(12 + 8 + j.size() + (b.isEmpty() ? 0 : 8 + b.size())));
    out.u32(quint32(j.size())).u32(0x4E4F534A);
    out.data.append(j);
    if (!b.isEmpty()) {
        out.u32(quint32(b.size())).u32(0x004E4942);
        out.data.append(b);
    }
    return out.data;
}

QByteArray pngBytes() {
    QImage image(2, 2, QImage::Format_RGB32);
    image.fill(qRgb(255, 0, 0));
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return buffer.data();
}

// Loads JSON and returns the error; for the error tests. A load whose result disagrees with its
// error counts as a failure.
Document::Error loadError(const QByteArray &json, const QString &baseDirectory = QString()) {
    Document doc;
    const bool ok = doc.loadFromData(json, baseDirectory);
    CHECK(ok == (doc.error() == Document::NoError));
    CHECK(ok == doc.isValid());
    CHECK(ok == doc.errorString().isEmpty());
    return doc.error();
}

void testTriangle() {
    Document doc;
    CHECK(!doc.isValid());
    CHECK(doc.loadFromData(dataUriTriangle()));
    CHECK(doc.isValid());
    CHECK(doc.error() == Document::NoError);
    CHECK(doc.errorString().isEmpty());
    CHECK(doc.generator() == QLatin1StringView("nt test"));
    CHECK(doc.meshes().size() == 1);
    CHECK(doc.meshes().at(0).name == QLatin1StringView("tri"));
    const Primitive &p = doc.meshes().at(0).primitives.at(0);
    CHECK(p.mode == Primitive::Triangles);
    CHECK(p.material == -1);
    CHECK(p.positions.size() == 3);
    CHECK(fuzzyEqual(p.positions.at(1), QVector3D(1, 0, 0)));
    CHECK(p.normals.isEmpty());
    CHECK(p.texCoords0.isEmpty());
    CHECK((p.indices == QList<quint32>{0, 1, 2}));
    CHECK(fuzzyEqual(p.boundsMin, QVector3D(0, 0, 0)));
    CHECK(fuzzyEqual(p.boundsMax, QVector3D(1, 1, 0)));
    CHECK(doc.defaultScene() == 0);

    // Copies share the data and survive a reload of the original.
    Document copy = doc;
    CHECK(doc.loadFromData("not json") == false);
    CHECK(!doc.isValid());
    CHECK(doc.meshes().isEmpty());
    CHECK(copy.isValid());
    CHECK(copy.meshes().at(0).primitives.at(0).positions.size() == 3);
}

void testGlb() {
    const QByteArray png = pngBytes();
    CHECK(!png.isEmpty());
    QByteArray bin = triangleBuffer();
    const qsizetype imageOffset = bin.size();
    bin.append(png);
    QByteArray json = triangleJson(QStringLiteral(R"({"byteLength": %1})").arg(bin.size()), QStringLiteral(R"(,
        "images": [{"bufferView": 2, "mimeType": "image/png", "name": "red"}],
        "textures": [{"source": 0}],
        "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}])"));
    json.replace(R"({"buffer": 0, "byteOffset": 36, "byteLength": 6})",
                 QStringLiteral(R"({"buffer": 0, "byteOffset": 36, "byteLength": 6},
                                   {"buffer": 0, "byteOffset": %1, "byteLength": %2})")
                     .arg(imageOffset)
                     .arg(png.size())
                     .toUtf8());

    Document doc;
    CHECK(doc.loadFromData(glb(json, bin)));
    CHECK(doc.errorString().isEmpty());
    CHECK(doc.meshes().size() == 1);
    CHECK(doc.images().size() == 1);
    const Image &image = doc.images().at(0);
    CHECK(image.name == QLatin1StringView("red"));
    CHECK(image.mimeType == QLatin1StringView("image/png"));
    CHECK(image.fileName.isEmpty());
    CHECK(image.encodedData() == png);
    const QImage decoded = image.toImage();
    CHECK(decoded.size() == QSize(2, 2));
    CHECK(decoded.pixel(1, 1) == qRgb(255, 0, 0));
    CHECK(doc.materials().at(0).baseColorTexture.index == 0);

    // A truncated container, a wrong version and a missing JSON chunk.
    QByteArray bad = glb(json, bin);
    CHECK(loadError(bad.left(bad.size() - 10)) == Document::ParseError);
    bad = glb(json, bin);
    bad[4] = 1;
    CHECK(loadError(bad) == Document::VersionError);
    bad = glb(json, bin);
    bad[16] = 'X';
    CHECK(loadError(bad) == Document::ParseError);
    // A buffer without a URI needs the BIN chunk.
    CHECK(loadError(glb(json, QByteArray())) == Document::DataError);
}

void testQuantizedAndSparse() {
    // Positions as normalized signed shorts with a byte stride of 8, texture coordinates as
    // normalized unsigned bytes, colours as RGB floats, and a sparse accessor that moves vertex 2.
    Bytes b;
    b.u16(0).u16(0).u16(0).u16(0xffff);                               // (0, 0, 0) and padding
    b.u16(32767).u16(0).u16(0).u16(0xffff);                           // (1, 0, 0)
    b.u16(0).u16(0x8001).u16(0).u16(0xffff);                          // (0, -1, 0)
    b.u8(0).u8(0).u8(255).u8(0).u8(0).u8(255);                        // (0,0) (1,0) (0,1)
    b.pad();                                                          // offset 32
    b.f32(1).f32(0).f32(0).f32(0).f32(1).f32(0).f32(0).f32(0).f32(1); // offset 32: colours
    b.u8(2).pad();                                                    // offset 68: sparse index
    b.f32(5).f32(6).f32(7);                                           // offset 72: sparse value
    const QByteArray json = QStringLiteral(R"({
        "asset": {"version": "2.0"},
        "extensionsRequired": ["KHR_mesh_quantization"],
        "extensionsUsed": ["KHR_mesh_quantization"],
        "buffers": [{"byteLength": %1, "uri": "%2"}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": 24, "byteStride": 8},
            {"buffer": 0, "byteOffset": 24, "byteLength": 6},
            {"buffer": 0, "byteOffset": 32, "byteLength": 36},
            {"buffer": 0, "byteOffset": 68, "byteLength": 1},
            {"buffer": 0, "byteOffset": 72, "byteLength": 12}
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5122, "normalized": true, "count": 3, "type": "VEC3"},
            {"bufferView": 1, "componentType": 5121, "normalized": true, "count": 3, "type": "VEC2"},
            {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"componentType": 5126, "count": 3, "type": "VEC3",
             "sparse": {"count": 1, "indices": {"bufferView": 3, "componentType": 5121},
                        "values": {"bufferView": 4}}}
        ],
        "meshes": [
            {"primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 1, "COLOR_0": 2}}]},
            {"primitives": [{"attributes": {"POSITION": 3}}, {"attributes": {"NORMAL": 2}}]}
        ]
    })")
                                .arg(b.data.size())
                                .arg(dataUri(b.data))
                                .toUtf8();
    Document doc;
    CHECK(doc.loadFromData(json));
    CHECK(doc.errorString().isEmpty());
    CHECK(doc.extensionsUsed() == QStringList{QStringLiteral("KHR_mesh_quantization")});
    if (doc.meshes().size() != 2)
        return;
    const Primitive &p = doc.meshes().at(0).primitives.at(0);
    CHECK(fuzzyEqual(p.positions.at(1), QVector3D(1, 0, 0)));
    CHECK(fuzzyEqual(p.positions.at(2), QVector3D(0, -1, 0)));
    CHECK(p.texCoords0.size() == 3);
    CHECK(fuzzyEqual(p.texCoords0.at(1).x(), 1.0f) && fuzzyEqual(p.texCoords0.at(1).y(), 0.0f));
    CHECK(fuzzyEqual(p.texCoords0.at(2).y(), 1.0f));
    CHECK(p.colors.size() == 3);
    CHECK(p.colors.at(1) == QVector4D(0, 1, 0, 1));
    CHECK((p.indices == QList<quint32>{0, 1, 2}));

    // The sparse accessor has no bufferView: zeros, with vertex 2 replaced.
    const Mesh &sparse = doc.meshes().at(1);
    CHECK(sparse.primitives.size() == 1); // the primitive without POSITION is left out
    CHECK(fuzzyEqual(sparse.primitives.at(0).positions.at(0), QVector3D(0, 0, 0)));
    CHECK(fuzzyEqual(sparse.primitives.at(0).positions.at(2), QVector3D(5, 6, 7)));
}

// Builds a document whose one primitive has six vertices, no indices and the given mode.
QByteArray modeJson(int mode) {
    Bytes b;
    for (int i = 0; i < 6; ++i)
        b.f32(float(i)).f32(float(i % 2)).f32(0);
    return QStringLiteral(R"({
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": 72, "uri": "%1"}],
        "bufferViews": [{"buffer": 0, "byteLength": 72}],
        "accessors": [{"bufferView": 0, "componentType": 5126, "count": 6, "type": "VEC3"}],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "mode": %2}]}]
    })")
        .arg(dataUri(b.data))
        .arg(mode)
        .toUtf8();
}

QList<quint32> modeIndices(int mode, Primitive::Mode *kind) {
    Document doc;
    if (!doc.loadFromData(modeJson(mode)))
        return {9999};
    *kind = doc.meshes().at(0).primitives.at(0).mode;
    return doc.meshes().at(0).primitives.at(0).indices;
}

void testModes() {
    Primitive::Mode kind = Primitive::Points;
    CHECK((modeIndices(0, &kind) == QList<quint32>{0, 1, 2, 3, 4, 5}));
    CHECK(kind == Primitive::Points);
    CHECK((modeIndices(1, &kind) == QList<quint32>{0, 1, 2, 3, 4, 5}));
    CHECK(kind == Primitive::Lines);
    CHECK((modeIndices(2, &kind) == QList<quint32>{0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 0}));
    CHECK(kind == Primitive::Lines);
    CHECK((modeIndices(3, &kind) == QList<quint32>{0, 1, 1, 2, 2, 3, 3, 4, 4, 5}));
    CHECK((modeIndices(4, &kind) == QList<quint32>{0, 1, 2, 3, 4, 5}));
    CHECK(kind == Primitive::Triangles);
    // Strips alternate the winding so every triangle faces the same way.
    CHECK((modeIndices(5, &kind) == QList<quint32>{0, 1, 2, 1, 3, 2, 2, 3, 4, 3, 5, 4}));
    CHECK(kind == Primitive::Triangles);
    CHECK((modeIndices(6, &kind) == QList<quint32>{1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5, 0}));
    CHECK(loadError(modeJson(7)) == Document::DataError);
}

void testHierarchy() {
    // Node 0 translates by (10, 0, 0) and has children 1 (rotated 90 degrees about Z, scaled by 2)
    // and 2 (a column-major matrix translating by (0, 0, 5)); node 3 is a child of node 1.
    const QByteArray json = R"({
        "asset": {"version": "2.0"},
        "scene": 1,
        "nodes": [
            {"name": "root", "translation": [10, 0, 0], "children": [1, 2]},
            {"rotation": [0, 0, 0.7071068, 0.7071068], "scale": [2, 2, 2], "children": [3]},
            {"matrix": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,5,1]},
            {"translation": [1, 0, 0]},
            {"name": "lonely"}
        ],
        "scenes": [{"nodes": [4]}, {"name": "main", "nodes": [0]}]
    })";
    Document doc;
    CHECK(doc.loadFromData(json));
    CHECK(doc.errorString().isEmpty());
    CHECK(doc.defaultScene() == 1);
    CHECK(doc.scenes().at(1).name == QLatin1StringView("main"));
    CHECK(doc.parentNode(0) == -1);
    CHECK(doc.parentNode(3) == 1);
    CHECK(doc.parentNode(99) == -1);

    // Node 3's origin: (1,0,0) scaled by 2 is (2,0,0), rotated about Z is (0,2,0), then moved by 10.
    CHECK(fuzzyEqual(doc.worldTransform(3).map(QVector3D()), QVector3D(10, 2, 0)));
    CHECK(fuzzyEqual(doc.worldTransform(2).map(QVector3D()), QVector3D(10, 0, 5)));
    CHECK(doc.worldTransform(-1).isIdentity());

    const QList<Instance> instances = doc.instances();
    CHECK(instances.size() == 4);
    if (instances.size() == 4) {
        CHECK(instances.at(0).node == 0);
        CHECK(instances.at(1).node == 1);
        CHECK(instances.at(2).node == 3);
        CHECK(instances.at(3).node == 2);
        CHECK(fuzzyEqual(instances.at(2).transform.map(QVector3D()), QVector3D(10, 2, 0)));
    }
    CHECK(doc.instances(0).size() == 1);
    CHECK(doc.instances(0).at(0).node == 4);
    CHECK(doc.instances(5).isEmpty());

    // Without scenes, every root node is an instance.
    Document noScenes;
    CHECK(noScenes.loadFromData(R"({"asset": {"version": "2.0"}, "nodes": [{"children": [1]}, {}, {}]})"));
    CHECK(noScenes.defaultScene() == -1);
    CHECK(noScenes.instances().size() == 3);
}

void testMaterialsCamerasLights() {
    const QByteArray json = R"({
        "asset": {"version": "2.1", "copyright": "CC0"},
        "extensionsRequired": ["KHR_lights_punctual", "KHR_materials_unlit", "KHR_texture_transform"],
        "images": [{"uri": "textures/base%20color.png"}, {"uri": "x.ktx2", "mimeType": "image/ktx2"}],
        "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 33071, "wrapT": 33648}, {"magFilter": 1}],
        "textures": [{"source": 0, "sampler": 0}, {"extensions": {"KHR_texture_basisu": {"source": 1}}}],
        "materials": [
            {"name": "m",
             "pbrMetallicRoughness": {"baseColorFactor": [0.5, 0.25, 1, 0.75],
                                      "baseColorTexture": {"index": 0, "texCoord": 1},
                                      "metallicFactor": 0, "roughnessFactor": 0.3,
                                      "metallicRoughnessTexture": {"index": 1}},
             "normalTexture": {"index": 1, "scale": 0.5,
                               "extensions": {"KHR_texture_transform": {"offset": [0.25, 0.5], "rotation": 1.5,
                                                                        "scale": [5, 4], "texCoord": 1}}},
             "occlusionTexture": {"index": 0, "strength": 0.25},
             "emissiveFactor": [1, 0.5, 0],
             "alphaMode": "MASK", "alphaCutoff": 0.3, "doubleSided": true,
             "extensions": {"KHR_materials_emissive_strength": {"emissiveStrength": 4},
                            "KHR_materials_unlit": {}}},
            {"extensions": {"KHR_materials_ior": {"ior": 1.33},
                            "KHR_materials_transmission": {"transmissionFactor": 0.75,
                                                           "transmissionTexture": {"index": 1}},
                            "KHR_materials_volume": {"thicknessFactor": 0.1, "thicknessTexture": {"index": 0},
                                                     "attenuationColor": [0.8, 0.95, 1],
                                                     "attenuationDistance": 0.5}}},
            {}
        ],
        "cameras": [
            {"type": "perspective", "perspective": {"yfov": 0.5, "aspectRatio": 1.5, "znear": 0.1}},
            {"type": "orthographic", "orthographic": {"xmag": 2, "ymag": 3, "znear": 0, "zfar": 50}}
        ],
        "extensions": {"KHR_lights_punctual": {"lights": [
            {"type": "spot", "color": [1, 0.5, 0.25], "intensity": 20, "range": 8,
             "spot": {"innerConeAngle": 0.1, "outerConeAngle": 0.4}},
            {"type": "directional"}
        ]}},
        "nodes": [{"camera": 1, "extensions": {"KHR_lights_punctual": {"light": 0}}}]
    })";
    Document doc;
    CHECK(doc.loadFromData(json, QStringLiteral("/scenes/city")));
    CHECK(doc.errorString().isEmpty());
    CHECK(doc.copyright() == QLatin1StringView("CC0"));

    CHECK(doc.images().size() == 2);
    CHECK(doc.images().at(0).fileName == QLatin1StringView("/scenes/city/textures/base color.png"));
    CHECK(doc.images().at(0).data.isEmpty());
    CHECK(doc.images().at(1).mimeType == QLatin1StringView("image/ktx2"));
    CHECK(doc.images().at(0).encodedData().isEmpty()); // the file does not exist
    CHECK(doc.images().at(0).toImage().isNull());

    CHECK(doc.samplers().at(0).magFilter == Sampler::Linear);
    CHECK(doc.samplers().at(0).minFilter == Sampler::LinearMipmapLinear);
    CHECK(doc.samplers().at(0).wrapS == Sampler::ClampToEdge);
    CHECK(doc.samplers().at(0).wrapT == Sampler::MirroredRepeat);
    CHECK(doc.samplers().at(1).magFilter == Sampler::DefaultFilter);
    CHECK(doc.samplers().at(1).wrapS == Sampler::Repeat);
    CHECK(doc.textures().at(0).sampler == 0);
    CHECK(doc.textures().at(1).source == 1);
    CHECK(doc.textures().at(1).sampler == -1);

    const Material &m = doc.materials().at(0);
    CHECK(m.name == QLatin1StringView("m"));
    CHECK(m.baseColorFactor == QVector4D(0.5f, 0.25f, 1.0f, 0.75f));
    CHECK(m.baseColorTexture.isValid());
    CHECK(m.baseColorTexture.texCoord == 1);
    CHECK(fuzzyEqual(m.metallicFactor, 0.0f));
    CHECK(fuzzyEqual(m.roughnessFactor, 0.3f));
    CHECK(m.metallicRoughnessTexture.index == 1);
    CHECK(fuzzyEqual(m.normalTexture.scale, 0.5f));
    CHECK(m.normalTexture.uvOffset == QVector2D(0.25f, 0.5f) && fuzzyEqual(m.normalTexture.uvRotation, 1.5f));
    CHECK(m.normalTexture.uvScale == QVector2D(5.0f, 4.0f) && m.normalTexture.texCoord == 1);
    CHECK(m.baseColorTexture.uvScale == QVector2D(1.0f, 1.0f) && m.baseColorTexture.uvRotation == 0.0f);
    CHECK(fuzzyEqual(m.occlusionTexture.scale, 0.25f));
    CHECK(!m.emissiveTexture.isValid());
    CHECK(m.emissiveFactor == QVector3D(1, 0.5f, 0));
    CHECK(fuzzyEqual(m.emissiveStrength, 4.0f));
    CHECK(m.alphaMode == Material::Mask);
    CHECK(fuzzyEqual(m.alphaCutoff, 0.3f));
    CHECK(m.doubleSided);
    CHECK(m.unlit);
    const Material &defaults = doc.materials().at(2);
    CHECK(defaults.baseColorFactor == QVector4D(1, 1, 1, 1));
    CHECK(fuzzyEqual(defaults.metallicFactor, 1.0f));
    CHECK(defaults.alphaMode == Material::Opaque);
    CHECK(!defaults.unlit && !defaults.doubleSided);
    CHECK(fuzzyEqual(defaults.ior, 1.5f) && defaults.transmissionFactor == 0.0f && defaults.thicknessFactor == 0.0f);
    CHECK(std::isinf(defaults.attenuationDistance) && defaults.attenuationColor == QVector3D(1, 1, 1));
    const Material &glass = doc.materials().at(1);
    CHECK(fuzzyEqual(glass.ior, 1.33f));
    CHECK(fuzzyEqual(glass.transmissionFactor, 0.75f) && glass.transmissionTexture.index == 1);
    CHECK(fuzzyEqual(glass.thicknessFactor, 0.1f) && glass.thicknessTexture.index == 0);
    CHECK(glass.attenuationColor == QVector3D(0.8f, 0.95f, 1.0f) && fuzzyEqual(glass.attenuationDistance, 0.5f));

    CHECK(doc.cameras().at(0).type == Camera::Perspective);
    CHECK(fuzzyEqual(doc.cameras().at(0).yfov, 0.5f));
    CHECK(fuzzyEqual(doc.cameras().at(0).aspectRatio, 1.5f));
    CHECK(fuzzyEqual(doc.cameras().at(0).zfar, 0.0f));
    CHECK(doc.cameras().at(1).type == Camera::Orthographic);
    CHECK(fuzzyEqual(doc.cameras().at(1).ymag, 3.0f));
    CHECK(fuzzyEqual(doc.cameras().at(1).zfar, 50.0f));

    CHECK(doc.lights().size() == 2);
    const Light &spot = doc.lights().at(0);
    CHECK(spot.type == Light::Spot);
    CHECK(spot.color == QVector3D(1, 0.5f, 0.25f));
    CHECK(fuzzyEqual(spot.intensity, 20.0f));
    CHECK(fuzzyEqual(spot.range, 8.0f));
    CHECK(fuzzyEqual(spot.outerConeAngle, 0.4f));
    CHECK(doc.lights().at(1).type == Light::Directional);
    CHECK(fuzzyEqual(doc.lights().at(1).intensity, 1.0f));
    CHECK(doc.nodes().at(0).camera == 1);
    CHECK(doc.nodes().at(0).light == 0);
}

void testExternalFiles() {
    QTemporaryDir dir;
    CHECK(dir.isValid());
    {
        QFile bin(dir.filePath(QStringLiteral("tri.bin")));
        CHECK(bin.open(QIODevice::WriteOnly));
        bin.write(triangleBuffer());
        QFile png(dir.filePath(QStringLiteral("red.png")));
        CHECK(png.open(QIODevice::WriteOnly));
        png.write(pngBytes());
        QFile gltf(dir.filePath(QStringLiteral("tri.gltf")));
        CHECK(gltf.open(QIODevice::WriteOnly));
        gltf.write(triangleJson(QStringLiteral(R"({"byteLength": 44, "uri": "tri.bin"})"),
                                QStringLiteral(R"(, "images": [{"uri": "red.png"}])")));
    }
    Document doc;
    CHECK(doc.load(dir.filePath(QStringLiteral("tri.gltf"))));
    CHECK(doc.errorString().isEmpty());
    CHECK(doc.meshes().size() == 1);
    CHECK(doc.images().at(0).fileName == dir.filePath(QStringLiteral("red.png")));
    CHECK(doc.images().at(0).toImage().size() == QSize(2, 2));

    // The same file read from memory with no base directory cannot find tri.bin.
    QFile gltf(dir.filePath(QStringLiteral("tri.gltf")));
    CHECK(gltf.open(QIODevice::ReadOnly));
    const QByteArray json = gltf.readAll();
    CHECK(loadError(json) == Document::FileError);
    CHECK(loadError(json, dir.path()) == Document::NoError);

    CHECK(!doc.load(dir.filePath(QStringLiteral("missing.gltf"))));
    CHECK(doc.error() == Document::FileError);
    CHECK(doc.errorString().contains(QLatin1StringView("missing.gltf")));
    CHECK(doc.meshes().isEmpty());
}

void testErrors() {
    const QByteArray good = dataUriTriangle();
    CHECK(loadError(good) == Document::NoError);
    CHECK(loadError("{") == Document::ParseError);
    CHECK(loadError("[]") == Document::ParseError);
    CHECK(loadError(R"({"asset": {}})") == Document::ParseError);
    CHECK(loadError(R"({"asset": {"version": "1.0"}})") == Document::VersionError);
    CHECK(loadError(R"({"asset": {"version": "2.0", "minVersion": "2.1"}})") == Document::VersionError);
    CHECK(loadError(R"({"asset": {"version": "2.0"}, "extensionsRequired": ["KHR_draco_mesh_compression"]})") ==
          Document::ExtensionError);
    // An extension that is only used, not required, does not stop loading.
    CHECK(loadError(R"({"asset": {"version": "2.0"}, "extensionsUsed": ["KHR_draco_mesh_compression"]})") ==
          Document::NoError);

    auto broken = [&](const char *from, const char *to) {
        QByteArray json = good;
        json.replace(from, to);
        CHECK(json != good);
        return loadError(json);
    };
    CHECK(broken(R"("indices": 1)", R"("indices": 2)") == Document::DataError);
    CHECK(broken(R"("indices": 1)", R"("indices": 0)") == Document::DataError);           // not a scalar
    CHECK(broken(R"("indices": 1)", R"("indices": 1.5)") == Document::DataError);         // not an integer
    CHECK(broken(R"("byteLength": 36})", R"("byteLength": 60})") == Document::DataError); // past the buffer
    CHECK(broken(R"("count": 3, "type": "VEC3")", R"("count": 4, "type": "VEC3")") == Document::DataError);
    CHECK(broken(R"("count": 3, "type": "VEC3")", R"("count": 3, "type": "VEC5")") == Document::DataError);
    CHECK(broken(R"("componentType": 5126)", R"("componentType": 5124)") == Document::DataError);
    CHECK(broken(R"("byteLength": 44)", R"("byteLength": 64)") == Document::DataError); // URI holds 44 bytes
    CHECK(broken("data:application/octet-stream;base64,", "data:application/octet-stream;base64,!!") ==
          Document::DataError);
    CHECK(broken(R"("nodes": [{"mesh": 0}])", R"("nodes": [{"mesh": 3}])") == Document::DataError);
    CHECK(broken(R"("nodes": [{"mesh": 0}])", R"("nodes": [{"mesh": 0, "children": [0]}])") == Document::DataError);
    CHECK(broken(R"("nodes": [{"mesh": 0}])", R"("nodes": [{"children": [1]}, {"children": [0]}])") ==
          Document::DataError); // a cycle with no root
    CHECK(broken(R"("nodes": [{"mesh": 0}])", R"("nodes": [{"children": [2]}, {"children": [2]}, {}])") ==
          Document::DataError); // two parents
    CHECK(broken(R"("scenes": [{"nodes": [0]}])", R"("scenes": [{"nodes": [0]}], "scene": 1)") == Document::DataError);
    CHECK(broken(R"("nodes": [{"mesh": 0}])", R"("nodes": [{"translation": [1, 2]}])") == Document::DataError);

    // URIs must be relative paths.
    const QByteArray external = triangleJson(QStringLiteral(R"({"byteLength": 44, "uri": "/etc/passwd"})"));
    CHECK(loadError(external) == Document::FileError);
    const QByteArray remote = triangleJson(QStringLiteral(R"({"byteLength": 44, "uri": "http://example.com/a"})"));
    CHECK(loadError(remote) == Document::FileError);

    Document doc;
    CHECK(!doc.loadFromData(good.left(good.size() / 2)));
    CHECK(doc.error() == Document::ParseError);
    CHECK(!doc.errorString().isEmpty());
    CHECK(!doc.isValid());

    QByteArray outOfRange = good;
    outOfRange.replace(R"("nodes": [{"mesh": 0}])", R"("nodes": [{"mesh": 3}])");
    CHECK(!doc.loadFromData(outOfRange));
    CHECK(doc.errorString() == QLatin1StringView("node 0: mesh is not a valid index"));
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    testTriangle();
    testGlb();
    testQuantizedAndSparse();
    testModes();
    testHierarchy();
    testMaterialsCamerasLights();
    testExternalFiles();
    testErrors();
    if (g_failures == 0)
        std::printf("nt_gltf_tests: all passed\n");
    return g_failures == 0 ? 0 : 1;
}
