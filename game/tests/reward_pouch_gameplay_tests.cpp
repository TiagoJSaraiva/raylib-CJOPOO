#include "enemy_common.h"
#include "enemy_spawner.h"
#include "inventory_operations.h"
#include "projectile.h"
#include "reward_pouch_operations.h"
#include "room_manager.h"

#include <cassert>
#include <iostream>
#include <limits>

namespace {
namespace Ops = RewardPouchOperations;
namespace Inv = InventoryOperations;
using Type = RewardPouchType;

Room MakeRoom(std::uint64_t seed = 42, BiomeType biome = BiomeType::Dungeon) {
    RoomLayout layout;
    layout.widthTiles = layout.heightTiles = 10;
    layout.tileBounds = {0, 0, 10, 10};
    Room room({0, 1}, {RoomType::Normal, biome, seed}, layout);
    room.SetPropsInitialized();
    return room;
}

InventoryUIState MakeInventory(bool full = false) {
    InventoryUIState state;
    state.items = {{1, "Weapon", ItemCategory::Weapon},
                   {101, "Tira de couro", ItemCategory::Material},
                   {102, "Lingote de bronze", ItemCategory::Material}};
    Inv::EnsureInventoryMeta(state);
    if (full) for (int i = 0; i < 24; ++i) Inv::SetInventorySlot(state, i, 1, 1);
    return state;
}

void CheckInventoryContracts() {
    auto state = MakeInventory(true);
    Inv::SetInventorySlot(state, 0, 101, 98);
    assert(Inv::AddItemToInventory(state, 101, 3) == -1);
    assert(state.inventoryQuantities[0] == 98);
    assert(Inv::AddItemToInventoryPartial(state, 101, 3) == 1);
    assert(state.inventoryQuantities[0] == 99 && state.inventoryItems[0] == "Tira de couro");
    assert(state.inventoryTypes[0] == ItemCategory::Material);
    assert(Inv::AddItemToInventoryPartial(state, 101, 1) == 0);
    Inv::ClearInventorySlot(state, 1);
    Inv::ClearInventorySlot(state, 2);
    assert(Inv::AddItemToInventoryPartial(state, 101, 250) == 198);
    assert(state.inventoryQuantities[1] == 99 && state.inventoryQuantities[2] == 99);
    Inv::ClearInventorySlot(state, 3);
    assert(Inv::AddItemToInventoryPartial(state, 1, 2) == 1);
    assert(state.inventoryQuantities[3] == 1);
    assert(Inv::AddItemToInventoryPartial(state, 999, 1) == 0);
    assert(Inv::AddItemToInventoryPartial(state, 101, 0) == 0);
    assert(Inv::AddItemToInventoryPartial(state, -1, 1) == 0);
}

void CheckPartialPickupAndGates() {
    auto room = MakeRoom();
    auto state = MakeInventory(true);
    int initialCoins = state.coins;
    Inv::SetInventorySlot(state, 0, 101, 98);
    room.RewardPouches().push_back({Type::Medium, {100, 100}, 30, {{101, 3}, {102, 2}}});
    Rectangle bounds{90, 90, 20, 20};
    assert(Ops::Collect(room, {200, 200, 20, 20}, state, true, false).empty());
    assert(Ops::Collect(room, bounds, state, false, false).empty());
    assert(Ops::Collect(room, bounds, state, true, true).empty());
    state.open = true;
    assert(Ops::Collect(room, bounds, state, true, false).empty());
    state.open = false;
    assert(state.coins == initialCoins && room.RewardPouches()[0].coins == 30);

    auto receipts = Ops::Collect(room, bounds, state, true, false);
    assert(receipts.size() == 1 && receipts[0].coins == 30);
    assert(receipts[0].items.size() == 1 && receipts[0].items[0].quantity == 1);
    assert(state.coins == initialCoins + 30 && state.inventoryQuantities[0] == 99);
    assert(room.RewardPouches().size() == 1 && room.RewardPouches()[0].coins == 0);
    assert(room.RewardPouches()[0].items[0].quantity == 2);
    for (int i = 0; i < 10; ++i) assert(Ops::Collect(room, bounds, state, true, false).empty());
    assert(state.coins == initialCoins + 30);

    Inv::ClearInventorySlot(state, 1);
    receipts = Ops::Collect(room, bounds, state, true, false);
    assert(receipts.size() == 1 && receipts[0].coins == 0);
    assert(state.inventoryItemIds[1] == 101 && state.inventoryQuantities[1] == 2);
    assert(room.RewardPouches()[0].items.size() == 1 && room.RewardPouches()[0].items[0].itemId == 102);
    Inv::ClearInventorySlot(state, 2);
    receipts = Ops::Collect(room, bounds, state, true, false);
    assert(receipts.size() == 1 && receipts[0].items[0].quantity == 2);
    assert(state.inventoryItemIds[2] == 102 && room.RewardPouches().empty());
    assert(Ops::Collect(room, bounds, state, true, false).empty());

    room.RewardPouches() = {{Type::Small, {100, 100}, 12, {}}, {Type::Big, {100, 100}, 48, {}}};
    assert(Ops::Collect(room, bounds, state, true, false).size() == 2);
    assert(room.RewardPouches().empty() && state.coins == initialCoins + 90);
    state.coins = std::numeric_limits<int>::max() - 1;
    room.RewardPouches().push_back({Type::Small, {100, 100}, 12, {}});
    receipts = Ops::Collect(room, bounds, state, true, false);
    assert(receipts[0].coins == 1 && room.RewardPouches()[0].coins == 11);
}

void CheckCombatTransitions() {
    auto room = MakeRoom();
    auto state = MakeInventory();
    int initialCoins = state.coins;
    EnemyConfig config;
    config.maxHealth = 10;
    config.rewardPouchDrops = {{Type::Big, 100}};
    EnemyCommon enemy(config, 0, nullptr, {});
    enemy.Initialize(room, {200, 200});
    assert(!Ops::DamageEnemy(room, enemy, 100)); // Fade-in immunity.
    PlayerCharacter player = CreateKnightCharacter();
    ProjectileSystem shots;
    enemy.Update({1.0f, player, {200, 200}, room, true, shots});
    assert(enemy.HasCompletedFade());
    assert(!Ops::DamageEnemy(room, enemy, 4));
    assert(room.RewardPouches().empty());
    assert(Ops::DamageEnemy(room, enemy, 6));
    for (int hit = 0; hit < 5; ++hit) assert(!Ops::DamageEnemy(room, enemy, 100));
    assert(room.RewardPouches().size() == 1 && room.RewardPouches()[0].type == Type::Big);
    assert(room.RewardPouches()[0].position.x == 200 && room.RewardPouches()[0].position.y == 200);
    assert(state.coins == initialCoins); // Rewards require the collection path.

    PropDefinition definition;
    definition.destructible = DestructibleProp{10, {{Type::Small, 100}}};
    // Both projectile systems feed the same DamageProp path in the game.
    for (bool enemyAttack : {false, true}) {
        PropInstance prop{&definition, {4, 4, 1, 1}, 10, false};
        ProjectileSystem playerShots, enemyShots;
        auto& system = enemyAttack ? enemyShots : playerShots;
        ProjectileBlueprint blueprint;
        blueprint.kind = ProjectileKind::Ranged;
        ThrownProjectileBlueprint shot;
        shot.common.damage = 12;
        shot.common.lifespanSeconds = 2;
        shot.ammunition.speed = 500;
        shot.ammunition.maxDistance = 1000;
        shot.ammunition.radius = 2;
        blueprint.thrownProjectiles.push_back(shot);
        system.SpawnProjectile(blueprint, {{200, 288}, nullptr, {1, 0}});
        system.Update(0.3f);
        auto events = system.CollectDamageEventsForTargets({{prop.TargetId(), {}, 0, prop.Hitbox(), 0}});
        assert(events.size() == 1);
        size_t before = room.RewardPouches().size();
        assert(Ops::DamageProp(room, prop, events[0].damage.amount));
        assert(prop.destroyed && room.RewardPouches().size() == before + 1);
        assert(room.RewardPouches().back().position.x == 288 && room.RewardPouches().back().position.y == 288);
        assert(!Ops::DamageProp(room, prop, 12));
        assert(room.RewardPouches().size() == before + 1);
    }
    PropDefinition solid;
    PropInstance indestructible{&solid, {1, 1, 1, 1}, 0, false};
    assert(!Ops::DamageProp(room, indestructible, 100));
    assert(state.coins == initialCoins);
}

void CheckPersistenceAndRng() {
    auto first = MakeRoom(), second = MakeRoom();
    std::mt19937 unrelated(123);
    for (int i = 0; i < 100; ++i) {
        for (int j = 0; j < i; ++j) (void)unrelated();
        assert(Ops::Spawn(first, {{Type::Big, 100}}, {40, 50}));
        assert(Ops::Spawn(second, {{Type::Big, 100}}, {40, 50}));
        assert(first.RewardPouches().back().coins == second.RewardPouches().back().coins);
        const auto& a = first.RewardPouches().back().items;
        const auto& b = second.RewardPouches().back().items;
        assert(a.size() == b.size());
        for (size_t j = 0; j < a.size(); ++j) assert(a[j].quantity == b[j].quantity && a[j].itemId == b[j].itemId);
    }
    RoomManager manager(327);
    auto coords = manager.GetCurrentCoords();
    manager.GetCurrentRoom().RewardPouches().push_back({Type::Small, {100, 100}, 0, {{101, 2}}});
    manager.EnsureNeighborsGenerated(coords);
    auto doors = manager.GetCurrentRoom().Layout().doors;
    bool moved = false;
    for (const auto& door : doors) {
        if (!manager.MoveToNeighbor(door.direction)) continue;
        moved = true;
        auto inventory = MakeInventory();
        int before = inventory.coins;
        assert(Ops::Collect(manager.GetCurrentRoom(), {90, 90, 20, 20}, inventory, true, false).empty());
        assert(inventory.coins == before);
        assert(manager.MoveToNeighbor(Opposite(door.direction)));
        break;
    }
    assert(moved && manager.GetCurrentCoords() == coords);
    assert(manager.GetCurrentRoom().RewardPouches()[0].items[0].quantity == 2);
    manager = RoomManager(327);
    assert(manager.GetCurrentRoom().RewardPouches().empty());
}

void CheckDefaultTables() {
    EnemySpawner spawner;
    for (auto biome : {BiomeType::Cave, BiomeType::Dungeon, BiomeType::Mansion}) {
        auto room = MakeRoom(765, biome);
        std::vector<std::unique_ptr<Enemy>> enemies;
        std::mt19937 rng(1);
        spawner.SpawnEnemiesForRoom(room, enemies, rng);
        assert(!enemies.empty());
        for (const auto& enemy : enemies) {
            int counts[4]{};
            for (int roll = 0; roll < 100; ++roll) {
                auto type = RewardPouchRules::SelectDrop(enemy->GetRewardPouchDrops(), roll);
                ++counts[type ? static_cast<int>(*type) : 3];
            }
            if (biome == BiomeType::Cave) assert(counts[0] == 60 && counts[1] == 10 && counts[2] == 0 && counts[3] == 30);
            if (biome == BiomeType::Dungeon) assert(counts[0] == 50 && counts[1] == 20 && counts[2] == 5 && counts[3] == 25);
            if (biome == BiomeType::Mansion) assert(counts[0] == 40 && counts[1] == 30 && counts[2] == 10 && counts[3] == 20);
        }
    }
    const auto& vase = GetPropDefinitions()[0];
    assert(vase.destructible);
    const auto& table = vase.destructible->rewardPouchDrops;
    assert(RewardPouchRules::SelectDrop(table, 19) == Type::Small);
    assert(!RewardPouchRules::SelectDrop(table, 20));
    for (size_t i = 1; i < GetPropDefinitions().size(); ++i) assert(!GetPropDefinitions()[i].destructible);
}
}

int main() {
    CheckInventoryContracts();
    CheckPartialPickupAndGates();
    CheckCombatTransitions();
    CheckPersistenceAndRng();
    CheckDefaultTables();
    std::cout << "Reward pouch gameplay tests passed\n";
}
