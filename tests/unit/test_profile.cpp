// SPDX-License-Identifier: GPL-3.0-or-later
#include "test_util.hpp"
#include "prismatic/profile.hpp"
#include "prismatic/presets.hpp"
#include "prismatic/environment.hpp"
#include "prismatic/camera.hpp"
using namespace prismatic;

static void test_presets() {
    auto names = presetNames();
    CHECK_EQ(names.size(), (size_t)10);
    // Round-trip every preset through JSON.
    for (auto& p : allPresets()) {
        Preset q = presetFromJson(presetToJson(p));
        CHECK_EQ(q.name, p.name);
        CHECK_NEAR(q.bloomIntensity, p.bloomIntensity, 1e-4);
        CHECK_NEAR(q.heightScale, p.heightScale, 1e-4);
    }
    CHECK_EQ(getPreset("HD-2.5D BALANCED").name, std::string("HD-2.5D BALANCED"));
    CHECK_EQ(getPreset("does-not-exist").name, std::string("CUSTOM"));
}

static void test_profile_roundtrip() {
    Profile p = defaultProfile();
    p.system = System::NDS;
    p.gameCode = "IPGE";
    p.romSha256 = "deadbeef";
    p.title = "Secret";
    p.environment.timeOfDay = 20.0f;
    ProfileRule r; r.scope = RuleScope::Tile; r.key = "abc123"; r.hasMaterial = true; r.material = Material::Water;
    p.rules.push_back(r);

    std::string full = serializeProfile(p, false);
    Profile q; std::string err;
    CHECK(parseProfile(full, q, err));
    CHECK_EQ(q.gameCode, std::string("IPGE"));
    CHECK_EQ(q.romSha256, std::string("deadbeef"));
    CHECK_EQ(q.rules.size(), (size_t)1);
    CHECK_EQ(q.rules[0].material, Material::Water);

    // Copyright-safe export strips ROM-derived identity.
    std::string safe = serializeProfile(p, true);
    Profile s; CHECK(parseProfile(safe, s, err));
    CHECK_EQ(s.romSha256, std::string(""));
    CHECK_EQ(s.title, std::string(""));
}

static void test_precedence() {
    Profile p = defaultProfile();
    ProfileRule def; def.scope = RuleScope::Default; def.hasHeight = true; def.heightScale = 0.1f;
    ProfileRule game; game.scope = RuleScope::Game; game.hasHeight = true; game.heightScale = 0.2f;
    ProfileRule tile; tile.scope = RuleScope::Tile; tile.key = "HASH"; tile.hasHeight = true; tile.heightScale = 0.9f;
    p.rules = {def, game, tile};
    auto rc = resolveTile(p, "HASH", "ts", "map");
    CHECK(rc.hasHeight);
    CHECK_NEAR(rc.heightScale, 0.9f, 1e-4);  // tile beats game/default
    auto rc2 = resolveTile(p, "OTHER", "ts", "map");
    CHECK_NEAR(rc2.heightScale, 0.2f, 1e-4);  // falls back to game
}

static void test_validation() {
    std::string bad = R"({"version":1,"environment":{"timeOfDay":99}})";
    Profile q; std::string err;
    CHECK(!parseProfile(bad, q, err));
    CHECK(!err.empty());
    // Malformed JSON must not crash.
    CHECK(!parseProfile("{not json", q, err));
}

static void test_environment() {
    EnvironmentState noon; noon.timeOfDay = 12.0f;
    EnvironmentState mid; mid.timeOfDay = 0.0f;
    EnvLighting d = computeEnvLighting(noon);
    EnvLighting n = computeEnvLighting(mid);
    CHECK(d.sunIntensity > n.sunIntensity);  // brighter at noon
    CHECK(n.ambientSky.z >= n.ambientSky.x);  // night is cool/bluish
}

static void test_camera_safe() {
    FloatBuffer h(16, 16, 1.0f);  // tall everywhere
    CameraConfig c; c.gameplaySafe = true; c.parallax = 5.0f; c.safeMarginY = 7.0f;
    auto off = computeParallaxOffsetY(h, c);
    CHECK(maxAbsOffset(off) <= c.safeMarginY + 1e-3f);
    c.gameplaySafe = false;
    auto off2 = computeParallaxOffsetY(h, c);
    CHECK(maxAbsOffset(off2) > c.safeMarginY);  // unclamped exceeds margin
}

int main() {
    RUN(test_presets);
    RUN(test_profile_roundtrip);
    RUN(test_precedence);
    RUN(test_validation);
    RUN(test_environment);
    RUN(test_camera_safe);
    return REPORT();
}
