# REPL, live reload and debug machinery only in those builds

Everything that exists to look inside a running program (the REPL's loop and socket thread, the reflection hooks,
the function pointers and forwarders a live reload swaps, the check point at the end of every loop pass, the
allocation table that names leaks) is compiled only into the build that asks for it. An ordinary or `--optimized`
build carries none of it, so its C is the same whether or not the program would ever be inspected.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#repl-live-reload-and-debug-machinery-only-in-those-builds).
- The proof: none of its own; what an ordinary build leaves out it leaves out by
  [Tree shaking: what `main` can reach](../../../docs/proofs.md#tree-shaking-what-main-can-reach), and the
  inspecting itself is written only where a flag asks for it.

## The four forms

- [`naive/`](naive/): a thousand monsters in a list and ten rounds of adding up their health.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a struct per monster, a
  growable array of pointers, a function per Spite function. A C program has no REPL or live reload unless its
  author writes one, which is what a production Spite build carries of them.
- [`expert.c`](expert.c): the health as one array of numbers on the stack, added up ten times.
- [`generated.c`](generated.c): `Naive` and `total_health` as the `--optimized` build writes them.

## What to look at in generated.c

Both loops end at their `index_ = (index_ + 1);` and `round_ = (round_ + 1);`: nothing follows them. In a
`--repl-port` or `--hot-reload` build of the same program, each pass of both loops ends with the check point,

```c
if (__atomic_load_n(&spite_check_point_wanted, __ATOMIC_RELAXED) != 0) { ... Scheduler_check_point(spite_check_point); ... }
```

and in a `--hot-reload` build `Naive_total_health` is a forwarder,
`return __atomic_load_n(&Naive_total_health___slot, __ATOMIC_ACQUIRE)(self, monsters_);`, in front of the
`Naive_total_health___hot` that does the work, and `Naive___fields(self)->console_` reads the attribute through
the layout a reload may change. Neither inspectable build is tree-shaken, and both pass `monsters` counted
(`List_Monster___retain(monsters_)`) where the production build calls `Naive_total_health___held_0`, which takes
it uncounted. `naive.c` and `expert.c` have none of it, like the production build.

## Why there is no time

The machinery is not in the production build at all, so there is nothing to time there: its C is byte for byte
the same as an ordinary build's, and the inspectable builds are slower on purpose and are never measured. What to
compare is what each build holds. Every build below was written from `naive/` with `--check --c-source` and the
flag in the first column (none for the ordinary build), never run, then built with `clang -O2` on Windows. Lines
and bytes are `wc -l -c`; functions are the lines that start a function definition (a return type, a name and
`) {` at the end of the line); structs are lines starting `struct X {` or `typedef struct X {`; check points are
the lines holding `check_point`:

| Build | Lines of C | Bytes of C | Functions defined | Structs | Check point lines | Executable bytes |
|---|---|---|---|---|---|---|
| ordinary, and `--optimized` (the same C) | 1 911 | 97 884 | 146 | 23 | 0 | 168 960 |
| `--debug-memory` | 2 542 | 153 092 | 189 | 24 | 0 | 193 536 |
| `--repl-port=4000` | 48 852 | 3 288 240 | 2 884 | 180 | 53 | 1 400 832 |
| `--hot-reload` | 59 956 | 4 680 209 | 4 965 | 164 | 82 | 1 611 776 |

`--debug-memory` adds the allocation table and the class-name table and nothing else (643 more lines). The two
inspectable builds keep every function of the library and the compiler's helpers, since the prompt or a reload may
call any of them, which is most of their size; a `--repl-port` build carries the allocation table too (150 lines
of its C name `AllocationTable`). Building the `--repl-port` C took 8.3 s and the `--hot-reload` C 7.3 s, against 0.5 s for
the production C.

## Timings

<!-- timings -->
<!-- /timings -->
