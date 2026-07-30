// SPDX-License-Identifier: GPL-3.0-or-later
#include "synthetic_backend.hpp"
#include "prismatic/composite.hpp"
#include <array>
#include <cstring>

namespace prismatic {

namespace {

// Convert 8 rows of 8 hex chars ('.' = transparent index 0) into a Tile.
Tile parseTile(const std::array<const char*, 8>& rows) {
    Tile t;
    for (int y = 0; y < 8; ++y) {
        const char* r = rows[y];
        for (int x = 0; x < 8; ++x) {
            char c = r[x];
            uint8_t v = 0;
            if (c >= '0' && c <= '9') v = (uint8_t)(c - '0');
            else if (c >= 'a' && c <= 'f') v = (uint8_t)(c - 'a' + 10);
            else v = 0;  // '.' or space
            t.set(x, y, v);
        }
    }
    return t;
}

// Background tileset indices.
enum BT { Grass0 = 0, Grass1, PathT, WaterT, Water2T, FlowerT, SandT, TrunkT,
          CanopyT, WallT, RoofT, DoorT, EmptyT, BT_Count };

// Simple index-buffer canvas for procedural sprites.
struct Canvas {
    int w, h;
    std::vector<uint8_t> px;
    Canvas(int w_, int h_) : w(w_), h(h_), px((size_t)w_ * h_, 0) {}
    void set(int x, int y, uint8_t v) { if (x >= 0 && y >= 0 && x < w && y < h) px[(size_t)y * w + x] = v; }
    uint8_t get(int x, int y) const { return (x >= 0 && y >= 0 && x < w && y < h) ? px[(size_t)y * w + x] : 0; }
    void fillRect(int x0, int y0, int x1, int y1, uint8_t v) {
        for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) set(x, y, v);
    }
    void fillCircle(int cx, int cy, int r, uint8_t v) {
        for (int y = cy - r; y <= cy + r; ++y)
            for (int x = cx - r; x <= cx + r; ++x) {
                int dx = x - cx, dy = y - cy;
                if (dx * dx + dy * dy <= r * r) set(x, y, v);
            }
    }
};

// Slice a 16x16 canvas into 4 tiles (row-major 2x2).
std::vector<Tile> sliceToTiles(const Canvas& c) {
    std::vector<Tile> tiles;
    int tw = c.w / 8, th = c.h / 8;
    for (int ty = 0; ty < th; ++ty)
        for (int tx = 0; tx < tw; ++tx) {
            Tile t;
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    t.set(x, y, c.get(tx * 8 + x, ty * 8 + y));
            tiles.push_back(t);
        }
    return tiles;
}

// ---- Procedural chibi player (our own art, 16x16) -------------------------
// palette: 0 transp,1 outline,2 skin,3 hair,4 shirt,5 pants,6 shoe,7 white
Canvas makeChibi(int facing, int phase) {
    Canvas c(16, 16);
    // Head
    c.fillCircle(8, 5, 3, 1);       // outline
    c.fillCircle(8, 5, 2, 2);       // skin
    // Hair
    c.fillRect(5, 2, 10, 3, 3);
    if (facing == 1) c.fillRect(5, 2, 10, 6, 3);  // back of head
    // Eyes
    if (facing == 0) { c.set(7, 5, 1); c.set(9, 5, 1); }
    else if (facing == 2) { c.set(6, 5, 1); }
    else if (facing == 3) { c.set(10, 5, 1); }
    // Body
    c.fillRect(5, 8, 10, 11, 1);    // outline
    c.fillRect(6, 8, 9, 11, 4);     // shirt
    // Arms
    c.fillRect(4, 8, 4, 10, 2);
    c.fillRect(11, 8, 11, 10, 2);
    // Legs (animated)
    int lLift = (phase == 1) ? 1 : 0;
    int rLift = (phase == 1) ? 0 : 1;
    c.fillRect(6, 12, 7, 14 - lLift, 5); c.set(6, 15 - lLift, 6); c.set(7, 15 - lLift, 6);
    c.fillRect(8, 12, 9, 14 - rLift, 5); c.set(8, 15 - rLift, 6); c.set(9, 15 - rLift, 6);
    return c;
}

// ---- Procedural follower critter (our own design, NOT any real character) --
// palette: 0 transp,1 outline,2 body,3 leaf,4 belly,5 eyewhite,6 eyeblack,7 cheek
Canvas makeCritter(int facing, int phase) {
    Canvas c(16, 16);
    // Sprout leaf on top
    c.fillRect(7, 1, 8, 3, 3);
    c.set(6, 2, 3); c.set(9, 2, 3);
    // Body
    c.fillCircle(8, 9, 5, 1);       // outline
    c.fillCircle(8, 9, 4, 2);       // body
    c.fillCircle(8, 11, 2, 4);      // belly
    // Eyes
    int ex = (facing == 2) ? -1 : (facing == 3) ? 1 : 0;
    c.set(6 + ex, 8, 5); c.set(6 + ex, 8, 5);
    c.set(6 + ex, 8, 6); c.set(10 + ex, 8, 6);
    c.set(6 + ex, 7, 5); c.set(10 + ex, 7, 5);
    // Cheeks
    c.set(5, 10, 7); c.set(11, 10, 7);
    // Feet (animated)
    int f = (phase == 1) ? 1 : 0;
    c.set(6, 14 - f, 1); c.set(7, 14 - f, 1);
    c.set(9, 13 + f, 1); c.set(10, 13 + f, 1);
    return c;
}

Canvas makeCursor() {
    Canvas c(16, 16);
    // A simple pointing hand / arrow, palette: 0 transp,1 outline,2 white,3 accent
    for (int i = 0; i < 8; ++i) { c.set(3, 3 + i, 1); c.set(3 + i, 3, 1); }
    for (int i = 0; i < 6; ++i) c.set(4 + i, 4 + i, 3);
    c.fillRect(4, 4, 5, 8, 2);
    c.set(4, 9, 1); c.set(5, 10, 1); c.set(6, 11, 1);
    return c;
}

}  // namespace

SyntheticDsBackend::SyntheticDsBackend() { buildAssets(); reset(); }

void SyntheticDsBackend::buildAssets() {
    bgPalettes_.assign(1, Palette{
        Color{0, 0, 0, 0},        // 0 transparent
        Color{56, 120, 48},       // 1 grass dark
        Color{96, 168, 72},       // 2 grass light
        Color{198, 170, 110},     // 3 path
        Color{168, 140, 86},      // 4 path dark
        Color{40, 88, 160},       // 5 water dark
        Color{84, 150, 210},      // 6 water light
        Color{226, 214, 170},     // 7 sand
        Color{110, 74, 40},       // 8 trunk
        Color{30, 92, 46},        // 9 leaf dark
        Color{66, 150, 78},       // 10 leaf light (a)
        Color{206, 198, 178},     // 11 wall (b)
        Color{176, 66, 66},       // 12 roof (c)
        Color{240, 224, 96},      // 13 flower (d)
        Color{36, 40, 44},        // 14 outline (e)
        Color{245, 245, 245},     // 15 white (f)
    });

    bgTiles_.resize(BT_Count);
    bgTiles_[Grass0] = parseTile({"11111111","12111121","11111111","11211111","11111211","21111112","11111111","11112111"});
    bgTiles_[Grass1] = parseTile({"11111111","11121111","12111211","11111111","11211121","11111111","21111112","11121111"});
    bgTiles_[PathT]  = parseTile({"33333333","33433433","33333333","34333343","33333333","33433433","33333333","43333334"});
    bgTiles_[WaterT] = parseTile({"55665566","56655665","66556655","65566556","55665566","56655665","66556655","65566556"});
    bgTiles_[Water2T]= parseTile({"66556655","65566556","55665566","56655665","66556655","65566556","55665566","56655665"});
    bgTiles_[FlowerT]= parseTile({"11111111","1d1121d1","111d1111","11111d11","1d111111","1111d111","11d11111","1111111d"});
    bgTiles_[SandT]  = parseTile({"77777777","77677767","77777777","76777677","77777777","77677767","77777777","67777776"});
    bgTiles_[TrunkT] = parseTile({"11888811","18888881","18888881","18888881","18888881","18888881","18888881","11888811"});
    bgTiles_[CanopyT]= parseTile({"ee9aa9ee","e9aaaa9e","9aaaaaa9","aaa99aaa","9aaaaaa9","e9aaaa9e","ee9aa9ee","eee99eee"});
    bgTiles_[WallT]  = parseTile({"bbbbbbbb","bebbbbeb","bbbbbbbb","bbbbbbbb","bebbbbeb","bbbbbbbb","bbbbbbbb","eeeeeeee"});
    bgTiles_[RoofT]  = parseTile({"ffffffff","cccccccc","cccccccc","cccccccc","cccccccc","cccccccc","cccccccc","cccccccc"});
    bgTiles_[DoorT]  = parseTile({"bbbbbbbb","bb8888bb","b888888b","b888888b","b88ff88b","b888888b","b888888b","b888888b"});
    bgTiles_[EmptyT] = parseTile({"........","........","........","........","........","........","........","........"});

    playerPalette_ = Palette{
        Color{0, 0, 0, 0}, Color{28, 28, 44}, Color{232, 200, 160}, Color{96, 56, 32},
        Color{208, 64, 64}, Color{48, 72, 168}, Color{32, 32, 32}, Color{245, 245, 245}};
    followerPalette_ = Palette{
        Color{0, 0, 0, 0}, Color{24, 40, 24}, Color{96, 184, 96}, Color{64, 150, 72},
        Color{200, 240, 200}, Color{245, 245, 245}, Color{20, 20, 20}, Color{240, 150, 150}};

    // Player: 4 facings x 2 phases, each sliced into 4 tiles (32 tiles total).
    playerTiles_.clear();
    for (int facing = 0; facing < 4; ++facing)
        for (int phase = 0; phase < 2; ++phase) {
            auto tiles = sliceToTiles(makeChibi(facing, phase));
            playerTiles_.insert(playerTiles_.end(), tiles.begin(), tiles.end());
        }
    followerTiles_.clear();
    for (int facing = 0; facing < 4; ++facing)
        for (int phase = 0; phase < 2; ++phase) {
            auto tiles = sliceToTiles(makeCritter(facing, phase));
            followerTiles_.insert(followerTiles_.end(), tiles.begin(), tiles.end());
        }

    // UI palette + tiles for the bottom screen.
    uiPalettes_.assign(1, Palette{
        Color{0, 0, 0, 0}, Color{40, 48, 72}, Color{72, 88, 132}, Color{200, 210, 240},
        Color{230, 235, 255}, Color{120, 180, 255}, Color{20, 24, 36}, Color{245, 245, 245}});
    uiTiles_.clear();
    uiTiles_.push_back(parseTile({"22222222","21212121","22222222","22122212","22222222","21212121","22222222","22122212"})); // 0 fill
    uiTiles_.push_back(parseTile({"33333333","36666663","36222263","36222263","36222263","36222263","36666663","33333333"})); // 1 border
    uiTiles_.push_back(parseTile({"22222222","24444442","22222222","24444442","22222222","24444442","22222222","22222222"})); // 2 text lines
    uiTiles_.push_back(parseTile({"........","........","........","........","........","........","........","........"})); // 3 empty
}

void SyntheticDsBackend::reset() {
    frame_ = 0;
    input_ = InputState{};
    playerX_ = 24 * kTileSize;
    playerY_ = 22 * kTileSize;
    facing_ = 0;
    moving_ = false;
    history_.clear();
    for (int i = 0; i < 64; ++i) history_.push_back({playerX_, playerY_, facing_});
}

void SyntheticDsBackend::setInput(const InputState& in) { input_ = in; }

void SyntheticDsBackend::advanceFrame() {
    int dx = 0, dy = 0;
    bool userDrove = input_.up || input_.down || input_.left || input_.right;
    if (userDrove) {
        if (input_.left) { dx = -1; facing_ = 2; }
        else if (input_.right) { dx = 1; facing_ = 3; }
        else if (input_.up) { dy = -1; facing_ = 1; }
        else if (input_.down) { dy = 1; facing_ = 0; }
    } else {
        // Deterministic scripted loop so headless captures show motion.
        int phase = (int)(frame_ % 320);
        if (phase < 96) { dx = 1; facing_ = 3; }
        else if (phase < 160) { dy = 1; facing_ = 0; }
        else if (phase < 256) { dx = -1; facing_ = 2; }
        else { dy = -1; facing_ = 1; }
    }
    moving_ = (dx != 0 || dy != 0);
    playerX_ = clampi(playerX_ + dx, 3 * kTileSize, (kMapW - 4) * kTileSize);
    playerY_ = clampi(playerY_ + dy, 3 * kTileSize, (kMapH - 4) * kTileSize);
    history_.push_back({playerX_, playerY_, facing_});
    if (history_.size() > 64) history_.pop_front();
    ++frame_;
}

// ---- World authoring ------------------------------------------------------
static bool isTree(int tx, int ty) {
    if (tx < 4 || ty < 4 || tx >= 60 || ty >= 60) return false;
    if (ty >= 18 && ty <= 21) return false;          // clear the road
    if (tx >= 22 && tx <= 25) return false;
    if (tx >= 39 && tx <= 49 && ty >= 7 && ty <= 15) return false;  // pond area
    if (tx >= 49 && tx <= 56 && ty >= 39 && ty <= 45) return false; // building
    return ((tx * 13 + ty * 7) % 23) == 0;
}

SyntheticDsBackend::Cell SyntheticDsBackend::worldCell(int tx, int ty) const {
    if (tx < 2 || ty < 2 || tx >= kMapW - 2 || ty >= kMapH - 2) return Cell::Water;  // moat
    // Building (walls lower row + door).
    if (tx >= 50 && tx <= 55 && ty == 44) return (tx == 52) ? Cell::DoorMat : Cell::WallLower;
    // Pond.
    if (tx >= 40 && tx <= 48 && ty >= 8 && ty <= 14) return Cell::Water;
    // Roads.
    if ((ty == 19 || ty == 20) || (tx == 23 || tx == 24)) return Cell::Path;
    if (isTree(tx, ty)) return Cell::TreeTrunk;
    if (((tx * ty) % 17) == 0) return Cell::Flower;
    return Cell::Grass;
}

bool SyntheticDsBackend::hasCanopy(int tx, int ty) const {
    return isTree(tx, ty) || isTree(tx, ty + 1);  // canopy over trunk and one above
}

bool SyntheticDsBackend::hasRoof(int tx, int ty) const {
    return tx >= 50 && tx <= 55 && ty >= 40 && ty <= 43;
}

// ---- Frame construction ---------------------------------------------------
StructuredFrame SyntheticDsBackend::topScreen() const {
    StructuredFrame f;
    f.screenWidth = kScreenW;
    f.screenHeight = kScreenH;
    f.backdrop = Color{20, 30, 40};
    f.frameIndex = frame_;

    int camX = clampi(playerX_ - kScreenW / 2 + 8, 0, kMapW * kTileSize - kScreenW);
    int camY = clampi(playerY_ - kScreenH / 2 + 8, 0, kMapH * kTileSize - kScreenH);

    auto waterAnim = (frame_ / 16) & 1;

    // Ground layer (BG3, priority 0).
    BackgroundLayer ground;
    ground.index = 3;
    ground.priority = 0;
    ground.widthTiles = kMapW;
    ground.heightTiles = kMapH;
    ground.scrollX = camX;
    ground.scrollY = camY;
    ground.tileset = bgTiles_;
    ground.palettes = bgPalettes_;
    ground.map.resize((size_t)kMapW * kMapH);
    // Overhead layer (BG1, priority 2) — occludes sprites (walk-behind).
    BackgroundLayer overhead = ground;
    overhead.index = 1;
    overhead.priority = 2;
    overhead.map.assign((size_t)kMapW * kMapH, TileRef{EmptyT, 0, false, false, 2});

    for (int ty = 0; ty < kMapH; ++ty) {
        for (int tx = 0; tx < kMapW; ++tx) {
            size_t i = (size_t)ty * kMapW + tx;
            uint16_t g = Grass0;
            switch (worldCell(tx, ty)) {
                case Cell::Grass: g = ((tx + ty) & 1) ? Grass1 : Grass0; break;
                case Cell::Path: g = PathT; break;
                case Cell::Water: g = (((tx + ty) & 1) ^ waterAnim) ? Water2T : WaterT; break;
                case Cell::Flower: g = FlowerT; break;
                case Cell::Sand: g = SandT; break;
                case Cell::TreeTrunk: g = TrunkT; break;
                case Cell::WallLower: g = WallT; break;
                case Cell::DoorMat: g = DoorT; break;
            }
            ground.map[i] = TileRef{g, 0, false, false, 0};
            if (hasCanopy(tx, ty)) overhead.map[i] = TileRef{CanopyT, 0, false, false, 2};
            else if (hasRoof(tx, ty)) overhead.map[i] = TileRef{RoofT, 0, false, false, 2};
        }
    }
    f.backgrounds.push_back(std::move(ground));
    f.backgrounds.push_back(std::move(overhead));

    // Player sprite (priority 1).
    int walkPhase = moving_ ? (int)((frame_ / 8) & 1) : 0;
    {
        Sprite pl;
        pl.id = 1;
        pl.x = playerX_ - camX;
        pl.y = playerY_ - camY;
        pl.width = 16; pl.height = 16;
        pl.priority = 1;
        pl.palette = playerPalette_;
        int base = (facing_ * 2 + walkPhase) * 4;
        pl.tiles.assign(playerTiles_.begin() + base, playerTiles_.begin() + base + 4);
        f.sprites.push_back(std::move(pl));
    }
    // Follower critter, trailing via history.
    {
        const auto& h = history_[history_.size() > 20 ? history_.size() - 20 : 0];
        int fFacing = h[2];
        int fPhase = moving_ ? (int)((frame_ / 8) & 1) : 0;
        Sprite fo;
        fo.id = 2;
        fo.x = h[0] - camX;
        fo.y = h[1] - camY;
        fo.width = 16; fo.height = 16;
        fo.priority = 1;
        fo.palette = followerPalette_;
        int base = (fFacing * 2 + fPhase) * 4;
        fo.tiles.assign(followerTiles_.begin() + base, followerTiles_.begin() + base + 4);
        f.sprites.push_back(std::move(fo));
    }
    return f;
}

StructuredFrame SyntheticDsBackend::bottomScreen() const {
    StructuredFrame f;
    f.screenWidth = kScreenW;
    f.screenHeight = kScreenH;
    f.backdrop = Color{28, 34, 52};
    f.frameIndex = frame_;

    BackgroundLayer ui;
    ui.index = 0;
    ui.priority = 0;
    ui.widthTiles = kScreenW / kTileSize;   // 32
    ui.heightTiles = kScreenH / kTileSize;  // 24
    ui.scrollX = 0; ui.scrollY = 0;
    ui.tileset = uiTiles_;
    ui.palettes = uiPalettes_;
    ui.map.resize((size_t)ui.widthTiles * ui.heightTiles);
    for (int ty = 0; ty < ui.heightTiles; ++ty) {
        for (int tx = 0; tx < ui.widthTiles; ++tx) {
            size_t i = (size_t)ty * ui.widthTiles + tx;
            uint16_t t = 0;  // fill
            bool ring = (tx == 1 || tx == ui.widthTiles - 2 || ty == 1 || ty == ui.heightTiles - 2);
            bool outer = (tx == 0 || tx == ui.widthTiles - 1 || ty == 0 || ty == ui.heightTiles - 1);
            if (outer) t = 3;             // empty margin
            else if (ring) t = 1;         // border
            else if ((ty % 3) == 0 && tx > 2 && tx < ui.widthTiles - 3) t = 2;  // text rows
            else t = 0;                   // fill
            ui.map[i] = TileRef{t, 0, false, false, 0};
        }
    }
    f.backgrounds.push_back(std::move(ui));

    // Menu cursor sprite that steps through rows.
    {
        static const Palette cursorPal{
            Color{0, 0, 0, 0}, Color{20, 24, 36}, Color{245, 245, 245}, Color{120, 180, 255}};
        Sprite cur;
        cur.id = 1;
        cur.x = 24;
        cur.y = 24 + (int)((frame_ / 48) % 4) * 24;
        cur.width = 16; cur.height = 16;
        cur.priority = 1;
        cur.palette = cursorPal;
        cur.tiles = sliceToTiles(makeCursor());
        f.sprites.push_back(std::move(cur));
    }
    // Touch marker.
    if (input_.touchActive) {
        static const Palette touchPal{
            Color{0, 0, 0, 0}, Color{120, 180, 255}, Color{245, 245, 245}, Color{60, 120, 220}};
        Canvas ring(16, 16);
        ring.fillCircle(8, 8, 6, 1);
        ring.fillCircle(8, 8, 4, 0);
        Sprite tm;
        tm.id = 2;
        tm.x = clampi(input_.touchX - 8, 0, kScreenW - 16);
        tm.y = clampi(input_.touchY - 8, 0, kScreenH - 16);
        tm.width = 16; tm.height = 16;
        tm.priority = 2;
        tm.palette = touchPal;
        tm.tiles = sliceToTiles(ring);
        f.sprites.push_back(std::move(tm));
    }
    return f;
}

StructuredFrame SyntheticDsBackend::structuredFrame(int screen) const {
    return screen == 1 ? bottomScreen() : topScreen();
}

Image SyntheticDsBackend::framebuffer(int screen) const {
    return compositeNative(structuredFrame(screen));
}

AdapterInfo SyntheticDsBackend::info() const {
    AdapterInfo a;
    a.backendName = "Synthetic DS (PRISMATIC fixture)";
    a.apiVersion = kAdapterApiVersion;
    a.system = System::Synthetic;
    a.compatibility = CompatibilityLevel::Level3_Full;
    return a;
}

GameIdentity SyntheticDsBackend::identity() const {
    GameIdentity g;
    g.system = System::Synthetic;
    g.title = "PRISMATIC Synthetic Overworld";
    g.gameCode = "SYNTH";
    g.romLoaded = false;
    return g;
}

Capabilities SyntheticDsBackend::capabilities() const {
    Capabilities c;
    c.add(Cap_Framebuffer); c.add(Cap_Backgrounds); c.add(Cap_Sprites);
    c.add(Cap_Palettes); c.add(Cap_Priorities); c.add(Cap_Scroll);
    c.add(Cap_DualScreen); c.add(Cap_Touch); c.add(Cap_TileFlip);
    c.add(Cap_PerTilePalette);
    return c;
}

ScreenRouting SyntheticDsBackend::screenRouting() const {
    ScreenRouting r;
    r.screenCount = 2;
    r.topOnPrimary = true;
    r.allowManualSwap = true;
    return r;
}

std::unique_ptr<EmulatorAdapter> makeSyntheticBackend() {
    return std::make_unique<SyntheticDsBackend>();
}

}  // namespace prismatic
