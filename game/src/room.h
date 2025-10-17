#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "room_types.h"

struct Doorway {
    Direction direction{Direction::North};
    int offset{0};
    int width{DOOR_WIDTH_TILES};
    int corridorLength{2};
    RoomCoords targetCoords{};
    TileRect corridorTiles{};
    bool targetGenerated{false};
    bool sealed{false};
};

struct RoomSeedData {
    RoomType type{RoomType::Unknown};
    BiomeType biome{BiomeType::Unknown};
    std::uint64_t seed{0};
};

struct RoomLayout {
    int widthTiles{0};
    int heightTiles{0};
    TileRect tileBounds{};
    std::vector<Doorway> doors;
};

class Room {
public:
    Room(RoomCoords coords, RoomSeedData seedData, RoomLayout layout);

    RoomCoords GetCoords() const { return coords_; }
    RoomType GetType() const { return seedData_.type; }
    BiomeType GetBiome() const { return seedData_.biome; }
    std::uint64_t GetSeed() const { return seedData_.seed; }

    RoomLayout& Layout() { return layout_; }
    const RoomLayout& Layout() const { return layout_; }

    Doorway* FindDoor(Direction direction);
    Doorway* FindDoorTo(const RoomCoords& target);
    const Doorway* FindDoor(Direction direction) const;

    bool IsVisited() const { return visited_; }
    void SetVisited(bool visited) { visited_ = visited; }

    bool DoorsInitialized() const { return doorsInitialized_; }
    void SetDoorsInitialized(bool initialized) { doorsInitialized_ = initialized; }

    std::optional<Direction> GetEntranceDirection() const { return entranceDirection_; }
    void SetEntranceDirection(std::optional<Direction> direction) { entranceDirection_ = direction; }

private:
    RoomCoords coords_{};
    RoomSeedData seedData_{};
    RoomLayout layout_{};
    bool visited_{false};
    bool doorsInitialized_{false};
    std::optional<Direction> entranceDirection_{};
};
