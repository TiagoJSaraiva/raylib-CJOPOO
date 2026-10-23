#include "reward_pouch_operations.h"

#include "enemy.h"
#include "inventory_operations.h"
#include "room.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace RewardPouchOperations {
bool Spawn(Room& room, const RewardPouchDropTable& table, Vector2 position) {
    auto pouch = RewardPouchRules::Roll(table, position, room.RewardPouchRng());
    if (!pouch) return false;
    room.RewardPouches().push_back(std::move(*pouch));
    return true;
}

bool DamageEnemy(Room& room, Enemy& enemy, float amount) {
    if (!enemy.TakeDamage(amount)) return false;
    Spawn(room, enemy.GetRewardPouchDrops(), enemy.GetPosition());
    return true;
}

bool DamageProp(Room& room, PropInstance& prop, float amount) {
    if (!prop.TakeDamage(amount)) return false;
    Rectangle box = prop.Hitbox();
    Spawn(room, prop.definition->destructible->rewardPouchDrops,
          {box.x + box.width * 0.5f, box.y + box.height * 0.5f});
    return true;
}

std::vector<RewardPouchPickup> Collect(Room& room, Rectangle playerBounds,
                                      InventoryUIState& inventory, bool playerAlive,
                                      bool inputBlocked) {
    std::vector<RewardPouchPickup> receipts;
    if (!playerAlive || inventory.open || inputBlocked) return receipts;

    for (auto& pouch : room.RewardPouches()) {
        if (!CheckCollisionRecs(playerBounds, pouch.Hitbox())) continue;
        RewardPouchPickup receipt;
        receipt.position = pouch.position;
        // Keep any excess in the pouch rather than overflowing the currency counter.
        int roomForCoins = std::numeric_limits<int>::max() - std::max(0, inventory.coins);
        receipt.coins = std::min(pouch.coins, roomForCoins);
        inventory.coins += receipt.coins;
        pouch.coins -= receipt.coins;
        for (auto& item : pouch.items) {
            int inserted = InventoryOperations::AddItemToInventoryPartial(inventory, item.itemId, item.quantity);
            if (inserted > 0) {
                item.quantity -= inserted;
                receipt.items.push_back({item.itemId, inserted});
            }
        }
        pouch.items.erase(std::remove_if(pouch.items.begin(), pouch.items.end(),
                                        [](const RewardPouchItem& item) { return item.quantity <= 0; }),
                          pouch.items.end());
        if (receipt.coins > 0 || !receipt.items.empty()) receipts.push_back(std::move(receipt));
    }
    auto& pouches = room.RewardPouches();
    pouches.erase(std::remove_if(pouches.begin(), pouches.end(),
                                [](const RewardPouchInstance& pouch) { return pouch.Empty(); }),
                  pouches.end());
    return receipts;
}
}
