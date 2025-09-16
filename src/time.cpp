#include "gitfly/time.hpp"

#include <cstdio>

#if defined(_WIN32)
#include <time.h>
#include <windows.h>
static std::time_t timegm_portable(std::tm *t) { return _mkgmtime(t); }
#else
// POSIX/macOS have timegm
static std::time_t timegm_portable(std::tm *t) { return timegm(t); }
#endif

namespace gitfly::timeutil {

int local_utc_offset_minutes(std::time_t t) {
  std::tm lt{}, gt{};
#if defined(_WIN32)
  localtime_s(&lt, &t);
  gmtime_s(&gt, &t);
#else
  localtime_r(&t, &lt);
  gmtime_r(&t, &gt);
#endif
  // Convert both back to epoch and subtract: local - UTC
  const std::time_t local_epoch = std::mktime(&lt);
  const std::time_t utc_epoch = timegm_portable(&gt);
  const long diff = local_epoch - utc_epoch; // seconds
  return static_cast<int>(diff / 60);        // minutes
}

std::string tz_offset_string(int minutes) {
  char buf[8];
  char sign = minutes >= 0 ? '+' : '-';
  int m = minutes >= 0 ? minutes : -minutes;
  int hh = m / 60;
  int mm = m % 60;
  std::snprintf(buf, sizeof(buf), "%c%02d%02d", sign, hh, mm);
  return std::string(buf);
}

std::string make_signature(const Identity &id, std::time_t when, int tz_minutes) {
  return id.name + " <" + id.email + "> " + std::to_string(static_cast<long long>(when)) + " " +
         tz_offset_string(tz_minutes);
}

} // namespace gitfly::timeutil
