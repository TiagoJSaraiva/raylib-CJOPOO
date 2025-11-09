#pragma once

#include "raylib.h"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>

struct PlayerCharacter;
struct WeaponState;
struct WeaponBlueprint;

enum class InventoryViewMode {
    Inventory,
    Forge,
    Shop
};

enum class ItemCategory {
    None,
    Weapon,
    Armor,
    Consumable,
    Material,
    Result
};

struct ItemDefinition {
    int id{0};
    std::string name;
    ItemCategory category{ItemCategory::None};
    std::string description;
    int rarity{1};
    int baseValue{0};
    int value{0};
    const WeaponBlueprint* weaponBlueprint{nullptr};
};

struct InventoryUIState {
    bool open{false};
    InventoryViewMode mode{InventoryViewMode::Inventory};
    int selectedInventoryIndex{-1};
    int selectedEquipmentIndex{-1};
    int selectedWeaponIndex{-1};
    int selectedShopIndex{-1};
    int selectedForgeSlot{-1};
    int lastDetailItemId{-1};
    float forgeSuccessChance{0.0f};
    int forgeAdjustHundreds{0};
    int forgeAdjustTens{0};
    int forgeAdjustOnes{0};
    int forgeBaseCost{0};
    bool forgeBroken{false};
    int coins{125};
    int shopRollsLeft{1};
    float sellPriceMultiplier{0.2f}; // Base sell multiplier; meta progression can scale this later
    bool forgeEditingCost{false};
    std::array<int, 2> forgeInputIds{0, 0};
    std::array<std::string, 2> forgeInputNames{};
    std::array<int, 2> forgeInputQuantities{0, 0};
    int forgeResultId{0};
    std::string forgeResultName;
    int forgeResultQuantity{0};
    std::string feedbackMessage;
    float feedbackTimer{0.0f};

    // Placeholder data for prototype
    std::vector<ItemDefinition> items;
    std::vector<int> weaponSlotIds;
    std::vector<int> equipmentSlotIds;
    std::vector<int> inventoryItemIds;
    std::vector<int> inventoryQuantities;
    std::vector<int> shopItemIds;
    std::vector<int> shopPrices;
    std::vector<int> shopStock;
    std::vector<std::string> weaponSlots;
    std::vector<std::string> equipmentSlots;
    std::vector<std::string> inventoryItems;
    std::vector<ItemCategory> inventoryTypes;
    std::vector<std::string> shopItems;
    std::vector<ItemCategory> shopTypes;
    std::unordered_map<uint64_t, int> forgeRecipes;
    std::unordered_map<std::string, int> itemNameToId;
    Vector2 detailAbilityScroll{0.0f, 0.0f};
};

void InitializeInventoryUIDummyData(InventoryUIState& state);
void RenderInventoryUI(InventoryUIState& state,
                       const PlayerCharacter& player,
                       const WeaponState& leftWeapon,
                       const WeaponState& rightWeapon,
                       Vector2 screenSize);

const WeaponBlueprint* ResolveWeaponBlueprint(const InventoryUIState& state, int itemId);
