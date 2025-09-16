#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace gitfly::diff {

// Compute a unified diff for two sequences of lines.
// `path` is used in headers; it is not used for matching.
std::string unified_diff(const std::vector<std::string>& a,
                         const std::vector<std::string>& b,
                         std::string_view path);

// Utility to split raw text into lines (keeps newlines trimmed).
std::vector<std::string> split_lines(std::string_view text);

} // namespace gitfly::diff

