#include "room_collision_rules.h"

#include <algorithm>

namespace RoomCollisionRules {
namespace {

// Valores independentes para permitir ajuste fino do limite inferior por bioma.
constexpr int kLobbyBottomInsetPixels = 15;
constexpr int kCaveBottomInsetPixels = 15;
constexpr int kMansionBottomInsetPixels = 15;
constexpr int kDungeonBottomInsetPixels = 15;
constexpr int kDefaultBottomInsetPixels = 15;

} // namespace

int BottomInsetPixels(BiomeType biome) {
    switch (biome) {
        case BiomeType::Lobby: return kLobbyBottomInsetPixels;
        case BiomeType::Cave: return kCaveBottomInsetPixels;
        case BiomeType::Mansion: return kMansionBottomInsetPixels;
        case BiomeType::Dungeon: return kDungeonBottomInsetPixels;
        case BiomeType::Unknown: return kDefaultBottomInsetPixels;
    }
    return kDefaultBottomInsetPixels;
}

Rectangle RoomBoundsForCollision(const TileRect& tileBounds, BiomeType biome) {
    const float tileSize = static_cast<float>(TILE_SIZE);
    const float height = static_cast<float>(tileBounds.height) * tileSize;
    const float bottomInset = static_cast<float>(BottomInsetPixels(biome));

    return Rectangle{
        static_cast<float>(tileBounds.x) * tileSize,
        static_cast<float>(tileBounds.y) * tileSize,
        static_cast<float>(tileBounds.width) * tileSize,
        std::max(0.0f, height - bottomInset)
    };
}

} // namespace RoomCollisionRules
