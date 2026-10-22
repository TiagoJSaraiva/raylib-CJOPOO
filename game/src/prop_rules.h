#pragma once
#include "prop.h"

class Room;
namespace PropRules {
bool Overlaps(Rectangle a, Rectangle b);
Rectangle EntityBounds(Vector2 position, float halfWidth, float halfHeight);
void Initialize(Room& room, const std::vector<PropDefinition>& catalog = GetPropDefinitions());
bool IsFree(const Room& room, Rectangle bounds);
Vector2 ResolveMovement(const Room& room, Vector2 from, Vector2 desired, float halfWidth, float halfHeight);
Vector2 NavigationTarget(const Room& room, Vector2 from, Vector2 target, float halfWidth, float halfHeight);
float ContactDamage(const Room& room, Rectangle attemptedBounds, Rectangle resolvedBounds, float seconds);
// All remaining floor tiles must form one connected component.
bool FloorConnected(const Room& room, const TileRect* candidate = nullptr);
bool PlacementAllowed(const Room& room, TileRect candidate);
}
