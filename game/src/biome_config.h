#pragma once

#include <optional>
#include <string_view>

#include "room_types.h"

namespace BiomeConfig {

// Converte o nome de um bioma selecionável em uma configuração opcional.
std::optional<BiomeType> ParseBiomeOverride(std::string_view value);

// Lê biome.txt do diretório de trabalho do jogo. Arquivo ausente ou vazio mantém o sorteio.
std::optional<BiomeType> LoadBiomeOverride(const char* path = "biome.txt");

} // namespace BiomeConfig
