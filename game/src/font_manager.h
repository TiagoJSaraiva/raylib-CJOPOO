#pragma once

#include "raylib.h"

#include <string>

// Recebe o caminho e o tamanho base, tenta carregar a fonte principal e substitui a atual (mantendo a padrao da Raylib se falhar).
void LoadGameFont(const std::string& path = "assets/font/alagard.ttf",
                  int baseSize = 32);

// Nao recebe parametros; libera a fonte customizada ativa e retorna para a fonte padrao da Raylib.
void UnloadGameFont();

// Nao recebe parametros; devolve por referencia a fonte atualmente em uso (customizada ou padrao da Raylib).
const Font& GetGameFont();
