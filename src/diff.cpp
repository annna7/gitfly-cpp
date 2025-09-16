#include "gitfly/diff.hpp"

#include <algorithm>
#include <sstream>

namespace gitfly::diff {

std::vector<std::string> split_lines(std::string_view text) {
  std::vector<std::string> out;
  std::string cur;
  for (const char c : text) {
    if (c == '\n') {
      out.push_back(std::move(cur));
      cur.clear();
    } else if (c != '\r') {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) {
    out.push_back(std::move(cur));
  }
  return out;
}

// Myers O(ND) diff to produce ops: '=' keep, '-' del, '+' add.
static void myers_diff(const std::vector<std::string> &a, const std::vector<std::string> &b,
                       std::vector<char> &ops) {
  const int N = static_cast<int>(a.size());
  const int M = static_cast<int>(b.size());
  const int MAX = N + M;
  const int OFFSET = MAX;
  std::vector<int> v(2 * MAX + 1, 0);
  std::vector<std::vector<int>> trace;
  trace.reserve(MAX + 1);

  for (int d = 0; d <= MAX; ++d) {
    trace.push_back(v); // snapshot before exploring this D layer
    for (int k = -d; k <= d; k += 2) {
      int x;
      if (k == -d || (k != d && v[OFFSET + k - 1] < v[OFFSET + k + 1])) {
        x = v[OFFSET + k + 1];     // down (insertion)
      } else {
        x = v[OFFSET + k - 1] + 1; // right (deletion)
      }
      int y = x - k;
      while (x < N && y < M && a[x] == b[y]) { ++x; ++y; }
      v[OFFSET + k] = x;
      if (x >= N && y >= M) {
        // backtrack using snapshots
        std::vector<char> rev_ops;
        int cx = N, cy = M;
        for (int dd = d; dd >= 0; --dd) {
          const auto &vv = trace[dd];
          int kk = cx - cy;
          int prev_k;
          bool down;
          if (kk == -dd || (kk != dd && vv[OFFSET + kk - 1] < vv[OFFSET + kk + 1])) {
            prev_k = kk + 1; down = true;
          } else { prev_k = kk - 1; down = false; }
          int px = vv[OFFSET + prev_k];
          int py = px - prev_k;
          if (!down) ++px; // came from right => consumed from a
          while (cx > px && cy > py) { rev_ops.push_back('='); --cx; --cy; }
          if (dd > 0) rev_ops.push_back(down ? '+' : '-');
          cx = px; cy = py;
        }
        ops.assign(rev_ops.rbegin(), rev_ops.rend());
        return;
      }
    }
  }
}

std::string unified_diff(const std::vector<std::string> &a, const std::vector<std::string> &b,
                         std::string_view path) {
  std::vector<char> ops;
  ops.reserve(a.size() + b.size());
  myers_diff(a, b, ops);

  std::ostringstream out;
  out << "--- a/" << path << "\n";
  out << "+++ b/" << path << "\n";
  out << "@@\n"; // simplified hunk header
  size_t ia = 0;
  size_t ib = 0;
  for (const char op : ops) {
    if (op == '=') {
      out << ' ' << a[ia++] << "\n";
      ++ib;
    } else if (op == '-') {
      out << '-' << a[ia++] << "\n";
    } else {
      out << '+' << b[ib++] << "\n";
    }
  }
  // Emit any remaining trailing deletions/additions not covered in ops
  while (ia < a.size()) {
    out << '-' << a[ia++] << "\n";
  }
  while (ib < b.size()) {
    out << '+' << b[ib++] << "\n";
  }
  return out.str();
}

} // namespace gitfly::diff
