#include "gitfly/remote.hpp"
#include "gitfly/tcp_remote.hpp"
#include "gitfly/consts.hpp"

#include <filesystem>
#include <iostream>
#include <string>

int cmd_clone(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "usage: gitfly clone <src> <dest>\n";
    return 2;
  }
  try {
    if (const std::string src = argv[1]; src.rfind("tcp://", 0) == 0) {
      // parse tcp://host:port
      auto rest = src.substr(6);
      const auto colon = rest.find(':');
      const std::string host = rest.substr(0, colon);
      const int port = colon == std::string::npos ? gitfly::consts::portNumber : std::stoi(rest.substr(colon + 1));
      gitfly::tcpremote::clone_repo(host, port, argv[2]);
    } else {
      gitfly::remote::clone_repo(argv[1], argv[2]);
    }
    std::cout << "Cloned into '" << argv[2] << "'\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "clone: " << e.what() << "\n";
    return 1;
  }
}
