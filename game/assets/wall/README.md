# Sprites de parede

Coloque os PNGs em `lobby/`, `cave/`, `dungeon/` ou `mansion/`, dentro da pasta
de orientação correspondente: `side/`, `upper/` ou `bottom/`. Execute o jogo
com `game/` como diretório atual.

O nome deve seguir o mesmo padrão dos sprites de piso: `id-peso.png`, por
exemplo `1-2.png`, `2-5.png`, `3-2.png`. ID e peso são inteiros positivos de
até 64 bits sem sinal. O ID serve apenas para compor o nome; nomes repetidos
com pesos diferentes são aceitos. O peso é relativo dentro do catálogo de cada
bioma e orientação.

`upper/` contém as paredes superiores, `bottom/` as paredes inferiores e
`side/` as paredes laterais esquerda e direita. O mesmo catálogo de `side/` é
usado nos dois lados. Cada imagem precisa ser legível e ter dimensões positivas;
não precisa ser quadrada. As imagens de `upper/` e `bottom/` são esticadas para
128 × 128 pixels, cobrindo dois tiles na horizontal e dois na vertical. As imagens
de `side/` são esticadas para uma faixa vertical de 16 × 128 pixels, cobrindo dois
tiles de altura e alinhada à borda do piso. Capas de cantos e portas usam apenas
um tile de altura. O filtro bilinear é usado, sem rotação ou espelhamento. A
proporção original não é mantida, e a transparência é preservada.

Os catálogos são carregados ao abrir o aplicativo. Reinicie-o para incorporar
novos arquivos ou alterações de peso. Arquivos inválidos são ignorados. Se um
catálogo não tiver imagens válidas, o jogo mantém o desenho procedural atual
para `upper/` e `bottom/` e usa uma parede procedural para `side/`.

As escolhas por módulo são determinísticas para a seed da partida, posição,
bioma e orientação. Corredores compartilham as escolhas entre as salas
conectadas, e as paredes acompanham a visibilidade gradual da sala.

## Verificação visual

Execute `mingw32-make game` e `game.exe` a partir de `game/`. Execute
`mingw32-make test` para validar as regras e a geometria. O teste opcional
`mingw32-make test-wall-render` usa uma janela oculta e assets temporários e
exige contexto gráfico.

- Conferir as laterais e os cantos de salas e corredores nas quatro direções.
- Conferir portas abertas, seladas e a transição entre biomas.
- Conferir escala, transparência, variação determinística e fallback sem PNGs.
- Confirmar que paredes de fundo ficam atrás das entidades e paredes
  inferiores ficam na camada frontal.
- Confirmar que bottom walls são ancoradas um tile acima e que toda a fileira
  da mesma sala ou corredor reduz e restaura o alpha em conjunto ao cruzar o player.
