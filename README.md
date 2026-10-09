# Spite

Spite is a small, opinionated language meant to be written mostly by AI and skimmed by humans: one way to do
each thing, no macros, metaprogramming and a real standard library instead of loops. It compiles to C, and the
compiler is written in Spite and compiles itself. The compiler is also its own formatter and linter. There is
no separate style guide to follow: it rewrites your file to the one true style, and a badly named variable or
function (the wrong case, or an abbreviation such as `msg` for `message`) is a compile error, not a warning.

> **Spite: you write your intention, the compiler reminds you that you are a moron, and emits the fastest
> possible code.**

**Why Spite:** [WHY.md](WHY.md) for people, [WHY_AI.md](WHY_AI.md) for language models choosing a language.

**Built mostly by AI, decided by a person.** Nearly all of Spite's code, docs and tests are written by AI agents,
and every rule of the language is decided by its author, Mortaro. That is the experiment, not a shortcut: a
language meant to be written by AI is best designed by watching AI write it. Every mistake an agent makes is a
guardrail the language is missing, and it becomes a rule the compiler enforces, so the next writer, human or model,
cannot make it. Nothing is accepted because it looks right: the specification is normative, every decision is
logged with its reason, and every claim is a program the test suite runs.
[Written by AI, decided by a person](WHY.md#written-by-ai-decided-by-a-person) says more.

Rust has Rustaceans; Spite has morons. A moron is anyone who writes Spite (people, AI agents and the language's
own author), said with affection, and the language is built so a moron's code still comes out right and fast. In
practice:

- **You write the plain program; the compiler decides how it runs.** Lists, loops and classes say what you
  mean. Threads, waiting, memory layout, alignment and where every value lives are the compiler's choice, proven
  while compiling, with no runtime shipped beside the program, so the same language reaches from an operating
  system to a web page. A plain program slower than the same program tuned by hand is a compiler bug:
  [Write it plainly](docs/write_it_plainly.md).
- **The compiler refuses mistakes instead of guessing.** An error names the problem and the fix (a race, a
  value that may be null, a borrowed item kept too long, a misspelt or abbreviated name), and there is no
  warning to ignore: it compiles or it tells you why not.
- **Anything that can go wrong silently is a bug.** Every failure is loud (a compile
  error, or a crash that names its cause) and never a wrong value, a lost write, a skipped step, a leak or a
  hang. Something that can be absent is a `T?` you must handle; a real developer mistake crashes with the line
  that made it; a fault below Spite still prints where it happened. What the language refuses is listed in
  [docs/failure.md](docs/failure.md#nothing-fails-silently).
- **Every optimisation is written down.** What the compiler does behind your back is listed in
  [docs/optimizations.md](docs/optimizations.md), so it surprises nobody.

A file is a class named after it, and a program is a folder: `battle/battle.spite` is the entry, and its constructor
runs the program.

```gdscript title=battle/monster.spite
var name = ""
var health = 0

func Monster(new_name: String, new_health: Integer) {
    name = new_name
    health = new_health
}

func alive(): Boolean {
    return health > 0
}
```
```gdscript title=battle/battle.spite entry
var console = Console()

func Battle() {
    var monsters = [Monster("slime", 12), Monster("ghost", 0), Monster("troll", 30)]
    var standing = monsters.filter_alive().sum_health()
    console.print("health still standing:", standing)
    var troll = monsters.last()
    crash troll
    show_name(troll)
    show_health(troll)
}

func show_attribute(attribute: Spite.AttributeDeclaration<Monster>, monster: Monster) {
    console.print(attribute.name, "=", monster.attributes[attribute])
}
```
```output
health still standing: 42
name = troll
health = 30
```

Nobody wrote `filter_alive` or `sum_health`. `List` has templates, `filter_<member>()` and `sum_<member>()`, and
the compiler writes the two this program calls for `Monster`, then fuses the chain into one loop with no list in
between. `show_name` and `show_health` are the same idea turned on a class: `show_attribute` takes a
`Spite.AttributeDeclaration<Monster>`, so naming an attribute in place of `attribute` makes the compiler write a typed function
for it, one for `name` and one for `health`. A class is an ordinary object too, an instance of `Spite.Class`, so
`Monster.attributes.each(show)` walks every attribute, unrolled while compiling. Nothing is looked
up while the program runs, and whatever it does not call is not in the executable
([docs/collections.md](docs/collections.md), [docs/metaprogramming.md](docs/metaprogramming.md)).

## Reading order

[`docs/`](docs/README.md) is the tutorial: each page teaches one part of the language, and every titled program on
it is compiled and run by `bash check.sh`. The formal definition is [`specs/`](specs/README.md), one page per docs
page with every rule, edge case and exact error text. Read the docs in this order, from a first program to what the
compiler proves; each page ends with a link to the next:

1. [Getting started](docs/getting_started.md): build the compiler, run hello world, a program of two classes.
2. [Write it plainly](docs/write_it_plainly.md): why you write the plain program and the compiler decides
   threads, layout and memory, and how to write code it can make fast.
3. [Classes and files](docs/classes_and_files.md): a file is a class; what it holds and in what order;
   constructors and singletons.
4. [Programs](docs/programs.md): the entry file, the launcher, `Arguments`, `Environment` and `Build`.
5. [Values and types](docs/values_and_types.md): numbers and the casting rule, `String`, enums, unions, `type`
   shapes.
6. [Nullable values and failure](docs/failure.md): `T?` and narrowing, `assert`, `crash`, and nothing else.
7. [Functions and operators](docs/functions_and_operators.md): function values, variadic arguments, operators
   as functions.
8. [Control flow](docs/control_flow.md): `if`, `while` (the only loop), `switch`.
9. [Style](docs/style.md): the compiler is the formatter and the linter (names, comments, nothing unused).
10. [Memory](docs/memory.md): reference counting, `copy`, `drop`, `Memory.Address`, `Memory.Heap` and choosing
   an allocator.
11. [Metaprogramming](docs/metaprogramming.md): Symbol codegen, generics and codegen values, tree shaking.
12. [Reflection](docs/reflection.md): `Spite.Class`, `Spite.Function`, namespaces, instances.
13. [Packages](docs/packages.md): `load`, namespaces, reopening classes (mods).
14. [Concurrency](docs/concurrency.md): `Concurrent` and `Parallel`, without `async`/`await`.
15. [Standard library](docs/standard_library.md): `String`, files, folders, processes, the console, sockets.
16. [Collections](docs/collections.md): `List`, `Dictionary` and member templates.
17. [JSON and binary](docs/json.md): any value to JSON text or compact bytes and back.
18. [Time](docs/time.md): instants, durations, the calendar and time zones.
19. [Game maths](docs/game_maths.md): `Vector2` to `Vector4`, `Matrix3`, `Matrix4`, `Quaternion`.
20. [Foreign libraries](docs/foreign_libraries.md): `DynamicLibrary`, libraries in C, C++, Rust, Zig and Go, bindings that speak Spite, and one folder per operating system.
21. [Targets](docs/targets.md): the web and isomorphic classes.
22. [The compiler](docs/compiler.md): every command and flag, and where the outputs go.
23. [The REPL and live reload](docs/repl.md): inspect and change a running program.
24. [Testing](docs/testing.md): a test is a function that crashes.
25. [Optimizations](docs/optimizations.md): everything the compiler optimises without being asked.
26. [Proofs](docs/proofs.md): every fact the compiler proves while compiling, and when it does not apply.

Writing Spite with an AI? Give it the skill in [skills/spite/](skills/spite/SKILL.md), the whole language on one
dense page: copy that folder into your project's `.claude/skills/spite/` (or paste `reference.md` into any model's
context).

## Status

The docs describe the language as decided; [design/status.md](design/status.md) says, page by page, what is not
built yet. In short:

| Area | Status |
|---|---|
| Files as classes, `singleton` and `generic $name` header lines, enforced file order | implemented |
| Values, numbers as classes (`this`, casts by assignment), `T?` narrowing, `assert` and `crash` | implemented |
| Functions as values, variadic `...args: List<T>`, operators as functions | implemented |
| Enums, unions, shapes (`type`), `value == Class` tests | implemented |
| Metaprogramming: templates whose name carries the member, walks over reflection objects folded while compiling, compile-time type tests, fused member-template chains | implemented |
| Reflection: every class an instance of `Spite.Class` (with `Spite.Namespace`, `Spite.Function`, `Spite.Attribute`, `Spite.Argument`), get-only members | implemented |
| Memory: reference counting, the `Memory` namespace (`Memory.Address`, `Memory.Heap`, `Memory.Arena`), an allocator per object, `TypedMemory`, tree-shaken output | implemented; `Vector<T>` and `Memory.Frame` planned |
| `Build` (compile time) and `Environment` (run time), the visible launcher | implemented |
| Standard library in Spite: `String`, `List`, `Dictionary`, `JsonWriter`/`JsonReader`, `BinaryWriter`/`BinaryReader`, `File`, `Directory`, `FileSystemWatcher`, `Process`, `Program`, `Console`, `Socket`, time (`Instant`, `Date`, `TimeZones`) | implemented |
| Concurrency: `Concurrent` (compile-time state machines, hidden async IO) and `Parallel` (the thread pool), singletons made safe by the compiler | implemented on Windows; checking what a `Parallel` function reaches is not built |
| Foreign libraries (`DynamicLibrary`), one folder per operating system | implemented; Linux and macOS folders compile but have never run |
| REPL: `--repl`, `--repl-port`, `spite connect`; live reload (`--hot-reload`) | implemented; live reload runs on Windows ([docs/repl.md](docs/repl.md)) |
| Self hosting | done: the compiler is Spite, and the only hand-written C is `bootstrap/source/generation/prelude.spite` |
| Web target, isomorphic classes | planned ([docs/targets.md](docs/targets.md)) |

Decisions waiting on Mortaro are collected in [`mortaros_missing_decisions.md`](mortaros_missing_decisions.md).

## Build and run

Spite compiles itself. `bootstrap/seed/<system>/spite_compiler.c` is the committed fixpoint C for Linux and for
Windows, so building the compiler needs nothing but a C compiler.

```
cc -O2 -Wno-parentheses-equality bootstrap/seed/linux/spite_compiler.c -o spite -lm
./spite examples/hello
```

Check the compiler: `check.sh` builds the seed, requires generation 2 and generation 3 to be byte identical,
runs every program in `conformance/` requiring exact output and balanced allocations, and reports whether the
committed seeds are current (`bash check.sh --update-seed` refreshes every one of them, from either system). It uses the first of `cc`, `clang` or
`gcc` it finds, or whatever `CC` names.

```
bash check.sh
```

## Write your own

Build the compiler once, then point it at a program's folder:

```
cc -O2 -Wno-parentheses-equality bootstrap/seed/linux/spite_compiler.c -o spite -lm
export CC=cc                     # the compiler shells out to this to build the C it emits
./spite path/to/folder
```

That builds the executable into `.spite/build/path/to/folder/folder.exe` in the folder you ran it from, and runs
it: nothing is written beside the source. The compiler reads the whole program first and then produces every
output asked for: `--check` only checks that it compiles and writes nothing, `--build` builds the executable
without running it, and `--executable-path=` puts it somewhere else ([docs/compiler.md](docs/compiler.md)).

On Windows the C compiler usually lives inside Visual Studio rather than on `PATH`, and its path
contains spaces, so use the short form:

```
CL="/c/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/Llvm/x64/bin/clang.exe"
"$CL" -O2 -Wno-parentheses-equality -Wno-deprecated-declarations bootstrap/seed/windows/spite_compiler.c -o spite.exe
export CC="$(cygpath -d "$CL") -Wno-deprecated-declarations"
./spite.exe path/to/thing
```

**Put each program in its own folder.** `spite arena` loads the whole `arena` folder ([docs/packages.md](docs/packages.md)), so
every `.spite` file in it is part of the same program.

**Save as UTF-8 without a byte order mark.** The lexer rejects a file that starts with one, and PowerShell's
`Set-Content -Encoding utf8` writes one by default: use `-Encoding utf8NoBOM`, or an editor set to UTF-8
without BOM.

**`docs/` is the ground truth for what the language is**: every titled program in it, and every program in
`conformance/` and `examples/`, is compiled and checked by `bash check.sh`. What is decided but not built yet is
listed in [design/status.md](design/status.md).

## Where to look

- [`RESEARCH.md`](RESEARCH.md): if you have agent time to donate, the research Spite needs and how to contribute
  findings.
- [`AGENTS.md`](AGENTS.md): how to work in this repository (gitmoji commits, how decisions are recorded, and the
  conventions for shared files when more than one agent is running).
- [`SPITE.md`](SPITE.md): things that cause Mortaro spite, with what to do instead. Read it before proposing
  a language feature or a way of working; it is the point of the project.
- [`docs/`](docs/README.md): the tutorial, for people learning the language: one page per topic that teaches it.
  Every titled Spite code block in `docs/` is compiled and checked as part of `bash check.sh`.
- [`specs/`](specs/README.md): the language specification, normative: one page per docs page, with every rule,
  edge case and exact error text. When anything else disagrees with the specification, the specification wins.
- [`design/`](design/): for the people building Spite: the [decision log](design/decisions.md), what is
  [not built yet](design/status.md), [open questions](design/open_questions.md), proposals,
  [known issues](design/KNOWN_ISSUES.md), and [how the compiler builds itself](design/self_hosting.md).
- [`skills/spite/`](skills/spite/SKILL.md): the skill an AI loads before writing Spite, with the whole language on
  one dense page ([reference.md](skills/spite/reference.md)).
- [`WHY.md`](WHY.md), [`WHY_AI.md`](WHY_AI.md) and [`llms.txt`](llms.txt): why to use Spite, for people and for
  language models.
- [`PLAN.md`](PLAN.md): implementation milestones, decisions made where the docs were silent, and what is
  left.
- [`bootstrap/COMPILER_PLAN.md`](bootstrap/COMPILER_PLAN.md): the compiler's own plan and progress log: what it
  implements today, and what it does not.
- [`examples/`](examples/): idiomatic sample programs the end-to-end test suite also runs.

## How to contribute

Spite is built mostly by AI agents, so the most useful thing to give is agent time. [RESEARCH.md](RESEARCH.md) lists
the research Spite needs (closing the gap to C tuned by hand, layout, cores, the compiler's own backend) and how to
hand back what you find: a finding in the research notebook, a benchmark case, or a pull request whose commits
name the model that wrote them. Read [AGENTS.md](AGENTS.md) before changing the compiler.
