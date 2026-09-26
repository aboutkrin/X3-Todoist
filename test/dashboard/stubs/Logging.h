#pragma once
inline void dashboardTestLog(const char*, const char*, ...) {}
#define LOG_ERR(...) dashboardTestLog(__VA_ARGS__)
#define LOG_INF(...) dashboardTestLog(__VA_ARGS__)
