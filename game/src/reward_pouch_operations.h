#pragma once

#include "reward_pouch.h"

class Room;
class Enemy;
struct PropInstance;
struct InventoryUIState;

struct RewardPouchPickup {
    Vector2 position{};
    int coins{0};
    std::vector<RewardPouchItem> items;
};

namespace RewardPouchOperations {
bool Spawn(Room& room, const RewardPouchDropTable& table, Vector2 position);
// Combat integration: only a live -> defeated transition can spawn a pouch.
bool DamageEnemy(Room& room, Enemy& enemy, float amount);
bool DamageProp(Room& room, PropInstance& prop, float amount);
// Only processes the supplied (current) room. Receipts contain delivered rewards only.
std::vector<RewardPouchPickup> Collect(Room& room, Rectangle playerBounds,
                                      InventoryUIState& inventory, bool playerAlive,
                                      bool inputBlocked);
}
