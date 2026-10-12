# To do: IR-1, the high level of the IR (branch `backend-ir`)

For the next model to pick up. Started by Space Bunny Free, stopped by its rate limit; reviewed by Claude Opus 5.5 on
2026-10-10; continued by Space Bunny Free on 2026-10-11. Read first, in order: `AGENTS.md`, [the proposal and its
review](proposals/high_level_ir.md) (the review at its end overrides the proposal), [what was built and what it
covers](backend_high_level_ir.md), and decisions D558 to D561, D569 and D583 in [decisions.md](decisions.md).

## Where it stands

- The form is built and follows the review, in `compiler/source/backend/high_*.spite` (about 1,700 lines):
  the whole program in one `HighProgram`; blocks with explicit edges; loops and list walks as one operation with a
  body; abstract objects as owners; a new value on every write; joins that record keep or drop; an operation of
  unknown effect wherever the walk does not build something.
- The compiler sources compile with no errors, and so does the compiler built from them.
- The option is `--high-level-report=<path>`. It builds the high level and writes the comparison with
  `generation/overlap_facts.spite`.
- It runs over the whole conformance corpus and the examples with no program stopping: 413 programs, 541 functions
  the walk built, **541 agreed and 0 differ**. What it reaches is in
  [backend_high_level_ir.md](backend_high_level_ir.md#the-coverage-the-corpus-reaches).
- Calls are not compared yet: the C writes a call as a call, as an inlined body, a folded copy or not at all, and
  writes one the program never wrote for a conversion to text. Comparing them would measure the generator, so the
  comparison of calls comes back when those decisions move onto the high level.
- `check.sh` runs `scripts/high_level_coverage.sh` as one job (`job_high_level`) over the whole corpus, and a
  difference is a red build.
- Asking for the report is a setting of `Build`, and every `Build` setting is a field of every program's C, so the
  option moved all 102 benchmark `generated.c` by one line and the error `diagnostics/unknown_compiler_flag` holds.
  Both were regenerated in the same commit, as `scripts/cases/extract.sh` and the diagnostic's own text ask.
- Nothing in `check.sh` runs the comparison yet.

## To do, in order

1. **Raise coverage, case by case**, from the summary's reasons, most often first:
   - a call on a value whose class the walk cannot name (17 functions): a value that is a `List` item, a value a
     call returned, a value narrowed by an `assert`;
   - a call to a member a library class does not declare (20 functions): a foreign function reached through a
     `DynamicLibrary`, and a generated accessor;
   - a call to a member of a class of the program that does not declare it (8 functions): a call the generator
     wrote for a reflection or an attributes read;
   - a container (the `c:` key of a list's items), which is one whole class of functions;
   - the owner tags of D556, which decide whether two keys with the same innermost tag touch;
   - a getter, a setter, a switch, a template, a codegen value used as a type: one function each.
2. **Calls come back** when the decisions that rename or drop them (inlining, folding, the conversion to text a
   program never wrote) move onto the high level. Until then the report says so with the count.
3. **Gate:** `bash check.sh --update-seed`, then `bash check.sh`, last line starting with `OK`.
   - Iterate faster first: build the seed once with `-O1` and run `<seed> compiler --check` from the repository
     root, where no output means it compiled.
   - Then build this branch's compiler with
     `<seed> compiler --build --executable-path=.spite/ir_compiler.exe` and run it on single programs.

Then IR-2 (what a function surely wrote when it answered `true`) and IR-3 (a counted `while` as a band site, D559),
as the proposal's slices say.

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
- **A list walk named the element's function as a call**, while the C names the list's template; the walk records no
  call for it.
- **Every reason the comparison does not cover a function names the function**, and the comparison decides coverage
  one kind of fact at a time (see the design page).
- **A call the program declared is compared by the name the program gave it**, not by a name the generator gave a
  copy of it (`___held_0`, `___for_0_Integer`), and not by a function the compiler generated.

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