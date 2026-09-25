#pragma once
#include <ctime>
struct ClockStub {
  bool localTime(tm&) { return false; }
  bool syncFromNTP() { return false; }
};
inline ClockStub halClock;
