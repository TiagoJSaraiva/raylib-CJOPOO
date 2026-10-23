#include "wall_prop.h"

#include "door_visual_rules.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <tuple>

namespace {
auto CorridorKey(TileRect bounds) {
    return std::make_tuple(bounds.x, bounds.y, bounds.width, bounds.height);
}

std::uint64_t PlacementSeed(std::uint64_t seed, const WallPropKey& key) {
    auto value = HashCombine(seed, 0x57414C4C50524F50ULL);
    auto add = [&](int part) { value = HashCombine(value, static_cast<std::uint64_t>(part)); };
    add(static_cast<int>(key.host.kind));
    add(key.host.room.x); add(key.host.room.y);
    add(key.host.bounds.x); add(key.host.bounds.y);
    add(key.host.bounds.width); add(key.host.bounds.height);
    add(static_cast<int>(key.section)); add(key.position.x); add(key.position.y);
    return value;
}

bool Supports(const WallPropDefinition& definition, const WallRules::Module& module) {
    return definition.widthTiles > 0 && definition.widthTiles <= module.widthTiles &&
           std::isfinite(definition.weight) && definition.weight > 0 &&
           std::find(definition.sections.begin(), definition.sections.end(), module.segment.section) != definition.sections.end() &&
           std::find(definition.biomes.begin(), definition.biomes.end(), module.segment.biome) != definition.biomes.end();
}

bool DoorNeighbor(const WallRules::Module& module, const Room& room) {
    const auto& bounds = room.Layout().tileBounds;
    const auto& segment = module.segment;
    for (const auto& door : room.Layout().doors) {
        if (door.sealed) continue;
        if ((door.direction == Direction::North && segment.section != WallRules::Section::Upper) ||
            (door.direction == Direction::South && segment.section != WallRules::Section::Bottom) ||
            (door.direction != Direction::North && door.direction != Direction::South)) continue;
        const int doorX = bounds.x + door.offset;
        if (segment.position.x + module.widthTiles == doorX ||
            segment.position.x == doorX + door.width) return true;
    }
    return false;
}
}

Rectangle WallPropInstance::Bounds() const {
    Rectangle bounds = WallRules::ModuleBounds(module);
    bounds.width = static_cast<float>(definition->widthTiles * TILE_SIZE);
    return bounds;
}

float WallPropInstance::Depth() const {
    auto bounds = Bounds();
    return bounds.y + bounds.height;
}

Rectangle WallPropInstance::LightClip() const {
    const auto& b = key.host.bounds;
    return {float(b.x * TILE_SIZE), float((b.y - 2) * TILE_SIZE),
            float(b.width * TILE_SIZE), float((b.height + 3) * TILE_SIZE)};
}

const std::vector<WallPropDefinition>& GetWallPropDefinitions() {
    static const std::vector<WallPropDefinition> catalog = [] {
        WallPropDefinition torch;
        torch.id = "torch";
        torch.texturePath = "assets/img/wallprop/torch.png";
        torch.animation = PropAnimation{995, 957, 8, 0.10f, false};
        torch.light = PropLight{320.0f, 0.65f, {255, 210, 140, 255}, {0.50f, 0.36f}};
        return std::vector<WallPropDefinition>{torch};
    }();
    return catalog;
}

namespace WallPropRules {
std::vector<WallPropInstance> Generate(const std::vector<WallRules::RoomView>& rooms, std::uint64_t seed,
                                     const std::vector<WallPropDefinition>& catalog, GenerationSettings settings) {
    std::vector<WallRules::RoomView> complete;
    std::unordered_map<RoomCoords, const Room*, RoomCoordsHash> owners;
    for (const auto& view : rooms) if (view.room) {
        complete.push_back({view.room, 1.0f});
        owners.emplace(view.room->GetCoords(), view.room);
    }
    const auto frame = WallRules::BuildFrame(complete);
    auto modules = WallRules::BuildModules(frame);
    std::sort(modules.begin(), modules.end(), [](const auto& a, const auto& b) {
        return std::tie(a.segment.position.y, a.segment.position.x, a.segment.section) <
               std::tie(b.segment.position.y, b.segment.position.x, b.segment.section);
    });
    std::vector<WallPropInstance> result;
    for (const auto& module : modules) {
        const auto& segment = module.segment;
        if (segment.section == WallRules::Section::Side || segment.host.kind == WallRules::HostKind::None) continue;
        // A module can straddle two compatible wall styles; props must stay on one host.
        bool sameHost = true;
        for (int dx = 0; dx < module.widthTiles; ++dx) {
            const auto part = frame.find({{segment.position.x + dx, segment.position.y}, segment.section});
            if (part == frame.end() || !(part->second.host == segment.host)) sameHost = false;
        }
        if (!sameHost) continue;

        bool guaranteed = false;
        if (segment.host.kind == WallRules::HostKind::Room) {
            const auto found = owners.find(segment.host.room);
            const Room* owner = found == owners.end() ? nullptr : found->second;
            if (!owner || owner->GetType() == RoomType::Lobby || owner->GetType() == RoomType::Unknown) continue;
            if (owner->GetType() != RoomType::Normal) {
                if (!DoorNeighbor(module, *owner)) continue;
                guaranteed = true;
            }
        }

        WallPropKey key{segment.host, segment.section, segment.position};
        std::mt19937_64 rng(PlacementSeed(seed, key));
        if (!guaranteed && std::generate_canonical<double, 53>(rng) >= std::clamp(settings.occupancyChance, 0.0f, 1.0f)) continue;
        std::vector<const WallPropDefinition*> choices;
        std::vector<double> weights;
        for (const auto& definition : catalog) if (Supports(definition, module)) {
            choices.push_back(&definition); weights.push_back(definition.weight);
        }
        if (choices.empty()) continue;
        const auto* definition = choices[std::discrete_distribution<std::size_t>(weights.begin(), weights.end())(rng)];
        bool blocked = false;
        for (const auto& placed : result) {
            if (!(placed.key.host == key.host) || placed.key.section != key.section ||
                placed.key.position.y != key.position.y) continue;
            const int gap = guaranteed ? 0 : std::max(0, settings.gapTiles);
            if (key.position.x < placed.key.position.x + placed.definition->widthTiles + gap &&
                key.position.x + definition->widthTiles + gap > placed.key.position.x) blocked = true;
        }
        if (blocked) continue;
        float phase = 0.0f;
        if (definition->animation) {
            const auto& clip = *definition->animation;
            phase = static_cast<float>(std::generate_canonical<double, 53>(rng)) *
                    std::max(0.0f, clip.secondsPerFrame) * std::max(1, clip.frameCount);
        }
        result.push_back({key, definition, module, phase});
    }
    return result;
}

std::vector<WallPropView> VisibleInstances(const std::vector<WallPropInstance>& props,
                                         const std::vector<WallRules::RoomView>& rooms) {
    const auto frame = WallRules::BuildFrame(rooms);
    // Index shared doors once instead of searching the entire map for each source every frame.
    std::map<std::tuple<int, int, int, int>, float> corridorExposure;
    for (const auto& view : rooms) {
        if (!view.room || view.visibility <= 0) continue;
        for (const auto& door : view.room->Layout().doors) {
            if (door.sealed || door.corridorTiles.width <= 0 || door.corridorTiles.height <= 0) continue;
            auto& exposure = corridorExposure[CorridorKey(door.corridorTiles)];
            exposure = std::max(exposure, std::clamp(view.visibility, 0.0f, 1.0f) *
                                          DoorVisualRules::OpenProgress(door.doorState.get()));
        }
    }
    std::vector<WallPropView> result;
    for (const auto& prop : props) {
        float visibility = 1.0f;
        for (int dx = 0; dx < prop.module.widthTiles; ++dx) {
            const auto part = frame.find({{prop.key.position.x + dx, prop.key.position.y}, prop.key.section});
            if (part == frame.end() || !(part->second.host == prop.key.host)) { visibility = 0; break; }
            visibility = std::min(visibility, part->second.visibility);
        }
        if (prop.key.host.kind == WallRules::HostKind::Corridor && visibility > 0) {
            const auto exposure = corridorExposure.find(CorridorKey(prop.key.host.bounds));
            visibility = exposure == corridorExposure.end() ? 0 : std::min(visibility, exposure->second);
        }
        if (visibility > 0) result.push_back({&prop, visibility});
    }
    return result;
}
}
