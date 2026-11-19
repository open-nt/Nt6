// MIT License - see LICENSE
//
// The grass and weed generators of ntgentree. A grass tuft is a clump of blades, each a bent ribbon, with optional
// seed stalks above them; a weed is a rosette, a leafy stem, a frond cluster or a set of arching canes. Each is
// built in three levels of detail by drawing fewer and wider blades or leaves.
#pragma once

#include "geometry.h"
#include "options.h"

#include <algorithm>
#include <cmath>

namespace gentree {

namespace plant_detail {

struct PlantLod {
    float countFraction;
    float widthScale;
    int segs;
    int sides;
};

constexpr PlantLod kLods[3] = {{1.0f, 1.0f, 5, 4}, {0.45f, 1.6f, 3, 3}, {0.15f, 3.0f, 2, 3}};

struct CurvedLeaf {
    Vec3 base;
    float azimuth = 0;
    float elevation = 1.0f; // radians above the ground
    float length = 0.2f;
    float width = 0.02f;
    float petiole = 0; // fraction of the length that is a stalk
    float curl = 0;    // radians the tip bends towards the ground
    float twist = 0;
    int segs = 5;
    Profile profile = Profile::Blade;
    float tintBase[3] = {1, 1, 1};
    float tintTip[3] = {1, 1, 1};
    float phase = 0;
    float flex = 1.0f; // weight of the sway at the tip
};

// A ribbon that leaves the ground or a stem at an angle and bends over.
inline void addCurvedLeaf(Primitive &prim, const CurvedLeaf &c) {
    const Vec3 h = {std::cos(c.azimuth), 0, std::sin(c.azimuth)};
    const Vec3 axis = {-h.z, 0, h.x};
    Vec3 d = h * std::cos(c.elevation) + Vec3{0, 1, 0} * std::sin(c.elevation);
    Vec3 p = c.base;
    const float step = c.length / fl(c.segs);
    std::vector<Ring> rings;
    for (int i = 0; i <= c.segs; ++i) {
        const float t = fl(i) / fl(c.segs);
        if (i > 0) {
            d = rotate(d, axis, -c.curl / fl(c.segs) * (0.3f + 1.4f * t));
            p += d * step;
        }
        Ring r;
        r.c = p;
        r.side = normalize(rotate(axis, d, c.twist * t));
        r.n = cross(r.side, d);
        const float stalk = 0.5f * std::max(0.004f, c.width * 0.12f);
        r.hw = t < c.petiole ? stalk
                             : std::max(stalk * 0.5f,
                                        0.5f * c.width * profileWidth(c.profile, (t - c.petiole) / (1.0f - c.petiole)));
        if (i == c.segs)
            r.hw = c.profile == Profile::Blade || c.profile == Profile::Lanceolate ? 0.0f : r.hw;
        r.v = 1.0f - t;
        for (int k = 0; k < 3; ++k)
            r.tint[k] = lerp(c.tintBase[k], c.tintTip[k], t);
        r.wind[0] = 0;
        r.wind[1] = t * t * c.flex;
        r.wind[2] = 0.3f * t * c.flex;
        r.wind[3] = c.phase;
        rings.push_back(r);
    }
    addStrip(prim, rings);
}

// A small flower: petals around an axis, either flat like a daisy or in all directions like a thistle.
inline void addFlower(Primitive &prim, Rng &rng, Vec3 pos, int petals, float radius, bool ball, float phase) {
    const float tint[3] = {1, 1, 1};
    const float wind[4] = {0.0f, 0.6f, 0.5f, phase};
    for (int i = 0; i < petals; ++i) {
        Vec3 axis;
        if (ball) {
            axis = normalize(rng.unitVector() + Vec3{0, 1.2f, 0});
        } else {
            const float a = 2.0f * kPi * fl(i) / fl(petals);
            axis = normalize(Vec3{std::cos(a), 0.35f, std::sin(a)});
        }
        const Vec3 normal = normalize(cross(axis, rng.unitVector()));
        addLeaf(prim, pos, axis, normal, radius * rng.range(0.85f, 1.1f), radius * (petals > 12 ? 0.18f : 0.6f), 1,
                Profile::Lanceolate, tint, wind);
    }
}

} // namespace plant_detail

inline Result buildGrass(const GrassSpecies &sp, const Options &opt) {
    using namespace plant_detail;
    Result res;
    Document &doc = res.doc;
    doc.generator = "ntgentree";
    const std::string id = sp.id;
    Textures tex(doc, opt.textures);
    const int bladeImg = tex.get(id, "blade");
    const std::string sensor = "vegetation.grass";

    Material blade;
    blade.name = "blade";
    blade.base[0] = bladeImg >= 0 ? 1.0f : sp.blade.r;
    blade.base[1] = bladeImg >= 0 ? 1.0f : sp.blade.g;
    blade.base[2] = bladeImg >= 0 ? 1.0f : sp.blade.b;
    blade.roughness = 0.7f;
    blade.doubleSided = true;
    blade.mask = bladeImg >= 0;
    blade.image = bladeImg;
    blade.clamp = true;
    blade.extras = sensorExtras(sensor, opticsFor(sensor));
    Material seed;
    seed.name = "seed";
    seed.base[0] = sp.seed.r;
    seed.base[1] = sp.seed.g;
    seed.base[2] = sp.seed.b;
    seed.doubleSided = true;
    seed.extras = sensorExtras(sensor, opticsFor(sensor));
    Material stalk;
    stalk.name = "stalk";
    stalk.base[0] = (sp.blade.r + sp.seed.r) * 0.5f;
    stalk.base[1] = (sp.blade.g + sp.seed.g) * 0.5f;
    stalk.base[2] = (sp.blade.b + sp.seed.b) * 0.5f;
    stalk.extras = sensorExtras(sensor, opticsFor(sensor));
    doc.materials = {blade, seed, stalk};

    Rng rng(opt.seed * 0x9e3779b97f4a7c15ULL + 0x7654321ULL);
    const float height = opt.height > 0 ? opt.height : rng.range(sp.heightMin, sp.heightMax);
    const int bladeCount = opt.blades > 0 ? opt.blades : sp.blades;
    const bool textured = bladeImg >= 0;

    std::string lodJson;
    std::size_t counts[3] = {0, 0, 0};
    for (int lod = 0; lod < 3; ++lod) {
        const PlantLod &pl = kLods[lod];
        Mesh mesh;
        mesh.name = "LOD" + std::to_string(lod);
        Primitive &bp = mesh.primitive(0);
        const int n = std::max(8, static_cast<int>(fl(bladeCount) * pl.countFraction));
        for (int j = 0; j < n; ++j) {
            // The same blade index gives the same blade at every level of detail, so the tuft keeps its shape.
            const int i = static_cast<int>(static_cast<long long>(j) * bladeCount / n);
            Rng br(opt.seed * std::uint64_t{1000003} + static_cast<std::uint64_t>(i) * std::uint64_t{7919} + 17);
            const float az = br.range(0.0f, 2.0f * kPi);
            const float rr = sp.hummock ? sp.radius * 0.3f * br.uniform() : sp.radius * std::sqrt(br.uniform());
            CurvedLeaf c;
            c.base = {rr * std::cos(az), 0, rr * std::sin(az)};
            c.azimuth = az + br.sym(0.5f);
            const float lean = clampf(sp.lean + br.normal() * sp.leanVar, 0.02f, 1.45f);
            c.elevation = kPi * 0.5f - lean;
            c.length = height * (sp.hummock ? br.range(0.8f, 1.0f) : br.range(0.55f, 1.1f));
            c.width = sp.bladeWidth * pl.widthScale * br.range(0.8f, 1.2f);
            c.curl = sp.curl * br.range(0.6f, 1.2f);
            c.twist = br.sym(0.8f);
            c.segs = pl.segs;
            c.profile = Profile::Blade;
            const float dark = textured ? 0.8f : 0.0f;
            for (int k = 0; k < 3; ++k) {
                const float baseTint =
                    textured ? dark : (k == 0 ? sp.baseTint.r : (k == 1 ? sp.baseTint.g : sp.baseTint.b));
                c.tintBase[k] = baseTint * br.range(0.9f, 1.05f);
                c.tintTip[k] = br.range(0.92f, 1.0f);
            }
            c.phase = br.uniform();
            addCurvedLeaf(bp, c);
        }
        counts[lod] = static_cast<std::size_t>(n);
        if (sp.seedHeads) {
            const int heads = std::max(1, static_cast<int>(fl(sp.seedHeadCount) * pl.countFraction + 0.5f));
            for (int j = 0; j < heads; ++j) {
                Rng sr(opt.seed * std::uint64_t{2000003} + static_cast<std::uint64_t>(j) * std::uint64_t{104729} + 5);
                const float az = sr.range(0.0f, 2.0f * kPi);
                const float rr = sp.radius * 0.6f * std::sqrt(sr.uniform());
                const float top = height * sp.seedStalkRatio * sr.range(0.9f, 1.1f);
                const float leanAz = sr.range(0.0f, 2.0f * kPi);
                const float lean = sr.range(0.05f, 0.3f);
                std::vector<TubePoint> pts;
                Vec3 p = {rr * std::cos(az), 0, rr * std::sin(az)};
                Vec3 d = normalize(
                    Vec3{std::sin(lean) * std::cos(leanAz), std::cos(lean), std::sin(lean) * std::sin(leanAz)});
                const int segs = pl.segs;
                std::vector<Vec3> path;
                for (int i = 0; i <= segs; ++i) {
                    pts.push_back({p, 0.0012f * (1.0f - 0.4f * fl(i) / fl(segs)), 0, fl(i) / fl(segs)});
                    path.push_back(p);
                    p += d * (top / fl(segs));
                    d = normalize(d + Vec3{std::cos(leanAz), -0.2f, std::sin(leanAz)} * (0.12f * sr.uniform()));
                }
                const float tint[3] = {1, 1, 1};
                addTube(mesh.primitive(2), pts, std::max(3, pl.sides - 1), 1.0f, 0.1f, tint, sr.uniform());
                const int spikelets = lod == 0 ? 10 : (lod == 1 ? 5 : 3);
                const float sw[4] = {0.0f, 1.0f, 0.4f, sr.uniform()};
                for (int s = 0; s < spikelets; ++s) {
                    const float t = 0.72f + 0.28f * fl(s) / fl(spikelets);
                    const float f = t * fl(segs);
                    std::size_t k = std::min(static_cast<std::size_t>(f), path.size() - 2);
                    const Vec3 at = lerp(path[k], path[k + 1], f - fl(k));
                    const Vec3 axis = normalize(Vec3{0, 1, 0} + sr.unitVector() * 0.5f);
                    const float lodScale = fl(lod + 1);
                    addLeaf(mesh.primitive(1), at, axis, sr.unitVector(), 0.025f * lodScale, 0.007f * lodScale, 1,
                            Profile::Lanceolate, tint, sw);
                }
            }
        }
        doc.meshes.push_back(mesh);
    }
    const float distances[3] = {height * 40.0f, height * 120.0f, 0.0f};
    doc.nodes.push_back(Node());
    doc.nodes[0].name = id;
    for (int lod = 0; lod < 3; ++lod) {
        const Mesh &m = doc.meshes[static_cast<std::size_t>(lod)];
        res.lods.push_back({m.name, m.triangles(), distances[lod]});
        Node n;
        n.name = m.name;
        n.mesh = lod;
        n.extras = "{\"nt\":{\"lod\":" + std::to_string(lod) + ",\"maxDistance\":" + jsonNumber(distances[lod]) + "}}";
        doc.nodes.push_back(n);
        doc.nodes[0].children.push_back(static_cast<int>(doc.nodes.size() - 1));
        lodJson += (lod ? "," : "") + std::string("{\"node\":") + jsonEscape(m.name) +
                   ",\"triangles\":" + std::to_string(m.triangles()) + ",\"blades\":" + std::to_string(counts[lod]) +
                   ",\"maxDistance\":" + jsonNumber(distances[lod]) + "}";
    }
    doc.sceneExtras =
        "{\"nt\":{\"kind\":\"grass\",\"species\":" + jsonEscape(id) + ",\"common\":" + jsonEscape(sp.common) +
        ",\"latin\":" + jsonEscape(sp.latin) + ",\"region\":" + jsonEscape(sp.region) +
        ",\"seed\":" + std::to_string(opt.seed) + ",\"height\":" + jsonNumber(height) +
        ",\"radius\":" + jsonNumber(sp.radius) + ",\"sensorMaterial\":" + jsonEscape(sensor) +
        ",\"textures\":{\"blade\":" + (bladeImg >= 0 ? "true" : "false") + "},\"lods\":[" + lodJson + "]}}";
    res.kind = "grass";
    res.id = id;
    res.sensor = sensor;
    res.height = height;
    return res;
}

namespace plant_detail {

// A cane or stem that curves; the points are returned for placing leaves along it.
inline std::vector<Vec3> addStem(Primitive &prim, Rng &rng, Vec3 base, Vec3 dir, float length, float r0, float r1,
                                 float arch, int segs, int sides, const float tint[3], float phase) {
    std::vector<TubePoint> pts;
    std::vector<Vec3> path;
    Vec3 d = normalize(dir);
    Vec3 p = base;
    const Vec3 h = normalize(Vec3{d.x, 0, d.z});
    const Vec3 axis = {-h.z, 0, h.x};
    for (int i = 0; i <= segs; ++i) {
        const float t = fl(i) / fl(segs);
        pts.push_back({p, lerp(r0, r1, t), 0.0f, t});
        path.push_back(p);
        d = rotate(d, axis, -arch / fl(segs));
        d = normalize(d + rng.unitVector() * 0.05f);
        p += d * (length / fl(segs));
    }
    addTube(prim, pts, sides, 1.0f, 0.2f, tint, phase);
    return path;
}

} // namespace plant_detail

inline Result buildWeed(const WeedSpecies &sp, const Options &opt) {
    using namespace plant_detail;
    Result res;
    Document &doc = res.doc;
    doc.generator = "ntgentree";
    const std::string id = sp.id;
    Textures tex(doc, opt.textures);
    const int leafImg = tex.get(id, "leaf");
    const std::string sensor = "vegetation.forb";

    Material leaf;
    leaf.name = "leaf";
    leaf.base[0] = leafImg >= 0 ? 1.0f : sp.leaf.r;
    leaf.base[1] = leafImg >= 0 ? 1.0f : sp.leaf.g;
    leaf.base[2] = leafImg >= 0 ? 1.0f : sp.leaf.b;
    leaf.roughness = 0.7f;
    leaf.doubleSided = true;
    leaf.mask = leafImg >= 0;
    leaf.image = leafImg;
    leaf.clamp = true;
    leaf.extras = sensorExtras(sensor, opticsFor(sensor));
    Material stem;
    stem.name = "stem";
    stem.base[0] = sp.stem.r;
    stem.base[1] = sp.stem.g;
    stem.base[2] = sp.stem.b;
    stem.extras = sensorExtras(sensor, opticsFor(sensor));
    Material flower;
    flower.name = "flower";
    flower.base[0] = sp.flower.r;
    flower.base[1] = sp.flower.g;
    flower.base[2] = sp.flower.b;
    flower.doubleSided = true;
    flower.extras = sensorExtras(sensor, opticsFor(sensor));
    doc.materials = {leaf, stem, flower};

    Rng rng(opt.seed * 0x9e3779b97f4a7c15ULL + 0x2468ACEULL);
    const float sizeScale = opt.height > 0 && sp.stemHeight > 0 ? opt.height / sp.stemHeight : 1.0f;
    const bool textured = leafImg >= 0;
    float height = sp.stemHeight > 0 ? sp.stemHeight * sizeScale : sp.leafLength * sizeScale;

    std::string lodJson;
    for (int lod = 0; lod < 3; ++lod) {
        const PlantLod &pl = kLods[lod];
        Mesh mesh;
        mesh.name = "LOD" + std::to_string(lod);
        Primitive &lp = mesh.primitive(0);
        Primitive &sp1 = mesh.primitive(1);
        Primitive &fp = mesh.primitive(2);
        // The same sequence at every level of detail: the rng restarts, and the finer levels draw more.
        Rng wr(opt.seed * 3000017ULL + 11ULL);
        const float stemTint[3] = {1, 1, 1};
        auto leafTint = [&](CurvedLeaf &c, float shade) {
            for (int k = 0; k < 3; ++k) {
                c.tintBase[k] = textured ? 0.8f * shade : 0.7f * shade;
                c.tintTip[k] = shade;
            }
        };
        const int leaves = std::max(3, static_cast<int>(fl(sp.leaves) * pl.countFraction + 0.5f));
        switch (sp.form) {
        case WeedForm::Rosette:
        case WeedForm::Frond: {
            const bool frond = sp.form == WeedForm::Frond;
            for (int i = 0; i < leaves; ++i) {
                const float f = fl(i) / fl(leaves);
                CurvedLeaf c;
                const float spread = frond ? 0.2f * sp.leafLength * sizeScale * wr.uniform() : 0.0f;
                const float a = fl(i) * kGoldenAngle + wr.sym(0.2f);
                c.base = {spread * std::cos(a), 0, spread * std::sin(a)};
                c.azimuth = a;
                c.elevation = clampf(sp.elevation * (0.6f + 0.8f * f) + wr.sym(0.1f), 0.1f, 1.5f);
                c.length = sp.leafLength * sizeScale * (1.0f - 0.45f * f) * wr.range(0.85f, 1.1f);
                c.width = c.length * sp.leafAspect * pl.widthScale;
                c.petiole = sp.petiole;
                c.curl = sp.curl * wr.range(0.7f, 1.2f);
                c.twist = wr.sym(0.5f);
                c.segs = frond ? pl.segs + 2 : pl.segs;
                c.profile = frond ? Profile::Triangle : (sp.leafAspect < 0.3f ? Profile::Lanceolate : Profile::Broad);
                leafTint(c, wr.range(0.9f, 1.0f));
                c.phase = wr.uniform();
                addCurvedLeaf(lp, c);
            }
            for (int s = 0; s < sp.stems && sp.stemHeight > 0 && sp.flowerRadius > 0; ++s) {
                const float a = wr.range(0.0f, 2.0f * kPi);
                const float len = sp.stemHeight * sizeScale * wr.range(0.8f, 1.1f);
                const Vec3 dir = normalize(Vec3{0.12f * std::cos(a), 1.0f, 0.12f * std::sin(a)});
                const Vec3 start = {0.02f * std::cos(a), 0, 0.02f * std::sin(a)};
                const std::vector<Vec3> path = addStem(sp1, wr, start, dir, len, 0.0035f, 0.0022f, wr.range(0.1f, 0.5f),
                                                       pl.segs, pl.sides, stemTint, wr.uniform());
                const float phase = wr.uniform();
                // Leaves clasp the stem and shrink towards the top.
                const int stemLeaves =
                    sp.stemLeaves > 0 ? std::max(2, static_cast<int>(fl(sp.stemLeaves) * pl.countFraction + 0.5f)) : 0;
                for (int k = 0; k < stemLeaves; ++k) {
                    const float t = 0.12f + 0.7f * fl(k) / fl(stemLeaves);
                    const std::size_t idx =
                        std::min(static_cast<std::size_t>(t * fl(path.size() - 1)), path.size() - 2);
                    CurvedLeaf c;
                    c.base = lerp(path[idx], path[idx + 1], t * fl(path.size() - 1) - fl(idx));
                    c.azimuth = fl(k) * kGoldenAngle + a;
                    c.elevation = 0.7f;
                    c.length = sp.leafLength * sizeScale * (0.55f - 0.35f * t) * wr.range(0.85f, 1.1f);
                    c.width = c.length * sp.leafAspect * pl.widthScale;
                    c.curl = 0.5f;
                    c.segs = pl.segs;
                    c.profile = sp.leafAspect < 0.3f ? Profile::Lanceolate : Profile::Broad;
                    leafTint(c, wr.range(0.9f, 1.0f));
                    c.phase = wr.uniform();
                    addCurvedLeaf(lp, c);
                }
                if (sp.flowerSpike) {
                    const int flowers = std::max(3, static_cast<int>(18 * pl.countFraction));
                    for (int k = 0; k < flowers; ++k) {
                        const float t = 0.55f + 0.45f * fl(k) / fl(flowers);
                        const std::size_t idx =
                            std::min(static_cast<std::size_t>(t * fl(path.size() - 1)), path.size() - 2);
                        const Vec3 at = lerp(path[idx], path[idx + 1], t * fl(path.size() - 1) - fl(idx));
                        const Vec3 out =
                            normalize(Vec3{std::cos(fl(k) * kGoldenAngle), 0.4f, std::sin(fl(k) * kGoldenAngle)});
                        addFlower(fp, wr, at + out * 0.01f, sp.petals, sp.flowerRadius * sizeScale * 1.3f, false,
                                  phase);
                    }
                } else {
                    const int petals = std::max(8, static_cast<int>(fl(sp.petals) * std::max(pl.countFraction, 0.4f)));
                    addFlower(fp, wr, path.back(), petals, sp.flowerRadius * sizeScale, sp.flowerBall, phase);
                }
            }
            break;
        }
        case WeedForm::Stemmed: {
            const int pairs = std::max(2, leaves / 2);
            for (int s = 0; s < sp.stems; ++s) {
                const float a = wr.range(0.0f, 2.0f * kPi);
                const float sr = 0.06f * std::sqrt(wr.uniform());
                const Vec3 start = {sr * std::cos(a), 0, sr * std::sin(a)};
                const Vec3 dir = normalize(Vec3{wr.sym(0.12f), 1.0f, wr.sym(0.12f)});
                const float len = sp.stemHeight * sizeScale * wr.range(0.8f, 1.1f);
                const std::vector<Vec3> path = addStem(sp1, wr, start, dir, len, 0.004f, 0.0025f, wr.range(0.0f, 0.2f),
                                                       pl.segs + 2, pl.sides, stemTint, wr.uniform());
                for (int k = 0; k < pairs; ++k) {
                    const float t = 0.15f + 0.8f * fl(k) / fl(pairs);
                    const std::size_t idx =
                        std::min(static_cast<std::size_t>(t * fl(path.size() - 1)), path.size() - 2);
                    const Vec3 at = lerp(path[idx], path[idx + 1], t * fl(path.size() - 1) - fl(idx));
                    const float az = (k % 2 ? 0.5f * kPi : 0.0f) + a + wr.sym(0.2f);
                    for (int side = 0; side < 2; ++side) {
                        CurvedLeaf c;
                        c.base = at;
                        c.azimuth = az + kPi * fl(side);
                        c.elevation = 0.5f;
                        c.length = sp.leafLength * sizeScale * (1.2f - 0.6f * t) * wr.range(0.85f, 1.1f);
                        c.width = c.length * sp.leafAspect * pl.widthScale;
                        c.petiole = sp.petiole;
                        c.curl = 0.5f;
                        c.segs = pl.segs;
                        c.profile = Profile::Broad;
                        leafTint(c, wr.range(0.9f, 1.0f));
                        c.phase = wr.uniform();
                        addCurvedLeaf(lp, c);
                    }
                }
            }
            break;
        }
        case WeedForm::Cane: {
            for (int s = 0; s < sp.stems; ++s) {
                const float a = wr.range(0.0f, 2.0f * kPi);
                const Vec3 dir = normalize(Vec3{0.55f * std::cos(a), 1.0f, 0.55f * std::sin(a)});
                const float len = sp.stemHeight * sizeScale * 2.0f * wr.range(0.8f, 1.1f);
                const std::vector<Vec3> path =
                    addStem(sp1, wr, {0, 0, 0}, dir, len, 0.006f, 0.0025f, 2.0f, 12, pl.sides, stemTint, wr.uniform());
                const int nodes = std::max(3, static_cast<int>(fl(sp.leaves) * 1.5f * pl.countFraction));
                for (int k = 0; k < nodes; ++k) {
                    const float t = 0.12f + 0.86f * fl(k) / fl(nodes);
                    const std::size_t idx =
                        std::min(static_cast<std::size_t>(t * fl(path.size() - 1)), path.size() - 2);
                    const Vec3 at = lerp(path[idx], path[idx + 1], t * fl(path.size() - 1) - fl(idx));
                    const Vec3 tangent = normalize(path[idx + 1] - path[idx]);
                    const float az = fl(k) * kGoldenAngle;
                    for (int leaflet = 0; leaflet < 3; ++leaflet) {
                        CurvedLeaf c;
                        c.base = at;
                        c.azimuth = az + 0.5f * fl(leaflet - 1);
                        c.elevation = clampf(0.25f + 0.3f * tangent.y, 0.1f, 0.9f);
                        c.length = sp.leafLength * sizeScale * (leaflet == 1 ? 1.3f : 1.0f) * wr.range(0.85f, 1.15f);
                        c.width = c.length * sp.leafAspect * pl.widthScale;
                        c.petiole = 0.15f;
                        c.curl = 0.4f;
                        c.segs = pl.segs;
                        c.profile = Profile::Broad;
                        leafTint(c, wr.range(0.9f, 1.0f));
                        c.phase = wr.uniform();
                        addCurvedLeaf(lp, c);
                    }
                    if (sp.flowerRadius > 0 && wr.uniform() < 0.25f)
                        addFlower(fp, wr, at + Vec3{0, 0.02f, 0}, sp.petals, sp.flowerRadius, false, wr.uniform());
                }
            }
            break;
        }
        }
        doc.meshes.push_back(mesh);
    }
    if (sp.form == WeedForm::Rosette && sp.stemHeight <= 0)
        height = sp.leafLength * sizeScale;
    if (sp.form == WeedForm::Frond)
        height = sp.leafLength * sizeScale * 0.9f;
    if (sp.form == WeedForm::Cane)
        height = sp.stemHeight * sizeScale;

    const float distances[3] = {height * 40.0f, height * 120.0f, 0.0f};
    doc.nodes.push_back(Node());
    doc.nodes[0].name = id;
    for (int lod = 0; lod < 3; ++lod) {
        const Mesh &m = doc.meshes[static_cast<std::size_t>(lod)];
        res.lods.push_back({m.name, m.triangles(), distances[lod]});
        Node n;
        n.name = m.name;
        n.mesh = lod;
        n.extras = "{\"nt\":{\"lod\":" + std::to_string(lod) + ",\"maxDistance\":" + jsonNumber(distances[lod]) + "}}";
        doc.nodes.push_back(n);
        doc.nodes[0].children.push_back(static_cast<int>(doc.nodes.size() - 1));
        lodJson += (lod ? "," : "") + std::string("{\"node\":") + jsonEscape(m.name) +
                   ",\"triangles\":" + std::to_string(m.triangles()) +
                   ",\"maxDistance\":" + jsonNumber(distances[lod]) + "}";
    }
    doc.sceneExtras = "{\"nt\":{\"kind\":\"weed\",\"species\":" + jsonEscape(id) +
                      ",\"common\":" + jsonEscape(sp.common) + ",\"latin\":" + jsonEscape(sp.latin) +
                      ",\"region\":" + jsonEscape(sp.region) + ",\"seed\":" + std::to_string(opt.seed) +
                      ",\"height\":" + jsonNumber(height) + ",\"sensorMaterial\":" + jsonEscape(sensor) +
                      ",\"textures\":{\"leaf\":" + (leafImg >= 0 ? "true" : "false") + "},\"lods\":[" + lodJson + "]}}";
    res.kind = "weed";
    res.id = id;
    res.sensor = sensor;
    res.height = height;
    return res;
}

} // namespace gentree
