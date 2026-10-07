# The compiler places memory

`heap.allocate(count * 8)` reads as a request to the heap, and `heap.free(squares)` as giving it back, on every
call. The compiler proves the buffer is freed in the block that made it and only written and read through, so it
gets a 256-byte slot in the function's own frame, and the heap is asked only when the size at run time is larger.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#the-compiler-places-memory).
- The proof: [Buffers in the frame](../../../docs/proofs.md#a-buffer-freed-in-its-block-lives-in-the-frame).

## The four forms

- [`naive/`](naive/): 1 000 000 calls of `sum_of_squares(32, offset)`, each allocating a buffer of 32 `Long`s,
  writing the squares into it through a `TypedMemory<Long>`, adding them up and freeing it.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: `malloc` and `free` per call,
  and a function for each read and write.
- [`expert.c`](expert.c): the same work tuned by hand: the squares in an array on the stack, written and added up in
  one pass.
- [`generated.c`](generated.c): what the compiler writes for `sum_of_squares`.

## What to look at in generated.c

The first line of `Naive_sum_of_squares` is `int64_t spite_temp_1[32];`, the frame slot, and the allocation is
`squares_ = spite_temp_2 <= 256 ? (int64_t)(intptr_t)spite_temp_1 : Memory_Heap_allocate(...)`: the heap is called
only for a size larger than the slot, and the free at the end is skipped when the address is the slot's. Under
`--debug-memory` the whole program makes 6 allocations, none of them in the loop, where `naive.c` calls `malloc`
1 000 000 times. `expert.c` has the same array on its stack with no test of the size; what still separates
`generated.c` from it is the overflow check on each `+` and `*`, not the memory.

## Timings

<!-- timings -->
<!-- /timings -->
