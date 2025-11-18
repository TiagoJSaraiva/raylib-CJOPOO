#pragma once

#include "enemy.h"

#include <string>

#include "weapon.h"

struct EnemySpriteInfo {
    std::string idleSpritePath{};
    std::string walkingSpriteSheetPath{};
    int frameWidth{64};
    int frameHeight{64};
    int frameCount{1};
    float secondsPerFrame{0.18f};
};

class EnemyCommon : public Enemy {
public:
    EnemyCommon(const EnemyConfig& config,
                float range,
                const WeaponBlueprint* weapon,
                const EnemySpriteInfo& spriteInfo);

    void Update(const EnemyUpdateContext& context) override;
    void Draw(const EnemyDrawContext& context) const override;

    static void ShutdownSpriteCache();

private:
    void EnsureTexturesLoaded() const;
    void UpdateAnimation(float deltaSeconds, bool isMoving);
    void AttemptAttack(const EnemyUpdateContext& context, const Vector2& toPlayer, float distanceToPlayer);

    const WeaponBlueprint* weapon_{nullptr};
    float range_{320.0f};
    EnemySpriteInfo spriteInfo_{};

    mutable Texture2D idleTexture_{};
    mutable Texture2D walkingTexture_{};
    mutable bool texturesLoaded_{false};

    float attackCooldown_{0.0f};
    float animationTimer_{0.0f};
    int currentFrame_{0};
    bool facingLeft_{false};
    bool isMoving_{false};
};
