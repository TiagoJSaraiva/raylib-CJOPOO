#pragma once

#include <algorithm>
#include <string>

#include "player.h"
#include "projectile.h"

struct WeaponDamageParams {
    float baseDamage{0.0f};
    float attributeScaling{0.0f};
};

struct WeaponCadenceParams {
    float baseAttacksPerSecond{0.0f};
    float dexterityGainPerPoint{0.0f};
    float attacksPerSecondCap{0.0f};
};

struct WeaponCriticalParams {
    float baseChance{0.0f};
    float chancePerLetalidade{0.0f};
    float multiplier{1.0f};
};

struct WeaponBlueprint {
    std::string name{};
    ProjectileBlueprint projectile{};
    float cooldownSeconds{0.3f};
    bool holdToFire{false};
    WeaponAttributeKey attributeKey{WeaponAttributeKey::Strength};
    WeaponDamageParams damage{};
    WeaponCadenceParams cadence{};
    WeaponCriticalParams critical{};
    PlayerAttributes passiveBonuses{};
};

struct WeaponDerivedStats {
    float damagePerShot{0.0f};
    float attackIntervalSeconds{0.0f};
    float criticalChance{0.0f};
    float criticalMultiplier{1.0f};
};

struct WeaponState {
    const WeaponBlueprint* blueprint{nullptr};
    float cooldownTimer{0.0f};
    WeaponDerivedStats derived{};

    void Update(float deltaSeconds) {
        if (cooldownTimer > 0.0f) {
            cooldownTimer -= deltaSeconds;
            if (cooldownTimer < 0.0f) {
                cooldownTimer = 0.0f;
            }
        }
    }

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

    bool CanFire() const {
        return blueprint != nullptr && cooldownTimer <= 0.0f;
    }

    void ResetCooldown() {
        if (blueprint == nullptr) {
            return;
        }

        float interval = (derived.attackIntervalSeconds > 0.0f) ? derived.attackIntervalSeconds : blueprint->cooldownSeconds;
        cooldownTimer = std::max(0.0f, interval);
    }

    void ApplyDerivedToProjectile(ProjectileBlueprint& projectile) const {
        if (derived.damagePerShot > 0.0f) {
            projectile.common.damage = derived.damagePerShot;
        }
        projectile.common.criticalChance = derived.criticalChance;
        projectile.common.criticalMultiplier = derived.criticalMultiplier;
    }
};
