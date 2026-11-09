// PC-s előnézet: a valódi lib/Display/Display.cpp-t futtatja csonkolt Arduino/TFT réteggel,
// és a kirajzolt képeket PPM fájlokba menti (build.sh -> render_png.py).
// Hardver nélkül az elrendezés, a szöveg szélessége és a rajzolás minősége látható; az idő- és
// sebességviszonyokat (képkocka/s) NEM mutatja.
#include "Display.h"
#include <functional>
#include <string>
#include <type_traits>

uint32_t g_millis = 0;
SerialStub Serial;
SPIClass SPI;
uint16_t g_screen[128 * 128];

struct FakeSnap : ISnapshotSource { Snapshot s; Snapshot snapshot() const override { return s; } };
struct FakeSink : ICommandSink {
  void setMode(Mode, int8_t) override {} void setExtOk(uint8_t, bool) override {}
  void ackFault() override {} void reboot() override {}
};

static void dump(const char* dir, const char* name) {
  char path[256]; snprintf(path, sizeof(path), "%s/%s.ppm", dir, name);
  FILE* f = fopen(path, "wb"); fprintf(f, "P6\n128 128\n255\n");
  for (int i = 0; i < 128 * 128; ++i) {
    const uint16_t p = g_screen[i];
    const uint8_t r = (p >> 11) & 31, g = (p >> 5) & 63, b = p & 31;
    fputc((r << 3) | (r >> 2), f); fputc((g << 2) | (g >> 4), f); fputc((b << 3) | (b >> 2), f);
  }
  fclose(f);
}

int main(int argc, char** argv) {
  const char* dir = argc > 1 ? argv[1] : "/tmp/preview";
  FakeSnap fs; FakeSink sink; ConfigStore store; TimeService ts;
  Display d(fs, sink, store, ts);
  d.cv_ = new std::remove_pointer<decltype(d.cv_)>::type(128, 128);
  d.prevBuf_ = (uint16_t*)calloc(128 * 128, 2);

  auto base = [&](Snapshot& s) {
    s = Snapshot();
    snprintf(s.fw, sizeof(s.fw), "0.2.0-r005");
    for (uint8_t i = 0; i < kSources; ++i) { s.present[i] = true; s.enabled[i] = true; s.eligible[i] = true; }
    s.fsm = FsmState::Active; s.active = 0; s.heat = HeatPhase::Heating; s.targetC = 55; s.hystC = 2;
    s.tempValid = true; s.tempC = 47.3f; s.wifiConnected = true; s.rssi = -61; s.mode = Mode::Auto;
  };
  auto shot = [&](const char* name, uint8_t page, std::function<void(Snapshot&, Config&)> setup, uint32_t anim = 4200) {
    Snapshot s; base(s); Config c; setup(s, c);
    fs.s = s; store.cfg = c;
    d.page_ = page; d.tempInit_ = false; d.dispTemp_ = 0; d.dispTarget_ = c.targetC; d.footX_ = 16.0f + page * 32.0f;
    d.animMs_ = 0; d.trans_ = false; d.toastUntilMs_ = 0;
    for (int i = 0; i < 80; ++i) d.step(33, s, c);              // az átfutó értékek beállnak
    d.animMs_ += anim;
    d.draw(s, c);
    dump(dir, name);
  };

  shot("01_home_heating",  Display::PgHome, [](Snapshot&, Config&) {});
  shot("02_home_hot",      Display::PgHome, [](Snapshot& s, Config&) { s.tempC = 58.4f; s.heat = HeatPhase::Reached; });
  shot("03_home_cold",     Display::PgHome, [](Snapshot& s, Config&) { s.tempC = 21.0f; s.active = 2; });
  shot("04_home_off",      Display::PgHome, [](Snapshot& s, Config& c) { c.targetC = 0; s.heat = HeatPhase::Off; s.active = -1; });
  shot("05_sensor_fault",  Display::PgHome, [](Snapshot& s, Config&) { s.tempValid = false; s.errorMask = 1u << (uint8_t)ErrorCode::E01; s.active = 1; });
  shot("06_fault",         Display::PgHome, [](Snapshot& s, Config&) { s.fsm = FsmState::Fault; s.fsmError = ErrorCode::E05; s.errorMask = 1u << (uint8_t)ErrorCode::E05; s.active = -1; });
  shot("07_waiting_nopw",  Display::PgHome, [](Snapshot& s, Config&) { s.active = -1; for (auto& p : s.present) p = false; s.heat = HeatPhase::Heating; });
  {
    const int8_t pairs[][2] = {{-1, 0}, {0, 1}, {1, 2}};
    for (auto& pr : pairs) { Event e; e.type = EventType::Switch; e.from = pr[0]; e.to = pr[1]; e.setMsg("priority"); d.pushEvent(e); }
    Event e; e.type = EventType::Error; e.code = ErrorCode::E08; e.raised = true; d.pushEvent(e);
    e.code = ErrorCode::E08; e.raised = false; d.pushEvent(e);
    Event b; b.type = EventType::System; b.setMsg("boot"); d.pushEvent(b);
  }
  shot("08_log",           Display::PgLog,      [](Snapshot&, Config&) {});
  shot("09_errors",        Display::PgErrors,   [](Snapshot& s, Config&) { s.errorMask = (1u << (uint8_t)ErrorCode::E08) | (1u << (uint8_t)ErrorCode::E09) | (1u << (uint8_t)ErrorCode::E13); });
  shot("10_settings",      Display::PgSettings, [](Snapshot&, Config& c) { c.guar.enabled = true; c.guar.minTempC = 40; c.guar.readyAtMin = 6 * 60 + 30; });
  printf("kesz: %s\n", dir);
  return 0;
}
