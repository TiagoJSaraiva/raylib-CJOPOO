/*******************************************************************************************
*
*   raylib [core] example - Basic window
*
*   Welcome to raylib!
*
*   To test examples, just press F6 and execute raylib_compile_execute script
*   Note that compiled executable is placed in the same folder as .c file
*
*   You can find all basic examples on C:\raylib\raylib\examples folder or
*   raylib official webpage: www.raylib.com
*
*   Enjoy using raylib. :)
*
*   This example has been created using raylib 1.0 (www.raylib.com)
*   raylib is licensed under an unmodified zlib/libpng license (View raylib.h for details)
*
*   Copyright (c) 2014 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include "raymath.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
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
constexpr float PLAYER_RADIUS = 18.0f;

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

bool IsPointInsideRectWithRadius(const Rectangle& rect, const Vector2& position, float radius, float tolerance = 0.0f) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return false;
    }
    const float minX = rect.x + radius - tolerance;
    const float maxX = rect.x + rect.width - radius + tolerance;
    const float minY = rect.y + radius - tolerance;
    const float maxY = rect.y + rect.height - radius + tolerance;
    return position.x >= minX && position.x <= maxX && position.y >= minY && position.y <= maxY;
}

Vector2 ClampPointToRect(const Rectangle& rect, const Vector2& position, float radius, float tolerance = 0.0f) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return position;
    }

    float minX = rect.x + radius - tolerance;
    float maxX = rect.x + rect.width - radius + tolerance;
    float minY = rect.y + radius - tolerance;
    float maxY = rect.y + rect.height - radius + tolerance;

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

void ClampPlayerToAccessibleArea(Vector2& position, float radius, const RoomLayout& layout) {
    constexpr float tolerance = PLAYER_RADIUS;

    Rectangle floor = TileRectToPixels(layout.tileBounds);

    struct AccessibleRegion {
        Rectangle rect;
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
            doorRegions.push_back(AccessibleRegion{doorway, door.direction, false});
        }

        Rectangle corridor = TileRectToPixels(door.corridorTiles);
        if (corridor.width > 0.0f && corridor.height > 0.0f) {
            doorRegions.push_back(AccessibleRegion{corridor, door.direction, true});
        }
    }

    if (IsPointInsideRectWithRadius(floor, position, radius, tolerance)) {
        return;
    }

    auto isInsideRegion = [&](const AccessibleRegion& region) {
        if (!region.isCorridor) {
            return IsPointInsideRectWithRadius(region.rect, position, radius, tolerance);
        }

        const Rectangle& rect = region.rect;
        if (rect.width <= 0.0f || rect.height <= 0.0f) {
            return false;
        }

        if (region.direction == Direction::North || region.direction == Direction::South) {
                float minX = rect.x + radius - tolerance;
                float maxX = rect.x + rect.width - radius + tolerance;
                float minY = rect.y - tolerance;
                float maxY = rect.y + rect.height + tolerance;
            return position.x >= minX && position.x <= maxX && position.y >= minY && position.y <= maxY;
        }

        float minY = rect.y + radius - tolerance;
        float maxY = rect.y + rect.height - radius + tolerance;
        float minX = rect.x - tolerance;
        float maxX = rect.x + rect.width + tolerance;
        return position.y >= minY && position.y <= maxY && position.x >= minX && position.x <= maxX;
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

    auto consider = [&](const AccessibleRegion& region) {
        const Rectangle& rect = region.rect;
        if (rect.width <= 0.0f || rect.height <= 0.0f) {
            return;
        }

        Vector2 candidate = ClampPointToRect(rect, position, radius, tolerance);
        float distSq = Vector2DistanceSqr(position, candidate);
        if (distSq < bestDistanceSq) {
            bestDistanceSq = distSq;
            bestPosition = candidate;
        }
    };

    for (const AccessibleRegion& region : doorRegions) {
        consider(region);
    }

    consider(AccessibleRegion{floor, Direction::North, false});

    position = bestPosition;
}

bool ShouldTransitionThroughDoor(const Doorway& door, const Vector2& position) {
    const Rectangle corridor = TileRectToPixels(door.corridorTiles);
    if (corridor.width > 0.0f && corridor.height > 0.0f) {
        constexpr float kLateralTolerance = 8.0f;
        constexpr float kDepthFactor = 0.45f; // require the player to step modestly into the corridor
        const float corridorLeft = corridor.x;
        const float corridorRight = corridor.x + corridor.width;
        const float corridorTop = corridor.y;
        const float corridorBottom = corridor.y + corridor.height;
        const float depthOffset = PLAYER_RADIUS * kDepthFactor;

        switch (door.direction) {
            case Direction::North:
                if (position.x < corridorLeft - kLateralTolerance || position.x > corridorRight + kLateralTolerance) {
                    return false;
                }
                return position.y <= (corridorBottom - depthOffset);
            case Direction::South:
                if (position.x < corridorLeft - kLateralTolerance || position.x > corridorRight + kLateralTolerance) {
                    return false;
                }
                return position.y >= (corridorTop + depthOffset);
            case Direction::East:
                if (position.y < corridorTop - kLateralTolerance || position.y > corridorBottom + kLateralTolerance) {
                    return false;
                }
                return position.x >= (corridorLeft + depthOffset);
            case Direction::West:
                if (position.y < corridorTop - kLateralTolerance || position.y > corridorBottom + kLateralTolerance) {
                    return false;
                }
                return position.x <= (corridorRight - depthOffset);
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
        ClampPlayerToAccessibleArea(desiredPosition, PLAYER_RADIUS, activeRoom.Layout());

        Room* currentRoomPtr = &activeRoom;
        bool movedRoom = false;

        for (auto& door : activeRoom.Layout().doors) {
            if (door.sealed) {
                continue;
            }

            Rectangle interact = DoorInteractionArea(activeRoom.Layout(), door);
            bool colliding = CheckCollisionCircleRec(desiredPosition, PLAYER_RADIUS, interact);
            bool movingToward = IsInputMovingToward(door.direction, input);
            bool shouldTransition = ShouldTransitionThroughDoor(door, desiredPosition);

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

        ClampPlayerToAccessibleArea(desiredPosition, PLAYER_RADIUS, currentRoomPtr->Layout());
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

        DrawCircleV(playerPosition, PLAYER_RADIUS, Color{120, 180, 220, 255});
        DrawCircleLines(static_cast<int>(playerPosition.x), static_cast<int>(playerPosition.y), PLAYER_RADIUS, Color{30, 60, 90, 255});

        EndMode2D();

        DrawText("WASD: mover | Aproximar-se de uma porta para trocar de sala", 20, 20, 20, RAYWHITE);
    DrawText(TextFormat("Salas geradas: %d", static_cast<int>(roomManager.Rooms().size())), 20, 50, 18, Color{200, 200, 220, 255});
    DrawText(TextFormat("Seed: %llu", static_cast<unsigned long long>(roomManager.GetWorldSeed())), 20, 80, 18, Color{200, 200, 220, 255});

        EndDrawing();
    }

    CloseWindow();
    return 0;
}