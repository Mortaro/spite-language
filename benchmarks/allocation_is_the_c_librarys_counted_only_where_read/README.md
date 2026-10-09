# Allocation is the C library's, counted only where read

Every Spite object is made with `SPITE_MALLOC` and let go with `SPITE_FREE`, which reads as though Spite had an
allocator of its own. In a program that never reads `Memory.Heap.live_allocations()` (nor `live_bytes()`), the two
are the C library's `malloc` and `free` with nothing beside them; only a program that still reads the count after
tree shaking gets the counting versions.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#allocation-is-the-c-librarys-counted-only-where-read).
- The proof: none of its own; it stands on [Tree shaking](../../docs/proofs.md#tree-shaking-what-main-can-reach),
  which decides whether `live_allocations` is still in the program.

## The four forms

- [`naive/`](naive/): 2 000 rounds, each making a chain of 1 000 `Node`s (each holding the one before), adding up
  their values and letting the chain go: 2 000 000 objects made and freed one by one. The program never asks how
  many allocations are live.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per class, `malloc`
  per node, and each chain freed node by node.
- [`expert.c`](expert.c): the same work tuned by hand: one array of nodes kept across the rounds, each chain linked
  through it, nothing allocated per node.
- [`highlights.c`](highlights.c): the two macros every allocation goes through, and what the compiler writes to make
  and free a `Node`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`#define SPITE_MALLOC(size) malloc(size)` and `#define SPITE_FREE(pointer) free(pointer)`: the first line of each
pair is for a library a `--hot-reload` program loads, which allocates through the program it is loaded into, and
the second is this program's. `Node___allocate` calls `SPITE_MALLOC(sizeof(Node))` and `Node___free` ends with
`SPITE_FREE(self)`, so each node costs what it costs in `naive.c`: one `malloc` and one `free`, with no counter, table
or list of blocks beside them. What still separates `naive/` from `naive.c` is the reference count each node
carries (counted up as the next node takes it, down as the chain goes), and `Node___free` letting the chain go by
calling itself through `Node___release(self->next_)` where `naive.c` walks it in a loop. `expert.c` makes no
allocation per node at all.

The same program with one line added at the end, `var live = heap.live_allocations()` (and `live` printed), has
in place of the two `#define`s a counted `spite_counted_realloc` and `spite_counted_free` (the comment above them
says `The program reads Memory.Heap.live_allocations(), so allocations are counted.`). Built the same way
(`--optimized`) and run twice each on this machine: 102 and 101 ms against 83 and 81 ms for `naive/`, so the
count that this program does not read would cost it about a quarter of its time.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 76 960 | 194 048 |
| naive C: `naive.c`, `clang -O2` | 58 695 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 2 769 | 139 264 |

Spite takes 1.31 times naive C's time and 27.79 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=76960 naive=58695 expert=2769 -->
<!-- /timings -->
