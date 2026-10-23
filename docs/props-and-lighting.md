# Props e iluminação

Props de chão são gerados uma vez, depois das portas e antes dos inimigos. Somente
salas `Normal` recebem props; o catálogo inicial inclui apenas `Dungeon`.
O estado pertence à sala: vida e destruição persistem ao sair e retornar,
mas são recriados numa nova partida. Não há persistência em disco.

## Configuração

O catálogo fica em `game/src/prop.cpp`. Cada definição contém identificador,
nome, lista de biomas, largura e altura em tiles, peso, disposição opcional,
colisão e dano por segundo. Destruição, spritesheet e luz são configurações
opcionais por composição. A tabela `rewardPouchDrops` configura a chance de
deixar uma bolsa ao destruir o prop. O vaso tem 20% de chance de uma bolsa
`small`; os demais props continuam indestrutíveis. Consulte
[Reward pouches](reward-pouches.md) para pools, coleta e testes.

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

## Props de parede

O catálogo de parede fica em `game/src/wall_prop.cpp`, separado das props
de chão e de suas identidades de combate. `WallPropDefinition` configura
identificador, caminho do PNG, largura em tiles, seções, biomas, peso e,
opcionalmente, `PropAnimation` e `PropLight`. O catálogo precisa viver por
toda a partida. Acrescentar uma decoração estática ou sem luz exige apenas
uma nova definição; seções `Side` ainda não são suportadas pela geração.

`WallPropInstance` referencia um módulo real de `WallRules`. O frame inteiro,
incluindo margens transparentes, é escalado para a largura declarada; sua
base coincide com a base do retângulo visual da parede. `ModuleBounds`
centraliza esse retângulo para paredes, oclusão e props. A renderização
compartilha seleção de frames, escala, cache de texturas e fallback magenta
com as props de chão; os caminhos completos identificam as texturas no cache.

| Identificador | Encaixe | Animação | Luz |
|---|---|---|---|
| `torch` | 2 tiles / 128 px | 8 frames horizontais de 995×957 px; 0,10 s/frame | Raio 320 px; intensidade 0,65; RGB `(255, 210, 140)` |

O PNG fica em `assets/img/wallprop/torch.png` e é preservado sem recorte.
A origem da luz é `(0,50; 0,36)` dentro do frame. A fase da animação deriva
da identidade da instância. Tochas não recebem ataques, não bloqueiam
movimento e não causam dano nem oferecem interação.

### Geração e ciclo de vida

- Salas normais de Dungeon usam chance de 35% por módulo elegível e reserva
  de 2 tiles livres entre footprints na mesma superfície contínua.
- Salas especiais de Dungeon recebem uma tocha garantida no módulo
  imediatamente à esquerda e à direita de cada porta north/south não selada.
  Um lado sem espaço é omitido, módulos compartilhados não duplicam props,
  e o espaçamento aleatório não impede as colocações garantidas.
- Corredores usam o bioma resolvido por `FloorRules::CorridorBiome` e recebem
  tochas somente quando ele for Dungeon. Trechos upper/bottom são elegíveis;
  corredores verticais com apenas paredes side ficam sem tochas.
- Vãos, módulos residuais de 1 tile e módulos que cruzam anfitriões são
  excluídos. Lobby não recebe essas props.

`WallPropRules::GenerationSettings` centraliza chance e espaçamento. O RNG
é independente do mapa, combate e props de chão. A geração usa todas as
salas conhecidas com visibilidade plena e ordenação estável, sem depender
da revelação ou ordem de entrada no mapa de salas.

O registro pertence a `RoomManager` e é reconstruído deterministicamente
após a geração/alinhamento de portas. A identidade combina anfitrião, seção
e posição: salas usam suas coordenadas e bounds; corredores usam seu
footprint global, compartilhado pelas duas portas. Assim não há desenhos
ou fontes de luz duplicados. A mesma geometria e seed reproduzem posição
e fase ao retornar; uma nova partida substitui o registro. Não há gravação
em disco. Views de renderização só são válidas até a próxima atualização
do registro.

### Camadas, revelação e luz

Upper props são desenhadas depois das paredes de fundo e antes dos atores.
Bottom props participam da ordenação dos atores pela base do módulo e são
cobertas pelo passe de paredes frontais. A bottom continua transitando de
100% para 60% de opacidade em 0,2 s quando encobre o jogador. A tocha mantém
sua própria opacidade: é vista através da parede translúcida, sem um segundo
fade. O estado de oclusão não participa da emissão de luz.

A revelação da sala afeta tanto o desenho quanto a intensidade da fonte.
Em corredores, a exposição combina revelação e progresso de abertura da
porta compartilhada; portas fechadas não expõem tochas nem emitem essa luz.
Máscaras escuras de portas continuam sendo aplicadas normalmente.

`SceneLighting::Apply` aceita fontes adicionais `SceneLightSource`, com
posição global, configuração de luz, visibilidade e retângulo de recorte.
As fontes de parede são limitadas à sala ou ao corredor anfitrião, incluindo
a faixa vertical de paredes, e não iluminam salas vizinhas. As paredes dos
corredores também recebem a luminosidade ambiente do respectivo bioma.
Tochas e velas usam o mesmo passe multiplicativo, com saturação, câmera e
zoom; interfaces continuam fora desse passe.

## Validação

Execute dentro de `game/`:

```powershell
mingw32-make game
mingw32-make test
mingw32-make test-prop-render
mingw32-make test-wall-render
mingw32-make test-wall-prop-render
```

Os testes de regras não exigem janela. Os testes de combate e gameplay
usam a Raylib vendorizada, sem abrir janela. O teste gráfico exige contexto
OpenGL e usa uma janela oculta, assets temporários e leitura de uma textura
de tamanho fixo, independente da escala de tela do Windows. Ele cobre
escala, transparência, animação nas duas orientações, profundidade,
revelação, múltiplas fontes, zoom, isolamento entre salas e redimensionamento.
Uma prévia com os quatro sprites reais é salva em `game/obj/props-preview.png`.

Os testes de wall props cobrem determinismo, biomas, encaixe, espaçamento,
portas de salas especiais, corredores compartilhados, revelação e abertura
de portas. A suíte de geração também verifica a integração do registro com
`RoomManager` e sua substituição ao reiniciar. O teste gráfico compara pixels
durante entrada e saída da translucidez da bottom, profundidade dos atores,
os oito frames, margens transparentes, luz além do alcance da vela, recorte
em salas/corredores e câmera com zoom. Usa janela oculta e assets temporários.
A prévia `game/obj/wall-props-preview.png` usa os sprites reais e os passes de
parede e iluminação, mostrando upper, bottom opaca, bottom translúcida e um
corredor aberto. Ambas as prévias são artefatos locais, fora do Git.

Verificação manual em `game.exe`, iniciado de `game/`:

1. Explorar até encontrar salas normais de Dungeon e conferir variedade,
   distância entre props, entradas livres e circulação até todas as portas.
2. Caminhar contra os quatro props e observar a ordenação ao passar à frente
   e atrás deles; testar também junto às paredes e às entradas.
3. Observar inimigos nascendo fora dos objetos, perseguindo ao redor deles
   e retornando à origem ao sair da sala.
4. Quebrar vasos com armas corpo a corpo e à distância. Posicionar-se para
   que ataques inimigos também atinjam vasos. Confirmar liberação imediata
   do espaço, chance de drop de uma bolsa, ausência de cura e passagem de
   ataques pelos outros props. As recompensas só são entregues na coleta.
5. Sair e retornar: vasos quebrados continuam ausentes. Reiniciar a partida
   recria os props. Salas especiais e os demais biomas não recebem props.
6. Conferir luz suave perto da vela, legibilidade longe dela, transições de
   revelação e corredores fechados. HUD, inventário, prompts e console
   mantêm suas cores. Observar encaixe da iluminação ao mover a câmera.
7. Em Dungeon, conferir tochas nas upper e luz junto às bottom opacas.
   Aproximar o jogador da bottom: a fileira fica translúcida e revela as
   tochas através da parede; afastar restaura a oclusão. Conferir a ordem
   em relação ao jogador e inimigos e observar a animação sem mover o encaixe.
8. Conferir pares de tochas junto às portas north/south de salas especiais
   quando houver espaço. Portas east/west não garantem tochas nessas salas.
   Abrir corredores horizontais e conferir aparecimento gradual, luz e
   ausência de duplicação ao atravessar e retornar. Corredores verticais
   não recebem tochas em suas paredes laterais.
