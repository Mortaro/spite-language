# Reads in a row overlap

Two or more `var name = file.read()` written one after another, none naming a variable an earlier one declared,
are started together: every read but the last becomes a `Concurrent`, and all of them are joined before the next
statement. The program waits for the slowest read instead of each in turn, and sees the same values.

- The optimisation: [docs/optimizations.md](../../docs/optimizations.md#reads-in-a-row-overlap).
- The proof: none of its own; the shape is a rule of the syntax
  ([concurrency.md](../../docs/concurrency.md#reads-in-a-row-overlap)), and the overlapped read is a state
  machine under [Which calls suspend a `Concurrent`](../../docs/proofs.md#which-calls-suspend-a-concurrent).

## The four forms

- [`naive/`](naive/): two files of about 90 kB written once, then 40 rounds that each read both in a row and add up
  their lengths, then both files removed.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: each read opens the file,
  measures it, reads it into a new buffer and closes it, one after the other.
- [`expert.c`](expert.c): the same work tuned by hand: each round reads the first file on a thread of its own while
  the program's thread reads the second, with the system's own calls, into a buffer each made once.
- [`highlights.c`](highlights.c): the two reads, the state machine the first one runs as, the step that hands its
  `fread` to a helper thread, and the plain `File_read_into` the second read reaches.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an
  `--optimized` build for Windows writes it.

## What to look at in highlights.c

`Naive_read_both___held_0_1` makes the first read a `Concurrent__Nullable_String___make` of `File_read`, calls
`File_read(second_file_)` itself, and joins the first with `Concurrent__Nullable_String__join(first_)` before the
`crash` lines, which then read both. `File_read___step` is `read`'s state machine: it opens, seeks and allocates on
the program's thread and waits only at `File_read_into___step`, which starts the `fread` on a helper thread
(`Scheduler_offload_start`) and answers whether it is over. The second, plain read reaches `File_read_into`, which
offloads its `fread` too while a `Concurrent` is alive (`Scheduler_waits_here`), so the two `fread`s run at once and
the event loop waits for both.

Only the `fread`s overlap: `fopen`, the seeks and `fclose` of both reads run on the program's thread one after the
other. And with files the system has cached, as here, the overlap costs more than it saves (one run of the `-O2` builds
`scripts/cases/check.sh` makes took 21 ms for the Spite against 14 ms for `naive.c`): a `Concurrent`, a frame per wait and a helper thread per `fread`, against reads that take
a fraction of a millisecond each. The page's promise is for reads that wait (a cold disk, a network share, a
socket), where the slowest read is what the program waits for. `naive.c` reads one file after the other;
`expert.c` overlaps the whole of both reads, open and close included, with one thread per round.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 15 085 | 268 288 |
| naive C: `naive.c`, `clang -O2` | 12 745 | 158 208 |
| expert C: `expert.c`, `clang -O2` | 11 188 | 148 992 |

Spite takes 1.18 times naive C's time and 1.35 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=15085 naive=12745 expert=11188 -->
<!-- /timings -->
