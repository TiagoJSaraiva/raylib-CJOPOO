#include "prop_renderer.h"
#include "rlgl.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>

namespace {
namespace fs=std::filesystem;
struct TemporaryAssets {
    fs::path original=fs::current_path();
    fs::path root=fs::temp_directory_path()/("endless-prop-render-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    TemporaryAssets() { fs::create_directories(root/"assets/img/props");fs::current_path(root); }
    ~TemporaryAssets() { fs::current_path(original);fs::remove_all(root); }
};
RenderTexture2D target{};
Image Capture() {
    rlDrawRenderBatchActive();
    EndTextureMode();
    Image image=LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    BeginTextureMode(target);
    return image;
}
bool Near(int value,int expected,int tolerance=3) { return std::abs(value-expected)<=tolerance; }
Room MakeRoom(BiomeType biome=BiomeType::Dungeon) {
    RoomLayout layout;layout.widthTiles=8;layout.heightTiles=8;layout.tileBounds={0,0,8,8};
    return Room({0,0},{RoomType::Normal,biome,1},layout);
}
void CheckSprites() {
    TemporaryAssets assets;
    Image sprite=GenImageColor(20,40,BLANK);
    ImageDrawRectangle(&sprite,4,0,12,20,RED);ImageDrawRectangle(&sprite,4,20,12,20,BLUE);
    assert(ExportImage(sprite,"assets/img/props/test-static.png"));UnloadImage(sprite);
    Image sheet=GenImageColor(16,32,RED);ImageDrawRectangle(&sheet,0,16,16,16,GREEN);
    assert(ExportImage(sheet,"assets/img/props/test-animated.png"));UnloadImage(sheet);
    sheet=GenImageColor(32,16,RED);ImageDrawRectangle(&sheet,16,0,16,16,GREEN);
    assert(ExportImage(sheet,"assets/img/props/test-horizontal.png"));UnloadImage(sheet);
    PropDefinition def;def.id="test-static";
    PropInstance prop{&def,{2,4,2,1},0,false};
    PropRenderer renderer;
    auto geometry=PropRenderer::Geometry(prop,20,40,0);
    assert(geometry.destination.width==128&&geometry.destination.height==256&&geometry.destination.y==64);
    BeginTextureMode(target);ClearBackground(GRAY);renderer.Draw(prop,1,0);
    Image image=Capture();EndTextureMode();
    auto top=GetImageColor(image,190,70),bottom=GetImageColor(image,190,310),margin=GetImageColor(image,130,200);
    assert(top.r>200&&top.b<70&&bottom.b>150&&bottom.r<60);
    assert(Near(margin.r,GRAY.r));UnloadImage(image);
    def.id="test-animated";def.animation=PropAnimation{16,16,2,0.1f,true};
    prop.tiles={1,2,1,1};
    for(int i=0;i<2;++i) {
        BeginTextureMode(target);ClearBackground(BLACK);renderer.Draw(prop,1,i*0.1f);
        image=Capture();EndTextureMode();Color pixel=GetImageColor(image,90,150);
        assert(i==0?pixel.r>200:pixel.g>200);UnloadImage(image);
    }
    def.id="test-horizontal";def.animation->vertical=false;
    BeginTextureMode(target);ClearBackground(BLACK);renderer.Draw(prop,1,0.1f);
    image=Capture();EndTextureMode();assert(GetImageColor(image,90,150).g>200);UnloadImage(image);
    BeginTextureMode(target);ClearBackground(GRAY);prop.destroyed=true;renderer.Draw(prop,1,0);
    image=Capture();EndTextureMode();assert(Near(GetImageColor(image,90,150).r,GRAY.r));UnloadImage(image);
    prop.destroyed=false;
    BeginTextureMode(target);ClearBackground(BLACK);
    std::vector<SceneActor> actors{{200,[]{DrawRectangle(0,0,100,100,BLUE);}},{100,[]{DrawRectangle(0,0,100,100,RED);}}};
    DrawDepthSorted(actors);image=Capture();EndTextureMode();assert(GetImageColor(image,50,50).b>150);UnloadImage(image);
    renderer.Unload();renderer.Unload();
}

void CheckLighting() {
    auto room=MakeRoom();PropRenderer renderer;SceneLighting lighting;
    Camera2D camera{};camera.zoom=1;
    BeginTextureMode(target);ClearBackground(WHITE);DrawRectangle(100,100,40,40,RED);
    lighting.Apply({{&room,1}},camera,renderer,0,&target);
    Image first=Capture();EndTextureMode();
    assert(Near(GetImageColor(first,110,110).r,int(RED.r*0.65f)));
    assert(GetImageColor(first,110,110).g<30);UnloadImage(first);
    for(auto biome:{BiomeType::Dungeon,BiomeType::Cave,BiomeType::Mansion,BiomeType::Lobby}) {
        auto sample=MakeRoom(biome);
        BeginTextureMode(target);ClearBackground(WHITE);lighting.Apply({{&sample,1}},camera,renderer,0,&target);
        Image image=Capture();EndTextureMode();
        assert(Near(GetImageColor(image,200,200).r,int(255*SceneLighting::Ambient(biome))));UnloadImage(image);
    }
    const auto& barrel=GetPropDefinitions()[1];
    room.Props().push_back({&barrel,{3,3,1,1},0,false});
    Vector2 source=renderer.LightPosition(room.Props()[0],0);
    BeginTextureMode(target);ClearBackground(WHITE);lighting.Apply({{&room,1}},camera,renderer,0,&target);
    Image image=Capture();EndTextureMode();
    auto near=GetImageColor(image,int(source.x),int(source.y));auto far=GetImageColor(image,10,480);
    assert(near.r>far.r+30&&near.r>=near.g&&near.g>near.b);assert(Near(far.r,166));UnloadImage(image);
    room.Props().push_back(room.Props()[0]);
    BeginTextureMode(target);ClearBackground(WHITE);lighting.Apply({{&room,1}},camera,renderer,0,&target);
    image=Capture();EndTextureMode();auto doubled=GetImageColor(image,int(source.x),int(source.y));
    assert(doubled.g>near.g&&doubled.r==255);UnloadImage(image);room.Props().pop_back();
    // Hidden rooms have no effect. Partial reveal reduces both darkness and emitted light.
    BeginTextureMode(target);ClearBackground(WHITE);lighting.Apply({{&room,0}},camera,renderer,0,&target);
    image=Capture();EndTextureMode();assert(GetImageColor(image,200,200).r==255);UnloadImage(image);
    BeginTextureMode(target);ClearBackground(WHITE);lighting.Apply({{&room,0.5f}},camera,renderer,0,&target);
    image=Capture();EndTextureMode();assert(Near(GetImageColor(image,10,480).r,210));UnloadImage(image);
    BeginTextureMode(target);ClearBackground(WHITE);DrawRectangle(0,0,40,512,BLACK);
    lighting.Apply({{&room,1}},camera,renderer,0,&target);DrawRectangle(0,0,10,10,RED);
    image=Capture();EndTextureMode();assert(GetImageColor(image,20,200).r==0);assert(GetImageColor(image,5,5).r==RED.r);UnloadImage(image);
    // Zoomed source near the boundary must not illuminate the neighboring room.
    auto neighbor=MakeRoom();neighbor.Layout().tileBounds.x=8;
    room.Props()[0].tiles={7,3,1,1};camera.zoom=0.5f;
    BeginTextureMode(target);ClearBackground(WHITE);lighting.Apply({{&room,1},{&neighbor,1}},camera,renderer,0,&target);
    image=Capture();EndTextureMode();assert(Near(GetImageColor(image,270,100).r,166));UnloadImage(image);
    room.Props()[0].tiles={3,3,1,1};camera.zoom=1;
    room.Props()[0].destroyed=true;
    BeginTextureMode(target);ClearBackground(WHITE);lighting.Apply({{&room,1}},camera,renderer,0,&target);
    image=Capture();EndTextureMode();assert(Near(GetImageColor(image,int(source.x),int(source.y)).r,166));UnloadImage(image);
    // Reallocate the map when rendering to a different output size.
    auto original=target;target=LoadRenderTexture(128,128);
    BeginTextureMode(target);ClearBackground(WHITE);lighting.Apply({{&room,1}},camera,renderer,0,&target);
    image=Capture();EndTextureMode();assert(Near(GetImageColor(image,100,100).r,166));UnloadImage(image);
    UnloadRenderTexture(target);target=original;
    lighting.Unload();lighting.Unload();renderer.Unload();
}

void Preview() {
    auto room=MakeRoom();PropRenderer renderer;SceneLighting lighting;
    const auto& defs=GetPropDefinitions();
    room.Props()={{&defs[0],{1,2,1,1},10,false},{&defs[1],{3,3,1,1},0,false},
                  {&defs[2],{6,2,1,1},0,false},{&defs[3],{3,5,2,1},0,false}};
    BeginTextureMode(target);ClearBackground({55,52,57,255});
    for(int y=0;y<8;++y) for(int x=0;x<8;++x)
        DrawRectangle(x*64,y*64,63,63,{82,75,78,255});
    for(const auto& p:room.Props()) renderer.Draw(p,1,0);
    Camera2D camera{};camera.zoom=1;
    lighting.Apply({{&room,1}},camera,renderer,0,&target);
    Image image=Capture();EndTextureMode();
    fs::create_directories("obj");assert(ExportImage(image,"obj/props-preview.png"));UnloadImage(image);
    renderer.Unload();lighting.Unload();
}
}
int main() {
    SetTraceLogLevel(LOG_WARNING);SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(512,512,"Props rendering test");assert(IsWindowReady());
    target=LoadRenderTexture(512,512);
    CheckSprites();CheckLighting();Preview();
    UnloadRenderTexture(target);CloseWindow();
    std::cout<<"Prop render tests passed; preview: obj/props-preview.png\n";
}
