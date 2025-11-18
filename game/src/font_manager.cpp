#include "font_manager.h"

#include "raygui.h"

#include <iostream>

namespace {
Font g_gameFont = GetFontDefault(); // Fonte atualmente usada pelo jogo (pode ser padrao ou customizada)
bool g_fontOwned = false; // Indica se a fonte carregada foi alocada pelo jogo e precisa ser liberada
}

void LoadGameFont(const std::string& path, int baseSize) { // Recebe caminho e tamanho base, recarrega a fonte e atualiza o estado global retornando automaticamente
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

void UnloadGameFont() { // Sem parametros; libera fonte customizada se houver e aplica fonte padrao novamente
    if (g_fontOwned && g_gameFont.texture.id != 0) {
        UnloadFont(g_gameFont);
        g_gameFont = GetFontDefault();
        g_fontOwned = false;
    }

    GuiSetFont(g_gameFont);
}

const Font& GetGameFont() { // Sem parametros; retorna por referencia a fonte hoje ativa
    return g_gameFont;
}
