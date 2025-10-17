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

} // namespace

int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Prototype - Room Generation");
    SetTargetFPS(60);

    const std::uint64_t worldSeed = GenerateWorldSeed();
    RoomManager roomManager{worldSeed};
    RoomRenderer roomRenderer;

    roomManager.EnsureNeighborsGenerated(roomManager.GetCurrentCoords());

    Vector2 playerPosition = RoomCenter(roomManager.GetCurrentRoom().Layout());

    Camera2D camera{};
    camera.offset = Vector2{SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f};
    camera.target = playerPosition;
    camera.zoom = 1.0f;

    while (!WindowShouldClose()) {
        const float delta = GetFrameTime();

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

        BeginDrawing();
        ClearBackground(Color{24, 26, 33, 255});

        BeginMode2D(camera);
        for (const auto& entry : roomManager.Rooms()) {
            const Room& room = *entry.second;
            bool isActive = (room.GetCoords() == roomManager.GetCurrentCoords());
            roomRenderer.DrawRoom(room, isActive);
        }

        Rectangle renderRect{
            playerPosition.x - PLAYER_RENDER_HALF_SIZE,
            playerPosition.y - PLAYER_RENDER_HALF_SIZE,
            PLAYER_RENDER_HALF_SIZE * 2.0f,
            PLAYER_RENDER_HALF_SIZE * 2.0f
        };
        DrawRectangleRec(renderRect, Color{120, 180, 220, 255});
        DrawRectangleLinesEx(renderRect, 2.0f, Color{30, 60, 90, 255});

        EndMode2D();

        EndDrawing();
    }

    CloseWindow();
    return 0;
}