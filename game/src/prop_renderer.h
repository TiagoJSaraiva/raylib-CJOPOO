#pragma once
#include "floor_tiles.h"
#include <unordered_map>
#include <functional>

struct PropSpriteGeometry { Rectangle source; Rectangle destination; };
struct SceneActor { float depth; std::function<void()> draw; };
void DrawDepthSorted(std::vector<SceneActor>& actors);

class PropRenderer {
public:
    ~PropRenderer() { Unload(); }
    PropRenderer() = default;
    PropRenderer(const PropRenderer&) = delete;
    PropRenderer& operator=(const PropRenderer&) = delete;
    void Draw(const PropInstance& prop, float visibility, float elapsedSeconds);
    Vector2 LightPosition(const PropInstance& prop, float elapsedSeconds);
    static PropSpriteGeometry Geometry(const PropInstance& prop, int width, int height, float elapsedSeconds);
    void Unload();
private:
    Texture2D Acquire(const PropDefinition& definition);
    std::unordered_map<std::string, Texture2D> textures_;
};

class SceneLighting {
public:
    ~SceneLighting() { Unload(); }
    SceneLighting() = default;
    SceneLighting(const SceneLighting&) = delete;
    SceneLighting& operator=(const SceneLighting&) = delete;
    static float Ambient(BiomeType biome);
    // Called in screen space, after world drawing and before prompts/HUD.
    void Apply(const std::vector<FloorRoomView>& rooms, Camera2D camera, PropRenderer& props, float elapsedSeconds, const RenderTexture2D* output = nullptr);
    void Unload();
private:
    RenderTexture2D map_{};
    Texture2D gradient_{};
};
