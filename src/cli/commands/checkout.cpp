#include "gitfly/repo.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int cmd_checkout(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: gitfly checkout <branch | gitfly::consts::kOidHexLen-hex-commit>\n";
    return 2;
  }
  const gitfly::Repository repo{std::filesystem::current_path()};
  if (!repo.is_initialized()) {
    std::cerr << "checkout: not a gitfly repo (run `gitfly init`)\n";
    return 1;
  }
  try {
    repo.checkout(argv[1]);
    std::cout << "Switched to " << argv[1] << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "checkout: " << e.what() << "\n";
    return 1;
  }
}
