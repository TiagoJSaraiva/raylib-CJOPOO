#include "inventory_rules.h"

#include <cassert>

int main() {
    InventoryUIState state{};
    state.items = {
        ItemDefinition{1, "weapon", ItemCategory::Weapon},
        ItemDefinition{2, "consumable", ItemCategory::Consumable},
        ItemDefinition{3, "material", ItemCategory::Material}
    };

    assert(InventoryRules::ItemCategoryFromId(state, 1) == ItemCategory::Weapon);
    assert(InventoryRules::ItemCategoryFromId(state, 999) == ItemCategory::None);
    assert(!InventoryRules::IsStackableCategory(ItemCategory::Weapon));
    assert(InventoryRules::IsStackableCategory(ItemCategory::Consumable));
    assert(InventoryRules::IsStackableCategory(ItemCategory::Material));
    assert(InventoryRules::MaxStackForCategory(ItemCategory::Consumable) == 10);
    assert(InventoryRules::MaxStackForCategory(ItemCategory::Material) == 99);
    assert(InventoryRules::MaxStackForCategory(ItemCategory::Armor) == 1);
    assert(InventoryRules::CalculateItemPrice(0, -5) == 20);
    assert(InventoryRules::CalculateItemPrice(3, 80) == 140);

    return 0;
}
