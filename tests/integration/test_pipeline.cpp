// SPDX-License-Identifier: GPL-3.0-or-later
#include "test_util.hpp"
#include "prismatic/pipeline.hpp"
#include "synthetic_backend.hpp"
using namespace prismatic;

static uint64_t hashImage(const Image& img) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](uint8_t b) { h ^= b; h *= 1099511628211ull; };
    for (const Color& c : img.pixels) { mix(c.r); mix(c.g); mix(c.b); mix(c.a); }
    return h;
}

static void advance(EmulatorAdapter& a, int n) {
    a.reset();
    InputState in;
    for (int i = 0; i < n; ++i) { a.setInput(in); a.advanceFrame(); }
}

static void test_backend_structure() {
    auto b = makeSyntheticBackend();
    CHECK_EQ(b->screenCount(), 2);
    advance(*b, 50);
    StructuredFrame top = b->structuredFrame(0);
    CHECK_EQ(top.screenWidth, 256);
    CHECK_EQ(top.screenHeight, 192);
    CHECK(top.backgrounds.size() >= 2);       // ground + overhead
    CHECK(top.sprites.size() >= 2);           // player + follower
    // framebuffer equals native composite of the structured frame.
    CHECK_EQ(hashImage(b->framebuffer(0)), hashImage(compositeNative(top)));
}

static void test_backend_determinism() {
    auto a = makeSyntheticBackend();
    auto b = makeSyntheticBackend();
    advance(*a, 137);
    advance(*b, 137);
    CHECK_EQ(hashImage(a->framebuffer(0)), hashImage(b->framebuffer(0)));
    CHECK_EQ(hashImage(a->framebuffer(1)), hashImage(b->framebuffer(1)));
}

static bool nonTrivial(const Image& img, Color backdrop) {
    int diff = 0;
    for (const Color& c : img.pixels) if (!(c == backdrop)) if (++diff > 500) return true;
    return false;
}

static void test_pipeline_render() {
    auto b = makeSyntheticBackend();
    advance(*b, 100);
    PrismaticPipeline pipe;
    pipe.setPresetByName("HD-2.5D BALANCED");
    RenderResult r = pipe.renderScreen(*b, 0);
    // Enhanced is upscaled.
    CHECK_EQ(r.enhanced.width, r.width * pipe.preset().upscale);
    CHECK_EQ(r.enhanced.height, r.height * pipe.preset().upscale);
    // Output is non-trivial (not a flat backdrop).
    CHECK(nonTrivial(r.enhancedNative, Color{20, 30, 40}));
    // Debug views produced at native res.
    CHECK_EQ(r.normalView.width, r.width);
    CHECK_EQ(r.objectIdView.width, r.width);
}

static void test_render_determinism() {
    auto b = makeSyntheticBackend();
    advance(*b, 100);
    PrismaticPipeline p1, p2;
    p1.setPresetByName("CINEMATIC HD-2D");
    p2.setPresetByName("CINEMATIC HD-2D");
    RenderResult a = p1.renderScreen(*b, 0);
    RenderResult c = p2.renderScreen(*b, 0);
    CHECK_EQ(hashImage(a.enhancedNative), hashImage(c.enhancedNative));
    CHECK_EQ(hashImage(a.depthView), hashImage(c.depthView));
}

int main() {
    RUN(test_backend_structure);
    RUN(test_backend_determinism);
    RUN(test_pipeline_render);
    RUN(test_render_determinism);
    return REPORT();
}
