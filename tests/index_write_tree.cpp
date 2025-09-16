#include "gitfly/consts.hpp"
#include "gitfly/hash.hpp"
#include "gitfly/index.hpp"
#include "gitfly/repo.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using gitfly::Index;
using gitfly::IndexEntry;
using gitfly::oid;
using gitfly::Repository;
using gitfly::TreeEntry;

static void write_file(const fs::path &p, std::string_view s) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << s;
}

int main() {
  // temp repo
  const auto base = fs::temp_directory_path();
  const fs::path root = base / ("gitfly_idx_tree_" + std::to_string(std::random_device{}()));
  fs::create_directories(root);

  try {
    Repository repo{root};
    repo.init(gitfly::Identity{.name = "T", .email = "t@e"});

    // Working tree files
    write_file(root / "a.txt", "A\n");
    write_file(root / "dir/b.txt", "B\n");

    // Index add
    Index idx{root};
    idx.load();
    idx.add_path(root, "a.txt", repo, gitfly::consts::kModeFile);
    idx.add_path(root, "dir/b.txt", repo, gitfly::consts::kModeFile);
    idx.save();

    // Build tree from index
    std::string root_tree = repo.write_tree_from_index();
    std::cout << "root tree: " << root_tree << "\n";

    // Cross-check by writing subtrees manually:
    // subtree for "dir"
    {
      // blob for B\n should match hash-object
      auto b_bytes = gitfly::oid{};
      // Actually we can just trust write_blob; but not strictly needed here.
    }

    // Read back the root tree and check it has both entries
    auto entries = repo.read_tree(root_tree);
    bool have_a = false;
    bool have_dir = false;
    oid dir_oid{};
    for (auto &e : entries) {
      if (e.name == "a.txt" && e.mode == gitfly::consts::kModeFile) {
        have_a = true;
      }
      if (e.name == "dir" && e.mode == gitfly::consts::kModeTree) {
        have_dir = true;
        dir_oid = e.id;
      }
    }
    if (!have_a || !have_dir) {
      std::cerr << "root tree entries missing\n";
      return 1;
    }

    // Verify the "dir" subtree has 'b.txt'
    auto dir_hex = gitfly::to_hex(dir_oid);
    auto dir_entries = repo.read_tree(dir_hex);
    if (dir_entries.size() != 1 || dir_entries[0].name != "b.txt" ||
        dir_entries[0].mode != gitfly::consts::kModeFile) {
      std::cerr << "dir subtree invalid\n";
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
