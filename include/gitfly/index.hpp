#pragma once
#include "gitfly/hash.hpp"
#include "gitfly/consts.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <map>

namespace gitfly {

class Repository; // fwd decl to avoid header cycle

struct IndexEntry {
  std::uint32_t mode;  // e.g., gitfly::consts::kModeFile
  oid           oid;   // blob id (20 bytes)
  std::string   path;  // "dir/file", UTF-8, no leading '/'
};

class Index {
public:
  explicit Index(std::filesystem::path repo_root);

  // Parse .gitfly/index if it exists (no throw if missing)
  void load();

  // Overwrite .gitfly/index with current entries
  void save() const;

  // Read file at working-dir `wd/relpath`, write blob via repo, add/replace an entry
  void add_path(const std::filesystem::path& wd,
                std::string_view relpath, 
                const Repository& repo,
                std::uint32_t mode = gitfly::consts::kModeFile);

  // Remove a path from index (no error if absent)
  void remove_path(std::string_view relpath);

  const std::vector<IndexEntry>& entries() const { return entries_; }
  std::map<std::string,std::string> as_path_oid_map() const;

private:
  std::filesystem::path index_path() const;

  std::filesystem::path repo_root_;
  std::vector<IndexEntry> entries_;
};

} // namespace gitfly
