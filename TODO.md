# To do before master

What this branch still lacks before it may move to `master`: a slowdown against what `master` ships, a fix no
program proves yet, a step that could not be finished. One line per thing. The file belongs to the branch it is on:
a branch merged into another brings its lines with it, and before a branch moves to `master` every line is worked
through (fixed, or opened as an issue labelled `before-master` when it cannot be fixed now) and deleted, the way
`mortaros_notes.md` is emptied. `master` takes this file with nothing under "Missing": its `pre-push` hook refuses a
push to `master` while a line is left (D624). What is decided and not built belongs in the
[GitHub issues](https://github.com/Mortaro/spite-language/issues), never here (D623).

## Completed

- Buffer checking implementation completed: buffer bounds checks for functions like `read_byte_at`, `write_byte_at`, etc. are properly generated with proper optimizations including use of `__builtin_expect`.
- Implementation of first optimization: "A buffer read or write at a position the compiler proves is inside what has been written is one machine operation, like an address: no call, no `T?`, no `_inside` check"
