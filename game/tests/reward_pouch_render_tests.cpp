#include "reward_pouch_renderer.h"
#include "prop_renderer.h"
#include "rlgl.h"

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace {
namespace fs = std::filesystem;
RenderTexture2D target{};

struct TemporaryAssets {
    fs::path original = fs::current_path();
    fs::path temp = fs::temp_directory_path();
    fs::path root = temp / ("endless-pouch-render-" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    TemporaryAssets() {
        fs::create_directories(root / "assets/img/reward-pouch");
        fs::current_path(root);
    }
    ~TemporaryAssets() {
        fs::current_path(original);
        // Only remove this test's explicitly named temporary directory.
        if (root.is_absolute() && root.parent_path() == temp &&
            root.filename().string().find("endless-pouch-render-") == 0) fs::remove_all(root);
    }
};

Image Capture() {
    rlDrawRenderBatchActive();
    EndTextureMode();
    Image image = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&image);
    return image;
}

bool Near(int actual, int expected) { return std::abs(actual - expected) <= 3; }

void CheckRendering() {
    TemporaryAssets assets;
    Image sprite = GenImageColor(20, 40, BLANK);
    ImageDrawRectangle(&sprite, 4, 0, 12, 40, RED);
    assert(ExportImage(sprite, "assets/img/reward-pouch/small.png"));
    UnloadImage(sprite);

    RewardPouchRenderer renderer;
    RewardPouchInstance pouch{RewardPouchType::Small, {100, 100}, 12, {}};
    Rectangle hitbox = pouch.Hitbox();
    assert(hitbox.x == 88 && hitbox.y == 92 && hitbox.width == 24 && hitbox.height == 16);
    Rectangle dest = RewardPouchRenderer::Destination(pouch, 20, 40);
    assert(dest.x == 84 && dest.y == 44 && dest.width == 32 && dest.height == 64);
    assert(dest.y + dest.height == pouch.Depth());
    BeginTextureMode(target);
    ClearBackground(BLUE);
    renderer.Draw(pouch, 1);
    Image image = Capture();
    Color center = GetImageColor(image, 100, 80);
    Color transparentMargin = GetImageColor(image, 85, 80);
    assert(center.r > 200 && center.b < 70);
    assert(Near(transparentMargin.r, BLUE.r) && Near(transparentMargin.b, BLUE.b));
    assert(Near(GetImageColor(image, 100, 110).b, BLUE.b));
    UnloadImage(image);

    for (float visibility : {0.0f, 0.5f}) {
        BeginTextureMode(target);
        ClearBackground(BLACK);
        renderer.Draw(pouch, visibility);
        image = Capture();
        assert(Near(GetImageColor(image, 100, 80).r, int(RED.r * visibility)));
        UnloadImage(image);
    }
    // A partially collected pouch remains visible; an empty one does not.
    pouch.coins = 0;
    pouch.items = {{101, 1}};
    BeginTextureMode(target); ClearBackground(BLACK); renderer.Draw(pouch, 1);
    image = Capture(); assert(GetImageColor(image, 100, 80).r > 200); UnloadImage(image);
    pouch.items.clear();
    BeginTextureMode(target); ClearBackground(BLACK); renderer.Draw(pouch, 1);
    image = Capture(); assert(GetImageColor(image, 100, 80).r == 0); UnloadImage(image);
    pouch.coins = 12;

    // Draw through the same actor queue as characters/props.
    for (bool playerInFront : {true, false}) {
        BeginTextureMode(target); ClearBackground(BLACK);
        std::vector<SceneActor> actors{
            {pouch.Depth(), [&] { renderer.Draw(pouch, 1); }},
            {playerInFront ? 120.0f : 60.0f, [] { DrawRectangle(90, 70, 20, 20, BLUE); }}
        };
        DrawDepthSorted(actors);
        image = Capture();
        Color pixel = GetImageColor(image, 100, 80);
        assert(playerInFront ? pixel.b > 200 : pixel.r > 200);
        UnloadImage(image);
    }

    RoomLayout layout;
    layout.widthTiles = layout.heightTiles = 4;
    layout.tileBounds = {0, 0, 4, 4};
    Room room({0, 0}, {RoomType::Normal, BiomeType::Dungeon, 1}, layout);
    PropRenderer props;
    SceneLighting lighting;
    Camera2D camera{};
    camera.zoom = 1;
    BeginTextureMode(target); ClearBackground(BLACK); renderer.Draw(pouch, 1);
    lighting.Apply({{&room, 1}}, camera, props, 0, &target);
    image = Capture();
    assert(Near(GetImageColor(image, 100, 80).r, int(RED.r * SceneLighting::Ambient(BiomeType::Dungeon))));
    UnloadImage(image);
    lighting.Unload();
    props.Unload();

    // Cached textures survive changes on disk until explicitly unloaded.
    assert(fs::remove("assets/img/reward-pouch/small.png"));
    BeginTextureMode(target); ClearBackground(BLACK); renderer.Draw(pouch, 1);
    image = Capture(); assert(GetImageColor(image, 100, 80).r > 200); UnloadImage(image);
    renderer.Unload();
    renderer.Unload();
    BeginTextureMode(target); ClearBackground(BLACK); renderer.Draw(pouch, 1);
    image = Capture();
    auto fallback = GetImageColor(image, 100, 100);
    assert(fallback.r > 200 && fallback.b > 100);
    UnloadImage(image);
    renderer.Unload();
}

void PreviewRealAssets() {
    RewardPouchRenderer renderer;
    BeginTextureMode(target);
    ClearBackground({45, 45, 52, 255});
    int x = 48;
    for (auto type : {RewardPouchType::Small, RewardPouchType::Medium, RewardPouchType::Big}) {
        const auto& definition = GetRewardPouchDefinition(type);
        std::string path = "assets/img/reward-pouch/" + definition.id + ".png";
        Image asset = LoadImage(path.c_str());
        assert(asset.data && asset.width > 0 && asset.height > 0);
        UnloadImage(asset);
        renderer.Draw({type, {float(x), 100}, 12, {}}, 1);
        DrawText(definition.id.c_str(), x - 24, 128, 12, WHITE);
        x += 80;
    }
    Image image = Capture();
    fs::create_directories("obj");
    assert(ExportImage(image, "obj/reward-pouches-preview.png"));
    UnloadImage(image);
    renderer.Unload();
}
}

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(256, 256, "Reward pouch rendering test");
    assert(IsWindowReady());
    target = LoadRenderTexture(256, 256);
    assert(target.id);
    CheckRendering();
    PreviewRealAssets();
    UnloadRenderTexture(target);
    CloseWindow();
    std::cout << "Reward pouch render tests passed; preview: obj/reward-pouches-preview.png\n";
}
