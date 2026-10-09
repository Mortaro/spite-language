# Archetypes chosen by the compiler: a list's items grouped by the parts they have

**Status:** a research study with experiments, for the question in
[optimization_research.md](../optimization_research.md#archetypes-chosen-by-the-compiler-mortaro-2026-10-09) (D550).
Not a decision, and nothing here is built. Every rule below is proposed by Claude, unconfirmed. Nothing in the
compiler was changed for it.

Mortaro's direction: some entity systems make the programmer mark each component "sparse" or "table" so the engine
can group entities by the set of components they have (archetypes); no moron should ever have to think of that, and
it must not be limited to games but come out of the box.

The general shape is not entities at all. It is any list whose items differ in which optional parts they have: an
attribute that is `null` for some items (`texture: Texture?`), a member of a union (`List<Payload>`), a part set
and cleared over time. This page measures, on four cases of which three are not games, when grouping the items by
the parts they have pays, when a part is cheaper kept apart, and what moving an item costs; then turns the
crossovers into rules a compiler could prove per use, says how they map onto facts the compiler already
establishes, what a profile (D534) settles, and in what order to build it.

## Words

| word | what it means here |
|---|---|
| part | an attribute of a class of objects that an item may or may not have: `velocity: Velocity?`, or which member of a union an item is |
| shape | the set of parts an item has right now (a texture and no velocity) |
| table (archetype) | one store per shape: columns holding, for every item of that shape, each field of the item and of its parts, side by side |
| move | what a table layout does when an item gains or loses a part: its fields copied to the end of the other shape's table, the last row of its old table moved into the gap |
| sparse set | one store per part, apart from its items: a dense array of the part's values, the item that owns each row, and a map from item to row |
| hybrid | the parts that never change after an item is made decide its table; the parts that come and go are sparse sets beside the tables |
| columns with holes (dense) | columns over every item, the optional fields included, with a byte saying which parts are there |
| inline | each part inside the object that owns it, with a presence bit, instead of an object of its own behind a pointer |

The entity systems that ask for "table" or "sparse" ask exactly the question the hybrid answers: which parts stay
and which churn. The compiler sees every place a part is assigned, so it can answer it itself.

## What was measured

Four cases in `benchmarks/`, each a naive Spite program, `naive.c` written with objects and pointers, and the hand
forms of the table above (`expert_tables.c`, `expert_sparse.c`, `expert_hybrid.c` where a part churns,
`expert_dense.c`, `expert_inline.c`, and for the union `expert_tagged.c`); `expert.c` includes the fastest at the
case's defaults. Every form prints the same answer (D549), and `check.sh` checks it at the defaults.

| case | the list | the parts | what churns |
|---|---|---|---|
| [shapes_with_optional_parts](../../benchmarks/shapes_with_optional_parts/) | 200 000 shapes | `texture: Texture?`, `velocity: Velocity?`, each independently on a share of the shapes | nothing: parts are given when a shape is made |
| [inventory_with_fields_set_and_cleared](../../benchmarks/inventory_with_fields_set_and_cleared/) | 200 000 records | `supplier: Supplier?` (never changes), `reservation: Reservation?` | the reservation of `--churn` records picked at random a pass, set or cleared |
| [events_with_different_payloads](../../benchmarks/events_with_different_payloads/) | a queue of 100 000 events, refilled forty times | which member of `union Payload { Click Key Resize }` each event is | the whole queue, every round; the keys' order and the resizes' order are part of the answer |
| [entities_with_components_added_and_removed](../../benchmarks/entities_with_components_added_and_removed/) | 200 000 entities | `velocity` and `health` (never change), `burning` | `--churn` entities set on fire a pass; each fire put out by the entity when its ticks run out (one to eight passes) |

[sweep.sh](compiler_archetypes/sweep.sh) times every form across sizes (4 096 items, in the first two levels of
cache; 65 536 and a million, in the third; four million, far past it), densities (1%, 50% and 99% of the items
having each part) and churn (none, a thousandth, a hundredth, a tenth and every item a pass), the best of several
interleaved runs, and writes [results.md](compiler_archetypes/results.md): for each case a table of the whole
programs and a table of the passes alone (the list already made). [pools.sh](compiler_archetypes/pools.sh) measures
rule P by a hand edit of the compiler's own C ([pool_parts.py](compiler_archetypes/pool_parts.py)). The machine is
the one of the [data-oriented study](data_oriented_layout.md#the-machine): a Ryzen 9 5950X (32 KB first level,
512 KB second, 32 MB third per eight cores), Windows 11, clang 19.1.5, shared with other sessions building and
benchmarking, so single numbers move by a few percent and the largest sizes by more.

RESULTS_PLACEHOLDER
