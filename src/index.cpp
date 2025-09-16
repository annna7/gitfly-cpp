#include "gitfly/index.hpp"

#include "gitfly/fs.hpp"
#include "gitfly/hash.hpp"
#include "gitfly/repo.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace gitfly {

Index::Index(std::filesystem::path repo_root) : repo_root_(std::move(repo_root)) {}

std::filesystem::path Index::index_path() const { return repo_root_ / ".gitfly" / "index"; }

static std::string trim(std::string s) {
  auto isspace2 = [](unsigned char c) { return std::isspace(c) != 0; };
  while (!s.empty() && isspace2(s.back()))
    s.pop_back();
  std::size_t i = 0;
  while (i < s.size() && isspace2(s[i]))
    ++i;
  return s.substr(i);
}

void Index::load() {
  entries_.clear();
  const auto p = index_path();
  if (!fs::exists(p))
    return;

  std::ifstream ifs(p);
  if (!ifs)
    throw std::runtime_error("open index for read failed");

  std::string line;
  while (std::getline(ifs, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#')
      continue;

    // format: "<octal> <hex> <path>"
    std::istringstream is(line);
    std::string mode_str, hex, path;
    if (!(is >> mode_str >> hex))
      continue;             // skip malformed
    std::getline(is, path); // rest of line
    path = trim(path);

    // parse octal
    std::uint32_t mode = 0;
    for (char c : mode_str) {
      if (c < '0' || c > '7') {
        mode = 0;
        break;
      }
      mode = (mode << 3) + (c - '0');
    }
    if (mode == 0 || path.empty() || hex.size() != gitfly::consts::kOidHexLen)
      continue;

    IndexEntry e{};
    e.mode = mode;
    if (!from_hex(hex, e.oid))
      continue;
    e.path = std::move(path);
    entries_.push_back(std::move(e));
  }

  // Keep file order stable: sort by path
  std::ranges::sort(entries_, [](auto &a, auto &b) { return a.path < b.path; });
}

void Index::save() const {
  // Write to a temp then atomically replace
  std::ostringstream os;
  for (const auto &e : entries_) {
    // write octal mode
    char mode_buf[16];
    std::snprintf(mode_buf, sizeof(mode_buf), "%o", e.mode);
    os << mode_buf << ' ' << to_hex(e.oid) << ' ' << e.path << '\n';
  }
  const auto s = os.str();
  fs::write_file_atomic(
      index_path(),
      std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(s.data()), s.size()));
}

void Index::add_path(const std::filesystem::path &wd, std::string_view relpath,
                     const Repository &repo, std::uint32_t mode) {
  // Read file
  auto bytes = fs::read_file(wd / std::filesystem::path(relpath));

  // Write blob -> hex oid
  const auto hex_oid = repo.write_blob(bytes);

  // Parse hex into Oid (binary)
  oid bin{};
  if (!from_hex(hex_oid, bin)) {
    throw std::runtime_error("write_blob produced bad hex oid");
  }

  // Replace or insert
  std::string path(relpath);
  auto it = std::ranges::find_if(entries_, [&](const IndexEntry &e) { return e.path == path; });
  if (it != entries_.end()) {
    it->mode = mode;
    it->oid = bin;
  } else {
    entries_.push_back(IndexEntry{.mode = mode, .oid = bin, .path = std::move(path)});
  }

  // Keep sorted by path for determinism
  std::ranges::sort(entries_, [](auto &a, auto &b) { return a.path < b.path; });
}

void Index::remove_path(std::string_view relpath) {
  const std::string key(relpath);
  std::erase_if(entries_, [&](const IndexEntry &e) { return e.path == key; });
}

std::map<std::string, std::string> Index::as_path_oid_map() const {
  std::map<std::string, std::string> m;
  for (const auto &e : entries_) {
    m[e.path] = to_hex(e.oid);
  }
  return m;
}

} // namespace gitfly
