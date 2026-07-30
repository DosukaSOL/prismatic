// SPDX-License-Identifier: GPL-3.0-or-later
// Bright-pass + separable-friendly blur tap for bloom (Vulkan/GLSL).
// Runs as a fullscreen pass over the lit HDR target.
#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uHDR;
layout(set = 0, binding = 1) uniform Bloom {
    vec4 cfg;   // x threshold, y intensity, z texelX, w texelY
    vec4 dir;   // xy blur direction (0 = bright pass only)
} B;

float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

void main() {
    if (B.dir.x == 0.0 && B.dir.y == 0.0) {
        vec3 c = texture(uHDR, vUV).rgb;
        float l = luma(c);
        vec3 b = l > B.cfg.x ? c * ((l - B.cfg.x) / (l + 1e-4)) : vec3(0.0);
        outColor = vec4(b, 1.0);
        return;
    }
    // 9-tap Gaussian along dir.
    vec2 step = B.dir.xy * vec2(B.cfg.z, B.cfg.w);
    float w[5] = float[](0.227, 0.194, 0.121, 0.054, 0.016);
    vec3 sum = texture(uHDR, vUV).rgb * w[0];
    for (int i = 1; i < 5; ++i) {
        sum += texture(uHDR, vUV + step * float(i)).rgb * w[i];
        sum += texture(uHDR, vUV - step * float(i)).rgb * w[i];
    }
    outColor = vec4(sum * B.cfg.y, 1.0);
}
