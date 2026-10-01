---
name: spite
description: Use when writing, reviewing, fixing or explaining Spite code (.spite files), or when choosing how to build, run or debug a Spite program.
---

# Writing Spite

Spite is a small language with one way to do each thing. The compiler is also its formatter and its linter, it
never warns, and every error names the fix. So the fastest way to write correct Spite is to write the plain
version, compile, and do exactly what the compiler says.

The whole language, every rule the compiler enforces and the habits from other languages it rejects, is in
[reference.md](reference.md) beside this file. Read it before writing more than a few lines of Spite: each habit
it lists costs a compile round trip to rediscover. The full documentation is
[docs/](https://github.com/Mortaro/spite-language/blob/master/docs/README.md).

## The loop

1. Write the plain, readable version. Do not hand-optimise: the compiler fuses chains, specialises generics,
   places memory and removes what is unused on its own.
2. Compile with `spite program --run=false` (a program is a folder: `spite game`, never `spite game/game.spite`).
   Every compile rewrites your files into the one style first, so read the file back before editing it again.
3. Every error of the program comes in one run, as `path:line: error: message (in Class.function)`. Each one says
   what to write instead: write that, and nothing cleverer.
4. Run it. A failure is a `spite.crash` or `spite.fault` line on the error stream naming the file, the line and the
   values there. Open that line; do not add logging.
5. To inspect a running program, run it with `--hot-reload --repl-port=4000` and ask it
   (`spite connect 4000 --command="..."`): read any value by its path, set breakpoints, save a file to reload it.
   Every answer is one JSON line.

## Rules that matter most

- A file is a class named after the file; a program is a folder whose entry file is named after the folder. A file
  holds declarations only, in a fixed order; every `var` has a default.
- Full words only: `message`, never `msg`; `Integer`, never `Int`. `snake_case` values and functions, `PascalCase`
  types. No single letters.
- No `for`, `break` or `continue`. Prefer the list templates (`monsters.filter_alive().sum_health()`,
  `names.each(say_hello)`); a `while` that only does what a template does is an error naming the template.
- No exceptions and no error values. Absence is a `T?` the caller must narrow (`if`, `assert`, `crash`, `switch`).
  `assert condition` returns "nothing" where that is a fine answer; `crash condition` halts on a developer
  mistake. Neither takes a message: the crash report shows the values.
- Never write defensive code: no checks the compiler already proves (proving the proven is an error), no default
  that a caller could mistake for a real answer.
- No lambdas, no destructuring, no overloading, no `async`/`await`. Pass a named function; one name is one
  function; a caller makes a call concurrent with `Concurrent(f)` or `Parallel(f)`, and reading the handle waits.
- A call or a constructor is never an argument of another call: compute it into a named `var` first.
- A comment is one line outside functions and nothing but a link to a markdown heading. Function bodies hold no
  blank lines: name the part and call it.
- Every object is a reference counted by the compiler. Two objects that hold each other leak: hold the back
  reference as `Weak<T>`.
- Singletons are bound once beside the attributes (`var console = Console()`), never called inline.

## Installing this skill in a project

Copy this folder (`SKILL.md` and `reference.md`) into the project's `.claude/skills/spite/`, or into
`~/.claude/skills/spite/` for every project.
