#include "weapon_blueprints.h"

#include "projectile.h"

// Tweak weapon balance here: each blueprint defines how a weapon behaves.
// - damage: dano base e quanto cresce por ponto do atributo listado em attributeKey.
// - cadence: ataques por segundo base, ganho por Destreza e limite maximo.
// - critical: chance base, ganho por ponto de Letalidade e multiplicador.
// - passiveBonuses: atributos que o jogador recebe enquanto a arma esta equipada.
// Ajuste tambem os parametros do ProjectileBlueprint para mudar alcance, hitbox e velocidade dos golpes.

namespace {

// Conjuntos internos para montar os projetis e stats de cada arma.

// Configura hitbox/visual do ataque de broquel.
ProjectileBlueprint MakeBroquelProjectileBlueprint() {
    short length = 38;
    short thickness = 80;
    short radius = 50;

    ProjectileBlueprint blueprint{}; // Container with default projectile values
    blueprint.kind = ProjectileKind::Blunt; // Shield bash uses the blunt (arc) hitbox logic
    blueprint.common.damage = 10.0f; // Baseline damage in case no weapon scaling overrides it
    blueprint.common.lifespanSeconds = 0.75f; // Hitbox persists roughly for the bash animation
    blueprint.common.projectileSpeed = 0.0f; // Arc stays attached to the wielder instead of travelling
    blueprint.common.displayLength = length;
    blueprint.common.displayThickness = thickness;
    blueprint.common.projectilesPerShot = 1; // Single swing per activation
    blueprint.common.randomSpreadDegrees = 0.0f; // No spread because the arc is deterministic
    blueprint.common.debugColor = Color{210, 240, 160, 255}; // Light green tint for debugging visuals
    blueprint.common.projectileSpritePath = "assets/img/weapons/Broquel.png"; // Sprite renderizado a frente do jogador
    blueprint.common.projectileRotationOffsetDegrees = 180.0f; // Mantem o broquel virado na direção correta
    blueprint.common.projectileForwardOffset = radius; // Empurra o sprite um pouco para fora do corpo do jogador
    blueprint.common.perTargetHitCooldownSeconds = 0.45f; // Pequeno intervalo de invulnerabilidade por alvo

    blueprint.blunt.radius = radius; // Distancia do centro da hitbox ate o centro do escudo
    blueprint.blunt.travelDegrees = 0.0f; // Permanece alinhado ao alvo durante a batida
    blueprint.blunt.length = length; // Comprimento do retangulo de colisao do broquel
    blueprint.blunt.thickness = thickness; // Largura do volume para colisao e desenho
    blueprint.blunt.followOwner = true; // Arc keeps following the player as they move

    return blueprint;
}

// Blueprint completo do Broquel (estatísticas, cadência, passivos).
WeaponBlueprint MakeBroquelWeaponBlueprint() {
    WeaponBlueprint blueprint{}; // Blueprint bundling weapon stats and visuals
    blueprint.name = "Broquel"; // Display name shown in UI
    blueprint.projectile = MakeBroquelProjectileBlueprint(); // Uses the shield bash projectile defined above
    blueprint.cooldownSeconds = 0.9f; // Base interval between attacks before cadence modifiers
    blueprint.holdToFire = false; // Single tap triggers the bash; holding does not repeat automatically
    blueprint.attributeKey = WeaponAttributeKey::Constitution; // Shield scales with the user's Constitution stat
    blueprint.damage.baseDamage = 10.0f; // Flat damage applied even with zero Constitution
    blueprint.damage.attributeScaling = 1.5f; // Additional damage gained per Constitution point
    blueprint.cadence.baseAttacksPerSecond = 0.6f; // Broquel is intentionally slow to swing
    blueprint.cadence.dexterityGainPerPoint = 0.18f; // Dexterity increases bash frequency modestly
    blueprint.cadence.attacksPerSecondCap = 2.2f; // Upper limit to keep defensive role in check
    blueprint.critical.baseChance = 0.05f; // Low innate crit chance for a shield strike
    blueprint.critical.chancePerLetalidade = 0.005f; // Small crit gain per Letalidade point
    blueprint.critical.multiplier = 1.2f; // Shield crits hurt slightly more than base hits
    blueprint.passiveBonuses.primary.defesa = 5; // Grants flat defense while equipped
    blueprint.passiveBonuses.secondary.sorte = 2.0f; // Small luck boost to reward defensive play
    blueprint.inventorySprite.spritePath = "assets/img/weapons/Broquel.png";
    blueprint.inventorySprite.drawSize = Vector2{48.0f, 40.0f};
    blueprint.inventorySprite.rotationDegrees = 90.0f;

    return blueprint;
}

// Hitbox e aparencia do golpe da espada curta.
ProjectileBlueprint MakeEspadaCurtaProjectileBlueprint() {
    ProjectileBlueprint blueprint{}; // Baseline projectile setup for sword slashes
    blueprint.kind = ProjectileKind::Swing; // Uses the swing arc implementation
    blueprint.common.damage = 12.0f; // Fallback damage before weapon scaling is applied
    blueprint.common.lifespanSeconds = 0.35f; // Slash persists roughly for the swing animation
    blueprint.common.projectilesPerShot = 1; // Single arc per attack trigger
    blueprint.common.randomSpreadDegrees = 0.0f; // Swords do not randomize their swing angle
    blueprint.common.debugColor = Color{240, 210, 180, 255}; // Light beige for debug overlays
    blueprint.common.weaponSpritePath = "assets/img/weapons/Espada_Curta.png"; // Sprite renderizado a frente do jogador
    blueprint.common.displayLength = 110.0f; 
    blueprint.common.displayThickness = 28.0f; 
    blueprint.common.perTargetHitCooldownSeconds = 0.50f; // Cooldown por alvo para evitar multi-hits absurdos

    blueprint.swing.length = 110.0f; // Length from the player to the tip of the swing
    blueprint.swing.thickness = 28.0f; // Width of the arc area to hit enemies
    blueprint.swing.travelDegrees = 110.0f; // Angle covered during the swing animation
    blueprint.swing.followOwner = true; // Slash remains anchored to the wielder's position

    return blueprint;
}

// Define escala/atributos da Espada Curta e vincula o projétil.
WeaponBlueprint MakeEspadaCurtaWeaponBlueprint() {
    WeaponBlueprint blueprint{}; // Container describing how the short sword behaves
    blueprint.name = "Espada Curta"; // UI label for this weapon
    blueprint.projectile = MakeEspadaCurtaProjectileBlueprint(); // Reuses the slash projectile above
    blueprint.cooldownSeconds = 0.6f; // Base cooldown before cadence modifiers kick in
    blueprint.holdToFire = false; // Player must tap each swing manually
    blueprint.attributeKey = WeaponAttributeKey::Strength; // Sword damage scales with Strength
    blueprint.damage.baseDamage = 12.0f; // Base Strength-independent damage
    blueprint.damage.attributeScaling = 1.5f; // Extra damage per point of Strength
    blueprint.cadence.baseAttacksPerSecond = 1.4f; // Naturally quicker than heavy weapons
    blueprint.cadence.dexterityGainPerPoint = 0.12f; // Dexterity adds moderate swing speed
    blueprint.cadence.attacksPerSecondCap = 3.0f; // Prevents absurd attack speed stacking
    blueprint.critical.baseChance = 0.08f; // Baseline crit rate for precise blade hits
    blueprint.critical.chancePerLetalidade = 0.006f; // Letalidade further boosts crit chance
    blueprint.critical.multiplier = 1.3f; // Crits cut slightly deeper than normal blows
    blueprint.passiveBonuses.primary.destreza = 1; // Grants a small Dexterity bonus when wielded
    blueprint.passiveBonuses.secondary.letalidade = 2.0f; // Minor crit chance support for agile builds
    blueprint.inventorySprite.spritePath = "assets/img/weapons/Espada_Curta.png";
    blueprint.inventorySprite.drawSize = Vector2{18.0f, 64.0f};
    blueprint.inventorySprite.rotationDegrees = Presets::toLeft;
    return blueprint;
}

// Configura thrust da Machadinha (projétil Spear).
ProjectileBlueprint MakeMachadinhaProjectileBlueprint() {
    short length = 62;
    short thickness = 26;

    ProjectileBlueprint blueprint{}; // Projectile container for the hatchet thrust
    blueprint.kind = ProjectileKind::Spear; // Hatchet attacks with a quick lunge-style hitbox
    blueprint.common.damage = 14.0f; // Fallback damage for systems that skip weapon scaling
    blueprint.common.lifespanSeconds = 0.45f; // Time window for the thrust to extend and retract
    blueprint.common.projectilesPerShot = 1; // Single thrust per attack trigger
    blueprint.common.randomSpreadDegrees = 0.0f; // Thrust always follows the same angle
    blueprint.common.debugColor = Color{210, 190, 160, 255}; // Warm tone for debug visualization
    blueprint.common.spriteId = "machadinha_thrust"; // Placeholder sprite id for the thrust effect
    blueprint.common.perTargetHitCooldownSeconds = 0.60f; // Invulnerabilidade curta apos o golpe de machadinha
    blueprint.common.weaponSpritePath = "assets/img/weapons/Machadinha.png"; // Sprite renderizado a frente do jogador
    blueprint.common.displayLength = length;
    blueprint.common.displayThickness = thickness;

    blueprint.spear.length = length; // Effective damaging length of the hatchet thrust
    blueprint.spear.thickness = thickness; // Effective width of the hatchet shaft during the thrust
    blueprint.spear.reach = 56.0f; // Distance from the wielder's hands before the blade begins
    blueprint.spear.extendDuration = 0.22f; // Time spent extending forward
    blueprint.spear.idleTime = 0.05f; // Brief pause while the hatchet is fully extended
    blueprint.spear.retractDuration = 0.20f; // Time to pull the hatchet back
    blueprint.spear.followOwner = true; // Origin sticks with the wielder while lunging
    blueprint.spear.offset = Vector2{8.0f, -6.0f}; // Slight offset so the thrust lines up with the sprite hands

    return blueprint;
}

// Estatísticas gerais da machadinha, com foco em dano bruto.
WeaponBlueprint MakeMachadinhaWeaponBlueprint() {
    WeaponBlueprint blueprint{}; // Blueprint for the hatchet weapon behavior
    blueprint.name = "Machadinha"; // Display name shown to the player
    blueprint.projectile = MakeMachadinhaProjectileBlueprint(); // Uses the thrusting projectile above
    blueprint.cooldownSeconds = 0.75f; // Base delay between swings before cadence overrides
    blueprint.holdToFire = false; // Requires manual input per swing
    blueprint.attributeKey = WeaponAttributeKey::Strength; // Damage scales with raw Strength
    blueprint.damage.baseDamage = 16.0f; // High base damage for a heavy chopping weapon
    blueprint.damage.attributeScaling = 1.8f; // Strong Strength scaling to reward bruiser builds
    blueprint.cadence.baseAttacksPerSecond = 1.1f; // Slower than swords but faster than great weapons
    blueprint.cadence.dexterityGainPerPoint = 0.10f; // Dexterity lightly improves swing speed
    blueprint.cadence.attacksPerSecondCap = 2.6f; // Caps haste so the hatchet keeps its heft
    blueprint.critical.baseChance = 0.10f; // Hatchet has decent crit chance due to sharp edge
    blueprint.critical.chancePerLetalidade = 0.007f; // Crit chance increases modestly with Letalidade
    blueprint.critical.multiplier = 1.45f; // Crits deal juicy bursts when they land
    blueprint.passiveBonuses.primary.vigor = 2; // Adds vigor to support aggressive play
    blueprint.passiveBonuses.secondary.letalidade = 3.0f; // Provides extra Letalidade while equipped
    blueprint.inventorySprite.spritePath = "assets/img/weapons/Machadinha.png";
    blueprint.inventorySprite.drawSize = Vector2{16.0f, 64.0f};
    blueprint.inventorySprite.rotationDegrees = Presets::toLeft;
    return blueprint;
}

// Projétil que executa o giro completo da Espada Rúnica.
ProjectileBlueprint MakeEspadaRunicaProjectileBlueprint() {
    ProjectileBlueprint blueprint{}; // Projectile container for the rune blade spin
    blueprint.kind = ProjectileKind::FullCircleSwing; // Performs a 360-degree spinning attack
    blueprint.common.damage = 22.0f; // Fallback damage reflecting its legendary power
    blueprint.common.lifespanSeconds = 0.0f; // Duration computed from revolution speed instead of lifespan
    blueprint.common.projectilesPerShot = 1; // Single spinning hitbox per activation
    blueprint.common.randomSpreadDegrees = 0.0f; // Spin always follows the same trajectory
    blueprint.common.debugColor = Color{255, 200, 140, 255}; // Amber tone for debug visuals
    blueprint.common.spriteId = "espada_runica_spin"; // Placeholder sprite for the rune spin effect
    blueprint.common.weaponSpritePath = "assets/img/weapons/Espada_Runica.png"; // Sprite renderizado a frente do jogador
    blueprint.common.displayMode = WeaponDisplayMode::AimAligned; // Garante que o sprite siga a direcao do giro
    blueprint.common.displayOffset = Vector2{1.0f, -4.0f}; // Posiciona a empunhadura no centro do jogador
    blueprint.common.displayLength = 130.0f;
    blueprint.common.displayThickness = 34.0f;
    blueprint.common.perTargetHitCooldownSeconds = 0.4f; // Permite multihits com janela controlada

    blueprint.fullCircle.length = 130.0f; // Spin reaches slightly further than a typical sword
    blueprint.fullCircle.thickness = 34.0f; // Wide arc to cover the entire spin radius
    blueprint.fullCircle.revolutions = 1.6f; // Spin travels a bit more than a full rotation
    blueprint.fullCircle.angularSpeedDegreesPerSecond = 480.0f; // Fast revolution to feel explosive
    blueprint.fullCircle.followOwner = true; // Spin continues tracking the player position

    return blueprint;
}

// Define comportamento lendário da Espada Rúnica.
WeaponBlueprint MakeEspadaRunicaWeaponBlueprint() {
    WeaponBlueprint blueprint{}; // Blueprint describing the legendary rune sword
    blueprint.name = "Espada Runica"; // UI-facing name of the weapon
    blueprint.projectile = MakeEspadaRunicaProjectileBlueprint(); // Uses the spinning projectile above
    blueprint.cooldownSeconds = 2.6f; // Base downtime before another rune spin is ready
    blueprint.holdToFire = false; // Requires deliberate activation
    blueprint.attributeKey = WeaponAttributeKey::Mysticism; // Rune blade scales with magical prowess
    blueprint.damage.baseDamage = 20.0f; // Substantial base damage even without attributes
    blueprint.damage.attributeScaling = 2.4f; // High scaling to reward invested Mysticism
    blueprint.cadence.baseAttacksPerSecond = 0.55f; // Slow attack rate befitting a heavy finisher
    blueprint.cadence.dexterityGainPerPoint = 0.06f; // Dexterity slightly trims the downtime
    blueprint.cadence.attacksPerSecondCap = 1.2f; // Prevents the spin from becoming spammy
    blueprint.critical.baseChance = 0.14f; // Elevated crit chance thanks to empowered runes
    blueprint.critical.chancePerLetalidade = 0.010f; // Letalidade heavily boosts crit potential
    blueprint.critical.multiplier = 1.65f; // Crit hits erupt with significant extra damage
    blueprint.passiveBonuses.primary.inteligencia = 3; // Equipping boosts Intelligence to fit the theme
    blueprint.passiveBonuses.secondary.letalidade = 6.0f; // Grants lethal expertise while wielded
    blueprint.inventorySprite.spritePath = "assets/img/weapons/Espada_Runica.png";
    blueprint.inventorySprite.drawSize = Vector2{26.0f, 64.0f};
    blueprint.inventorySprite.rotationDegrees = Presets::toLeft;
    return blueprint;
}

// Configura render e flecha disparada pelo Arco Simples.
ProjectileBlueprint MakeArcoSimplesProjectileBlueprint() {
    ProjectileBlueprint blueprint{}; // Container for standard arrow projectiles
    blueprint.kind = ProjectileKind::Ranged; // Bow itself is rendered via ranged display logic
    blueprint.common.projectilesPerShot = 1; // Fires a single arrow per attack
    blueprint.common.randomSpreadDegrees = 4.0f; // Small spread to mimic minor inaccuracy
    blueprint.common.weaponSpritePath = "assets/img/weapons/Arco_Simples.png"; // Sprite renderizado na mão do jogador
    blueprint.common.displayMode = WeaponDisplayMode::AimAligned; // Aligns bow sprite with aim direction
    blueprint.common.displayOffset = Vector2{1.0f, -4.0f}; // Alinha com o centro da hitbox do jogador
    blueprint.common.displayLength = 20.0f; // Visual length of the bow when rendered
    blueprint.common.displayThickness = 40.0f; // Visual thickness of the bow graphic
    blueprint.common.displayColor = Color{210, 190, 140, 255}; // Tint used when drawing the bow sprite
    blueprint.common.displayHoldSeconds = 0.35f; // Time the bow remains drawn after firing
    blueprint.common.debugColor = Color{255, 240, 180, 255}; // Reused for debug rendering if needed
    blueprint.common.spriteId = "arco_simples_arrow"; // Keeps reference for the projectile effect when needed

    blueprint.thrownSpawnForwardOffset = 34.0f; // Arrow leaves slightly ahead of the player's hands

    ThrownProjectileBlueprint arrow{};
    arrow.kind = ThrownProjectileKind::Ammunition;
    arrow.common.damage = 9.0f; // Baseline damage before weapon scaling
    arrow.common.lifespanSeconds = 1.6f; // Arrow despawns after travelling for this long
    arrow.common.debugColor = Color{255, 240, 180, 255}; // Pale tone for debug visualization
    arrow.common.spriteId = "arco_simples_arrow"; // Placeholder sprite reference for the arrow
    arrow.common.projectileSpritePath = "assets/img/projectiles/Arco_Simples_projetil.png"; // Caminho do sprite da flecha
    arrow.common.projectileForwardOffset = 12.0f; // Empurra a flecha um pouco para fora do arco ao spawnar
    arrow.ammunition.speed = 560.0f; // Travel speed of the arrow projectile
    arrow.ammunition.maxDistance = 860.0f; // Maximum travel distance before despawning
    arrow.ammunition.radius = 6.0f; // Collision radius for hitting enemies

    blueprint.thrownProjectiles.push_back(arrow);

    return blueprint;
}

// Descreve dano/cadência do Arco Simples, com foco em Destreza.
WeaponBlueprint MakeArcoSimplesWeaponBlueprint() {
    WeaponBlueprint blueprint{}; // Blueprint for the basic bow weapon
    blueprint.name = "Arco Simples"; // UI label for the bow
    blueprint.projectile = MakeArcoSimplesProjectileBlueprint(); // Uses the arrow projectile above
    blueprint.cooldownSeconds = 0.35f; // Base delay between shots before cadence adjustments
    blueprint.holdToFire = false; // Player taps for each shot (no auto-fire)
    blueprint.attributeKey = WeaponAttributeKey::Focus; // Bow damage scales with Focus
    blueprint.damage.baseDamage = 8.5f; // Base arrow damage without Focus contributions
    blueprint.damage.attributeScaling = 1.2f; // Extra damage per point of Focus
    blueprint.cadence.baseAttacksPerSecond = 2.2f; // Naturally quick firing rate for a bow
    blueprint.cadence.dexterityGainPerPoint = 0.14f; // Dexterity strongly boosts firing speed
    blueprint.cadence.attacksPerSecondCap = 3.6f; // Max cadence to keep it balanced
    blueprint.critical.baseChance = 0.12f; // Bows are accurate, so they crit more often
    blueprint.critical.chancePerLetalidade = 0.008f; // Letalidade improves crit chance noticeably
    blueprint.critical.multiplier = 1.45f; // Critical arrows inflict high burst damage
    blueprint.passiveBonuses.primary.destreza = 2; // Grants Dexterity to complement ranged play
    blueprint.passiveBonuses.secondary.letalidade = 5.0f; // Additional Letalidade encourages crit builds
    blueprint.inventorySprite.spritePath = "assets/img/weapons/Arco_Simples.png";
    blueprint.inventorySprite.drawSize = Vector2{48.0f, 24.0f};
    blueprint.inventorySprite.rotationDegrees = -90.0f;

    return blueprint;
}

// Projétil de feixe contínuo do Cajado de Carvalho.
ProjectileBlueprint MakeCajadoDeCarvalhoProjectileBlueprint() {
    ProjectileBlueprint blueprint{}; // Projectile container for the druidic beam
    blueprint.kind = ProjectileKind::Ranged; // Staff is rendered via ranged weapon display
    blueprint.common.projectilesPerShot = 1; // Single beam per activation
    blueprint.common.randomSpreadDegrees = 0.0f; // Beam fires straight without wobble
    blueprint.common.debugColor = Color{160, 240, 255, 235}; // Cyan debug color to represent magic
    blueprint.common.spriteId = "cajado_de_carvalho_beam"; // Placeholder sprite identifier for the beam
    blueprint.common.displayMode = WeaponDisplayMode::AimAligned; // Aligns staff graphic with aim
    blueprint.common.displayOffset = Vector2{1.0f, -4.0f}; // Offset to rest the staff in the player's hands
    blueprint.common.displayLength = 70.0f; // Visual length of the staff model when drawn
    blueprint.common.displayThickness = 20.0f; // Visual thickness for rendering the staff
    blueprint.common.displayColor = Color{100, 200, 255, 220}; // Tint used during sprite rendering
    blueprint.common.displayHoldSeconds = 0.5f; // How long the staff stays lit after firing
    blueprint.common.weaponSpritePath = "assets/img/weapons/Cajado_de_Carvalho.png"; // Caminho do sprite da arma
    blueprint.common.perTargetHitCooldownSeconds = 0.08f; // Mantém o ritmo de dano por alvo

    ThrownProjectileBlueprint beam{};
    beam.kind = ThrownProjectileKind::Laser;
    beam.followOwner = true; // Beam should stay anchored to the player while active
    beam.common.damage = 6.0f; // Base tick damage before weapon scaling
    beam.common.lifespanSeconds = 0.3f; // Beam lasts briefly with each pulse
    beam.common.debugColor = Color{160, 240, 255, 235}; // Same tint for debug rendering
    beam.common.projectileSpritePath = "assets/img/projectiles/laser_body.png"; // Caminho do sprite do feixe
    beam.common.spriteId = "cajado_de_carvalho_beam";
    beam.common.perTargetHitCooldownSeconds = 0.08f; // Replicates old tick cooldown
    beam.laser.length = 540.0f; // Maximum reach of the beam in world units
    beam.laser.thickness = 12.0f; // Collision thickness of the beam
    beam.laser.duration = 0.22f; // Time the laser stays active per activation
    beam.laser.startOffset = 10.0f; // Spawn beam slightly ahead of the staff tip so art is visible
    beam.laser.fadeOutDuration = 0.16f; // Tempo usado pelo fade-out; ajuste para acelerar ou desacelerar o sumiço
    beam.laser.staffHoldExtraSeconds = 0.75f; // Beam lingers visually for a short while

    blueprint.thrownProjectiles.push_back(beam);

    return blueprint;
}

// Blueprint da arma mágica baseada em Conhecimento.
WeaponBlueprint MakeCajadoDeCarvalhoWeaponBlueprint() {
    WeaponBlueprint blueprint{}; // Blueprint defining the nature staff behaviour
    blueprint.name = "Cajado de Carvalho"; // UI-facing name
    blueprint.projectile = MakeCajadoDeCarvalhoProjectileBlueprint(); // Uses the beam projectile above
    blueprint.cooldownSeconds = 0.2f; // Base delay between beam pulses before cadence modifiers
    blueprint.holdToFire = true; // Each pulse needs a tap; hold-to-channel can be added later
    blueprint.usesSeparateProjectileSprite = true; // Arma e projetil possuem sprites distintos
    blueprint.attributeKey = WeaponAttributeKey::Knowledge; // Damage scales with Knowledge for casters
    blueprint.damage.baseDamage = 2.0f; // Base damage per beam tick
    blueprint.damage.attributeScaling = 1.9f; // Strong scaling to emphasize spellcasting stats
    blueprint.cadence.baseAttacksPerSecond = 1.6f; // Moderate fire rate suitable for a staff
    blueprint.cadence.dexterityGainPerPoint = 0.08f; // Dexterity slightly speeds up pulses
    blueprint.cadence.attacksPerSecondCap = 3.0f; // Caps cadence to keep beams manageable
    blueprint.critical.baseChance = 0.10f; // Magical beams can crit occasionally
    blueprint.critical.chancePerLetalidade = 0.009f; // Letalidade buffs crit chance meaningfully
    blueprint.critical.multiplier = 1.55f; // Crit beams spike damage nicely
    blueprint.passiveBonuses.primary.inteligencia = 2; // Grants Intelligence when equipped
    blueprint.passiveBonuses.attack.foco = 2; // Adds direct Focus to the attack attribute pool
    blueprint.passiveBonuses.secondary.vampirismo = 1.5f; // Beam lifesteal encourages aggressive casting
    blueprint.inventorySprite.spritePath = "assets/img/weapons/Cajado_de_Carvalho.png";
    blueprint.inventorySprite.drawSize = Vector2{16.0f, 64.0f};
    blueprint.inventorySprite.rotationDegrees = Presets::toLeft;
    return blueprint;
}

} // namespace

const WeaponBlueprint& GetBroquelWeaponBlueprint() {
    static WeaponBlueprint blueprint = MakeBroquelWeaponBlueprint();
    return blueprint;
}

const WeaponBlueprint& GetEspadaCurtaWeaponBlueprint() {
    static WeaponBlueprint blueprint = MakeEspadaCurtaWeaponBlueprint();
    return blueprint;
}

const WeaponBlueprint& GetMachadinhaWeaponBlueprint() {
    static WeaponBlueprint blueprint = MakeMachadinhaWeaponBlueprint();
    return blueprint;
}

const WeaponBlueprint& GetEspadaRunicaWeaponBlueprint() {
    static WeaponBlueprint blueprint = MakeEspadaRunicaWeaponBlueprint();
    return blueprint;
}

const WeaponBlueprint& GetArcoSimplesWeaponBlueprint() {
    static WeaponBlueprint blueprint = MakeArcoSimplesWeaponBlueprint();
    return blueprint;
}

const WeaponBlueprint& GetCajadoDeCarvalhoWeaponBlueprint() {
    static WeaponBlueprint blueprint = MakeCajadoDeCarvalhoWeaponBlueprint();
    return blueprint;
}
