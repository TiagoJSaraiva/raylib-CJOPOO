#include "debug_console.h"

#include "font_manager.h"
#include "player.h"
#include "raygui.h"
#include "room_manager.h"
#include "station_persistence.h"
#include "ui_inventory.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <exception>
#include <limits>
#include <string>
#include <utility>

namespace {

std::string TrimCommand(const std::string& text) {
    size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
        ++start;
    }
    size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(start, end - start);
}

const ItemDefinition* FindDebugItemDefinitionById(const InventoryUIState& state, int itemId) {
    if (itemId <= 0) {
        return nullptr;
    }
    for (const ItemDefinition& def : state.items) {
        if (def.id == itemId) {
            return &def;
        }
    }
    return nullptr;
}

void PrepareInventoryForDebug(DebugConsoleState& state,
                              InventoryUIState& inventory,
                              RoomManager& manager) {
    SaveActiveStations(inventory, manager);
    ResetDebugInventoryContext(state, inventory);
    inventory.open = true;
    inventory.selectedInventoryIndex = -1;
    inventory.selectedEquipmentIndex = -1;
    inventory.selectedWeaponIndex = -1;
    inventory.selectedShopIndex = -1;
    inventory.selectedForgeSlot = -1;
    inventory.selectedChestIndex = -1;
    inventory.feedbackMessage.clear();
    inventory.feedbackTimer = 0.0f;
}

void ActivateDebugForgeContext(DebugConsoleState& state, InventoryUIState& inventory) {
    state.inventoryContext = DebugConsoleState::InventoryContext::Forge;
    state.forgeInstance = std::make_unique<ForgeInstance>();
    inventory.mode = InventoryViewMode::Forge;
    inventory.pendingForgeBreak = false;
    inventory.forgeState = ForgeState::Working;
    LoadForgeContents(inventory, *state.forgeInstance);
}

void ActivateDebugShopContext(DebugConsoleState& state, InventoryUIState& inventory) {
    state.inventoryContext = DebugConsoleState::InventoryContext::Shop;
    state.shopInstance = std::make_unique<ShopInstance>();
    state.shopInstance->items.clear();
    state.shopInstance->baseSeed = static_cast<std::uint64_t>(GetRandomValue(0, std::numeric_limits<int>::max()));
    state.shopInstance->rerollCount = 0;
    ResetShopTradeState(inventory);
    inventory.mode = InventoryViewMode::Shop;
    LoadShopContents(inventory, *state.shopInstance);
}

bool ActivateDebugChestContext(DebugConsoleState& state,
                               InventoryUIState& inventory,
                               RoomManager& manager,
                               std::unique_ptr<Chest> chest) {
    if (!chest) {
        return false;
    }
    state.inventoryContext = DebugConsoleState::InventoryContext::Chest;
    state.chestInstance = std::move(chest);
    inventory.mode = InventoryViewMode::Chest;
    LoadChestContents(inventory, *state.chestInstance);
    inventory.activeChestCoords = manager.GetCurrentCoords();
    return true;
}

} // namespace

std::string TrimDebugCommand(const std::string& text) {
    return TrimCommand(text);
}

void ClearDebugCommandBuffer(DebugConsoleState& state) {
    state.commandBuffer.fill('\0');
}

void CloseDebugConsole(DebugConsoleState& state) {
    state.open = false;
    state.textBoxActive = false;
    ClearDebugCommandBuffer(state);
}

void ResetDebugInventoryContext(DebugConsoleState& state, InventoryUIState& inventory) {
    using Context = DebugConsoleState::InventoryContext;
    if (state.inventoryContext == Context::Chest) {
        inventory.hasActiveChest = false;
        inventory.activeChest = nullptr;
        inventory.activeChestCoords = RoomCoords{};
        inventory.chestUiType = InventoryUIState::ChestUIType::None;
        inventory.chestSupportsDeposit = false;
        inventory.chestSupportsTakeAll = false;
        inventory.selectedChestIndex = -1;
        inventory.chestTitle.clear();
        inventory.chestItemIds.clear();
        inventory.chestItems.clear();
        inventory.chestQuantities.clear();
        inventory.chestTypes.clear();
    } else if (state.inventoryContext == Context::Forge) {
        inventory.selectedForgeSlot = -1;
        inventory.pendingForgeBreak = false;
        inventory.forgeState = ForgeState::Working;
    } else if (state.inventoryContext == Context::Shop) {
        ResetShopTradeState(inventory);
        inventory.selectedShopIndex = -1;
    }

    state.forgeInstance.reset();
    state.shopInstance.reset();
    state.chestInstance.reset();
    state.inventoryContext = Context::None;
    inventory.mode = InventoryViewMode::Inventory;
    inventory.open = false;
}

bool ExecuteDebugCommand(const std::string& rawCommand,
                         DebugConsoleState& state,
                         InventoryUIState& inventory,
                         PlayerCharacter& player,
                         RoomManager& manager) {
    std::string command = TrimCommand(rawCommand);
    if (command.empty()) {
        return false;
    }

    if (command == "inventory.openForje") {
        PrepareInventoryForDebug(state, inventory, manager);
        ActivateDebugForgeContext(state, inventory);
        return true;
    }

    if (command == "inventory.openShop") {
        PrepareInventoryForDebug(state, inventory, manager);
        ActivateDebugShopContext(state, inventory);
        return true;
    }

    if (command == "inventory.openChest") {
        PrepareInventoryForDebug(state, inventory, manager);
        auto chest = std::make_unique<CommonChest>(
            0.0f,
            0.0f,
            0.0f,
            Rectangle{0.0f, 0.0f, 0.0f, 0.0f},
            8,
            static_cast<std::uint64_t>(GetRandomValue(0, std::numeric_limits<int>::max())));
        return ActivateDebugChestContext(state, inventory, manager, std::move(chest));
    }

    constexpr const char* kItemGivePrefix = "item.give.";
    if (command.rfind(kItemGivePrefix, 0) == 0) {
        std::string idText = TrimCommand(command.substr(std::strlen(kItemGivePrefix)));
        try {
            int itemId = std::stoi(idText);
            if (itemId <= 0 || FindDebugItemDefinitionById(inventory, itemId) == nullptr) {
                return false;
            }
            PrepareInventoryForDebug(state, inventory, manager);
            auto chest = std::make_unique<PlayerChest>(
                0.0f,
                0.0f,
                0.0f,
                Rectangle{0.0f, 0.0f, 0.0f, 0.0f},
                8);
            chest->SetSlot(0, itemId, 1);
            return ActivateDebugChestContext(state, inventory, manager, std::move(chest));
        } catch (const std::exception&) {
            return false;
        }
    }

    constexpr const char* kHealthPrefix = "player.currentHealth.set";
    if (command.rfind(kHealthPrefix, 0) == 0) {
        std::string valueText = TrimCommand(command.substr(std::strlen(kHealthPrefix)));
        try {
            float value = std::stof(valueText);
            float maxHealth = std::max(player.derivedStats.maxHealth, 1.0f);
            if (value <= 0.0f || value > maxHealth) {
                return false;
            }
            player.currentHealth = value;
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    return false;
}

void DrawDebugConsoleOverlay(DebugConsoleState& state) {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    DrawRectangle(0, 0, screenWidth, screenHeight, Color{0, 0, 0, 140});

    const float panelWidth = 520.0f;
    const float panelHeight = 180.0f;
    Rectangle panel{
        screenWidth * 0.5f - panelWidth * 0.5f,
        screenHeight * 0.5f - panelHeight * 0.5f,
        panelWidth,
        panelHeight
    };
    DrawRectangleRec(panel, Color{22, 28, 40, 235});
    DrawRectangleLinesEx(panel, 2.0f, Color{200, 210, 230, 255});

    const Font& font = GetGameFont();
    constexpr float kTitleFontSize = 28.0f;
    const char* title = "Debug tool";
    Vector2 titleSize = MeasureTextEx(font, title, kTitleFontSize, 0.0f);
    Vector2 titlePos{panel.x + (panel.width - titleSize.x) * 0.5f, panel.y + 24.0f};
    DrawTextEx(font, title, titlePos, kTitleFontSize, 0.0f, Color{255, 255, 255, 255});

    Rectangle inputBounds{
        panel.x + 32.0f,
        panel.y + panel.height - 70.0f,
        panel.width - 64.0f,
        40.0f
    };
    GuiTextBox(inputBounds,
               state.commandBuffer.data(),
               static_cast<int>(state.commandBuffer.size()),
               state.textBoxActive);
}

void FlushTextInputBuffer() {
    while (GetCharPressed() != 0) {
    }
}
