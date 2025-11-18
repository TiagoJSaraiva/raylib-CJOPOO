#include "player.h"

#include <algorithm>
#include <cmath>

namespace {
// Constantes base usadas nos cálculos de atributos derivados.
constexpr float kBaseHealth = 100.0f;
constexpr float kHealthPerVigor = 12.0f;
constexpr float kBaseMovementSpeed = 250.0f;
constexpr float kMovementSpeedPerVelocidade = 0.03f;
constexpr float kMovementSpeedMaxMultiplier = 1.9f;
constexpr float kBasePickupRadius = 120.0f;
constexpr float kPickupRadiusPerPoint = 8.0f;
constexpr float kVampirismHealPercent = 0.02f;
constexpr float kDefenseReductionScale = 0.6f;
constexpr float kDefenseNormalization = 61.0f;
constexpr float kMaxDodgeChance = 0.6f;
constexpr float kCursePercent = 0.01f;
} // namespace

// Recalcula atributos agregados e estatísticas derivadas considerando itens e buffs temporários.
void PlayerCharacter::RecalculateStats() {
    totalAttributes = AddAttributes(baseAttributes, equipmentBonuses);
    totalAttributes = AddAttributes(totalAttributes, weaponBonuses);
    totalAttributes = AddAttributes(totalAttributes, temporaryBonuses);

    derivedStats.maxHealth = kBaseHealth + kHealthPerVigor * static_cast<float>(totalAttributes.primary.vigor);

    // Velocidade de movimento cresce multiplicativamente e é limitada por um teto.
    float speedMultiplier = 1.0f + kMovementSpeedPerVelocidade * static_cast<float>(totalAttributes.primary.velocidade);
    speedMultiplier = std::clamp(speedMultiplier, 0.0f, kMovementSpeedMaxMultiplier);
    derivedStats.movementSpeed = kBaseMovementSpeed * speedMultiplier;

    float defesa = static_cast<float>(totalAttributes.primary.defesa);
    if (defesa > 0.0f) {
        float reduction = kDefenseReductionScale * std::log(defesa + 1.0f) / std::log(kDefenseNormalization);
        derivedStats.damageMitigation = std::clamp(reduction, 0.0f, kDefenseReductionScale);
    } else {
        derivedStats.damageMitigation = 0.0f;
    }

    derivedStats.pickupRadius = kBasePickupRadius + kPickupRadiusPerPoint * totalAttributes.secondary.alcanceColeta;

    derivedStats.vampirismChance = std::max(0.0f, totalAttributes.secondary.vampirismo * 0.01f);
    derivedStats.vampirismHealPercent = kVampirismHealPercent;
    derivedStats.dodgeChance = std::clamp(totalAttributes.secondary.desvio * 0.01f, 0.0f, kMaxDodgeChance);
    derivedStats.flatDamageReduction = totalAttributes.secondary.reducaoDano;
    derivedStats.luckBonus = totalAttributes.secondary.sorte * 0.01f;

    float curse = static_cast<float>(totalAttributes.secondary.maldicao);
    float curseMultiplier = 1.0f + (curse * kCursePercent);
    derivedStats.damageTakenMultiplierFromCurse = std::max(0.0f, curseMultiplier);
    float dealtDivisor = std::max(0.1f, 1.0f + curse * kCursePercent);
    derivedStats.damageDealtMultiplierFromCurse = 1.0f / dealtDivisor;

    // Garante que a vida atual não passe do novo máximo após aplicar buffs.
    currentHealth = std::min(currentHealth, derivedStats.maxHealth);
}

// Retorna o valor do atributo ofensivo que corresponde ao tipo de arma informado.
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

// Cria o personagem padrão do protótipo com aparência e atributos iniciais.
PlayerCharacter CreateKnightCharacter() {
    PlayerCharacter knight{};
    knight.id = "knight";
    knight.displayName = "Cavaleiro";
    knight.description = "Um defensor veterano que domina armas corpo a corpo.";

    knight.appearance.idleSpritePath = "assets/img/character/cavaleiro/idle_sprite.png";
    knight.appearance.walking.spriteSheetPath = "assets/img/character/cavaleiro/walking_spritesheet.png";
    knight.appearance.walking.frameWidth = 38;
    knight.appearance.walking.frameHeight = 68;
    knight.appearance.walking.frameCount = 4;
    knight.appearance.walking.secondsPerFrame = 0.14f; // Ajuste este valor para acelerar/ralentizar a animacao de caminhada.
    knight.appearance.walking.verticalLayout = true;

    knight.baseAttributes = PlayerAttributes{};
    knight.equipmentBonuses.primary.defesa = 2;
    knight.equipmentBonuses.primary.vigor = 2;
    knight.equipmentBonuses.secondary.reducaoDano = 5.0f;

    // Aplica bônus iniciais antes de liberar o personagem para uso.
    knight.RecalculateStats();
    knight.currentHealth = knight.derivedStats.maxHealth;
    knight.currentArmor = 0.0f;

    return knight;
}
