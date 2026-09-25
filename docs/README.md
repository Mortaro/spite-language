# Spite documentation

Spite is a small, opinionated language meant to be written mostly by AI and skimmed by humans. There is one way
to do each thing, no macros, and metaprogramming instead of loops wherever it reads better. It compiles to C, and
its compiler is written in Spite.

These pages describe the language as it is. Every titled Spite program on them is real: `bash check.sh`
extracts each one, compiles it, runs it and compares what it prints with the output written under it -- or
checks that it fails with the error written under it -- and requires its memory to balance. If it is on these
pages, it works. Where something is decided but not built, the page says so in a line and links the
[manual](../manual.md), which is the normative reference and wins wherever the two disagree.

The code blocks are fenced as `gdscript` because GitHub cannot highlight Spite yet and GDScript's highlighter
reads it best (`func`, `var`, `name: Type`, `#` comments). The code is Spite throughout.

## Reading order

**Getting started**

1. [getting_started.md](getting_started.md) -- build the compiler, run hello world, a program of two classes.

**The language**

2. [classes_and_files.md](classes_and_files.md) -- a file is a class; what a file holds and in what order;
   constructors, singletons, `this`, references.
3. [programs.md](programs.md) -- the entry file, the launcher that loads a program, `Arguments`, run-time settings
   (`Environment`) and compile-time settings (`Build`, `target_operating_system`).
4. [values_and_types.md](values_and_types.md) -- numbers and the casting rule, numbers as classes, `String`,
   enums, unions, `type` shapes and duck typing.
5. [failure.md](failure.md) -- `T?` and narrowing, reading with `[]`, the three failure outcomes, `assert`
   guards, `crash` and its report.
6. [functions_and_operators.md](functions_and_operators.md) -- functions, function values, variadic arguments,
   operators as functions, attribute interception.
7. [control_flow.md](control_flow.md) -- `if`, `while` (the only loop), `switch` with `_:`, and `value == Class`.
8. [style.md](style.md) -- the formatter and the lints: names, comments, blank lines, one call per line, and the
   short forms the compiler insists on.
9. [memory.md](memory.md) -- reference counting, `copy`, `drop`, cycles, `Memory`, `TypedMemory`, and writing
   your own container.
10. [metaprogramming.md](metaprogramming.md) -- Symbol codegen, templates over another class, the plural walk,
    generics and compile-time type tests, tree shaking.
11. [reflection.md](reflection.md) -- `Spite.Class`, `Spite.Function`, namespaces, instances; read-only by design.
12. [packages.md](packages.md) -- `load`, namespaces, reopening classes (mods), the reserved `Spite` namespace.
13. [concurrency.md](concurrency.md) -- `Concurrent` and `Parallel`, and waiting without `async`/`await`.

**The standard library**

14. [standard_library.md](standard_library.md) -- every class at a glance; `String`, files, folders, processes,
    the console, the program, sockets.
15. [collections.md](collections.md) -- `List` and `Dictionary` as library code, member templates, chains that
    run as one loop, templates of your own.
16. [json.md](json.md) -- `Json<T>`: any value to JSON text and back.
    [time.md](time.md) -- instants, durations, the calendar, time zones as presentation, and ISO 8601 text.
17. [foreign_libraries.md](foreign_libraries.md) -- `DynamicLibrary`, and how each operating system's folder
    reopens the classes it changes.

**Tooling**

18. [compiler.md](compiler.md) -- every command and flag, the outputs and where they go, `--final_classes`, `--development`.
19. [repl.md](repl.md) -- the local and remote REPL, `spite connect`, and a replayed debugging session.
20. [testing.md](testing.md) -- a test is a function that crashes; the test package finds them itself.
21. [self_hosting.md](self_hosting.md) -- how the compiler builds itself, and what proves it.
22. [KNOWN_ISSUES.md](KNOWN_ISSUES.md) -- where the compiler falls short of the manual today.

**For AI writers**

23. [for_ai_writers.md](for_ai_writers.md) -- the whole language on one dense page. Paste it into an AI's context
    before it writes Spite.
