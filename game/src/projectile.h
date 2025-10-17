#pragma once

#include "raylib.h"

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
    Ammunition,
    Laser
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
    Color debugColor{200, 200, 255, 255};
    std::string spriteId{};
    WeaponDisplayMode displayMode{WeaponDisplayMode::Hidden};
    Vector2 displayOffset{24.0f, -8.0f};
    float displayLength{36.0f};
    float displayThickness{10.0f};
    Color displayColor{180, 180, 200, 255};
    float displayHoldSeconds{0.0f};
    float criticalChance{0.0f};
    float criticalMultiplier{1.0f};
};

struct BluntProjectileParams {
    float radius{48.0f};
    float travelDegrees{140.0f};
    float arcSpanDegrees{60.0f};
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
    float extendDistance{140.0f};
    float shaftThickness{14.0f};
    float tipLength{22.0f};
    float extendDuration{0.25f};
    float retractDuration{0.25f};
    bool followOwner{true};
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
    float muzzleOffset{24.0f};
};

struct LaserProjectileParams {
    float length{360.0f};
    float thickness{14.0f};
    float duration{0.3f};
};

struct ProjectileBlueprint {
    ProjectileKind kind{ProjectileKind::Blunt};
    ProjectileCommonParams common{};
    BluntProjectileParams blunt{};
    SwingProjectileParams swing{};
    SpearProjectileParams spear{};
    FullCircleSwingParams fullCircle{};
    AmmunitionProjectileParams ammunition{};
    LaserProjectileParams laser{};
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

    void SpawnProjectile(const ProjectileBlueprint& blueprint, const ProjectileSpawnContext& context);

    struct DamageEvent {
        float amount{0.0f};
        bool isCritical{false};
    };

    std::vector<DamageEvent> CollectDamageEvents(const Vector2& targetCenter, float targetRadius);

    struct ProjectileInstance;

private:
    std::vector<std::unique_ptr<ProjectileInstance>> projectiles_;
    std::mt19937 rng_;
};
