#include "cli/registry.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  gitfly::cli::register_all_commands(); // defined in register_commands.cpp

  if (argc < 2) {
    gitfly::cli::print_usage();
    return 2;
  }
  const std::string cmd = argv[1];

  const auto fn = gitfly::cli::find_command(cmd);
  if (!fn) {
    std::cerr << "unknown command: " << cmd << "\n";
    gitfly::cli::print_usage();
    return 2;
  }
  // Pass everything after the subcommand to the handler
  return fn(argc - 1, argv + 1);
}
