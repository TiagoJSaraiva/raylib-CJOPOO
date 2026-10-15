#pragma once

#include <array>
#include <memory>
#include <string>

#include "chest.h"
#include "room.h"

struct InventoryUIState;
struct PlayerCharacter;
class RoomManager;

struct DebugConsoleState {
    static constexpr int kMaxCommandLength = 96;

    enum class InventoryContext {
        None,
        Forge,
        Shop,
        Chest
    };

    bool open{false};
    bool textBoxActive{false};
    std::array<char, kMaxCommandLength> commandBuffer{};
    InventoryContext inventoryContext{InventoryContext::None};
    std::unique_ptr<ForgeInstance> forgeInstance{};
    std::unique_ptr<ShopInstance> shopInstance{};
    std::unique_ptr<Chest> chestInstance{};
};

void ClearDebugCommandBuffer(DebugConsoleState& state);
void CloseDebugConsole(DebugConsoleState& state);
std::string TrimDebugCommand(const std::string& text);
void ResetDebugInventoryContext(DebugConsoleState& state, InventoryUIState& inventory);
bool ExecuteDebugCommand(const std::string& rawCommand,
                         DebugConsoleState& state,
                         InventoryUIState& inventory,
                         PlayerCharacter& player,
                         RoomManager& manager);
void DrawDebugConsoleOverlay(DebugConsoleState& state);
void FlushTextInputBuffer();
