#include "gitfly/diff.hpp"
#include <iostream>
#include <vector>

int main() {
  using gitfly::diff::unified_diff;
  using gitfly::diff::split_lines;

  const char* A = "line1\nline2\nline3\n";
  const char* B = "line1\nlineZ\nline3\nline4\n";
  auto a = split_lines(A);
  auto b = split_lines(B);
  auto ud = unified_diff(a, b, "demo.txt");
  if (ud.find("--- a/demo.txt") == std::string::npos) { std::cerr << "missing header\n"; return 1; }
  if (ud.find("+++ b/demo.txt") == std::string::npos) { std::cerr << "missing header2\n"; return 1; }
  if (ud.find("-line2") == std::string::npos) { std::cerr << "missing deletion\n"; return 1; }
  if (ud.find("+lineZ") == std::string::npos) { std::cerr << "missing addition\n"; return 1; }
  if (ud.find("+line4") == std::string::npos) { std::cerr << "missing trailing addition\n"; return 1; }
  std::cout << "OK\n";
  return 0;
}

