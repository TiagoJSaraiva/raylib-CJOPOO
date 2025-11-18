#pragma once

#include "enemy.h"

#include <string>

#include "weapon.h"

// Define sprites usados para idle/movimento do inimigo comum.
struct EnemySpriteInfo {
    std::string idleSpritePath{};
    std::string walkingSpriteSheetPath{};
    int frameWidth{64};
    int frameHeight{64};
    int frameCount{1};
    float secondsPerFrame{0.18f};
};

// Implementa comportamento padrão de inimigos (andar/atacar à distância).
class EnemyCommon : public Enemy {
public:
    EnemyCommon(const EnemyConfig& config,
                float range,
                const WeaponBlueprint* weapon,
                const EnemySpriteInfo& spriteInfo);

    void Update(const EnemyUpdateContext& context) override;
    void Draw(const EnemyDrawContext& context) const override;

    // Limpa cache de sprites compartilhados entre instâncias.
    static void ShutdownSpriteCache();

private:
    // Garante que texturas foram carregadas antes de desenhar.
    void EnsureTexturesLoaded() const;
    // Avança frames de animação conforme movimento.
    void UpdateAnimation(float deltaSeconds, bool isMoving);
    // Dispara projéteis caso o jogador esteja no alcance.
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
