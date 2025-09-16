#include "gitfly/object_store.hpp"

#include "gitfly/consts.hpp"
#include "gitfly/fs.hpp"

#include <algorithm>
#include <stdexcept>

namespace gfs = gitfly::fs;

namespace gitfly {

std::filesystem::path ObjectStore::path_for_oid(const oid &object_id) const {
  const std::string hex = to_hex(object_id);
  const std::filesystem::path dir = gitdir_ / consts::kObjectsDir / hex.substr(0, 2);
  std::filesystem::path file = dir / hex.substr(2);
  return file;
}

Object ObjectStore::read(std::string_view hex_oid) const {
  oid oid{};
  if (!from_hex(hex_oid, oid)) {
    throw std::runtime_error("object_store: bad oid hex");
  }
  auto store = gfs::z_decompress(gfs::read_file(path_for_oid(oid)));

  auto it_space = std::ranges::find(store, static_cast<std::uint8_t>(' '));
  auto it_nul = std::find(it_space + 1, store.end(), static_cast<std::uint8_t>('\0'));
  if (it_space == store.end() || it_nul == store.end()) {
    throw std::runtime_error("object_store: invalid header");
  }
  std::string type(store.begin(), it_space);
  std::size_t payload_off = (it_nul - store.begin()) + 1;
  return Object{.type = std::move(type), .data = {store.begin() + payload_off, store.end()}};
}

std::string ObjectStore::write(std::string_view type, std::span<const std::uint8_t> payload) const {
  const std::string hdr = object_header(type, payload.size());
  std::vector<std::uint8_t> store;
  store.reserve(hdr.size() + payload.size());
  store.insert(store.end(), reinterpret_cast<const std::uint8_t *>(hdr.data()),
               reinterpret_cast<const std::uint8_t *>(hdr.data()) + hdr.size());
  store.insert(store.end(), payload.begin(), payload.end());

  oid store_id = sha1(store);
  auto path = path_for_oid(store_id);
  if (!std::filesystem::exists(path)) {
    auto compressed = gfs::z_compress(store);
    gfs::write_file_atomic(path, compressed);
  }
  return to_hex(store_id);
}

} // namespace gitfly
