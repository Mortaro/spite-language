# An inventory with fields set and cleared

An archetype case (not an optimisation the compiler makes yet): 200 000 records in a `List<Record>`, each with a
price and a stock, a `supplier: Supplier?` set when the record is made and never changed, and a
`reservation: Reservation?` that is set and cleared over time. Each of twenty passes sets or clears the reservation
of 2 000 records chosen at random (clearing one returns its quantity to the stock), then adds up the reserved
value, the late quantity of the records that have both a supplier and a reservation, and ages every reservation. It
is one of the measurements behind
[design/proposals/compiler_archetypes.md](../../design/proposals/compiler_archetypes.md): a part that never changes
and a part that churns, in one list.

## The forms

- [`naive/`](naive/): a `Record` class with two optional parts; each pass is a `while` that calls
  `record.toggle(seed)` on records picked by index, then `records.sum_reserved()`, `records.sum_late()` and
  `records.each_wait()`, each member testing the parts it needs. `--items=N`, `--density=P` (the percent of records
  with each part at the start), `--passes=N` and `--churn=N` (records toggled a pass) are `Environment` settings.
- [`naive.c`](naive.c): the same program as a C programmer writes it: a struct per record with a pointer to each
  part, one `malloc` per object, a reservation freed when it is cleared, the same loops and tests.
- [`expert_inline.c`](expert_inline.c): the parts inside the record with a byte saying which are there; setting and
  clearing only flip a bit.
- [`expert_dense.c`](expert_dense.c): columns over every record, the parts included (columns with holes).
- [`expert_sparse.c`](expert_sparse.c): price and stock as columns, each part in a sparse set.
- [`expert_tables.c`](expert_tables.c): one table per set of present parts; setting or clearing a reservation moves
  the record to the other table (its columns copied, the last row of the old table moved into the gap, and the map
  from record to table and row kept up to date).
- [`expert_hybrid.c`](expert_hybrid.c): the supplier, which never changes, decides the table (two tables, and a
  record never moves); the reservation, which churns, is a sparse set beside them.
- [`expert.c`](expert.c): the fastest by hand at the default settings, the hybrid. Which form wins elsewhere is in
  the proposal.
- [`highlights.c`](highlights.c): `struct Record`, `Record_toggle`, `Record_late` and the `sum_reserved` loop the
  compiler writes.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an `--optimized`
  build for Windows writes it.

## What to look at in highlights.c

`Record_toggle` lets the old `Reservation` go (a release that frees it) or makes a new one (a `malloc`, since no list
holds reservations and only a class a list holds gets a pool); `Record_late` tests both pointers for every record
and follows each to its own object. Every pass reads every record three times, to answer for the half or quarter
that has the parts. The hybrid walks only the reservations, in a dense array, and reaches the record's columns from
there; nothing it holds moves when a reservation comes or goes but the reservation's own row.

## Timings

<!-- timings -->
<!-- /timings -->

At other sizes, densities and churn: [results.md](../../design/proposals/compiler_archetypes/results.md#inventory_with_fields_set_and_cleared).
