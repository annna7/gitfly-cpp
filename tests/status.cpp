#include "gitfly/status.hpp"

#include "gitfly/index.hpp"
#include "gitfly/repo.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

namespace fs = std::filesystem;

static void write_file(const fs::path &p, std::string_view s) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << s;
}

static bool has_change(const std::vector<gitfly::Change> &xs, gitfly::ChangeKind k,
                       std::string_view path) {
  for (auto &c : xs)
    if (c.kind == k && c.path == path)
      return true;
  return false;
}

int main() {
  const fs::path root =
      fs::temp_directory_path() / ("gitfly_status_" + std::to_string(std::random_device{}()));
  fs::create_directories(root);

  try {
    gitfly::Repository repo{root};
    repo.init(gitfly::Identity{.name = "User", .email = "u@example.com"});

    // 1) Create a file and stage it; no HEAD yet => staged Added
    write_file(root / "a.txt", "hello\n");
    gitfly::Index idx{root};
    idx.load();
    idx.add_path(root, "a.txt", repo, gitfly::consts::kModeFile);
    idx.save();
    {
      auto st = gitfly::compute_status(repo);
      if (!has_change(st.staged, gitfly::ChangeKind::Added, "a.txt")) {
        std::cerr << "expected staged Added a.txt (initial)\n";
        return 1;
      }
      if (!st.unstaged.empty()) {
        std::cerr << "unexpected unstaged changes (initial)\n";
        return 1;
      }
      if (!st.untracked.empty()) {
        std::cerr << "unexpected untracked (initial)\n";
        return 1;
      }
    }

    // Commit -> clean
    repo.commit_index("first\n");
    {
      auto st = gitfly::compute_status(repo);
      if (!st.staged.empty() || !st.unstaged.empty() || !st.untracked.empty()) {
        std::cerr << "expected clean status after commit\n";
        return 1;
      }
    }

    // 2) Modify a tracked file -> unstaged Modified
    write_file(root / "a.txt", "hello world\n");
    {
      auto st = gitfly::compute_status(repo);
      if (!has_change(st.unstaged, gitfly::ChangeKind::Modified, "a.txt")) {
        std::cerr << "expected unstaged Modified a.txt\n";
        return 1;
      }
    }

    // Stage the modification -> staged Modified
    idx.load();
    idx.add_path(root, "a.txt", repo, gitfly::consts::kModeFile);
    idx.save();
    {
      auto st = gitfly::compute_status(repo);
      if (!has_change(st.staged, gitfly::ChangeKind::Modified, "a.txt")) {
        std::cerr << "expected staged Modified a.txt\n";
        return 1;
      }
      if (!st.unstaged.empty()) {
        std::cerr << "unexpected unstaged after stage\n";
        return 1;
      }
    }

    // 3) Add an untracked file -> appears in untracked
    write_file(root / "b.txt", "B\n");
    {
      auto st = gitfly::compute_status(repo);
      bool found = false;
      for (auto &p : st.untracked)
        if (p == "b.txt")
          found = true;
      if (!found) {
        std::cerr << "expected untracked b.txt\n";
        return 1;
      }
    }

    // Stage b.txt -> now staged Added
    idx.load();
    idx.add_path(root, "b.txt", repo, gitfly::consts::kModeFile);
    idx.save();
    {
      auto st = gitfly::compute_status(repo);
      if (!has_change(st.staged, gitfly::ChangeKind::Added, "b.txt")) {
        std::cerr << "expected staged Added b.txt\n";
        return 1;
      }
      bool found = false;
      for (auto &p : st.untracked)
        if (p == "b.txt")
          found = true;
      if (found) {
        std::cerr << "b.txt should not be untracked after stage\n";
        return 1;
      }
    }

    // 4) Delete a tracked file from working -> unstaged Deleted
    fs::remove(root / "a.txt");
    {
      auto st = gitfly::compute_status(repo);
      if (!has_change(st.unstaged, gitfly::ChangeKind::Deleted, "a.txt")) {
        std::cerr << "expected unstaged Deleted a.txt\n";
        return 1;
      }
    }

    // Stage the deletion (remove from index) -> staged Deleted
    idx.load();
    idx.remove_path("a.txt");
    idx.save();
    {
      auto st = gitfly::compute_status(repo);
      if (!has_change(st.staged, gitfly::ChangeKind::Deleted, "a.txt")) {
        std::cerr << "expected staged Deleted a.txt\n";
        return 1;
      }
      if (!has_change(st.staged, gitfly::ChangeKind::Added, "b.txt")) {
        std::cerr << "expected staged Added b.txt still present\n";
        return 1;
      }
    }

    std::cout << "status OK\n";
  } catch (const std::exception &e) {
    std::cerr << "exception: " << e.what() << "\n";
    fs::remove_all(root);
    return 1;
  }

  std::error_code ec;
  fs::remove_all(root, ec);
  return 0;
}
