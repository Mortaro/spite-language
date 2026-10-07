# Tree shaking the generated C

Once the program's C is written, the compiler keeps only what `main` can reach: every function nothing calls, from
the program, the library or the compiler's own prelude, is dropped with its prototype, and every class nothing
reachable uses is dropped with its struct, its reference counting, its reflection object and its text literals.
A program that never makes a socket, a process or a thread pool carries none of their C.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#tree-shaking-the-generated-c).
- The proof: [Tree shaking: what `main` can reach](../../docs/proofs.md#tree-shaking-what-main-can-reach).

## The four forms

- [`naive/`](naive/): three names in a list, joined with commas and printed with the longest one's length.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: a growable array of texts, a
  join into a new text and a walk for the longest. A C programmer writes only what the program uses, which is what
  tree shaking leaves of the Spite program.
- [`expert.c`](expert.c): the names and their lengths in arrays on the stack, joined once into a buffer on the stack.
- [`highlights.c`](highlights.c): `main` and the two functions of `Naive`, which with what they call is all the
  program is.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

What tree shaking does is what is not there, so `highlights.c` shows the roots it starts from: `main` makes the
`Launcher`, which makes `Naive`, then destroys the singletons and closes the one native library a kept function
calls into. Everything the program keeps is reached from those lines. The whole C has 12 structs: `SpiteString`,
`Launcher`, `Build`, `Console`, `DynamicLibrary`, `Memory_Arena`, `Memory_Heap`, `Naive`, `List_String` and
`List_Console_Printable` with their two `TypedMemory`. `naive.c` and `expert.c` hold only what the program does to
begin with. A few names of dropped classes are left behind in the full C, which cost the executable nothing but
are not what the section says: `typedef struct Dictionary Dictionary;` and `typedef struct List List;` with no
struct behind them, `typedef void* WebSocket_Message;`, and `SPITE_ALLOCATOR_` macros for `List_Socket`,
`List_SchedulerLoop` and `List_ThreadPoolJob`.

There is no time: shaking changes what the executable holds, not what it runs, so the three programs do the same
work at the same speed. What to compare is the size of the program. Every build of this program shakes except an
inspectable one, so the comparison is with `--development`, which keeps every function and class for the REPL and
live reload (and adds the machinery for them, so it is an upper bound on what shaking removes rather than the
exact amount). Both were written with `--check --c-source --optimized` (and `--development` for the second), the
executables built with `clang -O2` on Windows, and measured with `wc -l -c`, `grep -c` and `ls -l`:

| Program | Lines of C | Bytes of C | Functions defined | Structs | Executable bytes |
|---|---|---|---|---|---|
| this program, shaken | 1 920 | 101 768 | 188 | 12 | 170 496 |
| the same with `--development`, nothing shaken | 43 383 | 2 952 966 | 4 552 | 138 | 1 244 160 |
| `naive.c` | 72 | 2 217 | 6 | 1 | 138 752 |

The 32 kB between the shaken Spite program and `naive.c` are what this Spite program keeps that the C one does
without, such as the fault handler every Spite program has and the reports of its checks.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | not timed | 172 032 |
| naive C: `naive.c`, `clang -O2` | not timed | 138 752 |
| expert C: `expert.c`, `clang -O2` | not timed | 138 240 |

Measured 2026-10-07, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5; shared with another session building and benchmarking the compiler at the same time.
<!-- measured untimed -->
<!-- /timings -->
