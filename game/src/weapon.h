#pragma once

#include <algorithm>
#include <string>

#include "player.h"
#include "projectile.h"

// Define dano base da arma e quanto escala com o atributo principal.
struct WeaponDamageParams {
    float baseDamage{0.0f};
    float attributeScaling{0.0f};
};

// Controla cadência: ataques por segundo base, ganho por Destreza e limite superior.
struct WeaponCadenceParams {
    float baseAttacksPerSecond{0.0f};
    float dexterityGainPerPoint{0.0f};
    float attacksPerSecondCap{0.0f};
};

// Valores padrão de chance/ganho de crítico e multiplicador aplicado ao dano.
struct WeaponCriticalParams {
    float baseChance{0.0f};
    float chancePerLetalidade{0.0f};
    float multiplier{1.0f};
};

// Configuração do sprite mostrado na UI de inventário para a arma.
struct WeaponInventorySprite {
    std::string spritePath{};
    Vector2 drawSize{56.0f, 56.0f};
    Vector2 drawOffset{0.0f, 0.0f};
    float rotationDegrees{0.0f};
};

// Blueprint completo utilizado tanto pela UI quanto pelo sistema de tiro.
struct WeaponBlueprint {
    std::string name{};
    ProjectileBlueprint projectile{};
    float cooldownSeconds{0.3f};
    bool holdToFire{false};
    bool usesSeparateProjectileSprite{false};
    WeaponAttributeKey attributeKey{WeaponAttributeKey::Strength};
    WeaponDamageParams damage{};
    WeaponCadenceParams cadence{};
    WeaponCriticalParams critical{};
    PlayerAttributes passiveBonuses{};
    WeaponInventorySprite inventorySprite{};
};

// Estatísticas calculadas na hora com base nos atributos do jogador.
struct WeaponDerivedStats {
    float damagePerShot{0.0f};
    float attackIntervalSeconds{0.0f};
    float criticalChance{0.0f};
    float criticalMultiplier{1.0f};
};

// Estado runtime de uma arma empunhada (cooldown, stats aplicados, etc.).
struct WeaponState {
    const WeaponBlueprint* blueprint{nullptr};
    float cooldownTimer{0.0f};
    WeaponDerivedStats derived{};

    // Atualiza temporizador de cooldown a cada quadro.
    void Update(float deltaSeconds) {
        if (cooldownTimer > 0.0f) {
            cooldownTimer -= deltaSeconds;
            if (cooldownTimer < 0.0f) {
                cooldownTimer = 0.0f;
            }
        }
    }

    // Recalcula dano/cadência/crítico considerando atributos atuais do jogador.
    void RecalculateDerivedStats(const PlayerCharacter& player) {
        derived = WeaponDerivedStats{};

        if (blueprint == nullptr) {
            return;
        }

        derived.attackIntervalSeconds = blueprint->cooldownSeconds;
        derived.criticalMultiplier = (blueprint->critical.multiplier > 0.0f) ? blueprint->critical.multiplier : 1.0f;

        if (blueprint->damage.baseDamage > 0.0f || blueprint->damage.attributeScaling != 0.0f) {
            int attributeValue = player.GetAttackAttributeValue(blueprint->attributeKey);
            derived.damagePerShot = blueprint->damage.baseDamage + blueprint->damage.attributeScaling * static_cast<float>(attributeValue);
        }

        if (blueprint->cadence.baseAttacksPerSecond > 0.0f) {
            float aps = blueprint->cadence.baseAttacksPerSecond +
                        blueprint->cadence.dexterityGainPerPoint * static_cast<float>(player.totalAttributes.primary.destreza);
            if (blueprint->cadence.attacksPerSecondCap > 0.0f) {
                aps = std::min(aps, blueprint->cadence.attacksPerSecondCap);
            }
            if (aps > 0.0f) {
                derived.attackIntervalSeconds = 1.0f / aps;
            }
        }

        if (blueprint->critical.baseChance > 0.0f || blueprint->critical.chancePerLetalidade > 0.0f) {
            float chance = blueprint->critical.baseChance +
                           blueprint->critical.chancePerLetalidade * player.totalAttributes.secondary.letalidade;
            derived.criticalChance = std::clamp(chance, 0.0f, 0.75f);
        }
    }

    // Informa se a arma pode disparar novamente (cooldown zerado).
    bool CanFire() const {
        return blueprint != nullptr && cooldownTimer <= 0.0f;
    }

    // Reinicia cooldown baseado nas stats derivadas e retorna intervalo aplicado.
    float ResetCooldown() {
        if (blueprint == nullptr) {
            return 0.0f;
        }

        float interval = (derived.attackIntervalSeconds > 0.0f)
            ? derived.attackIntervalSeconds
            : blueprint->cooldownSeconds;
        interval = std::max(0.0f, interval);
        cooldownTimer = interval;
        return interval;
    }

    // Garante que o cooldown mínimo atual seja pelo menos o valor informado.
    void EnforceMinimumCooldown(float seconds) {
        if (blueprint == nullptr || seconds <= 0.0f) {
            return;
        }

        if (cooldownTimer < seconds) {
            cooldownTimer = seconds;
        }
    }

    // Propaga valores calculados (dano/crítico) para o projétil antes de spawnar.
    void ApplyDerivedToProjectile(ProjectileBlueprint& projectile) const {
        if (derived.damagePerShot > 0.0f) {
            projectile.common.damage = derived.damagePerShot;
        }
        projectile.common.criticalChance = derived.criticalChance;
        projectile.common.criticalMultiplier = derived.criticalMultiplier;

        for (auto& thrown : projectile.thrownProjectiles) {
            if (derived.damagePerShot > 0.0f) {
                thrown.common.damage = derived.damagePerShot;
            }
            thrown.common.criticalChance = derived.criticalChance;
            thrown.common.criticalMultiplier = derived.criticalMultiplier;
        }
    }
};
