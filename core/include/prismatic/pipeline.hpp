// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — high-level pipeline.
// Owns the material cache + active profile/preset/environment and turns an
// adapter screen into a rendered result. Used by the headless runner and app.
#pragma once
#include "prismatic/adapter.hpp"
#include "prismatic/renderer_software.hpp"
#include "prismatic/profile.hpp"

namespace prismatic {

class PrismaticPipeline {
public:
    PrismaticPipeline() { preset_ = getPreset("HD-2.5D BALANCED"); }

    void setProfile(const Profile& p) {
        profile_ = p;
        preset_ = p.hasPresetOverride ? p.presetOverride : getPreset(p.basePreset);
        env_ = p.environment;
        cache_.clear();
        applyProfileToCache(profile_, cache_);
    }
    const Profile& profile() const { return profile_; }

    void setPreset(const Preset& p) { preset_ = p; }
    void setPresetByName(const std::string& name) { preset_ = getPreset(name); }
    const Preset& preset() const { return preset_; }

    void setEnvironment(const EnvironmentState& e) { env_ = e; }
    const EnvironmentState& environment() const { return env_; }

    void clearLights() { lights_.lights.clear(); }
    void addLight(const Light& l) { lights_.lights.push_back(l); }

    MaterialCache& cache() { return cache_; }

    RenderResult renderScreen(EmulatorAdapter& adapter, int screen, int upscale = 0) {
        RenderRequest req;
        req.preset = preset_;
        req.environment = env_;
        req.lights = lights_;
        req.upscale = upscale;
        // Framebuffer-only backends (real cores that expose just pixels) take the
        // screen-space path; structured backends reconstruct from tiles/sprites.
        if (adapter.info().compatibility <= CompatibilityLevel::Level1_Framebuffer)
            return renderFramebuffer(adapter.framebuffer(screen), cache_, req);
        StructuredFrame frame = adapter.structuredFrame(screen);
        return renderStructured(frame, cache_, req);
    }

private:
    Profile profile_ = defaultProfile();
    Preset preset_;
    EnvironmentState env_;
    LightingInput lights_;
    MaterialCache cache_;
};

}  // namespace prismatic
