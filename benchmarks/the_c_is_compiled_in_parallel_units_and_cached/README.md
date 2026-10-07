# The C is compiled in parallel units, and cached

The C of a program bigger than 1.5 MB is split into a header and up to 64 translation units, compiled as many at
once as the machine has processors and linked, and each unit's object is kept in `.spite/objects` under the hash
of what it was compiled from. A build that changed nothing only links, and a build that changed one function's
body compiles only the unit that function is in.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#the-c-is-compiled-in-parallel-units-and-cached),
  with the rules in [docs/compiler.md](../../docs/compiler.md#translation-units-the-c-compiled-in-parallel-and-cached).
- The proof: none of its own in [docs/proofs.md](../../docs/proofs.md); a unit's object is reused only when
  the compiler's command, the header and the unit hash the same, so it is what compiling them again would make.

## The four forms

- [`naive/`](naive/): five hundred items, each with a price and a count, and the sum of their worth.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per item, a growable
  array of pointers, one file compiled whole by every build.
- [`expert.c`](expert.c): the prices and counts as two arrays on the stack and one loop over them.
- [`highlights.c`](highlights.c): `Item.worth` and the `sum_worth` template, the functions a body edit below changes
  and the code that calls it.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

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

## Compile time at scale

`bash scripts/build_times.sh [compiler] [program ...]` times building an executable (the whole `spite` command, the
Spite compile to C included) from one C file and from translation units: cold (the object cache emptied), warm
(built again, nothing changed) and after editing one function's body. Wall-clock milliseconds on Mortaro's machine
(32 logical processors, clang 19.1.5, other sessions compiling at the same time), the compiler itself built
`--optimized`. The large game's example is its biggest (14 MB of C), built from a copy, with its edit in a generic
column class; the synthetic program is 209 206 lines in 401 files (400 classes of 30 small functions), written by
the script:

| program | C | build | one file | cold | warm | one edit |
|---|---|---|---|---|---|---|
| the compiler (`bootstrap`) | 7.2 MB | default (`-O0`) | 18 989 | 22 467 | 12 799 | 13 298 |
| the compiler (`bootstrap`) | 7.2 MB | `--optimized` | 37 673 | 14 495 | 11 392 | 13 348 |
| a large game's example | 14.1 MB | default (`-O0`), one file only | 26 071 | 23 546 | 23 971 | 27 202 |
| a large game's example | 14.1 MB | `--optimized` | 80 127 | 45 670 | 30 696 | 67 173 |
| synthetic, 209 206 lines | 20.5 MB | default (`-O0`) | 31 577 | 32 067 | 17 417 | 19 939 |
| synthetic, 209 206 lines | 20.5 MB | `--optimized` | 181 193 | 49 965 | 34 260 | 37 330 |

The two default rows of the compiler and the synthetic program were measured again on 2026-09-30, when default
builds started splitting by the same size rule, with the machine busier than for the other rows (the compiler's
`--optimized` one-file build took 140.9 s in that run). Split at `-O0`, a cold build costs about what one file
does (22.5 s against 19.0 for the compiler, 32.1 against 31.6 for the synthetic program), since every unit reads
the whole header and `-O0` spends its time reading; a warm build or a one-function edit then compiles one unit or
none, 12.8 and 13.3 s against 19.0 for the compiler, 17.4 and 19.9 s against 31.6 for the synthetic program. The
game's default row is from before, built from one file whatever the number (its spread is the machine's noise). An
`--optimized` build is split: the compiler cold in 14.5 s instead of 37.7 (forced to 4, 8, 16 and 32 units: 23-26,
13-22, 23-26 and 27-32 s, so eight is the size rule's choice for it), and a warm or one-edit build is then the Spite
compile plus ThinLTO's link, which optimises the whole program again every time. The game's edit is slower than
its warm build because the edited class is generic, and every instantiation of it changed.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | not timed | 169 984 |
| naive C: `naive.c`, `clang -O2` | not timed | 138 752 |
| expert C: `expert.c`, `clang -O2` | not timed | 138 752 |

Measured 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured untimed -->
<!-- /timings -->
