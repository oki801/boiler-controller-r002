# ESP32 bojlervezérlő – 3 hálózatos forrásválasztó

> **Állapot:** ez egy megírt, de **nem kipróbált** első változat (a forráskód fordító-ellenőrzésen átment, de a PlatformIO-s build, a feltöltés és a futtatás még nem történt meg).
> A teljes állapot, a kockázatok és a döntési pontok: [`PROJECT.md`](PROJECT.md).

A projekt a rendszerterv szoftveres része: PlatformIO + Arduino framework, ESP32-S3 (WROOM-1 N16R8: 16 MB flash, 8 MB PSRAM).
Három külön 230 V-os hálózat közül választ egyet a közös bojlerre, hardveres és szoftveres reteszeléssel.

## Állapot

| Rész | Állapot |
|---|---|
| `Core`, `Control` (szelektor, reteszelés, állapotgép, biztonsági felügyelet) | kész, **88 PC-s unit teszt zöld** |
| `Hal`, `Sensors`, `Storage`, `Comms`, `App`, webes felület | megírva, **ESP32-n még nem fordítva és nem futtatva** |
| Kijelző (`lib/Display`, 1,44" ST7735) | megírva, ESP32-n nem fordítva |
| Hardver | a terv szerint, még nincs megépítve |

Az első `pio run` valószínűleg hoz néhány fordítási hibát vagy könyvtár-verzió eltérést, ezeket az első körben javítani kell.

## Változatszámozás

Minden módosítás eggyel növeli a `REVISION` számot, és a csomag, valamint a benne lévő könyvtár neve tartalmazza: `boiler-controller-r012`.
A firmware verziója is ezt mutatja (pl. `0.2.0-r012`, látszik a kijelző beállítások oldalán és a webes rendszer fülön).
A változások a `CHANGELOG.md` fájlban vannak. Csomagolás és számnövelés egy paranccsal: `python3 scripts/release.py "rövid megjegyzés"`.

## Gyors indulás

```bash
# 1) Logika tesztelése (nincs szükség hardverre)
pio test -e native                 # vagy: scripts/run_native_tests.sh

# 2) Fordítás és feltöltés USB-n (a webes felület a firmware-ben van, nem kell uploadfs)
pio run -e esp32s3 -t upload

# 3) Később hálózaton át
OTA_PASSWORD=boiler-ota pio run -e ota -t upload
```

**Első indítás:** ha nincs WiFi beállítva, az eszköz `Bojler-XXXX` nevű hozzáférési pontot indít
(jelszó: `bojler-setup`). A webes felület a `http://192.168.4.1` címen érhető el.
Belépés: `admin` / `boiler`. **Az első dolgod a jelszó cseréje legyen** (Beállítások fül).
WiFi-n `http://<deviceId>.local` (alapból `boiler01.local`).

## Felépítés

```
lib/Core      közös típusok, Config + validáció, EventBus, ErrorTracker, EnergyAccumulator, PresenceFilter
lib/Control   SourceSelector, InterlockGuard, SwitchoverFsm, SafetyMonitor   (hardverfüggetlen, tesztelt)
lib/Hal       GpioContactorBank (a kimenetek egyetlen tulajdonosa), TimeService, Sync, Sha256Hex
lib/Sensors   TempSensor (DS18B20), PowerMeter (PZEM-004T V3), NetworkDetector (hálózat-jelenlét)
lib/Storage   ConfigStore (NVS), EnergyStore (NVS), LogService (CSV, LittleFS)
lib/Comms     WifiManager, MqttService (+ Home Assistant discovery), WebService, OtaService
lib/App       Supervisor (task-ok), SharedState
test/         Unity tesztek (natív)         data/www  webes felület
```

## Biztonsági elvek a kódban

- A kontaktor-lábakat **csak** a `GpioContactorBank` írja, és minden zárás előtt az `InterlockGuard` engedélyezi
  (egyidejűség, visszajelzés, 2 s kemény minimális holtidő, ami konfigurációval sem csökkenthető).
- A web és az MQTT csak `ICommandSink` kéréseket és konfigurációt adhat, kontaktort nem vezérelhet.
- Reset, boot és lefagyás alatt minden kimenet alacsony (nyitva), a külső watchdog és az ARM-kapcsoló is lekapcsol.
- A kritikus hibák (E03 reteszelési sértés, E05 összeheggedt kontaktor) **zárolnak**, csak kézi nyugtázással oldhatók.
- Frissítés (OTA) alatt a forrásváltás tiltva van.

## Eltérések a rendszertervtől

- **Webes felület:** a WebSocket helyett 1 s-os lekérdezés (`/api/status`) van. Egyszerűbb, és a Basic hitelesítéssel
  megbízhatóbban működik. Később cserélhető.
- **Üzemmód** (auto / kényszerített / ki) újraindítás után mindig `auto`, hogy egy elfeledett kényszerítés ne maradjon be.
- **Külső engedély** (`ext_ok`, pl. napelemes többlet) alapértelmezetten *nincs*, amíg a Home Assistant el nem küldi.
- **Az 5 perces napló-flush:** a mérési napló RAM-pufferből ír, az események azonnal.

## MQTT / Home Assistant

Alap topic: `boiler/<deviceId>/`. Telemetria 10 s-onként, állapot változáskor (retained), parancsok a `cmd/...` alatt.
A HA entitások MQTT Discovery-vel automatikusan megjelennek. Részletek: rendszerterv 13. fejezet.
Az MQTT-n át a WiFi, a jelszavak és az azonosító **nem** módosítható.

## Célhőmérséklet, hőfokhatár, garantált minimum, értesítés, vízkő-figyelés

- **Célhőmérséklet és hiszterézis:** a vezérlő maga is termosztát. A célig fűt, a célnál lekapcsol, `cél − hiszterézis` alatt újraindul (alapból 55 és 2 °C). A 0 = kikapcsolva. A bojler saját termosztátja tartalék, állítsd a célnál magasabbra.
- **Forrásonkénti hőfokhatár:** minden hálózathoz megadható, hogy meddig fűthet (pl. napelem 70 °C, drága hálózat 55 °C).
  A határ elérésekor a hálózat nem jogosult, a szelektor a következőre lép vagy lekapcsol. Hiszterézissel indul újra.
- **Garantált minimum (fürdővíz):** csak este érvényes. Alapból 19:00-ra legyen meg a 45 °C, 90 perccel előbb indul,
  23:00-ig érvényes. Napközben a hidegebb víz is megfelel. Az ablakban a "garanciánál használható" hálózatok a beosztás
  és a külső engedély nélkül is fűthetnek. Hőmérő vagy érvényes idő nélkül nem kapcsol be (vakon nem vásárol drága áramot).
- **Értesítés:** alapból KI. Csatornák: Telegram, webhook (JSON), ntfy. Alapból csak kritikus hibára küld (E03, E05),
  ismétlés-védelemmel (15 perc) és "megszűnt" üzenettel. Újrapróbál, ha nincs hálózat. A webes felületen teszt üzenet küldhető.
  A helyi jelzés (kijelző, LED, MQTT `error` topic, Home Assistant) ettől függetlenül mindig működik.
  **Internetre kerüléskor:** az eszköz csak kifelé küld, ne nyiss rá portot, távoli eléréshez VPN-t használj.
  A HTTPS jelenleg nem ellenőrzi a tanúsítványt (`setInsecure`), éles használat előtt ezt kell megerősíteni vagy helyi továbbítót használni.
- **Vízkő-figyelés:** a tiszta fűtési ciklusokból az effektív hőkapacitást (Wh/°C) és a 230 V-ra normalizált fűtőteljesítményt
  követi. Az első 8 érvényes ciklus az alap, utána az utolsó 8 mediánját hasonlítja hozzá. Küszöb: 10% (állítható).
  A jelzés E15 (vízkő/üledék gyanú) és E16 (fűtőelem teljesítmény csökken). Vízkőmentesítés után a Rendszer fülön nullázható.
  Csak lassú változásra jó, és gyanút jelez, nem diagnózist.

## Webes felületek

- `/` a teljes kezelőfelület (beállítások, napló, trend, OTA).
- `/dash` a felhasználói dashboard (`data/www/dashboard.html`), a megadott fájl **változatlanul**, jelenleg statikus bemutató.
- `/dash3` ugyanaz folyékony mozgással (víz a tárcsában, átfutó értékek, csúszó menü), a kijelzővel közös design-nyelvvel: `docs/DESIGN.md`.
- `/dash2` az adatkötött alverzió (`data/www/dashboard_v2.html`, jóváhagyott kinézet): mért hőmérséklet, célhőmérséklet csúszka, hálózatok, napló, hibák. Az eszköz nélkül (a fájl közvetlen megnyitásakor) bemutató módban fut. Lásd `docs/DASHBOARD.md`.

## LittleFS és a webes felület

A LittleFS a gyakorlatban nyűgös (formázás, partíciócímke, kép/partíció méret eltérés, áramkimaradás), ezért
**a vezérlés semmilyen kritikus funkciója nem függ tőle**:

- a konfiguráció és az energiaszámlálók az NVS-ben vannak,
- a webes felület gzip-elve a firmware-be van ágyazva (`scripts/embed_ui.py` generálja az `include/ui_html.gz.h` fájlt
  a `data/www/index.html`-ből, minden fordításkor),
- a LittleFS csak a CSV-naplót tárolja. Ha nem csatolható, a vezérlés és a web változatlanul megy, csak a napló áll le (E11).

## ESP32-S3 N16R8 megjegyzések

- **Lábak:** a 26-37 (flash és octal PSRAM), a 19/20 (USB), a 43/44 (UART0) és a strapping lábak (0, 3, 45, 46) kimaradnak.
  A teljes kiosztás az `include/pins.h` fájlban van. Nincsenek bemenet-csak lábak, minden bemeneten van belső felhúzás.
- **Partíciók:** 2 × 3 MB firmware (OTA) és kb. 10 MB LittleFS a naplónak és a webes felületnek (`partitions_16MB.csv`).
- **USB:** a soros konzol és a JTAG a beépített USB-n megy (`ARDUINO_USB_CDC_ON_BOOT=1`), külön USB-UART nem kell.
- **PSRAM:** a 24 órás előzmény (10 s-os minták, ~69 kB) a PSRAM-ban van, a webes felület "Elmúlt 24 óra" grafikonját táplálja (`/api/history`).
  Csak RAM-ban él, újraindításkor üres. A tartós napló a LittleFS-en lévő CSV. PSRAM nélkül 8 órát tárol.
- **Hardveres SPI** a kijelzőhöz (az S3 FSPI), ezért gyorsabb a frissítés, mint a szoftveres SPI-vel.

## Teendők hardver előtt

1. `pio run` – fordítási hibák és könyvtár-verziók rendbetétele.
2. Kisfeszültségű próbapad (24 V, relék a kontaktorok helyett): reteszelés, watchdog, ARM mérése.
3. A PZEM és a DS18B20 tesztpadon, majd a hálózat-jelenlét optocsatolók (impulzusszám-küszöb finomhangolása).
4. Csak ezután a 230 V-os szerelés, villanyszerelővel.

## Helyi kijelző (1,44" ST7735, 128x128)

A kinézete a webes dashboardot követi: gyűrűs hőmérő (a mért hőmérséklet, fehér jelölő a célhőmérsékletnél), állapotikon
(felfűtés, várakozás, hőfok rendben, kikapcsolva, hiba, kézi üzem, nincs használható hálózat), három hálózat jelző, és ikonos lábléc
(főoldal, napló, hibák, beállítások). Előnézeti képek: `docs/kijelzo_elonezet_foldal.png`, `docs/kijelzo_elonezet_oldalak.png`. A képek a valódi `Display.cpp`-ből készülnek, PC-n: `tools/display_preview/build.sh` (g++, Python+Pillow, az első futáskor internet az Adafruit GFX-hez és az ArduinoJson-hoz).
(a valódi rajzoló kódból PC-n renderelve).

| Kijelző | ESP32-S3 |
|---|---|
| VCC, BL | 3V3 |
| GND | GND |
| CS | GPIO10 |
| DC / RS (A0) | GPIO12 |
| RST (RESET) | GPIO14 |
| SCK / CLK (SCL) | GPIO13 |
| MOSI / SDI (SDA) | GPIO11 |

Gombok (mind GND-re kötve, belső felhúzással; az UP/DOWN nélkül is működik a kijelző, csak a beállítás marad a webre):

| Gomb | Láb | Funkció |
|---|---|---|
| PAGE | GPIO0 | rövid nyomás: következő oldal; 1,5 s: hiba nyugtázása |
| DOWN | GPIO41 | főoldalon célhőmérséklet csökkentése (10 alatt: kikapcsolva); beállításoknál hiszterézis; napló/hibák: görgetés |
| UP | GPIO42 | ugyanez növelésre (kikapcsolt állapotból az utolsó cél) |

A módosítás 2,5 másodperc után mentődik (a "MENTVE" jelzés megjelenik), így kevés az NVS-írás. 30 másodperc után a kijelző visszatér a főoldalra.
A kijelző alapfontja nem tud ékezetet és emojit: a szövegek ASCII-ben vannak, az ikonokat a kód rajzolja. Ha a kép 2-3 px-et elcsúszik,
az `initR` paramétere `INITR_BLACKTAB` legyen.
2026.11.09
működik- 2026-11-09

### Módosítva
- Kijelző: a kis szövegek már nem a 2x-es (élsimított) vászonra kerülnek, hanem a `resolve()` után a kész 128x128-as képre rajzolódnak, így élesek, nem mosódnak el.
- Kijelző: a kis szövegek új betűtípusa `u8g2_font_helvR08_te` (arányos, 8 px), amely a Latin Extended-A tartományt is tartalmazza. A betűtípus a `SMALL_FONT` makróval egy helyen cserélhető.
- Kijelző: minden felirat ékezetes magyar szöveg lett (pl. `Hőmérő hiba`, `Nincs hálózat`, `BEÁLLÍTÁSOK`, `VIGYÁZAT! FORRÓ VÍZ!`), a hőfokok mellett `°C` jelenik meg.
- Kijelző: a naplósorok és a feliratok vágása UTF-8-biztos (`utf8Trim`), nem marad félbevágott ékezetes karakter.
- Kijelző: a mentési hiba toast szövege `NEM MENTHETO` helyett `MENTÉS HIBA`.

### Hozzáadva
- Függőség: `olikraus/U8g2_for_Adafruit_GFX` (`platformio.ini` → `lib_deps`).

### Változatlan
- A nagy piros kérdőjel (E01 hőmérőhiba), a nagy hőmérsékletszám, az ikonok és a gyűrű továbbra is a 2x-es vásznon készülnek (élsimítva).
