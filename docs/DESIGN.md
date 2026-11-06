# Design-nyelv: webes dashboard és helyi kijelző

Alap: a megadott dashboard (`data/www/dashboard.html`): sötét tengerészkék háttér, kerek termosztát-tárcsa színes gyűrűvel,
ikonos lábléc. Ugyanezt a nyelvet beszéli a webes v3 (`dashboard_v3.html`, `/dash3`) és a beépített 1,44" kijelző (`lib/Display`).

## 1. Közös elemek

| Elem | Jelentése | Web (v3) | LCD (128x128) |
|---|---|---|---|
| Tárcsa gyűrűje | a **mért** hőmérséklet, 0-60 °C, 12 óránál indul, óramutató irányában | `conic-gradient` | pontokból rajzolt gyűrű |
| Gyűrű színe | a hőmérséklet árnyalata: `hue = 220 − (T/60)·220` (kék → zöld → sárga → piros) | `hsl(hue 90% 55%)` | ugyanaz a képlet |
| Fehér jelölő | a **célhőmérséklet** a gyűrűn | forgó csík | forgó csík |
| "Víz" a tárcsában | szintje = hőmérséklet (0 °C alul, 60 °C felül), színe a gyűrű árnyalata, áttetszően | két SVG hullám | két szinuszhullám, áttetszően keverve a sötét háttérre |
| Nagy szám | a mért hőmérséklet, egész fokra kerekítve | 65 px | 3-as méretű betű |
| Állapotikon | felfűtés (láng), várakozás (zöld LED), rendben (mosoly), kikapcsolva (⏻), hiba (háromszög), kézi üzem (S1), nincs hálózat (homokóra) | emoji | rajzolt ikonok |
| Három hálózat | kitöltött: bojleren; keret: jelen van; piros keret: tiltva; szürke: nincs feszültség | chip-ek | doboz-jelzők |
| Lábléc | főoldal, napló, hibák, beállítások | csúszó kiemelés | csúszó aláhúzás |

Színek: háttér `#0f172a`, kártya `#1e293b`, panel `#111827`, gyűrű-sáv `#253142`; hálózatok: sárga `#facc15`, kék `#60a5fa`, lila `#d946ef`;
siker `#22c55e`, hiba `#ef4444`.

## 2. Mozgás (hogy folyékony legyen)

Elv: **semmi nem ugrik, minden átfut az új értékre**, és a mozgás mindig jelent valamit.

| Mozgás | Mit jelent | Időzítés |
|---|---|---|
| A szám, a gyűrű, a víz szintje és színe átfut az új értékre | a mért hőmérséklet változik | exponenciális simítás, 0,45 s időállandó |
| A jelölő elcsúszik az új célra | a célhőmérséklet változott | 0,16 s időállandó |
| A víz hullámzik | a tárcsa "él" | két hullám, ellentétes irányban, 7 s és 11 s |
| Buborékok emelkednek, a láng villódzik | éppen fűt | csak fűtés közben |
| A zöld LED lassan pulzál | vár (hiszterézis-sávban) | 2 s |
| A gyűrű piros ütemben lüktet | hiba | 1,6 s |
| Az oldalak becsúsznak, a lábléc kiemelése csúszik | navigáció | 0,25-0,45 s, lassuló görbe |
| Az első adatnál a hőmérséklet "feltölt" | a kapcsolat felépült | ~8 fokról indul |

A "kevesebb mozgás" rendszerbeállítás esetén a weben minden animáció kikapcsol, az értékek azonnal frissülnek.

## 3. A kijelző teljesítménye

- 30 kép/s mozgás, bevitel és oldalváltás közben; 15 kép/s a főoldalon (lassú hullám); 2 kép/s a statikus oldalakon.
- Memória: a villogásmentes rajzoláshoz 32 kB, az oldalváltás csúsztatásához további 32 kB (a PSRAM-ból). Ha nincs elég memória,
  az oldalváltás csúsztatás nélkül, azonnal történik, minden más működik.
- A kijelző külön, alacsony prioritású task-on fut, így a vezérlést nem lassítja.

## 4. Kipróbálás

- Webes előnézet: a `dashboard_v3.html` fájl az eszköz nélkül (böngészőben megnyitva) bemutató módban fut: lassú fűtési ciklus.
- Kijelző: `docs/kijelzo_elonezet_foldal.png`, `docs/kijelzo_elonezet_oldalak.png` (állóképek) és `docs/kijelzo_animacio.gif` (mozgás).
  Ezek a valódi rajzoló kódból készültek PC-n, nem rajzoltuk őket kézzel.
- Eszközön a mozgás simasága még nincs mérve (SPI-sebesség, tényleges képkockaszám).
