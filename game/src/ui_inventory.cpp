#include "ui_inventory.h"

#include "raygui.h"
#include "player.h"
#include "weapon.h"
#include "weapon_blueprints.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

const ItemDefinition* FindItemDefinition(const InventoryUIState& state, int id);

} // namespace

const WeaponBlueprint* ResolveWeaponBlueprint(const InventoryUIState& state, int itemId) {
    const ItemDefinition* def = FindItemDefinition(state, itemId);
    if (def == nullptr || def->category != ItemCategory::Weapon) {
        return nullptr;
    }
    return def->weaponBlueprint;
}

namespace {

constexpr float kFeedbackDuration = 2.5f;
constexpr float kBodyTextSpacing = 2.0f;
constexpr int kDefaultShopStock = 1; // Non-consumables default to a single copy
constexpr int kConsumableShopMinStock = 2;
constexpr int kConsumableShopMaxStock = 7;
constexpr int kConsumableMaxStack = 10;
constexpr int kMaterialMaxStack = 99;

void ShowMessage(InventoryUIState& state, const std::string& text) {
    state.feedbackMessage = text;
    state.feedbackTimer = kFeedbackDuration;
}

const ItemDefinition* FindItemDefinition(const InventoryUIState& state, int id) {
    if (id <= 0) {
        return nullptr;
    }
    for (const ItemDefinition& def : state.items) {
        if (def.id == id) {
            return &def;
        }
    }
    return nullptr;
}

std::string FormatFloat(float value, int decimals) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimals) << value;
    return oss.str();
}

const char* WeaponAttributeLabel(WeaponAttributeKey key) {
    switch (key) {
        case WeaponAttributeKey::Constitution:
            return "Constituicao";
        case WeaponAttributeKey::Strength:
            return "Forca";
        case WeaponAttributeKey::Focus:
            return "Foco";
        case WeaponAttributeKey::Mysticism:
            return "Misticismo";
        case WeaponAttributeKey::Knowledge:
            return "Conhecimento";
    }
    return "Atributo";
}

std::string FormatPassiveBonuses(const PlayerAttributes& bonuses) {
    std::ostringstream oss;
    int appended = 0;

    auto appendInt = [&](int value, const char* label) {
        if (value == 0) {
            return;
        }
        if (appended == 0) {
            oss << "Passivos:\n";
        }
        oss << "- " << label << " " << (value > 0 ? "+" : "") << value << "\n";
        ++appended;
    };

    auto appendFloat = [&](float value, const char* label, int decimals) {
        if (std::fabs(value) < 1e-4f) {
            return;
        }
        if (appended == 0) {
            oss << "Passivos:\n";
        }
        oss << "- " << label << " " << (value > 0.0f ? "+" : "") << FormatFloat(value, decimals) << "\n";
        ++appended;
    };

    appendInt(bonuses.primary.poder, "Poder");
    appendInt(bonuses.primary.defesa, "Defesa");
    appendInt(bonuses.primary.vigor, "Vigor");
    appendInt(bonuses.primary.velocidade, "Velocidade");
    appendInt(bonuses.primary.destreza, "Destreza");
    appendInt(bonuses.primary.inteligencia, "Inteligencia");

    appendInt(bonuses.attack.constituicao, "Constituicao");
    appendInt(bonuses.attack.forca, "Forca");
    appendInt(bonuses.attack.foco, "Foco");
    appendInt(bonuses.attack.misticismo, "Misticismo");
    appendInt(bonuses.attack.conhecimento, "Conhecimento");

    appendFloat(bonuses.secondary.vampirismo, "Vampirismo", 1);
    appendFloat(bonuses.secondary.letalidade, "Letalidade", 1);
    appendFloat(bonuses.secondary.reducaoDano, "Reducao de Dano", 1);
    appendFloat(bonuses.secondary.desvio, "Desvio", 1);
    appendFloat(bonuses.secondary.alcanceColeta, "Alcance de Coleta", 1);
    appendFloat(bonuses.secondary.sorte, "Sorte", 1);
    appendInt(bonuses.secondary.maldicao, "Maldicao");

    if (appended == 0) {
        return "Passivos: Nenhum";
    }

    std::string result = oss.str();
    if (!result.empty() && result.back() == '\n') {
        result.pop_back();
    }
    return result;
}

std::string FormatWeaponBlueprintDetails(const WeaponBlueprint& blueprint) {
    std::string text = "Arma: " + blueprint.name;

    const char* attributeLabel = WeaponAttributeLabel(blueprint.attributeKey);
    text += "\nAtributo chave: ";
    text += attributeLabel;

    if (blueprint.damage.baseDamage > 0.0f) {
        text += "\nDano base: " + FormatFloat(blueprint.damage.baseDamage, 1);
    }
    if (std::fabs(blueprint.damage.attributeScaling) > 1e-4f) {
        text += "\nEscala: ";
        if (blueprint.damage.attributeScaling > 0.0f) {
            text += "+";
        }
        text += FormatFloat(blueprint.damage.attributeScaling, 2);
        text += " por ponto de ";
        text += attributeLabel;
    }

    if (blueprint.cadence.baseAttacksPerSecond > 0.0f) {
        text += "\nCadencia base: " + FormatFloat(blueprint.cadence.baseAttacksPerSecond, 2) + " disparos/s";
    }
    if (blueprint.cadence.dexterityGainPerPoint > 0.0f) {
        text += "\nGanho por Destreza: +" + FormatFloat(blueprint.cadence.dexterityGainPerPoint, 2) + " disparos/s";
    }
    if (blueprint.cadence.attacksPerSecondCap > 0.0f) {
        text += "\nLimite de cadencia: " + FormatFloat(blueprint.cadence.attacksPerSecondCap, 2) + " disparos/s";
    }

    if (blueprint.critical.baseChance > 0.0f) {
        text += "\nCritico base: " + FormatFloat(blueprint.critical.baseChance * 100.0f, 1) + "%";
    }
    if (blueprint.critical.chancePerLetalidade > 0.0f) {
        text += "\nCritico por Letalidade: +" + FormatFloat(blueprint.critical.chancePerLetalidade * 100.0f, 2) + "%";
    }
    if (blueprint.critical.multiplier > 0.0f && std::fabs(blueprint.critical.multiplier - 1.0f) > 1e-4f) {
        text += "\nMultiplicador critico: x" + FormatFloat(blueprint.critical.multiplier, 2);
    }

    if (blueprint.projectile.kind == ProjectileKind::Ammunition) {
        if (blueprint.projectile.ammunition.speed > 0.0f) {
            text += "\nVelocidade do projetil: " + FormatFloat(blueprint.projectile.ammunition.speed, 0) + " px/s";
        }
        if (blueprint.projectile.ammunition.maxDistance > 0.0f) {
            text += "\nAlcance maximo: " + FormatFloat(blueprint.projectile.ammunition.maxDistance, 0) + " px";
        }
        if (blueprint.projectile.ammunition.radius > 0.0f) {
            text += "\nRaio da hitbox: " + FormatFloat(blueprint.projectile.ammunition.radius, 1) + " px";
        }
    }

    std::string passiveSummary = FormatPassiveBonuses(blueprint.passiveBonuses);
    if (!passiveSummary.empty()) {
        text += "\n" + passiveSummary;
    }

    return text;
}

int CalculateItemPrice(int rarity, int baseValue) {
    // Ajuste a formula de precos aqui: currently 20 * raridade + valor base individual.
    int safeRarity = std::max(1, rarity);
    int safeBase = std::max(0, baseValue);
    return 20 * safeRarity + safeBase;
}

int GetItemValue(const InventoryUIState& state, int itemId) {
    if (itemId <= 0) {
        return 0;
    }
    const ItemDefinition* def = FindItemDefinition(state, itemId);
    return def ? std::max(0, def->value) : 0;
}

int GetItemRarity(const InventoryUIState& state, int itemId) {
    if (itemId <= 0) {
        return 0;
    }
    const ItemDefinition* def = FindItemDefinition(state, itemId);
    return def ? std::max(0, def->rarity) : 0;
}

Color RarityToColor(int rarity) {
    switch (rarity) {
        case 1:
            return Color{160, 160, 160, 255}; // comum - cinza
        case 2:
            return Color{90, 180, 110, 255};  // incomum - verde
        case 3:
            return Color{80, 140, 225, 255};  // raro - azul
        case 4:
            return Color{170, 90, 210, 255};  // epico - roxo
        case 5:
            return Color{240, 200, 70, 255};  // lendario - amarelo
        case 6:
            return Color{150, 30, 70, 255};   // mitico - vermelho vinho
        default:
            return Color{110, 120, 140, 255};
    }
}

Color ResolveBorderColor(const InventoryUIState& state, int itemId) {
    int rarity = GetItemRarity(state, itemId);
    if (rarity <= 0) {
        return Color{70, 80, 100, 255};
    }
    return RarityToColor(rarity);
}

void RefreshForgeChance(InventoryUIState& state) {
    if (state.forgeBroken) {
        state.forgeSuccessChance = 0.0f;
        return;
    }

    if (state.forgeInputIds[0] <= 0 || state.forgeInputIds[1] <= 0) {
        state.forgeSuccessChance = 0.0f;
        return;
    }

    int valueA = GetItemValue(state, state.forgeInputIds[0]);
    int valueB = GetItemValue(state, state.forgeInputIds[1]);
    int totalValue = valueA + valueB;
    if (totalValue <= 0) {
        state.forgeSuccessChance = 0.0f;
        return;
    }

    float invested = static_cast<float>(std::max(0, state.forgeBaseCost));
    float ratio = invested / static_cast<float>(totalValue);
    state.forgeSuccessChance = std::clamp(ratio, 0.0f, 1.0f);
}

std::string ItemNameFromId(const InventoryUIState& state, int id) {
    const ItemDefinition* def = FindItemDefinition(state, id);
    return def ? def->name : std::string();
}

ItemCategory ItemCategoryFromId(const InventoryUIState& state, int id) {
    const ItemDefinition* def = FindItemDefinition(state, id);
    return def ? def->category : ItemCategory::None;
}

uint64_t MakeForgeKey(int idA, int idB) {
    if (idA > idB) {
        std::swap(idA, idB);
    }
    return (static_cast<uint64_t>(static_cast<uint32_t>(idA)) << 32) | static_cast<uint32_t>(idB);
}

void AppendForgeCombos(const InventoryUIState& state, int itemId, std::string& text) {
    if (itemId <= 0) {
        return;
    }
    std::string combos;
    for (const auto& entry : state.forgeRecipes) {
        int a = static_cast<int>(entry.first >> 32);
        int b = static_cast<int>(entry.first & 0xFFFFFFFFu);
        if (a == itemId || b == itemId) {
            int other = (a == itemId) ? b : a;
            std::string otherName = ItemNameFromId(state, other);
            std::string resultName = ItemNameFromId(state, entry.second);
            if (!otherName.empty() && !resultName.empty()) {
                combos += "- " + otherName + " -> " + resultName + "\n";
            }
        }
    }
    if (!combos.empty()) {
        text += "\nCombina com:\n" + combos;
    }
}

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

void EnsureWeaponCapacity(InventoryUIState& state, size_t size) {
    if (state.weaponSlotIds.size() < size) {
        state.weaponSlotIds.resize(size, 0);
    }
    if (state.weaponSlots.size() < size) {
        state.weaponSlots.resize(size);
    }
}

void EnsureEquipmentCapacity(InventoryUIState& state, size_t size) {
    if (state.equipmentSlotIds.size() < size) {
        state.equipmentSlotIds.resize(size, 0);
    }
    if (state.equipmentSlots.size() < size) {
        state.equipmentSlots.resize(size);
    }
}

void EnsureShopCapacity(InventoryUIState& state, size_t size) {
    if (state.shopItemIds.size() < size) {
        state.shopItemIds.resize(size, 0);
    }
    if (state.shopItems.size() < size) {
        state.shopItems.resize(size);
    }
    if (state.shopPrices.size() < size) {
        state.shopPrices.resize(size, 0);
    }
    if (state.shopTypes.size() < size) {
        state.shopTypes.resize(size, ItemCategory::None);
    }
        if (state.shopStock.size() < size) {
            state.shopStock.resize(size, 0);
        }
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
        const ItemDefinition* def = FindItemDefinition(state, itemId);
        state.inventoryItems[index] = def ? def->name : "?";
        state.inventoryQuantities[index] = std::max(1, quantity);
        state.inventoryTypes[index] = def ? def->category : ItemCategory::None;
    }
}

void SetWeaponSlot(InventoryUIState& state, int index, int itemId) {
    if (index < 0) {
        return;
    }
    EnsureWeaponCapacity(state, static_cast<size_t>(index) + 1);
    state.weaponSlotIds[index] = itemId;
    state.weaponSlots[index] = ItemNameFromId(state, itemId);
}

void SetEquipmentSlot(InventoryUIState& state, int index, int itemId) {
    if (index < 0) {
        return;
    }
    EnsureEquipmentCapacity(state, static_cast<size_t>(index) + 1);
    state.equipmentSlotIds[index] = itemId;
    state.equipmentSlots[index] = ItemNameFromId(state, itemId);
}

void SetShopSlot(InventoryUIState& state, int index, int itemId, int price, int stock) {
    if (index < 0) {
        return;
    }
    EnsureShopCapacity(state, static_cast<size_t>(index) + 1);
    state.shopItemIds[index] = itemId;
    state.shopItems[index] = ItemNameFromId(state, itemId);
    state.shopPrices[index] = price;
    state.shopTypes[index] = ItemCategoryFromId(state, itemId);
    state.shopStock[index] = std::max(0, stock);
}

void RollShopInventory(InventoryUIState& state) {
    const int desiredSlots = 4;
    EnsureShopCapacity(state, desiredSlots);
    state.shopItemIds.assign(desiredSlots, 0);
    state.shopItems.assign(desiredSlots, std::string());
    state.shopPrices.assign(desiredSlots, 0);
    state.shopTypes.assign(desiredSlots, ItemCategory::None);
    state.shopStock.assign(desiredSlots, 0);

    std::vector<int> availableIndices;
    availableIndices.reserve(state.items.size());
    for (int i = 0; i < static_cast<int>(state.items.size()); ++i) {
        if (state.items[i].id > 0) {
            availableIndices.push_back(i);
        }
    }

    for (int slot = 0; slot < desiredSlots; ++slot) {
        if (availableIndices.empty()) {
            SetShopSlot(state, slot, 0, 0, 0);
            continue;
        }
        int pick = GetRandomValue(0, static_cast<int>(availableIndices.size()) - 1);
        int index = availableIndices[pick];
        const ItemDefinition& def = state.items[index];
        int price = static_cast<int>(std::round(def.value * 1.3f));
        int finalPrice = (price <= 0) ? def.value : price;
        int stock = kDefaultShopStock;
        if (def.category == ItemCategory::Consumable) {
            stock = GetRandomValue(kConsumableShopMinStock, kConsumableShopMaxStock);
        }
        SetShopSlot(state, slot, def.id, finalPrice, stock);
        availableIndices.erase(availableIndices.begin() + pick);
    }

    state.selectedShopIndex = -1;
}

void ClearInventorySlot(InventoryUIState& state, int index) {
    SetInventorySlot(state, index, 0, 0);
}

bool ReduceConsumableStack(InventoryUIState& state, int index, int amount) {
    if (amount <= 0 || index < 0 || index >= static_cast<int>(state.inventoryItemIds.size())) {
        return false;
    }
    if (state.inventoryItemIds[index] == 0) {
        return false;
    }
    if (index >= static_cast<int>(state.inventoryTypes.size()) || state.inventoryTypes[index] != ItemCategory::Consumable) {
        return false;
    }
    int current = (index < static_cast<int>(state.inventoryQuantities.size())) ? state.inventoryQuantities[index] : 0;
    if (current < amount) {
        return false;
    }
    int remaining = current - amount;
    if (remaining > 0) {
        SetInventorySlot(state, index, state.inventoryItemIds[index], remaining);
    } else {
        ClearInventorySlot(state, index);
    }
    // NOTE: Reuse this helper when implementing consumable usage to decrement stacks after activating the item ability.
    return true;
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

    bool isStackable = (category == ItemCategory::Consumable || category == ItemCategory::Material);
    int maxStack = (category == ItemCategory::Consumable) ? kConsumableMaxStack : kMaterialMaxStack;

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

int FindEmptyForgeSlot(const InventoryUIState& state) {
    for (int i = 0; i < 2; ++i) {
        if (state.forgeInputIds[i] == 0) {
            return i;
        }
    }
    return -1;
}

void ClearForgeSlot(InventoryUIState& state, int slot) {
    if (slot < 0 || slot > 1) {
        return;
    }
    state.forgeInputIds[slot] = 0;
    state.forgeInputNames[slot].clear();
    state.forgeInputQuantities[slot] = 0;
    RefreshForgeChance(state);
}

void ClearForgeResult(InventoryUIState& state) {
    state.forgeResultId = 0;
    state.forgeResultName.clear();
    state.forgeResultQuantity = 0;
}

bool DetermineForgeOutcome(const InventoryUIState& state,
                           int& resultId,
                           int& resultQuantity) {
    int idA = state.forgeInputIds[0];
    int idB = state.forgeInputIds[1];
    if (idA <= 0 || idB <= 0) {
        return false;
    }
    uint64_t key = MakeForgeKey(idA, idB);
    auto it = state.forgeRecipes.find(key);
    if (it == state.forgeRecipes.end()) {
        return false;
    }
    resultId = it->second;
    resultQuantity = 1;
    return true;
}

void AttemptForge(InventoryUIState& state) {
    if (state.forgeBroken) {
        ShowMessage(state, "A bigorna esta quebrada.");
        return;
    }

    if (state.forgeResultId != 0) {
        ShowMessage(state, "Retire o resultado atual primeiro.");
        state.selectedForgeSlot = 2;
        return;
    }

    if (state.forgeInputIds[0] == 0 || state.forgeInputIds[1] == 0) {
        ShowMessage(state, "Para forjar, sao necessarios dois itens.");
        return;
    }

    int resultId = 0;
    int resultQuantity = 0;
    if (!DetermineForgeOutcome(state, resultId, resultQuantity)) {
        ShowMessage(state, "forja impossivel");
        return;
    }

    RefreshForgeChance(state);
    float successChance = state.forgeSuccessChance;

    int invested = std::max(0, state.forgeBaseCost);
    if (invested > state.coins) {
        ShowMessage(state, "Moedas insuficientes para investir.");
        return;
    }

    float roll = static_cast<float>(GetRandomValue(0, 1000000)) / 1000000.0f;
    bool success = roll <= successChance;

    state.coins = std::max(0, state.coins - invested);
    state.forgeBaseCost = 0;

    if (success) {
        ClearForgeResult(state);
        state.forgeResultId = resultId;
        state.forgeResultQuantity = std::max(1, resultQuantity);
        state.forgeResultName = ItemNameFromId(state, resultId);

        ClearForgeSlot(state, 0);
        ClearForgeSlot(state, 1);
        state.selectedForgeSlot = 2;
        ShowMessage(state, "Forja concluida!");
    } else {
        state.forgeBroken = true;
        state.selectedForgeSlot = -1;
        ShowMessage(state, "forja falhou! A bigorna quebrou.");
    }

    RefreshForgeChance(state);
}

int CalculateSaleValue(const InventoryUIState& state, int itemId, int quantity = 1) {
    if (itemId <= 0 || quantity <= 0) {
        return 0;
    }
    const ItemDefinition* def = FindItemDefinition(state, itemId);
    if (def == nullptr || def->value <= 0) {
        return 0;
    }
    float multiplier = std::max(0.0f, state.sellPriceMultiplier);
    float totalValue = static_cast<float>(def->value) * static_cast<float>(quantity) * multiplier;
    return static_cast<int>(std::round(totalValue));
}

void HandleDesequiparWeapon(InventoryUIState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.weaponSlotIds.size())) {
        return;
    }
    int itemId = state.weaponSlotIds[index];
    if (itemId == 0) {
        ShowMessage(state, "Nenhuma arma equipada.");
        return;
    }
    int slot = AddItemToInventory(state, itemId, 1);
    if (slot < 0) {
        ShowMessage(state, "Sem espaco no inventario.");
        return;
    }
    SetWeaponSlot(state, index, 0);
    state.selectedWeaponIndex = -1;
    state.selectedInventoryIndex = slot;
}

void HandleDesequiparArmor(InventoryUIState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.equipmentSlotIds.size())) {
        return;
    }
    int itemId = state.equipmentSlotIds[index];
    if (itemId == 0) {
        ShowMessage(state, "Nenhum equipamento equipado.");
        return;
    }
    int slot = AddItemToInventory(state, itemId, 1);
    if (slot < 0) {
        ShowMessage(state, "Sem espaco no inventario.");
        return;
    }
    SetEquipmentSlot(state, index, 0);
    state.selectedEquipmentIndex = -1;
    state.selectedInventoryIndex = slot;
}

void HandleDiscardWeapon(InventoryUIState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.weaponSlotIds.size())) {
        return;
    }
    SetWeaponSlot(state, index, 0);
    state.selectedWeaponIndex = -1;
}

void HandleDiscardArmor(InventoryUIState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.equipmentSlotIds.size())) {
        return;
    }
    SetEquipmentSlot(state, index, 0);
    state.selectedEquipmentIndex = -1;
}

void HandleDiscardInventory(InventoryUIState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.inventoryItemIds.size())) {
        return;
    }
    if (state.inventoryItemIds[index] == 0) {
        return;
    }
    ItemCategory type = ItemCategoryFromId(state, state.inventoryItemIds[index]);
    if (type == ItemCategory::Consumable) {
        if (!ReduceConsumableStack(state, index, 1)) {
            ShowMessage(state, "Falha ao descartar o consumivel.");
            return;
        }
        if (state.inventoryItemIds[index] == 0) {
            state.selectedInventoryIndex = -1;
        }
        return;
    }

    ClearInventorySlot(state, index);
    state.selectedInventoryIndex = -1;
}

void HandleSellWeapon(InventoryUIState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.weaponSlotIds.size())) {
        return;
    }
    if (state.weaponSlotIds[index] == 0) {
        ShowMessage(state, "Nenhum item para vender.");
        return;
    }
    int itemId = state.weaponSlotIds[index];
    int saleValue = CalculateSaleValue(state, itemId);
    if (saleValue <= 0) {
        ShowMessage(state, "Item sem valor de venda.");
        return;
    }
    SetWeaponSlot(state, index, 0);
    state.coins += saleValue;
    ShowMessage(state, TextFormat("Vendeu por %d moedas.", saleValue));
    state.selectedWeaponIndex = -1;
}

void HandleSellArmor(InventoryUIState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.equipmentSlotIds.size())) {
        return;
    }
    if (state.equipmentSlotIds[index] == 0) {
        ShowMessage(state, "Nenhum item para vender.");
        return;
    }
    int itemId = state.equipmentSlotIds[index];
    int saleValue = CalculateSaleValue(state, itemId);
    if (saleValue <= 0) {
        ShowMessage(state, "Item sem valor de venda.");
        return;
    }
    SetEquipmentSlot(state, index, 0);
    state.coins += saleValue;
    ShowMessage(state, TextFormat("Vendeu por %d moedas.", saleValue));
    state.selectedEquipmentIndex = -1;
}

void HandleSellInventory(InventoryUIState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.inventoryItemIds.size())) {
        return;
    }
    if (state.inventoryItemIds[index] == 0) {
        ShowMessage(state, "Nenhum item para vender.");
        return;
    }
    int itemId = state.inventoryItemIds[index];
    ItemCategory type = ItemCategoryFromId(state, itemId);
    if (type == ItemCategory::Consumable) {
        int saleValue = CalculateSaleValue(state, itemId, 1);
        if (saleValue <= 0) {
            ShowMessage(state, "Item sem valor de venda.");
            return;
        }
        if (!ReduceConsumableStack(state, index, 1)) {
            ShowMessage(state, "Falha ao atualizar o estoque do consumivel.");
            return;
        }
        state.coins += saleValue;
        ShowMessage(state, TextFormat("Vendeu 1 unidade por %d moedas.", saleValue));
        if (state.inventoryItemIds[index] == 0) {
            state.selectedInventoryIndex = -1;
        } else {
            state.selectedInventoryIndex = index;
        }
        return;
    }

    int quantity = std::max(1, state.inventoryQuantities[index]);
    int saleValue = CalculateSaleValue(state, itemId, quantity);
    if (saleValue <= 0) {
        ShowMessage(state, "Item sem valor de venda.");
        return;
    }
    ClearInventorySlot(state, index);
    state.coins += saleValue;
    ShowMessage(state, TextFormat("Vendeu por %d moedas.", saleValue));
    state.selectedInventoryIndex = -1;
}

void HandleEquipInventory(InventoryUIState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.inventoryItemIds.size())) {
        return;
    }
    int itemId = state.inventoryItemIds[index];
    if (itemId == 0) {
        ShowMessage(state, "Nenhum item para equipar.");
        return;
    }

    ItemCategory type = ItemCategoryFromId(state, itemId);
    if (type == ItemCategory::Weapon) {
        if (ResolveWeaponBlueprint(state, itemId) == nullptr) {
            ShowMessage(state, "Esta arma ainda nao pode ser utilizada.");
            return;
        }
        EnsureWeaponCapacity(state, std::max<size_t>(2, state.weaponSlotIds.size()));
        for (int slot = 0; slot < static_cast<int>(state.weaponSlotIds.size()); ++slot) {
            if (state.weaponSlotIds[slot] == 0) {
                SetWeaponSlot(state, slot, itemId);
                ClearInventorySlot(state, index);
                state.selectedInventoryIndex = -1;
                state.selectedWeaponIndex = slot;
                return;
            }
        }
        ShowMessage(state, "Sem slot de arma disponivel.");
        return;
    }

    if (type == ItemCategory::Armor) {
        EnsureEquipmentCapacity(state, std::max<size_t>(5, state.equipmentSlotIds.size()));
        for (int slot = 0; slot < static_cast<int>(state.equipmentSlotIds.size()); ++slot) {
            if (state.equipmentSlotIds[slot] == 0) {
                SetEquipmentSlot(state, slot, itemId);
                ClearInventorySlot(state, index);
                state.selectedInventoryIndex = -1;
                state.selectedEquipmentIndex = slot;
                return;
            }
        }
        ShowMessage(state, "Sem slot de equipamento disponivel.");
        return;
    }

    ShowMessage(state, "Este item nao pode ser equipado.");
}

void HandleSendInventoryToForge(InventoryUIState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.inventoryItemIds.size())) {
        return;
    }
    int itemId = state.inventoryItemIds[index];
    if (itemId == 0) {
        ShowMessage(state, "Nenhum item selecionado.");
        return;
    }
    ItemCategory type = ItemCategoryFromId(state, itemId);
    if (type == ItemCategory::Consumable) {
        ShowMessage(state, "Consumiveis nao podem ser forjados.");
        return;
    }
    if (state.forgeResultId != 0) {
        ShowMessage(state, "Retire o resultado atual primeiro.");
        state.selectedForgeSlot = 2;
        return;
    }
    int slot = FindEmptyForgeSlot(state);
    if (slot < 0) {
        ShowMessage(state, "A bigorna ja tem dois itens.");
        return;
    }
    state.forgeInputIds[slot] = itemId;
    state.forgeInputNames[slot] = ItemNameFromId(state, itemId);
    state.forgeInputQuantities[slot] = 1;

    int availableQuantity = (index < static_cast<int>(state.inventoryQuantities.size())) ? state.inventoryQuantities[index] : 1;
    if (availableQuantity > 1) {
        SetInventorySlot(state, index, itemId, availableQuantity - 1);
        state.selectedInventoryIndex = index;
    } else {
        ClearInventorySlot(state, index);
        state.selectedInventoryIndex = -1;
    }
    state.selectedForgeSlot = slot;
    RefreshForgeChance(state);
}

void HandleSendWeaponToForge(InventoryUIState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.weaponSlotIds.size())) {
        return;
    }
    int itemId = state.weaponSlotIds[index];
    if (itemId == 0) {
        ShowMessage(state, "Nenhum item selecionado.");
        return;
    }
    if (state.forgeResultId != 0) {
        ShowMessage(state, "Retire o resultado atual primeiro.");
        state.selectedForgeSlot = 2;
        return;
    }
    int slot = FindEmptyForgeSlot(state);
    if (slot < 0) {
        ShowMessage(state, "A bigorna ja tem dois itens.");
        return;
    }
    state.forgeInputIds[slot] = itemId;
    state.forgeInputNames[slot] = ItemNameFromId(state, itemId);
    state.forgeInputQuantities[slot] = 1;
    SetWeaponSlot(state, index, 0);
    state.selectedWeaponIndex = -1;
    state.selectedForgeSlot = slot;
    RefreshForgeChance(state);
}

void HandleSendArmorToForge(InventoryUIState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.equipmentSlotIds.size())) {
        return;
    }
    int itemId = state.equipmentSlotIds[index];
    if (itemId == 0) {
        ShowMessage(state, "Nenhum item selecionado.");
        return;
    }
    if (state.forgeResultId != 0) {
        ShowMessage(state, "Retire o resultado atual primeiro.");
        state.selectedForgeSlot = 2;
        return;
    }
    int slot = FindEmptyForgeSlot(state);
    if (slot < 0) {
        ShowMessage(state, "A bigorna ja tem dois itens.");
        return;
    }
    state.forgeInputIds[slot] = itemId;
    state.forgeInputNames[slot] = ItemNameFromId(state, itemId);
    state.forgeInputQuantities[slot] = 1;
    SetEquipmentSlot(state, index, 0);
    state.selectedEquipmentIndex = -1;
    state.selectedForgeSlot = slot;
    RefreshForgeChance(state);
}

void HandleRemoveFromForge(InventoryUIState& state, int slot) {
    if (slot == 0 || slot == 1) {
        int itemId = state.forgeInputIds[slot];
        if (itemId == 0) {
            return;
        }
        int target = AddItemToInventory(state, itemId, std::max(1, state.forgeInputQuantities[slot]));
        if (target < 0) {
            ShowMessage(state, "Sem espaco no inventario.");
            return;
        }
        ClearForgeSlot(state, slot);
        state.selectedForgeSlot = -1;
        state.selectedInventoryIndex = target;
        RefreshForgeChance(state);
        return;
    }

    if (slot == 2) {
        if (state.forgeResultId == 0) {
            return;
        }
        int target = AddItemToInventory(state, state.forgeResultId, state.forgeResultQuantity);
        if (target < 0) {
            ShowMessage(state, "Sem espaco no inventario.");
            return;
        }
        ClearForgeResult(state);
        state.selectedForgeSlot = -1;
        state.selectedInventoryIndex = target;
        RefreshForgeChance(state);
    }
}

void HandleBuyFromShop(InventoryUIState& state, int index) {
    if (index < 0 || index >= static_cast<int>(state.shopItemIds.size())) {
        return;
    }
    if (index >= static_cast<int>(state.shopStock.size()) || state.shopStock[index] <= 0) {
        ShowMessage(state, "Este item nao esta mais disponivel.");
        return;
    }
    if (state.coins < state.shopPrices[index]) {
        ShowMessage(state, "Moedas insuficientes.");
        return;
    }
    int itemId = state.shopItemIds[index];
    if (itemId == 0) {
        ShowMessage(state, "Item indisponivel.");
        return;
    }
    int slot = AddItemToInventory(state, itemId, 1);
    if (slot < 0) {
        ShowMessage(state, "Sem espaco no inventario.");
        return;
    }
    state.coins -= state.shopPrices[index];
    state.shopStock[index] = std::max(0, state.shopStock[index] - 1);
    ShowMessage(state, "Compra realizada.");
    state.selectedInventoryIndex = slot;
    state.selectedShopIndex = index;
}

void DrawMultilineText(const Rectangle& area, const std::string& text, float fontSize);

void DrawSlot(const InventoryUIState& state,
              Rectangle rect,
              const std::string& label,
              bool selected,
              int itemId,
              int quantity = -1,
              bool showQuantity = true) {
    DrawRectangleRec(rect, Color{54, 58, 72, 220});
    DrawRectangleLinesEx(rect, 2.0f, ResolveBorderColor(state, itemId));

    if (selected) {
        Rectangle selectionRect{rect.x - 3.0f, rect.y - 3.0f, rect.width + 6.0f, rect.height + 6.0f};
        DrawRectangleLinesEx(selectionRect, 1.0f, Color{255, 230, 160, 255});
    }

    if (!label.empty()) {
        const Font font = GetFontDefault();
        const float fontSize = 16.0f;
        Rectangle textBounds{rect.x + 6.0f, rect.y + 6.0f, rect.width - 12.0f, rect.height - 12.0f};
        std::istringstream words(label);
        std::string word;
        std::string currentLine;
        std::string wrapped;
        while (words >> word) {
            std::string candidate = currentLine.empty() ? word : currentLine + " " + word;
            float width = MeasureTextEx(font, candidate.c_str(), fontSize, kBodyTextSpacing).x;
            if (width <= textBounds.width) {
                currentLine = candidate;
            } else {
                if (!wrapped.empty()) {
                    wrapped += '\n';
                }
                if (!currentLine.empty()) {
                    wrapped += currentLine;
                    currentLine = word;
                } else {
                    wrapped += word;
                    currentLine.clear();
                }
            }
        }
        if (!currentLine.empty()) {
            if (!wrapped.empty()) {
                wrapped += '\n';
            }
            wrapped += currentLine;
        }

        DrawMultilineText(textBounds, wrapped, fontSize);
    }

    if (showQuantity && quantity >= 0) {
        std::string qty = std::to_string(quantity);
        Vector2 measure = MeasureTextEx(GetFontDefault(), qty.c_str(), 14.0f, 0.0f);
        Vector2 pos{rect.x + rect.width - measure.x - 5.0f, rect.y + rect.height - measure.y - 3.0f};
        DrawTextEx(GetFontDefault(), qty.c_str(), pos, 14.0f, 0.0f, Color{210, 225, 255, 255});
    }
}

bool SlotClicked(Rectangle rect) {
    return IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), rect);
}

void DrawAttributeLabel(Vector2 position, const std::string& label, int value) {
    const float fontSize = 20.0f;
    std::string text = label + ": " + std::to_string(value);
    DrawTextEx(GetFontDefault(), text.c_str(), position, fontSize, kBodyTextSpacing, Color{58, 68, 96, 255});
}

void DrawAttributeLabel(Vector2 position, const std::string& label, float value, int decimals = 2) {
    const float fontSize = 20.0f;
    std::string text = label + ": " + std::string(TextFormat("%0.*f", decimals, value));
    DrawTextEx(GetFontDefault(), text.c_str(), position, fontSize, kBodyTextSpacing, Color{58, 68, 96, 255});
}

void DrawMultilineText(const Rectangle& area, const std::string& text, float fontSize) {
    const Font font = GetFontDefault();
    float lineSpacing = 6.0f;
    float y = area.y;
    size_t start = 0;
    while (start < text.size() && y < area.y + area.height - fontSize) {
        size_t end = text.find('\n', start);
        std::string line = text.substr(start, (end == std::string::npos) ? text.size() - start : end - start);
        DrawTextEx(font, line.c_str(), Vector2{area.x, y}, fontSize, kBodyTextSpacing, Color{58, 68, 96, 255});
        y += fontSize + lineSpacing;
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
}

} // namespace

void InitializeInventoryUIDummyData(InventoryUIState& state) {
    state.items.clear();
    state.itemNameToId.clear();
    state.weaponSlotIds.clear();
    state.weaponSlots.clear();
    state.equipmentSlotIds.clear();
    state.equipmentSlots.clear();
    state.inventoryItemIds.clear();
    state.inventoryItems.clear();
    state.inventoryQuantities.clear();
    state.inventoryTypes.clear();
    state.shopItemIds.clear();
    state.shopItems.clear();
    state.shopPrices.clear();
    state.shopTypes.clear();
    state.shopStock.clear();
    state.sellPriceMultiplier = 0.2f; // Reset base sell rate when seeding dummy data
    state.forgeBaseCost = 0;
    state.forgeSuccessChance = 0.0f;
    state.forgeBroken = false;
    state.forgeRecipes.clear();
    state.forgeInputIds = {0, 0};
    state.forgeInputNames = {"", ""};
    state.forgeInputQuantities = {0, 0};
    ClearForgeResult(state);

    auto addItem = [&](int id,
                       const char* name,
                       ItemCategory category,
                       const char* description,
                       int rarity,
                       int baseValue,
                       const WeaponBlueprint* blueprint = nullptr) {
        ItemDefinition def{};
        def.id = id;
        def.name = name;
        def.category = category;
        def.description = description;
        def.rarity = std::max(1, rarity);
        def.baseValue = std::max(0, baseValue);
        def.value = CalculateItemPrice(def.rarity, def.baseValue);
        def.weaponBlueprint = blueprint;
        state.items.push_back(std::move(def));
        state.itemNameToId[name] = id;
    };

    addItem(1,  "Espada Curta",        ItemCategory::Weapon,      "Lamina equilibrada para iniciantes.", 2, 80,  &GetEspadaCurtaWeaponBlueprint());
    addItem(2,  "Machadinha",          ItemCategory::Weapon,      "Machado leve de uma mao.",        2, 70,  &GetMachadinhaWeaponBlueprint());
    addItem(3,  "Arco Simples",        ItemCategory::Weapon,      "Arco feito de madeira tratada.",  2, 100, &GetArcoSimplesWeaponBlueprint());
    addItem(4,  "Cajado de Carvalho",  ItemCategory::Weapon,      "Canaliza energia natural.",       3, 100, &GetCajadoDeCarvalhoWeaponBlueprint());
    addItem(21, "Broquel",             ItemCategory::Weapon,      "Escudo curto reforcado para contra-ataques.", 3, 90, &GetBroquelWeaponBlueprint());
    addItem(5,  "Escudo de Madeira",   ItemCategory::Armor,       "Protecao basica contra ataques.", 2, 50);
    addItem(6,  "Peitoral de Couro",   ItemCategory::Armor,       "Armadura leve e flexivel.",       3, 70);
    addItem(7,  "Elmo Simples",        ItemCategory::Armor,       "Protecao modesta para a cabeca.", 2, 55);
    addItem(8,  "Luvas Reforcadas",    ItemCategory::Armor,       "Garantem melhor empunhadura.",    2, 40);
    addItem(9,  "Botas Ageis",         ItemCategory::Armor,       "Aumentam a mobilidade.",         2, 45);
    addItem(10, "Amuleto Antigo",      ItemCategory::Armor,       "Relicario com energia selada.",   4, 130);
    addItem(11, "Pocao de Cura",       ItemCategory::Consumable,  "Recupera uma porcao de vida.",    1, 25);
    addItem(12, "Pocao de Energia",    ItemCategory::Consumable,  "Restaura vigor e foco.",          1, 35);
    addItem(13, "Lingote de Ferro",    ItemCategory::Material,    "Base para forja de armas.",       1, 10);
    addItem(14, "Gema Brilhante",      ItemCategory::Material,    "Rara e cheia de energia.",        3, 30);
    addItem(15, "Pergaminho Runico",   ItemCategory::Material,    "Inscrito com runas antigas.",     2, 35);
    addItem(16, "Essencia Arcana",     ItemCategory::Material,    "Concentrado de mana pura.",       3, 25);
    addItem(17, "Madeira Refinada",    ItemCategory::Material,    "Polida e resistente.",            1, 5);
    addItem(18, "Couro Tratado",       ItemCategory::Material,    "Pronto para virar armadura.",     1, 8);
    addItem(19, "Espada Runica",       ItemCategory::Weapon,      "Lamina encantada pelas runas.",    5, 320, &GetEspadaRunicaWeaponBlueprint());
    addItem(20, "Amuleto Radiante",    ItemCategory::Armor,       "Canaliza luz protetora.",         5, 280);

    EnsureWeaponCapacity(state, 2);
    SetWeaponSlot(state, 0, 1);
    SetWeaponSlot(state, 1, 3);

    EnsureEquipmentCapacity(state, 5);
    SetEquipmentSlot(state, 0, 6);
    SetEquipmentSlot(state, 1, 7);
    SetEquipmentSlot(state, 2, 8);
    SetEquipmentSlot(state, 3, 9);
    SetEquipmentSlot(state, 4, 10);

    const std::vector<std::pair<int, int>> inventorySeed{
        {1, 1}, {2, 1}, {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}, {8, 1}, {9, 1}, {10, 1},
    {11, 3}, {12, 2}, {13, 4}, {14, 2}, {15, 2}, {16, 2}, {17, 3}, {18, 3}, {19, 1}, {20, 1}, {21, 1}
    };

    state.inventoryItemIds.resize(24, 0);
    state.inventoryItems.resize(24);
    state.inventoryQuantities.resize(24, 0);
    state.inventoryTypes.resize(24, ItemCategory::None);

    for (size_t i = 0; i < inventorySeed.size(); ++i) {
        SetInventorySlot(state, static_cast<int>(i), inventorySeed[i].first, inventorySeed[i].second);
    }

    state.shopItemIds.clear();
    state.shopItems.clear();
    state.shopPrices.clear();
    state.shopTypes.clear();

    state.shopRollsLeft = 1;
    RollShopInventory(state);

    state.forgeRecipes[MakeForgeKey(13, 15)] = 19; // Lingote + Pergaminho -> Espada Runica
    state.forgeRecipes[MakeForgeKey(10, 14)] = 20; // Amuleto + Gema -> Amuleto Radiante

    state.coins = 125;
    RefreshForgeChance(state);
}

void RenderInventoryUI(InventoryUIState& state,
                       const PlayerCharacter& player,
                       const WeaponState& leftWeapon,
                       const WeaponState& rightWeapon,
                       Vector2 screenSize) {
    const int prevTextColor = GuiGetStyle(DEFAULT, TEXT_COLOR_NORMAL);
    const int prevFocusColor = GuiGetStyle(DEFAULT, TEXT_COLOR_FOCUSED);
    const int prevPressedColor = GuiGetStyle(DEFAULT, TEXT_COLOR_PRESSED);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0x3A445CFF);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, 0x243149FF);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, 0x1B2538FF);
    EnsureInventoryMeta(state);
    EnsureWeaponCapacity(state, std::max<size_t>(2, state.weaponSlotIds.size()));
    EnsureEquipmentCapacity(state, std::max<size_t>(5, state.equipmentSlotIds.size()));
    EnsureShopCapacity(state, state.shopItemIds.size());
    if (state.feedbackTimer > 0.0f) {
        state.feedbackTimer -= GetFrameTime();
        if (state.feedbackTimer <= 0.0f) {
            state.feedbackTimer = 0.0f;
            state.feedbackMessage.clear();
        }
    }
    const float windowWidth = std::min(1440.0f, screenSize.x - 140.0f);
    const float windowHeight = std::min(860.0f, screenSize.y - 140.0f);
    Rectangle windowRect{
        screenSize.x * 0.5f - windowWidth * 0.5f,
        screenSize.y * 0.5f - windowHeight * 0.5f,
        windowWidth,
        windowHeight
    };

    GuiPanel(windowRect, nullptr);

    // Top buttons
        Rectangle menuButton{
            windowRect.x - 120.0f,
            windowRect.y - 44.0f,
            140.0f, // Ajuste aqui para alterar a largura do botao Menu
            50.0f   // Ajuste aqui para alterar a altura do botao Menu
        }; // Ajuste aqui para mover o botao Menu
    GuiButton(menuButton, "Menu");

        Rectangle closeButton{
            windowRect.x + windowRect.width + 10.0f,
            windowRect.y - 44.0f,
            140.0f, // Ajuste aqui para alterar a largura do botao Fechar
            50.0f   // Ajuste aqui para alterar a altura do botao Fechar
        }; // Ajuste aqui para mover o botao Fechar
    if (GuiButton(closeButton, "Fechar")) {
        state.open = false;
        return;
    }

    const float padding = 22.0f;
    Rectangle attributesRect{windowRect.x + padding, windowRect.y + padding, 360.0f, windowRect.height - padding * 2.0f};
    GuiGroupBox(attributesRect, "Atributos");

    Vector2 attrPos{attributesRect.x + 20.0f, attributesRect.y + 36.0f};
    DrawAttributeLabel(attrPos, "Vida", static_cast<int>(std::round(player.currentHealth)));
    attrPos.y += 26.0f;
    DrawAttributeLabel(attrPos, "Vida Max", static_cast<int>(std::round(player.derivedStats.maxHealth)));
    attrPos.y += 32.0f;

    DrawAttributeLabel(attrPos, "Poder", player.totalAttributes.primary.poder);
    attrPos.y += 22.0f;
    DrawAttributeLabel(attrPos, "Defesa", player.totalAttributes.primary.defesa);
    attrPos.y += 22.0f;
    DrawAttributeLabel(attrPos, "Vigor", player.totalAttributes.primary.vigor);
    attrPos.y += 22.0f;
    DrawAttributeLabel(attrPos, "Velocidade", player.totalAttributes.primary.velocidade);
    attrPos.y += 22.0f;
    DrawAttributeLabel(attrPos, "Destreza", player.totalAttributes.primary.destreza);
    attrPos.y += 22.0f;
    DrawAttributeLabel(attrPos, "Inteligencia", player.totalAttributes.primary.inteligencia);

    attrPos.y += 32.0f;
    DrawAttributeLabel(attrPos, "Letalidade", player.totalAttributes.secondary.letalidade, 2);
    attrPos.y += 22.0f;
    DrawAttributeLabel(attrPos, "Sorte", player.totalAttributes.secondary.sorte, 2);
    attrPos.y += 22.0f;
    DrawAttributeLabel(attrPos, "Vampirismo", player.totalAttributes.secondary.vampirismo, 2);

    Rectangle contentRect{
        attributesRect.x + attributesRect.width + padding,
        windowRect.y + padding,
        windowRect.width - attributesRect.width - padding * 3.0f,
        windowRect.height - padding * 2.0f
    };
    GuiPanel(contentRect, nullptr);

    // Tabs for modes
    const char* modes = "Inventario;Bigorna;Loja";
    Rectangle tabArea{contentRect.x + 10.0f, contentRect.y + 6.0f, 420.0f, 32.0f};
    int modeIndex = static_cast<int>(state.mode);
    GuiToggleGroup(tabArea, modes, &modeIndex);
    state.mode = static_cast<InventoryViewMode>(modeIndex);

    Rectangle weaponsLabelRect{contentRect.x + 10.0f, contentRect.y + 52.0f, 140.0f, 22.0f};
    GuiLabel(weaponsLabelRect, "Armas");

    const float slotSize = 64.0f;
    const float slotSpacing = 12.0f;

    // Weapons slots (2)
    for (int i = 0; i < 2; ++i) {
        Rectangle slotRect{
            contentRect.x + 10.0f + (slotSize + slotSpacing) * i,
            contentRect.y + 68.0f,
            slotSize,
            slotSize
        };
        bool selected = (state.selectedWeaponIndex == i);
        std::string label = (i < static_cast<int>(state.weaponSlots.size())) ? state.weaponSlots[i] : "";
        int weaponId = (i < static_cast<int>(state.weaponSlotIds.size())) ? state.weaponSlotIds[i] : 0;
        DrawSlot(state, slotRect, label, selected, weaponId);
        if (SlotClicked(slotRect)) {
            state.selectedWeaponIndex = i;
            state.selectedInventoryIndex = -1;
            state.selectedEquipmentIndex = -1;
            state.selectedShopIndex = -1;
            state.selectedForgeSlot = -1;
        }
    }

    // Equipment slots row (5)
    Rectangle equipLabelRect{contentRect.x + 10.0f, weaponsLabelRect.y + 28.0f + slotSize, 160.0f, 22.0f};
    GuiLabel(equipLabelRect, "Equipamento");

    for (int i = 0; i < 5; ++i) {
        Rectangle slotRect{
            contentRect.x + 10.0f + (slotSize + slotSpacing) * i,
            equipLabelRect.y + 22.0f,
            slotSize,
            slotSize
        };
        bool selected = (state.selectedEquipmentIndex == i);
        std::string label = (i < static_cast<int>(state.equipmentSlots.size())) ? state.equipmentSlots[i] : "";
        int equipmentId = (i < static_cast<int>(state.equipmentSlotIds.size())) ? state.equipmentSlotIds[i] : 0;
        DrawSlot(state, slotRect, label, selected, equipmentId);
        if (SlotClicked(slotRect)) {
            state.selectedEquipmentIndex = i;
            state.selectedWeaponIndex = -1;
            state.selectedInventoryIndex = -1;
            state.selectedShopIndex = -1;
            state.selectedForgeSlot = -1;
        }
    }

    Rectangle inventoryLabelRect{contentRect.x + 10.0f, equipLabelRect.y + 30.0f + slotSize, 160.0f, 22.0f};
    GuiLabel(inventoryLabelRect, "Inventario");

    const int inventoryColumns = 8;
    const int inventoryRows = 3;
    for (int row = 0; row < inventoryRows; ++row) {
        for (int col = 0; col < inventoryColumns; ++col) {
            int index = row * inventoryColumns + col;
            Rectangle slotRect{
                contentRect.x + 10.0f + (slotSize + slotSpacing) * col,
                inventoryLabelRect.y + 20.0f + (slotSize + slotSpacing) * row,
                slotSize,
                slotSize
            };
            bool selected = (state.selectedInventoryIndex == index);
            std::string label = (index < static_cast<int>(state.inventoryItems.size())) ? state.inventoryItems[index] : "";
            ItemCategory slotType = (index < static_cast<int>(state.inventoryTypes.size())) ? state.inventoryTypes[index] : ItemCategory::None;
            bool showQuantity = (slotType == ItemCategory::Consumable || slotType == ItemCategory::Material);
            int quantity = -1;
            if (showQuantity && index < static_cast<int>(state.inventoryQuantities.size())) {
                quantity = state.inventoryQuantities[index];
            }
            if (label.empty()) {
                quantity = -1;
            }
            int itemId = (index < static_cast<int>(state.inventoryItemIds.size())) ? state.inventoryItemIds[index] : 0;
            DrawSlot(state, slotRect, label, selected, itemId, quantity, showQuantity);
            if (SlotClicked(slotRect)) {
                state.selectedInventoryIndex = index;
                state.selectedWeaponIndex = -1;
                state.selectedEquipmentIndex = -1;
                state.selectedShopIndex = -1;
                state.selectedForgeSlot = -1;
            }
        }
    }

    Rectangle coinsLabel{contentRect.x + 10.0f, inventoryLabelRect.y + 20.0f + (slotSize + slotSpacing) * inventoryRows + 12.0f, 180.0f, 24.0f};
    GuiLabel(coinsLabel, TextFormat("Moedas: %d", state.coins));

    // Detail panel on the right
    Rectangle detailRect{
        contentRect.x + contentRect.width - 320.0f,
        contentRect.y + 50.0f,
        310.0f,
        contentRect.height - 70.0f
    };
    GuiGroupBox(detailRect, "Detalhes");

    Rectangle detailContent{detailRect.x + 12.0f, detailRect.y + 26.0f, detailRect.width - 24.0f, detailRect.height - 38.0f};

    std::string detailText = "Clique em um item para ver seus atributos";
    int detailItemId = 0;

    if (state.selectedWeaponIndex >= 0 && state.selectedWeaponIndex < static_cast<int>(state.weaponSlots.size())) {
        const WeaponState& selectedWeapon = (state.selectedWeaponIndex == 0) ? leftWeapon : rightWeapon;
        if (selectedWeapon.blueprint != nullptr) {
            detailText = FormatWeaponBlueprintDetails(*selectedWeapon.blueprint);
            detailText += "\n\nStatus atual:";
            detailText += "\n- Dano por disparo: " + FormatFloat(selectedWeapon.derived.damagePerShot, 1);
            detailText += "\n- Intervalo entre disparos: " + FormatFloat(selectedWeapon.derived.attackIntervalSeconds, 2) + " s";
            detailText += "\n- Critico efetivo: " + FormatFloat(selectedWeapon.derived.criticalChance * 100.0f, 1) + "% (x" + FormatFloat(selectedWeapon.derived.criticalMultiplier, 2) + ")";
        } else {
            detailText = "Arma: Slot vazio";
        }
        if (state.selectedWeaponIndex < static_cast<int>(state.weaponSlotIds.size())) {
            detailItemId = state.weaponSlotIds[state.selectedWeaponIndex];
        }
    } else if (state.selectedEquipmentIndex >= 0 && state.selectedEquipmentIndex < static_cast<int>(state.equipmentSlots.size())) {
        detailText = "Equipamento: " + state.equipmentSlots[state.selectedEquipmentIndex] + "\nBônus: TBD";
        if (state.selectedEquipmentIndex < static_cast<int>(state.equipmentSlotIds.size())) {
            detailItemId = state.equipmentSlotIds[state.selectedEquipmentIndex];
        }
    } else if (state.selectedInventoryIndex >= 0 && state.selectedInventoryIndex < static_cast<int>(state.inventoryItems.size())) {
        if (state.selectedInventoryIndex < static_cast<int>(state.inventoryItemIds.size())) {
            detailItemId = state.inventoryItemIds[state.selectedInventoryIndex];
        }

        const ItemDefinition* def = FindItemDefinition(state, detailItemId);
        if (def != nullptr) {
            if (def->category == ItemCategory::Weapon && def->weaponBlueprint != nullptr) {
                detailText = FormatWeaponBlueprintDetails(*def->weaponBlueprint);
                if (!def->description.empty()) {
                    detailText += "\nDescricao: " + def->description;
                }
                detailText += "\nValor: " + std::to_string(def->value);
            } else {
                detailText = "Item: " + def->name;
                if (!def->description.empty()) {
                    detailText += "\nDescricao: " + def->description;
                }
                detailText += "\nValor: " + std::to_string(def->value);
            }
        } else {
            detailText = "Item: Slot vazio";
        }
    } else if (state.selectedShopIndex >= 0 && state.selectedShopIndex < static_cast<int>(state.shopItems.size())) {
        int stock = (state.selectedShopIndex < static_cast<int>(state.shopStock.size())) ? state.shopStock[state.selectedShopIndex] : 0;
        detailText = TextFormat("Loja: %s\nPreco: %d\nEstoque: %d",
                                state.shopItems[state.selectedShopIndex].c_str(),
                                state.shopPrices[state.selectedShopIndex],
                                std::max(0, stock));
        if (state.selectedShopIndex < static_cast<int>(state.shopItemIds.size())) {
            detailItemId = state.shopItemIds[state.selectedShopIndex];
        }
    } else if (state.selectedForgeSlot == 0 || state.selectedForgeSlot == 1) {
        int slot = state.selectedForgeSlot;
        if (slot >= 0 && slot < 2 && state.forgeInputIds[slot] != 0) {
            std::string name = state.forgeInputNames[slot];
            if (name.empty()) {
                name = ItemNameFromId(state, state.forgeInputIds[slot]);
            }
            detailText = "Bigorna: " + name + "\nStatus: Pronto para forjar";
            detailItemId = state.forgeInputIds[slot];
        }
    } else if (state.selectedForgeSlot == 2 && state.forgeResultId != 0) {
        std::string name = state.forgeResultName.empty() ? ItemNameFromId(state, state.forgeResultId) : state.forgeResultName;
        detailText = "Resultado: " + name + "\nStatus: Aguarda coleta";
        detailItemId = state.forgeResultId;
    }

    AppendForgeCombos(state, detailItemId, detailText);

    DrawMultilineText(detailContent, detailText, 18.0f);

    Rectangle actionButtonLeft{detailRect.x + 12.0f, detailRect.y + detailRect.height - 40.0f, 100.0f, 28.0f};
    Rectangle actionButtonRight{detailRect.x + detailRect.width - 112.0f, detailRect.y + detailRect.height - 40.0f, 100.0f, 28.0f};

    float bottomAreaTop = coinsLabel.y + 36.0f;
    float bottomAreaHeight = (contentRect.y + contentRect.height) - bottomAreaTop - 12.0f;
    bottomAreaHeight = std::max(0.0f, bottomAreaHeight);
    Rectangle bottomArea{
        contentRect.x + 10.0f,
        bottomAreaTop,
        std::max(0.0f, detailRect.x - contentRect.x - 30.0f),
        bottomAreaHeight
    };

    if (state.mode == InventoryViewMode::Forge && bottomArea.width > 40.0f && bottomArea.height > 40.0f) {
        GuiGroupBox(bottomArea, "Bigorna");

        float slotRowY = bottomArea.y + 48.0f;
        float slotStartX = bottomArea.x + 20.0f; // Ajuste aqui para deslocar os slots de forja (X)
        Rectangle inputSlotA{slotStartX, slotRowY, slotSize, slotSize};
        Rectangle inputSlotB{slotStartX + slotSize + slotSpacing, slotRowY, slotSize, slotSize};
        Rectangle arrowRect{inputSlotB.x + slotSize + 24.0f, slotRowY + slotSize * 0.5f - 20.0f, 40.0f, 40.0f};
        Rectangle resultSlot{arrowRect.x + arrowRect.width + 24.0f, slotRowY, slotSize, slotSize};

        DrawSlot(state,
                 inputSlotA,
                 state.forgeInputIds[0] == 0 ? "Slot 1" : (state.forgeInputNames[0].empty() ? ItemNameFromId(state, state.forgeInputIds[0]) : state.forgeInputNames[0]),
                 state.selectedForgeSlot == 0,
                 state.forgeInputIds[0],
                 -1,
                 false);
        DrawSlot(state,
                 inputSlotB,
                 state.forgeInputIds[1] == 0 ? "Slot 2" : (state.forgeInputNames[1].empty() ? ItemNameFromId(state, state.forgeInputIds[1]) : state.forgeInputNames[1]),
                 state.selectedForgeSlot == 1,
                 state.forgeInputIds[1],
                 -1,
                 false);

        DrawRectangleLinesEx(arrowRect, 2.0f, Color{200, 200, 220, 255});
        DrawTextEx(GetFontDefault(), "=>", Vector2{arrowRect.x + 8.0f, arrowRect.y + 8.0f}, 28.0f, 0.0f, Color{230, 230, 240, 255});

        bool showResultQuantity = state.forgeResultQuantity > 1;
        DrawSlot(state,
                 resultSlot,
                 state.forgeResultId == 0 ? "Resultado" : (state.forgeResultName.empty() ? ItemNameFromId(state, state.forgeResultId) : state.forgeResultName),
                 state.selectedForgeSlot == 2,
                 state.forgeResultId,
                 showResultQuantity ? state.forgeResultQuantity : -1,
                 showResultQuantity);

        if (state.forgeInputIds[0] != 0 && SlotClicked(inputSlotA)) {
            state.selectedForgeSlot = 0;
            state.selectedInventoryIndex = -1;
            state.selectedWeaponIndex = -1;
            state.selectedEquipmentIndex = -1;
            state.selectedShopIndex = -1;
        } else if (state.forgeInputIds[1] != 0 && SlotClicked(inputSlotB)) {
            state.selectedForgeSlot = 1;
            state.selectedInventoryIndex = -1;
            state.selectedWeaponIndex = -1;
            state.selectedEquipmentIndex = -1;
            state.selectedShopIndex = -1;
        } else if (state.forgeResultId != 0 && SlotClicked(resultSlot)) {
            state.selectedForgeSlot = 2;
            state.selectedInventoryIndex = -1;
            state.selectedWeaponIndex = -1;
            state.selectedEquipmentIndex = -1;
            state.selectedShopIndex = -1;
        }

        RefreshForgeChance(state);

        float chanceWidth = std::min(220.0f, bottomArea.x + bottomArea.width - (resultSlot.x + slotSize + 24.0f) - 20.0f);
        if (chanceWidth > 60.0f) {
            Rectangle chanceRect{resultSlot.x + slotSize + 60.0f, slotRowY + slotSize * 0.5f - 16.0f, chanceWidth, 32.0f}; // Ajuste aqui para mover a barra de chance
            if (state.forgeBroken) {
                DrawRectangleRec(chanceRect, Color{160, 32, 32, 230});
                DrawRectangleLinesEx(chanceRect, 2.0f, Color{90, 16, 16, 255});
                DrawTextEx(GetFontDefault(), "falha!", Vector2{chanceRect.x + 16.0f, chanceRect.y + 6.0f}, 24.0f, 0.0f, Color{255, 255, 255, 255});
            } else {
                GuiProgressBar(chanceRect, nullptr, nullptr, &state.forgeSuccessChance, 0.0f, 1.0f);
                DrawTextEx(GetFontDefault(), TextFormat("%d%%", static_cast<int>(state.forgeSuccessChance * 100.0f)),
                           Vector2{chanceRect.x + chanceRect.width * 0.5f - 18.0f, chanceRect.y + 6.0f}, 24.0f, 0.0f, Color{40, 48, 68, 255});
            }
        }

        float adjustTop = slotRowY + slotSize + 32.0f;
        Rectangle adjustRect{bottomArea.x + 20.0f, adjustTop, bottomArea.width - 40.0f, 112.0f};
        if (adjustRect.height > 64.0f) {
            GuiGroupBox(adjustRect, "Ajustes");

            Rectangle valueBoxRect{adjustRect.x + 16.0f, adjustRect.y + 36.0f, 92.0f, 36.0f};
            if (GuiValueBox(valueBoxRect, "", &state.forgeBaseCost, 0, 9999, state.forgeEditingCost)) {
                state.forgeEditingCost = !state.forgeEditingCost;
            }

            Rectangle stepButtonsRect{valueBoxRect.x + valueBoxRect.width + 22.0f, valueBoxRect.y - 6.0f, 88.0f, 30.0f};
            if (GuiButton(stepButtonsRect, "-10")) {
                state.forgeBaseCost = std::max(0, state.forgeBaseCost - 10);
            }
            stepButtonsRect.y += 34.0f;
            if (GuiButton(stepButtonsRect, "+10")) {
                state.forgeBaseCost = std::min(9999, state.forgeBaseCost + 10);
            }

            Rectangle onesButtonsRect{stepButtonsRect.x + 96.0f, valueBoxRect.y - 6.0f, 72.0f, 30.0f};
            if (GuiButton(onesButtonsRect, "-1")) {
                state.forgeBaseCost = std::max(0, state.forgeBaseCost - 1);
            }
            onesButtonsRect.y += 34.0f;
            if (GuiButton(onesButtonsRect, "+1")) {
                state.forgeBaseCost = std::min(9999, state.forgeBaseCost + 1);
            }

            Rectangle forgeButton{
                adjustRect.x + adjustRect.width * 0.5f + 100.0f,
                valueBoxRect.y,
                96.0f,
                36.0f
            };
            bool disableForge = state.forgeBroken;
            if (disableForge) {
                GuiDisable();
            }
            if (GuiButton(forgeButton, "Forjar")) {
                AttemptForge(state);
            }
            if (disableForge) {
                GuiEnable();
            }
        }

        state.forgeBaseCost = std::clamp(state.forgeBaseCost, 0, 9999);
        RefreshForgeChance(state);
    } else if (state.mode == InventoryViewMode::Shop && bottomArea.width > 40.0f && bottomArea.height > 40.0f) {
        GuiGroupBox(bottomArea, "Loja");
        float startX = bottomArea.x + 190.0f; // Ajuste aqui para deslocar os slots da loja no eixo X
        float startY = bottomArea.y + 44.0f; // Ajuste aqui para deslocar os slots da loja no eixo Y
        int columns = std::max(1, static_cast<int>((bottomArea.width - 40.0f) / (slotSize + slotSpacing)));
        columns = std::max(1, std::min(columns, 5));

        const float slotVerticalStep = slotSize + slotSpacing + 44.0f;
        for (int i = 0; i < static_cast<int>(state.shopItems.size()); ++i) {
            int col = columns > 0 ? i % columns : 0;
            int row = columns > 0 ? i / columns : 0;
            float slotX = startX + col * (slotSize + slotSpacing); // Ajuste fino por slot (X)
            float slotY = startY + row * slotVerticalStep; // Ajuste fino por slot (Y)
            if (slotY + slotSize + 44.0f > bottomArea.y + bottomArea.height - 12.0f) {
                break;
            }
            Rectangle slotRect{slotX, slotY, slotSize, slotSize};
            bool selected = (state.selectedShopIndex == i);
            int stock = (i < static_cast<int>(state.shopStock.size())) ? state.shopStock[i] : 0;
            int shopItemId = (i < static_cast<int>(state.shopItemIds.size())) ? state.shopItemIds[i] : 0;
            ItemCategory shopType = (i < static_cast<int>(state.shopTypes.size())) ? state.shopTypes[i] : ItemCategory::None;
            bool showQuantity = (shopType == ItemCategory::Consumable || shopType == ItemCategory::Material);
            DrawSlot(state, slotRect, state.shopItems[i], selected, shopItemId, std::max(0, stock), showQuantity);
            if (stock <= 0) {
                DrawRectangleRec(slotRect, Color{0, 0, 0, 140});
                DrawRectangleLinesEx(slotRect, 2.0f, ResolveBorderColor(state, shopItemId));
            }
            if (SlotClicked(slotRect)) {
                state.selectedShopIndex = i;
                state.selectedInventoryIndex = -1;
                state.selectedWeaponIndex = -1;
                state.selectedEquipmentIndex = -1;
                state.selectedForgeSlot = -1;
            }
            Rectangle priceRect{slotRect.x, slotRect.y + slotSize + 6.0f, slotSize, 20.0f};
            GuiLabel(priceRect, TextFormat("%d", state.shopPrices[i]));
            Rectangle stockRect{slotRect.x, priceRect.y + 18.0f, slotSize, 20.0f};
            GuiLabel(stockRect, TextFormat("Estoque: %d", std::max(0, stock)));
        }

        int totalRows = columns > 0 ? (static_cast<int>(state.shopItems.size()) + columns - 1) / columns : 0;
        float rerollButtonWidth = 180.0f;  // Ajuste aqui para alterar a largura do botao re-roll
        float rerollButtonHeight = 38.0f;  // Ajuste aqui para alterar a altura do botao re-roll
        float rerollButtonX = bottomArea.x + bottomArea.width * 0.5f - rerollButtonWidth * 0.5f; // Ajuste aqui para mover o botao re-roll no eixo X
        float rerollButtonY = startY + totalRows * slotVerticalStep + 20.0f;       // Ajuste aqui para mover o botao re-roll no eixo Y
        Rectangle rerollButton{rerollButtonX, rerollButtonY, rerollButtonWidth, rerollButtonHeight};

        const int rerollButtonTextSize = 22; // Ajuste aqui para alterar o tamanho da fonte do botao re-roll
        int previousButtonTextSize = GuiGetStyle(BUTTON, TEXT_SIZE);
        GuiSetStyle(BUTTON, TEXT_SIZE, rerollButtonTextSize);

        bool hasRerolls = state.shopRollsLeft > 0;
        if (!hasRerolls) {
            GuiDisable();
        }
        if (GuiButton(rerollButton, TextFormat("re-roll %dx", std::max(0, state.shopRollsLeft)))) {
            if (hasRerolls) {
                state.shopRollsLeft = std::max(0, state.shopRollsLeft - 1);
                RollShopInventory(state);
                ShowMessage(state, state.shopRollsLeft > 0 ? "Loja atualizada." : "Loja atualizada. Sem re-rolls restantes.");
            }
        }
        if (!hasRerolls) {
            GuiEnable();
        }
        GuiSetStyle(BUTTON, TEXT_SIZE, previousButtonTextSize);
    }

    enum class SelectionKind {
        None,
        Weapon,
        Equipment,
        Inventory,
        ForgeInput0,
        ForgeInput1,
        ForgeResult,
        ShopItem
    };

    SelectionKind selection = SelectionKind::None;
    if (state.mode == InventoryViewMode::Forge && state.selectedForgeSlot == 0 && state.forgeInputIds[0] != 0) {
        selection = SelectionKind::ForgeInput0;
    } else if (state.mode == InventoryViewMode::Forge && state.selectedForgeSlot == 1 && state.forgeInputIds[1] != 0) {
        selection = SelectionKind::ForgeInput1;
    } else if (state.mode == InventoryViewMode::Forge && state.selectedForgeSlot == 2 && state.forgeResultId != 0) {
        selection = SelectionKind::ForgeResult;
    } else if (state.mode == InventoryViewMode::Shop && state.selectedShopIndex >= 0 && state.selectedShopIndex < static_cast<int>(state.shopItems.size())) {
        selection = SelectionKind::ShopItem;
    } else if (state.selectedWeaponIndex >= 0 && state.selectedWeaponIndex < static_cast<int>(state.weaponSlots.size()) && !state.weaponSlots[state.selectedWeaponIndex].empty()) {
        selection = SelectionKind::Weapon;
    } else if (state.selectedEquipmentIndex >= 0 && state.selectedEquipmentIndex < static_cast<int>(state.equipmentSlots.size()) && !state.equipmentSlots[state.selectedEquipmentIndex].empty()) {
        selection = SelectionKind::Equipment;
    } else if (state.selectedInventoryIndex >= 0 && state.selectedInventoryIndex < static_cast<int>(state.inventoryItems.size()) && !state.inventoryItems[state.selectedInventoryIndex].empty()) {
        selection = SelectionKind::Inventory;
    }

    bool isForgeMode = state.mode == InventoryViewMode::Forge;
    bool isShopMode = state.mode == InventoryViewMode::Shop;

    bool showLeft = false;
    bool showRight = false;
    bool showSingle = false;
    std::string leftLabel;
    std::string rightLabel;
    std::string singleLabel;

    switch (selection) {
        case SelectionKind::Weapon:
            showLeft = true;
            showRight = true;
            leftLabel = "Desequipar";
            rightLabel = isForgeMode ? "Forjar" : (isShopMode ? "Vender" : "Descartar");
            break;
        case SelectionKind::Equipment:
            showLeft = true;
            showRight = true;
            leftLabel = "Desequipar";
            rightLabel = isForgeMode ? "Forjar" : (isShopMode ? "Vender" : "Descartar");
            break;
        case SelectionKind::Inventory:
            showLeft = true;
            showRight = true;
            leftLabel = "Equipar";
            rightLabel = isForgeMode ? "Forjar" : (isShopMode ? "Vender" : "Descartar");
            break;
        case SelectionKind::ForgeInput0:
        case SelectionKind::ForgeInput1:
        case SelectionKind::ForgeResult:
            showSingle = true;
            singleLabel = "Remover";
            break;
        case SelectionKind::ShopItem:
            showSingle = true;
            singleLabel = "Comprar";
            break;
        default:
            break;
    }

    if (!state.feedbackMessage.empty()) {
        DrawTextEx(GetFontDefault(), state.feedbackMessage.c_str(),
                   Vector2{detailRect.x + 12.0f, detailRect.y + detailRect.height - 72.0f},
                   18.0f, 0.0f, Color{176, 64, 64, 255});
    }

    if (showLeft) {
        if (GuiButton(actionButtonLeft, leftLabel.c_str())) {
            if (selection == SelectionKind::Weapon) {
                HandleDesequiparWeapon(state, state.selectedWeaponIndex);
            } else if (selection == SelectionKind::Equipment) {
                HandleDesequiparArmor(state, state.selectedEquipmentIndex);
            } else if (selection == SelectionKind::Inventory) {
                HandleEquipInventory(state, state.selectedInventoryIndex);
            }
        }
    }

    if (showRight) {
        if (GuiButton(actionButtonRight, rightLabel.c_str())) {
            if (selection == SelectionKind::Weapon) {
                if (isForgeMode) {
                    HandleSendWeaponToForge(state, state.selectedWeaponIndex);
                } else if (isShopMode) {
                    HandleSellWeapon(state, state.selectedWeaponIndex);
                } else {
                    HandleDiscardWeapon(state, state.selectedWeaponIndex);
                }
            } else if (selection == SelectionKind::Equipment) {
                if (isForgeMode) {
                    HandleSendArmorToForge(state, state.selectedEquipmentIndex);
                } else if (isShopMode) {
                    HandleSellArmor(state, state.selectedEquipmentIndex);
                } else {
                    HandleDiscardArmor(state, state.selectedEquipmentIndex);
                }
            } else if (selection == SelectionKind::Inventory) {
                if (isForgeMode) {
                    HandleSendInventoryToForge(state, state.selectedInventoryIndex);
                } else if (isShopMode) {
                    HandleSellInventory(state, state.selectedInventoryIndex);
                } else {
                    HandleDiscardInventory(state, state.selectedInventoryIndex);
                }
            }
        }
    }

    if (showSingle) {
        Rectangle singleButton{
            detailRect.x + detailRect.width * 0.5f - 60.0f,
            detailRect.y + detailRect.height - 40.0f,
            120.0f,
            28.0f
        };
        if (GuiButton(singleButton, singleLabel.c_str())) {
            if (selection == SelectionKind::ForgeInput0) {
                HandleRemoveFromForge(state, 0);
            } else if (selection == SelectionKind::ForgeInput1) {
                HandleRemoveFromForge(state, 1);
            } else if (selection == SelectionKind::ForgeResult) {
                HandleRemoveFromForge(state, 2);
            } else if (selection == SelectionKind::ShopItem) {
                HandleBuyFromShop(state, state.selectedShopIndex);
            }
        }
    }

    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, prevTextColor);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, prevFocusColor);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, prevPressedColor);
}
