#include "inventory_operations.h"

#include "inventory_rules.h"

#include <algorithm>
#include <string>

namespace InventoryOperations {

using namespace InventoryRules;

void EnsureInventoryMeta(InventoryUIState& state) {
    size_t targetSize = state.inventoryItemIds.size();
    targetSize = std::max(targetSize, state.inventoryItems.size());
    targetSize = std::max(targetSize, state.inventoryQuantities.size());
    targetSize = std::max(targetSize, state.inventoryTypes.size());
    if (targetSize < 24) {
        targetSize = 24;
    }
    state.inventoryItemIds.resize(targetSize, 0);
    state.inventoryItems.resize(targetSize);
    state.inventoryQuantities.resize(targetSize, 0);
    state.inventoryTypes.resize(targetSize, ItemCategory::None);
}


void SetInventorySlot(InventoryUIState& state, int index, int itemId, int quantity) {
    EnsureInventoryMeta(state);
    if (index < 0 || index >= static_cast<int>(state.inventoryItemIds.size())) {
        return;
    }
    state.inventoryItemIds[index] = itemId;
    if (itemId <= 0) {
        state.inventoryItems[index].clear();
        state.inventoryQuantities[index] = 0;
        state.inventoryTypes[index] = ItemCategory::None;
    } else {
        const ItemDefinition* def = nullptr;
        for (const ItemDefinition& candidate : state.items) {
            if (candidate.id == itemId) { def = &candidate; break; }
        }
        state.inventoryItems[index] = def ? def->name : "?";
        state.inventoryQuantities[index] = std::max(1, quantity);
        state.inventoryTypes[index] = def ? def->category : ItemCategory::None;
    }
}


// Busca o primeiro slot vazio no baú informado.
int FindEmptyChestSlot(const Chest& chest) {
    const auto& slots = chest.GetSlots();
    for (int i = 0; i < static_cast<int>(slots.size()); ++i) {
        if (slots[static_cast<size_t>(i)].itemId == 0) {
            return i;
        }
    }
    return -1;
}

// Verifica se o baú suporta receber determinada quantidade do item.
bool ChestCanAccept(const Chest& chest,
                    const InventoryUIState& state,
                    int itemId,
                    int quantity) {
    if (itemId <= 0 || quantity <= 0) {
        return false;
    }

    ItemCategory category = ItemCategoryFromId(state, itemId);
    bool stackable = IsStackableCategory(category);
    const auto& slots = chest.GetSlots();

    if (stackable) {
        int maxStack = MaxStackForCategory(category);
        int remaining = quantity;
        int availableStackSpace = 0;
        int emptySlots = 0;
        for (const Chest::Slot& slot : slots) {
            if (slot.itemId == itemId) {
                availableStackSpace += std::max(0, maxStack - slot.quantity);
            } else if (slot.itemId == 0) {
                ++emptySlots;
            }
        }
        return availableStackSpace + emptySlots * maxStack >= remaining;
    }

    int emptySlots = 0;
    for (const Chest::Slot& slot : slots) {
        if (slot.itemId == 0) {
            ++emptySlots;
        }
    }
    return emptySlots >= quantity;
}

// Deposita itens no baú lidando com pilhas/slots livres. Retorna primeiro slot usado.
int AddItemToChest(Chest& chest,
                   const InventoryUIState& state,
                   int itemId,
                   int quantity) {
    if (!ChestCanAccept(chest, state, itemId, quantity)) {
        return -1;
    }

    ItemCategory category = ItemCategoryFromId(state, itemId);
    bool stackable = IsStackableCategory(category);
    int remaining = quantity;
    int firstSlotUsed = -1;
    int maxStack = MaxStackForCategory(category);

    auto& slots = chest.GetSlots();

    if (stackable) {
        for (int i = 0; i < static_cast<int>(slots.size()) && remaining > 0; ++i) {
            Chest::Slot& slot = slots[static_cast<size_t>(i)];
            if (slot.itemId != itemId) {
                continue;
            }
            int addable = std::min(maxStack - slot.quantity, remaining);
            if (addable <= 0) {
                continue;
            }
            slot.quantity += addable;
            remaining -= addable;
            if (firstSlotUsed < 0) {
                firstSlotUsed = i;
            }
        }

        while (remaining > 0) {
            int slotIndex = FindEmptyChestSlot(chest);
            if (slotIndex < 0) {
                break;
            }
            Chest::Slot& slot = slots[static_cast<size_t>(slotIndex)];
            slot.itemId = itemId;
            slot.quantity = std::min(maxStack, remaining);
            remaining -= slot.quantity;
            if (firstSlotUsed < 0) {
                firstSlotUsed = slotIndex;
            }
        }

        return (remaining == 0) ? firstSlotUsed : -1;
    }

    while (remaining > 0) {
        int slotIndex = FindEmptyChestSlot(chest);
        if (slotIndex < 0) {
            break;
        }
        Chest::Slot& slot = slots[static_cast<size_t>(slotIndex)];
        slot.itemId = itemId;
        slot.quantity = 1;
        --remaining;
        if (firstSlotUsed < 0) {
            firstSlotUsed = slotIndex;
        }
    }

    return (remaining == 0) ? firstSlotUsed : -1;
}


void ClearInventorySlot(InventoryUIState& state, int index) {
    SetInventorySlot(state, index, 0, 0);
}

bool ReduceStackableSlot(InventoryUIState& state, int index, int amount) {
    if (amount <= 0 || index < 0 || index >= static_cast<int>(state.inventoryItemIds.size())) {
        return false;
    }
    int itemId = state.inventoryItemIds[index];
    if (itemId == 0) {
        return false;
    }
    ItemCategory category = ItemCategoryFromId(state, itemId);
    if (category != ItemCategory::Consumable && category != ItemCategory::Material) {
        return false;
    }
    int current = (index < static_cast<int>(state.inventoryQuantities.size())) ? state.inventoryQuantities[index] : 0;
    if (current < amount || current <= 0) {
        return false;
    }

    int remaining = current - amount;
    if (remaining > 0) {
        SetInventorySlot(state, index, itemId, remaining);
    } else {
        ClearInventorySlot(state, index);
    }
    return true;
}

bool ReduceConsumableStack(InventoryUIState& state, int index, int amount) {
    if (amount <= 0 || index < 0 || index >= static_cast<int>(state.inventoryItemIds.size())) {
        return false;
    }
    if (state.inventoryItemIds[index] == 0) {
        return false;
    }
    if (ItemCategoryFromId(state, state.inventoryItemIds[index]) != ItemCategory::Consumable) {
        return false;
    }
    bool reduced = ReduceStackableSlot(state, index, amount);
    // NOTE: Reuse this helper when implementing consumable usage to decrement stacks after activating the item ability.
    return reduced;
}

int FindEmptyInventorySlot(const InventoryUIState& state) {
    for (int i = 0; i < static_cast<int>(state.inventoryItemIds.size()); ++i) {
        if (state.inventoryItemIds[i] == 0) {
            return i;
        }
    }
    return -1;
}

int AddItemToInventory(InventoryUIState& state, int itemId, int quantity) {
    if (itemId <= 0 || quantity <= 0) {
        return -1;
    }

    EnsureInventoryMeta(state);
    ItemCategory category = ItemCategoryFromId(state, itemId);
    int remaining = quantity;
    int firstSlotUsed = -1;

    bool isStackable = IsStackableCategory(category);
    int maxStack = MaxStackForCategory(category);

    if (isStackable) {
        int availableStackSpace = 0;
        int emptySlots = 0;
        for (int i = 0; i < static_cast<int>(state.inventoryItemIds.size()); ++i) {
            if (state.inventoryItemIds[i] == itemId) {
                int currentQty = std::max(0, state.inventoryQuantities[i]);
                availableStackSpace += std::max(0, maxStack - currentQty);
            } else if (state.inventoryItemIds[i] == 0) {
                ++emptySlots;
            }
        }

        if (availableStackSpace + emptySlots * maxStack < remaining) {
            return -1;
        }

        for (int i = 0; i < static_cast<int>(state.inventoryItemIds.size()) && remaining > 0; ++i) {
            if (state.inventoryItemIds[i] != itemId) {
                continue;
            }
            int currentQty = std::max(0, state.inventoryQuantities[i]);
            int addable = std::min(maxStack - currentQty, remaining);
            if (addable <= 0) {
                continue;
            }
            SetInventorySlot(state, i, itemId, currentQty + addable);
            remaining -= addable;
            if (firstSlotUsed < 0) {
                firstSlotUsed = i;
            }
        }

        while (remaining > 0) {
            int slot = FindEmptyInventorySlot(state);
            if (slot < 0) {
                break;
            }
            int toAssign = std::min(maxStack, remaining);
            SetInventorySlot(state, slot, itemId, toAssign);
            remaining -= toAssign;
            if (firstSlotUsed < 0) {
                firstSlotUsed = slot;
            }
        }

        return (remaining == 0) ? firstSlotUsed : -1;
    }

    int emptySlots = 0;
    for (int id : state.inventoryItemIds) {
        if (id == 0) {
            ++emptySlots;
        }
    }
    if (emptySlots < remaining) {
        return -1;
    }

    while (remaining > 0) {
        int slot = FindEmptyInventorySlot(state);
        if (slot < 0) {
            break;
        }
        SetInventorySlot(state, slot, itemId, 1);
        --remaining;
        if (firstSlotUsed < 0) {
            firstSlotUsed = slot;
        }
    }

    return (remaining == 0) ? firstSlotUsed : -1;
}


} // namespace InventoryOperations
