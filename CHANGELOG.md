# Valtozasnaplo

Minden modositas eggyel noveli a REVISION szamot (a csomag neve: boiler-controller-rNNN).

- **r006** (2026-11-09): kijelzo kis szovegek elesitese: a kis betuk a resolve() utan a kesz 1x kepre rajzolodnak (nem a 2x vaszonra), U8g2 helvR08_te betu (ekezetes magyar feliratok, fokjel), UTF-8-biztos szovegvagas (utf8Trim); uj fuggoseg: olikraus/U8g2_for_Adafruit_GFX; a nagy piros ?, a nagy szamok, az ikonok es a gyuru valtozatlanok
- **r005** (2026-11-09): kijelzo elesimitas: SsCanvas 2x vaszon + lekicsinyites, sima DejaVu betuk, tortszam-pozicioju iv/viz/lang, rajzolasi ido naplo; PC-s elonezet (tools/display_preview); docs kepek ujrarenderelve
- **r004** (2026-11-09): D15: homero-hiba (E01) kepernyo a kijelzon (piros ?, forro viz figyelmeztetes); T10 OTA alapjelszo marad; ConfigStore.cppold es index.htmlold torolve; PROJECT.md T9-T11 lezarva
- **r003** (2026-11-08): kodatnezes javitasai: ConfigStore mentes (holtpont, visszaallas), elavult szenzoradat (E01), E10 szamlalo nyugtazaskor, celhomerseklet-korlat a hofokhatarhoz, eldobott parancs jelzese, stack-meres, kijelzo naplo es pins megjegyzes; PROJECT.md teendok
- **r002** (2026-11-06): LCD bekotes frissitve: CS=GPIO10, DC=GPIO12, RST=GPIO14, SCK=GPIO13, MOSI=GPIO11 (pins.h, README, rendszerterv)
- **r001** (2026-11-05): Alapallapot a szamozas bevezetesekor: 3 halozatos vezerles, celhomerseklet-termosztat, hofokhatar, garancia, ertesites, vizko-figyeles, dashboard /dash /dash2 /dash3, folyekony kijelzo-felulet
