#pragma once

#include "raylib.h"

#include <cstdint>
#include <vector>

class Chest {
public:
    struct Slot {
        int itemId{0};
        int quantity{0};
    };

    enum class Type {
        Common,
        Player
    };

    Chest(Type type,
          float anchorX,
          float anchorY,
          float interactionRadius,
          const Rectangle& hitbox,
          int capacity);
    virtual ~Chest() = default;

    Type GetType() const { return type_; }
    float AnchorX() const { return anchorX_; }
    float AnchorY() const { return anchorY_; }
    float InteractionRadius() const { return interactionRadius_; }
    const Rectangle& Hitbox() const { return hitbox_; }

    int Capacity() const { return static_cast<int>(slots_.size()); }

    const Slot& GetSlot(int index) const;
    Slot& AccessSlot(int index);
    const std::vector<Slot>& GetSlots() const { return slots_; }
    std::vector<Slot>& GetSlots() { return slots_; }

    void SetSlot(int index, int itemId, int quantity);
    void ClearSlot(int index);

    virtual bool SupportsDeposit() const = 0;
    virtual bool SupportsTakeAll() const = 0;
    virtual const char* DisplayName() const = 0;

protected:
    Type type_;
    float anchorX_{0.0f};
    float anchorY_{0.0f};
    float interactionRadius_{0.0f};
    Rectangle hitbox_{0.0f, 0.0f, 0.0f, 0.0f};
    std::vector<Slot> slots_;
};

class CommonChest : public Chest {
public:
    CommonChest(float anchorX,
                float anchorY,
                float interactionRadius,
                const Rectangle& hitbox,
                int capacity,
                std::uint64_t lootSeed);

    bool SupportsDeposit() const override { return false; }
    bool SupportsTakeAll() const override { return true; }
    const char* DisplayName() const override { return "Bau"; }

    std::uint64_t LootSeed() const { return lootSeed_; }
    bool IsGenerated() const { return generated_; }
    void MarkGenerated() { generated_ = true; }

private:
    std::uint64_t lootSeed_{0};
    bool generated_{false};
};

class PlayerChest : public Chest {
public:
    PlayerChest(float anchorX,
                float anchorY,
                float interactionRadius,
                const Rectangle& hitbox,
                int capacity);

    bool SupportsDeposit() const override { return true; }
    bool SupportsTakeAll() const override { return false; }
    const char* DisplayName() const override { return "Bau pessoal"; }
};
