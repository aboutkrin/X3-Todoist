#pragma once
struct HalPowerManager {
  struct Lock {
    ~Lock() {}
  };
  unsigned getBatteryPercentage() const { return 84; }
};
inline HalPowerManager powerManager;
