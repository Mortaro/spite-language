# Thread safety for singletons, the rest of the plan

The forms built so far are nothing for read-only state or a singleton no `Parallel` reaches, atomics for counters
and flags, and the lock (with its readers' side) as the fallback. The plan adds two more, so fewer singletons fall
back to the lock: state that is only appended to (a log, a command queue) gets a buffer per thread, merged in order,
and state each thread touches its own part of is split per thread; with them comes a compile-time check that a
singleton's functions hand out only numbers, text, copies or singletons made safe the same way.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#thread-safety-for-singletons-the-rest-of-the-plan).
- The proof: none yet, since none of the planned forms is built; the forms built so far stand on
  [Read-only and atomic singletons](../../docs/proofs.md#read-only-and-atomic-singletons).

## The four forms

- [`naive/`](naive/): four `Parallel` loggers, each recording a quarter of a million entries into one `EventLog`
  singleton that only appends them to a `List<Integer>`, then the program reads how many it holds.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: one log behind a mutex, four
  threads, every append taking and letting go of the mutex.
- [`expert.c`](expert.c): the same work tuned by hand, as the planned append-only form would make it: each thread
  appends to a buffer of its own with no lock, and the program's thread merges the four buffers in the threads'
  order once they are done.
- [`highlights.c`](highlights.c): the logger's loop, the locked `record` and its unlocked body.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

This is what the program compiles to today, before the plan. `EventLog` holds a `List`, so none of the built forms
applies and it takes the lock: `EventLog_record` takes `spite_guard_enter(&EventLog___guard)` around
`EventLog_record___unguarded`, a plain `List_Integer_append`, once per entry, since `Logger_log_all` also calls
`entry_for` and so is not a counted loop of calls to one singleton. The singleton's functions only ever append to
`entries` while the loggers run, and `count` reads it only after every `Parallel` has been read, which is the shape
the planned buffer per thread is for: with it, `record` would append to its thread's buffer with no lock and no
shared line, and the buffers would be merged in order before `count` reads them. `naive.c` does what the Spite does
today with a mutex; `expert.c` does what the plan would.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 77 380 | 230 400 |
| naive C: `naive.c`, `clang -O2` | 26 958 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 3 298 | 139 776 |

Spite takes 2.87 times naive C's time and 23.46 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=77380 naive=26958 expert=3298 -->
<!-- /timings -->
