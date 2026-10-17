# To do before master

What this branch still lacks before it may move to `master`: a slowdown against what `master` ships, a fix no
program proves yet, a step that could not be finished. One line per thing. The file belongs to the branch it is on:
a branch merged into another brings its lines with it, and before a branch moves to `master` every line is worked
through (fixed, or opened as an issue labelled `before-master` when it cannot be fixed now) and deleted, the way
`mortaros_notes.md` is emptied. `master` takes this file with nothing under "Missing": its `pre-push` hook refuses a
push to `master` while a line is left (D624). What is decided and not built belongs in the
[GitHub issues](https://github.com/Mortaro/spite-language/issues), never here (D623).

## Missing

- Buffer Part 2 codec performance is unverified against master: measure codecs and hashes with compilers built from both revisions, then fix any slowdown in read-only list arguments, frame-local `Buffer(n)`, or `list[base + counter]` bounds proofs (D590, issue 230)
- Regenerate the 102 benchmark C snapshots that the current compiler writes differently before the full check can pass
- Fix hot-reload and final-classes source tracking so recompiled standard-library code keeps access to Spite.Internal.Lock and ThreadLocal under D601
