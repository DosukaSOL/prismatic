#!/usr/bin/env python3
"""Prismatic Crystal recomp preprocessor.

Reads a pokecrystal disassembly checkout (kept LOCAL / gitignored) and bakes one
overworld map into a compact scene blob the C++ voxel renderer consumes:

  magic  "PVX4"
  u32    mapW, mapH        (pixels)
  u32    cell              (px per collision cell, = 16)
  u32    cols, rows        (= mapW/cell, mapH/cell)
  u8[]   classGrid[cols*rows]   (terrain class per 16x16 quadrant)
  u8[]   archeGrid[cols*rows]   (final voxel archetype per cell, ground-truth)
  u8[]   rgba[mapW*mapH*4]      (composited day-time map, RGBA8)
  u32    spriteCount
  { f32 tileX, tileY, scale; u32 w, h; u8 rgba[w*h*4] } * spriteCount
  u32    buildingCount
  { i32 x0, y0, x1, y1, doorX, doorY } * buildingCount   (cell coords)

This script contains only parsing logic (no game data). Its INPUT (pokecrystal)
and OUTPUT (scene blob) are gitignored — nothing copyrighted is committed.

Requires rgbgfx (RGBDS) on PATH to decode the tileset PNG to 2bpp.
"""
from __future__ import annotations

import argparse
import os
import re
import struct
import subprocess
import sys
import tempfile

# Terrain classes (must match crystal_scene.hpp on the C++ side).
CLASS_GROUND = 0
CLASS_GRASS = 1
CLASS_WATER = 2
CLASS_WALL = 3
CLASS_TREE = 4
CLASS_LEDGE = 5
CLASS_DOOR = 6

# Final per-cell voxel archetype (must match Arch in voxel_diorama.cpp). The baker
# decides these from ground truth (collision + tileset palette + warps + signs) so
# the C++ renderer never has to guess and assets never bleed into each other.
A_FLAT = 0
A_WATER = 1
A_GRASS = 2
A_LEDGE = 3
A_FOLIAGE = 4  # tree / foliage dome
A_ROCK = 5     # boulder / non-green wall lump
A_SIGN = 6
A_BUILDING = 7

# pokecrystal collision short-name -> terrain class.
COLL_CLASS = {
    "FLOOR": CLASS_GROUND,
    "WALL": CLASS_WALL,
    "TALL_GRASS": CLASS_GRASS,
    "LONG_GRASS": CLASS_GRASS,
    "HEADBUTT_TREE": CLASS_TREE,
    "CUT_TREE": CLASS_TREE,
    "WATER": CLASS_WATER,
    "WHIRLPOOL": CLASS_WATER,
    "BUOY": CLASS_WATER,
    "ICE": CLASS_WATER,
    "WATERFALL": CLASS_WATER,
    "LADDER": CLASS_GROUND,
    "STAIRCASE": CLASS_GROUND,
    "CAVE": CLASS_DOOR,
    "PIT": CLASS_GROUND,
    "DOOR": CLASS_DOOR,
    "WARP_PANEL": CLASS_DOOR,
    "WARP_CARPET_DOWN": CLASS_DOOR,
    "WARP_CARPET_UP": CLASS_DOOR,
    "WARP_CARPET_LEFT": CLASS_DOOR,
    "WARP_CARPET_RIGHT": CLASS_DOOR,
}

PAL_INDEX = {
    "GRAY": 0, "RED": 1, "GREEN": 2, "WATER": 3,
    "YELLOW": 4, "BROWN": 5, "ROOF": 6, "TEXT": 7,
}

# Overworld sprite recolour tables: source grayscale value -> RGB. The border-
# connected background (value 255) is made transparent via flood fill, so interior
# highlights (Suicune's white crest) survive. Approximates the in-game OBJ palette.
SPRITE_PALS = {
    "suicune": {0: (32, 36, 84), 85: (64, 104, 196), 170: (120, 200, 236), 255: (232, 244, 248)},
    "chris":   {0: (40, 40, 64), 85: (72, 96, 168), 170: (224, 88, 72), 255: (248, 224, 184)},
}
SPRITE_ALIASES = {"player": "chris", "hero": "chris"}


def load_sprite(pc: str, name: str, frame: int = 0):
    """Decode one 16x16 overworld sprite frame to RGBA, border background -> alpha 0."""
    from PIL import Image as _Img  # PIL verified present
    key = SPRITE_ALIASES.get(name, name)
    im = _Img.open(os.path.join(pc, "gfx", "sprites", f"{key}.png")).convert("L")
    px = im.load()
    y0 = frame * 16
    g = [[px[x, y0 + y] for x in range(16)] for y in range(16)]
    bg = g[0][0]
    # Flood-fill transparency inward from the border over background-valued pixels.
    trans = [[False] * 16 for _ in range(16)]
    stack = [(x, y) for x in range(16) for y in (0, 15) if g[y][x] == bg]
    stack += [(x, y) for y in range(16) for x in (0, 15) if g[y][x] == bg]
    while stack:
        x, y = stack.pop()
        if x < 0 or y < 0 or x >= 16 or y >= 16 or trans[y][x] or g[y][x] != bg:
            continue
        trans[y][x] = True
        stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    pal = SPRITE_PALS.get(key, {0: (0, 0, 0), 85: (85, 85, 85), 170: (170, 170, 170), 255: (255, 255, 255)})
    rgba = bytearray(16 * 16 * 4)
    for y in range(16):
        for x in range(16):
            i = (y * 16 + x) * 4
            if trans[y][x]:
                continue  # alpha stays 0
            r, gg, b = pal.get(g[y][x], (255, 0, 255))
            rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3] = r, gg, b, 255
    return 16, 16, bytes(rgba)


def coll_to_class(tok: str) -> int:
    tok = tok.strip().upper()
    if tok in COLL_CLASS:
        return COLL_CLASS[tok]
    # Family fallbacks for the many *_NN variants + ledges.
    if tok.startswith("HOP_"):
        return CLASS_LEDGE
    if "GRASS" in tok:
        return CLASS_GRASS
    if "TREE" in tok:
        return CLASS_TREE
    if any(k in tok for k in ("WATER", "WHIRLPOOL", "CURRENT", "WATERFALL", "BUOY", "ICE")):
        return CLASS_WATER
    if tok.startswith("WARP") or tok.startswith("DOOR"):
        return CLASS_DOOR
    return CLASS_GROUND  # garbage / numeric / unknown -> flat ground


def camel_to_snake(name: str) -> str:
    s = re.sub(r"(?<!^)(?=[A-Z])", "_", name)
    s = re.sub(r"(?<=[A-Za-z])(?=\d)", "_", s)  # Route36 -> ROUTE_36
    return s.upper()


def parse_map_dims(pc: str, map_name: str) -> tuple[int, int]:
    const = camel_to_snake(map_name)
    path = os.path.join(pc, "constants", "map_constants.asm")
    with open(path) as f:
        for line in f:
            m = re.search(r"map_const\s+" + re.escape(const) + r"\s*,\s*(\d+)\s*,\s*(\d+)", line)
            if m:
                return int(m.group(1)), int(m.group(2))
    raise SystemExit(f"map dims not found for {const} in {path}")


def rgbgfx_tiles(png: str) -> list[list[int]]:
    """Decode a tileset PNG to a list of 8x8 tiles (each 64 values 0-3)."""
    with tempfile.NamedTemporaryFile(suffix=".2bpp", delete=False) as tf:
        out = tf.name
    try:
        subprocess.run(["rgbgfx", "-o", out, png], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        data = open(out, "rb").read()
    finally:
        os.unlink(out)
    tiles = []
    for t in range(len(data) // 16):
        base = t * 16
        px = [0] * 64
        for row in range(8):
            lo = data[base + row * 2]
            hi = data[base + row * 2 + 1]
            for col in range(8):
                bit = 7 - col
                px[row * 8 + col] = ((hi >> bit) & 1) << 1 | ((lo >> bit) & 1)
        tiles.append(px)
    return tiles


def parse_palette_map(path: str) -> list[int]:
    pals = []
    with open(path) as f:
        for line in f:
            line = line.split(";")[0].strip()
            if not line.startswith("tilepal"):
                continue
            parts = [p.strip() for p in line[len("tilepal"):].split(",")]
            # parts[0] = vram bank; the rest are palette names.
            for name in parts[1:]:
                pals.append(PAL_INDEX.get(name.upper(), 0))
    return pals


def parse_day_palette(path: str) -> list[list[tuple[int, int, int]]]:
    """Return 8 palettes x 4 RGB888 colors from the '; day' group of bg_tiles.pal."""
    lines = open(path).read().splitlines()
    start = None
    for i, ln in enumerate(lines):
        if ln.strip().lower() == "; day":
            start = i + 1
            break
    if start is None:
        raise SystemExit("'; day' group not found in bg_tiles.pal")
    pals = []
    i = start
    while len(pals) < 8 and i < len(lines):
        ln = lines[i].split(";")[0].strip()
        i += 1
        if not ln.startswith("RGB"):
            continue
        nums = [int(n) for n in re.findall(r"\d+", ln[3:])]
        colors = []
        for c in range(4):
            r5, g5, b5 = nums[c * 3], nums[c * 3 + 1], nums[c * 3 + 2]
            colors.append((
                (r5 * 255 + 15) // 31,
                (g5 * 255 + 15) // 31,
                (b5 * 255 + 15) // 31,
            ))
        pals.append(colors)
    return pals


def parse_collision(path: str) -> list[list[int]]:
    """Return per-block [TL, TR, BL, BR] terrain classes."""
    blocks = []
    with open(path) as f:
        for line in f:
            code = line.split(";")[0].strip()
            if not code.startswith("tilecoll"):
                continue
            toks = [t.strip() for t in code[len("tilecoll"):].split(",")]
            blocks.append([coll_to_class(t) for t in toks[:4]])
    return blocks


def parse_warps(pc: str, map_name: str) -> list[tuple[int, int]]:
    """Return warp (door) positions in collision-cell coords from the map events."""
    path = os.path.join(pc, "maps", f"{map_name}.asm")
    warps = []
    with open(path) as f:
        for line in f:
            code = line.split(";")[0].strip()
            if not code.startswith("warp_event"):
                continue
            nums = re.findall(r"-?\d+", code[len("warp_event"):])
            if len(nums) >= 2:
                warps.append((int(nums[0]), int(nums[1])))
    return warps


def parse_signs(pc: str, map_name: str) -> list[tuple[int, int]]:
    """Return bg_event (signpost) positions in collision-cell coords."""
    path = os.path.join(pc, "maps", f"{map_name}.asm")
    signs = []
    with open(path) as f:
        for line in f:
            code = line.split(";")[0].strip()
            if not code.startswith("bg_event"):
                continue
            nums = re.findall(r"-?\d+", code[len("bg_event"):])
            if len(nums) >= 2:
                signs.append((int(nums[0]), int(nums[1])))
    return signs


def detect_buildings(warps, roof, is_wall, green, cols, rows):
    """Turn each warp door into a building footprint (x0,y0,x1,y1,doorX,doorY).

    Pass A: town houses whose roofs use the ROOF palette — flood the connected
    roof blob above the door, then extend down through the facade walls.

    Pass B: route gate houses whose roofs share the rock/cliff palette (so the
    ROOF flag never fires). Flood the connected non-green SOLID structure inside a
    small window clamped around the door, so the blob captures the gatehouse but
    cannot escape into the map-border cliffs (which are also non-green walls).

    A door with neither a roof nor a local solid structure (a bare route/cave
    mouth in the tree border) still yields no building."""
    buildings = []
    claimed = [[False] * cols for _ in range(rows)]
    struct = [[is_wall[y][x] and not green[y][x] for x in range(cols)] for y in range(rows)]

    def extend_down(x0, x1, yr, wy, solid):
        y1 = yr
        yy = yr + 1
        while yy < rows:
            cnt = sum(1 for xx in range(x0, x1 + 1) if solid[yy][xx])
            if cnt >= (x1 - x0 + 2) // 2:            # majority of the row is solid
                y1 = yy
                yy += 1
            else:
                break
        return max(y1, wy)                            # always reach the door row (grounded)

    # ---- Pass A: ROOF-palette houses (towns) ----
    for (wx, wy) in warps:
        seed = None
        for dy in range(0, 6):                       # search up from the door for roof
            yy = wy - dy
            if yy < 0:
                break
            for dx in (0, -1, 1, -2, 2):
                xx = wx + dx
                if 0 <= xx < cols and roof[yy][xx] and not claimed[yy][xx]:
                    seed = (xx, yy)
                    break
            if seed:
                break
        if seed is None:
            continue                                  # no roof -> try Pass B
        comp = {seed}
        stack = [seed]
        while stack:                                  # flood the connected roof
            x, y = stack.pop()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < cols and 0 <= ny < rows and roof[ny][nx] and (nx, ny) not in comp:
                    comp.add((nx, ny))
                    stack.append((nx, ny))
        for (x, y) in comp:
            claimed[y][x] = True
        xs = [c[0] for c in comp]
        ys = [c[1] for c in comp]
        x0, x1, y0 = min(xs), max(xs), min(ys)
        y1 = extend_down(x0, x1, max(ys), wy, is_wall)
        buildings.append((x0, y0, x1, y1, wx, wy))

    # ---- Pass B: gate houses whose roof shares the rock palette ----
    for (wx, wy) in warps:
        if any(x0 - 1 <= wx <= x1 + 1 and y0 - 1 <= wy <= y1 + 1
               for (x0, y0, x1, y1, _dx, _dy) in buildings):
            continue                                  # door already inside a building
        WX0, WX1 = max(0, wx - 4), min(cols - 1, wx + 4)   # local window: gates are compact,
        WY0, WY1 = max(0, wy - 6), min(rows - 1, wy + 1)   # clamp so we can't reach the border
        seed = None
        for (sx, sy) in [(wx, wy - 1), (wx, wy), (wx - 1, wy - 1), (wx + 1, wy - 1),
                         (wx - 1, wy), (wx + 1, wy), (wx, wy - 2)]:
            if WX0 <= sx <= WX1 and WY0 <= sy <= WY1 and struct[sy][sx] and not claimed[sy][sx]:
                seed = (sx, sy)
                break
        if seed is None:
            continue
        comp = {seed}
        stack = [seed]
        while stack:                                  # flood non-green solid, bounded to window
            x, y = stack.pop()
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if (WX0 <= nx <= WX1 and WY0 <= ny <= WY1 and struct[ny][nx]
                        and (nx, ny) not in comp and not claimed[ny][nx]):
                    comp.add((nx, ny))
                    stack.append((nx, ny))
        xs = [c[0] for c in comp]
        ys = [c[1] for c in comp]
        x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
        if (x1 - x0 + 1) > 8 or (y1 - y0 + 1) > 7:    # absurdly large -> clip to a door-anchored box
            x0, x1 = max(x0, wx - 3), min(x1, wx + 3)
            y0, y1 = max(y0, wy - 5), min(y1, wy)
        y1 = extend_down(x0, x1, y1, wy, struct)      # non-green only: stop at cliffs below
        for yy in range(y0, y1 + 1):
            for xx in range(x0, x1 + 1):
                claimed[yy][xx] = True
        buildings.append((x0, y0, x1, y1, wx, wy))
    return buildings


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pokecrystal", default="local_data/pokecrystal")
    ap.add_argument("--map", default="NewBarkTown")
    ap.add_argument("--tileset", default="johto")
    ap.add_argument("--out", default="local_data/scenes/NewBarkTown.pvx")
    ap.add_argument("--sprite", action="append", default=[],
                    help="place an overworld sprite: NAME@BX,BY[,SCALE] in block coords "
                         "(e.g. suicune@8,4  player@8,6,1.15)")
    args = ap.parse_args()

    pc = args.pokecrystal
    ts = args.tileset
    w_blocks, h_blocks = parse_map_dims(pc, args.map)

    tiles = rgbgfx_tiles(os.path.join(pc, "gfx", "tilesets", f"{ts}.png"))
    tilepal = parse_palette_map(os.path.join(pc, "gfx", "tilesets", f"{ts}_palette_map.asm"))
    daypal = parse_day_palette(os.path.join(pc, "gfx", "tilesets", "bg_tiles.pal"))
    collision = parse_collision(os.path.join(pc, "data", "tilesets", f"{ts}_collision.asm"))
    metatiles = open(os.path.join(pc, "data", "tilesets", f"{ts}_metatiles.bin"), "rb").read()
    blk = open(os.path.join(pc, "maps", f"{args.map}.blk"), "rb").read()

    n_meta = len(metatiles) // 16
    max_tid = max(metatiles) if metatiles else 0
    print(f"  map {args.map}: {w_blocks}x{h_blocks} blocks, {len(tiles)} tiles, "
          f"{n_meta} metatiles (max tile id {max_tid}), {len(collision)} collision rows")

    map_w = w_blocks * 32
    map_h = h_blocks * 32
    cell = 16
    cols = map_w // cell
    rows = map_h // cell
    rgba = bytearray(map_w * map_h * 4)
    classes = bytearray(cols * rows)
    roof = [[False] * cols for _ in range(rows)]   # cell dominated by ROOF-palette tiles
    green = [[False] * cols for _ in range(rows)]  # cell dominated by GREEN-palette tiles

    for by in range(h_blocks):
        for bx in range(w_blocks):
            block = blk[by * w_blocks + bx]
            meta = metatiles[block * 16: block * 16 + 16]
            # 4x4 tiles per block.
            for sy in range(4):
                for sx in range(4):
                    tid = meta[sy * 4 + sx]
                    tile = tiles[tid] if tid < len(tiles) else [0] * 64
                    pal = daypal[tilepal[tid]] if tid < len(tilepal) else daypal[0]
                    ox = bx * 32 + sx * 8
                    oy = by * 32 + sy * 8
                    for py in range(8):
                        for px in range(8):
                            r, g, b = pal[tile[py * 8 + px]]
                            di = ((oy + py) * map_w + (ox + px)) * 4
                            rgba[di] = r
                            rgba[di + 1] = g
                            rgba[di + 2] = b
                            rgba[di + 3] = 255
            # 2x2 collision quadrants per block -> class grid; dominant tile palette
            # per quadrant -> roof / green flags (the ground-truth material).
            cls = collision[block] if block < len(collision) else [CLASS_GROUND] * 4
            for qy in range(2):
                for qx in range(2):
                    classes[(by * 2 + qy) * cols + (bx * 2 + qx)] = cls[qy * 2 + qx]
                    pc_cnt: dict[int, int] = {}
                    for ty in range(2):
                        for tx in range(2):
                            tid = meta[(qy * 2 + ty) * 4 + (qx * 2 + tx)]
                            p = tilepal[tid] if tid < len(tilepal) else 0
                            pc_cnt[p] = pc_cnt.get(p, 0) + 1
                    dom = max(pc_cnt, key=pc_cnt.get)
                    cy, cx = by * 2 + qy, bx * 2 + qx
                    roof[cy][cx] = dom == PAL_INDEX["ROOF"]
                    green[cy][cx] = dom == PAL_INDEX["GREEN"]

    # ---- Ground-truth object placement (no pixel guessing on the C++ side) ----
    warps = parse_warps(pc, args.map)
    signs = parse_signs(pc, args.map)
    is_wall = [[classes[y * cols + x] in (CLASS_WALL, CLASS_DOOR) for x in range(cols)]
               for y in range(rows)]
    buildings = detect_buildings(warps, roof, is_wall, green, cols, rows)

    if os.environ.get("PVX_DEBUG"):
        hdr = "   " + "".join(str(x % 10) for x in range(cols))
        print("ROOF grid (R=roof-palette):\n" + hdr)
        for y in range(rows):
            print(f"{y:2} " + "".join("R" if roof[y][x] else "." for x in range(cols)))
        print("GREEN grid (G=green-palette):\n" + hdr)
        for y in range(rows):
            print(f"{y:2} " + "".join("G" if green[y][x] else "." for x in range(cols)))
        print(f"WARPS: {warps}")

    arche = bytearray(cols * rows)                    # default A_FLAT = 0
    for cy in range(rows):
        for cx in range(cols):
            c = classes[cy * cols + cx]
            if c == CLASS_WATER:
                a = A_WATER
            elif c == CLASS_GRASS:
                a = A_GRASS
            elif c == CLASS_LEDGE:
                a = A_LEDGE
            elif c == CLASS_TREE:
                a = A_FOLIAGE
            elif c == CLASS_WALL:
                a = A_FOLIAGE if green[cy][cx] else A_ROCK
            else:
                a = A_FLAT                             # FLOOR / DOOR
            arche[cy * cols + cx] = a
    for (x0, y0, x1, y1, _wx, _wy) in buildings:      # stamp building footprints
        for yy in range(y0, y1 + 1):
            for xx in range(x0, x1 + 1):
                arche[yy * cols + xx] = A_BUILDING
    for (sx, sy) in signs:                             # freestanding signs only
        if 0 <= sx < cols and 0 <= sy < rows and arche[sy * cols + sx] != A_BUILDING:
            arche[sy * cols + sx] = A_SIGN

    print(f"  placed {len(buildings)} building(s), "
          f"{sum(1 for i in arche if i == A_SIGN)} sign(s) from map events")

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    with open(args.out, "wb") as f:
        f.write(b"PVX4")
        f.write(struct.pack("<5I", map_w, map_h, cell, cols, rows))
        f.write(bytes(classes))
        f.write(bytes(arche))
        f.write(bytes(rgba))
        # Sprite billboards.
        sprites = []
        for spec in args.sprite:
            name, _, rest = spec.partition("@")
            parts = rest.split(",")
            bx, by = float(parts[0]), float(parts[1])
            scl = float(parts[2]) if len(parts) > 2 else 1.0
            sw, sh, srgba = load_sprite(pc, name.strip(), 0)
            # Block coords -> 8px-tile centre (block = 4 tiles); feet centred on block.
            tile_x = bx * 4 + 2.0
            tile_y = by * 4 + 2.0
            sprites.append((tile_x, tile_y, scl, sw, sh, srgba))
        f.write(struct.pack("<I", len(sprites)))
        for tx, ty, scl, sw, sh, srgba in sprites:
            f.write(struct.pack("<fffII", tx, ty, scl, sw, sh))
            f.write(srgba)
        f.write(struct.pack("<I", len(buildings)))     # explicit building rectangles
        for (x0, y0, x1, y1, wx, wy) in buildings:
            f.write(struct.pack("<6i", x0, y0, x1, y1, wx, wy))
    extra = f", {len(sprites)} sprite(s)" if sprites else ""
    print(f"  wrote {args.out} ({map_w}x{map_h}px, {cols}x{rows} cells{extra})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
