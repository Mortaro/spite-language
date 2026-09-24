# REPL and live reload

> **What is built:** `spite program --repl` runs the program, then answers `attributes`,
> `functions`, `help`, `exit`, paths such as `monsters[0].health` or `program.player_name`, assignment of a
> number, Bool, text or enum literal (`monsters[0].health = 5`, which prints the value read back), and calls with
> literal arguments that print what they return (`monsters[0].roar()`, `monsters.count()`), written in Spite
> (`library/read_evaluate_print_loop.spite`). `--repl_port` answers the same commands over TCP, one JSON line
> each, answered where the program waits ([concurrency.md](concurrency.md)), and `spite connect` is its
> client. `--hot_reload` swaps the classes whose files changed into the running program, keeping its state,
> when a file is saved or when the REPL is sent `reload` ([Live reload](#live-reload---hot_reload)); Windows runs it,
> and Linux and macOS are held to compiling. **Not built yet:** the meta commands `classes`, `describe`, `enums`
> and `memory`, walking a `Dictionary<T>` or a union, assigning a `T?`, a list element or a whole instance, and
> reloading a change to a class's attributes ([manual section 14](../manual.md#14-repl-and-live-reload--partial)).
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
(`--repl_port`, TCP). It is ordinary Spite, `ReadEvaluatePrintLoop` in the standard library, walking the program
through reflection ([reflection.md](reflection.md)). It reads and changes the running state, and with
`--hot_reload` it also swaps in code you edited while the program keeps running
([Live reload](#live-reload---hot_reload)). Typing new Spite code at the prompt is not built.

## The program this page uses

```gdscript title=repl_program/monster.spite
var name = "Monster"
var health = 10

func Monster(new_name: String, new_health: Int) {
    name = new_name
    health = new_health
}

func roar(): String {
    return "{name} roars!"
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
compiled only into a `--repl`/`--repl_port` build, never into a normal one, so it costs nothing when you are not
debugging.

## Local: `--repl`

```
spite repl_program --repl
```

Runs the constructor normally; when it returns, instead of exiting, reads commands from stdin with a
`spite> ` prompt until `exit` or end of input, then drops everything and exits cleanly (memory balanced, just
like a normal run).

## Remote: `--repl_port`

```
spite repl_program --repl_port=4000
```

Before the constructor runs, the program listens on `127.0.0.1:4000` **only**, and a background thread takes
one client at a time -- there is **no authentication**. Anyone who can reach that port on that machine can read
and mutate the running program. This is a local debugging tool for you and an AI on the same machine, never
something to expose past `127.0.0.1`. The program keeps running while it is served, and each command is answered
on the program's own thread the next time it waits -- a `Program().sleep`, a `Console.read_line()`, a file or socket
read -- so a command never sees it halfway through a step ([concurrency.md](concurrency.md) has the details and a
frame loop served between frames). When the constructor returns the process stays alive until a client sends
`exit`, which flushes what the program printed and ends it with exit code 0. The port is part of the build: a
program built with `--repl_port` always listens on that port, and a port another program holds stops it before
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

One command per line in, one line of JSON out: `{"ok":true,"value":"...","type":"Int"}` on success,
`{"ok":false,"error":"..."}` on failure. A multi-line value (such as the answer to `attributes`) uses `\n`
inside that one JSON string, never a real newline on the wire. `type` is the class of the value; it is empty
for an answer that is not a value (`help`, `attributes`, `functions`, `exit`) and `Nothing` for a call that
returns nothing. The entry instance is always rooted at the fixed name `program`, regardless of what the entry
class is actually called, and a path may leave `program.` out. JSON is what the wire speaks today, not a
promise: the format is free to become whatever an AI client reads best, binary included
([manual, decision D96](../manual.md#decision-log)).

The commands are the ones `--repl` answers:

- **paths**: `program`, `program.monsters`, `program.monsters[0].health`. A `T?` that holds a value is walked
  through.
- **calls**: `program.monsters.count()`, `program.monsters[0].roar()`, with literal arguments. A call must be the
  last part of an expression; chaining after `()` is not supported yet.
- **assignment**: `program.player_name = "Aria"` -- through `set_<attribute>` when the class declares one,
  answering the value read back afterwards.
- `attributes`, `functions`, `help`, `exit`.
- `reload` and `last_reload`, which answer in a `--hot_reload` build ([Live reload](#live-reload---hot_reload)) and
  fail everywhere else with `'reload' answers in a program built with --hot_reload, which swaps its code while it
  runs, and this one was not`.

## A worked debugging session

This session is replayed by `check.sh` against the program above, running with `--repl_port`: every answer
below is the exact line the program sends back.

```wire repl_program
$ spite connect 4000 --command="program.player_name"
{"ok":true,"value":"Hero","type":"String"}

$ spite connect 4000 --command="program.monsters.count()"
{"ok":true,"value":"2","type":"Int"}

$ spite connect 4000 --command="program.monsters[0]"
{"ok":true,"value":"Monster { name: Goblin, health: 30 }","type":"Monster"}

$ spite connect 4000 --command="program.monsters[0].roar()"
{"ok":true,"value":"Goblin roars!","type":"String"}

# Debugging a suspiciously low health value: mutate it and confirm.
$ spite connect 4000 --command="program.monsters[0].health = 5"
{"ok":true,"value":"5","type":"Int"}

$ spite connect 4000 --command="program.monsters[0].health"
{"ok":true,"value":"5","type":"Int"}

# A typo'd path is one line, never a crash.
$ spite connect 4000 --command="program.monstrs"
{"ok":false,"error":"no attribute 'monstrs' in program: console, player_name, player_age, monsters"}

$ spite connect 4000 --command="attributes"
{"ok":true,"value":"console: Console = Console {...}\nplayer_name: String = Hero\nplayer_age: Int = 20\nmonsters: List<Monster> = List<Monster>(2)","type":""}

$ spite connect 4000 --command="exit"
{"ok":true,"value":"","type":""}
```

For an AI driving this: issue one command per `spite connect --command="..."` call (or hold one interactive
session open), read the single JSON line back, and branch on `"ok"`. `attributes` and `functions` first, to see
what is there (only functions whose arguments are numbers, Bool, text or an enum can be called), then walk paths
down to the value in question before mutating anything.

## Live reload: `--hot_reload`

`--hot_reload` builds a program that can take new code while it runs. Save a file of the program, and the classes
that file declares are compiled again, on their own, into a small library the running program loads and swaps in
the next time it waits -- the same moments the REPL is answered at. Objects, singletons and everything the REPL
changed stay as they were: only functions change.

```
spite game --hot_reload --repl_port=4000
```

It works with or without a REPL. Without one, each swap is reported on the program's error output (`spite: rebuilt
Monster`); with `--repl` or `--repl_port`, `reload` swaps in what changed right away and answers what it rebuilt, and
`last_reload` answers what the last swap did, which is how you learn about one the file watcher made. A program
built without `--hot_reload` has none of this -- no watcher, no swapping, and `reload` answers that it was not
built for it. `--hot_reload` keeps every function (it implies `--development`), since a new version of a class
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
    var greeting = greeting()
    console.print(greeting)
}

func greeting(): String {
    return "hello, visit {visits}"
}
```
```output
hello, visit 1
```

`check.sh` runs this session against a copy of the program above, built with `--hot_reload --repl_port`:

```text
$ spite connect 4000 --command="program.visits = 42"
{"ok":true,"value":"42","type":"Int"}

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

- **One slot per function.** In a `--hot_reload` build, every function of the program's own classes (not the
  standard library's) is called through a slot the program can re-point: the function the rest of the code calls
  is a one-line forwarder to its slot. A normal build is untouched -- direct calls, and tree shaking. The slot
  costs about a nanosecond per call (one indirect call, which the C compiler also cannot inline):
  200 million calls to a one-line function took about 0.62 s instead of 0.44 s unoptimized, and 0.40 s instead of
  0.18 s with `--optimized`.
- **Only what changed is compiled again.** The build writes two files beside the executable: `game.reload_host`
  (every function the program has, its classes and their layouts) and `game.reload_files` (a hash of each of the
  program's files). A reload runs the compiler that built the program with the same options and `--mode=reload`.
  The compiler reads the program again -- which takes milliseconds -- but writes C only for the classes whose files
  changed and whatever those need that the program does not already have, and compiles that into
  `game_reload_1.dll` (`.so` on Linux, `.dylib` on macOS). Everything else the new code calls, it reaches in the
  running program.
- **The swap happens where the program waits** ([D37](../manual.md#decision-log)): the program loads the library
  with the operating system's loader, the one `DynamicLibrary` uses, hands it the addresses of the program's
  functions, and re-points the slots. Nothing runs halfway through a step. The program waits while the library is
  compiled, typically well under a second.
- **The watcher is the operating system's**, one per system folder of the library (`library/windows/hot_reload.spite`
  and the rest), started on a thread of its own: `FindFirstChangeNotification` on Windows, `inotify` on Linux and
  `kqueue` on macOS, with no polling. A burst of changes is waited out until 100 ms pass without one, so a save
  that writes a file in pieces reloads once. The program's own folder is watched, not the folders it `load`s; send
  `reload` after changing those.
- The compiler, its options and the executable are recorded in the build, so the program must run from the folder
  it was built from, as `spite game --hot_reload` does.
