// Utility helpers for hex and object-id computations
#include "gitfly/util.hpp"

#include "gitfly/consts.hpp"
#include "gitfly/hash.hpp"

#include <cctype>
#include <vector>

namespace gitfly {

bool looks_hex40(std::string_view str) {
  if (str.size() != consts::kOidHexLen) {
    return false;
  }
  return std::ranges::all_of(str,
                             [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); });
}

std::string compute_blob_hex_oid(std::span<const std::uint8_t> bytes) {
  const std::string hdr = object_header("blob", bytes.size());
  std::vector<std::uint8_t> store;
  store.reserve(hdr.size() + bytes.size());
  store.insert(store.end(), reinterpret_cast<const std::uint8_t *>(hdr.data()),
               reinterpret_cast<const std::uint8_t *>(hdr.data()) + hdr.size());
  store.insert(store.end(), bytes.begin(), bytes.end());
  const oid id = sha1(store);
  return to_hex(id);
}

namespace strutil {

void rstrip_newlines(std::string &s) {
  while (!s.empty()) {
    char c = s.back();
    if (c == '\n' || c == '\r') {
      s.pop_back();
    } else {
      break;
    }
  }
}

} // namespace strutil

} // namespace gitfly
