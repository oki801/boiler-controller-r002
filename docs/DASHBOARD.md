# Felhasználói dashboard

Két fájl van, a firmware mellé:

| Fájl | Mi ez | Állapot |
|---|---|---|
| `data/www/dashboard.html` | A megadott eredeti dashboard | **Változatlanul** be van ágyazva a firmware-be, a `/dash` címen érhető el. Statikus bemutató, adatkötés nélkül. |
| `data/www/dashboard_v3.html` | A v2 folyékony, animált változata, a kijelzővel közös design-nyelvvel | Beágyazva, a `/dash3` címen. Lásd `docs/DESIGN.md`. |
| `data/www/dashboard_v2.html` | Alverzió: ugyanaz a kinézet, valódi adatokkal | **Jóváhagyva és beágyazva**, a `/dash2` címen érhető el. Önmagában is megnyitható böngészőben (bemutató módban). |

## 0. A helyi kijelző

A beépített 1,44" kijelző ugyanezt a felületet követi (gyűrűs hőmérő, állapotikon, hálózat jelzők, ikonos lábléc). Lásd a `README.md` kijelző-fejezetét és a `docs/kijelzo_elonezet_*.png` képeket.

## 1. Döntések

| Kérdés | Döntés |
|---|---|
| A csúszka mit jelent? | **Célhőmérséklet.** A hiszterézis is a firmware vezérlési láncába kerül (lásd 2.). |
| Útvonalak | Maradnak: a teljes kezelőfelület a `/`, a dashboard a `/dash`. |
| Lábléc ikonmenü | Marad az eredeti: az ikonokra jelennek meg az oldalak. |
| Az eredeti fájl apróságai | Nem nyúlunk hozzájuk, a 5. pontban érthetően le vannak írva. |
| A kiegészítések | Külön alverzióként (`dashboard_v2.html`) mutatjuk meg, az eredeti érintetlen marad. |

## 2. Célhőmérséklet és hiszterézis a firmware-ben (megépítve)

A vezérlő mostantól maga is termosztát. A bojler saját termosztátja és hőkioldója tartalék marad (állítsd a célnál magasabbra).

| Helyzet | Mit csinál |
|---|---|
| víz ≥ cél | nem fűt (állapot: cél elérve) |
| víz ≤ cél − hiszterézis | fűt (állapot: felfűtés) |
| a kettő között | megtartja az előző állapotot: ha fűtött, tovább fűt a célig; ha a célnál kikapcsolt, vár |
| cél = 0 | kikapcsolva, a garantált minimum sem fűt |
| garantált minimum aktív | fűt, még ha a cél alacsonyabb is (ezért "garancia") |
| nincs hőmérő adat | nem tilt, a bojler termosztátja véd, E01 jelzi a hibát |

A forrásonkénti hőfokhatár továbbra is a cél alatt maradhat (pl. drága hálózat 55 °C, cél 60 °C). A beállítás a `/api/config`
`target` és `hyst` mezőin, MQTT-n a `cmd/target/set` parancson, Home Assistantban `number` entitáson érhető el.
Tesztelve van (PC-s tesztek), hardveren nem próbáltuk.

## 3. A v2 alverzió

Az eredeti kinézete és szerkezete marad (színek, gyűrű, csúszkák, lábléc). Az alábbiak **újak**, a fájlban `ÚJ` jelzéssel megjelölve:

1. **A nagy szám a mért hőmérséklet**, a gyűrű is ezt mutatja. A csúszka a célhőmérséklet, a gyűrűn egy fehér jelölő mutatja.
2. **Az állapot az eszközből jön** (felfűtés, várakozás, hőfok rendben, kikapcsolva, kézi üzem, hiba).
3. **Három hálózat jelző** (jelen van, bojleren, hőfokhatár miatt tiltva), egy **mérési sor** (°C, W, V, A) és egy **garancia jelzés**.
4. **Napló és hibák oldal valós adattal.** A beállítások az eszközre mentenek, nem a böngészőbe. Csak azt küldi el, amihez hozzányúltál.
5. **Bemutató mód:** ha az eszköz nem érhető el (például a fájlt közvetlenül megnyitod), mintaadattal fut, és ezt egy sárga sáv jelzi.

Kipróbálás: az eszközön a `http://<ip>/dash2` cím. A fájl önmagában is megnyitható böngészőben, ez a bemutató mód.
Ha belépéskor (HTTP Basic) a jelszó-ablakot megszakítod, az oldal szintén bemutató módba esik.

## 4. Miért jó az állapot-megfeleltetés (a korábbi 3. javaslat)

Az eredeti fájl a csúszkából számolja ki, mit mutasson ("felfűtés", "várakozás", "rendben"), de a csúszka csak a **kívánt** értéket
tudja, azt nem, hogy a bojler **éppen mit csinál**. A megfeleltetés azt jelenti, hogy a képernyőn látható szöveg és ikon az eszköz
valódi adataiból jön: van-e áram a bojleren, mennyi a mért víz hőmérséklete, melyik hálózat van rajta, van-e hiba.

Haszna: nem fordulhat elő, hogy a képernyő "felfűtés"-t mutat, miközben a bojler áram nélkül áll, vagy "rendben"-t, miközben hiba van.
Például a v2-ben a "Fűtene, de nincs használható hálózat" állapot külön látszik, ez az eredetiben nem lehetséges.

## 5. Megjegyzések az eredeti fájlhoz (nem módosítottuk)

Ezek nem hibák, hanem a bemutató jellegből adódó dolgok. Akkor számítanak, ha az eredetit élesben használnád.

1. **Fix 55 fok a logikában.** A fájl a "felfűtés / várakozás / rendben" állapotot a csúszka értékéből és egy beégetett 55 fokból számolja, ezért a csúszka mozgatásakor "rendben" lesz akkor is, ha a víz hideg. Élesben a mért hőmérséklet kell hozzá (ezt oldja meg a v2).
2. **A beállítás csak a böngészőben marad meg.** A mentés a `localStorage`-be megy, így egy másik telefon vagy gép nem látja, és az eszközhöz sem jut el.
3. **A napló és a hibák oldal fix szöveg.** Ugyanaz látszik mindig, függetlenül attól, mi történik.
4. **A lábléc csak emojit tartalmaz.** Neked így jó, és tényleg jól olvasható. Egyetlen hátránya, hogy a képernyőolvasó nem tudja felolvasni a gombok nevét. Ha ez fontos lesz, láthatatlan `aria-label` jelzés adható hozzájuk a látvány megváltoztatása nélkül.
5. **A csúszkáknak nincs szöveges címkéjük.** A hiszterézisnél van felirat, a célhőmérséklet csúszkán nincs, a nagy szám mellett ez egyértelmű, de a képernyőolvasónak nem.
6. **A 0 fok kikapcsolást jelent.** Kényelmes, de ha a csúszkát véletlenül balra húzod, a fűtés kikapcsol. A v2 ugyanígy viselkedik, mert ezt így kérted. Ha zavar, külön "Ki" kapcsoló adható.
7. **A csúszka 0–60 fok.** A firmware 75 fokig enged célt, és ha a konfigurációban 60 fölötti célt állítasz, a csúszka a szélén látszik. A v2 csak akkor ment célhőmérsékletet, ha a csúszkához hozzányúltál, így ezt nem írja felül.
8. **A pulzáló zöld jel nem veszi figyelembe a "kevesebb mozgás" rendszerbeállítást.** Aki ezt kéri a telefonján, azt is látja. A v2 ezt kezeli.
9. **Névütközés a kódban.** A `status` név egy beépített böngészői név is. A mai böngészőkben működik, de más néven biztosabb.

## 6. Nyitott kérdések

1. ~~Beágyazzuk a v2-t?~~ Megtörtént (`/dash2`).
2. Ha a v2 az eszközön is bevált, legyen a főoldal (`/`) a dashboard, a teljes felület pedig `/admin`? (Most marad a mostani kiosztás.)
3. A csúszka tartománya maradjon 0–60, vagy menjen 75-ig?
