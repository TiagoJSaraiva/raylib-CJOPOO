#include "room_manager.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <unordered_set>
#include <utility>

#include "room_types.h"

namespace {

constexpr int MIN_ROOM_SPACING_TILES = 2;
constexpr int MIN_CORRIDOR_LENGTH_TILES = MIN_ROOM_SPACING_TILES * 2;
constexpr int MAX_CORRIDOR_LENGTH_TILES = MIN_CORRIDOR_LENGTH_TILES * 3;
constexpr bool DEBUG_DOOR_GENERATION = true;

int RandomInt(std::mt19937_64& rng, int minInclusive, int maxInclusive) {
    std::uniform_int_distribution<int> dist(minInclusive, maxInclusive);
    return dist(rng);
}

double RandomDouble(std::mt19937_64& rng, double minInclusive, double maxInclusive) {
    std::uniform_real_distribution<double> dist(minInclusive, maxInclusive);
    return dist(rng);
}

struct RoomPlacement {
    TileRect roomBounds{};
    TileRect corridorBounds{};
    bool addedAny = false;
    int entranceOffset{0};
};

TileRect ExpandWithMargin(const TileRect& rect, int marginTiles) {
    TileRect expanded = rect;
    expanded.x -= marginTiles;
    expanded.y -= marginTiles;
    expanded.width += marginTiles * 2;
    expanded.height += marginTiles * 2;
    return expanded;
}

bool HasValidCorridor(const Doorway& door) {
    return door.corridorTiles.width > 0 && door.corridorTiles.height > 0;
}

int CorridorLengthForDirection(Direction direction, const TileRect& corridor) {
    if (direction == Direction::North || direction == Direction::South) {
        return corridor.height;
    }
    return corridor.width;
}

TileRect ComputeCorridorBetweenRooms(const RoomLayout& originLayout, const Doorway& door, const RoomLayout& neighborLayout) {
    TileRect corridor{};
    const TileRect& originBounds = originLayout.tileBounds;
    const TileRect& neighborBounds = neighborLayout.tileBounds;

    const int doorWorldX = originBounds.x + door.offset;

    switch (door.direction) {
        case Direction::North: {
            int neighborBottom = neighborBounds.y + neighborBounds.height;
            int originTop = originBounds.y;
            int gap = originTop - neighborBottom;
            if (gap <= 0) return {};
            corridor.x = doorWorldX;
            corridor.y = neighborBottom;
            corridor.width = door.width;
            corridor.height = gap;
            break;
        }
        case Direction::South: {
            int originBottom = originBounds.y + originBounds.height;
            int neighborTop = neighborBounds.y;
            int gap = neighborTop - originBottom;
            if (gap <= 0) return {};
            corridor.x = doorWorldX;
            corridor.y = originBottom;
            corridor.width = door.width;
            corridor.height = gap;
            break;
        }
        case Direction::East: {
            int originRight = originBounds.x + originBounds.width;
            int neighborLeft = neighborBounds.x;
            int gap = neighborLeft - originRight;
            if (gap <= 0) return {};
            corridor.x = originRight;
            corridor.y = originBounds.y + door.offset;
            corridor.width = gap;
            corridor.height = door.width;
            break;
        }
        case Direction::West: {
            int neighborRight = neighborBounds.x + neighborBounds.width;
            int originLeft = originBounds.x;
            int gap = originLeft - neighborRight;
            if (gap <= 0) return {};
            corridor.x = neighborRight;
            corridor.y = originBounds.y + door.offset;
            corridor.width = gap;
            corridor.height = door.width;
            break;
        }
    }

    return corridor;
}

RoomPlacement ComputePlacement(const Room& originRoom, const Doorway& originDoor, int widthTiles, int heightTiles, int entranceOffset) {
    const TileRect& originBounds = originRoom.Layout().tileBounds;
    RoomPlacement placement{};
    placement.entranceOffset = entranceOffset;

    const int doorWorldX = originBounds.x + originDoor.offset;
    const int doorWorldY = originBounds.y + originDoor.offset;

    switch (originDoor.direction) {
        case Direction::North: {
            placement.roomBounds.width = widthTiles;
            placement.roomBounds.height = heightTiles;
            placement.roomBounds.x = doorWorldX - entranceOffset;
            placement.roomBounds.y = originBounds.y - originDoor.corridorLength - heightTiles;
            placement.corridorBounds.x = doorWorldX;
            placement.corridorBounds.y = placement.roomBounds.y + placement.roomBounds.height;
            placement.corridorBounds.width = originDoor.width;
            placement.corridorBounds.height = originDoor.corridorLength;
            break;
        }
        case Direction::South: {
            placement.roomBounds.width = widthTiles;
            placement.roomBounds.height = heightTiles;
            placement.roomBounds.x = doorWorldX - entranceOffset;
            placement.roomBounds.y = originBounds.y + originBounds.height + originDoor.corridorLength;
            placement.corridorBounds.x = doorWorldX;
            placement.corridorBounds.y = originBounds.y + originBounds.height;
            placement.corridorBounds.width = originDoor.width;
            placement.corridorBounds.height = originDoor.corridorLength;
            break;
        }
        case Direction::East: {
            placement.roomBounds.width = widthTiles;
            placement.roomBounds.height = heightTiles;
            placement.roomBounds.x = originBounds.x + originBounds.width + originDoor.corridorLength;
            placement.roomBounds.y = doorWorldY - entranceOffset;
            placement.corridorBounds.x = originBounds.x + originBounds.width;
            placement.corridorBounds.y = doorWorldY;
            placement.corridorBounds.width = originDoor.corridorLength;
            placement.corridorBounds.height = originDoor.width;
            break;
        }
        case Direction::West: {
            placement.roomBounds.width = widthTiles;
            placement.roomBounds.height = heightTiles;
            placement.roomBounds.x = originBounds.x - originDoor.corridorLength - widthTiles;
            placement.roomBounds.y = doorWorldY - entranceOffset;
            placement.corridorBounds.x = placement.roomBounds.x + placement.roomBounds.width;
            placement.corridorBounds.y = doorWorldY;
            placement.corridorBounds.width = originDoor.corridorLength;
            placement.corridorBounds.height = originDoor.width;
            break;
        }
    }

    return placement;
}

int WallLengthForDirection(int widthTiles, int heightTiles, Direction direction) {
    if (direction == Direction::North || direction == Direction::South) {
        return widthTiles;
    }
    return heightTiles;
}

} // namespace

RoomManager::RoomManager(std::uint64_t worldSeed)
    : worldSeed_(worldSeed) {
    CreateInitialRoom();
    EnsureNeighborsGenerated(currentRoomCoords_);
}

Room& RoomManager::CreateInitialRoom() {
    RoomCoords coords{0, 0};
    RoomSeedData seedData{RoomType::Normal, MakeRoomSeed(worldSeed_, coords)};

    constexpr int kInitialRoomSize = 12;

    RoomLayout layout{};
    layout.widthTiles = kInitialRoomSize;
    layout.heightTiles = kInitialRoomSize;
    layout.tileBounds = TileRect{0, 0, kInitialRoomSize, kInitialRoomSize};

    Doorway startingDoor{};
    startingDoor.direction = Direction::North;
    startingDoor.offset = (kInitialRoomSize - startingDoor.width) / 2;
    startingDoor.corridorLength = std::max(4, MIN_CORRIDOR_LENGTH_TILES);
    startingDoor.targetCoords = coords + ToDirectionOffset(Direction::North);
    layout.doors.push_back(startingDoor);

    auto room = std::make_unique<Room>(coords, seedData, layout);
    currentRoomCoords_ = coords;
    rooms_.emplace(coords, std::move(room));
    roomsDiscovered_ = 1;

    auto& createdRoom = *rooms_.at(coords);
    createdRoom.SetEntranceDirection(std::nullopt);
    createdRoom.SetDoorsInitialized(true);
    createdRoom.SetVisited(true);
    return createdRoom;
}

Room& RoomManager::GetCurrentRoom() {
    return *rooms_.at(currentRoomCoords_);
}

const Room& RoomManager::GetCurrentRoom() const {
    return *rooms_.at(currentRoomCoords_);
}

Room& RoomManager::GetRoom(const RoomCoords& coords) {
    return *rooms_.at(coords);
}

const Room* RoomManager::TryGetRoom(const RoomCoords& coords) const {
    auto it = rooms_.find(coords);
    if (it != rooms_.end()) {
        return it->second.get();
    }
    return nullptr;
}

Room* RoomManager::FindRoom(const RoomCoords& coords) {
    auto it = rooms_.find(coords);
    if (it == rooms_.end()) {
        return nullptr;
    }
    return it->second.get();
}

const Room* RoomManager::FindRoom(const RoomCoords& coords) const {
    auto it = rooms_.find(coords);
    if (it == rooms_.end()) {
        return nullptr;
    }
    return it->second.get();
}

bool RoomManager::MoveToNeighbor(Direction direction) {
    Room& current = GetCurrentRoom();
    Doorway* door = current.FindDoor(direction);
    if (!door || door->sealed) {
        return false;
    }

    if (!door->targetGenerated) {
        if (!TryGenerateDoorTarget(current, *door)) {
            return false;
        }
    }

    const RoomCoords destination = door->targetCoords;
    if (!FindRoom(destination)) {
        TryGenerateDoorTarget(current, *door);
    }

    if (!FindRoom(destination)) {
        return false;
    }

    currentRoomCoords_ = destination;
    GetCurrentRoom().SetVisited(true);
    EnsureNeighborsGenerated(currentRoomCoords_);
    return true;
}

void RoomManager::EnsureNeighborsGenerated(const RoomCoords& coords, int radius) {
    if (radius < 0) {
        radius = 0;
    }

    std::unordered_set<RoomCoords, RoomCoordsHash> visited;
    EnsureNeighborsRecursive(coords, radius, visited);
}

void RoomManager::EnsureNeighborsRecursive(const RoomCoords& coords, int depth, std::unordered_set<RoomCoords, RoomCoordsHash>& visited) {
    if (!visited.insert(coords).second) {
        return;
    }

    Room& room = GetRoom(coords);

    if (depth >= 1) {
        EnsureDoorsGenerated(room);
    }

    for (auto& door : room.Layout().doors) {
        if (door.sealed) {
            continue;
        }

        if (!TryGenerateDoorTarget(room, door)) {
            continue;
        }

        if (depth > 0) {
            Room* neighbor = FindRoom(door.targetCoords);
            if (neighbor) {
                EnsureNeighborsRecursive(neighbor->GetCoords(), depth - 1, visited);
            }
        }
    }
}

void RoomManager::EnsureDoorsGenerated(Room& room) {
    if (room.DoorsInitialized()) {
        return;
    }

    ConfigureDoors(room, room.GetEntranceDirection());
    room.SetDoorsInitialized(true);
    if (DEBUG_DOOR_GENERATION) {
        std::cout << "Room (" << room.GetCoords().x << "," << room.GetCoords().y << ") initialized with "
                  << room.Layout().doors.size() << " total doors" << std::endl;
        for (const auto& door : room.Layout().doors) {
            std::cout << "  - dir=" << static_cast<int>(door.direction)
                      << " offset=" << door.offset
                      << " width=" << door.width
                      << " sealed=" << door.sealed
                      << " targetGenerated=" << door.targetGenerated
                      << " target=(" << door.targetCoords.x << "," << door.targetCoords.y << ")"
                      << std::endl;
        }
    }
}

bool RoomManager::TryGenerateDoorTarget(Room& room, Doorway& door) {
    door.targetCoords = room.GetCoords() + ToDirectionOffset(door.direction);

    if (door.sealed) {
        return false;
    }

    if (door.targetGenerated) {
        Room* neighbor = FindRoom(door.targetCoords);
        if (neighbor) {
            AlignWithNeighbor(room, door.direction, *neighbor);
        }
        return true;
    }

    Room* existing = FindRoom(door.targetCoords);
    if (existing) {
        Doorway* neighborDoor = existing->FindDoorTo(room.GetCoords());
        if (!neighborDoor || neighborDoor->sealed) {
            if (DEBUG_DOOR_GENERATION) {
                std::cout << "Sealing door from room (" << room.GetCoords().x << "," << room.GetCoords().y
                          << ") to existing neighbor (" << door.targetCoords.x << "," << door.targetCoords.y
                          << ") because neighbor door missing or sealed" << std::endl;
            }
            door.sealed = true;
            door.targetGenerated = false;
            return false;
        }

        door.offset = neighborDoor->offset;
        door.width = neighborDoor->width;

        if (!neighborDoor->targetGenerated || !HasValidCorridor(*neighborDoor)) {
            TileRect corridor = ComputeCorridorBetweenRooms(room.Layout(), door, existing->Layout());
            if (corridor.width <= 0 || corridor.height <= 0) {
                if (DEBUG_DOOR_GENERATION) {
                    std::cout << "Sealing door from room (" << room.GetCoords().x << "," << room.GetCoords().y
                              << ") to existing neighbor (" << door.targetCoords.x << "," << door.targetCoords.y
                              << ") because computed corridor is invalid" << std::endl;
                }
                door.sealed = true;
                door.targetGenerated = false;
                return false;
            }

            int corridorLength = CorridorLengthForDirection(door.direction, corridor);
            door.corridorTiles = corridor;
            door.corridorLength = corridorLength;
            door.sealed = false;
            door.targetGenerated = true;

            neighborDoor->offset = door.offset;
            neighborDoor->width = door.width;
            neighborDoor->corridorTiles = corridor;
            neighborDoor->corridorLength = corridorLength;
            neighborDoor->targetGenerated = true;
            neighborDoor->sealed = false;
        } else {
            door.corridorTiles = neighborDoor->corridorTiles;
            door.corridorLength = neighborDoor->corridorLength;
            door.sealed = false;
            door.targetGenerated = true;
        }

        return true;
    }

    CreateRoomFromDoor(room, door);
        if (DEBUG_DOOR_GENERATION) {
            std::cout << "Added door in room (" << room.GetCoords().x << "," << room.GetCoords().y << ") toward direction "
                      << static_cast<int>(door.direction) << " leading to coords (" << door.targetCoords.x
                      << "," << door.targetCoords.y << ")" << std::endl;
        }
    return door.targetGenerated;
}

Room& RoomManager::CreateRoomFromDoor(Room& originRoom, Doorway& originDoor) {
    const RoomCoords targetCoords = originRoom.GetCoords() + ToDirectionOffset(originDoor.direction);
    std::mt19937_64 rng(MakeRoomSeed(worldSeed_, targetCoords));

    const int maxAttempts = 12;

    originDoor.corridorLength = std::max(originDoor.corridorLength, MIN_CORRIDOR_LENGTH_TILES);

    RoomType selectedType = RoomType::Unknown;
    TileRect selectedBounds{};
    TileRect selectedCorridor{};
    int entranceOffset = 0;
    int widthTiles = 0;
    int heightTiles = 0;
    bool placementFound = false;

    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        selectedType = PickRoomType(targetCoords);

        switch (selectedType) {
            case RoomType::Shop:
            case RoomType::Forge:
            case RoomType::Chest:
                widthTiles = 8;
                heightTiles = 8;
                break;
            case RoomType::Boss:
                widthTiles = 12;
                heightTiles = 12;
                break;
            default:
                widthTiles = RandomInt(rng, 10, 20);
                heightTiles = RandomInt(rng, 10, 20);
                break;
        }

        const int wallLength = WallLengthForDirection(widthTiles, heightTiles, Opposite(originDoor.direction));
        const int maxOffset = std::max(1, wallLength - originDoor.width - 1);
        entranceOffset = RandomInt(rng, 1, maxOffset);

        if (originDoor.corridorLength < MIN_CORRIDOR_LENGTH_TILES || originDoor.corridorLength > MAX_CORRIDOR_LENGTH_TILES) {
            originDoor.corridorLength = RandomInt(rng, MIN_CORRIDOR_LENGTH_TILES, MAX_CORRIDOR_LENGTH_TILES);
        }

        RoomPlacement placement = ComputePlacement(originRoom, originDoor, widthTiles, heightTiles, entranceOffset);
        selectedBounds = placement.roomBounds;
        selectedCorridor = placement.corridorBounds;

        bool spaceOk = IsSpaceAvailable(selectedBounds);
        bool corridorBlocked = (selectedCorridor.width > 0 && selectedCorridor.height > 0 && CorridorIntersectsRooms(selectedCorridor));
        if (spaceOk && !corridorBlocked) {
            placementFound = true;
            break;
        }

        if (DEBUG_DOOR_GENERATION) {
            std::cout << "Attempt " << attempt + 1 << " failed for new room from (" << originRoom.GetCoords().x << ","
                      << originRoom.GetCoords().y << ") dir " << static_cast<int>(originDoor.direction)
                      << " | size=" << widthTiles << "x" << heightTiles
                      << " | offset=" << entranceOffset
                      << " | corridorLen=" << originDoor.corridorLength
                      << " | spaceOk=" << spaceOk
                      << " | corridorBlocked=" << corridorBlocked
                      << std::endl;
        }
    }

    if (!placementFound) {
        if (DEBUG_DOOR_GENERATION) {
            std::cout << "Failed to place new room for door from room (" << originRoom.GetCoords().x << ","
                      << originRoom.GetCoords().y << ") towards direction "
                      << static_cast<int>(originDoor.direction) << " after " << maxAttempts
                      << " attempts" << std::endl;
        }
        originDoor.sealed = true;
        originDoor.targetGenerated = false;
        return originRoom;
    }

    originDoor.targetCoords = targetCoords;
    originDoor.corridorTiles = selectedCorridor;
    originDoor.targetGenerated = true;
    originDoor.sealed = false;

    RoomSeedData seedData{selectedType, MakeRoomSeed(worldSeed_, targetCoords)};
    RoomLayout layout{};
    layout.widthTiles = widthTiles;
    layout.heightTiles = heightTiles;
    layout.tileBounds = selectedBounds;

    Doorway entranceDoor{};
    entranceDoor.direction = Opposite(originDoor.direction);
    entranceDoor.offset = entranceOffset;
    entranceDoor.width = originDoor.width;
    entranceDoor.corridorLength = originDoor.corridorLength;
    entranceDoor.targetCoords = originRoom.GetCoords();
    entranceDoor.corridorTiles = selectedCorridor;
    entranceDoor.targetGenerated = true;
    entranceDoor.sealed = false;

    layout.doors.push_back(entranceDoor);

    auto newRoom = std::make_unique<Room>(targetCoords, seedData, layout);
    Room& created = *newRoom;
    rooms_.emplace(targetCoords, std::move(newRoom));
    RegisterRoomDiscovery(selectedType);

    created.SetEntranceDirection(entranceDoor.direction);

    return created;
}

void RoomManager::ConfigureDoors(Room& room, std::optional<Direction> entranceDirection) {
    RoomLayout& layout = room.Layout();
    RoomCoords coords = room.GetCoords();

    std::mt19937_64 rng(MakeRoomSeed(worldSeed_, coords, 0xABCD));

    for (Direction direction : {Direction::North, Direction::South, Direction::East, Direction::West}) {
        if (entranceDirection && direction == *entranceDirection) {
            continue;
        }

        RoomCoords neighborCoords = coords + ToDirectionOffset(direction);
        Room* neighbor = FindRoom(neighborCoords);
        if (!neighbor) {
            continue;
        }

        Doorway* neighborDoor = neighbor->FindDoorTo(coords);
        if (!neighborDoor) {
            continue;
        }

        Doorway door{};
        door.direction = direction;
        door.offset = neighborDoor->offset;
        door.width = neighborDoor->width;
        door.corridorLength = neighborDoor->corridorLength;
        door.targetCoords = neighborCoords;
        door.corridorTiles = neighborDoor->corridorTiles;
        door.targetGenerated = true;
        layout.doors.push_back(door);
    }

    auto countOpenDoors = [&layout]() {
        int count = 0;
        for (const auto& door : layout.doors) {
            if (!door.sealed) {
                ++count;
            }
        }
        return count;
    };

    int openDoors = countOpenDoors();

    if (room.GetType() == RoomType::Boss) {
        return;
    }

    const int targetDoorGoal = 4;

    std::vector<Direction> candidates{Direction::North, Direction::South, Direction::East, Direction::West};
    if (entranceDirection) {
        candidates.erase(std::remove(candidates.begin(), candidates.end(), *entranceDirection), candidates.end());
    }

    candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](Direction dir) {
        return std::any_of(layout.doors.begin(), layout.doors.end(), [&](const Doorway& door) {
            return door.direction == dir;
        });
    }), candidates.end());

    Doorway* entranceDoor = entranceDirection ? room.FindDoor(*entranceDirection) : nullptr;
    int anchorOffset = entranceDoor ? entranceDoor->offset : layout.heightTiles / 2;

    bool addedAny = false;

    auto tryPlaceDoor = [&](Direction direction) -> bool {
        if (openDoors >= targetDoorGoal) {
            return false;
        }

        if (std::any_of(layout.doors.begin(), layout.doors.end(), [&](const Doorway& d) { return d.direction == direction; })) {
            if (DEBUG_DOOR_GENERATION) {
                std::cout << "Skipping direction " << static_cast<int>(direction) << " in room (" << coords.x << ","
                          << coords.y << ") because door already exists" << std::endl;
            }
            return false;
        }

        RoomCoords neighborCoords = coords + ToDirectionOffset(direction);
        if (Room* neighbor = FindRoom(neighborCoords)) {
            Doorway* neighborDoor = neighbor->FindDoorTo(coords);
            if (!neighborDoor || neighborDoor->sealed) {
                if (DEBUG_DOOR_GENERATION) {
                    std::cout << "Skipping direction " << static_cast<int>(direction) << " in room (" << coords.x
                              << "," << coords.y << ") because neighbor (" << neighborCoords.x << ","
                              << neighborCoords.y << ") has no matching open door" << std::endl;
                }
                return false;
            }
        }

        int wallLength = WallLengthForDirection(layout.widthTiles, layout.heightTiles, direction);
        if (wallLength <= DOOR_WIDTH_TILES + 2) {
            if (DEBUG_DOOR_GENERATION) {
                std::cout << "Skipping direction " << static_cast<int>(direction) << " in room (" << coords.x << ","
                          << coords.y << ") because wall length " << wallLength << " too small" << std::endl;
            }
            return false;
        }

        std::vector<int> offsets;
        offsets.reserve(std::max(1, wallLength - DOOR_WIDTH_TILES - 1));
        for (int offset = 1; offset <= wallLength - DOOR_WIDTH_TILES - 1; ++offset) {
            offsets.push_back(offset);
        }
        if (offsets.empty()) {
            return false;
        }

        struct OffsetCandidate {
            int offset;
            double score;
        };

        std::vector<OffsetCandidate> orderedOffsets;
        orderedOffsets.reserve(offsets.size());
        double proximityWeight = RandomDouble(rng, 0.0, 1.0);
        for (int offset : offsets) {
            double distance = std::abs(offset - anchorOffset);
            double jitter = RandomDouble(rng, 0.0, 1.0);
            double score = proximityWeight * distance + (1.0 - proximityWeight) * jitter;
            orderedOffsets.push_back({offset, score});
        }

        std::sort(orderedOffsets.begin(), orderedOffsets.end(), [](const OffsetCandidate& a, const OffsetCandidate& b) {
            if (a.score == b.score) {
                return a.offset < b.offset;
            }
            return a.score < b.score;
        });

        for (const auto& candidate : orderedOffsets) {
            std::vector<int> corridorOptions;
            corridorOptions.reserve(MAX_CORRIDOR_LENGTH_TILES - MIN_CORRIDOR_LENGTH_TILES + 1);
            for (int len = MAX_CORRIDOR_LENGTH_TILES; len >= MIN_CORRIDOR_LENGTH_TILES; --len) {
                corridorOptions.push_back(len);
            }
            std::shuffle(corridorOptions.begin(), corridorOptions.end(), rng);

            for (int corridorLength : corridorOptions) {
                Doorway stub{};
                stub.direction = direction;
                stub.offset = candidate.offset;
                stub.corridorLength = std::max(corridorLength, MIN_CORRIDOR_LENGTH_TILES);
                stub.targetCoords = coords + ToDirectionOffset(direction);
                layout.doors.push_back(stub);

                Doorway& newDoor = layout.doors.back();
                if (!TryGenerateDoorTarget(room, newDoor) || newDoor.sealed) {
                    if (DEBUG_DOOR_GENERATION) {
                        std::cout << "Attempt failed: room (" << coords.x << "," << coords.y << ") dir "
                                  << static_cast<int>(direction) << " offset=" << candidate.offset
                                  << " corridorLen=" << corridorLength << std::endl;
                    }
                    layout.doors.pop_back();
                    continue;
                }

                addedAny = true;
                ++openDoors;
                if (DEBUG_DOOR_GENERATION) {
                    std::cout << "Added door in room (" << coords.x << "," << coords.y << ") toward direction "
                              << static_cast<int>(direction) << " leading to coords (" << newDoor.targetCoords.x
                              << "," << newDoor.targetCoords.y << ")" << std::endl;
                }
                return true;
            }
        }

        if (DEBUG_DOOR_GENERATION) {
            std::cout << "Failed to add door in room (" << coords.x << "," << coords.y
                      << ") towards direction " << static_cast<int>(direction)
                      << " - all placements invalid" << std::endl;
        }

        return false;
    };

    std::shuffle(candidates.begin(), candidates.end(), rng);

    for (Direction direction : candidates) {
        if (openDoors >= targetDoorGoal) {
            break;
        }

        tryPlaceDoor(direction);
    }

    if (!addedAny && DEBUG_DOOR_GENERATION) {
        std::cout << "Room (" << coords.x << "," << coords.y << ") remains with " << openDoors
                  << " open doors after ConfigureDoors" << std::endl;
    }
}

void RoomManager::AlignWithNeighbor(Room& room, Direction direction, Room& neighbor) {
    Doorway* existing = room.FindDoor(direction);
    if (existing && existing->targetGenerated) {
        return;
    }

    Doorway* neighborDoor = neighbor.FindDoorTo(room.GetCoords());
    if (!neighborDoor || !neighborDoor->targetGenerated || !HasValidCorridor(*neighborDoor)) {
        return;
    }

    Doorway door{};
    door.direction = direction;
    door.offset = neighborDoor->offset;
    door.width = neighborDoor->width;
    door.corridorLength = neighborDoor->corridorLength;
    door.corridorTiles = neighborDoor->corridorTiles;
    door.targetCoords = neighbor.GetCoords();
    door.targetGenerated = true;
    door.sealed = false;

    if (existing) {
        *existing = door;
    } else {
        room.Layout().doors.push_back(door);
    }
}

RoomType RoomManager::PickRoomType(const RoomCoords& coords) {
    std::mt19937_64 rng(MakeRoomSeed(worldSeed_, coords, static_cast<std::uint64_t>(roomsDiscovered_)));

    double bossChance = 0.0;
    double normalChance = 80.0;

    if (!bossSpawned_) {
        normalChance = std::max(0.0, 79.0 - 0.5 * roomsSinceBoss_);
        bossChance = 1.0 + 0.5 * roomsSinceBoss_;
    }

    const double forgeChance = 5.0;
    const double shopChance = 5.0;
    const double chestChance = 10.0;

    double total = normalChance + forgeChance + shopChance + chestChance + bossChance;
    double pick = RandomDouble(rng, 0.0, total);

    if (pick < normalChance) {
        return RoomType::Normal;
    }
    pick -= normalChance;

    if (pick < forgeChance) {
        return RoomType::Forge;
    }
    pick -= forgeChance;

    if (pick < shopChance) {
        return RoomType::Shop;
    }
    pick -= shopChance;

    if (pick < chestChance) {
        return RoomType::Chest;
    }

    return RoomType::Boss;
}

void RoomManager::RegisterRoomDiscovery(RoomType type) {
    ++roomsDiscovered_;
    if (!bossSpawned_) {
        if (type == RoomType::Boss) {
            bossSpawned_ = true;
            roomsSinceBoss_ = 0;
        } else {
            ++roomsSinceBoss_;
        }
    }
}

bool RoomManager::IsSpaceAvailable(const TileRect& candidateBounds) const {
    for (const auto& [coords, room] : rooms_) {
        TileRect paddedExisting = ExpandWithMargin(room->Layout().tileBounds, MIN_ROOM_SPACING_TILES);
        if (Intersects(candidateBounds, paddedExisting)) {
            return false;
        }
    }
    return true;
}

bool RoomManager::CorridorIntersectsRooms(const TileRect& corridor) const {
    if (corridor.width == 0 || corridor.height == 0) {
        return false;
    }

    for (const auto& [coords, room] : rooms_) {
        if (Intersects(corridor, room->Layout().tileBounds)) {
            if (DEBUG_DOOR_GENERATION) {
                std::cout << "Corridor intersects room bounds at (" << coords.x << "," << coords.y
                          << ") corridor Rect{" << corridor.x << "," << corridor.y << "," << corridor.width
                          << "," << corridor.height << "} room Rect{" << room->Layout().tileBounds.x << ","
                          << room->Layout().tileBounds.y << "," << room->Layout().tileBounds.width << ","
                          << room->Layout().tileBounds.height << "}" << std::endl;
            }
            return true;
        }
        for (const auto& door : room->Layout().doors) {
            if (door.corridorTiles.width > 0 && door.corridorTiles.height > 0) {
                if (Intersects(corridor, door.corridorTiles)) {
                    if (DEBUG_DOOR_GENERATION) {
                        std::cout << "Corridor intersects existing corridor from room (" << coords.x << ","
                                  << coords.y << ") corridor Rect{" << corridor.x << "," << corridor.y << ","
                                  << corridor.width << "," << corridor.height << "} existing Rect{"
                                  << door.corridorTiles.x << "," << door.corridorTiles.y << ","
                                  << door.corridorTiles.width << "," << door.corridorTiles.height << "}" << std::endl;
                    }
                    return true;
                }
            }
        }
    }
    return false;
}
