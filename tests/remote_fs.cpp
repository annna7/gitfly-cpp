#include "gitfly/remote.hpp"
#include "gitfly/repo.hpp"
#include "gitfly/index.hpp"
#include "gitfly/refs.hpp"
#include "gitfly/fs.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

namespace fs = std::filesystem;

static void write_file(const fs::path& p, std::string_view s) {
  fs::create_directories(p.parent_path());
  std::ofstream(p, std::ios::binary) << s;
}

int main() {
  const fs::path remote = fs::temp_directory_path() / ("gitfly_remote_" + std::to_string(std::random_device{}()));
  const fs::path local  = fs::temp_directory_path() / ("gitfly_local_"  + std::to_string(std::random_device{}()));
  fs::create_directories(remote);
  fs::create_directories(local);

  try {
    // Initialize remote with two commits
    {
      gitfly::Repository repo{remote};
      repo.init(gitfly::Identity{"Remote", "r@example.com"});
      write_file(remote / "r.txt", "one\n");
      gitfly::Index idx{remote}; idx.load(); idx.add_path(remote, "r.txt", repo, gitfly::consts::kModeFile); idx.save();
      (void)repo.commit_index("c1\n");
      write_file(remote / "r.txt", "one\ntwo\n");
      idx.load(); idx.add_path(remote, "r.txt", repo, gitfly::consts::kModeFile); idx.save();
      (void)repo.commit_index("c2\n");
    }

    // Clone into local
    gitfly::remote::clone_repo(remote, local);
    {
      auto head_txt = gitfly::read_HEAD(local);
      if (!head_txt || head_txt->rfind("ref:",0)!=0) { std::cerr << "clone: local HEAD not symbolic\n"; return 1; }
      std::string rn = head_txt->substr(gitfly::consts::kRefPrefix.size()); while (!rn.empty() && (rn.back()=='\n'||rn.back()=='\r')) rn.pop_back();
      auto local_tip  = gitfly::read_ref(local, rn);
      auto remote_tip = gitfly::read_ref(remote, rn);
      if (!local_tip || !remote_tip || *local_tip != *remote_tip) { std::cerr << "clone: tips differ\n"; return 1; }
      // working file content should be present
      auto bytes = gitfly::fs::read_file(local / "r.txt");
      if (std::string(bytes.begin(), bytes.end()).find("two") == std::string::npos) {
        std::cerr << "clone: working tree missing content\n"; return 1;
      }
    }

    // Create local commit and push to remote
    {
      gitfly::Repository lrepo{local};
      write_file(local / "l.txt", "local\n");
      gitfly::Index idx{local}; idx.load(); idx.add_path(local, "l.txt", lrepo, gitfly::consts::kModeFile); idx.save();
      const std::string local_tip = lrepo.commit_index("local\n");
      gitfly::remote::push_branch(local, remote, "master");
      auto remote_tip = gitfly::read_ref(remote, gitfly::heads_ref("master"));
      if (!remote_tip || *remote_tip != local_tip) { std::cerr << "push: remote tip mismatch\n"; return 1; }
    }

    // Advance remote and fetch into local tracking ref
    std::string new_remote_tip;
    {
      gitfly::Repository rrepo{remote};
      write_file(remote / "r.txt", "one\ntwo\nthree\n");
      gitfly::Index idx{remote}; idx.load(); idx.add_path(remote, "r.txt", rrepo, gitfly::consts::kModeFile); idx.save();
      new_remote_tip = rrepo.commit_index("more\n");
    }
    {
      auto fres = gitfly::remote::fetch_head(local, remote, "origin");
      auto track = gitfly::read_ref(local, std::string("refs/remotes/origin/") + fres.branch);
      if (fres.branch != "master" || !track || *track != new_remote_tip) { std::cerr << "fetch: tracking ref mismatch\n"; return 1; }
      // And ensure the fetched commit object can be read from local object store
      gitfly::Repository lrepo{local};
      (void)lrepo.read_commit(new_remote_tip);
    }

    std::cout << "remote_fs OK\n";
  } catch (const std::exception& e) {
    std::cerr << "exception: " << e.what() << "\n";
    fs::remove_all(remote);
    fs::remove_all(local);
    return 1;
  }
  std::error_code ec; fs::remove_all(remote, ec); fs::remove_all(local, ec);
  return 0;
}

