# Singletons that hold nothing are static objects

A singleton with no attributes and no `drop()` (`Memory.Heap`, the `TypedMemory<T>` a list stores its items
through, `Build`) is one static object in a production build: never allocated, never counted, never freed. Every
list a program makes fetches two of them, and the fetch is the address of that object.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#singletons-that-hold-nothing-are-static-objects).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); a class with no attributes and no `drop()`
  has nothing to make, keep or tear down, which the compiler reads from its declaration.

## The four forms

- [`naive/`](naive/): a list of a thousand squares and a list of a hundred names, joined, printing the count, the
  largest square and the joined text's length.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a growable array of numbers, one
  of texts, each name made with `malloc`, and a join into a new text. C has no object for the heap or for how a list
  stores its items, so there is nothing of them to make: `malloc` and the element type are that already, which is
  what the optimisation brings the Spite program to.
- [`expert.c`](expert.c): the squares and the joined names in static buffers of their final size, and nothing
  allocated.
- [`generated.c`](generated.c): the three fetches of singletons that hold nothing, the defaults of a `List<Long>`
  that fetch two of them, and the heap's release.

## What to look at in generated.c

`spite_singleton_Memory_Heap`, `spite_singleton_TypedMemory__Long` and `spite_singleton_Build` each answer the
address of a `static` object whose header is written in the C (`{ { 1, 93 } }`: a count and the class), with no
first-use test, no lock and no `malloc`. `List_Long___init` stores both of the list's into its attributes, and
`Memory_Heap___release` is `(void)self`: a release that does nothing, which the list's own release calls. The whole C
has five such objects: `Memory_Heap`, `Build` and the `TypedMemory` of `Long`, `String` and `Console.Printable`.

## Why there is no time

The optimisation takes out a handful of allocations made once per program, which no clock can see at this size.
Compare the allocations instead, counted by `--debug-memory` on the same program built as production and as an
inspectable build, where each of these singletons is an ordinary one made at its first use (`spite
benchmarks/cases/singletons_that_hold_nothing_are_static_objects/naive --debug-memory`, then with `--development`
added):

| Build | Allocations | Lines of C |
|---|---|---|
| production (`--debug-memory`) | 10 | 2 157 |
| `--development` (`--debug-memory`) | 14 | 43 415 |

The four allocations more are the singletons that hold nothing which the program reaches while it runs, each made at
its first use in the inspectable build. The lines of C (written with `--check --c-source`, `--optimized` for the
first, counted with `grep -c`) differ for far more than this: an inspectable build keeps every function for the
REPL. `naive.c` and `expert.c` have no such objects to begin with.

## Timings

<!-- timings -->
<!-- /timings -->
