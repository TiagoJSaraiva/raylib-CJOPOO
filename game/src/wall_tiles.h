#pragma once

#include "wall_rules.h"

#include <array>
#include <vector>

// Owns the wall texture catalogs and stable per-run choices.
class WallTiles {
public:
    WallTiles();
    ~WallTiles();
    WallTiles(const WallTiles&) = delete;
    WallTiles& operator=(const WallTiles&) = delete;

    void BeginRun(std::uint64_t seed);
    void DrawBackground(const std::vector<WallRules::RoomView>& rooms) const;
    void DrawForeground(const std::vector<WallRules::RoomView>& rooms) const;
    void Unload();

private:
    struct Catalog {
        std::vector<Texture2D> textures;
        std::vector<std::uint64_t> cumulative;
    };

    void Draw(const WallRules::Frame& frame, bool foreground) const;
    void DrawFallback(const WallRules::Module& module) const;

    std::array<Catalog, 12> catalogs_{};
    mutable WallRules::TileMemory memory_;
};
