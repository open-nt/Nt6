// MIT License - see LICENSE
//
// Shared by the hit shaders: the primitive records, the vertex buffers behind them and the textures.

// What a ray brings back to the ray generation shader: the color the surface adds, and how far the hit was. When light
// goes on past the surface, by reflection or through glass, it also says where that ray starts, which way it goes and
// how much of what it finds is added (`carry`, zero when nothing follows); the ray generation shader traces it.
struct HitPayload {
    vec3 color;
    float distance; // along the ray, or -1 for a miss
    vec3 nextOrigin;
    float pad0;
    vec3 nextDirection;
    float pad1;
    vec3 carry;
    float pad2;
};

// The bits of Camera.flags.
const uint kFlagTraceEffects = 2u; // shadows and reflections

// One record per primitive of the scene, in the order the host built the bottom-level structures: the instance's
// custom index is the record of its mesh's first primitive, and the geometry index selects the primitive. The
// layout matches PrimitiveRecord in main.cpp.
struct Primitive {
    uint64_t positions;  // 3 floats per vertex
    uint64_t normals;    // 3 floats per vertex, or 0
    uint64_t indices;    // 3 uints per triangle
    uint64_t texCoords0; // 2 floats per vertex, or 0
    uint64_t texCoords1; // 2 floats per vertex, or 0
    uint64_t tangents;   // 4 floats per vertex, or 0
    vec4 baseColor;
    vec4 emissive;  // rgb times the strength; w is the metallic factor
    vec4 params;    // x roughness, y double sided, z alpha mode (0 opaque, 1 mask, 2 blend), w alpha cutoff
    vec4 textures;  // base color, metallic-roughness, normal, occlusion: texture indices, or -1
    vec4 textures2; // x emissive texture or -1, y bits: texture i reads TEXCOORD_1, z normal scale, w occlusion
                    // strength
    vec4 transmission; // x transmission factor, y index of refraction, z thickness, w attenuation distance
    vec4 attenuation;  // rgb: the color after that distance in the volume
    vec4 textures3;    // x transmission texture, y thickness texture, or -1 (slots 5 and 6 of the TEXCOORD_1 bits)
    vec4 uvTransform[14]; // per texture slot, two rows of KHR_texture_transform's matrix: (a, b, tx) and (c, d, ty)
};

layout(set = 0, binding = 2, std430) readonly buffer Primitives { Primitive records[]; } primitives;
layout(set = 0, binding = 3) uniform sampler2D textures[];
layout(buffer_reference, std430, buffer_reference_align = 4) readonly buffer Floats { float f[]; };
layout(buffer_reference, std430, buffer_reference_align = 4) readonly buffer Uints { uint u[]; };

vec2 fetch2(Floats data, uint vertex) { return vec2(data.f[2u * vertex], data.f[2u * vertex + 1u]); }

vec3 fetch3(Floats data, uint vertex) {
    return vec3(data.f[3u * vertex], data.f[3u * vertex + 1u], data.f[3u * vertex + 2u]);
}

vec4 fetch4(Floats data, uint vertex) {
    return vec4(data.f[4u * vertex], data.f[4u * vertex + 1u], data.f[4u * vertex + 2u], data.f[4u * vertex + 3u]);
}

uvec3 triangle(Primitive p, uint primitive) {
    Uints indices = Uints(p.indices);
    return uvec3(indices.u[3u * primitive], indices.u[3u * primitive + 1u], indices.u[3u * primitive + 2u]);
}

int textureIndex(float stored) { return int(stored); }

// Which texture coordinate set texture slot `slot` (0 base, 1 metallic-roughness, 2 normal, 3 occlusion,
// 4 emissive) reads.
int uvSet(Primitive p, int slot) { return (int(p.textures2.y) & (1 << slot)) != 0 ? 1 : 0; }

vec2 texCoordAt(Primitive p, uint vertex, int set) {
    const uint64_t address = set == 1 ? p.texCoords1 : p.texCoords0;
    if (address == 0ul)
        return vec2(0.0);
    return fetch2(Floats(address), vertex);
}

vec2 texCoord(Primitive p, uvec3 tri, vec3 weights, int set) {
    return texCoordAt(p, tri.x, set) * weights.x + texCoordAt(p, tri.y, set) * weights.y +
           texCoordAt(p, tri.z, set) * weights.z;
}

// The coordinates of texture slot `slot` after its KHR_texture_transform.
vec2 transformUv(Primitive p, int slot, vec2 uv) {
    const vec4 row0 = p.uvTransform[2 * slot];
    const vec4 row1 = p.uvTransform[2 * slot + 1];
    return vec2(dot(row0.xy, uv) + row0.z, dot(row1.xy, uv) + row1.z);
}

vec4 sampleTexture(int index, vec2 uv) { return textureLod(textures[nonuniformEXT(index)], uv, 0.0); }

// The alpha of the material at the hit, for the alpha modes.
float alphaAt(Primitive p, uint primitive, vec2 barycentrics) {
    float alpha = p.baseColor.a;
    const int baseMap = textureIndex(p.textures.x);
    if (baseMap >= 0) {
        const uvec3 tri = triangle(p, primitive);
        const vec3 weights = vec3(1.0 - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);
        alpha *= sampleTexture(baseMap, transformUv(p, 0, texCoord(p, tri, weights, uvSet(p, 0)))).a;
    }
    return alpha;
}
