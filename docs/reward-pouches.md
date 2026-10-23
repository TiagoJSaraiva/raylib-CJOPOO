# Reward pouches

Inimigos derrotados não creditam mais moedas automaticamente. Uma morte ou a
destruição de um prop configurado pode deixar **uma única bolsa** na posição
do evento. O conteúdo inteiro é sorteado na criação, e não durante a coleta.

## Catálogos e contratos

`game/src/reward_pouch.cpp` declara os tipos e as pools. Cada definição contém
o identificador do PNG, intervalo inclusivo de moedas, entradas independentes
de itens, tamanho da hitbox e largura de desenho. IDs de materiais já existentes:
101 = tira de couro; 102 = lingote de bronze.

| Tipo / PNG | Moedas garantidas | Itens adicionais (sorteios independentes) |
|---|---:|---|
| `small` | 12–20 | 20% de 1 tira de couro |
| `medium` | 24–36 | 50% de 1–2 tiras de couro; 20% de 1 lingote de bronze |
| `big` | 48–72 | 100% de 2–3 tiras de couro; 50% de 1–2 lingotes de bronze |

As chances de drop ficam em `EnemyConfig::rewardPouchDrops`, declaradas por
tipo em `enemy_spawner.cpp`, e em `DestructibleProp::rewardPouchDrops`, no catálogo
de `prop.cpp`. São percentuais inteiros, absolutos, com soma máxima de 100;
o restante significa nenhum drop. Tabelas vazias ou inválidas não geram bolsas.
Uma única seleção cumulativa escolhe o tipo; os tipos não são sorteados de
forma independente.

| Origem | small | medium | big | Nada |
|---|---:|---:|---:|---:|
| Caverna, melee e ranged | 60% | 10% | 0% | 30% |
| Dungeon, melee e ranged | 50% | 20% | 5% | 25% |
| Mansão, melee e ranged | 40% | 30% | 10% | 20% |
| Vaso | 20% | 0% | 0% | 80% |

`RewardPouchOperations::DamageEnemy` e `DamageProp` são os pontos de integração
com o combate. Só tentam gerar uma bolsa quando `TakeDamage` informa a transição
para morto/destruído; impactos repetidos não repetem o sorteio. Ataques dos dois
lados podem destruir vasos. Os outros props continuam indestrutíveis.

## Estado e coleta

Cada `Room` guarda suas bolsas e um `mt19937_64` exclusivo, inicializado pela seed
da sala com um salt de domínio. O loot não consome os RNGs de geração do mundo,
spawn de inimigos ou combate. A mesma seed e sequência de eventos reproduzem
o loot no mesmo toolchain. A ordem dos eventos pode alterar os próximos drops.

A bolsa nasce no centro do inimigo ou da hitbox do prop, sem física ou colisão
de bloqueio. A hitbox inicial mede 24×16 px, centrada na posição do drop. Coleta
ocorre por sobreposição com o jogador, apenas na sala atual, depois de resolver
combate e morte do jogador, com jogador vivo, inventário fechado e input não
bloqueado pelo console. Sorte e alcance de coleta não alteram esse fluxo.

`InventoryOperations::AddItemToInventoryPartial` retorna a quantidade inserida;
preenche pilhas existentes e depois slots livres, mantendo os limites atuais.
A operação integral `AddItemToInventory` conserva seu contrato anterior.

Moedas e itens que couberem são entregues imediatamente. Quantidades restantes
ficam na mesma bolsa; moedas já entregues são zeradas. Contatos repetidos não
ressorteiam nem duplicam o conteúdo. Moedas que excederiam o limite do contador
também permanecem na bolsa. Os recibos de `Collect` contêm apenas o que foi
transferido, para feedback flutuante de moedas e nomes/quantidades dos itens.
Sem transferência não há novo feedback.

As bolsas permanecem nas salas ao sair e voltar, inclusive após coleta parcial.
Não expiram e não bloqueiam portas. Uma nova run recria as salas e elimina todas
as bolsas; não há persistência em disco.

## Renderização

Os arquivos são `game/assets/img/reward-pouch/small.png`, `medium.png` e `big.png`.
Os PNGs provisórios são usados sem alteração. Inicialmente, todos são desenhados
com largura de 32 px, proporção preservada e base na borda inferior da hitbox.
Largura e hitbox são configuráveis por definição.

O renderer usa o mesmo sistema de profundidade, revelação e iluminação de props
e personagens. Assets ausentes recebem retângulo magenta e um aviso por tipo
carregado. Texturas são cacheadas e descarregadas antes de fechar a janela.

## Validação

Execute a partir de `game/`:

```powershell
mingw32-make game
mingw32-make test
mingw32-make test-reward-pouch-render
```

A suíte padrão inclui os testes de regras e gameplay das bolsas, sem janela.
Cobrem probabilidades e limites, conteúdo e reprodução por seed, integração
de mortes e destruição com projéteis dos dois lados, coleta parcial, gates de
input, pilhas e inventário cheio, múltiplas bolsas, preservação por sala e nova
run. O teste gráfico opcional usa janela oculta e pixels de uma render texture;
cobre proporção, transparência, revelação, profundidade, iluminação, cache e
fallback. Também salva `game/obj/reward-pouches-preview.png` com os assets reais.

Verificação manual em `game.exe`, com `game/` como diretório atual:

1. Derrotar inimigos à distância: confirmar que moedas só aumentam ao tocar a
   bolsa e que nem toda morte gera drop. Conferir feedback de moedas e materiais.
2. Destruir vasos com armas corpo a corpo, à distância e ataques inimigos.
   Confirmar no máximo uma bolsa por vaso e nenhuma mudança nos demais props.
3. Encher o inventário, deixando uma pilha de couro em 98. Coletar uma bolsa com
   mais couro: receber apenas o que cabe; liberar espaço e voltar para receber
   o restante, sem receber moedas novamente.
4. Sair da sala com bolsas no chão e voltar: posições e conteúdo devem persistir.
   Reiniciar a run: as bolsas anteriores devem desaparecer.
5. Conferir desenho ao passar à frente/atrás de bolsas, junto às velas, durante
   revelação de salas e com zoom. Substituir futuramente os PNGs mantendo os nomes.
6. Confirmar que inventário/console bloqueiam coleta e que um golpe letal no
   jogador impede coleta no mesmo frame.
