#include "chest.h"

#include <algorithm>

namespace {

const Chest::Slot kEmptySlot{}; // Slot vazio reutilizado quando o indice solicitado nao existe

} // namespace

Chest::Chest(Type type,
             float anchorX,
             float anchorY,
             float interactionRadius,
             const Rectangle& hitbox,
             int capacity) // Recebe configuracoes basicas do bau, cria slots e limita capacidade
    : type_(type),
      anchorX_(anchorX),
      anchorY_(anchorY),
      interactionRadius_(interactionRadius),
      hitbox_(hitbox) {
    capacity = std::max(0, capacity);
    slots_.resize(static_cast<size_t>(capacity));
}

const Chest::Slot& Chest::GetSlot(int index) const { // Recebe um indice, valida limites e devolve slot ou kEmptySlot
    if (index < 0 || index >= static_cast<int>(slots_.size())) {
        return kEmptySlot;
    }
    return slots_[static_cast<size_t>(index)];
}

Chest::Slot& Chest::AccessSlot(int index) { // Recebe um indice e entrega referencia mutavel ao slot (ou fallback seguro)
    if (slots_.empty()) {
        static Chest::Slot dummy{};
        dummy.itemId = 0;
        dummy.quantity = 0;
        return dummy;
    }
    if (index < 0 || index >= static_cast<int>(slots_.size())) {
        return slots_.front();
    }
    return slots_[static_cast<size_t>(index)];
}

void Chest::SetSlot(int index, int itemId, int quantity) { // Atualiza o slot indicado com item/quantidade, limpando se itemId invalido
    if (index < 0 || index >= static_cast<int>(slots_.size())) {
        return;
    }
    Chest::Slot& slot = slots_[static_cast<size_t>(index)];
    if (itemId <= 0) {
        slot.itemId = 0;
        slot.quantity = 0;
    } else {
        slot.itemId = itemId;
        slot.quantity = std::max(1, quantity);
    }
}

void Chest::ClearSlot(int index) { // Recebe um indice e limpa o slot chamando SetSlot com valores nulos
    SetSlot(index, 0, 0);
}

CommonChest::CommonChest(float anchorX,
                         float anchorY,
                         float interactionRadius,
                         const Rectangle& hitbox,
                         int capacity,
                         std::uint64_t lootSeed) // Inicializa bau comum com dados de posicao/capacidade e semente deterministica
    : Chest(Type::Common, anchorX, anchorY, interactionRadius, hitbox, capacity),
      lootSeed_(lootSeed) {
}

PlayerChest::PlayerChest(float anchorX,
                         float anchorY,
                         float interactionRadius,
                         const Rectangle& hitbox,
                         int capacity) // Cria bau pessoal usando configuracao recebida e tipo Player
    : Chest(Type::Player, anchorX, anchorY, interactionRadius, hitbox, capacity) {
}
