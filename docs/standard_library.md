# Standard library

The standard library is ordinary Spite in `library/`, and a program reads it the way it reads its own code:
every class a program can name -- `String`, `List`, `Integer`, `File`, `Memory.Heap` -- is a file there, and
`--final-classes` prints each one as the program uses it. Its values are reference counted like every other
object ([memory.md](memory.md)). When an operation cannot succeed it says so in its type rather than crashing:
an index or a key that is not there reads as `T?`, a file that cannot be read answers `null`, and text that does
not parse as a number reads as `0`.

`library/` holds what every operating system shares; `library/windows/`, `library/linux/` and `library/mac/`
reopen the classes each system does differently, and the launcher loads the one the program is compiled for
([foreign_libraries.md](foreign_libraries.md#each-operating-system-reopens-what-it-changes)).

**A program carries only the classes it uses** ([D177](decisions.md)). The library is part of every program's
source, but a production build keeps only the C that `main` can reach: a program that never makes a `Watcher`,
a `Socket`, a `Process` or a `ThreadPool` has none of their code, and none of the operating-system functions only
they call is looked up when the program starts ([optimizations.md](optimizations.md#tree-shaking-the-generated-c)).
What a class costs when it is used is what its Spite does, and each section below says where that is more than a
call. An inspectable build (`--repl`, `--repl-port`, `--hot-reload`, `--development`) keeps everything, so the
REPL can look at any of it ([D143](decisions.md)).

## What is in it

| Class | What it is | Page |
|---|---|---|
| `String` | immutable text | [below](#string) |
| `Integer`, `Long`, `Float`, `Double`, `Boolean`, ... | numbers, as classes | [values_and_types.md](values_and_types.md#numbers-are-classes) |
| `Nothing`, `Anything` | what a function returns when it returns nothing; the empty `type` every class fits | [functions_and_operators.md](functions_and_operators.md#calling-one) |
| `List<T>`, `Dictionary<T>` | containers, and the member templates | [collections.md](collections.md) |
| `Console` | the terminal: print, read a line | [below](#console) |
| `File`, `Directory` | files and folders | [below](#read-and-write-a-file) |
| `Watcher` | the paths that changed under a file or a folder, told by the operating system | [below](#watch-files-and-folders) |
| `Process` | run another program | [below](#run-a-process) |
| `Program` | this program: exit, sleep, environment variables, its own path | [below](#program) |
| `Clock` | elapsed time for measuring, and the wall clock | [below](#clock) |
| `Instant`, `Duration`, `Date`, `Time`, `DateTime`, `Period`, `TimeZone`, `TimeZones`, `TimeText` | exact time, the calendar, time zones as presentation, ISO 8601 text | [time.md](time.md) |
| `Environment`, `Build`, `Arguments` | settings and the command line | [programs.md](programs.md) |
| `Json<T>` | any value to JSON text and back | [json.md](json.md) |
| `Concurrent`, `Parallel`, `ThreadPool` | run a function while waiting, or on the thread pool; the handle is the value | [concurrency.md](concurrency.md) |
| `ThreadLocal<T>`, `Lock`, `ThreadSlot` | a value per thread, and a lock | [concurrency.md](concurrency.md#a-value-per-thread-and-a-lock) |
| `Socket` | TCP on `127.0.0.1`, which the remote REPL uses | [below](#socket) |
| `Memory.Address`, `Memory.Heap`, `Memory.Arena`, `TypedMemory<T>` | a place in memory, the allocators that own it, and values of any type there: the floor every other type is built on | [memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it) |
| `DynamicLibrary` | call a native library | [foreign_libraries.md](foreign_libraries.md) |
| `Spite.Class`, `Spite.Function`, ... | reflection | [reflection.md](reflection.md) |
| `Spite.Memory` | where a value lives, `value.memory` | [memory.md](memory.md#where-a-value-lives-memory) |

The other files of `library/` are the machinery these are built from -- `Scheduler` (the event loop under
`Concurrent`), `HotReload`, `ReadEvaluatePrintLoop`, `AllocationTable` (`--debug-memory`'s table), `NumberText`,
`JsonReader`, the time-zone readers -- ordinary classes a program can read and the REPL can inspect, which a
program has no reason to call.

## `String`

Immutable and reference counted. A value is placed inside written text, `"hello {name}"`, and two values join
with `+` ([values_and_types.md](values_and_types.md#string)). `==`, `!=`, `<` and `>` compare by content.

No member of a `String` can fail: an index past the end answers `""` or `0`, a range is clamped to the text, a
search that finds nothing answers `-1`, and text that is not a number converts to `0`. Assigning text to a number
calls the matching `to_<type>()`, so `var age: Integer = "42"` is `42`. Every member, with what it answers at
the edges, is in [the rules below](#string--implemented).

```gdscript title=string_members/string_members.spite entry
var console = Console()

func StringMembers() {
    var line = "  ada, grace, linus  "
    var names = line.trim().split(", ")
    var first = names.first()
    var shouted = first.upper_case()
    var count: Integer = "3"
    var missing = line.index_of("barbara")
    var tail = first.slice(1, 99)
    var past_end = first.character_at(10)
    console.print(shouted, count, missing, tail, "[{past_end}]")
    var file_lines = "one\ntwo".lines()
    var line_count = file_lines.count()
    var renamed = line.replace("ada", "Ada").trim()
    console.print(line_count, renamed)
}
```
```output
ADA 3 -1 da []
2 Ada, grace, linus
```

## Read and write a file

`File(path)` is a value -- a path -- and several may exist at once.

| Member | Result | Notes |
|---|---|---|
| `path` | `String` | |
| `read()` | `String?` | `null` when the file cannot be read |
| `write(text)` / `append(text)` | `Boolean` | replaces the content / adds to its end |
| `exists()` / `remove()` | `Boolean` | |
| `size()` | `Long?` | how many bytes it holds; `null` when it cannot be opened |
| `modified()` | `Instant?` | when it was last written ([time.md](time.md)); `null` when it does not exist |
| `read_bytes(position, count, address)` | `Long?` | reads up to `count` bytes starting `position` bytes in, into memory at `address`; answers how many it read (`0` at the end), `null` when it cannot be opened |
| `write_bytes(address, count)` | `Boolean` | replaces the content with `count` bytes from memory at `address` |
| `append_bytes(address, count)` | `Long?` | adds `count` bytes to the end; answers the position they start at, `null` when they could not all be written |

```gdscript title=file_tasks/file_tasks.spite entry
var console = Console()

func FileTasks() {
    var log_file = File(".spite-cache/documentation_demo_log.txt")
    log_file.write("first line")
    log_file.append(", second line")
    var log_exists = log_file.exists()
    console.print("exists", log_exists)
    var content = log_file.read()
    crash content
    console.print("content", content)
    var removed = log_file.remove()
    console.print("removed", removed)
    log_exists = log_file.exists()
    console.print("exists after remove", log_exists)
}
```
```output
exists true
content first line, second line
removed true
exists after remove false
```

`read()` is a `String?` -- narrow it with `if content { }`, `assert content` (which returns the function's
default) or `crash content` (which halts), exactly like any other `T?` ([failure.md](failure.md)). This entry
constructor uses `crash`, because `assert` is not allowed in a constructor.

**Bytes.** A cache or an asset file is bytes, not text: `read_bytes` reads into memory from any position, which is
what seeking is for, and `append_bytes` says where a record landed, which is what an index needs. The memory is a
`Memory.Address` from `Memory.Heap`, which a program fills and reads through `TypedMemory<T>`
([memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it)). Every call opens and closes the
file, so read a large file in one call and walk it in memory rather than record by record.

```gdscript title=byte_records/byte_records.spite entry
var console = Console()
var heap = Memory.Heap()
var longs = TypedMemory<Long>()

func ByteRecords() {
    var store = File(".spite-cache/documentation_byte_records.bin")
    var record = heap.allocate(8)
    longs.write_value(record, 0, 1111)
    store.write_bytes(record, 8)
    longs.write_value(record, 0, 2222)
    var second_at = store.append_bytes(record, 8)
    crash second_at
    var read_back = heap.allocate(8)
    var got = store.read_bytes(second_at, 8, read_back)
    crash got
    var second = longs.read_value(read_back, 0)
    var size = store.size()
    crash size
    console.print("the second record starts at", second_at, "and holds", second, "of", size, "bytes")
    heap.free(record)
    heap.free(read_back)
}
```
```output
the second record starts at 8 and holds 2222 of 16 bytes
```

**Independent reads overlap.** Two or more untyped `var name = file.read()` in a row (or `socket.read_line()`),
each on a name or an attribute and none naming an earlier one, are started together and all finished before the next
statement: the compiler starts each but the last as a `Concurrent` of its own, so the program waits once for the
slowest instead of once per file. Nothing is written for it, and nothing after the reads can see a difference --
a file written by the next statement is written after every read has finished
([concurrency.md](concurrency.md#reads-in-a-row-overlap)). What it costs is what a `Concurrent` costs, a frame on
the heap for each read but the last and the event loop that finishes them, and only in a program that reads this
way ([optimizations.md](optimizations.md#concurrency-machinery-only-where-it-is-used)). A read or write of a
`File`, `console.read_line()`, a `Socket`'s `accept_client()` and `read_line()`, and `program.sleep()` are the
library's waits: inside a `Concurrent` each lets other work run. Everything else, `Process.run()` included,
blocks the thread that calls it.

## List a directory

| Member | Result | Notes |
|---|---|---|
| `path` | `String` | |
| `entries()` | `List<Directory.Entry>` | every folder and file inside it, as `Directory` and `File` values |
| `folders()` / `files()` | `List<String>` | names only, sorted |
| `exists()` / `create()` | `Boolean` | |

```gdscript title=directory_tasks/directory_tasks.spite entry
var console = Console()

func DirectoryTasks() {
    var target = Directory(".spite-cache/documentation_demo_dir")
    target.create()
    var target_exists = target.exists()
    console.print("exists", target_exists)
    var examples = Directory("examples")
    var has_hello = examples.folders().contains("hello")
    console.print("has hello", has_hello)
}
```
```output
exists true
has hello true
```

`files()` and `folders()` answer names, not paths; `entries()` answers values you can walk.

## Walk a directory tree

A `Directory` has a `path`, the way a `File` does, and `entries()` lists what is inside it as a
`List<Directory.Entry>`: each entry is a `Directory` or a `File` whose `path` is already joined to its parent's,
so a `switch` tells them apart and a folder is walked by calling the same function again.

```gdscript title=directory_walk/directory_walk.spite entry
var console = Console()

func DirectoryWalk() {
    var root = Directory(".spite-cache/documentation_walk")
    var inner = Directory("{root.path}/inner")
    root.create()
    inner.create()
    File("{root.path}/top.txt").write("top")
    File("{inner.path}/deep.txt").write("deep")
    walk(root)
}

func walk(folder: Directory) {
    var entries: List<Directory.Entry> = folder.entries()
    var index = 0
    while index < entries.count() {
        var entry = entries.get_at(index)
        switch entry {
            Directory: {
                console.print("folder", entry.path)
                walk(entry)
            }
            File: console.print("file", entry.path)
        }
        index = index + 1
    }
}
```
```output
folder .spite-cache/documentation_walk/inner
file .spite-cache/documentation_walk/inner/deep.txt
file .spite-cache/documentation_walk/top.txt
```

Folders come first, then files, each sorted by name, and `.` and `..` are never listed.

## Watch files and folders

`Watcher()` is told by the operating system which paths changed, so nothing reads the disk over and over
(D111, D194; the name is proposed and not yet confirmed, [`mortaros_missing_decisions.md`](../mortaros_missing_decisions.md)
asks):

| Member | Result | Notes |
|---|---|---|
| `watch(path)` | `Boolean` | a file, or a folder with everything below it; `false` when there is nothing there to watch |
| `changes()` | `List<String>` | never waits: the paths changed since the last call, each once, or none |
| `wait_for_changes()` | | blocks this thread until `changes()` has something to answer |

A change is reported once the watcher has seen none for 100 ms, so a burst -- a save that writes a file in pieces,
a checkout that touches a hundred -- comes back as one list, each path once, in the order they first changed.
Each path is the watched path joined with what is below it, with `/` between. A folder is reported when it is
created, removed or renamed, not when what is inside it changes: the files inside are reported instead. The
100 ms are counted from when the watcher sees the change, which is in a call, so a program that calls `changes()`
once a frame sees a save about 100 ms after it happened.

```gdscript title=watch_folder/watch_folder.spite entry
var console = Console()
var program = Program()

func WatchFolder() {
    var folder = Directory(".spite-cache/documentation_watch")
    folder.create()
    var watcher = Watcher()
    watcher.watch(folder.path)
    File("{folder.path}/level.txt").write("three goblins")
    var changed = watcher.changes()
    var tries = 0
    while changed.is_empty() and tries < 250 {
        program.sleep(20)
        changed = watcher.changes()
        tries = tries + 1
    }
    var changed_paths = changed.join(", ")
    console.print("changed:", changed_paths)
    var again = watcher.changes()
    var again_count = again.count()
    console.print("changed since:", again_count)
}
```
```output
changed: .spite-cache/documentation_watch/level.txt
changed since: 0
```

`changes()` suits a program that already runs a loop, a game once a frame, a tool between steps. A thread with
nothing else to do calls `wait_for_changes()` and then `changes()`, which is what `--hot-reload` does
([repl.md](repl.md#how-it-works)); on the program's main thread it would stop everything else, `Concurrent` work
included, until something changes.

Each system's folder asks its own kernel: `ReadDirectoryChangesW` on the folder, with its sub-folders, on Windows;
an `inotify` watch on each folder on Linux, adding one for each new folder; and a `kqueue` entry on each file and
folder on macOS, adding new files when their folder changes. A file is watched through its folder on Windows and
Linux, so a save that replaces the file is still seen. Nothing of it is in a program that never calls `Watcher()`:
not the code, and not the lookups of the system's functions ([optimizations.md](optimizations.md#tree-shaking-the-generated-c)).
Linux and macOS are held to compiling by `check.sh`; only Windows runs today.

## Run a process

```gdscript title=process_tasks/process_tasks.spite entry
var console = Console()

func ProcessTasks() {
    var listing = Process("echo", ["build finished"])
    var code = listing.run()
    console.print("exit code", code)
    var trimmed_output = listing.output().trim()
    console.print("output", trimmed_output)
}
```
```output
exit code 0
output "build finished"
```

| Member | Result | Notes |
|---|---|---|
| `Process(command, arguments)` | | `arguments` is a `List<String>`, each put in double quotes for you |
| `run()` | `Integer` | runs it through the system's shell, waits for it and answers its exit code (`-1` when it could not start) |
| `output()` | `String` | what it wrote to its standard output, valid after `run()` |
| `run_attached()` | `Integer` | runs it with the program's own terminal, so what it writes and reads is the user's, and answers its exit code |

`output()` includes the child process's own trailing newline, which is why the example above calls `.trim()`.
What the child writes to its error stream is not captured: it goes to the program's own error stream. `run()`
is not one of the library's waits: it blocks the thread that calls it, and other `Concurrent` work does not run
meanwhile ([which calls wait](#read-and-write-a-file)).

## `Console`

`Console()` is a singleton: the same instance everywhere, so every file holds `var console = Console()`, and
`Console().print(...)` is an error naming it ([classes_and_files.md](classes_and_files.md#singletons)).

| Member | Does |
|---|---|
| `print(...values)` | each value's `to_string()`, separated by a space, then a line break |
| `write(...values)` | the same without the line break |
| `error(...values)` | like `print`, to the error stream |
| `debug(...values)` | each value's `to_debug()`, separated by a space, then a line break |
| `flush()` | writes out whatever the output and error streams still hold |
| `read_line()` | one line of input without its line break, as a `String?`: `null` only at the end of the input |

`print`, `write` and `error` are ordinary functions in `library/console.spite`, taking
`...values: List<Printable>`, where `Printable` is a `type` that requires `to_string(): String`. Every number,
`Boolean`, `String`, `Symbol`, enum value, `Spite.Class` and `Spite.Namespace` answers it, and a class of yours
prints once it declares `to_string()`. Passing one that does not is a compile error naming `to_string`; a `List`
or `Dictionary` has none either, so show it with `debug` or `join` it first
([the exact errors](#system-classes--implemented)).

```gdscript title=printable_doc/ticket.spite
var code = ""
var seats = 0

func Ticket(starting_code: String, starting_seats: Integer) {
    code = starting_code
    seats = starting_seats
}

func to_string(): String {
    return "{code} for {seats}"
}
```
```gdscript title=printable_doc/printable_doc.spite entry
var console = Console()

func PrintableDoc() {
    var ticket = Ticket("A12", 3)
    var boarding: Symbol = 'boarding'
    console.print("next:", ticket, boarding, true, 2.5, ticket.class)
}
```
```output
next: A12 for 3 boarding true 2.5 Ticket
```

`debug` is for a person or an AI reading a program's state rather than for its output. It takes
`...values: List<Debuggable>` (`to_debug(): String`), and every value answers that without writing anything: a
class shows its name and every attribute, nesting into the attributes that are classes, a `List` shows `[a, b]`, a
`Dictionary` `{"key": value}`, text is quoted, a `Symbol` or enum value is written `'like_this'`, and an absent
`T?` is `null`. An object already being shown further up is written `Name {...}`, so a cycle ends. A class that
declares its own `to_debug()` is shown by it wherever it appears, and a class nothing asks to show gets none:
the automatic one is `Spite.Debug<$value_type>` in `library/spite/debug.spite`, compiled only for the classes a
program debugs.

```gdscript title=debug_doc/crew.spite
var captain = ""
var sailors = List<String>()
var ship: Ship? = null
```
```gdscript title=debug_doc/ship.spite
var name = ""
var crew: Crew? = null

func Ship(starting_name: String) {
    name = starting_name
}
```
```gdscript title=debug_doc/debug_doc.spite entry
var console = Console()

func DebugDoc() {
    var crew = Crew()
    crew.captain = "Ana"
    crew.sailors.append("Bo")
    var ship = Ship("Gull")
    crew.ship = ship
    ship.crew = crew
    console.debug(crew, 3, "done")
    ship.crew = null
}
```
```output
Crew { captain: "Ana", sailors: ["Bo"], ship: Ship { name: "Gull", crew: Crew {...} } } 3 "done"
```

```gdscript title=console_input_doc/console_input_doc.spite entry
var console = Console()

func ConsoleInputDoc() {
    console.write("name? ")
    var name = console.read_line()
    if name {
        console.print("hello", name)
    } else {
        console.print("no input")
    }
}
```
```output
name? no input
```

## `Program`

`Program()` is this running program, a singleton bound once as `var program = Program()`.

| Member | Does |
|---|---|
| `exit(code)` | flushes what was printed and ends the process with `code` |
| `sleep(milliseconds)` | waits; other `Concurrent` work runs meanwhile ([concurrency.md](concurrency.md)) |
| `environment(name)` | the process environment variable, as a `String?` |
| `executable_path()` | the path of the running executable, as the operating system gives it |
| `live_allocations()` | how many allocations are alive, under `--debug-memory` |

The entry constructor returning normally is exit code `0`.

```gdscript title=program_basics/program_basics.spite entry
var console = Console()
var program = Program()

func ProgramBasics() {
    var missing = program.environment("SPITE_DOCUMENTATION_UNSET_VARIABLE")
    if missing {
        console.print("set to", missing)
    } else {
        console.print("not set")
    }
    program.sleep(1)
    console.print("slept")
}
```
```output
not set
slept
```

## `Clock`

`Clock()` is a singleton with two readings (the names are proposed, not yet confirmed):

| Member | Does |
|---|---|
| `elapsed_nanoseconds(): Long` | a monotonic clock with an arbitrary start: subtract two readings to measure |
| `elapsed_milliseconds(): Long` | the same, in milliseconds |
| `now(): Instant` | the wall clock, as an exact [`Instant`](time.md): show it through a time zone |

Each system reads its own clock, in `library/windows/clock.spite` (`QueryPerformanceCounter`,
`GetSystemTimeAsFileTime`) and the `linux` and `mac` folders (`clock_gettime`), through `DynamicLibrary`.

```gdscript title=clock_basics/clock_basics.spite entry
var console = Console()
var clock = Clock()
var program = Program()

func ClockBasics() {
    var started = clock.elapsed_nanoseconds()
    program.sleep(5)
    var finished = clock.elapsed_nanoseconds()
    var waited = (finished - started) / 1000000
    console.print("waited at least 4 ms:", waited >= 4)
}
```
```output
waited at least 4 ms: true
```

## `Socket`

`Socket()` is a TCP connection on `127.0.0.1` and nowhere else, which is what `--repl-port` and `spite connect`
are written with: `listen_locally(port)`, `accept_client(): Socket?`, `connect_locally(port)`,
`read_line(): String?`, `write_line(text)` and `close()`. A program that uses it waits in `accept_client` and
`read_line` the way it waits anywhere, so other `Concurrent` work runs meanwhile. It is public library surface
([D126](decisions.md)), and the networking library is to grow from it: HTTP and WebSocket, and TCP and UDP for
games, none of them built yet.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Standard library  **[partial]**

D1 (decided by Mortaro, 2026-09-19): every value below is reference counted, exactly like a user class -- a
`String`/`List<T>`/`Dictionary<T>` is freed the moment its last reference goes, not by a single-owner
convention. See [Memory](memory.md#memory--implemented) for the retain/release rules and the cycle caveat.

D14 and D177: every class here is Spite in `library/`, compiled with the program, and tree-shaken like the
program's own code, so a program carries only what it reaches ([Pure Spite](#pure-spite-dissolving-the-runtime--partial)
says what is still the compiler's).

#### String  **[implemented]**

An immutable value with a length (not a bare `char*`). A literal is static and never allocates; every other
`String` owns one buffer, freed once by its owner. **Its storage is Spite** (D108; the attribute names and the
constructor proposed by Claude, unconfirmed): `library/string.spite` declares `_bytes: Memory.Address` (the
characters, from `Memory.Heap`, with a 0 after the last), `_length: Long`, `_section: Spite.Memory.Section`
(`'heap'`, or `'constant'` for a literal) and `_capacity: Long`, the compiler writes the C layout from them, and
every member, `drop()` (which frees `_bytes`) and the in-place append are Spite functions reading them
([memory.md](memory.md#where-string-and-integer-keep-their-memory)). `String(bytes, length)` makes a `String` that
owns `length` bytes at `bytes`, which `heap.allocate` handed out with room for one more; it is what an address's
`text` and `sum` end in, and anything that fills a buffer itself can use it. The right side of `+` casts toward
`String` (every numeric type, `Boolean`, and enum all format to text -- see
[Numeric types](values_and_types.md#numeric-types--implemented-provisional) for `Float`/`Double`'s
shortest-round-trip printing); assigning a `String` to any numeric variable parses it, defaulting to `0`/`0.0` on
failure. `==`/`!=`/`<`/`>` compare by content, and
([Operators](functions_and_operators.md#operators--implemented)'s table) also work as
`equals(other)`/`less_than(other)`/`greater_than(other)`; `+` also works as `sum(other)`.

**Building text in a loop is linear** (proposed by Claude, unconfirmed; D36's hidden optimisation). `text = text +
piece`, `text = "{text}{piece}"` and any longer join that starts with the variable it is stored back into append
to that text in place when nothing else holds it, and otherwise copy it once with room to grow, so a `String`
shared by two names never changes under the other one. When it applies and what it saves is
[optimizations.md](optimizations.md#appending-to-text-in-place)'s; `conformance/stage6/text_building` pins the
allocation counts.

| Method | Result | Notes |
|---|---|---|
| `length()` | `Integer` | |
| `is_empty()` | `Boolean` | |
| `slice(start, end)` | `String` | clamped; `""` for an empty/invalid range |
| `character_at(index)` | `String` | `""` out of range |
| `code_at(index)` | `Integer` | `0` out of range |
| `contains(text)` / `starts_with(text)` / `ends_with(text)` | `Boolean` | |
| `index_of(text)` | `Integer` | `-1` when absent |
| `replace(from, to)` | `String` | replaces every occurrence |
| `trim()` / `upper_case()` / `lower_case()` | `String` | |
| `split(separator)` | `List<String>` | an empty separator splits into single characters |
| `lines()` | `List<String>` | splits on `\n` |
| `to_tiny()` / `to_short()` / `to_integer()` / `to_long()` | `Tiny` / `Short` / `Integer` / `Long` | `0` on a value that does not parse |
| `to_byte()` / `to_unsigned_short()` / `to_unsigned_integer()` / `to_unsigned_long()` | `Byte` / `UnsignedShort` / `UnsignedInteger` / `UnsignedLong` | `0` on a value that does not parse |
| `to_float()` / `to_double()` | `Float` / `Double` | `0.0` on a value that does not parse |
| `sum(other)` / `equals(other)` / `less_than(other)` / `greater_than(other)` | `String` / `Boolean` | the explicit call form of `+`/`==`/`<`/`>` |
| `to_string()` / `to_debug()` | `String` | the text itself / the text quoted, with `"`, `\` and a line feed escaped ([`Console.debug`](#console)) |

#### System classes  **[implemented]**

Ordinary classes in `library/`, each written once for every system, with the few functions that call the system
in `library/windows/`, `library/linux/` and `library/mac/`, which reopen the class and reach the system's own
library through `DynamicLibrary` (D73, D80). Only Windows runs today; the Linux and macOS folders are held to
compiling by `check.sh`. Members whose names are marked proposed below were named by Claude (unconfirmed); what
each member answers is in the section of this page that teaches it.

| Class | Members |
|---|---|
| `File(path)` | `path`, `read(): String?`, `write(text): Boolean`, `append(text): Boolean`, `exists(): Boolean`, `remove(): Boolean`; bytes (names proposed): `size(): Long?`, `modified(): Instant?`, `read_bytes(position, count, address): Long?`, `write_bytes(address, count): Boolean`, `append_bytes(address, count): Long?` -- [Read and write a file](#read-and-write-a-file). A `File` is a path with no open handle: each call opens and closes the file, positions replace seeking (`_fseeki64`/`_ftelli64` on Windows, so a position past 2 GB works), and `modified()` reads `GetFileAttributesExA`'s `ftLastWriteTime` on Windows and `stat`'s modification time on Linux and macOS |
| `Directory(path)` | `path`, `entries(): List<Directory.Entry>` (D93, below), `files(): List<String>`, `folders(): List<String>`, `exists(): Boolean`, `create(): Boolean` -- [List a directory](#list-a-directory) |
| `Process(command, arguments)` | `run(): Integer`, `output(): String`, `run_attached(): Integer` (proposed) -- [Run a process](#run-a-process). `run()` reads the child's standard output through `_popen`/`popen`; `run_attached()` is the C library's `system` |
| `Program()` | a singleton (D121): `exit(code)`, `sleep(milliseconds)`, `environment(name): String?`, `executable_path(): String` (proposed), `live_allocations(): Integer` -- [Program](#program). `exit` flushes `Console` first, since the C library's `exit` would drop what the program's own standard output still buffers |
| `Clock()` | a singleton (names proposed): `elapsed_nanoseconds(): Long`, `elapsed_milliseconds(): Long`, `now(): Instant` (D127; it replaced `unix_milliseconds(): Long`) -- [Clock](#clock), [Time](time.md#time-one-stored-instant-zones-for-presentation--implemented-on-windows-the-shape-proposed-by-claude-unconfirmed) |
| `Console()` | a singleton (D52): `print(...values)`, `write(...values)`, `error(...values)`, `debug(...values)`, `flush()`, `read_line(): String?` -- [Console](#console), and below |
| `Watcher()` | D194 (the name and the members proposed; `mortaros_missing_decisions.md` asks for the final name): `watch(path): Boolean`, `changes(): List<String>`, `wait_for_changes()` -- [Watch files and folders](#watch-files-and-folders), which is the rule. `HotReload` is built on it ([REPL and live reload](repl.md#repl-and-live-reload--partial)) |
| `Socket()` | public library surface (D126; the members proposed): `listen_locally(port): Boolean`, `accept_client(): Socket?`, `connect_locally(port): Boolean`, `read_line(): String?`, `write_line(text): Boolean`, `close()` -- TCP on `127.0.0.1` only, which `--repl-port` and `spite connect` use ([REPL and live reload](repl.md#repl-and-live-reload--partial)) |
| `Concurrent(function)`, `Parallel(function)` | names decided (D133): the handle stands in for what the function returned, and reading it is the wait (D134); `finished: Boolean` never waits; dropping the handle waits for it; there is no `wait()` and no `join()` -- [concurrency.md](concurrency.md) |
| `ThreadPool()` | the singleton every `Parallel` runs on (D135, D191; members proposed): `size(): Integer` worker threads, `worker_index(): Integer` (`-1` off the pool) -- [The thread pool](concurrency.md#the-thread-pool) |
| `ThreadLocal<T>()`, `Lock()`, `ThreadSlot()` | proposed: one value per thread, `get(): T?`, `set(value)`; a lock, `while_locked(function)`, `lock()`, `unlock()`; the raw per-thread `Long` both are built on, `read()`, `write(value)` -- [A value per thread, and a lock](concurrency.md#a-value-per-thread-and-a-lock) |
| `DynamicLibrary(file_name, naming, header)` | every foreign function, constant and type of a native library -- see [Foreign libraries](foreign_libraries.md#foreign-libraries--partial). `library/dynamic_library.spite` holds its `file_name` and `handle`, its constructor and `drop()`; opening, closing and finding a symbol are the compiler's reopening (D82) |
| `Memory.Heap()`, `Memory.Arena(block_bytes)`, `Memory.Address` | the floor every other type is built on (D108, D151, D178): allocators and the place they hand out -- [Memory](memory.md#memory-is-the-floor-and-you-can-build-on-it), whose rules they are ([Memory](memory.md#memory--implemented)) |
| `TypedMemory<$value_type>()` | `read_value(address, index)`, `write_value(address, index, value)`, `release_value(address, index)`, `value_bytes()`: values of any type in raw memory, reference counts kept right; what `List<T>` keeps its elements with, and what a container of your own uses (D98) |

**`Console` is a singleton** (D52): `Console()` is the same instance everywhere, `Console` is an ordinary class
name rather than a reserved word, and `console.class.name` is `"Console"` like any other class. Calling a member
on `Console()` directly is an error: `'Console' is a singleton: bind it once beside the attributes, 'var console =
Console()', and use 'console.print'`. `read_line()` answers `null` only at the end of the input with nothing
read, and drops a `\r` before the line feed. The entry constructor returning normally is exit code `0`.

**Printing is `to_string()`** (D109, decided by Mortaro; implemented). `print`, `write` and `error` are Spite in
`library/console.spite`, taking `...values: List<Printable>`, where

```gdscript
type Printable {
    to_string(): String
}
```

Every number, `Boolean`, `String` (whose `to_string()` answers itself), `Symbol`, enum value, `Spite.Class` (its
`.name`) and `Spite.Namespace` (its `.name_with_namespaces`) answers it, so everything that printed before prints
the same. A class of your own prints once it declares `func to_string(): String`, and passing one that does not is
the ordinary shape error, naming the function: `'Pet' does not fit type 'Printable': it has no function
'to_string'`. A `List` or a `Dictionary` has no `to_string()` either (`'List<Integer>' does not fit type
'Printable'`), so it is printed with `debug`, or joined first (`diagnostics/print_without_to_string`,
`conformance/stage6/printable_values`). Mortaro wrote the member as `to_string: Spite.Function<String>`; a `type`
writes a required function as `to_string(): String` today, and which form a shape uses is
[open question 11](open_questions.md). What stays the compiler's is only the floor: `_write_output(text)`,
`_write_error(text)` and `flush()` are declared nowhere in Spite, and the compiler supplies their C (`fwrite` and
`fflush`), as it does `Memory.Heap`'s. D147 wants even these as Spite over a few named primitives; not built.

As implemented (proposed by Claude, unconfirmed): printing a value costs what the call says -- the list of values
is a `List` like any variadic call's, a number goes through its box and its `to_string()`, and the text is written
with its length rather than up to its first zero byte. `conformance/stage6/text_building` and
`fused_chain_allocations` pin those allocations. A `crash` writes its operands itself, through each value's
`to_string()`, because it reports on the way out of a program that is stopping. `flush()` writes out what the
output and error streams hold and nothing more.

**`Console.debug` and `to_debug()`** (D109, decided by Mortaro: "each class has an automatic to_debug(): String
that returns something like `Class {attribute: value, other: value}` by nesting to_debugs"; Claude chose `debug`
over `print_json`; implemented). `debug(...values: List<Debuggable>)`, where `type Debuggable { to_debug():
String }`, writes each value's `to_debug()` separated by a space, then a line break. Every value answers it:

```text
Player { name: "hero", scores: [3, 7], bag: {"gold": 2}, partner: null, mood: 'calm' }
```

What follows is Claude's reading (proposed by Claude, unconfirmed):

- **A class shows its name and its attributes**, in declaration order, as `Name { attribute: value }`, and
  `Name {}` when it has none to show. An attribute that is a class is shown by *its* `to_debug()`, so a class that
  declares its own is shown by it wherever it appears. A `List` is `[a, b]`, a `Dictionary` is `{"key": value}`,
  text is quoted with `\"`, `\\` and `\n` escaped, a `Symbol` or enum value is written the way Spite writes it
  (`'calm'`), a number and a `Boolean` as they print, and an absent `T?` is `null`. A `Spite.Class` is its name.
- **Private attributes are left out.** The walk is the plural attribute template ([Symbol codegen](metaprogramming.md#symbol-codegen--implemented)) run from
  `Spite.DebugInstance`, and a plural over another class's attributes now ranges over the ones that class lets
  others read: a `_` attribute is its own business, and reading it from outside would be the ordinary private
  error. `Json` follows the same rule.
- **A cycle ends at an object already being shown**: it is written `Name {...}`, so `first.next.next` pointing
  back at `first` shows `Node { value: 1, next: Node { value: 2, next: Node {...} } }`. Each class keeps the
  objects it is in the middle of showing, compared with `==` (identity, unless the class defines `equals`), and
  a tree of distinct objects is shown whole however deep it goes.
- **It is Spite, and it costs nothing unused.** `library/spite/debug.spite` (`Spite.Debug<$value_type>`) turns any
  value into its text, and `library/spite/debug_instance.spite` (`Spite.DebugInstance<$value_type>`) walks a class
  instance. The generator's whole part is that asking a class for a `to_debug()` it does not declare answers one
  that calls them, created only when something asks, so a program that never debugs a class compiles nothing for
  it. A `type` or union that does not require `to_debug()` still answers it, dispatched to the class the value is.
- **The REPL keeps its own display** (`Player { name: hero, ... }`: text unquoted, nested objects as `Name {...}`,
  lists as `List<String>(...)`), which it builds from `Spite.Attribute`'s `.value` through `value.to_string()`
  (D164), and the documented sessions depend on it. Proposal: show a value as its `to_debug()` there too, now
  that `.value` is the typed instance (`mortaros_missing_decisions.md`).
- **What it costs**: in a program that calls `debug`, one generated `to_debug()` per class it shows, which
  builds the text as `String`s; `to_debug()` reads fields through Symbol templates, not through `Spite.Attribute`,
  so it boxes nothing. Every attribute it shows counts as read (D118, the ruling on walks).

`conformance/stage6/debug_values`, `docs/standard_library.md`.

**A directory is navigated through its entries** (D93, decided by Mortaro, 2026-09-24).  **[implemented]**
`Directory` has a `path` exactly as `File` does, and `entries()` answers a `List<Directory.Entry>`, where
`Directory.Entry` is the union of `Directory` and `File` declared in `library/directory.spite`. Each entry's `path`
is its parent's joined with its name, so a `switch` tells the two apart and a folder is walked by calling the same
function on it again ([Walk a directory tree](#walk-a-directory-tree); `conformance/stage4/directory_entries`).

What follows is Claude's reading (proposed by Claude, unconfirmed):

- **The name is `Entry`**, namespaced as `Directory.Entry`, because it is what a directory listing calls each of
  its items and it says nothing the class does not: `DirectoryEntry` would repeat the class it already lives in,
  and `Path` would claim a text value it is not.
- **Folders come first, then files, each sorted by name**; `.` and `..` are never listed. It is the order
  `folders()` then `files()` already give, so the three agree.
- **`files()` and `folders()` stay**: they answer names rather than values, which is what the compiler's own
  discovery wants, and each is one line over the same listing. With `entries()` they are redundant, and the
  proposal is to remove them once nothing in the repository reads names alone.
- Each operating system's folder lists a directory its own way (D80), through `entry_names(want_folders)`, since
  `entries()` is the public listing and Spite has no overloading.
- `--final-classes` prints `Directory` back out with its `union Entry`, which compiles because a union declared
  again replaces the earlier one ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)).

<a id="pure-spite-dissolving-the-runtime--planned"></a>

#### Pure Spite: dissolving the runtime  **[partial]**

D14 (decided by Mortaro, 2026-09-19): **the standard library is Spite, not hand-written C, and what the compiler
supplies below it is a short list of operations, not a file someone maintains.** D6 already required it:
`Spite.Class` is "an ordinary standard library class with an ordinary declaration", which cannot be true of a
library written in C. D147 (decided by Mortaro) sharpens the target to **zero hidden code**: everything the
compiler supplies must end up as explicit Spite, and nothing may tie Spite to C; D178 names what cannot be Spite,
the reads, writes and atomics at an address, as language primitives each backend lowers.

- **Purity costs no performance**: the output is still C, so a Spite-written `trim()` goes through the same
  optimiser a hand-written one would. It is the same machine code from a better source.
- **Library classes stop being special**: they are reopenable like any class, and an inspectable build shows
  every one of them to the REPL (D143), while a production build carries only those the program reaches (D177).

**What is Spite today**, in `library/`:

| Part | How |
|---|---|
| `String`, `List<T>`, `Dictionary<T>`, number to text (`library/number_text.spite`) | over `Memory.Heap` and `TypedMemory<T>`. The syntax stays the compiler's (`[]`, list literals, `List<T>()`), mapped onto these classes' functions, and per element type the compiler still writes four one-line functions a generic class cannot, plus `deep_copy` (the decision log's containers row, proposed by Claude, unconfirmed); the member templates are Spite in `library/list.spite` ([collections.md](collections.md#standard-library-metaprogramming--partial)) |
| `File`, `Directory`, `Process`, `Program`, `Console.read_line`, `Clock`, `Socket`, `Watcher`, `ThreadPool`, `Lock`, `ThreadSlot`, the time-zone database | written once in `library/`; each operating system's folder reopens the class with the few functions that call its own library (`ucrtbase.dll`/`kernel32.dll`, `libc.so.6`, `libSystem.dylib`) through `DynamicLibrary` (D71, D80). Only `library/windows/` runs today; the Linux and macOS folders are held to compiling |
| `--debug-memory`'s live table | `AllocationTable` (`library/allocation_table.spite`), an open-addressing set of live addresses with each object's class id beside it, which prints the leak report; the compiler emits only the small functions `SPITE_MALLOC`, `SPITE_REALLOC` and `SPITE_FREE` call under `--debug-memory` (the allocation-table row of the decision log, proposed by Claude, unconfirmed) |
| the REPL, live reload, the event loop | `ReadEvaluatePrintLoop`, `HotReload` and `Scheduler`, over reflection tables the compiler generates |
| printing, `to_debug()`, `Json<T>` | `Console`, `Spite.Debug<T>`, `Spite.DebugInstance<T>` and `Json<T>` ([json.md](json.md)) |

**What is still the compiler's** -- the floor, and what is left of D147's work:

1. **`Memory.Address`'s reads, writes and atomics** are language primitives, each one machine operation the
   backend writes where it is called, like `+` (D178; [memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it)).
   This is the floor that stays.
2. **Supplied bodies**: functions declared without a body whose C the compiler writes
   (`bootstrap/source/generation/prelude.spite`) -- `Console`'s `_write_output`, `_write_error` and `flush`
   (`fwrite`, `fflush`); `Memory.Heap`'s `allocate`, `resize`, `free` and `live_allocations` (the C library's
   `malloc`, or the `--debug-memory` table) and `Memory.Address`'s `copy_to` and `compare_bytes` (`memmove`,
   `memcmp`); `TypedMemory<T>`; the number classes' bit operations; `DynamicLibrary`'s opening, closing and
   symbol lookup (D82); the entry points and frames of `Concurrent`, `ThreadPool` and `Scheduler`; `HotReload`'s
   compiler hand-off; and `Spite.Attribute`'s and `Spite.Function`'s dispatch. **Not built (D147, D178):** each
   of these as Spite -- allocation from the operating system's pages, copying and comparing through
   `DynamicLibrary` -- so that no Spite source needs C.
3. **The object header**: retain, release and the class id are code the compiler emits, not functions anyone
   calls. For `String` that includes the two decisions only the header can make: a `'constant'` text is never
   counted, and `text = text + piece` grows the text in place only when its count is one (the compiler asks, then
   calls `String._unshared()` and `String._append_in_place(piece)`, which are Spite).
4. **Entry and exit**: `main`, and text literals, which are static data in the `'constant'` section, as symbols
   are (D70).

**Not built: WebAssembly.** There, `Memory.Heap`'s `allocate` would grow linear memory with `memory.grow`, and the
C runtime library is the JavaScript host's. **The web shim is the one honest exception, and its target is zero
hand-written lines.** A file that runs in the JavaScript virtual machine cannot be Spite by construction -- it is
on the far side of a boundary Spite does not own. What is achievable is that nobody writes it: the imports come
from `external js` declarations and the command-buffer drain loop is a switch over an opcode table, both data the
compiler can emit, so the source of truth stays Spite and nothing in the repository is written in a second
language ([targets.md](targets.md)).

#### examples/calculator  **[implemented]**

A recursive-descent arithmetic interpreter (`token.spite`, `lexer.spite`, `expression.spite` + one file per union
member, `parser.spite`, `evaluator.spite`, `calculator.spite`), reading its program from `program.txt` next to it.
Supports `+ - * /`, unary `-`, parentheses, variable assignment (`x = 1 + 2`) and reference, one statement per
line. The lexer scans `source.split("")` with a `while` loop that passes each character and its index on (so it
is not a loop a member template replaces, D171), keeping the current token's start as an attribute and slicing
it out on a token boundary; the parser is recursive, including precedence climbing written as tail recursion
(`parse_expression_rest`/`parse_term_rest`). The parser makes each `BinaryExpression(...)` on a line
of its own and hands it on by name, since no constructor is an argument (D202):
`left_expression`/`right_expression` are ordinary by-reference parameters (D1), stored directly into the
union-typed attribute.
