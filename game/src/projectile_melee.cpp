#include "projectile_internal.h"

#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <random>

using namespace projectile_detail;

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

    // Atualiza timers e acompanha o dono caso followOwner ativo.
    // Controla progresso da animação linear do golpe.
    // Calcula fases de extensão, idle e retração da estocada.
    // Mantém rotação contínua e encerra após completar as revoluções solicitadas.
    // Apenas controla duração do sprite exibido junto ao jogador.
    // Move o projétil em linha reta até alcançar distância máxima ou expirar.
    // Atualiza origem e controla tempo de exibição do feixe/fade.
    void Update(float deltaSeconds) override {
        if (followTarget_ != nullptr) {
            origin_ = *followTarget_;
        }

        elapsed_ += deltaSeconds;
        if (elapsed_ >= common_.lifespanSeconds && common_.lifespanSeconds > 0.0f) {
            expired_ = true;
        }
    }

    // Desenha sprite/forma do impacto conforme interpolação angular.
    // Desenha sprite alinhado ao vetor do golpe ou fallback debug.
    // Renderiza sprite ou hitbox retangular seguindo a direção da lança.
    // Desenha arma girando ao redor do jogador ou fallback retângulo.
    // Desenha sprite da arma ou representação simples, sem aplicar dano.
    // Prioriza sprite customizado, caindo para círculo colorido se não houver textura.
    // Desenha laser entre os pontos calculados, com fade opcional.
    void Draw() const override {
        if (expired_) {
            return;
        }

        const float duration = (common_.lifespanSeconds <= 0.0f) ? 1.0f : common_.lifespanSeconds;
        const float t = Clamp01(elapsed_ / duration);
        const float centerAngle = startCenterDegrees_ + (endCenterDegrees_ - startCenterDegrees_) * t;
        const float angleRad = centerAngle * DEG2RAD;
        Vector2 aimDir{std::cos(angleRad), std::sin(angleRad)};
        Vector2 rightDir{-aimDir.y, aimDir.x};
        float rectLength = (params_.length > 0.0f) ? params_.length : std::max(params_.thickness, 20.0f);
        float rectThickness = (params_.thickness > 0.0f) ? params_.thickness : rectLength * 0.35f;
        float halfLength = rectLength * 0.5f;
        float halfThickness = rectThickness * 0.5f;
        float hitboxOffset = params_.radius;
        Vector2 rectCenter = Vector2Add(origin_, Vector2Scale(aimDir, hitboxOffset));

        bool drewProjectileSprite = false;
        if (!common_.projectileSpritePath.empty()) {
            float spriteLength = (common_.displayLength > 0.0f) ? common_.displayLength : rectLength;
            float spriteThickness = (common_.displayThickness > 0.0f) ? common_.displayThickness : rectThickness;
            float spriteOffset = (common_.projectileForwardOffset != 0.0f)
                                     ? common_.projectileForwardOffset
                                     : hitboxOffset;
            Vector2 spriteCenter = Vector2Add(origin_, Vector2Scale(aimDir, spriteOffset));
            drewProjectileSprite = DrawProjectileSpriteFromPath(common_.projectileSpritePath,
                                                               spriteCenter,
                                                               centerAngle + common_.projectileRotationOffsetDegrees,
                                                               spriteLength,
                                                               spriteThickness,
                                                               WHITE);
        }

        bool drewWeaponSprite = false;
        if (common_.displayMode != WeaponDisplayMode::Hidden) {
            WeaponDisplayState displayState = ComputeWeaponDisplayState(common_, aimDir, centerAngle);
            displayState.angleDeg += common_.projectileRotationOffsetDegrees;
            Vector2 displayBase = Vector2Add(origin_, displayState.offset);
            drewWeaponSprite = DrawWeaponSpriteFromPath(common_.weaponSpritePath,
                                                       displayBase,
                                                       displayState.angleDeg,
                                                       common_.displayLength,
                                                       common_.displayThickness,
                                                       WHITE);
        }

        if (!drewProjectileSprite && !drewWeaponSprite) {
            Vector2 forward = Vector2Scale(aimDir, halfLength);
            Vector2 right = Vector2Scale(rightDir, halfThickness);
            Vector2 v0 = Vector2Subtract(Vector2Subtract(rectCenter, forward), right);
            Vector2 v1 = Vector2Add(Vector2Subtract(rectCenter, forward), right);
            Vector2 v2 = Vector2Add(Vector2Add(rectCenter, forward), right);
            Vector2 v3 = Vector2Subtract(Vector2Add(rectCenter, forward), right);
            DrawTriangle(v0, v1, v2, common_.debugColor);
            DrawTriangle(v0, v2, v3, common_.debugColor);
        }
    }

    // Testa colisão aproximada contra um alvo retangular expandido.
    // Usa distância até o segmento do golpe para gerar eventos de dano.
    // Detecta colisões ao longo da linha da lança considerando espessura.
    // Trata colisões com base na distância ao segmento do golpe em tempo real.
    // Verifica colisão de círculo para aplicar dano único.
    // Considera o feixe como segmento para aplicar dano repetido respeitando cooldown.
    void CollectHitEvents(const Vector2& targetCenter,
                          float targetRadius,
                          std::uintptr_t targetId,
                          float targetImmunitySeconds,
                          std::mt19937& rng,
                          std::vector<ProjectileSystem::DamageEvent>& outEvents) override {
        if (expired_ || common_.damage <= 0.0f) {
            return;
        }

        if (targetImmunitySeconds > 0.0f) {
            return;
        }

        const float duration = (common_.lifespanSeconds <= 0.0f) ? 1.0f : common_.lifespanSeconds;
        const float t = Clamp01(elapsed_ / duration);
        const float centerAngle = startCenterDegrees_ + (endCenterDegrees_ - startCenterDegrees_) * t;
        const float angleRad = centerAngle * DEG2RAD;
        Vector2 forward{std::cos(angleRad), std::sin(angleRad)};
        Vector2 right{-forward.y, forward.x};

        float rectLength = (params_.length > 0.0f) ? params_.length : std::max(params_.thickness, 20.0f);
        float rectThickness = (params_.thickness > 0.0f) ? params_.thickness : rectLength * 0.35f;
        float halfLength = rectLength * 0.5f;
        float halfThickness = rectThickness * 0.5f;
    float hitboxOffset = params_.radius;
    Vector2 rectCenter = Vector2Add(origin_, Vector2Scale(forward, hitboxOffset));

        Vector2 toTarget = Vector2Subtract(targetCenter, rectCenter);
        float localForward = Vector2DotProduct(toTarget, forward);
        float localRight = Vector2DotProduct(toTarget, right);
        float expandedHalfLength = halfLength + targetRadius;
        float expandedHalfThickness = halfThickness + targetRadius;

        if (std::abs(localForward) > expandedHalfLength || std::abs(localRight) > expandedHalfThickness) {
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
    ProjectileCommonParams common_{};
    BluntProjectileParams params_{};
    Vector2 origin_{};
    const Vector2* followTarget_{nullptr};
    float startCenterDegrees_{0.0f};
    float endCenterDegrees_{0.0f};
    float elapsed_{0.0f};
    bool expired_{false};
    PerTargetHitTracker perTargetHits_{};
};

// Golpes de corte tradicionais com hitbox em linha.

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
        const float adjustedAngle = centerAngle + common_.projectileRotationOffsetDegrees;
        const float angleRad = centerAngle * DEG2RAD;
        Vector2 aimDir{std::cos(angleRad), std::sin(angleRad)};

        bool drewSprite = false;
        if (!common_.projectileSpritePath.empty()) {
            float spriteLength = (common_.projectileSize > 0.0f) ? common_.projectileSize : params_.length;
            float spriteThickness = (params_.thickness > 0.0f) ? params_.thickness : spriteLength * 0.35f;
            Vector2 spriteBase = origin_;
            float forwardOffset = (spriteLength * 0.5f) + common_.projectileForwardOffset;
            Vector2 spriteCenter = Vector2Add(spriteBase, Vector2Scale(aimDir, forwardOffset));
            drewSprite = DrawProjectileSpriteFromPath(common_.projectileSpritePath,
                                                     spriteCenter,
                                                     adjustedAngle,
                                                     spriteLength,
                                                     spriteThickness,
                                                     WHITE);
        }

        if (!drewSprite && !common_.weaponSpritePath.empty()) {
            WeaponDisplayState displayState{};
            if (common_.displayMode != WeaponDisplayMode::Hidden) {
                displayState = ComputeWeaponDisplayState(common_, aimDir, centerAngle);
                displayState.angleDeg += common_.projectileRotationOffsetDegrees;
            } else {
                displayState.offset = Vector2{0.0f, 0.0f};
                displayState.angleDeg = adjustedAngle;
            }

            Vector2 displayBase = Vector2Add(origin_, displayState.offset);
            float desiredLength = (common_.displayLength > 0.0f) ? common_.displayLength : params_.length;
            float desiredThickness = (common_.displayThickness > 0.0f) ? common_.displayThickness : params_.thickness;
            drewSprite = DrawWeaponSpriteFromPath(common_.weaponSpritePath,
                                                  displayBase,
                                                  displayState.angleDeg,
                                                  desiredLength,
                                                  desiredThickness,
                                                  WHITE);
        }

        if (!drewSprite) {
            Rectangle rect{};
            rect.x = origin_.x;
            rect.y = origin_.y - params_.thickness * 0.5f;
            rect.width = params_.length;
            rect.height = params_.thickness;

            Vector2 pivot{0.0f, params_.thickness * 0.5f};
            DrawRectanglePro(rect, pivot, centerAngle, common_.debugColor);
        }
    }

    void CollectHitEvents(const Vector2& targetCenter,
                          float targetRadius,
                          std::uintptr_t targetId,
                          float targetImmunitySeconds,
                          std::mt19937& rng,
                          std::vector<ProjectileSystem::DamageEvent>& outEvents) override {
        if (expired_ || common_.damage <= 0.0f || params_.length <= 0.0f) {
            return;
        }

        if (targetImmunitySeconds > 0.0f) {
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
    ProjectileCommonParams common_{};
    SwingProjectileParams params_{};
    Vector2 origin_{};
    const Vector2* followTarget_{nullptr};
    float startCenterDegrees_{0.0f};
    float endCenterDegrees_{0.0f};
    float elapsed_{0.0f};
    bool expired_{false};
    PerTargetHitTracker perTargetHits_{};
};

// Estocadas lineares que avançam e retraem ao longo do tempo.

class SpearProjectile final : public ProjectileSystem::ProjectileInstance {
public:
    SpearProjectile(const ProjectileCommonParams& common,
                    const SpearProjectileParams& params,
                    Vector2 origin,
                    const Vector2* followTarget,
                    Vector2 followOffset,
                    Vector2 direction)
        : common_(common),
          params_(params),
          origin_(origin),
          followTarget_(followTarget),
          followOffset_(followOffset),
          direction_(Vector2Normalize(direction))
    {
        if (Vector2LengthSqr(direction_) <= 1e-5f) {
            direction_ = Vector2{1.0f, 0.0f};
        }
    }

    void Update(float deltaSeconds) override {
        if (followTarget_ != nullptr && params_.followOwner) {
            origin_ = Vector2Add(*followTarget_, followOffset_);
        }

        elapsed_ += deltaSeconds;

        const float reach = std::max(params_.reach, 0.0f);
        const float extendDuration = params_.extendDuration;
        const float idleTime = std::max(params_.idleTime, 0.0f);
        float retractDuration = params_.retractDuration;
        if (retractDuration <= 0.0f) {
            retractDuration = (extendDuration > 0.0f) ? extendDuration : 0.0f;
        }

        const float totalDuration = extendDuration + idleTime + retractDuration;

        float time = elapsed_;
        if (totalDuration <= 0.0f) {
            currentReach_ = reach;
            bool lifespanExpired = (common_.lifespanSeconds > 0.0f) && (elapsed_ >= common_.lifespanSeconds);
            if (lifespanExpired || common_.lifespanSeconds <= 0.0f) {
                currentReach_ = 0.0f;
                expired_ = true;
            }
            return;
        }

        if (time <= extendDuration) {
            float t = (extendDuration > 0.0f) ? Clamp01(time / extendDuration) : 1.0f;
            currentReach_ = reach * t;
        } else if (time <= extendDuration + idleTime) {
            currentReach_ = reach;
        } else {
            float retractTime = time - extendDuration - idleTime;
            if (retractDuration <= 0.0f) {
                currentReach_ = 0.0f;
            } else {
                float t = Clamp01(retractTime / retractDuration);
                currentReach_ = reach * (1.0f - t);
            }
        }

        bool lifespanExpired = (common_.lifespanSeconds > 0.0f) && (elapsed_ >= common_.lifespanSeconds);
        if ((totalDuration > 0.0f && time >= totalDuration) || lifespanExpired) {
            currentReach_ = 0.0f;
            expired_ = true;
        }
    }

    void Draw() const override {
        if ((expired_ && currentReach_ <= 1e-4f) || params_.length <= 1e-4f) {
            return;
        }

        Vector2 forward = direction_;
        Vector2 right{-forward.y, forward.x};
        Vector2 anchor = origin_;
        Vector2 start = Vector2Add(anchor, Vector2Scale(forward, currentReach_));
        Vector2 end = Vector2Add(start, Vector2Scale(forward, params_.length));
        Vector2 center = Vector2Scale(Vector2Add(start, end), 0.5f);

        float spriteLength = (common_.displayLength > 0.0f) ? common_.displayLength : params_.length;
        float spriteThickness = (common_.displayThickness > 0.0f) ? common_.displayThickness : params_.thickness;
        if (spriteThickness <= 0.0f) {
            spriteThickness = spriteLength * 0.2f;
        }

        const float drawAngle = DirectionToDegrees(forward) + common_.projectileRotationOffsetDegrees;
        bool drewSprite = false;

        if (!common_.projectileSpritePath.empty()) {
            drewSprite = DrawProjectileSpriteFromPath(common_.projectileSpritePath,
                                                     center,
                                                     drawAngle,
                                                     spriteLength,
                                                     spriteThickness,
                                                     WHITE);
        }

        if (!drewSprite && !common_.weaponSpritePath.empty()) {
            Vector2 spriteBase = start;
            float desiredLength = (common_.displayLength > 0.0f) ? common_.displayLength : spriteLength;
            float desiredThickness = (common_.displayThickness > 0.0f) ? common_.displayThickness : spriteThickness;
            drewSprite = DrawWeaponSpriteFromPath(common_.weaponSpritePath,
                                                 spriteBase,
                                                 drawAngle,
                                                 desiredLength,
                                                 desiredThickness,
                                                 WHITE);
        }

        if (!drewSprite) {
            float halfThickness = (params_.thickness > 0.0f) ? params_.thickness * 0.5f : spriteThickness * 0.5f;
            Vector2 offset = Vector2Scale(right, halfThickness);
            Vector2 nearLeft = Vector2Subtract(start, offset);
            Vector2 nearRight = Vector2Add(start, offset);
            Vector2 farRight = Vector2Add(end, offset);
            Vector2 farLeft = Vector2Subtract(end, offset);
            DrawTriangle(nearLeft, nearRight, farRight, common_.debugColor);
            DrawTriangle(nearLeft, farRight, farLeft, common_.debugColor);
        }
    }

    void CollectHitEvents(const Vector2& targetCenter,
                          float targetRadius,
                          std::uintptr_t targetId,
                          float targetImmunitySeconds,
                          std::mt19937& rng,
                          std::vector<ProjectileSystem::DamageEvent>& outEvents) override {
        if (common_.damage <= 0.0f || (expired_ && currentReach_ <= 1e-4f) || params_.length <= 1e-4f) {
            return;
        }

        if (targetImmunitySeconds > 0.0f) {
            return;
        }

        Vector2 forward = direction_;
        Vector2 anchor = origin_;
        Vector2 start = Vector2Add(anchor, Vector2Scale(forward, currentReach_));
        Vector2 end = Vector2Add(start, Vector2Scale(forward, params_.length));

        float distance = DistancePointToSegment(targetCenter, start, end);
        float effectiveRadius = ((params_.thickness > 0.0f) ? params_.thickness * 0.5f : params_.length * 0.1f) + targetRadius;

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
    ProjectileCommonParams common_{};
    SpearProjectileParams params_{};
    Vector2 origin_{};
    const Vector2* followTarget_{nullptr};
    Vector2 followOffset_{};
    Vector2 direction_{1.0f, 0.0f};
    float elapsed_{0.0f};
    float currentReach_{0.0f};
    bool expired_{false};
    PerTargetHitTracker perTargetHits_{};
};

// Armas que giram continuamente 360° em torno do jogador.

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

        bool drewSprite = false;
        if (common_.displayMode != WeaponDisplayMode::Hidden) {
            const float angleRad = currentAngleDeg_ * DEG2RAD;
            Vector2 aimDir{std::cos(angleRad), std::sin(angleRad)};
            WeaponDisplayState displayState = ComputeWeaponDisplayState(common_, aimDir, currentAngleDeg_);
            displayState.angleDeg += common_.projectileRotationOffsetDegrees;
            Vector2 displayBase = Vector2Add(origin_, displayState.offset);
            drewSprite = DrawWeaponSpriteFromPath(common_.weaponSpritePath,
                                                 displayBase,
                                                 displayState.angleDeg,
                                                 common_.displayLength,
                                                 common_.displayThickness,
                                                 WHITE);
        }

        if (!drewSprite) {
            Rectangle rect{};
            rect.x = origin_.x;
            rect.y = origin_.y - params_.thickness * 0.5f;
            rect.width = params_.length;
            rect.height = params_.thickness;

            Vector2 pivot{0.0f, params_.thickness * 0.5f};
            DrawRectanglePro(rect, pivot, currentAngleDeg_, common_.debugColor);
        }
    }

    void CollectHitEvents(const Vector2& targetCenter,
                          float targetRadius,
                          std::uintptr_t targetId,
                          float targetImmunitySeconds,
                          std::mt19937& rng,
                          std::vector<ProjectileSystem::DamageEvent>& outEvents) override {
        if (expired_ || common_.damage <= 0.0f || params_.length <= 1e-3f) {
            return;
        }

        if (targetImmunitySeconds > 0.0f) {
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
    ProjectileCommonParams common_{};
    FullCircleSwingParams params_{};
    Vector2 origin_{};
    const Vector2* followTarget_{nullptr};
    float currentAngleDeg_{0.0f};
    float totalRotationDeg_{0.0f};
    float elapsed_{0.0f};
    bool expired_{false};
    PerTargetHitTracker perTargetHits_{};
};

// Apenas exibe o sprite da arma para ataques à distância (sem hitbox própria).

} // namespace

namespace projectile_detail {
std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeBluntProjectile(
    const ProjectileCommonParams& common, const BluntProjectileParams& params, Vector2 origin,
    const Vector2* followTarget, float startCenterDegrees, float endCenterDegrees) {
    return std::make_unique<BluntProjectile>(common, params, origin, followTarget,
                                             startCenterDegrees, endCenterDegrees);
}

std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeSwingProjectile(
    const ProjectileCommonParams& common, const SwingProjectileParams& params, Vector2 origin,
    const Vector2* followTarget, float startCenterDegrees, float endCenterDegrees) {
    return std::make_unique<SwingProjectile>(common, params, origin, followTarget,
                                             startCenterDegrees, endCenterDegrees);
}

std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeSpearProjectile(
    const ProjectileCommonParams& common, const SpearProjectileParams& params, Vector2 anchor,
    const Vector2* followTarget, Vector2 followOffset, Vector2 direction) {
    return std::make_unique<SpearProjectile>(common, params, anchor, followTarget,
                                             followOffset, direction);
}

std::unique_ptr<ProjectileSystem::ProjectileInstance> MakeFullCircleSwingProjectile(
    const ProjectileCommonParams& common, const FullCircleSwingParams& params, Vector2 origin,
    const Vector2* followTarget, float baseAngleDegrees) {
    return std::make_unique<FullCircleSwingProjectile>(common, params, origin, followTarget,
                                                       baseAngleDegrees);
}

} // namespace projectile_detail