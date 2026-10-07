# Crash text out of the binary

A `crash` or `assert` site's condition text is written only into the `<program>.crashes` map beside the
executable, never into the program, and an `--optimized` build also leaves out each site's place, class and
function, so a site in the program is its 8-digit id and the values its report prints. `grep <id>
program.crashes` gives back the rest.

- The optimisation: [docs/optimizations.md](../../../docs/optimizations.md#crash-text-out-of-the-binary).
- The proof: none of its own in [docs/proofs.md](../../../docs/proofs.md); the site's text is known while
  compiling, and the id it is filed under is all a report needs to find it.

## The four forms

- [`naive/`](naive/): fifty prices, a sum of six picked ones with `crash prices[pick]` before each read, and a
  `price_at` that answers nothing past the end (`assert place < prices.count()`), then `crash last` on its answer.
  None of the three sites fails.
- [`naive.c`](naive.c): the same program as a C programmer writes it from the Spite: each check prints its place,
  its condition and its values, so that text is in the executable.
- [`expert.c`](expert.c): the prices and picks in arrays on the stack, each check a comparison that stops with a
  short code.
- [`generated.c`](generated.c): `total_of` and `price_at`, with their checks.

## What to look at in generated.c

The check `crash prices[pick]` is the `if` that calls `spite_failed_1(pick_, prices_, total_, index_)`, a cold
function of its own that prints the report from the site `spite.crash\t2b48e907`; the guard in `price_at`
records `spite_site_2()` in the crash trace. The `--optimized` build defines those sites as

```c
#define spite_site_1130() "spite.crash\t2b48e907"
#define spite_site_1132() "spite.assert\t2b3d99c4\tanswered=null\n"
```

and the default build as

```c
#define spite_site_1130() "spite.crash\t2b48e907\tbenchmarks/cases/crash_text_out_of_the_binary/naive/naive.spite:22\tNaive\ttotal_of"
#define spite_site_1132() "spite.assert\t2b3d99c4\tbenchmarks/cases/crash_text_out_of_the_binary/naive/naive.spite:30\tNaive\tprice_at\tanswered=null\n"
```

Neither has the condition: `place < prices.count()` and `prices [pick]` are only in `naive.crashes`, on the lines
`2b3d99c4 ... 30 5 Naive price_at assert-predicate place < prices.count() place:Integer` and
`2b48e907 ... 22 9 Naive total_of crash prices [pick] prices[pick]:Integer`. `naive.c` carries its condition and
place as text, and `expert.c` a number. The overflow checks keep their place and expression in every build
(`"total + prices[pick]"` and `spite_site_1()` above): only `crash` and `assert` sites are filed away.

## Why there is no time

No site fails, so no report is printed in any of the three programs, and the text only changes what the
executable holds. What to compare is that text. Both builds' C were written with `--check --c-source` (with and
without `--optimized`) and each built with `clang -O2` on Windows; the `.crashes` map came from an `--optimized
--build`. The 252 bytes are the difference `diff` shows between the two C files, which differ in nothing
else; Windows rounds an executable's sections to 512 bytes, so the executables differ by one step:

| Build | Place, class and function text of the 3 sites | Condition text | Executable bytes |
|---|---|---|---|
| `--optimized` | none | none | 172 544 |
| default | 252 bytes | none | 173 056 |
| `naive.c` | in its two messages, about 150 bytes with the conditions | both conditions | 140 288 |

The `.crashes` map is 42 004 bytes and 393 lines for this program, most of them the library's sites, and it lists
each of this program's three sites (the `assert` and the `crash prices[pick]` twice each, with the same text).

## Timings

<!-- timings -->
<!-- /timings -->
