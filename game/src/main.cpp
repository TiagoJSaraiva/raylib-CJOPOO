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

namespace {

constexpr int SCREEN_WIDTH = 1920;
constexpr int SCREEN_HEIGHT = 1080;
constexpr float PLAYER_HALF_SIZE = 18.0f;
constexpr float PLAYER_RENDER_HALF_SIZE = PLAYER_HALF_SIZE - 3.0f;

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

    const Font font = GetFontDefault();

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
        center.x - PLAYER_HALF_SIZE,
        center.y - PLAYER_HALF_SIZE,
        PLAYER_HALF_SIZE * 2.0f,
        PLAYER_HALF_SIZE * 2.0f
    };
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

bool IsSquareInsideRect(const Rectangle& rect, const Vector2& position, float halfSize, float tolerance = 0.0f) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return false;
    }
    const float minX = rect.x + halfSize - tolerance;
    const float maxX = rect.x + rect.width - halfSize + tolerance;
    const float minY = rect.y + halfSize - tolerance;
    const float maxY = rect.y + rect.height - halfSize + tolerance;
    return position.x >= minX && position.x <= maxX && position.y >= minY && position.y <= maxY;
}

Vector2 ClampSquareToRect(const Rectangle& rect, const Vector2& position, float halfSize, float tolerance = 0.0f) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return position;
    }

    float minX = rect.x + halfSize - tolerance;
    float maxX = rect.x + rect.width - halfSize + tolerance;
    float minY = rect.y + halfSize - tolerance;
    float maxY = rect.y + rect.height - halfSize + tolerance;

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

void ClampPlayerToAccessibleArea(Vector2& position, float halfSize, const RoomLayout& layout) {
    constexpr float tolerance = 4.0f;

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

    if (IsSquareInsideRect(floor, position, halfSize, tolerance)) {
        return;
    }

    auto isInsideRegion = [&](const AccessibleRegion& region) {
        if (!region.isCorridor) {
            return IsSquareInsideRect(region.detectRect, position, halfSize, tolerance);
        }

        const Rectangle& rect = region.detectRect;
        if (rect.width <= 0.0f || rect.height <= 0.0f) {
            return false;
        }

        if (region.direction == Direction::North || region.direction == Direction::South) {
            float minCenterX = rect.x + halfSize - tolerance;
            float maxCenterX = rect.x + rect.width - halfSize + tolerance;
            float minCenterY = rect.y - halfSize - tolerance;
            float maxCenterY = rect.y + rect.height + halfSize + tolerance;
            return position.x >= minCenterX && position.x <= maxCenterX && position.y >= minCenterY && position.y <= maxCenterY;
        }

        float minCenterY = rect.y + halfSize - tolerance;
        float maxCenterY = rect.y + rect.height - halfSize + tolerance;
        float minCenterX = rect.x - halfSize - tolerance;
        float maxCenterX = rect.x + rect.width + halfSize + tolerance;
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

        float minX = rect.x + halfSize - tolerance;
        float maxX = rect.x + rect.width - halfSize + tolerance;
        float minY = rect.y + halfSize - tolerance;
        float maxY = rect.y + rect.height - halfSize + tolerance;

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
            candidate = ClampSquareToRect(rect, position, halfSize, tolerance);
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

    Vector2 floorClamp = ClampSquareToRect(floor, position, halfSize, tolerance);
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
        constexpr float kForwardDepth = PLAYER_HALF_SIZE - 4.0f;
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
                return playerTop <= (corridorBottom - kForwardDepth);
            case Direction::South:
                if (movement.y <= kForwardEpsilon) {
                    return false;
                }
                if (playerRight < corridorLeft - kLateralTolerance || playerLeft > corridorRight + kLateralTolerance) {
                    return false;
                }
                return playerBottom >= (corridorTop + kForwardDepth);
            case Direction::East:
                if (movement.x <= kForwardEpsilon) {
                    return false;
                }
                if (playerBottom < corridorTop - kLateralTolerance || playerTop > corridorBottom + kLateralTolerance) {
                    return false;
                }
                return playerRight >= (corridorLeft + kForwardDepth);
            case Direction::West:
                if (movement.x >= -kForwardEpsilon) {
                    return false;
                }
                if (playerBottom < corridorTop - kLateralTolerance || playerTop > corridorBottom + kLateralTolerance) {
                    return false;
                }
                return playerLeft <= (corridorRight - kForwardDepth);
        }
    }

    return true;
}
} // namespace

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Prototype - Room Generation");
    SetTargetFPS(60);

    const std::uint64_t worldSeed = GenerateWorldSeed();
    RoomManager roomManager{worldSeed};
    RoomRenderer roomRenderer;
    ProjectileSystem projectileSystem;
    PlayerCharacter player = CreateKnightCharacter();
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

    while (!WindowShouldClose()) {
        const float delta = GetFrameTime();

        if (IsKeyPressed(KEY_TAB) || IsKeyPressed(KEY_I)) {
            inventoryUI.open = !inventoryUI.open;
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
        ClampPlayerToAccessibleArea(desiredPosition, PLAYER_HALF_SIZE, activeRoom.Layout());

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

            if (colliding && movingToward && !shouldTransition) {
                std::cout << "Transition blocked | door dir=" << static_cast<int>(door.direction)
                          << " playerPos=(" << desiredPosition.x << "," << desiredPosition.y << ")";
                Rectangle corridorRect = TileRectToPixels(door.corridorTiles);
                std::cout << " corridorRect=(" << corridorRect.x << "," << corridorRect.y << ","
                          << corridorRect.width << "," << corridorRect.height << ")" << std::endl;
            }

            if (colliding && movingToward && shouldTransition) {
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

        ClampPlayerToAccessibleArea(desiredPosition, PLAYER_HALF_SIZE, currentRoomPtr->Layout());
        playerPosition = desiredPosition;

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
            Vector2 labelMeasure = MeasureTextEx(GetFontDefault(), label, labelSize, 0.0f);
            Vector2 labelPos{trainingDummy.position.x - labelMeasure.x * 0.5f, trainingDummy.position.y + trainingDummy.radius + 10.0f};
            DrawTextEx(GetFontDefault(), label, labelPos, labelSize, 0.0f, Color{210, 220, 240, 220});
        }

        projectileSystem.Draw();

        if (!damageNumbers.empty()) {
            DrawDamageNumbers(damageNumbers);
        }

        Rectangle renderRect{
            playerPosition.x - PLAYER_RENDER_HALF_SIZE,
            playerPosition.y - PLAYER_RENDER_HALF_SIZE,
            PLAYER_RENDER_HALF_SIZE * 2.0f,
            PLAYER_RENDER_HALF_SIZE * 2.0f
        };
        DrawRectangleRec(renderRect, Color{120, 180, 220, 255});
        DrawRectangleLinesEx(renderRect, 2.0f, Color{30, 60, 90, 255});

        for (const auto& entry : roomManager.Rooms()) {
            const Room& room = *entry.second;
            bool isActive = (room.GetCoords() == roomManager.GetCurrentCoords());
            roomRenderer.DrawRoomForeground(room, isActive);
        }

        EndMode2D();

        if (inventoryUI.open) {
            RenderInventoryUI(inventoryUI, player, leftHandWeapon, rightHandWeapon,
                              Vector2{static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight())});
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}  