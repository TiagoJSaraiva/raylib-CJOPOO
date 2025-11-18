#include "enemy.h"

#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include "room.h"

namespace {

constexpr float kReturnArrivalThreshold = 4.0f;
constexpr float kMinSpawnRate = 0.01f;
constexpr float kMinSpeed = 20.0f;

Rectangle TileRectToPixels(const TileRect& rect) {
    return Rectangle{
        static_cast<float>(rect.x * TILE_SIZE),
        static_cast<float>(rect.y * TILE_SIZE),
        static_cast<float>(rect.width * TILE_SIZE),
        static_cast<float>(rect.height * TILE_SIZE)
    };
}

struct AccessibleRegion {
    Rectangle clampRect;
    Rectangle detectRect;
    Direction direction{Direction::North};
    bool isCorridor{false};
};

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

Vector2 ClampEntityToAccessibleArea(const RoomLayout& layout,
                                    const Vector2& position,
                                    float halfWidth,
                                    float halfHeight) {
    constexpr float tolerance = 0.0f;
    Rectangle floor = TileRectToPixels(layout.tileBounds);

    std::vector<AccessibleRegion> doorRegions;
    doorRegions.reserve(layout.doors.size() * 2);

    for (const auto& door : layout.doors) {
        if (door.sealed) {
            continue;
        }

        Rectangle doorway{};
        switch (door.direction) {
            case Direction::North:
                doorway = Rectangle{static_cast<float>((layout.tileBounds.x + door.offset) * TILE_SIZE),
                                    static_cast<float>(layout.tileBounds.y * TILE_SIZE),
                                    static_cast<float>(door.width * TILE_SIZE),
                                    static_cast<float>(TILE_SIZE)};
                break;
            case Direction::South:
                doorway = Rectangle{static_cast<float>((layout.tileBounds.x + door.offset) * TILE_SIZE),
                                    static_cast<float>((layout.tileBounds.y + layout.heightTiles - 1) * TILE_SIZE),
                                    static_cast<float>(door.width * TILE_SIZE),
                                    static_cast<float>(TILE_SIZE)};
                break;
            case Direction::East:
                doorway = Rectangle{static_cast<float>((layout.tileBounds.x + layout.widthTiles - 1) * TILE_SIZE),
                                    static_cast<float>((layout.tileBounds.y + door.offset) * TILE_SIZE),
                                    static_cast<float>(TILE_SIZE),
                                    static_cast<float>(door.width * TILE_SIZE)};
                break;
            case Direction::West:
                doorway = Rectangle{static_cast<float>(layout.tileBounds.x * TILE_SIZE),
                                    static_cast<float>((layout.tileBounds.y + door.offset) * TILE_SIZE),
                                    static_cast<float>(TILE_SIZE),
                                    static_cast<float>(door.width * TILE_SIZE)};
                break;
        }

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
        return position;
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
        return position;
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
            auto corridorCandidate = clampWithinCorridor(region);
            if (!corridorCandidate.has_value()) {
                return;
            }
            candidate = corridorCandidate.value();
        }

        float distanceSq = Vector2DistanceSqr(position, candidate);
        if (!foundCandidate || distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestPosition = candidate;
            foundCandidate = true;
        }
    };

    for (const AccessibleRegion& region : doorRegions) {
        consider(region);
    }

    if (!foundCandidate) {
        bestPosition = ClampBoxToRect(floor, position, halfWidth, halfHeight, tolerance);
    }

    return bestPosition;
}

} // namespace

Enemy::Enemy(const EnemyConfig& config)
        : name_(config.name),
            id_(config.id),
            biome_(config.biome),
            maxHealth_(std::max(1.0f, config.maxHealth)),
            currentHealth_(std::max(1.0f, config.maxHealth)),
            speed_(std::max(config.speed, kMinSpeed)),
            spawnRate_(std::max(config.spawnRate, kMinSpawnRate)),
            collisionRadius_(std::max(6.0f, config.collisionRadius)) {
        collisionHalfWidth_ = std::max(18.0f, collisionRadius_);
        collisionHalfHeight_ = std::max(16.0f, collisionRadius_ * 0.8f);
}

void Enemy::Initialize(Room& room, const Vector2& spawnPosition) {
    room_ = &room;
    roomCoords_ = room.GetCoords();
    position_ = spawnPosition;
    originalPosition_ = spawnPosition;
    HealToFull();
    ResetSpawnState();
}

void Enemy::ResetSpawnState() {
    active_ = false;
    fadeStarted_ = false;
    fadeCompleted_ = false;
    alpha_ = 0.0f;
    returningToOrigin_ = false;
    hasTakenDamage_ = false;
}

bool Enemy::TakeDamage(float amount) {
    if (!fadeCompleted_ || !IsAlive()) {
        return false;
    }
    if (amount <= 0.0f) {
        return false;
    }
    hasTakenDamage_ = true;
    currentHealth_ = std::max(0.0f, currentHealth_ - amount);
    return currentHealth_ <= 0.0f;
}

void Enemy::HealToFull() {
    currentHealth_ = maxHealth_;
}

Vector2 Enemy::ResolveRoomCollision(const RoomLayout& layout, const Vector2& desiredPosition) const {
    return ClampEntityToAccessibleArea(layout, desiredPosition, collisionHalfWidth_, collisionHalfHeight_);
}

bool Enemy::IsInsideRoomBounds(const RoomLayout& layout, const Vector2& position) const {
    Rectangle roomRect = TileRectToPixels(layout.tileBounds);
    float minX = roomRect.x + collisionHalfWidth_;
    float maxX = roomRect.x + roomRect.width - collisionHalfWidth_;
    float minY = roomRect.y + collisionHalfHeight_;
    float maxY = roomRect.y + roomRect.height - collisionHalfHeight_;
    return position.x >= minX && position.x <= maxX && position.y >= minY && position.y <= maxY;
}

void Enemy::BeginFadeIn() {
    fadeStarted_ = true;
}

void Enemy::ForceDeactivate() {
    active_ = false;
}

float Enemy::UpdateLifecycle(float deltaSeconds, bool playerInSameRoom) {
    if (playerInSameRoom && !fadeStarted_) {
        BeginFadeIn();
    }

    if (fadeStarted_ && !fadeCompleted_) {
        alpha_ += deltaSeconds / fadeDuration_;
        if (alpha_ >= 1.0f) {
            alpha_ = 1.0f;
            fadeCompleted_ = true;
        }
    }

    active_ = fadeCompleted_ && playerInSameRoom && !returningToOrigin_;
    if (!playerInSameRoom) {
        active_ = false;
    }

    return alpha_;
}

void Enemy::StartReturnToOrigin() {
    returningToOrigin_ = true;
}

void Enemy::MoveTowardsOriginal(float deltaSeconds, const RoomLayout& layout) {
    if (!returningToOrigin_) {
        return;
    }

    Vector2 desired = MoveTowards(originalPosition_, deltaSeconds, speed_);
    position_ = ResolveRoomCollision(layout, desired);

    if (Vector2Distance(position_, originalPosition_) <= kReturnArrivalThreshold) {
        position_ = originalPosition_;
        returningToOrigin_ = false;
    }
}

Vector2 Enemy::MoveTowards(const Vector2& target, float deltaSeconds, float speed) const {
    Vector2 delta = Vector2Subtract(target, position_);
    float distance = Vector2Length(delta);
    if (distance <= 1e-4f) {
        return position_;
    }
    Vector2 direction = Vector2Scale(delta, 1.0f / distance);
    float maxDistance = speed * deltaSeconds;
    if (maxDistance > distance) {
        return target;
    }
    return Vector2Add(position_, Vector2Scale(direction, maxDistance));
}
