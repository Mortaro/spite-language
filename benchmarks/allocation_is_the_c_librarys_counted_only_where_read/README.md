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
the second is this program's, with no counter, table or list of blocks beside them. A `Node` does not even go that
far: it is made and let go only on the program's own thread, so it has a pool of its own
([Objects of one class sit together](../../docs/optimizations.md#objects-of-one-class-sit-together)).
`Node___allocate` takes its node from `Node___pool_take()` and `Node___free` ends with `Node___pool_give(self)`, so
after the first round every node is one an earlier chain gave back, and no node costs a `malloc` or a `free`. What
still separates `naive/` from `expert.c` is the reference count each node carries (counted up as the next node takes
it, down as the chain goes), and `Node___free` letting the chain go by calling itself through
`Node___release(self->next_)` where `expert.c` lets nothing go at all.

The same program with one line added at the end, `var live = heap.live_allocations()` (and `live` printed), has
in place of the two `#define`s a counted `spite_counted_realloc` and `spite_counted_free` (the comment above them
says `The program reads Memory.Heap.live_allocations(), so allocations are counted.`), and no pool, since what it
counts are the C library's blocks. Built the same way (`--optimized`) and run twice each on this machine, before
nodes had a pool: 102 and 101 ms against 83 and 81 ms for `naive/`, so the count alone cost about a quarter of the
time, and it now costs the pool as well.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 16 253 | 198 144 |
| naive C: `naive.c`, `clang -O2` | 62 431 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 3 009 | 139 264 |

Spite takes 0.26 times naive C's time and 5.40 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with other sessions building and benchmarking the compiler at the same time.
<!-- measured spite=16253 naive=62431 expert=3009 -->
<!-- /timings -->
