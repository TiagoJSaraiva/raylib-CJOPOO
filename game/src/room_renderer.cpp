#include "room_renderer.h"

#include <cstdint>
#include <functional>
#include <random>
#include <unordered_set>
#include <vector>

#include "raylib.h"

#include "room_types.h"

namespace {

float TileToPixel(int tile) {
    return static_cast<float>(tile * TILE_SIZE);
}

Rectangle TileRectToPixels(const TileRect& rect) {
    return Rectangle{TileToPixel(rect.x), TileToPixel(rect.y), static_cast<float>(rect.width * TILE_SIZE), static_cast<float>(rect.height * TILE_SIZE)};
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

Color RandomWallColorForTile(int tileX, int tileY) {
    std::uint64_t seedX = static_cast<std::uint64_t>(static_cast<std::int64_t>(tileX));
    std::uint64_t seedY = static_cast<std::uint64_t>(static_cast<std::int64_t>(tileY));
    std::uint64_t seed = seedX * 0x9e3779b97f4a7c15ULL ^ seedY;
    seed ^= (seed >> 23);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> baseDist(90, 160);
    std::uniform_int_distribution<int> accentDist(50, 120);
    unsigned char r = static_cast<unsigned char>(baseDist(rng));
    unsigned char g = static_cast<unsigned char>(baseDist(rng));
    unsigned char b = static_cast<unsigned char>(accentDist(rng));
    return Color{r, g, b, 255};
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

void RoomRenderer::DrawRoomBackground(const Room& room, bool isActive) const {
    const RoomLayout& layout = room.Layout();
    RoomGeometry geometry = BuildRoomGeometry(layout);

    DrawRectangleRec(geometry.floorRect, Color{50, 52, 63, 255});

    for (const TileRect& corridor : geometry.corridorRects) {
        DrawRectangleRec(TileRectToPixels(corridor), Color{80, 80, 80, 255});
    }

    for (const TilePos& tile : geometry.walkableTiles) {
        TilePos northNeighbor{tile.x, tile.y - 1};
        if (geometry.walkableTiles.find(northNeighbor) == geometry.walkableTiles.end() && !TileInDoorSpan(tile, geometry.northDoorSpans)) {
            Color wallColor = RandomWallColorForTile(tile.x, tile.y - 1);
            DrawNorthWallColumn(tile.x, tile.y, wallColor);
        }
    }
}

void RoomRenderer::DrawRoomForeground(const Room& room, bool isActive) const {
    const RoomLayout& layout = room.Layout();
    RoomGeometry geometry = BuildRoomGeometry(layout);

    for (const TilePos& tile : geometry.walkableTiles) {
        TilePos southNeighbor{tile.x, tile.y + 1};
        if (geometry.walkableTiles.find(southNeighbor) == geometry.walkableTiles.end() && !TileInDoorSpan(tile, geometry.southDoorSpans)) {
            Color wallColor = RandomWallColorForTile(tile.x, tile.y + 1);
            DrawSouthWallColumn(tile.x, tile.y, wallColor);
        }
    }
}

void RoomRenderer::DrawRoom(const Room& room, bool isActive) const {
    DrawRoomBackground(room, isActive);
    DrawRoomForeground(room, isActive);
}
