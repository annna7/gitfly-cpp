#pragma once
#include <string>
#include <optional>
#include "cli/command.hpp"

namespace gitfly::cli {

void register_command(const std::string& name, command_fn fn, const std::string& help);
command_fn find_command(const std::string& name);
void print_usage();

// implemented in register_commands.cpp
void register_all_commands();

} // namespace gitfly::cli
