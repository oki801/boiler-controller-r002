# Kijelző betűkészletek

Az Adafruit GFX `fontconvert` eszközével előállított bitmap betűk, a **DejaVu Sans** (Bitstream Vera alapú, szabadon
felhasználható és beágyazható licenc) betűcsaládból. Mind ASCII, ékezetet nem tartalmaz (a kijelző szövegei is ASCII-ben vannak).
A betűket a `SsCanvas` kétszeres felbontású vásznon rajzolja, majd lekicsinyíti, így a szélek élsimítottak.

| Fájl | Betű | Karakterek | Használat (`setTextSize`) |
|---|---|---|---|
| `DejaVuCondBold9pt7b.h` | DejaVu Sans Condensed Bold, 9 pt | 0x20-0x7E | 1: minden normál szöveg |
| `DejaVuBold19pt7bQ.h` | DejaVu Sans Bold, 19 pt | csak `?` | 2: az ismeretlen állapot ikonja |
| `DejaVuBold29pt7bNum.h` | DejaVu Sans Bold, 29 pt | `-` `.` `/` `0`-`9` | 3: nagy hőmérséklet-szám, hiszterézis |
| `DejaVuBold49pt7bQ.h` | DejaVu Sans Bold, 49 pt | csak `?` | 5: a hőmérő-hiba képernyő |

Újragenerálás (a pont-méret a 141 dpi miatt kb. 1,96 px/pt, a vászon kétszeres felbontású):

```
gcc -o fontconvert fontconvert.c $(pkg-config --cflags --libs freetype2)    # Adafruit-GFX-Library/fontconvert
./fontconvert /usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf 9 32 126 > DejaVuCondBold9pt7b.h
./fontconvert /usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf 19 63 63  > DejaVuBold19pt7bQ.h
./fontconvert /usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf 29 45 57  > DejaVuBold29pt7bNum.h
./fontconvert /usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf 49 63 63  > DejaVuBold49pt7bQ.h
```
