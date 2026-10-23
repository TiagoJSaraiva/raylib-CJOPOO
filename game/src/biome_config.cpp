#include "biome_config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace BiomeConfig {
namespace {

std::string Normalize(std::string_view value) {
    std::size_t first = 0;
    std::size_t last = value.size();
    while (first < last && std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }

    std::string normalized(value.substr(first, last - first));
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return normalized;
}

} // namespace

std::optional<BiomeType> ParseBiomeOverride(std::string_view value) {
    const std::string normalized = Normalize(value);
    if (normalized == "cave") return BiomeType::Cave;
    if (normalized == "mansion") return BiomeType::Mansion;
    if (normalized == "dungeon") return BiomeType::Dungeon;
    return std::nullopt;
}

std::optional<BiomeType> LoadBiomeOverride(const char* path) {
    std::ifstream config(path);
    if (!config) {
        return std::nullopt;
    }

    const std::string value((std::istreambuf_iterator<char>(config)), std::istreambuf_iterator<char>());
    const std::string normalized = Normalize(value);
    if (normalized.empty()) {
        return std::nullopt;
    }

    const std::optional<BiomeType> biome = ParseBiomeOverride(normalized);
    if (!biome) {
        std::cerr << "[Config] Biome in " << path
                  << " must be cave, mansion, or dungeon; using the random biome.\n";
    }
    return biome;
}

} // namespace BiomeConfig
