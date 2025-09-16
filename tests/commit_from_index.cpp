#include "gitfly/hash.hpp"
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

int main() {
  const fs::path root =
      fs::temp_directory_path() / ("gitfly_cmt_" + std::to_string(std::random_device{}()));
  fs::create_directories(root);

  try {
    gitfly::Repository repo{root};
    repo.init(gitfly::Identity{.name = "User", .email = "u@example.com"});

    write_file(root / "a.txt", "hello\n");
    gitfly::Index idx{root};
    idx.load();
    idx.add_path(root, "a.txt", repo, gitfly::consts::kModeFile);
    idx.save();

    const std::string c1 = repo.commit_index("first\n");

    // The current branch should point at c1
    auto head_txt = gitfly::read_HEAD(root);
    if (!head_txt || head_txt->rfind("ref:", 0) != 0) {
      std::cerr << "HEAD not symbolic\n";
      return 1;
    }
    std::string rn = head_txt->substr(gitfly::consts::kRefPrefix.size());
    while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r'))
      rn.pop_back();
    auto ref = gitfly::read_ref(root, rn);
    if (!ref || *ref != c1) {
      std::cerr << "ref not updated\n";
      return 1;
    }

    // Second commit has parent=c1
    write_file(root / "b.txt", "B\n");
    idx.load();
    idx.add_path(root, "b.txt", repo, gitfly::consts::kModeFile);
    idx.save();
    const std::string c2 = repo.commit_index("second\n");

    auto ref2 = gitfly::read_ref(root, rn);
    if (!ref2 || *ref2 != c2) {
      std::cerr << "ref not updated to c2\n";
      return 1;
    }

    std::cout << "OK\n";
  } catch (const std::exception &e) {
    std::cerr << "exception: " << e.what() << "\n";
    fs::remove_all(root);
    return 1;
  }

  std::error_code ec;
  fs::remove_all(root, ec);
  return 0;
}
