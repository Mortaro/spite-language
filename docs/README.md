# Spite documentation

Spite is a small, opinionated language meant to be written mostly by AI and skimmed by humans. There is one way
to do each thing, no macros, and metaprogramming instead of loops wherever it reads better. It compiles to C, and
its compiler is written in Spite.

**These pages are the language's definition, not only its tutorial.** Each page teaches its part of the language
first and then states it in a closing **Rules in full** section: every rule, edge case and exact error text. There
is no other reference to check a page against: the rules on the page decide, and where the teaching above them
disagrees, the teaching is the bug.

Every titled Spite program on these pages is real: `bash check.sh` extracts each one, compiles it, runs it and
compares what it prints with the output written under it (or checks that it fails with the error written under
it), and requires its memory to balance. If it is on these pages, it works.

The code blocks are fenced as `gdscript` because GitHub cannot highlight Spite yet and GDScript's highlighter
reads it best (`func`, `var`, `name: Type`, `#` comments). The code is Spite throughout.

## Philosophy

- **The moron writes the plain program; Spite decides how it runs.** You write lists, loops and classes that say
  what the program means. Threads, waiting, memory layout, alignment and where every value lives are chosen by the
  compiler from what it proves about your code, and none of it is your job:
  [Write it plainly](write_it_plainly.md). A plain program that is slower than the same program tuned by hand is a
  bug in the compiler.
- Everything that can be decided while compiling is decided while compiling. Nothing runs beside a program to
  make it fast, so the same language builds an operating system, firmware for a small chip, a WebAssembly module
  for a web page, a server or a game.
- Spite is written mostly by AI and skimmed by humans. Code must read easily; it does not need to be pleasant to type.
- The language is extremely opinionated and small. There is one way to do each thing. There are no macros and no clever features.
- Readability comes from metaprogramming and the standard library: prefer `repositories.filter_active().count_stars()` over loops and ifs.
- No abbreviations, anywhere, except the language keywords themselves (`var`, `func`, `enum`).
- Compilation speed and live reload matter more than anything else in the toolchain.
- Spite compiles to C. The compiler is written in Spite and compiles itself, and so is the standard library:
  [Pure Spite](standard_library.md#pure-spite-dissolving-the-runtime).

## Reading order

Read the pages in this order the first time; each page assumes the ones before it and ends with a link to the next.

**Getting started**

1. [getting_started.md](getting_started.md): build the compiler, run hello world, a program of two classes.
2. [write_it_plainly.md](write_it_plainly.md): why you write the plain program and the compiler decides how it
   runs, what it does with it, and how to write code it can make fast.

**The language**

3. [classes_and_files.md](classes_and_files.md): the lexical rules; a file is a class; what a file holds and in
   what order; constructors, singletons, `this`, references.
4. [programs.md](programs.md): the entry file, the launcher that loads a program, `Arguments`, run-time settings
   (`Environment`) and compile-time settings (`Build`, `target_operating_system`).
5. [values_and_types.md](values_and_types.md): variables, numbers and the casting rule, numbers as classes,
   `String`, enums, unions, `type` shapes and duck typing.
6. [failure.md](failure.md): `T?` and narrowing, reading with `[]`, the three failure outcomes, `assert`
   guards, `crash` and its report.
7. [functions_and_operators.md](functions_and_operators.md): functions, function values, variadic arguments,
   operators as functions, attribute interception.
8. [control_flow.md](control_flow.md): `if`, `while` (the only loop), `switch` with `_:`, and `value == Class`.
9. [style.md](style.md): the formatter and the lints: names, comments, blank lines, nothing unused, one call
   per line, and the short forms the compiler insists on.
10. [memory.md](memory.md): reference counting, `copy`, `drop`, cycles, `Memory.Address`, `Memory.Heap`,
   placement, allocators, `TypedMemory`, and writing your own container.
11. [metaprogramming.md](metaprogramming.md): walks over a program's classes and functions, templates whose name
    carries the member, member templates, generics, codegen values and compile-time type tests, tree shaking.
12. [reflection.md](reflection.md): every class an instance of `Spite.Class`, a class against an instance, the
    `Spite` classes and their members, what folds and what is known only at run time.
13. [packages.md](packages.md): `load`, namespaces, reopening classes (mods), the reserved `Spite` namespace.
14. [concurrency.md](concurrency.md): `Concurrent` and `Parallel`, and waiting without `async`/`await`.

**The standard library**

15. [standard_library.md](standard_library.md): every class at a glance; `String`, files, folders, processes,
    the console, the program, sockets, and the floor the library is built on.
16. [collections.md](collections.md): `List` and `Dictionary` as library code, member templates, chains that
    run as one loop, templates of your own.
17. [json.md](json.md): `JsonWriter`/`JsonReader` and `BinaryWriter`/`BinaryReader`: any value to JSON text or
    compact bytes and back.
18. [time.md](time.md): instants, durations, the calendar, time zones as presentation, and ISO 8601 text.
19. [game_maths.md](game_maths.md): vectors, matrices and quaternions for games: `Vector3`, `Matrix4`,
    `Quaternion`, boxes, frustums and rays, column-major, Vulkan clip space, and what each operation costs.
20. [foreign_libraries.md](foreign_libraries.md): `DynamicLibrary`, libraries written in C, C++, Rust, Zig and Go,
    bindings that speak Spite, and how each operating system's folder reopens the classes it changes.
21. [targets.md](targets.md): other targets, the web, isomorphic classes and the wire format.

**Tooling**

22. [compiler.md](compiler.md): every command and flag, the outputs and where they go, `--final-classes`,
    `--development`, and which build to measure.
23. [repl.md](repl.md): the local and remote REPL, `spite connect`, live reload, and a replayed debugging
    session.
24. [testing.md](testing.md): a test is a function that crashes; the test package finds them itself, runs one by name, and why crashing tests suit an AI.
25. [optimizations.md](optimizations.md): everything the compiler optimises without being asked, and what (if
    anything) you could notice.
26. [proofs.md](proofs.md): every fact the compiler proves while compiling, what each one buys, and when it does
    not apply, so you know when to write the check yourself.

---

Next: [Getting started](getting_started.md), building the compiler and running a first program.
