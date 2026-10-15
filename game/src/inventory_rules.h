#pragma once

#include "ui_inventory.h"

namespace InventoryRules {

ItemCategory ItemCategoryFromId(const InventoryUIState& state, int id);
bool IsStackableCategory(ItemCategory category);
int MaxStackForCategory(ItemCategory category);
int CalculateItemPrice(int rarity, int baseValue);

} // namespace InventoryRules
