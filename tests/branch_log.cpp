#include "gitfly/fs.hpp"
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

static std::string parent_of(const gitfly::Repository &repo, const std::string &commit_hex) {
  using gitfly::from_hex;
  using gitfly::oid;
  using gitfly::to_hex;
  oid cid{};
  if (!from_hex(commit_hex, cid))
    return {};
  auto raw = gitfly::fs::z_decompress(gitfly::fs::read_file(repo.object_path_from_oid(cid)));
  const auto it_space = std::ranges::find(raw, static_cast<std::uint8_t>(' '));
  const auto it_nul = std::find(it_space + 1, raw.end(), static_cast<std::uint8_t>('\0'));
  if (it_space == raw.end() || it_nul == raw.end())
    return {};
  const std::string text(raw.begin() + (it_nul - raw.begin()) + 1, raw.end());
  // scan lines until blank
  std::string::size_type pos = 0;
  while (true) {
    auto nl2 = text.find('\n', pos);
    std::string line = (nl2 == std::string::npos ? text.substr(pos) : text.substr(pos, nl2 - pos));
    if (line.empty())
      break;
    if (line.rfind("parent ", 0) == 0)
      return line.substr(7);
    if (nl2 == std::string::npos)
      break;
    pos = nl2 + 1;
  }
  return {};
}

int main() {
  const fs::path root =
      fs::temp_directory_path() / ("gitfly_branch_log_" + std::to_string(std::random_device{}()));
  fs::create_directories(root);

  try {
    gitfly::Repository repo{root};
    repo.init(gitfly::Identity{"User", "u@example.com"});

    // two commits
    write_file(root / "a.txt", "A\n");
    gitfly::Index idx{root};
    idx.load();
    idx.add_path(root, "a.txt", repo, gitfly::consts::kModeFile);
    idx.save();
    std::string c1 = repo.commit_index("first\n");

    write_file(root / "b.txt", "B\n");
    idx.load();
    idx.add_path(root, "b.txt", repo, gitfly::consts::kModeFile);
    idx.save();
    std::string c2 = repo.commit_index("second\n");

    auto head_txt = gitfly::read_HEAD(root);
    if (!head_txt || head_txt->rfind("ref:", 0) != 0) {
      std::cerr << "HEAD not symbolic\n";
      return 1;
    }
    std::string rn = head_txt->substr(gitfly::consts::kRefPrefix.size());
    while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r'))
      rn.pop_back();
    if (auto tip = gitfly::read_ref(root, rn); !tip || *tip != c2) {
      std::cerr << "HEAD tip mismatch\n";
      return 1;
    }

    // branch creation at HEAD
    std::string new_ref = gitfly::heads_ref("feature");
    if (auto ex = gitfly::read_ref(root, new_ref); ex) {
      std::cerr << "unexpected existing feature branch\n";
      return 1;
    }
    gitfly::update_ref(root, new_ref, c2);
    if (auto got = gitfly::read_ref(root, new_ref); !got || *got != c2) {
      std::cerr << "branch ref not created properly\n";
      return 1;
    }

    // simple log traversal: c2 parent should be c1
    if (auto parent = parent_of(repo, c2); parent != c1) {
      std::cerr << "parent mismatch: got " << parent << "\n";
      return 1;
    }

    std::cout << "branch/log OK\n";
  } catch (const std::exception &e) {
    std::cerr << "exception: " << e.what() << "\n";
    fs::remove_all(root);
    return 1;
  }

  std::error_code ec;
  fs::remove_all(root, ec);
  return 0;
}
