#pragma once

#include "raylib.h"

#include <string>

// Loads the main game font from the specified path. If loading fails, the
// default Raylib font remains in use. Calling this more than once reloads the
// font and frees the previous one.
void LoadGameFont(const std::string& path = "assets/font/alagard.ttf",
                  int baseSize = 32);

// Releases the currently loaded custom font (if any) and reverts back to the
// Raylib default font.
void UnloadGameFont();

// Returns a reference to the font currently used across the UI and gameplay.
// Guaranteed to always return a valid font (either the custom font or Raylib's
// default font).
const Font& GetGameFont();
