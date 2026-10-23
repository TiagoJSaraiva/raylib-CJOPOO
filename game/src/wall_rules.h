#pragma once

#include "room.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace WallRules {

enum class Section : std::uint8_t {
    Side,
    Upper,
    Bottom
};

enum class WallType : std::uint8_t {
    Standard,
    Edge
};

enum class SidePlacement : std::uint8_t {
    Edge,
    InsetLeft,
    InsetRight
};

enum class BottomWallRowKind : std::uint8_t {
    None,
    Room,
    Corridor
};

struct BottomWallRowId {
    BottomWallRowKind kind{BottomWallRowKind::None};
    int ownerX{0};
    int ownerY{0};
    int ownerWidth{0};
    int ownerHeight{0};
    int edgeY{0};

    bool operator==(const BottomWallRowId& other) const noexcept {
        return kind == other.kind && ownerX == other.ownerX && ownerY == other.ownerY &&
               ownerWidth == other.ownerWidth && ownerHeight == other.ownerHeight &&
               edgeY == other.edgeY;
    }
};

struct Position {
    int x;
    int y;

    bool operator==(const Position& other) const noexcept {
        return x == other.x && y == other.y;
    }
};

struct RoomView {
    const Room* room;
    float visibility;
};

enum class HostKind : std::uint8_t { None, Room, Corridor };

// A corridor is identified by its world-space footprint, independent of its two doors.
struct HostId {
    HostKind kind{HostKind::None};
    TileRect bounds{};
    RoomCoords room{};
    bool operator==(const HostId& other) const noexcept {
        return kind == other.kind && room == other.room && bounds.x == other.bounds.x &&
               bounds.y == other.bounds.y && bounds.width == other.bounds.width &&
               bounds.height == other.bounds.height;
    }
};

struct Segment {
    Position position;       // Wall anchor tile; bottom sprites draw one tile above it.
    Position edgeTile;       // Walkable tile that produced this edge.
    Section section;
    BiomeType biome;
    float visibility;
    SidePlacement sidePlacement{SidePlacement::Edge};
    WallType wallType{WallType::Standard};
    BottomWallRowId bottomWallRow{};
    HostId host{};
};

struct Module {
    Segment segment;          // First tile in this module's horizontal or vertical span.
    int widthTiles{1};
    int heightTiles{1};
};

struct Key {
    Position position;
    Section section;
    SidePlacement sidePlacement{SidePlacement::Edge};

    bool operator==(const Key& other) const noexcept {
        return position == other.position && section == other.section &&
               sidePlacement == other.sidePlacement;
    }
};

struct KeyHash {
    std::size_t operator()(const Key& key) const noexcept;
};

struct BottomWallRowIdHash {
    std::size_t operator()(const BottomWallRowId& id) const noexcept;
};

using Frame = std::unordered_map<Key, Segment, KeyHash>;

// Builds the union of visible room and open-corridor geometry, then emits its exposed walls.
Frame BuildFrame(const std::vector<RoomView>& rooms);

// Groups compatible horizontal edges into 2x2 walls and regular side runs into 1x2 strips.
// All side modules, including inset caps, span two tiles vertically.
std::vector<Module> BuildModules(const Frame& frame);
Rectangle ModuleBounds(const Module& module);

constexpr std::size_t NoSprite = std::numeric_limits<std::size_t>::max();

struct Choice {
    BiomeType biome;
    WallType wallType{WallType::Standard};
    std::size_t sprite;
};

class TileMemory {
public:
    void Reset(std::uint64_t seed);
    const Choice& GetOrCreate(Position position, Section section, BiomeType biome,
                              const std::vector<std::uint64_t>& cumulative,
                              SidePlacement sidePlacement = SidePlacement::Edge,
                              WallType wallType = WallType::Standard);

private:
    std::uint64_t seed_{0};
    std::unordered_map<Key, Choice, KeyHash> choices_;
};

} // namespace WallRules
