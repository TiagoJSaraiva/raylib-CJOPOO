#pragma once

#include "chest.h"
#include "ui_inventory.h"

namespace InventoryOperations {

void EnsureInventoryMeta(InventoryUIState& state);
void SetInventorySlot(InventoryUIState& state, int index, int itemId, int quantity);
void ClearInventorySlot(InventoryUIState& state, int index);
bool ReduceStackableSlot(InventoryUIState& state, int index, int amount);
bool ReduceConsumableStack(InventoryUIState& state, int index, int amount);
int FindEmptyInventorySlot(const InventoryUIState& state);
int AddItemToInventory(InventoryUIState& state, int itemId, int quantity);
int FindEmptyChestSlot(const Chest& chest);
bool ChestCanAccept(const Chest& chest,
                    const InventoryUIState& state,
                    int itemId,
                    int quantity);
int AddItemToChest(Chest& chest,
                   const InventoryUIState& state,
                   int itemId,
                   int quantity);

} // namespace InventoryOperations
