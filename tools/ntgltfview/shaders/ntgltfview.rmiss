// MIT License - see LICENSE
//
// Miss: the sky.
#version 460
#extension GL_EXT_ray_tracing : require

#include "ntgltfview_camera.glsl"
#include "ntgltfview_sky.glsl"

struct HitPayload {
    vec3 color;
    float distance;
    vec3 nextOrigin;
    float pad0;
    vec3 nextDirection;
    float pad1;
    vec3 carry;
    float pad2;
};

layout(location = 0) rayPayloadInEXT HitPayload payload;

void main() {
    // A ray that came by a mirror sees the surround; the others see the background.
    payload.color = payload.pad0 > 0.5 ? environment(gl_WorldRayDirectionEXT) : sky(gl_WorldRayDirectionEXT);
    payload.distance = -1.0;
    payload.carry = vec3(0.0);
}
