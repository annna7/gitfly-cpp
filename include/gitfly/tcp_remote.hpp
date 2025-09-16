#pragma once
#include <string>

namespace gitfly::tcpremote {

struct FetchResult { std::string branch; std::string tip; };

// Client-side helpers for talking to `gitfly serve` over TCP.
void push_branch(const std::string& host, int port,
                 const std::string& repo_root,
                 const std::string& branch);

void clone_repo(const std::string& host, int port,
                const std::string& dest_root);

// Fetch remote HEAD into local repo as refs/remotes/<name>/<branch>.
FetchResult fetch_head(const std::string& host, int port,
                       const std::string& local_root,
                       const std::string& name = "origin");

} // namespace gitfly::tcpremote
