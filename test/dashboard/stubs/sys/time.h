#pragma once
#include <ctime>
struct DashboardTimeval {
  time_t tv_sec;
  long tv_usec;
};
#define timeval DashboardTimeval
inline int settimeofday(const DashboardTimeval*, const void*) { return 0; }
