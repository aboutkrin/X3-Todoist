#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include "DashboardModel.h"
using namespace dashboard;
static time_t date(int y, int m, int d, int h = 12, int min = 0) {
  tm t{};
  t.tm_year = y - 1900;
  t.tm_mon = m - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = min;
  t.tm_isdst = -1;
  return mktime(&t);
}
int main() {
  setenv("TZ", "ICT-7", 1);
  tzset();
  auto now = date(2026, 9, 26);
  assert(classify("2026-09-26", now) == Section::Today);
  assert(classify("2026-09-25", now) == Section::Overdue);
  assert(classify("2026-09-27", now) == Section::Upcoming);
  assert(classify("2026-10-03", now) == Section::Upcoming);
  assert(classify("2026-10-04", now) == Section::Hidden);
  assert(classify("", now) == Section::Hidden);
  assert(classify("2026-09-25T18:00:00Z", now) == Section::Today);
  assert(classify("2026-09-26T01:00:00+07:00", now) == Section::Today);
  assert(classify("2026-09-26T23:30:00-04:00", now) == Section::Upcoming);
  assert(classify("2026-09-26T23:30:00", now) == Section::Today);
  assert(classify("2026-09-26T10:00:00.000Z", now) == Section::Today);
  assert(classify("2026-09-26", date(2026, 9, 27, 0)) == Section::Overdue);
  assert(classify("2027-01-01", date(2026, 12, 31)) == Section::Upcoming);
  int64_t rank;
  int day;
  assert(!parseDue("2026-02-29", rank, day));
  assert(parseDue("2028-02-29", rank, day));
  assert(!parseDue("2026-09-26T25:00:00Z", rank, day));
  assert(!parseDue("2026-09-26garbage", rank, day));
  assert(!parseDue("2026-09-26T12:00:00bad", rank, day));
  IndexEntry first{}, second{};
  first.section = second.section = Section::Today;
  first.rank = second.rank = 42;
  first.priority = 4;
  second.priority = 1;
  assert(before(first, second));
  second.priority = 4;
  strcpy(first.id, "a");
  strcpy(second.id, "b");
  assert(before(first, second));
  std::vector<IndexEntry> list(512);
  for (size_t i = 0; i < list.size(); ++i) {
    list[i].rank = 512 - i;
    list[i].section = Section::Upcoming;
  }
  std::sort(list.begin(), list.end(), before);
  for (size_t i = 1; i < list.size(); ++i) assert(list[i - 1].rank < list[i].rank);
  assert((checksum(0xffffffff, "123456789", 9) ^ 0xffffffff) == 0xcbf43926);
  char text[5];
  assert(copyText(text, sizeof(text), "test"));
  assert(!copyText(text, sizeof(text), "tests"));
  // DST changes must group calendar days, rather than divide local epochs by 86400.
  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
  tzset();
  assert(classify("2026-03-09", date(2026, 3, 8, 0)) == Section::Upcoming);
  assert(classify("2026-11-02", date(2026, 11, 1, 0)) == Section::Upcoming);
  std::cout << "Dashboard model: date boundaries, offsets, DST, ordering, 512 entries and checksum passed\n";
}
