# To do: C written only from the desugared IR (branch `backend-ir`, D587)

For the next model to pick up. Read first: `AGENTS.md`, [the spec and its contracts](proposals/desugared_high_ir.md), and
decisions D586 to D589 in [decisions.md](decisions.md).

**Done means every line of C comes from the IR** (Mortaro, 2026-10-14) and the generator and every reader of C text
are deleted. Until then the generator is the fallback being emptied.

## Where it stands (2026-10-15)

- The pipeline is built: `compiler/source/intermediate/` (namespace `Intermediate`). `Lowering` and `Instances`
  desugar and monomorphise the typed tree; `Passes` runs eight passes in the spec's order (inlining, proofs,
  ownership, placement, arrangement, concurrency, folding, reach); `Printer` and `printing/` write the whole C
  program; `Pipeline` runs the three.
- It runs only in a compiler built with `var writes_from_intermediate = true` in `compiler/compiler.spite`
  (never committed on); the generator still writes every program's C in a normal build. Master carries all of it
  dormant (release 0.0.4).
- `--high-ir=<folder>` (the `Build` setting `high_ir`) writes the program's IR as text, one `.ir` per class.
- **Corpus through the pipeline** (`--debug-memory`, every conformance program and example, 372 with an expected
  output): round 12 passed **270**. The rest: about 38 refused by name (mostly reflection corners and the REPL),
  35 with a different output, 13 double frees (rows and class-object getters), a few C compile errors.
- **The compiler builds itself through the pipeline** as far as the C compile, which stopped on one error (fixed
  since, not yet re-run).
- **Benchmark parity**: every one of the 97 cases compiles through the pipeline; about 80 function rows still do
  more work than the generator's C (see the passes worker's list below).

## How to run a round

```
# in a checkout of backend-ir, with writes_from_intermediate set to true (do not commit it)
.spite/spite.exe compiler --build --executable-path=.spite/pipeline.exe      # about 25 minutes
.spite/pipeline.exe <program folder> --debug-memory                             # one program
.spite/pipeline.exe <program folder> --debug-memory --c-source --c-path=x.c     # its C
```

A refusal ("the C cannot be written from the high IR") names the function and the construct. A program that
hangs is a bug (three concurrency programs did; the cause was waiting primitives not marked, fixed since).

## Missing, by piece

**Lowering** (`lowering.spite`, `instances.spite`, `expression_lowering.spite`, `statement_lowering.spite`,
`reflection_lowering.spite`):
- a call on a `type` no class of the program fits (`Console.debug` over `Debuggable`, 11 programs);
- `to_string` on `Anything` in REPL builds (9), and on a union member without one;
- the entry class's own function list, a class passed as its `Spite.Class` object, `memory` and `attributes`
  reflection corners, the size of a C struct, a `type` or function value passed straight to C;
- the debug support classes `Spite_Debug__<T>` for automatic `to_debug`;
- parity: dictionary literals read only by literal keys, fused template chains, the number-keyed dictionary form,
  words inflected while compiling, a loop over a list of different classes run at once;
- remote REPL and hot reload builds (waiting on the printer's reload design, now in).

**Passes** (`passes.spite` and its folders):
- rows' fields are lent (needs the lowering's `Layout.kind = "row"`, now set);
- parity: loop-carried objects reassigned each pass still on the heap, some `get_at(i - 1)` proofs, the locked
  loop forms for reading and never-changing singletons, configured copies not appearing, "objects made for their
  owner are told apart", rows' weight rule, retains around held parameters;
- folding keyed on exact class identity except where a function names no layout (in progress).

**Printer** (`printer.spite`, `printing/`, `pipeline.spite`):
- hot reload stage 3 (migrations in the reload library, rebuilding functions that touch a changed layout); until
  then a reload that changes a class's attributes is refused by name;
- the remote loop and breakpoints;
- parity: `___allocate_constructed`, `___make_in`, deep copies on demand only.

**Then the switch-over**: when the whole corpus, the examples, the documentation programs, the diagnostics, the
benchmark cases and every special job of `check.sh` pass with `writes_from_intermediate` on, and the benchmarks are
no slower, set it on for good, delete the generator's emission and every reader of C text (`overlap_facts`,
`tree_shaker`, `count_sharing`, `class_pools`, `function_folder`, `configured_copies`, `nested_lists`, the C
splitting of `hot_reload_library`, `state_machine`), and the old `backend/high_*` files.

## Rules

- No em dashes; full words; a comment is only a link to a markdown section.
- Every failure is loud (D244): a construct not handled is refused by name, never printed wrong.
- A name prefix may not stand in for a namespace (D588): a family of classes is a folder.
- Commits start with a gitmoji, end with `Co-authored-by: <model> <noreply@...>`, stage explicit paths.
- Push `backend-ir`; master only after `bash check.sh --update-seed` passes on exactly what is pushed.
