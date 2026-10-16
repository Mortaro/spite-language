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
| `a[x]`, `a[x] = v` | `call get_at(a, x)`, `call set_at(a, x, v)` (the class's own functions; a pass decides whether a call becomes a direct read) |
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

**Nothing that is a function in the end becomes an operation of its own (D599).** What the source can say as a call
of an existing function is lowered as that call, and the passes decide what it becomes: a read of a list item is
`get_at`, a write `set_at`, never an intrinsic that reaches into the list's fields. A new intrinsic is only for what
has no Spite function under it: the floor above, and what the printer must know about C. Reflection and the REPL
follow the same rule: an attribute that stands for a list item reads and writes it through `get_at` and `set_at`.

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
runs only in a compiler built with `writes_from_intermediate` set in `compiler.spite` (never committed on), and its C
replaces the generator's whole; no `Build` setting, so no program's C moves for it (D585). The day the whole corpus
passes, the switch and the generator are deleted together.

**A store** consumes the value it stores, and the printer releases what the field held before it (the ownership pass
inserts no release for it), as the generator does.

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

## Lowering a body

The lowering walks each concrete function the program reaches, starting from `main` (a function the lowering
writes itself: it makes the launcher, stores its defaults and calls its constructor), and writes its body as blocks
in SSA form. A local is a value; where two paths meet, a local that differs arrives as a block parameter, and a
`while` carries every local its body assigns as a parameter of the block that tests the condition. A narrowed name
or path is read through `Nullable.value` (or `Union.value` for a union narrowed to one member) each time it is read.

What the lowering writes, for the passes and the printer:

- A call lists the receiver first; a store's operands are the object, then the value, with the field in `field`;
  a branch goes to its first target when its condition is true. A local's values and the block parameters that
  carry it are named after it.
- Making an object is `make`, a store for every attribute's default, then the constructor's call. A singleton is a
  call of a function the lowering writes, `spite_singleton_<class>` (or `..._with_<n>` for one keyed by literal
  arguments), with `singleton_of` set: it makes the object, and every call answers the one object.
- Numbers: `<Type>.sum`, `subtract`, `multiply`, `divide`, `remainder`, `negate`, `equals`, `less_than`,
  `greater_than`, and `<From>.to_<to>` for every conversion; a check that can fail carries its site (`overflow`,
  `division`, `narrowing`). `!=`, `<=` and `>=` are `Boolean.not` of `equals`, `greater_than` and `less_than`,
  except on `Float` and `Double`, where `<=` and `>=` are `less_than_or_equal` and `greater_than_or_equal`, since
  the negation is wrong for not-a-number. A literal that fits is a constant of the wanted type, never a conversion.
- `Boolean.not`, `Boolean.and`, `Boolean.equals`; `Enum.equals`, `Enum.to_integer`, `Enum.from_text`; `Symbol.equals`,
  `Symbol.text`; `String.join` (the parts already text), `String.equals`, `String.less_than`, `String.greater_than`,
  and inside the String class `String.bytes` and `String.stored_length`; `Reference.equals` for identity.
- `Nullable.has`, `Nullable.value` (trusted, after a narrowing the lowering restated; with a `presence` site when
  only the generator's proof says the value is there), `Nullable.wrap`; `Union.tag` (a `Symbol`, compared with a
  constant naming the member class, or switched on with targets tagged by member class), `Union.value` (with an
  `unlike` site when two values of a `type` must be of one class), `Union.wrap` (into a union or a `type`).
- `Object.copy`, `Object.deep_copy`, `Object.allocator` (the object, then the heap singleton);
  `Reflection.class`, `Reflection.attributes`, `Reflection.functions`, `Reflection.memory`, and
  `Reflection.class_object` with the class in `target`; a function value is `make Spite_Function` then
  `Function.bind(made, owner)` with the bound function in `literal` and `target`; a call through it is `call value`.
- `Foreign.library` (the library's number in `target`; its file, naming and header are in `Lowered.foreign_*`),
  `Foreign.call` (the symbol in `target`, the library's number in `literal`), `Foreign.constant`.
- `Arguments.program`, `Arguments.count`, `Arguments.get`, `Arguments.flag`.
- A call on a `type` is a `switch` on `Union.tag` with a target per member class and a default target that halts
  (its site says the value's class was not compiled for the call), so a `type` no class fits still lowers. A
  required function of a `type` read as a value binds `<type>___call_<name>`, a function the lowering writes whose
  body is that switch, to the `type` value with `Function.bind`. `to_string` and `to_debug` dispatch on any `type`
  without being required, falling back from `to_string` to `to_debug` per member.
- `Type.size` answers the C size of a `type` whose attributes are all numbers, with the `type` in `target` and its
  C struct in `literal`.
- What the generator folds while compiling is written as its answer: a dictionary local only read by literal keys
  is its entries, read where they are named; a chain of element templates is one call of the fused template the
  generator registers (`filter_is_active_then_sum_price`); `"word".pluralize()` is the inflected text; a maths
  call on constants is its value; a local bound to a reflection object is no local at all.
- `assert` returns the function's default; `crash` is a `halt` whose operands are the text, number and enum
  locals in scope, named in the site's `shown`.

## Monomorphisation

Every function the lowering reaches is one the generator already made concrete: an instance of a generic class
(`List_Integer`), a template instance, a specialised copy. The lowering asks the generator to resolve a name, a
type, a codegen value or a question folded while compiling, with the generator's own context set for the function
it lowers, so the decision is made once, by the same code, and the IR holds only its answer.

## Reflection as code

What reflection reads at run time is written as ordinary code over the library's `Spite` classes, never as an
intrinsic. A class object is a call of `spite_class_object_<label>`, a function the lowering writes with
`singleton_of` set to `Spite_Class`: it makes the `Spite.Class`, stores its flags, and fills its attribute
declarations and functions when the program asks for them. `value.attributes` and `value.functions` are calls of
`<class>___attributes(value)` and `<class>___functions(value)`, which make each `Spite.Attribute`,
`Spite.Function` and `Spite.Argument` with its constructor, store its index and value, and append it to a list. A
listed function is bound to its object with `Function.bind`, the same intrinsic a function value uses. Only what
the printer must know about C stays an intrinsic: the memory a value lives in (`Reflection.memory`) and the class
of a union's value (`Reflection.class`).

## Caching the high IR (D569)

The lowered IR is cached per function, so a check or a build relowers only what changed. What a function's IR is
depends on more than its own text, and the key says all of it:

- **The winning definition.** A class's functions can be spread over files: the class's own file, reopenings in other
  files and packages ([classes_and_files.md](../../docs/classes_and_files.md#reopening-a-class)), and the
  standard library's. Which definition wins depends on everything loaded. The key of a function is the definition
  that won, named by its file and its text, never the file a function first came from; a definition that lost is not
  the function and is never served for it.
- **The load set.** Loading more (a package, a new reopening file) can change which definition wins without
  changing any body. Every build resolves the winners first, compares them with the cache's, and relowers each
  function whose winner changed.
- **What it depends on.** The types, layouts and signatures it uses, the templates and codegen values that made it
  (monomorphisation), the facts it was lowered against (which functions wait, which classes are singletons), and the
  compiler's own version.

A stale or unreadable entry is rebuilt, never trusted (D244). `--check` stops after lowering: it parses, analyses
and lowers (from the cache where it can) and runs no pass and prints no C; the passes and the printer run only for a
build that runs.
