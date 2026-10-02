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

## How to iterate: keep the program running

Do not compile the whole program after every change. Start it once with live reload and a REPL, and let every save
swap into the running program:

```
spite game --hot-reload --repl-port=4000
```

It keeps running, even after its entry constructor returns, until a client sends `exit`. Then, for each change:

1. **Edit and save the file.** The watcher compiles only the classes that file declares and swaps them in, keeping
   every object's state.
2. **`spite connect 4000 --command="wait_reload"`** answers once the save is compiled and swapped in, with what was
   rebuilt (`rebuilt Monster`) or the compiler's errors. Never poll, never sleep: this is the wait. (`reload` swaps
   in what changed right away, without waiting for the watcher.)
3. **Ask the program instead of printing.** Every answer is one line of JSON, `{"ok":true,"value":...}`:
   - a path or a call: `player.health`, `monsters[0].roar()`, `World().entity_count()` (a singleton by name);
   - an assignment to try a value: `monsters[0].health = 5`;
   - `describe Monster` (attributes and functions), `classes`, `enums`, `attributes`, `functions`, `memory` (live
     allocations and bytes);
   - `eval stock.count() + 40` and `run stock.append(9)` compile code typed at the prompt into the running program.
4. **Debug where it goes wrong.** `break monster.spite:42` stops the program before that line; `where` says where it
   stopped, `locals` lists what is in scope, any path may start at a local's name, and `continue` goes on. `breaks`
   lists them, `clear` removes them.
5. **When a reload is held**, read why. Renaming an attribute is held because it would lose data, and the answer
   names the map to send: `reload {Hero.attributes['level']: "rank"}` keeps the value under the new name, and
   `reload {}` lets it go.

Leave the loop only when you need to:

- `spite game --check` compiles without running or writing anything, for a program that cannot stay running, or
  to see every error of the program in one run; `spite game --build` writes the executable without running it; `spite format --check game` lists files the formatter would change.
- A full run from a fresh start, and the tests (`spite tests`, [docs/testing.md](https://github.com/Mortaro/spite-language/blob/master/docs/testing.md)),
  before you call the work done.
- `--optimized` only to measure speed. A `--hot-reload` build is slower on purpose; never time one.

Every compile, a reload included, rewrites your files into the one style first: read a file back before editing it
again. Every error comes as `path:line: error: message (in Class.function)` and says what to write instead: write
that, and nothing cleverer. A failure at run time is a `spite.crash` or `spite.fault` line naming the file, the line
and the values there: open that line, and do not add logging. The commands are in
[docs/repl.md](https://github.com/Mortaro/spite-language/blob/master/docs/repl.md).

Write the plain, readable version first. Do not hand-optimise: the compiler fuses chains, specialises generics,
places memory and removes what is unused on its own.

## Bind a library instead of rewriting it

When a mature library already does the job (compression, a database, a codec, physics, a graphics API), bind it
rather than rewriting it in Spite; rewrite only what is small or what the program is about. Any library with a C ABI
can be bound: C as it is, C++ through `extern "C"`, Rust through `extern "C" fn` in a `cdylib`, Zig through
`export fn`, Go through `//export` in a `c-shared` build.

- Open it once beside the attributes: `var library = DynamicLibrary("libpack.so", 'identity', "")`, and call its
  functions as members (`library.pack_block(...)`; `_as_long`, `_as_double`, `_as_text` for wider results).
- Wrap it in a class of its own, and let nothing else touch the `DynamicLibrary`. Only Spite shapes come out: full
  word snake_case names, a Spite enum for each status (map each C number in a `switch` whose `_:` is a bare
  `crash`, so a number it does not list halts at the boundary), `T?` instead of a null handle or a not-found code,
  an object that holds the handle and gives it back in `drop()`, and `String` for text.
- What crosses is numbers, `Boolean`, text, lists of numbers and number-only `type`s; a class instance never does.
  Exceptions, generics, structs returned by value, varargs and another runtime's memory cannot cross: wrap them on
  the library's side.

[docs/foreign_libraries.md](https://github.com/Mortaro/spite-language/blob/master/docs/foreign_libraries.md#a-binding-speaks-spite)

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
