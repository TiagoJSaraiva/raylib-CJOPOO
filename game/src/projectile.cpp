#include "projectile.h"

#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>

namespace {

// Conversão auxiliar para transformar radianos em graus.
constexpr float kRadToDeg = 180.0f / PI;

// Mantém texturas carregadas uma vez para reutilização entre projeções de armas.
struct CachedTexture {
    Texture2D texture{};
    bool attemptedLoad{false};
};

// Armazena texturas carregadas por caminho para reutilização posterior.
std::unordered_map<std::string, CachedTexture> g_spriteCache{};

// Controla o tempo mínimo entre golpes do mesmo projétil e alvo.
struct PerTargetHitTracker {
    std::unordered_map<std::uintptr_t, float> lastHitSeconds{};

    // Retorna true se já se passou tempo suficiente desde o último acerto no alvo.
    bool CanHit(std::uintptr_t targetId, float currentTimeSeconds, float cooldownSeconds) const {
        if (cooldownSeconds <= 0.0f) {
            return true;
        }

        auto it = lastHitSeconds.find(targetId);
        if (it == lastHitSeconds.end()) {
            return true;
        }

        return (currentTimeSeconds - it->second) >= cooldownSeconds;
    }

    // Registra o instante em que o alvo foi atingido.
    void RecordHit(std::uintptr_t targetId, float currentTimeSeconds) {
        lastHitSeconds[targetId] = currentTimeSeconds;
    }
};

// Carrega textura apenas se existir fisicamente no disco.
Texture2D LoadTextureIfAvailable(const std::string& path) {
    if (path.empty()) {
        return Texture2D{};
    }

    if (FileExists(path.c_str())) {
        Texture2D texture = LoadTexture(path.c_str());
        if (texture.id != 0) {
            SetTextureFilter(texture, TEXTURE_FILTER_POINT);
            return texture;
        }
    }

    return Texture2D{};
}

// Retorna textura de sprite usando cache global para evitar carregamentos repetidos.
Texture2D AcquireSpriteTexture(const std::string& path) {
    if (path.empty()) {
        return Texture2D{};
    }

    auto& entry = g_spriteCache[path];
    if (!entry.attemptedLoad) {
        entry.attemptedLoad = true;
        entry.texture = LoadTextureIfAvailable(path);
    }

    return entry.texture;
}

// Limpa todas as texturas armazenadas quando o sistema é destruído.
void ReleaseSpriteCache() {
    for (auto& pair : g_spriteCache) {
        CachedTexture& entry = pair.second;
        if (entry.texture.id != 0) {
            UnloadTexture(entry.texture);
            entry.texture = Texture2D{};
        }
        entry.attemptedLoad = false;
    }

    g_spriteCache.clear();
}

// Clamps auxiliar limitado a [0,1] sem depender de std::clamp (evita incluir <algorithm> extra).
float Clamp01(float value) {
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

// Converte um vetor direção para ângulo em graus, cuidando de vetores quase nulos.
float DirectionToDegrees(Vector2 dir) {
    const float lengthSq = dir.x * dir.x + dir.y * dir.y;
    if (lengthSq <= 1e-5f) {
        return 0.0f;
    }
    return std::atan2(dir.y, dir.x) * kRadToDeg;
}

// Distância mínima entre um ponto e um segmento 2D; usada por armas do tipo linha.
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

// Calcula offset e rotação do sprite da arma conforme modo de exibição escolhido.
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

// Desenha um retângulo representando a arma caso ícones específicos não estejam disponíveis.
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

// Tenta desenhar sprite de arma; retorna false se não houver textura carregada.
bool DrawWeaponSpriteFromPath(const std::string& spritePath,
                              const Vector2& basePosition,
                              float angleDegrees,
                              float desiredLength,
                              float desiredThickness,
                              Color tint) {
    Texture2D texture = AcquireSpriteTexture(spritePath);
    if (texture.id == 0) {
        return false;
    }

    float length = (desiredLength > 0.0f) ? desiredLength : static_cast<float>(texture.height);
    float thickness = (desiredThickness > 0.0f) ? desiredThickness : static_cast<float>(texture.width);
    Rectangle src{0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height)};
    Rectangle dest{basePosition.x, basePosition.y, thickness, length};
    Vector2 origin{thickness * 0.5f, 0.0f};
    float rotation = angleDegrees - 90.0f;
    DrawTexturePro(texture, src, dest, origin, rotation, tint);
    return true;
}

// Desenha sprite de projétil posicionado no centro indicado.
bool DrawProjectileSpriteFromPath(const std::string& spritePath,
                                  const Vector2& center,
                                  float angleDegrees,
                                  float desiredLength,
                                  float desiredThickness,
                                  Color tint) {
    Texture2D texture = AcquireSpriteTexture(spritePath);
    if (texture.id == 0) {
        return false;
    }

    float length = (desiredLength > 0.0f) ? desiredLength : static_cast<float>(texture.height);
    float thickness = (desiredThickness > 0.0f) ? desiredThickness : static_cast<float>(texture.width);
    Rectangle src{0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height)};
    Rectangle dest{center.x, center.y, thickness, length};
    Vector2 origin{thickness * 0.5f, length * 0.5f};
    float rotation = angleDegrees - 90.0f;
    DrawTexturePro(texture, src, dest, origin, rotation, tint);
    return true;
}

// Versão especializada para raios/lasers que ocupam um segmento de reta.
bool DrawBeamSpriteFromPath(const std::string& spritePath,
                            const Vector2& start,
                            const Vector2& end,
                            float desiredThickness,
                            Color tint) {
    Texture2D texture = AcquireSpriteTexture(spritePath);
    if (texture.id == 0) {
        return false;
    }

    float length = Vector2Distance(start, end);
    if (length <= 1e-3f) {
        return false;
    }

    float thickness = (desiredThickness > 0.0f) ? desiredThickness : static_cast<float>(texture.width);
    Rectangle src{0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height)};
    Rectangle dest{start.x, start.y, thickness, length};
    Vector2 origin{thickness * 0.5f, 0.0f};
    Vector2 direction = Vector2Normalize(Vector2Subtract(end, start));
    float rotation = DirectionToDegrees(direction) - 90.0f;
    DrawTexturePro(texture, src, dest, origin, rotation, tint);
    return true;
}

} // namespace

// Classe base para todas as variações de projéteis, oferecendo interface de atualização/render.
struct ProjectileSystem::ProjectileInstance {
    virtual ~ProjectileInstance() = default;
    // Atualiza timers/posições internas conforme delta.
    virtual void Update(float deltaSeconds) = 0;
    // Responsável por desenhar o projétil, se ainda ativo.
    virtual void Draw() const = 0;
    // Indica se o projétil pode ser removido do vetor principal.
    virtual bool IsExpired() const = 0;
    // Opcional: coleta eventos de dano contra um alvo.
    virtual void CollectHitEvents(const Vector2&,
                                  float,
                                  std::uintptr_t,
                                  float,
                                  std::mt19937&,
                                  std::vector<ProjectileSystem::DamageEvent>&) {}
};

namespace {

// Representa ataques curtos e semicirculares como marretas.
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

// Inicializa RNG usado para spreads/críticos.
ProjectileSystem::ProjectileSystem() : rng_(std::random_device{}()) {}

// Libera sprites carregados quando o sistema é destruído.
ProjectileSystem::~ProjectileSystem() {
    ReleaseSpriteCache();
}

// Atualiza todos os projéteis ativos e limpa os que expiraram.
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

// Desenha cada projétil ativo (debug ou sprites customizados).
void ProjectileSystem::Draw() const {
    for (const auto& projectile : projectiles_) {
        projectile->Draw();
    }
}

// Remove instantaneamente todos os projéteis, usado ao reiniciar a run.
void ProjectileSystem::Clear() {
    projectiles_.clear();
}

// Instancia projéteis conforme blueprint e contexto atual do disparo.
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

        Vector2 aimDir = Vector2Rotate(baseAim, totalAngleOffset * DEG2RAD);
        if (Vector2LengthSqr(aimDir) <= 1e-6f) {
            aimDir = Vector2{1.0f, 0.0f};
        }
        aimDir = Vector2Normalize(aimDir);
        float aimAngleDeg = DirectionToDegrees(aimDir);

        WeaponDisplayState rangedDisplayState{};
        Vector2 rangedDisplayBase{};
        bool hasRangedDisplayState = false;

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
                const Vector2* followPtr = (blueprint.spear.followOwner && context.followTarget != nullptr)
                    ? context.followTarget
                    : nullptr;

                Vector2 right{-aimDir.y, aimDir.x};
                Vector2 offsetWorld = Vector2Add(Vector2Scale(aimDir, blueprint.spear.offset.x),
                                                 Vector2Scale(right, blueprint.spear.offset.y));
                if (std::abs(blueprint.common.projectileForwardOffset) > 1e-4f) {
                    offsetWorld = Vector2Add(offsetWorld, Vector2Scale(aimDir, blueprint.common.projectileForwardOffset));
                }
                Vector2 anchor = Vector2Add(spawnOrigin, offsetWorld);
                Vector2 followOffset{0.0f, 0.0f};
                if (followPtr != nullptr) {
                    followOffset = Vector2Subtract(anchor, *followPtr);
                }

                projectiles_.push_back(std::make_unique<SpearProjectile>(
                    blueprint.common,
                    blueprint.spear,
                    anchor,
                    followPtr,
                    followOffset,
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
            case ProjectileKind::Ranged: {
                const Vector2* followPtr = context.followTarget;
                Vector2 weaponOffset{0.0f, 0.0f};
                if (followPtr != nullptr) {
                    weaponOffset = Vector2Subtract(spawnOrigin, *followPtr);
                }

                rangedDisplayState = ComputeWeaponDisplayState(blueprint.common, aimDir, aimAngleDeg);
                rangedDisplayState.angleDeg += blueprint.common.projectileRotationOffsetDegrees;
                rangedDisplayBase = Vector2Add(spawnOrigin, rangedDisplayState.offset);
                hasRangedDisplayState = true;

                projectiles_.push_back(std::make_unique<RangedWeaponDisplayProjectile>(
                    blueprint.common,
                    spawnOrigin,
                    followPtr,
                    weaponOffset,
                    aimDir));
                break;
            }
        }

        if (!blueprint.thrownProjectiles.empty()) {
            for (const auto& thrown : blueprint.thrownProjectiles) {
                const Vector2* followPtr = (thrown.followOwner && context.followTarget != nullptr)
                    ? context.followTarget
                    : nullptr;

                switch (thrown.kind) {
                    case ThrownProjectileKind::Ammunition: {
                        Vector2 origin = spawnOrigin;
                        float forward = blueprint.thrownSpawnForwardOffset + thrown.common.projectileForwardOffset;
                        if (std::abs(forward) > 1e-4f) {
                            origin = Vector2Add(origin, Vector2Scale(aimDir, forward));
                        }

                        projectiles_.push_back(std::make_unique<ThrownAmmunitionProjectile>(
                            thrown.common,
                            thrown.ammunition,
                            origin,
                            aimDir));
                        break;
                    }
                    case ThrownProjectileKind::Laser: {
                        Vector2 origin = spawnOrigin;
                        float displayAngle = aimAngleDeg + blueprint.common.projectileRotationOffsetDegrees;
                        Vector2 startOffset{0.0f, 0.0f};

                        if (hasRangedDisplayState) {
                            origin = rangedDisplayBase;
                            displayAngle = rangedDisplayState.angleDeg;
                        } else if (blueprint.common.displayMode != WeaponDisplayMode::Hidden) {
                            WeaponDisplayState tempState = ComputeWeaponDisplayState(blueprint.common, aimDir, aimAngleDeg);
                            tempState.angleDeg += blueprint.common.projectileRotationOffsetDegrees;
                            origin = Vector2Add(spawnOrigin, tempState.offset);
                            displayAngle = tempState.angleDeg;
                        }

                        if (blueprint.common.displayMode != WeaponDisplayMode::Hidden) {
                            Vector2 forwardVec{blueprint.common.displayLength, 0.0f};
                            Vector2 rotatedForward = Vector2Rotate(forwardVec, displayAngle * DEG2RAD);
                            startOffset = Vector2Add(startOffset, rotatedForward);
                        }

                        float totalForward = blueprint.thrownSpawnForwardOffset + thrown.common.projectileForwardOffset + thrown.laser.startOffset;
                        if (std::abs(totalForward) > 1e-4f) {
                            startOffset = Vector2Add(startOffset, Vector2Scale(aimDir, totalForward));
                        }

                        Vector2 followOffset{0.0f, 0.0f};
                        if (followPtr != nullptr) {
                            followOffset = Vector2Subtract(origin, *followPtr);
                        }

                        projectiles_.push_back(std::make_unique<ThrownLaserProjectile>(
                            thrown.common,
                            thrown.laser,
                            origin,
                            followPtr,
                            followOffset,
                            aimDir,
                            startOffset));
                        break;
                    }
                }
            }
        }

        if (blueprint.common.delayBetweenProjectiles > 0.0f && i < projectileCount - 1) {
            accumulatedDelay += blueprint.common.delayBetweenProjectiles;
            // Future enhancement: enqueue delayed spawns when scheduling is supported.
        }
    }

    (void)accumulatedDelay;
}

std::vector<ProjectileSystem::DamageEvent> ProjectileSystem::CollectDamageEvents(const Vector2& targetCenter,
                                                                                float targetRadius,
                                                                                std::uintptr_t targetId,
                                                                                float targetImmunitySeconds) {
    // Percorre lista de projéteis acumulando todos os impactos contra o alvo especificado.
    std::vector<DamageEvent> events;
    events.reserve(projectiles_.size());

    for (auto& projectile : projectiles_) {
        projectile->CollectHitEvents(targetCenter, targetRadius, targetId, targetImmunitySeconds, rng_, events);
    }

    return events;
}
