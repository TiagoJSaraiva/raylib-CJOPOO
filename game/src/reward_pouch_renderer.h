#pragma once

#include "reward_pouch.h"

#include <unordered_map>

class RewardPouchRenderer {
public:
    RewardPouchRenderer() = default;
    ~RewardPouchRenderer() { Unload(); }
    RewardPouchRenderer(const RewardPouchRenderer&) = delete;
    RewardPouchRenderer& operator=(const RewardPouchRenderer&) = delete;

    void Draw(const RewardPouchInstance& pouch, float visibility);
    static Rectangle Destination(const RewardPouchInstance& pouch, int imageWidth, int imageHeight);
    void Unload();
private:
    Texture2D Acquire(RewardPouchType type);
    std::unordered_map<RewardPouchType, Texture2D> textures_;
};
