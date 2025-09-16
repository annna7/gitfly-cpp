#include "gitfly/config.hpp"

#include "gitfly/fs.hpp"

#include <sstream>
#include <string_view>

namespace {

std::string trim(std::string_view sv) {
  // left trim spaces/tabs
  while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
    sv.remove_prefix(1);
  // right trim spaces/tabs/CR
  while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r'))
    sv.remove_suffix(1);
  return std::string(sv);
}

} // namespace

namespace gitfly {

std::filesystem::path cfg_path(const std::filesystem::path &repo_root) {
  return repo_root / ".gitfly" / "config";
}

auto load_identity(const std::filesystem::path &repo_root) -> Identity {
  Identity out{};
  const auto path = cfg_path(repo_root);
  if (!fs::exists(path))
    return out;

  const auto bytes = fs::read_file(path);
  const std::string text(bytes.begin(), bytes.end());
  std::istringstream iss(text);

  constexpr std::string_view k_author = "author:";
  constexpr std::string_view k_email = "email:";

  std::string line;
  while (std::getline(iss, line)) {
    std::string_view sv{line};
    if (sv.empty() || sv[0] == '#')
      continue; // allow comments
    if (sv.rfind(k_author, 0) == 0) {
      out.name = trim(sv.substr(k_author.size()));
    } else if (sv.rfind(k_email, 0) == 0) {
      out.email = trim(sv.substr(k_email.size()));
    }
  }
  return out;
}

void save_identity(const std::filesystem::path &repo_root, const Identity &id) {
  std::ostringstream os;
  os << "author: " << id.name << '\n' << "email: " << id.email << '\n';

  const auto path = cfg_path(repo_root);
  const std::string s = os.str();
  const auto *data = reinterpret_cast<const std::uint8_t *>(s.data());
  fs::write_file_atomic(path, std::span(data, s.size()));
}

} // namespace gitfly
