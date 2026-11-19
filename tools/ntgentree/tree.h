// MIT License - see LICENSE
//
// The tree generator of ntgentree. A tree is grown as a skeleton: a trunk, then branches of up to three orders
// that leave their parent at an angle along a golden-angle spiral, bend, and are held inside a crown envelope that
// gives each species its shape. The skeleton is then meshed once for each level of detail. Leaves are placed on the
// last order of branches until their total area matches the leaf area index; when the leaf budget of a level of
// detail is smaller, each leaf grows so the area stays the same.
#pragma once

#include "geometry.h"
#include "options.h"

#include <algorithm>
#include <cmath>

namespace gentree {

namespace tree_detail {

struct Node3 {
    Vec3 p;
    float r = 0;
};

struct Branch {
    int level = 0;
    int parent = -1;
    int parentNode = 0; // the segment of the parent that this branch leaves
    float length = 0;
    float phase = 0;
    float tint[3] = {1, 1, 1};
    std::vector<Node3> nodes;
};

struct LodSpec {
    float minRadius;
    int maxLevel;
    float leafFraction;
    int leafSegs;
    int sides[4]; // by branch radius: over 15 cm, over 5 cm, over 1.2 cm, thinner
};

constexpr LodSpec kLods[3] = {
    {0.0015f, 3, 1.0f, 2, {12, 8, 5, 3}},
    {0.006f, 3, 1.0f / 6.0f, 2, {8, 6, 4, 3}},
    {0.02f, 1, 1.0f / 40.0f, 1, {6, 4, 3, 3}},
};

inline float rad(float deg) { return deg * kPi / 180.0f; }

} // namespace tree_detail

class TreeBuilder {
public:
    TreeBuilder(const TreeSpecies &species, const Options &options)
        : m_sp(species), m_opt(options), m_rng(options.seed * 0x9e3779b97f4a7c15ULL + 0x1234567ULL) {
        m_height = options.height > 0 ? options.height : m_rng.range(m_sp.heightMin, m_sp.heightMax);
        m_hedge = m_sp.hedgeLength > 0;
        m_spawnLevels = m_sp.fill > 0 ? std::min(m_sp.levels, 2) : m_sp.levels;
        m_terminal = m_sp.fill > 0 || m_sp.competition ? 3 : m_sp.levels;
        m_lobe[0] = m_rng.range(0.0f, 2.0f * kPi);
        m_lobe[1] = m_rng.range(0.0f, 2.0f * kPi);
        m_crownR = m_hedge ? m_sp.hedgeWidth * 0.5f : m_sp.crownRadius * m_height;
    }

    Result build();

private:
    using Node3 = tree_detail::Node3;
    using Branch = tree_detail::Branch;

    float envelope(float y) const;
    bool outside(Vec3 p, float slack = 1.3f, float margin = 0.4f) const;
    float limbLength(float y0, float theta) const;
    void growTrunk();
    void growStem(Vec3 base, float leanAzimuth, float leanDegrees, float topScale);
    int growBranch(int level, Vec3 start, Vec3 dir, float len, float r0, int parent, int parentNode);
    float lobe(Vec3 p) const;
    void fillCrown();
    std::vector<Vec3> sampleMarkers(int count);
    int markerCount() const;
    void growCompetition();
    void pipeRadii();
    void spawn(int parentIndex, int level);
    void sample(const Branch &b, float t, Vec3 &pos, Vec3 &dir, float &radius) const;
    Mesh meshLod(int lod, std::size_t &leaves);
    void placeLeaves(Primitive &prim, int leafLevel, std::size_t budget, std::size_t &placed);
    void addBranchTube(Primitive &prim, const Branch &b, const tree_detail::LodSpec &lod, int sidesOverride);
    float flexWeight(int level) const {
        static const float w[4] = {0.0f, 0.25f, 0.5f, 0.8f};
        return w[level];
    }
    int sidesFor(const tree_detail::LodSpec &lod, float r) const {
        return lod.sides[r > 0.15f ? 0 : (r > 0.05f ? 1 : (r > 0.012f ? 2 : 3))];
    }

    TreeSpecies m_sp;
    Options m_opt;
    Rng m_rng;
    float m_height = 10;
    float m_crownR = 3;
    bool m_hedge = false;
    int m_spawnLevels = 3; // orders grown at random
    int m_terminal = 3;    // the order that carries the leaves
    float m_lobe[2] = {0, 0};
    float m_trunkTop = 6;
    float m_trunkR = 0.3f;
    std::vector<Branch> m_branches;
    int m_barkMat = 0, m_leafMat = 1, m_proxyMat = 2, m_canopyMat = 3;
    int m_leafSegs = 3;
    bool m_leafTextured = false;
    float m_leafTint[3] = {1, 1, 1};
};

inline float TreeBuilder::envelope(float y) const {
    if (m_hedge)
        return y > 0.0f && y <= m_height ? m_crownR : 0.0f;
    const float cb = m_sp.crownBase;
    const float c = (y / m_height - cb) / (1.0f - cb);
    if (c <= 0.0f || c >= 1.0f)
        return 0.0f;
    if (m_sp.conic)
        return m_crownR * std::pow(1.0f - c, m_sp.profileA);
    return m_crownR * std::pow(std::fmax(0.0f, std::sin(kPi * std::pow(c, m_sp.profileA))), 0.8f);
}

// Whether a point of a growing branch is beyond the crown: a clipped box for a hedge, otherwise a bit outside the
// envelope, which lets branches reach and bend a little past it.
inline bool TreeBuilder::outside(Vec3 p, float slack, float margin) const {
    if (m_hedge)
        return p.y > m_height || p.y < 0.05f || std::fabs(p.x) > m_sp.hedgeLength * 0.5f || std::fabs(p.z) > m_crownR;
    const float horizontal = std::sqrt(p.x * p.x + p.z * p.z);
    return p.y > m_height * 1.03f || p.y < 0.1f || horizontal > envelope(p.y) * lobe(p) * slack + margin;
}

// How far the crown reaches in the direction of p, relative to the envelope. Real crowns are lopsided, because
// neighbours, wind and old damage shape them.
inline float TreeBuilder::lobe(Vec3 p) const {
    if (m_hedge || m_sp.asym <= 0)
        return 1.0f;
    const float a = std::atan2(p.z, p.x);
    return 1.0f + m_sp.asym * (0.6f * std::sin(2.0f * a + m_lobe[0]) +
                               0.4f * std::sin(3.0f * a + m_lobe[1] + 2.0f * p.y / m_height));
}

// The length of a limb that leaves the trunk at height y0 at angle theta from it, so that its tip reaches the
// envelope. The envelope depends on the height of the tip, which depends on the length, hence the iteration.
inline float TreeBuilder::limbLength(float y0, float theta) const {
    if (m_hedge)
        return m_crownR * 1.3f;
    float len = m_crownR;
    const float st = std::max(0.25f, std::sin(theta));
    for (int i = 0; i < 5; ++i) {
        const float tipY = std::min(y0 + len * std::cos(theta) * 0.85f, m_height * 0.97f);
        const float reach = std::max(envelope(tipY), 0.25f * envelope(y0) + 0.05f * m_crownR);
        len = reach / (st * 0.85f);
    }
    return clampf(len, 0.3f, 2.0f * m_crownR + 1.0f);
}

inline void TreeBuilder::sample(const Branch &b, float t, Vec3 &pos, Vec3 &dir, float &radius) const {
    const std::size_t n = b.nodes.size();
    const float f = clampf(t, 0.0f, 1.0f) * fl(n - 1);
    std::size_t i = static_cast<std::size_t>(f);
    if (i >= n - 1)
        i = n - 2;
    const float u = f - fl(i);
    pos = lerp(b.nodes[i].p, b.nodes[i + 1].p, u);
    dir = normalize(b.nodes[i + 1].p - b.nodes[i].p);
    radius = lerp(b.nodes[i].r, b.nodes[i + 1].r, u);
}

inline int TreeBuilder::growBranch(int level, Vec3 start, Vec3 dir, float len, float r0, int parent, int parentNode) {
    using tree_detail::rad;
    static const float segLen[4] = {0.8f, 0.6f, 0.35f, 0.22f};
    static const float noise[4] = {0.0f, 0.1f, 0.18f, 0.28f};
    if (m_branches.size() > 80000 || len < 0.08f)
        return -1;
    Branch b;
    b.level = level;
    b.parent = parent;
    b.parentNode = parentNode;
    b.phase = m_rng.uniform();
    const float shade = m_rng.range(0.82f, 1.0f);
    b.tint[0] = b.tint[1] = b.tint[2] = shade;
    const int nseg = std::clamp(static_cast<int>(len / segLen[level] + 0.5f), 3, 16);
    const float tipR = std::max(0.0012f, r0 * 0.18f);
    const float bend = rad(m_sp.curve[level - 1]) * m_rng.sym(1.0f);
    Vec3 axis = m_rng.unitVector();
    Vec3 d = dir;
    Vec3 p = start;
    b.nodes.push_back({start, r0});
    const float seg = len / fl(nseg);
    for (int i = 1; i <= nseg; ++i) {
        const Vec3 ax = normalize(axis - d * dot(axis, d));
        d = rotate(d, ax, bend / fl(nseg) * 2.0f);
        d = normalize(d + m_rng.unitVector() * noise[level]);
        d.y += m_sp.tropism[level - 1] * 0.2f;
        d = normalize(d);
        p += d * seg;
        if (outside(p))
            break;
        b.nodes.push_back({p, lerp(r0, tipR, fl(i) / fl(nseg))});
        b.length += seg;
    }
    if (b.nodes.size() < 2)
        return -1;
    // Twigs are green until they are a few millimetres thick.
    if (level == m_terminal) {
        b.tint[0] *= 0.9f;
        b.tint[1] *= 1.0f;
        b.tint[2] *= 0.75f;
    }
    m_branches.push_back(b);
    const int index = static_cast<int>(m_branches.size() - 1);
    if (level < m_spawnLevels)
        spawn(index, level + 1);
    return index;
}

// Grows the branches of the given order on the branch at parentIndex.
inline void TreeBuilder::spawn(int parentIndex, int level) {
    using tree_detail::rad;
    const Branch parent = m_branches[static_cast<std::size_t>(parentIndex)];
    const int l = level - 1;
    const float refLen = m_crownR * 1.2f;
    float scale = 1.0f;
    if (level == 2)
        scale = clampf(parent.length / refLen, 0.35f, 1.2f);
    else if (level == 3)
        scale = clampf(parent.length / (refLen * m_sp.lengthRatio[1]), 0.35f, 1.2f);
    int n = static_cast<int>(m_sp.count[l] * scale * m_rng.range(0.85f, 1.15f) + 0.5f);
    n = std::max(n, level == 1 ? 3 : 1);
    const float tStart = level == 1 ? m_sp.crownBase * m_height / std::max(parent.length, 1.0f) : 0.2f;
    float azimuth = m_rng.range(0.0f, 2.0f * kPi);
    // A tree whose trunk ends inside the crown also forks at the top of the trunk.
    const int extra = (level == 1 && m_sp.leaderEnd < 0.95f) ? 2 : 0;
    for (int k = 0; k < n + extra; ++k) {
        const bool leader = k >= n;
        const float tt = leader ? 1.0f : (fl(k) + 0.5f + m_rng.sym(0.4f)) / fl(n);
        const float t = leader ? 1.0f : clampf(tStart + (1.0f - tStart) * tt, 0.0f, 1.0f);
        Vec3 pos, pdir;
        float pr = 0;
        sample(parent, t, pos, pdir, pr);
        azimuth += kGoldenAngle + m_rng.sym(0.35f);
        float theta = rad(m_sp.downAngle[l] + m_rng.sym(10.0f));
        if (level == 1)
            theta *= leader ? 0.4f : (0.5f + 0.5f * (1.0f - tt));
        const Vec3 u = perpendicular(pdir);
        const Vec3 v = cross(pdir, u);
        const Vec3 dir =
            normalize(pdir * std::cos(theta) + (u * std::cos(azimuth) + v * std::sin(azimuth)) * std::sin(theta));
        float len;
        if (level == 1)
            len = limbLength(pos.y, theta) * m_rng.range(0.75f, 1.15f);
        else
            len = parent.length * m_sp.lengthRatio[l] * (1.0f - 0.4f * t) * m_rng.range(0.8f, 1.2f);
        const float radius = std::max(0.0015f, pr * m_sp.radiusRatio[l]);
        const std::size_t segs = parent.nodes.size() - 1;
        const int node = static_cast<int>(std::min(static_cast<std::size_t>(clampf(t, 0, 1) * fl(segs)), segs - 1));
        growBranch(level, pos, dir, len, radius, parentIndex, node);
    }
}

// How many attraction points a crown gets.
inline int TreeBuilder::markerCount() const {
    const float y0 = m_height * m_sp.crownBase;
    const float volume = m_hedge ? m_sp.hedgeLength * m_sp.hedgeWidth * m_height
                                 : (4.0f / 3.0f) * kPi * m_crownR * m_crownR * (m_height - y0) * 0.5f;
    return std::clamp(static_cast<int>(m_sp.fill * volume), 1500, 9000);
}

// Scatters attraction points in the crown, more of them near its surface.
inline std::vector<Vec3> TreeBuilder::sampleMarkers(int count) {
    const float y0 = m_height * m_sp.crownBase;
    std::vector<Vec3> points;
    for (int tries = 0; static_cast<int>(points.size()) < count && tries < count * 40; ++tries) {
        if (m_hedge) {
            points.push_back({m_rng.range(-0.5f * m_sp.hedgeLength, 0.5f * m_sp.hedgeLength),
                              m_rng.range(0.1f, m_height), m_rng.range(-m_crownR, m_crownR)});
            continue;
        }
        const float y = m_rng.range(y0, m_height);
        const float e = envelope(y);
        if (e <= 0 || m_rng.uniform() > (e * e) / (m_crownR * m_crownR))
            continue;
        const float a = m_rng.range(0.0f, 2.0f * kPi);
        const Vec3 dirn = {std::cos(a), 0, std::sin(a)};
        const float reach = e * lobe({dirn.x, y, dirn.z});
        // Denser towards the surface: the exponent is below the 0.5 that spreads points evenly over a disc.
        const float r = reach * std::pow(m_rng.uniform(), 0.38f);
        points.push_back({dirn.x * r, y, dirn.z * r});
    }
    return points;
}

// Grows the whole tree as a competition for space and light, after Palubicki et al. (2009), "Self-organizing tree
// models for image synthesis". Attraction points fill the crown. Every metamer (a segment of a shoot) has a terminal
// bud at the tip of a shoot and an axillary bud, which may grow a side shoot. A bud perceives the points in a cone
// in front of it, within a radius, and each point is seen by its nearest bud only. The number of points that a
// branch's buds see is its supply. It is passed down from the base by the Borchert-Honda model: at each fork the
// vigor goes to the main axis and the side branch in proportion to their supply, weighted by apical for the axis. A
// bud with a vigor of at least 1 grows that many metamers, towards the points it sees. Points are removed when a
// metamer comes near, so branches that are shaded by others find none and stop; side branches that have found no
// space for shed iterations die, which makes tall bare boles and hollow crowns.
inline void TreeBuilder::growCompetition() {
    using tree_detail::rad;
    struct Metamer {
        Vec3 p, dir, axDir;
        int parent = -1, main = -1, lateral = -1;
        int order = 0;
        int lastGain = 0;
        float azimuth = 0;
        bool axil = true;
        float qT = 0, qA = 0, qSub = 0;
        Vec3 vT, vA;
        float vig = 0, vigT = 0, vigA = 0;
        int gainSub = 0;
    };
    std::vector<Metamer> met;
    std::vector<int> roots;

    const float step = clampf(std::max(m_sp.leafLength * 2.5f, m_crownR / 28.0f), 0.03f, 0.6f);
    const float perception = std::max(6.0f * step, 0.7f * m_crownR);
    const float vigorScale = 2.5f; // metamers of growth for each marker that a tree sees
    const float cosCone = std::cos(rad(50.0f));
    const float occupancy = 1.6f * step;
    const float branchAngle = rad(m_sp.downAngle[0]);
    const float lambda = clampf(m_sp.apical, 0.05f, 0.95f);
    const int shedAge = std::max(2, static_cast<int>(m_sp.shed));
    const float y0 = m_height * m_sp.crownBase;

    auto axillaryDirection = [&](const Metamer &m) {
        const Vec3 u = perpendicular(m.dir);
        const Vec3 v = cross(m.dir, u);
        const Vec3 around = u * std::cos(m.azimuth) + v * std::sin(m.azimuth);
        return normalize(m.dir * std::cos(branchAngle) + around * std::sin(branchAngle));
    };
    auto addMetamer = [&](Vec3 p, Vec3 dir, int parent, int order, float azimuth, bool axil, int iteration) {
        Metamer m;
        m.p = p;
        m.dir = dir;
        m.parent = parent;
        m.order = order;
        m.azimuth = azimuth;
        m.axil = axil;
        m.lastGain = iteration;
        m.axDir = axillaryDirection(m);
        met.push_back(m);
        return static_cast<int>(met.size() - 1);
    };

    // The stems: a bare bole up to the crown, then the shoot.
    auto stem = [&](Vec3 base, float leanAzimuth, float leanDegrees) {
        Vec3 d = normalize(Vec3{std::sin(rad(leanDegrees)) * std::cos(leanAzimuth), 1.0f,
                                std::sin(rad(leanDegrees)) * std::sin(leanAzimuth)});
        const float bole = std::max(y0, 0.2f);
        const int n = std::max(2, static_cast<int>(bole / std::min(0.8f, std::max(step, bole / 6.0f))));
        Vec3 p = base;
        int prev = -1;
        int first = -1;
        for (int i = 0; i <= n; ++i) {
            if (i > 0) {
                p += d * (bole / fl(n));
                d = normalize(d + m_rng.unitVector() * std::sin(rad(m_sp.wobble)) + Vec3{0, 0.15f, 0});
            }
            const int id = addMetamer(p, d, prev, 0, m_rng.range(0.0f, 2.0f * kPi), i == n, 0);
            if (prev >= 0)
                met[static_cast<std::size_t>(prev)].main = id;
            else
                first = id;
            prev = id;
        }
        roots.push_back(first);
    };
    if (m_hedge) {
        const int n = std::max(3, static_cast<int>(m_sp.hedgeLength / 0.3f + 0.5f));
        for (int i = 0; i < n; ++i) {
            const float x = lerp(-0.5f * m_sp.hedgeLength + 0.1f, 0.5f * m_sp.hedgeLength - 0.1f,
                                 (fl(i) + m_rng.range(0.2f, 0.8f)) / fl(n));
            const float side = m_rng.uniform() < 0.5f ? 1.0f : -1.0f;
            stem({x, 0, side * m_rng.range(0.0f, 0.15f) * m_crownR}, side > 0 ? 0.5f * kPi : 1.5f * kPi,
                 m_sp.stemLean * m_rng.range(0.3f, 1.0f));
        }
    } else if (m_sp.stems <= 1) {
        stem({0, 0, 0}, m_rng.range(0.0f, 2.0f * kPi), m_sp.lean);
    } else {
        for (int i = 0; i < m_sp.stems; ++i) {
            const float a = m_rng.range(0.0f, 2.0f * kPi);
            const float r = 0.08f * m_crownR * std::sqrt(m_rng.uniform());
            stem({r * std::cos(a), 0, r * std::sin(a)}, a, m_sp.stemLean * m_rng.range(0.5f, 1.3f));
        }
    }

    std::vector<Vec3> markers = sampleMarkers(markerCount());
    const int maxMetamers = 80000;

    for (int iteration = 1; iteration <= 160 && !markers.empty() && static_cast<int>(met.size()) < maxMetamers;
         ++iteration) {
        const std::size_t n = met.size();
        std::map<long long, std::vector<int>> grid;
        auto cell = [&](float v) { return static_cast<long long>(std::floor(v / perception)) + 4096; };
        auto key = [&](long long x, long long y, long long z) { return (x * 8192 + y) * 8192 + z; };
        for (std::size_t i = 0; i < n; ++i) {
            met[i].qT = met[i].qA = 0;
            met[i].vT = met[i].vA = Vec3();
            grid[key(cell(met[i].p.x), cell(met[i].p.y), cell(met[i].p.z))].push_back(static_cast<int>(i));
        }

        // Each marker is seen by the nearest bud that has it in view.
        for (const Vec3 &mk : markers) {
            int best = -1;
            bool bestAxil = false;
            float bestD = perception;
            const long long cx = cell(mk.x), cy = cell(mk.y), cz = cell(mk.z);
            for (long long dx = -1; dx <= 1; ++dx)
                for (long long dy = -1; dy <= 1; ++dy)
                    for (long long dz = -1; dz <= 1; ++dz) {
                        const auto it = grid.find(key(cx + dx, cy + dy, cz + dz));
                        if (it == grid.end())
                            continue;
                        for (int id : it->second) {
                            const Metamer &m = met[static_cast<std::size_t>(id)];
                            const Vec3 to = mk - m.p;
                            const float d = length(to);
                            if (d >= bestD || d < 1e-6f)
                                continue;
                            const Vec3 u = to * (1.0f / d);
                            // The nearest bud wins, with a bud that points at the marker counting as nearer.
                            if (m.main < 0) {
                                const float a = dot(m.dir, u);
                                const float cost = d / (0.4f + 0.6f * a);
                                if (a >= cosCone && cost < bestD) {
                                    best = id;
                                    bestAxil = false;
                                    bestD = cost;
                                }
                            }
                            if (m.lateral < 0 && m.axil) {
                                const float a = dot(m.axDir, u);
                                const float cost = d / (0.4f + 0.6f * a);
                                if (a >= cosCone && cost < bestD) {
                                    best = id;
                                    bestAxil = true;
                                    bestD = cost;
                                }
                            }
                        }
                    }
            if (best >= 0) {
                Metamer &m = met[static_cast<std::size_t>(best)];
                const Vec3 u = normalize(mk - m.p);
                if (bestAxil) {
                    m.qA += 1;
                    m.vA += u;
                } else {
                    m.qT += 1;
                    m.vT += u;
                }
                m.lastGain = iteration;
            }
        }

        // Supply, basipetally; and when each part last found space.
        for (std::size_t i = n; i-- > 0;) {
            Metamer &m = met[i];
            const float mainQ = m.main >= 0 ? met[static_cast<std::size_t>(m.main)].qSub : m.qT;
            const float latQ = m.lateral >= 0 ? met[static_cast<std::size_t>(m.lateral)].qSub : m.qA;
            m.qSub = mainQ + latQ;
            m.gainSub = m.lastGain;
            if (m.main >= 0)
                m.gainSub = std::max(m.gainSub, met[static_cast<std::size_t>(m.main)].gainSub);
            if (m.lateral >= 0)
                m.gainSub = std::max(m.gainSub, met[static_cast<std::size_t>(m.lateral)].gainSub);
        }
        // Side branches that found no space for a while are shed.
        for (std::size_t i = 0; i < n; ++i) {
            Metamer &m = met[i];
            if (m.lateral < 0 || iteration - met[static_cast<std::size_t>(m.lateral)].gainSub <= shedAge)
                continue;
            std::vector<int> stack = {m.lateral};
            while (!stack.empty()) {
                const int id = stack.back();
                stack.pop_back();
                Metamer &dead = met[static_cast<std::size_t>(id)];
                if (dead.main >= 0)
                    stack.push_back(dead.main);
                if (dead.lateral >= 0)
                    stack.push_back(dead.lateral);
                dead.axil = false;
                dead.main = dead.lateral = -1;
                dead.qT = dead.qA = 0;
                dead.qSub = 0;
                dead.gainSub = 0;
                dead.p = Vec3{1e9f, 1e9f, 1e9f}; // out of the way of every later search
            }
            m.lateral = -1;
            m.axil = false;
            m.qA = 0;
        }

        // Vigor, acropetally (Borchert and Honda 1984).
        for (std::size_t i = 0; i < n; ++i)
            met[i].vig = met[i].vigT = met[i].vigA = 0;
        for (int r : roots)
            met[static_cast<std::size_t>(r)].vig = vigorScale * met[static_cast<std::size_t>(r)].qSub;
        for (std::size_t i = 0; i < n; ++i) {
            Metamer &m = met[i];
            if (m.p.x > 1e8f)
                continue;
            const float mainQ = m.main >= 0 ? met[static_cast<std::size_t>(m.main)].qSub : m.qT;
            const float latQ = m.lateral >= 0 ? met[static_cast<std::size_t>(m.lateral)].qSub : m.qA;
            const float den = lambda * mainQ + (1.0f - lambda) * latQ;
            const float vm = den > 0 ? m.vig * lambda * mainQ / den : 0.0f;
            const float vl = den > 0 ? m.vig * (1.0f - lambda) * latQ / den : 0.0f;
            if (m.main >= 0)
                met[static_cast<std::size_t>(m.main)].vig = vm;
            else
                m.vigT = vm;
            if (m.lateral >= 0)
                met[static_cast<std::size_t>(m.lateral)].vig = vl;
            else
                m.vigA = vl;
        }

        // Growth.
        bool grew = false;
        auto grow = [&](int from, Vec3 dir, Vec3 pull, bool hasPull, int count, bool lateral) {
            int prev = from;
            Vec3 d = dir;
            for (int k = 0; k < count; ++k) {
                const Metamer &pm = met[static_cast<std::size_t>(prev)];
                Vec3 nd = d * 0.55f;
                if (hasPull)
                    nd += pull * 0.35f;
                nd += Vec3{0, 0.06f, 0} + m_rng.unitVector() * 0.06f;
                nd = normalize(nd);
                const Vec3 p = pm.p + nd * step;
                if (outside(p, 1.03f, std::max(0.05f, 0.08f * m_crownR)))
                    return;
                const float az = pm.azimuth + kGoldenAngle;
                const int id =
                    addMetamer(p, nd, prev, (lateral && k == 0) ? pm.order + 1 : pm.order, az, true, iteration);
                if (lateral && k == 0)
                    met[static_cast<std::size_t>(prev)].lateral = id;
                else
                    met[static_cast<std::size_t>(prev)].main = id;
                prev = id;
                d = nd;
                grew = true;
            }
        };
        for (std::size_t i = 0; i < n; ++i) {
            if (met[i].p.x > 1e8f)
                continue;
            if (met[i].main < 0 && met[i].vigT >= 1.0f) {
                const Metamer m = met[i];
                grow(static_cast<int>(i), m.dir, m.qT > 0 ? normalize(m.vT) : m.dir, m.qT > 0,
                     std::min(3, static_cast<int>(m.vigT)), false);
            }
            if (met[i].lateral < 0 && met[i].axil && met[i].vigA >= 1.0f) {
                const Metamer m = met[i];
                const Vec3 pull = m.qA > 0 ? normalize(m.vA) : m.axDir;
                grow(static_cast<int>(i), normalize(m.axDir * 0.5f + pull * 0.5f), pull, m.qA > 0,
                     std::min(3, static_cast<int>(m.vigA)), true);
            }
        }
        if (!grew) {
            // No bud sees any marker: the stem keeps growing up, as a young tree does, until it reaches some.
            for (int r : roots) {
                int tip = r;
                while (met[static_cast<std::size_t>(tip)].main >= 0)
                    tip = met[static_cast<std::size_t>(tip)].main;
                const Metamer m = met[static_cast<std::size_t>(tip)];
                if (m.p.y < y0 + 0.4f * (m_height - y0))
                    grow(tip, m.dir, Vec3(), false, 3, false);
            }
        }
        if (!grew)
            break;

        // Markers that a metamer has come near are used up.
        std::map<long long, std::vector<int>> fresh;
        auto cellO = [&](float v) { return static_cast<long long>(std::floor(v / occupancy)) + 4096; };
        for (std::size_t i = 0; i < met.size(); ++i)
            if (met[i].p.x < 1e8f)
                fresh[key(cellO(met[i].p.x), cellO(met[i].p.y), cellO(met[i].p.z))].push_back(static_cast<int>(i));
        std::vector<Vec3> left;
        left.reserve(markers.size());
        for (const Vec3 &mk : markers) {
            bool used = false;
            const long long cx = cellO(mk.x), cy = cellO(mk.y), cz = cellO(mk.z);
            for (long long dx = -1; dx <= 1 && !used; ++dx)
                for (long long dy = -1; dy <= 1 && !used; ++dy)
                    for (long long dz = -1; dz <= 1 && !used; ++dz) {
                        const auto it = fresh.find(key(cx + dx, cy + dy, cz + dz));
                        if (it == fresh.end())
                            continue;
                        for (int id : it->second)
                            if (length(met[static_cast<std::size_t>(id)].p - mk) < occupancy) {
                                used = true;
                                break;
                            }
                    }
            if (!used)
                left.push_back(mk);
        }
        markers.swap(left);
    }

    // The metamers become branches: a chain along the main axis, and a new chain for each side shoot.
    struct Item {
        int meta, order, chain, node;
    };
    std::vector<Item> queue;
    for (int r : roots)
        queue.push_back({r, 0, -1, 0});
    const float tipFrac = m_sp.leaderEnd > 0.95f ? 0.03f : 0.25f;
    m_trunkR = m_height * m_sp.trunkRadiusRatio;
    for (std::size_t qi = 0; qi < queue.size(); ++qi) {
        const Item item = queue[qi];
        Branch b;
        b.level = std::min(item.order, 3);
        b.parent = item.chain;
        b.parentNode = item.node;
        b.phase = m_rng.uniform();
        const float shade = m_rng.range(0.82f, 1.0f);
        b.tint[0] = b.tint[1] = b.tint[2] = item.order == 0 ? 0.78f : shade;
        if (b.level == 3) {
            b.tint[0] *= 0.9f;
            b.tint[2] *= 0.75f;
        }
        const std::size_t self = m_branches.size();
        const Metamer &start = met[static_cast<std::size_t>(item.meta)];
        if (item.order > 0 && start.parent >= 0)
            b.nodes.push_back({met[static_cast<std::size_t>(start.parent)].p, 0.002f});
        for (int m = item.meta; m >= 0; m = met[static_cast<std::size_t>(m)].main) {
            const Metamer &cur = met[static_cast<std::size_t>(m)];
            b.nodes.push_back({cur.p, 0.002f});
            if (cur.lateral >= 0)
                queue.push_back(
                    {cur.lateral, item.order + 1, static_cast<int>(self), static_cast<int>(b.nodes.size()) - 1});
        }
        if (b.nodes.size() < 2)
            continue;
        for (std::size_t i = 1; i < b.nodes.size(); ++i)
            b.length += length(b.nodes[i].p - b.nodes[i - 1].p);
        if (item.order == 0) {
            // The trunk tapers to its top and flares at the ground.
            const float top = std::max(b.nodes.back().p.y, 0.2f);
            if (m_branches.empty())
                m_trunkTop = top;
            for (Node3 &nd : b.nodes) {
                const float h = nd.p.y;
                const float taper = 1.0f - (1.0f - tipFrac) * std::pow(clampf(h / top, 0, 1), 0.8f);
                const float flare = 1.0f + m_sp.flare * std::exp(-h / (0.6f + 0.04f * m_height));
                nd.r = m_trunkR * taper * flare;
            }
        }
        m_branches.push_back(b);
    }
}

// Fills the crown with twigs by space colonization. Attraction points are scattered in the crown, more of them near
// its surface, where trees have most of their foliage because of the light. Each point pulls the nearest branch
// node within a radius of influence; every node with points grows a segment towards the mean direction of its
// points, and a point is removed when a node comes close. Twigs therefore spread to every part of the crown, avoid
// crossing and meet where there is room, which a random spray of twigs does not do.
inline void TreeBuilder::fillCrown() {
    if (m_sp.fill <= 0)
        return;
    struct Marker {
        Vec3 p;
        int branch, index;
    };
    const float step = clampf(m_sp.leafLength * 2.5f, 0.03f, 0.35f);
    const float influence = std::max(6.0f * step, 0.3f * m_crownR);
    const float kill = 1.5f * step;
    const float y0 = m_height * m_sp.crownBase;
    const int count = markerCount();

    std::vector<Vec3> points = sampleMarkers(count);

    std::vector<Marker> nodes;
    std::map<long long, std::vector<int>> grid;
    auto key = [&](Vec3 p) {
        const long long ix = static_cast<long long>(std::floor(p.x / influence)) + 4096;
        const long long iy = static_cast<long long>(std::floor(p.y / influence)) + 4096;
        const long long iz = static_cast<long long>(std::floor(p.z / influence)) + 4096;
        return (ix * 8192 + iy) * 8192 + iz;
    };
    auto addNode = [&](Vec3 p, int branch, int index) {
        nodes.push_back({p, branch, index});
        grid[key(p)].push_back(static_cast<int>(nodes.size() - 1));
    };
    for (std::size_t bi = 0; bi < m_branches.size(); ++bi)
        for (std::size_t i = 0; i < m_branches[bi].nodes.size(); ++i)
            if (m_branches[bi].level > 0 || m_branches[bi].nodes[i].p.y >= y0)
                addNode(m_branches[bi].nodes[i].p, static_cast<int>(bi), static_cast<int>(i));
    const std::size_t skeleton = m_branches.size();

    // Calls f(node index) for each node within radius of p.
    auto near = [&](Vec3 p, float radius, auto &&f) {
        const long long cx = static_cast<long long>(std::floor(p.x / influence));
        const long long cy = static_cast<long long>(std::floor(p.y / influence));
        const long long cz = static_cast<long long>(std::floor(p.z / influence));
        for (long long dx = -1; dx <= 1; ++dx)
            for (long long dy = -1; dy <= 1; ++dy)
                for (long long dz = -1; dz <= 1; ++dz) {
                    const auto it = grid.find(((cx + dx + 4096) * 8192 + (cy + dy + 4096)) * 8192 + cz + dz + 4096);
                    if (it == grid.end())
                        continue;
                    for (int n : it->second)
                        if (length(nodes[static_cast<std::size_t>(n)].p - p) <= radius)
                            f(n);
                }
    };

    for (int iteration = 0; iteration < 160 && !points.empty() && nodes.size() < 70000; ++iteration) {
        std::vector<Vec3> pull(nodes.size());
        std::vector<int> pulls(nodes.size(), 0);
        bool any = false;
        for (const Vec3 &pt : points) {
            int best = -1;
            float bestD = influence;
            near(pt, influence, [&](int n) {
                const float d = length(nodes[static_cast<std::size_t>(n)].p - pt);
                if (d < bestD) {
                    bestD = d;
                    best = n;
                }
            });
            if (best >= 0) {
                const std::size_t b = static_cast<std::size_t>(best);
                pull[b] += normalize(pt - nodes[b].p);
                ++pulls[b];
                any = true;
            }
        }
        if (!any)
            break;
        const std::size_t before = nodes.size();
        for (std::size_t n = 0; n < before; ++n) {
            if (pulls[n] == 0)
                continue;
            const Marker from = nodes[n];
            const Vec3 dir = normalize(normalize(pull[n]) + m_rng.unitVector() * 0.2f + Vec3{0, 0.05f, 0});
            const Vec3 p = from.p + dir * step;
            if (outside(p, 1.03f, 0.05f))
                continue;
            Branch &host = m_branches[static_cast<std::size_t>(from.branch)];
            if (static_cast<std::size_t>(from.branch) >= skeleton &&
                static_cast<std::size_t>(from.index) + 1 == host.nodes.size()) {
                host.nodes.push_back({p, 0.002f});
                host.length += step;
                addNode(p, from.branch, from.index + 1);
            } else {
                Branch b;
                b.level = 3;
                b.parent = from.branch;
                b.parentNode = std::min(from.index, static_cast<int>(host.nodes.size()) - 2);
                b.phase = m_rng.uniform();
                const float shade = m_rng.range(0.82f, 1.0f);
                b.tint[0] = shade * 0.9f;
                b.tint[1] = shade;
                b.tint[2] = shade * 0.75f;
                b.nodes = {{from.p, 0.002f}, {p, 0.002f}};
                b.length = step;
                m_branches.push_back(b);
                addNode(p, static_cast<int>(m_branches.size() - 1), 1);
            }
        }
        // Points that a node has reached are used up.
        std::vector<Vec3> left;
        for (const Vec3 &pt : points) {
            bool reached = false;
            near(pt, kill, [&](int) { reached = true; });
            if (!reached)
                left.push_back(pt);
        }
        if (left.size() == points.size() && nodes.size() == before)
            break;
        points.swap(left);
    }
}

// Sets the radius of every branch from the pipe model (Shinozaki et al. 1964): the cross-section of a branch below a
// junction is the sum of those above it, so limbs swell where they take on branches and thin along their length.
// Radii are summed as r^2.5, which fits trees better than the 2 of a perfect pipe (MacDonald 1983, used by
// Palubicki et al. 2009). The result is scaled so that the limbs of the trunk keep the radius that the species asks
// for.
inline void TreeBuilder::pipeRadii() {
    constexpr double n = 2.5;
    constexpr double tip = 0.002;
    constexpr double taper = 1.0e-6; // growth of r^n per metre of branch with no branches on it
    std::vector<std::vector<double>> attach(m_branches.size());
    for (std::size_t i = 0; i < m_branches.size(); ++i)
        attach[i].assign(m_branches[i].nodes.size(), 0.0);
    std::vector<double> original(m_branches.size(), 0.0);
    for (std::size_t bi = m_branches.size(); bi-- > 0;) {
        Branch &b = m_branches[bi];
        if (b.level == 0 && !m_sp.competition)
            continue;
        original[bi] = static_cast<double>(b.nodes.front().r);
        const std::size_t k = b.nodes.size();
        std::vector<double> rn(k);
        rn[k - 1] = std::pow(tip, n) + attach[bi][k - 1];
        for (std::size_t i = k - 1; i-- > 0;)
            rn[i] = rn[i + 1] + attach[bi][i] + taper * static_cast<double>(length(b.nodes[i + 1].p - b.nodes[i].p));
        for (std::size_t i = 0; i < k; ++i)
            b.nodes[i].r = static_cast<float>(std::pow(rn[i], 1.0 / n));
        if (b.parent >= 0) {
            const std::size_t parent = static_cast<std::size_t>(b.parent);
            attach[parent][std::min(static_cast<std::size_t>(b.parentNode), attach[parent].size() - 1)] += rn[0];
        }
    }
    if (m_sp.competition) {
        // The trunk is part of the pipe model too; the scale gives it the radius that the species asks for at
        // breast height, and it flares at the ground.
        const Branch &trunk = m_branches[0];
        std::size_t at = 0;
        while (at + 1 < trunk.nodes.size() && trunk.nodes[at].p.y < 1.3f)
            ++at;
        const float s = clampf(m_trunkR / std::max(trunk.nodes[at].r, 1e-5f), 0.5f, 5.0f);
        for (Branch &b : m_branches)
            for (Node3 &nd : b.nodes) {
                nd.r = std::max(0.0012f, nd.r * s);
                if (b.level == 0)
                    nd.r *= 1.0f + m_sp.flare * std::exp(-nd.p.y / (0.6f + 0.04f * m_height));
            }
        return;
    }
    double before = 0, after = 0;
    int limbs = 0;
    for (std::size_t bi = 0; bi < m_branches.size(); ++bi)
        if (m_branches[bi].level == 1) {
            before += original[bi];
            after += static_cast<double>(m_branches[bi].nodes.front().r);
            ++limbs;
        }
    const float scale = limbs > 0 && after > 0 ? clampf(static_cast<float>(before / after), 0.5f, 3.0f) : 1.0f;
    for (Branch &b : m_branches) {
        if (b.level == 0)
            continue;
        for (Node3 &nd : b.nodes)
            nd.r = std::max(0.0012f, nd.r * scale);
        // A limb is thinner than the trunk where it leaves it.
        if (b.level == 1) {
            const Branch &trunk = m_branches[static_cast<std::size_t>(b.parent)];
            const std::size_t i = std::min(static_cast<std::size_t>(b.parentNode), trunk.nodes.size() - 1);
            const float cap = 0.85f * trunk.nodes[i].r;
            const float f = b.nodes.front().r > cap ? cap / b.nodes.front().r : 1.0f;
            for (Node3 &nd : b.nodes)
                nd.r *= f;
        }
    }
}

// Grows one stem from base, leaning leanDegrees towards leanAzimuth, and its branches.
inline void TreeBuilder::growStem(Vec3 base, float leanAzimuth, float leanDegrees, float topScale) {
    using tree_detail::rad;
    Branch b;
    b.level = 0;
    b.phase = m_rng.uniform();
    b.tint[0] = b.tint[1] = b.tint[2] = 0.78f;
    float top = std::max(m_height * m_sp.leaderEnd, m_height * m_sp.crownBase + 1.0f);
    top = std::min(top, m_height) * topScale;
    top = std::max(top, 0.2f);
    if (m_branches.empty()) {
        m_trunkTop = top;
        m_trunkR = m_height * m_sp.trunkRadiusRatio;
    }
    const float tipFrac = m_sp.leaderEnd > 0.95f ? 0.03f : 0.25f;
    const int nseg = std::clamp(static_cast<int>(top / 0.8f), m_sp.stems > 1 ? 4 : 8, 24);
    Vec3 d = normalize(Vec3{std::sin(rad(leanDegrees)) * std::cos(leanAzimuth), 1.0f,
                            std::sin(rad(leanDegrees)) * std::sin(leanAzimuth)});
    Vec3 p = base;
    float prev = 0;
    for (int i = 0; i <= nseg; ++i) {
        const float s = std::pow(fl(i) / fl(nseg), 1.4f);
        if (i > 0) {
            p += d * (top * (s - prev));
            d = normalize(d + m_rng.unitVector() * std::sin(rad(m_sp.wobble)) + Vec3{0, 0.15f, 0});
        }
        prev = s;
        const float h = p.y - base.y;
        const float taper = 1.0f - (1.0f - tipFrac) * std::pow(clampf(h / top, 0, 1), 0.8f);
        const float flare = 1.0f + m_sp.flare * std::exp(-h / (0.6f + 0.04f * m_height));
        b.nodes.push_back({p, m_trunkR * taper * flare});
    }
    for (std::size_t i = 1; i < b.nodes.size(); ++i)
        b.length += length(b.nodes[i].p - b.nodes[i - 1].p);
    m_branches.push_back(b);
    spawn(static_cast<int>(m_branches.size() - 1), 1);
}

// A tree has one trunk. A shrub has several stems that spread from the ground; a hedge has a row of them along its
// length.
inline void TreeBuilder::growTrunk() {
    using tree_detail::rad;
    if (m_hedge) {
        const int n = std::max(3, static_cast<int>(m_sp.hedgeLength / 0.3f + 0.5f));
        for (int i = 0; i < n; ++i) {
            const float x = lerp(-0.5f * m_sp.hedgeLength + 0.1f, 0.5f * m_sp.hedgeLength - 0.1f,
                                 (fl(i) + m_rng.range(0.2f, 0.8f)) / fl(n));
            const float side = m_rng.uniform() < 0.5f ? 1.0f : -1.0f;
            growStem({x, 0, side * m_rng.range(0.0f, 0.15f) * m_crownR}, side > 0 ? 0.5f * kPi : 1.5f * kPi,
                     m_sp.stemLean * m_rng.range(0.3f, 1.0f), m_rng.range(0.85f, 1.0f));
        }
        return;
    }
    if (m_sp.stems <= 1) {
        growStem({0, 0, 0}, m_rng.range(0.0f, 2.0f * kPi), m_sp.lean, 1.0f);
        return;
    }
    for (int i = 0; i < m_sp.stems; ++i) {
        const float a = m_rng.range(0.0f, 2.0f * kPi);
        const float r = 0.08f * m_crownR * std::sqrt(m_rng.uniform());
        growStem({r * std::cos(a), 0, r * std::sin(a)}, a, m_sp.stemLean * m_rng.range(0.5f, 1.3f),
                 m_rng.range(0.8f, 1.0f));
    }
}

inline void TreeBuilder::addBranchTube(Primitive &prim, const Branch &b, const tree_detail::LodSpec &lod,
                                       int sidesOverride) {
    std::vector<TubePoint> pts;
    for (std::size_t i = 0; i < b.nodes.size(); ++i) {
        if (b.nodes[i].r < lod.minRadius && i > 0)
            break;
        const float s = fl(i) / fl(b.nodes.size() - 1);
        TubePoint tp;
        tp.p = b.nodes[i].p;
        tp.r = b.nodes[i].r;
        tp.windX = std::pow(clampf(tp.p.y / m_height, 0, 1), 1.5f);
        tp.windY = flexWeight(b.level) * (0.4f + 0.6f * s);
        pts.push_back(tp);
    }
    if (pts.size() < 2)
        return;
    const int sides = sidesOverride > 0 ? sidesOverride : sidesFor(lod, pts.front().r);
    const float wraps = std::max(1.0f, std::round(2.0f * kPi * pts.front().r / 0.5f));
    addTube(prim, pts, sides, wraps, 0.5f, b.tint, b.phase);
}

inline void TreeBuilder::placeLeaves(Primitive &prim, int leafLevel, std::size_t budget, std::size_t &placed) {
    const TreeSpecies &sp = m_sp;
    const float fill = sp.leafShape == LeafShape::Needle ? 0.5f * 3.0f : 0.65f;
    const float leafArea = sp.leafLength * sp.leafLength * sp.leafAspect * fill;
    const float crownArea =
        m_hedge ? m_sp.hedgeLength * m_sp.hedgeWidth : kPi * (0.85f * m_crownR) * (0.85f * m_crownR);
    const double target = static_cast<double>(sp.lai * m_opt.density * crownArea);
    const double wanted = target / static_cast<double>(leafArea);
    const std::size_t count = static_cast<std::size_t>(std::min(wanted, static_cast<double>(budget)));
    if (count == 0)
        return;
    const float scale = static_cast<float>(std::sqrt(wanted / static_cast<double>(count)));

    struct Seg {
        std::size_t branch, node;
    };
    std::vector<Seg> segs;
    std::vector<double> cumulative;
    double total = 0;
    for (std::size_t bi = 0; bi < m_branches.size(); ++bi) {
        const Branch &b = m_branches[bi];
        if (b.level != leafLevel)
            continue;
        for (std::size_t i = 1; i < b.nodes.size(); ++i) {
            const double w = static_cast<double>(length(b.nodes[i].p - b.nodes[i - 1].p)) *
                             (0.25 + 0.75 * static_cast<double>(fl(i) / fl(b.nodes.size() - 1)));
            total += w;
            segs.push_back({bi, i});
            cumulative.push_back(total);
        }
    }
    if (segs.empty())
        return;

    const Profile profile = profileFor(sp.leafShape);
    const int blades = sp.leafShape == LeafShape::Needle ? 3 : 1;
    for (std::size_t k = 0; k < count; ++k) {
        const double u =
            (static_cast<double>(k) + static_cast<double>(m_rng.uniform())) / static_cast<double>(count) * total;
        const std::size_t si =
            static_cast<std::size_t>(std::lower_bound(cumulative.begin(), cumulative.end(), u) - cumulative.begin());
        const Seg &sg = segs[std::min(si, segs.size() - 1)];
        const Branch &b = m_branches[sg.branch];
        const Vec3 a = b.nodes[sg.node - 1].p, c = b.nodes[sg.node].p;
        const Vec3 tw = normalize(c - a);
        const float len = sp.leafLength * scale * m_rng.range(0.8f, 1.2f);
        Vec3 base = lerp(a, c, m_rng.uniform()) + m_rng.unitVector() * ((m_sp.fill > 0 ? 0.5f : 0.15f) * len);
        if (m_hedge) {
            // A clipped hedge has no leaves that start outside its box.
            base.x = clampf(base.x, -0.5f * sp.hedgeLength, 0.5f * sp.hedgeLength);
            base.y = clampf(base.y, 0.0f, m_height);
            base.z = clampf(base.z, -m_crownR, m_crownR);
        }
        const float droop = sp.leafDroop;
        Vec3 axis = tw * (1.0f - 0.5f * droop) + Vec3{0, -1, 0} * droop + m_rng.unitVector() * 0.6f;
        if (sp.leafShape == LeafShape::Needle)
            axis = tw + Vec3{0, -droop, 0} + m_rng.unitVector() * 0.5f;
        Vec3 normal = m_rng.unitVector() * 0.7f + Vec3{0, 1.0f - 0.6f * droop, 0};
        const float horizontal = std::sqrt(base.x * base.x + base.z * base.z);
        const float depth = clampf(horizontal / (envelope(base.y) + 1.0f), 0.0f, 1.0f);
        const float shade = clampf((0.65f + 0.35f * depth) * m_rng.range(0.88f, 1.08f), 0.0f, 1.0f);
        const float tint[3] = {shade * m_leafTint[0], shade * m_leafTint[1], shade * m_leafTint[2]};
        const float wind[4] = {std::pow(clampf(base.y / m_height, 0, 1), 1.5f), flexWeight(b.level), 1.0f, b.phase};
        addLeaf(prim, base, axis, normal, len, len * sp.leafAspect, blades > 1 ? 1 : m_leafSegs, profile, tint, wind,
                blades);
    }
    placed += count;
}

inline Mesh TreeBuilder::meshLod(int lod, std::size_t &leaves) {
    using namespace tree_detail;
    Mesh mesh;
    mesh.name = "LOD" + std::to_string(lod);
    const float barkTint[3] = {1, 1, 1};
    if (lod == 3) {
        // A trunk and a crown of revolution; a hedge is a box.
        if (m_hedge) {
            addBox(mesh.primitive(m_canopyMat), {-0.5f * m_sp.hedgeLength, 0, -m_crownR},
                   {0.5f * m_sp.hedgeLength, m_height, m_crownR}, m_leafTint, 0.5f);
            return mesh;
        }
        Primitive &bark = mesh.primitive(m_barkMat);
        const Branch &trunk = m_branches[0];
        std::vector<TubePoint> pts;
        for (const Node3 &n : trunk.nodes) {
            if (n.p.y > m_height * m_sp.crownBase + 1.0f)
                break;
            pts.push_back({n.p, n.r, 0, 0});
        }
        addTube(bark, pts, 5, 2.0f, 1.0f, barkTint, 0.0f);
        std::vector<std::pair<float, float>> prof;
        const float y0 = m_height * m_sp.crownBase;
        for (int i = 0; i <= 7; ++i) {
            const float y = lerp(y0, m_height, fl(i) / 7.0f);
            prof.push_back({y, i == 0 || i == 7 ? 0.0f : std::max(0.05f, envelope(y) * 0.9f)});
        }
        addLathe(mesh.primitive(m_canopyMat), prof, 8, m_leafTint, 0.5f, m_height);
        return mesh;
    }
    const LodSpec &spec = kLods[lod];
    Primitive &bark = mesh.primitive(m_barkMat);
    for (const Branch &b : m_branches)
        if (b.level <= spec.maxLevel)
            addBranchTube(bark, b, spec, 0);
    if (!m_opt.leafOff) {
        // The leaves go on the finest order of branch that this level of detail has.
        int leafLevel = 1;
        for (const Branch &b : m_branches)
            if (b.level <= std::min(m_terminal, spec.maxLevel) && b.level > leafLevel && b.nodes.size() > 1)
                leafLevel = b.level;
        // A coarse level keeps at least a few hundred leaves, or the cards that stand for them would be huge.
        const std::size_t floorCount = static_cast<std::size_t>(std::min(300, m_opt.maxLeaves / 2));
        const std::size_t budget =
            std::max<std::size_t>({1, floorCount, static_cast<std::size_t>(fl(m_opt.maxLeaves) * spec.leafFraction)});
        m_leafSegs = spec.leafSegs;
        placeLeaves(mesh.primitive(m_leafMat), leafLevel, budget, leaves);
    }
    return mesh;
}

inline Result TreeBuilder::build() {
    using namespace tree_detail;
    Result res;
    Document &doc = res.doc;
    doc.generator = "ntgentree";
    const std::string id = m_sp.id;

    Textures tex(doc, m_opt.textures);
    const int barkImg = tex.get(id, "bark");
    const int leafImg = tex.get(id, "leaf");
    m_leafTextured = leafImg >= 0;
    const Optics leafOptics = opticsFor(m_sp.sensor);
    const Optics barkOpt = barkOptics(m_sp.bark);

    Material bark;
    bark.name = "bark";
    bark.base[0] = barkImg >= 0 ? 1.0f : m_sp.bark.r;
    bark.base[1] = barkImg >= 0 ? 1.0f : m_sp.bark.g;
    bark.base[2] = barkImg >= 0 ? 1.0f : m_sp.bark.b;
    bark.roughness = 0.95f;
    bark.image = barkImg;
    bark.extras = sensorExtras(m_sp.sensor, barkOpt);
    Material leaf;
    leaf.name = "leaf";
    leaf.base[0] = m_leafTextured ? 1.0f : m_sp.leaf.r;
    leaf.base[1] = m_leafTextured ? 1.0f : m_sp.leaf.g;
    leaf.base[2] = m_leafTextured ? 1.0f : m_sp.leaf.b;
    leaf.roughness = 0.7f;
    leaf.doubleSided = true;
    leaf.mask = m_leafTextured;
    leaf.image = leafImg;
    leaf.clamp = true;
    leaf.extras = sensorExtras(m_sp.sensor, leafOptics);
    Material proxy;
    proxy.name = "proxy";
    proxy.base[0] = proxy.base[1] = proxy.base[2] = 0.5f;
    Material canopy;
    canopy.name = "canopy";
    canopy.base[0] = m_sp.leaf.r;
    canopy.base[1] = m_sp.leaf.g;
    canopy.base[2] = m_sp.leaf.b;
    doc.materials = {bark, leaf, proxy, canopy};

    if (m_sp.competition) {
        growCompetition();
    } else {
        growTrunk();
        fillCrown();
    }
    pipeRadii();

    std::size_t leafCounts[4] = {0, 0, 0, 0};
    for (int lod = 0; lod < 4; ++lod)
        doc.meshes.push_back(meshLod(lod, leafCounts[lod]));
    // Meshes 0 to 3 are the levels of detail, then come the proxies for collision, ballistics and line of sight.
    const float tint[3] = {1, 1, 1};
    auto addMesh = [&doc](Mesh &m) {
        doc.meshes.push_back(m);
        return static_cast<int>(doc.meshes.size() - 1);
    };
    int trunkMesh = -1, collisionMesh = -1, crownMesh = -1;
    {
        Mesh collision, crown;
        collision.name = "Collision";
        crown.name = "Crown";
        if (m_hedge) {
            const Vec3 lo = {-0.5f * m_sp.hedgeLength, 0, -m_crownR},
                       hi = {0.5f * m_sp.hedgeLength, m_height, m_crownR};
            addBox(collision.primitive(m_proxyMat), lo, hi, tint, 0.5f);
            addBox(crown.primitive(m_proxyMat), lo, hi, tint, 0.5f);
        } else {
            Mesh trunk;
            trunk.name = "Trunk";
            std::vector<TubePoint> pts;
            for (const Node3 &n : m_branches[0].nodes)
                pts.push_back({n.p, n.r, 0, 0});
            addTube(trunk.primitive(m_proxyMat), pts, 12, 1.0f, 1.0f, tint, 0.0f);
            trunkMesh = addMesh(trunk);

            if (m_sp.stems > 1) {
                // A shrub is hit as a whole, not stem by stem.
                const float r = 0.5f * m_crownR, h = 0.6f * m_height;
                std::vector<TubePoint> cyl = {{{0, 0, 0}, r, 0, 0}, {{0, h, 0}, 0.7f * r, 0, 0}};
                addTube(collision.primitive(m_proxyMat), cyl, 8, 1.0f, 1.0f, tint, 0.0f);
            } else {
                const float h = std::min(m_height * m_sp.crownBase + 0.5f, m_trunkTop);
                std::vector<TubePoint> cyl = {{{0, 0, 0}, m_trunkR * 0.9f, 0, 0}, {{0, h, 0}, m_trunkR * 0.7f, 0, 0}};
                addTube(collision.primitive(m_proxyMat), cyl, 6, 1.0f, 1.0f, tint, 0.0f);
            }
            std::vector<std::pair<float, float>> prof;
            const float y0 = m_height * m_sp.crownBase;
            for (int i = 0; i <= 8; ++i) {
                const float y = lerp(y0, m_height, fl(i) / 8.0f);
                prof.push_back({y, i == 0 || i == 8 ? 0.0f : std::max(0.05f, envelope(y))});
            }
            addLathe(crown.primitive(m_proxyMat), prof, 10, tint, 0.5f, m_height);
        }
        collisionMesh = addMesh(collision);
        crownMesh = addMesh(crown);
    }

    // The scene: the levels of detail, the felled variant and the proxies under one root.
    const float distances[4] = {m_height * 10.0f, m_height * 30.0f, m_height * 90.0f, 0.0f};
    auto addNode = [&doc](const std::string &name, int mesh, const std::string &extras) {
        Node n;
        n.name = name;
        n.mesh = mesh;
        n.extras = extras;
        doc.nodes.push_back(n);
        const int index = static_cast<int>(doc.nodes.size() - 1);
        doc.nodes[0].children.push_back(index);
        return index;
    };
    doc.nodes.push_back(Node());
    doc.nodes[0].name = id;
    std::string lodJson;
    for (int lod = 0; lod < 4; ++lod) {
        const Mesh &m = doc.meshes[static_cast<std::size_t>(lod)];
        res.lods.push_back({m.name, m.triangles(), distances[lod]});
        addNode(m.name, lod,
                "{\"nt\":{\"lod\":" + std::to_string(lod) + ",\"maxDistance\":" + jsonNumber(distances[lod]) + "}}");
        lodJson += (lod ? "," : "") + std::string("{\"node\":") + jsonEscape(m.name) +
                   ",\"triangles\":" + std::to_string(m.triangles()) +
                   ",\"leaves\":" + std::to_string(leafCounts[lod]) + ",\"maxDistance\":" + jsonNumber(distances[lod]) +
                   "}";
    }
    // A felled tree or shrub lies along +x, with its trunk on the ground. A hedge is not felled.
    if (!m_hedge) {
        const int felled = addNode("Felled", 1, "{\"nt\":{\"variant\":\"felled\"}}");
        doc.nodes[static_cast<std::size_t>(felled)].rotated = true;
        doc.nodes[static_cast<std::size_t>(felled)].rotation[2] = -std::sqrt(0.5f);
        doc.nodes[static_cast<std::size_t>(felled)].rotation[3] = std::sqrt(0.5f);
        doc.nodes[static_cast<std::size_t>(felled)].translation[1] = m_trunkR;
    }
    addNode("Collision", collisionMesh, "{\"nt\":{\"proxy\":\"collision\"}}");
    if (trunkMesh >= 0)
        addNode("Trunk", trunkMesh, "{\"nt\":{\"proxy\":\"ballistic\"}}");
    addNode("Crown", crownMesh, "{\"nt\":{\"proxy\":\"los\"}}");

    const std::string ballistic = m_hedge ? "Collision" : "Trunk";
    doc.sceneExtras =
        "{\"nt\":{\"kind\":" + jsonEscape(m_sp.kind) + ",\"species\":" + jsonEscape(id) +
        ",\"common\":" + jsonEscape(m_sp.common) + ",\"latin\":" + jsonEscape(m_sp.latin) +
        ",\"region\":" + jsonEscape(m_sp.region) + ",\"seed\":" + std::to_string(m_opt.seed) +
        ",\"height\":" + jsonNumber(m_height) + ",\"crownRadius\":" + jsonNumber(m_crownR) +
        ",\"trunkRadius\":" + jsonNumber(m_trunkR) +
        (m_hedge ? ",\"hedge\":{\"length\":" + jsonNumber(m_sp.hedgeLength) +
                       ",\"width\":" + jsonNumber(m_sp.hedgeWidth) + "}"
                 : std::string()) +
        ",\"leafAreaIndex\":" + jsonNumber(m_sp.lai * m_opt.density) +
        ",\"leafOff\":" + (m_opt.leafOff ? "true" : "false") + ",\"branches\":" + std::to_string(m_branches.size()) +
        ",\"sensorMaterial\":" + jsonEscape(m_sp.sensor) +
        ",\"textures\":{\"bark\":" + (barkImg >= 0 ? "true" : "false") +
        ",\"leaf\":" + (leafImg >= 0 ? "true" : "false") + "},\"lods\":[" + lodJson +
        "],\"variants\":{\"intact\":\"LOD0\"" + (m_hedge ? "" : ",\"felled\":\"Felled\"") +
        "},\"proxies\":{\"collision\":\"Collision\",\"los\":\"Crown\",\"ballistic\":" + jsonEscape(ballistic) +
        ",\"sensor\":\"Crown\"}}}";
    doc.root = 0;
    res.kind = m_sp.kind;
    res.felled = !m_hedge;
    res.ballistic = ballistic;
    res.id = id;
    res.sensor = m_sp.sensor;
    res.hasProxies = true;
    res.height = m_height;
    return res;
}

} // namespace gentree
