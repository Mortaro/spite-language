# The desugared high IR, and C written only from it (D587)

Mortaro, 2026-10-14: the high level is only useful to optimise when it is **desugared** (every operator is a call to
its function) and **monomorphised** (every template, codegen value and specialisation is resolved, so every function
and type in it is concrete). Then a pass can see what is the same and collapse it, and what can be optimised, without
special cases per syntax. And the pipeline is built whole and switched over, not grown a slice at a time: the work is
done when every line of C comes from it and the generator is deleted.

## The pipeline

```
typed tree (Analysis)  ->  lowering  ->  high IR  ->  passes  ->  C printer  ->  C
```

1. **Lowering** walks the typed tree once per concrete function and writes the high IR. It desugars and
   monomorphises; it decides nothing about memory, layout or checks.
2. **Passes** each read and rewrite the high IR, in a fixed order (below). Every decision the generator makes today
   is one of them. No pass reads C.
3. **The C printer** writes the program from the high IR and nothing else: types and layouts, every function, the
   intrinsics' C, the runtime support the program reaches, and `main`. It decides nothing either.

## The form

**A program** is its concrete classes (each a layout of fields), its concrete functions, its globals (singletons)
and its sites (where a crash or a check reports from). Nothing in it is generic.

**A type** is `Analysis.SpiteType` after monomorphisation: a scalar kind, a concrete class (a list of `Integer` is the
class `List_Integer`, made from `library/list.spite` like any other), a nullable, a union, an enum, a function value,
or nothing. The front end's types are reused, never mirrored.

**A function** is concrete: its C name, its class, its parameters (each with its ownership, decided by a pass), its
answer type, its blocks, and its values. A function made from a template or for a codegen value is its own function,
named by the specialisation. An **intrinsic** is a function with no body whose meaning the printer knows (below).

**A block** has parameters (the SSA form's phis: a value that differs by the path taken arrives as a block parameter,
and every jump passes the values for them), a list of operations, and one terminator.

**An operation** makes at most one value. There are few kinds, so every pass handles all of them:

| Kind | What it is |
|---|---|
| `constant` | a literal of a concrete type |
| `call` | a call of a concrete function (an intrinsic included) with its arguments in order, the receiver first |
| `call value` | a call through a function value |
| `load` | a field of an object (or of a value class held in place) |
| `store` | a field written |
| `make` | a new object of a concrete class, not yet constructed |
| `retain`, `release` | a count up or down, inserted only by the ownership pass |
| `site` | a check that halts with a report: overflow, bounds, narrowing, division, `crash` |

**A terminator** is `jump` (to a block, with its arguments), `branch` (on a Boolean, to two blocks), `switch` (on a
tag), `return`, or `halt` (a `crash` that always halts).

## Desugaring

| Written | High IR |
|---|---|
| `a + b`, `a < b`, `a == b` on numbers | `call Integer.sum(a, b)`, `call Integer.less_than(a, b)`, `call Integer.equals(a, b)` (intrinsics) |
| the same on a class | `call Coins.sum(a, b)`, `call Coins.less_than(a, b)` (the class's own functions) |
| `a != b`, `a >= b`, `a <= b` | `not` of `equals`, `less_than` or `greater_than` |
| `a and b`, `a or b` | branches |
| `"{x} items"` | `call String.join(...)` over the parts, each part through its `to_string` |
| `x.field` with a getter, a setter | `call get_field`, `call set_field` |
| `list.each_bump()`, `each`, `filter`, `sum`, a fused chain | the loop over the list's items, with the calls it makes |
| `switch` | `switch` on the tag, or branches |
| `assert c` | a branch to a block that returns the default |
| `crash c` | a branch to a `halt` with its site |
| `if value` on a nullable | `call Nullable.has(value)` and the narrowed value |
| a template, a codegen value `$T`, a member template | the concrete function it makes, called directly |
| reflection read while compiling | its constant |

## The intrinsics

The numbers' operations, the memory primitives the library is written on (`Memory`, `TypedMemory`), atomics,
foreign calls, and the few runtime calls (`spite_overflowed` and the other reports). Each is a function with a name
and a type and no body; the printer writes its C, and a pass may decide its **form** (an overflow check dropped
because a range proof holds, a bounds check dropped because the index is proven inside). The form is a field of the
operation, set by a pass, never a different operation.

## The passes, in order

1. **Specialise and inline** (per call site, across the call graph).
2. **Proofs**: ranges (an overflow check that cannot fail), bounds, narrowing, nullability, constant folding of closed
   computations.
3. **Ownership**: which parameters are lent, held or owned; retains, releases and moves; copies.
4. **Placement**: an object on the stack or in a frame, pools.
5. **Layout**: columns, nested lists inline, configured copies, item values, mirrors (D520).
6. **Concurrency**: bands, rows, overlap groups, carried values, locks and their elision, count sharing.
7. **Fold** identical functions.
8. **Reach**: keep only what `main` reaches (the tree shaker, on the IR).

Every pass that exists in the generator today moves here with its documentation in `docs/optimizations.md` and
`docs/proofs.md` unchanged in what a user can observe.

## How it is built

The pieces are written in parallel, each by one worker who owns its files, against this page and the IR classes in
`compiler/source/backend/`; nothing is switched on until the whole corpus, the examples, the documentation, the
benchmarks and the compiler itself pass through it. Then the generator and every C reader are deleted in one change.

## Reading the high IR

`spite <program> --high-ir=<folder>` (the `Build` setting `high_ir`) writes the program's high IR as text, one
`<class>.ir` file per concrete class, the way `final_classes` writes the program back out as Spite: the class and its
fields, then each function with its parameters (and their ownership once the ownership pass has decided it), its
blocks with their parameters, every operation with the value it makes and its type, and each block's terminator.
Values are numbered `v<n>` per function. It is for people and agents reading what the compiler made of a program,
so it writes the IR after the passes, as the printer sees it. A function whose body is not lowered says so.

## The contracts between the pieces

The code lives in `compiler/source/intermediate/` (namespace `Intermediate`, so inside it a class is named `Function`,
`Block`, `Lowering`). The core classes there (`Lowered`, `Layout`, `Field`, `Function`, `Parameter`, `Value`, `Block`,
`Operation`, `Terminator`, `Target`, `Site`) are the contract. A piece may add a field to one of them, never rename or
remove one.

**Running it.** `Pipeline(generator).run()` lowers, runs the passes in order, and prints. While it is being built it
runs only when the `Build` setting `intermediate_c` is on, and its C replaces the generator's whole; the day the whole
corpus passes, the setting and the generator are deleted together.

**Lowering** (`Lowering`, and `Instances` for monomorphisation) writes, for every function `main` can reach:

- one `Function` per concrete function, its `c_name` the name the C uses, its receiver as value `receiver` and its
  parameters as values, and blocks in SSA form with block parameters;
- intrinsics named `<type>.<operation>`: `Integer.sum`, `Integer.subtract`, `Integer.multiply`, `Integer.divide`,
  `Integer.remainder`, `Integer.equals`, `Integer.less_than`, `Integer.greater_than`, `Integer.negate`,
  `Boolean.not`, `Integer.to_long` and the other widenings, `Integer.to_byte` and the other narrowings (checked), the
  same for every number type, `String.join`, `String.equals`, `Nullable.has`, `Nullable.value`, `Union.tag`,
  `Union.value`, and every library function whose body the compiler supplies, named by its own `c_name` with
  `intrinsic` set to `supplied`;
- `make <layout>` followed by the constructor's `call`, which is how every object is made; defaults of fields are
  stores the lowering writes after the `make`;
- every check as the intrinsic call or terminator that can fail, with its `site` (an index into `Lowered.sites`, which
  holds the path, the line, the owner, the function and the source text the report prints);
- `assert c` as a branch to a block that returns the default; `crash c` as a branch to a `halt` with its site;
- no `retain`, `release` or copy at all: lowering says what the program does, not how its memory is kept.

**Passes** read and rewrite the `Lowered` in place, each a class with `run(lowered: Lowered)`:

- the ownership pass sets every `Parameter.ownership` (`owned`, `lent`, `held`), inserts `retain` and `release`
  operations and copies, and marks a value that moves (`form` `moved` on the operation that hands it on);
- a proof sets an operation's `form`: `unchecked` on a check proven never to fail, a constant in place of a closed
  computation;
- a layout pass sets `Layout.arrangement` and the forms of the loads and stores it changes.

**The printer** (`Printer`) writes the C from the `Lowered` after the passes, and from nothing else: every `Layout`
(its struct and its allocate, init, release, free and copy functions), every `Function` (its signature from its
parameters, its body from its blocks), each intrinsic's C (from its name and the operation's `form`), each site,
the runtime support the program reaches, and `main`. It decides nothing: a check it writes is a check a pass left.
