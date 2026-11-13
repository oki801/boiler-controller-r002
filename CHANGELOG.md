# Valtozasnaplo

Minden modositas eggyel noveli a REVISION szamot (a csomag neve: boiler-controller-rNNN).

- **r013** (2026-11-13): soros naplo: kijelzo periodikus sorai torolve; indulasi infoblokk (verzio, chip, memoria, beallitasok) es valtozas-alapu sorok (esemenyek, WiFi, MQTT, ido); PROJECT.md T32, README
- **r012** (2026-11-12): wifi-valaszto a beallitasok oldalon: halozatkereses legordulo listaba, jelszo, kiprobalas mentes nelkul (WifiTrial), IP-cim a weben; PROJECT.md T30-T31, README
- **r011** (2026-11-11): fordítasi hiba megszuntetese: az r010 szervizmod-kiserlet 3 utkozo fajla torolve (ServiceBank.h, ServiceMode.h, IServiceControl.h); tesztek SimBank/ServiceSim/FSM (99 teszt); LCD szervizmod-sav es nyers S1 pin; PROJECT.md T25-T29
- **r010D** (2026-11-10): szervizmod (szarazfutas): szimulalt kontaktorok es szimulalt bemenetek (S1, halozat-jelenlet, homerseklet), hibainjektalas (E04/E05), /service oldal es figyelmezteto sav; belepes/kilepes ujrainditassal, idokorlat 1-30 perc, ujrainditas/aramszunet utan nem marad bent; szervizmodban nincs energia-, elozmeny- es trendmentes, nincs kulso ertesites; a "Kezi uzem" uzenet a GPIO21 lab allasat is jelzi; scripts/version.py es release.py a szam utani betut (D) is kezeli
- **r009** (2026-11-10): kijelzo-task stack 6144 -> 8192 (mert szabad stack min 293 bajt volt); ismeretlen reset ok neve; PROJECT.md T21-T22
- **r008** (2026-11-10): indulasi naplo rendbetetele: 64 kB coredump particio, hianyzo NVS-kulcsok isKey vedelme (trend, energia, konfig), minden indulas okanak naplozasa; PROJECT.md T18-T20, README
- **r007** (2026-11-09): az eszkoz IP-cime a kijelzon: beallitasok oldal sora + 8 s-os felugro uzenet WiFi-csatlakozaskor (Snapshot.ip, WifiManager::ip32); PC-s elonezet U8g2-vel; PROJECT.md T16-T17, README
- **r006** (2026-11-09): kijelzo kis szovegek elesitese: a kis betuk a resolve() utan a kesz 1x kepre rajzolodnak (nem a 2x vaszonra), U8g2 helvR08_te betu (ekezetes magyar feliratok, fokjel), UTF-8-biztos szovegvagas (utf8Trim); uj fuggoseg: olikraus/U8g2_for_Adafruit_GFX; a nagy piros ?, a nagy szamok, az ikonok es a gyuru valtozatlanok
- **r005** (2026-11-09): kijelzo elesimitas: SsCanvas 2x vaszon + lekicsinyites, sima DejaVu betuk, tortszam-pozicioju iv/viz/lang, rajzolasi ido naplo; PC-s elonezet (tools/display_preview); docs kepek ujrarenderelve
- **r004** (2026-11-09): D15: homero-hiba (E01) kepernyo a kijelzon (piros ?, forro viz figyelmeztetes); T10 OTA alapjelszo marad; ConfigStore.cppold es index.htmlold torolve; PROJECT.md T9-T11 lezarva
- **r003** (2026-11-08): kodatnezes javitasai: ConfigStore mentes (holtpont, visszaallas), elavult szenzoradat (E01), E10 szamlalo nyugtazaskor, celhomerseklet-korlat a hofokhatarhoz, eldobott parancs jelzese, stack-meres, kijelzo naplo es pins megjegyzes; PROJECT.md teendok
- **r002** (2026-11-06): LCD bekotes frissitve: CS=GPIO10, DC=GPIO12, RST=GPIO14, SCK=GPIO13, MOSI=GPIO11 (pins.h, README, rendszerterv)
- **r001** (2026-11-05): Alapallapot a szamozas bevezetesekor: 3 halozatos vezerles, celhomerseklet-termosztat, hofokhatar, garancia, ertesites, vizko-figyeles, dashboard /dash /dash2 /dash3, folyekony kijelzo-felulet
