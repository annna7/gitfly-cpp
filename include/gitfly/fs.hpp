#pragma once
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace gitfly::fs {

bool exists(const std::filesystem::path& p);
void ensure_parent_dir(const std::filesystem::path& p);

std::vector<std::uint8_t> read_file(const std::filesystem::path& p);
void write_file_atomic(const std::filesystem::path& p, std::span<const std::uint8_t> data);

std::vector<std::uint8_t> z_compress(std::span<const std::uint8_t> data);
std::vector<std::uint8_t> z_decompress(std::span<const std::uint8_t> data);

} // namespace gitfly::fs
