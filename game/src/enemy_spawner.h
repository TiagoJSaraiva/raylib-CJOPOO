#pragma once

#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

#include "enemy_common.h"

class Room;

class EnemySpawner {
public:
    EnemySpawner();

    void SpawnEnemiesForRoom(Room& room,
                             std::vector<std::unique_ptr<Enemy>>& storage,
                             std::mt19937& rng) const;

private:
    struct EnemyTemplate {
        EnemyConfig config{};
        float range{240.0f};
        const WeaponBlueprint* weapon{nullptr};
        EnemySpriteInfo sprite{};
    };

    std::unordered_map<BiomeType, std::vector<EnemyTemplate>> templates_;

    void RegisterDefaults();
};
