#include "gitfly/refs.hpp"
#include "gitfly/remote.hpp"
#include "gitfly/repo.hpp"
#include "gitfly/tcp_remote.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int cmd_push(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: gitfly push <remote-path> [<branch>]\n";
    return 2;
  }
  std::string remote = argv[1];
  std::string branch;
  gitfly::Repository repo{std::filesystem::current_path()};
  if (argc >= 3)
    branch = argv[2];
  else {
    auto head_txt = gitfly::read_HEAD(repo.root());
    if (!head_txt || head_txt->rfind("ref:", 0) != 0) {
      std::cerr << "push: detached HEAD; specify branch\n";
      return 1;
    }
    std::string rn = head_txt->substr(gitfly::consts::kRefPrefix.size());
    while (!rn.empty() && (rn.back() == '\n' || rn.back() == '\r'))
      rn.pop_back();
    const std::string prefix = "refs/heads/";
    branch = rn.rfind(prefix, 0) == 0 ? rn.substr(prefix.size()) : rn;
  }
  try {
    if (remote.rfind("tcp://", 0) == 0) {
      auto rest = remote.substr(6);
      auto colon = rest.find(':');
      std::string host = rest.substr(0, colon);
      int port = colon == std::string::npos ? gitfly::consts::portNumber : std::stoi(rest.substr(colon + 1));
      gitfly::tcpremote::push_branch(host, port, repo.root().string(), branch);
    } else {
      gitfly::remote::push_branch(repo.root(), remote, branch);
    }
    std::cout << "Pushed to '" << branch << "' at " << remote << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "push: " << e.what() << "\n";
    return 1;
  }
}
