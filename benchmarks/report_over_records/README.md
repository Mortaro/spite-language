# A report over records

A data-oriented case (not an optimisation the compiler makes yet): two million sales of eight whole-number fields in
a `List<Sale>`, and a report that reads them three ways: one field of eight (the units sold), five of eight in 64
filtered passes (the profit of each region in each quarter), and all eight (a fingerprint of every sale). It is the
measurement behind the layout rules of
[design/proposals/data_oriented_layout.md](../../design/proposals/data_oriented_layout.md), and the pairs L1, L2
and S5 of [naive_programs_pairs.md](../../design/naive_programs_pairs.md#layout).

## The forms

- [`naive/`](naive/): a `Sale` class of eight `Integer`s, made from a seed, appended to a `List<Sale>`; then
  `sales.sum_quantity()`, a `while` per region and quarter that adds up the profit of the sales in it, and
  `sales.sum_fingerprint()`. `--records=N` sets how many sales (an `Environment` setting).
- [`naive.c`](naive.c): the same program as a C programmer writes it: a struct per sale, one `malloc` each, an
  array of pointers, the same loops.
- [`expert_aos.c`](expert_aos.c): the first step an expert takes: the structs inline in one array (array of
  structures), the same loops.
- [`expert_soa.c`](expert_soa.c): the second step: eight columns of 32-bit numbers (structure of arrays), the same
  loops, each vectorised by the C compiler.
- [`expert.c`](expert.c): as far as it goes by hand: the columns narrowed to the bytes the values need (15 bytes a
  sale instead of 32), and the three reports fused into one vectorised pass (the 64 filtered passes become one,
  since the sum the program prints distributes over the sales).
- [`highlights.c`](highlights.c): `struct Sale`, the profit loop and the two sums the compiler writes.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an `--optimized`
  build for Windows writes it.

Every C form takes `--records=N` too and prints the time of each phase (making, one field, five fields, all
fields) after its `microseconds` line.

## What to look at in highlights.c

`struct Sale` is the eight fields behind the 8-byte header every object has, and the list holds pointers to them:
the class pools put the sales side by side, so a walk reads them in memory order, but every pass still reads every
byte of every sale (40 of them, plus the 8-byte pointer) whatever fields it needs. In `Naive_profit_of___held_0`
every `+`, `-` and `*` is checked (`__builtin_*_overflow` with a call that halts), and the read of `sales[index]`
tests its bounds, so the C compiler cannot vectorise the loop even though it is a plain filtered sum. The Long sum
of `Integer` terms in `List_Sale_sum_quantity` and the profit loop can never overflow (fewer than 2^31 terms, each
below 2^32 in size), so those checks are provably dead; that proof is the first step the proposal suggests.

## Timings

<!-- timings -->
<!-- /timings -->

At other sizes, from 512 sales to eight million, and with each phase apart: [cases.md](../../design/proposals/data_oriented_layout/cases.md#report_over_records).
