#include "Display.h"
#include <pins.h>
#include <ArduinoJson.h>
#include <math.h>

namespace {
constexpr int W = 128, H = 128;
constexpr uint16_t c565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
constexpr uint16_t kBlack = 0x0000, kWhite = 0xFFFF;
constexpr uint16_t kGray = c565(110, 124, 134), kDark = c565(37, 49, 66), kPanel = c565(17, 24, 39);
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
    case ErrorCode::E01: return "Homero hiba";
    case ErrorCode::E02: return "PZEM nem valaszol";
    case ErrorCode::E03: return "Reteszeles hiba";
    case ErrorCode::E04: return "Nem huz be";
    case ErrorCode::E05: return "Beragadt kontakt";
    case ErrorCode::E06: return "Feszultseg hibas";
    case ErrorCode::E07: return "Tuluhomerseklet";
    case ErrorCode::E08: return "WiFi/MQTT kiesett";
    case ErrorCode::E09: return "Nincs pontos ido";
    case ErrorCode::E10: return "Gyakori reset";
    case ErrorCode::E11: return "Tarhely baj";
    case ErrorCode::E12: return "Konfig hibas";
    case ErrorCode::E13: return "Sok kapcsolas";
    case ErrorCode::E14: return "Nincs halozat";
    case ErrorCode::E15: return "Vizko gyanu";
    case ErrorCode::E16: return "Futoelem gyenge";
    default:             return "";
  }
}
float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
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
  cv_ = new GFXcanvas16(W, H);                // 32 kB, villogásmentes rajzoláshoz
  if (!cv_ || !cv_->getBuffer()) { delete cv_; cv_ = nullptr; return; }
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
    case EventType::Switch:  snprintf(n.text, sizeof(n.text), "Valt %d>%d %s", e.from + 1, e.to + 1, !strcmp(e.msg, "priority") ? "prio" : e.msg); n.color = kOk; break;
    case EventType::Error:   snprintf(n.text, sizeof(n.text), "%s %s", errorInfo(e.code).code, e.raised ? "fellepett" : "megszunt");
                             n.color = e.raised ? (errorInfo(e.code).sev >= Severity::Error ? kBad : kWarn) : kGray; break;
    case EventType::Command: snprintf(n.text, sizeof(n.text), "Parancs"); n.color = kGray; break;
    default:
      if (!strncmp(e.msg, "garantalt", 9)) snprintf(n.text, sizeof(n.text), "Garancia %s", strstr(e.msg, "be") ? "be" : "ki");
      else if (!strncmp(e.msg, "boot", 4)) snprintf(n.text, sizeof(n.text), "Indulas");
      else snprintf(n.text, sizeof(n.text), "%s", e.msg);
      n.color = kGray; break;
  }
  n.text[15] = 0;                                                  // a sorba (21 karakter - idő) ennyi fér
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
  if (longPress) { sink_.ackFault(); strlcpy(toast_, "NYUGTAZVA", sizeof(toast_)); toastUntilMs_ = animMs_ + 1500; return; }
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
    else             t = clampf(t + dir, 10.0f, c.safety.tMaxC);
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
  strlcpy(toast_, ok ? "MENTVE" : "NEM MENTHETO", sizeof(toast_));
  toastUntilMs_ = animMs_ + 1500;
  pendTarget_ = -1; pendHyst_ = -1;
}

void Display::loop() {
  bool redraw = true;
  uint32_t lastDrawMs = 0, lastStepMs = millis();
  for (;;) {
    const uint32_t now = millis();
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
  int16_t x1, y1; uint16_t w, h;
  cv_->setTextSize(size);
  cv_->getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  cv_->setTextColor(col);
  cv_->setCursor((W - (int)w) / 2, y);
  cv_->print(txt);
}

void Display::ring(int cx, int cy, int r, int th, float fillDeg, uint16_t col) {
  for (int a = 0; a < 360; a += 3) {                          // 12 óránál indul, óramutató járásával, mint a dashboard
    const float rad = a * 0.0174533f;
    cv_->fillCircle(cx + lroundf(r * sinf(rad)), cy - lroundf(r * cosf(rad)), th / 2, a <= fillDeg ? col : kDark);
  }
}

void Display::drawIcon(Icon k, int cx, int cy) {
  switch (k) {
    case Icon::Flame:                                          // ♨ felfűtés
    {
      const int sway = (int)lroundf(sinf(animMs_ * 0.0048f));                         // lassú ringás
      const int tipO = cy - 9 + (int)lroundf(2.2f * sinf(animMs_ * 0.0115f));         // villódzó csúcs
      const int tipI = cy - 3 + (int)lroundf(1.6f * sinf(animMs_ * 0.0170f + 1.0f));
      cv_->fillCircle(cx, cy + 3, 6, kOrange);
      cv_->fillTriangle(cx - 6, cy + 2, cx + 6, cy + 2, cx + sway, tipO, kOrange);
      cv_->fillCircle(cx, cy + 4, 3, kWarn);
      cv_->fillTriangle(cx - 3, cy + 3, cx + 3, cy + 3, cx + sway, tipI, kWarn);
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
      for (int a = 25; a <= 155; a += 10) { const float r = a * 0.0174533f; cv_->drawPixel(cx + lroundf(4.5f * cosf(r)), cy + 1 + lroundf(4.0f * sinf(r)), kBlack); }
      break;
    case Icon::Power:                                          // kikapcsolva
      for (int a = 40; a <= 320; a += 8) { const float r = a * 0.0174533f; cv_->fillCircle(cx + lroundf(7 * sinf(r)), cy - lroundf(7 * cosf(r)), 1, kGray); }
      cv_->fillRect(cx - 1, cy - 9, 3, 9, kGray);
      break;
    case Icon::Warn:                                           // hiba
      cv_->fillTriangle(cx, cy - 9, cx - 9, cy + 7, cx + 9, cy + 7, kBad);
      cv_->fillRect(cx - 1, cy - 3, 2, 6, kWhite);
      cv_->fillRect(cx - 1, cy + 4, 2, 2, kWhite);
      break;
    case Icon::Manual:                                         // kézi üzem (S1)
      cv_->drawRoundRect(cx - 10, cy - 7, 20, 14, 3, kWhite);
      cv_->setTextSize(1); cv_->setTextColor(kWhite); cv_->setCursor(cx - 5, cy - 3); cv_->print("S1");
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
  switch (page_) {
    case PgHome:     pageHome(s, c); break;
    case PgLog:      pageLog(); break;
    case PgErrors:   pageErrors(s); break;
    default:         pageSettings(s, c); break;
  }
  footer(s);
  if ((int32_t)(toastUntilMs_ - animMs_) > 0) {
    cv_->fillRoundRect(14, 46, 100, 26, 6, kWhite);
    centered(toast_, 55, 1, kBlack);
  }
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
  for (int x = cx - R; x <= cx + R; ++x) {
    const int dx = x - cx;
    const int half = (int)sqrtf((float)(R * R - dx * dx));
    const int yMin = cy - half, yMax = cy + half;
    const int yb = (int)(top + 1.8f * sinf(dx * 0.26f + ph2) + 1.5f);
    const int yf = (int)(top + 2.6f * sinf(dx * 0.17f + ph1));
    const int a = yb > yMin ? yb : yMin, b = yf > yMin ? yf : yMin;
    if (a <= yMax) cv_->drawFastVLine(x, a, yMax - a + 1, back);
    if (b <= yMax) cv_->drawFastVLine(x, b, yMax - b + 1, front);
  }
  if (s.heat == HeatPhase::Heating && s.active >= 0) {              // fűtéskor buborékok emelkednek
    for (int i = 0; i < 5; ++i) {
      const float ph = fmodf(animMs_ / (2400.0f + i * 350.0f) + i * 0.21f, 1.0f);
      const int by = (int)(cy + R - 3 - ph * (2.0f * R * 0.85f));
      const int bx = cx + (int)(R * 0.5f * sinf(i * 2.3f + ph * 4.0f));
      const int ddx = bx - cx, ddy = by - cy;
      if (by < (int)top + 2 || ddx * ddx + ddy * ddy > (R - 2) * (R - 2)) continue;
      cv_->fillCircle(bx, by, (i & 1) ? 2 : 1, c565(120, 150, 190));
    }
  }
}

void Display::pageHome(const Snapshot& s, const Config& c) {
  const float target = pendTarget_ >= 0 ? pendTarget_ : c.targetC;
  const bool off = target <= 0 || s.mode == Mode::Off;
  char buf[32], status[32];

  // állapot (a webes dashboard megfeleltetése): ikon + szöveg
  Icon icon = Icon::Unknown; const char* text = "Nincs homero adat";
  const bool crit = s.errorMask & ((1u << (uint8_t)ErrorCode::E03) | (1u << (uint8_t)ErrorCode::E05) | (1u << (uint8_t)ErrorCode::E07));
  const bool fault = s.fsm == FsmState::Fault || crit;
  if (fault)                       { icon = Icon::Warn; snprintf(status, sizeof(status), "Hiba %s", s.fsm == FsmState::Fault ? errorInfo(s.fsmError).code : "!"); text = status; }
  else if (s.manualBypass)         { icon = Icon::Manual; text = "Kezi uzem (S1)"; }
  else if (off)                    { icon = Icon::Power; text = "Kikapcsolva"; }
  else if (s.heat == HeatPhase::Heating && s.active < 0) { icon = Icon::Hourglass; text = "Nincs halozat"; }
  else if (s.heat == HeatPhase::Heating) { icon = Icon::Flame; text = s.guaranteeActive ? "Garancia: futes" : "Felfutes folyamatban"; }
  else if (s.heat == HeatPhase::Waiting) { icon = Icon::Led; text = "Varakozas"; }
  else if (s.heat == HeatPhase::Reached) { icon = Icon::Smile; text = "Hofok rendben"; }

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
    for (int rr = r - 7; rr <= r + 6; rr += 2) cv_->fillCircle(cx + lroundf(rr * sinf(rad)), cy - lroundf(rr * cosf(rad)), 1, kWhite);
  }

  // nagy szám
  if (off) { drawIcon(Icon::Power, cx, 47); }
  else {
    if (s.tempValid) snprintf(buf, sizeof(buf), "%d", (int)lroundf(dispTemp_)); else strlcpy(buf, "--", sizeof(buf));
    char tbuf[8]; strlcpy(tbuf, buf, sizeof(tbuf));
    cv_->setTextSize(3); cv_->setTextColor(kWhite);
    int16_t x1, y1; uint16_t w, h; cv_->getTextBounds(tbuf, 0, 0, &x1, &y1, &w, &h);
    cv_->setCursor(cx - (int)w / 2 - 4, 22); cv_->print(tbuf);
    cv_->setTextSize(1); cv_->setTextColor(kGray); cv_->setCursor(cx + (int)w / 2 - 1, 24); cv_->print("C");
  }
  if (!off) drawIcon(icon, cx, 67);

  // állapotsor és célhőmérséklet
  centered(text, 93, 1, kGray);
  if (!off) { snprintf(buf, sizeof(buf), "cel %.0fC%s", (double)target, pendTarget_ >= 0 ? "*" : ""); cv_->setTextSize(1); cv_->setTextColor(pendTarget_ >= 0 ? kWarn : kGray); cv_->setCursor(2, 2); cv_->print(buf); }
  if (time_.isValid()) { const uint16_t m = time_.minuteOfDay(); snprintf(buf, sizeof(buf), "%02u:%02u", m / 60, m % 60); cv_->setTextColor(kGray); cv_->setCursor(W - 32, 2); cv_->print(buf); }

  // három hálózat: kitöltött = bojleren, keret = jelen van, piros keret = tiltva, szürke = nincs feszültség
  for (uint8_t i = 0; i < kSources; ++i) {
    const int x = 4 + i * 42, y = 103;
    const bool on = (s.active == (int8_t)i);
    if (on) cv_->fillRoundRect(x, y, 38, 12, 3, kSrc[i]);
    else cv_->drawRoundRect(x, y, 38, 12, 3, !s.present[i] ? kDark : (s.blocked[i] || s.capped[i]) ? kBad : kSrc[i]);
    char n1[2] = {(char)('1' + i), 0};
    cv_->setTextSize(1); cv_->setTextColor(on ? kBlack : (s.present[i] ? kSrc[i] : kDark));
    cv_->setCursor(x + 16, y + 2); cv_->print(n1);
  }
}

void Display::pageLog() {
  cv_->setTextSize(1); cv_->setTextColor(kGray); cv_->setCursor(2, 2); cv_->print("NAPLO");
  Lock l(evMtx_);
  if (!evN_) { cv_->setTextColor(kGray); cv_->setCursor(2, 20); cv_->print("Meg nincs esemeny"); return; }
  if (scroll_ > (int)evN_ - 8) scroll_ = evN_ > 8 ? evN_ - 8 : 0;
  for (int row = 0; row < 8 && row + scroll_ < (int)evN_; ++row) {
    const Entry& e = ev_[(evHead_ + kEv - 1 - (row + scroll_)) % kEv];     // újabb felül
    const int y = 14 + row * 12;
    cv_->setTextColor(kGray);  cv_->setCursor(2, y);  cv_->print(e.hm);
    cv_->setTextColor(e.color); cv_->setCursor(36, y); cv_->print(e.text);
  }
}

void Display::pageErrors(const Snapshot& s) {
  cv_->setTextSize(1); cv_->setTextColor(kGray); cv_->setCursor(2, 2); cv_->print("HIBAK");
  int y = 16; bool any = false;
  for (uint8_t i = 1; i < static_cast<uint8_t>(ErrorCode::Count) && y < 104; ++i) {
    if (!((s.errorMask >> i) & 1u)) continue;
    any = true;
    const ErrorInfo& ei = errorInfo(static_cast<ErrorCode>(i));
    char t[24]; snprintf(t, sizeof(t), "%s %s", ei.code, shortErr(static_cast<ErrorCode>(i)));
    cv_->setTextColor(ei.sev >= Severity::Error ? kBad : kWarn); cv_->setCursor(2, y); cv_->print(t);
    y += 11;
  }
  if (!any) { drawIcon(Icon::Smile, 64, 50); centered("Nincs aktiv hiba", 66, 1, kOk); }
  else if (s.latched) { cv_->setTextColor(kGray); cv_->setCursor(2, 106); cv_->print("Nyugtaz: tartsd 1,5 s"); }
}

void Display::pageSettings(const Snapshot& s, const Config& c) {
  const float hyst = pendHyst_ >= 0 ? pendHyst_ : c.hystC;
  char buf[32];
  cv_->setTextSize(1); cv_->setTextColor(kGray); cv_->setCursor(2, 2); cv_->print("BEALLITASOK");
  cv_->setCursor(2, 18); cv_->print("Hiszterezis");
  snprintf(buf, sizeof(buf), "%.1f", (double)hyst);
  cv_->setTextSize(3); cv_->setTextColor(pendHyst_ >= 0 ? kWarn : kWhite); cv_->setCursor(2, 30); cv_->print(buf);
  cv_->setTextSize(1); cv_->setTextColor(kGray); cv_->setCursor(70, 38); cv_->print("C  (+/-)");
  snprintf(buf, sizeof(buf), "Cel: %.0fC", (double)c.targetC); cv_->setCursor(2, 62); cv_->print(buf);
  if (c.guar.enabled) { snprintf(buf, sizeof(buf), "Garancia %.0fC %02u:%02u", (double)c.guar.minTempC, c.guar.readyAtMin / 60, c.guar.readyAtMin % 60); cv_->setCursor(2, 74); cv_->print(buf); }
  snprintf(buf, sizeof(buf), "FW %s", s.fw); cv_->setCursor(2, 86); cv_->print(buf);
  snprintf(buf, sizeof(buf), "WiFi %s %ddBm", s.wifiConnected ? "ok" : (s.apMode ? "AP" : "nincs"), s.rssi); cv_->setCursor(2, 98); cv_->print(buf);
}
