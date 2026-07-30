// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/environment.hpp"
#include <array>
#include <cmath>

namespace prismatic {

const char* weatherName(Weather w) {
    switch (w) {
        case Weather::Rain: return "Rain";
        case Weather::Fog: return "Fog";
        case Weather::Snow: return "Snow";
        default: return "Clear";
    }
}
const char* locationTagName(LocationTag t) {
    switch (t) {
        case LocationTag::Interior: return "Interior";
        case LocationTag::Cave: return "Cave";
        case LocationTag::Water: return "Water";
        default: return "Overworld";
    }
}

namespace {
struct Key {
    float hour;
    Vec3 sky, ground, sun;
    float sunI, fog, exposure;
    Vec3 fogCol, grade;
};

// Authored day cycle key frames.
const std::array<Key, 7> kKeys = {{
    // hour   sky                 ground             sun                 sunI  fog    expo  fogCol              grade
    {0.0f,  {0.05f,0.07f,0.16f}, {0.03f,0.04f,0.08f},{0.25f,0.30f,0.55f},0.10f,0.10f,1.25f,{0.05f,0.07f,0.16f},{0.75f,0.82f,1.15f}},
    {5.5f,  {0.20f,0.18f,0.30f}, {0.10f,0.08f,0.10f},{0.85f,0.45f,0.35f},0.35f,0.18f,1.15f,{0.30f,0.25f,0.30f},{1.05f,0.92f,0.85f}},
    {7.5f,  {0.55f,0.55f,0.70f}, {0.30f,0.26f,0.22f},{1.00f,0.80f,0.60f},0.80f,0.10f,1.05f,{0.70f,0.68f,0.72f},{1.08f,1.00f,0.92f}},
    {12.0f, {0.60f,0.70f,0.90f}, {0.38f,0.36f,0.32f},{1.00f,0.98f,0.92f},1.00f,0.04f,1.00f,{0.75f,0.80f,0.90f},{1.00f,1.00f,1.00f}},
    {17.5f, {0.55f,0.48f,0.55f}, {0.32f,0.26f,0.22f},{1.00f,0.72f,0.45f},0.75f,0.12f,1.05f,{0.65f,0.55f,0.55f},{1.10f,0.98f,0.85f}},
    {19.5f, {0.25f,0.20f,0.32f}, {0.14f,0.10f,0.12f},{0.85f,0.42f,0.35f},0.35f,0.20f,1.15f,{0.30f,0.24f,0.32f},{1.05f,0.90f,0.90f}},
    {24.0f, {0.05f,0.07f,0.16f}, {0.03f,0.04f,0.08f},{0.25f,0.30f,0.55f},0.10f,0.10f,1.25f,{0.05f,0.07f,0.16f},{0.75f,0.82f,1.15f}},
}};

Vec3 mix3(const Vec3& a, const Vec3& b, float t) {
    return {lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t)};
}
}  // namespace

EnvLighting computeEnvLighting(const EnvironmentState& s) {
    float t = s.timeOfDay;
    while (t < 0) t += 24.0f;
    while (t >= 24.0f) t -= 24.0f;

    // Interpolate key frames.
    Key k = kKeys.front();
    for (size_t i = 0; i + 1 < kKeys.size(); ++i) {
        if (t >= kKeys[i].hour && t <= kKeys[i + 1].hour) {
            float f = (t - kKeys[i].hour) / (kKeys[i + 1].hour - kKeys[i].hour);
            const Key& a = kKeys[i];
            const Key& b = kKeys[i + 1];
            k.sky = mix3(a.sky, b.sky, f);
            k.ground = mix3(a.ground, b.ground, f);
            k.sun = mix3(a.sun, b.sun, f);
            k.sunI = lerpf(a.sunI, b.sunI, f);
            k.fog = lerpf(a.fog, b.fog, f);
            k.exposure = lerpf(a.exposure, b.exposure, f);
            k.fogCol = mix3(a.fogCol, b.fogCol, f);
            k.grade = mix3(a.grade, b.grade, f);
            break;
        }
    }

    EnvLighting e;
    e.ambientSky = k.sky;
    e.ambientGround = k.ground;
    e.sunColor = k.sun;
    e.sunIntensity = k.sunI;
    e.fogColor = k.fogCol;
    e.fogDensity = k.fog;
    e.exposureBias = k.exposure;
    e.gradeMul = k.grade;
    e.ambientIntensity = 1.0f;
    e.bloomBias = 1.0f;

    // Sun direction from time: morning left -> noon top -> evening right.
    float ax = clampf((t - 12.0f) / 6.0f, -1.0f, 1.0f);
    float elev = std::sin(clampf((t - 6.0f) / 12.0f, 0.0f, 1.0f) * 3.14159265f);
    e.sunDir = normalize(Vec3{ax * 0.7f, 0.35f + 0.4f * elev, -0.6f - 0.3f * elev});

    // Weather modifiers.
    switch (s.weather) {
        case Weather::Rain:
            e.sunIntensity *= 0.5f; e.fogDensity = std::max(e.fogDensity, 0.28f);
            e.fogColor = mix3(e.fogColor, {0.45f, 0.50f, 0.58f}, 0.5f);
            e.gradeMul = mix3(e.gradeMul, {0.85f, 0.92f, 1.10f}, 0.6f);
            e.exposureBias *= 0.95f; break;
        case Weather::Fog:
            e.fogDensity = std::max(e.fogDensity, 0.45f);
            e.sunIntensity *= 0.7f; break;
        case Weather::Snow:
            e.fogDensity = std::max(e.fogDensity, 0.2f);
            e.ambientSky = mix3(e.ambientSky, {0.85f, 0.88f, 0.95f}, 0.4f);
            e.gradeMul = mix3(e.gradeMul, {1.02f, 1.03f, 1.08f}, 0.5f);
            e.exposureBias *= 1.05f; break;
        default: break;
    }

    // Location tag modifiers.
    switch (s.tag) {
        case LocationTag::Interior:
            e.sunIntensity *= 0.15f; e.fogDensity = 0.0f;
            e.ambientSky = {0.45f, 0.42f, 0.38f}; e.ambientGround = {0.25f, 0.22f, 0.2f};
            e.gradeMul = mix3(e.gradeMul, {1.1f, 1.02f, 0.85f}, 0.7f); break;  // warm indoor
        case LocationTag::Cave:
            e.sunIntensity *= 0.05f; e.fogDensity = std::max(e.fogDensity, 0.15f);
            e.ambientSky = {0.14f, 0.16f, 0.22f}; e.ambientGround = {0.06f, 0.07f, 0.1f}; break;
        case LocationTag::Water:
            e.gradeMul = mix3(e.gradeMul, {0.8f, 0.95f, 1.2f}, 0.5f);
            e.fogColor = mix3(e.fogColor, {0.3f, 0.5f, 0.7f}, 0.5f); break;
        default: break;
    }
    return e;
}

}  // namespace prismatic
