#pragma once

#include "room_types.h"
#include "reward_pouch.h"
#include "raylib.h"
#include <optional>
#include <string>
#include <vector>

struct PropAnimation {
    int frameWidth{0};
    int frameHeight{0};
    int frameCount{1};
    float secondsPerFrame{0.12f};
    bool vertical{false};
};

struct PropLight {
    float radius{192.0f};
    float intensity{0.4f};
    Color color{255, 210, 140, 255};
    // Normalized position within the complete sprite, including transparent margins.
    Vector2 spritePosition{0.52f, 0.04f};
};

struct DestructibleProp {
    float maxHealth{10.0f};
    RewardPouchDropTable rewardPouchDrops;
};

struct PropDefinition {
    std::string id;
    std::string name;
    std::vector<BiomeType> biomes;
    int widthTiles{1};
    int heightTiles{1};
    float weight{1.0f};
    std::optional<float> disposition;
    bool solid{true};
    float damagePerSecond{0.0f};
    std::optional<DestructibleProp> destructible;
    std::optional<PropAnimation> animation;
    std::optional<PropLight> light;
};

struct PropInstance {
    const PropDefinition* definition{nullptr}; // Catalog outlives the run.
    TileRect tiles{};
    float health{0.0f};
    bool destroyed{false};
    Rectangle Hitbox() const;
    float Depth() const { return static_cast<float>((tiles.y + tiles.height) * TILE_SIZE); }
    std::uintptr_t TargetId() const { return reinterpret_cast<std::uintptr_t>(this); }
    bool TakeDamage(float amount);
};

const std::vector<PropDefinition>& GetPropDefinitions();
