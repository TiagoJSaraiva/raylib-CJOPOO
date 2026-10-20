#include "wall_tiles.h"

#include "floor_rules.h"
#include "room_types.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <random>
#include <tuple>

namespace {

constexpr const char* kBiomeDirectories[] = {"lobby", "cave", "mansion", "dungeon"};
constexpr const char* kSectionDirectories[] = {"side", "upper", "bottom"};

std::size_t BiomeIndex(BiomeType biome) {
    switch (biome) {
        case BiomeType::Lobby: return 0;
        case BiomeType::Cave: return 1;
        case BiomeType::Mansion: return 2;
        case BiomeType::Dungeon: return 3;
        default: return 0;
    }
}

std::size_t SectionIndex(WallRules::Section section) {
    return static_cast<std::size_t>(section);
}

Color WallBaseColorForBiome(BiomeType biome) {
    switch (biome) {
        case BiomeType::Cave: return Color{108, 108, 116, 255};
        case BiomeType::Mansion: return Color{128, 88, 56, 255};
        case BiomeType::Dungeon: return Color{76, 126, 86, 255};
        case BiomeType::Lobby: return Color{90, 92, 110, 255};
        default: return Color{90, 92, 110, 255};
    }
}

unsigned char ClampToByte(int value) {
    return static_cast<unsigned char>(std::clamp(value, 0, 255));
}

Color OffsetRgb(Color color, int delta) {
    color.r = ClampToByte(static_cast<int>(color.r) + delta);
    color.g = ClampToByte(static_cast<int>(color.g) + delta);
    color.b = ClampToByte(static_cast<int>(color.b) + delta);
    return color;
}

Color RandomWallColorForTile(int tileX, int tileY, Color baseColor) {
    const std::uint64_t seedX = static_cast<std::uint64_t>(static_cast<std::int64_t>(tileX));
    const std::uint64_t seedY = static_cast<std::uint64_t>(static_cast<std::int64_t>(tileY));
    std::uint64_t seed = seedX * 0x9e3779b97f4a7c15ULL ^ seedY;
    seed ^= (seed >> 23);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> delta(-12, 12);
    baseColor.r = ClampToByte(static_cast<int>(baseColor.r) + delta(rng));
    baseColor.g = ClampToByte(static_cast<int>(baseColor.g) + delta(rng));
    baseColor.b = ClampToByte(static_cast<int>(baseColor.b) + delta(rng));
    return baseColor;
}

void DrawNorthWallColumn(int tileX, int topTileY, Color baseColor) {
    const float x = static_cast<float>(tileX * TILE_SIZE);
    const float bottom = static_cast<float>(topTileY * TILE_SIZE);
    const float height = static_cast<float>(TILE_SIZE);
    const Rectangle wallRect{x, bottom - height, static_cast<float>(TILE_SIZE), height};
    DrawRectangleRec(wallRect, baseColor);
    DrawRectangleRec(Rectangle{x, bottom - height, static_cast<float>(TILE_SIZE), height * 0.2f},
                     OffsetRgb(baseColor, 25));
}

void DrawSouthWallColumn(int tileX, int floorTileY, Color baseColor) {
    const float x = static_cast<float>(tileX * TILE_SIZE);
    const float y = static_cast<float>(floorTileY * TILE_SIZE);
    const float size = static_cast<float>(TILE_SIZE);
    DrawRectangleRec(Rectangle{x, y, size, size}, baseColor);
    DrawRectangleRec(Rectangle{x, y, size, size * 0.18f}, OffsetRgb(baseColor, 24));
    DrawRectangleRec(Rectangle{x, y + size * 0.18f, size, size * 0.32f}, OffsetRgb(baseColor, 8));
    DrawRectangleRec(Rectangle{x, y + size * 0.76f, size, size * 0.24f}, OffsetRgb(baseColor, -34));
}

void DrawSideWallTile(WallRules::Position position, Color baseColor) {
    const float x = static_cast<float>(position.x * TILE_SIZE);
    const float y = static_cast<float>(position.y * TILE_SIZE);
    const float size = static_cast<float>(TILE_SIZE);
    DrawRectangleRec(Rectangle{x, y, size, size}, baseColor);
    DrawRectangleRec(Rectangle{x, y, size * 0.16f, size}, OffsetRgb(baseColor, 18));
    DrawRectangleRec(Rectangle{x + size * 0.84f, y, size * 0.16f, size}, OffsetRgb(baseColor, -28));
}

Texture2D LoadWallTexture(const std::filesystem::path& path) {
    Image image = LoadImage(path.generic_string().c_str());
    if (image.data == nullptr || !FloorRules::IsSquareImage(image.width, image.height)) {
        std::cerr << "[WallTiles] Expected a readable square image: " << path.generic_string() << '\n';
        if (image.data != nullptr) UnloadImage(image);
        return Texture2D{};
    }
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    if (texture.id != 0) SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    return texture;
}

} // namespace

WallTiles::WallTiles() {
    namespace fs = std::filesystem;
    for (std::size_t biome = 0; biome < 4; ++biome) {
        for (std::size_t section = 0; section < 3; ++section) {
            auto& catalog = catalogs_[biome * 3 + section];
            const fs::path directory = fs::path("assets/wall") / kBiomeDirectories[biome] /
                                       kSectionDirectories[section];
            std::vector<fs::path> paths;
            std::error_code error;
            fs::directory_iterator iterator(directory, error);
            const fs::directory_iterator end;
            while (!error && iterator != end) {
                const auto entry = *iterator;
                std::error_code fileError;
                if (entry.is_regular_file(fileError)) paths.push_back(entry.path());
                iterator.increment(error);
            }
            if (error) {
                std::cerr << "[WallTiles] Cannot read " << directory.generic_string()
                          << ": " << error.message() << '\n';
            }
            std::sort(paths.begin(), paths.end());
            for (const auto& path : paths) {
                const auto name = path.filename().string();
                const auto metadata = FloorRules::ParseFilename(name);
                if (!metadata) {
                    if (path.extension() == ".png" || path.extension() == ".PNG") {
                        std::cerr << "[WallTiles] Invalid filename: " << path.generic_string() << '\n';
                    }
                    continue;
                }
                Texture2D texture = LoadWallTexture(path);
                if (texture.id == 0) continue;
                if (!FloorRules::AddWeight(catalog.cumulative, metadata->weight)) {
                    std::cerr << "[WallTiles] Weight sum overflow: " << path.generic_string() << '\n';
                    UnloadTexture(texture);
                    continue;
                }
                catalog.textures.push_back(texture);
            }
            if (catalog.textures.empty()) {
                std::cerr << "[WallTiles] No valid sprites in " << directory.generic_string()
                          << "; using procedural wall color.\n";
            }
        }
    }
}

WallTiles::~WallTiles() { Unload(); }

void WallTiles::Unload() {
    for (auto& catalog : catalogs_) {
        for (auto texture : catalog.textures) UnloadTexture(texture);
        catalog.textures.clear();
        catalog.cumulative.clear();
    }
    memory_.Reset(0);
}

void WallTiles::BeginRun(std::uint64_t seed) { memory_.Reset(seed); }

void WallTiles::DrawBackground(const std::vector<WallRules::RoomView>& rooms) const {
    Draw(WallRules::BuildFrame(rooms), false);
}

void WallTiles::DrawForeground(const std::vector<WallRules::RoomView>& rooms) const {
    Draw(WallRules::BuildFrame(rooms), true);
}

void WallTiles::Draw(const WallRules::Frame& frame, bool foreground) const {
    std::vector<const WallRules::Segment*> segments;
    segments.reserve(frame.size());
    for (const auto& entry : frame) {
        const auto& segment = entry.second;
        if ((segment.section == WallRules::Section::Bottom) == foreground) {
            segments.push_back(&segment);
        }
    }
    std::sort(segments.begin(), segments.end(), [](const auto* a, const auto* b) {
        return std::tie(a->position.y, a->position.x, a->section) <
               std::tie(b->position.y, b->position.x, b->section);
    });

    for (const auto* segmentPointer : segments) {
        const auto& segment = *segmentPointer;

        const auto catalogIndex = BiomeIndex(segment.biome) * 3 + SectionIndex(segment.section);
        const auto& catalog = catalogs_[catalogIndex];
        const auto& choice = memory_.GetOrCreate(segment.position, segment.section,
                                                  segment.biome, catalog.cumulative);
        if (choice.sprite == WallRules::NoSprite) {
            DrawFallback(segment);
            continue;
        }

        const Texture2D texture = catalog.textures[choice.sprite];
        const Rectangle source{0.0f, 0.0f, static_cast<float>(texture.width),
                               static_cast<float>(texture.height)};
        const Rectangle destination{static_cast<float>(segment.position.x * TILE_SIZE),
                                    static_cast<float>(segment.position.y * TILE_SIZE),
                                    static_cast<float>(TILE_SIZE), static_cast<float>(TILE_SIZE)};
        DrawTexturePro(texture, source, destination, Vector2{0.0f, 0.0f}, 0.0f,
                       ColorAlpha(WHITE, segment.visibility));
    }
}

void WallTiles::DrawFallback(const WallRules::Segment& segment) const {
    Color color = WallBaseColorForBiome(segment.biome);
    int variationY = segment.position.y;
    if (segment.section == WallRules::Section::Upper) variationY = segment.edgeTile.y - 1;
    else if (segment.section == WallRules::Section::Bottom) variationY = segment.edgeTile.y + 1;
    color = RandomWallColorForTile(segment.edgeTile.x, variationY, color);
    color = ColorAlpha(color, segment.visibility);

    if (segment.section == WallRules::Section::Upper) {
        DrawNorthWallColumn(segment.edgeTile.x, segment.edgeTile.y, color);
    } else if (segment.section == WallRules::Section::Bottom) {
        DrawSouthWallColumn(segment.edgeTile.x, segment.edgeTile.y, color);
    } else {
        DrawSideWallTile(segment.position, color);
    }
}
