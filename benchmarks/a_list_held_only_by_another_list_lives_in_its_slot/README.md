# A list held only by another list lives in its slot

When nothing in a program names an inner list of a `List<List<T>>` except through its slot, each inner list's
object is kept inside the outer list's block: a read of `buckets[customer]` is the slot's address, with no pointer
to follow, and the lists sit side by side.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-list-held-only-by-another-list-lives-in-its-slot).
- The proof: [A list held only in a slot of another list](../../docs/proofs.md#a-list-held-only-in-a-slot-of-another-list).

## The four forms

- [`naive/`](naive/): 400 000 orders, each with a customer among 65 536 and an amount; ten rounds that group the
  orders into one list per customer, made afresh each round, then ask 400 000 questions, each the sum of one
  customer's amounts.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: an order `malloc`ed each, the
  buckets a growable array of pointers to bucket lists, each bucket `malloc`ed on its own and freed when the next
  round groups again.
- [`expert.c`](expert.c): the same work tuned by hand: the orders as two columns, and each round a counting sort of
  the amounts into one array with a start per customer, so a question reads two neighbouring starts and a run of
  amounts.
- [`highlights.c`](highlights.c): the memory functions of `List<List<Order>>`'s slots, and the grouping and the
  questions.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`TypedMemory__List_Order_value_bytes` is the size of a whole `List_Order`, not of a pointer to one, so the block of
`buckets` holds the lists themselves. `TypedMemory__List_Order_write_value` copies the list `group` makes into its
slot and leaves the made object empty to be let go; `TypedMemory__List_Order_read_value` answers the slot's address,
and `TypedMemory__List_Order_release_value` drops the list where it lies. In `Naive_ask`, `bucket` is the address of
`buckets`' slot, so each question reads the slot and then the bucket's items; `naive.c` reads a pointer first.
`expert.c` reads no bucket at all: its amounts are already in one array, grouped.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 142 935 | 208 896 |
| naive C: `naive.c`, `clang -O2` | 289 137 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 37 739 | 139 776 |

Spite takes 0.49 times naive C's time and 3.79 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=142935 naive=289137 expert=37739 -->
<!-- /timings -->
