#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace gitfly {

// Raw 20-byte SHA-1 object id (binary, not hex)
using oid = std::array<std::uint8_t, 20>;

/**
 * Compute SHA-1 of arbitrary bytes.
 * NOTE: For Git object ids, you must hash the full
 *   "<type> <size>\\0" + data
 * buffer. Use object_header(...) to build the header.
 */
oid sha1(std::span<const std::uint8_t> data);

// Convenience overload for string-like input (no copy).
inline oid sha1(std::string_view s) {
  return sha1(
      std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t *>(s.data()), s.size()));
}

/** Convert binary oid to 40-char lowercase hex. */
std::string to_hex(const oid &id);

/**
 * Parse 40-char hex into binary oid.
 * Returns false if length/characters are invalid.
 */
bool from_hex(std::string_view hex, oid &out);

/**
 * Build the Git object header used for hashing:
 *   "<type> <size>\\0"
 * Example:
 *   auto hdr = object_header("blob", bytes.size());
 *   auto store = hdr + std::string_view((char*)bytes.data(), bytes.size());
 *   auto id = sha1(store);
 */
inline std::string object_header(std::string_view type, std::size_t size) {
  std::string s;
  s.reserve(type.size() + 1 + 20 + 1); // rough reserve
  s.append(type);
  s.push_back(' ');
  s.append(std::to_string(size));
  s.push_back('\0');
  return s;
}

} // namespace gitfly
