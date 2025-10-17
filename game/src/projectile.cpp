#include "projectile.h"

#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {

constexpr float kRadToDeg = 180.0f / PI;

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
        if (Vector2LengthSqr(direction_) <= 1e-6f) {
            direction_ = Vector2{1.0f, 0.0f};
        }
                aimAngleDeg_ = DirectionToDegrees(direction_);
                displayState_ = ComputeWeaponDisplayState(common_, direction_, aimAngleDeg_);
    }

    void Update(float deltaSeconds) override {
        if (followTarget_ != nullptr) {
            weaponOrigin_ = Vector2Add(*followTarget_, weaponOffset_);
        }

        elapsed_ += deltaSeconds;
        const float duration = (params_.duration > 0.0f) ? params_.duration : common_.lifespanSeconds;
        if (duration > 0.0f && elapsed_ >= duration) {
            expired_ = true;
        }
        if (!expired_ && common_.lifespanSeconds > 0.0f && elapsed_ >= common_.lifespanSeconds) {
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

        Vector2 beamStart = displayBase;
        if (common_.displayMode != WeaponDisplayMode::Hidden) {
            Vector2 forward{common_.displayLength, 0.0f};
            Vector2 rotated = Vector2Rotate(forward, displayState_.angleDeg * DEG2RAD);
            beamStart = Vector2Add(displayBase, rotated);
        }
        Vector2 beamEnd = Vector2Add(beamStart, Vector2Scale(direction_, params_.length));
        DrawLineEx(beamStart, beamEnd, params_.thickness, common_.debugColor);
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
};

} // namespace

ProjectileSystem::ProjectileSystem() : rng_(std::random_device{}()) {}

ProjectileSystem::~ProjectileSystem() = default;

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

    for (int i = 0; i < blueprint.common.projectilesPerShot; ++i) {
        float spreadOffset = (blueprint.common.randomSpreadDegrees > 0.0f) ? spreadDist(rng_) : 0.0f;
        switch (blueprint.kind) {
            case ProjectileKind::Blunt: {
                float aimAngle = baseAngle + spreadOffset;
                float halfTravel = blueprint.blunt.travelDegrees * 0.5f;
                float startCenter = aimAngle - halfTravel;
                float endCenter = aimAngle + halfTravel;

                const Vector2* followPtr = (blueprint.blunt.followOwner && context.followTarget != nullptr)
                    ? context.followTarget
                    : nullptr;

                projectiles_.push_back(std::make_unique<BluntProjectile>(
                    blueprint.common,
                    blueprint.blunt,
                    context.origin,
                    followPtr,
                    startCenter,
                    endCenter));
                break;
            }
            case ProjectileKind::Swing: {
                float aimAngle = baseAngle + spreadOffset;
                float halfTravel = blueprint.swing.travelDegrees * 0.5f;
                float startCenter = aimAngle - halfTravel;
                float endCenter = aimAngle + halfTravel;

                const Vector2* followPtr = (blueprint.swing.followOwner && context.followTarget != nullptr)
                    ? context.followTarget
                    : nullptr;

                projectiles_.push_back(std::make_unique<SwingProjectile>(
                    blueprint.common,
                    blueprint.swing,
                    context.origin,
                    followPtr,
                    startCenter,
                    endCenter));
                break;
            }
            case ProjectileKind::Spear: {
                Vector2 aimDir = baseAim;
                if (spreadOffset != 0.0f) {
                    aimDir = Vector2Rotate(baseAim, spreadOffset * DEG2RAD);
                }
                const Vector2* followPtr = (blueprint.spear.followOwner && context.followTarget != nullptr)
                    ? context.followTarget
                    : nullptr;

                projectiles_.push_back(std::make_unique<SpearProjectile>(
                    blueprint.common,
                    blueprint.spear,
                    context.origin,
                    followPtr,
                    aimDir));
                break;
            }
            case ProjectileKind::FullCircleSwing: {
                float aimAngle = baseAngle + spreadOffset;

                const Vector2* followPtr = (blueprint.fullCircle.followOwner && context.followTarget != nullptr)
                    ? context.followTarget
                    : nullptr;

                projectiles_.push_back(std::make_unique<FullCircleSwingProjectile>(
                    blueprint.common,
                    blueprint.fullCircle,
                    context.origin,
                    followPtr,
                    aimAngle));
                break;
            }
            case ProjectileKind::Ammunition: {
                Vector2 aimDir = baseAim;
                if (spreadOffset != 0.0f) {
                    aimDir = Vector2Rotate(baseAim, spreadOffset * DEG2RAD);
                }

                const Vector2* followPtr = context.followTarget;
                Vector2 weaponOffset{0.0f, 0.0f};
                if (followPtr != nullptr) {
                    weaponOffset = Vector2Subtract(context.origin, *followPtr);
                }

                projectiles_.push_back(std::make_unique<AmmunitionProjectile>(
                    blueprint.common,
                    blueprint.ammunition,
                    context.origin,
                    followPtr,
                    weaponOffset,
                    aimDir));
                break;
            }
            case ProjectileKind::Laser: {
                Vector2 aimDir = baseAim;
                if (spreadOffset != 0.0f) {
                    aimDir = Vector2Rotate(baseAim, spreadOffset * DEG2RAD);
                }

                const Vector2* followPtr = context.followTarget;
                Vector2 weaponOffset{0.0f, 0.0f};
                if (followPtr != nullptr) {
                    weaponOffset = Vector2Subtract(context.origin, *followPtr);
                }

                projectiles_.push_back(std::make_unique<LaserProjectile>(
                    blueprint.common,
                    blueprint.laser,
                    context.origin,
                    followPtr,
                    weaponOffset,
                    aimDir));
                break;
            }
        }
    }
}
