# The high level of the backend's IR (proposal, for Mortaro)

What the first IR level must be so that it unblocks the compiler's own optimisations, not only its code generation,
and how to build it in slices that each leave `bash check.sh` green.

## What it is for

[own_backend.md](own_backend.md) names the first signal for backend work: the proofs no longer read generated C.
Today several of them do, so an own backend with no C text would lose them. Building the IR first is worth doing
anyway, because it makes every proof cheaper and per use (D520). The two first users are decided:

1. **what a function surely wrote when it answered `true`**, a conditional must-write summary, which is step 1 of
   D559's four steps in [handoff.md](../handoff.md).
2. **whether the passes of a loop write different slots**, so a counted `while` can be cut into band pieces
   (D559 steps 2 to 4).

The C generator stays fed from the typed tree for now. This proposal is only about the level above the C: the form
the facts live on. Emitting C from it is the second half of milestone M3 in [own_backend_plan.md](own_backend_plan.md).

## What it must carry

From Mortaro, 2026-10-09:

1. The high level serves the compiler's optimisations, not only code generation.
2. Control flow stays explicit.
3. A loop is one operation, not a loop of instructions (a counted `while` included), so a loop's body can be cut
   into band pieces.
4. Each value carries what it reads and what it writes, per field and per owning object.
5. The facts live on the IR, so the analyses that read generated C today move onto it instead of being written
   twice.

## What exists today, and what each part costs us

- **The typed tree.** One class per node kind, held in a `union`, every field mutable: `Syntax.Statements.Statement`
  has fifteen cases and `Syntax.Expressions.Expression` eighteen. A function's body is
  `Analysis.FunctionInfo.body: List<Statement>`. A counted `while` is the same `WhileStatement` as any other; there
  is no `for` loop in the language.
- **The emitting walk.** `generation/generator.spite` walks statements in `generate_statement_list` (line 35789)
  and expressions in `generate_expression` (line 27296), and every decision the compiler makes is made there:
  whether a value is a C local or a heap object (`store_declaration`, line 28261), placement in a stack frame
  (`placement.spite`), lock sides, pooling, mirroring, specialisation, hot-reload slots. Recognising a counted loop
  is `generate_counted_loop` (line 43321).
- **The C readers.** At the end, `CodeBuilder.render` (line 322) hands the generated text to passes that parse it:
  the tree shaker, `CountSharing` (line 363), `NestedLists` (line 371), `ConfiguredCopies` (line 373), `ClassPools`
  (line 390), `FunctionFolder` (line 393), and `ItemValues` is applied line by line (line 276).
- **The overlap facts.** `generation/overlap_facts.spite` (2,426 lines) reads the C of each function as lines
  (`bodies: Dictionary<String, List<String>>`, fed by `read_program` from `generator.spite` lines 7598, 36425 and
  36552) and answers, per function, an `OverlapFact` (`generation/overlap_fact.spite`): `reads`, `writes`, `calls`,
  `item_calls`, `loops`, `container`, `mutates`. The keys are C text: a field is `self->count_`, a list item is
  `({temporary})->items_)[`, and anything it cannot place is `raw` or `unknown`. Its answers drive the row and band
  conditions, the item values and the carried values.
- **The closest thing that exists.** `MirrorStep` and `MirrorFunction`, recorded by a second, quiet walk over the
  same typed tree (`mirror_record`, line 56868) and solved by `MirrorSolver` with a merge. It already walks `if`,
  loops, calls, field writes, slots and returns, and it already knows each call's effects. What it is not is a form
  the other analyses can use: it merges everything (it is flow-insensitive), its facts are keyed by C name, and it
  answers one question only.
- **What a call already knows.** `generation/call_effects.spite` summarises each function's effects while compiling
  (`release`, `field:#Name`, `grow:`, `store:`, `shrink:`, `parameter:`). The IR must carry these rather than
  recompute them.

## The shape

One function is one `HighFunction`: a list of blocks, each block a list of operations, with explicit edges between
them (a fallthrough edge, a taken and a dropped edge for an `if`, a body edge, a step edge and an exit edge for a
loop). Nothing is a block of text, and nothing is a name that means something only at one place in a file.

- **Operations are Spite's operations**: read a field, write a field, call a function, walk a list, assert, crash,
  return, jump. A list walk is one operation with a body, whether it is `list.each_<function>()` or a counted
  `while` over a list's items, and it says which it is.
- **Values are `HighValue`**: a type, an owner and a name. A field value's owner is the expression that holds the
  object; a slot value's owner is the list; a local's owner is the frame.
- **The facts live on the operations and the values, not in a side table**: every operation records what it reads
  and writes (owner, field), which calls it makes, whether it can end the block, and whether it can write raw
  memory. A per function summary is a fold over the operations with the join rules, so the summary is never a
  separate thing that can disagree with the operations it came from.

The names must say what they are: an owner is an `IrOwner`, not a string that may or may not be a field name.

## The forks, for Mortaro to call

1. **How a value is identified.** Named values with explicit joins, or a new value on every write (the form a
   must-write question wants: at a join, the slots whose value is the same on both sides are the ones the function
   surely wrote). New values on every write cost the IR a join operation and, later, moves at the joins; named
   values cost every analysis a merge. **Recommendation: a new value on every write**, because requirement 4 and the
   first user are both questions about versions across a join, and because a loop's passes writing different slots
   (the second user) is the same question one level up. Ownership is the part to watch: a join of two references
   needs a keep or a drop recorded on the join, or the low level will not be able to lower it.
2. **One walk or two.** The generator's walk knows decisions the typed tree does not (a frame, a pool, a dropped
   write-back), so an IR built by its own walk can differ from the C that walk produced. Two answers: keep both and
   make the C facts an oracle that must agree, or make the IR the only walk now. **Recommendation: the IR walk
   first, with the C facts kept as the oracle** until the IR gives the same answers on the whole corpus, then the
   C reading goes. Disagreement is a compile error naming both answers, never a wrong optimisation (D244).
3. **Which facts move first.** **Recommendation: the per function reads, writes, calls and loops**, which is what
   `overlap_facts.spite` answers today, computed from the IR for every function before a byte of C exists. That is a
   migration with an oracle, and the must-write summary then lands on the same facts.
4. **When a function's IR is built.** **Recommendation: on demand, per function**, when a fact about it is asked
   for, and dropped once the facts are folded, so a program that needs no fact about a function builds none of it.

## The slices

- **IR-1, the form and the walk.** `bootstrap/source/backend/high_ir.spite` and the walk that records it for the
  statements the corpus needs: declarations, assignments, field reads and writes, calls to the program's own
  functions, `if`, `while` (plain and counted), `return`, `assert` and `crash`. Explicit control flow, loops as
  operations, values with their reads and writes per field per owner. Every function the IR covers must give the
  facts the C gives today; a function where the two differ is a compile error that names the field and both
  answers, and the C path stays the one in use. Acceptance: the whole conformance corpus and examples, with the
  oracle on, and `bash check.sh` green.
- **IR-2, what a function surely wrote.** The conditional must-write summary on the facts IR-1 carries: a function
  that fills a scratch on every path but the one that answers `false`, and a caller that reads it only after a
  `true` answer. Acceptance: a conformance program, and its entry in [docs/proofs.md](../../docs/proofs.md).
- **IR-3, a counted loop as a band site.** Recognise the counted `while` as a loop operation, cut its body into a
  piece function, clone the scratch on the submitting thread, and check once per loop that the passes write
  different slots (D559 steps 2 to 4).
- **Later, the plan's M3 second half.** The low level, and C emitted from the IR for the smallest useful subset,
  function by function.

## What it costs

Nothing at run time: the IR is a compile-time structure, and no fact it carries is checked again by the program.
Tree shaking: a program that needs no fact about a function carries neither that function's IR nor the passes that
fold it, the same way a program that never writes an executable carries none of the M0 writer (D567). What it costs
is compile time and memory per function, which is what `bash check.sh` and the benchmarks will show.

## The open questions

1. The four forks above.
2. Whether the C facts stay as a permanent oracle for `--debug-memory` builds, or are deleted once the corpus
   agrees. Deleting them is the honest end: an analysis that reads C cannot outlive the C.
## Review (Mortaro and Claude Opus 5.5, 2026-10-09): what to change before building

The reading of today's compiler above is right, and the forks are the right questions. The shape is not yet what
Spite needs: as written, the IR is a sheet of facts beside the C path, while the generator keeps every decision.
What we want is that the IR is the program the compiler transforms.

1. **Decisions move onto the IR; the generator only prints.** Whether a value is a C local or a heap object,
   placement in a frame, pooling, a dropped write-back, specialisation: each becomes a pass that rewrites the high
   level or lowers it, and the generator prints the low level. While the generator still decides them, the IR
   describes the program before the decisions and the C is the program after them, so facts taken from the IR are
   about a different program from the one that runs. The oracle exists only to cover that gap, and it is a third
   walk over the same tree (after the typed tree and the mirror walk): the "written twice" that requirement 5 rules
   out.
2. **The oracle is a check of the compiler, never an error in a user's program.** During the move, comparing the
   IR's answers with the C reading belongs in `check.sh` (or a debug build of the compiler). A user's program must
   never fail to compile because two analyses inside the compiler disagree: that is a compiler bug. Each comparison
   is deleted the day its decision has moved.
3. **Objects have an identity.** An owner that is "the expression that holds the object" makes one object reached
   through two expressions look like two objects. Both first users ("the passes write different slots", and the
   per-object facts already built) are questions about which object. Give the high level abstract objects: one per
   place the program creates objects, and one per list for its items, with what may point to what. Versioning
   values (fork 1) is about the fields and slots of those objects, not only about locals.
4. **The whole program, kept until lowering.** Not built per function on demand and dropped once folded. The levers
   that beat expert C are whole-program: working out closed computations while compiling, specialising per call
   site, deciding parallelism across calls, choosing a data layout for the whole program. Facts are solved across
   the call graph. Tree shaking does not apply here: the IR exists only while compiling and ships nothing.
5. **Layout stays abstract at the high level (representation per use, D520).** A list of objects is still a logical
   collection there, so a later pass can choose columns, archetypes, chunks that fit the cache, or the device. A
   value that carries only "a type" with its C layout already fixed leaves the layout research nowhere to land.
6. **D559 is the first user, not the shape.** Keep IR-2 and IR-3 as the first proofs that the form works, but design
   the form for every pass in `design/naive_programs_pairs.md`.

The forks, answered:

1. A new value on every write: yes, over the fields and slots of abstract objects, with ownership (a keep or a
   drop) recorded at each join.
2. One walk as the goal. The migration moves one decision at a time out of the generator into a pass, with the C
   reading kept as a `check.sh` comparison until that decision has moved.
3. The per function reads, writes, calls and loops first: yes, keyed by abstract object and field from the start.
4. The whole program, kept until lowering.

The M3 paragraph added to [own_backend_plan.md](own_backend_plan.md) ("the facts live on the IR, so the analyses
move onto it") changes the same way: the analyses *and the decisions they drive* move onto it.
