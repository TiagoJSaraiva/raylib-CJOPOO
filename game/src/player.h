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
    int poder{0};
    int defesa{0};
    int vigor{0};
    int velocidade{0};
    int destreza{0};
    int inteligencia{0};
};

struct AttackAttributes {
    int constituicao{0};
    int forca{0};
    int foco{0};
    int misticismo{0};
    int conhecimento{0};
};

struct SecondaryAttributes {
    float vampirismo{0.0f};
    float letalidade{0.0f};
    float reducaoDano{0.0f};
    float desvio{0.0f};
    float alcanceColeta{0.0f};
    float sorte{0.0f};
    int maldicao{0};
};

struct PlayerAttributes {
    PrimaryAttributes primary{};
    AttackAttributes attack{};
    SecondaryAttributes secondary{};
};

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
