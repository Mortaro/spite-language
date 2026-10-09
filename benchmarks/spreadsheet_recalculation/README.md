# A spreadsheet recalculation

A data-oriented case (not an optimisation the compiler makes yet), and the one with gathered reads: a million cells,
each a formula over two other cells (one near it, one anywhere before it) or a constant, recalculated in order twenty
times after an edit, with the values summed after each round, and once at the end a sum over each cell's placement
(row, column and style, fields no recalculation reads). It is one of the measurements behind
[design/proposals/data_oriented_layout.md](../../design/proposals/data_oriented_layout.md): random reads of one field
of a fat record, where a structure of arrays wins by the size of the record, not by the vector width.

## The forms

- [`naive/`](naive/): a `Cell` class of eight fields with `computed(left, right)` switching on its formula, held in
  a `List<Cell>`; a `while` that reads `cells[cell.first].value` and `cells[cell.second].value` and writes
  `cell.value`, `sum_value()` after each round and `sum_placement()` at the end. `--cells=N` sets how many cells.
- [`naive.c`](naive.c): the same program as a C programmer writes it: a struct per cell, one `malloc` each, an
  array of pointers, the same steps.
- [`expert_aos.c`](expert_aos.c): the cells inline in one array of 48-byte structs, the same steps.
- [`expert_soa.c`](expert_soa.c): eight columns; each recalculation gathers its two operands from the value column
  alone (8 bytes a cell instead of 48).
- [`expert.c`](expert.c): split hot and cold: a 16-byte struct of formula, references and constant that the
  recalculation walks in order, the values in their own column narrowed to 32 bits (every value is a remainder of
  1000003 or a constant below 1000), the placement fields in a cold array, and the values' sum folded into the
  recalculation's pass.
- [`highlights.c`](highlights.c): `struct Cell`, the recalculation, `computed` and the values' sum.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an `--optimized`
  build for Windows writes it.

Every C form prints the time of making the cells, recalculating and placing after its `microseconds` line, and
takes `--cells=N` too.

## What to look at in highlights.c

`Naive_recalculate___held_0` follows two pointers per cell to read one 8-byte value from each of two 56-byte
objects (the header, eight fields and their padding), so each gathered read brings in a 64-byte line of which it
uses 8 bytes. `struct Cell` keeps `row`, `column` and `style` beside the hot fields, though only `Cell_placement` reads them, once.

## Timings

<!-- timings -->
| form | best µs | executable bytes |
|---|---|---|
| Spite: `naive/`, `--optimized` | 625 685 | 239 616 |
| naive C: `naive.c`, `clang -O2` | 585 704 | 141 824 |
| expert C: `expert.c`, `clang -O2` | 102 526 | 141 312 |

Spite takes 1.07 times naive C's time and 6.10 times expert C's (lower is faster).
Best of seven interleaved runs, 2026-10-09, Windows, AMD Ryzen 9 5950X 16-Core Processor, 32 logical processors, clang version 19.1.5.
<!-- measured spite=625685 naive=585704 expert=102526 -->
<!-- /timings -->

At other sizes, and with each phase apart: [cases.md](../../design/proposals/data_oriented_layout/cases.md#spreadsheet_recalculation).
