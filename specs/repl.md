# REPL and live reload

The specification of [REPL and live reload](../docs/repl.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Nothing needs a restart

No change to a program's code needs a restart. Every refusal in [What a reload can change](../docs/repl.md#what-a-reload-can-change)
is a gap to close, not a rule. The steps, in the order a game meets them:
1. **A class's attributes change**, [Changing a class's attributes](../docs/repl.md#changing-a-classs-attributes): the reload moves
   every live object to the new layout, copying kept attributes by name, giving new ones their defaults and
   releasing removed ones; an `Items`' own memory moves the same way. A rename is given at the prompt, never in the
   program's code: `reload {Hero.attributes['level']: "rank"}`. The rules:
   - A `--hot-reload` build gives each object of a program class two hidden words after its header, `spite_moved`
     (its attributes' block once they moved, else 0) and `spite_live` (its place in the class's list of live
     objects), reads every attribute through `<Class>___fields(object)`, and describes each class's layout --
     every attribute's name, type, offset, size and release, in a table the reload reads (`<Class>___layout`).
     An `Items` or `Vector` holding a class's objects, in its own memory or as references, is kept in a list of
     its own.
   - A class's layout is its attributes' names and types in order, and whether it fits an `Items`' own memory. A
     reload whose layout for a class differs from what the running program holds compiles the whole program. It
     compiles every function that reads that class's attributes or its size again and swaps it in; one it could
     not swap is refused by name (`the change reaches ...`), and the program keeps all of its code.
   - The library installs every slot it replaces or none, then moves the objects: each live object of a changed
     class gets a new block, made with the new defaults, into which each attribute of the same name and type is
     moved, or the attribute the reload's map pairs with it, first. Every attribute of the old layout nothing
     took is released after every object of every changed class has moved, so a release that reaches another
     moved object finds it moved. Objects made while moving (a new attribute's default) are made in the new layout
     and not moved again.
   - Nothing else runs while it installs and moves: the scheduler's tasks are paused, and every task on the thread
     pool (a `Parallel`'s work, a `parallel_each_` pass) stops first at the next pass of one of its loops, where it
     holds no attribute half read, and goes on once the swap is done; a task that starts meanwhile waits to start.
     A task that reaches no such point within 10 seconds (it waits on a lock, sleeps, or reads a socket) refuses
     the reload: `a task on another thread did not reach a loop's check point within 10 seconds, so nothing was
     swapped in and no object moved under it: reload again once that work has finished`. This is a
     `--hot-reload` build's alone: each task counts itself in and out of a gate, and each check point a task
     passes looks whether a swap is waiting.
   - An attribute of a new type is a new attribute: it holds its default. The reload names every class it moved,
     and for each what is new, gone, renamed or retyped. When, in one class, an attribute is gone while another is
     new and the reload was given no map, the reload is held: nothing is swapped in, and the answer names both and
     the map that would keep the values. `reload` with a map (`{}` included) compiles again with it; the map
     applies to that compile only, and one naming an attribute the running class lacks, of another type, or one
     the reload does not rename is refused. The map is a map literal as a program writes one: entries apart with
     commas (a last comma allowed), each a key, a colon and a value, at any spacing; a key is
     `<Class>.attributes[<name>]`, the name a symbol (`'level'`) or a text, and a value is a text literal, escapes
     included, holding an attribute's name. Anything else is refused before anything is compiled, naming the entry
     and the form (`'Hero.attributes['level'] "rank"' is not an entry of the map: a key, a colon and a value, as
     Hero.attributes['level']: "rank"`).
     The moves are named in the answer and on the error output (`moved every Hero to its new attributes: health is
     new and holds its default`).
   - Once a class has moved, its code reads through the function for as long as the program runs.
   - A class that stops fitting an `Items`' own memory (an attribute of a class or a list, a `drop()`, a function
     that uses `this` as a value) moves each item out: it becomes an object of its own, made like any other with
     the class's new defaults, takes the item's attributes as a moved object does, joins the class's live objects,
     and the `Items` keeps a reference to it. A class that starts fitting moves each object an `Items` holds into
     its own memory once every object of the class has moved, and the object, which the `Items` held alone, is
     freed. When one of those objects is also held somewhere else (an attribute, another list, a local), the reload
     is refused before any slot is installed, `the program keeps the code it runs: Step now fits an Items' own
     memory, but an Items holds a Step that is held elsewhere too, ...`, since an item kept inline is never shared
     and moving it would part it from the other holder; letting the other reference go and reloading again moves
     it. The class's `Spite.Class` answers `fits_vector()` from the layout the program runs, so it follows the
     move. The answer names the move (`its objects moved out of an Items' own memory into objects of their
     own`, `its objects moved into an Items' own memory`).
2. **Dependents are rebuilt**: every function of a `--hot-reload` build has a slot, the standard library's, a
   number's (a function a program adds to `Integer`) and the helpers the compiler writes included, so a change
   whose dependents cannot be swapped alone compiles the whole program and swaps in every function whose C
   changed, keeping the heap. A helper that keeps state of its own (a static variable: a singleton's cache, a list
   of live objects, a foreign function loaded on first use) has no slot, since a new copy would start with its own
   empty state; a change that reaches one is refused by name. The running program's command line, its check point
   flag and its assert ring are shared with every reload library, so reloaded code reads the arguments the program
   started with. The functions the scheduler turns into waits (`Program.sleep`, `Console.read_line_into` and the
   like) keep their direct calls. A `--hot-reload` build is slower for it: every call of the standard library is
   an indirect call the C compiler cannot inline. It exists to give information while the program runs, and
   speed is measured on release builds.
3. **Enums change**: in a `--hot-reload` build every enum value's number is fixed for as long as the program runs, so
   nothing held has to be re-mapped. The build numbers each enum's values in order and writes the numbers into its C
   (`Mood_calm = 0`); a reload gives every value the running program knows its number again and a new value the
   next free one, so a value moved up the list or added before the others keeps what every object, list, local
   and key already holds. A removed value keeps its number and its name too: something may still hold it, and it
   still prints and compares as itself. A `switch` over an enum with no `_:` halts when it meets a value outside its
   cases (in a `--hot-reload` build, only a removed one can) with `spite: a switch over LiveEnum.Mood met the
   value 'angry', which a reload removed from it: restart the program, or put the value back`, rather than
   running no case. An enum whose values changed compiles the whole program, since the code naming its
   values and the REPL's tables are compiled again; an attribute of an enum type keeps its value, since a class's
   layout names an enum by its name, not its values. The number an enum value compiles to is a representation
   detail ([values_and_types.md](../docs/values_and_types.md)), so nothing else observes this.
4. **`environment.spite` and `build.spite`**: in a `--hot-reload` build `Environment` and `Build` move to new
   attributes like a class of the program, and a `Build` field that the program or a package declares is read by
   the program's own code from the `Build` singleton while it runs, instead of being folded into it (the standard
   library's own reads of the compiler's options stay folded). A change to either file, or a deleted file,
   compiles the whole program. When a reload swaps in `Environment`'s reading of its settings or `Build`'s values,
   the running singleton reads them again: `Build` takes the values the build gives, and every setting of
   `Environment` is read as when the program starts, from the command line it was started with, then the
   environment, then the declared default, so a setting added while the program runs finds a `--volume=7` given
   at start. A new setting's answer says so: `volume is new and is read like every setting, from the command
   line, the environment or its default`.

All of it lives only in a `--hot-reload` build: a normal build carries none of it.

A REPL that inspects and drives the *running* program, local (`--repl`) and remote (`--repl-port`), and live
reload (`--hot-reload`). The REPL is Spite, `library/read_evaluate_print_loop.spite`, walking the program through
reflection; nothing of it is in a build that did not ask for it. It answers paths, singletons by name, assignment of
a literal, calls with literal arguments and paths through what they answer, `attributes`, `functions`, `classes`,
`describe`, `enums`, `memory`, `help`, `exit`, `reload`, `last_reload` and `wait_reload`, assignment into elements,
`T?`s and of whole instances, and walking into a union. `--repl-port` runs it over `spite connect` and `Socket`,
answered where the program waits and at each loop's check point. Live reload runs on Windows. Code typed at the
prompt is compiled and run by `eval` and `run`.

### Reflection in a REPL build

The emitted `main` hands the loop the entry instance as a `Spite.Attribute` named `program` before every command. In
a `--repl` or `--repl-port` build a `Spite.Attribute` stays linked to the live value it describes, through four
functions the compiler supplies: `value_attributes()` and `value_functions()` read the value's own attributes and
functions (a list's attributes are its elements, named `0`, `1`, ...; a dictionary's are its entries, named by their
keys), `assign(text)` writes a number, `Boolean`, text or enum value into it (through the class's
`set_<attribute>(value)` when it declares one) and answers whether it could, and a `Spite.Function`'s
`call_with_text(arguments)` calls it with literal arguments and answers its result as a `Spite.Attribute?`, `null`
when an argument is not one the loop can write. Outside those builds they answer an empty list, `false` and `null`,
and the reflection they walk is not emitted. A function is callable from the prompt when every parameter is a
number, `Boolean`, text or an enum; an instantiated Symbol-codegen function (`set_age`) is one like any other, and a
template nothing calls is still never instantiated, so tree shaking of templates is unaffected. A REPL build is
inspectable ([compiler.md](compiler.md#inspectable-and-production-builds)): nothing else is tree-shaken either.

### Command language

A small subset of Spite expressions, rooted at the entry instance under the fixed name `program` (regardless of
the entry class's own Spite name):

- **paths**: `program`, `program.monsters`, `program.monsters[0].health`, `program.settings.volume`,
  `program.target`. A path may leave out the leading `program.`, so `player.health` and `program.player.health`
  name the same value. Walking through a non-null `T?` is transparent; a dictionary's entries are walked by key.
- **calls**: `program.monsters.count()`, `program.monsters[0].roar()`, `program.player.set_age(3)` (any
  callable function, including an instantiated Symbol-codegen one); `List<T>`'s and `Dictionary<Key, Value>`'s `count()`,
  and `Dictionary<Key, Value>`'s `keys()` (the keys, one a line, or `scores holds no keys`) and `has(key)`, each of which
  must end the expression. A call that answers a value is walked on like any path,
  `World().position_of(1).across`, and it runs once; a call that answers `Nothing` ends the expression, and a
  path after one is refused after it ran: `World().spawn() ran, and it answers nothing, so nothing can follow it`.
- **singletons by name**. A call whose name starts with a capital letter, alone or after a namespace (`World()`,
  `Ui.Panel()`, `Column<Position>()`), names a singleton class and binds the one instance the program holds, as the
  program's own `var world = World()` does; a path, call or assignment continues from it
  (`Tick().step_milliseconds = 50`). It never makes an instance: a singleton the program has not asked for yet is
  refused, `Tick has not been made yet: nothing in the program has asked for it, and the prompt never makes one`, and
  so is a class that is not a singleton (`'Position' is a class, not a singleton: the prompt never makes an object,
  it reads the ones the program holds, and 'classes' lists the singletons`) and a name no singleton has (`no
  singleton 'Wrld' in the program: 'classes' lists them`). A singleton made with arguments is named with them,
  literals as the program writes them, apart with commas: `Channel(1)` binds the instance the program's
  `Channel(1)` made, and an argument list the program never asks for has none (`the program holds no Channel(3): a
  singleton made with arguments has one instance for each argument list the program asks for, Channel(1),
  Channel(2)`), nor does the name without arguments; arguments given to one made without them are refused (`'World'
  is a singleton made without arguments, so it takes none: World()`). `classes` lists each such instance by its
  arguments (`Channel(1)`). The whole name is the class's qualified name with its generic arguments; the name without its namespace is
  accepted when one singleton has it, and a name two singletons share is refused naming both. A generic class that is
  not a singleton, such as `Lookup<Position>()`, is refused like any class.
- **How the loop finds them.** In a REPL build the compiler writes two functions of `ReadEvaluatePrintLoop`:
  `singletons()`, a `Spite.Attribute` for each singleton the program binds, named by its qualified name and linked
  to the instance when it has been made (read from the singleton's static slot with an acquiring load, never through
  its constructor), without a value when it has not; and `class_names()`, the program's own classes that are not
  singletons. Of the standard library's singletons only `Environment` and `Build` are listed: the rest (`Console`,
  `Scheduler`, `TypedMemory<T>`, ...) are the language's own machinery, and describing them would drag most of the
  library's classes into the build's reflection, which a reload that compiles only the changed classes would then
  have to reproduce. Any other build writes them as an empty list and empty text, and nothing calls them, so they
  are shaken out with the rest of the loop: a normal build carries none of it.
- **Breakpoints.** `break <file>:<line>`, `breaks`, `clear` and `clear <file>:<line>` answer in a program built with
  both `--hot-reload` and `--repl-port`, and everywhere else with `'break ...' answers in a program built with
  --hot-reload and --repl-port: ...`. The file is a path of the program ending in what is written (`ticker.spite`,
  `game/ticker.spite`); none or two such files are refused naming them, and a line no statement starts on is
  `no statement starts on 'ticker.spite:3': a breakpoint stops before a statement, so name the line a statement
  starts on`, the program keeping all of its code. How it is built: the running program writes the breakpoints
  beside its executable (`<program>.reload_breaks`) and runs a reload; the compiler adds every file whose
  breakpoints differ from the ones the running code has (kept in the reload state, `breakpoint <line> <path>`) to
  the files it compiles, and before the statement starting on each line writes a call of `spite_breakpoint(site,
  locals)` with `self` and every local in scope reflected as `Spite.Attribute`s; the executable's `spite_breakpoint`
  hands them to `ReadEvaluatePrintLoop.pause_at`. So a build carries nothing for breakpoints until one is set, and
  only the functions of the files holding one change. `pause_at` publishes the site and the locals and waits until
  `continue`; on the main thread it answers the prompt itself while it waits, and on another thread the main
  thread answers as usual (one thread stops at a time; another that reaches a breakpoint waits its turn).
  `where`, `locals` and `continue` answer `the program is not stopped at a breakpoint: 'break hero.spite:12' sets
  one` when it is not. Only the program's own classes can hold a breakpoint, since only their code is swapped.
- **`bytes`: native memory.** `bytes <path>` answers one JSON object: for a value the path holds an object of (an
  instance, a list, a dictionary, a singleton), `class`, `address` (hexadecimal text), `bytes` (the object's size),
  `hex` (its bytes, at most 4096 shown) or `"unreadable":true`, then for an instance `fields`, each with `name`,
  `class`, `offset` from the object's start, `bytes` and `value` (printed as the loop prints a nested value), and
  for a list `buffer` with the item buffer's `address`, `bytes`, `shown` and `hex`; for a number, `Boolean`, enum or
  text held by an attribute or an element, `class`, `address`, `bytes`, `hex` and `value` of its own slot. A value
  reached only through what a call answers, with no slot of its own, is refused naming what to ask instead.
  `bytes 0x1f2a40 64` reads a range: an address in hexadecimal and 1 to 4096 bytes, anything else refused with the
  form. **A read never crashes the inspected program**: the bytes are copied by `Memory.Inspector.hex_at`, which asks
  the operating system (`ReadProcessMemory` on the program's own process on Windows, a write into a pipe on Linux
  and macOS, which fails with `EFAULT` rather than faulting) and a range it cannot copy whole answers
  `unreadable: 0x10 for 8 bytes`. Reads only.
- **What `bytes` is built from.** Inspection is library Spite, and the REPL is one caller of it. Three members of
  `Spite.Attribute`, supplied by the compiler: `held_memory()`, the object a reflected value links to (its address
  and `sizeof` its C struct); `stored_memory()`, the slot an attribute or element is stored in, from which an
  attribute's offset is its address less its owner's; and `buffer_memory()`, a list's item buffer. Each answers
  `null` where there is none, and they answer only for a `Spite.Attribute` linked to a live value, which is a REPL
  build's (outside one every `Spite.Attribute` answers `null`). The copying is `Memory.Inspector`
  (`library/memory/inspector.spite`, with each system's `copy_readable` in its folder), a singleton any program can
  call on `value.memory`'s address. A program that never inspects carries none of it; the attribute's five hidden
  numbers are set only where a REPL build links it.
- **Generic functions beyond the instances the program holds.** The prompt calls what the build compiled: a generic
  class's functions on an instance some path reaches, or on a generic singleton (`Column<Position>().values[0]`). An
  expression the build never compiled, `Lookup<Position>().of(12)`, which makes a `Lookup`, needs code the program
  lacks, which `eval` supplies. In a `--hot-reload` build the reload machinery already compiles the whole program and
  writes a library of what changed; `eval` hands it the typed expression as the body of a function of a class of its
  own, compiles that library, loads it and calls the function once, with the loop's usual reflection for the answer.
  The program must allow the construction it asks for, so that path is refused for a singleton's constructor exactly
  as above.
- **assignment** of a number, `Boolean`, text or enum literal: `program.player.age = 5`,
  `program.monsters[1].name = "rat"`, `program.player.job = 'knight'`, through `set_<attribute>` when the class
  declares one, the same rule ordinary compiled Spite code follows. It answers the value read back afterwards,
  so a `set_<attribute>` that refused it shows the old one. A text literal, as an assigned value, a call's argument
  or a dictionary's key, is read as a program reads one, with the same escapes (`\n`, `\t`, `\r`, `\\`, `\"`, `\{`,
  and any other character after a backslash is that character); a hole is refused, since the prompt reads literals
  (`"the {hour} show" has a hole, and the prompt reads a text literal as it is written: \{ writes a brace, and
  'eval' makes text from values`).
- `attributes` (the entry instance's attributes, `name: Class = value` a line), `functions` (its functions,
  `name(argument: Class, ...): Returns` a line), both also of any path (`functions World()`, `attributes
  Tick()`); an empty list is never an empty answer: `program is a ReplWorld, which has no function the
  prompt can call: 'classes' lists the singletons, whose functions the prompt can call, such as
  World().entity_count()`, and `... which has no attributes`. A function whose name starts with `_` is private to
  its class and `drop()` runs only when a value is released, so the prompt neither lists nor calls either:
  `'_swap' is private to values, and the prompt calls what code outside the class may call`. `classes` answers the
  singletons the prompt can bind, `World()` a line with `not made yet` after one the program has not made, then the
  program's other classes a line each. Then `help`, `exit`, and `reload`, `last_reload` and `wait_reload`
  ([Live reload in detail](#live-reload-in-detail)).
- Printing a class instance shows `ClassName { attribute: value, ... }` one level deep: a nested class attribute
  prints as `ClassName {...}`; a list or dictionary prints as `List<T>(count)`/`Dictionary<Key, Value>(count)` regardless
  of depth; a `T?` prints `null` or its held value at the *same* depth; a union prints its active member through
  its `to_string()` (`Circle { radius: 2 }`) at any depth; text prints without quotes (`name: hero`); a call that
  returns `Nothing` prints nothing.
- Every error is one line and never a crash: an unknown attribute names the ones that do exist (`no attribute
  'monstrs' in program: console, player_name, player_age, monsters`), an index out of range says so (`index 5 is
  out of range: names holds 2`), a literal of the wrong kind is a type mismatch (`type mismatch: target.health is
  an Integer, and "x" is not one`), calling an attribute is `target.name is not callable`, and an unknown function
  is `no function 'nope' in program: 'functions' lists them`.
- **`eval` and `run`** answer in a program built with both `--hot-reload` and `--repl-port`, and everywhere else with
  `'eval ...' answers in a program built with --hot-reload and --repl-port: ...`. The running program writes the
  function beside its executable (`<program>.reload_prompt`) and reloads; the compiler reads the entry file with that
  text after it, so its lines keep their numbers, and never formats it into the file on disk. The entry file's hash
  then differs from the file, so the next reload compiles it again without the function, which the answer names
  as removed. The REPL calls the function through reflection like any function the prompt calls.
- **`describe <class>`** answers the class's qualified name (`, a singleton` after one), its attributes
  (`name: Class` a line; a private `_` one is left out) and its public functions with their signatures
  (`grow(by: Integer): Integer`), or `none`. The name is the qualified name, or the name without its namespace when
  one class has it; two classes sharing it are refused naming both, and a name no class has is `no class 'Nope' in
  the program: 'classes' lists them`. It covers the classes `classes` lists (the program's and every loaded
  package's, not the standard library's) from `class_descriptions()`, text the compiler writes into a REPL build
  like `class_names()`, so describing a class runs nothing.
- **`enums`** answers each of the program's enums, `Owner.Name: 'value', ...` a line, from `enum_names()`, which the
  compiler writes into a REPL build like `class_names()`; a program with none answers `the program declares no enum`.
- **`memory`** answers `<n> live allocations holding <b> bytes`, from `Program().live_allocations()` and
  `live_bytes()` ([memory.md](../docs/memory.md)), which any program can call.
- **Walking into a union**: a union value is walked as its active member, `shape.radius`, and prints with the
  member's class name. The reflected attribute keeps the union as its `.class`; `held_class()` answers the class of
  the object it holds when that differs (a union's active member), and `null` otherwise.
- **Assignment**: `null` goes into a `T?` (`nickname = null`) and is refused elsewhere (`mood is a Mood, which is
  never null: only a T? holds null`); a literal goes into a list or dictionary element through the element's own
  slot; and a right side that is not a literal is a path, whose instance an attribute or element of a class (or of
  its `T?`) then holds, through `set_<attribute>` when the class declares one, and only when the classes match:
  `follower is a Circle?, and spare is a Square: the loop assigns an instance of the attribute's own class`. A right
  side that is one construction of a class `classes` lists that is not a singleton (`follower = Circle(3)`,
  `Ui.Panel(2)`, `Lookup<Position>()`) makes its object: in a program built with `--hot-reload` and `--repl-port`
  it is compiled as `eval`'s code is, into `prompt_made_<n>(): <Class>` of the entry class, which the REPL calls
  once and assigns as a path's instance; code that does not compile answers the compiler's error; elsewhere it is
  refused (`'follower = Circle(3)' makes an object, which answers in a program built with --hot-reload and
  --repl-port: ...`). The
  compiler-supplied members are `Spite.Attribute`'s `assign_null()` and `assign_attribute(source)`, beside
  `assign(text)`; each answers whether it could.

### `--repl`

Runs the entry constructor normally; when it returns, instead of dropping the entry instance and
exiting, reads commands from stdin with a `spite> ` prompt until `exit` or end of input, then drops and
exits normally (memory balanced).

### `--repl-port=<port>`

`repl_port` is a `Build` field ([Build settings: `Build`](programs.md#build-settings-build)), so the
port is part of the build: there is no run-time override, and changing the port means rebuilding. Before the entry
constructor runs, the program starts a background thread with a TCP server bound to `127.0.0.1:<port>` **only**.
It never listens on any other interface, and there is **no authentication**: anyone who can reach that port on
that machine can read and mutate the running program. This is a local debugging tool, not something to expose
past `127.0.0.1`.

The line protocol is designed for an AI client: each request is one line of the command language; each
response is one line of JSON, `{"ok":true,"value":"...","type":"Integer"}` on success or
`{"ok":false,"error":"..."}` on failure (values and errors are JSON-escaped strings; a multi-line value, such as
the answer to `attributes`, uses `\n` inside that one JSON string, never a real newline in the wire bytes).
Clients connect and disconnect one at a time while the program keeps running; when the entry constructor returns,
the process stays alive serving the REPL until a client sends `exit`.

The design:

- **Spite, over `Socket`.** The server is `ReadEvaluatePrintLoop.serve` in `library/read_evaluate_print_loop.spite`,
  over the `Socket` class ([System classes](standard_library.md#system-classes)), whose operating-system
  members live in `library/windows/socket.spite` (`ws2_32.dll`), `library/linux/socket.spite` (`libc.so.6`) and
  `library/mac/socket.spite` (`libSystem.dylib`), each reopening `Socket` for its system. The REPL listens with
  `listen_locally`, which binds `127.0.0.1` and nothing else, so "loopback only" holds by construction.
  `ws2_32.dll` is opened when the first `Socket` is made, so a program without a REPL never loads it.
- **The thread.** The one piece of C this adds is written by the compiler, only in a `--repl-port` build: a
  two-line thread entry, `spite_remote_loop_thread`, that calls `ReadEvaluatePrintLoop.serve` with the entry
  instance. Spite starts it and waits for it through the operating system's folder (`CreateThread` and
  `WaitForSingleObject` from `kernel32.dll`; `pthread_create` and `pthread_join` elsewhere). No `Thread` class is
  added to the library: [concurrency.md](../docs/concurrency.md) says how a program is concurrent, and this thread belongs
  to the REPL.
- **Answered where the program waits.** The socket thread only reads a command, hands it to the program's
  scheduler ([concurrency.md](../docs/concurrency.md#the-repl-answers-at-the-waits)) and waits for the answer. The
  scheduler answers it on the program's thread the next time the program waits (`program.sleep`,
  `Console.read_line()`, a `File` read or write, a `Socket` accept or read, reading an unfinished `Concurrent`'s
  value, joining a `Parallel`), so a command sees the program between two steps, never in the middle of one, and
  the program's state is never read or written by two threads at once. After the constructor returns, the program
  waits for nothing but commands. A program that never waits is answered at its loops' check points (below);
  [concurrency.md](../docs/concurrency.md#the-repl-answers-at-the-waits) shows a frame loop served between frames and a
  busy loop served between passes. The standard library's own loops have no check points, so a command waits for a
  library call to return.
- **Every loop is a check point in a REPL build.** In a `--repl-port` or `--hot-reload` build, the generator ends
  every pass of every `while` in the program's own code (not `library/` or `launcher/`) with a check point: one
  relaxed load of a C flag that `Scheduler.signal()` raises, which the REPL's thread and the file watcher call when
  they hand something over. Only when it is raised does the pass call `Scheduler.check_point()`, which lowers it
  and, on the scheduler's thread, answers a pending command (the handover flag `answer_pending` already reads) and
  runs a pending reload. On any other thread (a `Parallel`, a helper) it does nothing. A loop that never waits is
  answered between two passes. A build without those flags has no check point: its C is byte for byte what it
  would otherwise be. `--repl` alone answers after the constructor returns, from the console, so it needs none. The
  cost where it exists is one load per pass: 400 000 000 passes took 106 ms, against 1 566 ms when each pass called
  `check_point()` ([optimizations.md](../docs/optimizations.md)).
- **The port is part of the build**, like any flag the compiler folds: `main` listens on it, on the main
  thread, before the constructor runs, so a client that connects any time after the program starts is served. A
  port another program holds stops the program before its constructor with `error: the REPL could not listen on
  127.0.0.1:<port>` and exit code 1.
- **The answer is shared with `--repl`.** `answer(command, program)` returns a `ReadEvaluatePrintLoop.Answer`
  (`succeeded`, `text`, `class_name`); the console loop prints `text`, and the socket loop writes it as JSON:
  `"type"` is the class of the value, empty for `help`, `attributes` and `functions`, and `Nothing` for a call
  that returns nothing. Every message the console loop prints as a complaint is `"ok":false` on the wire.
  JSON escapes are `\"`, `\\`, `\n`, `\r`, `\t` and `\u00XX` for any other control character.
- **`exit`** answers `{"ok":true,"value":"","type":""}`, closes the connection and calls `program.exit(0)`,
  which flushes what the program printed. It is handed over like any command, and the program ends at the next
  wait or pass of a loop it reaches after the one that answered it, so what the command before `exit` changed
  still runs (a loop told to stop prints its last line): it ends at once only when nothing would wake the wait it
  answered at (no sleep or helper thread running), or when the constructor has already returned. When the constructor returns first, `main` serves commands until then. With `--repl` too, the console
  loop runs first, and after its `exit` the process keeps serving.
- `spite program --repl-port=4000` is the form, as for every `Build` field; a port outside 1 to 65535 is
  `error: --repl-port takes a port number from 1 to 65535: spite program --repl-port=4000`.

### `spite connect <port>`

A tiny client built into the compiler binary itself, over the same `Socket` class: with no `--command`, an
interactive prompt that sends each typed line and pretty-prints the JSON response (`value (type)`, just `value`
when the type is empty or `Nothing`, or `error: message`); with `--command="..."`, sends that one command, prints
the raw JSON response line, and exits, which is the form tests and AI clients use. Nothing listening on the port
is `error: nothing is listening on 127.0.0.1:<port>` and exit code 1.

### Live reload in detail

A change is seen through the operating system and only what changed is rebuilt, and `--hot-reload` is a flag of its
own. [Live reload](../docs/repl.md#live-reload---hot-reload) teaches it; this section holds its rules.

- **The flag.** `hot_reload` is a `Build` field (default `false`). It **implies** `--development` rather than
  requiring it, since a new version of a class may call a function nothing called before. It works without a REPL:
  the watcher still swaps, and each swap or refusal is one line on the program's error output (`spite: rebuilt
  Monster`); an explicit rebuild needs `--repl` or `--repl-port`, whose `reload` swaps in what changed and answers
  what it rebuilt, whose `last_reload` answers what the last swap (the watcher's included) did, and whose
  `wait_reload` answers the same once the watcher is done (below). A build without
  it has no slots, no watcher and no reload code: `reload` answers `'reload' answers in a program built with
  --hot-reload, which swaps its code while it runs, and this one was not`, from a branch folded on the `Build`
  constant. As with `--repl-port`, a `--hot-reload` build swaps where the program waits and at each loop's check
  point. `spite program --hot-reload` runs the program attached to the terminal, like a REPL build.
- **A `--hot-reload` build compiles like the default build.** Its executable and every reload library are compiled
  at `-O0`, so the first build and every reload are as quick as a default build's; given `--optimized` too, both
  are compiled at `-O3`. It is one C file, so it has no link-time optimisation, and it keeps every
  run-time check of an inspectable build. Every call of a function a reload can
  replace still goes through its slot: the slot is read with an acquiring atomic load, which the C compiler may
  neither fold to the function the build started with nor hoist out of a loop, and a reload writes it with a
  sequentially consistent store.
- **`wait_reload` waits for the watcher.** It answers once the watcher is not compiling, and every file the
  running code was compiled from has the size and modification time it had when the last reload (the watcher's or
  `reload`'s) started compiling; then it swaps in what that compile made, if nothing has yet, and answers what
  `last_reload` would. Over `--repl-port` the waiting is done by the connection's thread, so the program keeps
  running meanwhile. A file new since the last compile is seen only through the watcher's event. It never hangs:
  after ten minutes it answers `the watcher has not swapped in the saved files after 10 minutes: 'reload'
  compiles them now`.
- **A reload waits for the waits of what it swaps.** A program function that waits inside a `Concurrent` is a state
  machine in a `--hot-reload` build too, so a `Concurrent` overlaps as in any other build. A reload that would swap
  a function while a `Concurrent` is stopped at a wait inside it (the function's own wait, or a call below it that
  has not returned) is held until no `Concurrent` is stopped inside any function it swaps; the program runs on
  meanwhile, and the reload is swapped in at the first check point after that. So a wait always finishes on the
  code that started it, and the code after it is the new code the next time the function is called. A reload held
  for more than 10 seconds says once which function it waits for: `spite: the reload waits for
  'Server.accept_next', where a Concurrent is stopped at a wait, and is swapped in once it returns`. `reload` and
  `wait_reload` answer once it is swapped in, as for any reload. Each such function's first frame (its machine's
  entry, `<name>___begin`) goes through a slot like every function, and each frame counts itself in a table kept
  by name while it lives, only in a `--hot-reload` build.
- **The swap mechanism: one slot per function.** In a `--hot-reload` build every function (the program's own,
  the standard library's, and every helper the compiler writes that keeps no static state of its own) and each
  program class's `_init` (its attribute defaults) is written as `<name>_hot`, with a function pointer
  `<name>_slot` holding it, and `<name>` becomes a one-line forwarder through the slot. Every call site, function
  value and REPL thunk still names `<name>`, so all of them follow a swap. The functions the scheduler turns into
  waits keep their direct calls. Any other build is unchanged: direct calls and tree shaking. The cost is one
  indirect call that the C compiler cannot inline, about a nanosecond: 200 million calls to a one-line function
  took about 0.62 s instead of 0.44 s at `-O0` and 0.40 s instead of 0.18 s with `--optimized` (Windows, clang).
- **What the build records.** Beside the executable, `<program>.reload_host` lists every function the executable
  defines with its C prototype and a hash of its C, its slots and the class each belongs to, its class ids in
  order with each class's identity (its C name), the C layout of every class and enum, every dictionary the build
  keyed by whole numbers (`key Long Columns@engine/columns.spite:11:43`), and what a reload needs to compile
  one class against the rest: the generic instances, object-literal classes and
  lent variants it made, in order; the functions each program class declares; the classes that fit each shape; the
  functions that can wait; a digest of each function's call effects and of the other whole-program studies; and
  the facts compiling each class produced, each with the classes that produced it (`fact <classes>\t<fact>`: an
  attribute read, a class admitted to a shape, a flag such as `threads`, a symbol, an allocator set on a class).
  `<program>.reload_files` holds a generation number, a hash of each of the program's files (every file it
  compiled from outside `library/`, not only those declaring its classes) and the build's output settings, so a
  reload compiles with the same `Build` values the running program has. The executable
  holds a table of its functions' addresses by name, the path of the compiler that built it and that compiler's
  options, so it must run from the directory it was built from.
- **Rebuilding: `spite reload <folder> <options> --executable-path=<program>`.** A reload runs that compiler with
  those options, less the outputs, and prints its errors to standard output, where the running program reads them.
  It reads the program's files and compares their hashes before compiling anything: nothing changed answers
  `unchanged` (under a second for a large game). Otherwise it **compiles only the changed classes**: it resolves
  every class, makes again the generic instances, object-literal classes, lent variants and made functions the
  manifest lists, takes the facts of every unchanged class from the manifest, and compiles the functions of the
  classes the changed files declare, and any function the running program lacks, skipping every other function the
  running program has with the same prototype. The class ids are seeded from the manifest by identity, so every
  class keeps the id its instances carry. **It compiles the whole program instead** when the changed code changes what other
  classes were compiled against: a parameter list or return type; a function the running program made for a
  changed class that is not made again; a fact whose set of classes changes, other than attribute reads and
  parameter-write answers (which decide only errors and the changed code); the digest of any function's call
  effects, the other whole-program studies, the functions that can wait, or the way a function takes its arguments;
  or the classes that fit a shape the changed code's C names. A whole compile compares the
  hash of every function's C with the manifest's, numbered names (`spite_temp_3`) and text constants compared by
  what they hold: every changed function is rebuilt with the changed ones, and a changed helper that keeps static
  state of its own, which has no slot, refuses the reload. A changed `static` function of the compiler's has no
  slot either: every function that calls it is rebuilt instead, with a copy of it. It writes no C for the whole program, only the library's, whose types and
  prototypes are shaken to what its functions use. With `SPITE_RELOAD_CHECK=<file>` set, `spite reload` treats that
  file as changed, compiles the reload both ways, and prints whether the fast one writes what the whole one does.
  **A changed file that declares none of the program's classes compiles the whole program**: a file reopening a
  class of the standard library, as `environment.spite` reopens `Environment`, `build.spite`, or a deleted file.
  The rebuilt set is the
  classes the changed files declare, plus every class whose code calls a function of the set that the running
  program has with another prototype or does not have at all (the running caller would still call the old one),
  repeated until nothing is added. It writes C only for the rebuilt classes' functions and what they reach that the
  running program lacks, reaching every other function through a pointer the library is handed when it is loaded,
  and compiles `<program>_reload_<n>.dll` (`.so`, `.dylib`). It prints `rebuilt A, B`, `removed A.f` when a function
  the running program has is gone, and the library's path.
- **Swapping at a drain point.** The running program opens the library with `LoadLibraryA`/`dlopen`, the
  calls `DynamicLibrary` opens a library with, and calls its `spite_reload_bind`, which looks up each running
  function it uses by name (the allocator included, so the library allocates and frees through the program) and then
  re-points the slots of the rebuilt functions whose prototypes are unchanged, with an atomic store. **The compile
  runs on a helper thread**: the watcher's thread compiles a change it sees,
  and the REPL's thread compiles before it hands `reload` over, one compile at a time; the finished library waits
  until the program's first wait or check point after it, where the program swaps it in, so the program never waits
  for the compiler. `reload` answers what its own compile produced (swapped in by then), and a later result
  replaces one not yet swapped in, except that `unchanged` never replaces a library. While the swap runs, the
  scheduler is paused and a wait blocks where it is, so nothing else runs until it is done. After a swap the file
  hashes are updated. A library is never unloaded: values it made, such as its text literals, may still be
  referenced.
- **What reloads.** A function body (the next call runs it; a call already running finishes the old one). A new
  function or a new class (the new code calls it). **Reflection follows the reload**: each program class's list of
  functions, `<Class>___functions`, has a slot like a function, and a rebuilt
  class's library swaps in its new list, so the REPL's `functions` lists a function a reload added and calls it at
  the prompt (a reload that adds `growl()` to `monster.spite` lets `monster.growl()` be called). The cost is one slot
  per class, only in a `--hot-reload` build. A changed parameter list or return type (a new function: the rebuilt
  class and its callers are rebuilt). A deleted function keeps its last code for whatever still holds it, a
  function value or the REPL, and the answer names it. An attribute's default value (through the `_init` slot, for
  instances made afterwards).
- **A reload keeps load order.** Every reload, fast or whole, compiles a class from all the files that declare or
  reopen it, in load order, so the declaration that is live after it is the one a fresh start would choose: the
  last loaded, except a `Build` field the program's own `build.spite` declares, which stays the program's. Editing a
  declaration a later load replaces changes nothing; deleting the replacement swaps the earlier one back in. A
  reload whose compile changes no code the program runs answers `no code the program runs changed, so nothing was
  rebuilt`.
- **What a reload refuses.** A file that does not compile is refused, with the compiler's error, and
  the program keeps all of its code, so a save caught half-written is harmless.
- **Watching.** `HotReload` (`library/hot_reload.spite`) watches through the standard library's
  `FileSystemWatcher` ([System classes](standard_library.md#system-classes)), the one watcher any program uses:
  `ReadDirectoryChangesW` with overlapped I/O from `kernel32.dll` on Windows, `inotify` and `poll` from `libc.so.6`
  on Linux, and `kqueue`/`kevent` on each folder and file from `libSystem.dylib` on macOS, with no polling. On a
  thread of its own, it calls `wait_for_changes()`, which returns once 100 ms pass without a change, then compiles
  the change and wakes the scheduler. The program's own folder is watched with every folder below it, and so is
  **every folder the program loads**: the compiler writes their list into the build, `HotReload.loaded_folders()`.
- **Windows' C runtime.** When the C compiler targets MSVC (its `-dumpmachine`), the program and its libraries are
  built against the C runtime DLL (`-fms-runtime-lib=dll`) so they share one heap and one standard output.
- **Crashes and parallel code.** A crash inside reloaded code reports the library's own assert trace. A `Parallel`
  running a function whose slot is re-pointed finishes the old code.

---

Next: [Testing](testing.md).
