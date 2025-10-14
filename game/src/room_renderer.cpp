#include "room_renderer.h"

#include "raylib.h"

#include "room_types.h"

namespace {

float TileToPixel(int tile) {
    return static_cast<float>(tile * TILE_SIZE);
}

Rectangle TileRectToPixels(const TileRect& rect) {
    return Rectangle{TileToPixel(rect.x), TileToPixel(rect.y), static_cast<float>(rect.width * TILE_SIZE), static_cast<float>(rect.height * TILE_SIZE)};
}

Rectangle DoorRectInsideRoom(const RoomLayout& layout, const Doorway& door) {
    Rectangle rect{};
    float baseX = TileToPixel(layout.tileBounds.x + door.offset);
    float span = static_cast<float>(door.width * TILE_SIZE);

    switch (door.direction) {
        case Direction::North:
            rect = Rectangle{baseX, TileToPixel(layout.tileBounds.y), span, static_cast<float>(TILE_SIZE)};
            break;
        case Direction::South:
            rect = Rectangle{
                baseX,
                TileToPixel(layout.tileBounds.y + layout.heightTiles) - static_cast<float>(TILE_SIZE),
                span,
                static_cast<float>(TILE_SIZE)
            };
            break;
        case Direction::East:
            rect = Rectangle{
                TileToPixel(layout.tileBounds.x + layout.widthTiles) - static_cast<float>(TILE_SIZE),
                TileToPixel(layout.tileBounds.y + door.offset),
                static_cast<float>(TILE_SIZE),
                span
            };
            break;
        case Direction::West:
            rect = Rectangle{
                TileToPixel(layout.tileBounds.x),
                TileToPixel(layout.tileBounds.y + door.offset),
                static_cast<float>(TILE_SIZE),
                span
            };
            break;
    }

    return rect;
}

} // namespace

void RoomRenderer::DrawRoom(const Room& room, bool isActive) const {
    const RoomLayout& layout = room.Layout();
    Rectangle floor = TileRectToPixels(layout.tileBounds);

    DrawRectangleRec(floor, Color{50, 52, 63, 255});

    for (const auto& door : layout.doors) {
        if (door.corridorTiles.width > 0 && door.corridorTiles.height > 0 && !door.sealed) {
            DrawRectangleRec(TileRectToPixels(door.corridorTiles), Color{80, 80, 80, 255});
        }
    }

    Color borderColor = isActive ? Color{220, 220, 120, 255} : Color{120, 120, 140, 255};
    DrawRectangleLinesEx(floor, isActive ? 6.0f : 4.0f, borderColor);

    DrawDoorwayDebug(room);
}

void RoomRenderer::DrawDoorwayDebug(const Room& room) const {
    const RoomLayout& layout = room.Layout();
    for (const auto& door : layout.doors) {
        Color doorColor = door.sealed ? Color{160, 60, 60, 255} : Color{180, 180, 120, 255};
        Rectangle doorRect = DoorRectInsideRoom(layout, door);
        DrawRectangleRec(doorRect, doorColor);
    }
}
