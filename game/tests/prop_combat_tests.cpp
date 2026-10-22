#include "projectile_internal.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace projectile_detail;
namespace {
ProjectileBlueprint Ammo() {
    ProjectileBlueprint b;b.kind=ProjectileKind::Ranged;
    ThrownProjectileBlueprint shot;shot.common.damage=12;shot.common.lifespanSeconds=2;
    shot.ammunition.speed=500;shot.ammunition.maxDistance=1000;shot.ammunition.radius=2;
    b.thrownProjectiles.push_back(shot);return b;
}
void CheckFirstImpact(bool reverse,bool rectangleFirst) {
    ProjectileSystem system;system.SpawnProjectile(Ammo(),{{0,0},nullptr,{1,0}});system.Update(0.5f);
    ProjectileTarget rect{1,{},0,Rectangle{rectangleFirst?70.0f:150.0f,-10,20,20},0};
    ProjectileTarget circle{2,{rectangleFirst?150.0f:70.0f,0},10,std::nullopt,0};
    auto events=system.CollectDamageEventsForTargets(reverse?std::vector<ProjectileTarget>{circle,rect}:std::vector<ProjectileTarget>{rect,circle});
    assert(events.size()==1&&events[0].targetId==(rectangleFirst?1u:2u));
    assert(events[0].damage.amount==12);
    assert(system.CollectDamageEventsForTargets({rect,circle}).empty());
}
void CheckFamilies() {
    ProjectileCommonParams c;c.damage=5;c.lifespanSeconds=1;c.perTargetHitCooldownSeconds=0.2f;
    ProjectileTarget target{44,{},0,Rectangle{40,-6,20,12},0};
    std::vector<std::unique_ptr<ProjectileSystem::ProjectileInstance>> projectiles;
    BluntProjectileParams blunt;blunt.radius=40;blunt.length=40;blunt.thickness=20;
    SwingProjectileParams swing;swing.length=80;swing.thickness=10;
    SpearProjectileParams spear;spear.length=80;spear.reach=0;
    FullCircleSwingParams full;full.length=80;full.thickness=10;full.angularSpeedDegreesPerSecond=0;full.revolutions=0;
    LaserProjectileParams laser;laser.length=80;laser.thickness=10;laser.duration=1;
    projectiles.push_back(MakeBluntProjectile(c,blunt,{0,0},nullptr,0,0));
    projectiles.push_back(MakeSwingProjectile(c,swing,{0,0},nullptr,0,0));
    projectiles.push_back(MakeSpearProjectile(c,spear,{0,0},nullptr,{0,0},{1,0}));
    projectiles.push_back(MakeFullCircleSwingProjectile(c,full,{0,0},nullptr,0));
    projectiles.push_back(MakeThrownLaserProjectile(c,laser,{0,0},nullptr,{0,0},{1,0},{0,0}));
    std::mt19937 rng(1);
    int family=0;
    for(auto& p:projectiles) {
        p->Update(0.01f);
        std::vector<ProjectileSystem::DamageEvent> events;
        p->CollectHitEvents(target,rng,events);
        if(events.size()!=1) std::cerr<<"Missing rectangle impact for family "<<family<<'\n';
        assert(events.size()==1);
        p->CollectHitEvents(target,rng,events);assert(events.size()==1);
        p->Update(0.21f);p->CollectHitEvents(target,rng,events);assert(events.size()==2);
        ++family;
    }
    // Rectangle corners must not use an enclosing circle (false positive).
    ProjectileTarget box{1,{},0,Rectangle{50,50,64,64},0};
    assert(!CombatGeometry::Segment(box,{0,30},{120,30},2));
    assert(CombatGeometry::Segment(box,{0,60},{120,60},2));
    assert(!CombatGeometry::Box(box,{20,20},{1,0},20,10));
}
void CheckFinalSegmentAndLegacyApi() {
    auto blueprint=Ammo();blueprint.thrownProjectiles[0].ammunition.maxDistance=100;
    ProjectileSystem shots;shots.SpawnProjectile(blueprint,{{0,0},nullptr,{1,0}});shots.Update(0.5f);
    auto hit=shots.CollectDamageEvents(Rectangle{80,-5,10,10},77);
    assert(hit.size()==1);
    shots.Clear();shots.SpawnProjectile(blueprint,{{0,0},nullptr,{1,0}});shots.Update(0.5f);
    assert(shots.CollectDamageEvents(Rectangle{130,-5,10,10},77).empty());
    shots.Clear();shots.SpawnProjectile(Ammo(),{{0,0},nullptr,{1,0}});shots.Update(0.2f);
    assert(shots.CollectDamageEvents(Vector2{90,0},10,88).size()==1);
}
}
int main() {
    for(bool reverse:{false,true}) for(bool rect:{false,true}) CheckFirstImpact(reverse,rect);
    CheckFamilies();CheckFinalSegmentAndLegacyApi();
    std::cout<<"Prop combat tests passed\n";
}
