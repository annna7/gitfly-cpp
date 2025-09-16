#pragma once
#include "gitfly/config.hpp"
#include "gitfly/consts.hpp"
#include "gitfly/hash.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace gitfly {

struct TreeEntry {
  std::uint32_t mode; // e.g., gitfly::consts::kModeFile file, 040000 dir (octal)
  std::string name;   // filename (no '/')
  oid id;             // 20-byte raw SHA-1 of referenced object
};

class Repository {
public:
  explicit Repository(std::filesystem::path root);

  // Core paths
  [[nodiscard]] const std::filesystem::path &root() const { return root_; }
  [[nodiscard]] auto git_dir() const -> std::filesystem::path { return root_ / consts::kGitDir; }
  [[nodiscard]] auto objects_dir() const -> std::filesystem::path {
    return git_dir() / consts::kObjectsDir;
  }
  [[nodiscard]] auto refs_dir() const -> std::filesystem::path {
    return git_dir() / consts::kRefsDir;
  }
  [[nodiscard]] auto heads_dir() const -> std::filesystem::path {
    return refs_dir() / consts::kHeadsDir;
  }
  [[nodiscard]] auto tags_dir() const -> std::filesystem::path {
    return refs_dir() / consts::kTagsDir;
  }
  [[nodiscard]] auto head_file() const -> std::filesystem::path {
    return git_dir() / consts::kHeadFile;
  }
  [[nodiscard]] auto config_file() const -> std::filesystem::path { return git_dir() / "config"; }
  
  // Milestone 1
  // Initialize a new repo structure under root_.
  // Fails if .gitfly already exists (to avoid clobber).
  void init(const Identity &identity = Identity{.name = "Your Name",
                                                .email = "you@example.com"}) const;

  // Convenience: does .gitfly exist?
  [[nodiscard]] auto is_initialized() const -> bool;

  // Object plumbing
  [[nodiscard]] auto write_blob(std::span<const std::uint8_t> bytes) const -> std::string;
  std::vector<std::uint8_t> read_blob(std::string_view hex_oid) const;

  [[nodiscard]] auto write_tree(const std::vector<TreeEntry> &entries) const -> std::string;
  std::vector<TreeEntry> read_tree(std::string_view hex_oid) const;

  [[nodiscard]] auto write_commit(std::string_view tree_hex,
                                  const std::vector<std::string> &parent_hexes,
                                  std::string_view author_line, std::string_view committer_line,
                                  std::string_view message) const -> std::string;

  struct CommitInfo {
    std::string tree_hex;
    std::vector<std::string> parents; // zero or more parents (40-hex each)
    std::string author;               // full author line after "author "
    std::string committer;            // full committer line
    std::string message;              // raw message (may contain newlines)
  };

  // Read and parse a commit object into headers + message.
  [[nodiscard]] auto read_commit(std::string_view commit_hex) const -> CommitInfo;

  // Graph query: is `ancestor_hex` an ancestor of `descendant_hex`?
  // Includes equality (a commit is an ancestor of itself).
  [[nodiscard]] auto is_commit_ancestor(std::string_view ancestor_hex,
                                        std::string_view descendant_hex) const -> bool;

  [[nodiscard]] auto write_tree_from_index() const -> std::string;
  [[nodiscard]] auto commit_index(std::string_view message) const -> std::string;
  // Like commit_index, but explicitly set additional parents (e.g., for merges).
  [[nodiscard]] auto commit_index_with_parents(std::string_view message,
                                               const std::vector<std::string> &extra_parents) const
      -> std::string;
  void checkout(std::string_view target) const;
  [[nodiscard]] auto object_path_from_oid(const oid &oid) const -> std::filesystem::path;

  // Merge the given branch name into the current branch (symbolic HEAD required).
  // - If giver is ancestor of current: no-op (Already up to date).
  // - If current is ancestor of giver: fast-forward (WD + index updated, ref advanced).
  // - Else: 3-way merge with conflict markers; leaves MERGE_HEAD on conflict.
  void merge_branch(const std::string &giver_branch) const;

private:
  static auto mode_to_ascii_octal(std::uint32_t mode) -> std::string;
  static auto ascii_octal_to_mode(std::string_view str) -> std::uint32_t;

  std::filesystem::path root_;
};

} // namespace gitfly
