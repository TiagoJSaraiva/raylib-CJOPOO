#include "prop_renderer.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <map>

void DrawDepthSorted(std::vector<SceneActor>& actors) {
    std::stable_sort(actors.begin(), actors.end(), [](const SceneActor& a, const SceneActor& b) { return a.depth < b.depth; });
    for (const auto& actor : actors) actor.draw();
}

Texture2D PropRenderer::Acquire(const PropDefinition& def) {
    auto it=textures_.find(def.id);
    if(it!=textures_.end()) return it->second;
    std::string path="assets/img/props/"+def.id+".png";
    Texture2D texture{};
    if(FileExists(path.c_str())) texture=LoadTexture(path.c_str());
    if(texture.id) SetTextureFilter(texture,TEXTURE_FILTER_BILINEAR);
    else TraceLog(LOG_WARNING,"[Props] Cannot load %s",path.c_str());
    textures_[def.id]=texture;
    return texture;
}

PropSpriteGeometry PropRenderer::Geometry(const PropInstance& p,int width,int height,float time) {
    Rectangle src{0,0,float(width),float(height)};
    if(p.definition->animation) {
        const auto& clip=*p.definition->animation;
        int capacity=clip.frameWidth>0&&clip.frameHeight>0 ?
            (clip.vertical ? height/clip.frameHeight : width/clip.frameWidth) : 0;
        if(capacity>0&&clip.frameCount>0&&clip.frameWidth<=width&&clip.frameHeight<=height) {
            int count=std::min(clip.frameCount,capacity);
            int frame=clip.secondsPerFrame>0 ? int(std::floor(std::max(0.0f,time)/clip.secondsPerFrame))%count : 0;
            src={float(clip.vertical?0:frame*clip.frameWidth),float(clip.vertical?frame*clip.frameHeight:0),
                 float(clip.frameWidth),float(clip.frameHeight)};
        }
    }
    Rectangle dest=p.Hitbox();
    dest.height=src.width>0 ? dest.width*src.height/src.width : dest.height;
    dest.y=p.Depth()-dest.height;
    return {src,dest};
}

void PropRenderer::Draw(const PropInstance& p,float visibility,float time) {
    if(p.destroyed||visibility<=0) return;
    Texture2D texture=Acquire(*p.definition);
    if(!texture.id) {
        DrawRectangleRec(p.Hitbox(),Fade(MAGENTA,visibility));
        return;
    }
    auto g=Geometry(p,texture.width,texture.height,time);
    DrawTexturePro(texture,g.source,g.destination,{0,0},0,Fade(WHITE,visibility));
}

Vector2 PropRenderer::LightPosition(const PropInstance& p,float time) {
    Texture2D texture=Acquire(*p.definition);
    auto g=Geometry(p,texture.width>0?texture.width:64,texture.height>0?texture.height:64,time);
    Vector2 offset=p.definition->light->spritePosition;
    return {g.destination.x+g.destination.width*offset.x,g.destination.y+g.destination.height*offset.y};
}

void PropRenderer::Unload() {
    for(auto& pair:textures_) if(pair.second.id) UnloadTexture(pair.second);
    textures_.clear();
}

float SceneLighting::Ambient(BiomeType biome) {
    switch(biome) {
        case BiomeType::Dungeon:return 0.65f;
        case BiomeType::Cave:return 0.75f;
        case BiomeType::Mansion:return 0.85f;
        default:return 1.0f;
    }
}

void SceneLighting::Apply(const std::vector<FloorRoomView>& rooms,Camera2D camera,PropRenderer& props,float time,const RenderTexture2D* output) {
    // Resource allocation may bind a framebuffer; finish the world pass first.
    rlDrawRenderBatchActive();
    int width=output?output->texture.width:GetScreenWidth();
    int height=output?output->texture.height:GetScreenHeight();
    if(width<=0||height<=0) return;
    if(map_.id&&(map_.texture.width!=width||map_.texture.height!=height)) { UnloadRenderTexture(map_);map_={}; }
    if(!map_.id) map_=LoadRenderTexture(width,height);
    if(!map_.id) return;
    if(!gradient_.id) {
        Image img=GenImageColor(128,128,BLANK);
        auto* pixels=static_cast<Color*>(img.data);
        for(int y=0;y<128;++y) for(int x=0;x<128;++x) {
            float distance=std::hypot((x-63.5f)/63.5f,(y-63.5f)/63.5f);
            float alpha=std::max(0.0f,1-distance);alpha*=alpha;
            pixels[y*128+x]={255,255,255,static_cast<unsigned char>(255*alpha)};
        }
        gradient_=LoadTextureFromImage(img);UnloadImage(img);
        SetTextureFilter(gradient_,TEXTURE_FILTER_BILINEAR);
    }
    // A tile union prevents overlapping room/corridor passes from darkening twice.
    FloorRules::Frame frame;
    std::unordered_map<RoomCoords,const Room*,RoomCoordsHash> lookup;
    for(const auto& view:rooms) lookup[view.room->GetCoords()]=view.room;
    for(const auto& view:rooms) {
        const auto& room=*view.room;
        auto bounds=room.Layout().tileBounds;
        // Include wall sprites above and below the floor.
        TileRect walls{bounds.x-1,bounds.y-2,bounds.width+2,bounds.height+3};
        FloorRules::AddRect(frame,walls,room.GetBiome(),view.visibility);
        for(const auto& door:room.Layout().doors) {
            if(door.sealed) continue;
            auto biome=room.GetBiome();
            auto other=lookup.find(door.targetCoords);
            if(other!=lookup.end()) biome=FloorRules::CorridorBiome(room.GetCoords(),biome,other->first,other->second->GetBiome());
            FloorRules::AddRect(frame,door.corridorTiles,biome,view.visibility);
        }
    }
    BeginTextureMode(map_);
    ClearBackground(WHITE);
    BeginMode2D(camera);
    for(const auto& entry:frame) {
        float level=1-(1-Ambient(entry.second.biome))*entry.second.visibility;
        unsigned char value=static_cast<unsigned char>(std::round(255*level));
        DrawRectangle(entry.first.x*TILE_SIZE,entry.first.y*TILE_SIZE,TILE_SIZE,TILE_SIZE,{value,value,value,255});
    }
    EndMode2D();
    // Clip each source to its room, so unrevealed neighbors never receive leaked light.
    BeginBlendMode(BLEND_ADDITIVE);
    for(const auto& view:rooms) {
        if(view.visibility<=0) continue;
        auto bounds=view.room->Layout().tileBounds;
        Vector2 top=GetWorldToScreen2D({float(bounds.x*TILE_SIZE),float((bounds.y-2)*TILE_SIZE)},camera);
        Vector2 bottom=GetWorldToScreen2D({float((bounds.x+bounds.width)*TILE_SIZE),float((bounds.y+bounds.height+1)*TILE_SIZE)},camera);
        BeginScissorMode(int(std::floor(top.x)),int(std::floor(top.y)),int(std::ceil(bottom.x-top.x)),int(std::ceil(bottom.y-top.y)));
        for(const auto& p:view.room->Props()) {
            if(p.destroyed||!p.definition->light) continue;
            const auto& light=*p.definition->light;
            Vector2 center=GetWorldToScreen2D(props.LightPosition(p,time),camera);
            float radius=std::max(0.0f,light.radius*camera.zoom);
            Color tint=light.color;
            float intensity=std::clamp(light.intensity*view.visibility,0.0f,1.0f);
            tint.r=static_cast<unsigned char>(tint.r*intensity);
            tint.g=static_cast<unsigned char>(tint.g*intensity);
            tint.b=static_cast<unsigned char>(tint.b*intensity);
            DrawTexturePro(gradient_,{0,0,128,128},{center.x-radius,center.y-radius,radius*2,radius*2},{0,0},0,tint);
        }
        EndScissorMode();
    }
    EndBlendMode();
    EndTextureMode();
    if(output) BeginTextureMode(*output);
    BeginBlendMode(BLEND_MULTIPLIED);
    DrawTextureRec(map_.texture,{0,0,float(width),-float(height)},{0,0},WHITE);
    EndBlendMode();
}

void SceneLighting::Unload() {
    if(map_.id) UnloadRenderTexture(map_);
    if(gradient_.id) UnloadTexture(gradient_);
    map_={};gradient_={};
}
