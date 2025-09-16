#include "gitfly/fs.hpp"
#include "gitfly/repo.hpp"
#include "gitfly/index.hpp"
#include "gitfly/refs.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>

namespace fs = std::filesystem;

static void write_file(const fs::path& p, std::string_view s) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << s;
}
 
int main() {
  const fs::path root = fs::temp_directory_path() / ("gitfly_merge_ff_" + std::to_string(std::random_device{}()));
  fs::create_directories(root);
  try {
    gitfly::Repository repo{root};
    repo.init(gitfly::Identity{"User", "u@example.com"});

    // Base commit on master
    write_file(root / "f.txt", "base\n");
    gitfly::Index idx{root}; idx.load(); idx.add_path(root, "f.txt", repo, gitfly::consts::kModeFile); idx.save();
    std::string c0 = repo.commit_index("c0\n");

    // Create feature branch at c0 and advance it by one commit
    gitfly::update_ref(root, gitfly::heads_ref("feature"), c0);
    repo.checkout("feature");
    write_file(root / "f.txt", "feature\n");
    idx.load(); idx.add_path(root, "f.txt", repo, gitfly::consts::kModeFile); idx.save();
    std::string cf = repo.commit_index("cf\n");

    // Return to master (at c0) and merge feature fast-forward
    repo.checkout("master");
    repo.merge_branch("feature");

    // Master should now point at cf; working tree should have feature content
    auto tip = gitfly::read_ref(root, gitfly::heads_ref("master"));
    if (!tip || *tip != cf) { std::cerr << "fast-forward: master tip mismatch\n"; return 1; }
    auto bytes = gitfly::fs::read_file(root / "f.txt");
    std::string content(bytes.begin(), bytes.end());
    if (content.find("feature") == std::string::npos) { std::cerr << "fast-forward: content mismatch\n"; return 1; }

    std::cout << "merge fast-forward OK\n";
  } catch (const std::exception& e) {
    std::cerr << "exception: " << e.what() << "\n";
    fs::remove_all(root);
    return 1; 
  }
  std::error_code ec; fs::remove_all(root, ec);
  return 0;
}
