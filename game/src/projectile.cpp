#include "projectile.h"

#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {

constexpr float kRadToDeg = 180.0f / PI;

struct LaserAssets {
    Texture2D body{};
    Texture2D staff{};
    bool attemptedLoad{false};
};

LaserAssets g_laserAssets{};

bool IsTextureLoaded(const Texture2D& texture) {
    return texture.id != 0;
}

Texture2D LoadTextureIfAvailable(const char* path) {
    if (FileExists(path)) {
        Texture2D texture = LoadTexture(path);
        if (texture.id != 0) {
            SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
            return texture;
        }
    }
    return Texture2D{};
}

void EnsureLaserAssetsLoaded() {
    if (g_laserAssets.attemptedLoad) {
        return;
    }

    g_laserAssets.attemptedLoad = true;
    g_laserAssets.body = LoadTextureIfAvailable("assets/img/projectiles/laser_body.png");
    g_laserAssets.staff = LoadTextureIfAvailable("assets/img/weapons/cajado_de_carvalho.png");
}

void UnloadLaserAssets() {
    if (!g_laserAssets.attemptedLoad) {
        return;
    }

    auto unload = [](Texture2D& texture) {
        if (texture.id != 0) {
            UnloadTexture(texture);
            texture = Texture2D{};
        }
    };

    unload(g_laserAssets.body);
    unload(g_laserAssets.staff);
    g_laserAssets.attemptedLoad = false;
}

float Clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

float DirectionToDegrees(Vector2 dir) {
    const float lengthSq = dir.x * dir.x + dir.y * dir.y;
    if (lengthSq <= 1e-5f) {
        return 0.0f;
    }
    return std::atan2(dir.y, dir.x) * kRadToDeg;
}

float NormalizeAngleDeg(float angleDegrees) {
    while (angleDegrees > 180.0f) {
        angleDegrees -= 360.0f;
    }
    while (angleDegrees < -180.0f) {
        angleDegrees += 360.0f;
    }
    return angleDegrees;
}

float DistancePointToSegment(Vector2 point, Vector2 segStart, Vector2 segEnd) {
    Vector2 segment = Vector2Subtract(segEnd, segStart);
    float segmentLengthSq = Vector2LengthSqr(segment);
    if (segmentLengthSq <= 1e-5f) {
        return Vector2Distance(point, segStart);
    }

    float t = Vector2DotProduct(Vector2Subtract(point, segStart), segment) / segmentLengthSq;
    t = Clamp01(t);
    Vector2 closest = Vector2Add(segStart, Vector2Scale(segment, t));
    return Vector2Distance(point, closest);
}

struct WeaponDisplayState {
    Vector2 offset{0.0f, 0.0f};
    float angleDeg{0.0f};
};

WeaponDisplayState ComputeWeaponDisplayState(const ProjectileCommonParams& common, const Vector2& aimDir, float aimAngleDeg) {
    WeaponDisplayState state{};
    switch (common.displayMode) {
        case WeaponDisplayMode::Hidden:
            state.offset = Vector2{0.0f, 0.0f};
            state.angleDeg = 0.0f;
            break;
        case WeaponDisplayMode::Fixed:
            state.offset = common.displayOffset;
            if (aimDir.x < 0.0f) {
                state.offset.x = -state.offset.x;
            }
            state.angleDeg = (aimDir.x < 0.0f) ? 180.0f : 0.0f;
            break;
        case WeaponDisplayMode::AimAligned:
            state.offset = common.displayOffset;
            if (aimDir.x < 0.0f) {
                state.offset.x = -state.offset.x;
            }
            state.angleDeg = aimAngleDeg;
            break;
    }

    return state;
}

void DrawWeaponDisplay(const ProjectileCommonParams& common, const Vector2& basePosition, float angleDegrees) {
    if (common.displayMode == WeaponDisplayMode::Hidden) {
        return;
    }

    Rectangle rect{};
    rect.x = basePosition.x;
    rect.y = basePosition.y - common.displayThickness * 0.5f;
    rect.width = common.displayLength;
    rect.height = common.displayThickness;

    Vector2 pivot{0.0f, common.displayThickness * 0.5f};
    float drawAngle = angleDegrees;
    if (common.displayMode == WeaponDisplayMode::Fixed) {
        drawAngle = angleDegrees;
    }

    DrawRectanglePro(rect, pivot, drawAngle, common.displayColor);
}

} // namespace

struct ProjectileSystem::ProjectileInstance {
    virtual ~ProjectileInstance() = default;
    virtual void Update(float deltaSeconds) = 0;
    virtual void Draw() const = 0;
    virtual bool IsExpired() const = 0;
    virtual void CollectHitEvents(const Vector2&, float, std::mt19937&, std::vector<ProjectileSystem::DamageEvent>&) {}
};

namespace {

class BluntProjectile final : public ProjectileSystem::ProjectileInstance {
public:
    BluntProjectile(const ProjectileCommonParams& common,
                    const BluntProjectileParams& params,
                    Vector2 origin,
                    const Vector2* followTarget,
                    float startCenterDegrees,
                    float endCenterDegrees)
        : common_(common),
          params_(params),
          origin_(origin),
          followTarget_(followTarget),
          startCenterDegrees_(startCenterDegrees),
          endCenterDegrees_(endCenterDegrees) {}

    void Update(float deltaSeconds) override {
        if (followTarget_ != nullptr) {
            origin_ = *followTarget_;
        }

        elapsed_ += deltaSeconds;
        if (elapsed_ >= common_.lifespanSeconds && common_.lifespanSeconds > 0.0f) {
            expired_ = true;
        }
    }

    void Draw() const override {
        if (expired_) {
            return;
        }

        const float duration = (common_.lifespanSeconds <= 0.0f) ? 1.0f : common_.lifespanSeconds;
        const float t = Clamp01(elapsed_ / duration);
        const float centerAngle = startCenterDegrees_ + (endCenterDegrees_ - startCenterDegrees_) * t;
        const float halfSpan = params_.arcSpanDegrees * 0.5f;
        const float startArc = centerAngle - halfSpan;
        const float endArc = centerAngle + halfSpan;
        const float innerRadius = std::max(0.0f, params_.radius - params_.thickness * 0.5f);
        const float outerRadius = params_.radius + params_.thickness * 0.5f;

        DrawRing(origin_, innerRadius, outerRadius, startArc, endArc, 36, common_.debugColor);
    }

    void CollectHitEvents(const Vector2& targetCenter,
                           float targetRadius,
                           std::mt19937& rng,
                           std::vector<ProjectileSystem::DamageEvent>& outEvents) override {
        if (damageApplied_ || common_.damage <= 0.0f) {
            return;
        }

        const float duration = (common_.lifespanSeconds <= 0.0f) ? 1.0f : common_.lifespanSeconds;
        const float t = Clamp01(elapsed_ / duration);
        const float centerAngle = startCenterDegrees_ + (endCenterDegrees_ - startCenterDegrees_) * t;
        const float halfSpan = params_.arcSpanDegrees * 0.5f;

        const float innerRadius = std::max(0.0f, params_.radius - params_.thickness * 0.5f);
        const float outerRadius = params_.radius + params_.thickness * 0.5f;

        Vector2 toTarget = Vector2Subtract(targetCenter, origin_);
        float distance = Vector2Length(toTarget);
        float angleToTarget = DirectionToDegrees(toTarget);
        float angleDiff = NormalizeAngleDeg(angleToTarget - centerAngle);

        constexpr float kAnglePadding = 6.0f;
        constexpr float kRadiusPadding = 6.0f;

        if (std::abs(angleDiff) > halfSpan + kAnglePadding) {
            return;
        }

        if (distance < innerRadius - targetRadius - kRadiusPadding || distance > outerRadius + targetRadius + kRadiusPadding) {
            return;
        }

        ProjectileSystem::DamageEvent event{};
        event.amount = common_.damage;

        if (common_.criticalChance > 0.0f) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng) < common_.criticalChance) {
                event.isCritical = true;
                float multiplier = (common_.criticalMultiplier > 0.0f) ? common_.criticalMultiplier : 1.0f;
                event.amount *= multiplier;
            }
        }

        damageApplied_ = true;
        outEvents.push_back(event);
    }

    bool IsExpired() const override {
        return expired_;
    }

private:
    ProjectileCommonParams common_{};
    BluntProjectileParams params_{};
    Vector2 origin_{};
    const Vector2* followTarget_{nullptr};
    float startCenterDegrees_{0.0f};
    float endCenterDegrees_{0.0f};
    float elapsed_{0.0f};
    bool expired_{false};
    bool damageApplied_{false};
};

class SwingProjectile final : public ProjectileSystem::ProjectileInstance {
public:
    SwingProjectile(const ProjectileCommonParams& common,
                    const SwingProjectileParams& params,
                    Vector2 origin,
                    const Vector2* followTarget,
                    float startCenterDegrees,
                    float endCenterDegrees)
        : common_(common),
          params_(params),
          origin_(origin),
          followTarget_(followTarget),
          startCenterDegrees_(startCenterDegrees),
          endCenterDegrees_(endCenterDegrees) {}

    void Update(float deltaSeconds) override {
        if (followTarget_ != nullptr) {
            origin_ = *followTarget_;
        }

        elapsed_ += deltaSeconds;
        if (common_.lifespanSeconds > 0.0f && elapsed_ >= common_.lifespanSeconds) {
            expired_ = true;
        }
    }

    void Draw() const override {
        if (expired_) {
            return;
        }

        const float duration = (common_.lifespanSeconds <= 0.0f) ? 1.0f : common_.lifespanSeconds;
        const float t = Clamp01(elapsed_ / duration);
        const float centerAngle = startCenterDegrees_ + (endCenterDegrees_ - startCenterDegrees_) * t;

        Rectangle rect{};
        rect.x = origin_.x;
        rect.y = origin_.y - params_.thickness * 0.5f;
        rect.width = params_.length;
        rect.height = params_.thickness;

        Vector2 pivot{0.0f, params_.thickness * 0.5f};
        DrawRectanglePro(rect, pivot, centerAngle, common_.debugColor);
    }

    void CollectHitEvents(const Vector2& targetCenter,
                           float targetRadius,
                           std::mt19937& rng,
                           std::vector<ProjectileSystem::DamageEvent>& outEvents) override {
        if (damageApplied_ || common_.damage <= 0.0f || params_.length <= 0.0f) {
            return;
        }

        const float duration = (common_.lifespanSeconds <= 0.0f) ? 1.0f : common_.lifespanSeconds;
        const float t = Clamp01(elapsed_ / duration);
        const float centerAngle = startCenterDegrees_ + (endCenterDegrees_ - startCenterDegrees_) * t;
        const float angleRad = centerAngle * DEG2RAD;
        Vector2 direction{std::cos(angleRad), std::sin(angleRad)};
        Vector2 endPoint = Vector2Add(origin_, Vector2Scale(direction, params_.length));

        float distance = DistancePointToSegment(targetCenter, origin_, endPoint);
        float effectiveRadius = params_.thickness * 0.5f + targetRadius;

        if (distance > effectiveRadius) {
            return;
        }

        ProjectileSystem::DamageEvent event{};
        event.amount = common_.damage;

        if (common_.criticalChance > 0.0f) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng) < common_.criticalChance) {
                event.isCritical = true;
                float multiplier = (common_.criticalMultiplier > 0.0f) ? common_.criticalMultiplier : 1.0f;
                event.amount *= multiplier;
            }
        }

        damageApplied_ = true;
        outEvents.push_back(event);
    }

    bool IsExpired() const override {
        return expired_;
    }

private:
    ProjectileCommonParams common_{};
    SwingProjectileParams params_{};
    Vector2 origin_{};
    const Vector2* followTarget_{nullptr};
    float startCenterDegrees_{0.0f};
    float endCenterDegrees_{0.0f};
    float elapsed_{0.0f};
    bool expired_{false};
    bool damageApplied_{false};
};

class SpearProjectile final : public ProjectileSystem::ProjectileInstance {
public:
    SpearProjectile(const ProjectileCommonParams& common,
                    const SpearProjectileParams& params,
                    Vector2 origin,
                    const Vector2* followTarget,
                    Vector2 direction)
        : common_(common),
          params_(params),
          origin_(origin),
          followTarget_(followTarget),
          direction_(Vector2Normalize(direction))
    {
        if (Vector2LengthSqr(direction_) <= 1e-5f) {
            direction_ = Vector2{1.0f, 0.0f};
        }
    }

    void Update(float deltaSeconds) override {
        if (followTarget_ != nullptr) {
            origin_ = *followTarget_;
        }

        elapsed_ += deltaSeconds;

        const float extendDuration = params_.extendDuration;
        float retractDuration = params_.retractDuration;
        if (retractDuration <= 0.0f) {
            retractDuration = extendDuration;
        }

        const float totalDuration = extendDuration + retractDuration;
        if (totalDuration <= 0.0f) {
            expired_ = true;
            currentDistance_ = 0.0f;
            return;
        }

        if (elapsed_ >= totalDuration) {
            elapsed_ = totalDuration;
            expired_ = true;
        }

        if (elapsed_ <= extendDuration) {
            const float t = (extendDuration > 0.0f) ? Clamp01(elapsed_ / extendDuration) : 1.0f;
            currentDistance_ = params_.extendDistance * t;
        } else {
            const float returnTime = elapsed_ - extendDuration;
            const float t = (retractDuration > 0.0f) ? Clamp01(returnTime / retractDuration) : 1.0f;
            currentDistance_ = params_.extendDistance * (1.0f - t);
        }
    }

    void Draw() const override {
        if (currentDistance_ <= 1e-3f) {
            return;
        }

        const Vector2 base = origin_;
        const Vector2 dir = direction_;
        Vector2 side = Vector2{-dir.y, dir.x};
        side = Vector2Normalize(side);
        side = Vector2Scale(side, params_.shaftThickness * 0.5f);

        const float shaftLength = std::max(0.0f, currentDistance_ - params_.tipLength);
        const Vector2 shaftEnd = Vector2Add(base, Vector2Scale(dir, shaftLength));
        const Vector2 tip = Vector2Add(base, Vector2Scale(dir, currentDistance_));

        const Vector2 baseLeft = Vector2Subtract(base, side);
        const Vector2 baseRight = Vector2Add(base, side);
        const Vector2 shaftLeft = Vector2Subtract(shaftEnd, side);
        const Vector2 shaftRight = Vector2Add(shaftEnd, side);

        DrawTriangle(baseLeft, baseRight, shaftRight, common_.debugColor);
        DrawTriangle(baseLeft, shaftLeft, shaftRight, common_.debugColor);
        DrawTriangle(shaftLeft, shaftRight, tip, common_.debugColor);
    }

    void CollectHitEvents(const Vector2& targetCenter,
                          float targetRadius,
                          std::mt19937& rng,
                          std::vector<ProjectileSystem::DamageEvent>& outEvents) override {
        if (damageApplied_ || expired_ || common_.damage <= 0.0f || currentDistance_ <= 1e-3f) {
            return;
        }

        const Vector2 base = origin_;
        const Vector2 tip = Vector2Add(origin_, Vector2Scale(direction_, currentDistance_));
        float distance = DistancePointToSegment(targetCenter, base, tip);
        float effectiveRadius = (std::max(params_.shaftThickness * 0.5f, params_.tipLength * 0.5f)) + targetRadius;

        if (distance > effectiveRadius) {
            return;
        }

        ProjectileSystem::DamageEvent event{};
        event.amount = common_.damage;

        if (common_.criticalChance > 0.0f) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng) < common_.criticalChance) {
                event.isCritical = true;
                float multiplier = (common_.criticalMultiplier > 0.0f) ? common_.criticalMultiplier : 1.0f;
                event.amount *= multiplier;
            }
        }

        damageApplied_ = true;
        outEvents.push_back(event);
    }

    bool IsExpired() const override {
        return expired_;
    }

private:
    ProjectileCommonParams common_{};
    SpearProjectileParams params_{};
    Vector2 origin_{};
    const Vector2* followTarget_{nullptr};
    Vector2 direction_{1.0f, 0.0f};
    float elapsed_{0.0f};
    float currentDistance_{0.0f};
    bool expired_{false};
    bool damageApplied_{false};
};

class FullCircleSwingProjectile final : public ProjectileSystem::ProjectileInstance {
public:
    FullCircleSwingProjectile(const ProjectileCommonParams& common,
                              const FullCircleSwingParams& params,
                              Vector2 origin,
                              const Vector2* followTarget,
                              float initialAngleDegrees)
        : common_(common),
          params_(params),
          origin_(origin),
          followTarget_(followTarget),
          currentAngleDeg_(initialAngleDegrees) {}

    void Update(float deltaSeconds) override {
        if (followTarget_ != nullptr && params_.followOwner) {
            origin_ = *followTarget_;
        }

        elapsed_ += deltaSeconds;

        const float rotationStep = params_.angularSpeedDegreesPerSecond * deltaSeconds;
        currentAngleDeg_ += rotationStep;
        totalRotationDeg_ += std::abs(rotationStep);

        const float targetRotation = std::abs(params_.revolutions) * 360.0f;
        if (targetRotation > 0.0f && totalRotationDeg_ >= targetRotation) {
            expired_ = true;
        }

        if (common_.lifespanSeconds > 0.0f && elapsed_ >= common_.lifespanSeconds) {
            expired_ = true;
        }

        if (params_.angularSpeedDegreesPerSecond == 0.0f && targetRotation > 0.0f) {
            // Prevent infinite lifetime if speed is zero but revolutions requested.
            expired_ = true;
        }
    }

    void Draw() const override {
        if (expired_) {
            return;
        }

        Rectangle rect{};
        rect.x = origin_.x;
        rect.y = origin_.y - params_.thickness * 0.5f;
        rect.width = params_.length;
        rect.height = params_.thickness;

        Vector2 pivot{0.0f, params_.thickness * 0.5f};
        DrawRectanglePro(rect, pivot, currentAngleDeg_, common_.debugColor);
    }

    void CollectHitEvents(const Vector2& targetCenter,
                          float targetRadius,
                          std::mt19937& rng,
                          std::vector<ProjectileSystem::DamageEvent>& outEvents) override {
        if (damageApplied_ || expired_ || common_.damage <= 0.0f || params_.length <= 1e-3f) {
            return;
        }

        const float angleRad = currentAngleDeg_ * DEG2RAD;
        Vector2 direction{std::cos(angleRad), std::sin(angleRad)};
        Vector2 start = origin_;
        Vector2 end = Vector2Add(start, Vector2Scale(direction, params_.length));

        float distance = DistancePointToSegment(targetCenter, start, end);
        float effectiveRadius = params_.thickness * 0.5f + targetRadius;

        if (distance > effectiveRadius) {
            return;
        }

        ProjectileSystem::DamageEvent event{};
        event.amount = common_.damage;

        if (common_.criticalChance > 0.0f) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng) < common_.criticalChance) {
                event.isCritical = true;
                float multiplier = (common_.criticalMultiplier > 0.0f) ? common_.criticalMultiplier : 1.0f;
                event.amount *= multiplier;
            }
        }

        damageApplied_ = true;
        outEvents.push_back(event);
    }

    bool IsExpired() const override {
        return expired_;
    }

private:
    ProjectileCommonParams common_{};
    FullCircleSwingParams params_{};
    Vector2 origin_{};
    const Vector2* followTarget_{nullptr};
    float currentAngleDeg_{0.0f};
    float totalRotationDeg_{0.0f};
    float elapsed_{0.0f};
    bool expired_{false};
    bool damageApplied_{false};
};

class AmmunitionProjectile final : public ProjectileSystem::ProjectileInstance {
public:
    AmmunitionProjectile(const ProjectileCommonParams& common,
                         const AmmunitionProjectileParams& params,
                         Vector2 weaponOrigin,
                         const Vector2* followTarget,
                         Vector2 weaponOffset,
                         Vector2 direction)
        : common_(common),
          params_(params),
          weaponOrigin_(weaponOrigin),
          followTarget_(followTarget),
          weaponOffset_(weaponOffset),
          direction_(Vector2Normalize(direction)) {
        if (Vector2LengthSqr(direction_) <= 1e-6f) {
            direction_ = Vector2{1.0f, 0.0f};
        }
        aimAngleDeg_ = DirectionToDegrees(direction_);
        displayState_ = ComputeWeaponDisplayState(common_, direction_, aimAngleDeg_);

        Vector2 displayBase = Vector2Add(weaponOrigin_, displayState_.offset);
        projectilePosition_ = Vector2Add(displayBase, Vector2Scale(direction_, params_.muzzleOffset));
    }

    void Update(float deltaSeconds) override {
        if (followTarget_ != nullptr) {
            weaponOrigin_ = Vector2Add(*followTarget_, weaponOffset_);
        }

        projectilePosition_ = Vector2Add(projectilePosition_, Vector2Scale(direction_, params_.speed * deltaSeconds));
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

        Vector2 displayBase = Vector2Add(weaponOrigin_, displayState_.offset);
        const bool showDisplay = (common_.displayMode != WeaponDisplayMode::Hidden) &&
                                 (common_.displayHoldSeconds <= 0.0f || elapsed_ < common_.displayHoldSeconds);
        if (showDisplay) {
            DrawWeaponDisplay(common_, displayBase, displayState_.angleDeg);
        }
        DrawCircleV(projectilePosition_, params_.radius, common_.debugColor);
    }

    void CollectHitEvents(const Vector2& targetCenter,
                          float targetRadius,
                          std::mt19937& rng,
                          std::vector<ProjectileSystem::DamageEvent>& outEvents) override {
        if (damageApplied_ || expired_ || common_.damage <= 0.0f) {
            return;
        }

        float distance = Vector2Distance(projectilePosition_, targetCenter);
        float effectiveRadius = params_.radius + targetRadius;

        if (distance > effectiveRadius) {
            return;
        }

        ProjectileSystem::DamageEvent event{};
        event.amount = common_.damage;

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
    Vector2 weaponOrigin_{};
    const Vector2* followTarget_{nullptr};
    Vector2 weaponOffset_{};
    Vector2 direction_{1.0f, 0.0f};
    float aimAngleDeg_{0.0f};
    WeaponDisplayState displayState_{};
    Vector2 projectilePosition_{};
    float traveled_{0.0f};
    float elapsed_{0.0f};
    bool expired_{false};
    bool damageApplied_{false};
};

class LaserProjectile final : public ProjectileSystem::ProjectileInstance {
public:
    LaserProjectile(const ProjectileCommonParams& common,
                    const LaserProjectileParams& params,
                    Vector2 weaponOrigin,
                    const Vector2* followTarget,
                    Vector2 weaponOffset,
                    Vector2 direction)
        : common_(common),
          params_(params),
          weaponOrigin_(weaponOrigin),
          followTarget_(followTarget),
          weaponOffset_(weaponOffset),
          direction_(Vector2Normalize(direction)) {
        EnsureLaserAssetsLoaded();
        if (Vector2LengthSqr(direction_) <= 1e-6f) {
            direction_ = Vector2{1.0f, 0.0f};
        }
        aimAngleDeg_ = DirectionToDegrees(direction_);
        displayState_ = ComputeWeaponDisplayState(common_, direction_, aimAngleDeg_);

        beamDuration_ = (params_.duration > 0.0f) ? params_.duration : common_.lifespanSeconds;
        if (beamDuration_ <= 0.0f && common_.lifespanSeconds > 0.0f) {
            beamDuration_ = common_.lifespanSeconds;
        }

        if (common_.displayHoldSeconds > 0.0f) {
            staffHoldDuration_ = std::max(beamDuration_, common_.displayHoldSeconds);
        } else if (beamDuration_ > 0.0f && params_.staffHoldExtraSeconds > 0.0f) {
            staffHoldDuration_ = beamDuration_ + params_.staffHoldExtraSeconds;
        } else if (params_.staffHoldExtraSeconds > 0.0f) {
            staffHoldDuration_ = params_.staffHoldExtraSeconds;
        } else {
            staffHoldDuration_ = beamDuration_;
        }

        finalLifetime_ = staffHoldDuration_;
        if (common_.lifespanSeconds > 0.0f) {
            finalLifetime_ = std::max(finalLifetime_, common_.lifespanSeconds);
        }
        if (beamDuration_ > 0.0f) {
            finalLifetime_ = std::max(finalLifetime_, beamDuration_);
        }
    }

    void Update(float deltaSeconds) override {
        if (followTarget_ != nullptr) {
            weaponOrigin_ = Vector2Add(*followTarget_, weaponOffset_);
        }

        elapsed_ += deltaSeconds;
        if (!beamExpired_ && beamDuration_ > 0.0f && elapsed_ >= beamDuration_) {
            beamExpired_ = true;
        }

        if (finalLifetime_ > 0.0f && elapsed_ >= finalLifetime_) {
            expired_ = true;
        } else if (finalLifetime_ <= 0.0f && common_.lifespanSeconds > 0.0f && elapsed_ >= common_.lifespanSeconds) {
            expired_ = true;
        }
    }

    void Draw() const override {
        if (expired_) {
            return;
        }

        Vector2 displayBase{};
        Vector2 beamStart{};
        Vector2 beamEnd{};
        ComputeBeamGeometry(displayBase, beamStart, beamEnd);

        const bool showDisplay = (common_.displayMode != WeaponDisplayMode::Hidden) &&
                                 (staffHoldDuration_ <= 0.0f || elapsed_ < staffHoldDuration_);
        if (showDisplay) {
            if (IsTextureLoaded(g_laserAssets.staff)) {
                const float thickness = std::max(common_.displayThickness, 1.0f);
                const float length = std::max(common_.displayLength, 1.0f);
                Rectangle src{0.0f, 0.0f, static_cast<float>(g_laserAssets.staff.width), static_cast<float>(g_laserAssets.staff.height)};
                Rectangle dest{displayBase.x, displayBase.y, thickness, length};
                Vector2 origin{thickness * 0.5f, 0.0f};
                float rotation = displayState_.angleDeg - 90.0f;
                DrawTexturePro(g_laserAssets.staff, src, dest, origin, rotation, WHITE);
            } else {
                DrawWeaponDisplay(common_, displayBase, displayState_.angleDeg);
            }
        }

    const float beamLength = Vector2Distance(beamStart, beamEnd);
    const float beamThickness = std::max(params_.thickness, 1.0f);
    const float beamAngle = DirectionToDegrees(direction_) - 90.0f;
    const bool beamVisible = (beamDuration_ <= 0.0f) || !beamExpired_;

        if (beamVisible) {
            float beamAlpha = 1.0f;
            if (beamDuration_ > 0.0f && params_.fadeOutDuration > 0.0f) {
                float fadeStart = beamDuration_ - params_.fadeOutDuration;
                if (elapsed_ >= fadeStart) {
                    float remaining = beamDuration_ - elapsed_;
                    beamAlpha = Clamp01(remaining / std::max(params_.fadeOutDuration, 1e-3f));
                }
            }

            Color beamTint = ColorAlpha(WHITE, beamAlpha);

            if (IsTextureLoaded(g_laserAssets.body)) {
                Rectangle bodySrc{0.0f, 0.0f, static_cast<float>(g_laserAssets.body.width), static_cast<float>(g_laserAssets.body.height)};
                Rectangle bodyDest{beamStart.x, beamStart.y, beamThickness, beamLength};
                Vector2 bodyOrigin{beamThickness * 0.5f, 0.0f};
                DrawTexturePro(g_laserAssets.body, bodySrc, bodyDest, bodyOrigin, beamAngle, beamTint);
            } else {
                Color lineColor = ColorAlpha(common_.debugColor, beamAlpha);
                DrawLineEx(beamStart, beamEnd, params_.thickness, lineColor);
            }
        }
    }

    void CollectHitEvents(const Vector2& targetCenter,
                          float targetRadius,
                          std::mt19937& rng,
                          std::vector<ProjectileSystem::DamageEvent>& outEvents) override {
        if (damageApplied_ || expired_ || common_.damage <= 0.0f) {
            return;
        }

        if (beamExpired_) {
            return;
        }

        Vector2 displayBase{};
        Vector2 beamStart{};
        Vector2 beamEnd{};
        ComputeBeamGeometry(displayBase, beamStart, beamEnd);

        float distance = DistancePointToSegment(targetCenter, beamStart, beamEnd);
        float effectiveRadius = params_.thickness * 0.5f + targetRadius;

        if (distance > effectiveRadius) {
            return;
        }

        ProjectileSystem::DamageEvent event{};
        event.amount = common_.damage;

        if (common_.criticalChance > 0.0f) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng) < common_.criticalChance) {
                event.isCritical = true;
                float multiplier = (common_.criticalMultiplier > 0.0f) ? common_.criticalMultiplier : 1.0f;
                event.amount *= multiplier;
            }
        }

        damageApplied_ = true;
        outEvents.push_back(event);
    }

    bool IsExpired() const override {
        return expired_;
    }

private:
    ProjectileCommonParams common_{};
    LaserProjectileParams params_{};
    Vector2 weaponOrigin_{};
    const Vector2* followTarget_{nullptr};
    Vector2 weaponOffset_{};
    Vector2 direction_{1.0f, 0.0f};
    float aimAngleDeg_{0.0f};
    WeaponDisplayState displayState_{};
    float elapsed_{0.0f};
    bool expired_{false};
    bool damageApplied_{false};
    float beamDuration_{0.0f};
    float staffHoldDuration_{0.0f};
    float finalLifetime_{0.0f};
    bool beamExpired_{false};

    void ComputeBeamGeometry(Vector2& outDisplayBase, Vector2& outBeamStart, Vector2& outBeamEnd) const {
        outDisplayBase = Vector2Add(weaponOrigin_, displayState_.offset);
        // Ajuste displayState_.offset para mover a arma em relação ao jogador.
        outBeamStart = outDisplayBase;
        if (common_.displayMode != WeaponDisplayMode::Hidden) {
            Vector2 forward{common_.displayLength, 0.0f};
            Vector2 rotated = Vector2Rotate(forward, displayState_.angleDeg * DEG2RAD);
            outBeamStart = Vector2Add(outDisplayBase, rotated);
        }
        if (params_.startOffset > 0.0f) {
            Vector2 offset = Vector2Scale(direction_, params_.startOffset);
            // Ajuste params_.startOffset para aproximar ou afastar o laser do jogador.
            outBeamStart = Vector2Add(outBeamStart, offset);
        }
        outBeamEnd = Vector2Add(outBeamStart, Vector2Scale(direction_, params_.length));
    }
};

} // namespace

ProjectileSystem::ProjectileSystem() : rng_(std::random_device{}()) {}

ProjectileSystem::~ProjectileSystem() {
    UnloadLaserAssets();
}

void ProjectileSystem::Update(float deltaSeconds) {
    for (auto& projectile : projectiles_) {
        projectile->Update(deltaSeconds);
    }

    projectiles_.erase(
        std::remove_if(projectiles_.begin(), projectiles_.end(), [](const std::unique_ptr<ProjectileInstance>& projectile) {
            return projectile->IsExpired();
        }),
        projectiles_.end());
}

void ProjectileSystem::Draw() const {
    for (const auto& projectile : projectiles_) {
        projectile->Draw();
    }
}

void ProjectileSystem::SpawnProjectile(const ProjectileBlueprint& blueprint, const ProjectileSpawnContext& context) {
    if (blueprint.common.projectilesPerShot <= 0) {
        return;
    }

    std::uniform_real_distribution<float> spreadDist(
        -blueprint.common.randomSpreadDegrees * 0.5f,
         blueprint.common.randomSpreadDegrees * 0.5f);

    Vector2 baseAim = context.aimDirection;
    if (Vector2LengthSqr(baseAim) <= 1e-6f) {
        baseAim = Vector2{1.0f, 0.0f};
    }
    baseAim = Vector2Normalize(baseAim);
    float baseAngle = DirectionToDegrees(baseAim);

    const int projectileCount = blueprint.common.projectilesPerShot;
    float accumulatedDelay = 0.0f;

    for (int i = 0; i < projectileCount; ++i) {
        float spreadOffset = (blueprint.common.randomSpreadDegrees > 0.0f) ? spreadDist(rng_) : 0.0f;
        float staticAngleOffset = (i < static_cast<int>(blueprint.common.angleOffsetsDegrees.size()))
            ? blueprint.common.angleOffsetsDegrees[i]
            : 0.0f;
        Vector2 positionalOffset = (i < static_cast<int>(blueprint.common.positionalOffsets.size()))
            ? blueprint.common.positionalOffsets[i]
            : Vector2{0.0f, 0.0f};

        float totalAngleOffset = spreadOffset + staticAngleOffset;
        float finalAngle = baseAngle + totalAngleOffset;
        Vector2 spawnOrigin = Vector2Add(context.origin, positionalOffset);

        switch (blueprint.kind) {
            case ProjectileKind::Blunt: {
                float halfTravel = blueprint.blunt.travelDegrees * 0.5f;
                float startCenter = finalAngle - halfTravel;
                float endCenter = finalAngle + halfTravel;

                const Vector2* followPtr = (blueprint.blunt.followOwner && context.followTarget != nullptr)
                    ? context.followTarget
                    : nullptr;

                projectiles_.push_back(std::make_unique<BluntProjectile>(
                    blueprint.common,
                    blueprint.blunt,
                    spawnOrigin,
                    followPtr,
                    startCenter,
                    endCenter));
                break;
            }
            case ProjectileKind::Swing: {
                float halfTravel = blueprint.swing.travelDegrees * 0.5f;
                float startCenter = finalAngle - halfTravel;
                float endCenter = finalAngle + halfTravel;

                const Vector2* followPtr = (blueprint.swing.followOwner && context.followTarget != nullptr)
                    ? context.followTarget
                    : nullptr;

                projectiles_.push_back(std::make_unique<SwingProjectile>(
                    blueprint.common,
                    blueprint.swing,
                    spawnOrigin,
                    followPtr,
                    startCenter,
                    endCenter));
                break;
            }
            case ProjectileKind::Spear: {
                Vector2 aimDir = Vector2Rotate(baseAim, totalAngleOffset * DEG2RAD);
                const Vector2* followPtr = (blueprint.spear.followOwner && context.followTarget != nullptr)
                    ? context.followTarget
                    : nullptr;

                projectiles_.push_back(std::make_unique<SpearProjectile>(
                    blueprint.common,
                    blueprint.spear,
                    spawnOrigin,
                    followPtr,
                    aimDir));
                break;
            }
            case ProjectileKind::FullCircleSwing: {
                const Vector2* followPtr = (blueprint.fullCircle.followOwner && context.followTarget != nullptr)
                    ? context.followTarget
                    : nullptr;

                projectiles_.push_back(std::make_unique<FullCircleSwingProjectile>(
                    blueprint.common,
                    blueprint.fullCircle,
                    spawnOrigin,
                    followPtr,
                    finalAngle));
                break;
            }
            case ProjectileKind::Ammunition: {
                Vector2 aimDir = Vector2Rotate(baseAim, totalAngleOffset * DEG2RAD);

                const Vector2* followPtr = context.followTarget;
                Vector2 weaponOffset{0.0f, 0.0f};
                if (followPtr != nullptr) {
                    weaponOffset = Vector2Subtract(spawnOrigin, *followPtr);
                }

                projectiles_.push_back(std::make_unique<AmmunitionProjectile>(
                    blueprint.common,
                    blueprint.ammunition,
                    spawnOrigin,
                    followPtr,
                    weaponOffset,
                    aimDir));
                break;
            }
            case ProjectileKind::Laser: {
                Vector2 aimDir = Vector2Rotate(baseAim, totalAngleOffset * DEG2RAD);

                const Vector2* followPtr = context.followTarget;
                Vector2 weaponOffset{0.0f, 0.0f};
                if (followPtr != nullptr) {
                    weaponOffset = Vector2Subtract(spawnOrigin, *followPtr);
                }

                projectiles_.push_back(std::make_unique<LaserProjectile>(
                    blueprint.common,
                    blueprint.laser,
                    spawnOrigin,
                    followPtr,
                    weaponOffset,
                    aimDir));
                break;
            }
        }

        if (blueprint.common.delayBetweenProjectiles > 0.0f && i < projectileCount - 1) {
            accumulatedDelay += blueprint.common.delayBetweenProjectiles;
            // Future enhancement: enqueue delayed spawns when scheduling is supported.
        }
    }

    (void)accumulatedDelay;
}

std::vector<ProjectileSystem::DamageEvent> ProjectileSystem::CollectDamageEvents(const Vector2& targetCenter, float targetRadius) {
    std::vector<DamageEvent> events;
    events.reserve(projectiles_.size());

    for (auto& projectile : projectiles_) {
        projectile->CollectHitEvents(targetCenter, targetRadius, rng_, events);
    }

    return events;
}
