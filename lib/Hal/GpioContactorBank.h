#pragma once
#include <Arduino.h>
#include <BoilerTypes.h>
#include <IClock.h>
#include <IContactorBank.h>
#include <InterlockGuard.h>

// A kontaktor-kimenetek egyetlen tulajdonosa. Minden zárást az InterlockGuard
// engedélyez (egyidejűség, holtidő). Reset/boot alatt a lábak alacsonyak -> nyitva.
class GpioContactorBank : public IContactorBank {
 public:
  GpioContactorBank(const uint8_t drv[kSources], const uint8_t fb[kSources], const IClock& clk,
                    uint8_t armPin, uint32_t minDeadMs = kHardMinDeadMs);
  void begin();                          // az ELSŐ dolog a setup()-ban
  bool close(uint8_t src) override;
  void openAll() override;
  bool feedback(uint8_t src) const override;
  uint8_t closedCount() const override;
  void setArm(bool on);                  // hardveres ARM-kapcsoló (tekercstáp)
  bool armed() const { return armed_; }
 private:
  uint8_t feedbackMask() const;
  uint8_t  drv_[kSources], fb_[kSources], armPin_;
  const IClock& clk_;
  uint32_t minDeadMs_, lastOpenMs_ = 0;
  uint8_t  commanded_ = 0;
  bool     armed_ = false;
};
