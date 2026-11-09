#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <EventBus.h>
#include <Snapshot.h>
#include <ICommandSink.h>
#include <ConfigStore.h>
#include <TimeService.h>
#include <Sync.h>

// Helyi kijelző (1,44" ST7735, 128x128): a webes dashboard kinézetét követi.
//   Oldalak (ikonos lábléc): főoldal (gyűrűs hőmérő, állapot, hálózatok), napló, hibák, beállítások.
//   Gombok:  PAGE (GPIO0): rövid = következő oldal, 1,5 s = hiba nyugtázása
//            UP/DOWN (GPIO42/41): főoldalon célhőmérséklet, beállítások oldalon hiszterézis, napló/hibák oldalon görgetés
// A kijelző csak olvas a pillanatképből; a beállítás módosítása a ConfigStore-on át megy (érvényesítéssel).
// Az alapfont nem tud ékezetet és emojit: a szövegek ASCII-ben vannak, az ikonok rajzoltak.
class Display {
 public:
  Display(const ISnapshotSource& snap, ICommandSink& sink, ConfigStore& cfg, const TimeService& time);
  void begin();
  static void eventThunk(const Event& e, void* ctx);     // EventBus-feliratkozó (a napló oldalhoz)

 private:
  enum Page : uint8_t { PgHome, PgLog, PgErrors, PgSettings, kPages };
  enum class Icon : uint8_t { Flame, Led, Smile, Power, Warn, Manual, Hourglass, Unknown };
  struct Entry { char hm[6]; char text[24]; uint16_t color; };
  struct Button { uint8_t pin; bool down = false, longDone = false; uint32_t since = 0, lastRep = 0; };

  static void task(void* p);
  void loop();
  void pollButtons(uint32_t now, bool& redraw);
  void onPage(bool longPress, uint32_t now);
  void onStep(int dir, uint32_t now);                    // +1 / -1
  void flushPending(uint32_t now);
  void pushEvent(const Event& e);
  void step(uint32_t dtMs, const Snapshot& s, const Config& c);   // animációs állapot léptetése (átfutó értékek, fázis)
  void startTransition();                                          // oldalváltás: az előző kép elcsúszik
  void composeSlide(float p);
  void water(const Snapshot& s, int cx, int cy, int R);

  void draw(const Snapshot& s, const Config& c);
  void pageHome(const Snapshot& s, const Config& c);
  void sensorFaultView();                          // E01: nagy piros ? és figyelmeztetés (a víz hőfoka ismeretlen)
  void pageLog();
  void pageErrors(const Snapshot& s);
  void pageSettings(const Snapshot& s, const Config& c);
  void footer(const Snapshot& s);
  void ring(int cx, int cy, int r, int th, float fillDeg, uint16_t col);
  void drawIcon(Icon k, int cx, int cy);
  void centered(const char* txt, int y, uint8_t size, uint16_t col);
  void name(const Config& c, uint8_t i, char* out, size_t n) const;

  const ISnapshotSource& snap_;
  ICommandSink& sink_;
  ConfigStore& cfg_;
  const TimeService& time_;
  Adafruit_ST7735 tft_;
  GFXcanvas16* cv_ = nullptr;

  Button btnPage_{pins_page()}, btnUp_{pins_up()}, btnDown_{pins_down()};
  static uint8_t pins_page();
  static uint8_t pins_up();
  static uint8_t pins_down();

  uint8_t  page_ = PgHome;
  // animáció
  float    dispTemp_ = 0, dispTarget_ = 55, footX_ = 16;           // a MEGJELENÍTETT (átfutó) értékek
  bool     tempInit_ = false, tweening_ = false, trans_ = false;
  uint32_t animMs_ = 0, transStart_ = 0;
  uint16_t* prevBuf_ = nullptr;                                    // az előző oldal képe a csúsztatáshoz (32 kB)
  int      scroll_ = 0;
  uint32_t lastInputMs_ = 0, toastUntilMs_ = 0, saveAtMs_ = 0;
  char     toast_[20] = "";
  float    pendTarget_ = -1, pendHyst_ = -1, lastOnTarget_ = 55;   // -1 = nincs függő módosítás

  static constexpr uint8_t kEv = 16;
  Entry   ev_[kEv];
  uint8_t evHead_ = 0, evN_ = 0;
  Mutex   evMtx_;
};
