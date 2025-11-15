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

ForgeInstance* Room::GetForge() {
    if (!forge_.has_value()) {
        return nullptr;
    }
    return &forge_.value();
}

const ForgeInstance* Room::GetForge() const {
    if (!forge_.has_value()) {
        return nullptr;
    }
    return &forge_.value();
}

void Room::SetForge(const ForgeInstance& forge) {
    forge_ = forge;
}

void Room::ClearForge() {
    forge_.reset();
}

ShopInstance* Room::GetShop() {
    if (!shop_.has_value()) {
        return nullptr;
    }
    return &shop_.value();
}

const ShopInstance* Room::GetShop() const {
    if (!shop_.has_value()) {
        return nullptr;
    }
    return &shop_.value();
}

void Room::SetShop(const ShopInstance& shop) {
    shop_ = shop;
}

void Room::ClearShop() {
    shop_.reset();
}

Chest* Room::GetChest() {
    return chest_.get();
}

const Chest* Room::GetChest() const {
    return chest_.get();
}

void Room::SetChest(std::unique_ptr<Chest> chest) {
    chest_ = std::move(chest);
}

void Room::ClearChest() {
    chest_.reset();
}
