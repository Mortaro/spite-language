# Before master

This file holds one thing: what keeps `next` from being merged into `master` (D620). Everything else that is decided
and not built is an issue in the [GitHub issues](https://github.com/Mortaro/spite-language/issues), and only there, so
the two can never disagree (D623).
Each line here names its issue, which is labelled `before-master`; when the issue is closed, its line goes.

Work that is in `next` but slower than what `master` ships, or not yet proven, is listed here. An agent fixes these
first; when the list is empty and a full check passes, `next` is merged into `master`.

- [#230](https://github.com/Mortaro/spite-language/issues/230): the codecs and hashes on `Buffer` (Buffer Part 2, D590) run 23 to 50% slower than on `master` until the compiler passes a list argument it only reads without counting it, keeps a local `Buffer(n)` in the frame, and proves `list[base + counter]` in bounds; time them with `bash benchmarks/run.sh` before and after
- [#123](https://github.com/Mortaro/spite-language/issues/123): `Process.run` reads the pipe in binary mode on Windows so a byte 0x1A no longer ends the output, but no program proves it yet: running a child process that writes 0x1A through `Process` hung in a test, which needs finding too
