#include "projectile_internal.h"

#include "raymath.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <unordered_map>

using namespace projectile_detail;

namespace projectile_detail {

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

// Inicializa RNG usado para spreads/críticos.
ProjectileSystem::ProjectileSystem() : rng_(std::random_device{}()) {}

// Libera sprites carregados quando o sistema é destruído.
ProjectileSystem::~ProjectileSystem() {
    projectile_detail::ReleaseSpriteCache();
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

                projectiles_.push_back(projectile_detail::MakeBluntProjectile(
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

                projectiles_.push_back(projectile_detail::MakeSwingProjectile(
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

                projectiles_.push_back(projectile_detail::MakeSpearProjectile(
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

                projectiles_.push_back(projectile_detail::MakeFullCircleSwingProjectile(
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

                projectiles_.push_back(projectile_detail::MakeRangedWeaponDisplayProjectile(
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

                        projectiles_.push_back(projectile_detail::MakeThrownAmmunitionProjectile(
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

                        projectiles_.push_back(projectile_detail::MakeThrownLaserProjectile(
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
