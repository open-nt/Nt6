// MIT License - see LICENSE
//
// The sky that rays see when they miss: lighter towards the horizon. Smooth surfaces mirror a richer surround than the
// background shows, a studio with a bright floor-to-sky gradient, a darker ground and a soft box where the key light
// is, so that metal has something to reflect.
// The panorama, when there is one (--environment), seen in direction `direction`, tone mapped like the surfaces.
layout(set = 0, binding = 6) uniform sampler2D environmentMap;

vec3 panorama(vec3 direction) {
    const vec2 uv = vec2(atan(direction.x, direction.z) / 6.2831853 + 0.5,
                         acos(clamp(direction.y, -1.0, 1.0)) / 3.1415927);
    const vec3 radiance = textureLod(environmentMap, uv, 0.0).rgb;
    return pow(radiance / (radiance + vec3(1.0)), vec3(1.0 / 2.2));
}

vec3 sky(vec3 direction) {
    if ((camera.flags & kFlagEnvironment) != 0u)
        return panorama(direction);
    const float t = clamp(direction.y * 0.5 + 0.5, 0.0, 1.0);
    return mix(vec3(0.32, 0.34, 0.38), vec3(0.10, 0.14, 0.24), t);
}

vec3 environment(vec3 direction) {
    if ((camera.flags & kFlagEnvironment) != 0u)
        return panorama(direction);
    const float up = direction.y;
    vec3 color = up >= 0.0 ? mix(vec3(0.62, 0.66, 0.72), vec3(0.20, 0.34, 0.62), pow(up, 0.6))
                           : mix(vec3(0.42, 0.36, 0.30), vec3(0.07, 0.06, 0.06), pow(-up, 0.45));
    // A band of bright windows around the horizon, and the soft box of the key light.
    const float azimuth = atan(direction.x, direction.z);
    color += vec3(0.9) * smoothstep(0.55, 0.9, sin(azimuth * 3.0)) * smoothstep(0.35, 0.0, abs(up - 0.18));
    color += vec3(1.4, 1.35, 1.2) * pow(max(dot(direction, normalize(vec3(0.45, 1.0, 0.35))), 0.0), 24.0);
    return color;
}
