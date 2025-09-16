#pragma once
#include "gitfly/config.hpp"
#include <string>
#include <ctime>

namespace gitfly::timeutil {

// Minutes east of UTC (e.g., +180 = +0300). Uses the local timezone at `t`.
auto local_utc_offset_minutes(std::time_t time) -> int;

// Format ±HHMM from minutes (e.g., +180 -> "+0300", -420 -> "-0700")
auto tz_offset_string(int minutes) -> std::string;

// Build "Name <email> 1714412345 +0300"
auto make_signature(const Identity& identity, std::time_t when, int tz_minutes) -> std::string;

} // namespace gitfly::timeutil
