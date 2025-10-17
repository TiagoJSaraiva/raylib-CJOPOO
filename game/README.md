# Protótipo do Jogo (raylib-CJOPOO)

## Descrição do Jogo

Este é um protótipo de jogo top-down rogue-lite desenvolvido em C++ usando a biblioteca raylib. O jogador controla um personagem (o "Cavaleiro") em um ambiente gerado proceduralmente, composto por salas conectadas por portas e corredores. O foco principal é o combate: o jogador usa armas equipadas nas mãos esquerda e direita (como broquel e espada longa) para disparar projéteis variados (cortes, investidas, balas, lasers) contra inimigos.

O jogo inclui:
- Sistema de atributos (poder, defesa, vigor, etc.) que escalam dano, cadência e crítico das armas.
- Geração procedural de salas e biomas (caverna, mansão, dungeon), com portas que se abrem ao limpar inimigos.
- Mecânicas de loot, economia e forja para customizar equipamentos.
- Estado de jogo com lobby inicial, salas de combate e possíveis salas especiais (loja, forja, baú).

O objetivo é limpar salas eliminando inimigos para progredir, coletar itens e sobreviver a biomas cada vez mais desafiadores. Detalhes completos de mecânicas, balanceamento e design estão na pasta `anotacoes/` do repositório.

## Como Contribuir com o Código

Este projeto é estruturado para facilitar colaboração. Abaixo, um resumo do que cada arquivo em `src/` faz, para que você saiba onde mexer ao implementar novas funcionalidades ou corrigir bugs. Recomenda-se ler as anotações em `anotacoes/` antes de alterações profundas.

### Arquivos Principais

- **`main.cpp`**: Ponto de entrada do jogo. Contém o loop principal (`while (!WindowShouldClose())`), inicialização da câmera, mundo (salas), armas e projéteis. Gerencia entrada do jogador (mouse para disparar armas esquerda/direita), atualização de estado e renderização básica. Aqui você adiciona novos sistemas ou modifica o fluxo de jogo.

- **`player.h` / `player.cpp`**: Define a classe `PlayerCharacter`, que armazena atributos base, bônus de armas e calcula estatísticas derivadas (vida, velocidade, etc.). Use para adicionar novos atributos ou modificar como o jogador escala com equipamentos.

- **`weapon.h`**: Contém blueprints de armas (`WeaponBlueprint`), parâmetros de dano/cadência/crítico e o estado runtime (`WeaponState`). Responsável por aplicar atributos do jogador aos projéteis. Modifique aqui para novas armas ou mecânicas de combate.

- **`projectile.h` / `projectile.cpp`**: Implementa tipos de projéteis (blunt, swing, spear, ammunition, laser) e o `ProjectileSystem` que os gerencia (spawn, update, draw). Adicione novos tipos de ataque ou projéteis aqui.

- **`room.h` / `room.cpp`**: Modela uma sala individual (`Room`), com layout em tiles, portas e metadados (bioma, tipo). Use para modificar estrutura de salas ou adicionar propriedades.

- **`room_manager.h` / `room_manager.cpp`**: Gerencia o grafo de salas, geração procedural, navegação entre portas e seed global para reprodutibilidade. Modifique para alterar lógica de geração de mapas ou biomas.

- **`room_renderer.h` / `room_renderer.cpp`**: Responsável por desenhar salas (chão, paredes, portas) com cores por bioma. Use para ajustes visuais ou novos elementos de render.

- **`room_types.h`**: Define enums e structs auxiliares (direções, coordenadas, tipos de sala/bioma). Adicione novos tipos aqui se expandir o mundo.

### Diretrizes Gerais para Contribuição

- **Compilação**: Use `mingw32-make` no Windows (MinGW). O projeto usa C++17 e raylib 5.x.
- **Estrutura**: Mantenha lógica separada (ex.: combate em `weapon.h`, geração em `room_manager.cpp`).
- **Testes**: Após mudanças, compile e teste o jogo. Use o boneco de treino no lobby para validar dano/crítico.
- **Documentação**: Adicione comentários em português nos arquivos fonte explicando funções e blocos. Consulte `anotacoes/` para design.
- **Pull Requests**: Descreva mudanças, teste em diferentes salas e mencione se afeta balanceamento.

Para dúvidas sobre mecânicas ou design, consulte os arquivos em `anotacoes/` (ex.: `02_mecanicas_e_sistemas.txt` para regras de jogo).