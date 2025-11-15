#pragma once

#include "raylib.h"
#include "room.h"

#include <array>

class RoomRenderer {
public:
    RoomRenderer();
    ~RoomRenderer();

    RoomRenderer(const RoomRenderer&) = delete;
    RoomRenderer& operator=(const RoomRenderer&) = delete;

    void DrawRoom(const Room& room, bool isActive) const;
    void DrawRoomBackground(const Room& room, bool isActive) const;
    void DrawRoomForeground(const Room& room, bool isActive) const;
    void DrawForgeInstance(const ForgeInstance& forge, bool isActive) const;
    void DrawShopInstance(const ShopInstance& shop, bool isActive) const;
    void DrawChestInstance(const Chest& chest, bool isActive) const;

private:
    void DrawForgeForRoom(const Room& room, bool isActive) const;
    void DrawForgeSprite(const ForgeInstance& forge, bool isActive) const;
    void DrawShopForRoom(const Room& room, bool isActive) const;
    void DrawShopSprite(const ShopInstance& shop, bool isActive) const;
    void DrawChestForRoom(const Room& room, bool isActive) const;
    void DrawChestSprite(const Chest& chest, bool isActive) const;

    Texture2D forgeTexture_{};
    Texture2D forgeBrokenTexture_{};
    std::array<Texture2D, 3> shopTextures_{};
    Texture2D chestTexture_{};
};
