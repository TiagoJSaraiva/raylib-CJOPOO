#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <cctype>
#include <cstring>
#include <optional>
#include <limits>
#include <iostream>
#include <memory>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

#include "room_manager.h"
#include "room_renderer.h"
#include "room_types.h"
#include "projectile.h"
#include "player.h"
#include "weapon.h"
#include "weapon_blueprints.h"
#include "raygui.h"
#include "ui_inventory.h"
#include "font_manager.h"
#include "chest.h"
#include "hud.h"
#include "enemy_spawner.h"
#include "enemy_common.h"

namespace {

constexpr int SCREEN_WIDTH = 1920;
constexpr int SCREEN_HEIGHT = 1080;
constexpr float PLAYER_HALF_WIDTH = 20.0f;
constexpr float PLAYER_HALF_HEIGHT = 16.0f;
constexpr float PLAYER_RENDER_HALF_WIDTH = PLAYER_HALF_WIDTH - 3.0f;
constexpr float PLAYER_RENDER_HALF_HEIGHT = PLAYER_HALF_HEIGHT - 3.0f;
constexpr float PLAYER_COLLISION_RADIUS = (PLAYER_HALF_WIDTH > PLAYER_HALF_HEIGHT)
    ? PLAYER_HALF_WIDTH
    : PLAYER_HALF_HEIGHT;

struct CharacterSpriteResources {
    Texture2D idle{};
    Texture2D walking{};
    CharacterAnimationClip clip{};
    int frameCount{0};
    float animationTimer{0.0f};
    int currentFrame{0};
};

struct DamageNumber {
    Vector2 position{};
    float amount{0.0f};
    bool isCritical{false};
    float age{0.0f};
    float lifetime{1.0f};
};

struct DoorRenderData {
    Doorway* doorway{nullptr};
    DoorInstance* instance{nullptr};
    Rectangle hitbox{};
    Rectangle collisionHitbox{};
    bool frontView{true};
    float alpha{1.0f};
    bool showPrompt{false};
    bool isLocked{false};
    Vector2 promptAnchor{};
    BiomeType biome{BiomeType::Unknown};
    bool drawAfterPlayer{false};
    bool fromActiveRoom{false};
    bool drawAboveMask{false};
};

struct DoorMaskData {
    Rectangle corridorMask{};
    bool hasCorridorMask{false};
    float alpha{1.0f};
};

struct RoomRevealState {
    float alpha{0.0f};
};

constexpr float DOOR_COLLIDER_THICKNESS = 24.0f;
constexpr float DOOR_OFFSET_FROM_ROOM = 15.0f;
constexpr float DOOR_INTERACTION_DISTANCE = 150.0f;
constexpr float DOOR_FADE_DURATION = 1.0f;
constexpr float DOOR_MASK_CLEARANCE = 1.0f;
constexpr float HORIZONTAL_CORRIDOR_MASK_EXTRA_HEIGHT = static_cast<float>(TILE_SIZE);
constexpr float HORIZONTAL_CORRIDOR_MASK_VERTICAL_OFFSET = static_cast<float>(TILE_SIZE) * 0.5f;

void UpdateDamageNumbers(std::vector<DamageNumber>& numbers, float deltaSeconds) {
    for (auto& number : numbers) {
        number.age += deltaSeconds;
        number.position.y -= 26.0f * deltaSeconds;
    }

    numbers.erase(
        std::remove_if(numbers.begin(), numbers.end(), [](const DamageNumber& number) {
            return number.age >= number.lifetime;
        }),
        numbers.end());
}

void DrawDamageNumbers(const std::vector<DamageNumber>& numbers) {
    if (numbers.empty()) {
        return;
    }

    const Font& font = GetGameFont();

    for (const auto& number : numbers) {
        float alpha = 1.0f - (number.age / number.lifetime);
        if (alpha <= 0.0f) {
            continue;
        }

        int displayValue = static_cast<int>(std::lround(number.amount));
        if (displayValue < 0) {
            displayValue = 0;
        }

        std::string text = std::to_string(displayValue);
        if (number.isCritical) {
            text.push_back('!');
        }

        const float fontSize = number.isCritical ? 30.0f : 24.0f;
        Color baseColor = number.isCritical ? Color{255, 120, 120, 255} : Color{235, 235, 240, 255};
        baseColor.a = static_cast<unsigned char>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);

        Vector2 measure = MeasureTextEx(font, text.c_str(), fontSize, 0.0f);
        Vector2 drawPos{number.position.x - measure.x * 0.5f, number.position.y - measure.y};
        DrawTextEx(font, text.c_str(), drawPos, fontSize, 0.0f, baseColor);
    }
}

void PushDamageNumber(std::vector<DamageNumber>& numbers,
                      const Vector2& position,
                      float amount,
                      bool isCritical,
                      float lifetime = 1.0f) {
    DamageNumber number{};
    number.amount = amount;
    number.isCritical = isCritical;
    number.lifetime = lifetime;
    number.position = position;
    numbers.push_back(number);
}

ForgeInstance* ResolveTrackedForge(RoomManager& manager, const InventoryUIState& uiState) {
    if (!uiState.hasActiveForge) {
        return nullptr;
    }
    Room* trackedRoom = manager.TryGetRoom(uiState.activeForgeCoords);
    if (trackedRoom == nullptr) {
        return nullptr;
    }
    return trackedRoom->GetForge();
}

void SaveActiveForgeContents(InventoryUIState& uiState, RoomManager& manager) {
    if (ForgeInstance* forge = ResolveTrackedForge(manager, uiState)) {
        StoreForgeContents(uiState, *forge);
    }
}

ShopInstance* ResolveTrackedShop(RoomManager& manager, const InventoryUIState& uiState) {
    if (!uiState.hasActiveShop) {
        return nullptr;
    }
    Room* trackedRoom = manager.TryGetRoom(uiState.activeShopCoords);
    if (trackedRoom == nullptr) {
        return nullptr;
    }
    return trackedRoom->GetShop();
}

void SaveActiveShopContents(InventoryUIState& uiState, RoomManager& manager) {
    if (ShopInstance* shop = ResolveTrackedShop(manager, uiState)) {
        StoreShopContents(uiState, *shop);
    }
}

void SaveActiveStations(InventoryUIState& uiState, RoomManager& manager) {
    SaveActiveForgeContents(uiState, manager);
    SaveActiveShopContents(uiState, manager);
}

float DoorVisibilityAlpha(const DoorInstance& state) {
    if (state.open) {
        return 0.0f;
    }
    if (!state.opening) {
        return 1.0f;
    }
    float t = std::clamp(state.fadeProgress / DOOR_FADE_DURATION, 0.0f, 1.0f);
    return 1.0f - t;
}

float TileToPixel(int tile);
Vector2 SnapToPixel(const Vector2& value);
Rectangle DoorRectInsideRoom(const RoomLayout& layout, const Doorway& door);

Rectangle ComputeDoorHitbox(const RoomLayout& layout, const Doorway& door) {
    Rectangle rect{};
    const float tileSize = static_cast<float>(TILE_SIZE);
    switch (door.direction) {
        case Direction::North: {
            float baseX = TileToPixel(layout.tileBounds.x + door.offset);
            rect.x = baseX;
            rect.width = static_cast<float>(door.width) * tileSize;
            rect.height = DOOR_COLLIDER_THICKNESS;
            rect.y = TileToPixel(layout.tileBounds.y) - DOOR_OFFSET_FROM_ROOM - rect.height;
            break;
        }
        case Direction::South: {
            float baseX = TileToPixel(layout.tileBounds.x + door.offset);
            rect.x = baseX;
            rect.width = static_cast<float>(door.width) * tileSize;
            rect.height = DOOR_COLLIDER_THICKNESS;
            rect.y = TileToPixel(layout.tileBounds.y + layout.heightTiles) + DOOR_OFFSET_FROM_ROOM;
            break;
        }
        case Direction::East: {
            float baseY = TileToPixel(layout.tileBounds.y + door.offset);
            rect.y = baseY;
            rect.height = static_cast<float>(door.width) * tileSize;
            rect.width = DOOR_COLLIDER_THICKNESS;
            rect.x = TileToPixel(layout.tileBounds.x + layout.widthTiles) + DOOR_OFFSET_FROM_ROOM;
            break;
        }
        case Direction::West: {
            float baseY = TileToPixel(layout.tileBounds.y + door.offset);
            rect.y = baseY;
            rect.height = static_cast<float>(door.width) * tileSize;
            rect.width = DOOR_COLLIDER_THICKNESS;
            rect.x = TileToPixel(layout.tileBounds.x) - DOOR_OFFSET_FROM_ROOM - rect.width;
            break;
        }
    }
    return rect;
}

Rectangle ComputeDoorCollisionHitbox(const RoomLayout& layout,
                                     const Doorway& door,
                                     const Rectangle& renderHitbox) {
    Rectangle collision = renderHitbox;
    switch (door.direction) {
        case Direction::North: {
            Rectangle doorway = DoorRectInsideRoom(layout, door);
            collision.x = doorway.x;
            collision.width = doorway.width;
            collision.height = DOOR_COLLIDER_THICKNESS;
            float roomTop = TileToPixel(layout.tileBounds.y);
            collision.y = roomTop - collision.height;
            break;
        }
        case Direction::South: {
            Rectangle doorway = DoorRectInsideRoom(layout, door);
            collision.x = doorway.x;
            collision.width = doorway.width;
            collision.height = DOOR_COLLIDER_THICKNESS;
            float roomBottom = TileToPixel(layout.tileBounds.y + layout.heightTiles);
            collision.y = roomBottom;
            break;
        }
        case Direction::East:
        case Direction::West:
            collision = renderHitbox;
            break;
    }
    return collision;
}

bool ClipCorridorMaskBehindDoor(Direction direction,
                                const Rectangle& doorHitbox,
                                Rectangle& corridorMask) {
    const float maskRight = corridorMask.x + corridorMask.width;
    const float maskBottom = corridorMask.y + corridorMask.height;

    switch (direction) {
        case Direction::North: {
            float doorBack = doorHitbox.y - DOOR_MASK_CLEARANCE;
            float newBottom = std::min(doorBack, maskBottom);
            if (newBottom <= corridorMask.y) {
                return false;
            }
            corridorMask.height = newBottom - corridorMask.y;
            return corridorMask.height > 0.0f;
        }
        case Direction::South: {
            float doorBack = doorHitbox.y + doorHitbox.height + DOOR_MASK_CLEARANCE;
            float newY = std::max(doorBack, corridorMask.y);
            if (newY >= maskBottom) {
                return false;
            }
            corridorMask.height = maskBottom - newY;
            corridorMask.y = newY;
            return corridorMask.height > 0.0f;
        }
        case Direction::East: {
            float doorBack = doorHitbox.x + doorHitbox.width + DOOR_MASK_CLEARANCE;
            float newX = std::max(doorBack, corridorMask.x);
            if (newX >= maskRight) {
                return false;
            }
            corridorMask.width = maskRight - newX;
            corridorMask.x = newX;
            return corridorMask.width > 0.0f;
        }
        case Direction::West: {
            float doorBack = doorHitbox.x - DOOR_MASK_CLEARANCE;
            float newRight = std::min(doorBack, maskRight);
            if (newRight <= corridorMask.x) {
                return false;
            }
            corridorMask.width = newRight - corridorMask.x;
            return corridorMask.width > 0.0f;
        }
    }

    return false;
}

Color DoorMaskColor(float alpha) {
    Color color{24, 26, 33, 255};
    color.a = static_cast<unsigned char>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
    return color;
}

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

Texture2D LoadTextureIfExists(const std::string& path) {
    if (path.empty()) {
        return Texture2D{};
    }

    if (!FileExists(path.c_str())) {
        std::cerr << "[Character] Sprite nao encontrado: " << path << std::endl;
        return Texture2D{};
    }

    Texture2D texture = LoadTexture(path.c_str());
    if (texture.id != 0) {
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    }
    return texture;
}

void UnloadTextureIfValid(Texture2D& texture) {
    if (texture.id != 0) {
        UnloadTexture(texture);
        texture = Texture2D{};
    }
}

void UnloadCharacterSprites(CharacterSpriteResources& resources) {
    UnloadTextureIfValid(resources.idle);
    UnloadTextureIfValid(resources.walking);
    resources.frameCount = 0;
    resources.animationTimer = 0.0f;
    resources.currentFrame = 0;
}

void LoadCharacterSprites(const CharacterAppearanceBlueprint& appearance, CharacterSpriteResources& outResources) {
    UnloadCharacterSprites(outResources);

    outResources.idle = LoadTextureIfExists(appearance.idleSpritePath);
    outResources.walking = LoadTextureIfExists(appearance.walking.spriteSheetPath);
    outResources.clip = appearance.walking;

    if (outResources.walking.id != 0) {
        if (outResources.clip.frameWidth <= 0) {
            outResources.clip.frameWidth = outResources.walking.width;
        }
        if (outResources.clip.frameHeight <= 0) {
            if (appearance.walking.verticalLayout) {
                outResources.clip.frameHeight = (appearance.walking.frameCount > 0)
                    ? outResources.walking.height / appearance.walking.frameCount
                    : outResources.walking.height;
            } else {
                outResources.clip.frameHeight = outResources.walking.height;
            }
        }

        if (outResources.clip.verticalLayout) {
            if (outResources.clip.frameHeight > 0) {
                outResources.frameCount = outResources.walking.height / outResources.clip.frameHeight;
            }
        } else {
            if (outResources.clip.frameWidth > 0) {
                outResources.frameCount = outResources.walking.width / outResources.clip.frameWidth;
            }
        }
    }

    if (outResources.frameCount <= 0) {
        outResources.frameCount = std::max(appearance.walking.frameCount, 1);
    }
}

void UpdateCharacterAnimation(CharacterSpriteResources& resources, bool isMoving, float deltaSeconds) {
    if (resources.walking.id == 0 || resources.frameCount <= 1) {
        resources.currentFrame = 0;
        resources.animationTimer = 0.0f;
        return;
    }

    if (!isMoving) {
        resources.currentFrame = 0;
        resources.animationTimer = 0.0f;
        return;
    }

    float frameDuration = (resources.clip.secondsPerFrame > 0.0f) ? resources.clip.secondsPerFrame : 0.12f;
    resources.animationTimer += deltaSeconds;

    while (resources.animationTimer >= frameDuration) {
        resources.animationTimer -= frameDuration;
        resources.currentFrame = (resources.currentFrame + 1) % std::max(resources.frameCount, 1);
    }
}

bool DrawCharacterSprite(const CharacterSpriteResources& resources,
                         Vector2 anchorPosition,
                         bool isMoving) {
    const Texture2D* texture = nullptr;
    Rectangle src{0.0f, 0.0f, 0.0f, 0.0f};
    float spriteWidth = 0.0f;
    float spriteHeight = 0.0f;

    if (isMoving && resources.walking.id != 0 && resources.frameCount > 0) {
        texture = &resources.walking;
        spriteWidth = static_cast<float>((resources.clip.frameWidth > 0) ? resources.clip.frameWidth : resources.walking.width);
        spriteHeight = static_cast<float>((resources.clip.frameHeight > 0) ? resources.clip.frameHeight : resources.walking.height);
        src.width = spriteWidth;
        src.height = spriteHeight;

        if (resources.clip.verticalLayout) {
            src.y = spriteHeight * static_cast<float>(resources.currentFrame % resources.frameCount);
        } else {
            src.x = spriteWidth * static_cast<float>(resources.currentFrame % resources.frameCount);
        }
    } else if (resources.idle.id != 0) {
        texture = &resources.idle;
        spriteWidth = static_cast<float>(resources.idle.width);
        spriteHeight = static_cast<float>(resources.idle.height);
        src.width = spriteWidth;
        src.height = spriteHeight;
    }

    if (texture == nullptr || texture->id == 0 || spriteWidth <= 0.0f || spriteHeight <= 0.0f) {
        return false;
    }

    float bottomY = anchorPosition.y + PLAYER_HALF_HEIGHT;
    Rectangle dest{
        anchorPosition.x - spriteWidth * 0.5f,
        bottomY - spriteHeight,
        spriteWidth,
        spriteHeight
    };

    DrawTexturePro(*texture, src, dest, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    return true;
}

PlayerAttributes GatherWeaponPassiveBonuses(const WeaponState& leftWeapon,
                                            const WeaponState& rightWeapon) {
    PlayerAttributes totals{};
    if (leftWeapon.blueprint != nullptr) {
        totals = AddAttributes(totals, leftWeapon.blueprint->passiveBonuses);
    }
    if (rightWeapon.blueprint != nullptr) {
        totals = AddAttributes(totals, rightWeapon.blueprint->passiveBonuses);
    }
    return totals;
}

void RefreshPlayerWeaponBonuses(PlayerCharacter& player,
                                const WeaponState& leftWeapon,
                                const WeaponState& rightWeapon) {
    player.weaponBonuses = GatherWeaponPassiveBonuses(leftWeapon, rightWeapon);
    player.RecalculateStats();
}

bool SyncWeaponStateFromSlot(const InventoryUIState& inventoryUI,
                             int slotIndex,
                             WeaponState& weaponState) {
    int itemId = (slotIndex < static_cast<int>(inventoryUI.weaponSlotIds.size()))
                     ? inventoryUI.weaponSlotIds[slotIndex]
                     : 0;
    const WeaponBlueprint* desiredBlueprint = (itemId > 0) ? ResolveWeaponBlueprint(inventoryUI, itemId) : nullptr;
    if (itemId > 0 && desiredBlueprint == nullptr) {
        return false;
    }
    if (weaponState.blueprint != desiredBlueprint) {
        weaponState.blueprint = desiredBlueprint;
        weaponState.cooldownTimer = 0.0f;
        weaponState.derived = WeaponDerivedStats{};
        return true;
    }
    return false;
}

bool SyncEquippedWeapons(const InventoryUIState& inventoryUI,
                         WeaponState& leftWeapon,
                         WeaponState& rightWeapon) {
    bool changed = false;
    changed |= SyncWeaponStateFromSlot(inventoryUI, 0, leftWeapon);
    changed |= SyncWeaponStateFromSlot(inventoryUI, 1, rightWeapon);
    return changed;
}

struct TrainingDummy {
    Vector2 position{};
    float radius{48.0f};
    RoomCoords homeRoom{};
    bool isImmune{false};
    float immunitySecondsRemaining{0.0f};
};

std::uint64_t GenerateWorldSeed() {
    std::random_device rd;
    std::uint64_t seed = (static_cast<std::uint64_t>(rd()) << 32) ^ static_cast<std::uint64_t>(rd());
    if (seed == 0) {
        seed = static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    }
    return seed;
}

float TileToPixel(int tile) {
    return static_cast<float>(tile * TILE_SIZE);
}

Rectangle TileRectToPixels(const TileRect& rect) {
    return Rectangle{
        TileToPixel(rect.x),
        TileToPixel(rect.y),
        static_cast<float>(rect.width * TILE_SIZE),
        static_cast<float>(rect.height * TILE_SIZE)
    };
}

Rectangle DoorRectInsideRoom(const RoomLayout& layout, const Doorway& door) {
    Rectangle rect{};
    float baseX = TileToPixel(layout.tileBounds.x + door.offset);
    float span = static_cast<float>(door.width * TILE_SIZE);

    switch (door.direction) {
        case Direction::North:
            rect = Rectangle{baseX, TileToPixel(layout.tileBounds.y), span, static_cast<float>(TILE_SIZE)};
            break;
        case Direction::South:
            rect = Rectangle{
                baseX,
                TileToPixel(layout.tileBounds.y + layout.heightTiles - 1),
                span,
                static_cast<float>(TILE_SIZE)
            };
            break;
        case Direction::East:
            rect = Rectangle{
                TileToPixel(layout.tileBounds.x + layout.widthTiles - 1),
                TileToPixel(layout.tileBounds.y + door.offset),
                static_cast<float>(TILE_SIZE),
                span
            };
            break;
        case Direction::West:
            rect = Rectangle{
                TileToPixel(layout.tileBounds.x),
                TileToPixel(layout.tileBounds.y + door.offset),
                static_cast<float>(TILE_SIZE),
                span
            };
            break;
    }

    return rect;
}

Rectangle PlayerBounds(const Vector2& center) {
    return Rectangle{
        center.x - PLAYER_HALF_WIDTH,
        center.y - PLAYER_HALF_HEIGHT,
        PLAYER_HALF_WIDTH * 2.0f,
        PLAYER_HALF_HEIGHT * 2.0f
    };
}

Vector2 ResolveCollisionWithRectangle(const Rectangle& obstacle,
                                      Vector2 position,
                                      float halfWidth,
                                      float halfHeight) {
    Rectangle playerRect{
        position.x - halfWidth,
        position.y - halfHeight,
        halfWidth * 2.0f,
        halfHeight * 2.0f
    };

    if (!CheckCollisionRecs(playerRect, obstacle)) {
        return position;
    }

    float playerCenterX = playerRect.x + playerRect.width * 0.5f;
    float playerCenterY = playerRect.y + playerRect.height * 0.5f;
    float obstacleCenterX = obstacle.x + obstacle.width * 0.5f;
    float obstacleCenterY = obstacle.y + obstacle.height * 0.5f;

    float deltaX = playerCenterX - obstacleCenterX;
    float deltaY = playerCenterY - obstacleCenterY;
    float overlapX = (obstacle.width * 0.5f + playerRect.width * 0.5f) - std::abs(deltaX);
    float overlapY = (obstacle.height * 0.5f + playerRect.height * 0.5f) - std::abs(deltaY);

    if (overlapX < overlapY) {
        position.x += (deltaX < 0.0f ? -overlapX : overlapX);
    } else {
        position.y += (deltaY < 0.0f ? -overlapY : overlapY);
    }

    return position;
}

Vector2 ResolveCollisionWithForge(const ForgeInstance& forge,
                                  Vector2 position,
                                  float halfWidth,
                                  float halfHeight) {
    return ResolveCollisionWithRectangle(forge.hitbox, position, halfWidth, halfHeight);
}

Vector2 ResolveCollisionWithShop(const ShopInstance& shop,
                                 Vector2 position,
                                 float halfWidth,
                                 float halfHeight) {
    return ResolveCollisionWithRectangle(shop.hitbox, position, halfWidth, halfHeight);
}

Vector2 ResolveCollisionWithChest(const Chest& chest,
                                  Vector2 position,
                                  float halfWidth,
                                  float halfHeight) {
    return ResolveCollisionWithRectangle(chest.Hitbox(), position, halfWidth, halfHeight);
}

Vector2 RoomCenter(const RoomLayout& layout) {
    Rectangle bounds = TileRectToPixels(layout.tileBounds);
    return Vector2{bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f};
}

Vector2 SnapToPixel(const Vector2& value) {
    return Vector2{std::round(value.x), std::round(value.y)};
}

Rectangle DoorInteractionArea(const RoomLayout& layout, const Doorway& door) {
    if (door.corridorTiles.width > 0 && door.corridorTiles.height > 0) {
        return TileRectToPixels(door.corridorTiles);
    }
    return DoorRectInsideRoom(layout, door);
}

void UpdateDoorInteractionForRoom(Room& room, bool hasActiveEnemies) {
    RoomLayout& layout = room.Layout();
    for (Doorway& doorway : layout.doors) {
        if (!doorway.doorState) {
            continue;
        }

        DoorInstance& doorState = *doorway.doorState;
        bool isClosed = !(doorState.open || doorState.opening);

        if (hasActiveEnemies && isClosed) {
            if (doorState.interactionState == DoorInteractionState::Unlocked) {
                doorState.interactionState = DoorInteractionState::Unavailable;
            }
            continue;
        }

        if (!hasActiveEnemies && doorState.interactionState == DoorInteractionState::Unavailable) {
            doorState.interactionState = DoorInteractionState::Unlocked;
        }
    }
}

bool IsInputMovingToward(Direction direction, const Vector2& input) {
    constexpr float kEpsilon = 0.1f;
    switch (direction) {
        case Direction::North:
            return input.y < -kEpsilon;
        case Direction::South:
            return input.y > kEpsilon;
        case Direction::East:
            return input.x > kEpsilon;
        case Direction::West:
            return input.x < -kEpsilon;
    }
    return false;
}

bool IsBoxInsideRect(const Rectangle& rect,
                     const Vector2& position,
                     float halfWidth,
                     float halfHeight,
                     float tolerance = 0.0f) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return false;
    }
    const float minX = rect.x + halfWidth - tolerance;
    const float maxX = rect.x + rect.width - halfWidth + tolerance;
    const float minY = rect.y + halfHeight - tolerance;
    const float maxY = rect.y + rect.height - halfHeight + tolerance;
    return position.x >= minX && position.x <= maxX && position.y >= minY && position.y <= maxY;
}

Vector2 ClampBoxToRect(const Rectangle& rect,
                       const Vector2& position,
                       float halfWidth,
                       float halfHeight,
                       float tolerance = 0.0f) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return position;
    }

    float minX = rect.x + halfWidth - tolerance;
    float maxX = rect.x + rect.width - halfWidth + tolerance;
    float minY = rect.y + halfHeight - tolerance;
    float maxY = rect.y + rect.height - halfHeight + tolerance;

    if (minX > maxX) {
        float midX = rect.x + rect.width * 0.5f;
        minX = maxX = midX;
    }
    if (minY > maxY) {
        float midY = rect.y + rect.height * 0.5f;
        minY = maxY = midY;
    }

    Vector2 clamped{};
    clamped.x = std::clamp(position.x, minX, maxX);
    clamped.y = std::clamp(position.y, minY, maxY);
    return clamped;
}

void ClampPlayerToAccessibleArea(Vector2& position,
                                 float halfWidth,
                                 float halfHeight,
                                 const RoomLayout& layout) {
    constexpr float tolerance = 0.0f;

    Rectangle floor = TileRectToPixels(layout.tileBounds);

    struct AccessibleRegion {
        Rectangle clampRect;
        Rectangle detectRect;
        Direction direction;
        bool isCorridor{false};
    };

    std::vector<AccessibleRegion> doorRegions;
    doorRegions.reserve(layout.doors.size() * 2);

    for (const auto& door : layout.doors) {
        if (door.sealed) {
            continue;
        }

        Rectangle doorway = DoorRectInsideRoom(layout, door);
        if (doorway.width > 0.0f && doorway.height > 0.0f) {
            doorRegions.push_back(AccessibleRegion{doorway, doorway, door.direction, false});
        }

        Rectangle corridor = TileRectToPixels(door.corridorTiles);
        if (corridor.width > 0.0f && corridor.height > 0.0f) {
            Rectangle detectCorridor = corridor;
            const float extension = TILE_SIZE * 0.5f;
            switch (door.direction) {
                case Direction::North:
                    detectCorridor.height += extension;
                    break;
                case Direction::South:
                    detectCorridor.y -= extension;
                    detectCorridor.height += extension;
                    break;
                case Direction::East:
                    detectCorridor.x -= extension;
                    detectCorridor.width += extension;
                    break;
                case Direction::West:
                    detectCorridor.width += extension;
                    break;
            }
            doorRegions.push_back(AccessibleRegion{corridor, detectCorridor, door.direction, true});
        }
    }

    if (IsBoxInsideRect(floor, position, halfWidth, halfHeight, tolerance)) {
        return;
    }

    auto isInsideRegion = [&](const AccessibleRegion& region) {
        if (!region.isCorridor) {
            return IsBoxInsideRect(region.detectRect, position, halfWidth, halfHeight, tolerance);
        }

        const Rectangle& rect = region.detectRect;
        if (rect.width <= 0.0f || rect.height <= 0.0f) {
            return false;
        }

        if (region.direction == Direction::North || region.direction == Direction::South) {
            float minCenterX = rect.x + halfWidth - tolerance;
            float maxCenterX = rect.x + rect.width - halfWidth + tolerance;
            float minCenterY = rect.y - halfHeight - tolerance;
            float maxCenterY = rect.y + rect.height + halfHeight + tolerance;
            return position.x >= minCenterX && position.x <= maxCenterX && position.y >= minCenterY && position.y <= maxCenterY;
        }

        float minCenterY = rect.y + halfHeight - tolerance;
        float maxCenterY = rect.y + rect.height - halfHeight + tolerance;
        float minCenterX = rect.x - halfWidth - tolerance;
        float maxCenterX = rect.x + rect.width + halfWidth + tolerance;
        return position.y >= minCenterY && position.y <= maxCenterY && position.x >= minCenterX && position.x <= maxCenterX;
    };

    bool insideDoorRegion = false;
    for (const AccessibleRegion& region : doorRegions) {
        if (isInsideRegion(region)) {
            insideDoorRegion = true;
            break;
        }
    }

    if (insideDoorRegion) {
        return;
    }

    Vector2 bestPosition = position;
    float bestDistanceSq = std::numeric_limits<float>::max();
    bool foundCandidate = false;

    auto clampWithinCorridor = [&](const AccessibleRegion& region) -> std::optional<Vector2> {
        const Rectangle& rect = region.clampRect;
        if (rect.width <= 0.0f || rect.height <= 0.0f) {
            return std::nullopt;
        }

    float minX = rect.x + halfWidth - tolerance;
    float maxX = rect.x + rect.width - halfWidth + tolerance;
    float minY = rect.y + halfHeight - tolerance;
    float maxY = rect.y + rect.height - halfHeight + tolerance;

        Vector2 clamped = position;

        switch (region.direction) {
            case Direction::North:
                if (position.y > maxY) {
                    return std::nullopt;
                }
                clamped.x = std::clamp(clamped.x, minX, maxX);
                clamped.y = std::clamp(clamped.y, minY, maxY);
                break;
            case Direction::South:
                if (position.y < minY) {
                    return std::nullopt;
                }
                clamped.x = std::clamp(clamped.x, minX, maxX);
                clamped.y = std::clamp(clamped.y, minY, maxY);
                break;
            case Direction::East:
                if (position.x < minX) {
                    return std::nullopt;
                }
                clamped.y = std::clamp(clamped.y, minY, maxY);
                clamped.x = std::clamp(clamped.x, minX, maxX);
                break;
            case Direction::West:
                if (position.x > maxX) {
                    return std::nullopt;
                }
                clamped.y = std::clamp(clamped.y, minY, maxY);
                clamped.x = std::clamp(clamped.x, minX, maxX);
                break;
        }

        return clamped;
    };

    auto consider = [&](const AccessibleRegion& region) {
        Vector2 candidate = position;

        if (!region.isCorridor) {
            const Rectangle& rect = region.clampRect;
            if (rect.width <= 0.0f || rect.height <= 0.0f) {
                return;
            }
            candidate = ClampBoxToRect(rect, position, halfWidth, halfHeight, tolerance);
        } else {
            std::optional<Vector2> corridorClamp = clampWithinCorridor(region);
            if (!corridorClamp.has_value()) {
                return;
            }
            candidate = corridorClamp.value();
        }

        float distSq = Vector2DistanceSqr(position, candidate);
        if (distSq < bestDistanceSq) {
            bestDistanceSq = distSq;
            bestPosition = candidate;
            foundCandidate = true;
        }
    };

    for (const AccessibleRegion& region : doorRegions) {
        consider(region);
    }

    Vector2 floorClamp = ClampBoxToRect(floor, position, halfWidth, halfHeight, tolerance);
    float floorDistSq = Vector2DistanceSqr(position, floorClamp);
    if (!foundCandidate || floorDistSq < bestDistanceSq) {
        bestPosition = floorClamp;
    }

    position = bestPosition;
}

bool ShouldTransitionThroughDoor(const Doorway& door, const Vector2& position, const Vector2& movement) {
    constexpr float kForwardEpsilon = 0.05f;

    const Rectangle corridor = TileRectToPixels(door.corridorTiles);
    if (corridor.width > 0.0f && corridor.height > 0.0f) {
        constexpr float kLateralTolerance = 8.0f;
        constexpr float kForwardDepthVertical = PLAYER_HALF_HEIGHT - 4.0f;
        constexpr float kForwardDepthHorizontal = PLAYER_HALF_WIDTH - 4.0f;
        const float corridorLeft = corridor.x;
        const float corridorRight = corridor.x + corridor.width;
        const float corridorTop = corridor.y;
        const float corridorBottom = corridor.y + corridor.height;
        const Rectangle playerRect = PlayerBounds(position);
        const float playerLeft = playerRect.x;
        const float playerRight = playerRect.x + playerRect.width;
        const float playerTop = playerRect.y;
        const float playerBottom = playerRect.y + playerRect.height;

        switch (door.direction) {
            case Direction::North:
                if (movement.y >= -kForwardEpsilon) {
                    return false;
                }
                if (playerRight < corridorLeft - kLateralTolerance || playerLeft > corridorRight + kLateralTolerance) {
                    return false;
                }
                return playerTop <= (corridorBottom - kForwardDepthVertical);
            case Direction::South:
                if (movement.y <= kForwardEpsilon) {
                    return false;
                }
                if (playerRight < corridorLeft - kLateralTolerance || playerLeft > corridorRight + kLateralTolerance) {
                    return false;
                }
                return playerBottom >= (corridorTop + kForwardDepthVertical);
            case Direction::East:
                if (movement.x <= kForwardEpsilon) {
                    return false;
                }
                if (playerBottom < corridorTop - kLateralTolerance || playerTop > corridorBottom + kLateralTolerance) {
                    return false;
                }
                return playerRight >= (corridorLeft + kForwardDepthHorizontal);
            case Direction::West:
                if (movement.x >= -kForwardEpsilon) {
                    return false;
                }
                if (playerBottom < corridorTop - kLateralTolerance || playerTop > corridorBottom + kLateralTolerance) {
                    return false;
                }
                return playerLeft <= (corridorRight - kForwardDepthHorizontal);
        }
    }

    return true;
}
} // namespace

int main() {
    SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST | FLAG_VSYNC_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Prototype - Room Generation");
    const int monitorIndex = GetCurrentMonitor();
    const Vector2 monitorPosition = GetMonitorPosition(monitorIndex);
    SetWindowPosition(static_cast<int>(monitorPosition.x), static_cast<int>(monitorPosition.y));
    SetTargetFPS(60);
    LoadGameFont("assets/font/alagard.ttf", 32);

    const std::uint64_t worldSeed = GenerateWorldSeed();
    RoomManager roomManager{worldSeed};
    RoomRenderer roomRenderer;
    ProjectileSystem projectileSystem;
    ProjectileSystem enemyProjectileSystem;
    EnemySpawner enemySpawner;
    std::mt19937 enemyRng(static_cast<std::mt19937::result_type>(worldSeed));
    using EnemyList = std::vector<std::unique_ptr<Enemy>>;
    std::unordered_map<RoomCoords, EnemyList, RoomCoordsHash> roomEnemies;
    std::unordered_set<RoomCoords, RoomCoordsHash> roomsWithSpawnedEnemies;
    PlayerCharacter player = CreateKnightCharacter();
    CharacterSpriteResources playerSprites{};
    LoadCharacterSprites(player.appearance, playerSprites);
    WeaponState leftHandWeapon;
    leftHandWeapon.blueprint = &GetEspadaCurtaWeaponBlueprint();
    WeaponState rightHandWeapon;
    rightHandWeapon.blueprint = &GetArcoSimplesWeaponBlueprint();

    InventoryUIState inventoryUI;
    DebugConsoleState debugConsole;
    InitializeInventoryUIDummyData(inventoryUI);
    SyncEquipmentBonuses(inventoryUI, player);

    RefreshPlayerWeaponBonuses(player, leftHandWeapon, rightHandWeapon);

    leftHandWeapon.RecalculateDerivedStats(player);
    rightHandWeapon.RecalculateDerivedStats(player);

    roomManager.EnsureNeighborsGenerated(roomManager.GetCurrentCoords());

    Vector2 playerPosition = RoomCenter(roomManager.GetCurrentRoom().Layout());

    const Vector2 trainingDummyOffset{TILE_SIZE * 2.5f, 0.0f};
    TrainingDummy trainingDummy{};
    trainingDummy.homeRoom = roomManager.GetCurrentCoords();
    trainingDummy.position = Vector2Add(playerPosition, trainingDummyOffset);
    trainingDummy.radius = 52.0f;


    std::vector<DamageNumber> damageNumbers;
    std::vector<DoorRenderData> doorRenderData;
    doorRenderData.reserve(8);
    std::vector<DoorMaskData> doorMaskData;
    doorMaskData.reserve(16);
    std::unordered_map<RoomCoords, RoomRevealState, RoomCoordsHash> roomRevealStates;

    auto ensureRoomEnemies = [&](Room& room) {
        RoomCoords coords = room.GetCoords();
        if (roomsWithSpawnedEnemies.count(coords) > 0) {
            return;
        }
        EnemyList& storage = roomEnemies[coords];
        enemySpawner.SpawnEnemiesForRoom(room, storage, enemyRng);
        roomsWithSpawnedEnemies.insert(coords);
    };

    Camera2D camera{};
    camera.offset = Vector2{SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f};
    camera.target = playerPosition;
    camera.zoom = 1.0f;

    bool playerIsMoving = false;

    while (!WindowShouldClose()) {
        const float delta = GetFrameTime();

        bool shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (shiftHeld && IsKeyPressed(KEY_ZERO)) {
            if (debugConsole.open) {
                CloseDebugConsole(debugConsole);
            } else {
                debugConsole.open = true;
                debugConsole.textBoxActive = true;
                ClearDebugCommandBuffer(debugConsole);
                FlushTextInputBuffer();
            }
        }

        if (debugConsole.open) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                CloseDebugConsole(debugConsole);
            } else if (IsKeyPressed(KEY_ENTER)) {
                std::string commandText = debugConsole.commandBuffer.data();
                std::string trimmedCommand = TrimCommand(commandText);
                bool executed = ExecuteDebugCommand(commandText, debugConsole, inventoryUI, player, roomManager);
                if (!executed && !trimmedCommand.empty()) {
                    std::cout << "[Debug] Comando desconhecido: " << trimmedCommand << std::endl;
                }
                CloseDebugConsole(debugConsole);
            }
        }

        bool debugInputBlocked = debugConsole.open;

        if (!debugInputBlocked && (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_I))) {
            bool wasOpen = inventoryUI.open;
            inventoryUI.open = !inventoryUI.open;
            if (inventoryUI.open) {
                inventoryUI.mode = InventoryViewMode::Inventory;
                inventoryUI.selectedForgeSlot = -1;
                inventoryUI.selectedShopIndex = -1;
            } else if (wasOpen) {
                SaveActiveStations(inventoryUI, roomManager);
            }
        }

        SyncEquipmentBonuses(inventoryUI, player);

        if (SyncEquippedWeapons(inventoryUI, leftHandWeapon, rightHandWeapon)) {
            RefreshPlayerWeaponBonuses(player, leftHandWeapon, rightHandWeapon);
        }

        leftHandWeapon.Update(delta);
        rightHandWeapon.Update(delta);

        leftHandWeapon.RecalculateDerivedStats(player);
        rightHandWeapon.RecalculateDerivedStats(player);

        if (!inventoryUI.weaponSlots.empty()) {
            inventoryUI.weaponSlots[0] = leftHandWeapon.blueprint ? leftHandWeapon.blueprint->name : "--";
        }
        if (inventoryUI.weaponSlots.size() >= 2) {
            inventoryUI.weaponSlots[1] = rightHandWeapon.blueprint ? rightHandWeapon.blueprint->name : "--";
        }

        Vector2 input{0.0f, 0.0f};
        if (!inventoryUI.open && !debugInputBlocked) {
            if (IsKeyDown(KEY_W)) input.y -= 1.0f;
            if (IsKeyDown(KEY_S)) input.y += 1.0f;
            if (IsKeyDown(KEY_A)) input.x -= 1.0f;
            if (IsKeyDown(KEY_D)) input.x += 1.0f;
        }

        Vector2 desiredPosition = playerPosition;
        if (Vector2LengthSqr(input) > 0.0f) {
            input = Vector2Normalize(input);
            desiredPosition = Vector2Add(desiredPosition, Vector2Scale(input, player.derivedStats.movementSpeed * delta));
        }

        Room& activeRoom = roomManager.GetCurrentRoom();
        ensureRoomEnemies(activeRoom);
        ClampPlayerToAccessibleArea(desiredPosition, PLAYER_HALF_WIDTH, PLAYER_HALF_HEIGHT, activeRoom.Layout());
        if (const ForgeInstance* forge = activeRoom.GetForge()) {
            desiredPosition = ResolveCollisionWithForge(*forge, desiredPosition, PLAYER_HALF_WIDTH, PLAYER_HALF_HEIGHT);
        }
        if (const ShopInstance* shop = activeRoom.GetShop()) {
            desiredPosition = ResolveCollisionWithShop(*shop, desiredPosition, PLAYER_HALF_WIDTH, PLAYER_HALF_HEIGHT);
        }
        if (const Chest* chest = activeRoom.GetChest()) {
            desiredPosition = ResolveCollisionWithChest(*chest, desiredPosition, PLAYER_HALF_WIDTH, PLAYER_HALF_HEIGHT);
        }
        for (const DoorRenderData& doorData : doorRenderData) {
            if (!doorData.fromActiveRoom) {
                continue;
            }
            const Rectangle& collider = (doorData.collisionHitbox.width > 0.0f &&
                                         doorData.collisionHitbox.height > 0.0f)
                                            ? doorData.collisionHitbox
                                            : doorData.hitbox;
            desiredPosition = ResolveCollisionWithRectangle(collider, desiredPosition, PLAYER_HALF_WIDTH, PLAYER_HALF_HEIGHT);
        }

        Vector2 movementDelta = Vector2Subtract(desiredPosition, playerPosition);

        Room* currentRoomPtr = &activeRoom;
        bool movedRoom = false;

        for (auto& door : activeRoom.Layout().doors) {
            if (door.sealed) {
                continue;
            }

            Rectangle interact = DoorInteractionArea(activeRoom.Layout(), door);
            Rectangle playerRect = PlayerBounds(desiredPosition);
            bool colliding = CheckCollisionRecs(playerRect, interact);
            bool movingToward = IsInputMovingToward(door.direction, input);
            bool shouldTransition = ShouldTransitionThroughDoor(door, desiredPosition, movementDelta);

            if (colliding && movingToward && shouldTransition) {
                SaveActiveStations(inventoryUI, roomManager);
                if (roomManager.MoveToNeighbor(door.direction)) {
                    Room& newRoom = roomManager.GetCurrentRoom();
                    std::cout << "Entered room at (" << newRoom.GetCoords().x << "," << newRoom.GetCoords().y << ")" << std::endl;
                    roomManager.EnsureNeighborsGenerated(roomManager.GetCurrentCoords());

                    currentRoomPtr = &newRoom;
                    movedRoom = true;
                }
                break;
            }
        }

        if (!movedRoom) {
            roomManager.EnsureNeighborsGenerated(roomManager.GetCurrentCoords());
        }

        ensureRoomEnemies(*currentRoomPtr);

        ClampPlayerToAccessibleArea(desiredPosition, PLAYER_HALF_WIDTH, PLAYER_HALF_HEIGHT, currentRoomPtr->Layout());
        if (const ForgeInstance* forge = currentRoomPtr->GetForge()) {
            desiredPosition = ResolveCollisionWithForge(*forge, desiredPosition, PLAYER_HALF_WIDTH, PLAYER_HALF_HEIGHT);
        }
        if (const ShopInstance* shop = currentRoomPtr->GetShop()) {
            desiredPosition = ResolveCollisionWithShop(*shop, desiredPosition, PLAYER_HALF_WIDTH, PLAYER_HALF_HEIGHT);
        }
        movementDelta = Vector2Subtract(desiredPosition, playerPosition);
        playerPosition = desiredPosition;
        playerIsMoving = Vector2LengthSqr(movementDelta) > 1.0f;

        UpdateCharacterAnimation(playerSprites, playerIsMoving, delta);

        camera.target = playerPosition;

        Vector2 mouseScreen = GetMousePosition();
        Vector2 mouseWorld = GetScreenToWorld2D(mouseScreen, camera);
        Vector2 aim = Vector2Subtract(mouseWorld, playerPosition);
        if (Vector2LengthSqr(aim) < 1e-6f) {
            aim = Vector2{1.0f, 0.0f};
        }

        ProjectileSpawnContext spawnContext{};
        spawnContext.origin = playerPosition;
        spawnContext.followTarget = &playerPosition;
        spawnContext.aimDirection = aim;

        for (auto& enemyEntry : roomEnemies) {
            Room* enemyRoom = roomManager.TryGetRoom(enemyEntry.first);
            if (enemyRoom == nullptr) {
                continue;
            }
            bool playerInside = (enemyEntry.first == roomManager.GetCurrentCoords());
            for (auto& enemyPtr : enemyEntry.second) {
                if (!enemyPtr || !enemyPtr->IsAlive()) {
                    continue;
                }
                EnemyUpdateContext enemyContext{
                    delta,
                    player,
                    playerPosition,
                    *enemyRoom,
                    playerInside,
                    enemyProjectileSystem
                };
                enemyPtr->Update(enemyContext);
            }
        }

        ForgeInstance* activeForge = nullptr;
        ShopInstance* activeShop = nullptr;
        Vector2 forgeAnchor{0.0f, 0.0f};
        Vector2 shopAnchor{0.0f, 0.0f};
        float forgeRadius = 0.0f;
        float shopRadius = 0.0f;
        bool forgeNearby = false;
        bool shopNearby = false;
        Chest* activeChest = nullptr;
        Vector2 chestAnchor{0.0f, 0.0f};
        float chestRadius = 0.0f;
        bool chestNearby = false;

        if (!inventoryUI.open && !debugInputBlocked) {
            auto weaponInputActive = [](const WeaponState& weapon, int mouseButton) {
                if (weapon.blueprint == nullptr) {
                    return false;
                }
                return weapon.blueprint->holdToFire ? IsMouseButtonDown(mouseButton) : IsMouseButtonPressed(mouseButton);
            };

            if (leftHandWeapon.CanFire() && weaponInputActive(leftHandWeapon, MOUSE_LEFT_BUTTON)) {
                ProjectileBlueprint projectileConfig = leftHandWeapon.blueprint->projectile;
                leftHandWeapon.ApplyDerivedToProjectile(projectileConfig);
                projectileSystem.SpawnProjectile(projectileConfig, spawnContext);
                float appliedCooldown = leftHandWeapon.ResetCooldown();
                rightHandWeapon.EnforceMinimumCooldown(appliedCooldown);
            }

            if (rightHandWeapon.CanFire() && weaponInputActive(rightHandWeapon, MOUSE_RIGHT_BUTTON)) {
                ProjectileBlueprint projectileConfig = rightHandWeapon.blueprint->projectile;
                rightHandWeapon.ApplyDerivedToProjectile(projectileConfig);
                projectileSystem.SpawnProjectile(projectileConfig, spawnContext);
                float appliedCooldown = rightHandWeapon.ResetCooldown();
                leftHandWeapon.EnforceMinimumCooldown(appliedCooldown);
            }
        }

        projectileSystem.Update(delta);

        for (auto& enemyEntry : roomEnemies) {
            Room* enemyRoom = roomManager.TryGetRoom(enemyEntry.first);
            if (enemyRoom == nullptr) {
                continue;
            }
            auto& enemyList = enemyEntry.second;
            for (auto& enemyPtr : enemyList) {
                if (!enemyPtr || !enemyPtr->IsAlive() || !enemyPtr->HasCompletedFade()) {
                    continue;
                }
                auto hits = projectileSystem.CollectDamageEvents(
                    enemyPtr->GetPosition(),
                    enemyPtr->GetCollisionRadius(),
                    reinterpret_cast<std::uintptr_t>(enemyPtr.get()),
                    0.0f);
                if (hits.empty()) {
                    continue;
                }
                for (const auto& hit : hits) {
                    bool died = enemyPtr->TakeDamage(hit.amount);
                    PushDamageNumber(damageNumbers, enemyPtr->GetPosition(), hit.amount, hit.isCritical);
                    if (died) {
                        break;
                    }
                }
            }
            enemyList.erase(
                std::remove_if(enemyList.begin(), enemyList.end(),
                               [](const std::unique_ptr<Enemy>& enemy) {
                                   return !enemy || !enemy->IsAlive();
                               }),
                enemyList.end());

            bool hasActiveEnemies = std::any_of(
                enemyList.begin(),
                enemyList.end(),
                [](const std::unique_ptr<Enemy>& enemy) {
                    return enemy && enemy->IsAlive();
                });
            UpdateDoorInteractionForRoom(*enemyRoom, hasActiveEnemies);
        }

        enemyProjectileSystem.Update(delta);

        auto playerHits = enemyProjectileSystem.CollectDamageEvents(
            playerPosition,
            PLAYER_COLLISION_RADIUS,
            reinterpret_cast<std::uintptr_t>(&player),
            0.0f);
        for (const auto& hit : playerHits) {
            player.currentHealth = std::max(0.0f, player.currentHealth - hit.amount);
            PushDamageNumber(damageNumbers, playerPosition, hit.amount, hit.isCritical);
        }

        Room& interactionRoom = roomManager.GetCurrentRoom();
        activeForge = interactionRoom.GetForge();
        activeShop = interactionRoom.GetShop();
        activeChest = interactionRoom.GetChest();
        const RoomCoords interactionCoords = interactionRoom.GetCoords();

        roomRevealStates[interactionCoords].alpha = 1.0f;
        for (const auto& entry : roomManager.Rooms()) {
            const Room& room = *entry.second;
            if (room.IsVisited()) {
                roomRevealStates[room.GetCoords()].alpha = 1.0f;
            }
        }

        auto resolveRoomVisibility = [&](const Room& room) {
            if (room.GetCoords() == roomManager.GetCurrentCoords()) {
                return 1.0f;
            }
            if (room.IsVisited()) {
                return 1.0f;
            }
            auto it = roomRevealStates.find(room.GetCoords());
            if (it != roomRevealStates.end()) {
                return std::clamp(it->second.alpha, 0.0f, 1.0f);
            }
            return 0.0f;
        };

        doorRenderData.clear();
        doorMaskData.clear();
        doorRenderData.reserve(roomManager.Rooms().size() * 4);
        doorMaskData.reserve(roomManager.Rooms().size() * 4);
        std::unordered_set<DoorInstance*> animatedDoorInstances;
        animatedDoorInstances.reserve(16);

        for (auto& entry : roomManager.Rooms()) {
            Room& room = *entry.second;
            float roomVisibility = resolveRoomVisibility(room);
            if (roomVisibility <= 0.0f) {
                continue;
            }

            bool isActiveRoom = (room.GetCoords() == interactionCoords);
            RoomLayout& layout = room.Layout();
            BiomeType biome = room.GetBiome();

            for (Doorway& door : layout.doors) {
                if (door.sealed || !door.targetGenerated || !door.doorState) {
                    continue;
                }

                DoorInstance& doorState = *door.doorState;
                DoorInstance* instancePtr = door.doorState.get();
                Rectangle doorHitbox = ComputeDoorHitbox(layout, door);
                Rectangle doorCollisionHitbox = ComputeDoorCollisionHitbox(layout, door, doorHitbox);

                if (doorState.opening && !doorState.open) {
                    if (animatedDoorInstances.insert(instancePtr).second) {
                        doorState.fadeProgress = std::min(DOOR_FADE_DURATION, doorState.fadeProgress + delta);
                        if (doorState.fadeProgress >= DOOR_FADE_DURATION) {
                            doorState.fadeProgress = DOOR_FADE_DURATION;
                            doorState.open = true;
                            doorState.maskActive = false;
                        }
                    }
                }

                float doorAlpha = DoorVisibilityAlpha(doorState);
                if (!doorState.open) {
                    float revealAmount = 1.0f - doorAlpha;
                    if (revealAmount > 0.0f) {
                        RoomRevealState& revealState = roomRevealStates[door.targetCoords];
                        revealState.alpha = std::max(revealState.alpha, revealAmount);
                    }
                }

                if (!doorState.open && doorState.maskActive) {
                    DoorMaskData mask{};
                    mask.alpha = doorAlpha * roomVisibility;
                    if (door.corridorTiles.width > 0 && door.corridorTiles.height > 0) {
                        Rectangle corridorMask = TileRectToPixels(door.corridorTiles);
                        if (ClipCorridorMaskBehindDoor(door.direction, doorHitbox, corridorMask)) {
                            if (door.direction == Direction::East || door.direction == Direction::West) {
                                const float wallThickness = static_cast<float>(TILE_SIZE);
                                corridorMask.y -= wallThickness;
                                corridorMask.height += wallThickness * 2.0f;
                                // Ajuste o offset/extra height usando as constantes declaradas no topo do arquivo.
                                corridorMask.y -= HORIZONTAL_CORRIDOR_MASK_VERTICAL_OFFSET;
                                corridorMask.height += HORIZONTAL_CORRIDOR_MASK_VERTICAL_OFFSET;
                                corridorMask.height += HORIZONTAL_CORRIDOR_MASK_EXTRA_HEIGHT;
                            }
                            mask.hasCorridorMask = true;
                            mask.corridorMask = corridorMask;
                        }
                    }
                    if (mask.hasCorridorMask) {
                        doorMaskData.push_back(mask);
                    }
                }

                if (doorState.open) {
                    continue;
                }

                DoorRenderData data{};
                data.doorway = &door;
                data.instance = &doorState;
                data.biome = biome;
                data.frontView = (door.direction == Direction::North || door.direction == Direction::South);
                data.hitbox = doorHitbox;
                data.collisionHitbox = doorCollisionHitbox;
                data.alpha = doorAlpha * roomVisibility;
                data.drawAfterPlayer = (data.hitbox.y > playerPosition.y);
                data.fromActiveRoom = isActiveRoom;
                data.drawAboveMask = (door.direction == Direction::North || door.direction == Direction::South);
                doorRenderData.push_back(data);
            }
        }

        bool debugForgeActive = debugConsole.inventoryContext == DebugConsoleState::InventoryContext::Forge;
        bool debugShopActive = debugConsole.inventoryContext == DebugConsoleState::InventoryContext::Shop;
        bool debugChestActive = debugConsole.inventoryContext == DebugConsoleState::InventoryContext::Chest;

        if ((inventoryUI.hasActiveForge && inventoryUI.activeForgeCoords != interactionCoords) ||
            (inventoryUI.hasActiveShop && inventoryUI.activeShopCoords != interactionCoords) ||
            (inventoryUI.hasActiveChest && inventoryUI.activeChestCoords != interactionCoords)) {
            SaveActiveStations(inventoryUI, roomManager);
            if (inventoryUI.hasActiveForge && inventoryUI.activeForgeCoords != interactionCoords) {
                inventoryUI.hasActiveForge = false;
                inventoryUI.pendingForgeBreak = false;
                if (inventoryUI.mode == InventoryViewMode::Forge && inventoryUI.open) {
                    inventoryUI.mode = InventoryViewMode::Inventory;
                    inventoryUI.selectedForgeSlot = -1;
                }
            }
            if (inventoryUI.hasActiveShop && inventoryUI.activeShopCoords != interactionCoords) {
                inventoryUI.hasActiveShop = false;
                inventoryUI.selectedShopIndex = -1;
                inventoryUI.shopTradeActive = false;
                inventoryUI.shopTradeReadyToConfirm = false;
                inventoryUI.shopTradeInventoryIndex = -1;
                inventoryUI.shopTradeShopIndex = -1;
                inventoryUI.shopTradeRequiredRarity = 0;
                if (inventoryUI.mode == InventoryViewMode::Shop && inventoryUI.open) {
                    inventoryUI.mode = InventoryViewMode::Inventory;
                }
            }
            if (inventoryUI.hasActiveChest && inventoryUI.activeChestCoords != interactionCoords) {
                inventoryUI.hasActiveChest = false;
                inventoryUI.activeChest = nullptr;
                inventoryUI.chestUiType = InventoryUIState::ChestUIType::None;
                inventoryUI.chestSupportsDeposit = false;
                inventoryUI.chestSupportsTakeAll = false;
                inventoryUI.selectedChestIndex = -1;
                if (inventoryUI.mode == InventoryViewMode::Chest && inventoryUI.open) {
                    inventoryUI.mode = InventoryViewMode::Inventory;
                }
            }
        }

        if (!debugForgeActive) {
            if (activeForge != nullptr) {
                forgeAnchor = Vector2{activeForge->anchorX, activeForge->anchorY};
                forgeRadius = activeForge->interactionRadius;
                float distanceSq = Vector2DistanceSqr(playerPosition, forgeAnchor);
                forgeNearby = distanceSq <= (forgeRadius * forgeRadius);

                if (!debugInputBlocked && forgeNearby && IsKeyPressed(KEY_E)) {
                    SaveActiveStations(inventoryUI, roomManager);
                    inventoryUI.open = true;
                    inventoryUI.mode = InventoryViewMode::Forge;
                    inventoryUI.selectedForgeSlot = -1;
                    inventoryUI.hasActiveForge = true;
                    inventoryUI.activeForgeCoords = interactionCoords;
                    inventoryUI.pendingForgeBreak = false;
                    LoadForgeContents(inventoryUI, *activeForge);

                    if (activeForge->IsBroken()) {
                        inventoryUI.feedbackMessage = "A forja esta quebrada... precisa de reparos.";
                        inventoryUI.feedbackTimer = 2.5f;
                    } else {
                        inventoryUI.feedbackMessage.clear();
                        inventoryUI.feedbackTimer = 0.0f;
                    }
                }
            } else {
                SaveActiveForgeContents(inventoryUI, roomManager);
                if (inventoryUI.mode == InventoryViewMode::Forge) {
                    if (inventoryUI.open) {
                        inventoryUI.mode = InventoryViewMode::Inventory;
                        inventoryUI.selectedForgeSlot = -1;
                    }
                    inventoryUI.forgeState = ForgeState::Working;
                }
                inventoryUI.hasActiveForge = false;
                inventoryUI.pendingForgeBreak = false;
            }
        }

        if (!debugShopActive) {
            if (activeShop != nullptr) {
                shopAnchor = Vector2{activeShop->anchorX, activeShop->anchorY};
                shopRadius = activeShop->interactionRadius;
                float distanceSq = Vector2DistanceSqr(playerPosition, shopAnchor);
                shopNearby = distanceSq <= (shopRadius * shopRadius);

                if (!debugInputBlocked && shopNearby && IsKeyPressed(KEY_E)) {
                    SaveActiveStations(inventoryUI, roomManager);
                    inventoryUI.open = true;
                    inventoryUI.mode = InventoryViewMode::Shop;
                    inventoryUI.selectedShopIndex = -1;
                    inventoryUI.hasActiveShop = true;
                    inventoryUI.activeShopCoords = interactionCoords;
                    LoadShopContents(inventoryUI, *activeShop);
                    inventoryUI.feedbackMessage.clear();
                    inventoryUI.feedbackTimer = 0.0f;
                }
            } else if (inventoryUI.hasActiveShop) {
                SaveActiveShopContents(inventoryUI, roomManager);
                if (inventoryUI.mode == InventoryViewMode::Shop) {
                    if (inventoryUI.open) {
                        inventoryUI.mode = InventoryViewMode::Inventory;
                    }
                    inventoryUI.selectedShopIndex = -1;
                }
                inventoryUI.hasActiveShop = false;
                inventoryUI.shopTradeActive = false;
                inventoryUI.shopTradeReadyToConfirm = false;
                inventoryUI.shopTradeInventoryIndex = -1;
                inventoryUI.shopTradeShopIndex = -1;
                inventoryUI.shopTradeRequiredRarity = 0;
            }
        }

        if (!debugChestActive) {
            if (activeChest != nullptr) {
                chestAnchor = Vector2{activeChest->AnchorX(), activeChest->AnchorY()};
                chestRadius = activeChest->InteractionRadius();
                float distanceSq = Vector2DistanceSqr(playerPosition, chestAnchor);
                chestNearby = distanceSq <= (chestRadius * chestRadius);

                if (!debugInputBlocked && chestNearby && IsKeyPressed(KEY_E)) {
                    SaveActiveStations(inventoryUI, roomManager);
                    inventoryUI.open = true;
                    inventoryUI.mode = InventoryViewMode::Chest;
                    inventoryUI.selectedChestIndex = -1;
                    inventoryUI.selectedInventoryIndex = -1;
                    inventoryUI.selectedWeaponIndex = -1;
                    inventoryUI.selectedEquipmentIndex = -1;
                    inventoryUI.selectedShopIndex = -1;
                    inventoryUI.selectedForgeSlot = -1;
                    inventoryUI.hasActiveChest = true;
                    inventoryUI.activeChestCoords = interactionCoords;
                    LoadChestContents(inventoryUI, *activeChest);
                }
            } else if (inventoryUI.hasActiveChest) {
                if (inventoryUI.mode == InventoryViewMode::Chest) {
                    if (inventoryUI.open) {
                        inventoryUI.mode = InventoryViewMode::Inventory;
                    }
                    inventoryUI.selectedChestIndex = -1;
                }
                inventoryUI.hasActiveChest = false;
                inventoryUI.activeChest = nullptr;
                inventoryUI.chestUiType = InventoryUIState::ChestUIType::None;
                inventoryUI.chestSupportsDeposit = false;
                inventoryUI.chestSupportsTakeAll = false;
                inventoryUI.activeChestCoords = RoomCoords{};
                inventoryUI.chestTitle.clear();
                inventoryUI.chestItemIds.clear();
                inventoryUI.chestItems.clear();
                inventoryUI.chestQuantities.clear();
                inventoryUI.chestTypes.clear();
            }
        }

        if (!inventoryUI.open &&
            debugConsole.inventoryContext != DebugConsoleState::InventoryContext::None) {
            ResetDebugInventoryContext(debugConsole, inventoryUI);
        }

        if (trainingDummy.isImmune) {
            trainingDummy.immunitySecondsRemaining -= delta;
            if (trainingDummy.immunitySecondsRemaining <= 0.0f) {
                trainingDummy.immunitySecondsRemaining = 0.0f;
                trainingDummy.isImmune = false;
            }
        }

        bool dummyActive = (roomManager.GetCurrentCoords() == trainingDummy.homeRoom);
        if (dummyActive) {
            float dummyImmunity = trainingDummy.isImmune ? trainingDummy.immunitySecondsRemaining : 0.0f;
            std::vector<ProjectileSystem::DamageEvent> damageEvents = projectileSystem.CollectDamageEvents(
                trainingDummy.position,
                trainingDummy.radius,
                reinterpret_cast<std::uintptr_t>(&trainingDummy),
                dummyImmunity);
            for (const auto& event : damageEvents) {
                DamageNumber number{};
                number.amount = event.amount;
                number.isCritical = event.isCritical;
                number.lifetime = event.isCritical ? 1.4f : 1.0f;
                int jitterX = GetRandomValue(-12, 12);
                int jitterY = GetRandomValue(-6, 6);
                number.position = Vector2{
                    trainingDummy.position.x + static_cast<float>(jitterX),
                    trainingDummy.position.y - trainingDummy.radius + static_cast<float>(jitterY)
                };
                damageNumbers.push_back(number);

                trainingDummy.immunitySecondsRemaining = std::max(trainingDummy.immunitySecondsRemaining,
                                                                   event.suggestedImmunitySeconds);
                trainingDummy.isImmune = trainingDummy.immunitySecondsRemaining > 0.0f;
            }
        }

        UpdateDamageNumbers(damageNumbers, delta);

        DoorRenderData* activeDoorPrompt = nullptr;
        float closestDoorDistance = std::numeric_limits<float>::max();
        for (DoorRenderData& doorData : doorRenderData) {
            doorData.showPrompt = false;
            doorData.isLocked = false;
            if (!doorData.fromActiveRoom) {
                continue;
            }
            if (doorData.instance == nullptr) {
                continue;
            }
            DoorInstance& doorState = *doorData.instance;
            if (doorState.open || doorState.opening) {
                continue;
            }
            if (doorState.interactionState == DoorInteractionState::Unavailable) {
                continue;
            }

            Vector2 doorCenter{
                doorData.hitbox.x + doorData.hitbox.width * 0.5f,
                doorData.hitbox.y + doorData.hitbox.height * 0.5f
            };
            float distance = Vector2Distance(playerPosition, doorCenter);
            if (distance <= DOOR_INTERACTION_DISTANCE && distance < closestDoorDistance) {
                closestDoorDistance = distance;
                activeDoorPrompt = &doorData;
                doorData.promptAnchor = doorCenter;
            }
        }

        if (activeDoorPrompt != nullptr) {
            activeDoorPrompt->showPrompt = true;
            activeDoorPrompt->isLocked = (activeDoorPrompt->instance->interactionState == DoorInteractionState::Locked);
            if (!debugInputBlocked && !inventoryUI.open && IsKeyPressed(KEY_E)) {
                if (!activeDoorPrompt->isLocked) {
                    activeDoorPrompt->instance->opening = true;
                    activeDoorPrompt->instance->fadeProgress = 0.0f;
                }
            }
        }

        Vector2 snappedPlayerPosition = SnapToPixel(playerPosition);
        Camera2D renderCamera = camera;
        renderCamera.target = snappedPlayerPosition;

        BeginDrawing();
        ClearBackground(Color{24, 26, 33, 255});

        BeginMode2D(renderCamera);
        for (const auto& entry : roomManager.Rooms()) {
            const Room& room = *entry.second;
            bool isActive = (room.GetCoords() == roomManager.GetCurrentCoords());
            float roomVisibility = resolveRoomVisibility(room);
            if (roomVisibility <= 0.0f) {
                continue;
            }
            roomRenderer.DrawRoomBackground(room, isActive, roomVisibility);
        }

        if (dummyActive) {
            DrawCircleV(trainingDummy.position, trainingDummy.radius, Color{96, 128, 196, 80});
            DrawCircleLines(static_cast<int>(trainingDummy.position.x), static_cast<int>(trainingDummy.position.y), trainingDummy.radius, Color{190, 210, 255, 220});

            const char* label = "Dummy de treino";
            const float labelSize = 20.0f;
            const Font& font = GetGameFont();
            Vector2 labelMeasure = MeasureTextEx(font, label, labelSize, 0.0f);
            Vector2 labelPos{trainingDummy.position.x - labelMeasure.x * 0.5f, trainingDummy.position.y + trainingDummy.radius + 10.0f};
            DrawTextEx(font, label, labelPos, labelSize, 0.0f, Color{210, 220, 240, 220});
        }

        bool drawForgeAfterPlayer = false;
        bool drawShopAfterPlayer = false;
        bool drawChestAfterPlayer = false;
        if (activeForge != nullptr) {
            float playerBottom = playerPosition.y + PLAYER_HALF_HEIGHT;
            bool playerInFront = (playerBottom >= activeForge->anchorY);
            if (playerInFront) {
                roomRenderer.DrawForgeInstance(*activeForge, true);
            } else {
                drawForgeAfterPlayer = true;
            }
        }

        if (activeShop != nullptr) {
            float playerBottom = playerPosition.y + PLAYER_HALF_HEIGHT;
            bool playerInFront = (playerBottom >= activeShop->anchorY);
            if (playerInFront) {
                roomRenderer.DrawShopInstance(*activeShop, true);
            } else {
                drawShopAfterPlayer = true;
            }
        }

        if (activeChest != nullptr) {
            float playerBottom = playerPosition.y + PLAYER_HALF_HEIGHT;
            bool playerInFront = (playerBottom >= activeChest->AnchorY());
            if (playerInFront) {
                roomRenderer.DrawChestInstance(*activeChest, true);
            } else {
                drawChestAfterPlayer = true;
            }
        }

        auto drawEnemies = [&](bool drawAfterPlayer) {
            for (const auto& enemyEntry : roomEnemies) {
                const Room* enemyRoom = roomManager.TryGetRoom(enemyEntry.first);
                if (enemyRoom == nullptr) {
                    continue;
                }
                float roomVisibility = resolveRoomVisibility(*enemyRoom);
                if (roomVisibility <= 0.0f) {
                    continue;
                }
                bool isActiveRoom = (enemyRoom->GetCoords() == roomManager.GetCurrentCoords());
                for (const auto& enemyPtr : enemyEntry.second) {
                    if (!enemyPtr || !enemyPtr->IsAlive()) {
                        continue;
                    }
                    bool enemyAfterPlayer = false;
                    if (isActiveRoom) {
                        enemyAfterPlayer = (enemyPtr->GetPosition().y >= playerPosition.y);
                    }
                    if (enemyAfterPlayer != drawAfterPlayer) {
                        continue;
                    }
                    EnemyDrawContext drawContext{};
                    drawContext.roomVisibility = roomVisibility;
                    drawContext.isActiveRoom = isActiveRoom;
                    enemyPtr->Draw(drawContext);
                }
            }
        };

        auto drawDoors = [&](bool drawAfterPlayer, bool aboveMask) {
            for (const DoorRenderData& doorData : doorRenderData) {
                if (doorData.drawAfterPlayer != drawAfterPlayer || doorData.drawAboveMask != aboveMask || doorData.doorway == nullptr) {
                    continue;
                }
                roomRenderer.DrawDoorSprite(doorData.hitbox, doorData.doorway->direction, doorData.biome, doorData.alpha);
            }
        };

        drawDoors(false, false);
        drawEnemies(false);

        if (!DrawCharacterSprite(playerSprites, snappedPlayerPosition, playerIsMoving)) {
            Rectangle renderRect{
                snappedPlayerPosition.x - PLAYER_RENDER_HALF_WIDTH,
                snappedPlayerPosition.y - PLAYER_RENDER_HALF_HEIGHT,
                PLAYER_RENDER_HALF_WIDTH * 2.0f,
                PLAYER_RENDER_HALF_HEIGHT * 2.0f
            };
            DrawRectangleRec(renderRect, Color{120, 180, 220, 255});
            DrawRectangleLinesEx(renderRect, 2.0f, Color{30, 60, 90, 255});
        }

        drawEnemies(true);
        drawDoors(true, false);

        if (drawForgeAfterPlayer && activeForge != nullptr) {
            roomRenderer.DrawForgeInstance(*activeForge, true);
        }
        if (drawShopAfterPlayer && activeShop != nullptr) {
            roomRenderer.DrawShopInstance(*activeShop, true);
        }
        if (drawChestAfterPlayer && activeChest != nullptr) {
            roomRenderer.DrawChestInstance(*activeChest, true);
        }

        projectileSystem.Draw();
        enemyProjectileSystem.Draw();

        if (!damageNumbers.empty()) {
            DrawDamageNumbers(damageNumbers);
        }

        for (const auto& entry : roomManager.Rooms()) {
            const Room& room = *entry.second;
            bool isActive = (room.GetCoords() == roomManager.GetCurrentCoords());
            float roomVisibility = resolveRoomVisibility(room);
            if (roomVisibility <= 0.0f) {
                continue;
            }
            roomRenderer.DrawRoomForeground(room, isActive, roomVisibility);
        }

        for (const DoorMaskData& mask : doorMaskData) {
            if (!mask.hasCorridorMask) {
                continue;
            }
            Color maskColor = DoorMaskColor(mask.alpha);
            DrawRectangleRec(mask.corridorMask, maskColor);
        }

        drawDoors(false, true);
        drawDoors(true, true);

        if (activeForge != nullptr && forgeNearby) {
            const char* promptText = activeForge->IsBroken() ? "Forja quebrada (E para inspecionar)" : "Pressione E para usar a forja";
            const Font& font = GetGameFont();
            const float fontSize = 22.0f;
            Vector2 textSize = MeasureTextEx(font, promptText, fontSize, 0.0f);
            float bubblePadding = 12.0f;
            float bubbleWidth = textSize.x + bubblePadding * 2.0f;
            float bubbleHeight = textSize.y + bubblePadding * 1.5f;
            float bubbleX = forgeAnchor.x - bubbleWidth * 0.5f;
            float bubbleY = forgeAnchor.y - forgeRadius - bubbleHeight - 10.0f;
            bubbleY = std::max(bubbleY, forgeAnchor.y - forgeRadius - 180.0f);
            Rectangle bubble{bubbleX, bubbleY, bubbleWidth, bubbleHeight};
            DrawRectangleRec(bubble, Color{20, 26, 36, 210});
            DrawRectangleLinesEx(bubble, 2.0f, Color{70, 92, 126, 240});
            Vector2 textPos{bubble.x + bubblePadding, bubble.y + bubblePadding * 0.6f};
            DrawTextEx(font, promptText, textPos, fontSize, 0.0f, Color{235, 240, 252, 255});
        }

        if (activeShop != nullptr && shopNearby) {
            const char* promptText = "Pressione E para acessar a loja";
            const Font& font = GetGameFont();
            const float fontSize = 22.0f;
            Vector2 textSize = MeasureTextEx(font, promptText, fontSize, 0.0f);
            float bubblePadding = 12.0f;
            float bubbleWidth = textSize.x + bubblePadding * 2.0f;
            float bubbleHeight = textSize.y + bubblePadding * 1.5f;
            float bubbleX = shopAnchor.x - bubbleWidth * 0.5f;
            float bubbleY = shopAnchor.y - shopRadius - bubbleHeight - 10.0f;
            bubbleY = std::max(bubbleY, shopAnchor.y - shopRadius - 180.0f);
            Rectangle bubble{bubbleX, bubbleY, bubbleWidth, bubbleHeight};
            DrawRectangleRec(bubble, Color{20, 26, 36, 210});
            DrawRectangleLinesEx(bubble, 2.0f, Color{70, 92, 126, 240});
            Vector2 textPos{bubble.x + bubblePadding, bubble.y + bubblePadding * 0.6f};
            DrawTextEx(font, promptText, textPos, fontSize, 0.0f, Color{235, 240, 252, 255});
        }

        if (activeChest != nullptr && chestNearby) {
            const char* promptText = "Pressione E para abrir o bau";
            const Font& font = GetGameFont();
            const float fontSize = 22.0f;
            Vector2 textSize = MeasureTextEx(font, promptText, fontSize, 0.0f);
            float bubblePadding = 12.0f;
            float bubbleWidth = textSize.x + bubblePadding * 2.0f;
            float bubbleHeight = textSize.y + bubblePadding * 1.5f;
            float bubbleX = chestAnchor.x - bubbleWidth * 0.5f;
            float bubbleY = chestAnchor.y - chestRadius - bubbleHeight - 10.0f;
            bubbleY = std::max(bubbleY, chestAnchor.y - chestRadius - 180.0f);
            Rectangle bubble{bubbleX, bubbleY, bubbleWidth, bubbleHeight};
            DrawRectangleRec(bubble, Color{20, 26, 36, 210});
            DrawRectangleLinesEx(bubble, 2.0f, Color{70, 92, 126, 240});
            Vector2 textPos{bubble.x + bubblePadding, bubble.y + bubblePadding * 0.6f};
            DrawTextEx(font, promptText, textPos, fontSize, 0.0f, Color{235, 240, 252, 255});
        }

        for (const DoorRenderData& doorData : doorRenderData) {
            if (!doorData.showPrompt) {
                continue;
            }
            const char* promptText = doorData.isLocked ? "A porta esta trancada" : "Pressione E para abrir a porta";
            const Font& font = GetGameFont();
            const float fontSize = 22.0f;
            Vector2 textSize = MeasureTextEx(font, promptText, fontSize, 0.0f);
            float bubblePadding = 12.0f;
            float bubbleWidth = textSize.x + bubblePadding * 2.0f;
            float bubbleHeight = textSize.y + bubblePadding * 1.4f;
            float bubbleX = doorData.promptAnchor.x - bubbleWidth * 0.5f;
            float bubbleY = doorData.promptAnchor.y - doorData.hitbox.height - bubbleHeight - 20.0f;
            Rectangle bubble{bubbleX, bubbleY, bubbleWidth, bubbleHeight};
            DrawRectangleRec(bubble, Color{20, 26, 36, 210});
            DrawRectangleLinesEx(bubble, 2.0f, Color{70, 92, 126, 240});
            Vector2 textPos{bubble.x + bubblePadding, bubble.y + bubblePadding * 0.4f};
            DrawTextEx(font, promptText, textPos, fontSize, 0.0f, Color{235, 240, 252, 255});
        }

        EndMode2D();

        DrawHUD(player, inventoryUI);

        if (inventoryUI.open) {
            RenderInventoryUI(inventoryUI, player, leftHandWeapon, rightHandWeapon,
                              Vector2{static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())},
                              activeShop);
        }

        if (debugConsole.open) {
            DrawDebugConsoleOverlay(debugConsole);
        }

        SaveActiveStations(inventoryUI, roomManager);

        EndDrawing();
    }

    EnemyCommon::ShutdownSpriteCache();
    UnloadCharacterSprites(playerSprites);
    UnloadGameFont();
    CloseWindow();
    return 0;
}  