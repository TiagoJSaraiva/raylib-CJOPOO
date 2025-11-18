#pragma once

#include "raylib.h"

#include <memory>
#include <string>

#include "room_types.h"

class Room;
class PlayerCharacter;
class ProjectileSystem;
struct RoomLayout;

struct EnemyConfig {
    int id{0};
    std::string name{};
    BiomeType biome{BiomeType::Unknown};
    float maxHealth{1.0f};
    float speed{120.0f};
    float spawnRate{1.0f};
    float collisionRadius{22.0f};
};

struct EnemyUpdateContext {
    float deltaSeconds{0.0f};
    const PlayerCharacter& player;
    Vector2 playerPosition{};
    const Room& room;
    bool playerInSameRoom{false};
    ProjectileSystem& projectileSystem;
};

struct EnemyDrawContext {
    float roomVisibility{1.0f};
    bool isActiveRoom{false};
};

class Enemy {
public:
    explicit Enemy(const EnemyConfig& config);
    virtual ~Enemy() = default;

    virtual void Update(const EnemyUpdateContext& context) = 0;
    virtual void Draw(const EnemyDrawContext& context) const = 0;

    void Initialize(Room& room, const Vector2& spawnPosition);
    void ResetSpawnState();

    const std::string& GetName() const { return name_; }
    int GetId() const { return id_; }
    BiomeType GetBiome() const { return biome_; }
    float GetSpawnRate() const { return spawnRate_; }

    const Vector2& GetPosition() const { return position_; }
    void SetPosition(const Vector2& position) { position_ = position; }
    const Vector2* GetPositionAddress() const { return &position_; }
    const Vector2& GetOriginalPosition() const { return originalPosition_; }
    void SetOriginalPosition(const Vector2& position) { originalPosition_ = position; }

    float GetCollisionRadius() const { return collisionRadius_; }
    float GetHalfWidth() const { return collisionHalfWidth_; }
    float GetHalfHeight() const { return collisionHalfHeight_; }

    float GetSpeed() const { return speed_; }
    float GetHealthFraction() const { return (maxHealth_ > 0.0f) ? (currentHealth_ / maxHealth_) : 0.0f; }
    bool HasTakenDamage() const { return hasTakenDamage_; }

    bool IsActive() const { return active_; }
    bool HasCompletedFade() const { return fadeCompleted_; }
    float GetAlpha() const { return alpha_; }

    bool IsAlive() const { return currentHealth_ > 0.0f; }
    float GetCurrentHealth() const { return currentHealth_; }
    float GetMaxHealth() const { return maxHealth_; }

    bool TakeDamage(float amount);
    void HealToFull();

protected:
    Vector2 ResolveRoomCollision(const RoomLayout& layout, const Vector2& desiredPosition) const;
    bool IsInsideRoomBounds(const RoomLayout& layout, const Vector2& position) const;

    void BeginFadeIn();
    void ForceDeactivate();
    float UpdateLifecycle(float deltaSeconds, bool playerInSameRoom);

    void StartReturnToOrigin();
    bool IsReturningToOrigin() const { return returningToOrigin_; }
    void MoveTowardsOriginal(float deltaSeconds, const RoomLayout& layout);

    Vector2 MoveTowards(const Vector2& target, float deltaSeconds, float speed) const;

    Room* GetRoom() const { return room_; }

    float collisionHalfWidth_{20.0f};
    float collisionHalfHeight_{18.0f};

private:
    std::string name_{};
    int id_{0};
    BiomeType biome_{BiomeType::Unknown};
    float maxHealth_{1.0f};
    float currentHealth_{1.0f};
    float speed_{120.0f};
    float spawnRate_{1.0f};

    Vector2 position_{};
    Vector2 originalPosition_{};

    Room* room_{nullptr};
    RoomCoords roomCoords_{};

    bool active_{false};
    bool fadeStarted_{false};
    bool fadeCompleted_{false};
    float alpha_{0.0f};
    float fadeDuration_{0.45f};

    bool returningToOrigin_{false};
    bool hasTakenDamage_{false};

    float collisionRadius_{22.0f};
};
