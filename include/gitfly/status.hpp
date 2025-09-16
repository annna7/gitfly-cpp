#pragma once
#include <string>
#include <vector>

namespace gitfly {

enum class ChangeKind : std::uint8_t { Added, Modified, Deleted };

struct Change {
  ChangeKind kind;
  std::string path;  // repo-relative
};

struct Status {
  std::vector<Change> staged;     // HEAD vs index
  std::vector<Change> unstaged;   // working vs index
  std::vector<std::string> untracked; // working - index
};

// Compute a minimal status snapshot.
class Repository; // fwd
auto compute_status(const Repository& repo) -> Status;

} // namespace gitfly
