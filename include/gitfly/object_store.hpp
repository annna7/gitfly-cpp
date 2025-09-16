#pragma once
#include "gitfly/hash.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace gitfly {

struct Object {
  std::string type;                  // "blob" | "tree" | "commit" | etc.
  std::vector<std::uint8_t> data;    // payload bytes (no header)
};

class ObjectStore {
public:
  explicit ObjectStore(std::filesystem::path gitdir)
    : gitdir_(std::move(gitdir)) {}

  // Read and decompress object identified by 40-hex; returns type and payload.
  Object read(std::string_view hex_oid) const;

  // Write object with given type/payload. Returns 40-hex id.
  std::string write(std::string_view type, std::span<const std::uint8_t> payload) const;

  // Get filesystem path for a binary oid.
  std::filesystem::path path_for_oid(const oid& object_id) const;

private:
  std::filesystem::path gitdir_;
};

} // namespace gitfly
