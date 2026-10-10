#!/bin/bash
# What a release of the compiler is made from, and where every checkout of this repository keeps what it built.
# Sourced by check.sh and by scripts/hooks/pre-push, so both agree on which build a commit is.
#
# The inputs are every file under bootstrap/, launcher/ and library/: the compiler's sources and seeds, and the two
# folders a release ships beside spite.exe. A change anywhere else (docs/, specs/, design/, tests) needs no new
# release. Each input is named by its path and its git blob, so the working tree and a commit give the same answer
# when their files are the same.

release_folders=(bootstrap launcher library)

# The folder every checkout and worktree of this repository shares, beside the main checkout's .git.
shared_spite_folder() {
  local common
  common=$(git rev-parse --path-format=absolute --git-common-dir) || return 1
  echo "$(dirname "$common")/.spite/shared"
}

# The inputs of the working tree as it is now, committed or not.
working_tree_inputs() {
  git ls-files -c -o --exclude-standard -- "${release_folders[@]}" | LC_ALL=C sort -u | while IFS= read -r path; do
    [ -f "$path" ] && echo "$path"
  done > "$1.paths"
  git hash-object --stdin-paths < "$1.paths" > "$1.blobs"
  paste -d ' ' "$1.paths" "$1.blobs" | sha1sum | cut -d ' ' -f 1
  rm -f "$1.paths" "$1.blobs"
}

# The inputs of a commit.
commit_inputs() {
  git ls-tree -r "$1" -- "${release_folders[@]}" | awk -F '\t' '{ split($1, field, " "); print $2 " " field[3] }' \
    | LC_ALL=C sort | sha1sum | cut -d ' ' -f 1
}
