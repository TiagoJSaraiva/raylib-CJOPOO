#include "inventory_rules.h"

#include <algorithm>

namespace InventoryRules {

namespace {

constexpr int kConsumableMaxStack = 10;
constexpr int kMaterialMaxStack = 99;

} // namespace

// Define se o item pode empilhar no inventário ou no baú.
bool IsStackableCategory(ItemCategory category) {
    return category == ItemCategory::Consumable || category == ItemCategory::Material;
}

// Quantidade máxima por pilha em função da categoria.
int MaxStackForCategory(ItemCategory category) {
    if (category == ItemCategory::Consumable) {
        return kConsumableMaxStack;
    }
    if (category == ItemCategory::Material) {
        return kMaterialMaxStack;
    }
    return 1;
}


int CalculateItemPrice(int rarity, int baseValue) {
    // Ajuste a formula de precos aqui: currently 20 * raridade + valor base individual.
    int safeRarity = std::max(1, rarity);
    int safeBase = std::max(0, baseValue);
    return 20 * safeRarity + safeBase;
}


ItemCategory ItemCategoryFromId(const InventoryUIState& state, int id) {
    if (id <= 0) { return ItemCategory::None; }
    for (const ItemDefinition& candidate : state.items) {
        if (candidate.id == id) { return candidate.category; }
    }
    return ItemCategory::None;
}


} // namespace InventoryRules
