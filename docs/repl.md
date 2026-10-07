# REPL and live reload

`spite program --repl` runs the program, then answers `attributes`, `functions` and `classes`, `help`, `exit`, paths
such as `monsters[0].health` or `program.player_name`, a singleton by its class's name (`World().entity_count()`,
`Tick().step_milliseconds`), assignment of a number, Boolean, text or enum literal (`monsters[0].health = 5`, which
prints the value read back), and calls with literal arguments that print what they return, chained like any path
(`monsters[0].roar()`, `monsters.count()`, `World().position_of(1).across`), written in Spite
(`library/read_evaluate_print_loop.spite`). `--repl-port` answers the same commands over TCP, one JSON line each,
answered where the program waits or at the end of a loop's pass
([concurrency.md](concurrency.md#the-repl-answers-at-the-waits)), and `spite connect` is its client. `--hot-reload`
swaps the classes whose files changed into the running program, keeping its state, when a file is saved or when the
REPL is sent `reload` ([Live reload](#live-reload---hot-reload)), moving live objects to their class's new attributes
when those change. `bytes <path>` answers a value's address, size, bytes and layout as JSON
([Native memory](#native-memory-bytes)). `describe Monster`, `enums` and `memory` describe a class, the program's
enums and the heap in use; an assignment takes `null` into a `T?`, a literal into a list or dictionary element, and
another path's instance (`follower = leader`); a path walks into a union's active member. A reload swaps in a change
to an enum's values, each held value keeping its meaning. With `--hot-reload`, `eval <expression>` and
`run <statement>` compile code typed at the prompt into the running program and run it once
([below](#code-typed-at-the-prompt)).

```text
spite> monsters[0]
Monster { name: Goblin, health: 30 }
spite> monsters[0].health = 5
5
spite> monsters[0].roar()
Goblin roars!
spite> monstrs
no attribute 'monstrs' in program: console, player_name, player_age, monsters
```

The REPL inspects and drives the *running* program, local (`--repl`, standard input) and remote
(`--repl-port`, TCP). It is ordinary Spite, `ReadEvaluatePrintLoop` in the standard library, walking the program
through reflection ([reflection.md](reflection.md)). It reads and changes the running state, and with
`--hot-reload` it also swaps in code you edited while the program keeps running
([Live reload](#live-reload---hot-reload)).

A REPL or live-reload build is slower on purpose: it trades speed for what it tells you while the program runs.
Never measure performance with one; benchmarks use a production build
([Measure only a production build](compiler.md#measure-only-a-production-build)).

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
    var goblin = Monster("Goblin", 30)
    monsters.append(goblin)
    var orc = Monster("Orc", 50)
    monsters.append(orc)
    console.print("player", player_name, "age", player_age)
    var monsters_count = monsters.count()
    console.print("monster count", monsters_count)
}
```
```output
player Hero age 20
monster count 2
```

What the loop needs to reach a live value (the attributes and functions of every class the loop can reach,
and the member templates that fit each list's elements, so `monsters.sum_health()` works at the prompt) is
compiled only into a `--repl`/`--repl-port` build, never into a normal one, so it costs nothing when you are not
debugging. A REPL build is also an [inspectable build](compiler.md#development-builds-and-tree-shaking):
nothing is tree-shaken, so every internal can be reached, and the executable is larger than a production build of
the same program. For the same reason the REPL's reads count only in that build: an attribute the program's own
code never reads is still an error in a normal one ([style.md](style.md#nothing-unused)), which is why `Monster`
above has an `is_alive()`.

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
one client at a time, and there is **no authentication**. Anyone who can reach that port on that machine can read
and mutate the running program. This is a local debugging tool for you and an AI on the same machine, never
something to expose past `127.0.0.1`. The program keeps running while it is served, and each command is answered
on the program's own thread the next time it waits (a `program.sleep`, a `Console.read_line()`, a file or socket
read) or reaches the end of a pass of one of its loops, so a command never sees it halfway through a step
([concurrency.md](concurrency.md#the-repl-answers-at-the-waits) serves a frame loop between frames and a busy loop
between passes). When the constructor returns the process stays alive until a client sends
`exit`, which flushes what the program printed and ends it with exit code 0. The port is part of the build: a
program built with `--repl-port` always listens on that port, and a port another program holds stops it before
its constructor runs, with `error: the REPL could not listen on 127.0.0.1:<port>`. With `--repl` as well, the
console loop runs first, and after its `exit` the process keeps serving the port. What serving costs, and only in
a `--repl-port` build: one thread waiting on the socket, and a call with two atomic loads at the end of every pass
of the program's own loops.

Talk to it with the compiler's own client:

```
spite connect 4000
spite connect 4000 --command="program.player_name"
```

With no `--command` it is interactive: a `spite> ` prompt that sends each line and prints the answer as
`value (type)` or `error: message`. With `--command` it sends that one command, prints the raw JSON line, and

exits, which is the form tests and AI clients use.

## The wire protocol

One command per line in, one line of JSON out: `{"ok":true,"value":"...","type":"Integer"}` on success,
`{"ok":false,"error":"..."}` on failure. A multi-line value (such as the answer to `attributes`) uses `\n`
inside that one JSON string, never a real newline on the wire. `type` is the class of the value; it is empty
for an answer that is not a value (`help`, `attributes`, `functions`, `exit`) and `Nothing` for a call that
returns nothing. The entry instance is always rooted at the fixed name `program`, regardless of what the entry
class is actually called, and a path may leave `program.` out. The wire format is JSON, but it is not a
promise: the format is free to become whatever an AI client reads best, binary included.

The commands are the ones `--repl` answers:

- **paths**: `program`, `program.monsters`, `program.monsters[0].health`. A `T?` that holds a value is walked
  through, and a `Dictionary`'s entries are walked by key: `program.settings.volume`.
- **calls**: `program.monsters.count()`, `program.monsters[0].roar()`, with literal arguments. A call that answers
  an object is walked on like any path: `World().position_of(1).across`.
- **singletons**, by their class's name: `World()`, `Tick().step_milliseconds`, `Ui.Panel().title`,
  `Column<Position>().values[0]`, and one made with arguments by its arguments: `Channel(1).number`. The name binds
  the instance the program already has and never makes one.
- **a dictionary's** `count()`, `keys()` and `has(key)`: `scores.has("ann")`.
- **assignment** of a number, `Boolean`, text or enum literal: `program.player_name = "Aria"`, through
  `set_<attribute>` when the class declares one, answering the value read back afterwards. A text literal is
  written as in a program, escapes included (`"a \"quoted\" word"`). The target may be a list or dictionary element
  (`names[1] = "cat"`, `scores["ann"] = 9`), a `T?` takes `null`, and an attribute or element of a class takes
  another path's instance of that class (`follower = leader`), which then holds the same object, or one made at the
  prompt (`follower = Circle(3)`) in a program built with `--hot-reload` and `--repl-port`
  ([Code typed at the prompt](#code-typed-at-the-prompt)).
- `describe Circle` (a class's attributes and functions, with their types and signatures), `enums` (each
  of the program's enums with its values) and `memory` (the live allocations and the bytes they hold).
- `attributes`, `functions` (of the program, or of any path: `functions World()`), `classes` (the singletons the
  prompt can bind, then the program's other classes), `help`, `exit`.
- `reload`, `last_reload` and `wait_reload`, which answer in a `--hot-reload` build
  ([Live reload](#live-reload---hot-reload)) and fail everywhere else with `'reload' answers in a program built with --hot-reload, which swaps its code while it
  runs, and this one was not`.

## A worked debugging session

This session runs against the program above, built with `--repl-port`: every answer below is the exact line the
program sends back.

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
session open), read the single JSON line back, and branch on `"ok"`. `attributes`, `functions` and `classes` first, to see
what is there (only functions whose arguments are numbers, Boolean, text or an enum can be called), then walk paths
down to the value in question before mutating anything.

## Singletons: where a game's state lives

A game's state rarely hangs off its entry class: the entry makes an app and runs it, and the world, the clock and
the columns of components are singletons. The prompt binds a singleton by its class's name, the way the program's
own code does (`World()`, `Ui.Panel()`, `Column<Position>()`), and reads, calls and assigns through it. It
binds the one instance the program already made; it never makes one, so reading a singleton the program has not
asked for yet says so instead of creating it.

```gdscript title=repl_world/position.spite
var across = 0
var along = 0

func Position(new_across: Integer, new_along: Integer) {
    across = new_across
    along = new_along
}

func distance_to(other_across: Integer, other_along: Integer): Integer {
    var across_gap = across - other_across
    var along_gap = along - other_along
    return across_gap * across_gap + along_gap * along_gap
}
```
```gdscript title=repl_world/tick.spite
singleton

var step = 0
var step_milliseconds = 16
```
```gdscript title=repl_world/world.spite
singleton

var positions = List<Position>()
var tick = Tick()

func spawn(across: Integer, along: Integer) {
    var made = Position(across, along)
    positions.append(made)
    tick.step = tick.step + 1
}

func entity_count(): Integer {
    return positions.count()
}

func position_of(entity: Integer): Position? {
    return positions[entity]
}

func step_length(): Integer {
    return tick.step_milliseconds
}
```
```gdscript title=repl_world/repl_world.spite entry
var console = Console()
var world = World()

func ReplWorld() {
    world.spawn(3, 4)
    world.spawn(7, 1)
    var count = world.entity_count()
    console.print("entities", count)
}
```
```output
entities 2
```

This session runs against it, built with `--repl-port`:

```wire repl_world
# The entry class holds a console and the world; the game's state is in its singletons.
$ spite connect 4000 --command="attributes"
{"ok":true,"value":"console: Console = Console {...}\nworld: World = World {...}","type":""}

$ spite connect 4000 --command="World().entity_count()"
{"ok":true,"value":"2","type":"Integer"}

$ spite connect 4000 --command="World().position_of(1)"
{"ok":true,"value":"Position { across: 7, along: 1 }","type":"Position?"}

$ spite connect 4000 --command="World().position_of(0).distance_to(0, 0)"
{"ok":true,"value":"25","type":"Integer"}

$ spite connect 4000 --command="Tick().step_milliseconds = 50"
{"ok":true,"value":"50","type":"Integer"}

$ spite connect 4000 --command="functions World()"
{"ok":true,"value":"spawn(across: Integer, along: Integer): Nothing\nentity_count(): Integer\nposition_of(entity: Integer): Position?\nstep_length(): Integer\nto_debug(): String","type":""}

$ spite connect 4000 --command="Position()"
{"ok":false,"error":"'Position' is a class, not a singleton: the prompt never makes an object, it reads the ones the program holds, and 'classes' lists the singletons"}

$ spite connect 4000 --command="exit"
{"ok":true,"value":"","type":""}
```

A singleton made with arguments has one instance for each argument list
([classes_and_files.md](classes_and_files.md#one-instance-per-argument-values)), and the prompt names each by its
arguments, as the program does: `Channel(1)` is the instance the program's `Channel(1)` made, and an argument list
the program never asks for has no instance to read. A dictionary answers `keys()` and `has(key)` beside `count()`,
and a text literal at the prompt is written as in a program, with its escapes:

```gdscript title=repl_radio/channel.spite
singleton

var number = 0
var messages = List<String>()

func Channel(channel_number: Integer) {
    number = channel_number
}
```
```gdscript title=repl_radio/repl_radio.spite entry
var console = Console()
var news = Channel(1)
var music = Channel(2)
var volumes = Dictionary<Integer>()
var station = "city"

func ReplRadio() {
    news.messages.append("storm tonight")
    volumes["news"] = 7
    volumes["music"] = 4
    var tuned = volumes.count()
    console.print(station, "tuned", tuned, "and", music.number)
}
```
```output
city tuned 2 and 2
```

```wire repl_radio
$ spite connect 4000 --command="classes"
{"ok":true,"value":"Build()\nChannel(1)\nChannel(2)\nReplRadio","type":""}

$ spite connect 4000 --command="Channel(1).messages.count()"
{"ok":true,"value":"1","type":"Integer"}

$ spite connect 4000 --command="Channel(3)"
{"ok":false,"error":"the program holds no Channel(3): a singleton made with arguments has one instance for each argument list the program asks for, Channel(1), Channel(2)"}

$ spite connect 4000 --command="volumes.keys()"
{"ok":true,"value":"news\nmusic","type":"List<String>"}

$ spite connect 4000 --command="volumes.has(\"talk\")"
{"ok":true,"value":"false","type":"Boolean"}

$ spite connect 4000 --command="station = \"the \\\"late\\\" show\""
{"ok":true,"value":"the \"late\" show","type":"String"}

$ spite connect 4000 --command="station = \"the {hour} show\""
{"ok":false,"error":"\"the {hour} show\" has a hole, and the prompt reads a text literal as it is written: \\{ writes a brace, and 'eval' makes text from values"}

$ spite connect 4000 --command="exit"
{"ok":true,"value":"","type":""}
```

## Native memory: `bytes`

`bytes <path>` answers where a value lives and what its bytes are, as one JSON object an agent can diff between two
processes, a client's player against the server's, while it rubber-bands. For an instance it gives the class,
the address, the size, the bytes in hexadecimal, and each attribute's offset, size and value; for a list, the list
object and its buffer of items; for a number or text held by an attribute or a list, its own slot:

```text
spite> bytes World().positions[1]
{"class":"Position","address":"0x1e8647ec050","bytes":16,"hex":"04000000580000000700000001000000","fields":[{"name":"across","class":"Integer","offset":8,"bytes":4,"value":"7"},{"name":"along","class":"Integer","offset":12,"bytes":4,"value":"1"}]}
spite> bytes World().positions
{"class":"List<Position>","address":"0x1e8647f6100","bytes":40,"hex":"0300...","buffer":{"address":"0x1e8647f5f50","bytes":16,"shown":16,"hex":"f0bf7e64e801000050c07e64e8010000"}}
spite> bytes Tick().step_milliseconds
{"class":"Integer","address":"0x1e8647ebf1c","bytes":4,"hex":"10000000","value":"16"}
spite> bytes 0x1e8647ebf10 16
{"address":"0x1e8647ebf10","bytes":16,"hex":"02000000590000000200000010000000"}
spite> bytes 0x10 8
unreadable: 0x10 for 8 bytes
```

The first eight bytes of every object are its header, the reference count and the class's number. A read only
reads, and it never crashes the program it inspects: the bytes are copied through the operating system's checked
read, and an address that is not readable answers `unreadable`. At most 4096 bytes are shown at a time.

## Breakpoints

A program built with `--hot-reload` and `--repl-port` can be stopped at a line while it runs:

```text
$ spite connect 4000 --command="break ticker.spite:16"
{"ok":true,"value":"the program stops before ticker.spite:16: rebuilt Ticker","type":""}

$ spite connect 4000 --command="where"
{"ok":true,"value":"game/ticker.spite:16","type":""}

$ spite connect 4000 --command="locals"
{"ok":true,"value":"self: Ticker = Ticker {...}\namount: Integer = 1\ndoubled: Integer = 2","type":""}

$ spite connect 4000 --command="doubled"
{"ok":true,"value":"2","type":"Integer"}

$ spite connect 4000 --command="continue"
{"ok":true,"value":"continued from game/ticker.spite:16","type":""}
```

`break <file>:<line>` compiles a stop into the function that line belongs to and swaps it in, the way a reload
swaps in a saved file; a program that never sets one carries nothing for it. The next time any code reaches the
line, the program stops before it and waits for the prompt: `where` answers where it stopped, `locals` lists
`self` and every local in scope there, a path may start at a local's name (`doubled`, `self.total`), every other
command answers as usual, and `continue` lets the program go on. `breaks` lists the breakpoints and `clear` (or
`clear ticker.spite:16`) removes them, compiling the code again without the stop. A call made at the prompt that
reaches a breakpoint answers `stopped at ... while answering` at once, and prints what it returned on the error
output once it goes on. A stopped program is waiting, so a reload the watcher compiled meanwhile is swapped in
there, as at any wait.

## Code typed at the prompt

In a program built with `--hot-reload` and `--repl-port`, `eval` compiles an expression into the running program
and answers its value as text, and `run` compiles a statement and runs it:

```text
$ spite connect 4000 --command="eval stock.count() + 40"
{"ok":true,"value":"41","type":"String"}

$ spite connect 4000 --command="eval Lookup().of(12)"
{"ok":true,"value":"22","type":"String"}

$ spite connect 4000 --command="run stock.append(9)"
{"ok":true,"value":"","type":"Nothing"}
```

The code is compiled as the body of a new function of the entry class, `prompt_answer_<n>(): String` for
`eval`, whose value becomes text as it would in `var text: String = value`, and `prompt_run_<n>()` for `run`,
swapped in by a reload and called once, so it reads and changes what the entry class reaches, makes objects
(`Lookup().of(12)` makes a `Lookup` the program never held), and calls any function, a generic class's included.
A value that does not become text, and code that does not compile, answer the compiler's error, and the program
keeps its code. The function stays until the next reload, which compiles the entry file again without it.

An assignment whose right side makes an object is compiled the same way, so an attribute can be given an object
the program never made:

```text
$ spite connect 4000 --command="follower = Circle(3)"
{"ok":true,"value":"Circle { radius: 3 }","type":"Circle?"}
```

The construction becomes `prompt_made_<n>(): Circle`, which the REPL calls once and whose object it assigns as
it assigns another path's.

## Live reload: `--hot-reload`

`--hot-reload` builds a program that can take new code while it runs. Save a file of the program, and the classes
that file declares are compiled again, on their own, into a small library the running program loads and swaps in
the next time it waits or ends a pass of a loop, the same moments the REPL is answered at. Objects, singletons
and everything the REPL changed stay as they were: only functions change. Every function sits in a slot so it can
be swapped, which makes the build slower; measure performance only with a production build
([Measure only a production build](compiler.md#measure-only-a-production-build)).

```
spite game --hot-reload --repl-port=4000
```

It works with or without a REPL. Without one, each swap is reported on the program's error output (`spite: rebuilt
Monster`); with `--repl` or `--repl-port`, `reload` swaps in what changed right away and answers what it rebuilt, and
`last_reload` answers what the last swap did, which is how you learn about one the file watcher made.
`wait_reload` waits for the watcher: it answers once every save of the program's files has been compiled and
swapped in (or refused), with what `last_reload` would then say, so a tool that saves a file never polls. A program
built without `--hot-reload` has none of this: no watcher, no swapping, and `reload` answers that it was not
built for it. `--hot-reload` keeps every function (it implies `--development`), since a new version of a class
may call one nothing called before, and it compiles like the default build, at `-O0`, so a reload is quick. It is
slower than a release build, since its C is not optimised and every call goes through a slot a reload can
re-point: a `--hot-reload` build trades run speed for live information, and speed is measured on release builds.

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

This session runs against a copy of the program above, built with `--hot-reload --repl-port`:

```text
$ spite connect 4000 --command="program.visits = 42"
{"ok":true,"value":"42","type":"Integer"}

# hot_counter.spite: greeting() now returns "welcome back, visit {visits}"
$ spite connect 4000 --command="reload"
{"ok":true,"value":"rebuilt HotCounter","type":""}

$ spite connect 4000 --command="greeting()"
{"ok":true,"value":"welcome back, visit 42","type":"String"}

# monster.spite: roar() now returns "{name} roars louder", and nothing is sent: the watcher sees the save
$ spite connect 4000 --command="wait_reload"
{"ok":true,"value":"rebuilt Monster","type":""}

$ spite connect 4000 --command="monster.roar()"
{"ok":true,"value":"Goblin roars louder","type":"String"}

$ spite connect 4000 --command="exit"
{"ok":true,"value":"","type":""}
```

`visits` kept the 42 set before the first reload, and the second reload rebuilt `Monster` alone. When the watcher
has already swapped a save in, `reload` finds nothing left to do and answers `nothing changed since the code the
program runs`. For an AI: edit the file and send `reload`; or save and send `wait_reload`, which answers once the
watcher has swapped the save in.

### What a reload can change

| You change | What happens |
|---|---|
| a function's body | the next call runs the new body; a call already running finishes the old one |
| a new function on an existing class | the new code calls it, the REPL's `functions` lists it and the prompt can call it |
| a function's parameters or return type | it becomes a new function: the rebuilt class and every class that calls it are rebuilt too |
| a function you deleted | whatever still holds it (a function value, the REPL) keeps its last code; the answer says `removed Monster.roar` |
| an attribute's default value | instances made after the reload get the new default |
| a class's attributes: added, removed, renamed or retyped | every live object of the class moves to the new attributes, components in an `Items`' own memory too ([below](#changing-a-classs-attributes)); the answer says what each class's objects kept |
| an enum's values: added, removed or reordered | the whole program is compiled again, and every value a running object holds keeps its meaning; a `switch` that meets a value the reload removed halts naming it |
| a file that does not compile | refused with the compiler's error, and the program keeps all of its code |
| a function of a class the program reopens, a class of the standard library or a number (`func doubled(): Integer` in `integer.spite`) | the whole program is compiled again and every function whose code changed is swapped in, the standard library's and the compiler's helpers included |
| `environment.spite`: a setting added, removed or given a new default | the whole program is compiled again, `Environment` moves to its new attributes, and every setting is read again the way it is when the program starts: from the command line it was started with, the environment, or the new default |
| `build.spite`: a field added, removed or given a new default | the whole program is compiled again, and the program's code sees the new value: in a `--hot-reload` build a `Build` field the program declares is read while it runs |
| a file deleted | the program is compiled without it, as for any other change |
| a function, attribute or constructor that a later `load` reopens ([monkey patching](packages.md#monkey-patching-mods)) | the reload keeps load order, as a fresh start would: an edit to the earlier declaration changes nothing while the later one replaces it, an edit to the later one goes live, deleting the later one (or its file) brings the earlier one back, and a replacement a later load gains goes live; a `Build` field the program's own `build.spite` declares stays the program's ([Build options](compiler.md#build-options)). When nothing the program runs changed, the answer says `no code the program runs changed, so nothing was rebuilt` |
| a new class | its functions are compiled into the new code, and `classes` and `describe` see it: the REPL's tables of classes, singletons and enums are swapped in with the code |

A refused reload leaves the program exactly as it was, so a save that caught a file half-written is harmless: the
next save reloads.

### Changing a class's attributes

Add an attribute to a class, remove one, rename one or change its type, and save: the reload moves every object of
the class the running program holds to the new attributes, and the program carries on with the same objects: the
same identity, the same references to them. An attribute that
kept its name and its type keeps its value. A new attribute, or one whose type changed, holds its default, as it
would in an object made now. A removed attribute is released, once every object has moved. The answer names every
class it moved and what its objects kept, so nothing changes silently.

```gdscript title=live_party/hero.spite
var name = ""
var level = 1

func Hero(starting_name: String) {
    name = starting_name
}

func describe(): String {
    return "{name} at level {level}"
}
```
```gdscript title=live_party/step.spite
var distance = 0.0

func Step(new_distance: Float) {
    distance = new_distance
}
```
```gdscript title=live_party/live_party.spite entry
var console = Console()
var hero = Hero("Ann")
var steps = Items<Step>()

func LiveParty() {
    var first = Step(1.5)
    steps.append(first)
    var second = Step(2.5)
    steps.append(second)
    var described = describe()
    console.print(described)
}

func describe(): String {
    return "{hero.describe()}, walked {walked()}"
}

func walked(): Float {
    var total = 0.0
    var index = 0
    while index < steps.count() {
        total = total + steps[index].distance
        index = index + 1
    }
    return total
}
```
```output
Ann at level 1, walked 4
```

This session runs against a copy of the program, built with `--hot-reload --repl-port`. `Step` fits in
an `Items`' own memory, so its objects are not separate objects at all, and they move too:

```text
$ spite connect 4000 --command="program.hero.level = 7"
{"ok":true,"value":"7","type":"Integer"}

# hero.spite: a new attribute, 'var health = 100', and describe() says "{name} at level {level} with {health}"
# step.spite: a new attribute, 'var pace = 2.0', before 'distance'
$ spite connect 4000 --command="reload"
{"ok":true,"value":"rebuilt Hero, Step, ...\nmoved every Hero to its new attributes: health is new and holds its default\nmoved every Step to its new attributes: pace is new and holds its default","type":""}

$ spite connect 4000 --command="describe()"
{"ok":true,"value":"Ann at level 7 with 100, walked 4","type":"String"}
```

**A renamed attribute keeps its value** through a map given at the prompt, never in the program's code. A save
where, in one class, an attribute is gone while another is new could be a rename or a removal and an addition,
and a reload never guesses: it holds the change, swaps in nothing, and says what it holds. Rename `level` to
`rank` in `hero.spite` and save:

```text
$ spite connect 4000 --command="wait_reload"
{"ok":false,"error":"...the reload of Hero is held, since it would lose data: level is gone and rank is new, ... reload {Hero.attributes['level']: \"rank\"}; to let what level holds be released: reload {}"}

$ spite connect 4000 --command="reload {Hero.attributes['level']: \"rank\"}"
{"ok":true,"value":"rebuilt Hero, ...\nmoved every Hero to its new attributes: level is now rank","type":""}
```

The map pairs an attribute of the running class with the name of the new one, the same shape as a serializer's
map of names ([json.md](json.md#a-key-that-is-not-an-attributes-name)), and applies to that one reload. `rank`
holds the 7 `level` held. `reload {}` lets the old values go instead. An attribute of the map that the running
class lacks, of another type than the new one, or one that the reload does not rename is refused by name.

What it costs: a change to a class's attributes compiles the whole program, since every class that reads them is
compiled again: about 40 seconds for a large game's server, against a few for a change to function bodies alone.
The saves after it are fast again: a reload that compiled the whole program becomes what the next one is compared
with ([How it works](#how-it-works)).

**A class that starts or stops fitting an `Items`' own memory moves too.** Give `Step` an attribute an item kept
inline cannot hold, `var notes = List<String>()`, and save: each `Step` the `Items` kept in its own memory becomes an
object of its own, and the `Items` keeps references to them, as it does for any class that does not fit. Remove it
again and the objects move back into the `Items`' own memory, unless one of them is also held somewhere else, since
an item kept inline is never shared; the reload is then refused, naming the class, and swaps in nothing:

```text
$ spite connect 4000 --command="wait_reload"
{"ok":true,"value":"rebuilt Step, ...\nmoved every Step to its new attributes: notes is new and holds its default; its objects moved out of an Items' own memory into objects of their own","type":""}
```

### Knowing what a reload rebuilt

A program that makes things from its own code (an engine that cooks assets with recipes written in Spite) needs
to know when that code changed, so it can make them again with the new code. Watching the files cannot tell it:
the watcher sees the save before the new code is swapped in. The standard library's `Reload` singleton answers from the swaps themselves: `generation()` is how many reloads the program has swapped in, and
`rebuilt_since(generation)` the qualified names of the classes rebuilt after that one, as `$type.name` gives them.

```gdscript title=recooking/recooking.spite entry
var console = Console()
var reload = Reload()
var cooked = 0

func Recooking() {
    cook_again_if_changed()
    console.print("cooked with generation {cooked}")
}

func cook_again_if_changed() {
    var rebuilt = reload.rebuilt_since(cooked)
    if rebuilt.contains("Recooking") {
        console.print("the recipe changed: cooking again")
    }
    cooked = reload.generation()
}
```
```output
cooked with generation 0
```

A loop that asks every pass costs a comparison of two numbers until a reload happens. In a build without
`--hot-reload` nothing is ever swapped in: `generation()` is `0` and `rebuilt_since` answers an empty list, both
worked out while compiling, so the check folds away with the branch it guards.

### How it works

- **One slot per function.** In a `--hot-reload` build every function is called through a slot the program can
  re-point: the program's own, the standard library's, and the helpers the compiler writes. The function the rest
  of the code calls is a one-line forwarder to its slot. A normal build is untouched: direct calls, and tree
  shaking. The slot costs about a nanosecond per call, one indirect call the C compiler cannot inline
  ([measured](../specs/repl.md#live-reload-in-detail)),
  and only in a `--hot-reload` build.
- **Only what changed is compiled again.** The build writes two files beside the executable: `game.reload_host`,
  what the running program holds (every function with its C prototype, its classes and their layouts, and the
  facts its code was compiled against, below) and `game.reload_files`, a hash of each of the program's files. A
  reload runs the compiler that built the program with the same options, as `spite reload`. It reads the
  program's files and compares their hashes first, so a save that changed nothing answers at once. Otherwise it
  compiles **only the classes of the changed files**: every class is read and checked, as the changed code needs,
  but no other class's functions are compiled: the new code reaches them in the running program. The C it writes
  is only the changed classes' functions and whatever those need that the running program lacks, with the types
  and declarations they use, and it compiles that into `game_reload_1.dll` (`.so` on Linux, `.dylib` on macOS).
  For a large game's server (about 460 files; 880,000 lines of C for the whole program) a changed system is swapped
  in about 6 seconds after the save (5 of them the compiler, 1 the C compiler and starting up), where compiling the
  whole program took 20 to 30 seconds, and a save that changed
  nothing answers in under a second.
- **A reload is compiled against what the running program was compiled with, and compiles the whole program when
  that changes.** Compiling one class depends on the rest: which functions can wait, which classes fit a shape,
  which attributes something reads, the call effects that decide whether a borrowed value must be counted, how a
  dictionary is keyed, which functions the program made for a class (a setter, a template instance, a `to_debug`).
  The build records these in the manifest and a reload starts from them. When the changed code changes one of
  them (or a parameter list, or the layout of a class), code in other classes compiled against the old one would
  be stale, so the reload compiles the whole program instead, and says why on the compiler's error output
  (`spite: compiling the whole program, since outside HotCounter changed`). A whole compile compares every function
  it writes with the running program's, and every function whose C changed is swapped in, whatever class it
  belongs to, a helper the compiler wrote included. Only a helper that keeps state of its own (a singleton's
  cache, a table the program fills as it runs) has no slot, since a new copy would start empty: a change that
  reaches one refuses the reload with `the change reaches '<name>', which the running program cannot swap:
  restart the program to take the change, or undo it`. Either way what swaps in is what compiling the whole
  program writes, which compiling a reload of every file both ways and comparing them checks
  (`SPITE_RELOAD_CHECK=<file>`).
- **A reload that compiled the whole program is the next one's baseline.** After it, every function the program
  runs is what that compile wrote, so the facts, the hashes and the class ids it compiled with describe the
  running program better than the build's do. It writes them beside its library (`game_reload_2.baseline`); when
  the program swaps the library in, it keeps them as `game.reload_baseline`, and the next reload compares with them
  instead of the build's manifest; the functions and slots the executable holds stay the build's. A save after a
  change that compiled everything is fast again. Starting the program again starts from the build
  (`game.reload_start`), since a new process runs the build's code.
- **One reload at a time.** A reload compares with the files and the baseline of the code the program will run,
  which swapping a library in writes, so a reload waits to compile until the library before it has been swapped in
  (or refused): the watcher and the prompt never compile against the code before a swap that is still on its way,
  and never read those files while a swap writes them.
- **Every object of a program class can move.** In a `--hot-reload` build each object of the program's own classes
  carries two hidden words after its header: where its attributes live when they have moved, and its place in a
  list of the class's live objects, which each allocation adds to and each release takes from. Code reads an
  attribute through `Monster___fields(object)`, which in the build is the object itself. When a reload changes a
  class's attributes, the new code reads through a function instead (the moved attributes when there are some,
  the object otherwise), and the reload lays each live object's attributes out anew in a block of their own and
  points the object at it: the object stays where it was, so every reference to it (a local of a function
  running now, a list, another object) still holds it. An `Items` that keeps a class's objects (in its own memory
  or as references) is in a list of its own, and moves every item in place of its old block; when the class starts
  or stops fitting its own memory, every item moves between the two kinds of storage. The functions that read a class's
  attributes without being its own (its allocation, release and copy, the REPL's reflection, a union's dispatch,
  a template of the standard library made for it, `Items<Step>`) are called through slots too, so they move
  with it. A reload moves nothing until it has installed every slot, and releases nothing a removed attribute held
  until every object has moved. All of it is in a `--hot-reload` build only.
- **The swap happens where the program waits**, or at the end of a pass of one of its
  loops: the program loads the library
  with the operating system's loader, the one `DynamicLibrary` uses, hands it the addresses of the program's
  functions, and re-points the slots. Nothing runs halfway through a step. The library is compiled on a helper
  thread while the program keeps running, so a game keeps drawing frames while its code is rebuilt.
- **The watcher is the standard library's [`FileSystemWatcher`](standard_library.md#watch-files-and-folders)**, the one any
  program can use, started on a thread of its own: `ReadDirectoryChangesW` on Windows, `inotify` on Linux and
  `kqueue` on macOS, with no polling. It is watching before the program's entry runs and before the REPL listens,
  so a save made once the REPL answers is never missed. A save made while the build was still compiling is not
  missed either: when the program starts, each of its files is held against the text the build compiled, and if
  any differs the watcher compiles once before it waits, so `wait_reload` waits for that compile too. The
  operating system wakes the thread; a burst of changes is waited out until 100 ms pass without one, so a save
  that writes a file in pieces reloads once. Only a change to a `.spite` file, or to a folder, compiles again, and
  never one under a `.spite/` folder: the compiler writes its own output there, so a compile started by its own
  output (which, with an error in the saved code, would refuse the same code again and again) cannot happen. When the program ends, the watcher is stopped before anything it
  uses is let go, after the compile it is running finishes: it looks every quarter of a second whether the program
  is ending, so ending a `--hot-reload` program can take that long. The program's own folder is watched with every folder below it, and so is every folder it `load`s, except
  a repository's checkout under `.spite/git/` ([packages.md](packages.md#loading-a-repository-pinned-to-a-commit)),
  which is read-only: a pinned commit never changes, so there is nothing to reload there, and a change to that
  package is a new commit and a restart. Each reload's library is written beside the executable, into
  `.spite/build/` by default; only when that lies inside a watched folder (`spite .` run from inside the
  program's own folder) does it wake the watcher once more, for a check that finds nothing changed.
- The compiler, its options and the executable are recorded in the build, so the program must run from the folder
  it was built from, as `spite game --hot-reload` does.

---

Next: [Testing](testing.md), a test is a package that crashes.
