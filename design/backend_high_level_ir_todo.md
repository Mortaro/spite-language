# To do: IR-1, the high level of the IR (branch `backend-ir`)

For the next model to pick up. Started by Space Bunny Free, stopped by its rate limit; reviewed by Claude Opus 5.5 on
2026-10-10. Read first, in order: `AGENTS.md`, [the proposal and its review](proposals/high_level_ir.md) (the review
at its end overrides the proposal), [what was built](backend_high_level_ir.md), and decisions D558 to D561 and D569
in [decisions.md](decisions.md).

## Where it stands

- The form is built and follows the review, in `bootstrap/source/backend/high_*.spite` (about 1,650 lines):
  - the whole program in one `HighProgram`;
  - blocks with explicit edges;
  - loops and list walks as one operation with a body;
  - abstract objects as owners;
  - a new value on every write;
  - joins that record keep or drop;
  - an operation of unknown effect wherever the walk does not build something.
- The compiler sources compile with no errors. The option `--high-ir-report=<path>` builds the high level and writes
  the comparison with `generation/overlap_facts.spite` (`high_check.spite`).
- It does not work yet. Run on three programs with this branch's compiler:
  - **`conformance/stage1/arithmetic` and `conformance/stage6/class_pools`:** they stop at the `crash` in
    `note_passed_value` (`high_walk.spite:516`) with `object_number=-1`: a value passed to a call whose owner is the
    frame, not an object.
  - **`conformance/stage6/count_bound_proofs`:** it covers 0 functions: "an operation of unknown effect: a call on a
    value this walk does not know".
- Nothing in `check.sh` runs the comparison yet.

## To do, in order

0. **Merge `origin/master` into this branch** (it is behind by many commits).
   - Renumber the branch's D567 row (the Windows executable writer of M0) to the next free number, since master
     gave D567 to the hand-copy rule.
   - Keep master's `check.sh`: since D574 it starts from the last compiler that passed and releases on push.
1. **Fix the crash in `note_passed_value`.** A value of the frame (owner `-1`) passed to a call is not an object's
   value; it is recorded as the local it is, not looked up as an object.
2. **Resolve calls on values.** Find the receiver's class from the typed tree, and the abstract object it is from the
   value's owner. This is why `count_bound_proofs` covers nothing.
3. **Rename the option** `high_ir_report` to `high_level_report` in `bootstrap/bootstrap.spite` and
   `library/build.spite`: names are full words.
4. **Run the report over the whole conformance corpus and the examples,** and write the coverage (functions covered,
   agreed, differed) into [backend_high_level_ir.md](backend_high_level_ir.md).
   - Raise coverage case by case.
   - A difference is a bug in the walk or in the C reader: fix the one that is wrong and never suppress it.
5. **Add a `check.sh` job** that runs the report over the corpus and fails on any difference, naming the function,
   the field and both answers.
   - It is a check of the compiler, never an error in a user's program.
   - It must not change any generated C: the benchmarks' `generated.c` stay byte-identical.
6. **Fix the design page:** [backend_high_level_ir.md](backend_high_level_ir.md) still says a program that asks no
   fact about a function does not build that function's high level. D569 keeps the whole program's high level in
   memory and caches it on disk keyed by each function's source and what it depends on, so that sentence goes.
7. **Gate:** `bash check.sh --update-seed`, then `bash check.sh`, last line starting with `OK`.
   - Iterate faster first: build the seed once with `-O2` and run `<seed> bootstrap --check` from the repository
     root, where no output means it compiled.
   - Then build this branch's compiler with `<seed> bootstrap --build --executable-path=.spite/ir_compiler.exe` and
     run it on single programs.

Then IR-2 (what a function surely wrote when it answered `true`) and IR-3 (a counted `while` as a band site, D559),
as the proposal's slices say.

## Rules (refused by the hook or `check.sh` otherwise)

- No em dashes anywhere: not the character, not two hyphens between spaces (git's double-hyphen separator between spaces counts too).
- Names in full words, no abbreviations.
- Every failure is loud (D244). A case the walk does not understand is an unknown-effect operation or a crash
  naming its cause, never a wrong fact.
- Commits:
  - start with one gitmoji;
  - end with `Co-authored-by: <the model you are> <noreply@...>`;
  - stage explicit paths only.
- A reversible decision you make:
  - append a row to `design/decisions.md` with the next free number, marked "decided by <model> under D509, to
    confirm";
  - list it under "To confirm" in `mortaros_missing_decisions.md`.
- Push this branch, never `master`. It merges into `backend-experimental` once the gate is green.
