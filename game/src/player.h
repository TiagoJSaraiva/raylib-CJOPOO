#pragma once

#include <algorithm>
#include <string>

enum class WeaponAttributeKey {
    Constitution,
    Strength,
    Focus,
    Mysticism,
    Knowledge
};

struct PrimaryAttributes {
    int poder{0}; // Aumenta o dano total em %
    int defesa{0}; // Reduz o dano recebido em %
    int vigor{0}; // Aumenta a vida maxima
    int velocidade{0}; // Aumenta a velocidade de movimento (não sei se está funcionando)
    int destreza{0}; // Aumenta a velocidade de ataque
    int inteligencia{0}; // Diminui o cooldown de habilidades (habilidades ainda não existem, mas esse atributo será usado no futuro)
};

struct AttackAttributes {
    int constituicao{0}; // Aumenta o dano de armas de atributo constituição
    int forca{0}; // Aumenta o dano de armas de atributo força
    int foco{0}; // Aumenta o dano de armas de atributo foco
    int misticismo{0}; // Aumenta o dano de armas de atributo misticismo
    int conhecimento{0}; // Aumenta o dano de armas de atributo conhecimento
};

struct SecondaryAttributes {
    float vampirismo{0.0f}; // % do dano causado que é convertido em vida (ainda não implementado)
    float letalidade{0.0f}; // Aumenta a chance de acerto crítico.
    float reducaoDano{0.0f}; // Reduz o dano recebido em valor flat (ainda não sei se foi implementado)
    float desvio{0.0f}; // Aumenta a chance de esquiva - esquiva: evita 100% do dano de um ataque (ainda não sei se foi implementado)
    float alcanceColeta{0.0f}; // Será removido. Não necessário.
    float sorte{0.0f}; // Aumenta a chance de encontrar itens raros (ainda não implementado)
    int maldicao{0}; // Reduz o dano causado e aumenta o dano recebido (ainda não sei se foi implementado)
};

struct PlayerAttributes {
    PrimaryAttributes primary{};
    AttackAttributes attack{};
    SecondaryAttributes secondary{};
};

inline bool operator==(const PrimaryAttributes& lhs, const PrimaryAttributes& rhs) {
    return lhs.poder == rhs.poder && lhs.defesa == rhs.defesa && lhs.vigor == rhs.vigor &&
           lhs.velocidade == rhs.velocidade && lhs.destreza == rhs.destreza && lhs.inteligencia == rhs.inteligencia;
}

inline bool operator==(const AttackAttributes& lhs, const AttackAttributes& rhs) {
    return lhs.constituicao == rhs.constituicao && lhs.forca == rhs.forca && lhs.foco == rhs.foco &&
           lhs.misticismo == rhs.misticismo && lhs.conhecimento == rhs.conhecimento;
}

inline bool operator==(const SecondaryAttributes& lhs, const SecondaryAttributes& rhs) {
    return lhs.vampirismo == rhs.vampirismo && lhs.letalidade == rhs.letalidade && lhs.reducaoDano == rhs.reducaoDano &&
           lhs.desvio == rhs.desvio && lhs.alcanceColeta == rhs.alcanceColeta && lhs.sorte == rhs.sorte &&
           lhs.maldicao == rhs.maldicao;
}

inline bool operator==(const PlayerAttributes& lhs, const PlayerAttributes& rhs) {
    return lhs.primary == rhs.primary && lhs.attack == rhs.attack && lhs.secondary == rhs.secondary;
}

inline bool operator!=(const PlayerAttributes& lhs, const PlayerAttributes& rhs) {
    return !(lhs == rhs);
}

inline PlayerAttributes AddAttributes(const PlayerAttributes& a, const PlayerAttributes& b) {
    PlayerAttributes result{};
    result.primary.poder = a.primary.poder + b.primary.poder;
    result.primary.defesa = a.primary.defesa + b.primary.defesa;
    result.primary.vigor = a.primary.vigor + b.primary.vigor;
    result.primary.velocidade = a.primary.velocidade + b.primary.velocidade;
    result.primary.destreza = a.primary.destreza + b.primary.destreza;
    result.primary.inteligencia = a.primary.inteligencia + b.primary.inteligencia;

    result.attack.constituicao = a.attack.constituicao + b.attack.constituicao;
    result.attack.forca = a.attack.forca + b.attack.forca;
    result.attack.foco = a.attack.foco + b.attack.foco;
    result.attack.misticismo = a.attack.misticismo + b.attack.misticismo;
    result.attack.conhecimento = a.attack.conhecimento + b.attack.conhecimento;

    result.secondary.vampirismo = a.secondary.vampirismo + b.secondary.vampirismo;
    result.secondary.letalidade = a.secondary.letalidade + b.secondary.letalidade;
    result.secondary.reducaoDano = a.secondary.reducaoDano + b.secondary.reducaoDano;
    result.secondary.desvio = a.secondary.desvio + b.secondary.desvio;
    result.secondary.alcanceColeta = a.secondary.alcanceColeta + b.secondary.alcanceColeta;
    result.secondary.sorte = a.secondary.sorte + b.secondary.sorte;
    result.secondary.maldicao = a.secondary.maldicao + b.secondary.maldicao;
    return result;
}

inline PlayerAttributes& AddAttributesInPlace(PlayerAttributes& target, const PlayerAttributes& source) {
    target = AddAttributes(target, source);
    return target;
}

struct PlayerDerivedStats {
    float maxHealth{100.0f};
    float movementSpeed{250.0f};
    float damageMitigation{0.0f};
    float pickupRadius{120.0f};
    float vampirismChance{0.0f};
    float vampirismHealPercent{0.02f};
    float dodgeChance{0.0f};
    float flatDamageReduction{0.0f};
    float luckBonus{0.0f};
    float damageTakenMultiplierFromCurse{1.0f};
    float damageDealtMultiplierFromCurse{1.0f};
};

struct CharacterAnimationClip {
    std::string spriteSheetPath{};
    int frameWidth{0};
    int frameHeight{0};
    int frameCount{0};
    float secondsPerFrame{0.12f};
    bool verticalLayout{true};
};

struct CharacterAppearanceBlueprint {
    std::string idleSpritePath{};
    CharacterAnimationClip walking{};
};

struct PlayerCharacter {
    std::string id{};
    std::string displayName{};
    std::string description{};
    CharacterAppearanceBlueprint appearance{};
    PlayerAttributes baseAttributes{};
    PlayerAttributes equipmentBonuses{};
    PlayerAttributes weaponBonuses{};
    PlayerAttributes temporaryBonuses{};
    PlayerAttributes totalAttributes{};
    PlayerDerivedStats derivedStats{};
    float currentHealth{0.0f};
    float currentArmor{0.0f};

    void RecalculateStats();
    int GetAttackAttributeValue(WeaponAttributeKey key) const;
};

PlayerCharacter CreateKnightCharacter();
