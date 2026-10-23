# Arquitetura atual

O jogo usa módulos C++ organizados por domínio. `main.cpp` compõe os sistemas e executa o loop do jogo; os headers em `game/src` definem os contratos entre as áreas.

## Módulos

| Área | Responsabilidade |
|---|---|
| Salas | `room` guarda o estado; `room_manager` coordena navegação e geração; `room_features` prepara forja, loja e baús; `room_renderer` desenha a cena das salas. |
| Jogador e combate | `player` guarda atributos; `enemy` e `enemy_spawner` cuidam dos inimigos; `projectile` coordena ciclo de vida e criação; `projectile_melee` e `projectile_ranged` implementam as famílias. |
| Inventário | `inventory_rules` contém políticas de categorias, pilhas e preços; `inventory_operations` manipula slots e armazenamento; `ui_inventory` liga essas operações aos controles e desenha as telas. |
| Interface e recursos | `hud` desenha informações de jogo; `font_manager` administra a fonte; `chest` define os tipos de armazenamento; `debug_console` implementa comandos de depuração. |
| Integração | `main.cpp` conecta os módulos e processa o ciclo do jogo; `station_persistence` sincroniza forjas e lojas entre UI e salas. |

## Props e luz

`prop` define o catálogo e o estado por instância; `prop_rules` isola geração,
colisão, navegação e dano ambiental. `prop_renderer` desenha sprites e compõe
iluminação; `combat_geometry` fornece interseções e trajetos para alvos
circulares e retangulares. As salas guardam os props durante toda a partida.

Consulte [Props e iluminação](props-and-lighting.md) para configuração,
contratos, testes e verificação manual.

## Reward pouches

`reward_pouch` declara pools e isola sorteios; `reward_pouch_operations` integra
mortes/destruição ao spawn e entrega recompensas com coleta parcial;
`reward_pouch_renderer` administra texturas e desenho. Cada sala guarda suas
bolsas e RNG de loot, preservados durante a run. Consulte
[Reward pouches](reward-pouches.md) para configuração, contratos e validação.

## Build, testes e execução

- O jogo usa C++17 e Raylib 5.5, vendorizada em `game/vendor/raylib`.
- `game/Makefile` compila a Raylib e todos os `.cpp` em `game/src/`.
- `mingw32-make test` executa testes sem framework externo para as regras isoladas do inventário.
- Os assets usam caminhos relativos a `assets/`; execute o binário com `game/` como diretório de trabalho.
- `game/inc/rlcpp` contém wrappers C++ de raylib e `game/inc/raygui.h` contém raygui.

## Direção de evolução

Os limites por domínio agora permitem extrair responsabilidades em etapas menores. Mantenha a diferença entre dados/regras, operações e apresentação ao ampliar inventário e combate. Faça uma extração por vez, preserve as APIs dos headers existentes e valide os fluxos afetados antes de alterar gameplay.
