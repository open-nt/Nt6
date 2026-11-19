// MIT License - see LICENSE
//
// The mesh model of ntgentree and a writer for binary glTF (.glb). Vertices carry a position, a normal, UV
// coordinates, a colour tint and the wind weights in the application-specific attribute _WIND, which the
// survey (doc/internal/digital-twin-survey.md) names for sway in the vertex shader.
#pragma once

#include "vecmath.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

namespace gentree {

struct Vertex {
    Vec3 p, n;
    float u = 0, v = 0;
    float c[4] = {1, 1, 1, 1};
    // _WIND: x trunk bend weight, y branch flex weight, z leaf flutter weight, w per-branch phase in [0, 1).
    float w[4] = {0, 0, 0, 0};
};

struct Primitive {
    int material = 0;
    std::vector<Vertex> verts;
    std::vector<std::uint32_t> indices;

    std::uint32_t add(const Vertex &v) {
        verts.push_back(v);
        return static_cast<std::uint32_t>(verts.size() - 1);
    }
    void tri(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
        indices.push_back(a);
        indices.push_back(b);
        indices.push_back(c);
    }
    std::size_t triangles() const { return indices.size() / 3; }
};

struct Mesh {
    std::string name;
    std::deque<Primitive> prims; // a deque keeps references to primitives valid as more are added

    std::size_t triangles() const {
        std::size_t n = 0;
        for (const Primitive &p : prims)
            n += p.triangles();
        return n;
    }
    // The primitive for a material, created on first use.
    Primitive &primitive(int material) {
        for (Primitive &p : prims)
            if (p.material == material)
                return p;
        prims.push_back(Primitive());
        prims.back().material = material;
        return prims.back();
    }
};

struct Material {
    std::string name;
    float base[4] = {1, 1, 1, 1};
    float roughness = 0.9f;
    bool doubleSided = false;
    bool mask = false;
    int image = -1;
    bool clamp = false; // clamp to edge instead of repeat
    std::string extras; // a JSON object, or empty
};

struct Image {
    std::string name;
    std::string mime;
    std::vector<std::uint8_t> data;
};

struct Node {
    std::string name;
    int mesh = -1;
    std::vector<int> children;
    bool rotated = false;
    float rotation[4] = {0, 0, 0, 1};
    float translation[3] = {0, 0, 0};
    std::string extras; // a JSON object, or empty
};

struct Document {
    std::string generator;
    std::vector<Material> materials;
    std::vector<Image> images;
    std::vector<Mesh> meshes;
    std::vector<Node> nodes;
    int root = 0;
    std::string sceneExtras; // a JSON object, or empty
};

inline std::string jsonEscape(const std::string &s) {
    std::string o = "\"";
    for (const char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (ch == '"' || ch == '\\') {
            o += '\\';
            o += ch;
        } else if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", c);
            o += buf;
        } else {
            o += ch;
        }
    }
    return o + "\"";
}

inline std::string jsonNumber(double v) {
    char buf[40];
    std::snprintf(buf, sizeof buf, "%.7g", v);
    return buf;
}

// A weight or tint in [0, 1] as a normalised byte.
inline std::uint8_t toByte(float v) {
    return static_cast<std::uint8_t>(std::lround(std::fmin(std::fmax(v, 0.0f), 1.0f) * 255.0f));
}

namespace detail {

class Binary {
public:
    // Appends data aligned to 4 bytes and returns the offset.
    std::size_t append(const void *p, std::size_t n) {
        while (m_data.size() % 4 != 0)
            m_data.push_back(0);
        const std::size_t offset = m_data.size();
        const auto *b = static_cast<const std::uint8_t *>(p);
        m_data.insert(m_data.end(), b, b + n);
        return offset;
    }
    std::vector<std::uint8_t> &data() { return m_data; }

private:
    std::vector<std::uint8_t> m_data;
};

struct Views {
    std::string views, accessors;
    int viewCount = 0, accessorCount = 0;

    // Adds a buffer view and an accessor over the same bytes; returns the accessor index.
    int add(Binary &bin, const void *p, std::size_t bytes, int target, int componentType, std::size_t count,
            const char *type, const std::string &bounds) {
        const std::size_t offset = bin.append(p, bytes);
        if (viewCount++)
            views += ',';
        views += "{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) + ",\"byteLength\":" + std::to_string(bytes) +
                 ",\"target\":" + std::to_string(target) + "}";
        if (accessorCount)
            accessors += ',';
        accessors += "{\"bufferView\":" + std::to_string(viewCount - 1) +
                     ",\"componentType\":" + std::to_string(componentType) + ",\"count\":" + std::to_string(count) +
                     ",\"type\":\"" + type + "\"" + bounds + "}";
        return accessorCount++;
    }
};

inline void putU32(std::vector<std::uint8_t> &o, std::uint32_t v) {
    for (int i = 0; i < 4; ++i)
        o.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xff));
}

} // namespace detail

// Serialises the document as GLB. Each primitive gets its own accessors; images are embedded as they are.
inline std::vector<std::uint8_t> writeGlb(const Document &doc) {
    using namespace detail;
    Binary bin;
    Views vw;

    std::string meshesJson;
    for (std::size_t mi = 0; mi < doc.meshes.size(); ++mi) {
        const Mesh &mesh = doc.meshes[mi];
        if (mi)
            meshesJson += ',';
        meshesJson += "{\"name\":" + jsonEscape(mesh.name) + ",\"primitives\":[";
        bool first = true;
        for (const Primitive &prim : mesh.prims) {
            if (prim.indices.empty())
                continue;
            const std::size_t n = prim.verts.size();
            std::vector<float> pos(n * 3), nrm(n * 3), uv(n * 2);
            std::vector<std::uint8_t> col(n * 4), wind(n * 4);
            float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
            for (std::size_t i = 0; i < n; ++i) {
                const Vertex &v = prim.verts[i];
                const float p[3] = {v.p.x, v.p.y, v.p.z};
                for (int k = 0; k < 3; ++k) {
                    pos[i * 3 + static_cast<std::size_t>(k)] = p[k];
                    lo[k] = std::fmin(lo[k], p[k]);
                    hi[k] = std::fmax(hi[k], p[k]);
                }
                nrm[i * 3] = v.n.x;
                nrm[i * 3 + 1] = v.n.y;
                nrm[i * 3 + 2] = v.n.z;
                uv[i * 2] = v.u;
                uv[i * 2 + 1] = v.v;
                for (std::size_t k = 0; k < 4; ++k) {
                    col[i * 4 + k] = toByte(v.c[k]);
                    wind[i * 4 + k] = toByte(v.w[k]);
                }
            }
            const std::string bounds = ",\"min\":[" + jsonNumber(lo[0]) + "," + jsonNumber(lo[1]) + "," +
                                       jsonNumber(lo[2]) + "],\"max\":[" + jsonNumber(hi[0]) + "," + jsonNumber(hi[1]) +
                                       "," + jsonNumber(hi[2]) + "]";
            constexpr int kArray = 34962, kElement = 34963, kFloat = 5126, kUInt = 5125, kUByte = 5121;
            const int aPos = vw.add(bin, pos.data(), pos.size() * 4, kArray, kFloat, n, "VEC3", bounds);
            const int aNrm = vw.add(bin, nrm.data(), nrm.size() * 4, kArray, kFloat, n, "VEC3", "");
            const int aUv = vw.add(bin, uv.data(), uv.size() * 4, kArray, kFloat, n, "VEC2", "");
            const int aCol = vw.add(bin, col.data(), col.size(), kArray, kUByte, n, "VEC4", ",\"normalized\":true");
            const int aWind = vw.add(bin, wind.data(), wind.size(), kArray, kUByte, n, "VEC4", ",\"normalized\":true");
            const int aIdx = vw.add(bin, prim.indices.data(), prim.indices.size() * 4, kElement, kUInt,
                                    prim.indices.size(), "SCALAR", "");
            if (!first)
                meshesJson += ',';
            first = false;
            meshesJson += "{\"attributes\":{\"POSITION\":" + std::to_string(aPos) +
                          ",\"NORMAL\":" + std::to_string(aNrm) + ",\"TEXCOORD_0\":" + std::to_string(aUv) +
                          ",\"COLOR_0\":" + std::to_string(aCol) + ",\"_WIND\":" + std::to_string(aWind) +
                          "},\"indices\":" + std::to_string(aIdx) + ",\"material\":" + std::to_string(prim.material) +
                          ",\"mode\":4}";
        }
        meshesJson += "]}";
    }

    std::string imagesJson, texturesJson,
        samplersJson = "{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":10497,"
                       "\"wrapT\":10497},{\"magFilter\":9729,\"minFilter\":9987,"
                       "\"wrapS\":33071,\"wrapT\":33071}";
    for (std::size_t i = 0; i < doc.images.size(); ++i) {
        const Image &img = doc.images[i];
        const std::size_t offset = bin.append(img.data.data(), img.data.size());
        if (vw.viewCount++)
            vw.views += ',';
        vw.views += "{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) +
                    ",\"byteLength\":" + std::to_string(img.data.size()) + "}";
        if (i)
            imagesJson += ',';
        imagesJson += "{\"name\":" + jsonEscape(img.name) + ",\"mimeType\":" + jsonEscape(img.mime) +
                      ",\"bufferView\":" + std::to_string(vw.viewCount - 1) + "}";
    }
    // One texture for each material that has an image.
    std::string materialsJson;
    int textureCount = 0;
    for (std::size_t i = 0; i < doc.materials.size(); ++i) {
        const Material &m = doc.materials[i];
        if (i)
            materialsJson += ',';
        materialsJson += "{\"name\":" + jsonEscape(m.name) + ",\"pbrMetallicRoughness\":{\"baseColorFactor\":[" +
                         jsonNumber(m.base[0]) + "," + jsonNumber(m.base[1]) + "," + jsonNumber(m.base[2]) + "," +
                         jsonNumber(m.base[3]) + "]";
        if (m.image >= 0) {
            if (textureCount++)
                texturesJson += ',';
            texturesJson +=
                "{\"sampler\":" + std::string(m.clamp ? "1" : "0") + ",\"source\":" + std::to_string(m.image) + "}";
            materialsJson += ",\"baseColorTexture\":{\"index\":" + std::to_string(textureCount - 1) + "}";
        }
        materialsJson += ",\"metallicFactor\":0,\"roughnessFactor\":" + jsonNumber(m.roughness) + "}";
        materialsJson += std::string(",\"alphaMode\":\"") + (m.mask ? "MASK\",\"alphaCutoff\":0.5" : "OPAQUE\"");
        if (m.doubleSided)
            materialsJson += ",\"doubleSided\":true";
        if (!m.extras.empty())
            materialsJson += ",\"extras\":" + m.extras;
        materialsJson += "}";
    }

    std::string nodesJson;
    for (std::size_t i = 0; i < doc.nodes.size(); ++i) {
        const Node &n = doc.nodes[i];
        if (i)
            nodesJson += ',';
        nodesJson += "{\"name\":" + jsonEscape(n.name);
        if (n.mesh >= 0)
            nodesJson += ",\"mesh\":" + std::to_string(n.mesh);
        if (!n.children.empty()) {
            nodesJson += ",\"children\":[";
            for (std::size_t k = 0; k < n.children.size(); ++k)
                nodesJson += (k ? "," : "") + std::to_string(n.children[k]);
            nodesJson += "]";
        }
        if (n.rotated)
            nodesJson += ",\"rotation\":[" + jsonNumber(n.rotation[0]) + "," + jsonNumber(n.rotation[1]) + "," +
                         jsonNumber(n.rotation[2]) + "," + jsonNumber(n.rotation[3]) + "]";
        if (n.translation[0] != 0 || n.translation[1] != 0 || n.translation[2] != 0)
            nodesJson += ",\"translation\":[" + jsonNumber(n.translation[0]) + "," + jsonNumber(n.translation[1]) +
                         "," + jsonNumber(n.translation[2]) + "]";
        if (!n.extras.empty())
            nodesJson += ",\"extras\":" + n.extras;
        nodesJson += "}";
    }

    std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":" + jsonEscape(doc.generator) + "},";
    json += "\"scene\":0,\"scenes\":[{\"nodes\":[" + std::to_string(doc.root) + "]";
    if (!doc.sceneExtras.empty())
        json += ",\"extras\":" + doc.sceneExtras;
    json += "}],\"nodes\":[" + nodesJson + "],\"meshes\":[" + meshesJson + "],\"materials\":[" + materialsJson + "]";
    if (!doc.images.empty())
        json +=
            ",\"images\":[" + imagesJson + "],\"textures\":[" + texturesJson + "],\"samplers\":[" + samplersJson + "]";
    json += ",\"accessors\":[" + vw.accessors + "],\"bufferViews\":[" + vw.views +
            "],\"buffers\":[{\"byteLength\":" + std::to_string(bin.data().size() + (4 - bin.data().size() % 4) % 4) +
            "}]}";

    while (json.size() % 4 != 0)
        json += ' ';
    std::vector<std::uint8_t> &payload = bin.data();
    while (payload.size() % 4 != 0)
        payload.push_back(0);

    std::vector<std::uint8_t> out;
    out.reserve(28 + json.size() + payload.size());
    putU32(out, 0x46546c67); // "glTF"
    putU32(out, 2);
    putU32(out, static_cast<std::uint32_t>(28 + json.size() + payload.size()));
    putU32(out, static_cast<std::uint32_t>(json.size()));
    putU32(out, 0x4e4f534a); // "JSON"
    out.insert(out.end(), json.begin(), json.end());
    putU32(out, static_cast<std::uint32_t>(payload.size()));
    putU32(out, 0x004e4942); // "BIN"
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

} // namespace gentree
