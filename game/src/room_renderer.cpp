#include "room_renderer.h"

#include <algorithm>
#include <iostream>
#include <vector>

#include "room_types.h"

namespace {

// Ajustes visuais usados para posicionar sprites das portas por direção.
constexpr float VERTICAL_DOOR_SPRITE_ROOM_OFFSET_NORTH = 15.0f; // Offset exclusivo para portas Norte
constexpr float VERTICAL_DOOR_SPRITE_ROOM_OFFSET_SOUTH = 42.0f; // Offset exclusivo para portas Sul
constexpr float HORIZONTAL_DOOR_SPRITE_ROOM_OFFSET = 1.0f; // Ajusta distancia para portas Leste/Oeste
constexpr float HORIZONTAL_DOOR_SPRITE_HEIGHT_OFFSET = -30.0f; // Ajusta offset vertical adicional para portas Leste/Oeste

// Tenta carregar textura de mobiliário aplicando filtro adequado, logando falhas.
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

// Descarrega textura caso seja válida para evitar vazamentos.
void UnloadTextureIfValid(Texture2D& texture) {
    if (texture.id != 0) {
        UnloadTexture(texture);
        texture = Texture2D{};
    }
}

// Seleciona índice da textura de porta baseado no bioma.
std::size_t DoorTextureIndex(BiomeType biome) {
    switch (biome) {
        case BiomeType::Cave:
            return 0;
        case BiomeType::Dungeon:
            return 1;
        case BiomeType::Mansion:
            return 2;
        default:
            return 0;
    }
}

} // namespace

// Carrega texturas necessárias para renderizar props e portas.
RoomRenderer::RoomRenderer() {
    forgeTexture_ = LoadFurnitureTexture("assets/img/furniture/forja/Forja.png");
    forgeBrokenTexture_ = LoadFurnitureTexture("assets/img/furniture/forja/Forja_broken.png");
    shopTextures_[0] = LoadFurnitureTexture("assets/img/furniture/loja/Loja1.png");
    shopTextures_[1] = LoadFurnitureTexture("assets/img/furniture/loja/Loja2.png");
    shopTextures_[2] = LoadFurnitureTexture("assets/img/furniture/loja/Loja3.png");
    chestTexture_ = LoadFurnitureTexture("assets/img/furniture/bau/Bau.png");
    biomeDoorTextures_[0].front = LoadFurnitureTexture("assets/img/furniture/door/Caverna_door_front.png");
    biomeDoorTextures_[0].side = LoadFurnitureTexture("assets/img/furniture/door/Caverna_door_side.png");
    biomeDoorTextures_[1].front = LoadFurnitureTexture("assets/img/furniture/door/Dungeon_door_front.png");
    biomeDoorTextures_[1].side = LoadFurnitureTexture("assets/img/furniture/door/Dungeon_door_side.png");
    biomeDoorTextures_[2].front = LoadFurnitureTexture("assets/img/furniture/door/Mansao_door_front.png");
    biomeDoorTextures_[2].side = LoadFurnitureTexture("assets/img/furniture/door/Mansao_door_side.png");
}

// Libera texturas carregadas na destruição do renderer.
RoomRenderer::~RoomRenderer() {
    UnloadTextures();
}

void RoomRenderer::UnloadTextures() {
    floorTiles_.Unload();
    wallTiles_.Unload();
    UnloadTextureIfValid(forgeTexture_);
    UnloadTextureIfValid(forgeBrokenTexture_);
    for (Texture2D& texture : shopTextures_) {
        UnloadTextureIfValid(texture);
    }
    UnloadTextureIfValid(chestTexture_);
    UnloadDoorTextures();
}

// Desenha piso, corredores e paredes de fundo da sala.
void RoomRenderer::DrawRoomBackground(const Room& room, bool isActive, float visibility) const {
    DrawFloors({FloorRoomView{&room, visibility}});
    DrawRoomBackgroundWalls(room, isActive, visibility);
}

void RoomRenderer::BeginFloorRun(std::uint64_t seed) {
    floorTiles_.BeginRun(seed);
    wallTiles_.BeginRun(seed);
}

void RoomRenderer::DrawFloors(const std::vector<FloorRoomView>& rooms) const {
    floorTiles_.Draw(rooms);
}

void RoomRenderer::DrawWallBackground(const std::vector<FloorRoomView>& rooms) const {
    std::vector<WallRules::RoomView> wallRooms;
    wallRooms.reserve(rooms.size());
    for (const auto& view : rooms) wallRooms.push_back(WallRules::RoomView{view.room, view.visibility});
    wallTiles_.DrawBackground(wallRooms);
}

void RoomRenderer::DrawWallForeground(const std::vector<FloorRoomView>& rooms) const {
    std::vector<WallRules::RoomView> wallRooms;
    wallRooms.reserve(rooms.size());
    for (const auto& view : rooms) wallRooms.push_back(WallRules::RoomView{view.room, view.visibility});
    wallTiles_.DrawForeground(wallRooms);
}

void RoomRenderer::DrawWallForeground(const std::vector<FloorRoomView>& rooms,
                                      Rectangle playerBounds, float deltaSeconds) const {
    std::vector<WallRules::RoomView> wallRooms;
    wallRooms.reserve(rooms.size());
    for (const auto& view : rooms) wallRooms.push_back(WallRules::RoomView{view.room, view.visibility});
    wallTiles_.DrawForeground(wallRooms, playerBounds, deltaSeconds);
}

void RoomRenderer::DrawRoomBackgroundWalls(const Room& room, bool isActive, float visibility) const {
    (void)isActive;
    DrawWallBackground({FloorRoomView{&room, visibility}});
}

void RoomRenderer::DrawRoomForeground(const Room& room, bool isActive, float visibility) const {
    wallTiles_.DrawForeground({WallRules::RoomView{&room, visibility}});
    DrawRoomPropsForeground(room, isActive, visibility);
}

void RoomRenderer::DrawRoomPropsForeground(const Room& room, bool isActive, float visibility) const {
    if (!isActive) {
        DrawForgeForRoom(room, isActive, visibility);
        DrawShopForRoom(room, isActive, visibility);
        DrawChestForRoom(room, isActive, visibility);
    }
}

// Desenha forja da sala caso exista.
void RoomRenderer::DrawForgeForRoom(const Room& room, bool isActive, float visibility) const {
    const ForgeInstance* forge = room.GetForge();
    if (forge == nullptr) {
        return;
    }
    DrawForgeSprite(*forge, isActive, visibility);
}

// Renderiza sprite específico da forja (inteira ou quebrada).
void RoomRenderer::DrawForgeSprite(const ForgeInstance& forge, bool isActive, float visibility) const {
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
    tint = ColorAlpha(tint, visibility);

    DrawTexturePro(*texture, src, dest, Vector2{0.0f, 0.0f}, 0.0f, tint);
}

// Versão utilitária para desenhar forja com visibilidade total.
void RoomRenderer::DrawForgeInstance(const ForgeInstance& forge, bool isActive) const {
    DrawForgeSprite(forge, isActive, 1.0f);
}

// Desenha loja instalada na sala, se houver.
void RoomRenderer::DrawShopForRoom(const Room& room, bool isActive, float visibility) const {
    const ShopInstance* shop = room.GetShop();
    if (shop == nullptr) {
        return;
    }
    DrawShopSprite(*shop, isActive, visibility);
}

// Renderiza sprite da loja aplicando variante configurada.
void RoomRenderer::DrawShopSprite(const ShopInstance& shop, bool isActive, float visibility) const {
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
    tint = ColorAlpha(tint, visibility);
    DrawTexturePro(texture, src, dest, Vector2{0.0f, 0.0f}, 0.0f, tint);
}

// Helper para desenhar loja em contexto externo (HUD/debug).
void RoomRenderer::DrawShopInstance(const ShopInstance& shop, bool isActive) const {
    DrawShopSprite(shop, isActive, 1.0f);
}

// Desenha baú da sala respeitando visibilidade.
void RoomRenderer::DrawChestForRoom(const Room& room, bool isActive, float visibility) const {
    const Chest* chest = room.GetChest();
    if (chest == nullptr) {
        return;
    }
    DrawChestSprite(*chest, isActive, visibility);
}

// Renderiza sprite do baú compartilhado entre cofres comuns/player.
void RoomRenderer::DrawChestSprite(const Chest& chest, bool isActive, float visibility) const {
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

    Color tint = isActive ? WHITE : Color{255, 255, 255, 190};
    tint = ColorAlpha(tint, visibility);
    DrawTexturePro(chestTexture_, src, dest, Vector2{0.0f, 0.0f}, 0.0f, tint);
}

// Versão direta para desenhar baú sem fade de visibilidade.
void RoomRenderer::DrawChestInstance(const Chest& chest, bool isActive) const {
    DrawChestSprite(chest, isActive, 1.0f);
}

// Pipeline completo de desenho da sala (fundo + frente).
void RoomRenderer::DrawRoom(const Room& room, bool isActive, float visibility) const {
    DrawRoomBackground(room, isActive, visibility);
    DrawRoomForeground(room, isActive, visibility);
}

// Recupera conjunto de texturas (frontal/lateral) compatível com o bioma.
const RoomRenderer::DoorTextureSet& RoomRenderer::DoorTexturesForBiome(BiomeType biome) const {
    std::size_t index = DoorTextureIndex(biome);
    if (index >= biomeDoorTextures_.size()) {
        index = 0;
    }
    return biomeDoorTextures_[index];
}

// Libera texturas específicas das portas de cada bioma.
void RoomRenderer::UnloadDoorTextures() {
    for (DoorTextureSet& set : biomeDoorTextures_) {
        UnloadTextureIfValid(set.front);
        UnloadTextureIfValid(set.side);
    }
}

// Desenha sprite de porta orientado conforme direção e com alpha customizado.
void RoomRenderer::DrawDoorSprite(const Rectangle& hitbox,
                                  Direction direction,
                                  BiomeType biome,
                                  float alpha) const {
    if (alpha <= 0.0f) {
        return;
    }

    const DoorTextureSet& textures = DoorTexturesForBiome(biome);
    const Texture2D* texture = nullptr;
    bool frontView = (direction == Direction::North || direction == Direction::South);
    if (frontView) {
        texture = &textures.front;
    } else {
        texture = &textures.side;
    }

    if (texture == nullptr || texture->id == 0) {
        return;
    }

    Rectangle src{0.0f, 0.0f, static_cast<float>(texture->width), static_cast<float>(texture->height)};
    Rectangle dest{};

    if (frontView) {
        float scale = (src.width > 0.0f) ? (hitbox.width / src.width) : 1.0f;
        if (scale <= 0.0f) {
            scale = 1.0f;
        }
        dest.width = hitbox.width;
        dest.height = src.height * scale;
        dest.x = hitbox.x;
        float baseY = hitbox.y + hitbox.height;
        dest.y = baseY - dest.height - 38.0f;
        float verticalOffset = (direction == Direction::North)
                                   ? VERTICAL_DOOR_SPRITE_ROOM_OFFSET_NORTH
                                   : VERTICAL_DOOR_SPRITE_ROOM_OFFSET_SOUTH;
        dest.y += verticalOffset;
    } else {
        float scale = (src.height > 0.0f) ? (hitbox.height / src.height) : 1.0f;
        if (scale <= 0.0f) {
            scale = 1.0f;
        }
        dest.height = hitbox.height;
        dest.width = src.width * scale;
        dest.x = hitbox.x + (hitbox.width - dest.width) * 0.5f;
        dest.y = hitbox.y;
        if (HORIZONTAL_DOOR_SPRITE_ROOM_OFFSET != 0.0f) {
            // Ajusta distancia do sprite em relacao a sala (Leste empurra, Oeste puxa)
            dest.x += (direction == Direction::East ? HORIZONTAL_DOOR_SPRITE_ROOM_OFFSET : -HORIZONTAL_DOOR_SPRITE_ROOM_OFFSET);
        }
        dest.y += HORIZONTAL_DOOR_SPRITE_HEIGHT_OFFSET;
    }

    Color tint{255, 255, 255, static_cast<unsigned char>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)};
    DrawTexturePro(*texture, src, dest, Vector2{0.0f, 0.0f}, 0.0f, tint);
}
