// MIT License - see LICENSE
//
// Closest hit: finds the triangle's vertices through the buffer addresses of its primitive's record, builds the
// shading normal (interpolated, then perturbed by the normal map), and shades the metallic-roughness material with a
// head light and a key light: Lambert diffuse and GGX specular, ambient scaled by the occlusion map, plus emission.
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require

#include "ntgltfview_common.glsl"

layout(set = 0, binding = 0) uniform accelerationStructureEXT scene;

#include "ntgltfview_camera.glsl"
#include "ntgltfview_sky.glsl"

layout(location = 0) rayPayloadInEXT HitPayload payload;
layout(location = 1) rayPayloadEXT float visibility;
hitAttributeEXT vec2 barycentrics;

const vec3 kKeyLight = normalize(vec3(0.45, 1.0, 0.35));

const float kPi = 3.14159265;

// GGX normal distribution, Smith-Schlick geometry and Schlick's Fresnel: the usual metallic-roughness model.
vec3 specular(vec3 f0, float roughness, float nDotL, float nDotV, float nDotH, float vDotH) {
    const float a = max(roughness * roughness, 0.002);
    const float a2 = a * a;
    const float d = nDotH * nDotH * (a2 - 1.0) + 1.0;
    const float distribution = a2 / (kPi * d * d);
    const float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    const float geometry = (nDotL / (nDotL * (1.0 - k) + k)) * (nDotV / (nDotV * (1.0 - k) + k));
    const vec3 fresnel = f0 + (vec3(1.0) - f0) * pow(1.0 - vDotH, 5.0);
    return distribution * geometry * fresnel / max(4.0 * nDotL * nDotV, 1.0e-4);
}

// 1 when `point` sees the key light, 0 when something is in between. The any-hit shader sees through cut-outs.
float keyLightVisibility(vec3 point, vec3 side) {
    const float bias = max(1.0e-4, 1.0e-4 * gl_HitTEXT);
    visibility = 0.0;
    traceRayEXT(scene, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xff, 0, 0, 1,
                point + side * bias, bias, kKeyLight, 1.0e4, 1);
    return visibility;
}

void main() {
    const Primitive p = primitives.records[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
    const uvec3 tri = triangle(p, uint(gl_PrimitiveID));
    const vec3 weights = vec3(1.0 - barycentrics.x - barycentrics.y, barycentrics.x, barycentrics.y);
    Floats positions = Floats(p.positions);
    const vec3 p0 = fetch3(positions, tri.x);
    const vec3 p1 = fetch3(positions, tri.y);
    const vec3 p2 = fetch3(positions, tri.z);
    const vec2 uv0 = texCoord(p, tri, weights, 0);
    const vec2 uv1 = texCoord(p, tri, weights, 1);

    // The geometric normal, and the interpolated one when the mesh has normals.
    const vec3 faceNormal = normalize(mat3(gl_ObjectToWorldEXT) * cross(p1 - p0, p2 - p0));
    const vec3 point = vec3(gl_ObjectToWorldEXT * vec4(p0 * weights.x + p1 * weights.y + p2 * weights.z, 1.0));
    vec3 normal = faceNormal;
    if (p.normals != 0ul) {
        Floats normals = Floats(p.normals);
        normal = fetch3(normals, tri.x) * weights.x + fetch3(normals, tri.y) * weights.y +
                 fetch3(normals, tri.z) * weights.z;
        normal = normalize(mat3(gl_ObjectToWorldEXT) * normal);
    }
    const vec3 view = -gl_WorldRayDirectionEXT;
    // Light the side the ray hit.
    const vec3 side = dot(faceNormal, view) < 0.0 ? -faceNormal : faceNormal;
    if (dot(faceNormal, view) < 0.0)
        normal = -normal;

    // The normal map perturbs it in the tangent frame: the mesh's tangents, or ones derived from the triangle.
    const int normalMap = textureIndex(p.textures.z);
    if (normalMap >= 0) {
        vec3 tangent;
        float handedness = 1.0;
        if (p.tangents != 0ul) {
            Floats tangents = Floats(p.tangents);
            const vec4 t = fetch4(tangents, tri.x) * weights.x + fetch4(tangents, tri.y) * weights.y +
                           fetch4(tangents, tri.z) * weights.z;
            tangent = mat3(gl_ObjectToWorldEXT) * t.xyz;
            handedness = t.w < 0.0 ? -1.0 : 1.0;
        } else {
            const int set = uvSet(p, 2);
            const vec2 duv0 = transformUv(p, 2, texCoordAt(p, tri.x, set));
            const vec2 duv1 = transformUv(p, 2, texCoordAt(p, tri.y, set)) - duv0;
            const vec2 duv2 = transformUv(p, 2, texCoordAt(p, tri.z, set)) - duv0;
            const float det = duv1.x * duv2.y - duv2.x * duv1.y;
            const vec3 e1 = mat3(gl_ObjectToWorldEXT) * (p1 - p0);
            const vec3 e2 = mat3(gl_ObjectToWorldEXT) * (p2 - p0);
            tangent = abs(det) > 1.0e-8 ? (e1 * duv2.y - e2 * duv1.y) / det : e1;
            if (det < 0.0)
                handedness = -1.0;
        }
        tangent = normalize(tangent - normal * dot(normal, tangent));
        if (dot(tangent, tangent) > 0.5) {
            const vec3 bitangent = cross(normal, tangent) * handedness;
            vec3 sampled = sampleTexture(normalMap, transformUv(p, 2, uvSet(p, 2) == 1 ? uv1 : uv0)).xyz * 2.0 - 1.0;
            sampled.xy *= p.textures2.z; // the normal scale
            normal = normalize(mat3(tangent, bitangent, normal) * sampled);
        }
    }

    // The material terms, each a factor times its texture.
    vec4 base = p.baseColor;
    const int baseMap = textureIndex(p.textures.x);
    if (baseMap >= 0)
        base *= sampleTexture(baseMap, transformUv(p, 0, uvSet(p, 0) == 1 ? uv1 : uv0));
    float metallic = p.emissive.w;
    float roughness = p.params.x;
    const int mrMap = textureIndex(p.textures.y);
    if (mrMap >= 0) {
        const vec4 mr = sampleTexture(mrMap, transformUv(p, 1, uvSet(p, 1) == 1 ? uv1 : uv0));
        roughness *= mr.g;
        metallic *= mr.b;
    }
    float occlusion = 1.0;
    const int occlusionMap = textureIndex(p.textures.w);
    if (occlusionMap >= 0)
        occlusion = 1.0 + p.textures2.w * (sampleTexture(occlusionMap, transformUv(p, 3, uvSet(p, 3) == 1 ? uv1 : uv0)).r - 1.0);
    vec3 emissive = p.emissive.rgb;
    const int emissiveMap = textureIndex(p.textures2.x);
    if (emissiveMap >= 0)
        emissive *= sampleTexture(emissiveMap, transformUv(p, 4, uvSet(p, 4) == 1 ? uv1 : uv0)).rgb;
    roughness = clamp(roughness, 0.03, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);

    const bool traced = (camera.flags & kFlagTraceEffects) != 0u;

    // Glass lets the light through instead of scattering it: the transmission factor times its texture.
    float transmission = 0.0;
    if (traced) {
        transmission = p.transmission.x;
        const int transmissionMap = textureIndex(p.textures3.x);
        if (transmissionMap >= 0)
            transmission *= sampleTexture(transmissionMap, transformUv(p, 5, uvSet(p, 5) == 1 ? uv1 : uv0)).r;
        transmission = clamp(transmission, 0.0, 1.0);
    }

    const vec3 f0 = mix(vec3(0.04), base.rgb, metallic);
    const vec3 diffuse = base.rgb * (1.0 - metallic) * (1.0 - transmission);
    const float nDotV = max(dot(normal, view), 1.0e-4);
    vec3 lit = 0.18 * base.rgb * occlusion * (1.0 - transmission);
    const vec3 lightDirections[2] = vec3[2](view, kKeyLight);
    const float lightPower[2] = float[2](0.55, 0.45);
    for (int i = 0; i < 2; ++i) {
        const vec3 light = lightDirections[i];
        const float nDotL = dot(normal, light);
        if (nDotL <= 0.0)
            continue;
        // The head light is where the camera is, so only the key light can be blocked.
        if (traced && i == 1 && keyLightVisibility(point, side) == 0.0)
            continue;
        const vec3 halfway = normalize(light + view);
        const float nDotH = max(dot(normal, halfway), 0.0);
        const float vDotH = max(dot(view, halfway), 0.0);
        // The power is scaled by pi so that the diffuse term equals a plain Lambert of that weight.
        lit += (diffuse + kPi * specular(f0, roughness, nDotL, nDotV, nDotH, vDotH)) * nDotL * lightPower[i];
    }
    lit += emissive;
    lit = lit / (lit + vec3(1.0));   // a soft roll-off instead of clipping
    vec3 color = pow(lit, vec3(1.0 / 2.2)); // the swapchain is not an sRGB format
    payload.distance = gl_HitTEXT;
    payload.carry = vec3(0.0);
    const float bias = max(1.0e-4, 1.0e-4 * gl_HitTEXT);
    const vec3 incoming = gl_WorldRayDirectionEXT;

    if (transmission > 0.0) {
        // Glass: a Fresnel share of the sky is reflected, the rest goes on through the volume and takes its color. A
        // volume with a thickness bends the ray in and moves it sideways by the way it travels there; the ray
        // leaves in its old direction, as it would from a slab. Without a thickness the ray goes straight on.
        const float ior = max(p.transmission.y, 1.0);
        const float r0 = (ior - 1.0) / (ior + 1.0);
        const float fresnel = r0 * r0 + (1.0 - r0 * r0) * pow(1.0 - nDotV, 5.0);
        // Only the surface the ray enters through tints and displaces it; leaving the far wall costs nothing.
        const bool entering = dot(faceNormal, view) >= 0.0;
        float thickness = entering ? p.transmission.z : 0.0;
        const int thicknessMap = textureIndex(p.textures3.y);
        if (thicknessMap >= 0)
            thickness *= sampleTexture(thicknessMap, transformUv(p, 6, uvSet(p, 6) == 1 ? uv1 : uv0)).g;
        thickness *= length(gl_ObjectToWorldEXT[0]); // the thickness is in the mesh's units
        // Beer-Lambert: the attenuation color is what is left of white light after the attenuation distance.
        const vec3 tint = (entering ? base.rgb : vec3(1.0)) * pow(clamp(p.attenuation.rgb, vec3(1.0e-4), vec3(1.0)),
                                         vec3(thickness / max(p.transmission.w, 1.0e-6)));
        color += transmission * fresnel * environment(reflect(incoming, normal));
        payload.carry = transmission * (1.0 - fresnel) * tint;
        payload.nextDirection = incoming;
        payload.nextOrigin = point - side * bias;
        const vec3 refracted = refract(incoming, normal, 1.0 / ior);
        if (thickness > 0.0 && dot(refracted, refracted) > 0.0)
            payload.nextOrigin = point + refracted * thickness;
    } else if (traced && p.params.z > 1.5) {
        // A blended surface shows what is behind it by the rest of its alpha.
        color *= base.a;
        payload.carry = vec3(1.0 - base.a);
        payload.nextDirection = incoming;
        payload.nextOrigin = point - side * bias;
    } else if (traced) {
        // Smooth surfaces mirror what is around them, by Fresnel's share: the ray generation shader follows the ray.
        const float smoothness = 1.0 - smoothstep(0.2, 0.6, roughness);
        const vec3 fresnel = f0 + (vec3(1.0) - f0) * pow(1.0 - nDotV, 5.0);
        const vec3 mirror = reflect(-view, normal);
        if (smoothness > 0.0 && dot(mirror, side) > 0.0) {
            const float reflectivity = smoothness * dot(fresnel, vec3(1.0 / 3.0));
            color *= 1.0 - reflectivity;
            payload.carry = vec3(reflectivity);
            payload.pad1 = 1.0; // what the ray sees is the surround
            payload.nextOrigin = point + side * bias;
            payload.nextDirection = mirror;
        }
    }
    payload.color = color;
}
