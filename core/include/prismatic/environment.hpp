// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC core — environment & time-of-day.
//
// Produces the environmental lighting term (sky/ground ambient, key light,
// fog, exposure and colour-grade bias) from a time-of-day (0..24h), a weather
// state and a location tag. Day -> night is interpolated across authored key
// frames; weather and tag apply deterministic modifiers.
#pragma once
#include "prismatic/types.hpp"

namespace prismatic {

enum class Weather { Clear, Rain, Fog, Snow };
enum class LocationTag { Overworld, Interior, Cave, Water };

struct EnvironmentState {
    float timeOfDay = 12.0f;   // hours [0,24)
    Weather weather = Weather::Clear;
    LocationTag tag = LocationTag::Overworld;
};

struct EnvLighting {
    Vec3 sunDir{0.3f, 0.5f, -0.8f};  // travel direction of the key light
    Vec3 sunColor{1, 1, 1};
    float sunIntensity = 1.0f;
    Vec3 ambientSky{0.5f, 0.6f, 0.75f};
    Vec3 ambientGround{0.3f, 0.28f, 0.25f};
    float ambientIntensity = 1.0f;
    Vec3 fogColor{0.7f, 0.75f, 0.85f};
    float fogDensity = 0.0f;
    float exposureBias = 1.0f;
    float bloomBias = 1.0f;
    Vec3 gradeMul{1, 1, 1};   // multiplicative colour grade
};

EnvLighting computeEnvLighting(const EnvironmentState& s);

const char* weatherName(Weather w);
const char* locationTagName(LocationTag t);

}  // namespace prismatic
