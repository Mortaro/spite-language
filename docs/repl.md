# REPL and live reload

> **What is built:** `spite program --repl` runs the program, then answers `attributes`,
> `functions`, `help`, `exit`, paths such as `monsters[0].health` or `program.player_name`, assignment of a
> number, Boolean, text or enum literal (`monsters[0].health = 5`, which prints the value read back), and calls with
> literal arguments that print what they return (`monsters[0].roar()`, `monsters.count()`), written in Spite
> (`library/read_evaluate_print_loop.spite`). `--repl-port` answers the same commands over TCP, one JSON line
> each, answered where the program waits ([concurrency.md](concurrency.md)), and `spite connect` is its
> client. `--hot-reload` swaps the classes whose files changed into the running program, keeping its state,
> when a file is saved or when the REPL is sent `reload` ([Live reload](#live-reload---hot-reload)); Windows runs it,
> and Linux and macOS are held to compiling. **Not built yet:** the meta commands `classes`, `describe`, `enums`
> and `memory`, walking a `Dictionary<T>` or a union, assigning a `T?`, a list element or a whole instance, and
> reloading a change to a class's attributes ([the rules in full](#repl-and-live-reload--partial)).
>
> ```text
> spite> monsters[0]
> Monster { name: Goblin, health: 30 }
> spite> monsters[0].health = 5
> 5
> spite> monsters[0].roar()
> Goblin roars!
> spite> monstrs
> no attribute 'monstrs' in program: console, player_name, player_age, monsters
> ```

The REPL inspects and drives the *running* program -- local (`--repl`, standard input) and remote
(`--repl-port`, TCP). It is ordinary Spite, `ReadEvaluatePrintLoop` in the standard library, walking the program
through reflection ([reflection.md](reflection.md)). It reads and changes the running state, and with
`--hot-reload` it also swaps in code you edited while the program keeps running
([Live reload](#live-reload---hot-reload)). Typing new Spite code at the prompt is not built.

## The program this page uses

```gdscript title=repl_program/monster.spite
var name = "Monster"
var health = 10

func Monster(new_name: String, new_health: Integer) {
    name = new_name
    health = new_health
}

func roar(): String {
    return "{name} roars!"
}

func is_alive(): Boolean {
    return health > 0
}
```
```gdscript title=repl_program/repl_program.spite entry
var console = Console()
var player_name = "Hero"
var player_age = 20
var monsters = List<Monster>()

func ReplProgram() {
    monsters.append(Monster("Goblin", 30))
    monsters.append(Monster("Orc", 50))
    console.print("player", player_name, "age", player_age)
    var monsters_count = monsters.count()
    console.print("monster count", monsters_count)
}
```
```output
player Hero age 20
monster count 2
```

What the loop needs to reach a live value -- the attributes and functions of every class the loop can reach,
and the member templates that fit each list's elements, so `monsters.sum_health()` works at the prompt -- is
compiled only into a `--repl`/`--repl-port` build, never into a normal one, so it costs nothing when you are not
debugging. For the same reason the REPL's reads count only in that build: an attribute the program's own code
never reads is still an error in a normal one ([style.md](style.md#nothing-unused)), which is why `Monster` above
has an `is_alive()`.

## Local: `--repl`

```
spite repl_program --repl
```

Runs the constructor normally; when it returns, instead of exiting, reads commands from stdin with a
`spite> ` prompt until `exit` or end of input, then drops everything and exits cleanly (memory balanced, just
like a normal run).

## Remote: `--repl-port`

```
spite repl_program --repl-port=4000
```

Before the constructor runs, the program listens on `127.0.0.1:4000` **only**, and a background thread takes
one client at a time -- there is **no authentication**. Anyone who can reach that port on that machine can read
and mutate the running program. This is a local debugging tool for you and an AI on the same machine, never
something to expose past `127.0.0.1`. The program keeps running while it is served, and each command is answered
on the program's own thread the next time it waits -- a `program.sleep`, a `Console.read_line()`, a file or socket
read -- so a command never sees it halfway through a step ([concurrency.md](concurrency.md) has the details and a
frame loop served between frames). When the constructor returns the process stays alive until a client sends
`exit`, which flushes what the program printed and ends it with exit code 0. The port is part of the build: a
program built with `--repl-port` always listens on that port, and a port another program holds stops it before
its constructor runs, with `error: the REPL could not listen on 127.0.0.1:<port>`. With `--repl` as well, the
console loop runs first, and after its `exit` the process keeps serving the port.

Talk to it with the compiler's own client:

```
spite connect 4000
spite connect 4000 --command="program.player_name"
```

With no `--command` it is interactive: a `spite> ` prompt that sends each line and prints the answer as
`value (type)` or `error: message`. With `--command` it sends that one command, prints the raw JSON line, and
exits -- the form tests and AI clients use.

## The wire protocol

One command per line in, one line of JSON out: `{"ok":true,"value":"...","type":"Integer"}` on success,
`{"ok":false,"error":"..."}` on failure. A multi-line value (such as the answer to `attributes`) uses `\n`
inside that one JSON string, never a real newline on the wire. `type` is the class of the value; it is empty
for an answer that is not a value (`help`, `attributes`, `functions`, `exit`) and `Nothing` for a call that
returns nothing. The entry instance is always rooted at the fixed name `program`, regardless of what the entry
class is actually called, and a path may leave `program.` out. JSON is what the wire speaks today, not a
promise: the format is free to become whatever an AI client reads best, binary included
([D96](decisions.md)).

The commands are the ones `--repl` answers:

- **paths**: `program`, `program.monsters`, `program.monsters[0].health`. A `T?` that holds a value is walked
  through.
- **calls**: `program.monsters.count()`, `program.monsters[0].roar()`, with literal arguments. A call must be the
  last part of an expression; chaining after `()` is not supported yet.
- **assignment**: `program.player_name = "Aria"` -- through `set_<attribute>` when the class declares one,
  answering the value read back afterwards.
- `attributes`, `functions`, `help`, `exit`.
- `reload` and `last_reload`, which answer in a `--hot-reload` build ([Live reload](#live-reload---hot-reload)) and
  fail everywhere else with `'reload' answers in a program built with --hot-reload, which swaps its code while it
  runs, and this one was not`.

## A worked debugging session

This session is replayed by `check.sh` against the program above, running with `--repl-port`: every answer
below is the exact line the program sends back.

```wire repl_program
$ spite connect 4000 --command="program.player_name"
{"ok":true,"value":"Hero","type":"String"}

$ spite connect 4000 --command="program.monsters.count()"
{"ok":true,"value":"2","type":"Integer"}

$ spite connect 4000 --command="program.monsters[0]"
{"ok":true,"value":"Monster { name: Goblin, health: 30 }","type":"Monster"}

$ spite connect 4000 --command="program.monsters[0].roar()"
{"ok":true,"value":"Goblin roars!","type":"String"}

# Debugging a suspiciously low health value: mutate it and confirm.
$ spite connect 4000 --command="program.monsters[0].health = 5"
{"ok":true,"value":"5","type":"Integer"}

$ spite connect 4000 --command="program.monsters[0].health"
{"ok":true,"value":"5","type":"Integer"}

# A typo'd path is one line, never a crash.
$ spite connect 4000 --command="program.monstrs"
{"ok":false,"error":"no attribute 'monstrs' in program: console, player_name, player_age, monsters"}

$ spite connect 4000 --command="attributes"
{"ok":true,"value":"console: Console = Console {...}\nplayer_name: String = Hero\nplayer_age: Integer = 20\nmonsters: List<Monster> = List<Monster>(2)","type":""}

$ spite connect 4000 --command="exit"
{"ok":true,"value":"","type":""}
```

For an AI driving this: issue one command per `spite connect --command="..."` call (or hold one interactive
session open), read the single JSON line back, and branch on `"ok"`. `attributes` and `functions` first, to see
what is there (only functions whose arguments are numbers, Boolean, text or an enum can be called), then walk paths
down to the value in question before mutating anything.

## Live reload: `--hot-reload`

`--hot-reload` builds a program that can take new code while it runs. Save a file of the program, and the classes
that file declares are compiled again, on their own, into a small library the running program loads and swaps in
the next time it waits -- the same moments the REPL is answered at. Objects, singletons and everything the REPL
changed stay as they were: only functions change.

```
spite game --hot-reload --repl-port=4000
```

It works with or without a REPL. Without one, each swap is reported on the program's error output (`spite: rebuilt
Monster`); with `--repl` or `--repl-port`, `reload` swaps in what changed right away and answers what it rebuilt, and
`last_reload` answers what the last swap did, which is how you learn about one the file watcher made. A program
built without `--hot-reload` has none of this -- no watcher, no swapping, and `reload` answers that it was not
built for it. `--hot-reload` keeps every function (it implies `--development`), since a new version of a class
may call one nothing called before.

```gdscript title=hot_counter/monster.spite
var name = ""

func Monster(starting_name: String) {
    name = starting_name
}

func roar(): String {
    return "{name} roars"
}
```
```gdscript title=hot_counter/hot_counter.spite entry
var console = Console()
var visits = 0
var monster = Monster("Goblin")

func HotCounter() {
    visits = 1
    var welcome = greeting()
    console.print(welcome)
}

func greeting(): String {
    return "hello, visit {visits}"
}

func monster_roar(): String {
    return monster.roar()
}
```
```output
hello, visit 1
```

`check.sh` runs this session against a copy of the program above, built with `--hot-reload --repl-port`:

```text
$ spite connect 4000 --command="program.visits = 42"
{"ok":true,"value":"42","type":"Integer"}

# hot_counter.spite: greeting() now returns "welcome back, visit {visits}"
$ spite connect 4000 --command="reload"
{"ok":true,"value":"rebuilt HotCounter","type":""}

$ spite connect 4000 --command="greeting()"
{"ok":true,"value":"welcome back, visit 42","type":"String"}

# monster.spite: roar() now returns "{name} roars louder", and nothing is sent: the watcher sees the save
$ spite connect 4000 --command="last_reload"
{"ok":true,"value":"rebuilt Monster","type":""}

$ spite connect 4000 --command="monster.roar()"
{"ok":true,"value":"Goblin roars louder","type":"String"}

$ spite connect 4000 --command="exit"
{"ok":true,"value":"","type":""}
```

`visits` kept the 42 set before the first reload, and the second reload rebuilt `Monster` alone. When the watcher
has already swapped a save in, `reload` finds nothing left to do and answers `nothing changed since the code the
program runs`. For an AI: edit the file, send `reload`, and read `last_reload` if the answer says nothing changed.

### What a reload can change

| You change | What happens |
|---|---|
| a function's body | the next call runs the new body; a call already running finishes the old one |
| a new function on an existing class | the new code calls it; the REPL's `functions` keeps listing what the program started with |
| a function's parameters or return type | it becomes a new function: the rebuilt class and every class that calls it are rebuilt too |
| a function you deleted | whatever still holds it -- a function value, the REPL -- keeps its last code; the answer says `removed Monster.roar` |
| an attribute's default value | instances made after the reload get the new default |
| a class's attributes or enums | refused: `the attributes of Monster changed, ... restart the program to change a class's attributes or enums`, and the program keeps all of its code |
| a file that does not compile | refused with the compiler's error, and the program keeps all of its code |
| a new class | its functions are compiled into the new code; the REPL does not see it until a restart |

A refused reload leaves the program exactly as it was, so a save that caught a file half-written is harmless: the
next save reloads.

### How it works

- **One slot per function.** In a `--hot-reload` build, every function of the program's own classes (not the
  standard library's) is called through a slot the program can re-point: the function the rest of the code calls
  is a one-line forwarder to its slot. A normal build is untouched -- direct calls, and tree shaking. The slot
  costs about a nanosecond per call (one indirect call, which the C compiler also cannot inline):
  200 million calls to a one-line function took about 0.62 s instead of 0.44 s unoptimized, and 0.40 s instead of
  0.18 s with `--optimized`.
- **Only what changed is compiled again.** The build writes two files beside the executable: `game.reload_host`
  (every function the program has, its classes and their layouts) and `game.reload_files` (a hash of each of the
  program's files). A reload runs the compiler that built the program with the same options, as `spite reload`.
  The compiler reads the program again -- which takes milliseconds -- but writes C only for the classes whose files
  changed and whatever those need that the program does not already have, and compiles that into
  `game_reload_1.dll` (`.so` on Linux, `.dylib` on macOS). Everything else the new code calls, it reaches in the
  running program.
- **The swap happens where the program waits** ([D37](decisions.md)): the program loads the library
  with the operating system's loader, the one `DynamicLibrary` uses, hands it the addresses of the program's
  functions, and re-points the slots. Nothing runs halfway through a step. The program waits while the library is
  compiled, typically well under a second.
- **The watcher is the standard library's [`Watcher`](standard_library.md#watch-files-and-folders)**, the one any
  program can use, started on a thread of its own: `ReadDirectoryChangesW` on Windows, `inotify` on Linux and
  `kqueue` on macOS, with no polling. The thread sits in `wait_for_changes()`, which the operating system wakes;
  a burst of changes is waited out until 100 ms pass without one, so a save that writes a file in pieces reloads
  once. The program's own folder is watched with every folder below it, not the folders it `load`s; send
  `reload` after changing those. A build beside its program (the default) writes each reload's library into that
  folder too, which wakes the watcher once more for a check that finds nothing changed; `--executable-path=`
  elsewhere avoids it.
- The compiler, its options and the executable are recorded in the build, so the program must run from the folder
  it was built from, as `spite game --hot-reload` does.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### REPL and live reload  **[partial]**

Milestone 6a: a REPL that inspects and drives the *running* program, local (`--repl`) and remote
(`--repl-port`). **Status (2026-09-23), D72:** the REPL is written in Spite -- `library/read_evaluate_print_loop.spite` -- and
`runtime/spite_repl.h`, the C interpreter the subsections below were first designed around, is deleted. `spite
program.spite --repl` runs the entry constructor, then loops on `spite> `: `attributes` lists the entry instance's
attributes, `functions` its functions with their arguments, `help`, `exit`, and the command language below is built
for paths, assignment and calls (`conformance/stage6/interactive_loop`, `conformance/stage6/interactive_paths`):
`player.weapon.damage`, `program.monsters[1].name`, `player.health = 12` (through `set_health` when the class has
one), `player.job = 'knight'`, `add(2, 3)`, `monsters[0].roar()`, `monsters.count()`. The emitted `main` hands the
loop the entry instance as a `Spite.Attribute` named `program` before every command, and a `run` build runs the
program attached to the terminal instead of capturing its output. In a `--repl` build a `Spite.Attribute` stays
linked to the live value it describes, through four functions the compiler supplies (proposed by Claude,
unconfirmed): `value_attributes()` and `value_functions()` read the value's own attributes and functions (a
list's attributes are its elements, named `0`, `1`, ...), `assign(text)` writes a number, Boolean, text or enum
value into it and answers whether it could, and a `Spite.Function`'s `call_with_text(arguments)` calls it with
literal arguments and answers its result as a `Spite.Attribute?` -- `null` when an argument is not one the loop
can write. Outside `--repl` they answer an empty list, `false` and `null`. **Status (2026-09-24):** `--repl-port`
and `spite connect` are built, in Spite, over `Socket` ([System classes](standard_library.md#system-classes--implemented)) through `DynamicLibrary` -- see their
subsections below; `docs/repl.md`'s worked session is replayed by `check.sh` against a real program. **Not built
yet:** walking a `Dictionary<T>` or a union, assigning a `T?`, a list element or a whole instance, the meta
commands `classes`, `describe`, `enums` and `memory`. D37's drain points are built (2026-09-24): the remote loop's
commands are answered on the program's thread where it waits -- see "Answered where the program waits" below and
[Concurrency: `Concurrent`, `Parallel` and hidden waiting](concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting--implemented-on-windows-names-and-mechanism-proposed-by-claude-unconfirmed)'s "Concurrency". **Status (2026-09-24), milestone 6b:** live reload is built behind `--hot-reload`
(D111, D112) on Windows, with the Linux and macOS folders held to compiling -- see "Live reload and 6b" below.
Compiling and executing new Spite code typed at the prompt, and `Class.instances`, are not started.

#### Reflection tables  **[planned]**

Emitted only when `--repl` or `--repl-port` is given (never for a normal build): for every class the
compiler actually emits, its qualified name, attributes (name, type name, C offset, kind), and every
callable function whose parameters are all scalar/String/enum -- an ordinary method, or an already-
*instantiated* Symbol-codegen function (`set_age`, ...; a template nothing calls is still never
instantiated, so tree shaking for templates is unaffected). Each reflected function gets a generated
uniform thunk, `(self pointer, parsed arguments) -> rendered String result`. Enums get their value
names; `List<T>`/`Dictionary<T>`/`T?`/a union get the element/member kind and C offsets needed
to walk them generically, all resolved with the real `offsetof`/`sizeof` operators at compile time, so
the interpreter (`src/runtime/spite_repl.h`) never has to reason about struct layout itself. D1 (milestone
9a): a class/`List<T>`/`Dictionary<T>`/`String` attribute (or a `T?` of one) is always itself a
pointer now, so every one of those is `via_pointer` (walking through it -- and through a `T?` of
one, which is just that same nullable pointer -- is always transparent); only a still-embedded scalar/
enum/union attribute (or a `T?` of one) is not.

A class itself was already emitted whenever reachable (unaffected by REPL mode); a class's own
*functions*, it turns out, were never tree-shaken to begin with (`classes.emitClassBodies` already
emits every function of a resolved class, called or not), so item 1 of the milestone -- "relax tree
shaking of functions of reachable classes" -- needed no separate mechanism.

#### Command language

A small subset of Spite expressions, interpreted by `src/runtime/spite_repl.h` directly over the
reflection tables, rooted at the entry instance under the fixed name `program` (regardless of the
entry class's own Spite name):

- **paths**: `program`, `program.monsters`, `program.monsters[0].health`, `program.settings["volume"]`,
  `program.target`. Walking through a non-null `T?` is transparent; a union shows its active
  member and walks straight into it.
- **calls**: `program.monsters.count()`, `program.monsters[0].roar()`, `program.player.set_age(3)` (any
  reflected function, including an instantiated Symbol-codegen one); `List<T>`'s `count()`;
  `Dictionary<T>`'s `count()`/`keys()`/`has(key)`. A call must be the last part of an expression --
  chaining a `.`/`[...]` after `()` is not supported this milestone.
- **assignment** of a scalar/String/enum literal: `program.player.age = 5`, `program.monsters[1].name =
  "rat"`, `program.player.job = 'knight'`. Always a raw field write; additionally, when a `set_<attribute>`
  function is itself reflected (i.e. some Spite code already called it, instantiating it), the REPL calls
  that instead of writing the field directly -- the same "attribute write goes through `set_<attribute>`"
  rule ordinary compiled Spite code follows.
- **meta commands**: `classes` (every reflected class's qualified name), `describe Engine.Renderer`
  (its attributes and functions with signatures), `enums`, `memory` (live allocation count/bytes from the
  debug allocator -- REPL modes always compile with it enabled, regardless of `--debug-memory`), `help`,
  `exit`.
- Printing a class instance shows `ClassName { attribute: value, ... }` one level deep: a nested class/
  union attribute prints as `ClassName {...}` (never expanded further); a list/dictionary attribute
  prints as `List<T>(count)`/`Dictionary<T>(count)` regardless of depth; a `T?` prints `null` or
  transparently passes through to its held value at the *same* depth (it is never itself a level of
  nesting).
- Every error is one line and never a crash: an unknown attribute names the ones that do exist, an index
  or key out of range says so, a scalar/String/enum-only operation on the wrong kind of value is a type
  mismatch, and calling a non-function is "not callable".
- (proposed by Claude, unconfirmed; built in `library/read_evaluate_print_loop.spite`) Where the above is silent:
  a path may leave out the leading `program.`, so `player.health` and `program.player.health` name the same
  value; text prints without quotes (`name: hero`); an assignment prints the value read back afterwards, so a
  `set_<attribute>` that refused it shows the old one; a call that returns `Nothing` prints nothing; `functions`
  prints `name(argument: Class, ...): Returns`; a text literal has no escapes yet.

#### `--repl`  **[implemented]**

Runs the entry constructor normally; when it returns, instead of dropping the entry instance and
exiting, reads commands from stdin with a `spite> ` prompt until `exit` or end of input, then drops and
exits normally (memory balanced -- checked by its own end-to-end test the same way `--debug-memory`'s
own tests are).

#### `--repl-port=<port>`  **[implemented]**

`repl_port` is a `Build` field ([Build settings: `Build`](programs.md#build-settings-build--implemented)), so the port is part of the build. Before the entry constructor runs, starts a background thread with a
TCP server bound to `127.0.0.1:<port>` **only** -- it never listens on any other interface, and there is
**no authentication**: anyone who can reach that port on that machine can read and mutate the running
program. This is a local debugging tool, not something to expose past `127.0.0.1`.

The line protocol is designed for an AI client: each request is one line of the command language; each
response is one line of JSON, `{"ok":true,"value":"...","type":"Integer"}` on success or
`{"ok":false,"error":"..."}` on failure (values and errors are JSON-escaped strings; a multi-line value,
e.g. from `classes`/`enums`/`describe`, uses `\n` inside that one JSON string, never a real newline in the
wire bytes). The program keeps running (data races with it are accepted -- this is a debug tool) while
clients connect and disconnect one at a time; when the entry constructor returns, the process stays alive
serving the REPL until a client sends `exit`, at which point the server thread calls `exit(0)` directly
(there is no guarantee the main thread, possibly still inside the entry constructor's own `while` loop,
will ever unwind back to a clean `main` return, so this does not attempt one).

**As built (2026-09-24; the choices below proposed by Claude, unconfirmed):**

- **Spite, over `Socket`.** The server is `ReadEvaluatePrintLoop.serve` in `library/read_evaluate_print_loop.spite`,
  over the `Socket` class ([System classes](standard_library.md#system-classes--implemented)), whose operating-system members live in `library/windows/socket.spite`
  (`ws2_32.dll`), `library/linux/socket.spite` (`libc.so.6`) and `library/mac/socket.spite` (`libSystem.dylib`),
  reopened per D80. `Socket` has no way to bind anything but `127.0.0.1`, so "loopback only" holds by
  construction. `ws2_32.dll` is opened when the first `Socket` is made, so a program without a REPL never loads it.
- **The thread.** The one piece of C this adds is written by the compiler, only in a `--repl-port` build: a
  two-line thread entry, `spite_remote_loop_thread`, that calls `ReadEvaluatePrintLoop.serve` with the entry
  instance. Spite starts it and waits for it through the operating system's folder (`CreateThread` and
  `WaitForSingleObject` from `kernel32.dll`; `pthread_create` and `pthread_join` elsewhere). No `Thread` class is
  added to the library: D35 already says how a program is concurrent, and this thread belongs to the REPL.
- **Answered where the program waits** (D37, built 2026-09-24; proposed by Claude, unconfirmed). The socket thread
  only reads a command, hands it to the program's scheduler ([Concurrency: `Concurrent`, `Parallel` and hidden waiting](concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting--implemented-on-windows-names-and-mechanism-proposed-by-claude-unconfirmed), "Concurrency") and waits for the answer.
  The scheduler answers it on the program's thread the next time the program waits -- `program.sleep`,
  `Console.read_line()`, a `File` or `Socket` read or write, a `Concurrent` wait, a `Parallel` join -- so a command
  sees the program between two steps, never in the middle of one, and "data races are accepted" above no longer
  applies. After the constructor returns, the program waits for nothing but commands. A program that never waits
  is answered at its loops' check points (D174, below); `docs/concurrency.md` replays a frame loop served between
  frames and a busy loop served between passes.
- **Every loop is a check point in a REPL build** (D174, decided by Mortaro; the mechanism proposed by Claude,
  unconfirmed).  **[implemented]** In a `--repl-port` or `--hot-reload` build, the generator ends every pass of every
  `while` in the program's own code (not `library/` or `launcher/`) with a call to `Scheduler.check_point()`, which
  on the scheduler's thread answers a pending command (the handover flag `answer_pending` already reads) and runs a
  pending reload. On any other thread -- a `Parallel`, a helper -- it does nothing. A loop that never waits is
  answered between two passes, so the old compile error for such a loop, and `diagnostics/remote_loop_never_waits`,
  are gone. A build without those flags has no check point: its C is byte for byte what it was (checked on a busy
  loop and `examples/dungeon`). `--repl` alone answers after the constructor returns, from the console, so it needs
  none. The cost where it exists is a call and two atomic loads per pass.
- **The port is part of the build**, like any flag the compiler folds (D84): `main` listens on it, on the main
  thread, before the constructor runs, so a client that connects any time after the program starts is served. A
  port another program holds stops the program before its constructor with `error: the REPL could not listen on
  127.0.0.1:<port>` and exit code 1.
- **The answer is shared with `--repl`.** `answer(command, program)` returns a `ReadEvaluatePrintLoop.Answer`
  (`succeeded`, `text`, `class_name`); the console loop prints `text`, and the socket loop writes it as JSON:
  `"type"` is the class of the value, empty for `help`, `attributes` and `functions`, and `Nothing` for a call
  that returns nothing. Every message the console loop prints as a complaint is `"ok":false` on the wire.
  JSON escapes are `\"`, `\\`, `\n`, `\r`, `\t` and `\u00XX` for any other control character.
- **`exit`** answers `{"ok":true,"value":"","type":""}`, closes the connection and calls `program.exit(0)`,
  which flushes what the program printed. It is handed over like any command, so the program stops at a wait
  first. When the constructor returns first, `main` serves commands until then. With `--repl` too, the console loop runs first, and after its `exit` the process keeps serving.
- `spite program --repl-port=4000` is the form, as for every `Build` field; a port outside 1 to 65535 is an
  error naming it.

#### `spite connect <port>`  **[implemented]**

A tiny client built into the compiler binary itself, over the same `Socket` class: with no `--command`, an
interactive prompt that sends each typed line and pretty-prints the JSON response (`value (type)`, just `value`
when the type is empty or `Nothing`, or `error: message`); with `--command="..."`, sends that one command, prints
the raw JSON response line, and exits -- this is the form tests and AI clients use. Nothing listening on the port
is `error: nothing is listening on 127.0.0.1:<port>` and exit code 1. `check.sh` replays every ` ```wire ` block
in `docs/` this way (`scripts/docs_corpus.spite` writes it out beside its program): the program is built with
`--repl-port`, each `$ spite connect ... --command="..."` line is sent, and each answer must be the line written
under it, the program must end with exit code 0 after `exit`, and what it printed must be its ` ```output `.

#### Live reload and 6b  **[implemented on Windows; the mechanism and the rules below proposed by Claude, unconfirmed]**

D111 (a change is seen through the operating system and only what changed is rebuilt) and D112 (`--hot-reload`, a
flag of its own) as built on 2026-09-24. `docs/repl.md` ("Live reload") is the user's page, and `check.sh` runs its
session against a copy of the program it edits. Still not started: compiling new Spite code typed at the prompt,
and `Class.instances`.

- **The flag.** `hot_reload` is a `Build` field (default `false`). It **implies** `--development` rather than
  requiring it, since a new version of a class may call a function nothing called before. It works without a REPL:
  the watcher still swaps, and each swap or refusal is one line on the program's error output (`spite: rebuilt
  Monster`); an explicit rebuild needs `--repl` or `--repl-port`, whose `reload` swaps in what changed and answers
  what it rebuilt, and whose `last_reload` answers what the last swap (the watcher's included) did. A build without
  it has no slots, no watcher and no reload code: `reload` answers `'reload' answers in a program built with
  --hot-reload, which swaps its code while it runs, and this one was not`, from a branch folded on the `Build`
  constant. As with `--repl-port`, a `--hot-reload` build swaps where the program waits and at each loop's check
  point (D174). `spite program --hot-reload` runs the
  program attached to the terminal, like a REPL build.
- **The swap mechanism: one slot per function.** In a `--hot-reload` build every function of the program's own
  classes (not `library/` or `launcher/`), and each such class's `_init` (its attribute defaults), is written as
  `<name>_hot`, with a function pointer `<name>_slot` holding it, and `<name>` becomes a one-line forwarder through
  the slot. Every call site, function value and REPL thunk still names `<name>`, so all of them follow a swap;
  library functions stay direct calls. Any other build is unchanged: direct calls and tree shaking. The cost is one
  indirect call that the C compiler cannot inline, about a nanosecond: 200 million calls to a one-line function
  took about 0.62 s instead of 0.44 s at `-O0` and 0.40 s instead of 0.18 s with `--optimized` (Windows, clang).
- **What the build records.** Beside the executable, `<program>.reload_host` lists every function the executable
  defines with its C prototype, its slots, its class ids in order and the C layout of every class and enum;
  `<program>.reload_files` holds a generation number and a hash of each of the program's files. The executable
  holds a table of its functions' addresses by name, the path of the compiler that built it and that compiler's
  options, so it must run from the directory it was built from.
- **Rebuilding: `spite reload <folder> <options> --executable-path=<program>`** (a command since D128, when
  `--mode` went; proposed by Claude, unconfirmed). A reload runs that compiler with those options, less the
  outputs, and prints its errors to standard output, where the running program reads them.
  It reads the whole program again (milliseconds), with the class ids seeded from the manifest so every class keeps
  the id its instances carry, and compares file hashes: nothing changed answers `unchanged`. The rebuilt set is the
  classes the changed files declare, plus every class whose code calls a function of the set that the running
  program has with another prototype or does not have at all (the running caller would still call the old one),
  repeated until nothing is added. It writes C only for the rebuilt classes' functions and what they reach that the
  running program lacks, reaching every other function through a pointer the library is handed when it is loaded,
  and compiles `<program>_reload_<n>.dll` (`.so`, `.dylib`). It prints `rebuilt A, B`, `removed A.f` when a function
  the running program has is gone, and the library's path.
- **Swapping at a drain point** (D37). The running program opens the library with `LoadLibraryA`/`dlopen`, the
  calls `DynamicLibrary` opens a library with, and calls its `spite_reload_bind`, which looks up each running
  function it uses by name (the allocator included, so the library allocates and frees through the program) and then
  re-points the slots of the rebuilt functions whose prototypes are unchanged, with an atomic store. `reload` is a
  REPL command, so it runs where the program waits; the watcher's signal is handled in the scheduler's idle, beside
  the REPL's commands. While a reload runs, waits block instead of suspending, so nothing else runs until the swap
  is done; the program waits for the compile (under a second for a small program). After a swap the file hashes
  are updated. A library is never unloaded: values it made, such as its text literals, may still be referenced.
- **What reloads.** A function body (the next call runs it; a call already running finishes the old one). A new
  function or a new class (the new code calls it; the REPL's `functions` and reflection keep what the program
  started with). A changed parameter list or return type (a new function: the rebuilt class and its callers are
  rebuilt). A deleted function keeps its last code for whatever still holds it, a function value or the REPL, and
  the answer names it. An attribute's default value (through the `_init` slot, for instances made afterwards).
- **What needs a restart: a class's attributes or enums.** When the layout of a class or enum the running
  program has differs, the reload is refused -- `the attributes of Monster changed, and the running program's
  instances were made with the old ones: restart the program to change a class's attributes or enums, or undo that
  part of the change to reload the rest` -- and the program keeps all of its code. Migrating instances by attribute
  name (new attributes taking their defaults) needs every live instance and every reference to it, which the
  program cannot find today; `mortaros_missing_decisions.md` asks which to build. A file that does not compile is
  refused the same way, with the compiler's error, so a save caught half-written is harmless.
- **Watching** (D111, D194). `HotReload` (`library/hot_reload.spite`) watches through the standard library's
  `Watcher` ([System classes](standard_library.md#system-classes--implemented)), the one watcher any program uses: `ReadDirectoryChangesW` with overlapped I/O from
  `kernel32.dll` on Windows, `inotify` and `poll` from `libc.so.6` on Linux, and `kqueue`/`kevent` on each folder
  and file from `libSystem.dylib` on macOS -- no polling. On a thread of its own, it calls `wait_for_changes()`,
  which returns once 100 ms pass without a change, then sets a flag and wakes the scheduler. The program's own
  folder is watched with every folder below it, not the folders it `load`s; `reload` picks those up.
- **Windows' C runtime.** When the C compiler targets MSVC (its `-dumpmachine`), the program and its libraries are
  built against the C runtime DLL (`-fms-runtime-lib=dll`) so they share one heap and one standard output.
- **Untested:** Linux and macOS -- their watchers, `.so`/`.dylib` libraries and `Program.executable_path` (which
  the compiler uses to record itself) -- are held to compiling by `check.sh`, and are written the same way as
  Windows'. A crash inside reloaded code reports the library's own assert trace. A `Parallel` running a function
  whose slot is re-pointed finishes the old code.
