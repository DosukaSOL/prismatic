// SPDX-License-Identifier: GPL-3.0-or-later
#include "prismatic/camera.hpp"
#include <cmath>

namespace prismatic {

FloatBuffer computeParallaxOffsetY(const FloatBuffer& height, const CameraConfig& c) {
    FloatBuffer off(height.width, height.height, 0.0f);
    const float heightPx = 10.0f;  // maps unit height to pixels of lift
    for (int y = 0; y < height.height; ++y) {
        for (int x = 0; x < height.width; ++x) {
            float h = height.at(x, y);
            // Lift proportional to height; slight extra tilt from screen row.
            float lift = -(h * c.parallax * heightPx);
            float tilt = -(c.pitch * (float)(height.height - y) / height.height) * 2.0f * h;
            float o = lift + tilt;
            if (c.gameplaySafe) o = clampf(o, -c.safeMarginY, c.safeMarginY);
            off.at(x, y) = o;
        }
    }
    return off;
}

float maxAbsOffset(const FloatBuffer& offset) {
    float m = 0.0f;
    for (float v : offset.data) m = std::max(m, std::fabs(v));
    return m;
}

}  // namespace prismatic
