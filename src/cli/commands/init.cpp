#include "gitfly/config.hpp"
#include "gitfly/repo.hpp"

#include <filesystem>
#include <iostream>

int cmd_init(int /*argc*/, char ** /*argv*/) {
  try {
    const std::filesystem::path root = std::filesystem::current_path();
    const gitfly::Repository repo{root};
    // pick any default identity (you can wire a config command later)
    repo.init(gitfly::Identity{.name = "Your Name", .email = "you@example.com"});
    std::cout << "Initialized empty gitfly repository in " << (root / ".gitfly") << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "init: " << e.what() << "\n";
    return 1;
  }
}
