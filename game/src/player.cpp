#include "player.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kBaseHealth = 100.0f;
constexpr float kHealthPerVigor = 12.0f;
constexpr float kBaseMovementSpeed = 250.0f;
constexpr float kMovementSpeedPerVelocidade = 0.03f;
constexpr float kMovementSpeedMaxMultiplier = 1.9f;
constexpr float kBasePickupRadius = 120.0f;
constexpr float kPickupRadiusPerPoint = 8.0f;
constexpr float kVampirismHealPercent = 0.02f;
constexpr float kCurseDamageDelta = 0.04f;
} // namespace

void PlayerCharacter::RecalculateStats() {
    totalAttributes = AddAttributes(baseAttributes, equipmentBonuses);
    totalAttributes = AddAttributes(totalAttributes, weaponBonuses);
    totalAttributes = AddAttributes(totalAttributes, temporaryBonuses);

    derivedStats.maxHealth = kBaseHealth + kHealthPerVigor * static_cast<float>(totalAttributes.primary.vigor);

    float speedMultiplier = 1.0f + kMovementSpeedPerVelocidade * static_cast<float>(totalAttributes.primary.velocidade);
    speedMultiplier = std::clamp(speedMultiplier, 0.0f, kMovementSpeedMaxMultiplier);
    derivedStats.movementSpeed = kBaseMovementSpeed * speedMultiplier;

    float defesa = static_cast<float>(totalAttributes.primary.defesa);
    derivedStats.damageMitigation = (defesa <= 0.0f) ? 0.0f : defesa / (defesa + 100.0f);

    derivedStats.pickupRadius = kBasePickupRadius + kPickupRadiusPerPoint * totalAttributes.secondary.alcanceColeta;

    derivedStats.vampirismChance = totalAttributes.secondary.vampirismo * 0.01f;
    derivedStats.vampirismHealPercent = kVampirismHealPercent;
    derivedStats.dodgeChance = totalAttributes.secondary.desvio * 0.01f;
    derivedStats.flatDamageReduction = totalAttributes.secondary.reducaoDano;
    derivedStats.luckBonus = totalAttributes.secondary.sorte * 0.01f;

    float curse = static_cast<float>(totalAttributes.secondary.maldicao);
    derivedStats.damageTakenMultiplierFromCurse = 1.0f + kCurseDamageDelta * curse;
    derivedStats.damageDealtMultiplierFromCurse = std::max(0.0f, 1.0f - kCurseDamageDelta * curse);

    currentHealth = std::min(currentHealth, derivedStats.maxHealth);
}

int PlayerCharacter::GetAttackAttributeValue(WeaponAttributeKey key) const {
    switch (key) {
        case WeaponAttributeKey::Constitution:
            return totalAttributes.attack.constituicao;
        case WeaponAttributeKey::Strength:
            return totalAttributes.attack.forca;
        case WeaponAttributeKey::Focus:
            return totalAttributes.attack.foco;
        case WeaponAttributeKey::Mysticism:
            return totalAttributes.attack.misticismo;
        case WeaponAttributeKey::Knowledge:
            return totalAttributes.attack.conhecimento;
    }
    return 0;
}

PlayerCharacter CreateKnightCharacter() {
    PlayerCharacter knight{};
    knight.id = "knight";
    knight.displayName = "Cavaleiro";

    knight.baseAttributes = PlayerAttributes{};
    knight.equipmentBonuses.primary.defesa = 2;
    knight.equipmentBonuses.primary.vigor = 2;
    knight.equipmentBonuses.secondary.reducaoDano = 5.0f;

    knight.RecalculateStats();
    knight.currentHealth = knight.derivedStats.maxHealth;
    knight.currentArmor = 0.0f;

    return knight;
}
