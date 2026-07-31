// SPDX-License-Identifier: GPL-3.0-or-later
#include "test_util.hpp"
#include "prismatic/mod_registry.hpp"
#include <string>
using namespace prismatic;

static ModManifest sample(const std::string& id, ModKind kind = ModKind::Graphics) {
    ModManifest m;
    m.id = id;
    m.name = id + " name";
    m.kind = kind;
    m.author = "tester";
    m.description = "d";
    m.entry = "hook";
    return m;
}

static void test_kind_strings() {
    CHECK_EQ(std::string(modKindName(ModKind::Graphics)), std::string("graphics"));
    CHECK_EQ(std::string(modKindName(ModKind::Scene)), std::string("scene"));
    CHECK_EQ(std::string(modKindName(ModKind::Unknown)), std::string("unknown"));
    CHECK(modKindFromString("palette") == ModKind::Palette);
    CHECK(modKindFromString("nope") == ModKind::Unknown);
}

static void test_roundtrip() {
    ModManifest m = sample("prismatic.voxel-diorama");
    m.modVersion = "0.4.0";
    m.provides = {"renderer.overworld.2p5d", "scene.pvx4"};
    m.dependencies = {"prismatic.base"};
    m.builtin = true;

    std::string json = serializeModManifest(m);
    ModManifest q; std::string err;
    CHECK(parseModManifest(json, q, err));
    CHECK_EQ(q.id, m.id);
    CHECK(q.kind == ModKind::Graphics);
    CHECK_EQ(q.modVersion, std::string("0.4.0"));
    CHECK_EQ(q.provides.size(), (size_t)2);
    CHECK_EQ(q.provides[1], std::string("scene.pvx4"));
    CHECK_EQ(q.dependencies.size(), (size_t)1);
    CHECK_EQ(q.builtin, true);
}

static void test_parse_errors() {
    ModManifest m; std::string err;
    CHECK(!parseModManifest("{ this is not json", m, err));
    CHECK(!parseModManifest("42", m, err));  // root not an object
}

static void test_validate_manifest() {
    ModManifest bad;              // empty id/name, unknown kind
    auto issues = validateModManifest(bad);
    CHECK(issues.size() >= 3);

    ModManifest self = sample("a");
    self.dependencies = {"a"};    // self dependency
    CHECK(!validateModManifest(self).empty());

    CHECK(validateModManifest(sample("ok")).empty());
}

static void test_registry_add_find() {
    ModRegistry reg;
    CHECK(reg.add(sample("a")));
    CHECK(reg.add(sample("b")));
    CHECK(!reg.add(sample("a")));         // duplicate id rejected
    CHECK_EQ(reg.mods().size(), (size_t)2);
    CHECK(reg.find("a") != nullptr);
    CHECK(reg.find("missing") == nullptr);
}

static void test_dependency_validation() {
    ModRegistry reg;
    ModManifest a = sample("a");
    ModManifest b = sample("b");
    b.dependencies = {"a"};
    reg.add(a);
    reg.add(b);
    CHECK(reg.validate().empty());        // b -> a, both enabled: healthy

    reg.setEnabled("a", false);           // dependency now disabled
    CHECK(!reg.validate().empty());

    ModRegistry reg2;
    ModManifest c = sample("c");
    c.dependencies = {"ghost"};
    reg2.add(c);
    CHECK(!reg2.validate().empty());      // missing dependency
}

static void test_resolve_order() {
    ModRegistry reg;
    // c depends on b, b depends on a  =>  order a, b, c
    ModManifest a = sample("a");
    ModManifest b = sample("b"); b.dependencies = {"a"};
    ModManifest c = sample("c"); c.dependencies = {"b"};
    reg.add(c); reg.add(a); reg.add(b);   // insertion order deliberately scrambled

    std::vector<const LoadedMod*> order; std::string err;
    CHECK(reg.resolveOrder(order, err));
    CHECK_EQ(order.size(), (size_t)3);
    // Each dependency must appear before its dependent.
    auto indexOf = [&](const std::string& id) {
        for (size_t i = 0; i < order.size(); ++i)
            if (order[i]->manifest.id == id) return (int)i;
        return -1;
    };
    CHECK(indexOf("a") < indexOf("b"));
    CHECK(indexOf("b") < indexOf("c"));

    // Disabled mods are excluded from the resolved order.
    reg.setEnabled("c", false);
    CHECK(reg.resolveOrder(order, err));
    CHECK_EQ(order.size(), (size_t)2);
}

static void test_resolve_cycle() {
    ModRegistry reg;
    ModManifest a = sample("a"); a.dependencies = {"b"};
    ModManifest b = sample("b"); b.dependencies = {"a"};
    reg.add(a); reg.add(b);
    std::vector<const LoadedMod*> order; std::string err;
    CHECK(!reg.resolveOrder(order, err));  // cycle detected
    CHECK(order.empty());
}

int main() {
    RUN(test_kind_strings);
    RUN(test_roundtrip);
    RUN(test_parse_errors);
    RUN(test_validate_manifest);
    RUN(test_registry_add_find);
    RUN(test_dependency_validation);
    RUN(test_resolve_order);
    RUN(test_resolve_cycle);
    return REPORT();
}
