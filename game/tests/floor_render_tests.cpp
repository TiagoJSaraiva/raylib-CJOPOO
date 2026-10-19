#include "floor_tiles.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
namespace fs = std::filesystem;

// All generated assets stay in an isolated temporary working directory.
struct TemporaryAssets {
    fs::path original = fs::current_path();
    fs::path root = fs::temp_directory_path() /
        ("endless-rooms-floor-test-" + std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    TemporaryAssets() {
        for (const char* biome : {"lobby", "cave", "mansion", "dungeon"}) {
            fs::create_directories(root / "assets/floor" / biome);
        }
        fs::current_path(root);
    }
    ~TemporaryAssets() {
        fs::current_path(original);
        fs::remove_all(root);
    }
};

void Sprite(const char* path, int width, int height, Color color) {
    Image image = GenImageColor(width, height, color);
    assert(ExportImage(image, path));
    UnloadImage(image);
}

Room MakeRoom(RoomCoords coords, BiomeType biome, int tileX) {
    RoomLayout layout;
    layout.widthTiles = layout.heightTiles = 1;
    layout.tileBounds = TileRect{tileX, 0, 1, 1};
    return Room(coords, RoomSeedData{RoomType::Normal, biome, 1}, layout);
}

Image Render(FloorTiles& floors, const std::vector<FloorRoomView>& views, RenderTexture2D target) {
    BeginTextureMode(target);
    ClearBackground(BLACK);
    floors.Draw(views);
    EndTextureMode();
    Image result = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&result);
    return result;
}

void ExpectColor(Image image, int tileX, Color expected, float alpha = 1) {
    const auto actual = GetImageColor(image, tileX * TILE_SIZE + 32, 32);
    assert(std::abs(static_cast<int>(actual.r) - static_cast<int>(expected.r * alpha)) <= 2);
    assert(std::abs(static_cast<int>(actual.g) - static_cast<int>(expected.g * alpha)) <= 2);
    assert(std::abs(static_cast<int>(actual.b) - static_cast<int>(expected.b * alpha)) <= 2);
}

void ExpectSame(Image a, Image b) {
    for (int y = 0; y < a.height; ++y) {
        for (int x = 0; x < a.width; ++x) {
            const auto ca = GetImageColor(a, x, y), cb = GetImageColor(b, x, y);
            assert(ca.r == cb.r && ca.g == cb.g && ca.b == cb.b && ca.a == cb.a);
        }
    }
}
} // namespace

int main() {
    TemporaryAssets assets;
    Sprite("assets/floor/lobby/1-1.png", 64, 64, RED);
    Sprite("assets/floor/cave/1-2.png", 256, 256, GREEN);
    Sprite("assets/floor/cave/1-5.png", 128, 128, GREEN); // repeated ID is accepted
    Sprite("assets/floor/mansion/1-1.png", 32, 32, BLUE);
    Sprite("assets/floor/dungeon/1-1.png", 64, 64, GOLD);
    Sprite("assets/floor/cave/2-100.png", 128, 64, MAGENTA); // rejected rectangle
    Sprite("assets/floor/cave/0-100.png", 64, 64, MAGENTA); // rejected filename
    Sprite("assets/floor/cave/3-0.png", 64, 64, MAGENTA); // rejected zero weight
    fs::create_directories("assets/floor/cave/nested");
    Sprite("assets/floor/cave/nested/4-100.png", 64, 64, MAGENTA); // not recursive
    std::ofstream("assets/floor/cave/5-100.png") << "corrupt PNG";

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 64, "Floor rendering tests");
    assert(IsWindowReady());
    auto target = LoadRenderTexture(320, 64);
    assert(target.id != 0);
    {
        FloorTiles floors;
        floors.BeginRun(42);
        Room cave = MakeRoom({0, 0}, BiomeType::Cave, 0);
        Room lobby = MakeRoom({1, 0}, BiomeType::Lobby, 2);
        Room mansion = MakeRoom({2, 0}, BiomeType::Mansion, 3);
        Room dungeon = MakeRoom({3, 0}, BiomeType::Dungeon, 4);
        Doorway outward, inward;
        outward.targetCoords = lobby.GetCoords();
        inward.targetCoords = cave.GetCoords();
        outward.corridorTiles = inward.corridorTiles = TileRect{1, 0, 1, 1};
        cave.Layout().doors.push_back(outward);
        lobby.Layout().doors.push_back(inward);
        std::vector<FloorRoomView> views{{&cave, 0.25f}, {&lobby, 0.75f}, {&mansion, 1}, {&dungeon, 1}};
        Image initial = Render(floors, views, target);
        ExpectColor(initial, 0, GREEN, 0.25f);
        ExpectColor(initial, 1, GREEN, 0.75f); // once, highest alpha, external biome
        ExpectColor(initial, 2, RED, 0.75f);
        ExpectColor(initial, 3, BLUE);
        ExpectColor(initial, 4, GOLD);
        std::reverse(views.begin(), views.end());
        Image revisit = Render(floors, views, target);
        ExpectSame(initial, revisit);
        UnloadImage(revisit);
        floors.BeginRun(42);
        Image reset = Render(floors, views, target);
        ExpectSame(initial, reset);
        UnloadImage(reset);
        UnloadImage(initial);

        // Catalog remains in GPU memory after assets change and the run resets.
        fs::remove_all(assets.root / "assets/floor");
        floors.BeginRun(99);
        Image retained = Render(floors, {{&cave, 0}, {&lobby, 1}, {&mansion, 1}, {&dungeon, 1}}, target);
        ExpectColor(retained, 0, BLACK);
        ExpectColor(retained, 1, GREEN); // hidden cave still resolves the corridor biome
        ExpectColor(retained, 2, RED);
        ExpectColor(retained, 3, BLUE);
        ExpectColor(retained, 4, GOLD);
        UnloadImage(retained);
        floors.Unload();
        floors.Unload(); // explicit shutdown and destructor are safe together

        FloorTiles missing;
        Image fallback = Render(missing, {{&cave, 1}, {&lobby, 1}, {&mansion, 1}, {&dungeon, 1}}, target);
        ExpectColor(fallback, 0, Color{58, 62, 70, 255});
        ExpectColor(fallback, 1, Color{58, 62, 70, 255});
        ExpectColor(fallback, 2, Color{50, 52, 63, 255});
        ExpectColor(fallback, 3, Color{72, 54, 42, 255});
        ExpectColor(fallback, 4, Color{46, 66, 56, 255});
        UnloadImage(fallback);
    }
    UnloadRenderTexture(target);
    CloseWindow();
    std::cout << "Floor rendering tests passed\n";
}
