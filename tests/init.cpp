#include "gitfly/config.hpp"
#include "gitfly/repo.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

namespace fs = std::filesystem;

static std::string slurp(const fs::path &p) {
  std::ifstream ifs(p, std::ios::binary);
  return std::string{std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()};
}

int main() {
  // Make a unique temp repo root
  const auto base = fs::temp_directory_path();
  const std::string suffix = std::to_string(std::random_device{}());
  const fs::path repo_root = base / ("gitfly_init_test_" + suffix);

  try {
    fs::create_directories(repo_root);

    gitfly::Repository repo{repo_root};
    if (repo.is_initialized()) {
      std::cerr << "repo unexpectedly initialized before init()\n";
      return 1;
    }

    const gitfly::Identity id{.name = "Test User", .email = "test@example.com"};
    repo.init(id);

    // Check directory layout
    const fs::path gitdir = repo_root / ".gitfly";
    const fs::path head = gitdir / "HEAD";
    const fs::path config = gitdir / "config";
    const fs::path objects = gitdir / "objects";
    const fs::path refs = gitdir / "refs";
    const fs::path heads = refs / "heads";
    const fs::path tags = refs / "tags";

    if (!fs::exists(gitdir) || !fs::is_directory(gitdir)) {
      std::cerr << ".gitfly missing\n";
      return 1;
    }
    if (!fs::exists(objects) || !fs::is_directory(objects)) {
      std::cerr << "objects/ missing\n";
      return 1;
    }
    if (!fs::exists(refs) || !fs::is_directory(refs)) {
      std::cerr << "refs/ missing\n";
      return 1;
    }
    if (!fs::exists(heads) || !fs::is_directory(heads)) {
      std::cerr << "refs/heads/ missing\n";
      return 1;
    }
    if (!fs::exists(tags) || !fs::is_directory(tags)) {
      std::cerr << "refs/tags/ missing\n";
      return 1;
    }

    // Check HEAD contents
    if (!fs::exists(head)) {
      std::cerr << "HEAD missing\n";
      return 1;
    }
    const std::string head_txt = slurp(head);
    if (head_txt != "ref: refs/heads/master\n") {
      std::cerr << "HEAD content mismatch: [" << head_txt << "]\n";
      return 1;
    }

    // Check config contents + loader
    if (!fs::exists(config)) {
      std::cerr << "config missing\n";
      return 1;
    }
    const auto loaded = gitfly::load_identity(repo_root);
    if (loaded.name != id.name || loaded.email != id.email) {
      std::cerr << "config load mismatch: got {" << loaded.name << "," << loaded.email << "}\n";
      return 1;
    }

    // Calling init again should throw
    bool threw = false;
    try {
      repo.init(id);
    } catch (...) {
      threw = true;
    }
    if (!threw) {
      std::cerr << "init did not throw on already-initialized repo\n";
      return 1;
    }

    // Success
    std::cout << "init test OK: " << repo_root << "\n";
  } catch (const std::exception &e) {
    std::cerr << "exception: " << e.what() << "\n";
    fs::remove_all(repo_root);
    return 1;
  }

  // Clean up
  std::error_code ec;
  fs::remove_all(repo_root, ec);
  return 0;
}
