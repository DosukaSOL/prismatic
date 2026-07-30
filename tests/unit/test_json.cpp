// SPDX-License-Identifier: GPL-3.0-or-later
#include "test_util.hpp"
#include "prismatic/json.hpp"
#include <string>
using namespace prismatic;

static void test_roundtrip() {
    std::string src = R"({"a":1,"b":[true,false,null,2.5],"c":{"d":"hi\n"},"e":-3})";
    JsonValue v = JsonParser::parse(src);
    CHECK(v.isObject());
    CHECK_EQ(v.get("a").asInt(), 1);
    CHECK(v.get("b").isArray());
    CHECK_EQ(v.get("b").arr->size(), (size_t)4);
    CHECK_EQ((*v.get("b").arr)[0].asBool(), true);
    CHECK_NEAR((*v.get("b").arr)[3].asNumber(), 2.5, 1e-9);
    CHECK_EQ(v.get("c").get("d").asString(), std::string("hi\n"));
    CHECK_EQ(v.get("e").asInt(), -3);
    // Re-parse the dump to ensure it is valid JSON.
    JsonValue v2 = JsonParser::parse(v.dump());
    CHECK_EQ(v2.get("a").asInt(), 1);
}

static void test_errors() {
    bool threw = false;
    try { JsonParser::parse("{ bad }"); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { JsonParser::parse("[1,2"); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
}

static void test_depth_bound() {
    // Deeply nested arrays must be rejected, not crash.
    std::string deep;
    for (int i = 0; i < 200; ++i) deep += "[";
    for (int i = 0; i < 200; ++i) deep += "]";
    bool threw = false;
    try { JsonParser::parse(deep); } catch (const std::exception&) { threw = true; }
    CHECK(threw);
}

int main() {
    RUN(test_roundtrip);
    RUN(test_errors);
    RUN(test_depth_bound);
    return REPORT();
}
