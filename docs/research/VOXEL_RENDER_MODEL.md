# Voxel Diorama Render Model

How PRISMATIC turns a flat 2D Game Boy / GBC overworld (a tile map + tile art)
into a tilted 2.5D voxel diorama that matches the look of **gen1recomp** and its
**DramaticShape voxel mod**. This document records the reverse-engineered render
model and our own CPU-rasteriser port so the knowledge is not lost.

Reference targets (studied 2026-07-31):
- `bryanthaboi/gen1recomp` — LÖVE2D engine; `Tilt.lua` / `Zoom.lua` are a flat
  perspective only.
- `DramaticShape/DramaticShapeVoxelMod` (branch `master`) — a render-pipeline
  mod that builds the tile layer into **one static 3D mesh** with real depth,
  shadow map and shaders. This is where the genuine 3D *shapes* live.

Our implementation lives in [core/src/voxel_diorama.cpp](../../core/src/voxel_diorama.cpp)
(`renderVoxelHeightfield`) and is driven by
[tools/crystal-recomp/render_scene.cpp](../../tools/crystal-recomp/render_scene.cpp).

---

## 1. Why a naïve extrusion looks "boxed"

The first attempt extruded every "tall" cell to a flat-topped box at a uniform
height and merged neighbours. That produces a blocky wall of cubes — trees,
rocks and buildings all look identical. gen1recomp instead gives **each
connected object its own measured height and a shaped archetype**. That single
idea is the whole difference between "slop" and "diorama".

## 2. Geometry archetypes

Every cell is classified into one archetype and rendered with a matching shape:

| Archetype    | Used for                        | Shape |
|--------------|---------------------------------|-------|
| `flat`       | ground, water, void             | one ground quad; water recesses so shorelines show a lip |
| `top`        | ledge, roof, bed                | box with the art on the **top** face; a short side lip |
| `upright`/`volume` | wall, fence, sign, counter, **building** | box whose **south face folds the 2D art upright**, band by band |
| `object`     | per-pixel props (plants, signs) | per-pixel voxel prisms honouring the silhouette |
| **`cylinder`** | **tree canopies, hedges, boulders / rocks** | **one voxel dome per 16×16 cell, carved from the pixel outline** |
| `canopy`     | big 2×2-cell trees (Viridian Forest) | one 32 px hull |
| `post`       | fence posts                     | each cell alone in its own depth band |
| `stair`      | staircases                      | real stepped geometry |
| `bookcase`   | tall furniture                  | collapses to a one-cell-deep box |

### The fold-up rule (buildings, walls, fences)

The south (front) face of an upright volume is drawn by **folding the 2D tile
art upright, 8 px band by band**: band *k* samples the map row *k* tiles north of
the front edge. The top of the box wears the structure's topmost rows. Because
the whole facade is folded from **one continuous card**, doors and windows stay
perfectly aligned. This is why buildings read as real buildings and not as a
texture smeared on a cube.

### The cylinder rule (trees & rocks) — the crucial one

Round cells become **one voxel ball per 16×16 cell, carved from the darkest-pixel
outline, rounded in depth**. A row of trees therefore becomes a **row of
individual canopies**, and a rock field becomes individual lumps — never a
monolith. This is the fix for "the trees look flat / identical".

## 3. Heights (world pixels, cell = 16×16)

```
ground 0   water -2   void 0    ledge 6    fence 10   sign 12
wall 16    tree 16    cylinder 16          cliff 32   canopy 32   roof 28
bed 7      stool 8    counter 8  table 12   desk 24    prop 16
```

Note `wall` and `tree` are **16 px = exactly one cell**, *not* three. Height is
**measured from the drawing and repeat-aware**: a 6-row house is ~48 px, but a
40-row border forest is *rows of 16 px trees*, detected by the 16 px repeat unit
so each cell becomes its own object instead of one giant slab.

## 4. Buildings — roof shape

Buildings are matched by their exact tile grid and carry an authored band table:
`roofRows` (top rows that face up), `roofBack`, `roofFront`, `slab` (roof
thickness ≈ 4), `frontEave` (overhang ≈ 4), `ledge` (awning).

- **Roof elevation profile** = the topmost drawn row of each column. A drawn
  taper becomes a slope; a top-down roof stays level.
- **Gable** (`ChunkMesher.gableH`): the roof rises from the south eave to a ridge
  at the footprint middle, then falls to the north edge; the east/west flanks hip
  (drop toward the eave, corners rounded to 45°):
  `gableH(d) = h + rise · clamp(d ≤ mid ? d/mid : (extent−d)/(extent−mid))`.
- Sloped roofs group by `roofRows` depth: 16 (small house), 32 (lab), 64
  (museum). A flat civic block uses `roofRows = 32`, all top-down → a level roof.

## 5. World curve (Animal-Crossing tilt)

Every vertex is pushed **down** by the square of its horizontal distance from the
camera focus, so the far world curves away:

$$y' = y - k\,\big((x - c_x)^2 + (z - c_z)^2\big)$$

Exposed as a mod setting (our CLI flag `--curve K`), not baked into the pipeline.

## 6. Camera & shading

- **Camera**: a tilt ladder (0 / 15 / 35 / 50°, with a 75° rung) driving a real
  3D camera, plus survey zoom. Hotkeys cycle the rungs.
- **Shading**: a real sun **shadow map** (buildings and trees cast), per-vertex
  **ambient occlusion** baked into every corner seam (`AO_STRENGTH ≈ 2.4`,
  `AO_FLOOR ≈ 0.25`), and directional **face shades** — the **south/front face is
  full-bright (1.0)** because it carries the facade art; other faces are darker;
  `VOLUME_TOP_SHADE ≈ 0.85`. A tilt-shift post pass gives the miniature look.

> **Critical quality lesson:** gen1recomp keeps the **crisp Game Boy pixel
> texture on every face**. Smooth Lambert gradients across a dome destroy that
> texture and read as "blobby slop". Faces must be **flat-shaded** with the
> original pixel art sampled **nearest-neighbour** on top.

---

## 7. Our CPU port (`renderVoxelHeightfield`)

We cannot port LÖVE's GPU 3D pipeline, so we rasterise on the CPU:

1. **Classifier.** Per cell, `cellArt` returns the average colour and a
   green-**fraction** flag (`> 0.22` of pixels green-dominant ⇒ foliage; robust
   against brown trunks). Connected components over tall cells (`Wall|Tree`) are
   split into **buildings** (`2 ≤ min side`, `max side ≤ 7`, `fill ≥ 0.5`,
   `area ≥ 6`) versus per-cell **foliage** (green) or **rock** (non-green).

2. **Crisp voxel domes** (`drawDome`). Each cell is an 8×8 grid of 2 px voxel
   columns with height `h(i,j) = H·(1 − r²)^e` (`e = 0.5` tree, `0.9` rock).
   Columns with `h < 1` are skipped so the round footprint reveals the ground.
   The source pixel art rides the **top** faces (nearest sample); exposed
   south/east/west faces are **flat single-colour** (front `0.70×`, side
   `0.58×`). No Lambert gradient — this is what keeps the texture crisp.

3. **Folded buildings** (`drawBuilding`). Back wall, east/west walls rising to
   the roofline, a single-slope textured roof (from the roof art's top rows) and
   a full-bright folded front facade with the aligned door — drawn
   back → sides → roof → front (painter's order).

4. **Passes.** Pass 1 lays the flat ground per 8 px tile far→near (water recessed
   and tinted). Pass 2 walks cell rows far→near, dispatching each archetype and
   interleaving upright sprite billboards (bottom-anchored, drop shadow, alpha)
   for correct occlusion.

Heights used: `H_WATER −2`, `H_LEDGE 6`, `H_TREE 14`, `H_ROCK 6`, `H_BODY 18`,
`H_RIDGE 15`.

### Bugs fixed during the port (keep these in mind)

- **Textureless blob trees** — caused by wrapping the whole tile over a
  hemisphere plus smooth Lambert shading. Fixed by the crisp flat-shaded voxel
  columns above. *Never* apply a smooth gradient to pixel-art faces.
- **Flat building** — `drawBuilding` was triggered only when the object loop hit
  the region's top-left anchor cell; if that anchor was scrolled off the crop the
  building was skipped and only its flat roof art showed. Fixed by tracking a
  `bldDrawn` flag and drawing each building once at its front row
  (`clamp(cy1, ccy0, ccy1)`) whenever it horizontally overlaps the crop.

### CLI

```
render_scene --scene X.pvx --out PREFIX [--scale N] [--shader]
             [--region BX,BY,BW,BH]   # crop, block coords (1 block = 4 tiles)
             [--tilt DEG] [--focal F] [--zstep Z]
             [--curve K] [--height S] [--zoom Z]
```

`--shader` additionally writes `PREFIX_shaded.png` (2× supersample + bloom /
saturation / warm grade / vignette).

## 8. Status

Validated on New Bark Town, Cherrygrove City and the Route 36 Suicune encounter:
individual crisp voxel tree canopies, buildings with sloped roofs and aligned
doors/awnings, recessed water with clean shorelines, and upright Suicune / player
billboards. This is the quality bar; live in-engine playback and the Android
renderer are the remaining integration work.
