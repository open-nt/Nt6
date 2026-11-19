// MIT License - see LICENSE
//
// Geometry helpers of ntgentree: tubes for stems and branches, strips for leaves and blades, lathes for crowns,
// and the lookup of textures in a directory. Textures are embedded in the glTF as they are, so the generator needs
// no image decoder.
#pragma once

#include "glb.h"
#include "species.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace gentree {

inline float fl(std::size_t v) { return static_cast<float>(v); }
inline float fl(int v) { return static_cast<float>(v); }

struct TubePoint {
    Vec3 p;
    float r = 0;
    float windX = 0; // trunk bend weight
    float windY = 0; // branch flex weight
};

// Adds a tube along the points. The texture runs u around the tube, wrapped uWraps times, and v along it with one
// repeat for each vTile metres.
inline void addTube(Primitive &prim, const std::vector<TubePoint> &pts, int sides, float uWraps, float vTile,
                    const float tint[3], float phase) {
    const std::size_t n = pts.size();
    if (n < 2 || sides < 3)
        return;
    Vec3 normal = perpendicular(normalize(pts[1].p - pts[0].p));
    float travelled = 0;
    const std::uint32_t first = static_cast<std::uint32_t>(prim.verts.size());
    for (std::size_t i = 0; i < n; ++i) {
        const Vec3 a = pts[i == 0 ? 0 : i - 1].p;
        const Vec3 b = pts[i + 1 < n ? i + 1 : i].p;
        const Vec3 t = normalize(b - a);
        normal = normalize(normal - t * dot(normal, t));
        const Vec3 binormal = cross(t, normal);
        if (i > 0)
            travelled += length(pts[i].p - pts[i - 1].p);
        for (int j = 0; j <= sides; ++j) {
            const float ang = 2.0f * kPi * fl(j) / fl(sides);
            const Vec3 dir = normal * std::cos(ang) + binormal * std::sin(ang);
            Vertex v;
            v.p = pts[i].p + dir * pts[i].r;
            v.n = dir;
            v.u = uWraps * fl(j) / fl(sides);
            v.v = travelled / vTile;
            v.c[0] = tint[0];
            v.c[1] = tint[1];
            v.c[2] = tint[2];
            v.w[0] = pts[i].windX;
            v.w[1] = pts[i].windY;
            v.w[2] = 0;
            v.w[3] = phase;
            prim.add(v);
        }
    }
    const std::uint32_t stride = static_cast<std::uint32_t>(sides + 1);
    for (std::uint32_t i = 0; i + 1 < n; ++i) {
        for (std::uint32_t j = 0; j < static_cast<std::uint32_t>(sides); ++j) {
            const std::uint32_t a = first + i * stride + j;
            const std::uint32_t b = a + stride;
            prim.tri(a, a + 1, b);
            prim.tri(a + 1, b + 1, b);
        }
    }
}

// One cross-section of a strip: the centre, the direction across, the surface normal and the half width.
struct Ring {
    Vec3 c, side, n;
    float hw = 0;
    float v = 0; // texture coordinate along the strip, 1 at the base and 0 at the tip
    float tint[3] = {1, 1, 1};
    float wind[4] = {0, 0, 0, 0};
};

// Adds a strip through the rings: a leaf, a blade of grass, a petal. A ring of zero width becomes one vertex.
inline void addStrip(Primitive &prim, const std::vector<Ring> &rings) {
    std::vector<std::uint32_t> left, right;
    for (const Ring &r : rings) {
        Vertex v;
        v.n = r.n;
        v.v = r.v;
        for (int k = 0; k < 3; ++k)
            v.c[k] = r.tint[k];
        for (int k = 0; k < 4; ++k)
            v.w[k] = r.wind[k];
        if (r.hw < 1e-5f) {
            v.p = r.c;
            v.u = 0.5f;
            left.push_back(prim.add(v));
            right.push_back(left.back());
        } else {
            v.p = r.c - r.side * r.hw;
            v.u = 0;
            left.push_back(prim.add(v));
            v.p = r.c + r.side * r.hw;
            v.u = 1;
            right.push_back(prim.add(v));
        }
    }
    for (std::size_t i = 0; i + 1 < rings.size(); ++i) {
        const std::uint32_t al = left[i], ar = right[i], bl = left[i + 1], br = right[i + 1];
        if (al == ar) {
            if (bl != br)
                prim.tri(al, br, bl);
        } else if (bl == br) {
            prim.tri(al, ar, bl);
        } else {
            prim.tri(al, ar, bl);
            prim.tri(ar, br, bl);
        }
    }
}

enum class Profile { Broad, Lanceolate, Needle, Blade, Triangle };

// Relative width, 0 to 1, at fraction t of the length.
inline float profileWidth(Profile p, float t) {
    switch (p) {
    case Profile::Broad:
        return std::pow(std::fmax(0.0f, std::sin(kPi * std::pow(clampf(t, 0, 1), 0.8f))), 0.7f);
    case Profile::Lanceolate:
        return std::pow(std::fmax(0.0f, std::sin(kPi * std::pow(clampf(t, 0, 1), 0.9f))), 0.9f);
    case Profile::Needle:
        return std::pow(1.0f - clampf(t, 0, 1), 0.5f);
    case Profile::Blade:
        return std::pow(1.0f - clampf(t, 0, 1), 0.7f);
    case Profile::Triangle:
        return std::pow(1.0f - clampf(t, 0, 1), 0.8f) * (0.25f + 0.75f * std::min(1.0f, t * 6.0f));
    }
    return 1;
}

inline Profile profileFor(LeafShape s) {
    switch (s) {
    case LeafShape::Broad:
        return Profile::Broad;
    case LeafShape::Lanceolate:
        return Profile::Lanceolate;
    case LeafShape::Needle:
        return Profile::Needle;
    }
    return Profile::Broad;
}

// Adds a flat leaf that starts at base and extends along axis; normal says which way its face points. Needles are
// three blades around the axis, which makes a tuft that is seen from any side.
inline void addLeaf(Primitive &prim, Vec3 base, Vec3 axis, Vec3 normal, float len, float width, int segs,
                    Profile profile, const float tint[3], const float wind[4], int blades = 1) {
    axis = normalize(axis);
    normal = normalize(normal - axis * dot(normal, axis));
    for (int b = 0; b < blades; ++b) {
        const Vec3 nrm = rotate(normal, axis, kPi * fl(b) / fl(blades));
        const Vec3 side = cross(axis, nrm);
        std::vector<Ring> rings;
        for (int i = 0; i <= segs + 1; ++i) {
            const float t = fl(i) / fl(segs + 1);
            Ring r;
            r.c = base + axis * (len * t);
            r.side = side;
            r.n = nrm;
            r.hw = 0.5f * width * profileWidth(profile, t);
            r.v = 1.0f - t;
            for (int k = 0; k < 3; ++k)
                r.tint[k] = tint[k];
            for (int k = 0; k < 4; ++k)
                r.wind[k] = wind[k];
            rings.push_back(r);
        }
        addStrip(prim, rings);
    }
}

// A surface of revolution around the y axis through the profile of (height, radius) pairs, closed at the ends.
inline void addLathe(Primitive &prim, const std::vector<std::pair<float, float>> &profile, int around,
                     const float tint[3], float windScale, float topHeight) {
    const std::uint32_t first = static_cast<std::uint32_t>(prim.verts.size());
    for (const auto &hr : profile) {
        for (int j = 0; j <= around; ++j) {
            const float a = 2.0f * kPi * fl(j) / fl(around);
            Vertex v;
            v.p = {hr.second * std::cos(a), hr.first, hr.second * std::sin(a)};
            v.n = normalize(Vec3{std::cos(a), 0.3f, std::sin(a)});
            v.u = fl(j) / fl(around);
            v.v = hr.first;
            for (int k = 0; k < 3; ++k)
                v.c[k] = tint[k];
            v.w[0] = std::pow(clampf(hr.first / topHeight, 0, 1), 1.5f);
            v.w[1] = windScale;
            v.w[2] = 0.5f;
            prim.add(v);
        }
    }
    const std::uint32_t stride = static_cast<std::uint32_t>(around + 1);
    for (std::uint32_t i = 0; i + 1 < profile.size(); ++i) {
        for (std::uint32_t j = 0; j < static_cast<std::uint32_t>(around); ++j) {
            const std::uint32_t a = first + i * stride + j;
            const std::uint32_t b = a + stride;
            prim.tri(a, b, a + 1);
            prim.tri(a + 1, b, b + 1);
        }
    }
}

// The sensor properties of a material as the "extras" of a glTF material.
inline std::string sensorExtras(const std::string &sensorClass, const Optics &o) {
    return "{\"nt\":{\"sensor\":{\"class\":" + jsonEscape(sensorClass) +
           ",\"lidar\":{\"reflectance905\":" + jsonNumber(o.lidar905) +
           ",\"reflectance1550\":" + jsonNumber(o.lidar1550) +
           "},\"thermal\":{\"emissivityLwir\":" + jsonNumber(o.emissivity) +
           ",\"solarAbsorptivity\":" + jsonNumber(o.solarAbsorptivity) + "}}}}";
}

// An axis-aligned box with its own vertices for each face.
inline void addBox(Primitive &prim, Vec3 lo, Vec3 hi, const float tint[3], float windScale) {
    const Vec3 corner[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                            {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
    static const int faces[6][4] = {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 4, 7, 3}, {1, 2, 6, 5}, {0, 1, 5, 4}, {3, 7, 6, 2}};
    static const Vec3 normals[6] = {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}};
    for (int f = 0; f < 6; ++f) {
        std::uint32_t idx[4];
        for (int k = 0; k < 4; ++k) {
            Vertex v;
            v.p = corner[faces[f][k]];
            v.n = normals[f];
            v.u = (k == 1 || k == 2) ? 1.0f : 0.0f;
            v.v = (k >= 2) ? 1.0f : 0.0f;
            for (int c = 0; c < 3; ++c)
                v.c[c] = tint[c];
            v.w[0] = clampf(v.p.y / std::max(hi.y, 0.01f), 0, 1);
            v.w[1] = windScale;
            v.w[2] = 0.5f;
            idx[k] = prim.add(v);
        }
        prim.tri(idx[0], idx[1], idx[2]);
        prim.tri(idx[0], idx[2], idx[3]);
    }
}

// Finds textures in a directory as <id>_<slot>.png or .jpg, then <slot>.png or .jpg, and adds each image to the
// document once.
class Textures {
public:
    Textures(Document &doc, std::string dir) : m_doc(doc), m_dir(std::move(dir)) {}

    // Returns the index of the image for the slot, or -1 if there is none.
    int get(const std::string &id, const std::string &slot) {
        if (m_dir.empty())
            return -1;
        for (const std::string &stem : {id + "_" + slot, slot}) {
            for (const char *ext : {".png", ".jpg", ".jpeg"}) {
                const std::string path = m_dir + "/" + stem + ext;
                const auto found = m_cache.find(path);
                if (found != m_cache.end())
                    return found->second;
                std::ifstream in(path, std::ios::binary);
                if (!in)
                    continue;
                Image img;
                img.name = stem;
                img.data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
                static const std::uint8_t png[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
                if (img.data.size() > 8 && std::equal(png, png + 8, img.data.begin()))
                    img.mime = "image/png";
                else if (img.data.size() > 3 && img.data[0] == 0xff && img.data[1] == 0xd8 && img.data[2] == 0xff)
                    img.mime = "image/jpeg";
                else
                    throw std::runtime_error(path + " is neither a PNG nor a JPEG file");
                m_doc.images.push_back(std::move(img));
                const int index = static_cast<int>(m_doc.images.size() - 1);
                m_cache[path] = index;
                return index;
            }
        }
        return -1;
    }

private:
    Document &m_doc;
    std::string m_dir;
    std::map<std::string, int> m_cache;
};

} // namespace gentree
