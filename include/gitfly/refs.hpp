#pragma once
#include <filesystem>
#include <optional>
#include <string>

namespace gitfly {

// "refs/heads/<branch>"
std::string heads_ref(std::string_view branch);

// Read HEAD file as raw string (e.g., "ref: refs/heads/master\n" or a 40-hex id).
// Returns std::nullopt if HEAD does not exist yet.
std::optional<std::string> read_HEAD(const std::filesystem::path& repo_root);

// Write symbolic HEAD: "ref: <refname>\n"
void set_HEAD_symbolic(const std::filesystem::path& repo_root, const std::string& refname);

// Read a ref file (e.g., "refs/heads/master") -> 40-hex OID (without trailing newline).
std::optional<std::string> read_ref(const std::filesystem::path& repo_root, const std::string& refname);

// Overwrite/create a ref with the given 40-hex OID (adds trailing newline on disk).
void update_ref(const std::filesystem::path& repo_root, const std::string& refname, const std::string& hex_oid);

void set_HEAD_detached(const std::filesystem::path& repo_root, std::string_view hex_oid);

} // namespace gitfly
