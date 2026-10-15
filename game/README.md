# Build, testes e execução

## Pré-requisitos

- GNU Make (`mingw32-make` no Windows).
- MinGW-w64 com `g++`, `gcc` e `ar` disponíveis no `PATH`.
- Raylib 5.5 está em `vendor/raylib` e é compilada pelo Makefile do jogo.

O toolchain não é incluído no repositório. Se o MinGW estiver fora do `PATH`, passe `COMPILER_PATH` ao Make, por exemplo `mingw32-make game COMPILER_PATH=C:/caminho/do/mingw/bin`.

## Compilar e executar

Partindo da raiz do repositório, compile e execute com `game/` como diretório de trabalho, pois os caminhos dos assets são relativos a essa pasta:

```powershell
Set-Location game
mingw32-make game
./game.exe
```

## Testes

Dentro de `game/`, execute:

```powershell
mingw32-make test
```

Os testes atuais usam `assert` e não dependem de framework externo.

## Limpar saídas de build

Dentro de `game/`, execute `mingw32-make clean`. Isso remove os objetos do jogo, o executável e os objetos/biblioteca gerados dentro de `vendor/raylib/src`, além do executável de testes. DLLs e a Raylib compartilhada fora do repositório não são tocadas.
