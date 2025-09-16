#include "gitfly/consts.hpp"
#include "gitfly/index.hpp"
#include "gitfly/repo.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

// Add a single filesystem path if it is a regular file.
static auto add_one(gitfly::Index &idx, const gitfly::Repository &repo, const fs::path &root,
                    const fs::path &relpath) -> bool {
  if (const fs::path abs = root / relpath; !fs::exists(abs) || !fs::is_regular_file(abs)) {
    std::cerr << "add: skipping non-regular file: " << relpath << "\n";
    return false;
  }
  const auto rel = relpath.generic_string();
  idx.add_path(root, rel, repo, gitfly::consts::kModeFile);
  std::cout << "added: " << rel << "\n";
  return true;
}

int cmd_add(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: gitfly add <path> [<path> ...]\n";
    return 2;
  }

  const fs::path root = fs::current_path();
  gitfly::Repository repo{root};
  if (!repo.is_initialized()) {
    std::cerr << "add: not a gitfly repo (run `gitfly init`)\n";
    return 1;
  }

  // Collect unique paths while preserving order
  std::vector<fs::path> paths;
  paths.reserve(static_cast<std::size_t>(argc) - 1);
  for (int i = 1; i < argc; ++i) {
    fs::path path = argv[i];
    if (std::ranges::find(paths, path) == paths.end()) {
      paths.push_back(std::move(path));
    }
  }

  gitfly::Index idx{root};
  try {
    idx.load();
    bool any = false;
    for (const auto &path : paths) {
      any = add_one(idx, repo, root, path) || any;
    }
    if (any) {
      idx.save();
    }
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "add: " << e.what() << "\n";
    return 1;
  }
}
