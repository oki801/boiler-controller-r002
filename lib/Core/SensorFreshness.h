#pragma once
#include <stdint.h>

// Elavult szenzoradat észlelése. A SensorTask minden olvasás után növeli a sorszámot (seq); ha a sorszám
// a megadott ideig nem változik, a task lefagyott vagy elakadt, és a hőmérséklet/PZEM adat már nem hiteles.
// Tiszta osztály, hardver nélkül tesztelhető. A sorszám 0 = még nem volt első olvasás (ez nem elavulás).
class SensorFreshness {
 public:
  void update(uint32_t nowMs, uint32_t seq) {
    if (seq != lastSeq_) { lastSeq_ = seq; changedMs_ = nowMs; }
  }
  bool stale(uint32_t nowMs, uint32_t maxAgeMs) const {
    return lastSeq_ != 0 && (uint32_t)(nowMs - changedMs_) > maxAgeMs;   // túlcsordulás-biztos
  }
 private:
  uint32_t lastSeq_ = 0, changedMs_ = 0;
};
