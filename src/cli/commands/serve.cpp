#include "gitfly/consts.hpp"
#include "gitfly/fs.hpp"
#include "gitfly/refs.hpp"
#include "gitfly/repo.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <filesystem>
#include <iostream>
#include <netinet/in.h>
#include <set>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

static void write_all(int fd, const void *buf, size_t n) {
  const char *p = static_cast<const char *>(buf);
  while (n) {
    ssize_t w = ::send(fd, p, n, 0);
    if (w <= 0)
      throw std::runtime_error("send failed");
    p += w;
    n -= static_cast<size_t>(w);
  }
}

static void write_line(int fd, const std::string &s) {
  std::string t = s;
  t += "\n";
  write_all(fd, t.data(), t.size());
}

static std::string read_line(int fd) {
  std::string s;
  char c;
  while (true) {
    ssize_t r = ::recv(fd, &c, 1, 0);
    if (r <= 0)
      throw std::runtime_error("recv failed");
    if (c == '\n')
      break;
    s.push_back(c);
  }
  return s;
}

static void send_all_objects(int fd, const fs::path &objects_dir) {
  std::vector<fs::path> files;
  for (auto it = fs::recursive_directory_iterator(objects_dir);
       it != fs::recursive_directory_iterator(); ++it) {
    if (!it->is_regular_file())
      continue;
    files.push_back(it->path());
  }
  write_line(fd, std::string("NOBJ ") + std::to_string(files.size()));
  for (auto &p : files) {
    const auto rel = fs::relative(p, objects_dir).generic_string();
    std::string hex = rel;
    hex.erase(std::ranges::remove(hex, '/').begin(), hex.end());
    auto data = gitfly::fs::read_file(p);
    write_line(fd, std::string("OBJ ") + hex + " " + std::to_string(data.size()));
    write_all(fd, data.data(), data.size());
  }
  write_line(fd, "DONE");
}

static void recv_objects_into(int fd, const fs::path &objects_dir) {
  auto nline = read_line(fd);
  if (nline.rfind("NOBJ ", 0) != 0)
    throw std::runtime_error("bad NOBJ");
  size_t n = std::stoull(nline.substr(gitfly::consts::kRefPrefix.size()));
  for (size_t i = 0; i < n; ++i) {
    auto oline = read_line(fd);
    if (oline.rfind("OBJ ", 0) != 0)
      throw std::runtime_error("bad OBJ");
    std::istringstream is(oline.substr(4));
    std::string hex;
    size_t sz;
    is >> hex >> sz;
    std::vector<std::uint8_t> buf(sz);
    size_t got = 0;
    while (got < sz) {
      ssize_t r = ::recv(fd, buf.data() + got, sz - got, 0);
      if (r <= 0)
        throw std::runtime_error("recv obj failed");
      got += static_cast<size_t>(r);
    }
    fs::path dir = objects_dir / hex.substr(0, 2);
    fs::create_directories(dir);
    gitfly::fs::write_file_atomic(dir / hex.substr(2), buf);
  }
  auto done = read_line(fd);
  (void)done;
}

static void handle_client(int cfd, gitfly::Repository &repo) {
  auto hello = read_line(cfd);
  (void)hello;
  auto op = read_line(cfd);
  if (op.rfind("OP CLONE", 0) == 0 || op.rfind("OP FETCH", 0) == 0) {
    // advertise current branch + tip
    auto head_txt = gitfly::read_HEAD(repo.root());
    std::string branch = "DETACHED", tip;
    if (head_txt) {
      std::string s = *head_txt;
      while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
        s.pop_back();
      if (s.rfind("ref:", 0) == 0) {
        std::string rn = s.substr(gitfly::consts::kRefPrefix.size());
        branch =
            rn.rfind("refs/heads/", 0) == 0 ? rn.substr(std::string("refs/heads/").size()) : rn;
        if (auto t = gitfly::read_ref(repo.root(), rn); t)
          tip = *t;
      } else {
        tip = s;
        branch = "DETACHED";
      }
    }
    write_line(cfd, std::string("REF ") + branch + " " + tip);
    send_all_objects(cfd, repo.objects_dir());
  } else if (op.rfind("OP PUSH ", 0) == 0) {
    std::string branch = op.substr(8);
    auto nline = read_line(cfd);
    if (nline.rfind("NEW ", 0) != 0)
      throw std::runtime_error("bad NEW");
    std::string new_oid = nline.substr(4);
    write_line(cfd, "OKGO");
    recv_objects_into(cfd, repo.objects_dir());
    // fast-forward check
    auto cur_tip = gitfly::read_ref(repo.root(), gitfly::heads_ref(branch));
    if (cur_tip) {
      // reuse merge helper by creating a temporary repository instance
      auto is_ancestor = [&](const std::string &anc, const std::string &desc) {
        if (anc == desc)
          return true;
        std::vector<std::string> st{desc};
        std::set<std::string> seen;
        while (!st.empty()) {
          auto cur = st.back();
          st.pop_back();
          if (!seen.insert(cur).second)
            continue;
          auto info = repo.read_commit(cur);
          for (auto &p : info.parents) {
            if (p == anc)
              return true;
            st.push_back(p);
          }
        }
        return false;
      };
      if (!is_ancestor(*cur_tip, new_oid)) {
        write_line(cfd, "ERR non-fast-forward");
        return;
      }
    }
    gitfly::update_ref(repo.root(), gitfly::heads_ref(branch), new_oid);
    write_line(cfd, "OK");
  } else {
    write_line(cfd, "ERR unknown op");
  }
}

auto cmd_serve(int argc, char **argv) -> int {
  int port = gitfly::consts::portNumber;
  if (argc >= 2)
    port = std::stoi(argv[1]);
  gitfly::Repository repo{std::filesystem::current_path()};
  if (!repo.is_initialized()) {
    std::cerr << "serve: not a gitfly repo\n";
    return 1;
  }

  int sfd = ::socket(AF_INET6, SOCK_STREAM, 0);
  if (sfd == -1) {
    perror("socket");
    return 1;
  }
  int yes = 1;
  setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  struct sockaddr_in6 addr{};
  addr.sin6_family = AF_INET6;
  addr.sin6_addr = in6addr_any;
  addr.sin6_port = htons(port);
  if (bind(sfd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) != 0) {
    perror("bind");
    close(sfd);
    return 1;
  }
  if (listen(sfd, 16) != 0) {
    perror("listen");
    close(sfd);
    return 1;
  }
  std::cout << "gitfly serve listening on port " << port << " (Ctrl+C to stop)\n";
  while (true) {
    int cfd = accept(sfd, nullptr, nullptr);
    if (cfd < 0) {
      perror("accept");
      continue;
    }
    try {
      handle_client(cfd, repo);
    } catch (const std::exception &e) {
      std::cerr << "serve: " << e.what() << "\n";
    }
    ::close(cfd);
  }
}
