# Shader Reference Catalog

Catalog of shader techniques PRISMATIC uses, their purpose, cost tier, and
whether they are implemented in the tested slice. Techniques are standard and
implemented **originally**; no shader source is copied from unlicensed blogs or
videos (per the brief).

| Technique | Purpose | Stage | Cost | Status |
|---|---|---|---|---|
| Integer / sharp-bilinear scale | Level-1 crisp scaling | post | low | GLSL written + SPIR-V compiled |
| Color grade (exposure/gamma/contrast/saturation/temp/tint/LUT) | Level-1 grading | post | low | GLSL + software renderer |
| Tile-prism lighting (Lambert + ambient + point lights + rim) | Level-2/3 lit geometry | fwd frag | med | GLSL + software renderer |
| Contact shadow (screen-space, short) | ground contact | post | med | software renderer (approx); GLSL stub written |
| Bloom (threshold → blur → composite, highlight-protected) | glow | post | med | GLSL + software renderer |
| Height fog / depth fog (UI-excluded) | atmosphere | post | low | GLSL + software renderer |
| Tilt-shift depth of field (UI-excluded) | cinematic | post | med-high | GLSL written; software approx |
| Water (reflection/refraction/ripple/caustic approx) | water | fwd frag | high | GLSL stub; params modeled |
| Procedural normal/height from tile luminance (edge-aware) | material relief | compute/CPU | med | **CPU implemented + tested** |

## Preset ↔ parameter model
Presets are **data**, not code: a preset is a named set of parameter values
(bloom intensity/threshold/radius, fog density/color, DoF amount, light gains,
geometry depth, camera pitch/zoom, grade curbs…). The same shaders read these
uniforms. All 10 required presets (`ORIGINAL PLUS`, `FAITHFUL HD-2D`, `HD-2.5D
BALANCED`, `CINEMATIC HD-2D`, `DRAMATIC`, `DREAMLIKE`, `NIGHT GLOW`, `PIXEL
PERFECT`, `PERFORMANCE`, `CUSTOM`) are defined in `profiles/presets/` and loaded
by the core; unit tests assert their ranges are safe.

## Fidelity safety in shaders
- **Highlight protection** clamps light/bloom so original art is never blown
  out.
- **UI/text pass** is composited **after** DoF/fog and is never blurred.
- **Nearest-neighbor** sampling for sprites; palette-correct transparency.
