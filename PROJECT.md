# Bojlervezérlő: projekt állapot, terv és döntési pontok

Állapot: 2026. október 25. · szoftver v0.2 · ESP32-S3 N16R8 · PlatformIO + Arduino

> **Frissítés (2026-11-08, r004):** a tulajdonos szerint a firmware **működik**, a kijelző (LCD) és a beállítások menü is megvan.
> Az alábbi "nem kipróbált", "[MEGÍRVA]" és "nincs lefordítva" állítások ezért **részben elavultak**; a pontosításuk a 8. fejezet T12 teendője.
> A 2026-11-08-i kódátnézés (csak olvasás, futtatás nélkül) teendőit a **8. fejezet** gyűjti; a T1-T8 javításai az **r003**-ban, a T9-T11 döntései az **r004**-ben vannak (PC-s tesztek zöldek, ESP32-re nem fordítva, hardveren nem próbálva).

> **Fontos:** ez egy **terv** és egy **megírt, de nem kipróbált** első változat.
> A kódot a PlatformIO-s teljes build még **nem futtatta le**, **nincs linkelve, feltöltve, futtatva**, hardveren **semmi nem futott**,
> a 230 V-os rész **nincs megépítve**. Egyedül a hardverfüggetlen logikai réteg van PC-n tesztelve (92 teszt), és a teljes forráskód
> átment egy fordító-ellenőrzésen (lásd 2.1). Minden más állítás "megírva" vagy "terv".

Kapcsolódó dokumentumok: `README.md` (használat), a rendszerterv dokumentum (22 fejezet: architektúra, kapcsolási rajz,
BOM, lábkiosztás, állapotgép, tesztterv).

---

## 1. Jelmagyarázat

| Jel | Jelentése |
|---|---|
| **[TESZTELT-PC]** | PC-n futó unit teszt igazolja a logikát. Hardveren nem próbálva. |
| **[MEGÍRVA]** | A kód elkészült, de ESP32-re nem fordult le és nem futott. Hibák várhatók. |
| **[TERV]** | Csak a tervben van, kód nincs hozzá. |
| **[ELÁGAZÁS Dn]** | Döntési pont: lásd az 5. fejezetet. A mostani választás alapértelmezés, nem végleges. |

---

## 2. Mi készült el és mennyire biztos

| Rész | Állapot | Mi igazolja | Mi hiányzik |
|---|---|---|---|
| `Core`: típusok, konfiguráció és validáció, hibakódok, eseménybusz, hibakövetés | [TESZTELT-PC] | unit tesztek | hardveres kipróbálás |
| `Control`: hálózatválasztó, reteszelés, állapotgép, biztonsági felügyelet, hőfokhatár, garantált minimum, célhőmérséklet-termosztát | [TESZTELT-PC] | unit tesztek, köztük véletlen bejárás: két kontaktor sosem lehet egyszerre zárva | valós kontaktor-időzítések |
| Értesítési szabályok, vízkő-trend algoritmus, 24 órás előzmény-puffer, energiaelszámolás, jelenlét-szűrő | [TESZTELT-PC] | szimulált fűtési ciklusok, határesetek | valós mérési zaj (hőmérő késése, PZEM) |
| `Hal`: kontaktor-kimenetek, idő (NTP + NVS-mentés) | [MEGÍRVA] | semmi | fordítás, lábak, boot-viselkedés |
| `Sensors`: DS18B20, PZEM-004T, hálózat-jelenlét (impulzusszámlálás) | [MEGÍRVA] | semmi | szenzorok, a jelenlét-küszöb finomhangolása |
| `Storage`: konfiguráció (NVS), napló (LittleFS), előzmény (PSRAM), trend (NVS) | [MEGÍRVA] | semmi | fordítás, LittleFS viselkedés |
| `Comms`: WiFi, MQTT + Home Assistant discovery, webes API, OTA, értesítés | [MEGÍRVA] | semmi | fordítás, könyvtár-verziók, valódi HA |
| `App`: Supervisor és a task-ok | [MEGÍRVA] | semmi | fordítás, futás közbeni időzítések |
| `Display`: 1,44" ST7735, a dashboardot követő felület (gyűrűs hőmérő, ikonok, 4 oldal, UP/DOWN/PAGE gombok) | [MEGÍRVA] | a rajzoló kód valódi Adafruit GFX-szel PC-n renderelve (`docs/kijelzo_elonezet_*.png`), valódi fordítóval szintaxis-ellenőrizve | eszközön a kép-eltolás, olvashatóság, gombok |
| Webes felület (egy HTML, a firmware-be ágyazva) | [MEGÍRVA] | a JavaScript szintaxisa rendben, a grafikon-rajzoló mintaadattal fut | böngészős kipróbálás, telefonos nézet |
| `platformio.ini`, partíciós tábla | [MEGÍRVA] | semmi | a `pio run` még nem futott le (a fejlesztői sandboxból a csomagregiszter nem érhető el) |
| Hardver: kontaktorok, reteszelés, tápok, watchdog, kézi bypass | [TERV] | papíron (kapcsolási rajz, BOM) | minden |

Futtatható ellenőrzések: `scripts/run_native_tests.sh` (a partíciós tábla ellenőrzését is futtatja) vagy `pio test -e native`.

### 2.1 Fordító-ellenőrzés (2026-10-29)

A teljes ESP32-es forráskód (18 fájl: `lib/*`, `src/main.cpp`) lefordult **objektumfájlokká** a valódi **Arduino-ESP32 2.0.17** maggal és az
**xtensa-esp32s3 GCC 8.4** fordítóval, a `platformio.ini`-ben rögzített könyvtár-verziókkal (a PlatformIO nélkül, kézzel összeállított környezetben).
A szimbólum-ellenőrzés szerint a projektkódban nincs feloldatlan hivatkozás és nincs duplikált erős szimbólum.

Ez **nem** a PlatformIO-s build. Nem igazolt: a PlatformIO könyvtár-felderítése (LDF), a Windows-specifikus működés, a tényleges linkelés,
a program mérete a flash-ben, és bármilyen futás. A következő valódi lépés a `pio run -e esp32s3`.

---

## 3. Ismert kockázatok (ezeket várjuk az első kipróbálásnál)

1. **Fordítási hibák.** Az ESP32-es kód sosem ment át fordítón. A könyvtárak API-ja és verziói (ESPAsyncWebServer fork,
   PZEM, Adafruit ST7735, PubSubClient) eltérhetnek a feltételezettől.
2. **ESP32-S3 lábak.** A kiosztás az N16R8 modul korlátait követi (26–37 foglalt, 19/20 USB, strapping: 0, 3, 45, 46),
   de a konkrét devkit feliratai eltérhetnek. Beszerelés előtt egyeztetni kell az `include/pins.h` fájllal.
3. **LittleFS** a gyakorlatban nyűgös. Ezért csak a naplót tárolja, a vezérlés és a webes felület nem függ tőle (D3).
4. **Hálózat-jelenlét érzékelés.** A 100 Hz-es impulzusok számlálásának küszöbei (3 impulzus / 100 ms, 2 s kiesés-szűrés)
   számítottak, mérve nincsenek.
5. **Hőmérő és vízkő-figyelés.** A tartály-felületi DS18B20 késik, ez torzítja a Wh/°C értéket. A trend csak lassú változásra jó,
   és csak gyanút jelez (D9).
6. **Értesítés HTTPS** nem ellenőrzi a szerver tanúsítványát. Internetre kerülés előtt rendezni kell (D6).
7. **Kijelző** modulonként eltérő eltolású lehet (`INITR_144GREENTAB` vagy `INITR_BLACKTAB`).
8. **Időzítések** (holtidő, tartózkodás, stabilitás) alapértékek, valós terhelésen kell hangolni.
9. **Mechanikus reteszelés** (D2): a három kontaktoros mechanikus egység nem biztos, hogy létezik a kiválasztott családhoz.

---

## 4. A terv: mit szeretnénk (és hol tart)

| Funkció | Állapot |
|---|---|
| 3 hálózat közül automatikus választás, paraméterezhető prioritással | [TESZTELT-PC] a logika, [MEGÍRVA] a futtatás |
| Hardveres + mechanikus + szoftveres reteszelés, biztonsági váltási késleltetés | szoftver [TESZTELT-PC], hardver [TERV] |
| Célhőmérséklet-termosztát hiszterézissel (a vezérlő is termosztát, a bojler sajátja tartalék) | [TESZTELT-PC] |
| Forrásonkénti hőfokhatár | [TESZTELT-PC] |
| Garantált minimum esténként (alapból 19:00-ra 45 °C, napközben nincs) | [TESZTELT-PC] |
| DS18B20 hőmérő, PZEM-004T (V, A, W, kWh) | [MEGÍRVA] |
| WiFi, webes felület, OTA, MQTT, Home Assistant | [MEGÍRVA] |
| Napló (mérés, események, hálózatváltások, hibák) időbélyeggel | [MEGÍRVA] |
| 24 órás előzmény-grafikon (PSRAM) | puffer [TESZTELT-PC], többi [MEGÍRVA] |
| Helyi kijelző (ST7735): dashboard-szerű főoldal, napló, hibák, beállítások; célhőmérséklet állítás gombokkal | [MEGÍRVA] |
| Idő: NTP, utána belső kvarc, NVS-be mentett idő áramkimaradás után | [MEGÍRVA] |
| Értesítés kritikus hibára (alapból KI, később aktiválható) | szabályok [TESZTELT-PC], küldés [MEGÍRVA] |
| Vízkő- és fűtőelem-trendfigyelés (E15, E16) | algoritmus [TESZTELT-PC], tárolás és felület [MEGÍRVA] |
| Napelemes többlet szerinti engedélyezés | csak a kapu van meg (`ext_ok`), az adatforrás [TERV] (D7) |
| Kontaktor-kopásszámláló, költségszámítás, legionella-ciklus | [TERV] |
| Külső RTC, TLS az MQTT-hez, aláírt OTA, Ethernet | [TERV], csak ha szükséges (D4, D6, D14) |

---

## 5. Elágazási pontok (döntések)

Mindegyiknél: a mostani választás, mikor érdemes váltani, és mit jelent a váltás.

### D1. Platform [ELÁGAZÁS D1]
| Opció | Mikor |
|---|---|
| **A. PlatformIO + Arduino (mostani)** | Alap. Marad, amíg a `pio run` utáni javítások kezelhetők. |
| B. Hibrid: ESPHome + a tesztelt `Core`/`Control` mint külső komponens | Ha a kommunikációs réteg (web, MQTT, tárolás) tartósan sok vesződést okoz. A biztonsági logika változatlan marad, az ESPHome adja a WiFi, OTA, HA, kijelző, szenzor részt. |
| C. ESP-IDF + FreeRTOS (Arduino nélkül) | Csak ha az Arduino réteg korlátoz (hivatalos OTA-visszagörgetés, kisebb firmware). Sok újraírás. |

**Eldöntési pont:** az első `pio run` és az első 1-2 hét hibajavítása után.

### D2. Mechanikus reteszelés [ELÁGAZÁS D2]
| Opció | Leírás |
|---|---|
| **M1 (mostani)** | 3 kontaktor + vezetékezett NC-lánc (tükörérintkezővel). Ha a gyártó kínál 3-as mechanikus reteszelőt, azzal kiegészítve. |
| M2 | Kétszintű fa: 4 kontaktor, két mechanikusan reteszelt pár. Szigorúan mechanikus "legfeljebb egy forrás". |

**Eldöntési pont:** a kontaktorcsalád kiválasztásakor (van-e 3-as reteszelő).

### D3. Fájlrendszer és webes felület [ELÁGAZÁS D3]
| Opció | Leírás |
|---|---|
| **A. UI a firmware-ben, LittleFS csak naplóra (mostani)** | A vezérlés és a web nem függ a LittleFS-től. |
| B. Napló nélkül (csak MQTT/HA) | Ha a LittleFS tartósan megbízhatatlan. |
| C. SD-kártya a naplónak | Ha sok adat vagy cserélhető tár kell. |

### D4. Idő [ELÁGAZÁS D4]
| Opció | Leírás |
|---|---|
| **A. NTP + NVS-mentés (mostani)** | Kimaradás után a mentett időből indul (becsült), hibája a kimaradás hossza. |
| B. DS3231 külső RTC | Ha hosszú áramkimaradás és internetkiesés együtt is előfordul, és az idősáv pontossága számít. |

### D5. Helyi kijelző [ELÁGAZÁS D5]
| Opció | Leírás |
|---|---|
| **A. 1,44" ST7735 (mostani)** | Színes, hardveres SPI. |
| B. OLED vagy karakteres LCD | Ha az ST7735 nem válik be (kicsi, eltolás). |
| C. Nincs kijelző | A web és a Home Assistant elég. |

### D6. Értesítés és távoli elérés [ELÁGAZÁS D6]
| Kérdés | Opciók |
|---|---|
| Csatorna | Telegram · webhook (JSON) · ntfy · csak Home Assistant továbbítás (**mostani: kikapcsolva**) |
| TLS | ellenőrzés nélkül (**mostani, csak helyi hálózaton elfogadható**) · tanúsítvány-ellenőrzés · helyi továbbító |
| Távoli elérés | **VPN (javasolt)** · felhős HA-szolgáltatás · port-továbbítás (**ne**) |

**Eldöntési pont:** mielőtt az eszköz az internetre kerül.

### D7. Napelemes többlet [ELÁGAZÁS D7]
Az adatforrás még nincs megírva. Opciók: Home Assistant (a `ext_ok` kapcsolót vezérli, a legegyszerűbb) · inverter Modbus · Shelly mérő.
Hiszterézis kell a felhős időre.

### D8. Garancia hőmérő nélkül [ELÁGAZÁS D8]
| Opció | Leírás |
|---|---|
| **A. Nem kapcsol be (mostani)** | Vakon nem vásárol drága áramot, E01 jelzi a hibát. |
| B. Bekapcsol az ablakban | Ha a fürdővíz fontosabb a költségnél. |

### D9. Vízkő-figyelés [ELÁGAZÁS D9]
Küszöb 10%, alap az első 8 ciklus. Váltási pontok: más jel hozzáadása (a ciklus végi csúcshőmérséklet, ciklusidő), a hőmérő helye
(köpeny vagy termosztátzseb), vagy kiegészítés tényleges szemrevételezéssel. Csak gyanút jelez.

### D10. Előzmény tárolása [ELÁGAZÁS D10]
**RAM (PSRAM, 24 óra, mostani)** · LittleFS (túléli az újraindítást) · külső (InfluxDB/HA).

### D11. Mérés [ELÁGAZÁS D11]
**Egy PZEM a bojler kimenetén (mostani)** · forrásonként is egy (keresztellenőrzés, pontosabb feszültség-hiba felismerés).

### D12. Hálózati kapcsolat [ELÁGAZÁS D12]
**WiFi (mostani)** · Ethernet (az S3-on nincs beépített MAC, külső W5500 modul SPI-n).

### D13. Webes hitelesítés [ELÁGAZÁS D13]
**HTTP Basic, SHA-256 hash-elt jelszó (mostani, csak helyi hálózaton)** · HTTPS és tokenes belépés, ha az eszköz kívülről is elérhetővé válik.

### D14. Felhasználói felület (dashboard) [ELÁGAZÁS D14]
A felhasználói dashboard (`data/www/dashboard.html`, `/dash`) változatlanul be van ágyazva, de **statikus bemutató**, adatkötés nélkül.
A folyékony, animált változat (`dashboard_v3.html`, `/dash3`) és az adatkötött v2 (`/dash2`) be van ágyazva [MEGÍRVA], böngészőben nem próbálva eszközön. Részletek: `docs/DASHBOARD.md`.

**Eldöntve:** a csúszka a **célhőmérséklet**, a hiszterézis a firmware vezérlési láncában van (megépítve, PC-n tesztelve). Az útvonalak maradnak, a lábléc ikonmenü marad.

| Opció | Állapot |
|---|---|
| A. Dashboard a főoldal, a teljes felület `/admin` | nyitott, a v2 kipróbálása után |
| B. Dashboard `/dash`, a teljes felület `/` | **mostani** |
| C. Csak a dashboard | nem javasolt (a technikai beállítások kikerülnek) |

---

### D15. Érvénytelen hőmérő [ELÁGAZÁS D15]
**Fűtés a bojler beépített termosztátjával tovább, a kijelzőn nagy figyelmeztetés (eldöntve, r004)** · időkorlátos lekapcsolás (nem kell).
E01-nél (hiányzó, hibás vagy elavult hőmérő-adat) a vezérlő nem tudja a víz hőfokát, a termosztát `Unknown` fázisban nem tilt, az `overTemp` nem tud kiváltani. A védelmet a bojler saját termosztátja adja. Ezt a főoldalon jól látható piros `?` és a "VIGYAZAT! FORRO VIZ!" felirat jelzi. Kikapcsolt fűtésnél (cél 0 vagy `Off`) és zárolt hibánál (`Fault`) nem jelenik meg.

## 6. Javasolt lépések és kapuk

| # | Lépés | Kapu (mikor léphetünk tovább) |
|---|---|---|
| 1 | `pio run -e esp32s3`, fordítási hibák javítása | tiszta fordítás |
| 2 | `pio test -e native` | 92 teszt zöld |
| 3 | Kisfeszültségű próbapad: 24 V, relék a kontaktorok helyett, kijelző, gomb | reteszelés, watchdog és ARM mérésekkel igazolva (T05-T10) |
| 4 | Szenzorok: DS18B20, PZEM, jelenlét-optocsatolók | a mérések helyesek, a jelenlét-küszöb hangolva |
| 5 | Hálózat: WiFi, web, MQTT, Home Assistant, OTA | 48 órás stabil futás |
| 6 | **Elágazás: D1, D2, D6** | döntés megszületett |
| 7 | 230 V-os szerelés (villanyszerelővel), szigetelési és PE mérés | szabványos mérési jegyzőkönyv |
| 8 | Terheléses próba, hibainjektálás, majd 2 hét üzemi megfigyelés | tesztterv (T01-T18) minden eleme rendben |

A 230 V-os rész kivitelezését jogosult villanyszerelőnek kell végeznie. A reteszelés biztonságát a hardver adja, nem a szoftver.

---

## 7. Változásnapló

| Verzió | Tartalom |
|---|---|
| v0.1 | Core és Control (tesztelt), a hardver- és kommunikációs rétegek, webes felület, ST7735 kijelző, 24 órás előzmény, ESP32-S3 N16R8 kiosztás |
| v0.2 | Forrásonkénti hőfokhatár, garantált minimum, értesítés (alapból ki), vízkő-/fűtőelem-trend, NVS-be mentett idő, a webes felület beágyazása (LittleFS-független) |
| r003 | Kódátnézés javításai (T1-T8): konfig-mentés holtpont és visszaállás, elavult szenzoradat észlelése, E10 számláló nyugtázáskor, célhőmérséklet-korlát és állapotszöveg a kijelzőn, eldobott parancsok jelzése, stack-mérés. Részletek: `CHANGELOG.md`. |
| r004 | D15 döntés: hőmérő-hiba (E01) képernyő a kijelzőn; az OTA alapjelszó marad (T10); a `ConfigStore.cppold` és `index.htmlold` törölve (T11). |

---

## 8. Teendők (kódátnézés, 2026-11-08, r002 alapján)

A lista egy **statikus kódátnézésből** készült: a kód nem lett lefordítva és nem futott. A hibák gyanúk, amíg hardveren nincsenek igazolva.
Minden lényeges módosítás új revíziót kap (a következő: **r005**).

### 8.1 Javítandó hibák

| # | Prio | Teendő | Részlet | Állapot |
|---|---|---|---|---|
| T1 | P1 | `ConfigStore::patch` önzárolás és mentési hiba | A `patch()` tartja az `mtx_` mutexet, majd a `save()` → `toJson()` → `get()` ugyanazt újra zárolja. A `Mutex` (`xSemaphoreCreateMutex`) nem rekurzív, ezért holtpont várható a kijelzőről vagy a webről indított mentésnél. Ráadásul ha az NVS-írás elbukik, a `cfg_` már felül van írva, a `version_` viszont nem nő: a vezérlő a régi, a kijelző és a web az új értéket használja. | KÉSZ r003: a `patch()` előbb ment, csak sikeres NVS-írás után cseréli a futó konfigot; a `save()`/`toJsonCfg()` nem vesz zárat |
| T2 | P1 | Elavult szenzoradat észlelése | A `sensorTask` nincs a watchdogon. Ha lefagy, a vezérlés a régi hőmérséklettel dolgozik tovább, és az `valid` marad. Kell egy maximum-életkor (pl. 5 s) a `seq` változására: lejárat után a hőmérséklet és a PZEM érvénytelen (E01). | KÉSZ r003: új `SensorFreshness` (4 teszt), 5 s után hőmérséklet és PZEM érvénytelen (E01), esemény a naplóba |
| T3 | P2 | E10 számláló nyugtázáskor | A `rstc` csak 10 perc futás után nullázódik. Nyugtázás, majd 10 percen belüli újraindulás (akár áramszünet) újra zárolt E10-et ad. A nyugtázásnak nullázni kell. | KÉSZ r003: a nyugtázás nullázza a `rstc` számlálót |
| T4 | P2 | Cél a hőfokhatár fölött | A célhőmérséklet a kijelzőn a védelmi határig (75 °C) léptethető, a források `maxTempC`-je alapból 65 °C. A víz elérheti a hatást, a termosztát mégis `Heating`-ben marad, a kijelzőn "Nincs halozat" látszik. Kell: a léptetés clampelése a legmagasabb engedélyezett forrás-hőfokhatárra, és külön állapotszöveg, ha minden jelen lévő forrás hőfokhatáron van. A `validateConfig` nem módosítandó (hibás konfig = teljes visszaállás, E12). | KÉSZ r003: a léptetés a legmagasabb engedélyezett forrás-hőfokhatárnál megáll; új állapotszöveg: "Forras hofokhatar" |
| T5 | P3 | Eldobott parancsok jelzése | A `xQueueSend(..., 0)` hívások eredménye nincs ellenőrizve, tele sor esetén a nyugtázás vagy módváltás némán elvész. Kell egy esemény a naplóba. | KÉSZ r003: `postCmd()`, eldobott parancsról "parancs eldobva" esemény |
| T6 | P3 | Naplóoldal `Valt 0>1` | Induláskor a `from = -1` miatt "0" jelenik meg. | KÉSZ r003 |
| T7 | P3 | `pins.h` ellentmondás | A megjegyzés szerint a GPIO48 kimarad, de a `TFT_MISO = 48`. A kijelzőnek nincs MISO-ja, a láb csak helyfoglaló; a megjegyzést pontosítani kell. | KÉSZ r003 |
| T8 | P3 | Stack-méretek mérése | A control és display task stackje 6144 byte, a `Config` kb. 1 KB, a `snprintf("%f")` is sokat használ. Kell egy szabad-stack (high-water mark) kiírás. | KÉSZ r003: 60 s-onként ellenőriz, csak új minimumnál jelez (control: napló-esemény, display: soros konzol) |

### 8.2 Döntést igénylő pontok

| # | Téma | Kérdés | Állapot |
|---|---|---|---|
| T9 | Érvénytelen hőmérő (**D15**) | **Eldöntve:** a fűtés a bojler beépített termosztátjával tovább megy (nincs időkorlát, nincs automatikus lekapcsolás), de a kijelző főoldalán E01-nél nagy piros `?`, "HOMERO HIBA", "Vizhofok ismeretlen / Beepitett termosztat aktiv futes!" és villogó "VIGYAZAT! FORRO VIZ!" jelenik meg. KÉSZ r004 (kijelzőn hardveren nem próbálva). | döntve, KÉSZ r004 |
| T10 | Alapértelmezett jelszavak | **Eldöntve:** marad, mert a repó hamarosan újra privát lesz. A firmware nem változott. Ha az eszköz bárhol elérhetővé válik, az OTA-, web- és AP-jelszót cserélni kell (a `defaultCreds` jelzés ezt mutatja). | döntve, nincs változás |
| T11 | Maradvány fájlok | **Eldöntve és KÉSZ r004:** törölve a `lib/Storage/ConfigStore.cppold` és a `data/www/index.htmlold` (utóbbi azonos volt az `index.html`-lel, a kódban nem hivatkozott rájuk senki). A `dashboard.html`, `dashboard_v2.html`, `dashboard_v3.html` és az `index.html` **marad**: a `scripts/embed_ui.py` beágyazza őket, a firmware kiszolgálja (`/`, `/dash`, `/dash2`, `/dash3`). | KÉSZ r004 |

### 8.3 Dokumentáció és ellenőrzés

| # | Teendő | Állapot |
|---|---|---|
| T12 | A `PROJECT.md` 2., 2.1 és 3. fejezetét, valamint a README-t és a rendszertervet a működő állapotra frissíteni (mi fordul, mi fut hardveren, mi igazolt). | nyitott |
| T13 | Hardveren ellenőrizni az r003-at: a mentés kijelzőről és weben (T1), a stack-értékek (T8), az elavult-szenzor jelzés (T2: a DS18B20 lehúzása és a task lefagyasztása más eset!), a hőmérő-hiba képernyő (T9: DS18B20 lehúzása, a fedett kijelzőn a szöveg olvashatósága), `pio run` és `pio test -e native` eredménye. | nyitott |
