#pragma once
#ifdef _WIN32
#include <cstdlib>
#include <ctime>
#define setenv(name, value, overwrite) _putenv_s(name, value)
#define tzset _tzset
inline tm* localtime_r(const time_t* input, tm* output) { return localtime_s(output, input) == 0 ? output : nullptr; }
#endif
