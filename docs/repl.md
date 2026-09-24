# REPL and live reload

> **What is built** (D72): `spite program.spite --repl` runs the program, then answers `attributes`,
> `functions`, `help`, `exit`, paths such as `monsters[0].health` or `program.player_name`, assignment of a
> number, Bool, text or enum literal (`monsters[0].health = 5`, which prints the value read back), and calls with
> literal arguments that print what they return (`monsters[0].roar()`, `monsters.count()`), written in Spite
> (`library/read_evaluate_print_loop.spite`). The meta commands `classes`, `describe`, `enums`, `memory` and
> `--repl-port` are the design, not yet built (manual.md section 14).
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

Milestone 6a: a REPL that inspects and drives the *running* program -- local (`--repl`, stdin) and remote
(`--repl-port`, TCP). Compiling and running arbitrary new Spite code inside the process, `Class.instances`, and
swapping code while the program runs are all **[planned]**, blocked on manual.md's open question 4 (see
[memory.md](memory.md)'s boxed note). What exists today only *reads and mutates the existing running state* --
it is a debugger, not a live-coding console, yet.

## The program this page uses

```spite title=repl_program/monster.spite
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
```spite title=repl_program/repl_program.spite entry
var console = Console()
var player_name = "Hero"
var player_age = 20
var monsters = List<Monster>()

func ReplProgram() {
    monsters.append(Monster("Goblin", 30))
    monsters.append(Monster("Orc", 50))
    console.print("player", player_name, "age", player_age)
    console.print("monster count", monsters.count())
}
```
```output
player Hero age 20
monster count 2
```

Reflection tables (every reachable class's attributes, and every callable function whose parameters are all
scalar/String/enum) are only emitted for a `--repl`/`--repl-port` build -- never for a normal build, so this
costs nothing when you are not debugging.

## Local: `--repl`

```
spite repl_program.spite --repl
```

Runs the constructor normally; when it returns, instead of exiting, reads commands from stdin with a
`spite> ` prompt until `exit` or end of input, then drops everything and exits cleanly (memory balanced, just
like a normal run).

## Remote: `--repl-port`

```
spite repl_program.spite --repl-port 4000
```

Starts a background TCP server on `127.0.0.1:4000` **only**, before the constructor runs -- there is **no
authentication**. Anyone who can reach that port on that machine can read and mutate the running program. This
is a local debugging tool for you and an AI on the same machine, never something to expose past `127.0.0.1`.

Talk to it with the compiler's own built-in client:

```
spite connect 4000
spite connect 4000 --command="program.player_name"
```

## The wire protocol

One command per line in, one line of JSON out: `{"ok":true,"value":"...","type":"Int"}` on success,
`{"ok":false,"error":"..."}` on failure. A multi-line value (from `classes`/`enums`/`describe`) uses `\n`
inside that one JSON string, never a real newline on the wire. The entry instance is always rooted at the
fixed name `program`, regardless of what the entry class is actually called.

Commands are a small subset of Spite expressions:

- **paths**: `program`, `program.monsters`, `program.monsters[0].health`. Walking through a non-null
  A `T?` is transparent; a union shows its active member.
- **calls**: `program.monsters.count()`, `program.monsters[0].roar()` -- any reflected function. A call must be
  the last part of an expression; chaining after `()` is not supported yet.
- **assignment**: `program.player_name = "Aria"` -- a raw field write, unless a `set_<attribute>` function is
  itself already reflected (something in the compiled program already called it), in which case the REPL calls
  that instead, the same rule ordinary compiled code follows for attribute interception.
- **meta commands**: `classes`, `describe Monster`, `enums`, `memory` (live allocation count/bytes), `help`,
  `exit`.

## A worked debugging session

This is illustrative wire traffic for the program above, running with `--repl-port 4000` -- not re-executed by
this repository's test suite (there is no single expected stdout for an interactive session), but every
response shape is the one manual.md section 14 specifies.

```text
$ spite connect 4000 --command="program.player_name"
{"ok":true,"value":"Hero","type":"String"}

$ spite connect 4000 --command="program.monsters.count()"
{"ok":true,"value":"2","type":"Int"}

$ spite connect 4000 --command="program.monsters[0]"
{"ok":true,"value":"Monster { name: \"Goblin\", health: 30 }","type":"Monster"}

$ spite connect 4000 --command="program.monsters[0].roar()"
{"ok":true,"value":"Goblin roars!","type":"String"}

# Debugging a suspiciously low health value: mutate it and confirm.
$ spite connect 4000 --command="program.monsters[0].health = 5"
{"ok":true,"value":"5","type":"Int"}

$ spite connect 4000 --command="program.monsters[0].health"
{"ok":true,"value":"5","type":"Int"}

# A typo'd path is one line, never a crash.
$ spite connect 4000 --command="program.monstrs"
{"ok":false,"error":"unknown attribute 'monstrs' on Program (did you mean 'monsters'?)"}

$ spite connect 4000 --command="exit"
{"ok":true,"value":"","type":""}
```

For an AI driving this: issue one command per `spite connect --command="..."` call (or hold one interactive
session open), read the single JSON line back, and branch on `"ok"`. `describe <Class>` first, to see what is
actually reflected (only functions with scalar/String/enum parameters are callable at all), then walk paths
down to the value in question before mutating anything.
