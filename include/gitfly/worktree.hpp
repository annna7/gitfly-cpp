#pragma once
#include <filesystem>
#include <map>
#include <set>
#include <string>

namespace gitfly {

class Repository; // fwd

namespace worktree {

using PathOidMap = std::map<std::string, std::string>; // path -> 40-hex blob id

// Enumerate regular files under root, excluding .gitfly directory, as repo-relative paths
void enumerate_paths(const std::filesystem::path& root, std::set<std::string>& out_paths);

// Build path->hex map for working directory contents
auto build_working_map(const std::filesystem::path& root) -> PathOidMap;

// Build path->hex map from index file
auto index_to_map(const std::filesystem::path& root) -> PathOidMap;

// Build path->hex map from a tree object (recursive)
auto tree_to_map(const Repository& repo, const std::string& tree_hex) -> PathOidMap;

// Apply a snapshot (path->hex) to the working directory
void apply_snapshot(const Repository& repo, const PathOidMap& snapshot);

// Rewrite index entries to match the snapshot
void write_index_snapshot(const Repository& repo, const PathOidMap& snapshot);

} // namespace worktree

} // namespace gitfly

