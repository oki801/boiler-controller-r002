#pragma once
#include <Snapshot.h>
#include <Sync.h>

// A vezérlő task által írt, a többi task által olvasott pillanatkép (mutexszel védve).
class SharedState : public ISnapshotSource {
 public:
  void set(const Snapshot& s) { Lock l(m_); s_ = s; }
  Snapshot snapshot() const override { Lock l(m_); return s_; }
 private:
  mutable Mutex m_;
  Snapshot s_;
};
