#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <limits>
#include <iostream>
#include <random>
#include <vector>

#include "room_manager.h"
#include "room_renderer.h"
#include "room_types.h"
#include "projectile.h"
#include "weapon.h"

namespace {

constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;
constexpr float PLAYER_SPEED = 240.0f;
constexpr float PLAYER_HALF_SIZE = 18.0f;
constexpr float PLAYER_RENDER_HALF_SIZE = PLAYER_HALF_SIZE - 3.0f;

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

ProjectileBlueprint MakeBroquelProjectileBlueprint() {
    ProjectileBlueprint blueprint{};
    blueprint.kind = ProjectileKind::Blunt;
    blueprint.common.damage = 8.0f;
    blueprint.common.lifespanSeconds = 0.8f;
    blueprint.common.projectileSpeed = 0.0f;
    blueprint.common.projectileSize = 32.0f;
    blueprint.common.projectilesPerShot = 1;
    blueprint.common.randomSpreadDegrees = 0.0f;
    blueprint.common.debugColor = Color{210, 240, 160, 255};
    blueprint.common.spriteId = "broquel_projectile";

    blueprint.blunt.radius = 60.0f;
    blueprint.blunt.travelDegrees = 0.0f;
    blueprint.blunt.arcSpanDegrees = 60.0f;
    blueprint.blunt.thickness = 18.0f;
    blueprint.blunt.followOwner = true;

    return blueprint;
}

WeaponBlueprint MakeBroquelWeaponBlueprint() {
    WeaponBlueprint blueprint{};
    blueprint.name = "Broquel";
    blueprint.projectile = MakeBroquelProjectileBlueprint();
    blueprint.cooldownSeconds = 0.5f;
    blueprint.holdToFire = false;
    return blueprint;
}

const WeaponBlueprint& GetBroquelWeaponBlueprint() {
    static WeaponBlueprint blueprint = MakeBroquelWeaponBlueprint();
    return blueprint;
}

ProjectileBlueprint MakeLongswordProjectileBlueprint() {
    ProjectileBlueprint blueprint{};
    blueprint.kind = ProjectileKind::Swing;
    blueprint.common.damage = 12.0f;
    blueprint.common.lifespanSeconds = 0.35f;
    blueprint.common.projectilesPerShot = 1;
    blueprint.common.randomSpreadDegrees = 0.0f;
    blueprint.common.debugColor = Color{240, 210, 180, 255};
    blueprint.common.spriteId = "longsword_swing";

    blueprint.swing.length = 110.0f;
    blueprint.swing.thickness = 28.0f;
    blueprint.swing.travelDegrees = 110.0f;
    blueprint.swing.followOwner = true;

    return blueprint;
}

WeaponBlueprint MakeLongswordWeaponBlueprint() {
    WeaponBlueprint blueprint{};
    blueprint.name = "Espada Longa";
    blueprint.projectile = MakeLongswordProjectileBlueprint();
    blueprint.cooldownSeconds = 0.45f;
    blueprint.holdToFire = false;
    return blueprint;
}

const WeaponBlueprint& GetLongswordWeaponBlueprint() {
    static WeaponBlueprint blueprint = MakeLongswordWeaponBlueprint();
    return blueprint;
}

ProjectileBlueprint MakeSpearProjectileBlueprint() {
    ProjectileBlueprint blueprint{};
    blueprint.kind = ProjectileKind::Spear;
    blueprint.common.damage = 10.0f;
    blueprint.common.lifespanSeconds = 0.6f;
    blueprint.common.projectilesPerShot = 1;
    blueprint.common.randomSpreadDegrees = 0.0f;
    blueprint.common.debugColor = Color{200, 220, 255, 255};
    blueprint.common.spriteId = "spear_thrust";

    blueprint.spear.extendDistance = 160.0f;
    blueprint.spear.shaftThickness = 16.0f;
    blueprint.spear.tipLength = 26.0f;
    blueprint.spear.extendDuration = 0.3f;
    blueprint.spear.retractDuration = 0.3f;
    blueprint.spear.followOwner = true;

    return blueprint;
}

WeaponBlueprint MakeSpearWeaponBlueprint() {
    WeaponBlueprint blueprint{};
    blueprint.name = "Investida Lanca";
    blueprint.projectile = MakeSpearProjectileBlueprint();
    blueprint.cooldownSeconds = 0.8f;
    blueprint.holdToFire = false;
    return blueprint;
}

const WeaponBlueprint& GetSpearWeaponBlueprint() {
    static WeaponBlueprint blueprint = MakeSpearWeaponBlueprint();
    return blueprint;
}

ProjectileBlueprint MakeFullCircleSwingProjectileBlueprint() {
    ProjectileBlueprint blueprint{};
    blueprint.kind = ProjectileKind::FullCircleSwing;
    blueprint.common.damage = 16.0f;
    blueprint.common.lifespanSeconds = 0.0f; // Derived from revolutions and speed
    blueprint.common.projectilesPerShot = 1;
    blueprint.common.randomSpreadDegrees = 0.0f;
    blueprint.common.debugColor = Color{255, 200, 140, 255};
    blueprint.common.spriteId = "full_circle_swing";

    blueprint.fullCircle.length = 120.0f;
    blueprint.fullCircle.thickness = 30.0f;
    blueprint.fullCircle.revolutions = 1.5f;
    blueprint.fullCircle.angularSpeedDegreesPerSecond = 420.0f;
    blueprint.fullCircle.followOwner = true;

    return blueprint;
}

WeaponBlueprint MakeFullCircleSwingAbilityBlueprint() {
    WeaponBlueprint blueprint{};
    blueprint.name = "Sword Cyclone";
    blueprint.projectile = MakeFullCircleSwingProjectileBlueprint();
    blueprint.cooldownSeconds = 3.0f;
    blueprint.holdToFire = false;
    return blueprint;
}

const WeaponBlueprint& GetFullCircleSwingAbilityBlueprint() {
    static WeaponBlueprint blueprint = MakeFullCircleSwingAbilityBlueprint();
    return blueprint;
}

ProjectileBlueprint MakeArcaneBowProjectileBlueprint() {
    ProjectileBlueprint blueprint{};
    blueprint.kind = ProjectileKind::Ammunition;
    blueprint.common.damage = 9.0f;
    blueprint.common.lifespanSeconds = 1.6f;
    blueprint.common.projectilesPerShot = 1;
    blueprint.common.randomSpreadDegrees = 4.0f;
    blueprint.common.debugColor = Color{255, 240, 180, 255};
    blueprint.common.spriteId = "arcane_bow_shot";
    blueprint.common.displayMode = WeaponDisplayMode::AimAligned;
    blueprint.common.displayOffset = Vector2{28.0f, -6.0f};
    blueprint.common.displayLength = 46.0f;
    blueprint.common.displayThickness = 8.0f;
    blueprint.common.displayColor = Color{210, 190, 140, 255};
    blueprint.common.displayHoldSeconds = 0.35f;

    blueprint.ammunition.speed = 560.0f;
    blueprint.ammunition.maxDistance = 860.0f;
    blueprint.ammunition.radius = 6.0f;
    blueprint.ammunition.muzzleOffset = 34.0f;

    return blueprint;
}

WeaponBlueprint MakeArcaneBowWeaponBlueprint() {
    WeaponBlueprint blueprint{};
    blueprint.name = "Arco Arcano";
    blueprint.projectile = MakeArcaneBowProjectileBlueprint();
    blueprint.cooldownSeconds = 0.35f;
    blueprint.holdToFire = false;
    return blueprint;
}

const WeaponBlueprint& GetArcaneBowWeaponBlueprint() {
    static WeaponBlueprint blueprint = MakeArcaneBowWeaponBlueprint();
    return blueprint;
}

ProjectileBlueprint MakePrismaticBeamProjectileBlueprint() {
    ProjectileBlueprint blueprint{};
    blueprint.kind = ProjectileKind::Laser;
    blueprint.common.damage = 4.0f;
    blueprint.common.lifespanSeconds = 0.3f;
    blueprint.common.projectilesPerShot = 1;
    blueprint.common.randomSpreadDegrees = 0.0f;
    blueprint.common.debugColor = Color{160, 240, 255, 235};
    blueprint.common.spriteId = "prismatic_beam";
    blueprint.common.displayMode = WeaponDisplayMode::AimAligned;
    blueprint.common.displayOffset = Vector2{30.0f, -4.0f};
    blueprint.common.displayLength = 52.0f;
    blueprint.common.displayThickness = 12.0f;
    blueprint.common.displayColor = Color{100, 200, 255, 220};
    blueprint.common.displayHoldSeconds = 0.5f;

    blueprint.laser.length = 540.0f;
    blueprint.laser.thickness = 12.0f;
    blueprint.laser.duration = 0.22f;

    return blueprint;
}

WeaponBlueprint MakePrismaticBeamWeaponBlueprint() {
    WeaponBlueprint blueprint{};
    blueprint.name = "Cajado Prismático";
    blueprint.projectile = MakePrismaticBeamProjectileBlueprint();
    blueprint.cooldownSeconds = 0.5f;
    blueprint.holdToFire = false;
    return blueprint;
}

const WeaponBlueprint& GetPrismaticBeamWeaponBlueprint() {
    static WeaponBlueprint blueprint = MakePrismaticBeamWeaponBlueprint();
    return blueprint;
}

} // namespace

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Prototype - Room Generation");
    SetTargetFPS(60);

    const std::uint64_t worldSeed = GenerateWorldSeed();
    RoomManager roomManager{worldSeed};
    RoomRenderer roomRenderer;
    ProjectileSystem projectileSystem;
    WeaponState leftHandWeapon;
    leftHandWeapon.blueprint = &GetBroquelWeaponBlueprint();
    WeaponState rightHandWeapon;
    rightHandWeapon.blueprint = &GetLongswordWeaponBlueprint();
    WeaponState spearWeapon;
    spearWeapon.blueprint = &GetSpearWeaponBlueprint();
    WeaponState swordAbility;
    swordAbility.blueprint = &GetFullCircleSwingAbilityBlueprint();
    WeaponState bowWeapon;
    bowWeapon.blueprint = &GetArcaneBowWeaponBlueprint();
    WeaponState prismaticBeamWeapon;
    prismaticBeamWeapon.blueprint = &GetPrismaticBeamWeaponBlueprint();

    roomManager.EnsureNeighborsGenerated(roomManager.GetCurrentCoords());

    Vector2 playerPosition = RoomCenter(roomManager.GetCurrentRoom().Layout());

    Camera2D camera{};
    camera.offset = Vector2{SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f};
    camera.target = playerPosition;
    camera.zoom = 1.0f;

    while (!WindowShouldClose()) {
        const float delta = GetFrameTime();

    leftHandWeapon.Update(delta);
    rightHandWeapon.Update(delta);
    spearWeapon.Update(delta);
    swordAbility.Update(delta);
    bowWeapon.Update(delta);
    prismaticBeamWeapon.Update(delta);

        Vector2 input{0.0f, 0.0f};
        if (IsKeyDown(KEY_W)) input.y -= 1.0f;
        if (IsKeyDown(KEY_S)) input.y += 1.0f;
        if (IsKeyDown(KEY_A)) input.x -= 1.0f;
        if (IsKeyDown(KEY_D)) input.x += 1.0f;

        Vector2 desiredPosition = playerPosition;
        if (Vector2LengthSqr(input) > 0.0f) {
            input = Vector2Normalize(input);
            desiredPosition = Vector2Add(desiredPosition, Vector2Scale(input, PLAYER_SPEED * delta));
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

        if (leftHandWeapon.CanFire() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            projectileSystem.SpawnProjectile(leftHandWeapon.blueprint->projectile, spawnContext);
            leftHandWeapon.ResetCooldown();
        }

        if (rightHandWeapon.CanFire() && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            projectileSystem.SpawnProjectile(rightHandWeapon.blueprint->projectile, spawnContext);
            rightHandWeapon.ResetCooldown();
        }

        if (spearWeapon.CanFire() && IsKeyPressed(KEY_E)) {
            projectileSystem.SpawnProjectile(spearWeapon.blueprint->projectile, spawnContext);
            spearWeapon.ResetCooldown();
        }

        if (swordAbility.CanFire() && IsKeyPressed(KEY_R)) {
            projectileSystem.SpawnProjectile(swordAbility.blueprint->projectile, spawnContext);
            swordAbility.ResetCooldown();
        }

        if (bowWeapon.CanFire() && IsKeyPressed(KEY_F)) {
            projectileSystem.SpawnProjectile(bowWeapon.blueprint->projectile, spawnContext);
            bowWeapon.ResetCooldown();
        }

        if (prismaticBeamWeapon.CanFire() && IsKeyPressed(KEY_G)) {
            projectileSystem.SpawnProjectile(prismaticBeamWeapon.blueprint->projectile, spawnContext);
            prismaticBeamWeapon.ResetCooldown();
        }

        projectileSystem.Update(delta);

        BeginDrawing();
        ClearBackground(Color{24, 26, 33, 255});

        BeginMode2D(camera);
        for (const auto& entry : roomManager.Rooms()) {
            const Room& room = *entry.second;
            bool isActive = (room.GetCoords() == roomManager.GetCurrentCoords());
            roomRenderer.DrawRoomBackground(room, isActive);
        }

        projectileSystem.Draw();

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

        EndDrawing();
    }

    CloseWindow();
    return 0;
}