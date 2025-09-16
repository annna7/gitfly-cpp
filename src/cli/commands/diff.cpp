#include "gitfly/diff.hpp"

#include "gitfly/fs.hpp"
#include "gitfly/refs.hpp"
#include "gitfly/repo.hpp"
#include "gitfly/worktree.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

static std::vector<std::string> read_working_lines(const std::filesystem::path &root,
                                                   const std::string &rel) {
  auto bytes = gitfly::fs::read_file(root / rel);
  return gitfly::diff::split_lines(
      std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
}

static std::vector<std::string> read_blob_lines(const gitfly::Repository &repo,
                                                const std::string &hex) {
  auto bytes = repo.read_blob(hex);
  return gitfly::diff::split_lines(
      std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
}

int cmd_diff(int argc, char **argv) {
  bool cached = false;
  for (int i = 1; i < argc; ++i)
    if (std::string(argv[i]) == "--cached")
      cached = true;

  gitfly::Repository repo{std::filesystem::current_path()};
  if (!repo.is_initialized()) {
    std::cerr << "diff: not a gitfly repo (run `gitfly init`)\n";
    return 1;
  }

  // Determine baseline vs target maps
  gitfly::worktree::PathOidMap left, right; // left vs right snapshot
  if (cached) {
    // HEAD vs index
    if (auto head_txt = gitfly::read_HEAD(repo.root()); head_txt) {
      std::string h = *head_txt;
      while (!h.empty() && (h.back() == '\n' || h.back() == '\r'))
        h.pop_back();
      std::string commit_hex;
      if (h.rfind("ref:", 0) == 0) {
        std::string rn = h.substr(gitfly::consts::kRefPrefix.size());
        if (auto tip = gitfly::read_ref(repo.root(), rn); tip)
          commit_hex = *tip;
      } else
        commit_hex = h;
      if (!commit_hex.empty()) {
        auto info = repo.read_commit(commit_hex);
        left = gitfly::worktree::tree_to_map(repo, info.tree_hex);
      }
    }
    right = gitfly::worktree::index_to_map(repo.root());
  } else {
    // index vs working
    left = gitfly::worktree::index_to_map(repo.root());
    right = gitfly::worktree::build_working_map(repo.root());
  }

  // List all paths
  std::set<std::string> all;
  for (auto &[p, _] : left)
    all.insert(p);
  for (auto &[p, _] : right)
    all.insert(p);

  bool any = false;
  for (const auto &path : all) {
    const auto li = left.find(path);
    const auto ri = right.find(path);
    const bool in_l = li != left.end();
    const bool in_r = ri != right.end();
    if (in_l && in_r && li->second == ri->second)
      continue;
    any = true;
    std::vector<std::string> a, b;
    if (in_l)
      a = read_blob_lines(repo, li->second);
    if (in_r) {
      if (cached)
        b = read_blob_lines(repo, ri->second);
      else
        b = read_working_lines(repo.root(), path);
    }
    std::cout << gitfly::diff::unified_diff(a, b, path);
  }
  if (!any)
    std::cout << "(no differences)\n";
  return 0;
}
