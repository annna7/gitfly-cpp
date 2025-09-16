#include "cli/registry.hpp"

int cmd_init(int argc, char **argv);
int cmd_add(int argc, char **argv);
int cmd_commit(int argc, char **argv);
int cmd_status(int, char **);
int cmd_checkout(int, char **);
int cmd_branch(int, char **);
int cmd_log(int, char **);
int cmd_merge(int, char **);
int cmd_diff(int, char **);
int cmd_clone(int, char **);
int cmd_push(int, char **);
int cmd_serve(int, char **);
int cmd_fetch(int, char **);
int cmd_pull(int, char **);

namespace gitfly::cli {

void register_all_commands() {
  register_command("init", ::cmd_init, "Initialize a new repository");
  register_command("add", ::cmd_add, "Add file(s) to the index: gitfly add <path>...");
  register_command("commit", ::cmd_commit, "Commit staged changes: gitfly commit -m <message>");
  register_command("status", ::cmd_status, "Show staged/unstaged/untracked changes");
  register_command("checkout", ::cmd_checkout,
                   "Switch to branch/commit: gitfly checkout <name|oid>");
  register_command("branch", ::cmd_branch, "Create branch: gitfly branch <name>");
  register_command("log", ::cmd_log, "Show commit log from HEAD");
  register_command("merge", ::cmd_merge, "Merge branch into current: gitfly merge <name>");
  register_command("diff", ::cmd_diff, "Show diffs (working vs index or --cached)");
  register_command("clone", ::cmd_clone, "Clone a repository: gitfly clone <src> <dest>");
  register_command("push", ::cmd_push,
                   "Push current branch to local path: gitfly push <path> [branch]");
  register_command("serve", ::cmd_serve, "Serve this repo over TCP: gitfly serve [port]");
  register_command("fetch", ::cmd_fetch, "Fetch from remote: gitfly fetch <remote> [name]");
  register_command("pull", ::cmd_pull, "Fetch + integrate: gitfly pull <remote> [name]");
}

} // namespace gitfly::cli
