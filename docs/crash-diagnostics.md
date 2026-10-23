# Crashes, donos de projéteis e cache de wall props

## Falha identificada

Três dumps do executável após a introdução das tochas registraram uma
violação de leitura `0xc0000005` no deslocamento `0x24974`. O endereço foi
identificado como `ThrownLaserProjectile::Update(float)`, na leitura de
`followTarget_`, com chamadas de `ProjectileSystem::Update()` e do loop
principal. Esse deslocamento corresponde àquele build; pode mudar em
compilações futuras.

O inimigo morto era removido da coleção antes da atualização dos disparos
inimigos. Um laser ainda ativo conservava um ponteiro para sua posição já
liberada. Os PNGs investigados da loja e da tocha estavam íntegros; a textura
da tocha é compartilhada em cache e a animação muda somente o recorte.

## Contrato de validade

`ProjectileSpawnContext::ownerLifetime` é um
`std::optional<std::weak_ptr<const void>>`. Donos com posição de vida dinâmica,
como inimigos, devem preenchê-lo junto com `followTarget`. Sem marcador, o
chamador garante a validade da posição até limpar seus ataques; o jogador
usa esse contrato com a posição estável do loop principal.

O inimigo mantém a única referência forte do marcador. A morte o invalida
imediatamente, mesmo antes de remover a instância; a destruição também o
invalida. Reinicializar cria outro marcador. Curar um inimigo vivo preserva
o marcador; revivê-lo cria um novo, sem reativar ataques antigos. O marcador
não possui o inimigo e não prolonga sua vida.

O sistema associa o marcador apenas a ataques que realmente seguem uma
posição, incluindo a exibição de arma. Antes de atualizar qualquer família,
remove os ataques com dono inválido. Desenho e todas as APIs de coleta de
dano também ignoram esses ataques, inclusive antes da próxima atualização.
Munições e ataques configurados para não seguir o dono mantêm sua duração.
Um pedido de disparo com marcador já expirado é ignorado antes de ler a
posição do dono. Esse contrato pressupõe o loop de gameplay de uma thread.

## Revisões de geometria e logs

`RoomManager` mantém uma revisão estrutural e a revisão usada pelo registro
de wall props. Criação de salas, configuração, alinhamento e selamento de
portas invalidam o cache. Ao terminar a preparação dos vizinhos, as revisões
iguais permitem reutilizar o registro, sem repetir geração e sorteios.
As alterações estruturais de runtime são realizadas pelo gerenciador.

Abertura/fade da porta, interação, revelação e oclusão da bottom não mudam
a revisão estrutural. A transição de sala prepara os vizinhos uma vez; o
loop não repete essa preparação logo após `MoveToNeighbor()`.

Builds com `BUILD_MODE=DEBUG` definem `ENDLESS_DEBUG_DIAGNOSTICS` e registram
uma mensagem `[DEBUG][WallProps]` por reconstrução, contendo seed, revisão,
quantidade de salas e props. Os avisos de textura existentes permanecem.
Para recompilar também os objetos de um build release anterior:

```powershell
# Executar dentro de game/
mingw32-make -B game BUILD_MODE=DEBUG
```

Enquanto o mapa estiver estruturalmente estável, não devem aparecer novas
mensagens de reconstrução, mesmo ao animar tochas e abrir portas.

## Validação reproduzível

Execute dentro de `game/`:

```powershell
mingw32-make game
mingw32-make test
mingw32-make test-prop-render test-wall-render test-wall-prop-render
```

`projectile_lifetime_tests` cobre as seis famílias que seguem uma posição,
movimento do dono vivo, expiração antes da coleta/desenho, reutilização do
endereço, munições e ataques independentes, compatibilidade do jogador e
disparos reais de `EnemyCommon`. O teste torna a página da posição inacessível
antes da atualização; uma leitura indevida falha de forma reproduzível.
Também executa a sequência de matar e destruir o inimigo antes de atualizar
seu laser, além de cura, reinicialização e destruição direta.

Os testes de geração verificam 120 preparações consecutivas sem substituir
o registro, preservação durante mudanças visuais de portas e atualização
após expansão estrutural do mapa, comparando com a geração completa.

Verificação manual em `game.exe`, com `game/` como diretório atual:

1. Combater inimigos com laser e matá-los enquanto o feixe ainda estiver
   ativo. O feixe preso e a exibição da arma devem terminar imediatamente;
   munições independentes já lançadas devem continuar.
2. Explorar, atravessar portas e retornar às salas. Conferir que posições
   e fases relativas das tochas permanecem estáveis e que novas regiões
   recebem props normalmente.
3. Observar lojas, animação das tochas e luz da bottom opaca. Aproximar e
   afastar o jogador da bottom e conferir sua translucidez.
4. Em build DEBUG, conferir que as reconstruções ocorrem quando novas
   regiões são preparadas, sem repetição contínua ao permanecer parado.
5. Reiniciar a partida e repetir o combate e as transições. Dumps antigos
   ficam em `%LOCALAPPDATA%\CrashDumps`; novos crashes devem ser analisados
   contra o executável correspondente.
