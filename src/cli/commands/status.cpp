#include "gitfly/status.hpp"

#include "gitfly/refs.hpp"
#include "gitfly/repo.hpp"

#include <filesystem>
#include <iostream>

using gitfly::ChangeKind;

int cmd_status(int /*argc*/, char ** /*argv*/) {
  const gitfly::Repository repo{std::filesystem::current_path()};
  if (!repo.is_initialized()) {
    std::cerr << "status: not a gitfly repo (run `gitfly init`)\n";
    return 1;
  }

  // Print current branch or detached state
  auto head_txt = gitfly::read_HEAD(repo.root());
  if (head_txt && head_txt->rfind("ref:", 0) == 0) {
    std::string rn = head_txt->substr(gitfly::consts::kRefPrefix.size());
    while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r'))
      rn.pop_back();
    const std::string name =
        rn.rfind("refs/heads/", 0) == 0 ? rn.substr(std::string("refs/heads/").size()) : rn;
    std::cout << "On branch " << name << "\n\n";
  } else if (head_txt) {
    std::string h = *head_txt;
    while (!h.empty() && (h.back() == '\n' || h.back() == '\r'))
      h.pop_back();
    std::cout << "HEAD detached at " << h.substr(0, 7) << "\n\n";
  }

  auto st = gitfly::compute_status(repo);

  auto print_changes = [](const char *header, const std::vector<gitfly::Change> &xs) {
    std::cout << header << "\n";
    for (const auto &[kind, path] : xs) {
      const char code = (kind == ChangeKind::Added      ? 'A'
                         : kind == ChangeKind::Modified ? 'M'
                                                        : 'D');
      std::cout << "  " << code << "  " << path << "\n";
    }
    if (xs.empty())
      std::cout << "  (none)\n";
    std::cout << "\n";
  };

  print_changes("Changes to be committed:", st.staged);
  print_changes("Changes not staged for commit:", st.unstaged);

  std::cout << "Untracked files:\n";
  if (st.untracked.empty())
    std::cout << "  (none)\n";
  else
    for (auto &p : st.untracked)
      std::cout << "  " << p << "\n";
  std::cout << "\n";
  return 0;
}
