#include "gitfly/tcp_remote.hpp"

#include "gitfly/consts.hpp"
#include "gitfly/fs.hpp"
#include "gitfly/refs.hpp"
#include "gitfly/repo.hpp"
#include "gitfly/worktree.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <netdb.h>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace stdfs = std::filesystem;

namespace {

class UniqueFd {
public:
  UniqueFd() = default;
  explicit UniqueFd(int fd) noexcept : fd_{fd} {}

  UniqueFd(const UniqueFd &) = delete;
  auto operator=(const UniqueFd &) -> UniqueFd & = delete;

  UniqueFd(UniqueFd &&other) noexcept : fd_{other.fd_} { other.fd_ = -1; }
  auto operator=(UniqueFd &&other) noexcept -> UniqueFd & {
    if (this != &other) {
      close_if_open();
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }

  ~UniqueFd() { close_if_open(); }

  [[nodiscard]] auto valid() const noexcept -> bool { return fd_ != -1; }
  [[nodiscard]] explicit operator bool() const noexcept { return valid(); }
  [[nodiscard]] auto get() const noexcept -> int { return fd_; }

  void reset(int fd = -1) noexcept {
    if (fd_ != fd) {
      close_if_open();
      fd_ = fd;
    }
  }

private:
  int fd_{-1};

  void close_if_open() noexcept {
    if (fd_ != -1) {
      // best effort; no throw in destructor
      ::close(fd_);
      fd_ = -1;
    }
  }
};

[[nodiscard]] auto gai_error_to_exception(int rc, std::string_view where, std::string_view host,
                                          int port) -> std::runtime_error {
  std::ostringstream os;
  os << where << " failed for " << host << ":" << port << " — " << gai_strerror(rc);
  return std::runtime_error(os.str());
}

[[nodiscard]] auto connect_tcp(const std::string &host, int port) -> UniqueFd {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  addrinfo *res = nullptr;
  const std::string port_s = std::to_string(port);

  if (const int rc = ::getaddrinfo(host.c_str(), port_s.c_str(), &hints, &res); rc != 0) {
    throw gai_error_to_exception(rc, "getaddrinfo", host, port);
  }

  UniqueFd sock;
  for (addrinfo *rp = res; rp != nullptr; rp = rp->ai_next) {
    const int fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd == -1) {
      continue;
    }
    if (::connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      sock.reset(fd);
      break;
    }
    const int saved = errno;
    ::close(fd);
    // try next addr; if none succeed, we’ll throw below with the last errno
    errno = saved;
  }
  const int saved_errno = errno; // preserve before freeaddrinfo
  ::freeaddrinfo(res);

  if (!sock) {
    throw std::system_error(saved_errno, std::generic_category(), "connect");
  }
  return sock;
}

void send_all(int fd, const void *buf, size_t n) {
  const auto *p = static_cast<const std::uint8_t *>(buf);
  while (n != 0U) {
    const ssize_t w = ::send(fd, p, n, 0);
    if (w <= 0) {
      throw std::system_error(errno, std::generic_category(), "send");
    }
    p += static_cast<size_t>(w);
    n -= static_cast<size_t>(w);
  }
}

void send_line(int fd, std::string_view s) {
  std::string t(s);
  t.push_back('\n');
  send_all(fd, t.data(), t.size());
}

void recv_exact(int fd, std::uint8_t *dst, size_t n) {
  size_t got = 0;
  while (got < n) {
    const ssize_t r = ::recv(fd, dst + got, n - got, 0);
    if (r <= 0) {
      throw std::system_error(errno, std::generic_category(), "recv");
    }
    got += static_cast<size_t>(r);
  }
}

[[nodiscard]] auto recv_line(int fd) -> std::string {
  std::string s;
  char c = '\0';
  for (;;) {
    const ssize_t r = ::recv(fd, &c, 1, 0);
    if (r <= 0) {
      throw std::system_error(errno, std::generic_category(), "recv");
    }
    if (c == '\n') {
      break;
    }
    s.push_back(c);
  }
  return s;
}

[[nodiscard]] auto list_object_files(const stdfs::path &objects_dir) -> std::vector<stdfs::path> {
  std::vector<stdfs::path> files;
  if (!stdfs::exists(objects_dir)) {
    return files;
  }
  for (stdfs::recursive_directory_iterator it(objects_dir), end; it != end; ++it) {
    if (it->is_regular_file()) {
      files.push_back(it->path());
    }
  }
  return files;
}

[[nodiscard]] auto hex_from_objects_rel(std::string rel) -> std::string {
  // "aa/bbbbb..." -> "aabbbbb..."
  std::erase(rel, '/');
  return rel;
}

void send_all_objects(int fd, const stdfs::path &objects_dir) {
  const auto files = list_object_files(objects_dir);
  send_line(fd, "NOBJ " + std::to_string(files.size()));

  for (const auto &p : files) {
    const auto rel = stdfs::relative(p, objects_dir).generic_string();
    const std::string hex = hex_from_objects_rel(rel);
    const auto data = gitfly::fs::read_file(p);
    send_line(fd, "OBJ " + hex + " " + std::to_string(data.size()));
    if (!data.empty()) {
      send_all(fd, data.data(), data.size());
    }
  }
  send_line(fd, "DONE");
}

struct RefInfo {
  std::string branch; // "DETACHED" if detached
  std::string oid;    // 40-hex or empty
};

// Parses: "REF <branch> <oid>" or "REF DETACHED <oid>"
[[nodiscard]] auto parse_ref_header(std::string_view line) -> RefInfo {
  if (!line.starts_with("REF ")) {
    throw std::runtime_error("expected 'REF ' header");
  }
  line.remove_prefix(4);

  const auto sp = line.find(' ');
  if (sp == std::string_view::npos) {
    // Could be "REF <branch>" (no oid) — allow empty oid
    return RefInfo{std::string(line), std::string{}};
  }
  RefInfo out;
  out.branch = std::string(line.substr(0, sp));
  out.oid = std::string(line.substr(sp + 1));
  return out;
}

void recv_objects_into(int fd, const stdfs::path &objects_dir) {
  const std::string nline = recv_line(fd);
  if (!std::string_view(nline).starts_with("NOBJ ")) {
    throw std::runtime_error("expected NOBJ <n>");
  }
  const size_t n = std::stoull(nline.substr(gitfly::consts::kRefPrefix.size()));

  stdfs::create_directories(objects_dir);

  for (size_t i = 0; i < n; ++i) {
    const std::string oline = recv_line(fd);
    if (!std::string_view(oline).starts_with("OBJ ")) {
      throw std::runtime_error("expected OBJ <hex> <size>");
    }

    std::istringstream is(oline.substr(4));
    std::string hex;
    size_t sz = 0;
    is >> hex >> sz;
    if (hex.size() < 3U) {
      throw std::runtime_error("malformed object hex in OBJ header");
    }

    std::vector<std::uint8_t> buf(sz);
    if (sz != 0U) {
      recv_exact(fd, buf.data(), sz);
    }

    const stdfs::path dir = objects_dir / hex.substr(0, 2);
    stdfs::create_directories(dir);
    const stdfs::path file = dir / hex.substr(2);
    gitfly::fs::write_file_atomic(file, buf);
  }

  const std::string done = recv_line(fd);
  if (done != "DONE") {
    throw std::runtime_error("expected DONE after objects");
  }
}

} // namespace

namespace gitfly::tcpremote {

void push_branch(const std::string &host, int port, const std::string &repo_root,
                 const std::string &branch) {
  auto sock = connect_tcp(host, port);

  send_line(sock.get(), "HELLO 1");
  send_line(sock.get(), "OP PUSH " + branch);

  Repository repo{stdfs::path{repo_root}};
  const auto head_txt = read_HEAD(repo.root());
  if (!head_txt || head_txt->rfind("ref:", 0) != 0) {
    throw std::runtime_error("push requires symbolic HEAD");
  }
  const auto tip = read_ref(repo.root(), heads_ref(branch));
  if (!tip) {
    throw std::runtime_error("local branch has no tip");
  }

  send_line(sock.get(), "NEW " + *tip);

  const std::string okgo = recv_line(sock.get());
  if (okgo != "OKGO") {
    throw std::runtime_error("server refused push (expected OKGO)");
  }

  send_all_objects(sock.get(), repo.objects_dir());

  const std::string resp = recv_line(sock.get());
  if (resp != "OK") {
    throw std::runtime_error("push failed: " + resp);
  }
}

void clone_repo(const std::string &host, int port, const std::string &dest_root) {
  auto sock = connect_tcp(host, port);

  send_line(sock.get(), "HELLO 1");
  send_line(sock.get(), "OP CLONE");

  const RefInfo ref = parse_ref_header(recv_line(sock.get()));

  const stdfs::path objdir = stdfs::path(dest_root) / consts::kGitDir / consts::kObjectsDir;

  recv_objects_into(sock.get(), objdir);

  // Init basic repo structure and set HEAD / refs
  Repository repo{stdfs::path{dest_root}};
  stdfs::create_directories(repo.heads_dir());
  stdfs::create_directories(repo.tags_dir());

  if (ref.branch != "DETACHED") {
    set_HEAD_symbolic(dest_root, heads_ref(ref.branch));
    if (!ref.oid.empty()) {
      update_ref(dest_root, heads_ref(ref.branch), ref.oid);
    }
  } else {
    set_HEAD_detached(dest_root, ref.oid);
  }

  // Materialize working tree and index if we have a tip OID
  if (!ref.oid.empty()) {
    const auto info = repo.read_commit(ref.oid);
    const auto snap = worktree::tree_to_map(repo, info.tree_hex);
    worktree::apply_snapshot(repo, snap);
    worktree::write_index_snapshot(repo, snap);
  }
}

auto fetch_head(const std::string &host, int port, const std::string &local_root,
                const std::string &remote_name) -> FetchResult {
  auto sock = connect_tcp(host, port);

  send_line(sock.get(), "HELLO 1");
  send_line(sock.get(), "OP FETCH");

  const RefInfo ref = parse_ref_header(recv_line(sock.get()));

  const stdfs::path objdir = stdfs::path(local_root) / consts::kGitDir / consts::kObjectsDir;

  recv_objects_into(sock.get(), objdir);

  if (!ref.oid.empty() && ref.branch != "DETACHED") {
    Repository local_repo{stdfs::path{local_root}};
    const auto remdir = local_repo.refs_dir() / "remotes" / remote_name;
    stdfs::create_directories(remdir);
    update_ref(local_repo.root(), std::string("refs/remotes/") + remote_name + "/" + ref.branch,
               ref.oid);
  }

  return FetchResult{.branch = ref.branch, .tip = ref.oid};
}

} // namespace gitfly::tcpremote
