#include "floor_tiles.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>

namespace {
constexpr const char* kDirectories[] = {"lobby", "cave", "mansion", "dungeon"};

Color FloorColor(BiomeType biome) {
    switch (biome) {
        case BiomeType::Cave: return Color{58, 62, 70, 255};
        case BiomeType::Mansion: return Color{72, 54, 42, 255};
        case BiomeType::Dungeon: return Color{46, 66, 56, 255};
        default: return Color{50, 52, 63, 255};
    }
}

std::size_t CatalogIndex(BiomeType biome) {
    const auto index = static_cast<std::size_t>(biome);
    return index < 5 ? index : static_cast<std::size_t>(BiomeType::Unknown);
}
} // namespace

FloorTiles::FloorTiles() {
    namespace fs = std::filesystem;
    for (std::size_t i = 0; i < 4; ++i) {
        auto& catalog = catalogs_[i];
        const fs::path directory = fs::path("assets/floor") / kDirectories[i];
        std::vector<fs::path> paths;
        std::error_code error;
        fs::directory_iterator iterator(directory, error);
        const fs::directory_iterator end;
        while (!error && iterator != end) {
            const auto entry = *iterator;
            std::error_code fileError;
            if (entry.is_regular_file(fileError)) paths.push_back(entry.path());
            iterator.increment(error);
        }
        if (error) {
            std::cerr << "[FloorTiles] Cannot read " << directory.generic_string() << ": " << error.message() << '\n';
        }
        std::sort(paths.begin(), paths.end());
        for (const auto& path : paths) {
            const auto name = path.filename().string();
            const auto metadata = FloorRules::ParseFilename(name);
            if (!metadata) {
                // Placeholder files and documentation are not images.
                if (path.extension() == ".png" || path.extension() == ".PNG") {
                    std::cerr << "[FloorTiles] Invalid filename: " << path.generic_string() << '\n';
                }
                continue;
            }
            // Decode once, validate dimensions, then upload only valid images.
            Image image = LoadImage(path.generic_string().c_str());
            if (image.data == nullptr || !FloorRules::IsSquareImage(image.width, image.height)) {
                std::cerr << "[FloorTiles] Expected a readable square image: " << path.generic_string() << '\n';
                if (image.data != nullptr) UnloadImage(image);
                continue;
            }
            if (!FloorRules::AddWeight(catalog.cumulative, metadata->weight)) {
                std::cerr << "[FloorTiles] Weight sum overflow: " << path.generic_string() << '\n';
                UnloadImage(image);
                continue;
            }
            Texture2D texture = LoadTextureFromImage(image);
            UnloadImage(image);
            if (texture.id == 0) {
                catalog.cumulative.pop_back();
                std::cerr << "[FloorTiles] Cannot upload texture: " << path.generic_string() << '\n';
                continue;
            }
            SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
            catalog.textures.push_back(texture);
        }
        if (catalog.textures.empty()) {
            std::cerr << "[FloorTiles] No valid sprites in " << directory.generic_string()
                      << "; using solid floor color.\n";
        }
    }
}

FloorTiles::~FloorTiles() { Unload(); }

void FloorTiles::Unload() {
    for (auto& catalog : catalogs_) {
        for (auto texture : catalog.textures) UnloadTexture(texture);
        catalog.textures.clear();
        catalog.cumulative.clear();
    }
    memory_.Reset(0);
}

void FloorTiles::BeginRun(std::uint64_t seed) { memory_.Reset(seed); }

void FloorTiles::Draw(const std::vector<FloorRoomView>& rooms) const {
    std::unordered_map<RoomCoords, const Room*, RoomCoordsHash> lookup;
    for (const auto& view : rooms) {
        if (view.room != nullptr) lookup.emplace(view.room->GetCoords(), view.room);
    }
    FloorRules::Frame frame;
    for (const auto& view : rooms) {
        if (view.room == nullptr || !(view.visibility > 0.0f)) continue;
        const Room& room = *view.room;
        FloorRules::AddRect(frame, room.Layout().tileBounds, room.GetBiome(), view.visibility);
        for (const auto& door : room.Layout().doors) {
            if (door.sealed) continue;
            auto biome = room.GetBiome();
            const auto target = lookup.find(door.targetCoords);
            if (target != lookup.end()) {
                biome = FloorRules::CorridorBiome(room.GetCoords(), biome, target->first, target->second->GetBiome());
            }
            FloorRules::AddRect(frame, door.corridorTiles, biome, view.visibility);
        }
    }
    for (const auto& [position, tile] : frame) {
        const auto& choice = memory_.GetOrCreate(position, tile.biome, catalogs_[CatalogIndex(tile.biome)].cumulative);
        const Rectangle destination{static_cast<float>(position.x) * TILE_SIZE,
                                    static_cast<float>(position.y) * TILE_SIZE,
                                    static_cast<float>(TILE_SIZE), static_cast<float>(TILE_SIZE)};
        const auto& catalog = catalogs_[CatalogIndex(choice.biome)];
        if (choice.sprite == FloorRules::NoSprite) {
            DrawRectangleRec(destination, ColorAlpha(FloorColor(choice.biome), tile.visibility));
        } else {
            const auto texture = catalog.textures[choice.sprite];
            const Rectangle source{0, 0, static_cast<float>(texture.width), static_cast<float>(texture.height)};
            DrawTexturePro(texture, source, destination, Vector2{0, 0}, 0, ColorAlpha(WHITE, tile.visibility));
        }
    }
}
