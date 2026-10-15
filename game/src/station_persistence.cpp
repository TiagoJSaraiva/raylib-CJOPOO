#include "station_persistence.h"

#include "room_manager.h"
#include "room.h"

namespace {

ForgeInstance* ResolveTrackedForge(RoomManager& manager, const InventoryUIState& uiState) {
    if (!uiState.hasActiveForge) {
        return nullptr;
    }

    Room* trackedRoom = manager.TryGetRoom(uiState.activeForgeCoords);
    return trackedRoom == nullptr ? nullptr : trackedRoom->GetForge();
}

ShopInstance* ResolveTrackedShop(RoomManager& manager, const InventoryUIState& uiState) {
    if (!uiState.hasActiveShop) {
        return nullptr;
    }

    Room* trackedRoom = manager.TryGetRoom(uiState.activeShopCoords);
    return trackedRoom == nullptr ? nullptr : trackedRoom->GetShop();
}

} // namespace

void SaveActiveForgeContents(InventoryUIState& uiState, RoomManager& manager) {
    if (ForgeInstance* forge = ResolveTrackedForge(manager, uiState)) {
        StoreForgeContents(uiState, *forge);
    }
}

void SaveActiveShopContents(InventoryUIState& uiState, RoomManager& manager) {
    if (ShopInstance* shop = ResolveTrackedShop(manager, uiState)) {
        StoreShopContents(uiState, *shop);
    }
}

void SaveActiveStations(InventoryUIState& uiState, RoomManager& manager) {
    SaveActiveForgeContents(uiState, manager);
    SaveActiveShopContents(uiState, manager);
}
