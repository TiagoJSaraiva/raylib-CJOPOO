#include "room_renderer.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <random>
#include <unordered_set>
#include <vector>

#include "room_types.h"

namespace {

float TileToPixel(int tile) {
    return static_cast<float>(tile * TILE_SIZE);
}

Rectangle TileRectToPixels(const TileRect& rect) {
    return Rectangle{TileToPixel(rect.x), TileToPixel(rect.y), static_cast<float>(rect.width * TILE_SIZE), static_cast<float>(rect.height * TILE_SIZE)};
}

Texture2D LoadFurnitureTexture(const char* path) {
    Texture2D texture{};
    if (path == nullptr) {
        return texture;
    }
    if (!FileExists(path)) {
        std::cerr << "[RoomRenderer] Texture not found: " << path << std::endl;
        return texture;
    }
    texture = LoadTexture(path);
    if (texture.id != 0) {
        SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    }
    return texture;
}

void UnloadTextureIfValid(Texture2D& texture) {
    if (texture.id != 0) {
        UnloadTexture(texture);
        texture = Texture2D{};
    }
}

struct TilePos {
    int x;
    int y;

    bool operator==(const TilePos& other) const noexcept {
        return x == other.x && y == other.y;
    }
};

struct TilePosHash {
    std::size_t operator()(const TilePos& pos) const noexcept {
        std::size_t h1 = std::hash<int>{}(pos.x);
        std::size_t h2 = std::hash<int>{}(pos.y);
        return h1 ^ (h2 << 1);
    }
};

struct DoorSpan {
    int rowY;
    int startX;
    int endX; // Exclusive
};

struct RoomGeometry {
    Rectangle floorRect;
    std::unordered_set<TilePos, TilePosHash> walkableTiles;
    std::vector<DoorSpan> northDoorSpans;
    std::vector<DoorSpan> southDoorSpans;
    std::vector<TileRect> corridorRects;
};

void AddTilesForRect(const TileRect& rect, std::unordered_set<TilePos, TilePosHash>& tiles) {
    if (rect.width <= 0 || rect.height <= 0) {
        return;
    }

    for (int y = rect.y; y < rect.y + rect.height; ++y) {
        for (int x = rect.x; x < rect.x + rect.width; ++x) {
            tiles.insert(TilePos{x, y});
        }
    }
}

unsigned char ClampToByte(int value);

Color RandomWallColorForTile(int tileX, int tileY, Color baseColor) {
    std::uint64_t seedX = static_cast<std::uint64_t>(static_cast<std::int64_t>(tileX));
    std::uint64_t seedY = static_cast<std::uint64_t>(static_cast<std::int64_t>(tileY));
    std::uint64_t seed = seedX * 0x9e3779b97f4a7c15ULL ^ seedY;
    seed ^= (seed >> 23);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> delta(-12, 12);
    baseColor.r = ClampToByte(static_cast<int>(baseColor.r) + delta(rng));
    baseColor.g = ClampToByte(static_cast<int>(baseColor.g) + delta(rng));
    baseColor.b = ClampToByte(static_cast<int>(baseColor.b) + delta(rng));
    return baseColor;
}

unsigned char ClampToByte(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return static_cast<unsigned char>(value);
}

Color OffsetRgb(Color color, int delta) {
    color.r = ClampToByte(static_cast<int>(color.r) + delta);
    color.g = ClampToByte(static_cast<int>(color.g) + delta);
    color.b = ClampToByte(static_cast<int>(color.b) + delta);
    return color;
}

bool TileInDoorSpan(const TilePos& tile, const std::vector<DoorSpan>& spans) {
    for (const DoorSpan& span : spans) {
        if (tile.y == span.rowY && tile.x >= span.startX && tile.x < span.endX) {
            return true;
        }
    }
    return false;
}

void DrawNorthWallColumn(int tileX, int topTileY, const Color& baseColor) {
    constexpr float kWallHeightTiles = 1.0f;
    float x = TileToPixel(tileX);
    float bottom = TileToPixel(topTileY);
    float height = static_cast<float>(TILE_SIZE) * kWallHeightTiles;

    Rectangle wallRect{x, bottom - height, static_cast<float>(TILE_SIZE), height};
    DrawRectangleRec(wallRect, baseColor);

    const float trimHeight = height * 0.2f;
    if (trimHeight > 0.0f) {
        Rectangle trimRect{x, bottom - height, static_cast<float>(TILE_SIZE), trimHeight};
        DrawRectangleRec(trimRect, OffsetRgb(baseColor, 25));
    }
}

void DrawSouthWallColumn(int tileX, int floorTileY, const Color& baseColor) {
    const float tileSize = static_cast<float>(TILE_SIZE);
    float x = TileToPixel(tileX);
    float tileTop = TileToPixel(floorTileY);

    Rectangle baseRect{x, tileTop, tileSize, tileSize};
    DrawRectangleRec(baseRect, baseColor);

    const float highlightHeight = tileSize * 0.18f;
    if (highlightHeight > 0.0f) {
        Rectangle highlightRect{x, tileTop, tileSize, highlightHeight};
        DrawRectangleRec(highlightRect, OffsetRgb(baseColor, 24));
    }

    const float midShadeHeight = tileSize * 0.32f;
    if (midShadeHeight > 0.0f) {
        Rectangle midShadeRect{x, tileTop + highlightHeight, tileSize, midShadeHeight};
        DrawRectangleRec(midShadeRect, OffsetRgb(baseColor, 8));
    }

    const float shadowHeight = tileSize * 0.24f;
    if (shadowHeight > 0.0f) {
        Rectangle shadowRect{x, tileTop + tileSize - shadowHeight, tileSize, shadowHeight};
        DrawRectangleRec(shadowRect, OffsetRgb(baseColor, -34));
    }
}

Color FloorColorForBiome(BiomeType biome) {
    switch (biome) {
        case BiomeType::Cave:
            return Color{58, 62, 70, 255};
        case BiomeType::Mansion:
            return Color{72, 54, 42, 255};
        case BiomeType::Dungeon:
            return Color{46, 66, 56, 255};
        case BiomeType::Lobby:
            return Color{50, 52, 63, 255};
        default:
            return Color{50, 52, 63, 255};
    }
}

Color WallBaseColorForBiome(BiomeType biome) {
    switch (biome) {
        case BiomeType::Cave:
            return Color{108, 108, 116, 255};
        case BiomeType::Mansion:
            return Color{128, 88, 56, 255};
        case BiomeType::Dungeon:
            return Color{76, 126, 86, 255};
        case BiomeType::Lobby:
            return Color{90, 92, 110, 255};
        default:
            return Color{90, 92, 110, 255};
    }
}

RoomGeometry BuildRoomGeometry(const RoomLayout& layout) {
    RoomGeometry geometry{};
    geometry.floorRect = TileRectToPixels(layout.tileBounds);

    AddTilesForRect(layout.tileBounds, geometry.walkableTiles);

    for (const auto& door : layout.doors) {
        if (door.corridorTiles.width > 0 && door.corridorTiles.height > 0 && !door.sealed) {
            geometry.corridorRects.push_back(door.corridorTiles);
            AddTilesForRect(door.corridorTiles, geometry.walkableTiles);
        }

        if (door.sealed) {
            continue;
        }

        if (door.direction == Direction::North) {
            DoorSpan span{};
            span.rowY = layout.tileBounds.y;
            span.startX = layout.tileBounds.x + door.offset;
            span.endX = span.startX + door.width;
            geometry.northDoorSpans.push_back(span);

            if (door.corridorTiles.height > 0) {
                for (int y = door.corridorTiles.y; y < door.corridorTiles.y + door.corridorTiles.height; ++y) {
                    DoorSpan corridorSpan{y, door.corridorTiles.x, door.corridorTiles.x + door.corridorTiles.width};
                    geometry.northDoorSpans.push_back(corridorSpan);
                }
            }
        } else if (door.direction == Direction::South) {
            DoorSpan span{};
            span.rowY = layout.tileBounds.y + layout.heightTiles - 1;
            span.startX = layout.tileBounds.x + door.offset;
            span.endX = span.startX + door.width;
            geometry.southDoorSpans.push_back(span);

            if (door.corridorTiles.height > 0) {
                for (int y = door.corridorTiles.y; y < door.corridorTiles.y + door.corridorTiles.height; ++y) {
                    DoorSpan corridorSpan{y, door.corridorTiles.x, door.corridorTiles.x + door.corridorTiles.width};
                    geometry.southDoorSpans.push_back(corridorSpan);
                }
            }
        }
    }

    return geometry;
}

} // namespace

RoomRenderer::RoomRenderer() {
    forgeTexture_ = LoadFurnitureTexture("assets/img/furniture/forja/Forja.png");
    forgeBrokenTexture_ = LoadFurnitureTexture("assets/img/furniture/forja/Forja_broken.png");
    shopTextures_[0] = LoadFurnitureTexture("assets/img/furniture/loja/Loja1.png");
    shopTextures_[1] = LoadFurnitureTexture("assets/img/furniture/loja/Loja2.png");
    shopTextures_[2] = LoadFurnitureTexture("assets/img/furniture/loja/Loja3.png");
    chestTexture_ = LoadFurnitureTexture("assets/img/furniture/bau/Bau.png");
}

RoomRenderer::~RoomRenderer() {
    UnloadTextureIfValid(forgeTexture_);
    UnloadTextureIfValid(forgeBrokenTexture_);
    for (Texture2D& texture : shopTextures_) {
        UnloadTextureIfValid(texture);
    }
    UnloadTextureIfValid(chestTexture_);
}

void RoomRenderer::DrawRoomBackground(const Room& room, bool isActive) const {
    const RoomLayout& layout = room.Layout();
    RoomGeometry geometry = BuildRoomGeometry(layout);

    Color floorColor = FloorColorForBiome(room.GetBiome());
    DrawRectangleRec(geometry.floorRect, floorColor);

    Color corridorColor = OffsetRgb(floorColor, 14);
    for (const TileRect& corridor : geometry.corridorRects) {
        DrawRectangleRec(TileRectToPixels(corridor), corridorColor);
    }

    Color wallBase = WallBaseColorForBiome(room.GetBiome());
    for (const TilePos& tile : geometry.walkableTiles) {
        TilePos northNeighbor{tile.x, tile.y - 1};
        if (geometry.walkableTiles.find(northNeighbor) == geometry.walkableTiles.end() && !TileInDoorSpan(tile, geometry.northDoorSpans)) {
            Color wallColor = RandomWallColorForTile(tile.x, tile.y - 1, wallBase);
            DrawNorthWallColumn(tile.x, tile.y, wallColor);
        }
    }
}

void RoomRenderer::DrawRoomForeground(const Room& room, bool isActive) const {
    const RoomLayout& layout = room.Layout();
    RoomGeometry geometry = BuildRoomGeometry(layout);

    Color wallBase = WallBaseColorForBiome(room.GetBiome());
    for (const TilePos& tile : geometry.walkableTiles) {
        TilePos southNeighbor{tile.x, tile.y + 1};
        if (geometry.walkableTiles.find(southNeighbor) == geometry.walkableTiles.end() && !TileInDoorSpan(tile, geometry.southDoorSpans)) {
            Color wallColor = RandomWallColorForTile(tile.x, tile.y + 1, wallBase);
            DrawSouthWallColumn(tile.x, tile.y, wallColor);
        }
    }

    if (!isActive) {
        DrawForgeForRoom(room, isActive);
        DrawShopForRoom(room, isActive);
        DrawChestForRoom(room, isActive);
    }
}

void RoomRenderer::DrawForgeForRoom(const Room& room, bool isActive) const {
    const ForgeInstance* forge = room.GetForge();
    if (forge == nullptr) {
        return;
    }
    DrawForgeSprite(*forge, isActive);
}

void RoomRenderer::DrawForgeSprite(const ForgeInstance& forge, bool isActive) const {
    const Texture2D* texture = (forge.state == ForgeState::Broken) ? &forgeBrokenTexture_ : &forgeTexture_;
    if (texture->id == 0) {
        return;
    }

    Rectangle src{0.0f, 0.0f, static_cast<float>(texture->width), static_cast<float>(texture->height)};
    const float tileSize = static_cast<float>(TILE_SIZE);
    const float desiredWidth = tileSize * 2.6f;
    float scale = (src.width > 0.0f) ? (desiredWidth / src.width) : 1.0f;
    if (scale <= 0.0f) {
        scale = 1.0f;
    }

    Rectangle dest{};
    dest.width = desiredWidth;
    dest.height = src.height * scale;
    dest.x = forge.anchorX - dest.width * 0.5f;
    dest.y = forge.anchorY - dest.height;

    Color tint = WHITE;
    if (!isActive) {
        tint = Color{255, 255, 255, 180};
    }

    DrawTexturePro(*texture, src, dest, Vector2{0.0f, 0.0f}, 0.0f, tint);
}

void RoomRenderer::DrawForgeInstance(const ForgeInstance& forge, bool isActive) const {
    DrawForgeSprite(forge, isActive);
}

void RoomRenderer::DrawShopForRoom(const Room& room, bool isActive) const {
    const ShopInstance* shop = room.GetShop();
    if (shop == nullptr) {
        return;
    }
    DrawShopSprite(*shop, isActive);
}

void RoomRenderer::DrawShopSprite(const ShopInstance& shop, bool isActive) const {
    int variant = std::clamp(shop.textureVariant, 0, static_cast<int>(shopTextures_.size()) - 1);
    const Texture2D& texture = shopTextures_[variant];
    if (texture.id == 0) {
        return;
    }

    Rectangle src{0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height)};
    const float tileSize = static_cast<float>(TILE_SIZE);
    const float desiredWidth = tileSize * 3.2f;
    float scale = (src.width > 0.0f) ? (desiredWidth / src.width) : 1.0f;
    if (scale <= 0.0f) {
        scale = 1.0f;
    }

    Rectangle dest{};
    dest.width = desiredWidth;
    dest.height = src.height * scale;
    dest.x = shop.anchorX - dest.width * 0.5f;
    dest.y = shop.anchorY - dest.height;

    Color tint = isActive ? WHITE : Color{255, 255, 255, 180};
    DrawTexturePro(texture, src, dest, Vector2{0.0f, 0.0f}, 0.0f, tint);
}

void RoomRenderer::DrawShopInstance(const ShopInstance& shop, bool isActive) const {
    DrawShopSprite(shop, isActive);
}

void RoomRenderer::DrawChestForRoom(const Room& room, bool isActive) const {
    const Chest* chest = room.GetChest();
    if (chest == nullptr) {
        return;
    }
    DrawChestSprite(*chest, isActive);
}

void RoomRenderer::DrawChestSprite(const Chest& chest, bool isActive) const {
    if (chestTexture_.id == 0) {
        return;
    }

    Rectangle src{0.0f, 0.0f, static_cast<float>(chestTexture_.width), static_cast<float>(chestTexture_.height)};
    const float tileSize = static_cast<float>(TILE_SIZE);
    const float desiredWidth = tileSize * 1.6f;
    float scale = (src.width > 0.0f) ? (desiredWidth / src.width) : 1.0f;
    if (scale <= 0.0f) {
        scale = 1.0f;
    }

    Rectangle dest{};
    dest.width = desiredWidth;
    dest.height = src.height * scale;
    dest.x = chest.AnchorX() - dest.width * 0.5f;
    dest.y = chest.AnchorY() - dest.height;

    Color tint = isActive ? WHITE : Color{255, 255, 255, 180};
    DrawTexturePro(chestTexture_, src, dest, Vector2{0.0f, 0.0f}, 0.0f, tint);
}

void RoomRenderer::DrawChestInstance(const Chest& chest, bool isActive) const {
    DrawChestSprite(chest, isActive);
}

void RoomRenderer::DrawRoom(const Room& room, bool isActive) const {
    DrawRoomBackground(room, isActive);
    DrawRoomForeground(room, isActive);
}
