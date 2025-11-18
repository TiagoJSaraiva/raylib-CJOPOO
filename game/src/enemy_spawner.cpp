#include "enemy_spawner.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <string>

#include "raymath.h"

#include "room.h"
#include "weapon_blueprints.h"

namespace {

Rectangle TileRectToPixels(const TileRect& rect) {
    return Rectangle{
        static_cast<float>(rect.x * TILE_SIZE),
        static_cast<float>(rect.y * TILE_SIZE),
        static_cast<float>(rect.width * TILE_SIZE),
        static_cast<float>(rect.height * TILE_SIZE)
    };
}

EnemyConfig MakeConfig(int id,
                       const std::string& name,
                       BiomeType biome,
                       float maxHealth,
                       float speed,
                       float spawnRate,
                       float collisionRadius) {
    EnemyConfig config{};
    config.id = id;
    config.name = name;
    config.biome = biome;
    config.maxHealth = maxHealth;
    config.speed = speed;
    config.spawnRate = spawnRate;
    config.collisionRadius = collisionRadius;
    return config;
}

EnemySpriteInfo MakeSpriteInfo(const std::string& basePath) {
    EnemySpriteInfo info{};
        info.idleSpritePath = basePath + "/idle_sprite";
        info.walkingSpriteSheetPath = basePath + "/walking_spritesheet";
    info.frameWidth = 38;
    info.frameHeight = 68;
    info.frameCount = 4;
    info.secondsPerFrame = 0.16f;
    return info;
}

} // namespace

EnemySpawner::EnemySpawner() {
    RegisterDefaults();
}

void EnemySpawner::RegisterDefaults() {
    templates_.clear();

    // Caverna
    EnemyTemplate caveRanged{};
    caveRanged.config = MakeConfig(100, "caverna_ranged", BiomeType::Cave, 21.0f, 82.5f, 1.0f, 22.0f);
    caveRanged.range = 520.0f;
    caveRanged.weapon = &GetArcoSimplesWeaponBlueprint();
        caveRanged.sprite = MakeSpriteInfo("./assets/img/enemies/caverna_ranged");
    templates_[BiomeType::Cave].push_back(caveRanged);

    EnemyTemplate caveMelee{};
    caveMelee.config = MakeConfig(101, "caverna_melee", BiomeType::Cave, 40.0f, 95.0f, 1.2f, 24.0f);
    caveMelee.range = 140.0f;
    caveMelee.weapon = &GetEspadaCurtaWeaponBlueprint();
        caveMelee.sprite = MakeSpriteInfo("./assets/img/enemies/caverna_melee");
    templates_[BiomeType::Cave].push_back(caveMelee);

    // Dungeon
    EnemyTemplate dungeonRanged{};
    dungeonRanged.config = MakeConfig(110, "dungeon_ranged", BiomeType::Dungeon, 27.5f, 85.0f, 1.0f, 22.0f);
    dungeonRanged.range = 560.0f;
    dungeonRanged.weapon = &GetCajadoDeCarvalhoWeaponBlueprint();
        dungeonRanged.sprite = MakeSpriteInfo("./assets/img/enemies/dungeon_ranged");
    templates_[BiomeType::Dungeon].push_back(dungeonRanged);

    EnemyTemplate dungeonMelee{};
    dungeonMelee.config = MakeConfig(111, "dungeon_melee", BiomeType::Dungeon, 47.5f, 92.5f, 1.3f, 26.0f);
    dungeonMelee.range = 150.0f;
    dungeonMelee.weapon = &GetMachadinhaWeaponBlueprint();
        dungeonMelee.sprite = MakeSpriteInfo("./assets/img/enemies/dungeon_melee");
    templates_[BiomeType::Dungeon].push_back(dungeonMelee);

    // Mansao
    EnemyTemplate mansionRanged{};
    mansionRanged.config = MakeConfig(120, "mansao_ranged", BiomeType::Mansion, 30.0f, 87.5f, 1.1f, 22.0f);
    mansionRanged.range = 540.0f;
    mansionRanged.weapon = &GetArcoSimplesWeaponBlueprint();
        mansionRanged.sprite = MakeSpriteInfo("./assets/img/enemies/mansao_ranged");
    templates_[BiomeType::Mansion].push_back(mansionRanged);

    EnemyTemplate mansionMelee{};
    mansionMelee.config = MakeConfig(121, "mansao_melee", BiomeType::Mansion, 52.5f, 100.0f, 1.4f, 26.0f);
    mansionMelee.range = 160.0f;
    mansionMelee.weapon = &GetEspadaRunicaWeaponBlueprint();
        mansionMelee.sprite = MakeSpriteInfo("./assets/img/enemies/mansao_melee");
    templates_[BiomeType::Mansion].push_back(mansionMelee);
}

void EnemySpawner::SpawnEnemiesForRoom(Room& room,
                                       std::vector<std::unique_ptr<Enemy>>& storage,
                                       std::mt19937& rng) const {
    if (!storage.empty()) {
        return;
    }

    // Combat encounters only appear in standard rooms.
    if (room.GetType() != RoomType::Normal) {
        return;
    }

    auto it = templates_.find(room.GetBiome());
    if (it == templates_.end() || it->second.empty()) {
        return;
    }

    const RoomLayout& layout = room.Layout();
    if (layout.widthTiles <= 0 || layout.heightTiles <= 0) {
        return;
    }

    int tileArea = layout.widthTiles * layout.heightTiles;
    float baseCount = static_cast<float>(tileArea) / 10.0f;
    float adjustedCount = baseCount * 0.3f;
    int spawnCount = static_cast<int>(std::round(std::max(adjustedCount, 0.0f)));
    spawnCount = std::max(spawnCount, 1);

    const std::vector<EnemyTemplate>& defs = it->second;
    std::vector<double> weights;
    weights.reserve(defs.size());
    for (const auto& def : defs) {
        weights.push_back(std::max(def.config.spawnRate, 0.01f));
    }

    std::discrete_distribution<int> pick(weights.begin(), weights.end());

    Rectangle roomRect = TileRectToPixels(layout.tileBounds);
    float margin = TILE_SIZE * 0.75f;
    std::uniform_real_distribution<float> randX(roomRect.x + margin, roomRect.x + roomRect.width - margin);
    std::uniform_real_distribution<float> randY(roomRect.y + margin, roomRect.y + roomRect.height - margin);

    for (int i = 0; i < spawnCount; ++i) {
        int index = pick(rng);
        if (index < 0 || index >= static_cast<int>(defs.size())) {
            continue;
        }

        Vector2 spawnPosition{randX(rng), randY(rng)};
        const EnemyTemplate& selected = defs[index];
        auto enemy = std::make_unique<EnemyCommon>(selected.config, selected.range, selected.weapon, selected.sprite);
        enemy->Initialize(room, spawnPosition);
        storage.push_back(std::move(enemy));
    }
}
