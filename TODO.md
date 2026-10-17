# To do before master

What this branch still lacks before it may move to `master`: a slowdown against what `master` ships, a fix no
program proves yet, a step that could not be finished. One line per thing. The file belongs to the branch it is on:
a branch merged into another brings its lines with it, and before a branch moves to `master` every line is worked
through (fixed, or opened as an issue labelled `before-master` when it cannot be fixed now) and deleted, the way
`mortaros_notes.md` is emptied. `master` takes this file with nothing under "Missing": its `pre-push` hook refuses a
push to `master` while a line is left (D624). What is decided and not built belongs in the
[GitHub issues](https://github.com/Mortaro/spite-language/issues), never here (D623).

## Missing

- The codecs and hashes on `Buffer` (Sha256, Deflate, Argon2) run 23 to 50% slower than on `master` until the compiler passes a list argument it only reads without counting it, keeps a local `Buffer(n)` in the frame, and proves `list[base + counter]` in bounds; time them with `bash benchmarks/run.sh` before and after (D590, issue 230)
- `Process.run` reads the pipe in binary mode on Windows so a byte 0x1A no longer ends the output, but no program proves it yet: running a child process that writes 0x1A through `Process` hung in a test, which needs finding too (issue 123)
