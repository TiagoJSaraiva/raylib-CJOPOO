#include "room_manager.h"

#include "chest.h"
#include "room_types.h"

#include <algorithm>
#include <memory>

namespace {

float TileToPixel(int tile) {
    return static_cast<float>(tile * TILE_SIZE);
}

} // namespace

// Popula a sala recém-criada com seus recursos conforme o tipo.
void RoomManager::InitializeRoomFeatures(Room& room) {
    switch (room.GetType()) {
        case RoomType::Forge: {
            room.ClearChest();
            room.ClearShop();

            ForgeInstance forge{};
            forge.state = ForgeState::Working;

            const RoomLayout& layout = room.Layout();
            const TileRect& bounds = layout.tileBounds;

            const float tileSize = static_cast<float>(TILE_SIZE);
            constexpr float kFootprintWidthTiles = 2.0f;
            constexpr float kFootprintDepthTiles = 1.0f;

            const float availableWidth = static_cast<float>(layout.widthTiles);
            const float availableHeight = static_cast<float>(layout.heightTiles);

            const float horizontalMarginTiles = std::max(0.0f, (availableWidth - kFootprintWidthTiles) * 0.5f);
            const float verticalMarginTiles = std::max(0.0f, (availableHeight - kFootprintDepthTiles) * 0.5f);

            forge.anchorX = TileToPixel(bounds.x) + (horizontalMarginTiles + kFootprintWidthTiles * 0.5f) * tileSize;
            forge.anchorY = TileToPixel(bounds.y) + (verticalMarginTiles + kFootprintDepthTiles) * tileSize;

            forge.hitbox.width = (kFootprintWidthTiles + 1.0f) * tileSize;
            forge.hitbox.height = kFootprintDepthTiles * tileSize;
            forge.hitbox.x = forge.anchorX - forge.hitbox.width * 0.5f;
            forge.hitbox.y = forge.anchorY - forge.hitbox.height;

            forge.interactionRadius = tileSize * 2.2f;

            room.SetForge(forge);
            break;
        }
        case RoomType::Shop: {
            room.ClearChest();
            room.ClearForge();
            InitializeShopFeatures(room);
            break;
        }
        case RoomType::Chest: {
            room.ClearForge();
            room.ClearShop();
            InitializeChestFeatures(room, false);
            break;
        }
        case RoomType::Lobby: {
            room.ClearForge();
            room.ClearShop();
            InitializeChestFeatures(room, true);
            break;
        }
        default:
            room.ClearForge();
            room.ClearShop();
            room.ClearChest();
            break;
    }
}

// Calcula posicionamento e seed dos itens de loja.
void RoomManager::InitializeShopFeatures(Room& room) {
    const RoomLayout& layout = room.Layout();
    const TileRect& bounds = layout.tileBounds;

    const float tileSize = static_cast<float>(TILE_SIZE);
    constexpr float kFootprintWidthTiles = 3.0f;
    constexpr float kFootprintDepthTiles = 1.0f;

    ShopInstance shop{};
    shop.textureVariant = static_cast<int>(MakeRoomSeed(worldSeed_, room.GetCoords(), 0x51A7ULL) % 3ULL);
    shop.baseSeed = MakeRoomSeed(worldSeed_, room.GetCoords(), 0x5B0F5ULL);

    const float availableWidth = static_cast<float>(layout.widthTiles);
    const float availableHeight = static_cast<float>(layout.heightTiles);

    const float horizontalMarginTiles = std::max(0.0f, (availableWidth - kFootprintWidthTiles) * 0.5f);
    const float verticalMarginTiles = std::max(0.0f, (availableHeight - kFootprintDepthTiles) * 0.5f);

    shop.anchorX = TileToPixel(bounds.x) + (horizontalMarginTiles + kFootprintWidthTiles * 0.5f) * tileSize;
    shop.anchorY = TileToPixel(bounds.y) + (verticalMarginTiles + kFootprintDepthTiles) * tileSize;

    shop.hitbox.width = kFootprintWidthTiles * tileSize;  // Ajuste a largura da hitbox da loja aqui
    shop.hitbox.height = kFootprintDepthTiles * tileSize; // Ajuste a altura da hitbox da loja aqui
    shop.hitbox.x = shop.anchorX - shop.hitbox.width * 0.5f;
    shop.hitbox.y = shop.anchorY - shop.hitbox.height;

    shop.interactionRadius = tileSize * 2.4f;

    room.SetShop(shop);
}

// Instancia baú comum ou persistente e posiciona hitbox na sala.
void RoomManager::InitializeChestFeatures(Room& room, bool persistentPlayerChest) {
    const RoomLayout& layout = room.Layout();
    const TileRect& bounds = layout.tileBounds;

    const float tileSize = static_cast<float>(TILE_SIZE);
    constexpr float kFootprintWidthTiles = 1.6f;
    constexpr float kFootprintDepthTiles = 1.0f;

    const float availableWidth = static_cast<float>(layout.widthTiles);
    const float availableHeight = static_cast<float>(layout.heightTiles);

    const float horizontalMarginTiles = std::max(0.0f, (availableWidth - kFootprintWidthTiles) * 0.5f);
    const float verticalMarginTiles = std::max(0.0f, (availableHeight - kFootprintDepthTiles) * 0.5f);

    float anchorX = TileToPixel(bounds.x) + (horizontalMarginTiles + kFootprintWidthTiles * 0.5f) * tileSize;
    float anchorY = TileToPixel(bounds.y) + (verticalMarginTiles + kFootprintDepthTiles) * tileSize;

    Rectangle hitbox{};
    hitbox.width = kFootprintWidthTiles * tileSize;
    hitbox.height = kFootprintDepthTiles * tileSize * 0.9f;
    hitbox.x = anchorX - hitbox.width * 0.5f;
    hitbox.y = anchorY - hitbox.height;

    float interactionRadius = tileSize * 1.8f;

    std::unique_ptr<Chest> chest;
    if (persistentPlayerChest) {
        constexpr int kPlayerChestCapacity = 24;
        chest = std::make_unique<PlayerChest>(anchorX, anchorY, interactionRadius, hitbox, kPlayerChestCapacity);
    } else {
        constexpr int kCommonChestCapacity = 4;
        std::uint64_t lootSeed = MakeRoomSeed(worldSeed_, room.GetCoords(), 0xC73AULL);
        chest = std::make_unique<CommonChest>(anchorX, anchorY, interactionRadius, hitbox, kCommonChestCapacity, lootSeed);
    }

    room.SetChest(std::move(chest));
}

// Replica portas conhecidas dos vizinhos e cria novas quando possível.
