#include "room.h"

#include <utility>

Room::Room(RoomCoords coords, RoomSeedData seedData, RoomLayout layout)
    : coords_(coords), seedData_(seedData), layout_(std::move(layout)) {}

Doorway* Room::FindDoor(Direction direction) {
    for (auto& door : layout_.doors) {
        if (door.direction == direction) {
            return &door;
        }
    }
    return nullptr;
}

const Doorway* Room::FindDoor(Direction direction) const {
    for (const auto& door : layout_.doors) {
        if (door.direction == direction) {
            return &door;
        }
    }
    return nullptr;
}

Doorway* Room::FindDoorTo(const RoomCoords& target) {
    for (auto& door : layout_.doors) {
        if (door.targetCoords == target) {
            return &door;
        }
    }
    return nullptr;
}
