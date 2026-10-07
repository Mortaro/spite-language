# Concurrency machinery only where it is used

The scheduler, the state machines, the helper threads and the wrappers around every call that can wait exist only
in a program that makes a `Concurrent` (or overlaps reads in a row) or is built for the REPL or live reload. Every
other program's waits are the plain system calls, so a program that reads and writes files without a `Concurrent`
carries and runs none of it.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#concurrency-machinery-only-where-it-is-used).
- The proof: none of its own; it stands on
  [Which calls suspend a `Concurrent`](../../docs/proofs.md#which-calls-suspend-a-concurrent), which finds no
  function to make resumable in a program with no `Concurrent`, and on
  [Tree shaking](../../docs/proofs.md#tree-shaking-what-main-can-reach), which then drops the scheduler.

## The four forms

- [`naive/`](naive/): 50 rounds that each write a line to a file and read it back, adding up the characters read,
  then the file is removed.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: each write and each read opens
  the file with `fopen`, and the read measures it and reads it into a new buffer, with the C library's blocking
  calls.
- [`expert.c`](expert.c): the same work tuned by hand: the file opened once with the system's own calls, and each
  round written from the start, cut to its length and read back into a buffer on the stack.
- [`highlights.c`](highlights.c): the loop, and the `File` functions it reaches for a read.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_write_and_read___held_0` calls `File_read(notes_)` as an ordinary function, and `File_read` calls
`File_open_file`, `File_read_into` and the rest straight through to `fopen` and `fread`: no frame, no step function,
no helper thread and no event loop, the plain blocking calls in the order `naive.c` makes them. The whole C has no
`Scheduler` and no state machine: the word appears once, in a leftover `SPITE_ALLOCATOR_List_SchedulerLoop` macro
that nothing uses, against 438 lines that name it in the variant below. `naive.c` makes the same calls; `expert.c` keeps the file open, which saves the
opens and closes that are most of a round's time on Windows.

The same program with `var reading = Concurrent(notes.read)` in place of `var read = notes.read()` carries all of
it. Both written with `--check --c-source --optimized` and counted with `grep -c` and `wc -c`, and their allocations
counted by `--debug-memory`:

| Program | Lines of C | Bytes of C | Allocations |
|---|---|---|---|
| this program | 2 094 | 108 753 | 156 |
| the same with the read in a `Concurrent` | 5 053 | 285 969 | 528 |

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 37 798 | 179 200 |
| naive C: `naive.c`, `clang -O2` | 39 005 | 157 696 |
| expert C: `expert.c`, `clang -O2` | 1 675 | 142 848 |

Spite takes 0.97 times naive C's time and 22.57 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=37798 naive=39005 expert=1675 -->
<!-- /timings -->
