#pragma once
#include <filesystem>
#include <string>

namespace gitfly {

struct Identity {
  std::string name;
  std::string email;
};

// Read identity from .gitfly/config (empty fields if missing)
Identity load_identity(const std::filesystem::path& repo_root);

// Overwrite .gitfly/config with the given identity
void save_identity(const std::filesystem::path& repo_root, const Identity& id);

} // namespace gitfly
