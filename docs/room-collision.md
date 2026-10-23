# Colisão na borda inferior das salas

O limite de movimento termina alguns pixels antes da borda visual inferior da sala. Os valores ficam separados por bioma em `game/src/room_collision_rules.cpp`; atualmente, todos usam 15 px e podem ser ajustados individualmente.

Ao contornar props, a navegação inimiga ajusta os pontos de cada tile à área em que sua caixa de colisão cabe. Isso mantém a última linha de tiles acessível, mesmo quando o centro nominal do tile deixaria parte da caixa além do recuo inferior.

## Verificação manual

Execute o jogo a partir de `game/` e, em uma sala de cada bioma sem saída ao sul, caminhe para baixo até a borda. O jogador deve parar antes de cruzar o limite visual inferior. Em uma sala com porta ao sul aberta, confirme também que ainda é possível atravessar para a sala seguinte.

Em uma dungeon com inimigos vivos, posicione o jogador junto à parede inferior, fora do alcance de ataque e com uma jaula ou mesa bloqueando o caminho direto do inimigo. Ele deve contornar o prop e continuar até entrar no alcance de ataque. Repita com o caminho direto livre e movendo o jogador pela borda inferior; o inimigo não deve parar longe do jogador nem atravessar props. Confira novamente a passagem por uma porta sul aberta após o combate.

As regressões automatizadas de navegação e perseguição ficam em `prop_rules_tests.cpp` e `prop_gameplay_tests.cpp` e são executadas por `mingw32-make test` dentro de `game/`.
