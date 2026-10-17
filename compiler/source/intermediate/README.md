# The high IR: a guide for working on it

This is the guide to `compiler/source/intermediate/` (namespace `Intermediate`), the compiler's high-level intermediate
representation and everything that makes it, rewrites it and prints C from it. The normative specification of the form,
the desugaring, the intrinsics, the pass order and the contracts is
[design/proposals/desugared_high_ir.md](../../../design/proposals/desugared_high_ir.md); this page says how the code
that exists today is arranged, how to run it, and where it stands. When this page and the specification disagree, the
specification wins and the difference is a bug to report.

Two things in the code read oddly until you know them. A comment in the compiler's sources is only a link to a heading
of a markdown page, so a rule is documented here, in the specification or in `docs/`, never in a comment. And a bare
`assert condition` at the top of a function is a guard: when the condition is false the function returns its default
(`false`, `null`, `0`) at once, so the lines below it are the case the guard lets through.

- [What the IR is and why](#what-the-ir-is-and-why)
- [The pipeline](#the-pipeline)
- [The form](#the-form)
- [The passes](#the-passes)
- [The printer](#the-printer)
- [How to work on it](#how-to-work-on-it)
- [Where it stands](#where-it-stands)
- [The older walk in compiler/source/backend](#the-older-walk-in-compilersourcebackend)

## What the IR is and why

The IR is the whole program after the compiler has understood it and before any C exists. It is **desugared**: every
operator, index, interpolation, `switch` and `assert` is already the call or the branch it means, so a pass never needs
a case per syntax. It is **monomorphised**: every template, codegen value and specialisation is resolved, so every
function, class and type in it is concrete and nothing in it is generic. Every line of the program's C is written from
it and from nothing else (D587), so no optimisation reads C text and no decision is made while printing.

It has few kinds of operation on purpose (D587, D599). **Nothing that is a function in the end becomes an operation of
its own**: what the source can say as a call of an existing function is lowered as that call, and a pass decides what
the call becomes (a read of a list item is `get_at`, a write is `set_at`, never an intrinsic that reaches into the
list's fields). A new intrinsic is only for what has no Spite function under it. The simpler the IR, the more the
passes can see to be the same and collapse, and the less a pass needs special cases; every operation kind added is a
case that every pass must learn.

It is held only while compiling and ships nothing, so it keeps Spite at zero runtime: whatever a pass decides here is
decided once, in the compiler, and the program carries the result as plain C.

## The pipeline

```
typed tree (Analysis) -> Lowering and Instances -> Passes -> Printer -> C
```

| Piece | Files | May decide | May not decide |
|---|---|---|---|
| Lowering, Instances | `lowering.spite`, `instances.spite`, `expression_lowering.spite`, `statement_lowering.spite`, `reflection_lowering.spite`, `emitter.spite` | what the program says: which concrete functions and classes are reached, which call each operator is, where a check can fail (its site) | anything about memory, layout, concurrency or which check to drop: it writes no `retain`, `release`, copy or frame |
| Passes | `passes.spite` and the folders `inlining/`, `proofs/`, `ownership/`, `placement/`, `arrangement/`, `concurrency/`, `facts/`, with `folding.spite` and `reach.spite` | everything the generator used to decide, each as a rewrite of the IR | reading or writing C, or changing what the program does |
| Printer | `printer.spite`, `printing/` | nothing | whether a check, count or copy exists: it prints what the passes left, and refuses by name what it cannot print |

`Pipeline` (`pipeline.spite`) runs the three: `prepare()` lowers, copies the settings and facts the generator holds
into the `Lowered` (debug memory, hot reload, threads, foreign libraries, enums and symbols), and runs `Passes`;
`run()` then prints. Lowering asks the front end's generator to resolve a name, a type, a codegen value or a question
folded while compiling, with the generator's own context set for the function it lowers, so each decision is made once
by the same code and the IR holds only its answer.

## The form

All of it is a handful of small classes, the contract between the pieces. A piece may add a field to one of them and
never renames or removes one.

| Class | What it is |
|---|---|
| `Lowered` | the program: `layouts`, `concrete_functions`, `sites`, the `entry` function's C name, the build settings (`hot_reload`, `debug_memory`, `makes_threads`, and the others the passes and the printer ask) and the foreign-library and symbol tables |
| `Layout` | a concrete class: its C name, its `stored_fields` and what the passes decided about its storage |
| `Field` | a stored attribute: name, type, and the flags `atomic` and `changes` |
| `Function` | a concrete function: C name, `owner` (the layout's C name), `parameters`, `answer`, `blocks`, `values`, and the flags passes and the printer read |
| `Parameter` | name, type, the value that holds it, and its `ownership` |
| `Block` | `parameters` (the phis), `operations`, one `terminator` |
| `Value` | one SSA value: its number, type, name and where it came from |
| `Operation` | one step that makes at most one value |
| `Terminator` | how a block ends |
| `Target` | a block to go to with the values it receives, and an optional `tag` (a switch case) |
| `Site` | where a crash or check reports from: path, line, owner, function, source text |

`Local`, `Incoming`, `Condition`, `ForeignStruct` and `ListFunction` are lowering's own bookkeeping and never reach a pass.

**Layout.** `kind` is `class` (the default), `row`, `union` or `tagged union`; `is_value` and `is_singleton` mark value
classes and singletons. `arrangement` is how the passes decided to store it: `object` (the default), `slots` (a list
held only by another list lives in its slot) or `static` (a singleton that holds nothing is a static object). Other
fields hold the decisions of later passes: `pooled`, `atomic_counts`, `thread_form`, `copy_with_table`, and the hot
reload facts (`hot`, `fits`, `migrating`, `changing`, `storage_change`, `renamed_from`). A `row` is the borrowed-items
object of [a row](../../../docs/memory.md#a-row-of-borrowed-items-for-one-call); one that escapes is turned into a
`class` by the ownership pass and `escape_reason` says why.

**Function.** A function is SSA: `values` is the list of its values, a local that differs by path arrives as a block
parameter, and a `while` carries every local its body assigns as the parameters of its test block. `receiver` is the
number of the receiver's value (or `-1`); `parameters[i].value` is the parameter's. `intrinsic` is empty for a function
with a body, the intrinsic's name for one without, and `supplied` for a library function whose body the compiler
writes. The other fields mark what a pass found: `specialised_from`, `not_lowered` (the reason the body could not be
lowered, which the printer refuses by name), `singleton_of`, `locking`, `resumable`, `waits`, `waiting_kind`,
`lends_answer`, `slotted`, `folded_into`, `filled_by`. `made_for` names what a function the lowering wrote for the passes is for; `started` marks `<callee>___started(receiver, site)`, written for every function of the program's own classes that takes no arguments and answers nothing, in a program that sleeps and starts waits: it builds a `Concurrent` over the receiver's function and hands it to `WaitsInFlight<Nothing>._keep(handle, site)`, and the concurrency pass rewrites a call marked `started` into a call of it.

**Operation kinds.** There are eight, and every pass handles all of them.

| Kind | What it is |
|---|---|
| `constant` | a literal of a concrete type in `literal`; also names a function or a layout where the operation says so |
| `call` | a call of the concrete function named in `callee`, an intrinsic included, with `operands` in order, the receiver first |
| `call value` | a call through a function value: the first operand is the function value |
| `load` | field `field` of the object in `operands[0]` |
| `store` | `operands` are the object then the value; the field is in `field`. A store consumes the value, and the printer releases what the field held before it |
| `make` | a new object of `layout`, not yet constructed; the constructor is a `call` right after it |
| `retain`, `release` | a count up or down, inserted only by the ownership pass |

A check that can fail is not an operation kind in the code: it is the intrinsic `call` that can fail (`Integer.sum`,
`Integer.to_byte`, `Nullable.value`) carrying a `site` (an index into `Lowered.sites`), and a `crash` is a `halt`
terminator with a site. A pass that proves a check never fails sets the operation's `form` to `unchecked`.

An operation also carries `spite_type`, `line`, `kept_because` (why a pass left it), and `target`, a second name some
operations need (the bound function of `Function.bind`, the maker of `Singleton.peek`).

**The `form` field.** A pass chooses how an operation is printed by setting `form`, never by changing its kind or
callee. The forms in use are `unchecked`, `no_overflow`, `nonzero`, `exact`, `unguarded`, `wrapping`, `skipping`,
`borrowed`, `moved`, `frame`, `in_place`, `table`, `dropped_default`, `overlap`, `started`, `locked`, and the atomic
group (`atomic`, `atomics`, `atomic_sum`, `atomic_subtract`, `atomic_fused`). Each is set by a pass (the section of that pass
in [The passes](#the-passes) names the ones it sets); the printer reads it and decides nothing else.

**Terminators.** `jump` (one target with its arguments), `branch` (operand is a Boolean; the first target is taken when
true), `switch` (on a tag, with one tagged target per case, a union's member class or an enum's value), `return`
(optional operand and `site`), `halt` (a `crash` that always halts, with its site and the operands it shows). A block
with kind `none` is not yet ended, which only lowering sees.

**Intrinsics.** A function with no body whose meaning the printer knows, named `<Type>.<operation>`: the numbers
(`Integer.sum`, `Integer.less_than`, `Integer.to_byte`), `Boolean.not`, `String.join`, `Nullable.has`,
`Nullable.value`, `Nullable.wrap`, `Union.tag`, `Union.value`, `Union.wrap`, `Object.deep_copy`, `Object.allocator`,
`Function.bind`, `Reflection.memory`, `Foreign.call`, `Arguments.get`, `Singleton.peek` and the rest. The printer's
`printing/intrinsics.spite` holds their C; `facts/intrinsics.spite` is how a pass asks what a callee means. The list is
closed by the D599 rule above.

**Sites.** `Lowered.sites` is the table every report prints from, so a crash message says the same thing wherever the
check ends up (inlined, folded, moved). An operation's or terminator's `site` is an index; the printer refuses an
index the table does not hold.

### Reading `--high-ir`

`--high-ir=<folder>` writes one `<c name>.ir` per layout (`text.spite` is the writer). A short real example, the
program below run with `spite counter --high-ir=out`:

```
var console = Console()
var total = 0

func counter() {
    var index = 0
    while index < 3 {
        add(index)
        index = index + 1
    }
    console.print(total)
}

func add(amount: Integer) {
    total = total + amount
}
```

gives `Counter.ir` (shortened):

```
class Counter (Counter), arranged as object
    field console: Console
    field total: Integer

constructor Counter_Counter(v0: receiver): nothing
    block 0():
        v1: Integer = constant 0
        jump  block 1(v1)
    block 1(v2):
        v3: Integer = constant 3
        v4: Boolean = call Integer.less_than(v2, v3)
        branch v4 block 2(), block 3()
    block 2():
        call Counter_add(v0, v2)
        v5: Integer = constant 1
        v6: Integer = call Integer.sum(v2, v5) [unchecked] site 50
        jump  block 1(v6)
    block 3():
        v7: Console = load v0.console
        ...
        release v8
        return

function Counter_add(v0: receiver, v1 amount: owned Integer): nothing
    block 0():
        v2: Integer = load v0.total
        v3: Integer = call Integer.sum(v2, v1) site 67
        store total v0, v3
        return
```

How to read it: `v<n>` is value `n` of the function, with its type after the colon. `block 1(v2)` is a block whose
parameter is `v2`: the loop counter, which arrives as `v1` from block 0 and as `v6` from block 2 (every `jump` lists
the values it passes). `[unchecked]` is the `form` a proof set: the loop bound proves `index + 1` fits, so the overflow
check is gone; `Counter_add`'s `Integer.sum` keeps its `site 67` because nothing proves it. `owned` after a parameter is
its ownership. `release v8` and `[frame]` on a `make` are what the ownership and placement passes added. The text is
written after the passes, so it is what the printer sees.

## The passes

`Passes.run` runs these in this order, then `tidy` (removes unused values and unreachable blocks). Each pass is a class
with `run(lowered)` that rewrites the `Lowered` in place, and each rebuilds the facts it needs from `facts/` (a rewrite
makes the earlier facts stale). Hot reload, the REPL and development builds skip the passes that would remove what
they inspect (the guards at the top of each `run`). The docs for what a user can observe are in
[docs/optimizations.md](../../../docs/optimizations.md) and [docs/proofs.md](../../../docs/proofs.md); a change to a
pass updates them in the same commit.

### Facts (`facts/`)

Not a pass: the read-only questions the passes ask.

| File | What it answers |
|---|---|
| `flow.spite`, `loop_edges.spite`, `between.spite`, `place.spite` | block order and edges, loops, what lies between two places |
| `uses.spite` | where a value is defined and every place that reads it |
| `effects.spite`, `summary.spite` | per function: what it reads, writes, calls, whether it waits, and whether it can let go of an object |
| `value_kinds.spite` | whether a type is counted, whether a layout is the library's |
| `intrinsics.spite` | what a callee means, getters and members |
| `thread_reach.spite` | which classes more than one thread can reach |
| `rewrites.spite`, `function_copy.spite` | the shared edits: remove an unused value, copy a function |

### 1. Inlining (`inlining/`)

Decides which calls to library functions are replaced by the callee's body, and drops what the inlined code no longer
needs. `Inlining` inlines a callee only when it is small (at most six blocks and sixteen operations), a library
function, not recursive, has no check or `halt`, and is not one of the lending reads (`get_at`, `first`, `last`);
`InlineCopy` splices the blocks in. `DefaultElision` removes a default store the constructor immediately replaces
(form `dropped_default`); `CarriedObjects` keeps an object a loop carries from being remade each pass. Skipped under
hot reload.

### 2. Proofs (`proofs/`)

Decides which checks cannot fail and which conditions are known, and sets the operation's `form`. `ConstantFolding`
folds closed computations to constants; `BranchFolding` decides branches and drops the blocks no path reaches;
`ShortCircuits` threads `and`/`or`; `Hoisting` lifts a loop's invariant reads. `Spans`, `NumberSpan` and `LoopShape`
prove arithmetic fits (`unchecked`, `no_overflow`), `Relations` proves a read is inside its bounds (`unguarded`,
`exact`), `ListedValues` decides a test against a value a list never holds, `InsideCopies` and `KnownFact` carry a
proof across a call that cannot change it. Rule: a proof only ever removes a check, never adds one, and a call keeps a
proof only when its effect summary says it cannot change what the proof used. Proofs hold only when
`Lowered.proves_ranges()` (not hot reload, REPL or remote loop).

### 3. Ownership (`ownership/`)

Decides who counts what: it sets every `Parameter.ownership`, inserts `retain` and `release`, marks the moves, and makes
the copies. The conventions:

| Ownership | Meaning |
|---|---|
| `owned` | the callee owns the argument and releases it at every way out; the caller passes a count it no longer needs |
| `held` | the caller holds the name for the whole call and the callee never assigns it, so neither retains nor releases (the `___held_` copy) |
| `lent` | the value is an item lent from a list or a row, or the callee lends its answer on; no count is made at all |

Inside a function a value's origin is one of `plain` (not counted), `static`, `held`, `owned`, `borrowed`, `lent` or
`lending`, and `Counting` inserts a count only where an origin needs one.

`ParameterConventions` decides the conventions to a fixed point, `Consumption` knows what hands a value on (a
projection, a wrap, a lent return), `Borrows` classifies origins, `Counting` inserts the counts, `PairedCounts` cancels
a retain and release that meet, `Copies` decides a deep copy and whether it needs a table, `SlotStores` removes a write
back of what a slot already holds, and `TextAppends` marks appends to text done in place.

**The row rule.** A row borrows its items, counts nothing, and its free releases nothing. It becomes a class only if it
escapes: it is stored into a field, returned, passed to a parameter that keeps it, or given to the intrinsics that keep
a value (`write_value`, `swap_values`); every other intrinsic never keeps it. `RowEscapes` runs before the conventions
are final and decides it, to a fixed point over `keeping` parameters (a parameter that stores or returns what it is
given, or passes it on to one that does).

### 4. Placement (`placement/`)

Decides where memory lives. `Placement` puts an object that never leaves its function in the frame (`make` with form
`frame`, released with form `frame`), and a local list or buffer of known size in the frame; `Escape` is the proof
that it never leaves; `ResultSlots` makes a result in the caller's frame; `Pools` pools the objects of a class made
on one thread (`Layout.pooled`). Skipped for inspectable builds, and pools are skipped under `--debug-memory`.

### 5. Arrangement (`arrangement/`)

Decides how a class is laid out: `make_static` marks a singleton that holds nothing as `static`; `SlotLists` stores a
list held only by another list in its slot (`arrangement` `slots`); `TableCopies` and `TableCopy` read a table filled
once as constants (form `table`, with the intrinsic `List.holds_table`). Not run for inspectable builds.

### 6. Concurrency (`concurrency/`)

Decides what runs at once and how threads share. `Waits` finds the calls that suspend, marks functions `waits` and
`resumable`, and starts waits that can overlap (`started`, `overlap`); `Bands` and `Rows` find loops and calls whose
passes write only their own item or share nothing written, and run them in pieces (`Bands.run`, `Rows.run`, setting
`makes_threads`); `SharedCounts` makes a class's counts plain when no other thread reaches it (`atomic_counts`);
`SingletonForms` chooses the cheapest safe form of each singleton (read-only, atomic, reading functions share the lock;
forms `locked`, `atomic...`, `in_place`) and `LockedLoops` takes a lock once around a counted loop of calls to one
singleton. Rule: a decision here is only made when the effect facts prove it; where they cannot, the call keeps its
lock or its count.

### 7. Folding (`folding.spite`)

Replaces identical functions by one: the key is the function with values renumbered and types written exactly, and
`folded_into` names the survivor; every `callee` and function-naming `literal` is redirected, to a fixed point (at most
eight rounds). Keyed on exact class identity except where a function names no layout.

### 8. Reach (`reach.spite`)

The tree shaker on the IR: starts from `entry` and `runtime_roots`, follows calls, function-naming literals, types,
copies and union tags, and drops every function and layout nothing reaches. A program that does not use a feature
carries none of it.

## The printer

`Printer` (`printer.spite`) writes the C from the `Lowered` after the passes, and from nothing else. `print_pieces`
wires the pieces, refuses what cannot be written, then prints enums, functions, and layouts; `assembled` joins them in
the order of a C file (header, types, prototypes, support, bodies, `main`). It decides nothing: a check it writes is a
check a pass left, a count it writes is a count a pass inserted.

| File of `printing/` | What it prints |
|---|---|
| `body.spite` | each function's signature and body, from its blocks, operations and terminators |
| `intrinsics.spite` | the C of each intrinsic, from its name and the operation's `form` |
| `layouts.spite`, `copies.spite` | each layout's struct and its allocate, init, release, free and copy functions, enums, deep copies |
| `runtime.spite`, `output.spite` | the runtime support the program reaches, and the collected output; `refuse` is how a piece says what it cannot print |
| `sites.spite` | the site table: each site's report text |
| `naming.spite` | the C names and types of layouts, values and symbols |
| `machines.spite` | the state machines of functions that wait |
| `hot.spite` | the hot reload manifest and library, and the remote loop |

**Refusals.** What the printer cannot write is refused by name, never printed wrong (D244): the build stops with `error:
the C cannot be written from the high IR:` followed by one line per refusal, each naming the function and the
construct. A function whose body lowering could not write carries its reason in `not_lowered`, and the printer refuses
it by that reason.

## How to work on it

**Switching the pipeline on.** In `compiler/compiler.spite` the variable `writes_from_intermediate` is `false`
(committed); when it is `true`, `Compiler.write_from_intermediate` runs `Pipeline` after the generator and its C
replaces the generator's whole. Set it to `true` in your checkout to try the pipeline and **never commit it on**. It
is not a `Build` setting, so no program's C moves for it. The day the corpus passes through the pipeline the switch
and the generator are deleted together.

**Building a pipeline compiler.** With the variable `true`:

```
.spite/spite.exe compiler --build --executable-path=.spite/pipeline.exe
```

It takes about 25 minutes. Builds and corpus runs cost hours and quota, so batch them: type-check while fixing with
`.spite/spite.exe compiler --check` (about 6 minutes), make every fix you can see, then build once and run the corpus
once ([AGENTS.md](../../../AGENTS.md#builds-and-test-runs-cost-hours-and-quota-batch-them)). Never build to test a
single fix, at most two builds run at once on the machine, and when several agents work in parallel only the
coordinator builds. `--check` also validates the comment links in the compiler sources against the headings of the
pages they name, so a renamed heading breaks them.

**Reading the IR.** `--high-ir=<folder>` writes the IR after the passes, even when the printer then refuses, so a
refusal can be diagnosed from the IR it was made on. It works in any compiler that has the `Intermediate` namespace,
not only a pipeline compiler (it runs `Pipeline.prepare()` itself).

**Reading the C.** `--c-source --c-path=out.c` writes the program's C (the pipeline's, in a pipeline compiler).

**Running one program.** With an existing compiler, never a new build:

```
timeout 120 .spite/pipeline.exe <program folder> --debug-memory
timeout 120 .spite/pipeline.exe <program folder> --debug-memory --c-source --c-path=x.c
```

`--debug-memory` also checks that every allocation is freed. A program that hangs is a bug: the timeout is how you
find out (waiting primitives not marked as waiting once did it).

**Testing a fix in the C without a build.** Take the C from `--c-source --c-path=x.c`, edit the function you believe is
wrongly printed, and compile it with the C compiler and flags the compiler itself passes (`-O0 -I.
-Wno-parentheses-equality`, and `-lm` when the C includes `math.h`). If the edited C is right, the fix belongs in the
pass or the printer that decided that C, and it is verified by the next batch build, not by this.

**Refusals by name (D244).** Every failure is loud: a construct a piece does not handle is refused by name (the
printer's `refuse`, lowering's `not_lowered`), never lowered or printed as something else. When you cannot build a case
now, make it refuse with a message that names the function and the construct; a silent wrong value is the bug.

**Rules for this code.**

- No em dashes, full words, and a comment is only a link to a markdown heading that exists.
- A name prefix may not stand in for a namespace (D588): a family of classes is a folder.
- Every optimisation and every proof is documented in the same commit in `docs/optimizations.md` and `docs/proofs.md`.
- Whatever can be worked out while compiling is worked out while compiling; ask of everything whether it tree-shakes.
- Commits start with a gitmoji and name the model; stage explicit paths, never `git add -A`; push `backend-ir`, and
  master only after `bash check.sh --update-seed` passes on exactly what is pushed.

**The IR verifier** (`verifier.spite`). It runs after every pass and after `tidy`, called from `Passes.run` with the
pass's name. It refuses a value that is used but never defined, a use that its definition does not come before on every
path (by dominance), and a jump whose arguments do not match the target block's parameters. The first ten problems become
`error: the high IR is not valid: ...`, each naming the pass that made the IR invalid, so the pass to fix is known.

## The front end (D618): the target, built directly

The flow is `source -> syntax tree -> high IR, with every name resolved and every check made while it is built ->
passes -> printer`. Nothing else resolves or checks: `compiler/source/generation/` does not exist in the final form,
and neither does any reader of C text. This is built as the final form, not migrated question by question.

| Piece | Owns | Files |
|---|---|---|
| Parser | text to syntax tree, and the errors of spelling, layout and naming it already raises | `syntax/`, `discovery/` (unchanged) |
| Program model | every class, union, enum, function, field and generic instance of the program, built from the syntax tree and `analysis/`: lookups by name and by qualified name, generic and template instantiation, getters and setters, values folded while compiling, reflection makers | `intermediate/model/` (new), replacing every `generator.` call, `front()`, and every `Generation.` type the lowering uses |
| Checks | every compile error, raised by the lowering at the place it builds the operation the error is about, with the exact text its `specs/` page gives; the first error stops the compile, as today | `intermediate/checks/<spec page>.spite`, one file per page of `specs/`, called from the lowering |
| Lowering | building the IR from the syntax tree through the model, calling the checks | `lowering.spite`, `expression_lowering.spite`, `statement_lowering.spite`, `reflection_lowering.spite`, `instances.spite` |
| Passes, printer | unchanged | as above |

`compiler.spite` runs discovery, then `Pipeline`; there is no generator pass and no `writes_from_intermediate` switch.
The cut deletes `generation/`, the C readers (`overlap_facts`, `tree_shaker`, `count_sharing`, `class_pools`,
`function_folder`, `configured_copies`, `nested_lists`, the C splitting of `hot_reload_library`, `state_machine`) and
the older walk in `backend/high_*.spite` in one change.

**Where a check is called from.** `Instances` holds one `Checks.Report` (`intermediate/checks/report.spite`) for the
whole compile, reached as `known().report` from `ExpressionLowering`, `StatementLowering` and `ReflectionLowering`.
`Instances.enter_function` sets its place (path, line, owner, function) and `StatementLowering.lower_statement` its
line, so a check only says what is wrong: `known().report.refuse("the exact text of the spec page")` makes
`<path>:<line>: error: <text> (in <Owner>.<function>)`. Only the first refusal is kept; `Lowering.lower` stops at
the function that raised it and `Pipeline.prepare` answers it as `error_message`, so no pass or printer runs. A
check lives in `checks/<spec page>.spite` as a class whose functions take what they judge (a syntax node, a type, a
model entry) and the `Checks.Report`; the lowering calls it at the place it builds the operation the error is
about and returns when `report.failed` is true.

**The program model (`model/`).** `Model.Catalog` is the program: built once from the discovery result, it holds every
class, enum, union and `type` shape (`classes`, `enums`, `unions`, each an `Analysis` info object) and answers by name.
Everything the lowering used to ask the generator is asked here, and nothing in `model/` writes C or reads C text.

| Class (file) | What it answers |
|---|---|
| `Catalog` (`catalog.spite`) | `class_by_c_name`, `class_by_qualified`, `class_named(name, from_namespace)` (the namespace search of [classes_and_files](../../../specs/classes_and_files.md)), `union_by_c_name`, `union_named`, `union_qualified`, `enum_by_c_name`, `enum_named`, `enum_qualified`, `enum_with_value`, `class_of_type(type)`, `list_class(element)`, `dictionary_class(dictionary)`, `function_of(class, name)` (a declared function, else the template instance that name means), `getter_of`, `setter_of`, `signature_of(function)`, `constructed_type(class)`, `resolve_type(reference, owner)`, `allocate_class_id` |
| `Types` (`types.spite`) | the type constructors (`integer_type()`, `list_of`, `nullable_of`, `dictionary_of`, `class_type`, `union_type`, `enum_type`, `signature_of`), `label` and `exact_label` (the names of container and generic classes), `same`, `is_reference`, `is_wider`, `without_nullable` |
| `Scope` (`scope.spite`) | a function's names with their declared types, and which paths are narrowed to non-null: `declare`, `written_type`, `narrow_path`, `is_path_narrowed`, `forget_path`, `visible_names`, `inside_loop`; a block makes `Scope(parent)` |
| `Paths` (`paths.spite`) | `text_of(expression)`, the text of a name path (`monster.target`, `list[index]`) that narrowing is keyed by; empty for anything that is not one |
| `Typing` (`typing.spite`) | `type_of(catalog, expression, scope, owner)`: the type an expression has, from the syntax tree and the catalog alone, with narrowing applied; null when it has none (and a refusal was raised) |
| `Declarations`, `Resolution`, `Bindings` | building the catalog: registration of classes, enums, unions and shapes; the resolution of type references, attributes and functions; the instance of a generic class for the values given (`instantiate`, named `Store__List_Integer`) |
| `Templates` (`templates.spite`) | template functions: a function with a `Symbol` parameter is a template, recorded in the class's `templates` instead of its functions (`register_template`), and `function_instance(catalog, owner, name)` makes the instance a call by `name` means when the class declares none |
| `Evaluation` (`evaluation.spite`) | values worked out while compiling: the types a `$name` stands for (`codegen_member_type`), the value of a `$name` for an instance, and reflection read at compile time |
| `Parts` (`parts.spite`) | non-null reads of the parts of a syntax node, which the language's narrowing rules need as functions |

The lowering holds one `Model.Catalog` (through `Instances`) and one `Model.Scope` per block; it asks `Typing` for the type of
an expression it needs before it has lowered it, and types a value it has lowered from the value itself. A check
needs only a node, a `Model.Scope`, the catalog and the `Checks.Report`.

**What proves it:** every program in `diagnostics/` gives its exact `expected_errors.txt`, every conformance program,
example and documentation program its exact output with balanced memory, the benchmarks allocate no more and run no
slower than the generator's C, and the compiler builds itself to a fixpoint.

**Where an error's rule is:** its `specs/` page is normative (D511). `generation/generator.spite` is only a reference
for how the rule was implemented; read it for the cases the spec page states, and when the two disagree the spec wins.

## Where it stands

The pipeline is built whole: lowering, the eight passes and the printer all exist, and a pipeline compiler compiles
the benchmarks' programs and most of the corpus. It is dormant: the generator still writes every program's C in a normal
build, and master carries all of it unused. Done means every line of C comes from the IR and the generator and every
reader of C text are deleted in one change; until then the generator is the fallback being emptied.

What remains, by piece:

- **Lowering:** a call on a `type` that no class of the program fits; `to_string` on `Anything` in REPL builds; the
  corners of reflection (the entry class's function list, a class passed as its `Spite.Class` object, the size of a C
  struct, a `type` or function value passed straight to C); the debug support classes for automatic `to_debug`;
  parity with folds the generator does (dictionary literals read only by literal keys, fused template chains, words
  inflected while compiling, a loop over a list of different classes run at once); remote REPL and hot reload builds.
- **Passes:** parity with the generator's C (loop-carried objects reassigned each pass still on the heap, some
  `get_at(i - 1)` proofs, the locked-loop forms for reading and never-changing singletons, configured copies, objects
  made for their owner, retains around held parameters); about 80 function rows still do more work than the
  generator's C.
- **Printer:** hot reload stage 3 (migrations in the reload library; a reload that changes a class's attributes is
  refused by name until then), the remote loop and breakpoints, and `___allocate_constructed`, `___make_in` and deep
  copies on demand only.
- **The switch-over:** when the corpus, the examples, the documentation programs, the diagnostics, the benchmarks and
  every special job of `check.sh` pass with the switch on and no benchmark is slower, the switch goes on for good and
  the generator's emission, every reader of C text (`overlap_facts`, `tree_shaker`, `count_sharing`, `class_pools`,
  `function_folder`, `configured_copies`, `nested_lists`, the C splitting of `hot_reload_library`, `state_machine`) and
  the older walk below are deleted.

The list of decided and unbuilt work is `TODO.md` on master (this branch does not carry it). The decision log
is [DECISIONS.md](../../../DECISIONS.md): D569 (caching), D586, D587, D588, D599.

**Known contradictions between the code and the specification** (each is a bug to fix, not to document):

- D599 says nothing that is a function in the end becomes an operation: the intrinsics `List.holds_table`,
  `Attribute.hold`, `Attribute.place`, `Attribute.bind_item` and `Instances.fill` reach into list and reflection
  storage and need review against it.

### Caching (D569)

The lowered IR is cached per function, so a check or a build relowers only what changed; the key names the winning
definition of a function (by file and text, never the file it first came from), the load set, and everything it
depends on (types, layouts, signatures, the templates and codegen values that made it, the facts it was lowered
against, and the compiler's version). A stale or unreadable entry is rebuilt, never trusted (D244). `--check` stops
after lowering and runs no pass. The full rule is in
[the specification](../../../design/proposals/desugared_high_ir.md#caching-the-high-ir-d569).

## The older walk in compiler/source/backend

Before the desugared IR, the compiler built a different, partial high level, `compiler/source/backend/high_*.spite`
(`HighProgram`, `HighWalk`, `HighBand`, `HighCheck`, `HighEmit`). It describes the program before the generator's
decisions and is compared, in `check.sh`'s `high_level` job, with the facts read from the generated C. It is being
replaced by everything above and is deleted at the switch-over; until then its sources link to the headings below, so
they stay. Nothing new is built on it.

### The whole program

One `HighProgram` holds the whole program until it is lowered. It holds the abstract objects the program knows and the
functions it walked, each with its blocks and operations, and the classes the program makes somewhere. The whole
program is kept because what beats a hand-written program is worked out across the call graph.

### Functions, blocks and operations

A `HighFunction` is blocks in the order the walk built them. A block is a list of operations with the edges that
leave it; an edge (`HighEdge`) has a kind and a target: `fallthrough`; `taken` and `dropped` for an `if`; `condition`,
`body`, `step` and `exit` for a loop; `fails` and `holds` for an `assert`. A block whose last operation is a `return` or
a bare `crash` has no edges and the walk stops there; an `if` whose two branches both end leaves its join block
`unreachable`.

An operation is one of Spite's own operations (constant, local read or write, field read or write, slot read or
write, computation, call, made object, list walk, assert, crash, return, loop, and an operation of unknown effect). A
loop is one operation with a body block of its own, joined by the edges `body`, `step` and `exit`. An operation says all
of what it does: `result`, `operands` in order (`-1` for the receiver), `operator` for a computation, `literal` for a
constant, `condition_text` for an `assert` or `crash`. `and` and `or` are control flow (a `branch`, a right-side
block, a `choice` in the join). An operation of unknown effect says why the walk did not build the case, and the
function holding one is not covered: an unknown effect is never an empty effect, so a wrong fact is never written
(D244).

### Values and owners

Every value is a `HighValue`: its kind (`constant`, `local`, `field`, `slot`), its owner (the frame is `-1`, a field or
slot belongs to an abstract object), name, version and type. Every write makes a new value, one version above the one
it replaces. Where two paths meet, a name that holds different values records a `HighJoin` with a merged value (the
phi): an `if` joins what its branches leave (a branch that ends does not reach the join); a loop merges at its
condition block, each merge recording the entry value and the value the body leaves; a local declared inside a branch
or body is not joined; a value read before anything wrote it is the one value the function was entered with. A join
moves values and never drops one. Writing a field of a reference type reads it first, as its own operation, because
what it held is released before the new value goes in. A value number of `-1` is "no value here", and nothing is asked
of it.

### Abstract objects

A `HighObject` has an identity: it was made at a place (`made`), is a list's items (`items`), or is the receiver a
function was called on (`receiver`), with its class, its `place` and whether it is reached through a field. One object
per place that makes one and one per list for its items, so facts are questions about an object, never a name. The
layout stays abstract (D520); a later pass chooses columns or chunks.

### What the walk covers

`HighWalk` builds one function from the typed tree: declarations, assignments, field, slot and local reads and writes,
calls to the program's own functions, `if`, `while` (plain and counted), `return`, `assert`, `crash`, a list walk with
its body, and arithmetic, comparisons and text. Anything else (a `switch`, a template, a library or foreign call, an
unresolved type, a getter or setter) is recorded as an operation of unknown effect that says what it did not build.
The walk never skips a statement silently.

### The facts folded from the operations

A function's facts are folded from its operations, so the summary cannot disagree with them: the reads and writes it
names (keyed by abstract object and field), the calls it makes (by C name), and whether it has a loop. A value of the
frame is not a fact.

### What a function surely wrote

`HighMustWrite` is a forward pass over the blocks giving the fields a function surely wrote before it answered `true`,
before it answered `false`, and before it returned at all. Where paths meet only what every path wrote is kept, every
block starts from "wrote everything" and the pass only takes away, a loop's body that may run no times writes nothing
surely, and each list is its own copy. `--high-level-report` prints it and `conformance/stage6/must_write_scratch`
checks it word for word. No pass reads it yet.

### What a band site needs

A band site is a walk over a list's items calling one of the item's own functions on each item (`list.each_update()`),
which the generator runs in chunks on several threads when the function, run on an item, writes only that item.
`HighBand` answers the first of its three questions on the high level (does the function write only its own item), by
following every call with the item as the receiver and answering "cannot say", never yes, for anything it cannot
follow. The other two are whether anything it calls counts references and whether banding is worth it. The comparison
fails `check.sh` on only one disagreement, **the high level answering yes where the C reader answers no**, since that
would band a loop the C path keeps whole.

### C written from the high level

The first step of printing from the high level: a function's C body is written by `HighEmit` instead of the generator's
walk, wherever it can write all of it, in every build. The generator still runs its own walk first, so every diagnostic
and study it makes stays. `HighEmit` writes control flow from the blocks' edges and writes numbers and Booleans, fields
through the receiver or a borrowed parameter, comparisons, `and`, `or`, `not`, `if`, `while`, `return`, owned
parameters released at every way out, and a constructor's body. A function is left to the generator whole when the walk
did not build all of it, it holds an operation `HighEmit` does not write, it is a library function, its body is supplied
by the compiler, it answers into a result slot, the build reloads code, or the generator's own lines for it hold a
marker for a later pass. Decisions still made by the generator (a check's need, a parameter's ownership) are read as
facts and recorded on the expression, never as C. All of this is what `Printer` and the passes replace.

### What the comparison checks

A function is **covered** when the walk built all of it and every fact it names is one the high level can name; it is
not covered when it holds an operation of unknown effect, makes an object, walks a list with a template, touches a
container, names an object reached through a field, or names a field of a class the program never makes. A covered
function is compared one kind of fact at a time: reads and writes (when every key is a plain field key), loops (when no
call into the library is made), and not calls. A compared kind that differs fails `check.sh`, naming the function, the
kind, the key and both answers. The comparison checks the compiler and is never an error in a user's program.
`scripts/high_level_coverage.sh <compiler>` runs it over the corpus, and each comparison is deleted the day its decision
has moved onto the high level. It costs nothing at run time and compile time only when `--high-level-report` is asked.
