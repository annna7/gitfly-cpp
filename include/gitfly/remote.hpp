#pragma once
#include <filesystem>
#include <string>

namespace gitfly::remote {

struct FetchResult {
  std::string branch; // "DETACHED" if server is detached
  std::string tip;    // 40-hex commit id or empty if none
};
 
// Clone a repository at `src` into directory `dst` (created if missing).
void clone_repo(const std::filesystem::path& src, const std::filesystem::path& dst);

// Push current branch from `local` repo into `remote` repo (fast-forward only).
// Branch name must be provided; remote ref is `refs/heads/<branch>`.
void push_branch(const std::filesystem::path& local,
                 const std::filesystem::path& remote,
                 const std::string& branch);

// Fetch remote HEAD (branch+tip) into local repo as refs/remotes/<name>/<branch>.
// Returns the advertised branch name and tip.
FetchResult fetch_head(const std::filesystem::path& local,
                       const std::filesystem::path& remote,
                       const std::string& name = "origin");

} // namespace gitfly::remote
