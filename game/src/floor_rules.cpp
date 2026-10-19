#include "floor_rules.h"

#include <algorithm>
#include <charconv>
#include <random>
#include <string_view>
#include <system_error>

namespace FloorRules {
namespace {
bool PositiveInteger(std::string_view text, std::uint64_t& value) {
    if (text.empty() || text.find_first_not_of("0123456789") != std::string_view::npos) {
        return false;
    }
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return result.ec == std::errc{} && result.ptr == text.data() + text.size() && value > 0;
}
} // namespace

std::optional<Metadata> ParseFilename(const std::string& filename) {
    const std::string_view name(filename);
    if (name.size() < 7 || name.substr(name.size() - 4) != ".png") {
        return std::nullopt;
    }
    const auto stem = name.substr(0, name.size() - 4);
    const auto separator = stem.find('-');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    Metadata metadata{};
    if (!PositiveInteger(stem.substr(0, separator), metadata.id) ||
        !PositiveInteger(stem.substr(separator + 1), metadata.weight)) {
        return std::nullopt;
    }
    return metadata;
}

bool AddWeight(std::vector<std::uint64_t>& cumulative, std::uint64_t weight) {
    const std::uint64_t total = cumulative.empty() ? 0 : cumulative.back();
    if (weight == 0 || weight > std::numeric_limits<std::uint64_t>::max() - total) {
        return false;
    }
    cumulative.push_back(total + weight);
    return true;
}

std::size_t SelectTicket(const std::vector<std::uint64_t>& cumulative, std::uint64_t ticket) {
    if (cumulative.empty() || ticket >= cumulative.back()) {
        return NoSprite;
    }
    return static_cast<std::size_t>(std::upper_bound(cumulative.begin(), cumulative.end(), ticket) - cumulative.begin());
}

bool IsSquareImage(int width, int height) {
    return width > 0 && width == height;
}

std::size_t PositionHash::operator()(const Position& position) const noexcept {
    return static_cast<std::size_t>(HashCombine(static_cast<std::uint64_t>(position.x),
                                               static_cast<std::uint64_t>(position.y)));
}

BiomeType CorridorBiome(RoomCoords a, BiomeType biomeA, RoomCoords b, BiomeType biomeB) {
    if (biomeA == biomeB) return biomeA;
    if (biomeA == BiomeType::Lobby) return biomeB;
    if (biomeB == BiomeType::Lobby) return biomeA;
    return (a.x < b.x || (a.x == b.x && a.y < b.y)) ? biomeA : biomeB;
}

void AddRect(Frame& frame, const TileRect& rect, BiomeType biome, float visibility) {
    if (!(visibility > 0.0f)) return;
    visibility = std::min(visibility, 1.0f);
    for (int y = rect.y; y < rect.y + rect.height; ++y) {
        for (int x = rect.x; x < rect.x + rect.width; ++x) {
            auto [it, inserted] = frame.emplace(Position{x, y}, VisibleTile{biome, visibility});
            if (!inserted) {
                it->second.visibility = std::max(it->second.visibility, visibility);
            }
        }
    }
}

void TileMemory::Reset(std::uint64_t seed) {
    seed_ = seed;
    tiles_.clear();
}

const Choice& TileMemory::GetOrCreate(Position position, BiomeType biome,
                                     const std::vector<std::uint64_t>& cumulative) {
    const auto found = tiles_.find(position);
    if (found != tiles_.end()) return found->second;

    std::size_t sprite = NoSprite;
    if (!cumulative.empty()) {
        const auto tileSeed = MakeRoomSeed(seed_, RoomCoords{position.x, position.y},
                                          HashCombine(0x464C4F4F52ULL, static_cast<std::uint64_t>(biome)));
        std::mt19937_64 rng(tileSeed);
        std::uniform_int_distribution<std::uint64_t> distribution(0, cumulative.back() - 1);
        sprite = SelectTicket(cumulative, distribution(rng));
    }
    return tiles_.emplace(position, Choice{biome, sprite}).first->second;
}
} // namespace FloorRules
