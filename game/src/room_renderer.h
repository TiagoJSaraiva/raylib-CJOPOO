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

    void DrawRoom(const Room& room, bool isActive, float visibility) const;
    void DrawRoomBackground(const Room& room, bool isActive, float visibility) const;
    void DrawRoomForeground(const Room& room, bool isActive, float visibility) const;
    void DrawForgeInstance(const ForgeInstance& forge, bool isActive) const;
    void DrawShopInstance(const ShopInstance& shop, bool isActive) const;
    void DrawChestInstance(const Chest& chest, bool isActive) const;
    void DrawDoorSprite(const Rectangle& hitbox,
                        Direction direction,
                        BiomeType biome,
                        float alpha) const;

private:
    void DrawForgeForRoom(const Room& room, bool isActive, float visibility) const;
    void DrawForgeSprite(const ForgeInstance& forge, bool isActive, float visibility) const;
    void DrawShopForRoom(const Room& room, bool isActive, float visibility) const;
    void DrawShopSprite(const ShopInstance& shop, bool isActive, float visibility) const;
    void DrawChestForRoom(const Room& room, bool isActive, float visibility) const;
    void DrawChestSprite(const Chest& chest, bool isActive, float visibility) const;

    struct DoorTextureSet {
        Texture2D front{};
        Texture2D side{};
    };

    const DoorTextureSet& DoorTexturesForBiome(BiomeType biome) const;
    void UnloadDoorTextures();

    Texture2D forgeTexture_{};
    Texture2D forgeBrokenTexture_{};
    std::array<Texture2D, 3> shopTextures_{};
    Texture2D chestTexture_{};
    std::array<DoorTextureSet, 3> biomeDoorTextures_{};
};
