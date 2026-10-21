#include "wall_tiles.h"
#include "door_visual_rules.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>

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

Room MakeRoom(RoomCoords coords, BiomeType biome, int x, int y,
              int width = 3, int height = 2) {
    RoomLayout layout;
    layout.widthTiles = width;
    layout.heightTiles = height;
    layout.tileBounds = TileRect{x, y, width, height};
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

Image RenderWithPlayer(WallTiles& walls, const std::vector<WallRules::RoomView>& views,
                       RenderTexture2D target, Rectangle playerBounds, float deltaSeconds) {
    BeginTextureMode(target);
    ClearBackground(BLACK);
    walls.DrawBackground(views);
    walls.DrawForeground(views, playerBounds, deltaSeconds);
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
    if (std::abs(static_cast<int>(actual.r) - expected.r) > 3 ||
        std::abs(static_cast<int>(actual.g) - expected.g) > 3 ||
        std::abs(static_cast<int>(actual.b) - expected.b) > 3) {
        std::cerr << "Pixel mismatch at (" << pixelX << ", " << pixelY << "): got RGB("
                  << static_cast<int>(actual.r) << ", " << static_cast<int>(actual.g) << ", "
                  << static_cast<int>(actual.b) << "), expected RGB("
                  << static_cast<int>(expected.r) << ", " << static_cast<int>(expected.g)
                  << ", " << static_cast<int>(expected.b) << ")\n";
    }
    assert(std::abs(static_cast<int>(actual.r) - expected.r) <= 3);
    assert(std::abs(static_cast<int>(actual.g) - expected.g) <= 3);
    assert(std::abs(static_cast<int>(actual.b) - expected.b) <= 3);
}

Color BlendOver(Color source, Color background, float visibility = 1.0f) {
    const float alpha = (static_cast<float>(source.a) / 255.0f) * visibility;
    return Color{
        static_cast<unsigned char>(std::lround(source.r * alpha + background.r * (1.0f - alpha))),
        static_cast<unsigned char>(std::lround(source.g * alpha + background.g * (1.0f - alpha))),
        static_cast<unsigned char>(std::lround(source.b * alpha + background.b * (1.0f - alpha))),
        255
    };
}

Color ProceduralSideColor(int tileX, int tileY, Color base) {
    const std::uint64_t seedX = static_cast<std::uint64_t>(static_cast<std::int64_t>(tileX));
    const std::uint64_t seedY = static_cast<std::uint64_t>(static_cast<std::int64_t>(tileY));
    std::uint64_t seed = seedX * 0x9e3779b97f4a7c15ULL ^ seedY;
    seed ^= (seed >> 23);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> delta(-12, 12);
    const auto adjust = [&delta, &rng](unsigned char value) {
        return static_cast<unsigned char>(std::clamp(static_cast<int>(value) + delta(rng), 0, 255));
    };
    return Color{adjust(base.r), adjust(base.g), adjust(base.b), base.a};
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

void TestTwoTileWallRendering(RenderTexture2D target) {
    WallTiles walls;
    walls.BeginRun(7001);
    Room room = MakeRoom({0, 0}, BiomeType::Cave, 2, 2, 4, 4);
    const Color upper = SpriteColor(1, WallRules::Section::Upper);
    const Color bottom = SpriteColor(1, WallRules::Section::Bottom);
    const Color side = SpriteColor(1, WallRules::Section::Side);
    const int leftEdgeX = 2 * TILE_SIZE;
    const int rightEdgeX = 6 * TILE_SIZE;

    Image background = Render(walls, {{&room, 1.0f}}, target, false);
    for (const int tileX : {2, 3, 4, 5}) {
        ExpectPixelColor(background, tileX * TILE_SIZE + TILE_SIZE / 2,
                         TILE_SIZE + TILE_SIZE / 2, upper);
    }
    ExpectPixelColor(background, 6 * TILE_SIZE + 32,
                     TILE_SIZE + TILE_SIZE / 2, BLACK);
    for (const int tileY : {2, 3, 4, 5}) {
        ExpectPixelColor(background, leftEdgeX - 8,
                         tileY * TILE_SIZE + TILE_SIZE / 2, side);
        ExpectPixelColor(background, rightEdgeX + 8,
                         tileY * TILE_SIZE + TILE_SIZE / 2, side);
    }
    ExpectPixelColor(background, leftEdgeX - 17, 3 * TILE_SIZE + 32, BLACK);
    ExpectPixelColor(background, rightEdgeX + 17, 3 * TILE_SIZE + 32, BLACK);
    ExpectPixelColor(background, leftEdgeX - 8, 6 * TILE_SIZE + 32, BLACK);
    const int sideTop = 2 * TILE_SIZE;
    const int sideBottom = 6 * TILE_SIZE;
    assert(sideBottom - sideTop == 4 * TILE_SIZE);
    UnloadImage(background);

    Image full = Render(walls, {{&room, 1.0f}}, target, true);
    for (const int tileX : {2, 3, 4, 5}) {
        for (const int tileY : {4, 5}) {
            ExpectPixelColor(full, tileX * TILE_SIZE + TILE_SIZE / 2,
                             tileY * TILE_SIZE + TILE_SIZE / 2, bottom);
        }
    }
    ExpectPixelColor(full, 3 * TILE_SIZE + 32, 6 * TILE_SIZE + 32, BLACK);
    ExpectPixelColor(full, 6 * TILE_SIZE + 32,
                     6 * TILE_SIZE + TILE_SIZE / 2, BLACK);
    UnloadImage(full);

    Room alignedRoom = MakeRoom({1, 0}, BiomeType::Cave, 12, 2, 4, 4);
    const std::vector<WallRules::RoomView> views{{&room, 1.0f}, {&alignedRoom, 1.0f}};
    const int sampleX = 3 * TILE_SIZE + TILE_SIZE / 2;
    const int sampleY = 5 * TILE_SIZE + TILE_SIZE / 2;
    const Rectangle playerBehindWall{
        3.0f * TILE_SIZE + 12.0f,
        5.0f * TILE_SIZE + 16.0f,
        40.0f,
        32.0f
    };
    const Rectangle playerAwayFromWall{
        8.0f * TILE_SIZE,
        playerBehindWall.y,
        playerBehindWall.width,
        playerBehindWall.height
    };

    Image outside = RenderWithPlayer(walls, views, target, playerAwayFromWall, 0.2f);
    ExpectPixelColor(outside, sampleX, sampleY, bottom);
    UnloadImage(outside);

    Image entering = RenderWithPlayer(walls, views, target, playerBehindWall, 0.1f);
    ExpectColor(entering, 3, 5, bottom, 0.8f);
    ExpectColor(entering, 4, 5, bottom, 0.8f); // The entire room row tweens together.
    ExpectPixelColor(entering, 13 * TILE_SIZE + 32, sampleY, bottom);
    UnloadImage(entering);
    Image faded = RenderWithPlayer(walls, views, target, playerBehindWall, 0.1f);
    ExpectColor(faded, 3, 5, bottom, 0.6f);
    ExpectColor(faded, 4, 5, bottom, 0.6f);
    ExpectPixelColor(faded, 13 * TILE_SIZE + 32, sampleY, bottom);
    UnloadImage(faded);

    Image leaving = RenderWithPlayer(walls, views, target, playerAwayFromWall, 0.1f);
    ExpectColor(leaving, 3, 5, bottom, 0.8f);
    ExpectColor(leaving, 4, 5, bottom, 0.8f);
    UnloadImage(leaving);
    Image restored = RenderWithPlayer(walls, views, target, playerAwayFromWall, 0.1f);
    ExpectPixelColor(restored, sampleX, sampleY, bottom);
    ExpectPixelColor(restored, 13 * TILE_SIZE + 32, sampleY, bottom);
    UnloadImage(restored);

    Room corridorRoom = MakeRoom({2, 0}, BiomeType::Cave, 10, 1, 3, 4);
    auto eastDoorState = std::make_shared<DoorInstance>();
    Doorway eastDoor;
    eastDoor.direction = Direction::East;
    eastDoor.offset = 1;
    eastDoor.width = 2;
    eastDoor.corridorTiles = TileRect{13, 2, 4, 2};
    eastDoor.doorState = eastDoorState;
    eastDoorState->open = true;
    corridorRoom.Layout().doors.push_back(eastDoor);

    const std::vector<WallRules::RoomView> corridorViews{{&corridorRoom, 1.0f}};
    const Rectangle playerInCorridor{
        13.0f * TILE_SIZE + 12.0f,
        3.0f * TILE_SIZE + 16.0f,
        40.0f,
        32.0f
    };
    Image corridorEntering = RenderWithPlayer(walls, corridorViews, target, playerInCorridor, 0.1f);
    ExpectColor(corridorEntering, 15, 3, bottom, 0.8f);
    ExpectPixelColor(corridorEntering, 13 * TILE_SIZE + 8,
                     3 * TILE_SIZE + 32, side);
    UnloadImage(corridorEntering);
    Image corridorFaded = RenderWithPlayer(walls, corridorViews, target, playerInCorridor, 0.1f);
    ExpectColor(corridorFaded, 15, 3, bottom, 0.6f);
    ExpectPixelColor(corridorFaded, 13 * TILE_SIZE + 8,
                     3 * TILE_SIZE + 32, side);
    UnloadImage(corridorFaded);
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
        const int leftEdgeX = x * TILE_SIZE;
        const int rightEdgeX = (x + 3) * TILE_SIZE;
        const int bottomAnchorY = 2 * TILE_SIZE + TILE_SIZE / 2;
        const int bottomMiddleY = 3 * TILE_SIZE + TILE_SIZE / 2;
        const Color side = SpriteColor(biome, WallRules::Section::Side);
        const Color bottom = SpriteColor(biome, WallRules::Section::Bottom);
        for (const int tileX : {x, x + 1}) {
            ExpectColor(full, tileX, 2, bottom);
            ExpectColor(full, tileX, 3, bottom);
        }
        ExpectPixelColor(full, (x + 2) * TILE_SIZE + TILE_SIZE / 2,
                         bottomAnchorY, bottom);
        ExpectPixelColor(full, (x + 2) * TILE_SIZE + TILE_SIZE / 2,
                         bottomMiddleY, BLACK);
        if (biome == 0) {
            ExpectTranslucentPixel(full, leftEdgeX - 8, bottomMiddleY, side);
            ExpectTranslucentPixel(full, rightEdgeX + 8, bottomMiddleY, side);
        } else {
            ExpectPixelColor(full, leftEdgeX - 8, bottomMiddleY, side);
            ExpectPixelColor(full, rightEdgeX + 8, bottomMiddleY, side);
        }
        ExpectPixelColor(full, leftEdgeX + TILE_SIZE / 8, bottomMiddleY, bottom);
        ExpectPixelColor(full, rightEdgeX - TILE_SIZE / 8, bottomAnchorY, bottom);
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

    Room southRoom = MakeRoom({4, 1}, BiomeType::Lobby, 5, 2);
    auto southDoorState = std::make_shared<DoorInstance>();
    Doorway southDoor;
    southDoor.direction = Direction::South;
    southDoor.offset = 1;
    southDoor.width = 1;
    southDoor.corridorTiles = TileRect{6, 4, 1, 2};
    southDoor.doorState = southDoorState;
    southRoom.Layout().doors.push_back(southDoor);

    const int bottomWallY = 3 * TILE_SIZE + TILE_SIZE / 2;
    const int bottomWallTopY = 2 * TILE_SIZE + TILE_SIZE / 2;
    const int leftInnerX = 6 * TILE_SIZE - TILE_SIZE / 8;
    const int rightInnerX = 7 * TILE_SIZE + TILE_SIZE / 8;
    const int leftOuterX = 5 * TILE_SIZE + TILE_SIZE / 8;
    const int rightOuterX = 8 * TILE_SIZE - TILE_SIZE / 8;
    const Color lobbySide = SpriteColor(0, WallRules::Section::Side);
    const Color lobbyBottom = SpriteColor(0, WallRules::Section::Bottom);

    Image closedSouth = Render(walls, {{&southRoom, 1.0f}}, target, true);
    ExpectPixelColor(closedSouth, leftInnerX, bottomWallY, BLACK);
    ExpectPixelColor(closedSouth, rightInnerX, bottomWallY, BLACK);
    ExpectPixelColor(closedSouth, leftOuterX, bottomWallY, BLACK);
    ExpectPixelColor(closedSouth, rightOuterX, bottomWallY, BLACK);
    const int bottomAnchorY = 2 * TILE_SIZE + TILE_SIZE / 2;
    const Color closedTopLeft = GetImageColor(closedSouth, leftInnerX, bottomWallTopY);
    const Color closedTopRight = GetImageColor(closedSouth, rightInnerX, bottomWallTopY);
    ExpectPixelColor(closedSouth, leftOuterX, bottomAnchorY, lobbyBottom);
    ExpectPixelColor(closedSouth, rightOuterX, bottomAnchorY, lobbyBottom);
    ExpectPixelColor(closedSouth, leftInnerX, 4 * TILE_SIZE + TILE_SIZE / 2, BLACK);
    UnloadImage(closedSouth);

    southDoorState->opening = true;
    southDoorState->fadeProgress = DoorVisualRules::FadeDuration * 0.5f;
    Image openingSouth = Render(walls, {{&southRoom, 1.0f}}, target, true);
    const Color halfVisibilityCorner = BlendOver(lobbySide, BLACK, 0.5f);
    ExpectPixelColor(openingSouth, leftInnerX, bottomWallY, halfVisibilityCorner);
    ExpectPixelColor(openingSouth, rightInnerX, bottomWallY, halfVisibilityCorner);
    ExpectPixelColor(openingSouth, leftInnerX, bottomWallTopY,
                     BlendOver(lobbySide, closedTopLeft, 0.5f));
    ExpectPixelColor(openingSouth, rightInnerX, bottomWallTopY,
                     BlendOver(lobbySide, closedTopRight, 0.5f));
    ExpectPixelColor(openingSouth, leftOuterX, bottomWallY, BLACK);
    ExpectPixelColor(openingSouth, rightOuterX, bottomWallY, BLACK);
    UnloadImage(openingSouth);

    southDoorState->open = true;
    Image openSouth = Render(walls, {{&southRoom, 1.0f}}, target, true);
    const Color fullVisibilityCorner = BlendOver(lobbySide, BLACK);
    ExpectPixelColor(openSouth, leftInnerX, bottomWallY, fullVisibilityCorner);
    ExpectPixelColor(openSouth, rightInnerX, bottomWallY, fullVisibilityCorner);
    ExpectPixelColor(openSouth, leftInnerX, bottomWallTopY,
                     BlendOver(lobbySide, closedTopLeft));
    ExpectPixelColor(openSouth, rightInnerX, bottomWallTopY,
                     BlendOver(lobbySide, closedTopRight));
    ExpectPixelColor(openSouth, leftOuterX, bottomWallY, BLACK);
    ExpectPixelColor(openSouth, rightOuterX, bottomWallY, BLACK);
    ExpectPixelColor(openSouth, leftOuterX, bottomAnchorY, lobbyBottom);
    ExpectPixelColor(openSouth, rightOuterX, bottomAnchorY, lobbyBottom);
    ExpectTranslucentPixel(openSouth, leftInnerX, 4 * TILE_SIZE + TILE_SIZE / 2, lobbySide);
    // The two-tile overlays cover the full sides of the terminal bottom wall.
    ExpectPixelColor(openSouth, 6 * TILE_SIZE, bottomWallY, BLACK);
    ExpectPixelColor(openSouth, 7 * TILE_SIZE + TILE_SIZE / 4, bottomWallY, BLACK);
    ExpectPixelColor(openSouth, 7 * TILE_SIZE + TILE_SIZE / 4, bottomAnchorY, lobbyBottom);
    UnloadImage(openSouth);

    Room horizontalEast = MakeRoom({5, 0}, BiomeType::Lobby, 10, 1, 3, 4);
    auto horizontalEastState = std::make_shared<DoorInstance>();
    Doorway horizontalEastDoor;
    horizontalEastDoor.direction = Direction::East;
    horizontalEastDoor.offset = 1;
    horizontalEastDoor.width = 2;
    horizontalEastDoor.corridorTiles = TileRect{13, 2, 2, 2};
    horizontalEastDoor.doorState = horizontalEastState;
    horizontalEast.Layout().doors.push_back(horizontalEastDoor);

    const int eastUpperX = 13 * TILE_SIZE + TILE_SIZE / 8;
    const int eastUpperWallTopY = TILE_SIZE / 2;
    const int eastWallMiddleY = TILE_SIZE + TILE_SIZE / 2;
    const int eastBottomWallTopY = 2 * TILE_SIZE + TILE_SIZE / 2;
    const int eastLowerY = 3 * TILE_SIZE + TILE_SIZE / 2;
    Image closedEast = Render(walls, {{&horizontalEast, 1.0f}}, target, true);
    const Color closedEastUpper = GetImageColor(closedEast, eastUpperX, eastUpperWallTopY);
    const Color closedEastUpperMiddle = GetImageColor(closedEast, eastUpperX, eastWallMiddleY);
    const Color closedEastBottomTop = GetImageColor(closedEast, eastUpperX, eastBottomWallTopY);
    const Color closedEastLower = GetImageColor(closedEast, eastUpperX, eastLowerY);
    ExpectPixelColor(closedEast, 13 * TILE_SIZE + TILE_SIZE / 2, eastWallMiddleY,
                     SpriteColor(0, WallRules::Section::Upper));
    ExpectPixelColor(closedEast, 13 * TILE_SIZE + TILE_SIZE / 2, eastLowerY,
                     SpriteColor(0, WallRules::Section::Bottom));
    UnloadImage(closedEast);

    horizontalEastState->opening = true;
    horizontalEastState->fadeProgress = DoorVisualRules::FadeDuration * 0.5f;
    Image openingEast = Render(walls, {{&horizontalEast, 1.0f}}, target, true);
    ExpectPixelColor(openingEast, eastUpperX, eastUpperWallTopY,
                     closedEastUpper);
    ExpectPixelColor(openingEast, eastUpperX, eastWallMiddleY,
                     closedEastUpperMiddle);
    ExpectPixelColor(openingEast, eastUpperX, eastBottomWallTopY,
                     BlendOver(lobbySide, closedEastBottomTop, 0.5f));
    ExpectPixelColor(openingEast, eastUpperX, eastLowerY,
                     BlendOver(lobbySide, closedEastLower, 0.5f));
    UnloadImage(openingEast);

    horizontalEastState->open = true;
    Image openEast = Render(walls, {{&horizontalEast, 1.0f}}, target, true);
    ExpectPixelColor(openEast, eastUpperX, eastUpperWallTopY,
                     closedEastUpper);
    ExpectPixelColor(openEast, eastUpperX, eastWallMiddleY,
                     closedEastUpperMiddle);
    ExpectPixelColor(openEast, eastUpperX, eastBottomWallTopY,
                     BlendOver(lobbySide, closedEastBottomTop));
    ExpectPixelColor(openEast, eastUpperX, eastLowerY,
                     BlendOver(lobbySide, closedEastLower));
    ExpectPixelColor(openEast, 13 * TILE_SIZE + TILE_SIZE / 2, eastWallMiddleY,
                     SpriteColor(0, WallRules::Section::Upper));
    ExpectPixelColor(openEast, 13 * TILE_SIZE + TILE_SIZE / 2, eastLowerY,
                     SpriteColor(0, WallRules::Section::Bottom));
    UnloadImage(openEast);

    Room horizontalWest = MakeRoom({6, 0}, BiomeType::Lobby, 18, 1, 3, 4);
    auto horizontalWestState = std::make_shared<DoorInstance>();
    horizontalWestState->open = true;
    Doorway horizontalWestDoor;
    horizontalWestDoor.direction = Direction::West;
    horizontalWestDoor.offset = 1;
    horizontalWestDoor.width = 2;
    horizontalWestDoor.corridorTiles = TileRect{16, 2, 2, 2};
    horizontalWestDoor.doorState = horizontalWestState;
    horizontalWest.Layout().doors.push_back(horizontalWestDoor);
    Image openWest = Render(walls, {{&horizontalWest, 1.0f}}, target, true);
    const int westCapX = 17 * TILE_SIZE + TILE_SIZE - TILE_SIZE / 8;
    ExpectPixelColor(openWest, westCapX, eastUpperWallTopY,
                     SpriteColor(0, WallRules::Section::Upper));
    ExpectPixelColor(openWest, westCapX, eastWallMiddleY,
                     SpriteColor(0, WallRules::Section::Upper));
    ExpectPixelColor(openWest, westCapX, eastBottomWallTopY,
                     BlendOver(lobbySide, SpriteColor(0, WallRules::Section::Bottom)));
    ExpectPixelColor(openWest, westCapX, eastLowerY,
                     BlendOver(lobbySide, SpriteColor(0, WallRules::Section::Bottom)));
    ExpectPixelColor(openWest, 17 * TILE_SIZE + TILE_SIZE / 2, eastWallMiddleY,
                     SpriteColor(0, WallRules::Section::Upper));
    ExpectPixelColor(openWest, 17 * TILE_SIZE + TILE_SIZE / 2, eastLowerY,
                     SpriteColor(0, WallRules::Section::Bottom));
    UnloadImage(openWest);

    Image faded = Render(walls, {{&lobby, 0.5f}}, target, false);
    const Color lobbySideFaded = GetImageColor(faded, lobbyLeftSidePixelX, lobbySidePixelY);
    assert(lobbySideFaded.r > 0 && lobbySideFaded.r < lobbySideAtFullVisibility.r);
    assert(lobbySideFaded.g > 0 && lobbySideFaded.g < lobbySideAtFullVisibility.g);
    assert(lobbySideFaded.b > 0 && lobbySideFaded.b < lobbySideAtFullVisibility.b);
    UnloadImage(faded);

    Room corridorRoom = MakeRoom({4, 0}, BiomeType::Cave, 5, 2);
    auto corridorDoorState = std::make_shared<DoorInstance>();
    Doorway northDoor;
    northDoor.direction = Direction::North;
    northDoor.offset = 1;
    northDoor.width = 1;
    northDoor.corridorTiles = TileRect{6, 0, 1, 2};
    northDoor.doorState = corridorDoorState;
    corridorRoom.Layout().doors.push_back(northDoor);

    const int corridorLeftStripX = 6 * TILE_SIZE - TILE_SIZE / 8;
    const int corridorRightStripX = 7 * TILE_SIZE + TILE_SIZE / 8;
    const int corridorWallSampleY = TILE_SIZE / 2;
    const Color corridorSide = SpriteColor(1, WallRules::Section::Side);
    Image hiddenCorridor = Render(walls, {{&corridorRoom, 1.0f}}, target, false);
    ExpectPixelColor(hiddenCorridor, corridorLeftStripX, corridorWallSampleY, BLACK);
    ExpectPixelColor(hiddenCorridor, corridorRightStripX, corridorWallSampleY, BLACK);
    const int roomSideSampleX = 5 * TILE_SIZE - TILE_SIZE / 8;
    ExpectPixelColor(hiddenCorridor, roomSideSampleX, 2 * TILE_SIZE + TILE_SIZE / 2, corridorSide);
    UnloadImage(hiddenCorridor);

    corridorDoorState->opening = true;
    corridorDoorState->fadeProgress = DoorVisualRules::FadeDuration * 0.5f;
    Image openingCorridor = Render(walls, {{&corridorRoom, 1.0f}}, target, false);
    ExpectTranslucentPixel(openingCorridor, corridorLeftStripX, corridorWallSampleY, corridorSide);
    ExpectTranslucentPixel(openingCorridor, corridorRightStripX, corridorWallSampleY, corridorSide);
    UnloadImage(openingCorridor);

    corridorDoorState->open = true;
    Image openCorridor = Render(walls, {{&corridorRoom, 1.0f}}, target, false);
    ExpectPixelColor(openCorridor, corridorLeftStripX, corridorWallSampleY, corridorSide);
    ExpectPixelColor(openCorridor, corridorRightStripX, corridorWallSampleY, corridorSide);
    UnloadImage(openCorridor);

    // Catalogs stay resident after the asset files change.
    fs::remove_all("assets/wall");
    Image retained = Render(walls, {{&cave, 1.0f}}, target, true);
    ExpectPixelColor(retained, 8 * TILE_SIZE - 8, 2 * TILE_SIZE + TILE_SIZE / 2,
                     SpriteColor(1, WallRules::Section::Side));
    UnloadImage(retained);
    walls.Unload();
    walls.Unload();
}

void TestVerticalPassageTerminalSideRendering(RenderTexture2D target) {
    WallTiles walls;
    walls.BeginRun(7002);
    Room room = MakeRoom({2, 0}, BiomeType::Cave, 10, 4, 5, 4);
    auto doorState = std::make_shared<DoorInstance>();
    doorState->open = true;

    Doorway northDoor;
    northDoor.direction = Direction::North;
    northDoor.offset = 1;
    northDoor.width = 2;
    northDoor.corridorTiles = TileRect{11, 1, 2, 3};
    northDoor.doorState = doorState;
    room.Layout().doors.push_back(northDoor);

    const Color side = SpriteColor(1, WallRules::Section::Side);
    Image image = Render(walls, {{&room, 1.0f}}, target, false);
    for (const int x : {10 * TILE_SIZE + TILE_SIZE - 8, 13 * TILE_SIZE + 8}) {
        ExpectPixelColor(image, x, TILE_SIZE / 2, side);
        ExpectPixelColor(image, x, TILE_SIZE + TILE_SIZE / 2, side);
    }
    ExpectPixelColor(image, 11 * TILE_SIZE - 17, TILE_SIZE / 2, BLACK);
    ExpectPixelColor(image, 13 * TILE_SIZE + 16, TILE_SIZE / 2, BLACK);
    UnloadImage(image);
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
    ExpectFallbackColor(image, 5, 2, Color{90, 92, 110, 255}); // bottom, anchored one tile up
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
    const int bottomMiddleY = 3 * TILE_SIZE + TILE_SIZE / 2;
    ExpectPixelColor(image, leftEdgeX + TILE_SIZE / 8, bottomMiddleY,
                     GetImageColor(image, leftEdgeX + TILE_SIZE / 2, bottomMiddleY));
    ExpectPixelColor(image, rightEdgeX - TILE_SIZE / 8, bottomMiddleY,
                     GetImageColor(image, rightEdgeX - TILE_SIZE / 2, bottomMiddleY));
    UnloadImage(image);

    Room corridorRoom = MakeRoom({1, 0}, BiomeType::Lobby, 5, 2);
    auto corridorDoorState = std::make_shared<DoorInstance>();
    Doorway northDoor;
    northDoor.direction = Direction::North;
    northDoor.offset = 1;
    northDoor.width = 1;
    northDoor.corridorTiles = TileRect{6, 0, 1, 2};
    northDoor.doorState = corridorDoorState;
    corridorRoom.Layout().doors.push_back(northDoor);

    const int corridorLeftStripX = 6 * TILE_SIZE - TILE_SIZE / 8;
    const int corridorRightStripX = 7 * TILE_SIZE + TILE_SIZE / 8;
    const int corridorWallSampleY = TILE_SIZE / 2;
    Image hiddenCorridor = Render(walls, {{&corridorRoom, 1.0f}}, target, false);
    ExpectPixelColor(hiddenCorridor, corridorLeftStripX, corridorWallSampleY, BLACK);
    ExpectPixelColor(hiddenCorridor, corridorRightStripX, corridorWallSampleY, BLACK);
    UnloadImage(hiddenCorridor);

    corridorDoorState->opening = true;
    corridorDoorState->fadeProgress = DoorVisualRules::FadeDuration * 0.5f;
    Image openingCorridor = Render(walls, {{&corridorRoom, 1.0f}}, target, false);
    const Color openingLeft = GetImageColor(openingCorridor, corridorLeftStripX, corridorWallSampleY);
    const Color openingRight = GetImageColor(openingCorridor, corridorRightStripX, corridorWallSampleY);
    assert(openingLeft.r > 0 && openingLeft.g > 0 && openingLeft.b > 0);
    assert(openingRight.r > 0 && openingRight.g > 0 && openingRight.b > 0);
    UnloadImage(openingCorridor);

    corridorDoorState->open = true;
    Image openCorridor = Render(walls, {{&corridorRoom, 1.0f}}, target, false);
    const Color openLeft = GetImageColor(openCorridor, corridorLeftStripX, corridorWallSampleY);
    const Color openRight = GetImageColor(openCorridor, corridorRightStripX, corridorWallSampleY);
    assert(openLeft.r > openingLeft.r && openLeft.g > openingLeft.g && openLeft.b > openingLeft.b);
    assert(openRight.r > openingRight.r && openRight.g > openingRight.g && openRight.b > openingRight.b);
    UnloadImage(openCorridor);

    Room southRoom = MakeRoom({2, 0}, BiomeType::Lobby, 5, 2);
    auto southDoorState = std::make_shared<DoorInstance>();
    Doorway southDoor;
    southDoor.direction = Direction::South;
    southDoor.offset = 1;
    southDoor.width = 1;
    southDoor.corridorTiles = TileRect{6, 4, 1, 2};
    southDoor.doorState = southDoorState;
    southRoom.Layout().doors.push_back(southDoor);

    const int capY = 3 * TILE_SIZE + TILE_SIZE / 2;
    const int capTopY = 2 * TILE_SIZE + TILE_SIZE / 2;
    const int capLeftX = 6 * TILE_SIZE - TILE_SIZE / 8;
    const int capRightX = 7 * TILE_SIZE + TILE_SIZE / 8;
    const int outerLeftX = 5 * TILE_SIZE + TILE_SIZE / 8;
    const int outerRightX = 8 * TILE_SIZE - TILE_SIZE / 8;
    Image closedSouth = Render(walls, {{&southRoom, 1.0f}}, target, true);
    const Color closedLeft = GetImageColor(closedSouth, capLeftX, capY);
    const Color closedRight = GetImageColor(closedSouth, capRightX, capY);
    const Color closedTopLeft = GetImageColor(closedSouth, capLeftX, capTopY);
    const Color closedTopRight = GetImageColor(closedSouth, capRightX, capTopY);
    ExpectPixelColor(closedSouth, capLeftX, capY,
                     GetImageColor(closedSouth, 5 * TILE_SIZE + TILE_SIZE / 2, capY));
    ExpectPixelColor(closedSouth, capRightX, capY,
                     GetImageColor(closedSouth, 7 * TILE_SIZE + TILE_SIZE / 2, capY));
    const Color closedOuterLeft = GetImageColor(closedSouth, outerLeftX, capY);
    const Color closedOuterRight = GetImageColor(closedSouth, outerRightX, capY);
    UnloadImage(closedSouth);

    southDoorState->opening = true;
    southDoorState->fadeProgress = DoorVisualRules::FadeDuration * 0.5f;
    Image openingSouth = Render(walls, {{&southRoom, 1.0f}}, target, true);
    const Color sideFallback = ProceduralSideColor(6, 2, fallbackBase);
    ExpectPixelColor(openingSouth, capLeftX, capY, BlendOver(sideFallback, closedLeft, 0.5f));
    ExpectPixelColor(openingSouth, capRightX, capY, BlendOver(sideFallback, closedRight, 0.5f));
    ExpectPixelColor(openingSouth, capLeftX, capTopY,
                     BlendOver(sideFallback, closedTopLeft, 0.5f));
    ExpectPixelColor(openingSouth, capRightX, capTopY,
                     BlendOver(sideFallback, closedTopRight, 0.5f));
    ExpectPixelColor(openingSouth, outerLeftX, capY, closedOuterLeft);
    ExpectPixelColor(openingSouth, outerRightX, capY, closedOuterRight);
    UnloadImage(openingSouth);

    southDoorState->open = true;
    Image openSouth = Render(walls, {{&southRoom, 1.0f}}, target, true);
    ExpectPixelColor(openSouth, capLeftX, capY, sideFallback);
    ExpectPixelColor(openSouth, capRightX, capY, sideFallback);
    ExpectPixelColor(openSouth, capLeftX, capTopY, sideFallback);
    ExpectPixelColor(openSouth, capRightX, capTopY, sideFallback);
    ExpectPixelColor(openSouth, 6 * TILE_SIZE, capY, BLACK);
    ExpectPixelColor(openSouth, 7 * TILE_SIZE + TILE_SIZE / 4, capY,
                     GetImageColor(openSouth, outerRightX, capY));
    UnloadImage(openSouth);

    Room horizontalEast = MakeRoom({5, 0}, BiomeType::Lobby, 10, 1, 3, 4);
    auto horizontalEastState = std::make_shared<DoorInstance>();
    Doorway horizontalEastDoor;
    horizontalEastDoor.direction = Direction::East;
    horizontalEastDoor.offset = 1;
    horizontalEastDoor.width = 2;
    horizontalEastDoor.corridorTiles = TileRect{13, 2, 2, 2};
    horizontalEastDoor.doorState = horizontalEastState;
    horizontalEast.Layout().doors.push_back(horizontalEastDoor);

    const int eastUpperX = 13 * TILE_SIZE + TILE_SIZE / 8;
    const int eastUpperTopY = TILE_SIZE / 2;
    const int eastUpperY = TILE_SIZE + TILE_SIZE / 2;
    const int eastBottomTopY = 2 * TILE_SIZE + TILE_SIZE / 2;
    const int eastBottomY = 3 * TILE_SIZE + TILE_SIZE / 2;
    Image closedEast = Render(walls, {{&horizontalEast, 1.0f}}, target, true);
    const Color closedUpperTop = GetImageColor(closedEast, eastUpperX, eastUpperTopY);
    const Color closedUpper = GetImageColor(closedEast, eastUpperX, eastUpperY);
    const Color closedBottomTop = GetImageColor(closedEast, eastUpperX, eastBottomTopY);
    const Color closedBottom = GetImageColor(closedEast, eastUpperX, eastBottomY);
    UnloadImage(closedEast);

    horizontalEastState->opening = true;
    horizontalEastState->fadeProgress = DoorVisualRules::FadeDuration * 0.5f;
    Image openingEast = Render(walls, {{&horizontalEast, 1.0f}}, target, true);
    const Color sideFallbackBottom = ProceduralSideColor(13, 2, fallbackBase);
    ExpectPixelColor(openingEast, eastUpperX, eastUpperTopY,
                     closedUpperTop);
    ExpectPixelColor(openingEast, eastUpperX, eastUpperY,
                     closedUpper);
    ExpectPixelColor(openingEast, eastUpperX, eastBottomTopY,
                     BlendOver(sideFallbackBottom, closedBottomTop, 0.5f));
    ExpectPixelColor(openingEast, eastUpperX, eastBottomY,
                     BlendOver(sideFallbackBottom, closedBottom, 0.5f));
    UnloadImage(openingEast);

    horizontalEastState->open = true;
    Image openEast = Render(walls, {{&horizontalEast, 1.0f}}, target, true);
    ExpectPixelColor(openEast, eastUpperX, eastUpperTopY,
                     closedUpperTop);
    ExpectPixelColor(openEast, eastUpperX, eastUpperY,
                     closedUpper);
    ExpectPixelColor(openEast, eastUpperX, eastBottomTopY,
                     BlendOver(sideFallbackBottom, closedBottomTop));
    ExpectPixelColor(openEast, eastUpperX, eastBottomY,
                     BlendOver(sideFallbackBottom, closedBottom));
    UnloadImage(openEast);

    Room moduleRoom = MakeRoom({8, 0}, BiomeType::Lobby, 2, 2, 4, 4);
    Image moduleFallback = Render(walls, {{&moduleRoom, 1.0f}}, target, true);
    for (const int tileX : {2, 3, 4, 5}) {
        ExpectFallbackColor(moduleFallback, tileX, 0, fallbackBase);
        ExpectFallbackColor(moduleFallback, tileX, 1, fallbackBase);
        ExpectFallbackColor(moduleFallback, tileX, 4, fallbackBase);
        ExpectFallbackColor(moduleFallback, tileX, 5, fallbackBase);
        ExpectPixelColor(moduleFallback, tileX * TILE_SIZE + TILE_SIZE / 2,
                         6 * TILE_SIZE + TILE_SIZE / 2, BLACK);
    }
    for (const int tileY : {2, 3, 4, 5}) {
        ExpectFallbackPixelColor(moduleFallback, 2 * TILE_SIZE - 8,
                                 tileY * TILE_SIZE + TILE_SIZE / 2, fallbackBase);
        ExpectFallbackPixelColor(moduleFallback, 6 * TILE_SIZE + 8,
                                 tileY * TILE_SIZE + TILE_SIZE / 2, fallbackBase);
    }
    ExpectPixelColor(moduleFallback, 2 * TILE_SIZE - 17, 3 * TILE_SIZE + 32, BLACK);
    ExpectPixelColor(moduleFallback, 6 * TILE_SIZE + 17, 3 * TILE_SIZE + 32, BLACK);
    UnloadImage(moduleFallback);
}

} // namespace

int main() {
    TemporaryAssets assets;
    AddTestSprites();
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 64, "Wall rendering tests");
    assert(IsWindowReady());
    auto target = LoadRenderTexture(26 * TILE_SIZE, 10 * TILE_SIZE);
    assert(target.id != 0);
    TestTwoTileWallRendering(target);
    TestVerticalPassageTerminalSideRendering(target);
    TestWallCatalogsAndLayers(target);
    TestFallback(target);
    UnloadRenderTexture(target);
    CloseWindow();
    std::cout << "Wall rendering tests passed\n";
}
