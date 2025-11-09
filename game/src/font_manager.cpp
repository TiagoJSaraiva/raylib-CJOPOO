#include "font_manager.h"

#include "raygui.h"

#include <iostream>

namespace {
Font g_gameFont = GetFontDefault();
bool g_fontOwned = false;
}

void LoadGameFont(const std::string& path, int baseSize) {
    if (g_fontOwned && g_gameFont.texture.id != 0) {
        UnloadFont(g_gameFont);
        g_gameFont = GetFontDefault();
        g_fontOwned = false;
    }

    if (!path.empty() && FileExists(path.c_str())) {
        Font loaded = LoadFontEx(path.c_str(), baseSize, nullptr, 0);
        if (loaded.texture.id != 0) {
            SetTextureFilter(loaded.texture, TEXTURE_FILTER_POINT);
            g_gameFont = loaded;
            g_fontOwned = true;
            GuiSetFont(g_gameFont);
            return;
        }

        if (loaded.texture.id != 0) {
            UnloadFont(loaded);
        }
        std::cerr << "[Font] Falha ao carregar fonte: " << path << std::endl;
    } else if (!path.empty()) {
        std::cerr << "[Font] Arquivo de fonte nao encontrado: " << path << std::endl;
    }

    g_gameFont = GetFontDefault();
    g_fontOwned = false;
    GuiSetFont(g_gameFont);
}

void UnloadGameFont() {
    if (g_fontOwned && g_gameFont.texture.id != 0) {
        UnloadFont(g_gameFont);
        g_gameFont = GetFontDefault();
        g_fontOwned = false;
    }

    GuiSetFont(g_gameFont);
}

const Font& GetGameFont() {
    return g_gameFont;
}
