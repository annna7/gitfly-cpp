#include "gitfly/index.hpp"
#include "gitfly/refs.hpp"
#include "gitfly/repo.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>

namespace fs = std::filesystem;

static void write_file(const fs::path &p, std::string_view s) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << s;
}

int main() {
  const fs::path root =
      fs::temp_directory_path() / ("gitfly_merge_" + std::to_string(std::random_device{}()));
  fs::create_directories(root);

  try {
    gitfly::Repository repo{root};
    repo.init(gitfly::Identity{.name = "User", .email = "u@example.com"});

    // initial commit on master
    write_file(root / "f.txt", "base\n");
    gitfly::Index idx{root};
    idx.load();
    idx.add_path(root, "f.txt", repo, gitfly::consts::kModeFile);
    idx.save();
    std::string c0 = repo.commit_index("c0\n");

    // create feature branch at c0
    auto head_txt = gitfly::read_HEAD(root);
    std::string rn = head_txt->substr(gitfly::consts::kRefPrefix.size());
    while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r'))
      rn.pop_back();
    gitfly::update_ref(root, gitfly::heads_ref("feature"), c0);

    // advance master by one commit
    write_file(root / "m.txt", "M\n");
    idx.load();
    idx.add_path(root, "m.txt", repo, gitfly::consts::kModeFile);
    idx.save();
    std::string cm = repo.commit_index("master\n");

    // advance feature by two commits from c0: simulate on-disk by checking out c0, but
    // simpler: directly write object and ref, then materialize via checkout to master again
    // Here: update feature ref to a new commit by making a temp repo object chain is complex;
    // instead reuse working dir: checkout feature, add file, commit, checkout master back.
    repo.checkout("feature");
    write_file(root / "f.txt", "feature\n");
    idx.load();
    idx.add_path(root, "f.txt", repo, gitfly::consts::kModeFile);
    idx.save();
    std::string cf = repo.commit_index("feature\n");

    // return to master and modify f.txt differently
    repo.checkout("master");
    write_file(root / "f.txt", "master\n");
    idx.load();
    idx.add_path(root, "f.txt", repo, gitfly::consts::kModeFile);
    idx.save();
    std::string cm2 = repo.commit_index("master-change\n");

    // Now master has cm2, feature has cf. Merge feature into master.
    bool conflict = false;
    try {
      repo.merge_branch("feature");
    } catch (...) {
      conflict = true;
    }
    // We modified f.txt differently in both branches => expect conflict
    if (!conflict) {
      std::cerr << "expected conflict in merge test\n";
      return 1;
    }
    const auto mh = root / ".gitfly" / "MERGE_HEAD";
    if (!fs::exists(mh)) {
      std::cerr << "MERGE_HEAD missing after conflict\n";
      return 1;
    }

    // Try to commit prematurely (guard should reject due to unstaged changes)
    bool threw = false;
    try {
      (void)repo.commit_index("premature\n");
    } catch (...) {
      threw = true;
    }
    if (!threw) {
      std::cerr << "commit should fail with MERGE_HEAD and unstaged changes\n";
      return 1;
    }

    // Resolve: write a resolved version, stage and commit using normal path
    write_file(root / "f.txt", "resolved\n");
    idx.load();
    idx.add_path(root, "f.txt", repo, gitfly::consts::kModeFile);
    idx.save();
    std::string mcommit = repo.commit_index("merge-resolved\n");

    // MERGE_HEAD should be cleared and merge commit should have two parents (cm2, cf)
    if (fs::exists(mh)) {
      std::cerr << "MERGE_HEAD not cleared after merge commit\n";
      return 1;
    }
    auto info = repo.read_commit(mcommit);
    if (info.parents.size() != 2) {
      std::cerr << "merge commit does not have two parents\n";
      return 1;
    }

    std::cout << "merge conflict + finalize OK\n";
  } catch (const std::exception &e) {
    std::cerr << "exception: " << e.what() << "\n";
    fs::remove_all(root);
    return 1;
  }

  std::error_code ec;
  fs::remove_all(root, ec);
  return 0;
}
