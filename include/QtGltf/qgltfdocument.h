// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtGui/qimage.h>
#include <QtGui/qmatrix4x4.h>
#include <QtGui/qquaternion.h>
#include <QtGui/qvectornd.h>

#include <limits>

namespace QtGltf {

struct TextureInfo {
    int index = -1;
    int texCoord = 0;
    float scale = 1.0f;
    QVector2D uvOffset;
    float uvRotation = 0.0f;
    QVector2D uvScale = QVector2D(1.0f, 1.0f);

    bool isValid() const { return index >= 0; }
};

struct Material {
    enum AlphaMode { Opaque, Mask, Blend };

    QString name;
    QVector4D baseColorFactor = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
    TextureInfo baseColorTexture;
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    TextureInfo metallicRoughnessTexture;
    TextureInfo normalTexture;
    TextureInfo occlusionTexture;
    TextureInfo emissiveTexture;
    QVector3D emissiveFactor;
    float emissiveStrength = 1.0f;
    AlphaMode alphaMode = Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
    bool unlit = false;
    float ior = 1.5f;
    float transmissionFactor = 0.0f;
    TextureInfo transmissionTexture;
    float thicknessFactor = 0.0f;
    TextureInfo thicknessTexture;
    QVector3D attenuationColor = QVector3D(1.0f, 1.0f, 1.0f);
    float attenuationDistance = std::numeric_limits<float>::infinity();
};

struct Sampler {
    enum Filter {
        DefaultFilter = 0,
        Nearest = 9728,
        Linear = 9729,
        NearestMipmapNearest = 9984,
        LinearMipmapNearest = 9985,
        NearestMipmapLinear = 9986,
        LinearMipmapLinear = 9987
    };
    enum Wrap { Repeat = 10497, ClampToEdge = 33071, MirroredRepeat = 33648 };

    QString name;
    Filter magFilter = DefaultFilter;
    Filter minFilter = DefaultFilter;
    Wrap wrapS = Repeat;
    Wrap wrapT = Repeat;
};

struct Texture {
    QString name;
    int source = -1;
    int sampler = -1;
};

struct Image {
    QString name;
    QString mimeType;
    QString fileName;
    QByteArray data;

    QByteArray encodedData() const;
    QImage toImage() const;
};

struct Primitive {
    enum Mode { Points, Lines, Triangles };

    Mode mode = Triangles;
    int material = -1;
    QList<QVector3D> positions;
    QList<QVector3D> normals;
    QList<QVector4D> tangents;
    QList<QVector2D> texCoords0;
    QList<QVector2D> texCoords1;
    QList<QVector4D> colors;
    QList<quint32> indices;
    QVector3D boundsMin;
    QVector3D boundsMax;
};

struct Mesh {
    QString name;
    QList<Primitive> primitives;
};

struct Camera {
    enum Type { Perspective, Orthographic };

    QString name;
    Type type = Perspective;
    float yfov = 0.8f;
    float aspectRatio = 0.0f;
    float xmag = 1.0f;
    float ymag = 1.0f;
    float znear = 0.01f;
    float zfar = 0.0f;
};

struct Light {
    enum Type { Directional, Point, Spot };

    QString name;
    Type type = Point;
    QVector3D color = QVector3D(1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;
    float range = 0.0f;
    float innerConeAngle = 0.0f;
    float outerConeAngle = 0.785398163f;
};

struct Node {
    QString name;
    int mesh = -1;
    int camera = -1;
    int light = -1;
    QList<int> children;
    QMatrix4x4 transform;
};

struct Scene {
    QString name;
    QList<int> nodes;
};

struct Instance {
    int node = -1;
    QMatrix4x4 transform;
};

class DocumentData;

class Document {
public:
    enum Error { NoError, FileError, ParseError, VersionError, ExtensionError, DataError };

    Document();
    Document(const Document &other);
    Document(Document &&other) noexcept;
    Document &operator=(const Document &other);
    Document &operator=(Document &&other) noexcept;
    ~Document();

    bool load(const QString &fileName);
    bool loadFromData(const QByteArray &data, const QString &baseDirectory = QString());

    bool isValid() const;
    Error error() const;
    QString errorString() const;

    QString generator() const;
    QString copyright() const;
    QStringList extensionsUsed() const;

    const QList<Scene> &scenes() const;
    int defaultScene() const;
    const QList<Node> &nodes() const;
    const QList<Mesh> &meshes() const;
    const QList<Material> &materials() const;
    const QList<Texture> &textures() const;
    const QList<Sampler> &samplers() const;
    const QList<Image> &images() const;
    const QList<Camera> &cameras() const;
    const QList<Light> &lights() const;

    int parentNode(int node) const;
    QMatrix4x4 worldTransform(int node) const;
    QList<Instance> instances(int scene = -1) const;

private:
    QSharedDataPointer<DocumentData> d;
};

} // namespace QtGltf
