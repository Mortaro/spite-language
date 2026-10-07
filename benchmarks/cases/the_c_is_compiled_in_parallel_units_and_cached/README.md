# The C is compiled in parallel units, and cached

The C of a program bigger than 1.5 MB is split into a header and up to 64 translation units, compiled as many at
once as the machine has processors and linked, and each unit's object is kept in `.spite/objects` under the hash
of what it was compiled from. A build that changed nothing only links, and a build that changed one function's
body compiles only the unit that function is in.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#the-c-is-compiled-in-parallel-units-and-cached),
  with the rules in [docs/compiler.md](../../../docs/compiler.md#translation-units-the-c-compiled-in-parallel-and-cached).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); a unit's object is reused only when
  the compiler's command, the header and the unit hash the same, so it is what compiling them again would make.

## The four forms

- [`naive/`](naive/): five hundred items, each with a price and a count, and the sum of their worth.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per item, a growable
  array of pointers, one file compiled whole by every build.
- [`expert.c`](expert.c): the prices and counts as two arrays on the stack and one loop over them.
- [`generated.c`](generated.c): `Item.worth` and the `sum_worth` template, the functions a body edit below changes
  and the code that calls it.

## What to look at in generated.c

Nothing in a function's C says which unit it is in: each function goes to the unit a hash of its name picks, so
editing `Item_worth` changes that unit alone. The production build of this program does not split at all: its
`--optimized` C is 96 884 bytes, under the 1.5 MB at which units start, so it is one file, compiled as before and
not cached. That is the rule, not a fallback; a program of this size gains nothing from units. `naive.c` and
`expert.c` are one file each too.

## Why there is no time

The units change how long a build takes, never how the program runs: the three programs here run the same work,
and their times say nothing about building. What to compare is the build. Since the production C of this program
is too small to split, the measurement uses its `--development` build, whose C keeps every function for
inspection (2 976 169 bytes, so two units of about 1.0 and 1.5 MB and a 0.75 MB header on this machine of 32
processors). A copy of `naive/` in a scratch folder was built with `spite <folder> --development --build
--executable-path=...`, timed by the shell around the whole command; the edit changed `return price * count` in
`item.spite` to `return count * price`, which changes one function's body and no type, function or constant's
place. The last row is the same C written with `--check --c-source --development` and built as one file with
`clang -O0 -march=native`, as a default build compiles, the best of three. One run each, on Windows with clang, on
a machine other sessions were building on:

| Build | Whole command | After the 2.5 s of writing the C |
|---|---|---|
| units, cache empty (cold) | 3.9 s | 1.4 s: both units compiled, then linked |
| units, nothing changed (warm) | 3.2 s | 0.7 s: both objects found, only the link |
| units, after the edit | 3.9 s | 1.4 s: one unit compiled again, the other found |
| units, the edit built again | 3.2 s | 0.7 s |
| the same C as one file | | 1.3 s |

At two units on a machine compiling other work, the cold build is no faster than one file, and the edit, which recompiled the
1.0 MB unit and reused the 1.5 MB one, took as long as the cold build within the noise of one run; what the cache
does pay here is the build that changed nothing, half the C compiler's time. The gain grows with the program: the
compiler's own 7 MB of C is eight units.

## Timings

<!-- timings -->
<!-- /timings -->
