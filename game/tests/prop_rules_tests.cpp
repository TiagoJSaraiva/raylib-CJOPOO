#include "prop_rules.h"
#include "room.h"
#include "room_manager.h"
#include "room_collision_rules.h"
#include <cassert>
#include <cmath>
#include <iostream>

namespace {
Room MakeRoom(int width=12,int height=12,RoomType type=RoomType::Normal,BiomeType biome=BiomeType::Dungeon,std::uint64_t seed=17) {
    RoomLayout layout;
    layout.widthTiles=width;layout.heightTiles=height;layout.tileBounds={-3,2,width,height};
    for(auto direction:{Direction::North,Direction::South,Direction::East,Direction::West}) {
        Doorway door;door.direction=direction;door.offset=4;layout.doors.push_back(door);
    }
    return Room({0,1},{type,biome,seed},layout);
}

void CheckGeneration() {
    for(std::uint64_t seed=1;seed<=24;++seed) {
        Room a=MakeRoom(12,12,RoomType::Normal,BiomeType::Dungeon,seed);
        Room b=MakeRoom(12,12,RoomType::Normal,BiomeType::Dungeon,seed);
        PropRules::Initialize(a);PropRules::Initialize(b);
        assert(!a.Props().empty()&&a.Props().size()==b.Props().size());
        int area=0;
        for(std::size_t i=0;i<a.Props().size();++i) {
            const auto& p=a.Props()[i];const auto& q=b.Props()[i];
            assert(p.definition==q.definition&&p.tiles.x==q.tiles.x&&p.tiles.y==q.tiles.y);
            assert(p.tiles.x>=-3&&p.tiles.y>=2&&p.tiles.x+p.tiles.width<=9&&p.tiles.y+p.tiles.height<=14);
            area+=p.tiles.width*p.tiles.height;
            for(std::size_t j=i+1;j<a.Props().size();++j) {
                auto expanded=p.tiles;--expanded.x;--expanded.y;expanded.width+=2;expanded.height+=2;
                assert(!Intersects(expanded,a.Props()[j].tiles));
            }
        }
        assert(area<=int(144*0.15));assert(PropRules::FloorConnected(a));
        auto original=a.Props().size();auto identity=a.Props()[0].TargetId();
        a.Props()[0].TakeDamage(1000);
        PropRules::Initialize(a);
        assert(a.Props().size()==original&&a.Props()[0].TargetId()==identity);
    }
    for(auto type:{RoomType::Lobby,RoomType::Forge,RoomType::Shop,RoomType::Chest,RoomType::Boss,RoomType::Puzzle}) {
        auto r=MakeRoom(12,12,type);PropRules::Initialize(r);assert(r.Props().empty());
    }
    for(auto biome:{BiomeType::Cave,BiomeType::Mansion,BiomeType::Lobby}) {
        auto r=MakeRoom(12,12,RoomType::Normal,biome);PropRules::Initialize(r);assert(r.Props().empty());
    }
    auto tiny=MakeRoom(1,1);PropRules::Initialize(tiny);assert(tiny.Props().empty());
    auto room=MakeRoom();
    assert(!PropRules::PlacementAllowed(room,{0,2,2,1})); // North entry.
    assert(!PropRules::PlacementAllowed(room,{-4,3,1,1}));
    std::vector<PropDefinition> custom=GetPropDefinitions();
    custom.resize(1);custom[0].biomes={BiomeType::Cave,BiomeType::Mansion};
    auto cave=MakeRoom(10,10,RoomType::Normal,BiomeType::Cave);PropRules::Initialize(cave,custom);
    assert(!cave.Props().empty());
}

void CheckMovementAndDamage() {
    auto room=MakeRoom(8,8);room.Layout().tileBounds={0,0,8,8};
    auto vase=GetPropDefinitions()[0];vase.damagePerSecond=7.5f;
    room.Props().push_back({&vase,{3,2,1,3},10,false});
    assert(!PropRules::IsFree(room,{192,128,64,64}));
    Vector2 from{100,220},goal{350,220};
    auto blocked=PropRules::ResolveMovement(room,from,goal,26,21);
    assert(blocked.x<=166&&blocked.y==from.y); // Swept movement cannot tunnel.
    Vector2 p=from;
    for(int i=0;i<600&&std::hypot(p.x-goal.x,p.y-goal.y)>1;++i) {
        auto waypoint=PropRules::NavigationTarget(room,p,goal,26,21);
        float dx=waypoint.x-p.x,dy=waypoint.y-p.y,d=std::hypot(dx,dy);
        float fraction=d>0?std::min(1.0f,3.0f/d):0;
        p=PropRules::ResolveMovement(room,p,{p.x+dx*fraction,p.y+dy*fraction},26,21);
        assert(PropRules::IsFree(room,PropRules::EntityBounds(p,26,21)));
    }
    assert(std::hypot(p.x-goal.x,p.y-goal.y)<1);
    float sixty=0,hundred=0;
    for(int i=0;i<60;++i) sixty+=PropRules::ContactDamage(room,{180,170,30,30},{150,170,30,30},1.0f/60);
    for(int i=0;i<100;++i) hundred+=PropRules::ContactDamage(room,{180,170,30,30},{150,170,30,30},0.01f);
    assert(std::abs(sixty-7.5f)<0.001f&&std::abs(sixty-hundred)<0.001f);
    vase.solid=false;
    assert(PropRules::ContactDamage(room,{192,170,30,30},{150,170,30,30},1)==0);
    assert(PropRules::ContactDamage(room,{150,170,30,30},{192,170,30,30},1)==7.5f);
    vase.solid=true;
    assert(room.Props()[0].TakeDamage(10));
    assert(!room.Props()[0].TakeDamage(10));
    assert(PropRules::IsFree(room,{192,128,64,64}));
    auto direct=PropRules::NavigationTarget(room,from,goal,26,21);
    assert(direct.x==goal.x&&direct.y==goal.y);
    assert(PropRules::ContactDamage(room,{192,170,30,30},{192,170,30,30},1)==0);
}

void CheckBottomEdgeNavigation() {
    for(auto offset:{RoomCoords{0,0},RoomCoords{-13,-12}}) {
        for(float radius:{22.0f,24.0f,26.0f}) {
            auto room=MakeRoom(10,10);
            room.Layout().tileBounds={offset.x,offset.y,10,10};
            const float hw=radius,hh=radius*0.8f;
            const auto floor=RoomCollisionRules::RoomBoundsForCollision(room.Layout().tileBounds,room.GetBiome());
            const Vector2 from{floor.x+100,floor.y+220};
            const Vector2 goal{floor.x+500,floor.y+floor.height-16};
            auto direct=PropRules::NavigationTarget(room,from,goal,hw,hh);
            assert(direct.x==goal.x&&direct.y==goal.y);

            room.Props().push_back({&GetPropDefinitions()[2],{offset.x+3,offset.y+2,1,6},0,false});
            auto first=PropRules::NavigationTarget(room,from,goal,hw,hh);
            assert(std::hypot(first.x-from.x,first.y-from.y)>1);
            Vector2 p=from;
            for(int frame=0;frame<1000&&std::hypot(p.x-goal.x,p.y-goal.y)>1;++frame) {
                auto waypoint=PropRules::NavigationTarget(room,p,goal,hw,hh);
                if(waypoint.x!=goal.x||waypoint.y!=goal.y) {
                    auto bounds=PropRules::EntityBounds(waypoint,hw,hh);
                    assert(bounds.x>=floor.x-0.001f&&bounds.y>=floor.y-0.001f);
                    assert(bounds.x+bounds.width<=floor.x+floor.width+0.001f);
                    assert(bounds.y+bounds.height<=floor.y+floor.height+0.001f);
                }
                float dx=waypoint.x-p.x,dy=waypoint.y-p.y,d=std::hypot(dx,dy);
                float fraction=d>0?std::min(1.0f,3.0f/d):0;
                p=PropRules::ResolveMovement(room,p,{p.x+dx*fraction,p.y+dy*fraction},hw,hh);
                assert(PropRules::IsFree(room,PropRules::EntityBounds(p,hw,hh)));
            }
            assert(std::hypot(p.x-goal.x,p.y-goal.y)<1);

            // A wall spanning the room still makes the destination unreachable.
            room.Props()[0].tiles={offset.x+3,offset.y,1,10};
            auto unreachable=PropRules::NavigationTarget(room,from,goal,hw,hh);
            assert(unreachable.x==from.x&&unreachable.y==from.y);
            room.Props()[0].destroyed=true;
            direct=PropRules::NavigationTarget(room,from,goal,hw,hh);
            assert(direct.x==goal.x&&direct.y==goal.y);
        }
    }
}

void CheckRunLifecycle() {
    RoomManager first(19),second(19);
    for(const auto& entry:first.Rooms()) {
        const auto& room=*entry.second;
        const auto* copy=second.TryGetRoom(entry.first);assert(copy);
        assert(room.Props().size()==copy->Props().size());
        if(room.GetType()!=RoomType::Normal||room.GetBiome()!=BiomeType::Dungeon) assert(room.Props().empty());
    }
    // RoomManager owns the same instances when rediscovering rooms.
    for(const auto& entry:first.Rooms()) {
        auto& room=first.GetRoom(entry.first);
        for(auto& prop:room.Props()) if(prop.definition->destructible) {
            auto id=prop.TargetId();prop.TakeDamage(100);
            first.EnsureNeighborsGenerated(entry.first,0);
            assert(prop.TargetId()==id&&prop.destroyed);
            const auto* fresh=second.TryGetRoom(entry.first);
            for(const auto& original:fresh->Props()) assert(!original.destroyed);
            return;
        }
    }
}
}
int main() {
    CheckGeneration();CheckMovementAndDamage();CheckBottomEdgeNavigation();CheckRunLifecycle();
    std::cout<<"Prop rules tests passed\n";
}
