#include "reward_pouch.h"

#include <algorithm>

const std::array<RewardPouchDefinition, 3>& GetRewardPouchDefinitions() {
    // Existing inventory IDs: 101 = tira de couro, 102 = lingote de bronze.
    static const std::array<RewardPouchDefinition, 3> definitions{{
        {"small", 12, 20, {{101, 20, 1, 1}}},
        {"medium", 24, 36, {{101, 50, 1, 2}, {102, 20, 1, 1}}},
        {"big", 48, 72, {{101, 100, 2, 3}, {102, 50, 1, 2}}}
    }};
    return definitions;
}

const RewardPouchDefinition& GetRewardPouchDefinition(RewardPouchType type) {
    return GetRewardPouchDefinitions().at(static_cast<size_t>(type));
}

Rectangle RewardPouchInstance::Hitbox() const {
    Vector2 size = GetRewardPouchDefinition(type).hitboxSize;
    return {position.x - size.x * 0.5f, position.y - size.y * 0.5f, size.x, size.y};
}

float RewardPouchInstance::Depth() const {
    Rectangle box = Hitbox();
    return box.y + box.height;
}

bool RewardPouchInstance::Empty() const {
    return coins <= 0 && std::none_of(items.begin(), items.end(),
                                    [](const RewardPouchItem& item) { return item.quantity > 0; });
}

namespace RewardPouchRules {
namespace {
bool ValidTable(const RewardPouchDropTable& table) {
    int sum = 0;
    for (const auto& entry : table) {
        if (entry.chancePercent < 0 || entry.chancePercent > 100 - sum ||
            static_cast<size_t>(entry.type) >= GetRewardPouchDefinitions().size()) return false;
        sum += entry.chancePercent;
    }
    return true;
}
}

std::optional<RewardPouchType> SelectDrop(const RewardPouchDropTable& table, int rollPercent) {
    if (rollPercent < 0 || rollPercent >= 100 || !ValidTable(table)) return std::nullopt;
    int cumulative = 0;
    for (const auto& entry : table) {
        cumulative += entry.chancePercent;
        if (rollPercent < cumulative) return entry.type;
    }
    return std::nullopt;
}

std::optional<RewardPouchInstance> Roll(const RewardPouchDropTable& table,
                                      Vector2 position, std::mt19937_64& rng) {
    if (table.empty() || !ValidTable(table)) return std::nullopt;
    const auto type = SelectDrop(table, std::uniform_int_distribution<int>(0, 99)(rng));
    if (!type) return std::nullopt;

    const auto& definition = GetRewardPouchDefinition(*type);
    RewardPouchInstance pouch;
    pouch.type = *type;
    pouch.position = position;
    pouch.coins = std::uniform_int_distribution<int>(definition.minCoins, definition.maxCoins)(rng);
    for (const auto& rule : definition.items) {
        // Independent rolls allow several item entries in the same pouch.
        if (std::uniform_int_distribution<int>(0, 99)(rng) < rule.chancePercent) {
            int quantity = std::uniform_int_distribution<int>(rule.minQuantity, rule.maxQuantity)(rng);
            pouch.items.push_back({rule.itemId, quantity});
        }
    }
    return pouch;
}
}
