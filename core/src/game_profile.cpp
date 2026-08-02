// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/game_profile.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace prismatic {
namespace {

JsonValue vec3Json(const float* v) {
    JsonValue a = JsonValue::makeArray();
    a.push(JsonValue((double)v[0])); a.push(JsonValue((double)v[1])); a.push(JsonValue((double)v[2]));
    return a;
}
void readVec3(const JsonValue& j, float* out) {
    if (!j.isArray() || j.arr->size() < 3) return;
    for (int i = 0; i < 3; ++i) out[i] = (float)(*j.arr)[i].asNumber(out[i]);
}
JsonValue strArray(const std::vector<std::string>& v) {
    JsonValue a = JsonValue::makeArray();
    for (auto& s : v) a.push(JsonValue(s));
    return a;
}
std::vector<std::string> readStrArray(const JsonValue& j) {
    std::vector<std::string> out;
    if (j.isArray()) for (auto& e : *j.arr) if (e.isString()) out.push_back(e.str);
    return out;
}
JsonValue intArray(const std::vector<int>& v) {
    JsonValue a = JsonValue::makeArray();
    for (int i : v) a.push(JsonValue(i));
    return a;
}
std::vector<int> readIntArray(const JsonValue& j) {
    std::vector<int> out;
    if (j.isArray()) for (auto& e : *j.arr) if (e.isNumber()) out.push_back(e.asInt());
    return out;
}
uint32_t parseAddr(const std::string& s) {
    return (uint32_t)std::strtoul(s.c_str(), nullptr, 16);
}
char hexdig(unsigned v) { return v < 10 ? (char)('0' + v) : (char)('A' + v - 10); }
std::string addrString(uint32_t a) {
    std::string s = "0x";
    for (int i = 28; i >= 0; i -= 4) s += hexdig((a >> i) & 0xF);
    return s;
}

}  // namespace

JsonValue gameProfileToJson(const GameProfile& p) {
    JsonValue j = JsonValue::makeObject();
    j.set("version", p.version);
    j.set("kind", "prismprofile");
    j.set("id", p.id);
    j.set("name", p.name);
    j.set("system", p.system);
    j.set("gameCodes", strArray(p.gameCodes));
    j.set("romSha256", strArray(p.romSha256));

    JsonValue probes = JsonValue::makeArray();
    for (auto& pr : p.probes) {
        JsonValue o = JsonValue::makeObject();
        o.set("id", pr.id);
        o.set("addr", addrString(pr.addr));
        o.set("bytes", pr.bytes);
        probes.push(o);
    }
    j.set("probes", probes);

    JsonValue times = JsonValue::makeArray();
    for (auto& t : p.timeRanges) {
        JsonValue o = JsonValue::makeObject();
        o.set("name", t.name);
        o.set("start", t.startHour);
        o.set("end", t.endHour);
        times.push(o);
    }
    j.set("timeRanges", times);

    JsonValue lights = JsonValue::makeArray();
    for (auto& l : p.lights) {
        JsonValue o = JsonValue::makeObject();
        o.set("id", l.id);
        o.set("type", l.type);
        o.set("pos", vec3Json(l.pos));
        o.set("color", vec3Json(l.color));
        o.set("luminance", (double)l.luminance);
        o.set("range", (double)l.range);
        o.set("attenuation", l.attenuation);
        o.set("maps", intArray(l.maps));
        o.set("times", strArray(l.times));
        o.set("castsShadows", l.castsShadows);
        o.set("flicker", (double)l.flicker);
        lights.push(o);
    }
    j.set("lights", lights);

    JsonValue envs = JsonValue::makeArray();
    for (auto& e : p.environments) {
        JsonValue o = JsonValue::makeObject();
        o.set("id", e.id);
        o.set("maps", intArray(e.maps));
        o.set("times", strArray(e.times));
        o.set("indoor", e.indoor);
        o.set("ambientColor", vec3Json(e.ambientColor));
        o.set("ambientIntensity", (double)e.ambientIntensity);
        o.set("exposure", (double)e.exposure);
        o.set("bloomThreshold", (double)e.bloomThreshold);
        o.set("bloomIntensity", (double)e.bloomIntensity);
        envs.push(o);
    }
    j.set("environments", envs);
    return j;
}

bool parseGameProfile(const std::string& json, GameProfile& out, std::string& error) {
    JsonValue j;
    try {
        j = JsonParser::parse(json);
    } catch (const std::exception& e) {
        error = std::string("JSON parse error: ") + e.what();
        return false;
    }
    if (!j.isObject()) { error = "profile root must be an object"; return false; }
    GameProfile p;
    p.version = j.get("version").asInt(0);
    if (p.version <= 0 || p.version > kGameProfileVersion) {
        error = "unsupported profile version";
        return false;
    }
    p.id = j.get("id").asString("");
    p.name = j.get("name").asString("");
    p.system = j.get("system").asString("");
    p.gameCodes = readStrArray(j.get("gameCodes"));
    p.romSha256 = readStrArray(j.get("romSha256"));

    const JsonValue& probes = j.get("probes");
    if (probes.isArray())
        for (auto& o : *probes.arr) {
            StateProbe pr;
            pr.id = o.get("id").asString("");
            pr.addr = parseAddr(o.get("addr").asString("0"));
            pr.bytes = o.get("bytes").asInt(2);
            p.probes.push_back(pr);
        }
    const JsonValue& times = j.get("timeRanges");
    if (times.isArray())
        for (auto& o : *times.arr) {
            TimeRange t;
            t.name = o.get("name").asString("");
            t.startHour = o.get("start").asInt(0);
            t.endHour = o.get("end").asInt(0);
            p.timeRanges.push_back(t);
        }
    const JsonValue& lights = j.get("lights");
    if (lights.isArray())
        for (auto& o : *lights.arr) {
            LightAnchor l;
            l.id = o.get("id").asString("");
            l.type = o.get("type").asString("point");
            readVec3(o.get("pos"), l.pos);
            readVec3(o.get("color"), l.color);
            l.luminance = (float)o.get("luminance").asNumber(l.luminance);
            l.range = (float)o.get("range").asNumber(l.range);
            l.attenuation = o.get("attenuation").asString(l.attenuation);
            l.maps = readIntArray(o.get("maps"));
            l.times = readStrArray(o.get("times"));
            l.castsShadows = o.get("castsShadows").asBool(false);
            l.flicker = (float)o.get("flicker").asNumber(0.0);
            p.lights.push_back(l);
        }
    const JsonValue& envs = j.get("environments");
    if (envs.isArray())
        for (auto& o : *envs.arr) {
            SceneEnvironment e;
            e.id = o.get("id").asString("");
            e.maps = readIntArray(o.get("maps"));
            e.times = readStrArray(o.get("times"));
            e.indoor = o.get("indoor").asBool(false);
            readVec3(o.get("ambientColor"), e.ambientColor);
            e.ambientIntensity = (float)o.get("ambientIntensity").asNumber(1.0);
            e.exposure = (float)o.get("exposure").asNumber(1.0);
            e.bloomThreshold = (float)o.get("bloomThreshold").asNumber(0.80);
            e.bloomIntensity = (float)o.get("bloomIntensity").asNumber(0.35);
            p.environments.push_back(e);
        }
    out = std::move(p);
    return true;
}

std::string serializeGameProfile(const GameProfile& p) {
    return gameProfileToJson(p).dump(2);
}

bool loadGameProfileFile(const std::string& path, GameProfile& out, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "cannot open " + path; return false; }
    std::ostringstream ss;
    ss << f.rdbuf();
    return parseGameProfile(ss.str(), out, error);
}

}  // namespace prismatic
