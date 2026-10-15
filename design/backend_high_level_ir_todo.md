# To do: the high level of the IR (branch `backend-ir`)

For the next model to pick up. Started by Space Bunny Free, reviewed by Claude Opus 5.5 on 2026-10-10, continued by
Space Bunny Free on 2026-10-11 to 14, then reviewed and continued by Claude Opus 5.5 on 2026-10-14. Read first, in
order: `AGENTS.md`, [the proposal and its review](proposals/high_level_ir.md) (the review at its end overrides the
proposal), [what is built and what it covers](backend_high_level_ir.md), and decisions D558 to D561, D569, D583 to
D585 in [decisions.md](decisions.md).

## The goal, from Mortaro (2026-10-14)

1. **No optimisation reads generated C text.** Every decision the compiler makes is made on a structure.
2. **The final C is written from the high level**, before the low level exists, so that when the low level and the
   machine code arrive, every decision is already on the IR and only the printing changes.

**Done means every line of C comes from the IR** (Mortaro, 2026-10-14): not only the function bodies the printer
covers, but the classes' layouts, their allocate, release and copy functions, dispatch, the program's `main`, the
prelude it needs, and every line a later pass rewrites today. Until then the generator is a fallback being emptied,
and the work is not finished.

The two are one migration: the passes that rewrite the final C (below) can only stop reading text once the C they
rewrite is written from a structure, and a decision can only move onto the high level once the high level is the
program that is printed.

## Where it stands

- **The form** is in `compiler/source/backend/high_*.spite`: the whole program in one `HighProgram`; blocks with
  explicit edges; loops and list walks as one operation with a body; abstract objects as owners; a new value on every
  write with merged values (phis) at every join and at every loop's condition; a failed `assert` as a way out; and an
  operation of unknown effect wherever the walk does not build something.
- **Operations are lossless** for what the walk builds: each has its result, its operands in order, its operator and
  its literal, `and` and `or` are branches, and a list walk over a member function is a `call` marked `per_item`.
- **C is written from the high level** (`HighEmit`) for every function it covers, in every build: see
  [C written from the high level](backend_high_level_ir.md#c-written-from-the-high-level) for what it covers and why
  the rest is still the generator's.
- **What a function surely wrote** (`HighMustWrite`) is a forward must analysis over predecessors, checked word for
  word by `conformance/stage6/must_write_scratch/expected_high_level.txt`. Nothing reads it yet.
- **A band site's first question** (`HighBand`) is asked for every `list.each_member()` site, following which object
  is the item through every call, and compared with the C reader by the safety rule on the design page.
- The comparison runs over the whole corpus from `check.sh` (`job_high_level`, `scripts/high_level_coverage.sh`).

## Fixed on 2026-10-14 (so it is not looked for again)

- What a function surely wrote read a block's outgoing edges as its predecessors, and shared one list between blocks,
  so its answers were the returning block's own writes: a field written before an `if` was never surely written.
- An `else` branch started from the values the `then` branch left; a join kept the `then` branch's value; a loop's
  writes were forgotten after it; a branch that returned still joined.
- A bare `crash` (`crash false`, which always halts) was walked past, and `crash true` (which never halts) ended the
  block.
- A failed `assert` returns the function's default: it was not a way out of the function.
- `and` and `or` were walked as if both sides always run.
- A list walk over a member function recorded no call at all (an `assert` used as a filter skipped it), so the per
  item function's writes were invisible to every question.
- A function walked on demand was never folded, so it looked as if it wrote and called nothing: a list's `append`
  made a band site look like it wrote only its own item, which would have banded a loop that must stay whole.
- The band comparison compared two different questions (a function holding a counted loop, against the C reader
  treating that function as a per item function). It asks the generator's own question now, per band site.

## To do, in order

1. **Gate and push the branch.** `bash check.sh --update-seed` on exactly what is pushed, commit the seeds and the
   benchmarks' `generated.c` it rewrites, push `backend-ir` (never `master`).
2. **Widen the printer, one generator decision at a time**, each moved onto the high level as a pass, with the whole
   corpus proving it:
   - arithmetic: the overflow check per site, and the proof that drops it for a counter (`never_negative`,
     `counter_counts_up` in the generator) as a high level pass;
   - fractions' literals;
   - calls to the program's own functions, with the generator's names for copies decided on the high level
     (specialisation per call site is a whole-program pass, which is what the high level is for);
   - references: retain and release as operations the walk records, so ownership is on the IR;
   - `assert` and `crash` with their sites and reports;
   - texts, lists and dictionaries.
3. **Move the band decision.** `band_condition` (`generation/generator.spite`) asks `HighBand` instead of the C
   reader once every band site agrees or the high level is stricter only where D556's owner proof is missing; then
   the band's other two questions (does it count references, is it worth banding).
4. **Move the rest of `OverlapFacts`** (2,426 lines reading the C): the overlap groups (`settle_overlap_at`), the row
   conditions (`row_condition`), and the carried values (`CarriedValues.lost_write`). Each is a question about reads
   and writes per object and field, the call graph and loop shape, which the high level carries.
5. **The passes that rewrite the final C**, once the functions they touch are printed from the high level. Each reads
   the text today because the text is the only form the output has:

   | Pass | What it decides | What the IR must carry instead |
   |---|---|---|
   | `TreeShaker` | which functions, types, statics and macros are reachable | the call graph and every use of a type, global and function value |
   | `CountSharing` | which classes' counts no thread shares, so they need no atomics | thread entries, the call graph, where each class is retained, released and made |
   | `NestedLists` | an inner list held only by an outer list's slots is laid out inline | field types and every use of the inner list (a layout pass, D520) |
   | `ConfiguredCopies` | lists filled once are read as constants, per configuration | per field writers and callers, function values |
   | `ClassPools` | a class gets a free list | allocation and free sites per class, thread facts |
   | `FunctionFolder` | identical functions become one | equality of IR functions |
   | `ItemValues` (the splice only) | a marked test becomes a constant | the decision is already on the tree; the splice becomes an IR rewrite |
   | `rewrite_mirrors` (the splice only) | a field mirrored in a local | the solver already works on structure; the splice becomes an IR rewrite |
   | smaller ones in the generator | locks (`state_touching_functions`), `___unguarded` calls, unread waits | per function field writes and receiver escapes, call sites, def-use |

   `StateMachine`, `UnitSplitter` and hot reload rewrite text too, but they are lowering and packaging, not
   decisions: they become the printer's job.
6. **The low level**, and machine code from it (milestone M3's second half, `proposals/own_backend_plan.md`), once
   the printer writes every function.

## How to run it

```
# the compiler a check keeps (or the seed), then the branch's compiler
.spite/spite.exe compiler --check                                    # no output means the sources compile
.spite/spite.exe compiler --build --executable-path=.spite/ir_compiler.exe
COVERAGE_JOBS=8 bash scripts/high_level_coverage.sh .spite/ir_compiler.exe
```

The script must end without `FAILED`, and its summary's `functions differed` must be 0. `bash check.sh
--update-seed` runs the same thing as one of its jobs, and runs every program with its C partly written by the
printer, which is what proves the printer.

## Rules (refused by the hook or `check.sh` otherwise)

- No em dashes anywhere: not the character, not two hyphens between spaces.
- Names in full words, no abbreviations; a comment is only a link to a design or docs section.
- Every failure is loud (D244). A case the walk does not understand is an unknown-effect operation, a case the
  printer does not write leaves the function to the generator, and a question the high level cannot answer says it
  cannot say: never a wrong fact, never a skipped statement. Do not use `assert` as a filter in a function that
  returns nothing unless skipping really is the answer.
- Commits start with one gitmoji, end with `Co-authored-by: <the model you are> <noreply@...>`, and stage explicit
  paths only.
- A reversible decision you make: a row in `design/decisions.md` marked "decided by <model> under D509, to confirm",
  and a line under "To confirm" in `mortaros_missing_decisions.md`.
- Push this branch, never `master`. It merges into `backend-experimental` once the gate is green.
