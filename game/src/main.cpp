#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <optional>
#include <limits>
#include <iostream>
#include <random>
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

namespace {

constexpr int SCREEN_WIDTH = 1920;
constexpr int SCREEN_HEIGHT = 1080;
constexpr float PLAYER_HALF_WIDTH = 20.0f;
constexpr float PLAYER_HALF_HEIGHT = 16.0f;
constexpr float PLAYER_RENDER_HALF_WIDTH = PLAYER_HALF_WIDTH - 3.0f;
constexpr float PLAYER_RENDER_HALF_HEIGHT = PLAYER_HALF_HEIGHT - 3.0f;

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

Rectangle DoorInteractionArea(const RoomLayout& layout, const Doorway& door) {
    if (door.corridorTiles.width > 0 && door.corridorTiles.height > 0) {
        return TileRectToPixels(door.corridorTiles);
    }
    return DoorRectInsideRoom(layout, door);
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
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Prototype - Room Generation");
    SetTargetFPS(60);
    LoadGameFont("assets/font/alagard.ttf", 32);

    const std::uint64_t worldSeed = GenerateWorldSeed();
    RoomManager roomManager{worldSeed};
    RoomRenderer roomRenderer;
    ProjectileSystem projectileSystem;
    PlayerCharacter player = CreateKnightCharacter();
    CharacterSpriteResources playerSprites{};
    LoadCharacterSprites(player.appearance, playerSprites);
    WeaponState leftHandWeapon;
    leftHandWeapon.blueprint = &GetEspadaCurtaWeaponBlueprint();
    WeaponState rightHandWeapon;
    rightHandWeapon.blueprint = &GetArcoSimplesWeaponBlueprint();

    InventoryUIState inventoryUI;
    InitializeInventoryUIDummyData(inventoryUI);

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

    Camera2D camera{};
    camera.offset = Vector2{SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f};
    camera.target = playerPosition;
    camera.zoom = 1.0f;

    bool playerIsMoving = false;

    while (!WindowShouldClose()) {
        const float delta = GetFrameTime();

        if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_I)) {
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
        if (!inventoryUI.open) {
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

        if (!inventoryUI.open) {
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

        Room& interactionRoom = roomManager.GetCurrentRoom();
        activeForge = interactionRoom.GetForge();
        activeShop = interactionRoom.GetShop();
    activeChest = interactionRoom.GetChest();
        const RoomCoords interactionCoords = interactionRoom.GetCoords();

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

        if (activeForge != nullptr) {
            forgeAnchor = Vector2{activeForge->anchorX, activeForge->anchorY};
            forgeRadius = activeForge->interactionRadius;
            float distanceSq = Vector2DistanceSqr(playerPosition, forgeAnchor);
            forgeNearby = distanceSq <= (forgeRadius * forgeRadius);

            if (forgeNearby && IsKeyPressed(KEY_E)) {
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

        if (activeShop != nullptr) {
            shopAnchor = Vector2{activeShop->anchorX, activeShop->anchorY};
            shopRadius = activeShop->interactionRadius;
            float distanceSq = Vector2DistanceSqr(playerPosition, shopAnchor);
            shopNearby = distanceSq <= (shopRadius * shopRadius);

            if (shopNearby && IsKeyPressed(KEY_E)) {
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

        if (activeChest != nullptr) {
            chestAnchor = Vector2{activeChest->AnchorX(), activeChest->AnchorY()};
            chestRadius = activeChest->InteractionRadius();
            float distanceSq = Vector2DistanceSqr(playerPosition, chestAnchor);
            chestNearby = distanceSq <= (chestRadius * chestRadius);

            if (chestNearby && IsKeyPressed(KEY_E)) {
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

        BeginDrawing();
        ClearBackground(Color{24, 26, 33, 255});

        BeginMode2D(camera);
        for (const auto& entry : roomManager.Rooms()) {
            const Room& room = *entry.second;
            bool isActive = (room.GetCoords() == roomManager.GetCurrentCoords());
            roomRenderer.DrawRoomBackground(room, isActive);
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

        if (!DrawCharacterSprite(playerSprites, playerPosition, playerIsMoving)) {
            Rectangle renderRect{
                playerPosition.x - PLAYER_RENDER_HALF_WIDTH,
                playerPosition.y - PLAYER_RENDER_HALF_HEIGHT,
                PLAYER_RENDER_HALF_WIDTH * 2.0f,
                PLAYER_RENDER_HALF_HEIGHT * 2.0f
            };
            DrawRectangleRec(renderRect, Color{120, 180, 220, 255});
            DrawRectangleLinesEx(renderRect, 2.0f, Color{30, 60, 90, 255});
        }

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

        if (!damageNumbers.empty()) {
            DrawDamageNumbers(damageNumbers);
        }

        for (const auto& entry : roomManager.Rooms()) {
            const Room& room = *entry.second;
            bool isActive = (room.GetCoords() == roomManager.GetCurrentCoords());
            roomRenderer.DrawRoomForeground(room, isActive);
        }

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

        EndMode2D();

        if (inventoryUI.open) {
            RenderInventoryUI(inventoryUI, player, leftHandWeapon, rightHandWeapon,
                              Vector2{static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())},
                              activeShop);
        }

        SaveActiveStations(inventoryUI, roomManager);

        EndDrawing();
    }

    UnloadCharacterSprites(playerSprites);
    UnloadGameFont();
    CloseWindow();
    return 0;
}  