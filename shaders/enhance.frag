// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC enhancement fragment shader (Vulkan/GLSL).
//
// Mirrors the deterministic software renderer: hemisphere ambient + key light +
// a point light, rim on billboards, contact-shadow via an AO texture, emissive,
// filmic tonemap, contrast/saturation/grade, fog and vignette. The G-buffer
// (albedo/normal/height/material/emissive/AO/depth) is produced by the scene
// reconstruction stage and uploaded as textures.
#version 450

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uAlbedo;    // rgb sRGB
layout(set = 0, binding = 1) uniform sampler2D uNormal;    // rgb encoded normal
layout(set = 0, binding = 2) uniform sampler2D uAux;       // r=height g=emissive b=ao a=depth

layout(set = 0, binding = 3) uniform Params {
    vec4 sunDir;        // xyz travel dir, w intensity
    vec4 sunColor;      // rgb, a unused
    vec4 ambientSky;    // rgb, a intensity
    vec4 ambientGround; // rgb
    vec4 fogColor;      // rgb, a density
    vec4 grade;         // rgb multiply, a exposure
    vec4 lightPos;      // xy screen (uv 0..1), z height, w radius
    vec4 lightColor;    // rgb, a intensity
    vec4 post;          // x contrast, y saturation, z vignette, w bloomIntensity
    vec4 tune;          // x rimStrength, y contactShadow, z highlightProtect, w reserved
} P;

vec3 srgbToLinear(vec3 c) { return pow(c, vec3(2.2)); }
vec3 linearToSrgb(vec3 c) { return pow(clamp(c, 0.0, 1.0), vec3(1.0 / 2.2)); }
float luma(vec3 c) { return dot(c, vec3(0.2126, 0.7152, 0.0722)); }

vec3 aces(vec3 x) {
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main() {
    vec3 albedo = srgbToLinear(texture(uAlbedo, vUV).rgb);
    vec3 N = normalize(texture(uNormal, vUV).rgb * 2.0 - 1.0);
    vec4 aux = texture(uAux, vUV);
    float emissive = aux.g;
    float ao = aux.b;
    float depth = aux.a;

    vec3 toSun = normalize(-P.sunDir.xyz);
    float up = clamp(0.5 - 0.5 * N.y, 0.0, 1.0);
    vec3 ambient = mix(P.ambientGround.rgb, P.ambientSky.rgb, up) * P.ambientSky.a * ao;
    float ndl = max(dot(N, toSun), 0.0);
    vec3 sun = P.sunColor.rgb * (P.sunDir.w * ndl * ao);

    // Point light.
    vec2 d = (P.lightPos.xy - vUV) * vec2(256.0, 192.0);
    float dist = length(vec3(d, P.lightPos.z * 40.0));
    float atten = clamp(1.0 - dist / max(P.lightPos.w, 1.0), 0.0, 1.0);
    atten *= atten;
    vec3 Ldir = normalize(vec3(d, P.lightPos.z * 40.0));
    vec3 point = P.lightColor.rgb * (P.lightColor.a * atten * max(dot(N, Ldir), 0.0));

    vec3 lightTerm = ambient + sun + point;
    float rim = pow(clamp(1.0 - N.z, 0.0, 1.0), 2.0) * P.tune.x;
    lightTerm += P.sunColor.rgb * rim;

    vec3 color = albedo * lightTerm + albedo * (emissive * 1.5);

    // Exposure + tonemap.
    color = aces(color * P.grade.a);
    // Contrast.
    color = clamp((color - 0.5) * P.post.x + 0.5, 0.0, 1.0);
    // Saturation.
    float l = luma(color);
    color = mix(vec3(l), color, P.post.y);
    // Grade.
    color *= P.grade.rgb;
    // Fog.
    float fogF = clamp((1.0 - depth) * P.fogColor.a * 4.0, 0.0, 0.85);
    color = mix(color, P.fogColor.rgb, fogF);
    // Vignette.
    vec2 v = (vUV - 0.5) * 2.0;
    color *= 1.0 - P.post.z * clamp(dot(v, v), 0.0, 1.0);

    outColor = vec4(linearToSrgb(color), 1.0);
}
