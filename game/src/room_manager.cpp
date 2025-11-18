#include "room_manager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <unordered_set>
#include <utility>

#include "room_types.h"
#include "chest.h"

namespace {

constexpr int MIN_ROOM_SPACING_TILES = 2;
constexpr int MIN_CORRIDOR_LENGTH_TILES = MIN_ROOM_SPACING_TILES * 2;
constexpr int MAX_CORRIDOR_LENGTH_TILES = MIN_CORRIDOR_LENGTH_TILES * 3;

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

float TileToPixel(int tile) {
    return static_cast<float>(tile * TILE_SIZE);
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

std::shared_ptr<DoorInstance> CreateDoorInstance() {
    return std::make_shared<DoorInstance>();
}

std::shared_ptr<DoorInstance> EnsureDoorInstance(Doorway& door) {
    if (!door.doorState) {
        door.doorState = CreateDoorInstance();
    }
    return door.doorState;
}

} // namespace

RoomManager::RoomManager(std::uint64_t worldSeed)
    : worldSeed_(worldSeed) {
    CreateInitialRoom();
    EnsureNeighborsGenerated(currentRoomCoords_);
}

Room& RoomManager::CreateInitialRoom() {
    RoomCoords coords{0, 0};
    RoomSeedData seedData{RoomType::Lobby, BiomeType::Lobby, MakeRoomSeed(worldSeed_, coords)};

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
    EnsureDoorInstance(layout.doors.back());

    auto room = std::make_unique<Room>(coords, seedData, layout);
    currentRoomCoords_ = coords;
    rooms_.emplace(coords, std::move(room));
    roomsDiscovered_ = 1;

    auto& createdRoom = *rooms_.at(coords);
    createdRoom.SetEntranceDirection(std::nullopt);
    createdRoom.SetDoorsInitialized(true);
    createdRoom.SetVisited(true);
    InitializeRoomFeatures(createdRoom);
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

Room* RoomManager::TryGetRoom(const RoomCoords& coords) {
    return FindRoom(coords);
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
            door.sealed = true;
            door.targetGenerated = false;
            return false;
        }

        door.offset = neighborDoor->offset;
        door.width = neighborDoor->width;

        if (!neighborDoor->targetGenerated || !HasValidCorridor(*neighborDoor)) {
            TileRect corridor = ComputeCorridorBetweenRooms(room.Layout(), door, existing->Layout());
            if (corridor.width <= 0 || corridor.height <= 0) {
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

        std::shared_ptr<DoorInstance> sharedDoor = neighborDoor->doorState ? neighborDoor->doorState : CreateDoorInstance();
        neighborDoor->doorState = sharedDoor;
        door.doorState = sharedDoor;

        return true;
    }

    CreateRoomFromDoor(room, door);
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

    }

    if (!placementFound) {
        originDoor.sealed = true;
        originDoor.targetGenerated = false;
        return originRoom;
    }

    originDoor.targetCoords = targetCoords;
    originDoor.corridorTiles = selectedCorridor;
    originDoor.targetGenerated = true;
    originDoor.sealed = false;

    BiomeType biome = DetermineBiomeForRoom(originRoom, targetCoords);
    RoomSeedData seedData{selectedType, biome, MakeRoomSeed(worldSeed_, targetCoords)};
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
    entranceDoor.doorState = EnsureDoorInstance(originDoor);

    layout.doors.push_back(entranceDoor);

    auto newRoom = std::make_unique<Room>(targetCoords, seedData, layout);
    Room& created = *newRoom;
    rooms_.emplace(targetCoords, std::move(newRoom));
    RegisterRoomDiscovery(selectedType);

    created.SetEntranceDirection(entranceDoor.direction);
    InitializeRoomFeatures(created);

    return created;
}

void RoomManager::InitializeRoomFeatures(Room& room) {
    switch (room.GetType()) {
        case RoomType::Forge: {
            room.ClearChest();
            room.ClearShop();

            ForgeInstance forge{};
            forge.state = ForgeState::Working;

            const RoomLayout& layout = room.Layout();
            const TileRect& bounds = layout.tileBounds;

            const float tileSize = static_cast<float>(TILE_SIZE);
            constexpr float kFootprintWidthTiles = 2.0f;
            constexpr float kFootprintDepthTiles = 1.0f;

            const float availableWidth = static_cast<float>(layout.widthTiles);
            const float availableHeight = static_cast<float>(layout.heightTiles);

            const float horizontalMarginTiles = std::max(0.0f, (availableWidth - kFootprintWidthTiles) * 0.5f);
            const float verticalMarginTiles = std::max(0.0f, (availableHeight - kFootprintDepthTiles) * 0.5f);

            forge.anchorX = TileToPixel(bounds.x) + (horizontalMarginTiles + kFootprintWidthTiles * 0.5f) * tileSize;
            forge.anchorY = TileToPixel(bounds.y) + (verticalMarginTiles + kFootprintDepthTiles) * tileSize;

            forge.hitbox.width = (kFootprintWidthTiles + 1.0f) * tileSize;
            forge.hitbox.height = kFootprintDepthTiles * tileSize;
            forge.hitbox.x = forge.anchorX - forge.hitbox.width * 0.5f;
            forge.hitbox.y = forge.anchorY - forge.hitbox.height;

            forge.interactionRadius = tileSize * 2.2f;

            room.SetForge(forge);
            break;
        }
        case RoomType::Shop: {
            room.ClearChest();
            room.ClearForge();
            InitializeShopFeatures(room);
            break;
        }
        case RoomType::Chest: {
            room.ClearForge();
            room.ClearShop();
            InitializeChestFeatures(room, false);
            break;
        }
        case RoomType::Lobby: {
            room.ClearForge();
            room.ClearShop();
            InitializeChestFeatures(room, true);
            break;
        }
        default:
            room.ClearForge();
            room.ClearShop();
            room.ClearChest();
            break;
    }
}

void RoomManager::InitializeShopFeatures(Room& room) {
    const RoomLayout& layout = room.Layout();
    const TileRect& bounds = layout.tileBounds;

    const float tileSize = static_cast<float>(TILE_SIZE);
    constexpr float kFootprintWidthTiles = 3.0f;
    constexpr float kFootprintDepthTiles = 1.0f;

    ShopInstance shop{};
    shop.textureVariant = static_cast<int>(MakeRoomSeed(worldSeed_, room.GetCoords(), 0x51A7ULL) % 3ULL);
    shop.baseSeed = MakeRoomSeed(worldSeed_, room.GetCoords(), 0x5B0F5ULL);

    const float availableWidth = static_cast<float>(layout.widthTiles);
    const float availableHeight = static_cast<float>(layout.heightTiles);

    const float horizontalMarginTiles = std::max(0.0f, (availableWidth - kFootprintWidthTiles) * 0.5f);
    const float verticalMarginTiles = std::max(0.0f, (availableHeight - kFootprintDepthTiles) * 0.5f);

    shop.anchorX = TileToPixel(bounds.x) + (horizontalMarginTiles + kFootprintWidthTiles * 0.5f) * tileSize;
    shop.anchorY = TileToPixel(bounds.y) + (verticalMarginTiles + kFootprintDepthTiles) * tileSize;

    shop.hitbox.width = kFootprintWidthTiles * tileSize;  // Ajuste a largura da hitbox da loja aqui
    shop.hitbox.height = kFootprintDepthTiles * tileSize; // Ajuste a altura da hitbox da loja aqui
    shop.hitbox.x = shop.anchorX - shop.hitbox.width * 0.5f;
    shop.hitbox.y = shop.anchorY - shop.hitbox.height;

    shop.interactionRadius = tileSize * 2.4f;

    room.SetShop(shop);
}

void RoomManager::InitializeChestFeatures(Room& room, bool persistentPlayerChest) {
    const RoomLayout& layout = room.Layout();
    const TileRect& bounds = layout.tileBounds;

    const float tileSize = static_cast<float>(TILE_SIZE);
    constexpr float kFootprintWidthTiles = 1.6f;
    constexpr float kFootprintDepthTiles = 1.0f;

    const float availableWidth = static_cast<float>(layout.widthTiles);
    const float availableHeight = static_cast<float>(layout.heightTiles);

    const float horizontalMarginTiles = std::max(0.0f, (availableWidth - kFootprintWidthTiles) * 0.5f);
    const float verticalMarginTiles = std::max(0.0f, (availableHeight - kFootprintDepthTiles) * 0.5f);

    float anchorX = TileToPixel(bounds.x) + (horizontalMarginTiles + kFootprintWidthTiles * 0.5f) * tileSize;
    float anchorY = TileToPixel(bounds.y) + (verticalMarginTiles + kFootprintDepthTiles) * tileSize;

    Rectangle hitbox{};
    hitbox.width = kFootprintWidthTiles * tileSize;
    hitbox.height = kFootprintDepthTiles * tileSize * 0.9f;
    hitbox.x = anchorX - hitbox.width * 0.5f;
    hitbox.y = anchorY - hitbox.height;

    float interactionRadius = tileSize * 1.8f;

    std::unique_ptr<Chest> chest;
    if (persistentPlayerChest) {
        constexpr int kPlayerChestCapacity = 24;
        chest = std::make_unique<PlayerChest>(anchorX, anchorY, interactionRadius, hitbox, kPlayerChestCapacity);
    } else {
        constexpr int kCommonChestCapacity = 4;
        std::uint64_t lootSeed = MakeRoomSeed(worldSeed_, room.GetCoords(), 0xC73AULL);
        chest = std::make_unique<CommonChest>(anchorX, anchorY, interactionRadius, hitbox, kCommonChestCapacity, lootSeed);
    }

    room.SetChest(std::move(chest));
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
        std::shared_ptr<DoorInstance> sharedDoor = neighborDoor->doorState ? neighborDoor->doorState : CreateDoorInstance();
        neighborDoor->doorState = sharedDoor;
        door.doorState = sharedDoor;
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

    auto tryPlaceDoor = [&](Direction direction) -> bool {
        if (openDoors >= targetDoorGoal) {
            return false;
        }

        if (std::any_of(layout.doors.begin(), layout.doors.end(), [&](const Doorway& d) { return d.direction == direction; })) {
            return false;
        }

        RoomCoords neighborCoords = coords + ToDirectionOffset(direction);
        if (Room* neighbor = FindRoom(neighborCoords)) {
            Doorway* neighborDoor = neighbor->FindDoorTo(coords);
            if (!neighborDoor || neighborDoor->sealed) {
                return false;
            }
        }

        int wallLength = WallLengthForDirection(layout.widthTiles, layout.heightTiles, direction);
        if (wallLength <= DOOR_WIDTH_TILES + 2) {
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
                    layout.doors.pop_back();
                    continue;
                }

                EnsureDoorInstance(newDoor);

                ++openDoors;
                return true;
            }
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
    std::shared_ptr<DoorInstance> sharedDoor = neighborDoor->doorState ? neighborDoor->doorState : CreateDoorInstance();
    neighborDoor->doorState = sharedDoor;
    door.doorState = sharedDoor;

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

    const double forgeChance = 12.0; // ALTERAR NO FUTURO PARA AJUSTAR A FREQUENCIA DE FORGE
    const double shopChance = 12.0;
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

BiomeType RoomManager::DetermineBiomeForRoom(const Room& originRoom, const RoomCoords&) {
    BiomeType originBiome = originRoom.GetBiome();
    if (originBiome != BiomeType::Unknown && originBiome != BiomeType::Lobby) {
        return originBiome;
    }

    if (activeBiome_ == BiomeType::Unknown) {
        activeBiome_ = PickInitialBiome();
    }

    return activeBiome_;
}

BiomeType RoomManager::PickInitialBiome() {
    static constexpr std::array<BiomeType, 3> kAvailableBiomes{
        BiomeType::Cave,
        BiomeType::Mansion,
        BiomeType::Dungeon
    };

    std::mt19937_64 rng(MakeRoomSeed(worldSeed_, RoomCoords{0, 0}, 0xB10B1EULL));
    std::uniform_int_distribution<std::size_t> dist(0, kAvailableBiomes.size() - 1);
    return kAvailableBiomes[dist(rng)];
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
            return true;
        }
        for (const auto& door : room->Layout().doors) {
            if (door.corridorTiles.width > 0 && door.corridorTiles.height > 0) {
                if (Intersects(corridor, door.corridorTiles)) {
                    return true;
                }
            }
        }
    }
    return false;
}
