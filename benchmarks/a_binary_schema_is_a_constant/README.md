# A binary schema is a constant

`writer.schema()` and `reader.schema()` are the FNV-1a hash of the walk of the class's attributes as text, and
that walk is known while compiling. The compiler writes the text, hashes it and gives the C a macro that is the
number, so asking costs nothing at run time and allocates nothing.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#a-binary-schema-is-a-constant).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); the classes a writer or reader is made
  for are fixed in the build, so their walk, and its hash, are facts of the build.

## The four forms

- [`naive/`](naive/): 200 000 message headers, nine in ten the writer's schema and the rest an older number, and
  10 rounds counting the ones that match the reader's schema; then one reading written and read back, and the
  schema printed, so all three programs show that they agree on the number.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: `schema()` a function of the
  writer and of the reader that walks a table describing `Reading` into a growing text and hashes it, on every call
  (2 200 000 of them).
- [`expert.c`](expert.c): the schema written as a constant, the headers in one plain array, each round one
  branchless count.
- [`highlights.c`](highlights.c): the two macros the compiler writes for the schema, and the two functions that ask
  for it.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`BinaryWriter__Reading_schema` and `BinaryReader__Reading_schema` are macros that are the number
`4647632506827894408`, with the hashed text `Reading{sensor:Integer;level:Float;label:String}` in a comment
beside them. `Naive_is_current` is one comparison with that constant, and `Naive_received_headers` stores it
without calling anything. `naive.c` builds the 48-character text and hashes it on every call; `expert.c` writes the
constant where the macros put it. The Spite program's whole run takes about 2 ms on this machine, under the 10 ms the
other cases aim for, since making it longer would take `naive.c` past a second.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 2 229 | 197 632 |
| naive C: `naive.c`, `clang -O2` | 459 504 | 140 288 |
| expert C: `expert.c`, `clang -O2` | 607 | 140 800 |

Spite takes 0.00 times naive C's time and 3.67 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured spite=2229 naive=459504 expert=607 -->
<!-- /timings -->
