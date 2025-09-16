#include "gitfly/fs.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <zlib.h>

namespace gitfly::fs {

bool exists(const std::filesystem::path &p) {
  std::error_code ec;
  return std::filesystem::exists(p, ec);
}

void ensure_parent_dir(const std::filesystem::path &p) {
  std::error_code ec;
  std::filesystem::create_directories(p.parent_path(), ec);
  if (ec)
    throw std::runtime_error("mkdir -p failed: " + ec.message());
}

std::vector<std::uint8_t> read_file(const std::filesystem::path &p) {
  std::ifstream ifs(p, std::ios::binary);
  if (!ifs) {
    throw std::runtime_error("open for read failed: " + p.string());
  }
  ifs.seekg(0, std::ios::end);
  auto n = static_cast<std::size_t>(ifs.tellg());
  ifs.seekg(0);
  std::vector<std::uint8_t> buf(n);
  if (n)
    ifs.read(reinterpret_cast<char *>(buf.data()), static_cast<std::streamsize>(n));
  return buf;
}

void write_file_atomic(const std::filesystem::path &p, std::span<const std::uint8_t> data) {
  ensure_parent_dir(p);
  auto tmp = p;
  tmp += ".tmp";
  {
    std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      throw std::runtime_error("open temp for write failed: " + tmp.string());
    }
    if (!data.empty()) {
      ofs.write(reinterpret_cast<const char *>(data.data()),
                static_cast<std::streamsize>(data.size()));
    }
    ofs.flush();
    if (!ofs)
      throw std::runtime_error("flush temp failed: " + tmp.string());
  }
  std::error_code ec;
  std::filesystem::rename(tmp, p, ec);
  if (ec) {
    std::filesystem::remove(p, ec);
    std::filesystem::rename(tmp, p, ec);
    if (ec) {
      std::filesystem::remove(tmp);
      throw std::runtime_error("atomic replace failed: " + p.string() + ": " + ec.message());
    }
  }
}

std::vector<std::uint8_t> z_compress(std::span<const std::uint8_t> data) {
  uLongf bound = compressBound(static_cast<uLong>(data.size()));
  std::vector<std::uint8_t> out(bound);
  const int rc = compress2(out.data(), &bound, reinterpret_cast<const Bytef *>(data.data()),
                           static_cast<uLong>(data.size()), Z_BEST_SPEED);
  if (rc != Z_OK)
    throw std::runtime_error("zlib compress failed");
  out.resize(bound);
  return out;
}

std::vector<std::uint8_t> z_decompress(std::span<const std::uint8_t> data) {
  std::size_t cap = data.size() * 3;
  cap = std::max<size_t>(cap, 64);
  for (int i = 0; i < 6; ++i) {
    std::vector<std::uint8_t> out(cap);
    auto destLen = static_cast<uLongf>(out.size());
    const int rc = uncompress(out.data(), &destLen, reinterpret_cast<const Bytef *>(data.data()),
                              static_cast<uLong>(data.size()));
    if (rc == Z_OK) {
      out.resize(destLen);
      return out;
    }
    if (rc == Z_BUF_ERROR) {
      cap *= 2;
      continue;
    }
    throw std::runtime_error("zlib uncompress failed");
  }
  throw std::runtime_error("zlib uncompress overflow");
}

} // namespace gitfly::fs
