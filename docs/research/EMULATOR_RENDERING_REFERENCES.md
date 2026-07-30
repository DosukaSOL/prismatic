# Emulator Rendering References

How mature emulators expose the structured graphics PRISMATIC needs, and how we
map that onto the Emulator Adapter API. Verified items are from live fetches;
others are flagged for verification before code reuse.

## What "structured graphics capture" needs
For Level 2–3 enhancement we need more than the final framebuffer:
- background tile maps + tile graphics + palettes,
- OAM/sprite table with priority + affine params,
- scroll registers, window masks, blend state,
- (DS) 2D-engine per-screen output, and 3D geometry/native color + depth,
- per-screen routing (which engine → which physical screen).

## mGBA (GB/GBC/GBA) — MPL-2.0 [verified 2026-07-30]
- C core with clean module boundaries; provides framebuffer output and has
  in-tree **graphics inspection** used by its Qt debugger (tile/sprite/map/
  palette viewers) plus **Lua scripting** with memory access.
- Integration plan: build mGBA as a library; the **GbaAdapter** pulls the
  framebuffer via the core's video output, and reads BG/OAM/palette/scroll from
  I/O + VRAM/OAM memory regions (documented by GBATEK) to populate PRISMATIC's
  `StructuredFrame`. Capability flags advertise exactly what is available.

## melonDS (DS/DSi) — GPL-3.0 [verified 2026-07-30]
- C++ core with software + OpenGL 2D/3D renderers; exposes both screens.
- Integration plan: build melonDS as a library (GPL-3.0 build); the **NdsAdapter**
  provides two framebuffers (top/bottom) and screen-routing state. Structured
  2D capture (BG/OAM/palette) is read from the DS 2D-engine state; native 3D
  output is captured as color(+depth where the renderer exposes it). Where a
  datum is not exposed, the capability flag is cleared and PRISMATIC falls back.

## DeSmuME — GPL-2.0 [general knowledge — verify]
- Historically strong tile/OAM/map viewers. Study as a **concept** reference for
  what DS structured views are feasible; not a required dependency.

## libretro / RetroArch — [general knowledge — verify]
- The libretro core ABI + `retro_get_memory_data`/environment callbacks and the
  **slang** preset shader chain are strong prior art. PRISMATIC's adapter is
  *inspired by* this separation but is a **native, capability-flagged C++
  interface** tuned for structured capture and dual-display, not the libretro
  ABI (kept as a future option).

## Mapping to PRISMATIC
```
EmulatorAdapter (versioned)
  ├─ getFramebuffer(screen)          // always available (Level 0/1)
  ├─ getStructuredFrame(screen)      // if CAP_BG_LAYERS | CAP_OAM | ...
  │    ├─ backgrounds[] (tilemap + tileset + palette + scroll + priority)
  │    ├─ sprites[]     (OAM: pos, tile, palette, priority, affine, flip)
  │    ├─ windows[] / blend
  │    └─ nds3d (optional: polygons or captured color+depth)
  ├─ getScreenRouting()              // DS: which engine → which screen
  └─ capabilities()                  // bitset, queried before use
```
The synthetic backend implements this fully so the pipeline is testable now;
mGBA/melonDS adapters implement the same interface when wired.
