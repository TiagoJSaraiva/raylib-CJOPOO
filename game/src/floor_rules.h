#pragma once

#include "room_types.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace FloorRules {

struct Metadata {
    std::uint64_t id;
    std::uint64_t weight;
};

std::optional<Metadata> ParseFilename(const std::string& filename);
bool AddWeight(std::vector<std::uint64_t>& cumulative, std::uint64_t weight);
std::size_t SelectTicket(const std::vector<std::uint64_t>& cumulative, std::uint64_t ticket);
bool IsSquareImage(int width, int height);

struct Position {
    int x;
    int y;
    bool operator==(const Position& other) const { return x == other.x && y == other.y; }
};

struct PositionHash {
    std::size_t operator()(const Position& position) const noexcept;
};

BiomeType CorridorBiome(RoomCoords a, BiomeType biomeA, RoomCoords b, BiomeType biomeB);

struct VisibleTile {
    BiomeType biome;
    float visibility;
};

// A frame contains the union of visible room/corridor rectangles.
using Frame = std::unordered_map<Position, VisibleTile, PositionHash>;
void AddRect(Frame& frame, const TileRect& rect, BiomeType biome, float visibility);

constexpr std::size_t NoSprite = std::numeric_limits<std::size_t>::max();
struct Choice {
    BiomeType biome;
    std::size_t sprite;
};

class TileMemory {
public:
    void Reset(std::uint64_t seed);
    const Choice& GetOrCreate(Position position, BiomeType biome,
                              const std::vector<std::uint64_t>& cumulative);
    std::size_t Size() const { return tiles_.size(); }

private:
    std::uint64_t seed_{0};
    std::unordered_map<Position, Choice, PositionHash> tiles_;
};

} // namespace FloorRules
