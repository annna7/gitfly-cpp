#include "gitfly/worktree.hpp"

#include "gitfly/fs.hpp"
#include "gitfly/index.hpp"
#include "gitfly/repo.hpp"
#include "gitfly/util.hpp"

#include <filesystem>

namespace gfs = gitfly::fs;

namespace gitfly::worktree {

void enumerate_paths(const std::filesystem::path &root, std::set<std::string> &out_paths) {
  for (auto it = std::filesystem::recursive_directory_iterator(root);
       it != std::filesystem::recursive_directory_iterator(); ++it) {
    const auto &p = it->path();
    if (p.filename() == ".gitfly") {
      it.disable_recursion_pending();
      continue;
    }
    if (!it->is_regular_file()) {
      continue;
    }
    out_paths.insert(std::filesystem::relative(p, root).generic_string());
  }
}

PathOidMap build_working_map(const std::filesystem::path &root) {
  PathOidMap m;
  std::set<std::string> paths;
  enumerate_paths(root, paths);
  for (const auto &rel : paths) {
    auto bytes = gfs::read_file(root / rel);
    m[rel] = compute_blob_hex_oid(bytes);
  }
  return m;
}

PathOidMap index_to_map(const std::filesystem::path &root) {
  PathOidMap m;
  Index idx{root};
  idx.load();
  for (const auto &e : idx.entries())
    m[e.path] = to_hex(e.oid);
  return m;
}

static void tree_to_map_impl(const Repository &repo, const std::string &tree_hex,
                             const std::string &prefix, PathOidMap &out) {
  for (auto &e : repo.read_tree(tree_hex)) {
    if (e.mode == consts::kModeTree)
      tree_to_map_impl(repo, to_hex(e.id), prefix + e.name + "/", out);
    else
      out[prefix + e.name] = to_hex(e.id);
  }
}

PathOidMap tree_to_map(const Repository &repo, const std::string &tree_hex) {
  PathOidMap m;
  tree_to_map_impl(repo, tree_hex, "", m);
  return m;
}

void apply_snapshot(const Repository &repo, const PathOidMap &snapshot) {
  // Remove extra files first (anything not present in snapshot but present in working)
  std::set<std::string> working_paths;
  enumerate_paths(repo.root(), working_paths);
  for (const auto &p : working_paths) {
    if (!snapshot.contains(p))
      std::filesystem::remove(repo.root() / p);
  }
  // Write/update listed files
  for (const auto &[path, hex] : snapshot) {
    std::filesystem::create_directories((repo.root() / path).parent_path());
    auto bytes = repo.read_blob(hex);
    gfs::write_file_atomic(repo.root() / path, bytes);
  }
}

void write_index_snapshot(const Repository &repo, const PathOidMap &snapshot) {
  const auto &root = repo.root();
  Index idx{root};
  // Clear existing index by overwriting with empty content
  {
    std::vector<std::uint8_t> empty{};
    gfs::write_file_atomic(root / ".gitfly" / "index", empty);
  }
  // Re-add each path from the working tree (which should match snapshot)
  for (const auto &[path, _] : snapshot) {
    idx.add_path(root, path, repo, gitfly::consts::kModeFile);
  }
  idx.save();
}

} // namespace gitfly::worktree
