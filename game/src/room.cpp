#include "room.h"

#include <utility>

// Constrói uma sala concreta copiando seed/layout definidos na geração procedural.
Room::Room(RoomCoords coords, RoomSeedData seedData, RoomLayout layout)
    : coords_(coords), seedData_(seedData), layout_(std::move(layout)) {}

// Retorna porta correspondente à direção solicitada, caso exista no layout.
Doorway* Room::FindDoor(Direction direction) {
    for (auto& door : layout_.doors) {
        if (door.direction == direction) {
            return &door;
        }
    }
    return nullptr;
}

// Versão const da busca de porta, permitindo consulta em instâncias imutáveis.
const Doorway* Room::FindDoor(Direction direction) const {
    for (const auto& door : layout_.doors) {
        if (door.direction == direction) {
            return &door;
        }
    }
    return nullptr;
}

// Localiza porta cujo destino leva à sala indicada em target.
Doorway* Room::FindDoorTo(const RoomCoords& target) {
    for (auto& door : layout_.doors) {
        if (door.targetCoords == target) {
            return &door;
        }
    }
    return nullptr;
}

// Retorna ponteiro opcional para a forja ativa da sala.
ForgeInstance* Room::GetForge() {
    if (!forge_.has_value()) {
        return nullptr;
    }
    return &forge_.value();
}

// Versão const do acesso à forja, útil para renderização/consulta.
const ForgeInstance* Room::GetForge() const {
    if (!forge_.has_value()) {
        return nullptr;
    }
    return &forge_.value();
}

// Instala nova forja configurada para esta sala.
void Room::SetForge(const ForgeInstance& forge) {
    forge_ = forge;
}

// Remove a forja de forma segura.
void Room::ClearForge() {
    forge_.reset();
}

// Retorna referência opcional para a loja atual.
ShopInstance* Room::GetShop() {
    if (!shop_.has_value()) {
        return nullptr;
    }
    return &shop_.value();
}

// Versão const da consulta de loja.
const ShopInstance* Room::GetShop() const {
    if (!shop_.has_value()) {
        return nullptr;
    }
    return &shop_.value();
}

// Define dados da loja posicionada nesta sala.
void Room::SetShop(const ShopInstance& shop) {
    shop_ = shop;
}

// Limpa loja e libera espaço para nova geração.
void Room::ClearShop() {
    shop_.reset();
}

// Acesso direto ao baú (pode ser nullptr quando não existe).
Chest* Room::GetChest() {
    return chest_.get();
}

// Versão const do acesso ao baú para cenários de leitura apenas.
const Chest* Room::GetChest() const {
    return chest_.get();
}

// Move propriedade do baú recém-criado para a sala.
void Room::SetChest(std::unique_ptr<Chest> chest) {
    chest_ = std::move(chest);
}

// Remove e desaloca o baú associado.
void Room::ClearChest() {
    chest_.reset();
}
