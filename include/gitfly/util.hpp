#pragma once
#include <string>
#include <string_view>
#include <span>

namespace gitfly {

// Validate 40-char lowercase/uppercase hex
auto looks_hex40(std::string_view str) -> bool;

// Compute the Git blob object id for raw bytes without writing to the object store.
// Hashes header "blob <size>\0" + data and returns 40-hex.
auto compute_blob_hex_oid(std::span<const std::uint8_t> bytes) -> std::string;

// String helpers
namespace strutil {
  // Strip trailing CR/LF characters in place
  void rstrip_newlines(std::string& str);
}

}
