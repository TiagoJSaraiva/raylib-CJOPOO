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

void CheckWallPropRegistry() {
    RoomManager manager(1234, BiomeType::Dungeon);
    assert(!manager.WallProps().empty());
    const auto before = manager.WallProps();
    const auto revision = manager.GetLayoutRevision();
    const auto* storage = manager.WallProps().data();
    assert(revision == manager.GetWallPropsRevision());
    for (int frame = 0; frame < 120; ++frame) {
        manager.EnsureNeighborsGenerated(manager.GetCurrentCoords());
        assert(manager.GetLayoutRevision() == revision);
        assert(manager.GetWallPropsRevision() == revision);
        assert(manager.WallProps().data() == storage);
    }
    // Door animation, lock state, and visits affect exposure, not structural placement.
    for (const auto& entry : manager.Rooms()) {
        entry.second->SetVisited(true);
        for (auto& door : entry.second->Layout().doors) if (door.doorState) {
            door.doorState->opening = true;
            door.doorState->open = true;
            door.doorState->fadeProgress = 0.5f;
            door.doorState->interactionState = DoorInteractionState::Locked;
        }
    }
    manager.EnsureNeighborsGenerated(manager.GetCurrentCoords());
    assert(manager.GetWallPropsRevision() == revision && manager.WallProps().data() == storage);
    const auto& after = manager.WallProps();
    assert(before.size() == after.size());
    for (std::size_t i = 0; i < before.size(); ++i) {
        assert(before[i].key == after[i].key);
        assert(before[i].animationPhase == after[i].animationPhase);
    }
    RoomManager replay(1234, BiomeType::Dungeon);
    assert(before.size() == replay.WallProps().size());
    for (std::size_t i = 0; i < before.size(); ++i)
        assert(before[i].key == replay.WallProps()[i].key);

    // Expanding the map configures new rooms/doors and must refresh the registry.
    const auto roomCount = manager.Rooms().size();
    manager.EnsureNeighborsGenerated(manager.GetCurrentCoords(), 3);
    assert(manager.Rooms().size() > roomCount);
    assert(manager.GetLayoutRevision() > revision);
    assert(manager.GetWallPropsRevision() == manager.GetLayoutRevision());
    std::vector<WallRules::RoomView> views;
    for (const auto& entry : manager.Rooms()) views.push_back({entry.second.get(), 1});
    const auto expected = WallPropRules::Generate(views, manager.GetWorldSeed());
    assert(expected.size() == manager.WallProps().size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        assert(expected[i].key == manager.WallProps()[i].key);
        assert(expected[i].animationPhase == manager.WallProps()[i].animationPhase);
    }
    const auto expandedRevision = manager.GetWallPropsRevision();
    storage = manager.WallProps().data();
    manager.EnsureNeighborsGenerated(manager.GetCurrentCoords(), 3);
    assert(manager.GetWallPropsRevision() == expandedRevision && manager.WallProps().data() == storage);
    manager = RoomManager(5678, BiomeType::Cave);
    assert(manager.WallProps().empty());
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
    CheckWallPropRegistry();
    for (std::uint64_t seed = 1; seed <= 16; ++seed) CheckGeneratedWorld(seed);
    CheckForcedBiome(1234, BiomeType::Dungeon);
    CheckForcedBiome(5678, BiomeType::Cave);
    CheckForcedBiome(9012, BiomeType::Mansion);
    std::cout << "Room generation tests passed\n";
}
