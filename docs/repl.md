# REPL and live reload

> **What is built:** `spite program --repl` runs the program, then answers `attributes`,
> `functions` and `classes`, `help`, `exit`, paths such as `monsters[0].health` or `program.player_name`, a
> singleton by its class's name (`World().entity_count()`, `Tick().step_milliseconds`), assignment of a
> number, Boolean, text or enum literal (`monsters[0].health = 5`, which prints the value read back), and calls with
> literal arguments that print what they return, chained like any path (`monsters[0].roar()`, `monsters.count()`,
> `World().position_of(1).across`), written in Spite
> (`library/read_evaluate_print_loop.spite`). `--repl-port` answers the same commands over TCP, one JSON line
> each, answered where the program waits or at the end of a loop's pass ([concurrency.md](concurrency.md#the-repl-answers-at-the-waits)),
> and `spite connect` is its client. `--hot-reload` swaps the classes whose files changed into the running
> program, keeping its state, when a file is saved or when the REPL is sent `reload`
> ([Live reload](#live-reload---hot-reload)), moving live objects to their class's new attributes when those
> change; Windows runs it, and Linux and macOS are held to compiling.
> `bytes <path>` answers a value's address, size, bytes and layout as JSON ([Native memory](#native-memory-bytes)).
> `describe Monster`, `enums` and `memory` describe a class, the program's enums and the heap in use; an
> assignment takes `null` into a `T?`, a literal into a list or dictionary element, and another path's instance
> (`follower = leader`); a path walks into a union's active member.
> **Not built yet:** code typed at the prompt, a generic class's function on an instance nothing holds, and
> reloading a change to an enum's values -- [the list](#repl-and-live-reload--partial).
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

What the loop needs to reach a live value -- the attributes and functions of every class the loop can reach,
and the member templates that fit each list's elements, so `monsters.sum_health()` works at the prompt -- is
compiled only into a `--repl`/`--repl-port` build, never into a normal one, so it costs nothing when you are not
debugging (D177). A REPL build is also an [inspectable build](compiler.md#development-builds-and-tree-shaking):
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
one client at a time -- there is **no authentication**. Anyone who can reach that port on that machine can read
and mutate the running program. This is a local debugging tool for you and an AI on the same machine, never
something to expose past `127.0.0.1`. The program keeps running while it is served, and each command is answered
on the program's own thread the next time it waits -- a `program.sleep`, a `Console.read_line()`, a file or socket
read -- or reaches the end of a pass of one of its loops, so a command never sees it halfway through a step
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
  through, and a `Dictionary`'s entries are walked by key: `program.settings.volume`.
- **calls**: `program.monsters.count()`, `program.monsters[0].roar()`, with literal arguments. A call that answers
  an object is walked on like any path: `World().position_of(1).across`.
- **singletons**, by their class's name: `World()`, `Tick().step_milliseconds`, `Ui.Panel().title`,
  `Column<Position>().values[0]`. The name binds the instance the program already has and never makes one.
- **assignment** of a number, `Boolean`, text or enum literal: `program.player_name = "Aria"` -- through
  `set_<attribute>` when the class declares one, answering the value read back afterwards. The target may be a
  list or dictionary element (`names[1] = "cat"`, `scores["ann"] = 9`), a `T?` takes `null`, and an attribute or
  element of a class takes another path's instance of that class (`follower = leader`), which then holds the same
  object.
- `describe Circle` (a class's attributes at their defaults and the functions the prompt can call), `enums` (each
  of the program's enums with its values) and `memory` (the live allocations and the bytes they hold).
- `attributes`, `functions` (of the program, or of any path: `functions World()`), `classes` (the singletons the
  prompt can bind, then the program's other classes), `help`, `exit`.
- `reload`, `last_reload` and `wait_reload`, which answer in a `--hot-reload` build
  ([Live reload](#live-reload---hot-reload)) and fail everywhere else with `'reload' answers in a program built with --hot-reload, which swaps its code while it
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
session open), read the single JSON line back, and branch on `"ok"`. `attributes`, `functions` and `classes` first, to see
what is there (only functions whose arguments are numbers, Boolean, text or an enum can be called), then walk paths
down to the value in question before mutating anything.

## Singletons: where a game's state lives

A game's state rarely hangs off its entry class: the entry makes an app and runs it, and the world, the clock and
the columns of components are singletons. The prompt binds a singleton by its class's name, the way the program's
own code does -- `World()`, `Ui.Panel()`, `Column<Position>()` -- and reads, calls and assigns through it. It
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

`check.sh` replays this session against it, built with `--repl-port`:

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

## Native memory: `bytes`

`bytes <path>` answers where a value lives and what its bytes are, as one JSON object an agent can diff between two
processes -- a client's player against the server's, while it rubber-bands. For an instance it gives the class,
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

## Live reload: `--hot-reload`

`--hot-reload` builds a program that can take new code while it runs. Save a file of the program, and the classes
that file declares are compiled again, on their own, into a small library the running program loads and swaps in
the next time it waits or ends a pass of a loop -- the same moments the REPL is answered at. Objects, singletons
and everything the REPL
changed stay as they were: only functions change.

```
spite game --hot-reload --repl-port=4000
```

It works with or without a REPL. Without one, each swap is reported on the program's error output (`spite: rebuilt
Monster`); with `--repl` or `--repl-port`, `reload` swaps in what changed right away and answers what it rebuilt, and
`last_reload` answers what the last swap did, which is how you learn about one the file watcher made.
`wait_reload` waits for the watcher: it answers once every save of the program's files has been compiled and
swapped in (or refused), with what `last_reload` would then say, so a tool that saves a file never polls. A program
built without `--hot-reload` has none of this -- no watcher, no swapping, and `reload` answers that it was not
built for it. `--hot-reload` keeps every function (it implies `--development`), since a new version of a class
may call one nothing called before, and it is compiled at `--optimized`'s level, so a program that cooks assets or
simulates between saves runs at release speed.

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
| a function you deleted | whatever still holds it -- a function value, the REPL -- keeps its last code; the answer says `removed Monster.roar` |
| an attribute's default value | instances made after the reload get the new default |
| a class's attributes: added, removed, renamed or retyped | every live object of the class moves to the new attributes, components in an `Items`' own memory too ([below](#changing-a-classs-attributes)); the answer says what each class's objects kept |
| an enum's values | refused: `the layout of ... changed`, and the program keeps all of its code (not built yet, [D280](decisions.md)) |
| a file that does not compile | refused with the compiler's error, and the program keeps all of its code |
| a file that declares none of the program's classes: `environment.spite`, `build.spite`, a reopening of a class of the standard library | refused: `'environment.spite' changed, and a reload swaps only the functions of the program's own classes, ...`, and the program keeps all of its code |
| a new class | its functions are compiled into the new code; the REPL does not see it until a restart |

A refused reload leaves the program exactly as it was, so a save that caught a file half-written is harmless: the
next save reloads.

### Changing a class's attributes

Add an attribute to a class, remove one, rename one or change its type, and save: the reload moves every object of
the class the running program holds to the new attributes, and the program carries on with the same objects -- the
same identity, the same references to them (D280; the rules proposed by Claude, unconfirmed). An attribute that
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
        var step = steps.get_at(index)
        crash step
        total = total + step.distance
        index = index + 1
    }
    return total
}
```
```output
Ann at level 1, walked 4
```

`check.sh` runs this session against a copy of the program, built with `--hot-reload --repl-port`. `Step` fits in
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

**A renamed attribute keeps its value** when the class says what it was called, with a function beside it in the
same form as a JSON key's ([D273](decisions.md)): `renamed_from_<attribute>()` returns the old name. Without one, a
rename is a removed attribute and a new one, and the value is released.

```gdscript
var rank = 1

func renamed_from_rank(): String {
    return "level"
}
```

The reload answers `level is now rank`, and `rank` holds the 7 `level` held. The function does nothing once no
object has the old name, so it can be deleted after the reload, or kept.

What it costs: a change to a class's attributes compiles the whole program, since every class that reads them is
compiled again -- about 40 seconds for SlopTheseus's server, against a few for a change to function bodies alone.
The saves after it are fast again: a reload that compiled the whole program becomes what the next one is compared
with ([How it works](#how-it-works)). Not built yet ([D280](decisions.md)): an enum whose
values change, and a class that starts or stops fitting in an `Items`' own memory -- both are refused, naming the
class.

### Knowing what a reload rebuilt

A program that makes things from its own code -- an engine that cooks assets with recipes written in Spite -- needs
to know when that code changed, so it can make them again with the new code. Watching the files cannot tell it:
the watcher sees the save before the new code is swapped in. The standard library's `Reload` singleton answers from
the swaps themselves ([D290](decisions.md)): `generation()` is how many reloads the program has swapped in, and
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

- **One slot per function.** In a `--hot-reload` build, every function of the program's own classes (not the
  standard library's) is called through a slot the program can re-point: the function the rest of the code calls
  is a one-line forwarder to its slot. A normal build is untouched -- direct calls, and tree shaking. The slot
  costs about a nanosecond per call, one indirect call the C compiler cannot inline
  ([measured](#live-reload-and-6b--implemented-on-windows-the-mechanism-and-the-rules-below-proposed-by-claude-unconfirmed)),
  and only in a `--hot-reload` build.
- **Only what changed is compiled again.** The build writes two files beside the executable: `game.reload_host`,
  what the running program holds -- every function with its C prototype, its classes and their layouts, and the
  facts its code was compiled against (below) -- and `game.reload_files`, a hash of each of the program's files. A
  reload runs the compiler that built the program with the same options, as `spite reload`. It reads the
  program's files and compares their hashes first, so a save that changed nothing answers at once. Otherwise it
  compiles **only the classes of the changed files**: every class is read and checked, as the changed code needs,
  but no other class's functions are compiled -- the new code reaches them in the running program. The C it writes
  is only the changed classes' functions and whatever those need that the running program lacks, with the types
  and declarations they use, and it compiles that into `game_reload_1.dll` (`.so` on Linux, `.dylib` on macOS).
  For SlopTheseus's server (about 460 files; 880,000 lines of C for the whole program) a changed system is swapped
  in about 6 seconds after the save (5 of them the compiler, 1 the C compiler and starting up), where compiling the
  whole program took 20 to 30 seconds, and a save that changed
  nothing answers in under a second.
- **A reload is compiled against what the running program was compiled with, and compiles the whole program when
  that changes.** Compiling one class depends on the rest: which functions can wait, which classes fit a shape,
  which attributes something reads, the call effects that decide whether a borrowed value must be counted, how a
  dictionary is keyed, which functions the program made for a class (a setter, a template instance, a `to_debug`).
  The build records these in the manifest and a reload starts from them. When the changed code changes one of
  them -- or a parameter list, or the layout of a class -- code in other classes compiled against the old one would
  be stale, so the reload compiles the whole program instead, and says why on the compiler's error output
  (`spite: compiling the whole program, since outside HotCounter changed`). A whole compile compares every function
  it writes with the running program's: another program class whose C changed is rebuilt too, and a changed
  function the program cannot swap -- the standard library's, a helper the compiler writes -- refuses the reload
  with `the change reaches '<name>', which the running program cannot swap: restart the program to take the
  change, or undo it`. Either way what swaps in is what compiling the whole program writes; `check.sh` holds it by
  compiling a reload of every file of ten programs both ways and comparing them (`SPITE_RELOAD_CHECK=<file>`).
- **A reload that compiled the whole program is the next one's baseline.** After it, every function the program
  runs is what that compile wrote, so the facts, the hashes and the class ids it compiled with describe the
  running program better than the build's do. It writes them beside its library (`game_reload_2.baseline`); when
  the program swaps the library in, it keeps them as `game.reload_baseline`, and the next reload compares with them
  instead of the build's manifest -- the functions and slots the executable holds stay the build's. A save after a
  change that compiled everything is fast again. Starting the program again starts from the build
  (`game.reload_start`), since a new process runs the build's code.
- **Every object of a program class can move.** In a `--hot-reload` build each object of the program's own classes
  carries two hidden words after its header: where its attributes live when they have moved, and its place in a
  list of the class's live objects, which each allocation adds to and each release takes from. Code reads an
  attribute through `Monster___fields(object)`, which in the build is the object itself. When a reload changes a
  class's attributes, the new code reads through a function instead -- the moved attributes when there are some,
  the object otherwise -- and the reload lays each live object's attributes out anew in a block of their own and
  points the object at it: the object stays where it was, so every reference to it -- a local of a function
  running now, a list, another object -- still holds it. An `Items` that keeps a class's objects in its own memory
  is in a list of its own, and moves every item in place of its old block. The functions that read a class's
  attributes without being its own -- its allocation, release and copy, the REPL's reflection, a union's dispatch,
  a template of the standard library made for it (`Items<Step>`) -- are called through slots too, so they move
  with it. A reload moves nothing until it has installed every slot, and releases nothing a removed attribute held
  until every object has moved. All of it is in a `--hot-reload` build only (D143).
- **The swap happens where the program waits** ([D37](decisions.md)), or at the end of a pass of one of its
  loops: the program loads the library
  with the operating system's loader, the one `DynamicLibrary` uses, hands it the addresses of the program's
  functions, and re-points the slots. Nothing runs halfway through a step. The library is compiled on a helper
  thread while the program keeps running, so a game keeps drawing frames while its code is rebuilt.
- **The watcher is the standard library's [`Watcher`](standard_library.md#watch-files-and-folders)**, the one any
  program can use, started on a thread of its own: `ReadDirectoryChangesW` on Windows, `inotify` on Linux and
  `kqueue` on macOS, with no polling. The thread sits in `wait_for_changes()`, which the operating system wakes;
  a burst of changes is waited out until 100 ms pass without one, so a save that writes a file in pieces reloads
  once. The program's own folder is watched with every folder below it, and so is every folder it `load`s, except
  a repository's checkout under `.spite/git/` ([packages.md](packages.md#loading-a-repository-pinned-to-a-commit)),
  which is read-only: a pinned commit never changes, so there is nothing to reload there, and a change to that
  package is a new commit and a restart (D283). Each reload's library is written beside the executable, into
  `.spite/build/` by default (D283); only when that lies inside a watched folder -- `spite .` run from inside the
  program's own folder -- does it wake the watcher once more, for a check that finds nothing changed.
- The compiler, its options and the executable are recorded in the build, so the program must run from the folder
  it was built from, as `spite game --hot-reload` does.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### REPL and live reload  **[partial]**

**Decided, step 1 built ([D280](decisions.md), Mortaro): nothing needs a restart.** "the fact that anything needs a
restart is a fault in language or engine design." Every refusal in [What a reload can change](#what-a-reload-can-change)
is a gap to close, not a rule. The plan (proposed by Claude, unconfirmed), in the order Theseus meets them:
1. **A class's attributes change** -- **built**, [Changing a class's attributes](#changing-a-classs-attributes): the
   reload moves every live object to the new layout, copying kept attributes by name, giving new ones their
   defaults and releasing removed ones; an `Items`' own memory moves the same way. A rename is written as a function
   beside the attribute, `func renamed_from_<attribute>(): String { return "old_name" }`, the same form as
   `json_key_<attribute>` (D273), and may be deleted once the program has reloaded. The rules as built:
   - A `--hot-reload` build gives each object of a program class two hidden words after its header, `spite_moved`
     (its attributes' block once they moved, else 0) and `spite_live` (its place in the class's list of live
     objects), reads every attribute through `<Class>___fields(object)`, and describes each class's layout --
     every attribute's name, type, offset, size and release -- in a table the reload reads (`<Class>___layout`).
     An `Items` or `Vector` holding a class's objects in its own memory is kept in a list of its own.
   - A class's layout is its attributes' names and types in order, and whether it fits an `Items`' own memory. A
     reload whose layout for a class differs from what the running program holds compiles the whole program. It
     compiles every function that reads that class's attributes or its size again and swaps it in; one it could
     not swap is refused by name (`the change reaches ...`), and the program keeps all of its code.
   - The library installs every slot it replaces or none, then moves the objects: each live object of a changed
     class gets a new block, made with the new defaults, into which each attribute of the same name and type is
     moved -- or of the name `renamed_from_<attribute>()` gives, first. Every attribute of the old layout nothing
     took is released after every object of every changed class has moved, so a release that reaches another
     moved object finds it moved. Objects made while moving (a new attribute's default) are made in the new layout
     and not moved again.
   - An attribute of a new type is a new attribute: it holds its default. The reload names every class it moved,
     and for each what is new, gone, renamed or retyped (`moved every Hero to its new attributes: health is new and
     holds its default`), in its answer and on the error output.
   - Once a class has moved, its code reads through the function for as long as the program runs. Refused, not
     built: a class that starts or stops fitting an `Items`' own
     memory, an enum whose values change, and a change to a value class (`String` and the numbers).
2. **Dependents are rebuilt**: a change whose dependents cannot be swapped alone compiles the whole program into
   the reload library and re-points every slot, keeping the heap, instead of refusing.
3. **Enums change**: stored values are re-mapped by name; a value removed while something still holds it is
   refused naming the holder, as a stale value would otherwise be a silent wrong answer (D244).
4. **`environment.spite` and `build.spite`**: a `Build` field is a constant folded into the code, so changing one is
   step 2 -- the whole program recompiled with the new value and swapped in live.
All of it lives only in a `--hot-reload` build (D143): a normal build carries none of it.

A REPL that inspects and drives the *running* program, local (`--repl`) and remote (`--repl-port`), and live
reload (`--hot-reload`, D111, D112). The REPL is Spite, `library/read_evaluate_print_loop.spite` (D72), walking the
program through reflection; nothing of it is in a build that did not ask for it (D177, D143). **Built:** paths,
singletons by name, assignment of a literal, calls with literal arguments and paths through what they answer,
`attributes`, `functions`, `classes`, `describe`, `enums`, `memory`, `help`, `exit`, `reload`, `last_reload` and
`wait_reload`, assignment into elements, `T?`s and of whole instances, and walking into a union
(`conformance/stage6/interactive_loop`, `conformance/stage6/interactive_paths`,
`conformance/stage6/interactive_singletons`), `--repl-port` with
`spite connect` over `Socket`, answered where the program waits and at each loop's check point (D37, D174), and
live reload on Windows, with the Linux and macOS folders held to compiling (`conformance/stage6/interactive_inspection`
for D300). **Not built:** calling a generic class's function on an instance nothing holds (`Lookup<Position>().of(12)`, below);
compiling new Spite code typed at the prompt; and reloading a change to an enum's values or to whether a class fits
an `Items`' own memory.

#### Reflection in a REPL build  **[implemented]**

(The mechanism proposed by Claude, unconfirmed.) The emitted `main` hands the loop the entry instance as a
`Spite.Attribute` named `program` before every command. In a `--repl` or `--repl-port` build a `Spite.Attribute`
stays linked to the live value it describes, through four functions the compiler supplies: `value_attributes()`
and `value_functions()` read the value's own attributes and functions (a list's attributes are its elements, named
`0`, `1`, ...; a dictionary's are its entries, named by their keys), `assign(text)` writes a number, `Boolean`,
text or enum value into it -- through the class's `set_<attribute>(value)` when it declares one -- and answers
whether it could, and a `Spite.Function`'s `call_with_text(arguments)` calls it with literal arguments and answers
its result as a `Spite.Attribute?`, `null` when an argument is not one the loop can write. Outside those builds
they answer an empty list, `false` and `null`, and the reflection they walk is not emitted. A function is
callable from the prompt when every parameter is a number, `Boolean`, text or an enum; an instantiated
Symbol-codegen function (`set_age`) is one like any other, and a template nothing calls is still never
instantiated, so tree shaking of templates is unaffected. A REPL build is inspectable ([compiler.md](compiler.md#inspectable-and-production-builds)):
nothing else is tree-shaken either.

#### Command language

A small subset of Spite expressions, rooted at the entry instance under the fixed name `program` (regardless of
the entry class's own Spite name). Built:

- **paths**: `program`, `program.monsters`, `program.monsters[0].health`, `program.settings.volume`,
  `program.target`. A path may leave out the leading `program.`, so `player.health` and `program.player.health`
  name the same value (proposed by Claude, unconfirmed). Walking through a non-null `T?` is transparent; a
  dictionary's entries are walked by key.
- **calls**: `program.monsters.count()`, `program.monsters[0].roar()`, `program.player.set_age(3)` (any
  callable function, including an instantiated Symbol-codegen one); `List<T>`'s and `Dictionary<T>`'s `count()`,
  which must end the expression. A call that answers a value is walked on like any path,
  `World().position_of(1).across`, and it runs once; a call that answers `Nothing` ends the expression, and a
  path after one is refused after it ran: `World().spawn() ran, and it answers nothing, so nothing can follow it`
  (D286, decided by Claude under D205, implementing D242).
- **singletons by name** (D286, decided by Claude under D205, implementing D242). A call whose name starts with a
  capital letter, alone or after a namespace (`World()`, `Ui.Panel()`, `Column<Position>()`), names a singleton
  class and binds the one instance the program holds, as the program's own `var world = World()` does; a path,
  call or assignment continues from it (`Tick().step_milliseconds = 50`). It never makes an instance: a singleton
  the program has not asked for yet is refused, `Tick has not been made yet: nothing in the program has asked for
  it, and the prompt never makes one`, and so is a class that is not a singleton (`'Position' is a class, not a
  singleton: the prompt never makes an object, it reads the ones the program holds, and 'classes' lists the
  singletons`), an argument (`'World' is a singleton, so it takes no arguments: ...`) and a name no singleton has
  (`no singleton 'Wrld' in the program: 'classes' lists them`). The whole name is the class's qualified name with
  its generic arguments; the name without its namespace is accepted when one singleton has it, and a name two
  singletons share is refused naming both. A generic class that is not a singleton, such as `Lookup<Position>()`,
  is refused like any class: reaching its functions without an instance is the plan below.
- **How the loop finds them** (proposed by Claude, unconfirmed). In a REPL build the compiler writes two functions
  of `ReadEvaluatePrintLoop`: `singletons()`, a `Spite.Attribute` for each singleton the program binds, named by
  its qualified name and linked to the instance when it has been made (read from the singleton's static slot
  with an acquiring load, never through its constructor), without a value when it has not; and `class_names()`,
  the program's own classes that are not singletons. Of the standard library's singletons only `Environment` and
  `Build` are listed: the rest (`Console`, `Scheduler`, `TypedMemory<T>`, ...) are the language's own machinery,
  and describing them would drag most of the library's classes into the build's reflection, which a reload that
  compiles only the changed classes would then have to reproduce. Any other build writes them as an empty list and empty text, and nothing calls
  them, so they are shaken out with the rest of the loop: a normal build carries none of it (D143, D177).
- **`bytes`: native memory** (D291, decided by Claude under D205, implementing D281 and D282; the command's name
  provisional under D214).  **[implemented]** `bytes <path>` answers one JSON object: for a value the path holds
  an object of (an instance, a list, a dictionary, a singleton), `class`, `address` (hexadecimal text), `bytes`
  (the object's size), `hex` (its bytes, at most 4096 shown) or `"unreadable":true`, then for an instance `fields`,
  each with `name`, `class`, `offset` from the object's start, `bytes` and `value` (printed as the loop prints a
  nested value), and for a list `buffer` with the item buffer's `address`, `bytes`, `shown` and `hex`; for a
  number, `Boolean`, enum or text held by an attribute or an element, `class`, `address`, `bytes`, `hex` and
  `value` of its own slot. A value reached only through what a call answers, with no slot of its own, is refused
  naming what to ask instead. `bytes 0x1f2a40 64` reads a range: an address in hexadecimal and 1 to 4096 bytes,
  anything else refused with the form. **A read never crashes the inspected program** (D244): the bytes are
  copied by `Memory.Inspector.hex_at`, which asks the operating system -- `ReadProcessMemory` on the program's
  own process on Windows, a write into a pipe on Linux and macOS, which fails with `EFAULT` rather than
  faulting -- and a range it cannot copy whole answers `unreadable: 0x10 for 8 bytes`. Reads only.
- **What `bytes` is built from** (D282: inspection is library Spite, and the REPL one caller). Three members of
  `Spite.Attribute`, supplied by the compiler (proposed by Claude, unconfirmed): `held_memory()`, the object a
  reflected value links to (its address and `sizeof` its C struct); `stored_memory()`, the slot an attribute or
  element is stored in, from which an attribute's offset is its address less its owner's; and `buffer_memory()`,
  a list's item buffer. Each answers `null` where there is none, and they answer only for a `Spite.Attribute`
  linked to a live value, which is a REPL build's (outside one every `Spite.Attribute` answers `null`). The
  copying is `Memory.Inspector` (`library/memory/inspector.spite`, with each system's `copy_readable` in its
  folder), a singleton any program can call on `value.memory`'s address (`conformance/stage6/memory_inspector`).
  A program that never inspects carries none of it; the attribute's five hidden numbers are set only where a
  REPL build links it.
- **Generic functions beyond the instances the program holds** (the plan, proposed by Claude, unconfirmed; not
  built). The prompt calls what the build compiled: a generic class's functions on an instance some path reaches,
  or on a generic singleton (`Column<Position>().values[0]`). An expression the build never compiled --
  `Lookup<Position>().of(12)`, which makes a `Lookup` -- needs code the program lacks. In a `--hot-reload` build
  the reload machinery already compiles the whole program and writes a library of what changed; the prompt would
  hand it the typed expression as the body of a function of a class of its own, compile that library, load it
  and call the function once, with the loop's usual reflection for the answer. The program must allow the
  construction it asks for, so that path is refused for a singleton's constructor exactly as above.
- **assignment** of a number, `Boolean`, text or enum literal: `program.player.age = 5`,
  `program.monsters[1].name = "rat"`, `program.player.job = 'knight'`, through `set_<attribute>` when the class
  declares one -- the same rule ordinary compiled Spite code follows. It answers the value read back afterwards,
  so a `set_<attribute>` that refused it shows the old one. A text literal has no escapes yet.
- `attributes` (the entry instance's attributes, `name: Class = value` a line), `functions` (its functions,
  `name(argument: Class, ...): Returns` a line), both also of any path (`functions World()`, `attributes
  Tick()`); an empty list is never an empty answer (D244): `program is a ReplWorld, which has no function the
  prompt can call: 'classes' lists the singletons, whose functions the prompt can call, such as
  World().entity_count()`, and `... which has no attributes`. A function whose name starts with `_` is private to
  its class and `drop()` runs only when a value is released, so the prompt neither lists nor calls either:
  `'_swap' is private to values, and the prompt calls what code outside the class may call`. `classes` (D286) answers the singletons the prompt
  can bind, `World()` a line with `not made yet` after one the program has not made, then the program's other
  classes a line each. Then `help`, `exit`, and `reload`, `last_reload` and `wait_reload`
  ([Live reload and 6b](#live-reload-and-6b--implemented-on-windows-the-mechanism-and-the-rules-below-proposed-by-claude-unconfirmed)).
- Printing a class instance shows `ClassName { attribute: value, ... }` one level deep: a nested class attribute
  prints as `ClassName {...}`; a list or dictionary prints as `List<T>(count)`/`Dictionary<T>(count)` regardless
  of depth; a `T?` prints `null` or its held value at the *same* depth; a union prints its active member through
  its `to_string()` (`Circle { radius: 2 }`) at any depth; text prints without quotes (`name: hero`); a call that
  returns `Nothing` prints nothing.
- Every error is one line and never a crash: an unknown attribute names the ones that do exist (`no attribute
  'monstrs' in program: console, player_name, player_age, monsters`), an index out of range says so (`index 5 is
  out of range: names holds 2`), a literal of the wrong kind is a type mismatch (`type mismatch: target.health is
  an Integer, and "x" is not one`), calling an attribute is `target.name is not callable`, and an unknown function
  is `no function 'nope' in program: 'functions' lists them`.

**Not built** (the design, kept for when it is): `Dictionary<T>`'s `keys()` and `has(key)` at the prompt, and
making an object at the prompt (`follower = Circle(3)`), which is code typed at the prompt.

**Built by D300** (decided by Claude under D205, implementing D282; the command names and the three members
provisional under D214):

- **`describe <class>`** answers the class's qualified name (`, a singleton` after one), its attributes at their
  defaults (`name: Class = value` a line, read from the stand-in `Spite.Class.attributes` makes, so nothing runs)
  and the functions the prompt can call, with their signatures. The name is the qualified name, or the name without
  its namespace when one class has it; two classes sharing it are refused naming both, and a name no class has is
  `no class 'Nope' in the program: 'classes' lists them`. It reads `Spite.Class.instances`: the program's classes
  and every loaded package's, not the standard library's. The entry class has no stand-in, so it lists no function.
- **`enums`** answers each of the program's enums, `Owner.Name: 'value', ...` a line, from `enum_names()`, which the
  compiler writes into a REPL build like `class_names()`; a program with none answers `the program declares no enum`.
- **`memory`** answers `<n> live allocations holding <b> bytes`, from `Program().live_allocations()` and
  `live_bytes()` ([memory.md](memory.md)), which any program can call.
- **Walking into a union**: a union value is walked as its active member, `shape.radius`, and prints with the
  member's class name. The reflected attribute keeps the union as its `.class`; `held_class()` answers the class of
  the object it holds when that differs (a union's active member), and `null` otherwise.
- **Assignment**: `null` goes into a `T?` (`nickname = null`) and is refused elsewhere (`mood is a Mood, which is
  never null: only a T? holds null`); a literal goes into a list or dictionary element through the element's own
  slot; and a right side that is not a literal is a path, whose instance an attribute or element of a class (or of
  its `T?`) then holds -- through `set_<attribute>` when the class declares one, and only when the classes match:
  `follower is a Circle?, and spare is a Square: the loop assigns an instance of the attribute's own class`. The
  compiler-supplied members are `Spite.Attribute`'s `assign_null()` and `assign_attribute(source)`, beside
  `assign(text)`; each answers whether it could.

#### `--repl`  **[implemented]**

Runs the entry constructor normally; when it returns, instead of dropping the entry instance and
exiting, reads commands from stdin with a `spite> ` prompt until `exit` or end of input, then drops and
exits normally (memory balanced -- checked by its own end-to-end test the same way `--debug-memory`'s
own tests are).

#### `--repl-port=<port>`  **[implemented]**

`repl_port` is a `Build` field ([Build settings: `Build`](programs.md#build-settings-build--implemented)), so the
port is part of the build (D125: no run-time override; changing the port means rebuilding). Before the entry
constructor runs, the program starts a background thread with a TCP server bound to `127.0.0.1:<port>` **only**
-- it never listens on any other interface, and there is **no authentication**: anyone who can reach that port on
that machine can read and mutate the running program. This is a local debugging tool, not something to expose
past `127.0.0.1`.

The line protocol is designed for an AI client: each request is one line of the command language; each
response is one line of JSON, `{"ok":true,"value":"...","type":"Integer"}` on success or
`{"ok":false,"error":"..."}` on failure (values and errors are JSON-escaped strings; a multi-line value, such as
the answer to `attributes`, uses `\n` inside that one JSON string, never a real newline in the wire bytes).
Clients connect and disconnect one at a time while the program keeps running; when the entry constructor returns,
the process stays alive serving the REPL until a client sends `exit`.

The choices below are **(proposed by Claude, unconfirmed)**:

- **Spite, over `Socket`.** The server is `ReadEvaluatePrintLoop.serve` in `library/read_evaluate_print_loop.spite`,
  over the `Socket` class ([System classes](standard_library.md#system-classes--implemented)), whose operating-system members live in `library/windows/socket.spite`
  (`ws2_32.dll`), `library/linux/socket.spite` (`libc.so.6`) and `library/mac/socket.spite` (`libSystem.dylib`),
  reopened per D80. The REPL listens with `listen_locally`, which binds `127.0.0.1` and nothing else, so
  "loopback only" holds by construction. `ws2_32.dll` is opened when the first `Socket` is made, so a program without a REPL never loads it.
- **The thread.** The one piece of C this adds is written by the compiler, only in a `--repl-port` build: a
  two-line thread entry, `spite_remote_loop_thread`, that calls `ReadEvaluatePrintLoop.serve` with the entry
  instance. Spite starts it and waits for it through the operating system's folder (`CreateThread` and
  `WaitForSingleObject` from `kernel32.dll`; `pthread_create` and `pthread_join` elsewhere). No `Thread` class is
  added to the library: D35 already says how a program is concurrent, and this thread belongs to the REPL.
- **Answered where the program waits** (D37). The socket thread only reads a command, hands it to the program's
  scheduler ([concurrency.md](concurrency.md#the-repl-answers-at-the-waits)) and waits for the answer. The
  scheduler answers it on the program's thread the next time the program waits -- `program.sleep`,
  `Console.read_line()`, a `File` read or write, a `Socket` accept or read, reading an unfinished `Concurrent`'s
  value, joining a `Parallel` -- so a command sees the program between two steps, never in the middle of one, and
  the program's state is never read or written by two threads at once. After the constructor returns, the program
  waits for nothing but commands. A program that never waits is answered at its loops' check points (D174,
  below); `docs/concurrency.md` replays a frame loop served between frames and a busy loop served between passes.
  The standard library's own loops have no check points, so a command waits for a library call to return.
- **Every loop is a check point in a REPL build** (D174, decided by Mortaro; the mechanism proposed by Claude,
  unconfirmed). In a `--repl-port` or `--hot-reload` build, the generator ends every pass of every
  `while` in the program's own code (not `library/` or `launcher/`) with a check point: one relaxed load of a C flag
  that `Scheduler.signal()` raises, which the REPL's thread and the file watcher call when they hand something over
  (D211, decided by Claude under D205). Only when it is raised does the pass call `Scheduler.check_point()`, which
  lowers it and, on the scheduler's thread, answers a pending command (the handover flag `answer_pending` already
  reads) and runs a pending reload. On any other thread -- a `Parallel`, a helper -- it does nothing. A loop that never waits is
  answered between two passes, so the old compile error for such a loop, and `diagnostics/remote_loop_never_waits`,
  are gone. A build without those flags has no check point: its C is byte for byte what it was (checked on a busy
  loop and `examples/dungeon`). `--repl` alone answers after the constructor returns, from the console, so it needs
  none. The cost where it exists is one load per pass: 400 000 000 passes took 106 ms, against 1 566 ms when each
  pass called `check_point()` (`docs/optimizations.md`).
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
  first. When the constructor returns first, `main` serves commands until then. With `--repl` too, the console
  loop runs first, and after its `exit` the process keeps serving.
- `spite program --repl-port=4000` is the form, as for every `Build` field; a port outside 1 to 65535 is
  `error: --repl-port takes a port number from 1 to 65535: spite program --repl-port=4000`.

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
flag of its own), as built. [Live reload](#live-reload---hot-reload) above teaches it, and `check.sh` runs its
session against a copy of the program it edits.

- **The flag.** `hot_reload` is a `Build` field (default `false`). It **implies** `--development` rather than
  requiring it, since a new version of a class may call a function nothing called before. It works without a REPL:
  the watcher still swaps, and each swap or refusal is one line on the program's error output (`spite: rebuilt
  Monster`); an explicit rebuild needs `--repl` or `--repl-port`, whose `reload` swaps in what changed and answers
  what it rebuilt, whose `last_reload` answers what the last swap (the watcher's included) did, and whose
  `wait_reload` answers the same once the watcher is done (below). A build without
  it has no slots, no watcher and no reload code: `reload` answers `'reload' answers in a program built with
  --hot-reload, which swaps its code while it runs, and this one was not`, from a branch folded on the `Build`
  constant. As with `--repl-port`, a `--hot-reload` build swaps where the program waits and at each loop's check
  point (D174). `spite program --hot-reload` runs the program attached to the terminal, like a REPL build.
- **A `--hot-reload` build is optimised** (A74; the level proposed by Claude, unconfirmed). Its executable and
  every reload library are compiled at `--optimized`'s level (`-O3`) whether or not `--optimized` is given, so a
  program that cooks or simulates between saves runs at release speed and swapped-in code as fast as the code it
  replaces. It is one C file, so it has no link-time optimisation, and it keeps every run-time check of an
  inspectable build. Every call of a function a reload can replace still goes through its slot: the slot is read
  with an acquiring atomic load, which the C compiler may neither fold to the function the build started with nor
  hoist out of a loop, and a reload writes it with a sequentially consistent store.
- **`wait_reload` waits for the watcher** (asked for by `check.sh`, whose live-reload step polled `last_reload`;
  the name proposed by Claude, unconfirmed). It answers once the watcher is not compiling, and every file the
  running code was compiled from has the size and modification time it had when the last reload (the watcher's or
  `reload`'s) started compiling; then it swaps in what that compile made, if nothing has yet, and answers what
  `last_reload` would. Over `--repl-port` the waiting is done by the connection's thread, so the program keeps
  running meanwhile. A file new since the last compile is seen only through the watcher's event. It never hangs
  (D244): after ten minutes it answers `the watcher has not swapped in the saved files after 10 minutes: 'reload'
  compiles them now`.
- **A `Concurrent` does not overlap in a `--hot-reload` build** (proposed by Claude, unconfirmed). A program
  function, called through its slot, is not compiled into a state machine (D176), so a `Concurrent` of one runs to
  its end when it is made, before the line
  after it: `var ringing = Concurrent(ring)` followed by `console.print("started")` prints what `ring` prints
  first. The program's results are the same; only the overlap is lost.
- **The swap mechanism: one slot per function.** In a `--hot-reload` build every function of the program's own
  classes (not `library/` or `launcher/`), and each such class's `_init` (its attribute defaults), is written as
  `<name>_hot`, with a function pointer `<name>_slot` holding it, and `<name>` becomes a one-line forwarder through
  the slot. Every call site, function value and REPL thunk still names `<name>`, so all of them follow a swap;
  library functions stay direct calls. Any other build is unchanged: direct calls and tree shaking. The cost is one
  indirect call that the C compiler cannot inline, about a nanosecond: 200 million calls to a one-line function
  took about 0.62 s instead of 0.44 s at `-O0` and 0.40 s instead of 0.18 s with `--optimized` (Windows, clang).
- **What the build records.** Beside the executable, `<program>.reload_host` lists every function the executable
  defines with its C prototype and a hash of its C, its slots and the class each belongs to, its class ids in
  order with each class's identity (its C name), the C layout of every class and enum, every dictionary the build
  keyed by whole numbers (`key Long Columns@slop/columns.spite:11:43`, D224), and what a reload needs to compile
  one class against the rest (proposed by Claude, unconfirmed): the generic instances, object-literal classes and
  lent variants it made, in order; the functions each program class declares; the classes that fit each shape; the
  functions that can wait; a digest of each function's call effects and of the other whole-program studies; and
  the facts compiling each class produced, each with the classes that produced it (`fact <classes>\t<fact>`: an
  attribute read, a class admitted to a shape, a flag such as `threads`, a symbol, an allocator set on a class).
  `<program>.reload_files` holds a generation number, a hash of each of the program's files -- every file it
  compiled from outside `library/`, not only those declaring its classes -- and the build's output settings, so a
  reload compiles with the same `Build` values the running program has. The executable
  holds a table of its functions' addresses by name, the path of the compiler that built it and that compiler's
  options, so it must run from the directory it was built from.
- **Rebuilding: `spite reload <folder> <options> --executable-path=<program>`** (a command since D128, when
  `--mode` went; proposed by Claude, unconfirmed). A reload runs that compiler with those options, less the
  outputs, and prints its errors to standard output, where the running program reads them.
  It reads the program's files and compares their hashes before compiling anything: nothing changed answers
  `unchanged` (under a second for SlopTheseus; before, it compiled the whole program twice to say so, 90 seconds).
  Otherwise it **compiles only the changed classes** (proposed by Claude, unconfirmed): it resolves every class,
  makes again the generic instances, object-literal classes, lent variants and made functions the manifest lists,
  takes the facts of every unchanged class from the manifest, and compiles the functions of the classes the
  changed files declare, and any function the running program lacks, skipping every other function the running
  program has with the same prototype. The class ids are seeded from the manifest by identity, so every class
  keeps the id its instances carry (before, ids were seeded by display name, and the box of an enum named like a
  class -- `JsonSymbols.Color` beside the library's `Color` -- took that class's id), and the dictionary key kinds
  are seeded too, so a program whose kinds did not change is compiled once. **It compiles the whole program
  instead** when the changed code changes what other classes were compiled against: a parameter list or return
  type; a function the running program made for a changed class that is not made again; a fact whose set of
  classes changes, other than attribute reads and parameter-write answers (which decide only errors and the
  changed code); the digest of any function's call effects, the other whole-program studies, the functions that
  can wait, or the way a function takes its arguments; a dictionary's key kind; or the classes that fit a shape the
  changed code's C names. A whole compile compares the hash of every function's C with the manifest's, numbered
  names (`spite_temp_3`) and text constants compared by what they hold: a function of another program class that
  changed is rebuilt with the changed ones, and any other changed function refuses the reload. It writes no C for
  the whole program, only the library's, whose types and prototypes are shaken to what its functions use. With
  `SPITE_RELOAD_CHECK=<file>` set, `spite reload` treats that file as changed, compiles the reload both ways, and
  prints whether the fast one writes what the whole one does; it is how `check.sh` holds the fast reload.
  **A changed file that declares none of the program's classes is refused** (proposed
  by Claude, unconfirmed; D244): a file reopening a class of the standard library, as `environment.spite` reopens
  `Environment`, or `build.spite`, holds nothing a slot can swap, and before, its change was answered `unchanged`
  and silently not applied. Now the reload answers `'<file>' changed, and a reload swaps only the functions of the
  program's own classes, which it holds none of (it reopens a class of the standard library, or sets how the
  program is built): restart the program to change it, or undo that change to reload the rest`, and the program
  keeps all of its code. The rebuilt set is the
  classes the changed files declare, plus every class whose code calls a function of the set that the running
  program has with another prototype or does not have at all (the running caller would still call the old one),
  repeated until nothing is added. It writes C only for the rebuilt classes' functions and what they reach that the
  running program lacks, reaching every other function through a pointer the library is handed when it is loaded,
  and compiles `<program>_reload_<n>.dll` (`.so`, `.dylib`). It prints `rebuilt A, B`, `removed A.f` when a function
  the running program has is gone, and the library's path.
- **Swapping at a drain point** (D37). The running program opens the library with `LoadLibraryA`/`dlopen`, the
  calls `DynamicLibrary` opens a library with, and calls its `spite_reload_bind`, which looks up each running
  function it uses by name (the allocator included, so the library allocates and frees through the program) and then
  re-points the slots of the rebuilt functions whose prototypes are unchanged, with an atomic store. **The compile
  runs on a helper thread** (D211, decided by Claude under D205): the watcher's thread compiles a change it sees,
  and the REPL's thread compiles before it hands `reload` over, one compile at a time; the finished library waits
  until the program's first wait or check point after it, where the program swaps it in, so the program never waits
  for the compiler. `reload` answers what its own compile produced (swapped in by then), and a later result
  replaces one not yet swapped in, except that `unchanged` never replaces a library. While the swap runs, the
  scheduler is paused and a wait blocks where it is, so nothing else runs until it is done. After a swap the file
  hashes are updated. A library is never unloaded: values it made, such as its text literals, may still be
  referenced.
- **What reloads.** A function body (the next call runs it; a call already running finishes the old one). A new
  function or a new class (the new code calls it). **Reflection follows the reload** (D211, decided by Claude under
  D205): each program class's list of functions, `<Class>___functions`, has a slot like a function, and a rebuilt
  class's library swaps in its new list, so the REPL's `functions` lists a function a reload added and calls it at
  the prompt (`check.sh` adds `growl()` to `monster.spite` and calls `monster.growl()`). The cost is one slot per
  class, only in a `--hot-reload` build. A changed parameter list or return type (a new function: the rebuilt class and its callers are
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
  which returns once 100 ms pass without a change, then compiles the change and wakes the scheduler. The program's
  own folder is watched with every folder below it, and so is **every folder the program loads** (D211: the
  compiler writes their list into the build, `HotReload.loaded_folders()`; `check.sh` holds it for
  `conformance/stage6/load_on_build`).
- **Windows' C runtime.** When the C compiler targets MSVC (its `-dumpmachine`), the program and its libraries are
  built against the C runtime DLL (`-fms-runtime-lib=dll`) so they share one heap and one standard output.
- **Untested:** Linux and macOS -- their watchers, `.so`/`.dylib` libraries and `Program.executable_path` (which
  the compiler uses to record itself) -- are held to compiling by `check.sh`, and are written the same way as
  Windows'. A crash inside reloaded code reports the library's own assert trace. A `Parallel` running a function
  whose slot is re-pointed finishes the old code.
