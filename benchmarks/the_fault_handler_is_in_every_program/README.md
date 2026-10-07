# The fault handler is in every program

Every program installs a handler that reports a native fault (a null read in a foreign library, a stack overflow)
with the Spite function it happened in, because a silent end is a bug; it is the one piece of C nothing
tree-shakes. It is fixed code and a table of the program's functions, not a runtime system: nothing of it runs
until a fault.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#the-fault-handler-is-in-every-program).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); it is a cost, not something proven away,
  and what the table lists is what [Tree shaking: what `main` can
  reach](../../docs/proofs.md#tree-shaking-what-main-can-reach) kept.

## The four forms

- [`naive/`](naive/): a hundred numbers in a list, added up each times its place.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a growable array and a loop.
  It installs no handler, so a fault in it ends with whatever the system prints.
- [`expert.c`](expert.c): the numbers in an array on the stack and one loop; no handler either.
- [`highlights.c`](highlights.c): `main`, the handler's installation, and the rows of the function table for
  `Naive`'s functions and `main`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`main` starts with `spite_fault_install();`, which on Windows sets the unhandled-exception filter and a vectored
handler (that one only answers a corrupted heap, `0xC0000374`) and calls `spite_fault_thread`, which keeps 16 KB
of the stack back so an overflow can still be reported. Nothing else runs until a fault. Each row
`{(const void*)&Naive_Naive, "...naive.spite\tNaive", "Naive", 3}` is one function's address, its file and class,
its name and the line it starts on: what a fault report names. Taking every function's address keeps an
out-of-line copy of a small `static inline` function such as `Naive___release` that the C compiler would
otherwise inline everywhere and drop. This program calls no foreign library of its own (the `Console` writes with
`fwrite`), so the C has no store of the last foreign call: `SPITE_FAULT_FOREIGN` is not defined and the report's
`foreign=` part is left out. `naive.c` and `expert.c` have nothing in its place.

## Why there is no time

The handler runs only when the program faults, which none of the three does, so all three run the same work at
the same speed. What to compare is the size it adds. The table below was measured on Windows with `clang -O2`
and `ls -l`; the second row is the same C as the first with only the line `spite_fault_install();` deleted from
`main`, so the C compiler drops the handler, the stack walk and the function table that nothing else reaches
(Windows rounds an executable's sections to 512 bytes, so read the sizes to that):

| Program | Function table rows | Executable bytes |
|---|---|---|
| `naive/`, `--optimized` C | 99 | 166 400 |
| the same C without `spite_fault_install();` | 0 | 153 600 |
| `naive.c` | none | 138 752 |
| `expert.c` | none | 139 264 |

The handler, its table of 99 functions and the crash reports' stack walk cost this program 12.8 KB, about half
of what it keeps over `naive.c`; the rest is the program's own functions and the reports of its checks.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | not timed | 169 472 |
| naive C: `naive.c`, `clang -O2` | not timed | 138 752 |
| expert C: `expert.c`, `clang -O2` | not timed | 139 264 |

Measured 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured untimed -->
<!-- /timings -->
