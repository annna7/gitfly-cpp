#include "gitfly/repo.hpp"
#include "gitfly/hash.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using gitfly::Repository;
using gitfly::TreeEntry;
using gitfly::oid;

int main() {
  namespace fs = std::filesystem;

  // Use a temp-ish folder under your project dir
  fs::path repo_root = fs::current_path() / "sandbox_repo";
  std::cout << "Repo root: " << repo_root << "\n";

  Repository repo{repo_root};

  // ---- 1) Write a blob for "hello\n"
  const std::string content = "hello\n";
  const std::string blob_oid_hex = repo.write_blob(
      std::span<const std::uint8_t>(
          reinterpret_cast<const std::uint8_t*>(content.data()),
          content.size()));
  std::cout << "blob OID:   " << blob_oid_hex << "\n";

  // Read it back and show size
  auto back = repo.read_blob(blob_oid_hex);
  std::cout << "blob size:  " << back.size()
            << (std::string(back.begin(), back.end()) == content ? " (OK)" : " (DIFF!)")
            << "\n";

  // ---- 2) Write a tree with one file entry "hello.txt" -> blob
  oid blob_oid{};
  if (!gitfly::from_hex(blob_oid_hex, blob_oid)) {
    std::cerr << "from_hex failed\n"; return 1;
  }
  TreeEntry e{};
  e.mode = gitfly::consts::kModeFile;        // regular file (octal)
  e.name = "hello.txt";
  e.id   = blob_oid;

  std::string tree_oid_hex = repo.write_tree({e});
  std::cout << "tree OID:   " << tree_oid_hex << "\n";

  // Read the tree back and print entries
  auto entries = repo.read_tree(tree_oid_hex);
  for (const auto& t : entries) {
    std::cout << "  entry: mode=" << std::oct << t.mode << std::dec
              << " name=" << t.name
              << " oid=" << gitfly::to_hex(t.id) << "\n";
  }

  // ---- 3) Write a commit pointing to that tree (fixed author/committer lines)
  // Format must be EXACT: "Name <email> <epoch> <+/-HHMM>"
  std::string author    = "John Doe <john@example.com> 1714412345 +0300";
  std::string committer = "John Doe <john@example.com> 1714412345 +0300";
  std::string message   = "my first gitfly commit\n";

  std::string commit_oid_hex = repo.write_commit(
      tree_oid_hex, /*parents*/{},
      author, committer, message);

  std::cout << "commit OID: " << commit_oid_hex << "\n";

  std::cout << "\nOK. Objects were written to: " << (repo_root / ".gitfly" / "objects") << "\n";
  std::cout << "Now validate with Git (instructions printed in README or see chat).\n";
  return 0;
}
