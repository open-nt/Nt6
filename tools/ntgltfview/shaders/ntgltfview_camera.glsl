// MIT License - see LICENSE
//
// The push constants, shared by the ray generation and closest hit shaders. The layout matches CameraConstants in
// main.cpp.
// The bits of Camera.flags: the shadows and reflections of the ray traced effects live in ntgltfview_common.glsl.
const uint kFlagEnvironment = 4u; // an environment image is bound

layout(push_constant) uniform Camera {
    vec3 origin;
    float tanHalfFovY;
    vec3 forward;
    float aspect;
    vec3 right;
    uint flags;
    vec3 up;
    float depthNear;
    vec3 prevOrigin;
    float prevTanHalfFovY;
    vec3 prevForward;
    uint swapRedBlue; // 1 when the target is a BGRA image
    vec3 prevRight;
    float jitterX; // the sub-pixel offset of the rays, in pixels
    vec3 prevUp;
    float jitterY;
} camera;
