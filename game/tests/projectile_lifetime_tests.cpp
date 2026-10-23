#include "enemy_common.h"
#include "projectile.h"
#include "room.h"

#include <cassert>
#include <iostream>
#include <new>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {
// Make the original dangling-pointer read reproducibly fault instead of relying on heap reuse.
class GuardedPosition {
public:
    GuardedPosition() {
#ifdef _WIN32
        data_ = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
        data_ = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(data_ != MAP_FAILED);
#endif
        assert(data_);
        new (data_) Vector2{0, 0};
    }
    ~GuardedPosition() {
#ifdef _WIN32
        VirtualFree(data_, 0, MEM_RELEASE);
#else
        munmap(data_, 4096);
#endif
    }
    Vector2* Get() { return static_cast<Vector2*>(data_); }
    void MakeInaccessible() {
#ifdef _WIN32
        DWORD previous;
        assert(VirtualProtect(data_, 4096, PAGE_NOACCESS, &previous));
#else
        assert(mprotect(data_, 4096, PROT_NONE) == 0);
#endif
    }
private:
    void* data_{nullptr};
};

ProjectileBlueprint Following(int family) {
    ProjectileBlueprint blueprint;
    blueprint.common.damage = 5;
    blueprint.common.lifespanSeconds = 2;
    blueprint.common.displayHoldSeconds = 2;
    blueprint.blunt.radius = 40; blueprint.blunt.length = 80;
    blueprint.swing.length = 80; blueprint.swing.travelDegrees = 0;
    blueprint.spear.length = 80; blueprint.spear.reach = 0; blueprint.spear.idleTime = 1;
    blueprint.fullCircle.length = 80; blueprint.fullCircle.angularSpeedDegreesPerSecond = 0;
    blueprint.fullCircle.revolutions = 0;
    blueprint.kind = family < 4 ? static_cast<ProjectileKind>(family) : ProjectileKind::Ranged;
    if (family == 5) {
        ThrownProjectileBlueprint laser;
        laser.kind = ThrownProjectileKind::Laser; laser.followOwner = true;
        laser.common = blueprint.common; laser.laser.length = 80; laser.laser.duration = 2;
        blueprint.thrownProjectiles.push_back(laser);
    }
    return blueprint;
}

ProjectileSpawnContext Context(Vector2* position, const std::shared_ptr<int>& lifetime) {
    ProjectileSpawnContext context{*position, position, {1, 0}};
    context.ownerLifetime = std::weak_ptr<const void>(lifetime);
    return context;
}

void CheckFollowingFamilies() {
    for (int family = 0; family < 6; ++family) {
        GuardedPosition position;
        auto lifetime = std::make_shared<int>(0);
        ProjectileSystem system;
        auto context = Context(position.Get(), lifetime);
        system.SpawnProjectile(Following(family), context);
        *position.Get() = {100, 100};
        system.Update(0.01f);
        const auto hits = system.CollectDamageEvents(Rectangle{136, 96, 8, 8}, 1);
        assert(hits.size() == (family == 4 ? 0u : 1u));
        assert(system.CollectDamageEvents(Rectangle{36, -4, 8, 8}, 2).empty());

        lifetime.reset();
        position.MakeInaccessible();
        // All three collection APIs and Draw must ignore invalid owners even before Update.
        assert(system.CollectDamageEvents(Rectangle{136, 96, 8, 8}, 3).empty());
        assert(system.CollectDamageEvents(Vector2{140, 100}, 4, 4).empty());
        assert(system.CollectDamageEventsForTargets({{5, {140, 100}, 4, std::nullopt, 0}}).empty());
        system.Draw(); // No graphics context: every invalid-owner instance must be skipped.
        system.Update(0.01f); // Would fault on the protected page without the lifetime guard.
        assert(system.CollectDamageEvents(Rectangle{136, 96, 8, 8}, 6).empty());
        system.SpawnProjectile(Following(family), context); // Reject an already expired context safely.
        system.Update(0.01f);
    }
}

void CheckReusedAddress() {
    Vector2 position{0, 0};
    ProjectileSystem system;
    auto lifetime = std::make_shared<int>(0);
    system.SpawnProjectile(Following(5), Context(&position, lifetime));
    lifetime.reset();
    lifetime = std::make_shared<int>(0);
    position = {200, 200}; // Same position address, different owner generation.
    system.SpawnProjectile(Following(5), Context(&position, lifetime));
    system.Update(0.01f);
    assert(system.CollectDamageEvents(Rectangle{236, 196, 8, 8}, 1).size() == 1);
}

void CheckIndependentAttacks() {
    for (int family : {0, 5, 6}) {
        GuardedPosition position;
        auto lifetime = std::make_shared<int>(0);
        auto blueprint = Following(family);
        blueprint.blunt.followOwner = false;
        if (family == 5) blueprint.thrownProjectiles[0].followOwner = false;
        if (family == 6) {
            ThrownProjectileBlueprint ammo;
            ammo.common = blueprint.common;
            ammo.ammunition.speed = 100; ammo.ammunition.maxDistance = 1000; ammo.ammunition.radius = 2;
            ammo.followOwner = true; // Ammunition does not actually follow, even if this flag is set.
            blueprint.thrownProjectiles.push_back(ammo);
        }
        ProjectileSystem system;
        system.SpawnProjectile(blueprint, Context(position.Get(), lifetime));
        lifetime.reset(); position.MakeInaccessible();
        system.Update(0.5f);
        assert(system.CollectDamageEvents(Rectangle{36, -4, 8, 8}, 1).size() == 1);
    }
    Vector2 playerPosition{0, 0};
    ProjectileSystem legacy;
    legacy.SpawnProjectile(Following(5), {playerPosition, &playerPosition, {1, 0}});
    playerPosition = {100, 100}; legacy.Update(0.01f);
    assert(legacy.CollectDamageEvents(Rectangle{136, 96, 8, 8}, 1).size() == 1);
}

void CheckEnemyDeathAndRespawn() {
    RoomLayout layout; layout.widthTiles = 10; layout.heightTiles = 10; layout.tileBounds = {0, 0, 10, 10};
    Room room({}, {RoomType::Normal, BiomeType::Dungeon, 1}, layout);
    EnemyConfig config; config.maxHealth = 20;
    WeaponBlueprint weapon; weapon.projectile = Following(5); weapon.damage.baseDamage = 5;
    weapon.cooldownSeconds = 10;
    auto player = CreateKnightCharacter();
    ProjectileSystem shots;
    auto enemy = std::make_unique<EnemyCommon>(config, 500, &weapon, EnemySpriteInfo{});
    auto activate = [&] {
        enemy->Initialize(room, {100, 100});
        for (int frame = 0; frame < 60; ++frame)
            enemy->Update({0.02f, player, {140, 100}, room, true, shots});
        assert(enemy->HasCompletedFade());
        shots.Update(0.01f);
        assert(!shots.CollectDamageEvents(Rectangle{136, 96, 8, 8}, 1).empty());
    };
    activate();
    const auto oldLife = enemy->GetProjectileLifetime();
    assert(!oldLife.expired());
    assert(enemy->TakeDamage(20));
    assert(oldLife.expired());
    assert(shots.CollectDamageEvents(Rectangle{136, 96, 8, 8}, 2).empty());
    enemy->HealToFull();
    assert(!enemy->GetProjectileLifetime().expired() && oldLife.expired());
    assert(shots.CollectDamageEvents(Rectangle{136, 96, 8, 8}, 3).empty());
    const auto healedLife = enemy->GetProjectileLifetime();
    enemy->ResetSpawnState(); assert(healedLife.expired());
    shots.Update(0.01f);

    enemy = std::make_unique<EnemyCommon>(config, 500, &weapon, EnemySpriteInfo{});
    activate();
    const auto destroyedLife = enemy->GetProjectileLifetime();
    assert(enemy->TakeDamage(20));
    enemy.reset(); // The crash sequence: remove the dead enemy before updating its laser.
    assert(destroyedLife.expired());
    shots.Draw(); shots.Update(0.01f);
    assert(shots.CollectDamageEvents(Rectangle{136, 96, 8, 8}, 4).empty());
    enemy = std::make_unique<EnemyCommon>(config, 0, nullptr, EnemySpriteInfo{});
    const auto directDestruction = enemy->GetProjectileLifetime();
    enemy.reset(); assert(directDestruction.expired());
}
}

int main() {
    CheckFollowingFamilies(); CheckReusedAddress(); CheckIndependentAttacks(); CheckEnemyDeathAndRespawn();
    std::cout << "Projectile lifetime tests passed\n";
}
