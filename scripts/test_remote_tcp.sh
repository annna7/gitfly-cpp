#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

# Simple end-to-end test for TCP remote features using localhost.
# It creates a remote repo, serves it over TCP, then clones, pushes, fetches, and pulls.

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR="$ROOT_DIR/build"
GF_BIN="$BUILD_DIR/gitfly"
PORT="${PORT:-gitfly::consts::portNumber}"

bold() { printf "\033[1m%s\033[0m\n" "$*"; }
cmd()  {
  printf "\033[36m$"; for s in "$@"; do printf " %q" "$s"; done; printf "\033[0m\n";
}
hr()   { printf "\n\033[90m%s\033[0m\n" "$(printf '—%.0s' {1..80})"; }

bold "Building gitfly..."
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" >/dev/null
cmake --build "$BUILD_DIR" >/dev/null

REMOTE_DIR=$(mktemp -d -t gitfly-remote-XXXXXX)
LOCAL_DIR=$(mktemp -d -t gitfly-local-XXXXXX)

cleanup() {
  if [[ -n "${SERVE_PID:-}" ]]; then kill "$SERVE_PID" >/dev/null 2>&1 || true; wait "$SERVE_PID" 2>/dev/null || true; fi
  # Uncomment if you want cleanup:
  # rm -rf "$REMOTE_DIR" "$LOCAL_DIR"
}
trap cleanup EXIT

bold "1) Initialize remote repo at $REMOTE_DIR"
(
  cd "$REMOTE_DIR"
  cmd "$GF_BIN" init
  "$GF_BIN" init
  printf "hello\n" > r.txt
  cmd "$GF_BIN" add r.txt; "$GF_BIN" add r.txt
  cmd "$GF_BIN" commit -m initial; "$GF_BIN" commit -m initial
  printf "world\n" >> r.txt
  cmd "$GF_BIN" add r.txt; "$GF_BIN" add r.txt
  cmd "$GF_BIN" commit -m second; "$GF_BIN" commit -m second
)
hr

bold "2) Start TCP server on port $PORT"
cmd "$GF_BIN" serve "$PORT" "&"
# Start the server in a subshell and capture the PID into the parent shell.
SERVE_PID="$(
  cd "$REMOTE_DIR" && \
  "$GF_BIN" serve "$PORT" >/dev/null 2>&1 & echo $!
)"
sleep 0.3
echo "serve PID: ${SERVE_PID:-unknown}"
hr

bold "3) Clone over TCP into $LOCAL_DIR"
(
  cd "$LOCAL_DIR"
  cmd "$GF_BIN" clone "tcp://127.0.0.1:$PORT" .
  "$GF_BIN" clone "tcp://127.0.0.1:$PORT" .
  cmd "$GF_BIN" log; "$GF_BIN" log || true
)
hr

bold "4) Create a local commit and push over TCP"
(
  cd "$LOCAL_DIR"
  printf "local change\n" > local.txt
  cmd "$GF_BIN" add local.txt; "$GF_BIN" add local.txt
  cmd "$GF_BIN" commit -m "local commit"; "$GF_BIN" commit -m "local commit"
  cmd "$GF_BIN" push "tcp://127.0.0.1:$PORT" master; "$GF_BIN" push "tcp://127.0.0.1:$PORT" master
)
hr

bold "5) Advance remote and test fetch + pull"
(
  cd "$REMOTE_DIR"
  printf "remote more\n" >> r.txt
  cmd "$GF_BIN" add r.txt; "$GF_BIN" add r.txt
  cmd "$GF_BIN" commit -m "remote more"; "$GF_BIN" commit -m "remote more"
)
(
  cd "$LOCAL_DIR"
  cmd "$GF_BIN" fetch "tcp://127.0.0.1:$PORT" origin; "$GF_BIN" fetch "tcp://127.0.0.1:$PORT" origin || true
  cmd "$GF_BIN" pull  "tcp://127.0.0.1:$PORT" origin; "$GF_BIN" pull  "tcp://127.0.0.1:$PORT" origin || true
  cmd "$GF_BIN" log; "$GF_BIN" log || true
)

bold "Done. Remote: $REMOTE_DIR, Local: $LOCAL_DIR"
