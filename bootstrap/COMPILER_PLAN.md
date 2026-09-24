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

- `bash check.sh` is the green bar: builds the seed with whatever `CC` names (else the first of `cc`, `clang`,
  `gcc`), proves the fixpoint, then runs every program under `conformance/` and `examples/` (exact output and
  balanced `allocations == frees`), `tests/`, every program under `diagnostics/` (exact compile errors) and every
  titled code block in `docs/`. It leaves the freshly built compiler at `.spite-cache/spite_development.exe`.
- `bash check.sh --update-seed` refreshes the committed seed after an intended compiler change.
- One feature = one conformance (or diagnostics) program = one commit. Run `check.sh` per feature, not per file.
- `conformance/<stage>/<name>/<name>.spite` plus `expected_output.txt`; `diagnostics/<name>/<name>.spite` plus
  `expected_errors.txt`; either may carry `flags.txt` with the command line after the file, such as `-- --name=server` for a program's `Environment`, and a
  conformance program may carry `input.txt` for what it reads from the console.
- A documentation program is a fenced block in `docs/` headed `spite title=<folder>/<file>.spite [entry] [error]
  [vars=name:value] [build=name:value]` (`vars` go to the program after `--`, `build` to the compiler), followed by an ```output or ```diagnostic block. `scripts/docs_corpus` writes them
  out; `check.sh` runs them.
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
enums; unions with exhaustive narrowing `switch` and calls/reads directly on a union; `T?` with `assert`,
`crash` and plain `if` narrowing; `List<T>` and `Dictionary<T>` with their methods and index sugar; String
methods; `while`, `if`/`else`, `return`; reference counting with `copy()` and `drop()`; `Console`, `File`,
`Directory`, `Process`, `Program`; `crash` (interim report line); the `assert` rules (D27 to D29); Symbol codegen;
the `List<T>` member templates (D15); reflection (`value.class`, `value.attributes`, `symbol.class`); attribute
access through `set_`/`get_`; operators as functions; `type` shapes and object literals; constructor-declared
codegen values, compiler flag values and compile-time folding (D5, D9); `--debug_memory`; errors with file and
line.

## What comes next

1. Done: functions in `type` shapes (D16) and calls through a shape.
2. Function values: `Spite.Function<Arguments..., Return>`, `.owner`, `call_function()` (D39, D40), which needs
   the class that means "returns nothing".
3. The rest of `Spite.Class`. There are no static or "class-level" functions in Spite (D6): a class is an instance of
   `Spite.Class`, so `is_singleton()`, `instances`, `functions` and `attributes` are ordinary members of `Spite.Class`,
   declared in `library/spite/class.spite` with their defaults, and a class file that writes `func is_singleton()`
   is overriding that ordinary function for its own class object. Built so far as compiler-provided answers:
   `is_singleton()` (D8), `Monster.instances`, `Spite.Class.instances` (D49), `some_class.functions`.
   **`is_singleton()` is now declared in `library/spite/class.spite`** and answers truthfully through the class
   object: the library declares `var singleton = false` and `func is_singleton(): Bool { return singleton }`,
   and the compiler fills the *data* when it writes the class object, never the behaviour. That is the shape
   the rest of D6 follows. **`functions` (D57) and `attributes` are now declared the same way**, filled when
   the class object is written -- a class-level `attributes` entry's `.value` is the field's declared default,
   read off a `_default()` instance (proposed by Claude, unconfirmed) -- and `instances` needs no change (D57:
   a live registry, tracked only when asked). Still to do: `Monster.attributes['name']`, D11's symbol-keyed
   mapping, whose shape still needs design (milestone 14).
4. `deep_copy()` and the `Dictionary<T>` member templates.
5. The formatter (the compiler rewrites sources to the one style), then `--final-classes`.
6. Done 2026-09-23: arguments for the program being run -- everything after a bare `--` (the rule is proposed,
   unconfirmed; manual.md section 13).
7. `--development`, the REPL, live reload.
8. Fixed 2026-09-23: compiling the compiler itself leaked about 0.1% of its allocations. Two causes: a
   condition, `not` or `and`/`or` operand that owned what it tested never released it (`owned_truth`), and a
   `T?` on the left of `>` in `crash_map` -- which D64 now rejects. `check.sh` builds generation 2 with
   `-DSPITE_DEBUG_MEMORY` and requires the compiler to free everything while compiling itself; the debug
   allocator's live table became a hash set so that takes seconds, not minutes.

## Milestone 10b: the design pass

PLAN.md says 10b needs a design pass before any code. This is it. The shape is already decided -- D12 names the
objects and their members, D6 says they are ordinary members of `Spite.Class` -- so what was missing was the
mechanism and the order.

**The acceptance test is `--final-classes`, so it comes first.** manual.md section 16 item 2 does not ask for
reflection to work; it asks for it to be *visible*: "real generated members, not compiler magic", emitted as
source you can read. Every `Spite.*` answer today is C written by `generate_reflection_read` and friends,
keyed on a member name at the call site, and there is no way to look at it. Until `--final-classes` exists there
is no way to tell a finished 10b from the compiler magic it is meant to replace, which makes it the first step
rather than a later convenience. It needs the tree printer (`bootstrap/source/syntax/printer.spite`), which
already exists, and the discovery pass's merged class list, which also exists.

**Then the members move into the library.** `library/spite/class.spite` declares `name` and `namespace` and
nothing else; `attributes`, `functions`, `instances` and `is_singleton()` are compiler answers that never appear
in any file. D6 says they are ordinary members with defaults that a class file overrides. The mechanism that is
missing is one thing: a member declared in a library class whose body the compiler fills in per class object.
`is_singleton()` is the smallest case and already folds; `attributes` and `functions` are the same shape with a
generated body instead of a literal. Doing this removes special cases from `generate_reflection_read` rather
than adding to it, and it does not change the language, so it is the safe half.

**`Spite.Namespace` is the half that breaks source** (D41). The manual says `Spite.Class.namespace` is a
`Spite.Namespace?`; the library class says it is a `String`, and every reader in `conformance/`, `tests/` and
`docs/` treats it as one. The class has to be written (`.name`, `.full_name`, `.parent`, `.classes`,
`.namespaces`), the chain has to end at `null`, and every reader migrates with it in the same commit, the way
D45 and D47 were done.

**Two things listed under 10b do not belong to it.** Calling a function by `Symbol` with arguments needs the
typed `Spite.Function<Arguments..., Return>` of D39/D40, which is its own item above. Defining members from
data, and hooks that run when a class is reopened, are the compile-time class generation PLAN.md already
defers under "Later, deliberately deferred" -- 10b is enumeration, identity and visibility, and those two are
generation. A respond-to check needs nothing new once `.functions` is a real member: it is
`class.functions.find_by_name('greet')` and a null test.

So the order is: `--final-classes`; the members into `library/spite/class.spite`; `Spite.Namespace` with its
migration. 11c, milestone 12's D13 and milestone 14 all wait on the second of those, not the third.

## Progress log (newest last)

- 2026-09-20: this plan written. State: 30 conformance programs, 3 diagnostics programs, fixpoint holds. Latest additions: class reopening, the compiler's sources and `library/` carry no comments (D34), the C compiler comes from `CC` (or the first of `cc`, `clang`, `gcc`), `Program().environment(name)`. Decisions waiting to be implemented that change existing behaviour: D45 (`Monster?` replaces `Nullable<T>` and is sugar for a union with `Null`; mostly a front end and narrowing change, the representation stays a pointer), D39/D40 (function values). Queue found by running things: `examples/` and `tests/` predate several decisions and do not compile (first failure: `assert` in a function returning a bare value); `Process` does not quote its command, so a compiler path with spaces breaks unless `CC` is given as a short path.
- 2026-09-20 (tests first, PLAN.md milestone 16): `Process` quotes a command that is an existing path with spaces; D45 landed (`Monster?`, `switch` with `<Type>` and `Null` cases, the old spelling is a parse error, every source migrated; internally a `?` type is still `NullableType`, built by `Parser.with_optional_suffix`); a crash reports `spite.crash<TAB>path:line<TAB>Class<TAB>function` (16b); `check.sh` accepts programs that are meant to crash (`crashes.txt`), runs `examples/*/` with the corpus standard (16a: all nine examples migrated and balanced; the stale `tests/` fixtures were deleted), and runs the first test package `tests/tests.spite` (16c: 21 `test_` functions, `crash` as the only assertion, hand listed in the entry class). Unqualified class names resolve by walking up the namespaces, so a folder's entry class is visible to its siblings. State: 33 conformance + 9 examples, 4 diagnostics, tests passing, fixpoint holds. Next: 16d needs `instance.functions` and a callable function value; the smallest honest version is `Spite.Function` for zero argument functions that return nothing (`.name`, `.owner`, `call_function()`), which is a subset of D39/D40 and should be designed with them rather than bolted on. 16e (condition text, operand values, crash ids, the `.crashes` map) needs expression source text, which the AST does not keep yet: the lexer has token positions, so the parser can record the start and end token of a condition.
- 2026-09-20 (16d and half of 16e): a crash line ends with the condition's source text, rebuilt from its tokens by `Parser.source_text_between`. `Nothing` (D48), `Spite.Function` and `Spite.Argument` are library classes; `instance.functions` builds a `List<Spite.Function>` (`<Class>_functions`, generated only where used) with `.name`, `.arguments`, `.returns`; the compiler appends three hidden C members to `Spite_Function` (`spite_owner`, `spite_release_owner`, `spite_call`): the owner is retained by the function value and released with it, and `call_function()` calls through `spite_call`, which is only set for functions that take nothing and return `Nothing`. That is the subset of D39/D40 test discovery needs; the rest (typed `Spite.Function<Arguments..., Return>`, passing a function as a value by naming it, `.owner` as a readable attribute, calling with arguments) is not started. `tests/tests.spite` discovers its tests with it. State: 34 conformance + 9 examples, tests, 4 diagnostics.
- 2026-09-20 (lints as errors): `instance.functions` now includes functions made by Symbol codegen (the lists are written at the end of generation by `emit_functions_function_bodies`); unused locals and parameters are errors unless they start with `_`, and a used `_name` is an error too (`Scope` tracks `entry_used`/`entry_checked`/`entry_lines`; narrowing redeclarations are unchecked and mark every entry of that name as used; parameters of operator, `set_`/`get_`, Symbol codegen and reopening functions are exempt); the comment rule D34 is enforced in `Discovery.ProgramDiscovery.check_comments` over the lexer's `comment` tokens, with `//` and `/* */` rejected by the lexer itself. State: 35 conformance + 9 examples, tests, 9 diagnostics. Still open from the lints: naming (snake_case, PascalCase) and the abbreviation list.
- 2026-09-20 (continuing): comment links resolve from the entry file's folder; a crash line ends with the named operands of the failed comparison and their values (`emit_crash_operands`; calls are never evaluated twice); a `type` may require functions (D16): `FieldDeclaration.is_function`, admission checks arity, argument and return types, and calls go through `<Type>_call_<function>` written at the end like the readers and writers; naming and abbreviation lints are errors (`check_value_name`, `check_type_name`, `check_abbreviations`; errors on locals only appear once declaration-level errors are fixed, because generation stops after the resolve pass when it has errors); the member templates work on `Dictionary<T>` through its values. State: 36 conformance + 9 examples, tests, 12 diagnostics. Next without decisions: `deep_copy()`, crash ids and the `.crashes` map (D32/D33), the assert ring buffer (D25), the remaining numeric types, the formatter, `--final-classes`, the command line shape, `--development`, the REPL. With decisions pending: open question 8 (getter hazard), the empty-constructor lint (D50), `Spite.Class.instances` (D49) and class-level functions (D6).
- 2026-09-20 (night): an explicit empty constructor is a compile error (D50); crash and assert sites have content-derived ids and every build writes `<output>.crashes` (D32/D33; `record_site`, `content_hash`, `crash_map`; the driver keeps the map in `last_crash_map`); a leading byte order mark is skipped; `Spite.Class.instances` lists the program's classes and a class object answers `.functions` bound to a fresh instance (`Spite_Class` has a hidden C member `spite_functions`; class object bodies are written at the end by `emit_class_object_bodies`, and the end of generation now loops class objects, functions lists and pending functions until they settle; the entry class is never instantiated this way); `tests/tests.spite` finds every class whose name ends in `Tests`; singletons (D8, `declares_singleton`, `singleton_call`, released in `main`); `Person.instances` (tracking compiled in by a `#define SPITE_TRACKS_<Class>` placed in the typedef section, so it applies however late the request is discovered). State: 41 conformance + 9 examples, tests, 14 diagnostics. Proposals of mine waiting for confirmation are in manual.md's decision log (last rows). Next without decisions: the other members of `Spite.Class` that a class may override beyond `is_singleton` (D6: they are ordinary functions of `Spite.Class`, not static ones), `Class.attributes['name']` (D11), the assert ring buffer (D25), the remaining numeric types, the formatter, `--final-classes`, the command line shape, `--development`, the REPL.
- 2026-09-20 (late night): a crash prints the asserts that failed before it from a ring of 32 (D25; `spite_assert_trace`, defined right after the runtime header); `Tiny`, `Short`, `Byte`, `UnsignedShort`, `UnsignedInt`, `UnsignedLong`; the command line takes the file on its own, defaults to running it, has `--optimized`, prints usage when given nothing and reports an unreadable file instead of crashing. State: 42 conformance + 9 examples, tests, 14 diagnostics. The compiler's own leak when compiling itself is still there (about 3,000 of 2,000,000 allocations; `--mode=tokens` and `--mode=tree` are balanced, so it is in discovery or generation; even compiling `conformance/stage1/hello` leaks a dozen objects: Strings, `ClassRefType`s, five `Token`s and one `File`). Patterns already ruled out with small programs, all balanced: unions as arguments and returns, a typed local returned as a union, early returns inside loops with owned locals, reassigning a variable that holds an object, the narrowed-getter pattern (`var value = attribute; crash value; return value`), a temporary `File(...).read()`. To find it: build the seed with `-DSPITE_DEBUG_MEMORY`, compile `conformance/stage1/hello` with `--mode=c`, and shrink the compiler run (for example return early from `Generator.generate`) until the dozen disappears.
- 2026-09-20 (before dawn): hardening. `docs/for_ai_writers.md` was rewritten to match what the compiler enforces (its inline declarations were compiled). Found by writing deliberately wrong programs, now errors and covered by `diagnostics/common_mistakes` and `diagnostics/type_mistakes`: assigning to an attribute that does not exist, assigning to an undeclared name, a call with the wrong number of arguments, and any value whose type cannot become the needed one (`cast()` ends in `report_mismatch`; numbers, strings and enum values still convert toward the left; a class converts to a union or a `type` it fits). Using a `T?` without narrowing names the three ways to narrow. State: 42 conformance + 9 examples, tests, 16 diagnostics. The best next hardening step is more of the same: write the program an AI would get wrong, and make the compiler say what to write. Known soft spots: errors inside function bodies are not reported when the declaration pass already failed; a condition that is not a `Bool` is accepted; arithmetic on a class without the operator function reports, but arithmetic between a number and a class on the right does not name the types; `return` with a value in a function that returns nothing is accepted.
- 2026-09-20 (dawn): `if` and `while` need a Bool or a value that may be null; a `return` must match its function (`diagnostics/conditions_and_returns`). To avoid a cascade, "returns nothing" is not reported once the run already has errors. State at the end of the night: 42 conformance + 9 examples, tests, 17 diagnostics, fixpoint holds, seed current, `bin/spite` verified on an example and on the tests. Nothing is half done.
- 2026-09-20 (morning): `Console` is a standard library class rather than a reserved word (D52, from Mortaro's
  inbox): registered like `File`/`Directory`/`Process`/`Program`, declared a singleton, with `read_line()` added
  and `ConsoleType` gone from the type union, so an attribute holding a `Console` is an ordinary attribute
  instead of one the generator deleted from the struct; `print`/`write`/`error` stay compiler-written only
  because the language cannot yet declare a function taking any number of arguments of any type.
  `check.sh` feeds a program an `input.txt` when one sits beside it and `/dev/null` otherwise, so a program
  that waits for input can never hang the suite. D47 landed: `enum`, `type` and `union` declarations take no
  `=` and break their lines, with the old form and a comma both parse errors naming the fix; the 24
  declarations in the tree and the three in `docs/` moved with it (two seed generations: accept both forms,
  migrate, then reject). `scripts/regenerate_type_shape.py` was several decisions out of date and wrote
  `Nullable<T>` and empty constructors back into the files it owns; it now writes the current language.
  **`check.sh` runs `docs/` as a corpus.** Every fenced block titled `folder/file.spite` becomes a program:
  it must compile, run, print its ```output block and free everything it took, or -- marked `error` -- fail
  to compile with its ```diagnostic text in the message. The extractor is itself a Spite program
  (`scripts/docs_corpus.spite`, written in an evening and balanced on its first run), so `check.sh` still
  needs nothing but a C compiler. 17 of the 44 documentation programs failed the first time. Most were stale
  text; five were the compiler not doing what the manual says, now fixed with a `diagnostics/` program each:
  a statement at file scope was parsed and silently dropped, a return type without its colon said "expected
  '{' but found 'Int'", `load("spite")` collided with the reserved namespace, the terminal-`if` lint of
  manual.md section 5 did not exist (and caught three places in the compiler's own sources on its first run),
  and `.class` read through a `type`-shaped or union value answered the shape instead of the class -- it now
  reads the object's own tag at runtime, which closes `docs/KNOWN_ISSUES.md` item 1.
  State: 55 conformance + examples, tests, 23 diagnostics, 44 documentation programs, fixpoint holds, seed
  current. Found on the way, not fixed: a program run with `--mode=run` receives no command line arguments at
  all (`Arguments.count()` is 0), so there is no way yet to pass anything to the program being run -- which is
  why `scripts/docs_corpus.spite` writes to a fixed `.spite-cache/docs`. Its output is deterministic, so two
  `check.sh` runs at once write the same bytes.
- 2026-09-20 (late morning, hardening by writing the programs an AI would write): the test package grew from 21
  to 54 tests over ten more classes -- numbers and the casting rule, `T?` narrowing every way, `copy`/`deep_copy`
  and sharing, reflection, the String methods, operators through their named functions, `type` shapes, enums,
  unions, Symbol codegen interception and `drop()`. Writing them found three gaps, each now fixed with a
  `diagnostics/` program: `copy()` existed on a class but not on a `List<T>` or `Dictionary<T>`; a name reserved
  in C (`unsigned`, `static`, `stdout`) emitted C that would not compile and is now a naming error beside
  snake_case and the abbreviation list; `value == null` said "unsupported expression kind in this stage" and now
  names the four ways to narrow. A function declared twice in one file emitted two C functions with the same
  name and is now an error that explains how reopening differs. The lexer names its errors: an unclosed piece of
  text (a string written across two lines) used to be "could not lex 'path'" with no line at all.
  Then two rounds of deliberately wrong programs, with a message rewritten for each: `&&`/`||`/`!`, `++`/`+=`,
  `? :`, `new`, `this.`/`self.`, `import`/`require`, `elif`, `class Name {`, a parameter with a default, a bare
  `print(...)`, a PascalCase name used as a value, indexing text, reading a function without its parentheses, an
  incomplete `switch` (it now names the members it has no case for), and a parse error at a line break (it says
  "the end of the line" instead of printing one). `docs/for_ai_writers.md` ends with the table of all of them.
  State: 55 conformance + examples, 54 tests, 33 diagnostics, 44 documentation programs, fixpoint holds.
- 2026-09-20 (milestone 10a, and 10b designed): `library/` is discovered before the program instead of after it,
  which had the load order backwards -- a library class would have reopened a user class rather than the other
  way round. A `spite/` folder of your own now reopens `Spite.Class` and the rest (`conformance/stage6/reopen_library`
  adds `full_name()` and every class object in the program answers it); a file under `Spite` that reopens nothing
  is an error naming the fix (`diagnostics/new_spite_class`), so the namespace stays the standard library's
  without the folder being forbidden outright. `load("spite")` stays an error on its own name, which is the one
  asymmetry and is worth revisiting: a loaded root's own name never becomes a namespace, so it collides with
  nothing. The 10b design pass is above, under its own heading. State: 56 conformance + examples, 54 tests,
  34 diagnostics, 45 documentation programs, fixpoint holds, seed current.
- 2026-09-20 (Mortaro's inbox, three notes): **D53, text is written with its values inside it.** `"hello {name}"`
  places a value in text; `{ }` holds any single expression; `\{` is a brace meant literally. The lexer splits a
  piece of text into its parts and holes, and the parser parses each hole with an ordinary sub-parser and joins
  them, so nothing downstream knows interpolation exists. Joining written text with `+` is then an error naming
  the form, checked in the parser on a `+` actually written in the source (the concatenation interpolation itself
  builds is not caught by it). Two scripted passes rewrote 1032 concatenation chains across the compiler, the
  standard library, the corpus, the tests and the documentation; 212 literal braces in emitted C were escaped
  first. Five sites needed a hand: a chain whose operand ran past a comparison, one split over two lines, and
  four holes that themselves contained a join. **D54, an `if` whose only statement is a bare `return` is a
  precondition**, an error naming `assert`, with the condition negated when the guard was not already negative
  (parenthesised whenever it has an operator in it -- `not source.length() < 3` is not what the guard meant).
  32 of them in the compiler's own sources. **D55, a function body holds no empty lines**, checked beside the
  comment rule from the gap between a newline token and the next token, so text spanning lines is not mistaken
  for one; 164 left the compiler and 25 left the documentation's samples.
  State: 57 conformance + examples, 54 tests, 37 diagnostics, 45 documentation programs, fixpoint holds.
- 2026-09-20 (`--final-classes`, milestone 10b step one; started by Codex, finished here): the flag writes every
  discovered, merged class back out as Spite source under its namespace folders, with a `SourcePrinter` over the
  AST. Three things were wrong with it as found. The committed seed had none of it, which is the whole of the
  bug reported from a real run (`could not write 'build/final-classes/nothing.spite'`): `bin/spite` builds from
  the seed, and the seed predated the recursive directory creation. `bin/spite` then wrote the output to the
  repository root rather than the caller's folder, because it `cd`s to the root and only made *file* arguments
  absolute -- `--final-classes=` and `--output=` now get the same treatment. And `if value and not condition`
  emitted `(value && ((Type)(!has_error)))`, casting a Bool to a pointer, because the "right side casts toward
  the left" rule was being applied to `and`/`or`; each side of a logical operator is now emitted as its own
  truth test, which is also what removes the last warning from compiling the seed.
  `check.sh` prints `conformance/stage6/reopen_library` and runs what came out, requiring the same output: the
  printed classes are a program, not a report, which is what makes milestone 10b's "visible, not compiler magic"
  checkable. Provenance -- which root supplied each declaration -- is not shown and cannot be a comment (D34);
  it is manual.md open question 10.
  State: 57 conformance + examples, 54 tests, 37 diagnostics, 45 documentation programs, the printed program
  round trip, fixpoint holds.
- 2026-09-20 (`--final-classes` shows the compiler's own classes too): Mortaro found that it printed only the
  classes that came from a file. It now also writes `built_in/`, one `type` declaration per class the compiler
  provides -- `Console`, `File`, `Directory`, `Process`, `Program` -- read straight from the generator's own
  system class table, so the knowledge is not written down twice. A `type` is the language's existing way of
  naming members without bodies, so the view needed no new notation.
  **What is still missing is a gap in the compiler, not in the printer.** `Int`, `String`, `List<T>` and
  `Dictionary<T>` have no `ClassInfo` anywhere: the generator knows them by name in `generate_string_method`
  and friends, so there is no table to print and no honest way to invent one. Making `Int` printable means
  making it a class, which is milestone 15c. `Console.print`/`write`/`error` are variadic and have no signature
  the language can write yet (D39). And a generic prints as its template, because instantiations are created
  during generation while `--final-classes` runs after discovery -- moving it after generation is the next step
  and would also let it show which classes survived tree shaking.
- 2026-09-20 (`--final-classes` runs after generation): it discovered and printed, which meant it showed the
  merged *source* rather than the *final classes*. It now runs the generator and reads its class table
  afterwards, so three things changed at once: a class tree shaking removed is no longer written, a generic
  template is no longer written, and each instantiation is, under `instantiated/`, named for the values it was
  given (`WeaponIntTrue`, `PairStringInt`). A class that has a file is still printed from its own source, so
  nothing written by hand is paraphrased; a class the compiler provides is a `type` view of its members.
  `Int`, `String`, `List<T>` and `Dictionary<T>` are still the outstanding gap and still not the printer's:
  they have no `ClassInfo`, so making them printable is milestone 15c.
- 2026-09-21 (milestone 10b, second step begun): `is_singleton()` stopped being an answer the generator gives at
  a call site and became an ordinary member of `Spite.Class`, written in `library/spite/class.spite` as
  `var singleton = false` plus `func is_singleton(): Bool { return singleton }`. The compiler sets that one
  attribute while it writes each class object (`emit_class_object_bodies`) and does nothing else, so
  `some_class.is_singleton()` is a real call to a function that appears in `--final-classes` and that a
  reopening can see. The pattern to copy for the rest of D6 is exactly this: **the compiler fills data, never
  behaviour.** `attributes`, `functions` and `instances` are still answered by name in
  `generate_reflection_read` and are next; each needs its value written into the class object the same way,
  which is harder than `singleton` only because the value is a generated list rather than a Bool.
  State: 57 conformance + examples, 54 tests, 37 diagnostics, 45 documentation programs, fixpoint holds.
- 2026-09-21 (D56, from Mortaro's inbox after reading a printed `type` view): a shape names the types it
  requires, not the names they are given -- `render(Int): String`. The parser was already discarding the name,
  so this only made the syntax say what the language meant; writing a name is an error that shows the type to
  put in its place. A shape also no longer carries a constructor, because an entry whose name is capitalised
  would mean requiring a class to be constructible, which is forcing a class rather than describing a shape.
  Both were found by Mortaro reading `--final-classes` output, which is the flag doing its job: the views it
  prints are now written in the same shape syntax a human would write, and the round trip proves it, because
  the new rule caught the `built_in/` views on its first run.
  Left open, and recorded as manual.md open question 11: whether a required function should instead be an
  attribute holding a `Spite.Function<...>`. It needs D39's typed function values before it can be written.
- 2026-09-21 (hardening by writing an ordinary program): a lending-library program using enums, a `T?` attribute,
  the member templates, a `Dictionary<Int>` tally and interpolation found one real bug in its first run:
  `join(separator)` was only generated for `List<String>`, so `counts.values().join(",")` emitted a call to
  `List_Int_join`, which never existed -- C that does not compile, the worst outcome available. It now generates
  for every element that becomes text (a number, `Bool`, an enum value), reusing `string_conversion`, the same
  rule `+` and `"{value}"` use. The program is `examples/library_card`, so it stays covered.
- 2026-09-21 (more ordinary programs): a shop program over two namespaces -- a union of `Stock.Item` and
  `Stock.Bundle`, a `type` both satisfy, duck-typed calls on the union, `.class.name` through it and a
  `sum_price()` member template -- ran correctly and balanced on the first try. A form program over reflection
  and Symbol codegen found one regression from making `Console` a real class (D52): `Console().print(...)` on a
  temporary was rejected, because the printing interception asked `lookup_static_type`, which does not type a
  constructor call. It now also recognises a call whose callee resolves to `Console`, and
  `conformance/stage6/console_class` covers it.
- 2026-09-21 (D57, and milestone 10b's second step continued): `Spite.Class.functions` is an ordinary attribute
  declared in `library/spite/class.spite`, filled when the class object is written, instead of a hidden C
  function pointer read by a special case in `generate_reflection_read`. Mortaro's rule settled the design
  question that was blocking it: **tree-shake it, do not make it lazy** -- `class_level_functions_used` already
  decided whether any of it is emitted, so a program that never asks has none of it (`--mode=c` on
  `conformance/stage1/hello` contains zero `spite_class_functions_`, `tests/tests.spite` contains 63).
  The one thing that had to be handled: a class object owning its functions is a cycle, because a
  `Spite.Function` holds a `Spite.Class` for `.returns`. The program's exit clears each cache's list before
  releasing the class objects, which is "clear one side" from section 10 applied by the compiler rather than by
  the programmer. `instances` needs no change: it is tracked only when asked, and stays a live registry because
  which instances exist is not a compile-time fact.
- 2026-09-22 (milestone 10b finished): the members moved into `library/spite/class.spite` -- `attributes`
  beside `functions` (D57) and `is_singleton()` (D8) -- filled when the class object is written, with a
  class-level entry's `.value` being the field's declared default read off a `_default()` instance (proposed
  by Claude, unconfirmed; D11's symbol-keyed mapping stays milestone 14). `library/spite/namespace.spite`
  writes `Spite.Namespace`, and `Spite.Class.namespace` became `Spite.Namespace?` (D41): `namespace_read_used`
  gates the fill plus the parent chain and `namespace_tree_used` gates `.classes`/`.namespaces`, so
  `--mode=c` on `conformance/stage1/hello` still contains no namespace objects. Shutdown clears each class
  cache's `namespace` field and then every namespace cache's `classes`/`namespaces` lists before releasing
  them -- the same "clear one side" as D57, with the cycle here being class <-> namespace through `.classes`.
  Five readers migrated in the same change (`every_class`, `reflection`, `console_class`,
  `tests/reflection_tests`, `reopen_library`, plus the `docs/packages.md` block), all with expected outputs
  unchanged; a static receiver (`Sticker.attributes`) now compiles through a shared `generate_member_object`
  substitution; and two conformance programs were added: `namespace_objects` and `class_attributes`.
- 2026-09-22 (Mortaro's three follow-ups to 10b): bare `class` is the instance's class -- decided: not a
  keyword, an inherited attribute of any instance -- wired into `generate_identifier` after local/attribute
  lookup and before `find_class`, returning with `owning_override` set so the retained class object is
  released by whoever reads it. The static-path substitution in `generate_member_object` lost its
  `functions`/`attributes`/`namespace` whitelist and now serves every member read and method receiver
  (`instances` stays special-cased earlier in `generate_member_read`), so `Creature.name` reads the class
  object; `generate_method_call` gained a special case so a method the class object does not have still
  reports "'Creature' is a class, not a value: write 'Creature()'" instead of naming `Spite_Class`.
  Six tests added to `tests/reflection_tests` (bare `class`, `creature.class_name()` on `tests/creature`,
  `Creature.name`/`ReflectionTests.name`, and `if`/`assert`/`crash` narrowing of `Spite.Class.namespace`) plus
  `diagnostics/namespace_nullable` for the reported shape verbatim. Suite green with `--update-seed`
  (fixpoint holds, 39 diagnostics 0 wrong), `bin/spite` rebuilt and smoke-tested: `Probe.name`, bare
  `class.name`, local narrowing and `Console.is_singleton()` all print with balanced memory.
  **Found while writing it: D43's chain form has no implementation** -- `lookup_override` is consulted for bare
  identifiers only, so `assert Spite.Class.namespace` followed by `Spite.Class.namespace.name` still errors
  (manual section 5's chain bullets over-promise; no corpus program asserts a chain). Flagged for Mortaro, not
  changed here.
- 2026-09-23 (implements D43, per Mortaro: "the chain form for a class should not be a special case at all ...
  every attribute spite adds by default is just a normal attribute so any normal rule apply"): chain narrowing
  went in as one receiver-agnostic mechanism. `Scope` gained `narrowed_paths` with `narrow_path`/
  `is_path_narrowed` (equal to a recorded path or a prefix of one, walked up the scope chain like
  `lookup_override`), and `generate_member_read` went through a wrapper that answers a narrowed path with its
  plain type -- exactly what an overridden name does -- so member reads, calls, stores and conditions all see
  an ordinary value and no site knows about paths. `try_nullable_guard` gained `try_path_guard`: a member-path
  condition generates in a throwaway scope with its strict prefixes recorded (the condition reads through
  them), tests the whole path (`!= 0` for references, `.has_value` otherwise) and, when the path owns what it
  read, tests and releases it inside the guard, so it leaks nothing however often the guard is emitted. `if`,
  `assert`/`crash` and the switch subject record the path where the narrowing belongs; the switch now asks for
  the guard before generating its subject, so a deep chain cannot read through an unproven link. Five tests in
  `tests/reflection_tests` (assert/if/crash forms, `Spite.Attribute.class.namespace` as a deep chain, a null
  path failing the `if` without dereferencing), `conformance/stage6/namespace_objects` walks a nullable prefix
  with `crash`, and `diagnostics/path_narrow_else` pins the else-branch staying `T?`. Suite green with
  `--update-seed`: fixpoint holds at generation 3, 60 conformance/examples, tests, 40 diagnostics 0 wrong, 45
  docs, seed updated; `bin/spite` rebuilt and smoke-tested. **Open for Mortaro:** the manual's examples walk up
  with `class.namespace.namespace`, but `Spite.Namespace` declares only `.parent` -- recorded in the decision
  log's open point for 2026-09-23.
- 2026-09-23 (afternoon, Claude Opus 5.5, Mortaro away): **review of cfd2694** (written by another model): its
  D43 guard tested only the last link of a path while the commit said it tested each, so `if outer.inner.inner`
  with a null middle link dereferenced null; and nothing undid a narrowed path, so assigning null to it (or
  replacing a prefix) and reading on crashed. Both fixed: `try_path_guard` walks `path_links` and joins one test
  per nullable link with `&&`; `Scope.forget_path` runs on every assignment (a value that cannot be null keeps
  the written path), and `Scope.is_loop` plus `paths_read_from_outside` make undoing, inside a loop, a narrowing
  the loop has read an error. Then **the inbox**, moved into the manual as D58-D69 and open questions 12-20, and
  built: D58 `join` is one body; D59 was already true; D60 `_:` expands to its body once per remaining member,
  narrowed to each, and repeated case bodies are errors (`report_repeated_cases`, `generate_rest_case`); D61
  `--final-classes` prints Symbol-codegen instances (`generated_functions_source`), and check.sh round-trips
  `symbol_codegen` too; D63 copy-to-narrow is an error (`collect_body_facts` scans the body first), which found a
  leaking check-on-a-non-null (now an error too); D64 `[]` answers `T?` (`List_at`), with proofs by count, by
  bound (`narrow_proven_indices`, also on the left of `and`) and by `crash`, a `Bool?` condition is an error,
  and assigning through a `T?` -- previously dropped silently -- is an error; D65 `name_with_namespaces`; D66
  every test runs twice and must leave `Program().live_allocations()` where it was; D67 file order
  (`check_order` in discovery); D69 `T? == value` without narrowing (`generate_nullable_equality`), and a class's
  `equals` is skipped for a right side of its own class. D68 is recorded as conflicting with D10, with a
  proposal, and not built. Three bootstrap detours worth knowing: `_:` needed two seed generations (the seed must
  know `_` before sources rely on per-member narrowing), and D64 needed an intermediate compiler built from HEAD
  with the crash-on-a-plain-value check relaxed, whose output became the seed. Migrations ran by script and every
  changed line was audited for replacements inside quoted text (`scratchpad` scripts are not kept; the approach
  is in the commit messages). State: 62 conformance/examples, tests (each proven leak-free), 48 diagnostics, 45
  documentation programs, printed-program round trips, fixpoint holds, seed current.
- 2026-09-23 (later afternoon, Claude Opus 5.5): **typed function values** (D39/D40): a function named without
  being called is a `Spite.Function` bound to its instance, typed by a signature carried on `ClassRefType`
  (`has_signature`, `arguments`, `returns`), called as `f(x)`, `f.call_function(x)` or through any expression
  that holds one (`listeners[index](event)`); `.owner` is still unbuilt. **The formatter** (sections 12/13):
  `SourcePrinter` became it (minimum parentheses, receivers keep theirs, `else if`, width breaking, floats as
  written, link comments kept), `Syntax.Formatter` refuses any rewrite whose fully-parenthesised program
  differs from the original, `bin/spite format [--check]`, every compile formats the program's own files
  (`--no-format`; a file with an empty line in a function is left for D55 to reject), and `check.sh` keeps the
  tree and the documentation's programs formatted. **The compiler's own leak is gone**: a condition that owned
  what it tested never released it (`owned_truth`), and `crash_map` compared a `T?` with `>` -- a hole in D64,
  now an error for every operator but `==`/`!=`. `check.sh` builds generation 2 with `-DSPITE_DEBUG_MEMORY` and
  requires compiling itself to free everything; the debug allocator's live table became a hash set (2m28s to
  under 10s). **Program arguments**: everything after a bare `--` reaches the program (proposed, unconfirmed).
  Also: `--final-classes` no longer writes a `# replaced by` comment (D34) and prints a reopened class in file
  order (D67); an assignment forgets narrowings after its store, not before. Ordinary programs written to hunt
  bugs: `examples/event_bus`, plus a grid, a shop and a generic box that all ran first time. State: 65
  conformance/examples, leak-proven tests, 50 diagnostics, 46 documentation programs, fixpoint, leak-free
  self-compile, formatted tree. **Waiting on Mortaro:** D68 vs D10, open questions 12-20, whether the REPL is
  C or Spite (section 14), and milestone 11a's decisions (singleton key, `#` binding comments vs D34, how a
  platform-specific library is tested).
- 2026-09-23 (evening, Claude Opus 5.5, Mortaro: "do not wait for me, while we have tasks open keep implementing"):
  **D70 symbols** (Mortaro's answer to D68 vs D10): `SymbolType` joined `SpiteType`; a symbol literal where a
  `Symbol` is expected is a static entry of a table written only for used symbols (`ensure_symbol`), a `Symbol`
  decays to `String` wherever text is expected (`as_text`), `Symbol(text)` searches the table
  (`spite_symbol_find`), and every reflection name -- class, function, argument, attribute, namespace -- is a
  symbol; `T? == value` also takes the `T?` on the right. **Milestone 11a/11b**: `DynamicLibrary` is a built-in
  class whose literal constructor arguments pick one library per file and naming rule; foreign calls bind
  through `spite_foreign_<library>_<symbol>` pointers resolved when the library opens; 'identity', 'camel_case'
  and 'windows' naming (the abbreviation table read backwards: `abbreviated_words`/`unabbreviated_words`);
  numbers, Bool, enums, text, lists of numbers and number-only `type` values (by address, written back) cross;
  `_as_long`/`_as_double`/`_as_text`; header constants; a `_Static_assert` layout check under 'windows'.
  check.sh builds `fixture.c` files into libraries (gitignored). **15a proposed** in manual.md section 15, with
  `Memory` and `DynamicLibrary("c", ...)` built additively. Also: `spite <folder>`; storing into a narrowed name
  writes its `T?` and un-narrows it (walking a chain used to be rejected, and `x = null` stored a default
  object); `while value` narrows its body; a getter with no attribute is a read-only attribute. Bootstrap note:
  a new built-in class's runtime C must be wrapped in `#ifdef SPITE_CLASS_ID_<NAME>` for the first seed
  generation (the old compiler does not define the id), then unwrapped.
- 2026-09-24: D76. `$name` program variables are gone: `library/environment.spite` is a singleton a program
  reopens, and discovery prepends one `name = boolean_setting("name", name)` (or `integer_setting`/`text_setting`)
  per declared field to its constructor. `Arguments()` answers `spite_program_arguments`, set first thing in
  `main`. Bootstrap note: the seed checks every library class, so `Arguments()` had to reach the seed before
  `library/environment.spite` could use it -- the seed was refreshed with the file moved aside, then it went back.
- 2026-09-24: D73 containers. `List<T>` and `Dictionary<T>` are generic classes in `library/list.spite` and
  `library/dictionary.spite`; `ensure_list`/`ensure_dictionary` instantiate them under their old C names
  (`List_String`, `Dictionary_Int`) with `instantiated_container`, and `supply_list_functions` adds the four
  typed slot functions (`read_item`, `write_item`, `release_item`, `item_bytes`) and drops `contains`/`join` for
  element types without them. `split`/`lines` are in `library/string.spite`. `resolve_discovered` finds the
  discovered classes by `first_discovered_class` now, since a container can be instantiated before them.
  Bootstrap note: the seed treated a `List` class in `library/` as a user generic, so the constructor lookup had
  to check `List`/`Dictionary` first, and the C `split` had to change its name, in a seed of their own before the
  library files could land. Compiling the compiler dropped from about 13 s to 0.7 s: `join` no longer
  concatenates piece by piece.
- 2026-09-24: the rest of D73, and D75/D78. `Console`, `String`'s methods (`library/string.spite` is a value
  class: its functions are named `SpiteString_<name>` and reach the receiver through `length()`/`code_at()`),
  number parsing and formatting (`library/number_text.spite`), and joining/comparing/slicing text
  (`library/text_bytes.spite`, over `Memory.address_of`/`compare_bytes`/`take_text`) are Spite. `runtime/` is
  deleted: `prelude.spite` holds the C the compiler still writes, and `tree_shaker.spite` drops every emitted
  function `main` does not reach (off under `--development`; it keeps its own hash table, since a `Dictionary`
  scan cost seconds). `value == Class` is a class test and `if value == Class` narrows; a one-case switch that is
  one early return is an error (D75), and so is a call repeated in every branch of an `if` (D78). Parallel
  worktree agents built floats, the debug-memory table, REPL paths, `Environment`, containers, per-OS classes
  (D80) and the `Dictionary` hash table. Bootstrap note: merging agent branches always conflicts on the seed;
  when neither side's seed compiles the merge, hide the file the old seed cannot read, build g2 and g3 by hand,
  and copy g3 over the seed.
- 2026-09-24: D77. A call passed as an argument is an error unless it is a constructor, one level deep
  (`report_nested_calls` in the generator, run on every statement and attribute value). About a thousand calls
  across the repository were moved into named `var`s first, by a throwaway pass over the parser's tree printed back
  through the formatter, then reviewed by hand; the conditions on the right of `and`/`or` and in `while` loops
  were rewritten by hand. The check went in with the rewritten sources in one step, since generation 2 checks the
  compiler's own code.
- 2026-09-24: building text is linear. `generate_store` sends `text = text + piece` and `text = "{text}{piece}"`
  on a local `String` (no piece mentioning `text`) to `emit_append_in_place`, one `SpiteString_append` per piece;
  the prelude function grows the buffer in place when the count is 1 (`capacity` is new on `SpiteString`) and
  copies otherwise. 100 000 appends went from 6.9 s to 0.23 s. A plain `Symbol` template on `List` is now an
  error naming `Symbol<$element_type>` (`register_template`).
- 2026-09-24: D82/D83/D88/D92/D98/D100/D101. `register_system_classes` is gone. `Prelude.reopened_classes()` lists
  the classes the compiler reopens and `Prelude.reopening(path)` is their Spite source (bodiless `func`s, which
  the parser now reads and `SourcePrinter` prints); discovery merges them after `library/` with the ordinary
  reopening. `resolve_function` hands a bodiless declaration to `supply_body`, which takes the C from the prelude
  or computes it per instantiation (`typed_memory_body`, `task_body`, and `instantiate_template` for a number's
  `from_type`); a body beginning `#define` is emitted in the typedef section with no prototype. Number classes are
  value classes registered by name (`value_type_named`), their functions take the C value as `self`, `this` is
  that `self`, and `generate_value_method` calls them on a scalar receiver; `string_conversion` calls the class's
  `to_string()` (D107) and `cast` calls `numeric_conversion` (cached by type pair). `List<T>` reads its elements through
  `TypedMemory<$element_type>` and `supply_list_functions` only drops `contains`/`join`. The reflection classes'
  fields became `_name` and so on, the C that fills them followed, `generate_reflection_read` defers to a
  `get_<member>` getter, and `report_private_member` guards `_` names. Bootstrap notes: renaming the reflection
  fields and adding the header lines of master needed a seed built with the conflicting library files swapped
  for the ones the old seed could read (`library/spite/*`, the number classes, `list.spite`), then two
  generations from the working tree.
- 2026-09-24: D107/D108. `string_conversion` calls `to_string`. `string_layout()` writes `struct SpiteString`, the
  `SPITE_STATIC_STRING` initialiser and `spite_static_string_empty` into a typedef slot reserved right after the
  enums (`CodeBuilder.replace_typedef`), from the attributes of `library/string.spite`; `emit_string_support`
  writes `SpiteString_init/_allocate/_make/_retain/_release/_append` and `spite_string_from_bytes` (which calls
  `Memory.text`), and every other `String` function is Spite, so `emit_text_bytes` and the `generate_string_method`
  fallbacks are deleted. `resolve_field` turns a number's `_memory` into `ClassInfo.memory_bytes`, checked by
  `check_number_memory`. A singleton with no attribute and no `drop()` (`is_stateless_singleton`) is a function-level
  static with no-op retain and release. `placement.spite` finds frame candidates per block; `placed_in_frame` and
  `freed_from_frame` write the slot and the conditional free. Bootstrap notes: the step that moved `String`'s
  functions into Spite could not be compiled by the seed over the new `library/` (its prelude and `emit_text_bytes`
  defined the same C names), so generation 2 was built from the new compiler sources over the committed `library/`,
  and generation 2 compiled the real tree; declaring the attributes first needed a seed that tolerated them.
- 2026-09-24 (D109, printing): `generate_console_call`, `printed_statement_for` and the `Console` interception are
  gone; `print`, `write` and `error` are Spite in `library/console.spite` over `...values: List<Printable>`, and
  `Prelude.console_members` supplies the bodiless `_write_output`, `_write_error` and `flush`. `shaped_value` in
  `cast` boxes a number, `Bool` or enum value that meets a shape (`ensure_box` writes one `SpiteBox_<class>` per
  kind, and `spite_box_release`; the value class gets a class id the first time it is boxed), and
  `enum_value_class` gives each enum a class holding only `to_string()`. `class_id_test` accepts a `String` by
  `class_id <= 0`, since a constant text is `-1`. A `crash` writes its operands with `crash_operand_statement`.
  Bootstrap notes: the seed could not compile the new `library/console.spite` (its floor functions were unknown to
  the old prelude), so generation 2 was built from compiler sources that knew the floor and boxing but still
  intercepted `print` whenever `Console` declared none; that compiler compiled the real tree.
- 2026-09-24 (D109, debugging): `find_or_instantiate_function` answers a missing `to_debug` with `automatic_debug`,
  a bodiless function whose supplied C calls the `text` of a `Spite.DebugInstance<T>` singleton (a class) or of a
  `Spite.Debug<T>` (anything else), found by `debug_class_of`. `instance_type_of` gives the type a class's values
  have (`List<T>` for a list instance, the enum for a synthetic class). `synthetic_value_class` makes the classes
  an enum or a `Symbol` is boxed as. `shape_call_parameter_types`/`shape_call_return_type` let a shape call
  `to_debug` without requiring it; `emit_shape_callers` and the union dispatch find each member's function with
  `find_or_instantiate_function`. `instantiate_every_attribute` skips `_` attributes of another class.
