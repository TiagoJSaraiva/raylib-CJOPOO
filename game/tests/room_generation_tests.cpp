#include "room_manager.h"
#include "biome_config.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

namespace {

void CheckGeneratedWorld(std::uint64_t seed) {
    RoomManager manager(seed);
    assert(manager.Rooms().size() > 1);

    const Room& lobby = manager.GetRoom(RoomCoords{0, 0});
    const Doorway* lobbyNorth = lobby.FindDoor(Direction::North);
    assert(lobbyNorth != nullptr);
    assert(lobbyNorth->offset == 4);

    BiomeType selectedBiome = BiomeType::Unknown;
    for (const auto& [coords, roomPointer] : manager.Rooms()) {
        (void)coords;
        const Room& room = *roomPointer;
        if (room.GetType() == RoomType::Lobby) {
            assert(room.GetBiome() == BiomeType::Lobby);
        } else {
            assert(room.GetBiome() == BiomeType::Cave ||
                   room.GetBiome() == BiomeType::Mansion ||
                   room.GetBiome() == BiomeType::Dungeon);
            if (selectedBiome == BiomeType::Unknown) {
                selectedBiome = room.GetBiome();
            } else {
                assert(room.GetBiome() == selectedBiome);
            }
        }

        const RoomLayout& layout = room.Layout();
        assert(layout.widthTiles > 0 && layout.widthTiles % 2 == 0);
        assert(layout.heightTiles > 0 && layout.heightTiles % 2 == 0);

        for (const Doorway& door : layout.doors) {
            assert(door.offset >= 0 && door.offset % 2 == 0);

            const TileRect& corridor = door.corridorTiles;
            if (corridor.width <= 0 || corridor.height <= 0) continue;
            if (door.corridorLength < 4 || door.corridorLength > 6 ||
                door.corridorLength % 2 != 0) {
                std::cerr << "Unexpected corridor for seed " << seed << " at room ("
                          << room.GetCoords().x << ", " << room.GetCoords().y << "): direction="
                          << static_cast<int>(door.direction) << " length=" << door.corridorLength
                          << " bounds=" << corridor.width << "x" << corridor.height << '\n';
            }
            assert(door.corridorLength >= 4 && door.corridorLength <= 6);
            assert(door.corridorLength % 2 == 0);
            if (door.direction == Direction::North || door.direction == Direction::South) {
                assert(corridor.width == door.width);
                assert(corridor.height % 2 == 0);
            } else {
                assert(corridor.height == door.width);
                assert(corridor.width % 2 == 0);
            }
        }
    }
}

void CheckForcedBiome(std::uint64_t seed, BiomeType forcedBiome) {
    RoomManager manager(seed, forcedBiome);
    assert(manager.GetRoom(RoomCoords{0, 0}).GetBiome() == BiomeType::Lobby);
    assert(manager.Rooms().size() > 1);

    for (const auto& [coords, roomPointer] : manager.Rooms()) {
        (void)coords;
        const Room& room = *roomPointer;
        if (room.GetType() == RoomType::Lobby) continue;
        assert(room.GetBiome() == forcedBiome);
    }
}

void CheckBiomeConfigParsing() {
    assert(BiomeConfig::ParseBiomeOverride("cave") == BiomeType::Cave);
    assert(BiomeConfig::ParseBiomeOverride("  MANSION \n") == BiomeType::Mansion);
    assert(BiomeConfig::ParseBiomeOverride("\tDungeon  ") == BiomeType::Dungeon);
    assert(!BiomeConfig::ParseBiomeOverride(""));
    assert(!BiomeConfig::ParseBiomeOverride("   \n\t"));
    assert(!BiomeConfig::ParseBiomeOverride("lobby"));
    assert(!BiomeConfig::ParseBiomeOverride("unknown-biome"));
    assert(!BiomeConfig::LoadBiomeOverride("missing-biome-config-for-test.txt"));
}

} // namespace

int main() {
    CheckBiomeConfigParsing();
    for (std::uint64_t seed = 1; seed <= 16; ++seed) CheckGeneratedWorld(seed);
    CheckForcedBiome(1234, BiomeType::Dungeon);
    CheckForcedBiome(5678, BiomeType::Cave);
    CheckForcedBiome(9012, BiomeType::Mansion);
    std::cout << "Room generation tests passed\n";
}
