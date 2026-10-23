# Endless Rooms

**Endless Rooms** é um jogo de ação e exploração em 2D. Avance por salas geradas, enfrente inimigos e procure recursos para se preparar para os próximos desafios.

Durante a exploração, você pode encontrar armas, itens e locais especiais como lojas, forjas e baús. O projeto está em desenvolvimento e foi criado em C++17 com raylib 5.5.

## Comece a jogar

Consulte [INSTRUCTION.md](INSTRUCTION.md) para instalar as ferramentas necessárias, compilar, executar e testar o jogo.

### Selecionar bioma para testes

Para forçar o bioma das salas geradas em uma run, crie `game/biome.txt` com `cave`, `mansion` ou `dungeon`. O jogo lê esse arquivo ao iniciar, usando `game/` como diretório de trabalho. O lobby não muda. Se o arquivo estiver ausente ou vazio, o bioma continua sendo sorteado; um valor inválido também usa o sorteio e exibe um aviso.

## Sobre o projeto

- Código do jogo e assets: `game/`
- Organização dos módulos: [docs/architecture.md](docs/architecture.md)
- Orientações para agentes de desenvolvimento: [AGENTS.md](AGENTS.md)
- Convenção dos sprites de chão: [game/assets/floor/README.md](game/assets/floor/README.md)
