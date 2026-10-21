#include "room_collision_rules.h"

#include <cassert>
#include <iostream>

int main() {
    const TileRect tiles{3, 5, 8, 6};
    const BiomeType biomes[] = {
        BiomeType::Lobby,
        BiomeType::Cave,
        BiomeType::Mansion,
        BiomeType::Dungeon
    };

    for (BiomeType biome : biomes) {
        const int inset = RoomCollisionRules::BottomInsetPixels(biome);
        const Rectangle bounds = RoomCollisionRules::RoomBoundsForCollision(tiles, biome);
        assert(inset > 0);
        assert(bounds.x == 3.0f * TILE_SIZE);
        assert(bounds.y == 5.0f * TILE_SIZE);
        assert(bounds.width == 8.0f * TILE_SIZE);
        assert(bounds.height == 6.0f * TILE_SIZE - inset);
        assert(bounds.y + bounds.height == 11.0f * TILE_SIZE - inset);
    }

    const Rectangle shortRoom = RoomCollisionRules::RoomBoundsForCollision(
        TileRect{0, 0, 1, 0}, BiomeType::Lobby);
    assert(shortRoom.height == 0.0f);

    std::cout << "Room collision rule tests passed.\n";
    return 0;
}
