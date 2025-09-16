#include "gitfly/hash.hpp"
#include "gitfly/consts.hpp"

#include <cstdint>
#include <openssl/evp.h> // EVP_* digest API
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

namespace gitfly {

oid sha1(std::span<const std::uint8_t> data) {
  oid out{}; // 20 bytes

  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!ctx) {
    throw std::runtime_error("EVP_MD_CTX_new failed");
  }
 
  if (EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr) != 1) {
    EVP_MD_CTX_free(ctx);
    throw std::runtime_error("EVP_DigestInit_ex(EVP_sha1) failed");
  }
  if (!data.empty() && EVP_DigestUpdate(ctx, data.data(), data.size()) != 1) {
    EVP_MD_CTX_free(ctx);
    throw std::runtime_error("EVP_DigestUpdate failed");
  }

  unsigned int len = 0;
  if (EVP_DigestFinal_ex(ctx, out.data(), &len) != 1) {
    EVP_MD_CTX_free(ctx);
    throw std::runtime_error("EVP_DigestFinal_ex failed");
  }
  EVP_MD_CTX_free(ctx);

  if (len != out.size()) {
    throw std::runtime_error("SHA-1 produced unexpected length");
  }
  return out;
}

std::string to_hex(const oid &id) {
  static constexpr std::array<char, 16> kHex = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  std::string s;
  s.resize(gitfly::consts::kOidHexLen);
  for (int i = 0; i < gitfly::consts::kOidRawLen; ++i) {
    unsigned b = id[i];
    s[(2 * i) + 0] = kHex[(b >> 4) & 0xF];
    s[(2 * i) + 1] = kHex[b & 0xF];
  }
  return s; 
} 

bool from_hex(std::string_view hex, oid &out) {
  if (hex.size() != gitfly::consts::kOidHexLen) {
    return false;
  }
  auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
      return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
      return 10 + (c - 'A');
    }
    return -1;
  };
  for (int i = 0; i < 20; ++i) {
    int hi = nibble(hex[2 * i]);
    int lo = nibble(hex[(2 * i) + 1]);
    if (hi < 0 || lo < 0) {
      return false;
    }
    out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
  }
  return true;
}

} // namespace gitfly
