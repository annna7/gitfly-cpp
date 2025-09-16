#include "gitfly/consts.hpp"
#include "gitfly/remote.hpp"
#include "gitfly/tcp_remote.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int cmd_fetch(int argc, char **argv) {
  if (argc < 2) {
    std::cerr << "usage: gitfly fetch <remote> [<name>]\n";
    return 2;
  }
  std::string remote = argv[1];
  std::string name = (argc >= 3 ? argv[2] : std::string("origin"));
  std::filesystem::path local = std::filesystem::current_path();
  try {
    gitfly::remote::FetchResult res;
    if (remote.rfind("tcp://", 0) == 0) {
      auto rest = remote.substr(6); // strip "tcp://"
      auto colon = rest.find(':');
      std::string host = rest.substr(0, colon);
      int port = colon == std::string::npos ? gitfly::consts::portNumber : std::stoi(rest.substr(colon + 1));
      auto tres = gitfly::tcpremote::fetch_head(host, port, local.string(), name);
      res.branch = std::move(tres.branch);
      res.tip = std::move(tres.tip);
    } else {
      res = gitfly::remote::fetch_head(local, remote, name);
    }
    std::cout << "Fetched: " << (res.branch.empty() ? "(none)" : res.branch) << " "
              << (res.tip.empty() ? "(no tip)" : res.tip) << "\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "fetch: " << e.what() << "\n";
    return 1;
  }
}
 
