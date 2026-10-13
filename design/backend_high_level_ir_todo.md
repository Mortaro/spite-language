# To do: IR-1 and IR-2, the high level of the IR (branch `backend-ir`)

For the next model to pick up. Started by Space Bunny Free, stopped by its rate limit; reviewed by Claude Opus 5.5 on
2026-10-10; continued by Space Bunny Free on 2026-10-11 and 12. Read first, in order: `AGENTS.md`, [the proposal and
its review](proposals/high_level_ir.md) (the review at its end overrides the proposal), [what was built and what it
covers](backend_high_level_ir.md), and decisions D558 to D561, D569, D583 to D585 in [decisions.md](decisions.md).

## Where it stands

- The form is built and follows the review, in `compiler/source/backend/high_*.spite`: the whole program in one
  `HighProgram`; blocks with explicit edges; loops and list walks as one operation with a body; abstract objects as
  owners (a receiver, a made object, a field's value, a parameter, a list's items); a new value on every write;
  joins that record keep or drop; an operation of unknown effect wherever the walk does not build something.
- The option is `--high-level-report=<path>`.
- It runs over the whole conformance corpus and the examples with no program stopping: 413 programs, **680 functions
  the walk built, 680 agreed, 0 differ**. What it reaches is in
  [backend_high_level_ir.md](backend_high_level_ir.md#the-coverage-the-corpus-reaches).
- **Calls are not compared yet**: the C writes a call as a call, as an inlined body, a folded copy or not at all, and
  writes one the program never wrote for a conversion to text. Comparing them would measure the generator, so the
  comparison of calls comes back when those decisions move onto the high level.
- **IR-2, the conditional must-write summary, is built** (`HighMustWrite`): what a function surely wrote before it
  answered `true`, before it answered `false`, and before it returned. It is a forward pass over the high level's own
  blocks and joins, so it is a fold of the operations, never a second walk. Nothing reads it yet, so its entry in
  `docs/proofs.md` waits for its consumer.
- `check.sh` runs `scripts/high_level_coverage.sh` as one job (`job_high_level`), and a difference is a red build.
- `bash check.sh --update-seed` was green before IR-2; **run it again before pushing** (see the gate below).

## To do, in order

1. **Gate the branch.** `bash check.sh --update-seed`, then `bash check.sh`, last line starting with `OK`.
   - IR-2 changed the compiler's own C, so the seeds are stale until that run rewrites them.
   - Iterate faster first: build the seed once with `-O1` and run `<seed> compiler --check` from the repository root,
     where no output means the sources compile. Then build this branch's compiler with
     `<seed> compiler --build --executable-path=.spite/ir_compiler.exe` and run it on single programs.
2. **IR-3: a counted `while` as a band site** (D559 steps 2 to 4), the first consumer of the must-write summary and
   the first proof that reads the high level instead of the C. What it needs, and what is ready:
   - a function that fills a scratch on every path but the one that answers `false` is now summarised, so the
     caller-side proof (read only after `true`) has what it reads;
   - a counted `while` is already one loop operation with a body block and `body`, `step` and `exit` edges, so the
     body can be cut into a piece function;
   - the gate the generator asks today is `band_condition`'s `item_only` and `band_counts` on the C reader
     (`compiler/source/generation/generator.spite:37552`). That is the question to answer from the IR instead.
   - a list's items now have an object, so "the passes write different slots" is a question about objects, which is
     what the band check needs.
3. **The low level, and C emitted from the IR for the smallest useful subset, function by function** (milestone
   M3's second half, `proposals/own_backend_plan.md#m3-the-two-level-ir-and-a-first-lowering-d560`). Nothing of this
   is built. The first slice worth trying is the simplest one: a function with no call, no container and no object
   made in it, which is straight-line field reads, field writes and computations, emitted and checked against what
   the generator writes for that function.
4. **Raise coverage further**, from the summary's reasons: a name it does not know (478), a codegen value (180), a
   construction (176, a generator decision), an owner tag of D556 (91), and a getter, a setter or a switch.

## What was already fixed (so it is not looked for again)

- **A value passed to a call whose owner is the frame** (`object_number` `-1` in `note_passed_value`) crashed the walk.
  A plain local or constant points at no abstract object, so it is not looked up as one.
- **A field of an object the walk cannot name** crashed on an owner of `-1`; it is an operation of unknown effect
  naming that. A value number of `-1` is the walk's "no value here" and is left out of reads and writes rather than
  folded.
- **The class a value is of** is the generator's answer (`SpiteString`, a boxed value, a container), asked through
  `HighProgram` rather than a second rule written in the walk.
- **A call through a bare name that names another of the program's classes** is a made object of that class.
- **The C reader did not recognise `___make_into`** as a construction, so it left out every field of a class the C
  constructs that way. It is a construction the design page already names.
- **Writing a field whose type is a reference reads the field first**: what it held is released before the new value
  goes in, so the high level records that read too.
- **A `crash` with a condition that may be false** was ending the block, so every statement after it was skipped.
  Only a `crash true` ends a block. (This one was skipping steps, which is a bug under D244.)
- **A list literal, an object literal and a codegen value** were not built at all, which is why a third of the
  uncovered functions were uncovered. A text with holes was never missing: the parser lowers it to a binary join.
- **A parameter had no abstract object**, so every field read or written through one had no owner. It has one now,
  which is also what a per-parameter must-write summary needs.
- **A list's items had no abstract object**, so no field of an item could be named, which is what a band site is made
  of. They have one now, and the comparison names a field of an item the way the C names it.
- **A list walk named the element's function as a call**, while the C names the list's template; the walk records no
  call for it.
- **Every reason the comparison does not cover a function names the function**, and the comparison decides coverage
  one kind of fact at a time (see the design page).
- **A call the program declared is compared by the name the program gave it**, not by a name the generator gave a
  copy of it (`___held_0`, `___for_0_Integer`), and not by a function the compiler generated.
- **`compare_fields` used to assert** when the C reader named neither a readable nor a writable field (both its keys
  `raw`, say). That crashed the compiler on the user's program, which the design forbids; it now records the reason
  for each kind and compares neither.

## How to run it

```
# build the seed once (clang from Visual Studio, or set CC), then the branch's compiler
<seed> compiler --check                                   # no output means the sources compile
<seed> compiler --build --executable-path=.spite/ir_compiler.exe
COVERAGE_JOBS=8 bash scripts/high_level_coverage.sh .spite/ir_compiler.exe
```

The last line of that script's summary must say `functions differed 0`. `bash check.sh --update-seed` runs the same
thing as one of its jobs and is the gate.

## Rules (refused by the hook or `check.sh` otherwise)

- No em dashes anywhere: not the character, not two hyphens between spaces (git's double-hyphen separator between
  spaces counts too).
- Names in full words, no abbreviations.
- Every failure is loud (D244). A case the walk does not understand is an unknown-effect operation or a crash
  naming its cause, never a wrong fact, and never a skipped statement.
- Commits:
  - start with one gitmoji;
  - end with `Co-authored-by: <the model you are> <noreply@...>`;
  - stage explicit paths only.
- A reversible decision you make:
  - append a row to `design/decisions.md` with the next free number, marked "decided by <model> under D509, to
    confirm";
  - list it under "To confirm" in `mortaros_missing_decisions.md`.
- Push this branch, never `master`. It merges into `backend-experimental` once the gate is green.