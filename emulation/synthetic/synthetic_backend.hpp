// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — synthetic DS-like backend.
//
// IMPORTANT: This backend contains ONLY first-party, hand-authored placeholder
// pixel art. It does not contain, reproduce, or approximate any copyrighted
// game asset. Real game graphics are only ever obtained at runtime from the
// user's own ROM through a real emulator backend. This synthetic backend exists
// so the entire capture -> reconstruct -> enhance -> render pipeline can be
// exercised and tested deterministically with no ROM.
//
// It emulates a DS-shaped device: two 256x192 screens. The top screen is a
// scrolling overworld with a player sprite and a trailing "follower" critter
// (demonstrating OAM sprites, priorities, and tile occlusion). The bottom
// screen is a simple touch UI.
#pragma once
#include <memory>
#include <array>
#include <vector>
#include <deque>
#include "prismatic/adapter.hpp"

namespace prismatic {

class SyntheticDsBackend : public EmulatorAdapter {
public:
    static constexpr int kScreenW = 256;
    static constexpr int kScreenH = 192;
    static constexpr int kMapW = 64;  // world size in tiles
    static constexpr int kMapH = 64;

    SyntheticDsBackend();

    AdapterInfo info() const override;
    GameIdentity identity() const override;
    Capabilities capabilities() const override;
    ScreenRouting screenRouting() const override;
    int screenCount() const override { return 2; }

    void reset() override;
    void setInput(const InputState& in) override;
    void advanceFrame() override;
    uint64_t frameIndex() const override { return frame_; }

    Image framebuffer(int screen) const override;
    StructuredFrame structuredFrame(int screen) const override;

private:
    void buildAssets();
    StructuredFrame topScreen() const;
    StructuredFrame bottomScreen() const;

    // World-cell classification used to author both ground and overhead layers.
    enum class Cell : uint8_t { Grass, Path, Water, Flower, Sand, TreeTrunk, WallLower, DoorMat };
    Cell worldCell(int tx, int ty) const;
    bool hasCanopy(int tx, int ty) const;   // overhead layer occupancy
    bool hasRoof(int tx, int ty) const;

    // Assets (built once).
    std::vector<Tile> bgTiles_;      // shared background tileset
    std::vector<Palette> bgPalettes_;
    std::vector<Tile> playerTiles_;  // 4 dirs x 2 frames, each 2x2 tiles
    Palette playerPalette_;
    std::vector<Tile> followerTiles_;
    Palette followerPalette_;
    std::vector<Tile> uiTiles_;
    std::vector<Palette> uiPalettes_;

    // Runtime state.
    uint64_t frame_ = 0;
    InputState input_{};
    int playerX_ = 24 * kTileSize;  // world pixel position
    int playerY_ = 20 * kTileSize;
    int facing_ = 0;                // 0=down 1=up 2=left 3=right
    bool moving_ = false;
    std::deque<std::array<int, 3>> history_;  // {x,y,facing} for follower trail
};

std::unique_ptr<EmulatorAdapter> makeSyntheticBackend();

}  // namespace prismatic
