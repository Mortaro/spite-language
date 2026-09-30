# One `List`: can the compiler really pick the layout? (a study for D222)

**Status:** a feasibility study, not a decision and not a build. Everything below that reads as a rule is proposed
by Claude, unconfirmed. [D222](../decisions.md) asks for one list class, `List`, whose layout the compiler
chooses (an array in the frame when the size never changes, items inline when possible, SIMD for plain values,
`each` in parallel when it pays) and says to build it only if the optimisations are really reachable. Mortaro
added two ideas while the study ran, and both are answered here: keeping a reference into the list's storage as a
plain pointer when that is safe, and letting such a reference fall back to a weak one (slot plus generation) that
reads as `T?`.

## The answer in one table

| Optimisation | Verdict | Why, in one line |
|---|---|---|
| A fixed-size local list in the frame (or constant data) | **realistic** | 12% of the compiler's list sites qualify; D211 already does it for variadic lists |
| Plain values and text stored flat | **already done** | a `List<Float>` is a plain array of floats today; there is nothing to choose |
| SIMD on plain values | **realistic, but it is not a layout** | today no list loop vectorises; emitting the loop plainly makes it 8.5x (List) and 42x (Vector) faster |
| SIMD on a `Float` sum | **not realistic** | it changes the printed result; D36 forbids an optimisation that does |
| Objects inline, chosen automatically | **partly realistic** | proven for 0-8% of object lists; the engine package's columns are refused, and a refusal costs 4.3-4.9x (8.2 ms to 35-40 ms a tick) |
| Keeping a reference into inline storage (Mortaro) | **partly realistic** | safe when D169's call effects prove no resize while it lives; chunked storage to allow growth costs 1.9x on the stress shape |
| Weak references into storage, slot plus generation (Mortaro) | **realistic as a form of `Weak<T>`; not as a silent fallback** | +3% a tick in the real stress program; but turning a `T` into a `T?` is a change of meaning, not an optimisation |
| `each` in parallel on its own | **not realistic** | no candidates outside code that already says `parallel_each_`; light bodies get 2.4-2.6x slower; one pass makes the whole program's counting atomic (a List walk 3.4-3.5x slower) |

**Recommended first:** the frame and constant lists, and loops over plain values emitted so the C compiler can
vectorise them. Both help today's `List` without deciding anything about D222. Then fold `Vector<plain>` and
`Items<plain>` into `List` (their layout is already identical); keep a hand-picked inline collection for objects
until the compiler can prove ownership at `append` and report every refusal ([below](#what-to-build-first)).

## How it was measured

- **Where lists are, and what they could be:** `benchmarks/one_list/list_sites.py` reads Spite source line by line
  (a file is a class, one statement a line) and, for every `List` site (an attribute or a local whose type is a
  `List`), decides which layout it could get and why the others were refused. It follows a list into the
  functions it is passed to, to the attributes it is stored in and back to the callers it is returned to, and
  follows an appended value back to where it was made, through callers when it is a parameter. It changes
  nothing. Its limits are [at the end](#what-the-study-script-gets-wrong).
- **Corpora:** the compiler (`bootstrap/`, 925 list sites), `library/` (82), the 200 conformance programs (173),
  the 20 benchmarks (29), and a read-only copy of the engine package (`engine/`, `plugins/`, `examples/`, 417 files, 298
  sites) in `.spite`.
- **Speed:** the engine package's `examples/stress` compiled by this tree's compiler (`--optimized`, `clang -O2`, 200 000
  entities, 20 ticks, parallel stage); `benchmarks/one_list/layouts.c`, the layouts written by hand in C so each
  costs only what the layout costs; `benchmarks/one_list` (Spite), `each_` against `parallel_each_` on 200 000
  elements with a light and a heavy body; and a small probe of `List<Float>` and `Vector<Float>` loops, read with
  clang's vectoriser remarks. A Windows machine with 32 logical cores, clang 19.1.5. Times move 5-15% between
  runs on this machine; ranges are given where they did.

## 1. What makes each layout invisible

Every class is passed by reference ([D149](../decisions.md)): a `List` of objects holds the objects themselves,
shared with every other name that holds them. So a layout is an optimisation only when the program cannot tell.

### Objects inline

An inline list holds a copy of each object's attributes and no object. It means the same as a list of references
exactly when nothing can observe the difference:

1. **The appended object is owned by the list alone.** Nothing else holds it after the `append`. In practice it is a
   fresh object whose last use is the `append` (`var particle = Particle(index)`, then `particles.append(particle)`,
   in the same block, nothing after). Anything else, and a later change through the other holder would not show in
   the list's copy.
2. **No item read from the list escapes.** It is not kept in an attribute, another list or a second name, not
   returned, not passed as an argument, not captured in a function value, not compared by identity, and not handed
   to a passed function (`each(f)`). Reading and writing its members is fine.
3. **No second list shares the items.** `filter_`, `sort_by_` and `copy()` make a list of the same objects; inline
   they would be copies.
4. **Nothing counts the instances.** `Velocity.instances` sees objects, and an inline item is not one.
5. **The class fits inline** (D204's rule: numbers, `Boolean`, enums, text; no `drop()`, no `this` as a value).
6. **An item read is not used past a resize.** A pointer into the block is moved by growth and by removal.

What the compiler already computes: (5) is `fits_vector()` (D217); (6) is D204's check with D169's call effects
(`grow:`/`shrink:` facts through callers and parameters); (2) is D204's list of borrow errors, and the proof behind
[reading a list's elements uncounted](../../docs/optimizations.md#a-lists-templates-read-its-elements-without-counting-them).
What is new: (1) is an ownership proof at `append` across calls, walks and `Anything` parameters, which nothing does
today; (2) exists only as **errors** on a `Vector`, and for an automatic `List` each must become a silent fallback
to references plus a line in a report; (3) and (4) are checked nowhere, since a `Vector`'s `filter_` copies and its
items were never instances.

There is also an API difference to settle before any merge: `List`'s `[]` answers `T?` (null out of range),
`Vector`'s and `Items`' answer `T` and halt out of range. One `List` must pick one.

### An array in the frame

A local list lives in the frame (or, when it is a literal that never changes, in the program's constant data) when
its size is known while compiling (a literal, or appends only in straight-line code, never in a loop, never
removed from) and it never leaves the function: not returned, not stored, not put in a list, and passed only to
functions that only read it. What exists: D108's placement and D211's
[variadic list in the caller's frame](../../docs/optimizations.md#a-variadic-list-the-callee-only-reads-lives-in-the-callers-frame),
which is this rule for one case. What is new: counting straight-line appends, and the constant-data form.

### SIMD

Plain values (numbers, `Boolean`, enums) are already stored flat in a `List`, a `Vector` and an `Items`: the
layout is done. What stops vectorising is how the loop is written in C. Clang's own remarks on today's output:

| Loop (Spite) | clang says |
|---|---|
| `into[index] = from.get_at(index) * 1.5 + 0.25` over two `List<Float>` | could not determine number of loop iterations: `from.count()` is read again every pass |
| `total = total + from.get_at(index)` over a `List<Float>` | cannot prove it is safe to reorder floating-point operations |
| `numbers[index] = numbers[index] * 1.5 + 0.25` over a `Vector<Float>` | call instruction cannot be vectorized: the range check's crash report is inside the loop |

So SIMD needs (a) the count read once when the loop cannot change it (D169 already proves this for narrowing), (b)
the range check left out where `index < list.count()` proves it (the same proof), (c) `Float` arithmetic kept in
`float` (the C today multiplies by the `double` constant `1.5`, which halves the vector width and computes a
`Float` expression in double precision), and (d) two distinct lists marked as not overlapping (`restrict`), which
the compiler knows when they are two different objects. A `Float` or `Double` **sum** is different: vectorising it
reorders the additions and changes the last bits of the answer, which D36 does not allow an optimisation to do.
Integer sums vectorise freely.

### `each` in parallel

Everything D35 checks for `parallel_each_` (the member reaches only its own attributes that hold values, singletons
and locals), and three more facts:

1. **The items are distinct objects.** D35's check "cannot see two elements that are the same object"; that is
   acceptable when the programmer asked for a parallel pass, not when the compiler decides on its own. Only an
   inline list, or one whose appends are all proven fresh (fact 1 above), is distinct by construction.
2. **Nothing ordered happens.** D35 allows any singleton, and `Console` is one: a parallel `each` that prints
   prints in a different order. The member and everything it calls must reach no printing, file, socket, clock or
   random singleton, and write no program singleton.
3. **It pays.** The element count is known only at run time, so a branch on it is unavoidable; the body's weight
   can be judged while compiling (a loop inside it, or enough work).

And one cost that is not local at all: a program that runs any parallel pass switches **every** reference count in
the program to atomic operations ([Atomic reference counts only with threads](../../docs/optimizations.md#atomic-reference-counts-only-with-threads)).
A compiler that parallelises one `each` on its own slows code elsewhere (measured [below](#4-speed)).

### Keeping a reference into the storage (Mortaro's idea)

"Treat the vector as the storage without the user knowing, so we can take references from the internal vector
and think it's a normal pointer": an item read from an inline `List` may be kept past D204's block, even in an
attribute, as a plain pointer, as long as it is safe:

- **(a) Growth moves items.** Two ways out. *Prove* that nothing appends while the pointer lives: D169's call
  effects already follow `grow:` facts through calls, so D204's "not past a statement that may resize" extends
  from a block to any lifetime the compiler can bound (a local across statements, a system's run while the engine
  defers structural changes). Or make the storage *stable*: fixed-size chunks, so an append never moves an item.
  Chunks cost what the measurements say: equal on a sequential walk (156-181 µs against 150-161 µs a tick), but
  **1.8-1.9x slower on the stress shape's joins** (386-413 µs against 207-220 µs), where every read goes through
  the chunk table. Chunks are not worth it as a default; prove instead, and fall back.
- **(b) Removal and lifetime.** An inline item has no count, so a kept pointer cannot keep it alive, and
  `remove_swapping` moves the last item into the hole, so a pointer to the hole silently reads *another* item, which
  is worse than a dangling one. Options: *prove* no removal while the pointer lives (the same `shrink:` facts);
  *free slots* instead of swapping, which keeps positions but walks the holes (a quarter of the slots empty: 212 µs
  against 155 µs for the same live items packed, +37%); or *generation-checked handles* (below, about +2-3%).
- **(c) Threads.** A `Parallel` reaches only its own instance and what was handed to it alone (D179, D207), and a
  parallel pass only its element's own values (D35), so a pointer cannot be shared with a task that resizes its
  list. The gap is a program singleton reached from threads: D183 guards its functions with a lock, but a function
  that *returns* a pointer into its list lets it out of the lock. D183's return check ("hand out only numbers,
  text, copies or other safe singletons") is decided but [not built](../../docs/optimizations.md#thread-safety-for-singletons-the-rest-of-the-plan);
  it has to be before kept pointers are.

What the compiler can prove with facts it has: a pointer kept in a local, or in a row or argument for one call,
while the calls between may not resize or remove (D169). What it cannot: a pointer kept in an attribute across calls
it cannot bound (across a frame, a tick, a callback, a call through a function value). There, the honest answer is
the same fallback as everywhere: the list gets references, and the report names the line that kept the item.

### Weak references into the storage (Mortaro's second idea)

"This might actually also solve weak pointers, because the parent storage cleaned up the references": a reference
to an item kept beyond what can be proven becomes a slot plus a generation and reads as `T?`, null once the item
was removed or its slot reused, so D24's narrowing makes the check.

- **As `Weak<T>`, yes.** `Weak<T>` (D197) already answers `get(): T?`, so how it is represented is the compiler's
  business. Today it is a box found through a side table keyed by address. For an item of an inline list it can be
  the list's block, the slot and the slot's generation, with no table; for an object that lives outside any list,
  the side table stays. The choice is invisible because the API is already nullable. Cost: a 32-bit generation per
  slot, only in lists whose items some `Weak` holds (tree-shaken otherwise); the handle must keep the list's block
  alive (one counted reference to the block, not to items); and it stays on one thread, as D211 already rules for
  `Weak`.
- **As a silent fallback for a kept item, no.** A kept `var first = list[0]` is a `T` that stays alive when the list
  drops it (D149). Turning it into a `T?` that becomes null on removal changes what the program means, and whether
  the program even compiles (narrowing is required on a `T?`) would depend on what the compiler managed to prove.
  To keep layout invisible it would have to be the rule for every list, references included (a list owning its
  items and every outside reference weak), which does not fit the compiler's own object graphs, where an AST node
  sits in a list and in several other structures at once. That is a language decision for Mortaro, not an
  optimisation (recorded as a question below).
- **Per read:** one load and one compare against a proven borrow's nothing. Measured: **+3%** a tick in the real
  stress program (8.39 ms best against 8.15 ms, a generation check on each of the four component reads) and
  about **+2%** in the C join loop (between -4% and +4% run to run; 210-226 µs against 207-220 µs). The tick is dominated by the runner, so the check
  hides; in a tight loop it is still small, because the generation sits beside data the loop already reads.
- **Borrow or weak, with no syntax:** a borrow where the lifetime is proven (a block, a call, a statement range free
  of resizes); anything kept longer must be written as a `Weak`, which says what the program means, or else keeps the
  list on references. The build report (D36's planned "what could not be optimised") names, per hot loop, whether
  its reads are borrows, weak or counted references, and the line responsible.
- **Parent and child.** A parent holding `children: List<Child>` and each child holding `parent: Weak<Parent>`: the
  parent is a heap object (it holds a list, so it never fits inline), so the back reference stays a side-table
  `Weak`; the children may be inline if they fit, and references *to* a child from elsewhere are slot-and-generation
  `Weak`s. The cycle is broken exactly as today. It helps flat structures (entities and their components), not
  trees, since a tree node holds a list and is never inline.

## 2. How much real code qualifies

Share of `List` sites (attributes and locals) by what their items are, and which layout the study proves:

| Corpus | Sites | Plain values | Text | Objects (fit inline / not) | Objects proven inline | Frame array | Numbers (SIMD candidates) |
|---|---|---|---|---|---|---|---|
| compiler | 925 | 31 (3.4%) | 651 (70.4%) | 243 (2 / 241) | 0 (0%) | 107 (11.6%) | 25 (2.7%) |
| library | 82 | 13 (15.9%) | 53 (64.6%) | 16 (0 / 16) | 0 | 4 (4.9%) | 9 (11.0%) |
| engine package | 298 | 185 (62.1%) | 43 (14.4%) | 70 (21 / 49) | 2 (0.7%; 2.9% of object lists) | 6 (2.0%) | 178 (59.7%) |
| conformance | 173 | 40 (23.1%) | 59 (34.1%) | 74 (16 / 58) | 6 (3.5%; 8.1% of object lists) | 34 (19.7%) | 40 (23.1%) |
| benchmarks | 29 | 10 (34.5%) | 5 (17.2%) | 14 (8 / 6) | 1 (3.4%) | 0 | 9 (31.0%) |

What it says:

- **Most lists hold text or numbers**, 74-80% of sites in the compiler, the library and the engine package. Their storage is
  already flat, so "inline" is already true for them; a merged `List` changes nothing about their layout.
- **Lists of objects that could be inline are rare, and the proof rarely holds.** In the compiler, 241 of 243
  object lists hold AST nodes, analysis records and unions, which never fit. Where the class fits, the refusals are,
  in order: an item returned, passed as an argument or kept (the engine package 16 of 21 fitting sites; compiler 2 of 2);
  an appended value made elsewhere or still used after the append; `sort_by_`/`copy` sharing items; `.instances`.
- **The hand-picked inline sites mostly would not survive the switch.** Of 25 `Vector`/`Items` sites of a class
  that fits (benchmarks, conformance, the engine package), the study proves 4 inline if they were a `List`. 9 are generic
  columns (`Column<$component_type>`), which the script does not instantiate; the engine package's is analysed by hand
  below and is refused. The other 12 are refused mostly because the program keeps using the object after
  appending it (harmless under `Vector`'s copy-on-append, aliasing under `List`'s sharing) or because it builds
  rows, which the compiler's row rules (D206, D212, D217, D220) would accept (the script is pessimistic there).
- **Frame arrays are common in the compiler:** 107 sites (11.6%): 48 literals that never change (constant data
  rather than the frame), 36 lists filled by straight-line appends, 23 empty lists passed to readers. Two
  allocations saved each time such a function runs. The engine package has 6.
- **Loops:** the compiler walks lists in 543 loops, the engine package in 126. Of the loops over numbers (compiler 7,
  the engine package 61, conformance 13), the ones with no call, no `crash` and no early exit (the shape a vectoriser
  takes) are 0, 3 and 5; the engine package's are mostly byte packing and decoding (`target.write_float(mesh.vertices[i])`),
  where the call per element is the work. 2 of the engine package's 3 are `Float` sums, which stay scalar.
- **Parallel `each`:** 7 `each_`/`parallel_each_` loops in conformance, 4 with distinct items and no ordered effect,
  all 4 already written `parallel_each_`; the benchmarks' candidates are the ones this study wrote. The compiler's
  58 template loops are all over lists of objects that do not fit. The engine package has none: its systems are walked by
  the runner, and it already runs systems of a stage in parallel. Automatic parallelism has, in practice, nothing
  to find.

**The engine package's column, by hand.** `Column<Position>` in `examples/stress` is the list that decides the tick time.
As one `List`, it is refused for several independent reasons, each reachable in the stress program's C:
`value_at(name, row)` returns the item (`Slot.fetch`, `Lookup`, `Added`); `at(row)` returns it to the stream's row;
`remove_reference` moves the item into `buried` (another list) to spread frees over ticks; `write_at` stores back
the object a system was handed; and the appended value arrives through `insert_object(entity, value: Anything)`
from a walk over a bundle that still holds it. The engine package's own documentation says why its default is references:
"code that keeps a component and mutates it later (a `Lookup` result, a list from a `_all` system) depends on it."
With kept pointers proven by call effects (the engine defers spawns and despawns to command buffers applied after
a stage), `value_at` and `at` could become safe pointers; the ownership at `append` through the bundle walk and the
`buried` list would still need new analysis or an engine rewrite.

## 3. Representation flow and what specialising costs

A list's layout must be known wherever it flows. Specialisation is the same mechanism the compiler already uses
for rows (`<name>___lent_<positions>`) and for `Items` (one instantiation per storage): a function that takes or
returns a list whose items could be inline is compiled once per layout that reaches it, and the `List` class's own
members once per (item class, layout) pair actually used.

| Corpus | Function body lines | Functions taking or returning a list | ... of a class that fits inline | Upper bound on growth |
|---|---|---|---|---|
| compiler | 27 151 | 399 (6 563 lines) | 2 (21 lines) | 0.08% |
| engine package | 13 049 | 163 (1 877 lines) | 25 (368 lines) | 2.8% |
| library | 7 006 | 53 (433 lines) | 0 | 0% (the `List` members: once per pair used, as `Items` today) |

Lists of numbers and text need no second copy: their storage is the same either way. So code size is not the
problem: the stress program's C is 1 294 077 bytes inline and 1 297 159 bytes with references.

The problem is the **fallback**, because it is all-or-nothing for a list and every list its values flow into. The
cases that force references, from the refusals above: a function that stores, returns or passes on an item
(`value_at`), a passed function (`each(f)`, `filter(f)`), a list of lists, identity comparison, a list passed
as `Anything` or as a generic value, a call through a function value (it may do anything), `.instances`, and in a
hot-reload or REPL build every list (D143: internals stay objects). One such line anywhere in the program changes
the layout of a list elsewhere, silently, and the stress program shows what that costs.

## 4. Speed

**The engine package's stress shape** (200 000 entities, `Move` and `Regenerate`, 20 ticks, parallel stage, `--optimized`,
`clang -O2`):

| Layout | Average tick |
|---|---|
| Hand-picked inline: components `stored_inline`, `Items` columns, walked rows | 8.1-8.3 ms (best 8.15) |
| Automatic `List`, proof succeeds | the same C as above: 8.1-8.3 ms |
| Automatic `List`, proof refused (today's reference path) | 35.4-39.8 ms (**4.3-4.9x**) |
| Inline, a generation check on each of the four component reads | best 8.39 ms (+3%) |

The automatic layout is exactly as fast as the hand-picked one when the proof holds, since it is the same code, and
4-5x slower when it does not. For the engine package as written, it does not ([above](#2-how-much-real-code-qualifies)).

**The layouts by hand in C** (`benchmarks/one_list/layouts.c`, 200 000 entities, best of five rounds, three runs):

| Move over sparse columns (random joins) | µs a tick | each_integrate + filter_moving().sum_across() | µs a tick |
|---|---|---|---|
| flat inline | 207-220 | flat inline | 150-161 |
| chunked inline (1 024 a chunk) | 386-413 | chunked, walked chunk by chunk | 156-181 |
| flat, generation check | 210-226 | chunked, indexed | 165-181 |
| references, uncounted | 839-2 301 | references | 310-556 |
| references, counted | 1 361-3 208 | | |
| a quarter of the slots holes, skipped | 212-216 | | |
| the same live items packed | 155-158 | | |

**Plain values, 1 000 000 items, µs a pass:**

| Loop | Spite today | Spite's C with the loop emitted plainly | C by hand |
|---|---|---|---|
| `List<Float>`: x = y * 1.5 + 0.25 | 683 | 80 | 78-83 vectorised, 227-337 scalar |
| `Vector<Float>`: x = x * 1.5 + 0.25 | 2 783-2 808 | 67 | |
| `List<Float>` sum | 660-671 | (stays scalar) | 660 strict, 82 with `-ffast-math` |
| `Integer` sum | | | 118-168 vectorised, 154-166 scalar (memory-bound at this size) |
| `Integer` sum, each read a function call | | | 1 360-1 460 |

"Emitted plainly" is today's C with the count read once, the range checks left out (proven by the loop's
condition), `Float` kept `float`, and the two lists marked `restrict`, patched by hand (`.spite` only): clang
then vectorises both loops. The gain is 8.5x on `List` and 42x on `Vector`, and none of it needs a layout choice.

**`each` against `parallel_each_`** (`benchmarks/one_list`, 200 000 `Particle`s, µs a pass, three rounds):

| | List light | List light, parallel | List heavy | List heavy, parallel | Vector light | Vector light, parallel | Vector heavy | Vector heavy, parallel |
|---|---|---|---|---|---|---|---|---|
| program with a parallel pass | 850-903 | 353-405 | 99 198-99 671 | 4 071-4 192 | 119-137 | 304-330 | 98 614-98 902 | 4 023-4 069 |
| same program, every pass `each_` | 248-257 | | 97 306-98 158 | | 105-129 | | 95 672-97 768 | |

- A heavy body (a 200-step loop per element) is 24x faster in parallel on 32 threads: worth it, and visible while
  compiling (the body has a loop).
- A light body on inline storage is **2.4-2.6x slower** in parallel (the split and join cost more than the pass).
- A light body on a list of references is faster in parallel only because each element is a cache miss.
- Merely containing a parallel pass made the **sequential** `List` walk 3.4-3.5x slower (248-257 µs to 850-903 µs):
  the program's reference counts became atomic, and `each_step` reads its elements counted. So a compiler that
  parallelises one `each` by itself taxes every other counted operation in the program. (`each_step` being counted
  at all, when `each_simulate` over the same list is read uncounted, looks like a missed case of
  [the uncounted-read proof](../../docs/optimizations.md#a-lists-templates-read-its-elements-without-counting-them) worth a
  separate look.)

## 5. How `library/list.spite` would read

The layouts would be folded branches, exactly as `library/items.spite` folds on `$element_type.fits_vector()` today
and as a parallel pass shows its `ThreadPool.run_pieces` call. The difference is that the layout is a fact of the
list *site*, not of the item class, so the fold asks the list, not `$element_type`. A sketch (names provisional):

```gdscript
generic $element_type

var heap = Memory.Heap()
var inline = InlineMemory<$element_type>()
var references = TypedMemory<$element_type>()
var items: Memory.Address = 0
var item_count = 0
var capacity = 0

func append(value: $element_type) {
    make_room()
    if layout.inline {
        inline.write_item(items, item_count, value)
    } else {
        references.write_value(items, item_count, value)
    }
    item_count = item_count + 1
}

func each_member(member: Symbol<$element_type>) {
    if layout.parallel {
        ThreadPool().run_pieces(each_member_piece, item_count)
        return
    }
    var index = 0
    while index < item_count {
        var item = item_at(index)
        item.attributes[member]
        index = index + 1
    }
}
```

`layout.inline` would be decided per instantiation the way `fits_vector()` is, and the branch not taken is not
compiled. The frame array needs no branch: it is placement (D108), like a frame slot for `heap.allocate`, and
`make_room` on a proven fixed list is simply never called. SIMD has nothing to show in the source either: it is
the C compiler's work once the loop is emitted plainly. So the source stays honest about the two choices that change
what runs (inline or references, one thread or many) and about nothing else.

## What to build first

1. **Frame and constant lists** (realistic; small, safe wins; extends D211). A local list of known size that never
   leaves its function goes in the frame; a literal list that never changes goes in constant data. Measurable as
   allocations under `--debug-memory`; 12% of the compiler's list sites.
2. **Loops over plain values the C compiler can vectorise** (realistic; large wins; independent of D222). Read the
   count once when D169 proves the loop cannot change it, leave out range checks the loop's condition proves, keep
   `Float` arithmetic in `float`, and mark distinct lists `restrict`. `Float` sums stay in order.
3. **One `List` for plain values and text** (realistic; an API decision). `Vector<Integer>`, `Items<Integer>` and
   `List<Integer>` already have the same storage; folding them leaves only `[]`'s `T?`-or-halt question to decide.
4. **Objects: keep a hand-picked inline collection for now.** Automatic inline for objects becomes worth building
   only with three things together: an ownership proof at `append` across calls and walks; borrows extended from a
   block to any lifetime D169's call effects can bound (Mortaro's kept pointers, which is what would let
   the engine package's `value_at` stay inline); and the D36 build report, so a refusal names its line instead of costing
   4-5x silently. Until then, removing `Items` would make the engine package 4.3-4.9x slower.
5. **`Weak<T>` of an inline item as slot plus generation** (realistic, when someone needs it; +3%).
6. **Not automatic parallel `each`.** Keep `parallel_each_` a decision at the call site, as D35 wants. What the
   compiler may do silently is the opposite direction: run an explicit `parallel_each_` on one thread when its body
   is light and its items inline, since that only makes it faster.

Questions only Mortaro can answer, raised by this study (not recorded in `mortaros_missing_decisions.md` from this
branch, which commits only this page):

- Should a merged `List`'s `[]` answer `T?` (as `List` does) or `T` and halt (as `Vector` and `Items` do)?
- Is a silent fallback from inline to references acceptable if the build report names the line (D36), or should
  a list that was inline and becomes references be something the programmer is told more loudly?
- Should a list own its items, so that an item kept outside a list is weak (`T?`) by default? That is the only way
  the second idea becomes automatic, and it changes D149 for every list.

## What the study script gets wrong

It is a reading of source text, not the compiler, and it errs both ways:

- **Pessimistic:** calls are resolved by name and argument count, so a list passed to any function of that name
  counts all of them; generic classes are not instantiated (`Column<$component_type>` is reported as not
  analysable, and was analysed by hand); a row built from items (D206-D220) counts as an item escaping; an object
  appended to a list and then only read before anything changes it counts as shared.
- **Optimistic:** it does not follow D169's call effects, so a borrow kept across a call that resizes the list is
  missed (it found no borrow read past a resize on the list itself in any corpus); calls through function values
  and `missing_function` metaprogramming are not followed; hotness is not measured, only loop shape.
- A site is an attribute or a local whose type the script can see (a declared type, a constructor, a literal, or a
  function whose return type is written); lists reached only through expressions it cannot type are not counted.

The numbers are therefore a map of where the proofs fail and why, not a count the compiler would reproduce
exactly. The shape of the answer (most lists are text and numbers, object lists that fit are few, and the proof
fails on the code that matters) held under every correction made to the script while this was written.

## Reproducing

```
python benchmarks/one_list/list_sites.py compiler bootstrap
python benchmarks/one_list/list_sites.py library library
python benchmarks/one_list/list_sites.py conformance --each conformance/stage1 conformance/stage2 conformance/stage3 conformance/stage4 conformance/stage5 conformance/stage6
python benchmarks/one_list/list_sites.py benchmarks --each benchmarks
python benchmarks/one_list/list_sites.py engine <copy>/engine <copy>/plugins <copy>/examples
clang -O2 benchmarks/one_list/layouts.c -o layouts.exe && ./layouts.exe
bash benchmarks/run.sh .spite/spite_development.exe one_list
```

`--sites` prints every site with its verdict and the reasons it was refused; `--loops` every loop over a list. The
stress variants were made in a read-only copy of the engine package: removing `stored_inline()` from the four stress
components gives the reference path, and the generation check was patched into the generated C of the stream loops.
