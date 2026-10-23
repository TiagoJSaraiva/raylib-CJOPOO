#include "reward_pouch.h"

#include <cassert>
#include <iostream>
#include <set>

namespace {
using Type = RewardPouchType;

void CheckDropBoundaries() {
    const RewardPouchDropTable table{{Type::Small, 50}, {Type::Medium, 20}, {Type::Big, 5}};
    int counts[4]{};
    for (int roll = 0; roll < 100; ++roll) {
        auto type = RewardPouchRules::SelectDrop(table, roll);
        ++counts[type ? static_cast<int>(*type) : 3];
    }
    assert(counts[0] == 50 && counts[1] == 20 && counts[2] == 5 && counts[3] == 25);
    assert(RewardPouchRules::SelectDrop(table, 49) == Type::Small);
    assert(RewardPouchRules::SelectDrop(table, 50) == Type::Medium);
    assert(RewardPouchRules::SelectDrop(table, 70) == Type::Big);
    assert(!RewardPouchRules::SelectDrop(table, 75));
    assert(!RewardPouchRules::SelectDrop({}, 0));
    assert(!RewardPouchRules::SelectDrop(table, -1));
    assert(!RewardPouchRules::SelectDrop(table, 100));
    assert(!RewardPouchRules::SelectDrop({{Type::Small, 0}}, 0));
    assert(RewardPouchRules::SelectDrop({{Type::Big, 100}}, 99) == Type::Big);
    assert(!RewardPouchRules::SelectDrop({{Type::Small, 80}, {Type::Big, 21}}, 0));
    assert(!RewardPouchRules::SelectDrop({{Type::Small, -1}}, 0));
    assert(!RewardPouchRules::SelectDrop({{static_cast<Type>(20), 100}}, 0));
    std::mt19937_64 rng(12), untouched(12);
    assert(!RewardPouchRules::Roll({}, {0, 0}, rng));
    assert(rng() == untouched());
}

void CheckContents() {
    // Fixed seeds and thousands of samples exercise inclusive bounds and every
    // combination of the independent optional material rolls, without flaky tolerances.
    for (auto type : {Type::Small, Type::Medium, Type::Big}) {
        std::mt19937_64 rng(581), same(581);
        const auto& def = GetRewardPouchDefinition(type);
        std::set<int> coinValues;
        std::set<int> combinations;
        std::set<int> leatherQuantities, bronzeQuantities;
        for (int i = 0; i < 5000; ++i) {
            auto pouch = RewardPouchRules::Roll({{type, 100}}, {110, 220}, rng);
            auto repeated = RewardPouchRules::Roll({{type, 100}}, {110, 220}, same);
            assert(pouch && repeated && pouch->type == type);
            assert(pouch->coins >= def.minCoins && pouch->coins <= def.maxCoins);
            assert(pouch->coins == repeated->coins && pouch->items.size() == repeated->items.size());
            assert(pouch->position.x == 110 && pouch->position.y == 220);
            assert(!pouch->Empty());
            coinValues.insert(pouch->coins);
            int combination = 0;
            for (size_t j = 0; j < pouch->items.size(); ++j) {
                const auto& item = pouch->items[j];
                assert(item.itemId == repeated->items[j].itemId && item.quantity == repeated->items[j].quantity);
                if (item.itemId == 101) {
                    combination |= 1;
                    leatherQuantities.insert(item.quantity);
                    assert(item.quantity >= (type == Type::Big ? 2 : 1));
                    assert(item.quantity <= (type == Type::Big ? 3 : type == Type::Medium ? 2 : 1));
                } else {
                    assert(item.itemId == 102 && type != Type::Small);
                    combination |= 2;
                    bronzeQuantities.insert(item.quantity);
                    assert(item.quantity >= 1 && item.quantity <= (type == Type::Big ? 2 : 1));
                }
            }
            combinations.insert(combination);
        }
        assert(*coinValues.begin() == def.minCoins && *coinValues.rbegin() == def.maxCoins);
        if (type == Type::Small) {
            assert((combinations == std::set<int>{0, 1}));
            assert((leatherQuantities == std::set<int>{1}));
        } else if (type == Type::Medium) {
            assert((combinations == std::set<int>{0, 1, 2, 3}));
            assert((leatherQuantities == std::set<int>{1, 2}));
            assert((bronzeQuantities == std::set<int>{1}));
        } else {
            assert((combinations == std::set<int>{1, 3}));
            assert((leatherQuantities == std::set<int>{2, 3}));
            assert((bronzeQuantities == std::set<int>{1, 2}));
        }
    }
}
}

int main() {
    CheckDropBoundaries();
    CheckContents();
    std::cout << "Reward pouch rules tests passed\n";
}
