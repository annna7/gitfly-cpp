#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

# Demo script: recreates mock-dir-for-testing and runs a walkthrough
# of core gitfly commands, including merge edge cases.

bold() { printf "\033[1m%s\033[0m\n" "$*"; }
cmd()  {
  printf "\033[36m$"
  for s in "$@"; do printf " %q" "$s"; done
  printf "\033[0m\n"
}
hr()   { printf "\n\033[90m%s\033[0m\n" "$(printf '—%.0s' {1..80})"; }

REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEMO_DIR="$REPO_ROOT/mock-dir-for-testing"
BUILD_DIR="$REPO_ROOT/build"
GF_BIN="$BUILD_DIR/gitfly"

bold "Rebuilding gitfly (if needed)..."
OPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR:-$(brew --prefix openssl@3 2>/dev/null || true)}"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR" >/dev/null
cmake --build "$BUILD_DIR" >/dev/null

bold "Setting up fresh demo directory: $DEMO_DIR"
rm -rf "$DEMO_DIR"
mkdir -p "$DEMO_DIR"
cd "$DEMO_DIR"

run() {
  cmd "$GF_BIN" "$@"
  set +e
  "$GF_BIN" "$@"
  rc=$?
  set -e
  printf "(exit %d)\n" "$rc"
  hr
  return $rc
}

show() {
  local path="$1"
  if [[ -f "$path" ]]; then
    cmd "cat $path"
    sed -e 's/\t/  /g' "$path" || true
    hr
  fi
}

bold "1) Init"
run init
show .gitfly/HEAD
show .gitfly/config

bold "2) First commit"
echo "hello" > a.txt
run add a.txt
run status
run commit -m "initial"
OID0=$(cat .gitfly/refs/heads/master)
printf "master@ %s\n" "$OID0"
hr

bold "3) Create base for conflict"
echo "base" > conflict.txt
run add conflict.txt
run commit -m "add conflict base"
BASE=$(cat .gitfly/refs/heads/master)
hr

bold "4) Create feature branch"
run branch feature

bold "5) Advance master differently"
echo "master change" > conflict.txt
run add conflict.txt
run commit -m "master change"
MASTER_TIP=$(cat .gitfly/refs/heads/master)
hr

bold "6) Switch to feature and diverge"
run checkout feature
echo "feature change" > conflict.txt
run add conflict.txt
run commit -m "feature change"
FEATURE_TIP=$(cat .gitfly/refs/heads/feature)
hr

bold "7) Merge feature into master (expect conflict)"
run checkout master
if run merge feature; then
  bold "Unexpected: merge had no conflict."
else
  bold "As expected: merge reported conflict."
fi
show conflict.txt
run status
show .gitfly/MERGE_HEAD

bold "8) Finalize merge (resolve + commit)"
echo "resolved content" > conflict.txt
run add conflict.txt
run commit -m "merge resolved"
run log

bold "9) Fast-forward merge demo"
run branch topic
run checkout topic
echo "topic work" > topic.txt
run add topic.txt
run commit -m "topic work"
run checkout master
run merge topic
run log

bold "10) Detached HEAD checkout"
HEAD_OID=$(cat .gitfly/refs/heads/master)
run checkout "$HEAD_OID"
run status

bold "Demo completed. Directory left at: $DEMO_DIR"
