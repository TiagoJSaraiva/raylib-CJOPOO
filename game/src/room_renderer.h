#pragma once

#include "room.h"

class RoomRenderer {
public:
    void DrawRoom(const Room& room, bool isActive) const;
};
