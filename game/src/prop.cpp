#include "prop.h"
#include <algorithm>

Rectangle PropInstance::Hitbox() const {
    return {float(tiles.x * TILE_SIZE), float(tiles.y * TILE_SIZE),
            float(tiles.width * TILE_SIZE), float(tiles.height * TILE_SIZE)};
}

bool PropInstance::TakeDamage(float amount) {
    if (destroyed || !definition || !definition->destructible || amount <= 0.0f) return false;
    health = std::max(0.0f, health - amount);
    destroyed = health <= 0.0f;
    return destroyed;
}

const std::vector<PropDefinition>& GetPropDefinitions() {
    static const std::vector<PropDefinition> definitions = [] {
        std::vector<PropDefinition> result;
        auto add = [&](const char* id, const char* name, int width, float weight, float disposition) -> PropDefinition& {
            PropDefinition d;
            d.id = id; d.name = name; d.biomes = {BiomeType::Dungeon};
            d.widthTiles = width; d.weight = weight; d.disposition = disposition;
            result.push_back(d);
            return result.back();
        };
        add("gray-vase", "Vaso", 1, 6.0f, 0.0f).destructible = DestructibleProp{};
        add("barrel", "Barril com vela", 1, 1.0f, 0.0f).light = PropLight{};
        add("bone-jail", "Jaula", 1, 3.0f, 0.5f);
        add("torture-table", "Mesa de tortura", 2, 1.0f, 0.9f);
        return result;
    }();
    return definitions;
}
