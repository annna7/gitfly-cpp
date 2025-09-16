#include "gitfly/remote.hpp"

#include "gitfly/consts.hpp"
#include "gitfly/refs.hpp"
#include "gitfly/repo.hpp"
#include "gitfly/util.hpp"
#include "gitfly/worktree.hpp"

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace {

// Trim trailing '\n' or '\r' (HEAD files end with a newline)
inline std::string rstrip_newlines(std::string s) {
  gitfly::strutil::rstrip_newlines(s);
  return s;
}

// If HEAD is symbolic ("ref: <name>"), return that ref name; otherwise std::nullopt.
inline std::optional<std::string> head_symbolic_ref(const fs::path &repo_root) {
  const auto head_txt = gitfly::read_HEAD(repo_root);
  if (!head_txt) {
    return std::nullopt;
  }
  std::string s = rstrip_newlines(*head_txt);
  if (s.starts_with(gitfly::consts::kRefPrefix)) {
    return s.substr(std::string(gitfly::consts::kRefPrefix).size()); // after "ref: "
  }
  return std::nullopt;
}

// Return the 40-hex HEAD commit if available (symbolic or detached); else empty string.
inline std::string head_commit_hex(const fs::path &repo_root) {
  const auto head_txt = gitfly::read_HEAD(repo_root);
  if (!head_txt) {
    return {};
  }
  std::string s = rstrip_newlines(*head_txt);
  if (s.starts_with(gitfly::consts::kRefPrefix)) {
    const std::string refname = s.substr(std::string(gitfly::consts::kRefPrefix).size());
    if (auto tip = gitfly::read_ref(repo_root, refname)) {
      return *tip;
    }
    return {};
  }
  // detached
  return s;
}

// Naive ancestor check moved to Repository::is_commit_ancestor

// Copy all loose objects (files) missing in dst from src.
// src_obj = <repo>/.gitfly/objects ; dst_obj likewise.
inline void copy_missing_objects(const fs::path &src_obj, const fs::path &dst_obj) {
  for (auto it = fs::recursive_directory_iterator(src_obj);
       it != fs::recursive_directory_iterator(); ++it) {
    if (!it->is_regular_file()) {
      continue;
    }
    const fs::path rel = fs::relative(it->path(), src_obj);
    const fs::path out = dst_obj / rel;
    fs::create_directories(out.parent_path());
    if (!fs::exists(out))
      fs::copy_file(it->path(), out);
  }
}

} // namespace

namespace gitfly::remote {

void clone_repo(const fs::path &src, const fs::path &dst) {
  // Minimal validation
  if (!fs::exists(src / gitfly::consts::kGitDir)) {
    throw std::runtime_error("source is not a gitfly repo");
  }

  // Create destination and copy control dir
  fs::create_directories(dst);
  fs::copy(src / gitfly::consts::kGitDir, dst / gitfly::consts::kGitDir,
           fs::copy_options::recursive | fs::copy_options::skip_existing);

  // Materialize working tree at destination (if there’s a commit)
  Repository repo_dst{dst};
  const std::string commit_hex = head_commit_hex(dst);
  if (commit_hex.empty()) {
    return; // empty repo (no commits)
  }
  const auto info = repo_dst.read_commit(commit_hex);
  const auto snapshot = worktree::tree_to_map(repo_dst, info.tree_hex);
  worktree::apply_snapshot(repo_dst, snapshot);
  worktree::write_index_snapshot(repo_dst, snapshot);
}

void push_branch(const fs::path &local, const fs::path &remote, const std::string &branch) {
  Repository rlocal{local};
  Repository rremote{remote};
  if (!rlocal.is_initialized() || !rremote.is_initialized()) {
    throw std::runtime_error("both repos must be initialized");
  }

  // Require symbolic HEAD that matches the branch being pushed
  const std::string refname = heads_ref(branch);
  auto head_sym = head_symbolic_ref(local);
  if (!head_sym) {
    throw std::runtime_error("push requires symbolic HEAD");
  }
  std::string curref = rstrip_newlines(*head_sym);
  if (curref != refname) {
    throw std::runtime_error("current branch does not match push branch");
  }

  // Resolve tips
  const auto local_tip = read_ref(local, refname);
  if (!local_tip) {
    throw std::runtime_error("local branch has no tip");
  }
  const auto remote_tip = read_ref(remote, refname);

  // Fast-forward check
  if (remote_tip && !rlocal.is_commit_ancestor(*remote_tip, *local_tip)) {
    throw std::runtime_error("non-fast-forward");
  }

  // Copy missing objects
  copy_missing_objects(rlocal.objects_dir(), rremote.objects_dir());

  // Update remote ref
  update_ref(remote, refname, *local_tip);
}

FetchResult fetch_head(const fs::path &local, const fs::path &remote, const std::string &name) {
  Repository rlocal{local};
  Repository rremote{remote};
  if (!rlocal.is_initialized() || !rremote.is_initialized()) {
    throw std::runtime_error("both repos must be initialized");
  }

  // Determine remote “HEAD branch” & tip
  std::string branch = "DETACHED";
  std::string tip;

  if (const auto head_txt = read_HEAD(remote)) {
    std::string s = rstrip_newlines(*head_txt);
    if (s.starts_with(gitfly::consts::kRefPrefix)) {
      const std::string rn =
          s.substr(std::string(gitfly::consts::kRefPrefix).size()); // e.g., "refs/heads/master"
      const std::string heads_prefix = std::string(gitfly::consts::kRefsDir) + "/" +
                                       std::string(gitfly::consts::kHeadsDir) + "/";
      branch = (rn.starts_with(heads_prefix)) ? rn.substr(heads_prefix.size()) : rn;
      if (auto t = read_ref(remote, rn)) {
        tip = *t;
      }
    } else {
      // detached
      tip = s;
    }
  }

  // Bring over missing objects
  copy_missing_objects(rremote.objects_dir(), rlocal.objects_dir());

  // Update remote-tracking ref if we know the branch & tip
  if (!tip.empty() && branch != "DETACHED") {
    fs::create_directories(rlocal.refs_dir() / "remotes" / name);
    update_ref(local, std::string(gitfly::consts::kRefsDir) + "/remotes/" + name + "/" + branch,
               tip);
  }

  return FetchResult{.branch = branch, .tip = tip};
}

} // namespace gitfly::remote
