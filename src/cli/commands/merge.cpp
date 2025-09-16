#include "gitfly/repo.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int cmd_merge(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: gitfly merge <branch>\n";
    return 2;
  }
  const std::string giver = argv[1];
  const gitfly::Repository repo{std::filesystem::current_path()};
  if (!repo.is_initialized()) {
    std::cerr << "merge: not a gitfly repo (run `gitfly init`)\n";
    return 1;
  }
  try {
    repo.merge_branch(giver);
    std::cout << "Merge completed (or already up to date).\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "merge: " << e.what() << "\n";
    return 1;
  }
}
