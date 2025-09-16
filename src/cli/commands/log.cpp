#include "gitfly/refs.hpp"
#include "gitfly/repo.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int cmd_log(int /*argc*/, char ** /*argv*/) {
  gitfly::Repository repo{std::filesystem::current_path()};
  if (!repo.is_initialized()) {
    std::cerr << "log: not a gitfly repo (run `gitfly init`)\n";
    return 1;
  }
  try {
    auto head_txt = gitfly::read_HEAD(repo.root());
    if (!head_txt) {
      std::cerr << "log: no HEAD\n";
      return 1;
    }
    std::string commit_hex;
    if (head_txt->rfind("ref:", 0) == 0) {
      std::string rn = head_txt->substr(gitfly::consts::kRefPrefix.size());
      while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r'))
        rn.pop_back();
      auto tip = gitfly::read_ref(repo.root(), rn);
      if (!tip || tip->size() != gitfly::consts::kOidHexLen) {
        std::cerr << "log: branch has no commits\n";
        return 1;
      }
      commit_hex = *tip;
    } else {
      commit_hex = *head_txt;
      while (!commit_hex.empty() && (commit_hex.back() == '\n' || commit_hex.back() == '\r'))
        commit_hex.pop_back();
    }

    // Walk parents
    while (!commit_hex.empty()) {
      auto info = repo.read_commit(commit_hex);
      std::string parent = info.parents.empty() ? std::string() : info.parents.front();
      std::string author = info.author;
      std::string subject;
      {
        auto nl = info.message.find('\n');
        subject = (nl == std::string::npos ? info.message : info.message.substr(0, nl));
      }
      std::cout << "commit " << commit_hex << "\n";
      if (!author.empty())
        std::cout << "Author: " << author << "\n";
      if (!subject.empty())
        std::cout << "    " << subject << "\n";
      std::cout << "\n";
      if (parent.size() == gitfly::consts::kOidHexLen)
        commit_hex = parent;
      else
        break;
    }
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "log: " << e.what() << "\n";
    return 1;
  }
}
