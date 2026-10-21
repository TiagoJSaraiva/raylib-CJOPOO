#pragma once

#include "raylib.h"
#include "room_types.h"

namespace RoomCollisionRules {

// Recuo aplicado entre o limite visual inferior e o limite de movimento da sala.
int BottomInsetPixels(BiomeType biome);

// Constrói os limites de colisão em pixels para o retângulo de tiles da sala.
Rectangle RoomBoundsForCollision(const TileRect& tileBounds, BiomeType biome);

} // namespace RoomCollisionRules
