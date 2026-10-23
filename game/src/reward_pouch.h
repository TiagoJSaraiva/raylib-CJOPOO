#pragma once

#include "raylib.h"

#include <array>
#include <optional>
#include <random>
#include <string>
#include <vector>

enum class RewardPouchType { Small, Medium, Big };

struct RewardPouchDrop {
    RewardPouchType type{RewardPouchType::Small};
    int chancePercent{0};
};
using RewardPouchDropTable = std::vector<RewardPouchDrop>;

struct RewardPouchItemRule {
    int itemId{0};
    int chancePercent{0};
    int minQuantity{1};
    int maxQuantity{1};
};

struct RewardPouchDefinition {
    std::string id;
    int minCoins{0};
    int maxCoins{0};
    std::vector<RewardPouchItemRule> items;
    Vector2 hitboxSize{24.0f, 16.0f};
    float drawWidth{32.0f};
};

struct RewardPouchItem {
    int itemId{0};
    int quantity{0};
};

struct RewardPouchInstance {
    RewardPouchType type{RewardPouchType::Small};
    Vector2 position{};
    int coins{0};
    std::vector<RewardPouchItem> items;

    Rectangle Hitbox() const;
    float Depth() const;
    bool Empty() const;
};

const std::array<RewardPouchDefinition, 3>& GetRewardPouchDefinitions();
const RewardPouchDefinition& GetRewardPouchDefinition(RewardPouchType type);

namespace RewardPouchRules {
// Integer percent roll in [0,99]. Invalid tables/rolls yield no drop.
std::optional<RewardPouchType> SelectDrop(const RewardPouchDropTable& table, int rollPercent);
std::optional<RewardPouchInstance> Roll(const RewardPouchDropTable& table,
                                      Vector2 position, std::mt19937_64& rng);
}
