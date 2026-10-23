#pragma once

#include "prop_renderer.h"
#include "wall_prop.h"

namespace WallPropRendering {
void Draw(const WallPropView& view, PropRenderer& renderer, float elapsedSeconds);
std::vector<SceneLightSource> Lights(const std::vector<WallPropView>& views, PropRenderer& renderer,
                                    float elapsedSeconds);
}
