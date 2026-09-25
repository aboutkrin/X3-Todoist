#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>

namespace dashboard {
constexpr size_t MAX_TASKS = 512;
constexpr uint32_t REFRESH_SECONDS = 15 * 60;
enum class Section : uint8_t { Today, Overdue, Upcoming, Hidden };

// Versioned device-local cache, never sent over the network.
struct Task {
  char id[64]{};
  char parentId[64]{};
  char project[128]{};
  char title[1024]{};
  char description[1024]{};
  char due[48]{};
  uint8_t priority = 1;
  uint8_t recurring = 0;
};

struct IndexEntry {
  int64_t rank = 0;
  char id[64]{};
  uint16_t record = 0;
  Section section = Section::Hidden;
  uint8_t priority = 1;
};

// Accept date-only, floating local datetime and RFC3339 offsets. Date-only
// tasks sort after timed tasks on the same day.
bool parseDue(const char* text, int64_t& rank, int& day);
int localDay(time_t epoch);
Section classify(const char* due, time_t now);
bool before(const IndexEntry& left, const IndexEntry& right);
uint32_t checksum(uint32_t crc, const void* bytes, size_t count);
bool copyText(char* out, size_t capacity, const char* input);
}  // namespace dashboard
