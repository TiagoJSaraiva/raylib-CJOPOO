#pragma once

#include "raylib.h"

#include <cstdint>
#include <vector>

class Chest { // Representa um bau interativo com slots de itens e regras de saque
public:
    struct Slot { // Guarda um item e a quantidade ocupando um slot do bau
        int itemId{0}; // ID do item armazenado neste slot
        int quantity{0}; // Quantidade do item armazenada neste slot
    };

    enum class Type { // Tipifica o bau para diferenciar comportamento comum x pessoal
        Common,
        Player
    };

    Chest(Type type,
          float anchorX,
          float anchorY,
          float interactionRadius,
          const Rectangle& hitbox,
          int capacity); // Construtor que posiciona o bau e define capacidade/areas de interacao
    virtual ~Chest() = default; // Destrutor virtual padrao para permitir polimorfismo

    Type GetType() const { return type_; } // Retorna o tipo geral do bau
    float AnchorX() const { return anchorX_; } // Retorna a coordenada X usada como ancora visual
    float AnchorY() const { return anchorY_; } // Retorna a coordenada Y usada como ancora visual
    float InteractionRadius() const { return interactionRadius_; } // Retorna o raio em que o jogador pode interagir
    const Rectangle& Hitbox() const { return hitbox_; } // Retorna o hitbox utilizado para colisao

    int Capacity() const { return static_cast<int>(slots_.size()); } // Informa quantos slots o bau possui

    const Slot& GetSlot(int index) const; // Retorna leitura de um slot especifico
    Slot& AccessSlot(int index); // Retorna referencia mutavel para um slot especifico
    const std::vector<Slot>& GetSlots() const { return slots_; } // Permite iterar slots em modo leitura
    std::vector<Slot>& GetSlots() { return slots_; } // Permite iterar slots para modificacao

    void SetSlot(int index, int itemId, int quantity); // Define id/quantidade em um slot
    void ClearSlot(int index); // Esvazia completamente o slot indicado

    virtual bool SupportsDeposit() const = 0; // Indica se o bau aceita deposito de itens
    virtual bool SupportsTakeAll() const = 0; // Indica se ha botao de pegar tudo
    virtual const char* DisplayName() const = 0; // Informa o nome exibido no HUD

protected:
    Type type_; // Guarda o tipo do bau (comum ou pessoal)
    float anchorX_{0.0f}; // Posicao X usada para desenhar o bau
    float anchorY_{0.0f}; // Posicao Y usada para desenhar o bau
    float interactionRadius_{0.0f}; // Distancia maxima para liberar interacao com E
    Rectangle hitbox_{0.0f, 0.0f, 0.0f, 0.0f}; // Caixa usada para colisao fisica
    std::vector<Slot> slots_; // Lista de slots que armazenam itens
};

class CommonChest : public Chest { // Implementa bau padrao encontrado nas salas.
public:
    CommonChest(float anchorX,
                float anchorY,
                float interactionRadius,
                const Rectangle& hitbox,
                int capacity,
                std::uint64_t lootSeed); // Construtor que fixa capacidade e semente para loot aleatorio

    bool SupportsDeposit() const override { return false; } // Bau comum nao aceita deposito
    bool SupportsTakeAll() const override { return true; } // Permite pegar tudo de uma vez
    const char* DisplayName() const override { return "Bau"; } // Nome mostrado na UI

    std::uint64_t LootSeed() const { return lootSeed_; } // Retorna a seed usada para gerar loot
    bool IsGenerated() const { return generated_; } // Indica se o loot ja foi criado
    void MarkGenerated() { generated_ = true; } // Marca que o conteudo foi preenchido

private:
    std::uint64_t lootSeed_{0}; // Seed deterministica para gerar itens
    bool generated_{false}; // Flag para evitar gerar loot mais de uma vez
};

class PlayerChest : public Chest { // Bau persistente do jogador para armazenamento pessoal
public:
    PlayerChest(float anchorX,
                float anchorY,
                float interactionRadius,
                const Rectangle& hitbox,
                int capacity); // Construtor que define caracteristicas do bau pessoal

    bool SupportsDeposit() const override { return true; } // Permite guardar itens
    bool SupportsTakeAll() const override { return false; } // Nao oferece botao de pegar tudo
    const char* DisplayName() const override { return "Bau pessoal"; } // Nome exibido para este bau
};
