# Defaults the constructor replaces are never made

`var owner = Owner(0)` in `Item`, with a constructor whose second line is `owner = new_owner`, reads as an `Owner`
made for every item and thrown away at once. The compiler sees the constructor replace the attribute before
anything can read it, and the default has no effect but its memory, so the item is made with `owner` empty and the
constructor's line fills it: the default `Owner` is never made.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#defaults-the-constructor-replaces-are-never-made).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); the attribute is set in the opening run
  of the constructor's lines, before any line that could read it, and the default's own constructor only copies
  its parameter, so leaving it out changes nothing but the allocation.

## The four forms

- [`naive/`](naive/): 10 rounds, each making 50 000 items with an owner each into a `List<Item>`, and adding up
  their prices and their owners' ages.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per class, `malloc`
  per object, a growable array of pointers per list, and each item made with its default `Owner(0)`, which its
  constructor then frees and replaces.
- [`expert.c`](expert.c): the same work tuned by hand: no default, and the items as two columns of plain values
  (each item has its own owner, so the age sits beside the price).
- [`generated.c`](generated.c): what the compiler writes to set up a constructed `Item` and its constructor.

## What to look at in generated.c

`Item___init_constructed` sets `self->owner_ = 0;` where an `Item` made from its defaults would make an `Owner`, and
`Item___allocate_constructed`, which every `Item(...)` goes through, calls it. `Item_Item` then stores the owner
it is given; the `Owner___release(self->owner_)` before the store releases nothing, since the slot is empty. Under
`--debug-memory` the program makes 1 000 026 allocations, one per item and one per owner it is given, and none for
the 500 000 defaults.

`naive.c` makes each default and frees it two lines later, but clang at `-O2` sees that `malloc`, its one store
and its `free` together and removes them (`clang -O2 -S` of `naive.c` has two calls to `malloc` per item left, the
owner's and the item's), so in time `naive.c` pays nothing for the default either. What it does pay for, and
`naive/` does not, is the list of owners `map_owners` makes (`naive/` runs the chain as one loop) and a `malloc` per
item where the compiler takes items from a pool. `expert.c` allocates nothing per item.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 29 679 | 178 688 |
| naive C: `naive.c`, `clang -O2` | 56 403 | 139 776 |
| expert C: `expert.c`, `clang -O2` | 355 | 139 776 |

Spite takes 0.53 times naive C's time and 83.60 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=29679 naive=56403 expert=355 -->
<!-- /timings -->
