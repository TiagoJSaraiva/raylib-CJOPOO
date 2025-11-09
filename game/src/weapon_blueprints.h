#pragma once

#include "weapon.h"

namespace Presets { // Presets para os sprites do inventário
    constexpr float toLeft = -220.0f;
    constexpr float RotationShield1 = -6.0f;

    constexpr Vector2 SizeSword1{20.0f, 60.0f};
    constexpr Vector2 SizeShield1{58.0f, 58.0f};
    constexpr Vector2 SizeStaff1{16.0f, 60.0f};
}

const WeaponBlueprint& GetBroquelWeaponBlueprint();
const WeaponBlueprint& GetEspadaCurtaWeaponBlueprint();
const WeaponBlueprint& GetMachadinhaWeaponBlueprint();
const WeaponBlueprint& GetEspadaRunicaWeaponBlueprint();
const WeaponBlueprint& GetArcoSimplesWeaponBlueprint();
const WeaponBlueprint& GetCajadoDeCarvalhoWeaponBlueprint();
