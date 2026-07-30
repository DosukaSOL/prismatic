// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — foundation types.
#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>

namespace prismatic {

// ---- Scalar helpers -------------------------------------------------------
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstepf(float e0, float e1, float x) {
    if (e0 == e1) return x < e0 ? 0.0f : 1.0f;
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ---- Vectors --------------------------------------------------------------
struct Vec2 { float x = 0, y = 0; };
struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
};
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float length(const Vec3& a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(const Vec3& a) {
    float l = length(a);
    return l > 1e-8f ? a * (1.0f / l) : Vec3{0, 0, 1};
}
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// ---- Color (linear-ish sRGB stored as 8-bit RGBA) -------------------------
struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
    Color() = default;
    Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
    static Color fromFloat(float rf, float gf, float bf, float af = 1.0f) {
        return Color{
            (uint8_t)std::lround(clampf(rf, 0, 1) * 255.0f),
            (uint8_t)std::lround(clampf(gf, 0, 1) * 255.0f),
            (uint8_t)std::lround(clampf(bf, 0, 1) * 255.0f),
            (uint8_t)std::lround(clampf(af, 0, 1) * 255.0f)};
    }
    bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
};

// ---- Image ----------------------------------------------------------------
struct Image {
    int width = 0, height = 0;
    std::vector<Color> pixels;
    Image() = default;
    Image(int w, int h, Color fill = Color{0, 0, 0, 255})
        : width(w), height(h), pixels((size_t)w * h, fill) {}
    Color& at(int x, int y) { return pixels[(size_t)y * width + x]; }
    const Color& at(int x, int y) const { return pixels[(size_t)y * width + x]; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
    void set(int x, int y, Color c) { if (inBounds(x, y)) at(x, y) = c; }
};

// A single-channel float buffer (depth, masks, height maps).
struct FloatBuffer {
    int width = 0, height = 0;
    std::vector<float> data;
    FloatBuffer() = default;
    FloatBuffer(int w, int h, float fill = 0.0f)
        : width(w), height(h), data((size_t)w * h, fill) {}
    float& at(int x, int y) { return data[(size_t)y * width + x]; }
    float at(int x, int y) const { return data[(size_t)y * width + x]; }
    bool inBounds(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
};

}  // namespace prismatic
