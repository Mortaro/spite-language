# REPL and live reload

> **What is built:** `spite program --repl` runs the program, then answers `attributes`,
> `functions`, `help`, `exit`, paths such as `monsters[0].health` or `program.player_name`, assignment of a
> number, Bool, text or enum literal (`monsters[0].health = 5`, which prints the value read back), and calls with
> literal arguments that print what they return (`monsters[0].roar()`, `monsters.count()`), written in Spite
> (`library/read_evaluate_print_loop.spite`). `--repl_port` answers the same commands over TCP, one JSON line
> each, answered where the program waits ([concurrency.md](concurrency.md)), and `spite connect` is its
> client. **Not built yet:** the meta commands `classes`, `describe`, `enums` and `memory`, walking a
> `Dictionary<T>` or a union, and assigning a `T?`, a list element or a whole instance
> ([manual section 14](../manual.md#14-repl-and-live-reload--partial)).
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
through reflection ([reflection.md](reflection.md)). What exists today reads and changes the running state: it
is a debugger, not a live-coding console. Compiling new Spite code into the running process and swapping code
while it runs (live reload) are decided but not built
([manual section 14](../manual.md#live-reload-and-6b--planned)).

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
