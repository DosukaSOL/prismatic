// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/presets.hpp"

namespace prismatic {

std::vector<Preset> allPresets() {
    std::vector<Preset> v;

    Preset p;  // ---- ORIGINAL PLUS: barely-there depth, crisp -----------------
    p = Preset{};
    p.name = "ORIGINAL PLUS";
    p.normalStrength = 0.4f; p.heightScale = 0.4f; p.contactShadowStrength = 0.2f;
    p.rimStrength = 0.15f; p.bloomIntensity = 0.06f; p.vignette = 0.05f;
    p.parallax = 0.15f; p.pitch = 0.1f; p.pixelSharpness = 0.9f; p.saturation = 1.03f;
    v.push_back(p);

    p = Preset{};  // ---- FAITHFUL HD-2D -------------------------------------
    p.name = "FAITHFUL HD-2D";
    p.normalStrength = 0.8f; p.heightScale = 0.8f; p.contactShadowStrength = 0.45f;
    p.rimStrength = 0.35f; p.bloomThreshold = 0.78f; p.bloomIntensity = 0.2f;
    p.vignette = 0.12f; p.parallax = 0.4f; p.pitch = 0.25f; p.saturation = 1.08f;
    p.contrast = 1.05f;
    v.push_back(p);

    p = Preset{};  // ---- HD-2.5D BALANCED (flagship) ----------------------
    p.name = "HD-2.5D BALANCED";
    p.normalStrength = 1.0f; p.heightScale = 1.0f; p.contactShadowStrength = 0.6f;
    p.rimStrength = 0.5f; p.bloomThreshold = 0.72f; p.bloomIntensity = 0.32f;
    p.vignette = 0.16f; p.parallax = 0.7f; p.pitch = 0.35f; p.saturation = 1.12f;
    p.contrast = 1.08f;
    v.push_back(p);

    p = Preset{};  // ---- CINEMATIC HD-2D ---------------------------------
    p.name = "CINEMATIC HD-2D";
    p.normalStrength = 1.1f; p.heightScale = 1.1f; p.contactShadowStrength = 0.7f;
    p.rimStrength = 0.6f; p.bloomThreshold = 0.6f; p.bloomIntensity = 0.5f;
    p.vignette = 0.3f; p.parallax = 0.8f; p.pitch = 0.4f; p.saturation = 1.1f;
    p.contrast = 1.15f; p.grade = {1.06f, 1.0f, 0.92f}; p.exposure = 1.05f;
    v.push_back(p);

    p = Preset{};  // ---- DRAMATIC ----------------------------------------
    p.name = "DRAMATIC";
    p.normalStrength = 1.3f; p.heightScale = 1.2f; p.contactShadowStrength = 0.85f;
    p.rimStrength = 0.8f; p.bloomThreshold = 0.65f; p.bloomIntensity = 0.45f;
    p.vignette = 0.35f; p.parallax = 0.9f; p.pitch = 0.45f; p.saturation = 1.25f;
    p.contrast = 1.25f; p.highlightProtection = 0.5f;
    v.push_back(p);

    p = Preset{};  // ---- DREAMLIKE ---------------------------------------
    p.name = "DREAMLIKE";
    p.normalStrength = 0.7f; p.heightScale = 0.8f; p.contactShadowStrength = 0.35f;
    p.rimStrength = 0.5f; p.bloomThreshold = 0.5f; p.bloomIntensity = 0.75f;
    p.vignette = 0.2f; p.parallax = 0.5f; p.pitch = 0.3f; p.saturation = 0.95f;
    p.contrast = 0.92f; p.grade = {1.05f, 1.0f, 1.08f}; p.pixelSharpness = 0.2f;
    v.push_back(p);

    p = Preset{};  // ---- NIGHT GLOW --------------------------------------
    p.name = "NIGHT GLOW";
    p.normalStrength = 1.0f; p.heightScale = 1.0f; p.contactShadowStrength = 0.6f;
    p.rimStrength = 0.55f; p.bloomThreshold = 0.45f; p.bloomIntensity = 0.85f;
    p.vignette = 0.28f; p.parallax = 0.7f; p.pitch = 0.35f; p.saturation = 1.15f;
    p.contrast = 1.1f; p.grade = {0.85f, 0.92f, 1.2f}; p.exposure = 1.15f;
    v.push_back(p);

    p = Preset{};  // ---- PIXEL PERFECT: crisp, minimal post ---------------
    p.name = "PIXEL PERFECT";
    p.normalStrength = 0.0f; p.heightScale = 0.0f; p.contactShadowStrength = 0.0f;
    p.rimStrength = 0.0f; p.bloomIntensity = 0.0f; p.vignette = 0.0f;
    p.parallax = 0.0f; p.pitch = 0.0f; p.pixelSharpness = 1.0f; p.saturation = 1.0f;
    p.contrast = 1.0f; p.upscale = 4;
    v.push_back(p);

    p = Preset{};  // ---- PERFORMANCE: cheapest enhancement ----------------
    p.name = "PERFORMANCE";
    p.normalStrength = 0.5f; p.heightScale = 0.5f; p.contactShadowStrength = 0.25f;
    p.rimStrength = 0.2f; p.bloomIntensity = 0.0f; p.vignette = 0.08f;
    p.parallax = 0.25f; p.pitch = 0.15f; p.pixelSharpness = 0.8f; p.upscale = 2;
    v.push_back(p);

    p = Preset{};  // ---- CUSTOM: editable baseline ------------------------
    p.name = "CUSTOM";
    v.push_back(p);

    return v;
}

std::vector<std::string> presetNames() {
    std::vector<std::string> names;
    for (auto& p : allPresets()) names.push_back(p.name);
    return names;
}

Preset getPreset(const std::string& name) {
    for (auto& p : allPresets())
        if (p.name == name) return p;
    Preset custom;
    return custom;
}

JsonValue presetToJson(const Preset& p) {
    JsonValue j = JsonValue::makeObject();
    j.set("name", p.name);
    j.set("ambientStrength", p.ambientStrength);
    j.set("sunStrength", p.sunStrength);
    j.set("normalStrength", p.normalStrength);
    j.set("heightScale", p.heightScale);
    j.set("contactShadowStrength", p.contactShadowStrength);
    j.set("rimStrength", p.rimStrength);
    j.set("highlightProtection", p.highlightProtection);
    j.set("exposure", p.exposure);
    j.set("contrast", p.contrast);
    j.set("saturation", p.saturation);
    j.set("bloomThreshold", p.bloomThreshold);
    j.set("bloomIntensity", p.bloomIntensity);
    j.set("vignette", p.vignette);
    j.set("fogScale", p.fogScale);
    JsonValue g = JsonValue::makeArray();
    g.push(p.grade.x); g.push(p.grade.y); g.push(p.grade.z);
    j.set("grade", g);
    j.set("parallax", p.parallax);
    j.set("pitch", p.pitch);
    j.set("gameplaySafe", p.gameplaySafe);
    j.set("pixelSharpness", p.pixelSharpness);
    j.set("scanline", p.scanline);
    j.set("upscale", p.upscale);
    return j;
}

Preset presetFromJson(const JsonValue& j) {
    Preset p;
    p.name = j.get("name").asString(p.name);
    p.ambientStrength = (float)j.get("ambientStrength").asNumber(p.ambientStrength);
    p.sunStrength = (float)j.get("sunStrength").asNumber(p.sunStrength);
    p.normalStrength = (float)j.get("normalStrength").asNumber(p.normalStrength);
    p.heightScale = (float)j.get("heightScale").asNumber(p.heightScale);
    p.contactShadowStrength = (float)j.get("contactShadowStrength").asNumber(p.contactShadowStrength);
    p.rimStrength = (float)j.get("rimStrength").asNumber(p.rimStrength);
    p.highlightProtection = (float)j.get("highlightProtection").asNumber(p.highlightProtection);
    p.exposure = (float)j.get("exposure").asNumber(p.exposure);
    p.contrast = (float)j.get("contrast").asNumber(p.contrast);
    p.saturation = (float)j.get("saturation").asNumber(p.saturation);
    p.bloomThreshold = (float)j.get("bloomThreshold").asNumber(p.bloomThreshold);
    p.bloomIntensity = (float)j.get("bloomIntensity").asNumber(p.bloomIntensity);
    p.vignette = (float)j.get("vignette").asNumber(p.vignette);
    p.fogScale = (float)j.get("fogScale").asNumber(p.fogScale);
    const JsonValue& g = j.get("grade");
    if (g.isArray() && g.arr->size() == 3) {
        p.grade = {(float)(*g.arr)[0].asNumber(1), (float)(*g.arr)[1].asNumber(1), (float)(*g.arr)[2].asNumber(1)};
    }
    p.parallax = (float)j.get("parallax").asNumber(p.parallax);
    p.pitch = (float)j.get("pitch").asNumber(p.pitch);
    p.gameplaySafe = j.get("gameplaySafe").asBool(p.gameplaySafe);
    p.pixelSharpness = (float)j.get("pixelSharpness").asNumber(p.pixelSharpness);
    p.scanline = (float)j.get("scanline").asNumber(p.scanline);
    p.upscale = j.get("upscale").asInt(p.upscale);
    return p;
}

}  // namespace prismatic
