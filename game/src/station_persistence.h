#pragma once

#include "ui_inventory.h"

class RoomManager;

// Copies the active room's forge and shop contents back into the world state.
void SaveActiveForgeContents(InventoryUIState& uiState, RoomManager& manager);
void SaveActiveShopContents(InventoryUIState& uiState, RoomManager& manager);
void SaveActiveStations(InventoryUIState& uiState, RoomManager& manager);
