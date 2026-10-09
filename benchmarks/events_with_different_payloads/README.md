# Events with different payloads

An archetype case (not an optimisation the compiler makes yet): an event queue, a `List<Payload>` of a union of
`Click`, `Key` and `Resize`, filled with 100 000 events and handled in order, forty rounds. A click adds to a total,
a key is folded into a hash of the keys typed so far (so the keys' order is part of the answer), and a resize sets
the last width and height (so the resizes' order is too). Ten percent of the events are resizes and the rest are
clicks and keys. It is one of the measurements behind
[design/proposals/compiler_archetypes.md](../../design/proposals/compiler_archetypes.md): a list of a union grouped
by member, which is the archetype question for an item whose shape is which class it is.

## The forms

- [`naive/`](naive/): `union Payload { Click Key Resize }`, a `List<Payload>` cleared and refilled each round,
  then `events.each(handle)`, where `handle` switches on the member. `--events=N` (per round), `--density=P` (the
  percent of events that are resizes) and `--rounds=N` are `Environment` settings.
- [`naive.c`](naive.c): the same program as a C programmer writes it: each event a `malloc`'d struct with a tag, an
  array of pointers, every event freed when the queue is cleared, the same switch.
- [`expert_tagged.c`](expert_tagged.c): the events inline in one array of tagged unions, refilled in place, the
  same switch.
- [`expert_dense.c`](expert_dense.c): a column of tags and a column per payload field (columns with holes), one loop
  with a select per member.
- [`expert_tables.c`](expert_tables.c): one table of columns per member; each event is appended to its member's
  table, and each table is handled by a loop of its own with no switch. The order within a member is kept; the order
  between members is lost, which nothing sees, since the three handlers write nothing in common.
- [`expert.c`](expert.c): the fastest by hand at every setting measured, the tables.
- [`highlights.c`](highlights.c): `Naive_handle`, the `each(handle)` loop and the allocation of a `Click` the
  compiler writes.
- [`generated.c`](generated.c): all of the C the compiler writes from `naive/`, tree-shaken, as an `--optimized`
  build for Windows writes it.

Every C form prints the time of making the events and of handling them after its `microseconds` line.

## What to look at in highlights.c

Every event is an object of its own: `Click___allocate` is a `malloc`, since the list holds the union and not the
class, and only a class a list holds gets a pool. The loop counts each event up and down around the call
(`Naive_Payload___retain`, then a release inside `Naive_handle`), and `Naive_handle` reads the event's class from
its header to choose the branch. The tables hold no objects at all: a click is three numbers at the end of three
columns, and handling the clicks is one loop the C compiler turns into vector code.

## Timings

<!-- timings -->
<!-- /timings -->

At other sizes and densities: [results.md](../../design/proposals/compiler_archetypes/results.md#events_with_different_payloads).
