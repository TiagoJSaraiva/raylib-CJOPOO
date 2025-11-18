#pragma once

#include "raylib.h"

#include <array>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <string>
#include <vector>

#include "room.h"
#include "player.h"

struct PlayerCharacter;
struct WeaponState;
struct WeaponBlueprint;
struct InventoryUIState;
class Chest;
class CommonChest;

// Define qual tela está ativa no painel (inventário, forja, loja ou baú).
enum class InventoryViewMode {
    Inventory,
    Forge,
    Shop,
    Chest
};

// Agrupa itens em categorias para filtros, regras de pilha e crafting.
enum class ItemCategory {
    None,
    Weapon,
    Armor,
    Consumable,
    Material,
    Result
};

using ItemAbilityHandler = std::function<bool(InventoryUIState&, PlayerCharacter&, int)>;

// Representa habilidade ativa vinculada a um item (ex.: poção ou artefato).
struct ItemActiveAbility {
    std::string name;
    std::string description;
    float cooldownSeconds{0.0f};
    bool consumesItemOnUse{false};
    ItemAbilityHandler handler{};

    bool IsValid() const { return static_cast<bool>(handler); }
};

// Metadados de um item disponível no protótipo do inventário.
struct ItemDefinition {
    int id{0};
    std::string name;
    ItemCategory category{ItemCategory::None};
    std::string description;
    int rarity{1};
    int baseValue{0};
    int value{0};
    const WeaponBlueprint* weaponBlueprint{nullptr};
    PlayerAttributes attributeBonuses{};
    std::string inventorySpritePath;
    Vector2 inventorySpriteDrawSize{0.0f, 0.0f};
    ItemActiveAbility activeAbility{};

    bool HasActiveAbility() const { return activeAbility.IsValid(); }
};

// Estado completo da interface de inventário (slots, seleção, forja, loja, baú).
struct InventoryUIState {
    bool open{false};
    InventoryViewMode mode{InventoryViewMode::Inventory};
    int selectedInventoryIndex{-1};
    int selectedEquipmentIndex{-1};
    int selectedWeaponIndex{-1};
    int selectedShopIndex{-1};
    int selectedForgeSlot{-1};
    int selectedChestIndex{-1};
    int lastDetailItemId{-1};
    float forgeSuccessChance{0.0f};
    int forgeAdjustHundreds{0};
    int forgeAdjustTens{0};
    int forgeAdjustOnes{0};
    int forgeBaseCost{0};
    ForgeState forgeState{ForgeState::Working};
    bool hasActiveForge{false};
    RoomCoords activeForgeCoords{};
    bool pendingForgeBreak{false};
    bool hasActiveShop{false};
    RoomCoords activeShopCoords{};
    bool shopTradeActive{false};
    bool shopTradeReadyToConfirm{false};
    int shopTradeRequiredRarity{0};
    int shopTradeInventoryIndex{-1};
    int shopTradeShopIndex{-1};
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
    std::vector<float> equipmentAbilityCooldowns;
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

    enum class ChestUIType {
        None,
        Common,
        Player
    };

    bool hasActiveChest{false};
    RoomCoords activeChestCoords{};
    Chest* activeChest{nullptr};
    ChestUIType chestUiType{ChestUIType::None};
    bool chestSupportsDeposit{false};
    bool chestSupportsTakeAll{false};
    std::string chestTitle;
    std::vector<int> chestItemIds;
    std::vector<int> chestQuantities;
    std::vector<std::string> chestItems;
    std::vector<ItemCategory> chestTypes;
};

// Preenche o estado com dados fictícios enquanto o conteúdo real não existe.
void InitializeInventoryUIDummyData(InventoryUIState& state);
// Renderiza o painel completo, incluindo informações do jogador e lojas ativas.
void RenderInventoryUI(InventoryUIState& state,
                       const PlayerCharacter& player,
                       const WeaponState& leftWeapon,
                       const WeaponState& rightWeapon,
                       Vector2 screenSize,
                       ShopInstance* activeShop);

// Agrega bônus de equipamentos equipados para aplicar nas estatísticas.
PlayerAttributes GatherEquipmentBonuses(const InventoryUIState& state);
// Sincroniza atributos do jogador com o que está equipado no inventário.
bool SyncEquipmentBonuses(const InventoryUIState& state, PlayerCharacter& player);

// Recupera definição de item pelo id (retorna nullptr se inexistente).
const ItemDefinition* GetItemDefinition(const InventoryUIState& state, int id);
// Define item de um slot específico de equipamento.
void SetEquipmentSlot(InventoryUIState& state, int index, int itemId);

// Busca blueprint de arma associado ao item informado.
const WeaponBlueprint* ResolveWeaponBlueprint(const InventoryUIState& state, int itemId);
// Serializa conteúdo da forja ativa para interface.
void LoadForgeContents(InventoryUIState& state, const ForgeInstance& forge);
// Persiste alterações da UI de forja de volta ao mundo.
void StoreForgeContents(InventoryUIState& state, ForgeInstance& forge);
// Sincroniza vitrine da loja com o estado atual da sala.
void LoadShopContents(InventoryUIState& state, ShopInstance& shop);
// Grava mudanças feitas na UI sobre a loja em memória do jogo.
void StoreShopContents(const InventoryUIState& state, ShopInstance& shop);
// Sorteia novos itens e preços para a loja, opcionalmente já vinculando ShopInstance.
void RollShopInventory(InventoryUIState& state, ShopInstance* shop = nullptr);
// Reseta variáveis temporárias da tela de troca com NPC.
void ResetShopTradeState(InventoryUIState& state);
// Carrega os itens de um baú concreto para a UI de inventário.
void LoadChestContents(InventoryUIState& state, Chest& chest);
// Atualiza listas/textos exibidos na aba de baú.
void RefreshChestView(InventoryUIState& state);
// Garante que baús comuns possuam loot mínimo antes de abrir a UI.
void EnsureCommonChestLoot(CommonChest& chest, const InventoryUIState& state);
