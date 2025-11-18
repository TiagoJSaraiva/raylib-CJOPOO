#pragma once

#include <memory>
#include <optional>
#include <random>
#include <unordered_map>
#include <unordered_set>

#include "room.h"

// Responsável por gerar, armazenar e navegar entre salas do mapa.
class RoomManager {
public:
    explicit RoomManager(std::uint64_t worldSeed);

    Room& GetCurrentRoom();
    const Room& GetCurrentRoom() const;

    Room& GetRoom(const RoomCoords& coords);
    Room* TryGetRoom(const RoomCoords& coords);
    const Room* TryGetRoom(const RoomCoords& coords) const;

    bool MoveToNeighbor(Direction direction);
    void EnsureNeighborsGenerated(const RoomCoords& coords, int radius = 2);

    std::uint64_t GetWorldSeed() const { return worldSeed_; }
    RoomCoords GetCurrentCoords() const { return currentRoomCoords_; }

    const auto& Rooms() const { return rooms_; }

private:
    Room& CreateInitialRoom();
    Room& CreateRoomFromDoor(Room& originRoom, Doorway& originDoor);
    void ConfigureDoors(Room& room, std::optional<Direction> entranceDirection);
    void AlignWithNeighbor(Room& room, Direction direction, Room& neighbor);
    bool TryGenerateDoorTarget(Room& room, Doorway& door);
    void InitializeRoomFeatures(Room& room);
    void InitializeShopFeatures(Room& room);
    void InitializeChestFeatures(Room& room, bool persistentPlayerChest);
    RoomType PickRoomType(const RoomCoords& coords);
    void RegisterRoomDiscovery(RoomType type);
    BiomeType DetermineBiomeForRoom(const Room& originRoom, const RoomCoords& coords);
    BiomeType PickInitialBiome();
    void EnsureDoorsGenerated(Room& room);
    void EnsureNeighborsRecursive(const RoomCoords& coords, int depth, std::unordered_set<RoomCoords, RoomCoordsHash>& visited);

    Room* FindRoom(const RoomCoords& coords);
    const Room* FindRoom(const RoomCoords& coords) const;

    bool IsSpaceAvailable(const TileRect& candidateBounds) const;
    bool CorridorIntersectsRooms(const TileRect& corridor) const;

    std::uint64_t worldSeed_{0};
    RoomCoords currentRoomCoords_{};
    std::unordered_map<RoomCoords, std::unique_ptr<Room>, RoomCoordsHash> rooms_{};
    int roomsDiscovered_{0};
    bool bossSpawned_{false};
    int roomsSinceBoss_{0};
    BiomeType activeBiome_{BiomeType::Unknown};
};
