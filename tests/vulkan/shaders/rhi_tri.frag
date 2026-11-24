#version 440
layout(location = 0) in vec3 v_color;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform Block { vec4 tint; } ub;
layout(binding = 1) uniform sampler2D tex;
void main() {
    fragColor = vec4(v_color, 1.0) * ub.tint * texture(tex, vec2(0.5));
}
