# The low IR

The low IR is the program as a machine sees it: flat functions of instructions on virtual registers, with every byte
of memory and every jump explicit. It is made from the [high IR](high_ir.md) after the high passes, and every code
generator reads it: our own backends write machine code from it, and the C printer writes C from it for a system
without a backend ([the flow](README.md#the-flow)). Its code is `compiler/source/backend/`.

- [What it is](#what-it-is)
- [The form](#the-form)
- [Lowering to it](#lowering-to-it)
- [What reads it](#what-reads-it)

## What it is

The high IR keeps Spite's operations so that the passes can reason about them. The low IR has none left: a class is a
size and offsets, a list read is an address computed and loaded, a count is an add, a check is a compare and a branch
to a block that halts. It decides nothing about the program. Every choice it carries out (a check left out, an object in
the frame, a lock taken once, a loop run in pieces) was made by a high pass, and the low IR only spells it out, so a
code generator never needs to know what Spite is.

It is held in flat arrays (functions, blocks, instructions, registers), not as objects pointing at each other, so a
backend walks it fast. It has no text form except what its tests need ([D561](../DECISIONS.md)).

## The form

**Machine types.** A register or a memory access has one of: `i8`, `i16`, `i32`, `i64` (whole numbers, with the
signedness in the instruction that needs it, not in the type), `f32`, `f64`, and `address`. A Boolean is an `i8` that
holds 0 or 1.

**Functions.** A function has a name, its parameters and answer as machine types, its blocks, its registers, and its
frame: the slots for the objects and lists the placement pass put in the frame, each with its size and alignment.
Registers are virtual and in SSA form; a block's parameters carry the values that differ by path.

**Instructions.**

| Group | Instructions |
|---|---|
| Constants and moves | a constant of a machine type; the address of a function, a frame slot, or a constant in the program's data |
| Arithmetic | add, subtract, multiply, divide and remainder (signed or not), the same with an overflow result for checked arithmetic, negate, and, or, exclusive or, not, shifts; the decimal operations; conversions between the types |
| Comparisons | equal, not equal, less, less or equal, signed or not, and the decimal ones, each giving an `i8` |
| Memory | load and store of a machine type at an address plus an offset; an address plus an index times a size; copy and fill a run of bytes |
| Atomics | atomic load, store, add, exchange and compare-and-exchange, each with its ordering |
| Calls | a direct call, a call through an address, and a call into a system or foreign library with that library's calling convention |

**Terminators.** `jump` to a block with its arguments, `branch` on an `i8`, `switch` on a whole number to a table of
blocks, `return` with an optional value, and `halt` with a site: the program stops and reports from the site table, as
every crash does.

**Data.** The program's constant data: texts, tables the arrangement pass decided are constants, the site table, the
crash texts, and the static objects of singletons that hold nothing.

## Lowering to it

The high IR after its passes is lowered one function at a time:

- **Layouts** become sizes, alignments and field offsets, in the arrangement the passes chose.
- **Calls of intrinsics** become their instructions: `Integer.sum` with no proof becomes an add with an overflow result
  and a branch to a halt at its site; with the form `unchecked` it is a plain add.
- **Counts** become the add and the test that frees at zero, atomic when the ownership pass left a class's counts
  shared between threads; a `make` with the form `frame` is a frame slot, otherwise a call of the allocator.
- **Waits** become the state machine of their function: its state in an object, a switch on where it resumes.
- **Threads** become calls of the library's thread functions, as the concurrency pass decided.

A high IR construct this step cannot lower is refused by name, never lowered as something else
([D244](../DECISIONS.md)).

## What reads it

- **Our backends.** Instruction selection to the target's instructions, register allocation (linear scan in a
  development build, a better one in an optimised build that also uses the facts the passes proved), the encoder
  (`compiler/source/backend/x86/` for x86-64), our object format, our linker, and the system's executable writer.
- **The C printer.** For a system without a backend, each function is printed as C, one statement per instruction, and
  compiled by a C compiler. It decides nothing either: it prints what the low IR says.
