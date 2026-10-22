# Props e iluminação

Props são gerados uma vez, depois das portas e antes dos inimigos. Somente
salas `Normal` recebem props; o catálogo inicial inclui apenas `Dungeon`.
O estado pertence à sala: vida e destruição persistem ao sair e retornar,
mas são recriados numa nova partida. Não há persistência em disco.

## Configuração

O catálogo fica em `game/src/prop.cpp`. Cada definição contém identificador,
nome, lista de biomas, largura e altura em tiles, peso, disposição opcional,
colisão e dano por segundo. Destruição, spritesheet e luz são configurações
opcionais por composição. A pool de drops é reservada para uma etapa futura;
esta versão não entrega itens, moedas ou cura ao destruir props.

| Identificador | Dimensões | Peso | Disposição | Vida | Luz |
|---|---|---|---|---|---|
| `gray-vase` | 1×1 | 6 | 0 | 10 | Não |
| `barrel` | 1×1 | 1 | 0 | Indestrutível | Vela quente, raio 192 px |
| `bone-jail` | 1×1 | 3 | 0,5 | Indestrutível | Não |
| `torture-table` | 2×1 | 1 | 0,9 | Indestrutível | Não |

Todos começam com colisão para jogador e inimigos e dano de contato zero.
Para cadastrar outro prop, acrescente uma definição e seu PNG em
`game/assets/img/props/<identificador>.png`. O catálogo deve viver por toda
a partida. As instâncias geradas não devem ser adicionadas, removidas nem
reordenadas durante o combate: seus endereços identificam os alvos dos ataques.
A destruição mantém a instância e a marca como inativa.

A imagem inteira, incluindo margens transparentes, é escalada para a largura
do footprint. A proporção é preservada, e a base do arquivo coincide com a
base dos tiles ocupados. A altura não é limitada à hitbox. A colisão usa
sempre todos os tiles declarados. Spritesheets aceitam largura e altura de
frame, quantidade, orientação horizontal/vertical e segundos por frame.
Arquivos ausentes usam um retângulo magenta e registram um aviso uma vez.

O orçamento sorteado é um inteiro entre `ceil(área × 0,08)` e
`floor(área × 0,15)`, descontado pela área de cada objeto. O RNG deriva da
seed da sala e não altera a sequência do mapa ou dos ataques. Há um tile de
separação entre footprints, uma reserva de dois tiles de profundidade nas
entradas, e verificação de conectividade do piso. Se faltar espaço válido,
a sala recebe menos props. Disposição 0 favorece paredes, 1 favorece o
centro e ausência de disposição distribui uniformemente as posições válidas.

## Combate e movimento

Ataques dos dois lados danificam vasos. Props indestrutíveis permitem a
passagem de ataques. Munições consideram o trajeto percorrido no frame e
atingem o primeiro alvo geométrico, comparando personagens e vasos juntos.
Golpes e lasers preservam seus intervalos entre acertos. A interface de
alvo circular continua disponível; coleções de alvos mistos devem usar
`CollectDamageEventsForTargets` para resolver a ordem dos impactos.

Jogador e inimigos deslizam pelos obstáculos sem atravessá-los mesmo num
frame longo. O spawn de inimigos tenta até 64 posições por instância e
desiste quando nenhuma é livre. Inimigos usam o destino direto quando há
espaço e busca em largura pelos tiles quando o caminho está bloqueado,
considerando suas dimensões. A busca reflete o destino e os obstáculos
atuais a cada atualização, inclusive durante o retorno à origem.

O dano ambiental soma `danoPorSegundo × deltaSeconds`, sem arredondamento
ou mínimo por frame. Sólidos verificam a tentativa de contato antes da
correção do movimento; não sólidos verificam a posição final. Esse dano
reduz diretamente a vida, sem esquiva, armadura ou vampirismo.

## Iluminação

`SceneLighting::Ambient` centraliza a luminosidade: Dungeon 65%, Caverna
75%, Mansão 85% e Lobby 100%. A cena recebe um mapa de luz multiplicativo;
fontes adicionam luz colorida com queda radial suave e saturação. A vela
tem intensidade 0,4, cor RGB `(255, 210, 140)` e posição normalizada
`(0,52; 0,04)` dentro do sprite completo. Esses valores ficam em `PropLight`.

A luz acompanha câmera e zoom, respeita a revelação, e cada fonte é
limitada à própria sala. As máscaras das portas continuam escuras.
Não há sombras projetadas, emissão do jogador nem efeito sobre HUD,
inventário, console e prompts. Texturas e mapa de luz são descarregados
antes do fechamento da janela.

## Validação

Execute dentro de `game/`:

```powershell
mingw32-make game
mingw32-make test
mingw32-make test-prop-render
```

Os testes de regras não exigem janela. Os testes de combate e gameplay
usam a Raylib vendorizada, sem abrir janela. O teste gráfico exige contexto
OpenGL e usa uma janela oculta, assets temporários e leitura de uma textura
de tamanho fixo, independente da escala de tela do Windows. Ele cobre
escala, transparência, animação nas duas orientações, profundidade,
revelação, múltiplas fontes, zoom, isolamento entre salas e redimensionamento.
Uma prévia com os quatro sprites reais é salva em `game/obj/props-preview.png`.

Verificação manual em `game.exe`, iniciado de `game/`:

1. Explorar até encontrar salas normais de Dungeon e conferir variedade,
   distância entre props, entradas livres e circulação até todas as portas.
2. Caminhar contra os quatro props e observar a ordenação ao passar à frente
   e atrás deles; testar também junto às paredes e às entradas.
3. Observar inimigos nascendo fora dos objetos, perseguindo ao redor deles
   e retornando à origem ao sair da sala.
4. Quebrar vasos com armas corpo a corpo e à distância. Posicionar-se para
   que ataques inimigos também atinjam vasos. Confirmar liberação imediata
   do espaço, ausência de drops/recompensas/cura e passagem de ataques pelos
   outros props.
5. Sair e retornar: vasos quebrados continuam ausentes. Reiniciar a partida
   recria os props. Salas especiais e os demais biomas não recebem props.
6. Conferir luz suave perto da vela, legibilidade longe dela, transições de
   revelação e corredores fechados. HUD, inventário, prompts e console
   mantêm suas cores. Observar encaixe da iluminação ao mover a câmera.
