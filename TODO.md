# To do before master

What this branch still lacks before it may move to `master`: a slowdown against what `master` ships, a fix no
program proves yet, a step that could not be finished. One line per thing. The file belongs to the branch it is on:
a branch merged into another brings its lines with it, and before a branch moves to `master` every line is worked
through (fixed, or opened as an issue labelled `before-master` when it cannot be fixed now) and deleted, the way
`mortaros_notes.md` is emptied. `master` takes this file with nothing under "Missing": its `pre-push` hook refuses a
push to `master` while a line is left (D624). What is decided and not built belongs in the
[GitHub issues](https://github.com/Mortaro/spite-language/issues), never here (D623).

## Missing

- Buffer Part 2's codecs and hashes are slower than `master`: measured with each revision's compiler against its own library (`--optimized`, no `--debug-memory`, best of nine runs on a quiet machine), Sha256 over 1 MB is 15.7 ms against 17.8, Deflate 4.4 against 7.7, Argon2 191 against 284, and the combined codecs program is 1.03 to 2.17 times slower in its phases (D590, issue 230). A buffer read with a known byte order is a call answering a `T?` behind a bounds check where master's raw memory was one load, and `list[base + counter]` in a library loop is checked on every access. Two attempts are in and did not move it: reading a value element without the write-back watch, and folding the byte order test while compiling. A third, a `___held_` copy of a library function whose list parameter is only read, was reverted: it broke `conformance/stage6/waiting_systems`, because the compiler's questions about a call do not answer for the `___held_` copy a call is redirected to (the wait analysis, at least: a call to `while_locked` stopped being a wait). See `design/handoff.md`.
