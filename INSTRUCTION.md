# Instruções de build e execução

## Pré-requisitos

- GNU Make; no Windows, use `mingw32-make`.
- MinGW-w64 com `g++`, `gcc` e `ar` disponíveis no `PATH`.
- A Raylib 5.5 já está incluída em `game/vendor/raylib` e é compilada junto com o jogo.

O compilador e o Make não são incluídos no repositório. Se o MinGW não estiver no `PATH`, informe a pasta do toolchain com `COMPILER_PATH`, por exemplo:

```powershell
mingw32-make game COMPILER_PATH=C:/caminho/do/mingw/bin
```

## Compilar e executar

Execute os comandos a partir da raiz do repositório. O jogo deve ser iniciado com `game/` como diretório atual porque os caminhos dos assets são relativos a essa pasta.

```powershell
Set-Location game
mingw32-make game
.\game.exe
```

## Testes

Dentro de `game/`, rode:

```powershell
mingw32-make test
```

Os testes atuais não precisam de framework externo.

## Limpar arquivos gerados

Dentro de `game/`, rode:

```powershell
mingw32-make clean
```

O comando remove os objetos e executáveis gerados pelo projeto, além dos objetos e bibliotecas compilados dentro da Raylib vendorizada. Ele preserva os assets e as DLLs do projeto.

## Mais informações

- [Arquitetura e módulos](docs/architecture.md)
- [Orientações para agentes](AGENTS.md)
