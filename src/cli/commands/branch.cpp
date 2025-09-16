#include "gitfly/refs.hpp"
#include "gitfly/repo.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int cmd_branch(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: gitfly branch <name>\n";
    return 2;
  }
  std::string name = argv[1];
  gitfly::Repository repo{std::filesystem::current_path()};
  if (!repo.is_initialized()) {
    std::cerr << "branch: not a gitfly repo (run `gitfly init`)\n";
    return 1;
  }

  try {
    auto head_txt = gitfly::read_HEAD(repo.root());
    if (!head_txt) {
      std::cerr << "branch: HEAD is missing\n";
      return 1;
    }

    std::string current_commit;
    if (head_txt->rfind("ref:", 0) == 0) {
      std::string rn = head_txt->substr(gitfly::consts::kRefPrefix.size());
      while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r'))
        rn.pop_back();
      auto tip = gitfly::read_ref(repo.root(), rn);
      if (!tip || tip->size() != gitfly::consts::kOidHexLen) {
        std::cerr << "branch: current branch has no commits\n";
        return 1;
      }
      current_commit = *tip;
    } else {
      current_commit = *head_txt;
      while (!current_commit.empty() &&
             (current_commit.back() == '\n' || current_commit.back() == '\r'))
        current_commit.pop_back();
      if (current_commit.size() != gitfly::consts::kOidHexLen) {
        std::cerr << "branch: HEAD not on a commit\n";
        return 1;
      }
    }

    std::string refname = gitfly::heads_ref(name);
    if (auto exists = gitfly::read_ref(repo.root(), refname); exists) {
      std::cerr << "branch: ref already exists: " << refname << "\n";
      return 1;
    }
    gitfly::update_ref(repo.root(), refname, current_commit);
    std::cout << "Branch '" << name << "' created at " << current_commit.substr(0, 7) << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "branch: " << e.what() << "\n";
    return 1;
  }
}
