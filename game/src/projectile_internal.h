#pragma once

#include "projectile.h"

#include <unordered_map>

struct ProjectileSystem::ProjectileInstance {
    virtual ~ProjectileInstance() = default;
    virtual void Update(float deltaSeconds) = 0;
    virtual void Draw() const = 0;
    virtual bool IsExpired() const = 0;
    virtual float ImpactFraction(const ProjectileTarget&) const { return 0.0f; }
    virtual bool ConsumesOnImpact() const { return false; }
    virtual void CollectHitEvents(const ProjectileTarget&,
                                  std::mt19937&,
                                  std::vector<ProjectileSystem::DamageEvent>&) {}
};

namespace projectile_detail {

struct PerTargetHitTracker {
    std::unordered_map<std::uintptr_t, float> lastHitSeconds{};

    bool CanHit(std::uintptr_t targetId, float currentTimeSeconds, float cooldownSeconds) const {
        if (cooldownSeconds <= 0.0f) {
            return true;
        }
        auto it = lastHitSeconds.find(targetId);
        return it == lastHitSeconds.end() ||
               (currentTimeSeconds - it->second) >= cooldownSeconds;
    }

    void RecordHit(std::uintptr_t targetId, float currentTimeSeconds) {
        lastHitSeconds[targetId] = currentTimeSeconds;
    }
};

struct WeaponDisplayState {
    Vector2 offset{0.0f, 0.0f};
    float angleDeg{0.0f};
};

float Clamp01(float value);
float DirectionToDegrees(Vector2 direction);
float DistancePointToSegment(Vector2 point, Vector2 segStart, Vector2 segEnd);
WeaponDisplayState ComputeWeaponDisplayState(const ProjectileCommonParams& common,
                                              const Vector2& aimDirection,
                                              float aimAngleDegrees);
void DrawWeaponDisplay(const ProjectileCommonParams& common,
                       const Vector2& basePosition,
                       float angleDegrees);
bool DrawWeaponSpriteFromPath(const std::string& spritePath,
                              const Vector2& basePosition,
                              float angleDegrees,
                              float desiredLength,
                              float desiredThickness,
                              Color tint);
bool DrawProjectileSpriteFromPath(const std::string& spritePath,
                                  const Vector2& center,
                                  float angleDegrees,
                                  float desiredLength,
                                  float desiredThickness,
                                  Color tint);
bool DrawBeamSpriteFromPath(const std::string& spritePath,
                            const Vector2& start,
                            const Vector2& end,
                            float desiredThickness,
                            Color tint);
void ReleaseSpriteCache();

std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeBluntProjectile(
    const ProjectileCommonParams& common, const BluntProjectileParams& params, Vector2 origin,
    const Vector2* followTarget, float startCenterDegrees, float endCenterDegrees);
std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeSwingProjectile(
    const ProjectileCommonParams& common, const SwingProjectileParams& params, Vector2 origin,
    const Vector2* followTarget, float startCenterDegrees, float endCenterDegrees);
std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeSpearProjectile(
    const ProjectileCommonParams& common, const SpearProjectileParams& params, Vector2 anchor,
    const Vector2* followTarget, Vector2 followOffset, Vector2 direction);
std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeFullCircleSwingProjectile(
    const ProjectileCommonParams& common, const FullCircleSwingParams& params, Vector2 origin,
    const Vector2* followTarget, float baseAngleDegrees);
std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeRangedWeaponDisplayProjectile(
    const ProjectileCommonParams& common, Vector2 weaponOrigin, const Vector2* followTarget,
    Vector2 weaponOffset, Vector2 direction);
std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeThrownAmmunitionProjectile(
    const ProjectileCommonParams& common, const AmmunitionProjectileParams& params,
    Vector2 position, Vector2 direction);
std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeThrownLaserProjectile(
    const ProjectileCommonParams& common, const LaserProjectileParams& params, Vector2 origin,
    const Vector2* followTarget, Vector2 followOffset, Vector2 direction, Vector2 startOffset);

} // namespace projectile_detail
