# The high level of Spite's own backend

What [`compiler/source/backend/`](../compiler/source/backend) builds of a program before any code is written:
the form the compiler's own optimisations run on and, later, the form the low level is lowered from (D560, milestone
M3 of [own_backend_plan.md](proposals/own_backend_plan.md#m3-the-two-level-ir-and-a-first-lowering-d560), the proposal in
[high_level_ir.md](proposals/high_level_ir.md) and its review). The C generator keeps its decisions for now: the walk
built here describes the program before those decisions, and [`generation/overlap_facts.spite`](../compiler/source/generation/overlap_facts.spite),
which reads the generated C, answers for the program after them. While the generator still decides, the two describe
different programs, so every fact the two give is compared in [What the comparison checks](#what-the-comparison-checks)
and never in a user's program.

Nothing here is read at run time and nothing is laid out for a person to read (D561): the high level is held while
compiling and ships nothing. A program's whole high level exists only while it is being compiled.

## The whole program

One `HighProgram` holds the whole program, kept until it is lowered, not built one function at a time and thrown away:
what beats a hand-written program is worked out across the call graph (specialising per call site, choosing a data
layout for the whole program, deciding what may run at once), and nothing decided for one function alone survives that.

The program holds two lists: the abstract objects it knows (see [Abstract objects](#abstract-objects)) and the functions
it walked, each with the blocks and operations the walk built for it. It also holds the classes the program makes
somewhere, which the comparison needs and which the walk notes as it goes.

## Functions, blocks and operations

One function is one `HighFunction`: its blocks, in the order the walk built them. One block is a list of operations
with the edges that leave it. An edge is a `HighEdge` with its kind and the block it goes to: `fallthrough` for the
next block, `taken` and `dropped` for the two branches of an `if`, `condition`, `body`, `step` and `exit` for a loop.
A block that cannot go on at all (its last operation is a `return` or a `crash`) has no edges at all, and the walk
stops there.

An operation is one of Spite's own operations, not a piece of text: a constant, a local read, a local write, a field
read, a field write, a slot read, a slot write, a computation (arithmetic, a comparison, a text join), a call, a made
object, a list walk, an assert, a crash, a return, a loop, and an operation of unknown effect. A loop is one operation
with a body: the body is a block of its own, and the edges `body`, `step` and `exit` are what join it to the block
that holds the loop, so a loop's body can be cut into pieces later (D559). A list walk is one operation with a body
too, and says whether it is a counted `while`, a plain `while`, or a walk over a list's items.

A block that cannot go on at all has no edges at all, and the walk stops there: a `return` ends it, and so does a
bare `crash` (which is `crash false`, the one that always halts). A `crash` whose condition may hold halts only on the
path where it is false, so what follows it is walked as usual; a `crash true` never halts.

An `assert` whose condition is false returns the function's default and goes on nowhere, so it is a way out of the
function like a `return`: the walk ends the block with the `assert`, gives it a `fails` edge to a block holding that
`return` (answering `false` in a function that answers a `Boolean`) and a `holds` edge to the block the walk goes on
in. A question about what the function did before it answered sees that way out like any other.

An `if` whose two branches both end (each returns or halts) leaves nothing to walk after it: its join block is ended
as `unreachable`, and the statements after it are not walked, because nothing runs them.

Nothing is a block of C text and nothing is a name that means something at one place in a file.

**An operation says all of what it does**, so code can be written from it and not only facts read from it:

- `result` is the value it makes (a read's value, a write's new version, a computation's or a call's answer);
- `operands` are the values it takes, in order: a write takes the value written and the value holding the object
  (`-1` for the receiver), a slot read or write also takes the index, a call takes the value it is called on first
  (`-1` for the receiver) and then its arguments, a `return`, an `assert`, a `crash` and a `branch` take the value they
  test or answer;
- `operator` names a computation's operator (`add`, `less`, `logical_not`, ...), and `literal` a constant's value
  as written;
- `condition_text` is the source text of an `assert` or a `crash`, which its report prints.

An `if` and a loop's condition end with a `branch` operation that takes the value they test, so the edges `taken` and
`dropped` say which value chose them. `and` and `or` evaluate their right side only when it matters, so they are
control flow: the left side ends with a `branch`, the right side is a block of its own, and a `choice` operation in
the join answers the result. A walk over a list's items calling a member function is a `call` marked `per_item`
inside the walk's body, whose operand points at the list's items. A function that ends without a `return` ends with
one, so every way out of a function is a `return`.

An operation of unknown effect is what a case the walk does not build is recorded as. It says why, and the function
holding one is not covered (see [What the comparison checks](#what-the-comparison-checks)): an unknown effect is never
an empty effect, so a fact that would be wrong is never written (D244).

## Values and owners

Every value is a `HighValue`: what kind it is (`constant`, `local`, `field`, `slot`), the object that owns it, its
name, its version, and the type it was given. A local's owner is the frame (`-1`), a field's and a slot's owner is an
abstract object.

There is a new value on every write of a local, a field or a slot, each with its version one above the value it
replaces. That is the form the first questions need: at a join, the values that are the same on both sides are the
ones both sides wrote, and a value one side wrote alone is one the other side may not have written at all.

A value number of `-1` is the walk's "no value here". It is what a statement the walk did not build answers, and the
statement that could not be built says so as an operation of unknown effect, so nothing is ever written from it and no
fact that would be wrong is. Nothing is asked of a value that is not there: reading the owner, the class or the type
of `-1` answers nothing rather than reading past the end of the list.

Writing a field whose type is a reference reads the field first, as its own operation: what the field held is
released before the new value goes in, so the read is a read of that same field. The C writes it in that order and the
comparison needs both walks to say it in that order.

Where two paths meet, a name that holds a different value on each records a `HighJoin` with a **merged value** of its
own (the phi of an SSA form): the name, the value it held before, the value each side leaves there, and the merged
value every later read sees. Nothing after a join reads a value only one side wrote.

- **An `if`** joins what its branches leave, each starting from what the condition left. A branch that ends (it
  returns or halts) does not reach the join and has no say in it: after an `if` whose `then` returns, the values are
  the `else` branch's, unmerged.
- **A loop** merges at its condition block, since the condition is where every pass starts and where the loop is
  left. The merged value is made the first time the body or the condition reads the name, so a read in the body sees
  the value of the pass before, not the one the loop was entered with; a name the body writes and never reads before
  writing is merged when the body ends. Each merge records the value the loop was entered with and the value the body
  leaves (none when the body never steps back). After the loop, every name holds its merged value: the body may have
  run any number of times, none included.
- **A local declared inside** a branch or a body is not joined: nothing outside it can read it.
- **A value read before anything wrote it** in the function is the value the function was entered with, one per name,
  shared by every path, so two paths that only read a name never make a join for it.

A join moves values and never drops one: each side's value is taken into the merged value. What a write replaces is
released at the write (the read before a reference write, above), so a join needs no release of its own, and the low
level lowers a merged value of a reference type as a move from whichever side ran.

## Abstract objects

An object has an identity: `HighObject` says which place made it (`kind` is `made`), that it is a list's items
(`items`), or that it is the object a function was called on (`receiver`), the class it is of, the place that made it
(`place`, naming the function and line), and whether it is reached through a field (`through_field`).

One object per place the program creates an object, and one per list for its items. An object reached through two
expressions is one object, which is what makes "the passes of a loop write different slots" (D559) and the per-object
facts already built questions about an object rather than about a name.

The layout stays abstract: an object is of a class and holds fields by name, and nothing says where any of it sits in
memory (D520). A later pass chooses columns, archetypes or chunks, or the device.

## What the walk covers

`HighWalk` builds one function from the compiler's typed tree. It builds:

- declarations, assignments, field reads and writes, slot reads and writes, and local reads and writes;
- calls to the program's own functions;
- `if`, `while` (plain and counted), `return`, `assert` and `crash`, and a list walk with its body;
- arithmetic, comparisons and text as computations over the values they read.

Anything else (a `switch`, a template, a call to a library or a foreign function, a type the walk cannot resolve, a
getter or a setter) is recorded as an operation of unknown effect that says what it did not build. The walk never
skips a statement silently.

## The facts folded from the operations

A function's facts are folded from its operations, so the summary cannot disagree with the operations it came from:
the reads and writes it names, keyed by the abstract object and the field, the calls it makes (by the name the C gives
the function), and whether it has a loop. A value of the frame (`owner` `-1`) is not a fact: a local lives in the
frame and is not a field of an object.

## What a function surely wrote

`HighMustWrite` folds a second summary from the same blocks: the fields a function surely wrote before it answered
`true`, before it answered `false`, and before it returned at all. It is a forward pass over the blocks, so it is a
summary of the operations and the joins, never a second walk:

- what a block starts with is what every block that leads into it ended with (the fields all its predecessors wrote),
  and what it ends with adds the fields the block itself writes. Where paths meet, only what every path wrote is
  kept, which is what "surely wrote" means. The entry block, and a block nothing leads into, start with nothing.
- it is a must question, so every block starts from "wrote everything" and the pass only ever takes away, until
  nothing changes. A loop's condition block then settles at what the loop was entered with, since a body that may run
  no times surely writes nothing: a write inside a body is never surely written after the loop.
- every list it holds is its own copy: one block's state is never the same list as another's, so adding a block's
  writes can never add them to a block that did not make them.
- the per-answer sets are the intersection over the blocks that return that answer, so a field is in "when true" only
  if every path that answered `true` wrote it. A failed `assert` is one of those blocks.

The summary is per function and keyed by the abstract object and the field, the same keys the comparison uses. It is
what D559 step 1 needs: a caller that reads a scratch's field after a `true` answer needs no check, because the callee
wrote it. Nothing reads the summary yet, so its entry in `docs/proofs.md` waits for its consumer (see
[TODO.md](../TODO.md)); `--high-level-report` prints it for every function that returns, with the fields it names, and
`conformance/stage6/must_write_scratch` is the program that exercises it: its `expected_high_level.txt` holds the
summaries the report must print word for word, and `scripts/high_level_coverage.sh` fails when one is missing, so the
summary is a checked answer and not only printed.

## What a band site needs

A band site is a walk over a list's items that calls one of the item's own functions on each item
(`list.each_update()`): the generator's `band_condition` (`generation/generator.spite`) runs it in chunks on several
threads when the function, run on an item, writes only that item. D559 extends the same idea to a counted `while`
whose passes write different slots; that is the next user, and the question below is the one it will ask too.

The decision needs three answers, all of which `generation/overlap_facts.spite` gives today by reading the C:

- **does the function write only its own item?** Run with `self` being the item, every write it makes, and every
  write of every function it reaches, lands on that item, and no other object of the item's class has that field
  read or written anywhere it reaches (another pass could be writing it).
- **does anything it calls count references?** A retain, a release, a box's count, a counted attribute. While the
  count lives in the C this is a marker in the C's text; on the high level it is a write of a field or a slot whose
  type is a reference, or a call of a function that does one.
- **is it worth banding?** A weight from the C's line count with a multiplier per loop, compared with a minimum.

`HighBand` answers the first one on the high level. A walk over a list's items calling a member function is a `call`
operation marked `per_item`, whose first operand points at the list's items object. `HighBand` starts at that function
with its receiver as the item, and follows every call: the call's operands say which of the caller's objects it
passes, so the callee's receiver or parameter is the item exactly when the caller passed the item there. A library
function it reaches (a list's `append`, say) is walked on demand and folded like any other, so a list the item does not
own being appended to is a write elsewhere, as it is. Anything it cannot follow (an operation of unknown effect, a
function the walk cannot build, an object made on the way) makes it answer that it **cannot say**, never yes.

It is stricter than the C reader on purpose where the two differ in what they know: a list held in a field of the item
is not the item's own unless that field holds an object made for its owner (D556), which the high level does not prove
yet, so appending to it is a write elsewhere.

The comparison asks both for every band site, and only one disagreement fails `check.sh`: **the high level answering
yes where the C reader answers no**, since that would band a loop the C path keeps whole (a wrong answer, D244). The
other cases are reported, each with what both sides wrote:

- `stricter`: the high level says no where the C reader says yes. Nothing is wrong, but moving the decision onto the
  high level while one is left would lose a band, so each is a gap to close first.
- `blind`: the C reader follows a call it has no body for (an `io:` key) and answers no for everything; the high level
  reads the call.
- a site the high level cannot answer yet, with the reason.

The decision moves off the C reader when every band site of the corpus agrees or is `blind`; the other two questions
follow it.

The loop itself is already one operation with a body: `loop_kind` is `counted` when its condition compares a counter
with a bound by the generator's own rule (the operator is `less` and each side is a name or a `count()`), the body is
a block of its own, and `body`, `step` and `exit` are the edges that join it to the block holding the loop. Cutting
that body into a piece function is what makes each pass a function the compiler can give to another thread, and the
distinctness check is a run-time test over what the passes wrote.

## C written from the high level

The point of the high level is that the program the compiler transforms is the program it writes. The first step is
that a function's C body is written from its high level instead of by the generator's own walk, wherever the printer
(`HighEmit`) can write all of it, and the generator's C everywhere else. There is no setting for it: a function the
printer covers is written from the high level in every build, and `check.sh` running the whole corpus, the examples
and the compiler itself is what proves the two write the same program.

The generator still runs its own walk over every function first, and only its lines for the body are replaced. That
keeps every diagnostic it gives and every study it makes on the way (unused names, a function that falls off its end,
the nested lists, the mirrors), so a program never compiles with fewer checks because its C came from the high level.

What the printer writes today, and why the rest is not yet its:

| Part of a function | Written from the high level |
|---|---|
| constants that are whole numbers or Booleans | yes |
| reads and writes of locals and parameters of a number or Boolean type | yes |
| reads and writes of such a field, through the receiver or a parameter | yes |
| comparisons, `not`, and `and` and `or` (as the branches they are) | yes |
| `if`, `while` and `return` | yes |
| `+`, `-` and `*` on whole numbers, with or without their overflow check, and `/` and `%` where no check is needed | yes, with the generator's decision for that site (see below) |
| `/` and `%` that check for zero or for the one quotient that overflows, and `%` on fractions | no: the printer does not write those checks yet |
| a fraction's literal | no: the generator writes it with the precision of the value beside it |
| calls | no: the generator names copies (`___held_0`, `___for_0_Integer`), inlines and folds |
| a value of a class, a list, a text or a nullable type | no: retains and releases are the generator's |
| `assert` and `crash` | no: their site and their report are the generator's |
| a field of a singleton or a value class | no: the generator lays those out its own way |
| a parameter the function owns (one not lent or held) | yes: released at every way out, last parameter first, unless its release may wait for another thread |
| an answer of any type but a number or a Boolean | no: the generator converts to it (`Number`, a text from a number) |
| a value written or answered as a number of another type (`Integer` into `Byte`) | no: the generator checks the narrowing |
| a constructor's body | yes: the object is made by `___make` around it, which is still the generator's |
| an object parameter that is lent or held | yes: reading one to reach its fields borrows it |

**Decisions still made by the generator, read as facts.** Whether an operation needs its overflow check is decided by
the generator's range analysis (a counter that only counts up to a bound cannot overflow), which has not moved onto
the high level yet. The generator records that decision on the expression it made it for (`lowered`: `plain`,
`overflow`, `division`, `remainder` or `held`, and the line), never as C, and the printer writes the C for it. In the
same way the generator says which parameters the function owns, and how each is released. Each such fact is a
bridge: it goes when its decision is a pass over the high level.

The printer writes control flow from the blocks' edges: a block that ends in a `loop` is a `while (1)` whose condition
is printed inside it and leaves by `break`, a `branch` with `taken` and `dropped` edges is an `if` and its `else`, up to
the join, and a `branch` into a `right side` block is an `and` or an `or`, answering into one Boolean. A local is
declared at its first write in the C scope that holds it. Every other value is a temporary of its own, one per
operation, since a value read twice is two loads.

A function is left to the generator, whole, when any of these holds: the walk did not build all of it, it holds an
operation the printer does not write, it is a library function, its body is supplied by the compiler, it answers into
a result slot, the build reloads code while running, or **the generator's own lines for it hold a marker for a later
pass** (`/*` in its C: a mirror, an item value, an overlap group). That last rule is a guard for the move, not a
design: a later pass rewrites those markers in the text, so a body without them would lose that pass's decision. It is
deleted when those decisions are made on the high level.

## What the comparison checks

A function is **covered** when the walk built all of it and every fact it names is one the high level knows how to
name. A function is not covered when:

- an operation of unknown effect is in it;
- it makes an object: the C writes a construction as a call to `___make`, `___make_into` or `___default`, and which
  one is a decision of the generator that has not moved yet;
- it walks a list with a template: the C writes that as a call to a template the walk does not name;
- it touches a container: the key of a list's items is a container key, which the high level does not build yet;
- it names an object reached through a field: whether that field holds an object made for its owner (D556) is a whole
  program question, and the C reader's answer names an owner tag the walk does not compute yet;
- it names a field of a class the program never makes: the C reader leaves such a field out entirely.

A covered function is compared **one kind of fact at a time**, and each kind has its own rule of what the two walks
can name the same, because the generator decides some of them and not others:

- **reads and writes** are compared when every key the C reader gives is a field of a class (`f:Class#field_`) with no
  owner tag on it. A `raw` write, a container key (`c:Class|...`), an `io:` key or a tagged key is one the high level
  does not build yet, so that kind is not compared and the report names the key.
- **calls** are not compared at all yet: the C writes a call as a call, as an inlined body, as a folded copy or not
  at all, and writes one the program never wrote when a value becomes text. See [The coverage the corpus
  reaches](#the-coverage-the-corpus-reaches).
- **loops** are compared when the function makes no call into the library, since a variadic call is written as a loop
  of its own that the program never wrote.

A kind that is compared and the two answers differ fails `check.sh`, naming the function, the kind, the key and both
answers, and the report also writes what the high level answered in full, so the two can be read side by side. The
comparison is a check of the compiler and never an error in a user's program: a program whose two answers differ is a
bug in the compiler, and each comparison is deleted the day its decision has moved onto the high level.

`scripts/high_level_coverage.sh <compiler>` runs the report over the whole conformance corpus and the examples,
reading each program's `flags.txt` as `check.sh` does, and writes the totals and every reason a kind was not compared
to `.spite/high_level_coverage/summary.txt`. It fails on any difference. `check.sh` runs it as one job
(`job_high_level`) over the whole corpus, so a difference is a red build.

The report is asked for with `--high-level-report=<path>`, which is a setting of `Build`, and every setting of `Build`
is a field of every program's C. So adding it moved every benchmark's `generated.c` by one line, which the same
commit regenerates (`scripts/cases/extract.sh`), and it moved the error `diagnostics/unknown_compiler_flag` holds,
which lists the settings it knows. There is no way to ask a compiler-only question of a program without a setting of
its own, so the cost is one field in every program's C until the settings stop being fields (D578's compiler written
in Spite is where that would be decided).

## The coverage the corpus reaches

Measured on this branch by `scripts/high_level_coverage.sh` over every program of `conformance/` and `examples/`
that compiles, each with the flags `check.sh` gives it:

| | |
|---|---|
| programs with a report | 413 |
| functions the walk built | 680 |
| functions the two answers agree on | 680 |
| functions that differ | 0 |
| kinds of fact compared | 1,143 |
| kinds of fact not compared, each naming why | 366 |
| functions whose calls are not compared | 680 |
| functions the walk did not cover, each naming why | 2,401 |

Every function that differs is a bug in the walk or in the C reader, and it is fixed rather than left out. There are
none left on this branch.

**Calls are not compared yet, and that is the whole of the 680.** The C writes a call as a call, as an inlined body,
as a folded copy or not at all, and writes one the program never wrote when a value becomes text; the high level sees
none of that, so comparing calls would only measure the generator's decisions. The comparison of calls comes back
when those decisions move onto the high level, which is the migration the review describes.

**Reads and writes are compared**, and that is where the overlap proofs live: a field of a class, keyed by the
abstract object and the field, and a field of a list's items, keyed by the list and the element class. They are not
compared for the 366 functions where the C reader names something else: a `raw` write, a container key
(`c:Class|...`), an `io:` key, a key carrying an owner tag (D556, whose tags the walk does not compute yet), or the
whole-container key of a bounds check, which the generator writes or drops.

**Loops are compared** for every function that makes no call into the library, since a variadic call is written as a
loop of its own that the program never wrote.

The 2,401 functions the walk does not cover are its own limits, and the reasons name them. What is not built inside
them:

| What the walk does not build yet | Functions |
|---|---|
| a name it does not know (a foreign function reached through a handle, a generated accessor) | 478 |
| a codegen value (a `$T`, a member template's type) | 180 |
| a construction, where the C writes `___make_into` | 176 |
| an owner tag (D556) | 91 |
| a getter, a setter, a switch, a container's own changing function | the rest |

Every one of them is a case the walk records as an unknown effect that says what it did not build, never as an empty
effect, so a fact that would be wrong is never written (D244).

## What it costs

Nothing at run time: the high level is held while compiling and ships nothing, and no fact it carries is checked
again by the program. Tree shaking does not apply: the whole program's high level exists only while it is being
compiled, which is what D569 decides (the whole program kept in memory and cached on disk for a later build). What it
costs is compile time and memory for the whole program, and only when a fact is asked for: `--high-level-report`
builds it and an ordinary build does not, so the cost lands on `check.sh`'s one `high_level` job and on a developer
who runs the report, not on every build. The compiler is its own largest program, so it is where the cost shows:
building the compiler's high level takes minutes.

For scale on that: the seed compiler, which is optimised, compiles the compiler's sources in under four minutes;
generation 2, which is built unoptimised, takes far longer for the same work. A high level built on every build would
multiply that by the cost of the walk on top, which is why it is built only when asked for.