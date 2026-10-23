#include "reward_pouch_renderer.h"

#include <algorithm>

Texture2D RewardPouchRenderer::Acquire(RewardPouchType type) {
    auto found = textures_.find(type);
    if (found != textures_.end()) return found->second;
    std::string path = "assets/img/reward-pouch/" + GetRewardPouchDefinition(type).id + ".png";
    Texture2D texture{};
    if (FileExists(path.c_str())) texture = LoadTexture(path.c_str());
    if (texture.id) SetTextureFilter(texture, TEXTURE_FILTER_BILINEAR);
    else TraceLog(LOG_WARNING, "[Reward pouches] Cannot load %s", path.c_str());
    textures_[type] = texture;
    return texture;
}

Rectangle RewardPouchRenderer::Destination(const RewardPouchInstance& pouch, int imageWidth, int imageHeight) {
    float width = GetRewardPouchDefinition(pouch.type).drawWidth;
    float height = imageWidth > 0 ? width * imageHeight / imageWidth : width;
    return {pouch.position.x - width * 0.5f, pouch.Depth() - height, width, height};
}

void RewardPouchRenderer::Draw(const RewardPouchInstance& pouch, float visibility) {
    if (visibility <= 0.0f || pouch.Empty()) return;
    visibility = std::clamp(visibility, 0.0f, 1.0f);
    Texture2D texture = Acquire(pouch.type);
    if (!texture.id) {
        DrawRectangleRec(pouch.Hitbox(), Fade(MAGENTA, visibility));
        return;
    }
    DrawTexturePro(texture, {0, 0, float(texture.width), float(texture.height)},
                   Destination(pouch, texture.width, texture.height), {0, 0}, 0, Fade(WHITE, visibility));
}

void RewardPouchRenderer::Unload() {
    for (const auto& entry : textures_) if (entry.second.id) UnloadTexture(entry.second);
    textures_.clear();
}
