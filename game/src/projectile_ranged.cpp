#include "projectile_internal.h"

#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <random>

using namespace projectile_detail;

namespace {
class RangedWeaponDisplayProjectile final : public ProjectileSystem::ProjectileInstance {
public:
    RangedWeaponDisplayProjectile(const ProjectileCommonParams& common,
                                  Vector2 weaponOrigin,
                                  const Vector2* followTarget,
                                  Vector2 weaponOffset,
                                  Vector2 direction)
        : common_(common),
          weaponOrigin_(weaponOrigin),
          followTarget_(followTarget),
          weaponOffset_(weaponOffset),
          direction_(Vector2Normalize(direction)) {
        if (Vector2LengthSqr(direction_) <= 1e-6f) {
            direction_ = Vector2{1.0f, 0.0f};
        }
        aimAngleDeg_ = DirectionToDegrees(direction_);
        displayState_ = ComputeWeaponDisplayState(common_, direction_, aimAngleDeg_);
        displayState_.angleDeg += common_.projectileRotationOffsetDegrees;

        holdDuration_ = std::max(common_.displayHoldSeconds, common_.lifespanSeconds);
        if (holdDuration_ <= 0.0f) {
            holdDuration_ = 0.35f;
        }
    }

    void Update(float deltaSeconds) override {
        if (followTarget_ != nullptr) {
            weaponOrigin_ = Vector2Add(*followTarget_, weaponOffset_);
        }

        elapsed_ += deltaSeconds;
        if (holdDuration_ > 0.0f && elapsed_ >= holdDuration_) {
            expired_ = true;
        }
    }

    void Draw() const override {
        if (expired_) {
            return;
        }

        Vector2 displayBase = Vector2Add(weaponOrigin_, displayState_.offset);
        if (!common_.weaponSpritePath.empty()) {
            bool drewSprite = DrawWeaponSpriteFromPath(common_.weaponSpritePath,
                                                       displayBase,
                                                       displayState_.angleDeg,
                                                       common_.displayLength,
                                                       common_.displayThickness,
                                                       WHITE);
            if (drewSprite) {
                return;
            }
        }

        if (common_.displayMode != WeaponDisplayMode::Hidden) {
            DrawWeaponDisplay(common_, displayBase, displayState_.angleDeg);
        }
    }

    void CollectHitEvents(const Vector2&,
                          float,
                          std::uintptr_t,
                          float,
                          std::mt19937&,
                          std::vector<ProjectileSystem::DamageEvent>&) override {
        // Display-only projectiles do not generate damage events.
    }

    bool IsExpired() const override {
        return expired_;
    }

private:
    ProjectileCommonParams common_{};
    Vector2 weaponOrigin_{};
    const Vector2* followTarget_{nullptr};
    Vector2 weaponOffset_{};
    Vector2 direction_{1.0f, 0.0f};
    float aimAngleDeg_{0.0f};
    WeaponDisplayState displayState_{};
    float elapsed_{0.0f};
    float holdDuration_{0.0f};
    bool expired_{false};
};

// Projétil físico viajando em linha reta (flechas/balas) com duração limitada.

class ThrownAmmunitionProjectile final : public ProjectileSystem::ProjectileInstance {
public:
    ThrownAmmunitionProjectile(const ProjectileCommonParams& common,
                               const AmmunitionProjectileParams& params,
                               Vector2 position,
                               Vector2 direction)
        : common_(common),
          params_(params),
          position_(position),
          direction_(Vector2Normalize(direction)) {
        if (Vector2LengthSqr(direction_) <= 1e-6f) {
            direction_ = Vector2{1.0f, 0.0f};
        }
        aimAngleDeg_ = DirectionToDegrees(direction_);
    }

    void Update(float deltaSeconds) override {
        position_ = Vector2Add(position_, Vector2Scale(direction_, params_.speed * deltaSeconds));
        traveled_ += params_.speed * deltaSeconds;
        elapsed_ += deltaSeconds;

        bool distanceExceeded = (params_.maxDistance > 0.0f) && (traveled_ >= params_.maxDistance);
        bool lifespanExceeded = (common_.lifespanSeconds > 0.0f) && (elapsed_ >= common_.lifespanSeconds);
        if (distanceExceeded || lifespanExceeded) {
            expired_ = true;
        }
    }

    void Draw() const override {
        if (expired_) {
            return;
        }

        float projectileLength = (common_.projectileSize > 0.0f) ? common_.projectileSize : params_.radius * 2.0f;
        float projectileThickness = params_.radius * 2.0f;
        bool drewProjectileSprite = DrawProjectileSpriteFromPath(common_.projectileSpritePath,
                                                                 position_,
                                                                 aimAngleDeg_ + common_.projectileRotationOffsetDegrees,
                                                                 projectileLength,
                                                                 projectileThickness,
                                                                 WHITE);
        if (!drewProjectileSprite) {
            DrawCircleV(position_, params_.radius, common_.debugColor);
        }
    }

    void CollectHitEvents(const Vector2& targetCenter,
                          float targetRadius,
                          std::uintptr_t targetId,
                          float targetImmunitySeconds,
                          std::mt19937& rng,
                          std::vector<ProjectileSystem::DamageEvent>& outEvents) override {
        (void)targetId;
        if (damageApplied_ || expired_ || common_.damage <= 0.0f) {
            return;
        }

        if (targetImmunitySeconds > 0.0f) {
            return;
        }

        float distance = Vector2Distance(position_, targetCenter);
        float effectiveRadius = params_.radius + targetRadius;

        if (distance > effectiveRadius) {
            return;
        }

        ProjectileSystem::DamageEvent event{};
        event.amount = common_.damage;
        event.suggestedImmunitySeconds = std::max(common_.perTargetHitCooldownSeconds, 0.0f);

        if (common_.criticalChance > 0.0f) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng) < common_.criticalChance) {
                event.isCritical = true;
                float multiplier = (common_.criticalMultiplier > 0.0f) ? common_.criticalMultiplier : 1.0f;
                event.amount *= multiplier;
            }
        }

        damageApplied_ = true;
        expired_ = true;
        outEvents.push_back(event);
    }

    bool IsExpired() const override {
        return expired_;
    }

private:
    ProjectileCommonParams common_{};
    AmmunitionProjectileParams params_{};
    Vector2 position_{};
    Vector2 direction_{1.0f, 0.0f};
    float aimAngleDeg_{0.0f};
    float traveled_{0.0f};
    float elapsed_{0.0f};
    bool expired_{false};
    bool damageApplied_{false};
};

// Representa feixes contínuos/lasers arremessados com duração e fade.

class ThrownLaserProjectile final : public ProjectileSystem::ProjectileInstance {
public:
    ThrownLaserProjectile(const ProjectileCommonParams& common,
                          const LaserProjectileParams& params,
                          Vector2 origin,
                          const Vector2* followTarget,
                          Vector2 followOffset,
                          Vector2 direction,
                          Vector2 startOffset)
        : common_(common),
          params_(params),
          origin_(origin),
          followTarget_(followTarget),
          followOffset_(followOffset),
          direction_(Vector2Normalize(direction)),
          startOffset_(startOffset) {
        if (Vector2LengthSqr(direction_) <= 1e-6f) {
            direction_ = Vector2{1.0f, 0.0f};
        }

        beamDuration_ = (params_.duration > 0.0f) ? params_.duration : common_.lifespanSeconds;
        if (beamDuration_ <= 0.0f && common_.lifespanSeconds > 0.0f) {
            beamDuration_ = common_.lifespanSeconds;
        }

        finalLifetime_ = beamDuration_;
        if (params_.staffHoldExtraSeconds > 0.0f) {
            finalLifetime_ = std::max(finalLifetime_, beamDuration_ + params_.staffHoldExtraSeconds);
        }
        if (common_.lifespanSeconds > 0.0f) {
            finalLifetime_ = std::max(finalLifetime_, common_.lifespanSeconds);
        }
    }

    void Update(float deltaSeconds) override {
        if (followTarget_ != nullptr) {
            origin_ = Vector2Add(*followTarget_, followOffset_);
        }

        elapsed_ += deltaSeconds;
        if (!beamExpired_ && beamDuration_ > 0.0f && elapsed_ >= beamDuration_) {
            beamExpired_ = true;
        }

        if (finalLifetime_ > 0.0f && elapsed_ >= finalLifetime_) {
            expired_ = true;
        }
    }

    void Draw() const override {
        if (expired_ || !IsBeamVisible()) {
            return;
        }

        Vector2 beamStart{};
        Vector2 beamEnd{};
        ComputeBeamSegment(beamStart, beamEnd);

        float beamAlpha = 1.0f;
        if (beamDuration_ > 0.0f && params_.fadeOutDuration > 0.0f) {
            float fadeStart = beamDuration_ - params_.fadeOutDuration;
            if (elapsed_ >= fadeStart) {
                float remaining = beamDuration_ - elapsed_;
                beamAlpha = Clamp01(remaining / std::max(params_.fadeOutDuration, 1e-3f));
            }
        }

        Color beamTint = ColorAlpha(WHITE, beamAlpha);
        bool drewBeamSprite = DrawBeamSpriteFromPath(common_.projectileSpritePath,
                                                     beamStart,
                                                     beamEnd,
                                                     params_.thickness,
                                                     beamTint);
        if (!drewBeamSprite) {
            Color lineColor = ColorAlpha(common_.debugColor, beamAlpha);
            DrawLineEx(beamStart, beamEnd, params_.thickness, lineColor);
        }
    }

    void CollectHitEvents(const Vector2& targetCenter,
                          float targetRadius,
                          std::uintptr_t targetId,
                          float targetImmunitySeconds,
                          std::mt19937& rng,
                          std::vector<ProjectileSystem::DamageEvent>& outEvents) override {
        if (expired_ || common_.damage <= 0.0f || !IsBeamVisible()) {
            return;
        }

        if (targetImmunitySeconds > 0.0f) {
            return;
        }

        Vector2 beamStart{};
        Vector2 beamEnd{};
        ComputeBeamSegment(beamStart, beamEnd);

        float distance = DistancePointToSegment(targetCenter, beamStart, beamEnd);
        float effectiveRadius = params_.thickness * 0.5f + targetRadius;

        if (distance > effectiveRadius) {
            return;
        }

        const float hitCooldown = std::max(common_.perTargetHitCooldownSeconds, 0.0f);
        if (!perTargetHits_.CanHit(targetId, elapsed_, hitCooldown)) {
            return;
        }

        ProjectileSystem::DamageEvent event{};
        event.amount = common_.damage;
        event.suggestedImmunitySeconds = hitCooldown;

        if (common_.criticalChance > 0.0f) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng) < common_.criticalChance) {
                event.isCritical = true;
                float multiplier = (common_.criticalMultiplier > 0.0f) ? common_.criticalMultiplier : 1.0f;
                event.amount *= multiplier;
            }
        }

        perTargetHits_.RecordHit(targetId, elapsed_);
        outEvents.push_back(event);
    }

    bool IsExpired() const override {
        return expired_;
    }

private:
    // Verdadeiro enquanto o feixe principal ainda está em exibição.
    bool IsBeamVisible() const {
        return (beamDuration_ <= 0.0f) || !beamExpired_;
    }

    // Calcula início/fim do feixe usando offsets configurados.
    void ComputeBeamSegment(Vector2& outStart, Vector2& outEnd) const {
        outStart = Vector2Add(origin_, startOffset_);
        outEnd = Vector2Add(outStart, Vector2Scale(direction_, params_.length));
    }

    ProjectileCommonParams common_{};
    LaserProjectileParams params_{};
    Vector2 origin_{};
    const Vector2* followTarget_{nullptr};
    Vector2 followOffset_{};
    Vector2 direction_{1.0f, 0.0f};
    Vector2 startOffset_{};
    float elapsed_{0.0f};
    float beamDuration_{0.0f};
    float finalLifetime_{0.0f};
    bool beamExpired_{false};
    bool expired_{false};
    PerTargetHitTracker perTargetHits_{};
};


} // namespace

namespace projectile_detail {
std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeRangedWeaponDisplayProjectile(
    const ProjectileCommonParams& common, Vector2 weaponOrigin, const Vector2* followTarget,
    Vector2 weaponOffset, Vector2 direction) {
    return std::make_unique<RangedWeaponDisplayProjectile>(common, weaponOrigin, followTarget,
                                                           weaponOffset, direction);
}

std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeThrownAmmunitionProjectile(
    const ProjectileCommonParams& common, const AmmunitionProjectileParams& params,
    Vector2 position, Vector2 direction) {
    return std::make_unique<ThrownAmmunitionProjectile>(common, params, position, direction);
}

std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeThrownLaserProjectile(
    const ProjectileCommonParams& common, const LaserProjectileParams& params, Vector2 origin,
    const Vector2* followTarget, Vector2 followOffset, Vector2 direction, Vector2 startOffset) {
    return std::make_unique<ThrownLaserProjectile>(common, params, origin, followTarget,
                                                   followOffset, direction, startOffset);
}

} // namespace projectile_detail