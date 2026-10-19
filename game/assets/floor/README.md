# Sprites de chão

Coloque os PNGs diretamente em `cave/`, `dungeon/`, `mansion/` ou `lobby/`.
Execute o jogo com `game/` como diretório atual.

O nome deve ser `id-peso.png`, por exemplo `1-2.png`, `2-5.png`, `3-2.png`.
ID e peso são inteiros positivos de até 64 bits sem sinal (máximo
18446744073709551615). A extensão é exatamente `.png`. O ID serve apenas
para compor o nome: `1-2.png` e `1-5.png` podem coexistir. Zeros à esquerda
são aceitos. Nomes fora desse formato não são carregados.

O peso é relativo: com pesos 2, 5 e 2, as probabilidades são 2/9, 5/9 e 2/9.
Arquivos são ordenados pelo caminho antes de formar o catálogo. Se a soma
dos pesos exceder o limite de 64 bits, o arquivo que excederia a soma é
ignorado com aviso.

Cada imagem precisa ser quadrada, mas pode ter qualquer resolução positiva
(64×64, 128×128, 256×256 etc.). O jogo usa a imagem inteira, com filtragem
bilinear, para preencher um tile de 64×64 pixels, sem rotação, espelhamento
ou alteração de cor. Transparência do PNG é preservada.

As texturas são lidas uma vez ao abrir o aplicativo. Reinicie o aplicativo
para incorporar novos arquivos ou mudanças de peso. Arquivos inválidos,
corrompidos ou retangulares são ignorados; se não restar nenhum sprite
válido para um bioma, o piso usa a cor sólida original e registra um aviso.

As escolhas por posição ficam na memória durante a partida e são apagadas
ao iniciar outra. A mesma seed e o mesmo catálogo reproduzem o piso,
independentemente da ordem de descoberta. Corredores compartilham as
escolhas entre as duas salas; a conexão com o lobby usa o bioma externo.

## Verificação visual

Depois de adicionar os sprites, executar `mingw32-make game` e `game.exe`
a partir de `game/`. Executar `mingw32-make test` para as regras isoladas.
O teste opcional `mingw32-make test-floor-render` usa uma janela oculta e
assets temporários para conferir os quatro biomas, escala, transparência,
corredores compartilhados, persistência e fallback. Ele exige contexto gráfico.

- Conferir o lobby e iniciar partidas até observar cave, dungeon e mansion.
- Usar ao menos dois sprites distinguíveis por bioma; comparar uma sala antes
  e depois de sair e retornar, incluindo o corredor do lobby nos dois sentidos.
- Experimentar imagens 64×64 e 256×256 e verificar que ocupam o mesmo espaço.
- Conferir que portas fechadas mantêm suas máscaras, que a revelação do chão
  é gradual e que corredores compartilhados não ficam mais opacos.
- Mover a câmera e observar encaixe dos tiles, paredes e entidades.
- Reiniciar uma partida; conferir que o jogo continua funcionando sem reler
  o catálogo e que uma nova seed pode alterar os sprites.
- Reiniciar o aplicativo com uma pasta vazia, um PNG corrompido, um nome
  inválido e uma imagem retangular; conferir os avisos e o piso sólido.

Os diretórios são entregues sem arte final; os sprites devem ser adicionados
nesses locais para a validação visual com os assets do jogo.
