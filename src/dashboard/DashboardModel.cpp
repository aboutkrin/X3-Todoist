#include "DashboardModel.h"

#include <cstdio>
#include <cstdlib>

namespace dashboard {
namespace {
int civilDay(int year, int month, int day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  return era * 146097 + static_cast<int>(yoe * 365 + yoe / 4 - yoe / 100 + doy) - 719468;
}
bool validDate(int y, int m, int d) {
  if (y < 1970 || y > 2199 || m < 1 || m > 12 || d < 1) return false;
  constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = (y % 4 == 0) && (y % 100 != 0 || y % 400 == 0);
  return d <= days[m - 1] + (m == 2 && leap ? 1 : 0);
}
}  // namespace

int localDay(time_t epoch) {
  tm local{};
  localtime_r(&epoch, &local);
  return civilDay(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

bool parseDue(const char* text, int64_t& rank, int& day) {
  if (!text || strlen(text) < 10) return false;
  int y = 0, m = 0, d = 0;
  if (sscanf(text, "%4d-%2d-%2d", &y, &m, &d) != 3 || text[4] != '-' || text[7] != '-' || !validDate(y, m, d))
    return false;
  day = civilDay(y, m, d);
  if (strlen(text) == 10) {
    rank = static_cast<int64_t>(day) * 86400 + 86399;
    return true;
  }
  if (strlen(text) < 19 || text[10] != 'T') return false;
  int h = 0, min = 0, sec = 0;
  if (sscanf(text + 11, "%2d:%2d:%2d", &h, &min, &sec) != 3 || h > 23 || min > 59 || sec > 59 || h < 0 || min < 0 ||
      sec < 0)
    return false;
  const char* suffix = text + 19;
  if (*suffix == '.') {
    ++suffix;
    if (*suffix < '0' || *suffix > '9') return false;
    while (*suffix >= '0' && *suffix <= '9') ++suffix;
  }
  int64_t civil = static_cast<int64_t>(day) * 86400 + h * 3600 + min * 60 + sec;
  if (*suffix == '\0') {
    rank = civil;
    return true;
  }
  int offset = 0;
  if (*suffix == '+' || *suffix == '-') {
    int oh = 0, om = 0;
    if (strlen(suffix) != 6 || sscanf(suffix + 1, "%2d:%2d", &oh, &om) != 2 || oh > 23 || om > 59 || oh < 0 || om < 0)
      return false;
    offset = (oh * 60 + om) * 60 * (*suffix == '+' ? 1 : -1);
  } else if (strcmp(suffix, "Z") != 0)
    return false;
  const time_t utc = static_cast<time_t>(civil - offset);
  tm local{};
  localtime_r(&utc, &local);
  day = civilDay(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
  rank = static_cast<int64_t>(day) * 86400 + local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
  return true;
}

Section classify(const char* due, time_t now) {
  int64_t rank = 0;
  int day = 0;
  if (!parseDue(due, rank, day)) return Section::Hidden;
  const int today = localDay(now);
  if (day < today) return Section::Overdue;
  if (day == today) return Section::Today;
  return day <= today + 7 ? Section::Upcoming : Section::Hidden;
}

bool before(const IndexEntry& left, const IndexEntry& right) {
  if (left.rank != right.rank) return left.rank < right.rank;
  if (left.priority != right.priority) return left.priority > right.priority;
  return strcmp(left.id, right.id) < 0;
}

uint32_t checksum(uint32_t crc, const void* bytes, size_t count) {
  auto data = static_cast<const uint8_t*>(bytes);
  while (count--) {
    crc ^= *data++;
    for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return crc;
}

bool copyText(char* out, size_t capacity, const char* input) {
  if (!input) input = "";
  const size_t size = strlen(input);
  if (size >= capacity) return false;
  memmove(out, input, size + 1);
  return true;
}
}  // namespace dashboard
