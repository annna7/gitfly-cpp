#include "gitfly/status.hpp"
#include "gitfly/fs.hpp"
#include "gitfly/index.hpp"
#include "gitfly/refs.hpp"
#include "gitfly/repo.hpp"
#include "gitfly/util.hpp"
#include "gitfly/worktree.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace gfs = gitfly::fs;

namespace gitfly {

// Reuse worktree helpers: build working/index maps and tree maps

static std::optional<std::string> head_tree_hex(const Repository &repo) {
  auto head_txt = read_HEAD(repo.root());
  if (!head_txt)
    return std::nullopt;

  std::string s = *head_txt;
  if (s.starts_with("ref:")) {
    // symbolic HEAD -> lookup ref
    std::string rn = s.substr(gitfly::consts::kRefPrefix.size());
    while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r')) {
      rn.pop_back();
    }
    if (auto cur = read_ref(repo.root(), rn); cur && looks_hex40(*cur)) {
      auto info = repo.read_commit(*cur);
      if (looks_hex40(info.tree_hex)) {
        return info.tree_hex;
      }
      return std::nullopt;
    }
    return std::nullopt;
  } 
       // detached HEAD contains commit hex directly
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
      s.pop_back();
    }
    auto info = repo.read_commit(s);
    if (looks_hex40(info.tree_hex)) {
      return info.tree_hex;
    }
    return std::nullopt;
}

// Since object_path_from_oid is private, we won’t parse the HEAD commit here.
// We’ll treat “missing HEAD tree” as empty baseline (initial repo). You still get useful status.

Status compute_status(const Repository &repo) {
  std::map<std::string, std::string> head_map;  // path -> hex oid
  std::map<std::string, std::string> index_map; // path -> hex oid
  std::map<std::string, std::string> work_map;  // path -> hex oid

  // HEAD baseline: if there’s a current commit, use its tree; else treat as empty
  // (Simpler: empty HEAD = all index entries are "Added" in staged set)
  // If you later expose a public "read_commit_tree_hex", switch to it here.

  // HEAD map
  if (auto tree_hex = head_tree_hex(repo)) {
    head_map = worktree::tree_to_map(repo, *tree_hex);
  }

  // Index + working
  index_map = worktree::index_to_map(repo.root());
  work_map = worktree::build_working_map(repo.root());

  Status st;

  // staged = HEAD vs index  (we use empty HEAD here)
  {
    std::set<std::string> all;
    for (auto &[p, _] : head_map) {
      all.insert(p);
    }
    for (auto &[p, _] : index_map) {
      all.insert(p);
    }

    for (const auto &path : all) {
      const auto it_h = head_map.find(path);
      const auto it_i = index_map.find(path);
      const bool in_h = it_h != head_map.end();
      if (const bool in_i = it_i != index_map.end(); in_h && in_i) {
        if (it_h->second != it_i->second)
          st.staged.push_back({ChangeKind::Modified, path});
      } else if (!in_h && in_i) {
        st.staged.push_back({ChangeKind::Added, path});
      } else if (in_h && !in_i) {
        st.staged.push_back({ChangeKind::Deleted, path});
      }
    }
  }

  // unstaged = working vs index
  {
    std::set<std::string> all;
    for (auto &[p, _] : work_map)
      all.insert(p);
    for (auto &[p, _] : index_map)
      all.insert(p);

    for (const auto &path : all) {
      const auto itW = work_map.find(path);
      const auto itI = index_map.find(path);
      const bool inW = itW != work_map.end();
      const bool inI = itI != index_map.end();
      if (inW && inI) {
        if (itW->second != itI->second)
          st.unstaged.push_back({ChangeKind::Modified, path});
      } else if (inI && !inW) {
        st.unstaged.push_back({ChangeKind::Deleted, path});
      }
    }
  }

  // untracked = working - index
  for (auto &[p, _] : work_map) {
    if (!index_map.contains(p))
      st.untracked.push_back(p);
  }
  std::ranges::sort(st.untracked);
  return st;
}

} // namespace gitfly
