#include "enemy_spawner.h"
#include "enemy_common.h"
#include "player.h"
#include "prop_rules.h"
#include "room.h"
#include "room_collision_rules.h"
#include "projectile.h"
#include <cassert>
#include <cmath>
#include <iostream>

namespace {
Room MakeRoom(std::uint64_t seed=1) {
    RoomLayout layout;layout.widthTiles=10;layout.heightTiles=10;layout.tileBounds={0,0,10,10};
    return Room({0,1},{RoomType::Normal,BiomeType::Dungeon,seed},layout);
}
void CheckSpawn() {
    EnemySpawner spawner;
    for(std::uint64_t seed=1;seed<=20;++seed) {
        auto room=MakeRoom(seed);
        std::vector<std::unique_ptr<Enemy>> storage;
        std::mt19937 rng(static_cast<unsigned int>(seed));
        spawner.SpawnEnemiesForRoom(room,storage,rng);
        assert(room.PropsInitialized()&&!storage.empty());
        for(const auto& enemy:storage)
            assert(PropRules::IsFree(room,PropRules::EntityBounds(enemy->GetPosition(),enemy->GetHalfWidth(),enemy->GetHalfHeight())));
    }
    auto full=MakeRoom();full.SetPropsInitialized();
    full.Props().push_back({&GetPropDefinitions()[2],{0,0,10,10},0,false});
    std::vector<std::unique_ptr<Enemy>> storage;std::mt19937 rng(1);
    spawner.SpawnEnemiesForRoom(full,storage,rng);assert(storage.empty());
}
void CheckActualEnemyMovement() {
    auto room=MakeRoom();room.SetPropsInitialized();
    room.Props().push_back({&GetPropDefinitions()[2],{3,2,1,3},0,false});
    EnemyConfig config;config.maxHealth=20;config.speed=160;config.collisionRadius=26;
    EnemyCommon enemy(config,0,nullptr,{});
    Vector2 origin{100,220},target{350,220};enemy.Initialize(room,origin);
    PlayerCharacter player=CreateKnightCharacter();ProjectileSystem shots;
    for(int frame=0;frame<1200&&std::hypot(enemy.GetPosition().x-target.x,enemy.GetPosition().y-target.y)>1;++frame) {
        enemy.Update({0.02f,player,target,room,true,shots});
        assert(PropRules::IsFree(room,PropRules::EntityBounds(enemy.GetPosition(),enemy.GetHalfWidth(),enemy.GetHalfHeight())));
    }
    assert(std::hypot(enemy.GetPosition().x-target.x,enemy.GetPosition().y-target.y)<1);
    enemy.BeginRoomReset();
    for(int frame=0;frame<1200&&std::hypot(enemy.GetPosition().x-origin.x,enemy.GetPosition().y-origin.y)>0.01f;++frame)
        enemy.Update({0.02f,player,target,room,false,shots});
    assert(enemy.GetPosition().x==origin.x&&enemy.GetPosition().y==origin.y);
}
void CheckEnemyPursuitAtBottomEdge() {
    for(float radius:{22.0f,24.0f,26.0f}) {
        auto room=MakeRoom();room.SetPropsInitialized();
        room.Props().push_back({&GetPropDefinitions()[2],{3,2,1,6},0,false});
        EnemyConfig config;config.speed=160;config.collisionRadius=radius;
        constexpr float attackRange=150;
        EnemyCommon enemy(config,attackRange,nullptr,{});
        const auto floor=RoomCollisionRules::RoomBoundsForCollision(room.Layout().tileBounds,room.GetBiome());
        const Vector2 origin{100,220},target{500,floor.y+floor.height-16};
        enemy.Initialize(room,origin);
        PlayerCharacter player=CreateKnightCharacter();ProjectileSystem shots;
        // Finish the spawn fade without moving, then start the pursuit.
        enemy.Update({0.5f,player,origin,room,true,shots});
        assert(enemy.HasCompletedFade()&&enemy.IsActive());
        for(int frame=0;frame<1000;++frame) {
            enemy.Update({0.02f,player,target,room,true,shots});
            assert(enemy.IsActive());
            assert(PropRules::IsFree(room,PropRules::EntityBounds(enemy.GetPosition(),enemy.GetHalfWidth(),enemy.GetHalfHeight())));
            if(std::hypot(enemy.GetPosition().x-target.x,enemy.GetPosition().y-target.y)<=attackRange) break;
        }
        assert(std::hypot(enemy.GetPosition().x-target.x,enemy.GetPosition().y-target.y)<=attackRange);
    }
}
}
int main() {
    CheckSpawn();CheckActualEnemyMovement();CheckEnemyPursuitAtBottomEdge();
    std::cout<<"Prop gameplay tests passed\n";
}
