#include "gitfly/remote.hpp"
#include "gitfly/repo.hpp"
#include "gitfly/index.hpp"
#include "gitfly/refs.hpp"
#include "gitfly/worktree.hpp"
#include "gitfly/fs.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <stack>
#include <unordered_set>

namespace fs = std::filesystem;

static void write_file(const fs::path& p, std::string_view s) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << s;
}

static bool is_ancestor(const gitfly::Repository& repo, const std::string& ancestor,
                        const std::string& descendant) {
  if (ancestor == descendant) return true;
  std::stack<std::string> st; st.push(descendant);
  std::unordered_set<std::string> visited;
  while (!st.empty()) {
    auto cur = st.top(); st.pop();
    if (!visited.insert(cur).second) continue;
    auto info = repo.read_commit(cur);
    for (const auto& parent : info.parents) {
      if (parent == ancestor) return true;
      st.push(parent);
    }
  }
  return false;
}

int main() {
  const fs::path remote = fs::temp_directory_path() / ("gitfly_remote_pull_" + std::to_string(std::random_device{}()));
  const fs::path local  = fs::temp_directory_path() / ("gitfly_local_pull_"  + std::to_string(std::random_device{}()));
  fs::create_directories(remote);
  fs::create_directories(local);
  try {
    // Remote with one commit
    {
      gitfly::Repository repo{remote};
      repo.init(gitfly::Identity{"Remote", "r@example.com"});
      write_file(remote / "a.txt", "A\n");
      gitfly::Index idx{remote}; idx.load(); idx.add_path(remote, "a.txt", repo, 0100644); idx.save();
      (void)repo.commit_index("c1\n");
    }

    // Clone locally
    gitfly::remote::clone_repo(remote, local);
    gitfly::Repository lrepo{local};

    // Advance remote with a fast-forward change
    std::string new_tip;
    {
      gitfly::Repository rrepo{remote};
      write_file(remote / "b.txt", "B\n");
      gitfly::Index idx{remote}; idx.load(); idx.add_path(remote, "b.txt", rrepo, 0100644); idx.save();
      new_tip = rrepo.commit_index("c2\n");
    }

    // Fetch + integrate (simulate pull fast-forward)
    auto fres = gitfly::remote::fetch_head(local, remote, "origin");
    if (fres.tip.empty() || fres.branch != "master") { std::cerr << "fetch bad head\n"; return 1; }
    const std::string rn = gitfly::heads_ref("master");
    auto local_tip = gitfly::read_ref(local, rn);
    if (!local_tip) { std::cerr << "local tip missing\n"; return 1; }
    if (!is_ancestor(lrepo, *local_tip, fres.tip)) { std::cerr << "not a fast-forward\n"; return 1; }
    // Apply
    auto info = lrepo.read_commit(fres.tip);
    auto snap = gitfly::worktree::tree_to_map(lrepo, info.tree_hex);
    gitfly::worktree::apply_snapshot(lrepo, snap);
    gitfly::worktree::write_index_snapshot(lrepo, snap);
    gitfly::update_ref(local, rn, fres.tip);

    // Verify
    auto tip = gitfly::read_ref(local, rn);
    if (!tip || *tip != new_tip) { std::cerr << "pull ff: tip mismatch\n"; return 1; }
    auto a = gitfly::fs::read_file(local / "a.txt");
    auto b = gitfly::fs::read_file(local / "b.txt");
    if (a.empty() || b.empty()) { std::cerr << "pull ff: files missing\n"; return 1; }

    std::cout << "remote_pull OK\n";
  } catch (const std::exception& e) {
    std::cerr << "exception: " << e.what() << "\n";
    fs::remove_all(remote);
    fs::remove_all(local);
    return 1;
  }
  std::error_code ec; fs::remove_all(remote, ec); fs::remove_all(local, ec);
  return 0;
}
