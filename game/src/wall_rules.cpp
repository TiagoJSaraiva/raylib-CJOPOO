#include "wall_rules.h"

#include "door_visual_rules.h"
#include "floor_rules.h"

#include <algorithm>
#include <random>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace WallRules {
namespace {

struct TileInfo {
    BiomeType biome;
    float visibility;
    float sideVisibility;
    bool roomTile;
    bool southCorridorTile;
    BottomWallRowId bottomWallRow;
};

struct PositionHash {
    std::size_t operator()(const Position& position) const noexcept {
        return static_cast<std::size_t>(HashCombine(static_cast<std::uint64_t>(position.x),
                                                    static_cast<std::uint64_t>(position.y)));
    }
};

struct EdgeKey {
    Position tile;
    Direction direction;

    bool operator==(const EdgeKey& other) const noexcept {
        return tile == other.tile && direction == other.direction;
    }
};

struct EdgeKeyHash {
    std::size_t operator()(const EdgeKey& key) const noexcept {
        auto hash = HashCombine(static_cast<std::uint64_t>(key.tile.x),
                                static_cast<std::uint64_t>(key.tile.y));
        hash = HashCombine(hash, static_cast<std::uint64_t>(key.direction));
        return static_cast<std::size_t>(hash);
    }
};

struct HorizontalMouth {
    Position bottomWall;
    Position bottomEdgeTile;
    Position bottomRoomSide;
    BiomeType biome;
    float visibility;
    SidePlacement sidePlacement;
};

using WalkableTiles = std::unordered_map<Position, TileInfo, PositionHash>;

void AddRect(WalkableTiles& tiles, const TileRect& rect, BiomeType biome, float visibility,
             float sideVisibility = 1.0f, bool isCorridor = false,
             bool isSouthCorridor = false, BottomWallRowId bottomWallRow = {}) {
    if (!(visibility > 0.0f) || rect.width <= 0 || rect.height <= 0) return;
    visibility = std::min(visibility, 1.0f);
    sideVisibility = visibility * std::clamp(sideVisibility, 0.0f, 1.0f);
    for (int y = rect.y; y < rect.y + rect.height; ++y) {
        for (int x = rect.x; x < rect.x + rect.width; ++x) {
            const Position position{x, y};
            auto [it, inserted] = tiles.emplace(position,
                TileInfo{biome, visibility, sideVisibility, !isCorridor, isSouthCorridor,
                         bottomWallRow});
            if (!inserted) {
                it->second.visibility = std::max(it->second.visibility, visibility);
                it->second.sideVisibility = std::max(it->second.sideVisibility, sideVisibility);
                if (isCorridor && !it->second.roomTile) {
                    it->second.southCorridorTile = it->second.southCorridorTile || isSouthCorridor;
                }
            }
        }
    }
}

void AddOpenDoorEdges(const RoomLayout& layout, const Doorway& door,
                      std::unordered_set<EdgeKey, EdgeKeyHash>& openEdges) {
    if (door.sealed) return;

    for (int offset = 0; offset < door.width; ++offset) {
        const int along = door.offset + offset;
        Position tile{};
        switch (door.direction) {
            case Direction::North:
                tile = Position{layout.tileBounds.x + along, layout.tileBounds.y};
                break;
            case Direction::South:
                tile = Position{layout.tileBounds.x + along,
                                layout.tileBounds.y + layout.heightTiles - 1};
                break;
            case Direction::East:
                tile = Position{layout.tileBounds.x + layout.widthTiles - 1,
                                layout.tileBounds.y + along};
                break;
            case Direction::West:
                tile = Position{layout.tileBounds.x, layout.tileBounds.y + along};
                break;
        }
        openEdges.insert(EdgeKey{tile, door.direction});
    }

    // Keep the far end of an unfinished or one-sided corridor open, as the old wall spans did.
    const TileRect& corridor = door.corridorTiles;
    for (int y = corridor.y; y < corridor.y + corridor.height; ++y) {
        for (int x = corridor.x; x < corridor.x + corridor.width; ++x) {
            openEdges.insert(EdgeKey{Position{x, y}, door.direction});
        }
    }
}

bool HasWalkableNeighbor(const WalkableTiles& tiles, Position tile, Direction direction) {
    switch (direction) {
        case Direction::North: --tile.y; break;
        case Direction::South: ++tile.y; break;
        case Direction::East: ++tile.x; break;
        case Direction::West: --tile.x; break;
    }
    return tiles.find(tile) != tiles.end();
}

bool IsOpen(const std::unordered_set<EdgeKey, EdgeKeyHash>& openEdges,
            Position tile, Direction direction) {
    return openEdges.find(EdgeKey{tile, direction}) != openEdges.end();
}

void AddSegment(Frame& frame, Position position, Position edgeTile, Section section,
                const TileInfo& tile, float visibility,
                SidePlacement sidePlacement = SidePlacement::Edge) {
    const Key key{position, section, sidePlacement};
    auto [it, inserted] = frame.emplace(
        key, Segment{position, edgeTile, section, tile.biome, visibility, sidePlacement,
                     section == Section::Bottom ? tile.bottomWallRow : BottomWallRowId{}});
    if (!inserted) {
        it->second.visibility = std::max(it->second.visibility, visibility);
        const bool lowerBiome = static_cast<int>(tile.biome) < static_cast<int>(it->second.biome);
        const bool stableEdge = tile.biome == it->second.biome &&
            (edgeTile.x < it->second.edgeTile.x ||
             (edgeTile.x == it->second.edgeTile.x && edgeTile.y < it->second.edgeTile.y));
        if (lowerBiome || stableEdge) {
            it->second.biome = tile.biome;
            it->second.edgeTile = edgeTile;
            if (section == Section::Bottom) it->second.bottomWallRow = tile.bottomWallRow;
        }
    }
}

std::uint64_t WallSalt(Section section, BiomeType biome) {
    std::uint64_t salt = HashCombine(0x57414C4CULL, static_cast<std::uint64_t>(section));
    return HashCombine(salt, static_cast<std::uint64_t>(biome));
}

bool SameModuleStyle(const Segment& a, const Segment& b) {
    return a.section == b.section && a.biome == b.biome &&
           a.sidePlacement == b.sidePlacement &&
           (a.section != Section::Bottom || a.bottomWallRow == b.bottomWallRow);
}

bool IsRegularSideSegment(const Segment& segment) {
    return segment.section == Section::Side &&
           segment.sidePlacement == SidePlacement::Edge &&
           segment.position.y == segment.edgeTile.y;
}

bool IsUpperSideCornerCap(const Segment& segment) {
    return segment.section == Section::Side &&
           segment.sidePlacement == SidePlacement::Edge &&
           segment.position.y + 1 == segment.edgeTile.y;
}

} // namespace

std::size_t KeyHash::operator()(const Key& key) const noexcept {
    auto hash = HashCombine(static_cast<std::uint64_t>(key.position.x),
                            static_cast<std::uint64_t>(key.position.y));
    hash = HashCombine(hash, static_cast<std::uint64_t>(key.section));
    return static_cast<std::size_t>(HashCombine(hash, static_cast<std::uint64_t>(key.sidePlacement)));
}

std::size_t BottomWallRowIdHash::operator()(const BottomWallRowId& id) const noexcept {
    auto hash = HashCombine(0x424F54544F4DULL, static_cast<std::uint64_t>(id.kind));
    hash = HashCombine(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(id.ownerX)));
    hash = HashCombine(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(id.ownerY)));
    hash = HashCombine(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(id.ownerWidth)));
    hash = HashCombine(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(id.ownerHeight)));
    hash = HashCombine(hash, static_cast<std::uint64_t>(static_cast<std::int64_t>(id.edgeY)));
    return static_cast<std::size_t>(hash);
}

Frame BuildFrame(const std::vector<RoomView>& rooms) {
    std::vector<RoomView> ordered;
    ordered.reserve(rooms.size());
    for (const auto& view : rooms) {
        if (view.room != nullptr) ordered.push_back(view);
    }
    std::sort(ordered.begin(), ordered.end(), [](const RoomView& a, const RoomView& b) {
        const auto aCoords = a.room->GetCoords();
        const auto bCoords = b.room->GetCoords();
        return aCoords.x < bCoords.x || (aCoords.x == bCoords.x && aCoords.y < bCoords.y);
    });

    std::unordered_map<RoomCoords, const Room*, RoomCoordsHash> lookup;
    for (const auto& view : ordered) lookup.emplace(view.room->GetCoords(), view.room);

    WalkableTiles walkable;
    std::unordered_set<EdgeKey, EdgeKeyHash> openEdges;
    std::vector<HorizontalMouth> horizontalMouths;

    // Add room footprints first so corridor tiles use the resolved corridor biome only outside rooms.
    for (const auto& view : ordered) {
        if (!(view.visibility > 0.0f)) continue;
        const TileRect& bounds = view.room->Layout().tileBounds;
        const RoomCoords coords = view.room->GetCoords();
        const BottomWallRowId bottomWallRow{
            BottomWallRowKind::Room, coords.x, coords.y, bounds.width, bounds.height,
            bounds.y + bounds.height - 1
        };
        AddRect(walkable, bounds, view.room->GetBiome(), view.visibility, 1.0f,
                false, false, bottomWallRow);
    }

    for (const auto& view : ordered) {
        if (!(view.visibility > 0.0f)) continue;
        const Room& room = *view.room;
        for (const auto& door : room.Layout().doors) {
            if (door.sealed) continue;
            AddOpenDoorEdges(room.Layout(), door, openEdges);

            BiomeType corridorBiome = room.GetBiome();
            const auto target = lookup.find(door.targetCoords);
            if (target != lookup.end()) {
                corridorBiome = FloorRules::CorridorBiome(room.GetCoords(), room.GetBiome(),
                                                           target->first, target->second->GetBiome());
            }
            const float sideProgress = DoorVisualRules::OpenProgress(door.doorState.get());
            const BottomWallRowId bottomWallRow{
                BottomWallRowKind::Corridor, door.corridorTiles.x, door.corridorTiles.y,
                door.corridorTiles.width, door.corridorTiles.height,
                door.corridorTiles.y + door.corridorTiles.height - 1
            };
            AddRect(walkable, door.corridorTiles, corridorBiome, view.visibility, sideProgress,
                    true, door.direction == Direction::South, bottomWallRow);

            const TileRect& corridor = door.corridorTiles;
            if ((door.direction == Direction::East || door.direction == Direction::West) &&
                corridor.width > 0 && corridor.height > 0) {
                const int mouthX = door.direction == Direction::East
                    ? corridor.x
                    : corridor.x + corridor.width - 1;
                const Position bottomWall{mouthX, corridor.y + corridor.height - 1};
                horizontalMouths.push_back(HorizontalMouth{
                    bottomWall,
                    Position{mouthX, bottomWall.y},
                    Position{mouthX, bottomWall.y + 1},
                    corridorBiome,
                    view.visibility * sideProgress,
                    door.direction == Direction::East
                        ? SidePlacement::InsetLeft
                        : SidePlacement::InsetRight
                });
            }
        }
    }

    Frame frame;
    std::vector<Segment> corridorSides;
    for (const auto& [position, tile] : walkable) {
        const bool north = !HasWalkableNeighbor(walkable, position, Direction::North) &&
                           !IsOpen(openEdges, position, Direction::North);
        const bool south = !HasWalkableNeighbor(walkable, position, Direction::South) &&
                           !IsOpen(openEdges, position, Direction::South);
        const bool west = !HasWalkableNeighbor(walkable, position, Direction::West) &&
                          !IsOpen(openEdges, position, Direction::West);
        const bool east = !HasWalkableNeighbor(walkable, position, Direction::East) &&
                          !IsOpen(openEdges, position, Direction::East);

        if (north) AddSegment(frame, Position{position.x, position.y - 1}, position,
                              Section::Upper, tile, tile.visibility);
        if (south) AddSegment(frame, position, position, Section::Bottom, tile, tile.visibility);
        if (west && tile.sideVisibility > 0.0f) {
            const Position sidePosition{position.x - 1, position.y};
            AddSegment(frame, sidePosition, position,
                       Section::Side, tile, tile.sideVisibility);
            if (tile.southCorridorTile) {
                corridorSides.push_back(Segment{sidePosition, position, Section::Side,
                                                tile.biome, tile.sideVisibility});
            }
        }
        if (east && tile.sideVisibility > 0.0f) {
            const Position sidePosition{position.x + 1, position.y};
            AddSegment(frame, sidePosition, position,
                       Section::Side, tile, tile.sideVisibility);
            if (tile.southCorridorTile) {
                corridorSides.push_back(Segment{sidePosition, position, Section::Side,
                                                tile.biome, tile.sideVisibility});
            }
        }

        // Upper corner caps connect the side run to the upper wall without a gap.
        if (north && west && tile.sideVisibility > 0.0f) {
            AddSegment(frame, Position{position.x - 1, position.y - 1}, position,
                       Section::Side, tile, tile.sideVisibility);
        }
        if (north && east && tile.sideVisibility > 0.0f) {
            AddSegment(frame, Position{position.x + 1, position.y - 1}, position,
                       Section::Side, tile, tile.sideVisibility);
        }
    }

    // Extend corridor side walls over the adjacent bottom-wall tiles at a south exit.
    // Matching the side tile below an emitted bottom segment avoids caps at room exteriors.
    for (const auto& side : corridorSides) {
        const Position bottomPosition{side.position.x, side.position.y - 1};
        if (frame.find(Key{bottomPosition, Section::Bottom}) == frame.end()) continue;

        // A side segment to the left of its corridor tile caps the bottom wall's right edge,
        // and a segment to the right caps its left edge.
        const SidePlacement placement = side.position.x < side.edgeTile.x
            ? SidePlacement::InsetRight
            : SidePlacement::InsetLeft;
        const TileInfo capTile{side.biome, side.visibility, side.visibility};
        const Position bottomCap{bottomPosition.x, bottomPosition.y - 1};
        AddSegment(frame, bottomCap, side.edgeTile, Section::Side, capTile,
                   side.visibility, placement);
    }

    // Cap only the lower horizontal corridor wall where it meets a room side wall.
    // Upper corridor walls intentionally have no side cap.
    for (const auto& mouth : horizontalMouths) {
        if (!(mouth.visibility > 0.0f)) continue;

        const TileInfo capTile{mouth.biome, mouth.visibility, mouth.visibility};
        if (frame.find(Key{mouth.bottomWall, Section::Bottom}) != frame.end() &&
            frame.find(Key{mouth.bottomRoomSide, Section::Side}) != frame.end()) {
            // Bottom sprites are anchored one tile above their segment. Match
            // the cap's two-tile span to the complete bottom-wall sprite.
            const Position bottomCap{mouth.bottomWall.x, mouth.bottomWall.y - 1};
            AddSegment(frame, bottomCap, mouth.bottomEdgeTile, Section::Side,
                       capTile, mouth.visibility, mouth.sidePlacement);
        }
    }

    return frame;
}

std::vector<Module> BuildModules(const Frame& frame) {
    std::vector<const Segment*> ordered;
    ordered.reserve(frame.size());
    for (const auto& entry : frame) ordered.push_back(&entry.second);
    std::sort(ordered.begin(), ordered.end(), [](const Segment* a, const Segment* b) {
        return std::tie(a->position.y, a->position.x, a->section, a->sidePlacement) <
               std::tie(b->position.y, b->position.x, b->section, b->sidePlacement);
    });

    std::vector<const Segment*> sideSegments;
    sideSegments.reserve(frame.size());
    for (const auto& entry : frame) {
        if (entry.second.section == Section::Side) sideSegments.push_back(&entry.second);
    }
    std::sort(sideSegments.begin(), sideSegments.end(), [](const Segment* a, const Segment* b) {
        if (a->position.y != b->position.y) return a->position.y > b->position.y;
        return std::tie(a->position.x, a->sidePlacement) <
               std::tie(b->position.x, b->sidePlacement);
    });

    std::unordered_set<Key, KeyHash> consumed;
    consumed.reserve(frame.size());
    std::vector<Module> modules;
    modules.reserve(frame.size());

    // Build side strips from the bottom upward so an odd vertical run leaves its
    // terminal segment at the open upper end, where it can be extended upward.
    for (const Segment* segment : sideSegments) {
        const Key key{segment->position, segment->section, segment->sidePlacement};
        if (!consumed.insert(key).second) continue;

        Module module{*segment, 1, 2};
        if (IsUpperSideCornerCap(*segment)) {
            --module.segment.position.y;
        } else if (IsRegularSideSegment(*segment)) {
            const Position upperPosition{segment->position.x, segment->position.y - 1};
            const Key upperKey{upperPosition, Section::Side, SidePlacement::Edge};
            const auto upper = frame.find(upperKey);
            if (upper != frame.end() && IsRegularSideSegment(upper->second) &&
                SameModuleStyle(*segment, upper->second) &&
                upper->second.edgeTile.x == segment->edgeTile.x &&
                upper->second.edgeTile.y + 1 == segment->edgeTile.y) {
                module.segment = upper->second;
                module.segment.visibility = std::min(segment->visibility,
                                                     upper->second.visibility);
                consumed.insert(upperKey);
            } else if (upper == frame.end()) {
                // An unmatched upper terminal (including a single-tile run)
                // grows upward so the module stays aligned to its lower edge.
                --module.segment.position.y;
            }
        }

        modules.push_back(module);
    }

    for (const Segment* segment : ordered) {
        if (segment->section == Section::Side) continue;

        const Key key{segment->position, segment->section, segment->sidePlacement};
        if (!consumed.insert(key).second) continue;

        Module module{*segment, 1, 1};
        Position partnerPosition = segment->position;
        bool canPair = false;

        if (segment->section == Section::Upper || segment->section == Section::Bottom) {
            ++partnerPosition.x;
            canPair = true;
        }

        if (canPair) {
            const Key partnerKey{partnerPosition, segment->section, segment->sidePlacement};
            const auto partner = frame.find(partnerKey);
            if (partner != frame.end() && SameModuleStyle(*segment, partner->second)) {
                if (partner->second.edgeTile.x == segment->edgeTile.x + 1 &&
                    partner->second.edgeTile.y == segment->edgeTile.y) {
                    module.widthTiles = 2;
                    module.heightTiles = 2;
                    module.segment.visibility = std::min(segment->visibility,
                                                         partner->second.visibility);
                    consumed.insert(partnerKey);
                }
            }
        }

        modules.push_back(module);
    }

    return modules;
}

void TileMemory::Reset(std::uint64_t seed) {
    seed_ = seed;
    choices_.clear();
}

const Choice& TileMemory::GetOrCreate(Position position, Section section, BiomeType biome,
                                      const std::vector<std::uint64_t>& cumulative,
                                      SidePlacement sidePlacement) {
    const Key key{position, section, sidePlacement};
    const auto found = choices_.find(key);
    if (found != choices_.end() && found->second.biome == biome) return found->second;

    std::size_t sprite = NoSprite;
    if (!cumulative.empty()) {
        const auto tileSeed = MakeRoomSeed(seed_, RoomCoords{position.x, position.y}, WallSalt(section, biome));
        std::mt19937_64 rng(tileSeed);
        std::uniform_int_distribution<std::uint64_t> distribution(0, cumulative.back() - 1);
        sprite = FloorRules::SelectTicket(cumulative, distribution(rng));
    }
    auto [it, inserted] = choices_.insert_or_assign(key, Choice{biome, sprite});
    (void)inserted;
    return it->second;
}

} // namespace WallRules
