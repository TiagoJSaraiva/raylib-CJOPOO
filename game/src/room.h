#pragma once


#include "raylib.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "room_types.h"
#include "chest.h"

// Situação atual da porta para determinar se o jogador pode atravessar.
enum class DoorInteractionState {
    Unlocked,
    Locked,
    Unavailable
};

// Estado compartilhado entre salas conectadas por uma mesma porta.
struct DoorInstance {
    DoorInteractionState interactionState{DoorInteractionState::Unlocked};
    bool opening{false};
    bool open{false};
    float fadeProgress{0.0f};
    bool maskActive{true};
};

// Informações geométricas de uma porta dentro do layout da sala.
struct Doorway {
    Direction direction{Direction::North};
    int offset{0};
    int width{DOOR_WIDTH_TILES};
    int corridorLength{2};
    RoomCoords targetCoords{};
    TileRect corridorTiles{};
    bool targetGenerated{false};
    bool sealed{false};
    std::shared_ptr<DoorInstance> doorState{};
};

// Dados determinísticos usados para gerar a sala (tipo, bioma e seed).
struct RoomSeedData {
    RoomType type{RoomType::Unknown};
    BiomeType biome{BiomeType::Unknown};
    std::uint64_t seed{0};
};

// Representa o retângulo e as portas existentes em uma sala.
struct RoomLayout {
    int widthTiles{0};
    int heightTiles{0};
    TileRect tileBounds{};
    std::vector<Doorway> doors;
};

// Estado mecânico da forja (funcionando ou quebrada).
enum class ForgeState {
    Working,
    Broken
};

// Instância de forja posicionada dentro da sala, com slots e raio de interação.
struct ForgeInstance {
    float anchorX{0.0f};
    float anchorY{0.0f};
    float interactionRadius{96.0f};
    Rectangle hitbox{0.0f, 0.0f, 0.0f, 0.0f};
    ForgeState state{ForgeState::Working};

    struct Slot {
        int itemId{0};
        int quantity{0};
    };

    // Representa o conteúdo do UI de forja (dois insumos + resultado).
    struct Contents {
        std::array<Slot, 2> inputs{};
        Slot result{};
    } contents{};

    bool IsBroken() const { return state == ForgeState::Broken; }
    void SetBroken() { state = ForgeState::Broken; }
    void SetWorking() { state = ForgeState::Working; }
};

// Item listado em uma loja (id, preço e estoque atual).
struct ShopInventoryEntry {
    int itemId{0};
    int price{0};
    int stock{0};
};

// Entidade de loja com seed base para rerolls.
struct ShopInstance {
    float anchorX{0.0f};
    float anchorY{0.0f};
    float interactionRadius{120.0f};
    Rectangle hitbox{0.0f, 0.0f, 0.0f, 0.0f};
    int textureVariant{0};
    std::uint64_t baseSeed{0};
    std::uint32_t rerollCount{0};
    std::vector<ShopInventoryEntry> items;

    // Seed derivada que usa rerollCount para variar a oferta.
    std::uint64_t CurrentSeed() const {
        constexpr std::uint64_t kHash = 0x9E3779B97F4A7C15ULL;
        return baseSeed ^ (static_cast<std::uint64_t>(rerollCount) * kHash);
    }
};

// Classe central que agrega layout, objetos e estado runtime de uma sala.
class Room {
public:
    Room(RoomCoords coords, RoomSeedData seedData, RoomLayout layout);

    // Identificação básica e metadados do bioma/seed.
    RoomCoords GetCoords() const { return coords_; }
    RoomType GetType() const { return seedData_.type; }
    BiomeType GetBiome() const { return seedData_.biome; }
    std::uint64_t GetSeed() const { return seedData_.seed; }

    // Acesso ao layout para leitura/edição das portas e bounds.
    RoomLayout& Layout() { return layout_; }
    const RoomLayout& Layout() const { return layout_; }

    // Utilidades para localizar portas específicas.
    Doorway* FindDoor(Direction direction);
    Doorway* FindDoorTo(const RoomCoords& target);
    const Doorway* FindDoor(Direction direction) const;

    // Flag usada para mapear minimapa/revelação.
    bool IsVisited() const { return visited_; }
    void SetVisited(bool visited) { visited_ = visited; }

    bool DoorsInitialized() const { return doorsInitialized_; }
    void SetDoorsInitialized(bool initialized) { doorsInitialized_ = initialized; }

    std::optional<Direction> GetEntranceDirection() const { return entranceDirection_; }
    void SetEntranceDirection(std::optional<Direction> direction) { entranceDirection_ = direction; }

    // ---- Forja ----
    bool HasForge() const { return forge_.has_value(); }
    ForgeInstance* GetForge();
    const ForgeInstance* GetForge() const;
    void SetForge(const ForgeInstance& forge);
    void ClearForge();

    // ---- Loja ----
    bool HasShop() const { return shop_.has_value(); }
    ShopInstance* GetShop();
    const ShopInstance* GetShop() const;
    void SetShop(const ShopInstance& shop);
    void ClearShop();

    // ---- Baú ----
    bool HasChest() const { return static_cast<bool>(chest_); }
    Chest* GetChest();
    const Chest* GetChest() const;
    void SetChest(std::unique_ptr<Chest> chest);
    void ClearChest();

private:
    RoomCoords coords_{}; // Posição da sala no grid.
    RoomSeedData seedData_{}; // Dados usados para reproduzir o layout/bioma.
    RoomLayout layout_{}; // Estrutura física com portas e dimensões em tiles.
    bool visited_{false}; // Marca se o jogador já explorou esta sala.
    bool doorsInitialized_{false}; // Evita reinicializar portas mais de uma vez.
    std::optional<Direction> entranceDirection_{}; // Porta pela qual a sala foi acessada.
    std::optional<ForgeInstance> forge_{};
    std::optional<ShopInstance> shop_{};
    std::unique_ptr<Chest> chest_{};
};
