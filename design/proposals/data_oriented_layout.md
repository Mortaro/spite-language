# Data-oriented layout for any program: what the measurements say the compiler should prove

**Status:** a research study with experiments, for the question in
[optimization_research.md](../optimization_research.md#data-oriented-design-for-any-program-mortaro-2026-10-08). Not a
decision, and nothing here is built. Every rule below is proposed by Claude, unconfirmed. Nothing in the compiler
was changed for it.

Mortaro's direction: "one of the points of ECS is to best use the CPU cache instead of RAM memory, a lot of code that
is not meant to be ECS can be compiled into data driven loops and SIMD operations, we need to be able to prove when
its best to have each of those not only for games, general purpose, specially before we start our own backend that
needs to be more aware of cpus."

This page measures, on one machine, when each layout wins (objects behind pointers, an array of structures, a
structure of arrays, hot and cold groups, AoSoA blocks), when vector code pays, and when converting between two
layouts pays; then turns the crossovers into rules a compiler could prove, says how they map onto facts the
compiler already establishes, what an own backend has to know about the CPU, and in what order to build it.

## What was measured

Two kinds of experiment, both committed so they can be run again on another machine:

- **The layout sweep**, [sweep.c](data_oriented_layout/sweep.c) run by
  [sweep.sh](data_oriented_layout/sweep.sh): one record of eight 32-bit fields stored eight ways, walked by seven
  loops (one field of eight, four, all eight, a 50% filter, an update, and one or all fields in a random order), at
  eight sizes from 512 records to 4 194 304, built three ways (scalar, the C compiler's vectoriser for SSE2 as
  `benchmarks/run.sh` builds, and for AVX2), plus the cost of overflow checks in a sum and of converting between
  layouts. Every layout must give every read-only loop's answer, or the sweep stops. Each number is the least of
  three rounds of five samples. Full tables: [results_scalar.md](data_oriented_layout/results_scalar.md),
  [results_sse2.md](data_oriented_layout/results_sse2.md), [results_avx2.md](data_oriented_layout/results_avx2.md).
- **Five whole programs** in `benchmarks/`, none of them a game, each a naive Spite program, `naive.c` written with
  objects, and three hand forms: `expert_aos.c` (records inline), `expert_soa.c` (columns) and `expert.c` (the best
  by hand, data-oriented): [report_over_records](../../benchmarks/report_over_records/),
  [tokens_as_columns](../../benchmarks/tokens_as_columns/),
  [image_filter_over_planes](../../benchmarks/image_filter_over_planes/),
  [spreadsheet_recalculation](../../benchmarks/spreadsheet_recalculation/) and
  [records_sorted_by_one_field](../../benchmarks/records_sorted_by_one_field/). Each takes its size as a setting
  (`--records=N` and so on, an `Environment` setting in the Spite), and
  [cases.sh](data_oriented_layout/cases.sh) times every form at five sizes into
  [cases.md](data_oriented_layout/cases.md), with each C form's phases.

### The machine

| property | value |
|---|---|
| processor | AMD Ryzen 9 5950X (Zen 3), 16 cores, 32 logical processors |
| first level data cache | 32 KB per core, 8-way, 64-byte lines |
| second level cache | 512 KB per core, 8-way |
| third level cache | 32 MB per eight-core complex, 16-way (two complexes, 64 MB in all) |
| memory | 32 GB DDR4 at 2133 MT/s, four modules |
| vector instructions | SSE2 to AVX2 and FMA; no AVX-512 |
| system and C compiler | Windows 11, clang 19.1.5 |

The cache sizes are the system's own report (`GetLogicalProcessorInformationEx`; Windows has no `/proc/cpuinfo`).
The sweep pins itself to one core at high priority. The machine was shared with other sessions building and
benchmarking the compiler (about half of the processor busy), so single numbers move by a few percent and the
largest sizes by more; the tables keep the least of every round for that reason.

What each sweep size is against the caches, for the 32-byte record (an object in Spite's pool is 40 bytes and a
pointer, 48):

| records | array of structures | structure of arrays, one column | where the array of structures sits |
|---|---|---|---|
| 512 | 16 KB | 2 KB | first level |
| 4 096 | 128 KB | 16 KB | second level |
| 16 384 | 512 KB | 64 KB | the second level's edge |
| 65 536 | 2 MB | 256 KB | third level |
| 262 144 | 8 MB | 1 MB | third level |
| 1 048 576 | 32 MB | 4 MB | the third level's edge |
| 2 097 152 | 64 MB | 8 MB | memory |
| 4 194 304 | 128 MB | 16 MB | memory |

## The sweep: nanoseconds per record

The SSE2 build (what `run.sh` builds) unless the table says otherwise; four sizes of the eight, one per level. The
pool row is what Spite writes today for a `List` of a class: pointers to objects side by side in the class's pool.

### Reading one field of eight, in order

| layout | 512 | 16 384 | 262 144 | 4 194 304 |
|---|---|---|---|---|
| objects, one malloc each (naive C) | 0.22 | 0.53 | 0.56 | 3.57 |
| objects in a pool (Spite today) | 0.22 | 0.42 | 0.43 | 1.87 |
| pool, list order shuffled | 0.22 | 0.55 | 0.68 | 6.35 |
| array of structures | 0.20 | 0.26 | 0.29 | 1.36 |
| structure of arrays | 0.13 | 0.12 | 0.12 | 0.13 |
| hot and cold, 4 and 4 | 0.19 | 0.20 | 0.21 | 0.76 |
| AoSoA, 8 lanes | 0.17 | 0.21 | 0.26 | 0.76 |
| AoSoA, 16 lanes | 0.14 | 0.14 | 0.14 | 0.42 |

The same loop in the AVX2 build: structure of arrays 0.04, 0.04, 0.05 and 0.10; array of structures 0.14, 0.31, 0.32
and 2.16; pool 0.36, 0.44, 0.54 and 2.11.

### Reading four fields of eight, in order

| layout | 512 | 16 384 | 262 144 | 4 194 304 |
|---|---|---|---|---|
| objects in a pool (Spite today) | 0.58 | 0.68 | 0.63 | 2.14 |
| array of structures | 0.54 | 0.58 | 0.57 | 1.63 |
| structure of arrays | 0.47 | 0.48 | 0.48 | 0.61 |
| hot and cold, 4 and 4 | 0.44 | 0.44 | 0.44 | 0.93 |
| AoSoA, 8 lanes | 0.32 | 0.35 | 0.39 | 0.91 |
| structure of arrays, AVX2 | 0.13 | 0.13 | 0.19 | 0.58 |
| array of structures, AVX2 | 0.51 | 0.56 | 0.72 | 1.83 |

### Reading all eight fields, in order

| layout | 512 | 16 384 | 262 144 | 4 194 304 |
|---|---|---|---|---|
| objects in a pool (Spite today) | 1.58 | 1.63 | 1.66 | 2.64 |
| pool, list order shuffled | 1.57 | 3.11 | 4.46 | 28.46 |
| array of structures | 1.43 | 1.43 | 1.41 | 1.93 |
| structure of arrays | 0.84 | 0.85 | 0.88 | 1.42 |
| array of structures, scalar build | 1.41 | 1.45 | 1.46 | 1.98 |
| structure of arrays, scalar build | 1.65 | 1.71 | 1.69 | 1.73 |

### A filter that is true for half of the records, in a random pattern

| layout | 512 | 16 384 | 262 144 | 4 194 304 |
|---|---|---|---|---|
| objects in a pool (Spite today) | 0.32 | 0.61 | 3.30 | 3.67 |
| array of structures | 0.27 | 0.51 | 2.70 | 2.96 |
| structure of arrays | 0.25 | 0.52 | 2.73 | 2.73 |
| structure of arrays, AVX2 | 0.08 | 0.13 | 0.11 | 0.29 |
| array of structures, AVX2 | 0.27 | 0.63 | 3.07 | 3.46 |

### Updating one field from another, in order

| layout | 512 | 16 384 | 262 144 | 4 194 304 |
|---|---|---|---|---|
| objects in a pool (Spite today) | 0.35 | 0.60 | 0.59 | 3.82 |
| array of structures | 0.30 | 0.40 | 0.39 | 2.53 |
| structure of arrays | 0.07 | 0.08 | 0.09 | 0.29 |
| hot and cold, 4 and 4 | 0.26 | 0.31 | 0.31 | 1.46 |
| AoSoA, 16 lanes | 0.07 | 0.10 | 0.15 | 0.58 |

### Reading one field of eight, in a random order

| layout | 512 | 16 384 | 262 144 | 4 194 304 |
|---|---|---|---|---|
| objects in a pool (Spite today) | 0.35 | 0.96 | 1.22 | 11.44 |
| array of structures | 0.35 | 0.50 | 0.84 | 6.00 |
| structure of arrays | 0.27 | 0.35 | 0.58 | 1.15 |
| hot and cold, 4 and 4 | 0.34 | 0.48 | 0.80 | 5.21 |
| AoSoA, 16 lanes | 0.93 | 1.23 | 1.96 | 11.96 |

### Reading all eight fields, in a random order

| layout | 512 | 16 384 | 262 144 | 4 194 304 |
|---|---|---|---|---|
| objects in a pool (Spite today) | 2.02 | 4.68 | 7.82 | 58.45 |
| array of structures | 2.10 | 2.78 | 5.50 | 27.64 |
| structure of arrays | 2.52 | 5.11 | 8.81 | 49.63 |
| hot and cold, 4 and 4 | 2.16 | 3.14 | 5.58 | 31.13 |
| AoSoA, 8 lanes | 2.75 | 3.83 | 6.67 | 41.85 |

### What an overflow check costs a sum

One column of 32-bit values summed; Spite checks every `+` today, so this is the price of a check the compiler
could prove unneeded.

| sum | build | 512 | 16 384 | 262 144 | 4 194 304 |
|---|---|---|---|---|---|
| Long sum of Integer values, every + checked (Spite today) | SSE2 | 0.45 | 0.47 | 0.47 | 0.56 |
| Long sum of Integer values, check proven unneeded | SSE2 | 0.13 | 0.12 | 0.12 | 0.16 |
| Long sum of Integer values, every + checked (Spite today) | AVX2 | 0.25 | 0.45 | 0.45 | 0.43 |
| Long sum of Integer values, check proven unneeded | AVX2 | 0.06 | 0.06 | 0.06 | 0.08 |
| Integer sum, every + checked | AVX2 | 0.49 | 0.99 | 0.96 | 0.62 |
| Integer sum, checked once by a bound, halting where the checked loop would | AVX2 | 0.14 | 0.13 | 0.14 | 0.16 |
| Integer sum, checked once by a bound | SSE2 | 0.31 | 0.31 | 0.31 | 0.35 |
| Integer sum, every + checked | SSE2 | 0.24 | 0.24 | 0.24 | 0.26 |

The bound is the sum of the absolute values, kept in 64-bit lanes beside the sum: when it fits an `Integer`, no
prefix of the sum in written order can have left the range, so the answer is the checked loop's; when it does not,
the checked loop runs again from the start and halts at the very step the written program would. On SSE2 it loses
(no 64-bit compare or absolute value in that instruction set), on AVX2 it wins four to seven times.

### What converting costs

| conversion, one pass | 512 | 16 384 | 262 144 | 4 194 304 |
|---|---|---|---|---|
| array of structures to structure of arrays | 1.76 | 2.06 | 3.44 | 4.65 |
| structure of arrays to array of structures | 1.37 | 1.51 | 1.52 | 4.18 |
| pooled objects to one column | 0.19 | 0.68 | 0.82 | 2.21 |

## The five programs

Best microseconds of each whole program at two of the five sizes (8 192 records, where the program's data sits in
the second or third level, and 8 388 608, far past the third), from [cases.md](data_oriented_layout/cases.md); the
case READMEs have each at its own size, timed by `run.sh`.

| case and size | Spite | naive C | inline structs | columns | expert |
|---|---|---|---|---|---|
| report, 8 192 sales | 912 | 956 | 563 | 923 | 129 |
| report, 8 388 608 sales | 1 739 263 | 3 027 077 | 1 149 726 | 789 674 | 102 481 |
| tokens, 8 192 pieces | 600 | 649 | 438 | 473 | 431 |
| tokens, 8 388 608 pieces | 616 994 | 781 554 | 459 058 | 445 096 | 414 856 |
| image, 8 192 pixels | 253 | 347 | 79 | 55 | 50 |
| image, 8 388 608 pixels | 228 629 | 517 592 | 73 735 | 50 157 | 45 110 |
| spreadsheet, 8 192 cells | 1 495 | 1 087 | 504 | 469 | 449 |
| spreadsheet, 8 388 608 cells | 11 060 778 | 10 204 602 | 3 646 334 | 2 121 600 | 1 696 455 |
| sort, 8 192 orders | 961 | 1 208 | 486 | 254 | 280 |
| sort, 8 388 608 orders | 3 064 432 | 3 728 504 | 594 798 | 593 781 | 422 828 |

The phases that decide the layout, from the same runs (microseconds):

| phase | size | inline structs | columns | expert |
|---|---|---|---|---|
| report: 64 filtered passes over five fields of eight | 8 192 | 446 | 711 | (fused) |
| report: 64 filtered passes over five fields of eight | 131 072 | 8 771 | 10 076 | (fused) |
| report: 64 filtered passes over five fields of eight | 2 097 152 | 250 697 | 158 865 | (fused) |
| report: 64 filtered passes over five fields of eight | 8 388 608 | 1 024 475 | 670 159 | (fused) |
| tokens: the passes over the tokens | 8 388 608 | 66 436 | 37 354 | 17 314 |
| spreadsheet: twenty recalculations (two gathered reads a cell) | 8 192 | 388 | 333 | 330 |
| spreadsheet: twenty recalculations | 131 072 | 14 699 | 12 468 | 12 471 |
| spreadsheet: twenty recalculations | 2 097 152 | 773 882 | 281 544 | 221 030 |
| sort: sorting | 8 388 608 | 456 233 | 144 622 | 294 833 (with the gather) |
| sort: the two walks after it | 8 388 608 | 50 802 | 368 785 | 50 066 |
| sort: sorting and walking | 131 072 | 4 796 | 2 456 | 2 661 |
| image: filtering | 8 388 608 | 18 887 (interleaved) | 4 365 (planes) | 2 602 |
| image: measuring | 8 388 608 | 24 918 (interleaved) | 15 391 (planes) | 11 686 |

## The crossovers

1. **A loop that reads a subset of the fields in order: columns always win, and the win grows past the second
   level.** One field of eight: 1.5 times at the first level, 2.4 at the third, 10 in memory against inline structs
   (14 against Spite's pool). There is no size at which inline structs win. Four of eight in the SSE2 build: about even in
   cache (0.48 against 0.57, both bound by arithmetic), 2.7 times in memory; in AVX2, 4 times already in cache.
2. **Reading every field in order, columns still win when the loop is vectorised** (0.84 against 1.43 in cache, 1.42
   against 1.93 in memory), and draw when it is not (scalar: 1.69 against 1.46 in cache, 1.73 against 1.98 in
   memory). A record read whole in order is no reason to keep it whole.
3. **Random reads of the whole record: inline structs win, by 1.8 in memory** (27.6 against 49.6) and 1.2 in the
   first level; columns pay one cache line per field read, inline structs one per record. **Random reads of one
   field: columns win by 5 in memory**, mostly because one 16 MB column still fits the third level when 128 MB of
   records do not. The crossover is at two fields: a gathered loop that reads two or more
   fields of a record wants those fields together.
4. **Hot and cold groups are the answer to point 3 when gathers and walks mix.** The spreadsheet gathers one field
   (the value) at random and walks the formula fields in order: the hot struct of formula fields plus a value column
   beat both pure forms (221 against 282 and 774 ms at 2 million cells).
5. **A filter with a mispredicted branch costs the same in every layout (2.7 to 3.7 ns) until it becomes a masked
   vector select, which only columns allowed here: 0.08 to 0.29 ns in AVX2, ten to twenty-five times.** The
   branch predictor learns the pattern of a list that fits in the first two levels and is walked again (0.25 to
   0.6 ns at 512 and 16 384 records), so a small list hides the cost.
6. **A selective filter (one record in 64 passes it) in cache prefers the scalar branch over the vector select:**
   the report's 64 passes take 446 µs inline against 711 µs in columns at 8 192 sales, and 8.8 against 10.1 ms at
   131 072, because the select computes the body for every record and the branch skips 63 in 64. From 2 million
   sales (past the third level) columns win by 1.6. The crossover is the size where the pass becomes bound by memory.
7. **SIMD pays where the data is in cache or the loop is not bound by memory, and only once each lane is safe:**
   a checked Long sum is 3.9 (SSE2) to 7 (AVX2) times slower than the same sum proven unable to overflow; an
   Integer sum checked once by a bound is 3.5 to 7 times faster than checked per step on AVX2 and slower on SSE2.
8. **Memory order must follow list order.** Spite's pool walked in its list's order costs 1.87 ns a record for one
   field in memory; the same pool after the list was reordered (a sort, a shuffle) costs 6.35, and all eight fields
   28.5 against 2.64 (eleven times). After a sort, every later walk is a random walk until the objects move. The
   sort case confirms it: sorting keys and gathering whole records once into sorted order (the expert form) beats
   leaving them in place and gathering each field per walk once the orders pass the third level (423 against 594 ms
   at 8 million), and loses below it (2.7 against 2.5 ms at 131 072).
9. **A conversion pays back in a few passes in memory, in many in cache.** Inline structs to columns cost 4.65 ns a
   record in memory; each one-field pass saves 1.23 ns, so it pays from the fourth pass. Spite's pool to one column
   costs 2.21 ns and saves 1.74 a pass: it pays from the second. At 16 384 records (the second level's edge) the
   same conversion needs about fifteen passes.
10. **Narrowing a column is a layout too.** The report's expert form keeps 15 bytes a sale instead of 32 (the
    region in a byte, the price in two) and with the fused pass runs in 102 ms against 790 for 32-bit columns at 8
    million sales; the image's planes of bytes are a quarter of Spite's 32-bit channels before any other change.
11. **Where the scan of the input dominates, the layout of what it produces barely moves the total.** The tokens'
    passes run nine times faster in fused columns than in naive C's objects, but the whole program only 1.9 times,
    because writing and scanning the text is most of it in every form (four fifths in naive C, nearly all in the
    expert form).
12. **Fusion is a separate decision from layout, and can lose.** The image filter fused into one loop measured
    about four times slower than three vectorised passes over planes (the fused loop with several stores and sums of two
    widths is not vectorised), while the report's three reports fused into one pass are its biggest single win.

## The cost model (proposed by Claude, unconfirmed)

For each list and each loop over it, the compiler can work out, while compiling:

- **F(loop)**: the fields the loop reads and the fields it writes (with every function the body calls, followed all
  the way down), and **whole(loop)**: whether it uses an item as a whole (passes it where a reference is kept,
  compares identity, prints it, reflects on it).
- **pattern(loop)**: in order (the index is the loop's counter, or a member template), gathered (the index is read
  from data: `cells[cell.first]`), or by key.
- **P(loop)**: how many times it runs per change of the list (a constant, a counted loop around it, or unknown).
- **N**: the list's count (a constant, a proven bound, the profile of D530, or unknown).
- **S(field)**: each field's size in bytes after narrowing.

Then the bytes a pass moves, per record, for each layout:

| layout | in order | gathered |
|---|---|---|
| objects behind pointers (today) | 8 + the object (header and every field), padded to the line when objects straddle | one line per record, plus the pointer |
| inline structs | the whole struct, whatever the loop reads | one line per record (two when the struct straddles) |
| columns | the sum of S over F(loop), read and written | one line per field in F(loop) |
| groups (hot and cold) | the sizes of the groups F(loop) touches | one line per group touched |
| AoSoA, L lanes | as columns when L times the field's size is a whole line, else as the groups | one line per field, as columns |

and the time of a pass is the larger of its arithmetic and its bytes over the level's bandwidth (in order) or its
lines times the level's latency over the loads in flight (gathered), the level being where N times the bytes sit.
The measured constants on this machine, for one core: 24 to 31 GB/s from memory in order (one column of 4-byte
values at 0.13 ns a record is 31 GB/s, inline structs at 1.36 ns 24 GB/s, the pool's 48 bytes at 1.87 ns 26 GB/s),
and about 6 ns a gathered line from memory with the processor overlapping several (one field of inline structs in
a random order), against about 1 ns from the third level (one column in a random order, its 16 MB in the cache). The choice is the layout whose sum over the
program's loops of P times the pass's time, plus the conversions between phases, is least.

## The rules (each proposed by Claude, unconfirmed)

### Rule 1: columns for a list whose items never leave it

**When:** a `List<C>` (or `Vector<C>`, `Items<C>`) of a class whose fields are numbers, `Boolean`s and enums, whose
items are never seen as a whole outside the list, and at least one loop over it reads a strict subset of the
fields, in order or gathered one field at a time.
**What it becomes:** one column per field (a structure of arrays), the item an index; `list[index].field` a read of
one column.
**What must be proven:** no reference to an item is kept anywhere but the list (no attribute, no other list, no
return, no function value of it, no `this` that escapes from the class's own functions); no identity comparison,
no `.memory`, no reflection over an item; every function that receives an item is lent it (D257) and is compiled
again for an item that is (columns, index), its reads and writes rewritten; nothing observes the order of writes
to two fields of one item from another thread. A `Vector`'s borrowed items already satisfy most of this.
**Why:** crossovers 1, 2 and 7; the report's columns at 8 million sales take 790 ms against 1 739 for Spite today.
**Falls back:** the class pool, as today.
**When the sizes are unknown:** this rule needs no size: columns never lost an in-order pass in any measurement.

### Rule 2: keep together the fields a gathered loop reads together

**When:** a loop reads two or more fields of an item reached by an index read from data (a gather), or most loops
read the whole record in a random order (a lookup by key).
**What it becomes:** those fields in one inline group (an array of structures of the group); the rest in columns or
another group.
**What must be proven:** Rule 1's escape facts; the co-read sets of every gathered loop.
**Why:** crossovers 3 and 4: a gather of all eight fields costs 27.6 ns inline against 49.6 in columns; the
spreadsheet's hot and cold split is its fastest form.
**Falls back:** Rule 1's columns.

### Rule 3: AoSoA only for loops that read many fields in order, in cache, with vectors

**When:** a loop reads three or more fields in order and the list fits the second level, with vector code for the
target. **What it becomes:** blocks of as many records as one vector register holds of a field.
**Why:** four fields in the SSE2 build: 0.32 ns against 0.47 for columns at 512 records. Past the second level and
in random order AoSoA lost to columns or to inline structs in every table, so it is the last layout to build.

### Rule 4: a loop over columns becomes vector code once every lane is safe

**When:** the loop reads columns in order, writes at most the current index of each, and its body is arithmetic,
comparisons and selects. **What must be proven:** no two columns alias (they are the compiler's own allocations);
no dependence between iterations but reductions; each operation is lane-safe: an overflow check proven unneeded
by ranges (a Long sum of fewer than 2^31 Integer terms cannot overflow; `%`, `clamp`, `minimum`, `bits_and` bound
their results), or deferred to one check per block with the scalar loop run again from the block's start to halt
exactly where the written loop would; no read that can fail (bounds proven); division only by proven non-zero
divisors; decimal sums reordered only where the language allows it (not in a loop that compares decimals with `==`
or `!=`). An `if` becomes a mask and a select when both sides are safe to compute for every lane.
**Why:** crossovers 5 and 7 (ten to twenty-five times for a 50% filter, four to seven for a sum).
**When the selectivity is low and the list fits the cache:** keep the scalar branch (crossover 6). With N unknown,
compile both and branch once on the count against the target's third level, or take the choice from a profile
(D530).

### Rule 5: memory follows list order

**When:** a list of objects is reordered (a sort, a reversal, a filter into a new list, removals that move items)
and the list it came from is not read again, and a later loop walks the new order.
**What it becomes:** the objects moved (copied into new pool slots, or new rows of the columns) in the new order,
in the same pass that builds it.
**What must be proven:** no other reference to the objects survives (the old list dead, Rule 1's escape facts); an
object's address is observed by nothing (`.memory`, a foreign call).
**Why:** crossover 8: a walk in memory order is three to eleven times faster than the same walk after a sort.
**When N is unknown:** it pays from the first walk past the third level and loses slightly below it; branch on the
count, or profile.

### Rule 6: convert between phases when the passes pay for it

**When:** one phase uses the list whole (or in a layout of its own, such as appending one object at a time while
parsing) and a later phase walks fields of it P times. **What it becomes:** a compiled conversion loop at the phase
boundary (S3). **Pays when:** P times the pass's saving exceeds the conversion: from the second pass in memory for a
pool to one column, the fourth for inline structs to columns, about fifteen at the second level's edge (crossover
9). With P or N unknown, the decision is a run-time branch between the two compiled forms on the count, or a
profile.

### Rule 7: narrow a field to the bits its proven range needs

**When:** every value a field can hold is proven to lie in a range (from the expressions that assign it, or the
values a list only its class fills, as the compiler already lists them), and the field lives in a column or group.
**Why:** crossover 10. **Falls back:** the declared width. Reading it widens, so nothing a program sees changes.

### Rule 8: fuse passes over the same list only when the fused loop keeps its vector form

**When:** consecutive loops over one list share nothing written that another reads out of order (L8, S5). **Why:**
crossover 12: fusion is the report's largest win and the image's largest loss. The compiler decides it with the
cost model, after vectorising each candidate, not before.

## How the rules map onto what the compiler already proves

| fact a rule needs | where the compiler has the beginning of it |
|---|---|
| the fields a loop reads and writes, followed through every call | [overlap_facts.spite](../../bootstrap/source/generation/overlap_facts.spite) records, per function of the generated C, every `f:Class#field` it reads and writes, the contents of containers by the attribute that holds them, and whether it loops, merged through every call it reaches (D505). The same walk, run per loop body instead of per function, gives F(loop); today it answers for a call, not a loop, and has no counts. |
| that an item never leaves its list | [count_sharing.spite](../../bootstrap/source/generation/count_sharing.spite) walks every piece of the written-out C to find what code a thread or a function value may reach and where a pointer is taken; extended to "where a pointer to an object of class C is stored", it is Rule 1's and Rule 5's escape proof. Borrowed items (D257) already forbid keeping a `Vector`'s item. |
| where the objects of a list are made, and that they sit together | [class_pools.spite](../../bootstrap/source/generation/class_pools.spite) chooses, per class a list holds, a pool of blocks side by side; it is where a column store would replace the pool (Rule 1) and where objects would be moved in list order (Rule 5). |
| the values a field can hold | [item_values.spite](../../bootstrap/source/generation/item_values.spite) lists the values a list only its class fills can hold (S1 step 1); the same study of assignments, extended from enumerated values to ranges, gives Rule 7's narrowing and Rule 4's overflow proofs. |
| the count of a list and how often a loop runs | a constant or a `while index < list.count()` today; D530's profile for the rest. |

## What an own backend needs to know about the CPU

What decided every crossover above, and so what the backend has to know per target (from a table per processor
model, or measured once by a sweep like this one when the target is the build machine):

- the size, line size and associativity of each cache level, and which levels a core shares (the third level here
  is per eight cores): the sizes N times the bytes are compared against;
- bandwidth in order and latency gathered, per level, and how many misses a core keeps in flight: the two constants
  of the cost model;
- the vector width and what it can do in a lane: 64-bit compares and absolute values (SSE2 lacks them, which
  turned the deferred check from a win into a loss), masked loads and stores, gathers and their cost, saturating
  byte arithmetic (the image filter), widening multiplies;
- the hardware prefetcher: how many streams in order it follows (eight columns are eight streams; AoSoA and groups
  are fewer) and whether it fetches line pairs;
- the branch predictor's reach: how long a pattern it learns, which decides when a branch beats a select
  (crossovers 5 and 6);
- the cost of a store forwarded to a load of the same address (the histogram's four tables) and of page walks for
  columns of hundreds of megabytes (large pages).

The C backend lets clang decide most of the vector code from `restrict` and the loop's shape; the own backend has
to make each decision itself, with the proof of Rule 4 written out in the compiler rather than guessed by the C
compiler.

## Build order (proposed by Claude, unconfirmed)

1. **Drop the overflow checks a sum provably cannot need** (Rule 4's first half): a `Long` total of `Integer` terms
   added once per pass of a loop bounded by a list's count, and ranges from `%`, `clamp`, `minimum`. Small, needs no
   layout change, and every `sum_` template and report loop gets it: measured 3.9 to 7 times on a column already,
   and it is what every later vector loop needs first. **This is the first optimisation to build.**
2. **Per-loop field sets** in `--optimization-report`: for every list, the fields each loop reads and writes, its
   pattern and whether anything uses an item whole. No code changes; it is the data every layout rule needs, and
   shows on real programs (the engine, the compiler) where Rule 1 would apply.
3. **Memory follows list order** (Rule 5) for `sort_by_` and filters into a new list whose source is dead: one pass,
   inside the class pools, three to eleven times on every later walk past the third level.
4. **Columns for a list whose items never leave it** (Rule 1), first for a list local to one function or one
   attribute, of a class of plain numbers, every field its own column, functions lent an item compiled again for an
   index.
5. **Vector loops over columns** with masks and deferred checks (the rest of Rule 4), branching on the count for a
   selective filter.
6. **Groups for gathered fields** (Rule 2) and **narrowing** (Rule 7).
7. **Conversions between phases** (Rule 6) and **fusion by the cost model** (Rule 8), both reading D530's profile
   where the counts are not known while compiling.
8. **AoSoA** (Rule 3), last: the smallest and least general win measured.

## What this study did not settle

- How a lent item becomes (columns, index) when the function lent it is also called with an object from elsewhere:
  two copies of the function, or a refusal of Rule 1 for that list.
- Whether a list whose items are kept by another list (an index of the same records) can keep columns with a
  reference that is an index; the measurements say the gain is large, the proof is not designed.
- The second processor model: every constant here is one machine's. The sweep runs anywhere C does, and should be
  run on an Intel core with AVX-512 and on an ARM core with NEON before the cost model's constants are trusted.
