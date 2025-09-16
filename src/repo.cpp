#include "gitfly/repo.hpp"

#include "gitfly/consts.hpp"
#include "gitfly/fs.hpp"
#include "gitfly/index.hpp"
#include "gitfly/object_store.hpp"
#include "gitfly/refs.hpp"
#include "gitfly/status.hpp"
#include "gitfly/time.hpp"
#include "gitfly/util.hpp"
#include "gitfly/worktree.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace stdfs = std::filesystem;
namespace gfs   = gitfly::fs;

namespace {
[[nodiscard]] auto split_first(std::string_view path)
    -> std::pair<std::string, std::string> {
  const std::size_t pos = path.find('/');
  if (pos == std::string_view::npos) {
    return {std::string(path), std::string{}};
  }
  return {std::string(path.substr(0, pos)), std::string(path.substr(pos + 1))};
}
} // namespace

namespace gitfly {

Repository::Repository(stdfs::path root) : root_(std::move(root)) {}

auto Repository::is_initialized() const -> bool { return stdfs::exists(git_dir()); }

void Repository::init(const Identity& identity) const {
  if (is_initialized()) {
    throw std::runtime_error("A gitfly repository already exists at: " + git_dir().string());
  }

  std::error_code ec;
  stdfs::create_directories(objects_dir(), ec);
  if (ec) {
    throw std::runtime_error("create objects dir failed: " + ec.message());
  }
  stdfs::create_directories(heads_dir(), ec);
  if (ec) {
     throw std::runtime_error("create refs/heads dir failed: " + ec.message());
  }
  stdfs::create_directories(tags_dir(), ec);
  if (ec) {
    throw std::runtime_error("create refs/tags dir failed: " + ec.message());
  }
 
  set_HEAD_symbolic(root_, heads_ref(std::string(consts::kDefaultBranch)));
  save_identity(root_, identity);
}

// Paths

auto Repository::object_path_from_oid(const oid& id) const -> stdfs::path {
  const ObjectStore store{git_dir()};
  return store.path_for_oid(id);
}

// Modes

auto Repository::mode_to_ascii_octal(std::uint32_t mode) -> std::string {
  // snprintf is fine here; octal via std::to_chars is not available
  std::array<char, 16> buf{};
  std::snprintf(buf.data(), buf.size(), "%o", mode);
  return {buf.data()};
}

auto Repository::ascii_octal_to_mode(std::string_view s) -> std::uint32_t {
  std::uint32_t v = 0;
  for (const char c : s) {
    if (c < '0' || c > '7') {
      break;
    }
    v = static_cast<std::uint32_t>((v << 3U) + static_cast<unsigned>(c - '0'));
  }
  return v;
}

// Blobs

auto Repository::write_blob(std::span<const std::uint8_t> bytes) const -> std::string {
  ObjectStore store{git_dir()};
  return store.write(consts::kTypeBlob, bytes);
}

auto Repository::read_blob(std::string_view hex_oid) const -> std::vector<std::uint8_t> {
  const ObjectStore store{git_dir()};
  const auto [type, data] = store.read(hex_oid);
  if (type != consts::kTypeBlob) {
    throw std::runtime_error("object is not a blob");
  }
  return data;
}

// Trees (binary)

auto Repository::write_tree(const std::vector<TreeEntry>& entries_in) const -> std::string {
  auto entries = entries_in;
  std::ranges::sort(entries,
                    [](const TreeEntry& a, const TreeEntry& b) { return a.name < b.name; });

  std::string data;
  // (No magic reserve constant; let it grow or compute if you like.)

  for (const auto& e : entries) {
    const std::string mode = mode_to_ascii_octal(e.mode);
    data.append(mode);
    data.push_back(consts::kSpace);
    data.append(e.name);
    data.push_back(consts::kNul);
    data.append(reinterpret_cast<const char*>(e.id.data()),
                static_cast<std::size_t>(consts::kOidRawLen));
  }

  const ObjectStore store{git_dir()};
  const auto payload =
      std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(data.data()),
                                    data.size());
  return store.write(consts::kTypeTree, payload);
}

auto Repository::read_tree(std::string_view hex_oid) const -> std::vector<TreeEntry> {
  const ObjectStore os{git_dir()};
  const auto [type, data] = os.read(hex_oid);
  if (type != consts::kTypeTree) {
    throw std::runtime_error("object is not a tree");
  }

  std::vector<TreeEntry> out;
  auto p   = data.begin();
  const auto end = data.end();

  while (p < end) {
    const auto q_space = std::find(p, end, static_cast<std::uint8_t>(consts::kSpace));
    if (q_space == end) {
      throw std::runtime_error("tree parse: expected space");
    }
    const std::string mode_str(p, q_space);
    const std::uint32_t mode = ascii_octal_to_mode(mode_str);

    p = q_space + 1;
    const auto q_nul = std::find(p, end, static_cast<std::uint8_t>(consts::kNul));
    if (q_nul == end) {
      throw std::runtime_error("tree parse: expected NUL");
    }
    std::string name(p, q_nul);
    p = q_nul + 1;

    if (static_cast<std::size_t>(end - p) < consts::kOidRawLen) {
      throw std::runtime_error("tree parse: truncated oid");
    }

    TreeEntry e{};
    e.mode = mode;
    e.name = std::move(name);
    std::memcpy(e.id.data(), &(*p), consts::kOidRawLen);
    p += static_cast<std::ptrdiff_t>(consts::kOidRawLen);

    out.push_back(std::move(e));
  }
  return out;
}

// Commits

auto Repository::write_commit(std::string_view tree_hex,
                              const std::vector<std::string>& parent_hexes,
                              std::string_view author_line,
                              std::string_view committer_line,
                              std::string_view message) const -> std::string {
  std::string txt;
  // Avoid a magic reserve; small strings will optimize anyway.

  txt += std::string(consts::kTreePrefix);
  txt += std::string(tree_hex);
  txt += '\n';

  for (const auto& p : parent_hexes) {
    txt += std::string(consts::kParentPrefix);
    txt += p;
    txt += '\n';
  }

  txt += std::string(consts::kAuthorPrefix);
  txt += std::string(author_line);
  txt += '\n';

  txt += std::string(consts::kCommitterPrefix);
  txt += std::string(committer_line);
  txt += "\n\n";

  txt += std::string(message);

  const ObjectStore store{git_dir()};
  const auto payload =
      std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(txt.data()),
                                    txt.size());
  return store.write(consts::kTypeCommit, payload);
}

auto Repository::read_commit(std::string_view commit_hex) const -> CommitInfo {
  const ObjectStore store{git_dir()};
  const auto obj = store.read(commit_hex);
  if (obj.type != consts::kTypeCommit) {
    throw std::runtime_error("object is not a commit");
  }
  const std::string text(obj.data.begin(), obj.data.end());

  CommitInfo info{};
  std::size_t pos = 0;

  for (;;) {
    const std::size_t nl = text.find('\n', pos);
    const std::string line =
        (nl == std::string::npos) ? text.substr(pos) : text.substr(pos, nl - pos);

    if (line.empty()) {
      if (nl != std::string::npos) {
        info.message = text.substr(nl + 1);
      }
      break;
    }

    if (line.rfind(consts::kTreePrefix, 0) == 0) {
      info.tree_hex = line.substr(consts::kTreePrefix.size(), consts::kOidHexLen);
    } else if (line.rfind(consts::kParentPrefix, 0) == 0) {
      info.parents.push_back(line.substr(consts::kParentPrefix.size(), consts::kOidHexLen));
    } else if (line.rfind(consts::kAuthorPrefix, 0) == 0) {
      info.author = line.substr(consts::kAuthorPrefix.size());
    } else if (line.rfind(consts::kCommitterPrefix, 0) == 0) {
      info.committer = line.substr(consts::kCommitterPrefix.size());
    }

    if (nl == std::string::npos) break;
    pos = nl + 1;
  }

  return info;
}

auto Repository::is_commit_ancestor(std::string_view ancestor_hex,
                                    std::string_view descendant_hex) const -> bool {
  if (ancestor_hex == descendant_hex) return true;
  std::vector<std::string> stack{std::string(descendant_hex)};
  std::set<std::string> seen;
  while (!stack.empty()) {
    const auto cur = stack.back();
    stack.pop_back();
    if (!seen.insert(cur).second) continue;
    const auto info = read_commit(cur);
    for (const auto &p : info.parents) {
      if (p == ancestor_hex) return true;
      stack.push_back(p);
    }
  }
  return false;
}

auto Repository::write_tree_from_index() const -> std::string {
  Index idx{root_};
  idx.load();
  const auto& ents = idx.entries();

  const auto build = [&](const auto& self, const std::vector<IndexEntry>& group) -> std::string {
    struct PendingFile {
      std::string   name;
      std::uint32_t mode{};
      oid           id{};
    };

    std::map<std::string, std::vector<IndexEntry>> subdirs; // dirname -> child entries
    std::vector<PendingFile> files;

    for (const auto& e : group) {
      const auto [first, rest] = split_first(e.path);
      if (rest.empty()) {
        files.push_back(PendingFile{first, e.mode, e.oid});
      } else {
        IndexEntry child = e;
        child.path = rest;
        subdirs[first].push_back(std::move(child));
      }
    }

    std::vector<TreeEntry> tree_entries;
    tree_entries.reserve(files.size() + subdirs.size());

    for (const auto& f : files) {
      TreeEntry te{};
      te.mode = f.mode;
      te.name = f.name;
      te.id   = f.id;
      tree_entries.push_back(std::move(te));
    }

    for (auto& [dirname, child_entries] : subdirs) {
      const std::string subtree_hex = self(self, child_entries);

      oid subtree_oid{};
      if (!from_hex(subtree_hex, subtree_oid)) {
        throw std::runtime_error("bad subtree hex oid");
      }

      TreeEntry te{};
      te.mode = consts::kModeTree;
      te.name = dirname;
      te.id   = subtree_oid;
      tree_entries.push_back(std::move(te));
    }

    return write_tree(tree_entries);
  };

  return build(build, ents);
}

auto Repository::commit_index(std::string_view message) const -> std::string {
  if (!is_initialized()) {
    throw std::runtime_error("Not a gitfly repository (missing .gitfly)");
  }

  const std::string tree_hex = write_tree_from_index();

  std::vector<std::string> parents;
  const auto head_txt = read_HEAD(root_);
  if (head_txt && head_txt->rfind("ref:", 0) == 0) {
    std::string rn = head_txt->substr(std::string("ref: ").size());
    while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r')) rn.pop_back();
    if (const auto cur = read_ref(root_, rn); cur && cur->size() == consts::kOidHexLen) {
      parents.push_back(*cur);
    }
  } else if (head_txt) {
    std::string hex = *head_txt;
    while (!hex.empty() && (hex.back() == '\n' || hex.back() == '\r')) hex.pop_back();
    if (hex.size() == consts::kOidHexLen) parents.push_back(hex);
  }

  // Include MERGE_HEAD (if present) as an additional parent for merge finalization
  const auto merge_head = git_dir() / consts::kMergeHead;
  bool had_merge_head = false;
  if (stdfs::exists(merge_head)) {
    const auto bytes = gfs::read_file(merge_head);
    std::string mh(bytes.begin(), bytes.end());
    while (!mh.empty() && (mh.back() == '\n' || mh.back() == '\r')) mh.pop_back();
    if (looks_hex40(mh)) {
      if (parents.empty() || parents.front() != mh) parents.push_back(mh);
      had_merge_head = true;
    }
  }

  if (had_merge_head) {
    const auto st = compute_status(*this);
    if (!st.unstaged.empty() || !st.untracked.empty()) {
      throw std::runtime_error("cannot commit: merge in progress, unresolved paths present");
    }
  }

  const Identity id = load_identity(root_);
  const std::time_t now = std::time(nullptr);
  const int tz_min = timeutil::local_utc_offset_minutes(now);
  const std::string sig = timeutil::make_signature(id, now, tz_min);

  const std::string commit_hex = write_commit(tree_hex, parents, sig, sig, message);

  if (head_txt && head_txt->rfind("ref:", 0) == 0) {
    std::string rn = head_txt->substr(std::string("ref: ").size());
    while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r')) rn.pop_back();
    update_ref(root_, rn, commit_hex);
  } else {
    set_HEAD_detached(root_, commit_hex);
  }

  if (had_merge_head) {
    std::error_code ec;
    stdfs::remove(merge_head, ec);
  }

  return commit_hex;
}

auto Repository::commit_index_with_parents(std::string_view message,
                                           const std::vector<std::string>& extra_parents) const
    -> std::string {
  if (!is_initialized()) {
    throw std::runtime_error("Not a gitfly repository (missing .gitfly)");
  }

  const std::string tree_hex = write_tree_from_index();

  std::vector<std::string> parents;
  const auto head_txt = read_HEAD(root_);
  if (head_txt && head_txt->rfind("ref:", 0) == 0) {
    std::string rn = head_txt->substr(std::string("ref: ").size());
    while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r')) rn.pop_back();
    if (const auto cur = read_ref(root_, rn); cur && cur->size() == consts::kOidHexLen) {
      parents.push_back(*cur);
    }
  } else if (head_txt) {
    std::string hex = *head_txt;
    while (!hex.empty() && (hex.back() == '\n' || hex.back() == '\r')) hex.pop_back();
    if (hex.size() == consts::kOidHexLen) parents.push_back(hex);
  }
  parents.insert(parents.end(), extra_parents.begin(), extra_parents.end());

  const Identity id = load_identity(root_);
  const std::time_t now = std::time(nullptr);
  const int tz_min = timeutil::local_utc_offset_minutes(now);
  const std::string sig = timeutil::make_signature(id, now, tz_min);

  const std::string commit_hex = write_commit(tree_hex, parents, sig, sig, message);

  if (head_txt && head_txt->rfind("ref:", 0) == 0) {
    std::string rn = head_txt->substr(std::string("ref: ").size());
    while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r')) rn.pop_back();
    update_ref(root_, rn, commit_hex);
  } else {
    set_HEAD_detached(root_, commit_hex);
  }
  return commit_hex;
}

void Repository::checkout(std::string_view target) const {
  if (!is_initialized()) {
    throw std::runtime_error("not a gitfly repo (run `gitfly init`)");
  }

  // Require clean working tree vs index (simple clobber protection)
  const auto working_map = worktree::build_working_map(root_);
  const auto idx_map     = worktree::index_to_map(root_);
  {
    std::set<std::string> all;
    for (const auto& [p, _] : working_map) all.insert(p);
    for (const auto& [p, _] : idx_map)     all.insert(p);
    for (const auto& p : all) {
      const auto itW = working_map.find(p);
      const auto itI = idx_map.find(p);
      const std::string w = (itW == working_map.end()) ? std::string{} : itW->second;
      const std::string i = (itI == idx_map.end())     ? std::string{} : itI->second;
      if (w != i) {
        throw std::runtime_error("checkout aborted: unstaged changes present");
      }
    }
  }

  // Resolve target
  std::string commit_hex;
  std::string branch_ref;
  if (looks_hex40(target)) {
    commit_hex = std::string(target);
  } else {
    branch_ref = heads_ref(std::string(target));
    const auto tip = read_ref(root_, branch_ref);
    if (!tip) throw std::runtime_error("unknown branch: " + std::string(target));
    commit_hex = *tip;
  }

  const auto cinfo = read_commit(commit_hex);
  if (cinfo.tree_hex.size() != consts::kOidHexLen) {
    throw std::runtime_error("commit missing tree");
  }

  const auto snapshot = worktree::tree_to_map(*this, cinfo.tree_hex);
  worktree::apply_snapshot(*this, snapshot);
  worktree::write_index_snapshot(*this, snapshot);

  if (!branch_ref.empty()) {
    set_HEAD_symbolic(root_, branch_ref);
  } else {
    set_HEAD_detached(root_, commit_hex);
  }
}

// ------- Merge helpers -------

namespace {

[[nodiscard]] auto head_current_branch(const stdfs::path& root) -> std::string {
  const auto head_txt = read_HEAD(root);
  if (!head_txt || head_txt->rfind("ref:", 0) != 0) return {};
  std::string rn = head_txt->substr(std::string("ref: ").size());
  while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r')) rn.pop_back();
  return rn; // "refs/heads/<name>"
}

// (no longer needed here; exposed as Repository::is_commit_ancestor)

[[nodiscard]] auto lca_commit(const Repository& repo,
                              const std::string& a,
                              const std::string& b) -> std::optional<std::string> {
  std::set<std::string> ancestors_a;
  {
    std::vector<std::string> stack{a};
    while (!stack.empty()) {
      const auto cur = stack.back();
      stack.pop_back();
      if (!ancestors_a.insert(cur).second) continue;
      const auto info = repo.read_commit(cur);
      for (const auto& p : info.parents) stack.push_back(p);
    }
  }
  std::set<std::string> seen_b;
  std::vector<std::string> stack_b{b};
  while (!stack_b.empty()) {
    const auto cur = stack_b.back();
    stack_b.pop_back();
    if (!seen_b.insert(cur).second) continue;
    if (ancestors_a.count(cur) != 0U) return cur;
    const auto info = repo.read_commit(cur);
    for (const auto& p : info.parents) stack_b.push_back(p);
  }
  return std::nullopt;
}

} // namespace

void Repository::merge_branch(const std::string& giver_branch) const {
  if (!is_initialized()) throw std::runtime_error("not a gitfly repo");

  const std::string cur_ref = head_current_branch(root_);
  if (cur_ref.empty()) throw std::runtime_error("merges unsupported in detached HEAD state");

  const auto cur_tip = read_ref(root_, cur_ref);
  if (!cur_tip || cur_tip->size() != consts::kOidHexLen) {
    throw std::runtime_error("current branch has no commits");
  }

  const std::string giver_ref = heads_ref(giver_branch);
  const auto giver_tip = read_ref(root_, giver_ref);
  if (!giver_tip || giver_tip->size() != consts::kOidHexLen) {
    throw std::runtime_error("unknown branch: " + giver_branch);
  }
  if (*giver_tip == *cur_tip) throw std::runtime_error("cannot merge a branch with itself");

  if (is_commit_ancestor(*giver_tip, *cur_tip)) {
    // already up to date
    return;
  }
  if (is_commit_ancestor(*cur_tip, *giver_tip)) {
    // fast-forward
    const auto info = read_commit(*giver_tip);
    const auto tgt  = worktree::tree_to_map(*this, info.tree_hex);
    worktree::apply_snapshot(*this, tgt);
    worktree::write_index_snapshot(*this, tgt);
    update_ref(root_, cur_ref, *giver_tip);
    return;
  }

  // Record MERGE_HEAD
  {
    const auto p = git_dir() / consts::kMergeHead;
    const std::string s = *giver_tip + "\n";
    gfs::write_file_atomic(
        p, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(s.data()),
                                         s.size()));
  }

  const auto base = lca_commit(*this, *giver_tip, *cur_tip);
  if (!base) throw std::runtime_error("no common ancestor between branches");

  const auto cur_info  = read_commit(*cur_tip);
  const auto giv_info  = read_commit(*giver_tip);
  const auto base_info = read_commit(*base);

  auto ours   = worktree::tree_to_map(*this, cur_info.tree_hex);
  auto theirs = worktree::tree_to_map(*this, giv_info.tree_hex);
  auto baseM  = worktree::tree_to_map(*this, base_info.tree_hex);

  std::set<std::string> all;
  for (const auto& [p, _] : ours)   all.insert(p);
  for (const auto& [p, _] : theirs) all.insert(p);
  for (const auto& [p, _] : baseM)  all.insert(p);

  std::vector<std::string> conflicts;
  auto result = ours; // start from ours

  const auto get = [](const auto& m, const std::string& k) -> std::string {
    const auto it = m.find(k);
    return (it == m.end()) ? std::string{} : it->second;
  };

  for (const auto& path : all) {
    const std::string ob = get(baseM,  path);
    const std::string oo = get(ours,   path);
    const std::string ot = get(theirs, path);

    if (oo == ot) {
      continue; // identical changes or both unchanged
    }
    if (oo == ob && ot != ob) {
      // take theirs
      if (ot.empty()) {
        result.erase(path);
        stdfs::remove(root_ / path);
      } else {
        const auto bytes = read_blob(ot);
        gfs::write_file_atomic(root_ / path, bytes);
        result[path] = ot;
      }
      continue;
    }
    if (ot == ob && oo != ob) {
      continue; // keep ours
    }

    // conflict
    conflicts.push_back(path);

    std::string ours_txt;
    std::string theirs_txt;
    if (!oo.empty()) {
      const auto b = read_blob(oo);
      ours_txt.assign(reinterpret_cast<const char*>(b.data()), b.size());
    }
    if (!ot.empty()) {
      const auto b = read_blob(ot);
      theirs_txt.assign(reinterpret_cast<const char*>(b.data()), b.size());
    }

    // Produce hunk-level conflict markers by trimming common prefix/suffix
    // between ours and theirs. This avoids whole-file duplication when only
    // a small region differs.
    auto split_lines = [](const std::string& text) {
      std::vector<std::string> out;
      std::string cur;
      for (char c : text) {
        if (c == '\n') { out.push_back(std::move(cur)); cur.clear(); }
        else if (c != '\r') { cur.push_back(c); }
      }
      if (!cur.empty()) out.push_back(std::move(cur));
      return out;
    };

    const auto a = split_lines(ours_txt);
    const auto b = split_lines(theirs_txt);
    std::size_t pa = 0; // common prefix length
    while (pa < a.size() && pa < b.size() && a[pa] == b[pa]) ++pa;
    std::size_t sa = 0; // common suffix length
    while (sa < a.size() - pa && sa < b.size() - pa && a[a.size() - 1 - sa] == b[b.size() - 1 - sa]) ++sa;

    std::string merged;
    // Emit common prefix
    for (std::size_t i = 0; i < pa; ++i) {
      merged += a[i];
      merged.push_back('\n');
    }
    // Conflict region (middle only)
    merged += "<<<<<<< HEAD\n";
    for (std::size_t i = pa; i < a.size() - sa; ++i) {
      merged += a[i];
      merged.push_back('\n');
    }
    merged += "=======\n";
    for (std::size_t i = pa; i < b.size() - sa; ++i) {
      merged += b[i];
      merged.push_back('\n');
    }
    merged += ">>>>>>> ";
    merged += giver_branch;
    merged.push_back('\n');
    // Emit common suffix
    for (std::size_t i = sa; i > 0; --i) {
      merged += a[a.size() - i];
      merged.push_back('\n');
    }

    gfs::write_file_atomic(
        root_ / path,
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(merged.data()),
                                      merged.size()));
  }

  // Update index (exclude conflicting paths so user can resolve & re-add)
  if (!conflicts.empty()) {
    auto filtered = result;
    for (const auto& p : conflicts) filtered.erase(p);
    worktree::write_index_snapshot(*this, filtered);
  } else {
    worktree::write_index_snapshot(*this, result);
  }

  if (!conflicts.empty()) {
    std::string msg = "merge conflicts in: ";
    for (std::size_t i = 0; i < conflicts.size(); ++i) {
      if (i != 0U) { msg += ", "; }
      msg += conflicts[i];
    }
    throw std::runtime_error(msg); // leave MERGE_HEAD; user must resolve
  }

  // No conflicts → create merge commit with two parents, clear MERGE_HEAD
  (void)commit_index_with_parents("Merge branch '" + giver_branch + "'\n", {*giver_tip});
  std::error_code ec;
  stdfs::remove(git_dir() / consts::kMergeHead, ec);
}

} // namespace gitfly
