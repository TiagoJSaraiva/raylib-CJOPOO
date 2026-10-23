#include "wall_prop_renderer.h"

namespace WallPropRendering {
void Draw(const WallPropView& view, PropRenderer& renderer, float elapsedSeconds) {
    const auto& prop = *view.prop;
    const auto& definition = *prop.definition;
    renderer.DrawSprite(definition.texturePath, prop.Bounds(), definition.animation, view.visibility,
                        elapsedSeconds + prop.animationPhase);
}

std::vector<SceneLightSource> Lights(const std::vector<WallPropView>& views, PropRenderer& renderer,
                                    float elapsedSeconds) {
    std::vector<SceneLightSource> result;
    for (const auto& view : views) {
        const auto& prop = *view.prop;
        const auto& definition = *prop.definition;
        if (!definition.light || view.visibility <= 0) continue;
        const Vector2 position = renderer.SpriteLightPosition(definition.texturePath, prop.Bounds(), definition.animation,
                                                              *definition.light, elapsedSeconds + prop.animationPhase);
        result.push_back({position, *definition.light, prop.LightClip(), view.visibility});
    }
    return result;
}
}
