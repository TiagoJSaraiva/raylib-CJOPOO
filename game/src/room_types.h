#pragma once

#include <cstdint>
#include <functional>

constexpr int TILE_SIZE = 64;
constexpr int DOOR_WIDTH_TILES = 2;

enum class RoomType {
    Lobby,
    Normal,
    Shop,
    Forge,
    Chest,
    Boss,
    Puzzle,
    Unknown
};

enum class BiomeType {
    Lobby,
    Cave,
    Mansion,
    Dungeon,
    Unknown
};

enum class Direction {
    North,
    South,
    East,
    West
};

struct RoomCoords {
    int x{0};
    int y{0};

    RoomCoords() = default;
    RoomCoords(int px, int py) : x(px), y(py) {}

    bool operator==(const RoomCoords& other) const {
        return x == other.x && y == other.y;
    }

    RoomCoords operator+(const RoomCoords& other) const {
        return RoomCoords{x + other.x, y + other.y};
    }
};

struct RoomCoordsHash {
    std::size_t operator()(const RoomCoords& coords) const noexcept {
        std::size_t h1 = std::hash<int>{}(coords.x);
        std::size_t h2 = std::hash<int>{}(coords.y);
        return h1 ^ (h2 << 1);
    }
};

struct TileRect {
    int x{0};
    int y{0};
    int width{0};
    int height{0};
};

inline bool Intersects(const TileRect& a, const TileRect& b) {
    return !(a.x + a.width <= b.x || b.x + b.width <= a.x || a.y + a.height <= b.y || b.y + b.height <= a.y);
}

inline RoomCoords ToDirectionOffset(Direction direction) {
    switch (direction) {
        case Direction::North: return RoomCoords{0, -1};
        case Direction::South: return RoomCoords{0, 1};
        case Direction::East: return RoomCoords{1, 0};
        case Direction::West: return RoomCoords{-1, 0};
        default: return RoomCoords{0, 0};
    }
}

inline Direction Opposite(Direction direction) {
    switch (direction) {
        case Direction::North: return Direction::South;
        case Direction::South: return Direction::North;
        case Direction::East: return Direction::West;
        case Direction::West: return Direction::East;
        default: return Direction::North;
    }
}

inline std::uint64_t HashCombine(std::uint64_t seed, std::uint64_t value) {
    value += 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    return seed ^ value;
}

inline std::uint64_t MakeRoomSeed(std::uint64_t worldSeed, const RoomCoords& coords, std::uint64_t salt = 0) {
    std::uint64_t result = worldSeed;
    result = HashCombine(result, static_cast<std::uint64_t>(coords.x));
    result = HashCombine(result, static_cast<std::uint64_t>(coords.y));
    result = HashCombine(result, salt);
    return result;
}
