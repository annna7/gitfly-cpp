#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

# Complex merge-conflict simulation for gitfly.
# - Creates divergent changes across multiple files
# - Triggers line-level conflicts and shows conflict markers
# - Demonstrates resolving and finalizing the merge

bold() { printf "\033[1m%s\033[0m\n" "$*"; }
cmd()  { printf "\033[36m$"; for s in "$@"; do printf " %q" "$s"; done; printf "\033[0m\n"; }
hr()   { printf "\n\033[90m%s\033[0m\n" "$(printf '—%.0s' {1..80})"; }

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR="$ROOT_DIR/build"
GF_BIN="$BUILD_DIR/gitfly"

bold "Building gitfly..."
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" >/dev/null
cmake --build "$BUILD_DIR" >/dev/null

WORKDIR=$(mktemp -d -t gitfly-merge-XXXXXX)
bold "Working directory: $WORKDIR"
cd "$WORKDIR"

run() {
  cmd "$GF_BIN" "$@"
  set +e; "$GF_BIN" "$@"; local rc=$?; set -e
  printf "(exit %d)\n" "$rc"
  return $rc
}

show() {
  local path="$1"
  if [[ -f "$path" ]]; then
    cmd "sed -n '1,120p' $path"
    sed -n '1,120p' "$path" || true
  fi
}

hr; bold "1) Initialize repo and create base commit"
run init
cat > alpha.txt << 'EOF'
alpha L1
alpha L2 base
alpha L3
EOF
cat > beta.txt << 'EOF'
beta L1
beta L2 base
beta L3
EOF
cat > gamma.txt << 'EOF'
gamma L1
gamma L2 base
gamma L3
EOF
run add alpha.txt
run add beta.txt
run add gamma.txt
run commit -m "base files"

BASE=$(cat .gitfly/refs/heads/master)
printf "BASE=%s\n" "$BASE"

hr; bold "2) Create feature branch and diverge both sides"
run branch feature

bold "- Advance master differently"
sed -i '' -e 's/alpha L2 base/alpha L2 master/' alpha.txt
sed -i '' -e 's/beta L2 base/beta L2 master/' beta.txt
echo "master only" > master_only.txt
run add alpha.txt
run add beta.txt
run add master_only.txt
run commit -m "master changes"

MASTER_TIP=$(cat .gitfly/refs/heads/master)
printf "MASTER_TIP=%s\n" "$MASTER_TIP"

bold "- Switch to feature and change conflicting lines"
run checkout feature
sed -i '' -e 's/alpha L2 base/alpha L2 feature/' alpha.txt
sed -i '' -e 's/beta L2 base/beta L2 feature/' beta.txt
echo "feature only" > feature_only.txt
run add alpha.txt
run add beta.txt
run add feature_only.txt
run commit -m "feature changes"

FEATURE_TIP=$(cat .gitfly/refs/heads/feature)
printf "FEATURE_TIP=%s\n" "$FEATURE_TIP"

hr; bold "3) Merge feature into master (expect conflicts in alpha.txt and beta.txt)"
run checkout master
if run merge feature; then
  bold "Unexpected: merge had no conflicts"
else
  bold "As expected: merge reported conflicts"
fi

bold "— Conflicted files preview"
show alpha.txt
hr
show beta.txt

hr; bold "4) Status and MERGE_HEAD"
run status || true
if [[ -f .gitfly/MERGE_HEAD ]]; then
  cmd "cat .gitfly/MERGE_HEAD"; cat .gitfly/MERGE_HEAD
fi

hr; bold "5) Attempt commit without resolving (should fail)"
run commit -m "should fail due to unresolved" || true

hr; bold "6) Resolve conflicts and finalize merge"
cat > alpha.txt << 'EOF'
alpha L1
alpha L2 resolved
alpha L3
EOF
cat > beta.txt << 'EOF'
beta L1
beta L2 resolved
beta L3
EOF
run add alpha.txt
run add beta.txt
run commit -m "merge resolved"
run log

bold "Done. Repo left at: $WORKDIR"

