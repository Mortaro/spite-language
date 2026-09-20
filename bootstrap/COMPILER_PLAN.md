# The Spite compiler: plan and progress log

The compiler is written in Spite and compiles itself. This file is the single source of truth for how it is built,
how to work on it, what it supports, and what comes next. Every working session reads it first and appends to the
progress log before it ends. `manual.md` is the language; this file is the compiler.

## How it builds itself

- `bootstrap/spite_compiler.spite` is the entry class; `load("source")` brings in `bootstrap/source/`.
- `bootstrap/seed/spite_compiler.c` is the C the compiler emits for itself. Any C compiler turns it into a working
  Spite compiler (see `bootstrap/seed/README.md`); that compiler then compiles the Spite sources again.
- **The fixpoint is the law:** the compiler built from the seed (generation 2) and the compiler built by generation 2
  (generation 3) must emit byte identical C for the compiler's own sources. A change that alters how the compiler
  compiles its own source needs one extra generation to settle, which `check.sh` tries on its own.
- The compiler's sources may only use what the CURRENT seed supports. To use a new feature inside the compiler:
  add it, run `bash check.sh --update-seed`, and only then use it.

## How to work on it

- `bash check.sh` is the green bar: builds the seed with `zig cc`, proves the fixpoint, runs every program under
  `conformance/` (exact output and balanced `allocations == frees`) and every program under `diagnostics/` (exact
  compile errors). It leaves the freshly built compiler at `.spite-cache/spite_development.exe`.
- `bash check.sh --update-seed` refreshes the committed seed after an intended compiler change.
- One feature = one conformance (or diagnostics) program = one commit. Run `check.sh` per feature, not per file.
- `conformance/<stage>/<name>/<name>.spite` plus `expected_output.txt`; `diagnostics/<name>/<name>.spite` plus
  `expected_errors.txt`; either may carry `flags.txt` with compiler flags such as `--environment=server`.
- `scripts/patch_tool.py` applies a file of OLD/NEW blocks; `scripts/regenerate_type_shape.py` regenerates the
  `SpiteType` union and its narrowing helpers when a kind of type is added.

## Where things live

- `bootstrap/source/syntax/`: lexer, token kinds, the AST (one class per node, unions `Expression`, `Statement`,
  `Type`, `GenericArgument`), parser, tree printer. Every statement carries its `line`; a source file its `path`.
- `bootstrap/source/discovery/`: finds the entry folder, `load("...")` roots and the `library/` root, maps folders to
  namespaces, parses every file, merges reopened classes in load order.
- `bootstrap/source/analysis/`: `SpiteType` (a union of `Types.*`), `ClassInfo`, `FieldInfo`, `FunctionInfo`,
  `ParamInfo`, `EnumInfo`, `UnionInfo` (also used for `type` shapes), `TemplateInfo` (Symbol codegen), and the
  `AstShape`/`TypeShape` narrowing helpers.
- `bootstrap/source/generation/generator.spite`: the code generator. `CodeBuilder` keeps five output sections
  (typedefs, prototypes, support bodies, user bodies, main); `Scope` tracks locals, overrides and owned values.
- `library/`: the standard library written in Spite, loaded into every program (`Spite.Class`, `Spite.Attribute`).
  New standard library classes are written here first; C in `runtime/` is the fallback for what Spite cannot
  express yet (D14).
- `runtime/`: `spite_runtime.h` (strings, reference counting header, debug allocator), `system_*.h` (the C of
  `File`, `Directory`, `Process`, `Program`, `Arguments`), `spite_repl.h` (not wired in yet).

## Design notes that are not obvious from the code

- Every non scalar value is a heap object with `SpiteHeader { ref_count; class_id }`. One counter
  (`allocate_class_id`) numbers classes, lists, dictionaries, object literal classes and generic instances.
- Ownership: an `ExpressionResult` states whether it owns its value (`owning_override`); `owns(result, ast)` falls
  back to the expression's shape only when it does not. Callees consume their arguments; `get`/`first`/`last`
  return retained values; functions that only borrow go through `generate_borrowing_call`.
- A union is `typedef void* Name` and its tag is the object's `class_id`. `switch` narrows by redeclaring the
  subject in the case scope with a cast. A `type` shape is a union whose members are admitted structurally at the
  first cast; its readers, writers and release function are emitted at the very end so late members are included.
- Symbol codegen functions are stored as templates and instantiated per called name; functions created while
  another is being emitted are emitted afterwards (`emit_pending_functions`).
- Generic classes: the template is never resolved or emitted; `instantiate_generic` builds one class per distinct
  argument list and resolves it on the spot, saving and restoring the generator's current context.
- Errors: `report()` collects up to 25 `path:line: error: message (in Class.function)` lines and generation
  continues after an error, so one run reports everything.

## What the compiler supports

Classes per file, namespaces from folders, `load`, reopening; attributes with defaults; functions, constructors,
the entry constructor with `Arguments`; `Int`, `Long`, `Float`, `Double`, `Bool`, `String` and the casting rule;
enums; unions with exhaustive narrowing `switch` and calls/reads directly on a union; `Nullable<T>` with `assert`,
`crash` and plain `if` narrowing; `List<T>` and `Dictionary<T>` with their methods and index sugar; String
methods; `while`, `if`/`else`, `return`; reference counting with `copy()` and `drop()`; `Console`, `File`,
`Directory`, `Process`, `Program`; `crash` (interim report line); the `assert` rules (D27 to D29); Symbol codegen;
the `List<T>` member templates (D15); reflection (`value.class`, `value.attributes`, `symbol.class`); attribute
access through `set_`/`get_`; operators as functions; `type` shapes and object literals; constructor-declared
codegen values, compiler flag values and compile-time folding (D5, D9); `--debug_memory`; errors with file and
line.

## What comes next

1. Functions in `type` shapes (D16) and calls through a shape.
2. Function values: `Spite.Function<Arguments..., Return>`, `.owner`, `call_function()` (D39, D40), which needs
   the class that means "returns nothing".
3. Class-level functions (D6), singletons (D8), `Class.instances`, `Class.attributes['name']` (D11).
4. The rest of the failure model: crash ids, the `.crashes` map, the assert ring buffer (D25, D32, D33).
5. `Dictionary<T>` member templates, `deep_copy()`, the remaining numeric types.
6. The lints as compile errors: unused locals and parameters, naming, abbreviations, the comment rule (D34).
7. The formatter (the compiler rewrites sources to the one style), then `--final-classes`.
8. The command line in its final shape (`spite file.spite --optimized ...`), `--development`, the REPL, live reload.
9. Known problems: compiling the compiler itself leaks about 0.1% of its allocations (conformance programs are
   balanced, so it is a path only the large program exercises); `examples/`, `tests/` and the `docs/` samples
   predate several language decisions and need migrating or deleting.

## Progress log (newest last)

- 2026-09-20: this plan written. State: 30 conformance programs, 3 diagnostics programs, fixpoint holds. Latest additions: class reopening, the compiler's sources and `library/` carry no comments (D34), the C compiler comes from `CC` (or the first of `cc`, `clang`, `gcc`), `Program().environment(name)`. Decisions waiting to be implemented that change existing behaviour: D45 (`Monster?` replaces `Nullable<T>` and is sugar for a union with `Null`; mostly a front end and narrowing change, the representation stays a pointer), D39/D40 (function values). Queue found by running things: `examples/` and `tests/` predate several decisions and do not compile (first failure: `assert` in a function returning a bare value); `Process` does not quote its command, so a compiler path with spaces breaks unless `CC` is given as a short path.
- 2026-09-20 (tests first, PLAN.md milestone 16): `Process` quotes a command that is an existing path with spaces; D45 landed (`Monster?`, `switch` with `<Type>` and `Null` cases, the old spelling is a parse error, every source migrated; internally a `?` type is still `NullableType`, built by `Parser.with_optional_suffix`); a crash reports `spite.crash<TAB>path:line<TAB>Class<TAB>function` (16b); `check.sh` accepts programs that are meant to crash (`crashes.txt`), runs `examples/*/` with the corpus standard (16a: all nine examples migrated and balanced; the stale `tests/` fixtures were deleted), and runs the first test package `tests/tests.spite` (16c: 21 `test_` functions, `crash` as the only assertion, hand listed in the entry class). Unqualified class names resolve by walking up the namespaces, so a folder's entry class is visible to its siblings. State: 33 conformance + 9 examples, 4 diagnostics, tests passing, fixpoint holds. Next: 16d needs `instance.functions` and a callable function value; the smallest honest version is `Spite.Function` for zero argument functions that return nothing (`.name`, `.owner`, `call_function()`), which is a subset of D39/D40 and should be designed with them rather than bolted on. 16e (condition text, operand values, crash ids, the `.crashes` map) needs expression source text, which the AST does not keep yet: the lexer has token positions, so the parser can record the start and end token of a condition.
- 2026-09-20 (16d and half of 16e): a crash line ends with the condition's source text, rebuilt from its tokens by `Parser.source_text_between`. `Nothing` (D48), `Spite.Function` and `Spite.Argument` are library classes; `instance.functions` builds a `List<Spite.Function>` (`<Class>_functions`, generated only where used) with `.name`, `.arguments`, `.returns`; the compiler appends three hidden C members to `Spite_Function` (`spite_owner`, `spite_release_owner`, `spite_call`): the owner is retained by the function value and released with it, and `call_function()` calls through `spite_call`, which is only set for functions that take nothing and return `Nothing`. That is the subset of D39/D40 test discovery needs; the rest (typed `Spite.Function<Arguments..., Return>`, passing a function as a value by naming it, `.owner` as a readable attribute, calling with arguments) is not started. `tests/tests.spite` discovers its tests with it. State: 34 conformance + 9 examples, tests, 4 diagnostics.
