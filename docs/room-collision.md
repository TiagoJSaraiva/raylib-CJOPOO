# Colisão na borda inferior das salas

O limite de movimento termina alguns pixels antes da borda visual inferior da sala. Os valores ficam separados por bioma em `game/src/room_collision_rules.cpp`; por enquanto, todos começam em 8 px e podem ser ajustados individualmente.

## Verificação manual

Execute o jogo a partir de `game/` e, em uma sala de cada bioma sem saída ao sul, caminhe para baixo até a borda. O jogador deve parar antes de cruzar o limite visual inferior. Em uma sala com porta ao sul aberta, confirme também que ainda é possível atravessar para a sala seguinte.
