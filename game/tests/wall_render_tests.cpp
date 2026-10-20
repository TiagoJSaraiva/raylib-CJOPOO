#include "wall_tiles.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace {
namespace fs = std::filesystem;

struct TemporaryAssets {
    fs::path original = fs::current_path();
    fs::path root = fs::temp_directory_path() /
        ("endless-rooms-wall-test-" +
         std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));

    TemporaryAssets() {
        for (const char* biome : {"lobby", "cave", "dungeon", "mansion"}) {
            for (const char* section : {"side", "upper", "bottom"}) {
                fs::create_directories(root / "assets/wall" / biome / section);
            }
        }
        fs::current_path(root);
    }

    ~TemporaryAssets() {
        fs::current_path(original);
        fs::remove_all(root);
    }
};

void Sprite(const fs::path& path, int width, int height, Color color) {
    Image image = GenImageColor(width, height, color);
    assert(ExportImage(image, path.generic_string().c_str()));
    UnloadImage(image);
}

Room MakeRoom(RoomCoords coords, BiomeType biome, int x, int y) {
    RoomLayout layout;
    layout.widthTiles = 3;
    layout.heightTiles = 2;
    layout.tileBounds = TileRect{x, y, layout.widthTiles, layout.heightTiles};
    return Room(coords, RoomSeedData{RoomType::Normal, biome, 1}, layout);
}

Image Render(WallTiles& walls, const std::vector<WallRules::RoomView>& views,
             RenderTexture2D target, bool foreground) {
    BeginTextureMode(target);
    ClearBackground(BLACK);
    walls.DrawBackground(views);
    if (foreground) walls.DrawForeground(views);
    EndTextureMode();
    Image result = LoadImageFromTexture(target.texture);
    ImageFlipVertical(&result);
    return result;
}

void ExpectColor(Image image, int tileX, int tileY, Color expected, float visibility = 1.0f) {
    const auto actual = GetImageColor(image, tileX * TILE_SIZE + 32, tileY * TILE_SIZE + 32);
    const auto expectedR = static_cast<int>(expected.r * visibility);
    const auto expectedG = static_cast<int>(expected.g * visibility);
    const auto expectedB = static_cast<int>(expected.b * visibility);
    if (std::abs(static_cast<int>(actual.r) - expectedR) > 3 ||
        std::abs(static_cast<int>(actual.g) - expectedG) > 3 ||
        std::abs(static_cast<int>(actual.b) - expectedB) > 3) {
        std::cerr << "Color mismatch at tile (" << tileX << ", " << tileY << "): got RGB("
                  << static_cast<int>(actual.r) << ", " << static_cast<int>(actual.g) << ", "
                  << static_cast<int>(actual.b) << "), expected RGB(" << expectedR << ", "
                  << expectedG << ", " << expectedB << ")\n";
    }
    assert(std::abs(static_cast<int>(actual.r) - expectedR) <= 3);
    assert(std::abs(static_cast<int>(actual.g) - expectedG) <= 3);
    assert(std::abs(static_cast<int>(actual.b) - expectedB) <= 3);
}

Color Pixel(Image image, int tileX, int tileY) {
    return GetImageColor(image, tileX * TILE_SIZE + 32, tileY * TILE_SIZE + 32);
}

void ExpectTranslucent(Image image, int tileX, int tileY, Color source) {
    const Color actual = Pixel(image, tileX, tileY);
    assert(actual.r > 0 && actual.r < source.r);
    assert(actual.g > 0 && actual.g < source.g);
    assert(actual.b > 0 && actual.b < source.b);
}

void ExpectFallbackColor(Image image, int tileX, int tileY, Color base) {
    const auto actual = GetImageColor(image, tileX * TILE_SIZE + 32, tileY * TILE_SIZE + 32);
    assert(std::abs(static_cast<int>(actual.r) - static_cast<int>(base.r)) <= 45);
    assert(std::abs(static_cast<int>(actual.g) - static_cast<int>(base.g)) <= 45);
    assert(std::abs(static_cast<int>(actual.b) - static_cast<int>(base.b)) <= 45);
}

Color SpriteColor(std::size_t biome, WallRules::Section section) {
    if (biome == 0) {
        if (section == WallRules::Section::Side) return Color{200, 20, 10, 128};
        if (section == WallRules::Section::Upper) return Color{180, 30, 20, 255};
        return Color{220, 40, 30, 255};
    }
    if (biome == 1) {
        if (section == WallRules::Section::Side) return Color{20, 200, 30, 255};
        if (section == WallRules::Section::Upper) return Color{30, 180, 40, 255};
        return Color{40, 220, 50, 255};
    }
    if (biome == 2) {
        if (section == WallRules::Section::Side) return Color{20, 30, 200, 255};
        if (section == WallRules::Section::Upper) return Color{30, 40, 180, 255};
        return Color{40, 50, 220, 255};
    }
    if (section == WallRules::Section::Side) return Color{200, 180, 20, 255};
    if (section == WallRules::Section::Upper) return Color{180, 160, 30, 255};
    return Color{220, 200, 40, 255};
}

void AddTestSprites() {
    const char* biomes[] = {"lobby", "cave", "mansion", "dungeon"};
    const char* sections[] = {"side", "upper", "bottom"};
    for (std::size_t biome = 0; biome < 4; ++biome) {
        for (std::size_t section = 0; section < 3; ++section) {
            const auto wallSection = static_cast<WallRules::Section>(section);
            const int size = (biome == 1 && wallSection == WallRules::Section::Upper) ? 256 : 64;
            Sprite(fs::path("assets/wall") / biomes[biome] / sections[section] / "1-2.png",
                   size, size, SpriteColor(biome, wallSection));
        }
    }
}

void TestWallCatalogsAndLayers(RenderTexture2D target) {
    WallTiles walls;
    walls.BeginRun(42);
    Room lobby = MakeRoom({0, 0}, BiomeType::Lobby, 2, 2);
    Room cave = MakeRoom({1, 0}, BiomeType::Cave, 8, 2);
    Room mansion = MakeRoom({2, 0}, BiomeType::Mansion, 14, 2);
    Room dungeon = MakeRoom({3, 0}, BiomeType::Dungeon, 20, 2);
    const std::vector<WallRules::RoomView> views{
        {&lobby, 1.0f}, {&cave, 1.0f}, {&mansion, 1.0f}, {&dungeon, 1.0f}
    };

    Image background = Render(walls, views, target, false);
    for (std::size_t biome = 0; biome < 4; ++biome) {
        const int x = 2 + static_cast<int>(biome) * 6;
        const Color side = SpriteColor(biome, WallRules::Section::Side);
        const Color upper = SpriteColor(biome, WallRules::Section::Upper);
        if (biome == 0) ExpectTranslucent(background, x - 1, 2, side);
        else ExpectColor(background, x - 1, 2, side);
        ExpectColor(background, x, 1, upper);
        ExpectColor(background, x, 3, BLACK); // Bottom walls wait for the foreground pass.
    }
    const Color lobbySideAtFullVisibility = Pixel(background, 1, 2);
    UnloadImage(background);

    Image full = Render(walls, views, target, true);
    for (std::size_t biome = 0; biome < 4; ++biome) {
        const int x = 2 + static_cast<int>(biome) * 6;
        ExpectColor(full, x, 3, SpriteColor(biome, WallRules::Section::Bottom));
    }

    Image repeated = Render(walls, views, target, true);
    for (int y = 0; y < full.height; ++y) {
        for (int x = 0; x < full.width; ++x) {
            const auto a = GetImageColor(full, x, y);
            const auto b = GetImageColor(repeated, x, y);
            assert(a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a);
        }
    }
    UnloadImage(repeated);
    UnloadImage(full);

    Image faded = Render(walls, {{&lobby, 0.5f}}, target, false);
    const Color lobbySideFaded = Pixel(faded, 1, 2);
    assert(lobbySideFaded.r > 0 && lobbySideFaded.r < lobbySideAtFullVisibility.r);
    assert(lobbySideFaded.g > 0 && lobbySideFaded.g < lobbySideAtFullVisibility.g);
    assert(lobbySideFaded.b > 0 && lobbySideFaded.b < lobbySideAtFullVisibility.b);
    UnloadImage(faded);

    // Catalogs stay resident after the asset files change.
    fs::remove_all("assets/wall");
    Image retained = Render(walls, {{&cave, 1.0f}}, target, true);
    ExpectColor(retained, 7, 2, SpriteColor(1, WallRules::Section::Side));
    UnloadImage(retained);
    walls.Unload();
    walls.Unload();
}

void TestFallback(RenderTexture2D target) {
    fs::create_directories("assets/wall");
    WallTiles walls;
    walls.BeginRun(99);
    Room lobby = MakeRoom({0, 0}, BiomeType::Lobby, 5, 2);
    Image image = Render(walls, {{&lobby, 1.0f}}, target, true);
    ExpectFallbackColor(image, 4, 2, Color{90, 92, 110, 255}); // side
    ExpectFallbackColor(image, 5, 1, Color{90, 92, 110, 255}); // upper
    ExpectFallbackColor(image, 5, 3, Color{90, 92, 110, 255}); // bottom
    UnloadImage(image);
}

} // namespace

int main() {
    TemporaryAssets assets;
    AddTestSprites();
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 64, "Wall rendering tests");
    assert(IsWindowReady());
    auto target = LoadRenderTexture(26 * TILE_SIZE, 6 * TILE_SIZE);
    assert(target.id != 0);
    TestWallCatalogsAndLayers(target);
    TestFallback(target);
    UnloadRenderTexture(target);
    CloseWindow();
    std::cout << "Wall rendering tests passed\n";
}
