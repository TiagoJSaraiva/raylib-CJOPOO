#include "prop_rules.h"
#include "room.h"
#include "room_collision_rules.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <queue>
#include <random>

namespace PropRules {
bool Overlaps(Rectangle a, Rectangle b) {
    return a.x < b.x + b.width && a.x + a.width > b.x &&
           a.y < b.y + b.height && a.y + a.height > b.y;
}
Rectangle EntityBounds(Vector2 p, float hw, float hh) { return {p.x-hw, p.y-hh, hw*2, hh*2}; }

bool IsFree(const Room& room, Rectangle bounds) {
    for (const auto& prop : room.Props())
        if (!prop.destroyed && prop.definition->solid && Overlaps(bounds, prop.Hitbox())) return false;
    return true;
}

namespace {
bool Inside(TileRect a, TileRect b) {
    return a.width > 0 && a.height > 0 && a.x >= b.x && a.y >= b.y &&
           a.x+a.width <= b.x+b.width && a.y+a.height <= b.y+b.height;
}
bool Blocked(const Room& room, int x, int y, const TileRect* extra) {
    TileRect tile{x,y,1,1};
    if (extra && Intersects(tile,*extra)) return true;
    for (const auto& p : room.Props())
        if (!p.destroyed && p.definition->solid && Intersects(tile,p.tiles)) return true;
    return false;
}
bool SegmentClear(const Room& room, Vector2 a, Vector2 b, float hw, float hh) {
    // Samples no farther than 4 pixels apart, smaller than the narrowest footprint.
    int count = std::max(1, int(std::ceil(std::hypot(b.x-a.x,b.y-a.y)/4.0f)));
    for(int i=0;i<=count;++i) {
        float t=float(i)/count;
        if(!IsFree(room,EntityBounds({a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t},hw,hh))) return false;
    }
    return true;
}
}

bool FloorConnected(const Room& room, const TileRect* candidate) {
    const auto b=room.Layout().tileBounds;
    if(b.width<=0 || b.height<=0) return false;
    std::vector<bool> seen(b.width*b.height,false);
    std::queue<int> pending;
    int available=0;
    for(int y=0;y<b.height;++y) for(int x=0;x<b.width;++x) {
        if(Blocked(room,b.x+x,b.y+y,candidate)) continue;
        ++available;
        if(pending.empty()) { int i=y*b.width+x; pending.push(i); seen[i]=true; }
    }
    int visited=0;
    while(!pending.empty()) {
        int i=pending.front(); pending.pop(); ++visited;
        int x=i%b.width, y=i/b.width;
        for(auto d : {RoomCoords{1,0},RoomCoords{-1,0},RoomCoords{0,1},RoomCoords{0,-1}}) {
            int nx=x+d.x,ny=y+d.y;
            if(nx<0||ny<0||nx>=b.width||ny>=b.height) continue;
            int n=ny*b.width+nx;
            if(!seen[n]&&!Blocked(room,b.x+nx,b.y+ny,candidate)) { seen[n]=true; pending.push(n); }
        }
    }
    return available>0 && visited==available;
}

bool PlacementAllowed(const Room& room, TileRect c) {
    auto b=room.Layout().tileBounds;
    if(!Inside(c,b)) return false;
    for(const auto& p:room.Props()) {
        TileRect spaced{p.tiles.x-1,p.tiles.y-1,p.tiles.width+2,p.tiles.height+2};
        if(!p.destroyed&&Intersects(c,spaced)) return false;
    }
    for(const auto& door:room.Layout().doors) {
        if(door.sealed) continue;
        TileRect entry;
        switch(door.direction) {
            case Direction::North: entry={b.x+door.offset-1,b.y,door.width+2,2}; break;
            case Direction::South: entry={b.x+door.offset-1,b.y+b.height-2,door.width+2,2}; break;
            case Direction::East: entry={b.x+b.width-2,b.y+door.offset-1,2,door.width+2}; break;
            case Direction::West: entry={b.x,b.y+door.offset-1,2,door.width+2}; break;
        }
        if(Intersects(c,entry)) return false;
    }
    return FloorConnected(room,&c);
}

void Initialize(Room& room, const std::vector<PropDefinition>& catalog) {
    if(room.PropsInitialized()) return;
    room.SetPropsInitialized();
    if(room.GetType()!=RoomType::Normal) return;
    auto b=room.Layout().tileBounds;
    int area=b.width*b.height;
    if(area<=0) return;
    std::mt19937_64 rng(HashCombine(room.GetSeed(),0x50524F5053ULL));
    int low=int(std::ceil(area*0.08)),high=int(std::floor(area*0.15));
    if(high<low) return;
    int remaining=std::uniform_int_distribution<int>(low,high)(rng);
    while(remaining>0) {
        struct Choices { const PropDefinition* def; std::vector<TileRect> places; std::vector<double> weights; };
        std::vector<Choices> choices;
        std::vector<double> typeWeights;
        for(const auto& def:catalog) {
            if(def.widthTiles<=0||def.heightTiles<=0||def.weight<=0||
               def.widthTiles*def.heightTiles>remaining||
               std::find(def.biomes.begin(),def.biomes.end(),room.GetBiome())==def.biomes.end()) continue;
            Choices options{&def,{}, {}};
            for(int y=b.y;y<=b.y+b.height-def.heightTiles;++y)
                for(int x=b.x;x<=b.x+b.width-def.widthTiles;++x) {
                    TileRect c{x,y,def.widthTiles,def.heightTiles};
                    if(!PlacementAllowed(room,c)) continue;
                    options.places.push_back(c);
                    float dx=std::min(x-b.x,b.x+b.width-x-c.width);
                    float dy=std::min(y-b.y,b.y+b.height-y-c.height);
                    float maxDistance=std::max(1.0f,std::min((b.width-c.width)*0.5f,(b.height-c.height)*0.5f));
                    float proximity=std::min(dx,dy)/maxDistance;
                    float difference=proximity-def.disposition.value_or(proximity);
                    options.weights.push_back(1.0/(0.025+difference*difference));
                }
            if(!options.places.empty()) { choices.push_back(std::move(options)); typeWeights.push_back(def.weight); }
        }
        if(choices.empty()) break;
        auto& choice=choices[std::discrete_distribution<int>(typeWeights.begin(),typeWeights.end())(rng)];
        auto place=choice.places[std::discrete_distribution<int>(choice.weights.begin(),choice.weights.end())(rng)];
        float hp=choice.def->destructible ? choice.def->destructible->maxHealth : 0.0f;
        room.Props().push_back({choice.def,place,hp,false});
        remaining-=place.width*place.height;
    }
}

Vector2 ResolveMovement(const Room& room, Vector2 from, Vector2 desired, float hw, float hh) {
    int steps=std::max(1,int(std::ceil(std::max(std::abs(desired.x-from.x),std::abs(desired.y-from.y))/8.0f)));
    Vector2 step{(desired.x-from.x)/steps,(desired.y-from.y)/steps};
    Vector2 p=from;
    for(int i=0;i<steps;++i) {
        Vector2 x{p.x+step.x,p.y};
        if(IsFree(room,EntityBounds(x,hw,hh))) p=x;
        Vector2 y{p.x,p.y+step.y};
        if(IsFree(room,EntityBounds(y,hw,hh))) p=y;
    }
    return p;
}

Vector2 NavigationTarget(const Room& room, Vector2 from, Vector2 target, float hw, float hh) {
    if(SegmentClear(room,from,target,hw,hh)) return target;
    const auto b=room.Layout().tileBounds;
    if(b.width<=0||b.height<=0) return from;
    auto cell=[&](Vector2 p) { return RoomCoords{std::clamp(int(std::floor(p.x/TILE_SIZE))-b.x,0,b.width-1),
                                             std::clamp(int(std::floor(p.y/TILE_SIZE))-b.y,0,b.height-1)}; };
    const Rectangle floor=RoomCollisionRules::RoomBoundsForCollision(b,room.GetBiome());
    std::vector<std::optional<Vector2>> waypoints(b.width*b.height);
    for(int i=0;i<b.width*b.height;++i) {
        const float x=float((b.x+i%b.width)*TILE_SIZE);
        const float y=float((b.y+i/b.width)*TILE_SIZE);
        // Intersect the tile with the allowed area for the entity's center.
        // Edge tiles can remain reachable even when their nominal centers do not fit.
        const float minX=std::max(x,floor.x+hw);
        const float maxX=std::min(x+TILE_SIZE,floor.x+floor.width-hw);
        const float minY=std::max(y,floor.y+hh);
        const float maxY=std::min(y+TILE_SIZE,floor.y+floor.height-hh);
        if(minX>maxX||minY>maxY) continue;
        waypoints[i]=Vector2{std::clamp(x+TILE_SIZE/2.0f,minX,maxX),
                             std::clamp(y+TILE_SIZE/2.0f,minY,maxY)};
    }
    auto start=cell(from),goal=cell(target);
    int s=start.y*b.width+start.x,g=goal.y*b.width+goal.x;
    std::vector<int> parent(b.width*b.height,-1);
    std::queue<int> queue;
    parent[s]=s; queue.push(s);
    while(!queue.empty()&&parent[g]<0) {
        int i=queue.front();queue.pop();
        for(auto d:{RoomCoords{1,0},RoomCoords{-1,0},RoomCoords{0,1},RoomCoords{0,-1}}) {
            int nx=i%b.width+d.x,ny=i/b.width+d.y;
            if(nx<0||ny<0||nx>=b.width||ny>=b.height) continue;
            int n=ny*b.width+nx;
            if(parent[n]>=0||!waypoints[n]) continue;
            Vector2 previous=i==s ? from : *waypoints[i];
            if(SegmentClear(room,previous,*waypoints[n],hw,hh)) { parent[n]=i; queue.push(n); }
        }
    }
    if(parent[g]<0) return from;
    int next=g;
    // Choose the farthest visible waypoint to avoid oscillation at tile boundaries.
    while(next!=s) {
        Vector2 waypoint=*waypoints[next];
        if(SegmentClear(room,from,waypoint,hw,hh)) return waypoint;
        next=parent[next];
    }
    return waypoints[s].value_or(from);
}

float ContactDamage(const Room& room, Rectangle attempted, Rectangle resolved, float seconds) {
    float dps=0;
    for(const auto& p:room.Props()) {
        if(p.destroyed) continue;
        if(Overlaps(p.definition->solid?attempted:resolved,p.Hitbox()))
            dps+=std::max(0.0f,p.definition->damagePerSecond);
    }
    return dps*std::max(0.0f,seconds);
}
}
