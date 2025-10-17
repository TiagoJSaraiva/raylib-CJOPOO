#pragma once

#include <string>

#include "projectile.h"

struct WeaponBlueprint {
    std::string name{};
    ProjectileBlueprint projectile{};
    float cooldownSeconds{0.3f};
    bool holdToFire{false};
};

struct WeaponState {
    const WeaponBlueprint* blueprint{nullptr};
    float cooldownTimer{0.0f};

    void Update(float deltaSeconds) {
        if (cooldownTimer > 0.0f) {
            cooldownTimer -= deltaSeconds;
            if (cooldownTimer < 0.0f) {
                cooldownTimer = 0.0f;
            }
        }
    }

    bool CanFire() const {
        return blueprint != nullptr && cooldownTimer <= 0.0f;
    }

    void ResetCooldown() {
        if (blueprint != nullptr) {
            cooldownTimer = blueprint->cooldownSeconds;
        }
    }
};
