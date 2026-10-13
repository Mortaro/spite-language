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
`crash` whose condition always holds. A `crash` with a condition that may be false ends the block only on that path,
so what follows it is walked as usual.

Nothing is a block of C text and nothing is a name that means something at one place in a file.

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

A join between two blocks records a `HighJoin`: the name it is about, the value each side leaves there, which of them
is kept, and which is dropped. A join of an `if` joins what each branch leaves with what the condition left; a loop's
exit joins what its body left with what the walk entered the loop with, since the body may run no times. A value that
is a reference is kept or dropped there, so the low level has an ownership to record and never has to guess one.

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

- a block's state is the fields both of its predecessors wrote, plus the fields the block itself writes. A join
  therefore takes only what every side wrote, which is what "surely wrote" means.
- a loop settles by repetition, and a write inside a body is in the state only once every pass has it: a loop whose
  body writes a field on some passes does not surely write it, which is the safe answer (a lost write is a wrong
  answer, D244).
- the per-answer sets are the intersection over the blocks that return that answer, so a field is in "when true" only
  if every path that answered `true` wrote it.

The summary is per function and keyed by the abstract object and the field, the same keys the comparison uses. It is
what D559 step 1 needs: a caller that reads a scratch's field after a `true` answer needs no check, because the callee
wrote it. Nothing reads the summary yet, so its entry in `docs/proofs.md` waits for its consumer (see
[design/status.md](status.md)); `--high-level-report` prints it for every function that returns, and
`conformance/stage6/must_write_scratch` is the program that exercises it.

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
costs is compile time and memory for the whole program, which `bash check.sh` and the benchmarks show.