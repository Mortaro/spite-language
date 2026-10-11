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

For every covered function `generation/overlap_facts.spite` has a body for, the comparison asks it for the function's
reads, writes, calls and loops and asks the high level for the same, and each key of the high level is named the way
the C names it (the class by the name the C gives it, a field by its own name with `_` after it). A difference fails
`check.sh`, naming the function, the field and both answers. The comparison is a check of the compiler and never an
error in a user's program: a program whose two answers differ is a bug in the compiler, and each comparison is deleted
the day its decision has moved onto the high level.

## What it costs

Nothing at run time: the high level is held while compiling and ships nothing, and no fact it carries is checked
again by the program. A program that never asks a fact about a function does not build that function's high level,
and a compiler built without the comparison carries none of it. What it costs is compile time and memory per function,
which `bash check.sh` and the benchmarks show.