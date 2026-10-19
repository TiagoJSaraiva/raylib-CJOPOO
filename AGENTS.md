# Instruções para agentes

## Projeto

Endless Rooms é um jogo 2D em C++17 com raylib 5.5. O código e os assets de runtime ficam em `game/`. Os caminhos dos assets são relativos ao diretório de trabalho; execute o jogo a partir de `game/`.

## Build, testes e execução

- Pré-requisitos: GNU Make (`mingw32-make` no Windows) e MinGW-w64 com `g++`, `gcc` e `ar` no `PATH`.
- Os comandos de build e teste devem ser executados dentro de `game/`: `mingw32-make game` e `mingw32-make test`.
- Execução: `game.exe` com `game/` como diretório atual.
- A Raylib 5.5 está copiada em `game/vendor/raylib` e é compilada junto com o jogo. Não dependa de uma cópia externa nem altere outras instalações da Raylib.
- `COMPILER_PATH` é opcional e adiciona ao `PATH` a pasta do toolchain quando ela não está configurada globalmente.
- `clean` deve remover apenas saídas de build do projeto e da Raylib vendorizada.

## Mapa do código

- `game/src/main.cpp`: inicialização, loop principal, integração de gameplay e desenho da cena.
- `room.cpp`, `room_manager.cpp`, `room_features.cpp`, `room_renderer.cpp`: estado, geração geométrica, recursos especiais e renderização das salas.
- `player.cpp`, `enemy*.cpp`, `projectile.cpp`, `projectile_melee.cpp`, `projectile_ranged.cpp`: personagem, inimigos e combate. `projectile_internal.h` contém contratos internos das famílias de projéteis.
- `ui_inventory.cpp`: composição e desenho das telas de inventário, loja, forja e baú.
- `inventory_rules.cpp`, `inventory_operations.cpp`: regras de item e operações de slots/armazenamento.
- `debug_console.cpp`, `station_persistence.cpp`: comandos de depuração e persistência de conteúdo de lojas/forjas ativas.
- `weapon.cpp`, `weapon_blueprints.cpp`, `chest.cpp`, `hud.cpp`, `font_manager.cpp`: armas, armazenamento, HUD e fonte.
- `game/assets/`: sprites e fonte usados em runtime.
- `game/vendor/raylib/`: código third-party; preservar os avisos de licença.

## Convenções de mudança

- Antes de editar, confira `git status` e preserve alterações locais fora do escopo.
- Mantenha os headers públicos como contratos entre módulos. Extrações internas devem preservar comportamento e APIs públicas.
- O Makefile inclui automaticamente os arquivos `.cpp` em `game/src/`.
- Ao mudar regra de gameplay, acrescente validação reproduzível quando a regra puder ser isolada; documente os fluxos que dependem de verificação manual.
- Não adicione objetos, executáveis ou bibliotecas geradas ao Git. `game/obj/` e as saídas de build de `game/vendor/raylib/src/` são artefatos locais.
- `anotacoes/` é pessoal e ignorada pelo Git; não a publique nem altere sem pedido explícito.

- Toda resposta do agente a prompts neste projeto deve incluir uma sugestão de mensagem de commit.
