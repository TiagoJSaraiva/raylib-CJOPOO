#include "hud.h"

#include "font_manager.h"
#include "player.h"
#include "raylib.h"
#include "ui_inventory.h"
#include "weapon.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace {

constexpr float kHealthBarWidth = 256.0f;
constexpr float kHealthBarHeight = 64.0f;
constexpr float kHealthBarLeftPadding = 32.0f;
constexpr float kHealthBarBottomPadding = 32.0f;
constexpr float kHealthBarFontSize = 30.0f;
constexpr float kHealthBarTextSpacing = 0.0f;
constexpr Color kFilledColor{220, 20, 60, 150}; // traditional red with transparency
constexpr Color kEmptyColor{255, 140, 140, 150}; // lighter red background
constexpr Color kHealthTextColor{0, 0, 0, 255};

constexpr float kSlotSize = 64.0f;
constexpr float kSlotSpacing = 12.0f;
constexpr int kEquipmentSlotCount = 5;
constexpr int kWeaponSlotCount = 2;
constexpr float kEquipmentBottomPadding = 32.0f;
constexpr float kEquipmentRightPadding = 32.0f;
constexpr float kWeaponGroupGap = 32.0f;
constexpr float kEquipmentLabelRightOffset = 306.0f;
constexpr float kEquipmentLabelBottomOffset = 10.0f;
constexpr float kEquipmentLabelFontSize = 14.0f;
constexpr float kWeaponLabelFontSize = 14.0f;
constexpr float kWeaponLabelVerticalGap = 8.0f;
constexpr Vector2 kWeaponLabelOffset{-45.0f, 0.0f};
constexpr Color kSlotBackgroundColor{54, 58, 72, 220};
constexpr Color kEmptySlotBorder{70, 80, 100, 255};
constexpr Color kHudLabelColor{0, 0, 0, 255};
constexpr Color kHudLabelOutlineColor{255, 255, 255, 255};
constexpr float kHudLabelOutlineThickness = 1.0f;
constexpr float kSlotSpritePadding = 0.0f;

struct HudSpriteCacheEntry {
    Texture2D texture{};
    bool attemptedLoad{false};
};

std::unordered_map<std::string, HudSpriteCacheEntry> g_hudSpriteCache{};

float ResolveBarYPosition() {
    return static_cast<float>(GetScreenHeight()) - kHealthBarBottomPadding - kHealthBarHeight;
}

Texture2D LoadHudTexture(const std::string& path) {
    if (path.empty()) {
        return Texture2D{};
    }
    if (!FileExists(path.c_str())) {
        return Texture2D{};
    }
    Texture2D texture = LoadTexture(path.c_str());
    if (texture.id != 0) {
        SetTextureFilter(texture, TEXTURE_FILTER_POINT);
    }
    return texture;
}

Texture2D AcquireHudTexture(const std::string& path) {
    if (path.empty()) {
        return Texture2D{};
    }
    auto& entry = g_hudSpriteCache[path];
    if (!entry.attemptedLoad) {
        entry.attemptedLoad = true;
        entry.texture = LoadHudTexture(path);
    }
    return entry.texture;
}

const ItemDefinition* FindHudItemDefinition(const InventoryUIState& state, int itemId) {
    if (itemId <= 0) {
        return nullptr;
    }
    for (const ItemDefinition& def : state.items) {
        if (def.id == itemId) {
            return &def;
        }
    }
    return nullptr;
}

Color RarityToColor(int rarity) {
    switch (rarity) {
        case 1:
            return Color{160, 160, 160, 255};
        case 2:
            return Color{90, 180, 110, 255};
        case 3:
            return Color{80, 140, 225, 255};
        case 4:
            return Color{170, 90, 210, 255};
        case 5:
            return Color{240, 200, 70, 255};
        case 6:
            return Color{150, 30, 70, 255};
        default:
            return Color{110, 120, 140, 255};
    }
}

Color ResolveSlotBorderColor(const InventoryUIState& state, int itemId) {
    if (itemId <= 0) {
        return kEmptySlotBorder;
    }
    const ItemDefinition* def = FindHudItemDefinition(state, itemId);
    if (def == nullptr || def->rarity <= 0) {
        return kEmptySlotBorder;
    }
    return RarityToColor(def->rarity);
}

bool DrawHudWeaponSprite(const WeaponBlueprint& blueprint, const Rectangle& rect) {
    const WeaponInventorySprite& sprite = blueprint.inventorySprite;
    if (sprite.spritePath.empty()) {
        return false;
    }
    Texture2D texture = AcquireHudTexture(sprite.spritePath);
    if (texture.id == 0) {
        return false;
    }

    Vector2 size = sprite.drawSize;
    if (size.x <= 0.0f) {
        size.x = static_cast<float>(texture.width);
    }
    if (size.y <= 0.0f) {
        size.y = static_cast<float>(texture.height);
    }

    Rectangle src{0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height)};
    Vector2 center{
        rect.x + rect.width * 0.5f + sprite.drawOffset.x,
        rect.y + rect.height * 0.5f + sprite.drawOffset.y
    };
    Rectangle dest{center.x, center.y, size.x, size.y};
    Vector2 origin{size.x * 0.5f, size.y * 0.5f};
    DrawTexturePro(texture, src, dest, origin, sprite.rotationDegrees, WHITE);
    return true;
}

bool DrawHudItemSprite(const ItemDefinition& def, const Rectangle& rect) {
    if (def.inventorySpritePath.empty()) {
        return false;
    }
    Texture2D texture = AcquireHudTexture(def.inventorySpritePath);
    if (texture.id == 0) {
        return false;
    }

    Vector2 drawSize = def.inventorySpriteDrawSize;
    if (drawSize.x <= 0.0f || drawSize.y <= 0.0f) {
        float maxDim = static_cast<float>(std::max(texture.width, texture.height));
        float targetDim = std::max(0.0f, std::min(rect.width, rect.height) - kSlotSpritePadding);
        float scale = (maxDim > 0.0f) ? std::min(1.0f, targetDim / maxDim) : 1.0f;
        drawSize = Vector2{static_cast<float>(texture.width) * scale, static_cast<float>(texture.height) * scale};
    }

    Rectangle src{0.0f, 0.0f, static_cast<float>(texture.width), static_cast<float>(texture.height)};
    Vector2 center{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
    Rectangle dest{center.x, center.y, drawSize.x, drawSize.y};
    Vector2 origin{dest.width * 0.5f, dest.height * 0.5f};
    DrawTexturePro(texture, src, dest, origin, 0.0f, WHITE);
    return true;
}

bool DrawHudIcon(const InventoryUIState& state, const Rectangle& rect, int itemId) {
    if (itemId <= 0) {
        return false;
    }
    if (const WeaponBlueprint* blueprint = ResolveWeaponBlueprint(state, itemId)) {
        return DrawHudWeaponSprite(*blueprint, rect);
    }
    const ItemDefinition* def = FindHudItemDefinition(state, itemId);
    if (def == nullptr) {
        return false;
    }
    return DrawHudItemSprite(*def, rect);
}

void DrawHudSlotLabel(const std::string& label, const Rectangle& rect) {
    if (label.empty()) {
        return;
    }
    const float fontSize = 16.0f;
    const Font& font = GetGameFont();
    Vector2 textSize = MeasureTextEx(font, label.c_str(), fontSize, 0.0f);
    Vector2 pos{
        rect.x + (rect.width - textSize.x) * 0.5f,
        rect.y + (rect.height - textSize.y) * 0.5f
    };
    DrawTextEx(font, label.c_str(), pos, fontSize, 0.0f, kHudLabelColor);
}

void DrawHudSlot(const InventoryUIState& state,
                 const Rectangle& rect,
                 int itemId,
                 const std::string& label) {
    DrawRectangleRec(rect, kSlotBackgroundColor);
    DrawRectangleLinesEx(rect, 2.0f, ResolveSlotBorderColor(state, itemId));
    if (!DrawHudIcon(state, rect, itemId)) {
        DrawHudSlotLabel(label, rect);
    }
}

void DrawTextWithOutline(const std::string& text,
                         Vector2 position,
                         float fontSize,
                         float spacing,
                         Color fillColor,
                         Color outlineColor) {
    const Font& font = GetGameFont();
    const Vector2 offsets[] = {
        {-kHudLabelOutlineThickness, 0.0f},
        {kHudLabelOutlineThickness, 0.0f},
        {0.0f, -kHudLabelOutlineThickness},
        {0.0f, kHudLabelOutlineThickness}
    };
    for (const Vector2& offset : offsets) {
        Vector2 outlinePos{position.x + offset.x, position.y + offset.y};
        DrawTextEx(font, text.c_str(), outlinePos, fontSize, spacing, outlineColor);
    }
    DrawTextEx(font, text.c_str(), position, fontSize, spacing, fillColor);
}

float EquipmentRowStartX(float screenWidth) {
    const float rightmostSlotX = screenWidth - kEquipmentRightPadding - kSlotSize;
    return rightmostSlotX - (kSlotSize + kSlotSpacing) * static_cast<float>(kEquipmentSlotCount - 1);
}

float SlotRowY(float screenHeight) {
    return screenHeight - kEquipmentBottomPadding - kSlotSize;
}

float WeaponRowStartX(float equipmentStartX) {
    float width = kSlotSize * kWeaponSlotCount + kSlotSpacing * static_cast<float>(kWeaponSlotCount - 1);
    float target = equipmentStartX - kWeaponGroupGap - width;
    return std::max(16.0f, target);
}

void DrawEquipmentRow(const InventoryUIState& state, float startX, float slotY) {
    for (int i = 0; i < kEquipmentSlotCount; ++i) {
        float x = startX + static_cast<float>(i) * (kSlotSize + kSlotSpacing);
        Rectangle rect{x, slotY, kSlotSize, kSlotSize};
        int itemId = (i < static_cast<int>(state.equipmentSlotIds.size())) ? state.equipmentSlotIds[i] : 0;
        std::string label = (i < static_cast<int>(state.equipmentSlots.size())) ? state.equipmentSlots[i] : std::string();
        DrawHudSlot(state, rect, itemId, label);
    }
}

void DrawWeaponRow(const InventoryUIState& state, float startX, float slotY) {
    for (int i = 0; i < kWeaponSlotCount; ++i) {
        float x = startX + static_cast<float>(i) * (kSlotSize + kSlotSpacing);
        Rectangle rect{x, slotY, kSlotSize, kSlotSize};
        int itemId = (i < static_cast<int>(state.weaponSlotIds.size())) ? state.weaponSlotIds[i] : 0;
        std::string label = (i < static_cast<int>(state.weaponSlots.size())) ? state.weaponSlots[i] : std::string();
        DrawHudSlot(state, rect, itemId, label);
    }
}

void DrawEquipmentLabel(float screenWidth, float screenHeight) {
    const std::string text = "equipamento";
    const Font& font = GetGameFont();
    Vector2 textSize = MeasureTextEx(font, text.c_str(), kEquipmentLabelFontSize, 0.0f);
    float x = std::max(0.0f, screenWidth - kEquipmentLabelRightOffset - textSize.x);
    float y = std::max(0.0f, screenHeight - kEquipmentLabelBottomOffset - textSize.y);
    DrawTextWithOutline(text, Vector2{x, y}, kEquipmentLabelFontSize, 0.0f, kHudLabelColor, kHudLabelOutlineColor);
}

void DrawWeaponLabel(float startX, float slotY) {
    const std::string text = "armas";
    const Font& font = GetGameFont();
    float rowWidth = kSlotSize * kWeaponSlotCount + kSlotSpacing * static_cast<float>(kWeaponSlotCount - 1);
    Vector2 textSize = MeasureTextEx(font, text.c_str(), kWeaponLabelFontSize, 0.0f);
    float x = startX + (rowWidth - textSize.x) * 0.5f + kWeaponLabelOffset.x;
    float y = slotY + kSlotSize + kWeaponLabelVerticalGap + kWeaponLabelOffset.y;
    DrawTextWithOutline(text, Vector2{x, y}, kWeaponLabelFontSize, 0.0f, kHudLabelColor, kHudLabelOutlineColor);
}

void DrawEquipmentAndWeapons(const InventoryUIState& state) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    const float slotY = SlotRowY(screenHeight);
    const float equipmentStartX = EquipmentRowStartX(screenWidth);
    DrawEquipmentRow(state, equipmentStartX, slotY);
    float weaponStartX = WeaponRowStartX(equipmentStartX);
    DrawWeaponRow(state, weaponStartX, slotY);
    DrawEquipmentLabel(screenWidth, screenHeight);
    DrawWeaponLabel(weaponStartX, slotY);
}

} // namespace

void DrawHUD(const PlayerCharacter& player, const InventoryUIState& inventoryState) {
    const float barX = kHealthBarLeftPadding;
    const float barY = ResolveBarYPosition();
    const float totalWidth = kHealthBarWidth;
    const float totalHeight = kHealthBarHeight;

    const float maxHealth = std::max(player.derivedStats.maxHealth, 1.0f);
    const float clampedHealth = std::clamp(player.currentHealth, 0.0f, maxHealth);
    const float hpPercent = std::clamp(clampedHealth / maxHealth, 0.0f, 1.0f);

    const float filledWidth = totalWidth * hpPercent;
    const float filledX = barX + (totalWidth - filledWidth);

    DrawRectangle(static_cast<int>(barX), static_cast<int>(barY), static_cast<int>(totalWidth), static_cast<int>(totalHeight), kEmptyColor);
    DrawRectangle(static_cast<int>(filledX), static_cast<int>(barY), static_cast<int>(filledWidth), static_cast<int>(totalHeight), kFilledColor);

    const int currentHpValue = static_cast<int>(std::round(clampedHealth));
    const int maxHpValue = static_cast<int>(std::round(maxHealth));
    std::string hpText = std::to_string(currentHpValue) + "/" + std::to_string(maxHpValue);

    const Font& font = GetGameFont();
    const Vector2 textSize = MeasureTextEx(font, hpText.c_str(), kHealthBarFontSize, kHealthBarTextSpacing);
    const Vector2 textPos{
        barX + (totalWidth * 0.5f) - (textSize.x * 0.5f),
        barY + (totalHeight * 0.5f) - (textSize.y * 0.5f)
    };
    DrawTextEx(font, hpText.c_str(), textPos, kHealthBarFontSize, kHealthBarTextSpacing, kHealthTextColor);

    DrawEquipmentAndWeapons(inventoryState);
}
