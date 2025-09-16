#include "cli/registry.hpp"

#include <iostream>
#include <map>

namespace gitfly::cli {

struct entry {
  command_fn fn;
  std::string help;
};
static std::map<std::string, entry> &table() {
  static std::map<std::string, entry> t;
  return t;
}

void register_command(const std::string &name, command_fn fn, const std::string &help) {
  table()[name] = entry{.fn = fn, .help = help};
}

command_fn find_command(const std::string &name) {
  const auto it = table().find(name);
  return it == table().end() ? nullptr : it->second.fn;
}

void print_usage() {
  std::cerr << "usage: gitfly <command> [args]\n\n";
  std::cerr << "commands:\n";
  for (auto &[name, e] : table()) {
    std::cerr << "  " << name << "  " << e.help << "\n";
  }
}

} // namespace gitfly::cli
