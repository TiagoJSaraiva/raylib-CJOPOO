#include "combat_geometry.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float infinity=std::numeric_limits<float>::infinity();
float CircleSweep(Vector2 a,Vector2 b,Vector2 c,float r) {
    float dx=b.x-a.x,dy=b.y-a.y,ox=a.x-c.x,oy=a.y-c.y;
    float cc=ox*ox+oy*oy-r*r;
    if(cc<=0) return 0;
    float aa=dx*dx+dy*dy;
    if(aa<1e-10f) return infinity;
    float bb=ox*dx+oy*dy,disc=bb*bb-aa*cc;
    if(disc<0) return infinity;
    float t=(-bb-std::sqrt(disc))/aa;
    return t>=0&&t<=1?t:infinity;
}
float RectSweep(Vector2 a,Vector2 b,Rectangle r) {
    float lo=0,hi=1;
    auto axis=[&](float start,float delta,float min,float max) {
        if(std::abs(delta)<1e-8f) return start>=min&&start<=max;
        float t1=(min-start)/delta,t2=(max-start)/delta;
        if(t1>t2) std::swap(t1,t2);
        lo=std::max(lo,t1);hi=std::min(hi,t2);
        return lo<=hi;
    };
    if(!axis(a.x,b.x-a.x,r.x,r.x+r.width)||!axis(a.y,b.y-a.y,r.y,r.y+r.height)) return infinity;
    return lo;
}
}
namespace CombatGeometry {
float Sweep(Vector2 a,Vector2 b,float radius,const ProjectileTarget& t) {
    radius=std::max(0.0f,radius);
    if(!t.rectangle) return CircleSweep(a,b,t.center,radius+t.radius);
    Rectangle r=*t.rectangle;
    // Rounded rectangle = two expanded strips plus four corner circles.
    float result=std::min(RectSweep(a,b,{r.x-radius,r.y,r.width+2*radius,r.height}),
                          RectSweep(a,b,{r.x,r.y-radius,r.width,r.height+2*radius}));
    for(float x:{r.x,r.x+r.width}) for(float y:{r.y,r.y+r.height})
        result=std::min(result,CircleSweep(a,b,{x,y},radius));
    return result;
}
bool Segment(const ProjectileTarget& t,Vector2 a,Vector2 b,float radius) {
    return std::isfinite(Sweep(a,b,radius,t));
}
bool Box(const ProjectileTarget& t,Vector2 c,Vector2 f,float hl,float ht) {
    Vector2 right{-f.y,f.x};
    if(!t.rectangle) {
        float x=t.center.x-c.x,y=t.center.y-c.y;
        return std::abs(x*f.x+y*f.y)<=hl+t.radius&&std::abs(x*right.x+y*right.y)<=ht+t.radius;
    }
    auto r=*t.rectangle;
    float dx=r.x+r.width*0.5f-c.x,dy=r.y+r.height*0.5f-c.y;
    for(Vector2 axis:{Vector2{1,0},Vector2{0,1},f,right}) {
        float distance=std::abs(dx*axis.x+dy*axis.y);
        float boxRadius=hl*std::abs(f.x*axis.x+f.y*axis.y)+ht*std::abs(right.x*axis.x+right.y*axis.y);
        float rectRadius=r.width*0.5f*std::abs(axis.x)+r.height*0.5f*std::abs(axis.y);
        if(distance>boxRadius+rectRadius) return false;
    }
    return true;
}
}
