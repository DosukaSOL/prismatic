<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Roadmap

Concise forward plan. Nothing here is implemented unless its own doc/test says
so; the UI must never present roadmap items as available.

## Shader engine (future)

Game-aware, scene-aware lighting — real depth buffers, camera matrices,
layer-provenance masks and RAM-probed game state feeding profile-defined
world-anchored lights (never framebuffer-brightness guessing). Working branch:
`refactor/game-aware-renderer` (scene stream, savestates, RAM peek, lighting
core and the HGSS probe research are already committed there). Blocked on:
depth→world calibration, Render Lab UI, Vulkan port.

## Foldscape (separate project)

2D→2.5D/3D transformation lives in
[DosukaSOL/Foldscape](https://github.com/DosukaSOL/Foldscape). Prismatic will
load versioned `.foldscape` packages (manifest + geometry/light metadata, no
ROM content) and render them; Prismatic will not implement conversion.

## Pokémon (future families)

Platinum, then Black 2 / White 2 — each as its own
`pokemon-<family>-runtime-and-mods` repository with its own ROM database,
packages and validation. No shared "it worked on HGSS so it's compatible"
claims.

## Zelda (future)

Phantom Hourglass / Spirit Tracks family: heavy 3D + touch-driven controls make
them the best candidates after Pokémon for camera and lighting work. Requires
their own profiles, camera safety rules and validation.

## HGSS native runtime (future)

A pret/pokeheartgold-informed native runtime is the long-term execution mode
(free camera, high-refresh interpolation, deep mods). Until it boots real dumps
with saves + audio + input verified, the app reports **Native runtime: Not
Available**.
