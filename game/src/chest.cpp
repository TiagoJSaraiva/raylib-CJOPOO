#include "chest.h"

#include <algorithm>

namespace {

const Chest::Slot kEmptySlot{};

} // namespace

Chest::Chest(Type type,
             float anchorX,
             float anchorY,
             float interactionRadius,
             const Rectangle& hitbox,
             int capacity)
    : type_(type),
      anchorX_(anchorX),
      anchorY_(anchorY),
      interactionRadius_(interactionRadius),
      hitbox_(hitbox) {
    capacity = std::max(0, capacity);
    slots_.resize(static_cast<size_t>(capacity));
}

const Chest::Slot& Chest::GetSlot(int index) const {
    if (index < 0 || index >= static_cast<int>(slots_.size())) {
        return kEmptySlot;
    }
    return slots_[static_cast<size_t>(index)];
}

Chest::Slot& Chest::AccessSlot(int index) {
    if (slots_.empty()) {
        static Chest::Slot dummy{};
        dummy.itemId = 0;
        dummy.quantity = 0;
        return dummy;
    }
    if (index < 0 || index >= static_cast<int>(slots_.size())) {
        return slots_.front();
    }
    return slots_[static_cast<size_t>(index)];
}

void Chest::SetSlot(int index, int itemId, int quantity) {
    if (index < 0 || index >= static_cast<int>(slots_.size())) {
        return;
    }
    Chest::Slot& slot = slots_[static_cast<size_t>(index)];
    if (itemId <= 0) {
        slot.itemId = 0;
        slot.quantity = 0;
    } else {
        slot.itemId = itemId;
        slot.quantity = std::max(1, quantity);
    }
}

void Chest::ClearSlot(int index) {
    SetSlot(index, 0, 0);
}

CommonChest::CommonChest(float anchorX,
                         float anchorY,
                         float interactionRadius,
                         const Rectangle& hitbox,
                         int capacity,
                         std::uint64_t lootSeed)
    : Chest(Type::Common, anchorX, anchorY, interactionRadius, hitbox, capacity),
      lootSeed_(lootSeed) {
}

PlayerChest::PlayerChest(float anchorX,
                         float anchorY,
                         float interactionRadius,
                         const Rectangle& hitbox,
                         int capacity)
    : Chest(Type::Player, anchorX, anchorY, interactionRadius, hitbox, capacity) {
}
