#include "gitfly/refs.hpp"

#include "gitfly/consts.hpp"
#include "gitfly/fs.hpp"

#include <cctype>
#include <string>
#include <string_view>

namespace gitfly {

static std::filesystem::path git_dir(const std::filesystem::path &root) {
  return root / gitfly::consts::kGitDir;
}

static std::filesystem::path head_file(const std::filesystem::path &root) {
  return git_dir(root) / gitfly::consts::kHeadFile;
}

static std::filesystem::path ref_path(const std::filesystem::path &root,
                                      const std::string &refname) {
  return git_dir(root) / refname;
}

std::string heads_ref(std::string_view branch) {
  return std::string("refs/heads/") + std::string(branch);
}

std::optional<std::string> read_HEAD(const std::filesystem::path &repo_root) {
  const auto head_file_ptr = head_file(repo_root);
  if (!fs::exists(head_file_ptr)) {
    return std::nullopt;
  }
  auto bytes = fs::read_file(head_file_ptr);
  std::string str(bytes.begin(), bytes.end());
  return str;
}

void set_HEAD_symbolic(const std::filesystem::path &repo_root, const std::string &refname) {
  const std::string ref_str = "ref: " + refname + "\n";
  const auto p = head_file(repo_root);
  fs::write_file_atomic(
      p, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(ref_str.data()),
                                       ref_str.size()));
}

std::optional<std::string> read_ref(const std::filesystem::path &repo_root,
                                    const std::string &refname) {
  const auto p = ref_path(repo_root, refname);
  if (!fs::exists(p)) {
    return std::nullopt;
  }
  auto bytes = fs::read_file(p);
  std::string s(bytes.begin(), bytes.end());
  // strip trailing whitespace/newlines
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
    s.pop_back();
  return s;
}

void update_ref(const std::filesystem::path &repo_root, const std::string &refname,
                const std::string &hex_oid) {
  const auto p = ref_path(repo_root, refname);
  std::string s = hex_oid + "\n";
  fs::write_file_atomic(
      p, std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(s.data()), s.size()));
}

void set_HEAD_detached(const std::filesystem::path &repo_root, std::string_view hex_oid) {
  std::string s = std::string(hex_oid) + "\n";
  fs::write_file_atomic(
      head_file(repo_root),
      std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(s.data()), s.size()));
}

} // namespace gitfly
