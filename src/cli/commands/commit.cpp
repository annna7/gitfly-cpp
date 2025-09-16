#include "gitfly/consts.hpp"
#include "gitfly/refs.hpp"
#include "gitfly/repo.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int cmd_commit(int argc, char **argv) {
  // very small parser: gitfly commit -m "msg"
  std::string message;
  for (int i = 1; i < argc; ++i) {
    if (std::string a = argv[i]; (a == "-m" || a == "--message") && i + 1 < argc) {
      message = argv[++i];
    }
  }
  if (message.empty()) {
    std::cerr << "usage: gitfly commit -m <message>\n";
    return 2;
  }

  gitfly::Repository repo{std::filesystem::current_path()};
  if (!repo.is_initialized()) {
    std::cerr << "commit: not a gitfly repo (run `gitfly init`)\n";
    return 1;
  }

  try {
    // If a merge is in progress, give a helpful hint.
    if (std::filesystem::exists(repo.git_dir() / gitfly::consts::kMergeHead)) {
      std::cout << "Finalizing merge...\n";
      // Print concise parents summary: ours (HEAD) + theirs (MERGE_HEAD)
      std::string ours;
      if (auto head_txt = gitfly::read_HEAD(repo.root());
          head_txt && head_txt->rfind("ref:", 0) == 0) {
        std::string rn = head_txt->substr(gitfly::consts::kRefPrefix.size());
        while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r'))
          rn.pop_back();
        if (auto tip = gitfly::read_ref(repo.root(), rn); tip && tip->size() == 40)
          ours = *tip;
      } else if (head_txt) {
        ours = *head_txt;
        while (!ours.empty() && (ours.back() == '\n' || ours.back() == '\r'))
          ours.pop_back();
      }
      std::string theirs;
      {
        std::ifstream ifs(repo.git_dir() / std::string(gitfly::consts::kMergeHead),
                          std::ios::binary);
        std::getline(ifs, theirs);
      }
      auto abbr = [](const std::string &h) { return h.size() >= 7 ? h.substr(0, 7) : h; };
      if (!ours.empty() && !theirs.empty())
        std::cout << "Merge parents: " << abbr(ours) << " + " << abbr(theirs) << "\n";
    }
    // append newline like Git usually stores
    std::string oid = repo.commit_index(message + "\n");
    std::cout << oid << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "commit: " << e.what() << "\n";
    return 1;
  }
}
