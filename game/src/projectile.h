#pragma once

#include "raylib.h"

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

struct ProjectileSpawnContext;

enum class ProjectileKind {
    Blunt,
    Swing,
    Spear,
    FullCircleSwing,
    Ranged
};

enum class WeaponDisplayMode {
    Hidden,
    Fixed,
    AimAligned
};

struct ProjectileCommonParams {
    float damage{0.0f};
    float lifespanSeconds{0.0f};
    float projectileSpeed{0.0f};
    float projectileSize{0.0f};
    int projectilesPerShot{1};
    float randomSpreadDegrees{0.0f};
    std::vector<float> angleOffsetsDegrees{};
    std::vector<Vector2> positionalOffsets{};
    float delayBetweenProjectiles{0.0f};
    Color debugColor{200, 200, 255, 255};
    std::string spriteId{};
    std::string weaponSpritePath{};
    std::string projectileSpritePath{};
    float projectileRotationOffsetDegrees{0.0f};
    float projectileForwardOffset{0.0f};
    WeaponDisplayMode displayMode{WeaponDisplayMode::Hidden};
    Vector2 displayOffset{24.0f, -8.0f};
    float displayLength{36.0f};
    float displayThickness{10.0f};
    Color displayColor{180, 180, 200, 255};
    float displayHoldSeconds{0.0f};
    float criticalChance{0.0f};
    float criticalMultiplier{1.0f};
    float perTargetHitCooldownSeconds{0.0f};
};

struct BluntProjectileParams {
    float radius{48.0f};
    float travelDegrees{0.0f};
    float length{48.0f};
    float thickness{20.0f};
    bool followOwner{true};
};

struct SwingProjectileParams {
    float length{88.0f};
    float thickness{24.0f};
    float travelDegrees{120.0f};
    bool followOwner{true};
};

struct SpearProjectileParams {
    float length{96.0f};
    float thickness{16.0f};
    float reach{96.0f};
    float extendDuration{0.2f};
    float idleTime{0.0f};
    float retractDuration{0.2f};
    bool followOwner{true};
    Vector2 offset{0.0f, 0.0f};
};

struct FullCircleSwingParams {
    float length{96.0f};
    float thickness{28.0f};
    float revolutions{1.0f};
    float angularSpeedDegreesPerSecond{360.0f};
    bool followOwner{true};
};

struct AmmunitionProjectileParams {
    float speed{420.0f};
    float maxDistance{480.0f};
    float radius{6.0f};
};

struct LaserProjectileParams {
    float length{360.0f};
    float thickness{14.0f};
    float duration{0.3f};
    float startOffset{0.0f};
    float fadeOutDuration{0.18f};
    float staffHoldExtraSeconds{0.35f};
};

enum class ThrownProjectileKind {
    Ammunition,
    Laser
};

struct ThrownProjectileBlueprint {
    ThrownProjectileKind kind{ThrownProjectileKind::Ammunition};
    ProjectileCommonParams common{};
    AmmunitionProjectileParams ammunition{};
    LaserProjectileParams laser{};
    bool followOwner{false};
};

struct ProjectileBlueprint {
    ProjectileKind kind{ProjectileKind::Blunt};
    ProjectileCommonParams common{};
    BluntProjectileParams blunt{};
    SwingProjectileParams swing{};
    SpearProjectileParams spear{};
    FullCircleSwingParams fullCircle{};
    float thrownSpawnForwardOffset{0.0f};
    std::vector<ThrownProjectileBlueprint> thrownProjectiles{};
};

struct ProjectileSpawnContext {
    Vector2 origin{};
    const Vector2* followTarget{nullptr};
    Vector2 aimDirection{1.0f, 0.0f};
};

class ProjectileSystem {
public:
    ProjectileSystem();
    ~ProjectileSystem();

    ProjectileSystem(const ProjectileSystem&) = delete;
    ProjectileSystem& operator=(const ProjectileSystem&) = delete;
    void Update(float deltaSeconds);
    void Draw() const;
    void Clear();

    void SpawnProjectile(const ProjectileBlueprint& blueprint, const ProjectileSpawnContext& context);

    struct DamageEvent {
        float amount{0.0f};
        bool isCritical{false};
        float suggestedImmunitySeconds{0.0f};
    };

    std::vector<DamageEvent> CollectDamageEvents(const Vector2& targetCenter,
                                                float targetRadius,
                                                std::uintptr_t targetId = 0,
                                                float targetImmunitySeconds = 0.0f);

    struct ProjectileInstance;

private:
    std::vector<std::unique_ptr<ProjectileInstance>> projectiles_;
    std::mt19937 rng_;
};
