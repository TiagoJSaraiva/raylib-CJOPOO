#pragma once

#include "floor_rules.h"
#include "raylib.h"
#include "room.h"

#include <array>
#include <vector>

struct FloorRoomView {
    const Room* room;
    float visibility;
};

// Owns the immutable texture catalog and the choices for the current run.
class FloorTiles {
public:
    FloorTiles();
    ~FloorTiles();
    FloorTiles(const FloorTiles&) = delete;
    FloorTiles& operator=(const FloorTiles&) = delete;

    void BeginRun(std::uint64_t seed);
    // Include hidden rooms (visibility zero) to resolve both ends of corridors.
    void Draw(const std::vector<FloorRoomView>& rooms) const;
    void Unload();

private:
    struct Catalog {
        std::vector<Texture2D> textures;
        std::vector<std::uint64_t> cumulative;
    };
    std::array<Catalog, 5> catalogs_{};
    mutable FloorRules::TileMemory memory_;
};
