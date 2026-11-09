#include "Display.h"
#include <pins.h>
#include <ArduinoJson.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>   // lib_deps: olikraus/U8g2_for_Adafruit_GFX

// A kis (1-es méretű) szövegek betűtípusa. Egy sorban cserélhető:
// Az _te végű változatok a Latin Extended-A tartományt is tartalmazzák (ő, ű, é, á ...).
//   u8g2_font_helvR08_te   - arányos, 8 px, jól olvasható (alap)
//   u8g2_font_helvB08_te   - ugyanez félkövéren
//   u8g2_font_helvR10_te   - nagyobb (10 px), kevesebb szöveg fér egy sorba
#define SMALL_FONT u8g2_font_helvR08_te

namespace {
constexpr int W = 128, H = 128;
constexpr uint16_t c565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
constexpr uint16_t kBlack = 0x0000, kWhite = 0xFFFF;
constexpr uint16_t kGray = c565(138, 150, 160), kDark = c565(37, 49, 66), kPanel = c565(17, 24, 39);
constexpr uint16_t kOk = c565(34, 197, 94), kBad = c565(239, 68, 68), kWarn = c565(250, 204, 21), kOrange = c565(249, 115, 22);
const uint16_t kSrc[kSources] = {c565(250, 204, 21), c565(96, 165, 250), c565(217, 70, 239)};    // mint a webes felületen
constexpr float kScale = 60.0f;            // a dashboard gyűrűje 0-60 °C
constexpr int kFooterY = 118;

uint16_t hsl565(float h, float s, float l) {                // a dashboard colorFromTemp-jához
  const float c = (1 - fabsf(2 * l - 1)) * s, hp = h / 60.0f;
  const float x = c * (1 - fabsf(fmodf(hp, 2.0f) - 1));
  float r = 0, g = 0, b = 0;
  if (hp < 1) { r = c; g = x; } else if (hp < 2) { r = x; g = c; } else if (hp < 3) { g = c; b = x; }
  else if (hp < 4) { g = x; b = c; } else if (hp < 5) { r = x; b = c; } else { r = c; b = x; }
  const float m = l - c / 2;
  return c565((uint8_t)((r + m) * 255), (uint8_t)((g + m) * 255), (uint8_t)((b + m) * 255));
}
// HSL -> RGB (0-255); a "víz" áttetsző keveréséhez a webes dashboardhoz hasonlóan
void hslRgb(float h, float s, float l, uint8_t& R, uint8_t& G, uint8_t& B) {
  const float c = (1 - fabsf(2 * l - 1)) * s, hp = h / 60.0f;
  const float x = c * (1 - fabsf(fmodf(hp, 2.0f) - 1));
  float r = 0, g = 0, b = 0;
  if (hp < 1) { r = c; g = x; } else if (hp < 2) { r = x; g = c; } else if (hp < 3) { g = c; b = x; }
  else if (hp < 4) { g = x; b = c; } else if (hp < 5) { r = x; b = c; } else { r = c; b = x; }
  const float m = l - c / 2;
  R = (uint8_t)((r + m) * 255); G = (uint8_t)((g + m) * 255); B = (uint8_t)((b + m) * 255);
}
// a szín `a` arányban keverve a sötét tárcsa-háttérre (17, 24, 39)
uint16_t overNavy(uint8_t r, uint8_t g, uint8_t b, float a) {
  return c565((uint8_t)(17 + (r - 17) * a), (uint8_t)(24 + (g - 24) * a), (uint8_t)(39 + (b - 39) * a));
}
uint16_t colorFromTemp(float t) { float h = 220.0f - (t / kScale) * 220.0f; if (h < 0) h = 0; return hsl565(h, 0.9f, 0.55f); }

const char* shortErr(ErrorCode c) {
  switch (c) {
    case ErrorCode::E01: return "Hőmérő hiba";
    case ErrorCode::E02: return "PZEM nem válaszol";
    case ErrorCode::E03: return "Reteszelés hiba";
    case ErrorCode::E04: return "Nem húz be";
    case ErrorCode::E05: return "Beragadt kontakt";
    case ErrorCode::E06: return "Feszültség hibás";
    case ErrorCode::E07: return "Túlhőmérséklet";
    case ErrorCode::E08: return "WiFi/MQTT kiesett";
    case ErrorCode::E09: return "Nincs pontos idő";
    case ErrorCode::E10: return "Gyakori reset";
    case ErrorCode::E11: return "Tárhely baj";
    case ErrorCode::E12: return "Konfig hibás";
    case ErrorCode::E13: return "Sok kapcsolás";
    case ErrorCode::E14: return "Nincs hálózat";
    case ErrorCode::E15: return "Vízkő gyanú";
    case ErrorCode::E16: return "Fűtőelem gyenge";
    default:             return "";
  }
}
float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// ------------------------------------------------------------------ éles kis szöveg
// A kis szövegeket NEM a 2x-es (élsimított) vászonra rajzoljuk, mert a 2x -> 1x lekicsinyítés
// (resolve) a vékony, 1 pixeles betűvonalakat elmossa. Ehelyett a rajzolás közben csak
// "sorba állítjuk" őket, és a resolve UTÁN, a kész 1x-es képre rajzoljuk, pixelpontosan.
// A nagy számok és a nagy piros kérdőjel továbbra is a vásznon marad (élsimítva).
class BufGfx : public Adafruit_GFX {            // Adafruit_GFX a kész 128x128 RGB565 pufferre
 public:
  BufGfx() : Adafruit_GFX(W, H) {}
  void bind(uint16_t* b) { buf_ = b; }
  void drawPixel(int16_t x, int16_t y, uint16_t c) override {
    if (buf_ && x >= 0 && y >= 0 && x < W && y < H) buf_[y * W + x] = c;
  }
 private:
  uint16_t* buf_ = nullptr;
};

struct TextItem { int16_t x, y; uint16_t col; bool top; char s[32]; };
constexpr int kMaxText = 48;
TextItem gText[kMaxText];
int gTextN = 0;
BufGfx gGfx;
U8G2_FOR_ADAFRUIT_GFX gU8;
bool gU8Ready = false;

void u8Init() {
  if (gU8Ready) return;
  gU8.begin(gGfx);
  gU8.setFontMode(1);                            // átlátszó háttér
  gU8.setFontDirection(0);
  gU8.setFont(SMALL_FONT);
  gU8Ready = true;
}
int textWidth(const char* s) { u8Init(); return gU8.getUTF8Width(s); }
// UTF-8-biztos vágás: legfeljebb maxBytes bájt, és nem hagy félbevágott ékezetes karaktert a végén
void utf8Trim(char* s, size_t maxBytes) {
  size_t n = strlen(s);
  if (n > maxBytes) { n = maxBytes; s[n] = 0; }
  size_t i = n;
  while (i > 0 && ((uint8_t)s[i - 1] & 0xC0) == 0x80) --i;           // a záró folytatóbájtok átlépése
  if (i > 0) {
    const uint8_t lead = (uint8_t)s[i - 1];
    const size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
    if (n - (i - 1) < need) s[i - 1] = 0;                            // befejezetlen karakter: eldobjuk
  }
}
// y = a nagybetűk felső széle (mint az eddigi setCursor y-ja); top = a "MENTVE" üzenet, ez mindig látszik
void queueText(int x, int y, const char* s, uint16_t col, bool top = false) {
  if (gTextN >= kMaxText) return;
  TextItem& t = gText[gTextN++];
  t.x = (int16_t)x; t.y = (int16_t)y; t.col = col; t.top = top;
  strlcpy(t.s, s, sizeof(t.s));
  utf8Trim(t.s, sizeof(t.s) - 1);
}
void flushText(uint16_t* buf, bool toast) {
  u8Init();
  gGfx.bind(buf);
  gU8.setFont(SMALL_FONT);
  gU8.setFontMode(1);
  const int asc = gU8.getFontAscent();
  for (int i = 0; i < gTextN; ++i) {
    const TextItem& t = gText[i];
    if (toast && !t.top) {                       // az üzenet-doboz (14,46 - 114,72) alatti szöveg ne látsszon át
      const int w = gU8.getUTF8Width(t.s);
      if (t.x < 114 && t.x + w > 14 && t.y < 72 && t.y + asc + 2 > 46) continue;
    }
    gU8.setForegroundColor(t.col);
    gU8.drawUTF8(t.x, t.y + asc, t.s);           // a U8g2 az alapvonalat várja
  }
  gTextN = 0;
}
}  // namespace

uint8_t Display::pins_page() { return pins::BTN; }
uint8_t Display::pins_up()   { return pins::BTN_UP; }
uint8_t Display::pins_down() { return pins::BTN_DOWN; }

Display::Display(const ISnapshotSource& snap, ICommandSink& sink, ConfigStore& cfg, const TimeService& time)
    : snap_(snap), sink_(sink), cfg_(cfg), time_(time), tft_(pins::TFT_CS, pins::TFT_DC, pins::TFT_RST) {}

void Display::begin() {
  pinMode(pins::BTN, INPUT_PULLUP);
  pinMode(pins::BTN_UP, INPUT_PULLUP);        // nem kötött gomb = magas = nincs nyomás (a kijelző gombok nélkül is használható)
  pinMode(pins::BTN_DOWN, INPUT_PULLUP);
  SPI.begin(pins::TFT_SCK, pins::TFT_MISO, pins::TFT_MOSI, pins::TFT_CS);   // hardveres SPI; a MISO-nak szabad láb kell (lásd pins.h)
  tft_.initR(INITR_144GREENTAB);              // ha a kép 2-3 px-et elcsúszik: INITR_BLACKTAB
  tft_.setRotation(0);
  tft_.fillScreen(kBlack);
  cv_ = new SsCanvas(W, H);                   // 2x felbontású vászon (128 kB, PSRAM) + 32 kB kész kép; élsimításhoz
  if (!cv_ || !cv_->ok()) { Serial.printf("[display] nincs eleg memoria a vaszonhoz\n"); delete cv_; cv_ = nullptr; return; }
  prevBuf_ = static_cast<uint16_t*>(malloc(W * H * 2));              // ha nincs memória: nincs csúsztatás, a többi működik
  const Config c = cfg_.get();
  if (c.targetC > 0) lastOnTarget_ = c.targetC;
  xTaskCreatePinnedToCore(task, "display", 6144, this, 1, nullptr, 0);   // a vezérlő task-ok nem várnak rá
}

void Display::task(void* p) { static_cast<Display*>(p)->loop(); }

// ------------------------------------------------------------------ naplóesemények
void Display::eventThunk(const Event& e, void* ctx) { static_cast<Display*>(ctx)->pushEvent(e); }

void Display::pushEvent(const Event& e) {
  if (e.type == EventType::State) return;                      // túl beszédes
  Entry n = {};
  if (time_.isValid()) { const uint16_t m = time_.minuteOfDay(); snprintf(n.hm, sizeof(n.hm), "%02u:%02u", m / 60, m % 60); }
  else strlcpy(n.hm, "--:--", sizeof(n.hm));
  n.color = kWhite;
  switch (e.type) {
    case EventType::Switch:  snprintf(n.text, sizeof(n.text), "Vált %c>%d %s", e.from >= 0 ? (char)('1' + e.from) : '-', e.to + 1, !strcmp(e.msg, "priority") ? "prio" : e.msg); n.color = kOk; break;
    case EventType::Error:   snprintf(n.text, sizeof(n.text), "%s %s", errorInfo(e.code).code, e.raised ? "fellépett" : "megszűnt");
                             n.color = e.raised ? (errorInfo(e.code).sev >= Severity::Error ? kBad : kWarn) : kGray; break;
    case EventType::Command: snprintf(n.text, sizeof(n.text), "Parancs"); n.color = kGray; break;
    default:
      if (!strncmp(e.msg, "garantalt", 9)) snprintf(n.text, sizeof(n.text), "Garancia %s", strstr(e.msg, "be") ? "be" : "ki");
      else if (!strncmp(e.msg, "boot", 4)) snprintf(n.text, sizeof(n.text), "Indulás");
      else snprintf(n.text, sizeof(n.text), "%s", e.msg);
      n.color = kGray; break;
  }
  utf8Trim(n.text, 15);                                            // a sorba (21 karakter - idő) ennyi fér; UTF-8-biztos vágás
  Lock l(evMtx_);
  ev_[evHead_] = n;
  evHead_ = (evHead_ + 1) % kEv;
  if (evN_ < kEv) evN_++;
}

// ------------------------------------------------------------------ gombok
void Display::pollButtons(uint32_t now, bool& redraw) {
  const bool pg = digitalRead(btnPage_.pin) == LOW;
  if (pg && !btnPage_.down) { btnPage_.down = true; btnPage_.since = now; btnPage_.longDone = false; }
  if (pg && btnPage_.down && !btnPage_.longDone && (uint32_t)(now - btnPage_.since) >= 1500) { btnPage_.longDone = true; onPage(true, now); redraw = true; }
  if (!pg && btnPage_.down) {
    btnPage_.down = false;
    if (!btnPage_.longDone && (uint32_t)(now - btnPage_.since) >= 40) { onPage(false, now); redraw = true; }
  }
  Button* b[2] = {&btnUp_, &btnDown_};
  const int dir[2] = {+1, -1};
  for (int i = 0; i < 2; ++i) {
    const bool d = digitalRead(b[i]->pin) == LOW;
    if (d && !b[i]->down) { b[i]->down = true; b[i]->since = now; b[i]->lastRep = now; onStep(dir[i], now); redraw = true; }
    else if (d && b[i]->down && (uint32_t)(now - b[i]->since) > 500 && (uint32_t)(now - b[i]->lastRep) >= 120) {
      b[i]->lastRep = now; onStep(dir[i], now); redraw = true;                  // nyomva tartva ismétel
    }
    if (!d) b[i]->down = false;
  }
}

void Display::onPage(bool longPress, uint32_t now) {
  lastInputMs_ = now;
  if (longPress) { sink_.ackFault(); strlcpy(toast_, "NYUGTÁZVA", sizeof(toast_)); toastUntilMs_ = animMs_ + 1500; return; }
  startTransition();
  page_ = (page_ + 1) % kPages;
  scroll_ = 0;
}

void Display::onStep(int dir, uint32_t now) {
  lastInputMs_ = now;
  const Config c = cfg_.get();
  if (page_ == PgHome) {
    float t = pendTarget_ >= 0 ? pendTarget_ : c.targetC;
    if (t <= 0)      t = dir > 0 ? lastOnTarget_ : 0;                           // ki -> utolsó bekapcsolt cél
    else if (dir < 0 && t <= 10) t = 0;                                         // 10 alatt kikapcsolva
    else {
      float hi = c.safety.tMaxC;                                                 // a legmagasabb engedélyezett forrás-hőfokhatár felett
      float capMax = 0;                                                          // a víz úgysem melegszik tovább
      for (uint8_t i = 0; i < kSources; ++i) if (c.src[i].enabled && c.src[i].maxTempC > capMax) capMax = c.src[i].maxTempC;
      if (capMax >= 10.0f && capMax < hi) hi = capMax;
      float nt = t + dir;
      if (dir > 0 && nt > hi) nt = (t > hi) ? t : hi;                            // növelés nem lépi át; a már magasabb cél marad
      t = clampf(nt, 10.0f, c.safety.tMaxC);
    }
    pendTarget_ = t;
    saveAtMs_ = now + 2500;                                                     // késleltetett mentés: kevesebb NVS-írás
  } else if (page_ == PgSettings) {
    float h = pendHyst_ >= 0 ? pendHyst_ : c.hystC;
    pendHyst_ = clampf(h + 0.5f * dir, 0.5f, 10.0f);
    saveAtMs_ = now + 2500;
  } else {
    scroll_ += (dir < 0) ? 1 : -1;                                              // DOWN: régebbi, UP: újabb
    if (scroll_ < 0) scroll_ = 0;
  }
}

void Display::flushPending(uint32_t now) {
  if ((pendTarget_ < 0 && pendHyst_ < 0) || (int32_t)(now - saveAtMs_) < 0) return;
  JsonDocument d;
  if (pendTarget_ >= 0) d["target"] = pendTarget_;
  if (pendHyst_ >= 0)   d["hyst"] = pendHyst_;
  char err[64] = "";
  const bool ok = cfg_.patch(d.as<JsonVariantConst>(), err, sizeof(err));
  if (ok && pendTarget_ > 0) lastOnTarget_ = pendTarget_;
  strlcpy(toast_, ok ? "MENTVE" : "MENTÉS HIBA", sizeof(toast_));
  toastUntilMs_ = animMs_ + 1500;
  pendTarget_ = -1; pendHyst_ = -1;
}

void Display::loop() {
  bool redraw = true;
  uint32_t lastDrawMs = 0, lastStepMs = millis(), lastStackChkMs = 0, maxDrawMs = 0;
  UBaseType_t minStackFree = (UBaseType_t)-1;
  for (;;) {
    const uint32_t now = millis();
    if ((uint32_t)(now - lastStackChkMs) >= 60000) {                      // szabad stack: csak új minimumnál ír
      lastStackChkMs = now;
      const UBaseType_t fr = uxTaskGetStackHighWaterMark(nullptr);
      if (fr < minStackFree) { minStackFree = fr; Serial.printf("[stack] display szabad min: %u bajt\n", (unsigned)fr); }
      Serial.printf("[display] rajzolas max %u ms az elmult percben\n", (unsigned)maxDrawMs);   // 66 ms alatt marad a 15 fps
      maxDrawMs = 0;
    }
    pollButtons(now, redraw);
    flushPending(now);
    if (page_ != PgHome && (uint32_t)(now - lastInputMs_) > 30000) { startTransition(); page_ = PgHome; scroll_ = 0; redraw = true; }   // visszatér a főoldalra
    // folyékony mozgás: 30 fps mozgás/bevitel közben, 15 fps a főoldalon (lassú hullám), 2 fps a statikus oldalakon
    const bool busy = trans_ || tweening_ || (uint32_t)(now - lastInputMs_) < 1500;
    const uint32_t period = busy ? 33 : (page_ == PgHome ? 66 : 500);
    if (redraw || (uint32_t)(now - lastDrawMs) >= period) {
      const Snapshot snap = snap_.snapshot();
      const Config cfg = cfg_.get();
      step(now - lastStepMs, snap, cfg);
      lastStepMs = now; lastDrawMs = now; redraw = false;
      draw(snap, cfg);
      const uint32_t took = millis() - now;
      if (took > maxDrawMs) maxDrawMs = took;
    }
    vTaskDelay(pdMS_TO_TICKS(8));
  }
}

void Display::step(uint32_t dt, const Snapshot& s, const Config& c) {
  if (dt > 100) dt = 100;
  animMs_ += dt;
  const float tTarget = s.tempValid ? s.tempC : dispTemp_;
  if (!tempInit_ && s.tempValid) { dispTemp_ = tTarget > 8.0f ? tTarget - 8.0f : 0.0f; tempInit_ = true; }   // az első adatnál "feltölt"
  const float kT = 1.0f - expf(-(float)dt / 450.0f), kG = 1.0f - expf(-(float)dt / 160.0f), kF = 1.0f - expf(-(float)dt / 120.0f);
  dispTemp_ += (tTarget - dispTemp_) * kT;
  const float tgt = pendTarget_ >= 0 ? pendTarget_ : c.targetC;
  dispTarget_ += (tgt - dispTarget_) * kG;
  const float footGoal = 16.0f + page_ * 32.0f;
  footX_ += (footGoal - footX_) * kF;
  tweening_ = fabsf(tTarget - dispTemp_) > 0.05f || fabsf(tgt - dispTarget_) > 0.05f || fabsf(footGoal - footX_) > 0.5f;
}

void Display::startTransition() {
  if (!prevBuf_ || !cv_) return;
  memcpy(prevBuf_, cv_->getBuffer(), (size_t)W * H * 2);          // az utolsó kirajzolt kép
  trans_ = true;
  transStart_ = animMs_;
}

// Az előző kép balra kicsúszik, az új jobbról becsúszik (kiszáradó, lassuló mozgás).
void Display::composeSlide(float p) {
  const float e = 1.0f - (1.0f - p) * (1.0f - p) * (1.0f - p);
  const int off = (int)lroundf(e * W);
  static uint16_t row[W];
  const uint16_t* nb = cv_->getBuffer();
  for (int y = 0; y < H; ++y) {
    const uint16_t* pr = prevBuf_ + y * W;
    const uint16_t* nr = nb + y * W;
    for (int x = 0; x < W - off; ++x) row[x] = pr[x + off];
    for (int x = W - off; x < W; ++x) row[x] = nr[x - (W - off)];
    tft_.drawRGBBitmap(0, y, row, W, 1);
  }
}

// ------------------------------------------------------------------ rajzolás
void Display::name(const Config& c, uint8_t i, char* out, size_t n) const {
  size_t k = 0;
  for (const char* p = c.srcName[i]; *p && k + 1 < n; ++p) if ((uint8_t)*p < 128 && (uint8_t)*p >= 32) out[k++] = *p;
  out[k] = 0;
  if (!k) snprintf(out, n, "Halozat %d", i + 1);
}

void Display::centered(const char* txt, int y, uint8_t size, uint16_t col) {
  if (size == 1) {                                                  // kis szöveg: éles, a resolve után rajzolódik
    queueText((W - textWidth(txt)) / 2, y, txt, col);
    return;
  }
  int16_t x1, y1; uint16_t w, h;                                    // nagy szöveg (pl. a piros kérdőjel): marad a vásznon
  cv_->setTextSize(size);
  cv_->getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  cv_->setTextColor(col);
  cv_->setCursor((W - (int)w) / 2, y);
  cv_->print(txt);
}

void Display::ring(int cx, int cy, int r, int th, float fillDeg, uint16_t col) {
  // 12 óránál indul, óramutató járásával, mint a dashboard. Az ívet kis körök sora adja, törtszám-pozícióval
  // a nagy felbontású vásznon, így a szélek simák; a kitöltött szakasz színe a fillDeg-ig tart.
  for (float a = 0.0f; a < 360.0f; a += 1.5f) {
    const float rad = a * 0.0174533f;
    cv_->fillCircleF(cx + r * sinf(rad), cy - r * cosf(rad), th * 0.5f, a <= fillDeg ? col : kDark);
  }
}

void Display::drawIcon(Icon k, int cx, int cy) {
  switch (k) {
    case Icon::Flame:                                          // ♨ felfűtés
    {
      const float sway = sinf(animMs_ * 0.0048f);                                     // lassú ringás
      const float tipO = cy - 9 + 2.2f * sinf(animMs_ * 0.0115f);                     // villódzó csúcs
      const float tipI = cy - 3 + 1.6f * sinf(animMs_ * 0.0170f + 1.0f);
      cv_->fillCircle(cx, cy + 3, 6, kOrange);
      cv_->fillTriangleF(cx - 6, cy + 2, cx + 6, cy + 2, cx + sway, tipO, kOrange);
      cv_->fillCircle(cx, cy + 4, 3, kWarn);
      cv_->fillTriangleF(cx - 3, cy + 3, cx + 3, cy + 3, cx + sway, tipI, kWarn);
    }
      break;
    case Icon::Led:                                            // várakozás: zöld LED
    {
      const float pulse = 0.5f + 0.5f * sinf(animMs_ * 0.0031f);                        // lassú pulzálás
      cv_->drawCircle(cx, cy, 9 + (pulse > 0.6f ? 1 : 0), c565(0, (uint8_t)(60 + 70 * pulse), 40));
      cv_->drawCircle(cx, cy, 8, c565(0, (uint8_t)(40 + 70 * pulse), 30));
      cv_->fillCircle(cx, cy, 6, kOk);
      cv_->fillCircle(cx - 2, cy - 2, 2, c565(216, 255, 216));
    }
      break;
    case Icon::Smile:                                          // hőfok rendben
      cv_->fillCircle(cx, cy, 8, kWarn);
      cv_->fillCircle(cx - 3, cy - 2, 1, kBlack);
      cv_->fillCircle(cx + 3, cy - 2, 1, kBlack);
      for (int a = 25; a <= 155; a += 10) { const float r = a * 0.0174533f; cv_->fillCircleF(cx + 4.5f * cosf(r), cy + 1 + 4.0f * sinf(r), 0.7f, kBlack); }
      break;
    case Icon::Power:                                          // kikapcsolva
      for (int a = 40; a <= 320; a += 4) { const float r = a * 0.0174533f; cv_->fillCircleF(cx + 7 * sinf(r), cy - 7 * cosf(r), 1.0f, kGray); }
      cv_->fillRect(cx - 1, cy - 9, 3, 9, kGray);
      break;
    case Icon::Warn:                                           // hiba
      cv_->fillTriangle(cx, cy - 9, cx - 9, cy + 7, cx + 9, cy + 7, kBad);
      cv_->fillRect(cx - 1, cy - 3, 2, 6, kWhite);
      cv_->fillRect(cx - 1, cy + 4, 2, 2, kWhite);
      break;
    case Icon::Manual:                                         // kézi üzem (S1)
      cv_->drawRoundRect(cx - 10, cy - 7, 20, 14, 3, kWhite);
      queueText(cx - textWidth("S1") / 2, cy - 4, "S1", kWhite);
      break;
    case Icon::Hourglass:                                      // fűtene, de nincs használható hálózat
      cv_->fillTriangle(cx - 6, cy - 8, cx + 6, cy - 8, cx, cy, kWarn);
      cv_->fillTriangle(cx - 6, cy + 8, cx + 6, cy + 8, cx, cy, kWarn);
      cv_->drawFastHLine(cx - 7, cy - 9, 15, kWhite); cv_->drawFastHLine(cx - 7, cy + 9, 15, kWhite);
      break;
    default:
      cv_->setTextSize(2); cv_->setTextColor(kGray); cv_->setCursor(cx - 5, cy - 7); cv_->print("?");
  }
}

void Display::footer(const Snapshot& s) {
  cv_->drawFastHLine(0, kFooterY - 3, W, kDark);
  for (uint8_t i = 0; i < kPages; ++i) {
    const int cx = 16 + i * 32, cy = kFooterY + 4;
    const uint16_t col = (page_ == i) ? kWhite : kGray;
    switch (i) {
      case PgHome:  cv_->fillTriangle(cx - 6, cy, cx + 6, cy, cx, cy - 6, col); cv_->fillRect(cx - 4, cy, 9, 5, col); break;
      case PgLog:   for (int k = 0; k < 3; ++k) cv_->drawFastHLine(cx - 6, cy - 4 + k * 4, 13, col); break;
      case PgErrors: cv_->drawTriangle(cx, cy - 6, cx - 7, cy + 5, cx + 7, cy + 5, col); cv_->drawPixel(cx, cy - 1, col); cv_->drawFastVLine(cx, cy - 3, 4, col);
                     if (s.errorMask) cv_->fillCircle(cx + 7, cy - 5, 3, kBad); break;
      case PgSettings: cv_->drawCircle(cx, cy, 5, col); cv_->fillCircle(cx, cy, 1, col);
                     cv_->drawFastHLine(cx - 8, cy, 3, col); cv_->drawFastHLine(cx + 6, cy, 3, col);
                     cv_->drawFastVLine(cx, cy - 8, 3, col); cv_->drawFastVLine(cx, cy + 6, 3, col); break;
    }
  }
  cv_->fillRoundRect((int)lroundf(footX_) - 9, kFooterY + 10, 18, 2, 1, kWhite);          // csúszó kiemelés
}

void Display::draw(const Snapshot& s, const Config& c) {
  cv_->fillScreen(kBlack);
  cv_->setTextWrap(false);
  gTextN = 0;                                                       // a kis szövegek sorát minden képkockánál újraépítjük
  switch (page_) {
    case PgHome:     pageHome(s, c); break;
    case PgLog:      pageLog(); break;
    case PgErrors:   pageErrors(s); break;
    default:         pageSettings(s, c); break;
  }
  footer(s);
  const bool toastOn = (int32_t)(toastUntilMs_ - animMs_) > 0;
  if (toastOn) {
    cv_->fillRoundRect(14, 46, 100, 26, 6, kWhite);
    queueText((W - textWidth(toast_)) / 2, 55, toast_, kBlack, true);
  }
  cv_->resolve();                                                   // 2x -> 1x lekicsinyítés (élsimítás)
  flushText(const_cast<uint16_t*>(cv_->getBuffer()), toastOn);      // kis szövegek éles, pixelpontos rajzolása a kész képre
  if (trans_) {
    const float p = (float)(animMs_ - transStart_) / 240.0f;
    if (p < 1.0f && prevBuf_) { composeSlide(p); return; }
    trans_ = false;
  }
  tft_.drawRGBBitmap(0, 0, cv_->getBuffer(), W, H);
}

// A tárcsa belsejében a "víz": szintje a mért hőmérséklet, árnyalata a hőmérséklet színe (mint a webes dashboardon).
void Display::water(const Snapshot& s, int cx, int cy, int R) {
  const float frac = clampf(dispTemp_ / kScale, 0.04f, 0.98f);
  const float top = cy + R - frac * 2.0f * R;
  float hue = 220.0f - (dispTemp_ / kScale) * 220.0f; if (hue < 0) hue = 0;
  uint8_t wr, wg, wb; hslRgb(hue, 0.9f, 0.55f, wr, wg, wb);
  const uint16_t back = overNavy(wr, wg, wb, 0.16f), front = overNavy(wr, wg, wb, 0.27f);   // áttetsző víz, mint a weben
  const float ph1 = animMs_ * 0.0042f, ph2 = -animMs_ * 0.0029f + 1.7f;
  constexpr int S = SsCanvas::S;
  // a nagy felbontású vászon minden oszlopára külön: a hullám és a kör széle törtszám-magassággal simán fut
  for (int hx = (cx - R) * S; hx < (cx + R + 1) * S; ++hx) {
    const float dx = (hx + 0.5f) / S - 0.5f - cx;
    const float h2 = (float)R * R - dx * dx;
    if (h2 <= 0) continue;
    const float half = sqrtf(h2);
    const float yMin = cy - half, yMax = cy + half;
    const float yb = top + 1.8f * sinf(dx * 0.26f + ph2) + 1.5f;
    const float yf = top + 2.6f * sinf(dx * 0.17f + ph1);
    const float a = yb > yMin ? yb : yMin, b = yf > yMin ? yf : yMin;
    if (a < yMax) cv_->columnHi(hx, a, yMax, back);
    if (b < yMax) cv_->columnHi(hx, b, yMax, front);
  }
  if (s.heat == HeatPhase::Heating && s.active >= 0) {              // fűtéskor buborékok emelkednek
    for (int i = 0; i < 5; ++i) {
      const float ph = fmodf(animMs_ / (2400.0f + i * 350.0f) + i * 0.21f, 1.0f);
      const float by = cy + R - 3 - ph * (2.0f * R * 0.85f);
      const float bx = cx + R * 0.5f * sinf(i * 2.3f + ph * 4.0f);
      const float ddx = bx - cx, ddy = by - cy;
      if (by < top + 2 || ddx * ddx + ddy * ddy > (R - 2.0f) * (R - 2.0f)) continue;
      cv_->fillCircleF(bx, by, (i & 1) ? 1.8f : 1.1f, c565(120, 150, 190));
    }
  }
}

// E01 (a hőmérő hibás vagy hiányzik): a víz hőfoka ismeretlen, a fűtést csak a bojler beépített termosztátja
// korlátozza. Ezt a felhasználónak jól láthatóan tudnia kell, ezért a főoldal közepét teljesen ez foglalja el.
void Display::sensorFaultView() {
  const bool blink = ((animMs_ / 500) & 1) != 0;
  centered("?", 5, 5, kBad);                                          // nagy piros kérdőjel (változatlan, a vásznon marad)
  centered("HŐMÉRŐ HIBA", 49, 1, kBad);
  centered("Vízhőfok ismeretlen", 61, 1, kWarn);
  centered("Beépített termosztát", 71, 1, kWarn);
  centered("aktív fűtés!", 81, 1, kWarn);
  centered("VIGYÁZAT! FORRÓ VÍZ!", 92, 1, blink ? kBad : kWhite);     // villogó figyelmeztetés
}

void Display::pageHome(const Snapshot& s, const Config& c) {
  const float target = pendTarget_ >= 0 ? pendTarget_ : c.targetC;
  const bool off = target <= 0 || s.mode == Mode::Off;
  char buf[32], status[32];

  // állapot (a webes dashboard megfeleltetése): ikon + szöveg
  Icon icon = Icon::Unknown; const char* text = "Nincs hőmérő adat";
  const bool crit = s.errorMask & ((1u << (uint8_t)ErrorCode::E03) | (1u << (uint8_t)ErrorCode::E05) | (1u << (uint8_t)ErrorCode::E07));
  const bool fault = s.fsm == FsmState::Fault || crit;
  bool anyUsable = false, anyCapped = false;                                // minden jelen lévő forrás hőfokhatáron van-e
  for (uint8_t i = 0; i < kSources; ++i) if (s.enabled[i] && s.present[i]) { if (s.capped[i]) anyCapped = true; else if (!s.blocked[i]) anyUsable = true; }
  const bool capHold = anyCapped && !anyUsable;
  const bool sensorFault = !off && !fault && (s.errorMask & (1u << (uint8_t)ErrorCode::E01));   // fűtés engedélyezett, de a hőmérő nem látszik
  if (fault)                       { icon = Icon::Warn; snprintf(status, sizeof(status), "Hiba %s", s.fsm == FsmState::Fault ? errorInfo(s.fsmError).code : "!"); text = status; }
  else if (s.manualBypass)         { icon = Icon::Manual; text = "Kézi üzem (S1)"; }
  else if (off)                    { icon = Icon::Power; text = "Kikapcsolva"; }
  else if (s.heat == HeatPhase::Heating && s.active < 0 && capHold) { icon = Icon::Smile; text = "Forrás hőfokhatár"; }
  else if (s.heat == HeatPhase::Heating && s.active < 0) { icon = Icon::Hourglass; text = "Nincs hálózat"; }
  else if (s.heat == HeatPhase::Heating) { icon = Icon::Flame; text = s.guaranteeActive ? "Garancia: fűtés" : "Felfutés folyamatban"; }
  else if (s.heat == HeatPhase::Waiting) { icon = Icon::Led; text = "Várakozás"; }
  else if (s.heat == HeatPhase::Reached) { icon = Icon::Smile; text = "Hőfok rendben"; }

  if (sensorFault) {
    sensorFaultView();
  } else {
    // gyűrű: az átfutó (mért) hőmérsékletet mutatja, a fehér jelölő az átfutó célt
    const int cx = 64, cy = 47, r = 40;
    const bool haveT = s.tempValid && !off;
    cv_->fillCircle(cx, cy, r - 3, kPanel);                             // a tárcsa sötét belseje (mint a weben)
    if (!off && s.tempValid) water(s, cx, cy, r - 5);                  // víz a gyűrű belsejében
    if (!haveT) ring(cx, cy, r, 6, -1, kDark);
    else        ring(cx, cy, r, 6, clampf(dispTemp_ / kScale, 0, 1) * 360.0f, colorFromTemp(dispTemp_));
    if (fault) {                                                       // hiba: lüktető piros gyűrű
      const float pu = 0.5f + 0.5f * sinf(animMs_ * 0.0065f);
      cv_->drawCircle(cx, cy, r + 4, c565((uint8_t)(110 + 120 * pu), 20, 20));
    }
    if (!off) {
      const float rad = clampf(dispTarget_ / kScale, 0, 1) * 360.0f * 0.0174533f;
      for (float rr = r - 7.0f; rr <= r + 6.0f; rr += 0.75f) cv_->fillCircleF(cx + rr * sinf(rad), cy - rr * cosf(rad), 0.9f, kWhite);
    }

    // nagy szám (a vásznon marad, élsimítva)
    if (off) { drawIcon(Icon::Power, cx, 47); }
    else {
      if (s.tempValid) snprintf(buf, sizeof(buf), "%d", (int)lroundf(dispTemp_)); else strlcpy(buf, "--", sizeof(buf));
      char tbuf[8]; strlcpy(tbuf, buf, sizeof(tbuf));
      cv_->setTextSize(3); cv_->setTextColor(kWhite);
      int16_t x1, y1; uint16_t w, h; cv_->getTextBounds(tbuf, 0, 0, &x1, &y1, &w, &h);
      cv_->setCursor(cx - (int)w / 2 - 4, 22); cv_->print(tbuf);
      queueText(cx + (int)w / 2 + 2, 24, "°C", kGray);                  // mértékegység: éles kis szöveg
    }
    if (!off) drawIcon(icon, cx, 67);

    // állapotsor és célhőmérséklet
    centered(text, 93, 1, kGray);
  }
  if (!off) { snprintf(buf, sizeof(buf), "cél %.0f°C%s", (double)target, pendTarget_ >= 0 ? "*" : ""); queueText(2, 2, buf, pendTarget_ >= 0 ? kWarn : kGray); }
  if (time_.isValid()) { const uint16_t m = time_.minuteOfDay(); snprintf(buf, sizeof(buf), "%02u:%02u", m / 60, m % 60); queueText(W - 2 - textWidth(buf), 2, buf, kGray); }

  // három hálózat: kitöltött = bojleren, keret = jelen van, piros keret = tiltva, szürke = nincs feszültség
  for (uint8_t i = 0; i < kSources; ++i) {
    const int x = 4 + i * 42, y = 103;
    const bool on = (s.active == (int8_t)i);
    if (on) cv_->fillRoundRect(x, y, 38, 12, 3, kSrc[i]);
    else cv_->drawRoundRect(x, y, 38, 12, 3, !s.present[i] ? kDark : (s.blocked[i] || s.capped[i]) ? kBad : kSrc[i]);
    char n1[2] = {(char)('1' + i), 0};
    queueText(x + (38 - textWidth(n1)) / 2, y + 2, n1, on ? kBlack : (s.present[i] ? kSrc[i] : kDark));
  }
}

void Display::pageLog() {
  queueText(2, 2, "NAPLÓ", kGray);
  Lock l(evMtx_);
  if (!evN_) { queueText(2, 20, "Még nincs esemény", kGray); return; }
  if (scroll_ > (int)evN_ - 8) scroll_ = evN_ > 8 ? evN_ - 8 : 0;
  for (int row = 0; row < 8 && row + scroll_ < (int)evN_; ++row) {
    const Entry& e = ev_[(evHead_ + kEv - 1 - (row + scroll_)) % kEv];     // újabb felül
    const int y = 14 + row * 12;
    queueText(2, y, e.hm, kGray);
    queueText(36, y, e.text, e.color);
  }
}

void Display::pageErrors(const Snapshot& s) {
  queueText(2, 2, "HIBÁK", kGray);
  int y = 16; bool any = false;
  for (uint8_t i = 1; i < static_cast<uint8_t>(ErrorCode::Count) && y < 104; ++i) {
    if (!((s.errorMask >> i) & 1u)) continue;
    any = true;
    const ErrorInfo& ei = errorInfo(static_cast<ErrorCode>(i));
    char t[40]; snprintf(t, sizeof(t), "%s %s", ei.code, shortErr(static_cast<ErrorCode>(i)));
    queueText(2, y, t, ei.sev >= Severity::Error ? kBad : kWarn);
    y += 11;
  }
  if (!any) { drawIcon(Icon::Smile, 64, 50); centered("Nincs aktív hiba", 66, 1, kOk); }
  else if (s.latched) { queueText(2, 106, "Nyugtáz: tartsd 1,5 s", kGray); }
}

void Display::pageSettings(const Snapshot& s, const Config& c) {
  const float hyst = pendHyst_ >= 0 ? pendHyst_ : c.hystC;
  char buf[32];
  queueText(2, 2, "BEÁLLÍTÁSOK", kGray);
  queueText(2, 18, "Hiszterézis", kGray);
  snprintf(buf, sizeof(buf), "%.1f", (double)hyst);
  cv_->setTextSize(3); cv_->setTextColor(pendHyst_ >= 0 ? kWarn : kWhite); cv_->setCursor(2, 30); cv_->print(buf);   // nagy érték: a vásznon marad
  queueText(68, 38, "°C  (+/-)", kGray);
  snprintf(buf, sizeof(buf), "Cél: %.0f°C", (double)c.targetC); queueText(2, 62, buf, kGray);
  if (c.guar.enabled) { snprintf(buf, sizeof(buf), "Garancia %.0f°C %02u:%02u", (double)c.guar.minTempC, c.guar.readyAtMin / 60, c.guar.readyAtMin % 60); queueText(2, 74, buf, kGray); }
  snprintf(buf, sizeof(buf), "FW %s", s.fw); queueText(2, 86, buf, kGray);
  snprintf(buf, sizeof(buf), "WiFi %s %ddBm", s.wifiConnected ? "ok" : (s.apMode ? "AP" : "nincs"), s.rssi); queueText(2, 98, buf, kGray);
}
