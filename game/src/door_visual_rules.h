#pragma once

#include "room.h"

#include <algorithm>

namespace DoorVisualRules {

constexpr float FadeDuration = 1.0f;

inline float OpenProgress(const DoorInstance* state) {
    if (state == nullptr) return 0.0f;
    if (state->open) return 1.0f;
    if (!state->opening) return 0.0f;
    return std::clamp(state->fadeProgress / FadeDuration, 0.0f, 1.0f);
}

inline float MaskAlpha(const DoorInstance& state) {
    return 1.0f - OpenProgress(&state);
}

} // namespace DoorVisualRules
