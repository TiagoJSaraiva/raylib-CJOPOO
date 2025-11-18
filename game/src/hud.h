#pragma once

struct PlayerCharacter;
struct InventoryUIState;

// Recebe o jogador e o estado do inventario para desenhar os elementos de HUD correspondentes na tela.
void DrawHUD(const PlayerCharacter& player, const InventoryUIState& inventoryState);
