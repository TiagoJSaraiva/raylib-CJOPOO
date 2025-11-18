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

constexpr float kHealthBarWidth = 256.0f; // Largura total da barra de vida no HUD
constexpr float kHealthBarHeight = 64.0f; // Altura da barra de vida
constexpr float kHealthBarLeftPadding = 32.0f; // Espaco do lado esquerdo antes de desenhar a barra
constexpr float kHealthBarBottomPadding = 32.0f; // Distancia da barra ate a base da tela
constexpr float kHealthBarFontSize = 30.0f; // Tamanho da fonte para o texto de HP
constexpr float kHealthBarTextSpacing = 0.0f; // Espacamento entre caracteres na barra de HP
constexpr Color kFilledColor{220, 20, 60, 150}; // Cor preenchida da parte com vida
constexpr Color kEmptyColor{255, 140, 140, 150}; // Cor da parte vazia da barra de vida
constexpr Color kHealthTextColor{0, 0, 0, 255}; // Cor do texto numérico da barra de HP

constexpr float kSlotSize = 64.0f; // Tamanho padrao de cada slot de equipamento/arma
constexpr float kSlotSpacing = 12.0f; // Espaco entre slots
constexpr int kEquipmentSlotCount = 5; // Quantidade de slots exibidos para equipamentos
constexpr int kWeaponSlotCount = 2; // Quantidade de slots para armas
constexpr float kEquipmentBottomPadding = 32.0f; // Distancia da fileira de equipamentos ate a base da tela
constexpr float kEquipmentRightPadding = 32.0f; // Distancia dos equipamentos ate a lateral direita
constexpr float kWeaponGroupGap = 32.0f; // Separacao horizontal entre equipamentos e armas
constexpr float kEquipmentLabelRightOffset = 306.0f; // Offset horizontal para posicionar o rotulo "equipamento"
constexpr float kEquipmentLabelBottomOffset = 10.0f; // Offset vertical para o rotulo de equipamentos
constexpr float kEquipmentLabelFontSize = 14.0f; // Tamanho da fonte do rotulo de equipamentos
constexpr float kWeaponLabelFontSize = 14.0f; // Tamanho da fonte do rotulo de armas
constexpr float kWeaponLabelVerticalGap = 8.0f; // Distancia vertical entre slots de armas e rotulo
constexpr Vector2 kWeaponLabelOffset{-45.0f, 0.0f}; // Ajuste adicional para posicionar o texto "armas"
constexpr Color kSlotBackgroundColor{54, 58, 72, 220}; // Cor de fundo dos slots
constexpr Color kEmptySlotBorder{70, 80, 100, 255}; // Cor padrão do contorno quando o slot esta vazio
constexpr Color kHudLabelColor{0, 0, 0, 255}; // Cor principal dos rotulos de HUD
constexpr Color kHudLabelOutlineColor{255, 255, 255, 255}; // Cor do contorno dos rotulos
constexpr float kHudLabelOutlineThickness = 1.0f; // Espessura usada para o contorno de texto
constexpr float kSlotSpritePadding = 0.0f; // Margem interna aplicada quando ajusta sprites aos slots

struct HudSpriteCacheEntry { // Entrada cacheando textura carregada para icones do HUD
    Texture2D texture{}; // Textura carregada da sprite
    bool attemptedLoad{false}; // Indica se ja tentamos carregar o arquivo correspondente
};

std::unordered_map<std::string, HudSpriteCacheEntry> g_hudSpriteCache{}; // Cache global de sprites usados pelo HUD

float ResolveBarYPosition() { // Nao recebe parametros; calcula a coordenada Y da barra de vida com base no padding configurado
    return static_cast<float>(GetScreenHeight()) - kHealthBarBottomPadding - kHealthBarHeight;
}

Texture2D LoadHudTexture(const std::string& path) { // Recebe um caminho e tenta carregar a textura correspondente, aplicando filtro adequado
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

Texture2D AcquireHudTexture(const std::string& path) { // Recebe caminho, verifica cache e garante que a textura esteja carregada antes de retornar
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

const ItemDefinition* FindHudItemDefinition(const InventoryUIState& state, int itemId) { // Recebe estado/informação do item e procura definition correspondente nos registros para uso no HUD
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

Color RarityToColor(int rarity) { // Recebe nivel de raridade e devolve a cor associada para contorno do slot
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

Color ResolveSlotBorderColor(const InventoryUIState& state, int itemId) { // Determina a cor do contorno do slot a partir do item equipado ou usa cor padrao
    if (itemId <= 0) {
        return kEmptySlotBorder;
    }
    const ItemDefinition* def = FindHudItemDefinition(state, itemId);
    if (def == nullptr || def->rarity <= 0) {
        return kEmptySlotBorder;
    }
    return RarityToColor(def->rarity);
}

bool DrawHudWeaponSprite(const WeaponBlueprint& blueprint, const Rectangle& rect) { // Recebe blueprint/retangulo e desenha sprite de arma no slot, retornando sucesso
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

bool DrawHudItemSprite(const ItemDefinition& def, const Rectangle& rect) { // Recebe definicao do item e retangulo alvo para desenhar sprite estatico no HUD
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

bool DrawHudIcon(const InventoryUIState& state, const Rectangle& rect, int itemId) { // Decide qual sprite usar para o item especificado e tenta desenha-lo; retorna sucesso
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

void DrawHudSlotLabel(const std::string& label, const Rectangle& rect) { // Recebe label e retangulo e desenha o texto centralizado no slot quando nao ha icone
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
                 const std::string& label) { // Desenha fundo/contorno do slot e tenta renderizar icone ou label de fallback
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
                         Color outlineColor) { // Desenha texto com contorno renderizando offsets e depois o preenchimento principal
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

float EquipmentRowStartX(float screenWidth) { // Recebe largura da tela e calcula X inicial dos slots de equipamento
    const float rightmostSlotX = screenWidth - kEquipmentRightPadding - kSlotSize;
    return rightmostSlotX - (kSlotSize + kSlotSpacing) * static_cast<float>(kEquipmentSlotCount - 1);
}

float SlotRowY(float screenHeight) { // Recebe altura da tela e devolve Y base da fileira de slots
    return screenHeight - kEquipmentBottomPadding - kSlotSize;
}

float WeaponRowStartX(float equipmentStartX) { // Calcula a posicao inicial dos slots de armas a partir do inicio dos equipamentos
    float width = kSlotSize * kWeaponSlotCount + kSlotSpacing * static_cast<float>(kWeaponSlotCount - 1);
    float target = equipmentStartX - kWeaponGroupGap - width;
    return std::max(16.0f, target);
}

void DrawEquipmentRow(const InventoryUIState& state, float startX, float slotY) { // Itera pelos slots de equipamento e desenha cada um com seu label/item
    for (int i = 0; i < kEquipmentSlotCount; ++i) {
        float x = startX + static_cast<float>(i) * (kSlotSize + kSlotSpacing);
        Rectangle rect{x, slotY, kSlotSize, kSlotSize};
        int itemId = (i < static_cast<int>(state.equipmentSlotIds.size())) ? state.equipmentSlotIds[i] : 0;
        std::string label = (i < static_cast<int>(state.equipmentSlots.size())) ? state.equipmentSlots[i] : std::string();
        DrawHudSlot(state, rect, itemId, label);
    }
}

void DrawWeaponRow(const InventoryUIState& state, float startX, float slotY) { // Similar ao anterior mas para os slots de armas
    for (int i = 0; i < kWeaponSlotCount; ++i) {
        float x = startX + static_cast<float>(i) * (kSlotSize + kSlotSpacing);
        Rectangle rect{x, slotY, kSlotSize, kSlotSize};
        int itemId = (i < static_cast<int>(state.weaponSlotIds.size())) ? state.weaponSlotIds[i] : 0;
        std::string label = (i < static_cast<int>(state.weaponSlots.size())) ? state.weaponSlots[i] : std::string();
        DrawHudSlot(state, rect, itemId, label);
    }
}

void DrawEquipmentLabel(float screenWidth, float screenHeight) { // Recebe dimensoes da tela e desenha o rotulo "equipamento" acima da fileira
    const std::string text = "equipamento";
    const Font& font = GetGameFont();
    Vector2 textSize = MeasureTextEx(font, text.c_str(), kEquipmentLabelFontSize, 0.0f);
    float x = std::max(0.0f, screenWidth - kEquipmentLabelRightOffset - textSize.x);
    float y = std::max(0.0f, screenHeight - kEquipmentLabelBottomOffset - textSize.y);
    DrawTextWithOutline(text, Vector2{x, y}, kEquipmentLabelFontSize, 0.0f, kHudLabelColor, kHudLabelOutlineColor);
}

void DrawWeaponLabel(float startX, float slotY) { // Desenha o rotulo "armas" alinhado aos slots correspondentes
    const std::string text = "armas";
    const Font& font = GetGameFont();
    float rowWidth = kSlotSize * kWeaponSlotCount + kSlotSpacing * static_cast<float>(kWeaponSlotCount - 1);
    Vector2 textSize = MeasureTextEx(font, text.c_str(), kWeaponLabelFontSize, 0.0f);
    float x = startX + (rowWidth - textSize.x) * 0.5f + kWeaponLabelOffset.x;
    float y = slotY + kSlotSize + kWeaponLabelVerticalGap + kWeaponLabelOffset.y;
    DrawTextWithOutline(text, Vector2{x, y}, kWeaponLabelFontSize, 0.0f, kHudLabelColor, kHudLabelOutlineColor);
}

void DrawEquipmentAndWeapons(const InventoryUIState& state) { // Compoe a secao completa de slots/rotulos usando as funcoes acima
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

void DrawHUD(const PlayerCharacter& player, const InventoryUIState& inventoryState) { // Recebe o jogador e o estado de inventario, calcula barra de HP e desenha slots + textos no HUD, sem retorno
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
