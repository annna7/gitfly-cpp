#include <filesystem>
#include <iostream>
#include <string>
#include <set>

#include "gitfly/repo.hpp"
#include "gitfly/refs.hpp"
#include "gitfly/remote.hpp"
#include "gitfly/tcp_remote.hpp"
#include "gitfly/worktree.hpp"

static bool is_ancestor(const gitfly::Repository& repo, const std::string& anc, const std::string& desc) {
  if (anc == desc) return true;
  std::vector<std::string> st{desc};
  std::set<std::string> seen;
  while (!st.empty()) {
    auto cur = st.back(); st.pop_back();
    if (!seen.insert(cur).second) continue;
    auto info = repo.read_commit(cur);
    for (auto& p : info.parents) { if (p==anc) return true; st.push_back(p);} }
  return false;
}

int cmd_pull(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: gitfly pull <remote> [<name>]\n";
    return 2;
  }
  std::string remote = argv[1];
  std::string name = (argc >= 3 ? argv[2] : std::string("origin"));
  gitfly::Repository repo{std::filesystem::current_path()};
  if (!repo.is_initialized()) { std::cerr << "pull: not a gitfly repo\n"; return 1; }
  // Must be on a branch
  auto head_txt = gitfly::read_HEAD(repo.root());
  if (!head_txt || head_txt->rfind("ref:",0)!=0) { std::cerr << "pull: detached HEAD not supported\n"; return 1; }
  std::string rn = head_txt->substr(gitfly::consts::kRefPrefix.size()); while (!rn.empty() && (rn.back()=='\n'||rn.back()=='\r')) rn.pop_back();
  const std::string prefix = "refs/heads/";
  std::string cur_branch = rn.rfind(prefix,0)==0 ? rn.substr(prefix.size()) : rn;

  try {
    gitfly::remote::FetchResult fres;
    if (remote.rfind("tcp://", 0) == 0) {
      auto rest = remote.substr(6);
      auto colon = rest.find(':');
      std::string host = rest.substr(0, colon);
      int port = colon==std::string::npos ? gitfly::consts::portNumber : std::stoi(rest.substr(colon+1));
      auto tres = gitfly::tcpremote::fetch_head(host, port, repo.root().string(), name);
      fres.branch = std::move(tres.branch); fres.tip = std::move(tres.tip);
    } else {
      fres = gitfly::remote::fetch_head(repo.root(), remote, name);
    }
    if (fres.branch == "DETACHED" || fres.tip.empty()) { std::cerr << "pull: remote HEAD is detached or empty\n"; return 1; }
    // Fast-forward or merge
    auto local_tip = gitfly::read_ref(repo.root(), rn);
    if (!local_tip) { std::cerr << "pull: current branch has no tip\n"; return 1; }
    if (is_ancestor(repo, *local_tip, fres.tip)) {
      // FF: materialize and update ref
      auto info = repo.read_commit(fres.tip);
      auto tgt  = gitfly::worktree::tree_to_map(repo, info.tree_hex);
      gitfly::worktree::apply_snapshot(repo, tgt);
      gitfly::worktree::write_index_snapshot(repo, tgt);
      gitfly::update_ref(repo.root(), rn, fres.tip);
      std::cout << "Fast-forwarded to " << fres.tip.substr(0,7) << "\n";
      return 0;
    } 
          // Merge: create temp branch pointing to fetched tip and use merge_branch
      std::string tmp = ".pull_merge_tmp";
      gitfly::update_ref(repo.root(), gitfly::heads_ref(tmp), fres.tip);
      try {
        repo.merge_branch(tmp);
      } catch (const std::exception& e) {
        std::cerr << "merge: " << e.what() << "\n";
        return 1;
      }
      // remove temp ref
      std::error_code ec; std::filesystem::remove(repo.heads_dir() / tmp, ec);
      std::cout << "Merged remote changes from " << name << "/" << fres.branch << "\n";
      return 0;
   
  } catch (const std::exception& e) {
    std::cerr << "pull: " << e.what() << "\n";
    return 1;
  }
}
