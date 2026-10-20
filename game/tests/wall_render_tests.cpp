#include "wall_tiles.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
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

void ExpectPixelColor(Image image, int pixelX, int pixelY, Color expected) {
    const Color actual = GetImageColor(image, pixelX, pixelY);
    assert(std::abs(static_cast<int>(actual.r) - expected.r) <= 3);
    assert(std::abs(static_cast<int>(actual.g) - expected.g) <= 3);
    assert(std::abs(static_cast<int>(actual.b) - expected.b) <= 3);
}

void ExpectTranslucentPixel(Image image, int pixelX, int pixelY, Color source) {
    const Color actual = GetImageColor(image, pixelX, pixelY);
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

void ExpectFallbackPixelColor(Image image, int pixelX, int pixelY, Color base) {
    const auto actual = GetImageColor(image, pixelX, pixelY);
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
            const int width = (biome == 1 && wallSection == WallRules::Section::Upper) ? 919 : 64;
            const int height = (biome == 1 && wallSection == WallRules::Section::Upper) ? 811 : 64;
            Sprite(fs::path("assets/wall") / biomes[biome] / sections[section] / "1-2.png",
                   width, height, SpriteColor(biome, wallSection));
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
        const int leftEdgeX = x * TILE_SIZE;
        const int rightEdgeX = (x + 3) * TILE_SIZE;
        const int sideMiddleY = 2 * TILE_SIZE + TILE_SIZE / 2;
        const int upperMiddleY = TILE_SIZE + TILE_SIZE / 2;
        if (biome == 0) {
            ExpectTranslucentPixel(background, leftEdgeX - 8, sideMiddleY, side);
            ExpectTranslucentPixel(background, rightEdgeX + 8, sideMiddleY, side);
            ExpectTranslucentPixel(background, leftEdgeX - 8, upperMiddleY, side);
        } else {
            ExpectPixelColor(background, leftEdgeX - 8, sideMiddleY, side);
            ExpectPixelColor(background, rightEdgeX + 8, sideMiddleY, side);
            ExpectPixelColor(background, leftEdgeX - 8, upperMiddleY, side);
        }
        ExpectPixelColor(background, leftEdgeX - 17, sideMiddleY, BLACK);
        ExpectPixelColor(background, leftEdgeX, sideMiddleY, BLACK);
        ExpectPixelColor(background, rightEdgeX - 1, sideMiddleY, BLACK);
        ExpectPixelColor(background, rightEdgeX + 16, sideMiddleY, BLACK);
        ExpectColor(background, x, 1, upper);
        ExpectPixelColor(background, leftEdgeX, upperMiddleY, upper);
        ExpectPixelColor(background, rightEdgeX - 1, upperMiddleY, upper);
        if (biome == 0) ExpectTranslucentPixel(background, rightEdgeX + 8, upperMiddleY, side);
        else ExpectPixelColor(background, rightEdgeX + 8, upperMiddleY, side);
        ExpectColor(background, x, 3, BLACK); // Bottom walls wait for the foreground pass.
    }
    const Color rectangularUpper = SpriteColor(1, WallRules::Section::Upper);
    ExpectPixelColor(background, 8 * TILE_SIZE + 1, 1 * TILE_SIZE + 1, rectangularUpper);
    ExpectPixelColor(background, 8 * TILE_SIZE + TILE_SIZE - 2,
                     2 * TILE_SIZE - 2, rectangularUpper);
    const int lobbyLeftSidePixelX = 2 * TILE_SIZE - 8;
    const int lobbySidePixelY = 2 * TILE_SIZE + TILE_SIZE / 2;
    const Color lobbySideAtFullVisibility =
        GetImageColor(background, lobbyLeftSidePixelX, lobbySidePixelY);
    UnloadImage(background);

    Image full = Render(walls, views, target, true);
    for (std::size_t biome = 0; biome < 4; ++biome) {
        const int x = 2 + static_cast<int>(biome) * 6;
        ExpectColor(full, x, 3, SpriteColor(biome, WallRules::Section::Bottom));
        const int leftEdgeX = x * TILE_SIZE;
        const int rightEdgeX = (x + 3) * TILE_SIZE;
        const int bottomMiddleY = 3 * TILE_SIZE + TILE_SIZE / 2;
        const Color side = SpriteColor(biome, WallRules::Section::Side);
        const Color bottom = SpriteColor(biome, WallRules::Section::Bottom);
        if (biome == 0) {
            ExpectTranslucentPixel(full, leftEdgeX - 8, bottomMiddleY, side);
            ExpectTranslucentPixel(full, rightEdgeX + 8, bottomMiddleY, side);
        } else {
            ExpectPixelColor(full, leftEdgeX - 8, bottomMiddleY, side);
            ExpectPixelColor(full, rightEdgeX + 8, bottomMiddleY, side);
        }
        ExpectPixelColor(full, leftEdgeX, bottomMiddleY, bottom);
        ExpectPixelColor(full, rightEdgeX - 1, bottomMiddleY, bottom);
        ExpectPixelColor(full, leftEdgeX - 8, 4 * TILE_SIZE + TILE_SIZE / 2, BLACK);
        ExpectPixelColor(full, rightEdgeX + 8, 4 * TILE_SIZE + TILE_SIZE / 2, BLACK);
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
    const Color lobbySideFaded = GetImageColor(faded, lobbyLeftSidePixelX, lobbySidePixelY);
    assert(lobbySideFaded.r > 0 && lobbySideFaded.r < lobbySideAtFullVisibility.r);
    assert(lobbySideFaded.g > 0 && lobbySideFaded.g < lobbySideAtFullVisibility.g);
    assert(lobbySideFaded.b > 0 && lobbySideFaded.b < lobbySideAtFullVisibility.b);
    UnloadImage(faded);

    // Catalogs stay resident after the asset files change.
    fs::remove_all("assets/wall");
    Image retained = Render(walls, {{&cave, 1.0f}}, target, true);
    ExpectPixelColor(retained, 8 * TILE_SIZE - 8, 2 * TILE_SIZE + TILE_SIZE / 2,
                     SpriteColor(1, WallRules::Section::Side));
    UnloadImage(retained);
    walls.Unload();
    walls.Unload();
}

void TestFallback(RenderTexture2D target) {
    fs::create_directories("assets/wall");
    fs::create_directories("assets/wall/lobby/upper");
    {
        std::ofstream invalid("assets/wall/lobby/upper/1-1.png", std::ios::binary);
        invalid << "not a png";
    }
    WallTiles walls;
    walls.BeginRun(99);
    Room lobby = MakeRoom({0, 0}, BiomeType::Lobby, 5, 2);
    Image image = Render(walls, {{&lobby, 1.0f}}, target, true);
    ExpectFallbackColor(image, 5, 1, Color{90, 92, 110, 255}); // upper
    ExpectFallbackColor(image, 5, 3, Color{90, 92, 110, 255}); // bottom
    const int leftEdgeX = 5 * TILE_SIZE;
    const int rightEdgeX = 8 * TILE_SIZE;
    const int sideMiddleY = 2 * TILE_SIZE + TILE_SIZE / 2;
    ExpectPixelColor(image, leftEdgeX - 17, sideMiddleY, BLACK);
    ExpectPixelColor(image, rightEdgeX + 16, sideMiddleY, BLACK);
    ExpectPixelColor(image, leftEdgeX - 8, 4 * TILE_SIZE + TILE_SIZE / 2, BLACK);
    ExpectPixelColor(image, rightEdgeX + 8, 4 * TILE_SIZE + TILE_SIZE / 2, BLACK);
    const Color fallbackBase{90, 92, 110, 255};
    ExpectFallbackPixelColor(image, leftEdgeX - 8, sideMiddleY, fallbackBase);
    ExpectFallbackPixelColor(image, rightEdgeX + 8, sideMiddleY, fallbackBase);
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
