# The compiler places memory

`heap.allocate(count * 8)` reads as a request to the heap, and `heap.free(squares)` as giving it back, on every
call. The compiler proves the buffer is freed in the block that made it and only written and read through, so it
gets a 256-byte slot in the function's own frame, and the heap is asked only when the size at run time is larger.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#the-compiler-places-memory).
- The proof: [Buffers in the frame](../../docs/proofs.md#a-buffer-freed-in-its-block-lives-in-the-frame).

## The four forms

- [`naive/`](naive/): 1 000 000 calls of `sum_of_squares(32, offset)`, each allocating a buffer of 32 `Long`s,
  writing the squares into it through a `TypedMemory<Long>`, adding them up and freeing it.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: `malloc` and `free` per call,
  and a function for each read and write.
- [`expert.c`](expert.c): the same work tuned by hand: the squares in an array on the stack, written and added up in
  one pass.
- [`highlights.c`](highlights.c): what the compiler writes for `sum_of_squares`.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

The first line of `Naive_sum_of_squares` is `int64_t spite_temp_1[32];`, the frame slot, and the allocation is
`squares_ = spite_temp_2 <= 256 ? (int64_t)(intptr_t)spite_temp_1 : Memory_Heap_allocate(...)`: the heap is called
only for a size larger than the slot, and the free at the end is skipped when the address is the slot's. Under
`--debug-memory` the whole program makes 6 allocations, none of them in the loop, where `naive.c` calls `malloc`
1 000 000 times. `expert.c` has the same array on its stack with no test of the size; what still separates
`highlights.c` from it is the overflow check on each `+` and `*`, not the memory.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 31 150 | 175 104 |
| naive C: `naive.c`, `clang -O2` | 68 521 | 139 264 |
| expert C: `expert.c`, `clang -O2` | 17 859 | 139 264 |

Spite takes 0.45 times naive C's time and 1.74 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=31150 naive=68521 expert=17859 -->
<!-- /timings -->
