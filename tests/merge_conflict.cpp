#include "gitfly/repo.hpp"
#include "gitfly/index.hpp"
#include "gitfly/refs.hpp"
#include "gitfly/status.hpp"
#include "gitfly/diff.hpp"
#include "gitfly/fs.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

namespace fs = std::filesystem;

static void write_file(const fs::path& p, std::string_view s) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << s;
}

static bool contains(const std::string& hay, const std::string& needle) {
  return hay.find(needle) != std::string::npos;
}
 
int main() {
  const fs::path root = fs::temp_directory_path() / ("gitfly_merge_conflict_" + std::to_string(std::random_device{}()));
  fs::create_directories(root);
  try {
    gitfly::Repository repo{root};
    repo.init(gitfly::Identity{"User", "u@example.com"});

    // Base commit
    write_file(root / "c.txt", "base\n");
    gitfly::Index idx{root}; idx.load(); idx.add_path(root, "c.txt", repo, gitfly::consts::kModeFile); idx.save();
    (void)repo.commit_index("c0\n");

    // Feature branch change
    gitfly::update_ref(root, gitfly::heads_ref("feature"), gitfly::read_ref(root, gitfly::heads_ref("master")).value());
    repo.checkout("feature");
    write_file(root / "c.txt", "feature\n");
    idx.load(); idx.add_path(root, "c.txt", repo, gitfly::consts::kModeFile); idx.save();
    (void)repo.commit_index("cf\n");

    // Master change
    repo.checkout("master");
    write_file(root / "c.txt", "master\n");
    idx.load(); idx.add_path(root, "c.txt", repo, gitfly::consts::kModeFile); idx.save();
    (void)repo.commit_index("cm\n");

    // Merge -> expect conflict and MERGE_HEAD
    bool threw = false;
    try { repo.merge_branch("feature"); } catch (...) { threw = true; }
    if (!threw) { std::cerr << "expected conflict but merge succeeded\n"; return 1; }
    if (!fs::exists(root / ".gitfly" / "MERGE_HEAD")) { std::cerr << "MERGE_HEAD missing after conflict\n"; return 1; }

    // Conflict markers present in c.txt
    auto bytes = gitfly::fs::read_file(root / "c.txt");
    std::string content(bytes.begin(), bytes.end());
    if (!contains(content, "<<<<<<< HEAD") || !contains(content, "=======") || !contains(content, ">>>>>>> feature")) {
      std::cerr << "conflict markers missing in file\n"; return 1;
    }
    if (!contains(content, "master") || !contains(content, "feature")) {
      std::cerr << "conflict sides missing in file\n"; return 1;
    }

    // Status should show untracked (we excluded conflicted paths from index snapshot)
    auto st = gitfly::compute_status(repo);
    bool untracked_has = false;
    for (auto& p : st.untracked) {
      if (p == "c.txt") { 
        untracked_has = true;
      }
    }
    if (!untracked_has) { std::cerr << "status: c.txt not untracked in conflict state\n"; return 1; }

    // Diff engine sanity: master vs feature single-line change
    {
      auto a = gitfly::diff::split_lines("master\n");
      auto b = gitfly::diff::split_lines("feature\n");
      auto ud = gitfly::diff::unified_diff(a, b, "c.txt");
      if (!contains(ud, "-master") || !contains(ud, "+feature")) {
        std::cerr << "unified diff missing +/- lines\n"; return 1;
      }
    }

    std::cout << "merge conflict OK\n";
  } catch (const std::exception& e) {
    std::cerr << "exception: " << e.what() << "\n";
    fs::remove_all(root);
    return 1;
  }
  std::error_code ec; fs::remove_all(root, ec);
  return 0;
}

