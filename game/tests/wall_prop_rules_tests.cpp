#include "wall_prop.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

namespace {
Room MakeRoom(RoomCoords coords = {}, TileRect bounds = {0, 0, 12, 8},
              RoomType type = RoomType::Normal, BiomeType biome = BiomeType::Dungeon) {
    RoomLayout layout;
    layout.tileBounds = bounds; layout.widthTiles = bounds.width; layout.heightTiles = bounds.height;
    return Room(coords, {type, biome, 123}, layout);
}

void Equal(const std::vector<WallPropInstance>& a, const std::vector<WallPropInstance>& b) {
    assert(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        assert(a[i].key == b[i].key);
        assert(a[i].definition->id == b[i].definition->id);
        assert(a[i].animationPhase == b[i].animationPhase);
    }
}

void DeterminismAndFit() {
    auto room = MakeRoom();
    auto other = MakeRoom({1, 0}, {30, 0, 10, 8});
    const auto original = WallPropRules::Generate({{&room, 1}, {&other, 1}}, 456);
    assert(!original.empty());
    Equal(original, WallPropRules::Generate({{&other, 0}, {&room, 0.2f}}, 456));
    bool changed = false;
    for (int seed = 457; seed < 465; ++seed) {
        const auto different = WallPropRules::Generate({{&room, 1}, {&other, 1}}, seed);
        if (different.size() != original.size()) changed = true;
        else for (std::size_t i = 0; i < original.size(); ++i)
            if (!(different[i].key == original[i].key)) changed = true;
    }
    assert(changed);

    auto props = WallPropRules::Generate({{&room, 1}}, 123, GetWallPropDefinitions(), {1, 2});
    assert(!props.empty());
    for (std::size_t i = 0; i < props.size(); ++i) {
        const auto& prop = props[i];
        const auto bounds = prop.Bounds();
        assert(bounds.width == 128 && bounds.height == 128);
        assert(prop.module.widthTiles == 2);
        assert(prop.key.section != WallRules::Section::Side);
        assert(prop.Depth() == (prop.key.section == WallRules::Section::Upper ? 0 : 8 * TILE_SIZE));
        for (std::size_t j = i + 1; j < props.size(); ++j)
            if (prop.key.section == props[j].key.section)
                assert(std::abs(prop.key.position.x - props[j].key.position.x) >= 4);
    }
    auto odd = MakeRoom({}, {0, 0, 9, 8});
    for (const auto& prop : WallPropRules::Generate({{&odd, 1}}, 1, GetWallPropDefinitions(), {1, 0}))
        assert(prop.key.position.x + prop.definition->widthTiles <= 8); // Unpaired last tile excluded.
    auto narrow = MakeRoom({}, {0, 0, 1, 8});
    assert(WallPropRules::Generate({{&narrow, 1}}, 1, GetWallPropDefinitions(), {1, 0}).empty());
    assert(WallPropRules::Generate({{&room, 1}}, 1, GetWallPropDefinitions(), {0, 0}).empty());
    assert(WallPropRules::Generate({{&room, 1}}, 1, {}).empty());
    auto cave = MakeRoom({}, {0, 0, 12, 8}, RoomType::Normal, BiomeType::Cave);
    auto lobby = MakeRoom({}, {0, 0, 12, 8}, RoomType::Lobby);
    assert(WallPropRules::Generate({{&cave, 1}, {&lobby, 1}}, 1).empty());

    // A separately scoped run/newly discovered room cannot perturb existing hosts.
    std::vector<WallPropInstance> onlyRoom;
    for (const auto& prop : original) if (prop.key.host.room == room.GetCoords()) onlyRoom.push_back(prop);
    Equal(onlyRoom, WallPropRules::Generate({{&room, 1}}, 456));
}

void SpecialDoors() {
    for (auto type : {RoomType::Shop, RoomType::Forge, RoomType::Chest, RoomType::Boss, RoomType::Puzzle}) {
        auto room = MakeRoom({}, {0, 0, 8, 8}, type);
        assert(WallPropRules::Generate({{&room, 1}}, 12).empty());
        Doorway north; north.direction = Direction::North; north.offset = 2; north.width = 2;
        Doorway south = north; south.direction = Direction::South;
        Doorway east = north; east.direction = Direction::East;
        room.Layout().doors = {north, south, east};
        const auto props = WallPropRules::Generate({{&room, 1}}, 12, GetWallPropDefinitions(), {0, 100});
        assert(props.size() == 4); // Guaranteed, independent of occupancy/gap settings.
        for (const auto& prop : props) {
            assert(prop.key.position.x == 0 || prop.key.position.x == 4);
            assert(prop.key.position.x + 2 <= 2 || prop.key.position.x >= 4);
        }
        room.Layout().doors[0].sealed = true;
        assert(WallPropRules::Generate({{&room, 1}}, 12, GetWallPropDefinitions(), {0, 2}).size() == 2);
        room.Layout().doors[1].offset = 0;
        assert(WallPropRules::Generate({{&room, 1}}, 12, GetWallPropDefinitions(), {0, 2}).size() == 1);
        // Adjacent doorway reservations share the middle module without duplicating it.
        room.Layout().doors.clear(); north.offset = 2; room.Layout().doors.push_back(north);
        north.offset = 6; room.Layout().doors.push_back(north);
        assert(WallPropRules::Generate({{&room, 1}}, 12, GetWallPropDefinitions(), {0, 2}).size() == 2);
    }
}

void SharedCorridorsAndReveal() {
    auto left = MakeRoom();
    auto right = MakeRoom({1, 0}, {16, 0, 12, 8}, RoomType::Normal, BiomeType::Cave);
    Doorway east; east.direction = Direction::East; east.offset = 2; east.width = 2;
    east.corridorTiles = {12, 2, 4, 2}; east.targetCoords = right.GetCoords();
    east.doorState = std::make_shared<DoorInstance>(); east.doorState->open = true;
    Doorway west = east; west.direction = Direction::West; west.targetCoords = left.GetCoords();
    left.Layout().doors.push_back(east); right.Layout().doors.push_back(west);
    const auto props = WallPropRules::Generate({{&left, 1}, {&right, 1}}, 1, GetWallPropDefinitions(), {1, 2});
    Equal(props, WallPropRules::Generate({{&right, 0}, {&left, 0}}, 1, GetWallPropDefinitions(), {1, 2}));
    int corridors = 0;
    for (const auto& prop : props) if (prop.key.host.kind == WallRules::HostKind::Corridor) ++corridors;
    assert(corridors == 2);
    auto visible = WallPropRules::VisibleInstances(props, {{&left, 1}, {&right, 1}});
    assert(visible.size() == props.size());
    assert(WallPropRules::VisibleInstances(props, {{&left, 0}, {&right, 0}}).empty());
    east.doorState->open = false;
    visible = WallPropRules::VisibleInstances(props, {{&left, 1}, {&right, 1}});
    for (const auto& view : visible) assert(view.prop->key.host.kind != WallRules::HostKind::Corridor);
    east.doorState->opening = true; east.doorState->fadeProgress = 0.5f;
    visible = WallPropRules::VisibleInstances(props, {{&left, 0.8f}, {&right, 0.2f}});
    corridors = 0;
    for (const auto& view : visible) if (view.prop->key.host.kind == WallRules::HostKind::Corridor) {
        ++corridors; assert(std::abs(view.visibility - 0.4f) < 0.001f);
    }
    assert(corridors == 2);
    // Returning from a door/reveal transition does not move props or change their phase.
    Equal(props, WallPropRules::Generate({{&left, 1}, {&right, 1}}, 1, GetWallPropDefinitions(), {1, 2}));
}

void ExtensibleCatalog() {
    WallPropDefinition decoration;
    decoration.id = "banner"; decoration.texturePath = "banner.png";
    decoration.biomes = {BiomeType::Mansion}; decoration.sections = {WallRules::Section::Upper};
    const std::vector<WallPropDefinition> catalog{decoration};
    auto room = MakeRoom({}, {0, 0, 8, 8}, RoomType::Normal, BiomeType::Mansion);
    const auto props = WallPropRules::Generate({{&room, 1}}, 1, catalog, {1, 2});
    assert(!props.empty());
    for (const auto& prop : props) {
        assert(prop.definition == &catalog[0]);
        assert(prop.key.section == WallRules::Section::Upper);
        assert(!prop.definition->animation && !prop.definition->light);
    }
}
}

int main() {
    DeterminismAndFit(); SpecialDoors(); SharedCorridorsAndReveal(); ExtensibleCatalog();
    std::cout << "Wall prop rules tests passed\n";
}
