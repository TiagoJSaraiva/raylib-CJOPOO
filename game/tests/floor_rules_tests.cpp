#include "floor_rules.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <string>

using namespace FloorRules;

int main() {
    const auto metadata = ParseFilename("12-5.png");
    assert(metadata && metadata->id == 12 && metadata->weight == 5);
    assert(ParseFilename("1-2.png") && ParseFilename("1-5.png")); // IDs are labels, not unique keys.
    assert(ParseFilename("001-02.png"));
    assert(ParseFilename("18446744073709551615-18446744073709551615.png"));
    for (const std::string invalid : {"", "1.png", "1-2.jpg", "1-2.PNG", "a-2.png", "1-b.png",
                                     "0-2.png", "1-0.png", "-1-2.png", "1--2.png", "1-2-3.png",
                                     "1-0.5.png", "1-2 .png", " 1-2.png", "+1-2.png",
                                     "18446744073709551616-1.png", "1-18446744073709551616.png"}) {
        assert(!ParseFilename(invalid));
    }
    assert(IsSquareImage(64, 64));
    assert(IsSquareImage(256, 256));
    assert(IsSquareImage(32, 32));
    assert(!IsSquareImage(64, 128));
    assert(!IsSquareImage(0, 0));
    assert(!IsSquareImage(-1, -1));

    std::vector<std::uint64_t> weights;
    assert(!AddWeight(weights, 0) && weights.empty());
    assert(AddWeight(weights, 2));
    assert(AddWeight(weights, 5));
    assert(AddWeight(weights, 2));
    for (std::uint64_t ticket = 0; ticket < 9; ++ticket) {
        assert(SelectTicket(weights, ticket) == (ticket < 2 ? 0u : ticket < 7 ? 1u : 2u));
    }
    assert(SelectTicket(weights, 9) == NoSprite);
    assert(SelectTicket({}, 0) == NoSprite);
    std::vector<std::uint64_t> maximum;
    assert(AddWeight(maximum, std::numeric_limits<std::uint64_t>::max()));
    assert(!AddWeight(maximum, 1));
    assert(maximum.size() == 1);
    assert(SelectTicket(maximum, std::numeric_limits<std::uint64_t>::max() - 1) == 0);

    TileMemory first, reversed;
    first.Reset(42);
    reversed.Reset(42);
    for (int x = -20; x <= 20; ++x) first.GetOrCreate({x, -x}, BiomeType::Cave, weights);
    for (int x = 20; x >= -20; --x) {
        const auto a = first.GetOrCreate({x, -x}, BiomeType::Cave, weights);
        const auto b = reversed.GetOrCreate({x, -x}, BiomeType::Cave, weights);
        assert(a.biome == b.biome && a.sprite == b.sprite && a.sprite < 3);
    }
    assert(first.Size() == 41);
    const auto original = first.GetOrCreate({-3, 3}, BiomeType::Cave, weights);
    // Revisits reuse the recorded choice, even if a caller supplies another catalog.
    assert(first.GetOrCreate({-3, 3}, BiomeType::Mansion, {}).sprite == original.sprite);
    assert(first.GetOrCreate({-3, 3}, BiomeType::Mansion, {}).biome == BiomeType::Cave);
    first.Reset(42);
    assert(first.Size() == 0);
    assert(first.GetOrCreate({-3, 3}, BiomeType::Cave, weights).sprite == original.sprite);
    first.Reset(43);
    bool differs = false;
    for (int x = -20; x <= 20; ++x) {
        differs |= first.GetOrCreate({x, -x}, BiomeType::Cave, weights).sprite !=
                   reversed.GetOrCreate({x, -x}, BiomeType::Cave, weights).sprite;
    }
    assert(differs);
    first.Reset(42);
    assert(first.GetOrCreate({0, 0}, BiomeType::Lobby, {}).sprite == NoSprite);
    assert(first.GetOrCreate({1, 0}, BiomeType::Dungeon, maximum).sprite == 0);

    const RoomCoords lobby{0, 0}, neighbor{0, -1};
    assert(CorridorBiome(lobby, BiomeType::Lobby, neighbor, BiomeType::Cave) == BiomeType::Cave);
    assert(CorridorBiome(neighbor, BiomeType::Cave, lobby, BiomeType::Lobby) == BiomeType::Cave);
    assert(CorridorBiome(lobby, BiomeType::Dungeon, neighbor, BiomeType::Mansion) == BiomeType::Mansion);
    assert(CorridorBiome(neighbor, BiomeType::Mansion, lobby, BiomeType::Dungeon) == BiomeType::Mansion);
    assert(CorridorBiome({-2, 9}, BiomeType::Cave, {-1, -9}, BiomeType::Dungeon) == BiomeType::Cave);
    assert(CorridorBiome(lobby, BiomeType::Cave, neighbor, BiomeType::Cave) == BiomeType::Cave);

    Frame frame;
    const TileRect corridor{-2, -4, 2, 4};
    AddRect(frame, corridor, BiomeType::Cave, 0.3f);
    AddRect(frame, corridor, BiomeType::Cave, 0.8f);
    assert(frame.size() == 8);
    for (const auto& entry : frame) assert(entry.second.visibility == 0.8f);
    Frame other;
    AddRect(other, corridor, BiomeType::Cave, 0.8f);
    AddRect(other, corridor, BiomeType::Cave, 0.3f);
    assert(other.size() == frame.size());
    for (const auto& [position, tile] : other) {
        assert(tile.visibility == frame.at(position).visibility);
        assert(tile.biome == frame.at(position).biome);
    }
    AddRect(frame, {0, 0, 12, 12}, BiomeType::Lobby, 0);
    AddRect(frame, {0, 0, 12, 12}, BiomeType::Lobby, -1);
    AddRect(frame, {0, 0, 0, 10}, BiomeType::Lobby, 1);
    assert(frame.size() == 8);
    AddRect(frame, {0, 0, 1, 1}, BiomeType::Lobby, 2);
    assert(frame.at(Position{0, 0}).visibility == 1);
    std::cout << "Floor rules tests passed\n";
}
