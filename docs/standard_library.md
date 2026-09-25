# Standard library

The standard library is ordinary Spite in `library/`, and a program reads it the way it reads its own code:
every class a program can name -- `String`, `List`, `Int`, `File`, `Memory.Heap` -- is a file there, and
`--final-classes` prints each one as the program uses it. Its values are reference counted like every other
object ([memory.md](memory.md)). When an operation cannot succeed it says so in its type rather than crashing:
an index or a key that is not there reads as `T?`, a file that cannot be read answers `null`, and text that does
not parse as a number reads as `0`.

`library/` holds what every operating system shares; `library/windows/`, `library/linux/` and `library/mac/`
reopen the classes each system does differently, and the launcher loads the one the program is compiled for
([foreign_libraries.md](foreign_libraries.md#each-operating-system-reopens-what-it-changes)).

## What is in it

| Class | What it is | Page |
|---|---|---|
| `String` | immutable text | [below](#string) |
| `Int`, `Long`, `Float`, `Double`, `Bool`, ... | numbers, as classes | [values_and_types.md](values_and_types.md#numbers-are-classes) |
| `List<T>`, `Dictionary<T>` | containers, and the member templates | [collections.md](collections.md) |
| `Console` | the terminal: print, read a line | [below](#console) |
| `File`, `Directory` | files and folders | [below](#read-and-write-a-file) |
| `Watcher` | the paths that changed under a file or a folder, told by the operating system | [below](#watch-files-and-folders) |
| `Process` | run another program | [below](#run-a-process) |
| `Program` | this program: exit, sleep, environment variables | [below](#program) |
| `Clock` | elapsed time for measuring, and the wall clock | [below](#clock) |
| `Instant`, `Duration`, `LocalDate`, `LocalTime`, `LocalDateTime`, `Period`, `TimeZone`, `TimeZones`, `TimeText` | exact time, the calendar, time zones as presentation, ISO 8601 text | [time.md](time.md) |
| `Environment`, `Build`, `Arguments` | settings and the command line | [programs.md](programs.md) |
| `Json<T>` | any value to JSON text and back | [json.md](json.md) |
| `Concurrent`, `Parallel`, `ThreadPool` | run a function while waiting, or on the thread pool; the handle is the value | [concurrency.md](concurrency.md) |
| `ThreadLocal<T>`, `Lock`, `ThreadSlot` | a value per thread, and a lock | [concurrency.md](concurrency.md#a-value-per-thread-and-a-lock) |
| `Socket` | TCP on `127.0.0.1`, which the remote REPL uses | [below](#socket) |
| `Memory.Address`, `Memory.Heap`, `Memory.Arena`, `TypedMemory<T>` | a place in memory, the allocators that own it, and values of any type there: the floor every other type is built on | [memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it) |
| `DynamicLibrary` | call a native library | [foreign_libraries.md](foreign_libraries.md) |
| `Spite.Class`, `Spite.Function`, ... | reflection | [reflection.md](reflection.md) |

## `String`

Immutable and reference counted. A value is placed inside written text, `"hello {name}"`, and two values join
with `+` ([values_and_types.md](values_and_types.md#string)). `==`, `!=`, `<` and `>` compare by content.

| Member | Result | Notes |
|---|---|---|
| `length()` / `is_empty()` | `Int` / `Bool` | |
| `slice(start, end)` | `String` | clamped; `""` for an empty or invalid range |
| `character_at(index)` | `String` | `""` out of range |
| `code_at(index)` | `Int` | the byte's value; `0` out of range |
| `contains(text)` / `starts_with(text)` / `ends_with(text)` | `Bool` | |
| `index_of(text)` | `Int` | `-1` when absent |
| `replace(from, to)` | `String` | every occurrence |
| `trim()` / `upper_case()` / `lower_case()` | `String` | |
| `split(separator)` | `List<String>` | an empty separator splits into single characters |
| `lines()` | `List<String>` | split on `
` |
| `to_int()`, `to_long()`, `to_double()`, ... | a number | one per number type; `0` when it does not parse |

Assigning text to a number calls the matching `to_<type>()`: `var age: Int = "42"` is `42`.

## Read and write a file

`File(path)` is a value -- a path -- and several may exist at once.

| Member | Result | Notes |
|---|---|---|
| `path` | `String` | |
| `read()` | `String?` | `null` when the file cannot be read |
| `write(text)` / `append(text)` | `Bool` | replaces the content / adds to its end |
| `exists()` / `remove()` | `Bool` | |
| `size()` | `Long?` | how many bytes it holds; `null` when it cannot be opened |
| `modified()` | `Instant?` | when it was last written ([time.md](time.md)); `null` when it does not exist |
| `read_bytes(position, count, address)` | `Long?` | reads up to `count` bytes starting `position` bytes in, into memory at `address`; answers how many it read (`0` at the end), `null` when it cannot be opened |
| `write_bytes(address, count)` | `Bool` | replaces the content with `count` bytes from memory at `address` |
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

**Independent reads overlap.** Two or more `var name = file.read()` in a row (or `socket.read_line()`), each on a
name or an attribute and none naming an earlier one, are started together and all finished before the next
statement: the compiler starts each but the last as a `Concurrent` of its own, so the program waits once for the
slowest instead of once per file. Nothing is written for it, and nothing after the reads can see a difference --
a file written by the next statement is written after every read has finished
([concurrency.md](concurrency.md#reads-in-a-row-overlap)).

## List a directory

| Member | Result | Notes |
|---|---|---|
| `path` | `String` | |
| `entries()` | `List<Directory.Entry>` | every folder and file inside it, as `Directory` and `File` values |
| `folders()` / `files()` | `List<String>` | names only, sorted |
| `exists()` / `create()` | `Bool` | |

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
| `watch(path)` | `Bool` | a file, or a folder with everything below it; `false` when there is nothing there to watch |
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

`arguments` is a `List<String>`, each shell-quoted for you. `output()` is stdout+stderr merged, valid after
`run()` -- and it includes the child process's own trailing newline, which is why the example above calls
`.trim()`.

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
`Bool`, `String`, `Symbol`, enum value, `Spite.Class` and `Spite.Namespace` answers it, and a class of yours
prints once it declares `to_string()`. Passing one that does not is a compile error naming `to_string`. Writing
the characters out is the compiler's part (`_write_output`, `_write_error` and `flush` have no body in Spite).

```gdscript title=printable_doc/ticket.spite
var code = ""
var seats = 0

func Ticket(starting_code: String, starting_seats: Int) {
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
| `live_allocations()` | how many allocations are alive, under `--debug-memory` |

The entry constructor returning normally is exit code `0`.

```gdscript title=program_basics/program_basics.spite entry
var console = Console()

func ProgramBasics() {
    var program = Program()
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
`read_line` the way it waits anywhere, so other `Concurrent` work runs meanwhile. HTTP is not built.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Standard library  **[partial]**

D1 (decided by Mortaro, 2026-09-19): every value below is reference counted, exactly like a user class (section
10) -- a `String`/`List<T>`/`Dictionary<T>` is freed the moment its last reference goes, not by a single-owner
convention. See [Memory](memory.md#memory--implemented) for the retain/release rules and the cycle caveat.

#### String  **[implemented]**

An immutable value with a length (not a bare `char*`). A literal is static and never allocates; every other
`String` owns one buffer, freed once by its owner. **Its storage is Spite** (D108; the attribute names and the
constructor proposed by Claude, unconfirmed): `library/string.spite` declares `_bytes: Long` (the address of the
characters, from `Memory`, with a 0 after the last), `_length: Long`, `_section: Spite.Memory.Section` (`'heap'`,
or `'constant'` for a literal) and `_capacity: Long`, the compiler writes the C layout from them, and `length()`,
`code_at()`, `slice()`, `sum()`, `equals()`, `less_than()`, `greater_than()`, `drop()` (which frees `_bytes`) and
the in-place append are Spite functions reading them. `String(bytes, length)` makes a `String` that owns `length`
bytes at `bytes`, which `heap.allocate` handed out with room for one more; it is what an address's `text` and
`sum` end in, and anything that fills a buffer itself can use it. The right side of `+` casts toward `String` (every numeric
type, `Bool`, and enum all format to text -- see [Numeric types](values_and_types.md#numeric-types--implemented-provisional) for
`Float`/`Double`'s shortest-round-trip printing); assigning a `String` to any numeric variable parses it,
defaulting to `0`/`0.0` on failure. `==`/`!=`/`<`/`>` compare by content, and ([Operators](functions_and_operators.md#operators--implemented)'s Operators
table) also work as `equals(other)`/`less_than(other)`/`greater_than(other)`; `+` also works as `sum(other)`.

**Building text in a loop is linear** (proposed by Claude, unconfirmed; D36's hidden optimisation, 2026-09-24).
`text = text + piece`, `text = "{text}{piece}"` and any longer join that starts with the variable it is stored
back into (`text = "{text}, {name}: {count}"`) append to that text instead of copying it: when the variable is
the only holder of its `String`, the buffer grows in place, doubling like a `List`'s, and otherwise (a literal,
or a text another name or a list also holds) it is copied once, with room to grow. So a string shared by two
names never changes under the other one, and a hundred thousand appends take 0.2 s instead of 7 s. It applies to
a local variable or parameter of type `String` when no piece mentions that variable (`text = "{text}{text}"`
copies, as before); an attribute is not appended in place, since a call among the pieces could reach it.
`conformance/stage6/text_building` pins it: 200 000 appends and the sharing cases allocate 29 times (the `Launcher` included; 32 until `Memory` stopped being an allocation and the digits of a number were placed in the frame, D108), against
600 044 when every append copied.

| Method | Result | Notes |
|---|---|---|
| `length()` | `Int` | |
| `is_empty()` | `Bool` | |
| `slice(start, end)` | `String` | clamped; `""` for an empty/invalid range |
| `character_at(index)` | `String` | `""` out of range |
| `code_at(index)` | `Int` | `0` out of range |
| `contains(text)` / `starts_with(text)` / `ends_with(text)` | `Bool` | |
| `index_of(text)` | `Int` | `-1` when absent |
| `replace(from, to)` | `String` | replaces every occurrence |
| `trim()` / `upper_case()` / `lower_case()` | `String` | |
| `split(separator)` | `List<String>` | an empty separator splits into single characters |
| `lines()` | `List<String>` | splits on `\n` |
| `to_tiny()` / `to_short()` / `to_int()` / `to_long()` | `Tiny` / `Short` / `Int` / `Long` | `0` on a value that does not parse |
| `to_byte()` / `to_unsigned_short()` / `to_unsigned_int()` / `to_unsigned_long()` | `Byte` / `UnsignedShort` / `UnsignedInt` / `UnsignedLong` | `0` on a value that does not parse |
| `to_float()` / `to_double()` | `Float` / `Double` | `0.0` on a value that does not parse |
| `sum(other)` / `equals(other)` / `less_than(other)` / `greater_than(other)` | `String` / `Bool` | the explicit call form of `+`/`==`/`<`/`>` |

#### System classes  **[implemented]**

Built-in classes, resolved and emitted the same way an ordinary `.spite`-file class is, with hand-written bodies
instead of a parsed one. Portable C underneath (`stdio`/`stdlib`, `_popen`/`popen`, and `#ifdef _WIN32` for
directory listing and process spawning).

| Class | Members |
|---|---|
| `File(path)` | `read(): String?`, `write(text): Bool`, `append(text): Bool`, `exists(): Bool`, `remove(): Bool`; bytes (names proposed by Claude, unconfirmed): `size(): Long?`, `modified(): Instant?` (the last write, from `GetFileAttributesExA` or `stat`), `read_bytes(position, count, address): Long?` (how many were read, from any position), `write_bytes(address, count): Bool`, `append_bytes(address, count): Long?` (where they start) -- `null` when the file cannot be opened; each call opens and closes the file; `_fseeki64`/`_ftelli64` on Windows so a position past 2 GB works; `write_from` joins `read_into` among the waiting calls |
| `Directory(path)` | `path: String`, `entries(): List<Directory.Entry>` (D93: every folder and file inside it, as `Directory` and `File` values whose `path` is joined to this one -- see below), `files(): List<String>` (names, sorted), `folders(): List<String>` (sorted), `exists(): Bool`, `create(): Bool` |
| `Process(command, arguments)` | `run(): Int` (exit code; `arguments` is a `List<String>`, each shell-quoted), `output(): String` (stdout+stderr merged, valid after `run()`) |
| `Program()` | `exit(code)`: exits the process immediately with `code` |
| `Clock()` | a singleton (names proposed by Claude, unconfirmed, 2026-09-24): `elapsed_nanoseconds(): Long` and `elapsed_milliseconds(): Long` from a monotonic clock with an arbitrary start, for measuring; `now(): Instant`, the wall clock as an exact instant (D127, [Time](time.md#time-one-stored-instant-zones-for-presentation--implemented-on-windows-the-shape-proposed-by-claude-unconfirmed); it replaced `unix_milliseconds(): Long`). `library/clock.spite` with each system's reading in `library/windows|linux|mac/clock.spite` (`QueryPerformanceCounter`/`GetSystemTimeAsFileTime`, `clock_gettime`) |
| `Console()` | `print(...values)`, `write(...values)`, `error(...values)`, `flush()`, `read_line(): String?` -- see below |
| `Watcher()` (D194; the name and the members proposed by Claude, unconfirmed) | `watch(path): Bool` (a file, or a folder with everything below it; `false` when nothing is there), `changes(): List<String>` (never waits: the paths changed since the last call, each once, in the order they first changed, once the watcher has seen no change for 100 ms), `wait_for_changes()` (blocks the calling thread in the operating system until `changes()` has something) -- `ReadDirectoryChangesW`, `inotify` or `kqueue` from each system's folder, no polling; a folder is reported when created, removed or renamed, not when its contents change; `HotReload` is built on it ([REPL and live reload](repl.md#repl-and-live-reload--partial)) |
| `Socket()` (proposed by Claude, unconfirmed) | `listen_locally(port): Bool`, `accept_client(): Socket?`, `connect_locally(port): Bool`, `read_line(): String?`, `write_line(text): Bool`, `close()` -- TCP on `127.0.0.1` only, which `--repl-port` and `spite connect` use ([REPL and live reload](repl.md#repl-and-live-reload--partial)) |
| `Concurrent(function)`, `Parallel(function)` (names decided, D133) | the handle stands in for what the function returned, and reading it is the wait (D134); `finished: Bool` never waits; dropping the handle waits for it -- see "Concurrency" below |
| `ThreadPool()` (proposed by Claude, unconfirmed) | the singleton the `Parallel`s run on (D135): `size(): Int` worker threads, `worker_index(): Int` (`-1` off the pool) -- see "Concurrency" below |
| `ThreadLocal<T>()`, `Lock()`, `ThreadSlot()` (proposed by Claude, unconfirmed) | one value per thread: `get(): T?`, `set(value)`; a lock: `while_locked(function)`, `lock()`, `unlock()`; the raw per-thread `Long` both are built on: `read()`, `write(value)` -- see "Concurrency" below |
| `DynamicLibrary(file_name, naming, header)` | every foreign function, constant and type of a native library -- see [Foreign libraries](foreign_libraries.md#foreign-libraries--partial). `library/dynamic_library.spite` holds its `file_name` and `handle`, its constructor and `drop()`; opening, closing and finding a symbol are the compiler's reopening (D82) |
| `Memory.Heap()`, `Memory.Arena(block_bytes)`, `Memory.Address` | the floor every other type is built on (D98, D101, D108, D151, D178): the heap's `allocate`, `resize` and `free`, and an address's reads and writes (`library/` only), `copy_to`, `compare_bytes`, `text`, ... -- see "The floor, named" below. `library/memory/heap.spite` is the `singleton` line and `library/memory/address.spite` the number's memory, `text` and `terminated_text`, in Spite; every other function is the compiler's reopening, and where an allocation lives is the compiler's choice unless an object names its allocator ([Memory](memory.md#memory--implemented), "Allocators") |
| `TypedMemory<$value_type>()` | `read_value(address, index)`, `write_value(address, index, value)`, `release_value(address, index)`, `value_bytes()`: values of any type in raw memory, reference counts kept right; what `List<T>` keeps its elements with, and what a container of your own uses (D98) |

`Console` is one of them and is a singleton (D52): `Console()` is the same instance everywhere, `Console` is an
ordinary class name rather than a reserved word, and `console.class.name` is `"Console"` like any other class.
It has `print(...values)` (each value's `to_string()`, separated by a space, with a trailing newline),
`write(...values)` (the same without the trailing newline), `error(...values)` (the same as `print` but to the
error stream), `flush()`, and `read_line(): String?` (one line from the input stream without its line break, null
only at the end of input with nothing read). The entry constructor returning normally is exit code `0`.

**Printing is `to_string()`** (D109, decided by Mortaro; implemented). `print`, `write` and `error` are Spite in
`library/console.spite`, taking `...values: List<Printable>`, where

```gdscript
type Printable {
    to_string(): String
}
```

Every number, `Bool`, `String` (whose `to_string()` answers itself), `Symbol`, enum value, `Spite.Class` (its
`.name`) and `Spite.Namespace` (its `.name_with_namespaces`) answers it, so everything that printed before prints
the same. A class of your own prints once it declares `func to_string(): String`, and passing one that does not is
the ordinary shape error, naming the function: `'Pet' does not fit type 'Printable': it has no function
'to_string'` (`diagnostics/print_without_to_string`, `conformance/stage6/printable_values`). Mortaro wrote the
member as `to_string: Spite.Function<String>`; a `type` writes a required function as `to_string(): String` today,
and which form a shape uses is open question 11. What stays the compiler's is only the floor: `_write_output(text)`,
`_write_error(text)` and `flush()` have no body in Spite, and `Prelude` supplies their C (`fwrite` and `fflush`),
as it does `Memory`'s.

As implemented (proposed by Claude, unconfirmed): printing a value costs what the call says -- the list of values
is a `List` like any variadic call's, a number goes through its box and its `to_string()`, and the text is written
with its length rather than up to its first zero byte. `conformance/stage6/text_building` and
`fused_chain_allocations` pin those allocations. A `crash` still writes its operands itself, through each value's
`to_string()`, because it reports on the way out of a program that is stopping. `flush()` no longer writes a line
break after flushing, which the special case it replaces did.

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
  (`'calm'`), a number and a `Bool` as they print, and an absent `T?` is `null`. A `Spite.Class` is its name.
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
  lists as `List<String>(...)`), because it reads a running program through `Spite.Attribute`, whose values are
  already text, and the documented sessions depend on it. Proposal: make it `to_debug()` of the value once
  reflection can hand the loop a typed value (`mortaros_missing_decisions.md`).

`conformance/stage6/debug_values`, `docs/standard_library.md`.

**A directory is navigated through its entries** (D93, decided by Mortaro, 2026-09-24).  **[implemented]**
`Directory` has a `path` exactly as `File` does, and `entries()` answers a `List<Directory.Entry>`, where
`Directory.Entry` is the union of `Directory` and `File` declared in `library/directory.spite`. Each entry's `path`
is its parent's joined with its name, so a `switch` tells the two apart and a folder is walked by calling the same
function on it again (`conformance/stage4/directory_entries`, `docs/standard_library.md`):

```gdscript
func count_files(directory: Directory): Int {
    var total = 0
    var entries = directory.entries()
    var index = 0
    while index < entries.count() {
        var entry = entries.get_at(index)
        switch entry {
            Directory: total = total + count_files(entry)
            File: total = total + 1
        }
        index = index + 1
    }
    return total
}
```

What follows is Claude's reading (proposed by Claude, unconfirmed):

- **The name is `Entry`**, namespaced as `Directory.Entry`, because it is what a directory listing calls each of
  its items and it says nothing the class does not: `DirectoryEntry` would repeat the class it already lives in,
  and `Path` would claim a text value it is not.
- **Folders come first, then files, each sorted by name**; `.` and `..` are never listed. It is the order
  `folders()` then `files()` already give, so the three agree.
- **`files()` and `folders()` stay**: they answer names rather than values, which is what the compiler's own
  discovery wants, and each is one line over the same listing. With `entries()` they are redundant, and the
  proposal is to remove them once nothing in the repository reads names alone.
- Each operating system's folder still lists a directory its own way (D80); what it supplies is renamed
  `entry_names(want_folders)`, since `entries()` is now the public listing and Spite has no overloading.
- Found on the way: a reopened class that declares a `union` again, as `--final-classes` prints `Directory` back
  out, registered the union twice. A union declared again now replaces the earlier one, as a `type` already did
  ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)'s rule: the later declaration of a name wins).

#### Pure Spite: dissolving the runtime  **[planned]**

D14 (decided by Mortaro, 2026-09-19): **the standard library being hand-written C is temporary, and the target
is pure Spite.** `src/runtime/spite_runtime.h` (722 lines) and `src/runtime/spite_repl.h` (1392 lines) exist
because the compiler needed a `String` before Spite could express one; they are a bootstrapping decision (logged
2026-09-19, "built-in classes with hand-written bodies"), not the design.

D6 already requires part of this: `Spite.Class` is "an ordinary standard library class with an ordinary
declaration", which cannot be true while the standard library is C.

What the current runtime is, and where each part goes:

| Lines | What | Becomes |
|---|---|---|
| ~400 | `String` methods, retain/release, number to text, `List<T>`/`Dictionary<T>` support | ordinary `.spite` sources -- pure algorithms over memory, expressible today |
| ~150 | `File`/`Directory`/`Process`/sleep | `DynamicLibrary` calls ([Foreign libraries](foreign_libraries.md#foreign-libraries--partial)) -- exactly what the foreign function interface is for |
| ~130 | allocator wrappers and the `--debug-memory` live-pointer table | the table is Spite; `malloc`/`free` is the floor below |
| ~50 | float formatting through `snprintf("%g")` | a shortest-round-trip implementation in Spite, or an FFI call |

**The floor is a list of intrinsics, not a file.** A `String` needs allocation, allocation needs memory from the
operating system, obtaining it needs the foreign function interface, and naming a library there needs a `String`
-- so the bottom cannot route through the standard library. It resolves the way every self-hosted language
resolves it: five to ten compiler intrinsics that the compiler emits directly (raw memory in and out --
`mmap`/`VirtualAlloc` through the FFI on native, `memory.grow` as an instruction on wasm -- plus whatever the
emitted C needs before any Spite exists). Everything above that line is Spite.

- **Purity costs no performance here**, which is what makes it worth doing: the output is still C, so a
  Spite-written `trim()` goes through the same optimiser as the hand-written one. This is not a language
  rewriting its runtime in a slower language; it is the same machine code from a better source.
- It also delivers two things already wanted: standard library classes become reopenable ([Decided by Mortaro, being implemented](open_questions.md#decided-by-mortaro-being-implemented--planned) item 7) and
  inspectable in the REPL, because they stop being special.
- Order: the FFI (milestone 11) unlocks the system-call layer, self hosting (milestone 8b) makes it natural, and
  then the standard library moves file by file. `spite_repl.h` follows the same way -- it is an interpreter over
  generated tables, which is ordinary Spite once the reflection of milestone 10 exists.

**The floor, named** (milestone 15a; proposed by Claude, unconfirmed, 2026-09-23). With `DynamicLibrary` built
([Foreign libraries](foreign_libraries.md#foreign-libraries--partial)), the floor is small enough to list. Everything the runtime does today is either one of these, or
Spite above them:

1. **The `Memory` namespace** (D150, D151, D178; built 2026-09-25). **`Memory.Address`** is a place in memory: a
   number class (`library/memory/address.spite`, eight bytes, cast to and from `Long` like any number) whose
   `read_byte`/`read_short`/`read_unsigned_short`/`read_int`/`read_unsigned_int`/`read_long`/`read_float`/
   `read_double(offset)`, the matching `write_*(offset, value)`, and `exchange_long`, `read_long_atomically` and
   `write_long_atomically` are **language primitives**: bodiless declarations each backend lowers where they are
   called, like `+` -- one load, store or atomic instruction in the C backend, written as a macro, with no call.
   **Only `library/` may call them**: a function of a class the standard library declares (reopening one, as
   `--final-classes` output does, is writing library code), and any other class gets an error pointing at the
   standard library and `TypedMemory<T>` (`diagnostics/memory_access`). `copy_to(target, bytes)` and `compare_bytes(other, bytes)` are
   written the same way for now (the C library's `memmove` and `memcmp`), and `text(length)` and
   `terminated_text()` are Spite in the same file. **`Memory.Heap`** is the default allocator, a singleton whose
   `allocate(bytes): Memory.Address`, `resize(address, bytes)`, `free(address)` and `live_allocations()` are the
   compiler's (C `malloc`, or the `--debug-memory` table's functions). The free functions of the old singleton
   `Memory` (`allocate_bytes`, `read_long(address, offset)`, `copy_bytes`, `text(address, length)`, ...) are gone,
   and every class of `library/` binds `var heap = Memory.Heap()` instead of `var memory = Memory()`. **Not built
   yet (D178):** allocation from the operating system's pages, copying and comparing as plain Spite over
   `DynamicLibrary`; the heap is still C's `malloc`. There is still no `Pointer` type. **Before D178**, `Memory`
   was one built-in singleton holding all of this as functions taking the address first (`read_long(address,
   offset)`), since D98/D101 with sections and since D108 with the compiler choosing between them:
   `allocate_stack_bytes` is removed, and an allocation is placed in the function's frame when the rule in
   [Memory](memory.md#memory--implemented) proves the address never leaves it. `TypedMemory<$value_type>` puts values of any type in
   memory, and `List<T>` is written with it, so nothing about a container is the compiler's any more except its
   syntax (`[]`, list literals) and the few C paths listed in the containers row of the decision log.
2. **Text from memory**: since D108 this is Spite, not floor. `String` declares its storage (`_bytes`, `_length`,
   `_section`, `_capacity`) and its constructor `String(bytes: Memory.Address, length: Long)`, which takes bytes the
   heap handed out and writes the 0 after them; `address.text(length)` is three lines of Spite in
   `library/memory/address.spite` that allocate, copy and construct. `take_text` and `address_of` are gone: a
   `String`'s own functions read `_bytes`. Literals stay static data the compiler writes, as symbols already are (D70), in the
   `'constant'` section.
3. **The object header**: retain, release and the class id are code the compiler emits, not functions anyone
   calls, so they are part of code generation rather than a library. For `String` that includes two decisions
   only the header can make: a `'constant'` text is never counted, and `text = text + piece` grows the text in
   place only when its count is one (`SpiteString_append` asks, then calls `String._unshared()` and
   `String._append_in_place(piece)`, which are Spite).
4. **Entry and exit**: `main` is emitted by the compiler, and so is writing text out (`Console._write_output`,
   `_write_error`; D109 moved everything else about printing into Spite), so the floor also has
   `Console.flush()`: exiting through the C runtime's `exit` would drop what the program's own `stdout` still
   buffers, which is why `Program.exit` flushes first (found when every corpus program printed nothing).
5. **The C runtime through Spite, one operating system at a time** (D71 replaced Claude's `"c"` alias; D80
   replaced the one wrapper class per platform): each operating system's folder reopens the classes it changes
   and names its real file (`ucrtbase.dll`, `libc.so.6`, `libSystem.dylib`) in their own `DynamicLibrary`
   attributes, and what `File`, `Directory` and `Process` do on every system is written once in `library/`.
6. **On wasm**, `Memory.allocate` grows linear memory with `memory.grow`, and the C runtime library is the
   JavaScript host's, which is the web shim below.

That is the whole floor: `String`, `List<T>` and `Dictionary<T>` become Spite over `Memory`; `File`, `Directory`,
`Process` and `Program` become Spite over the C runtime, reached from each operating system's folder; the `--debug-memory` live table becomes Spite
over `Memory`; and float formatting is Spite over `Memory` (`library/number_text.spite`, 2026-09-24). What stays in C is exactly what the compiler emits, never a file someone maintains.

**Built so far, additively (2026-09-23):** items 1, 2's `Memory.text` and 5 -- `Memory` with `allocate_bytes`,
`resize`, `free`, the typed reads and writes and `copy_bytes` (`conformance/stage6/memory_floor`); the `"c"` alias
built beside it was removed by D71. **Moving onto it (D73):** `File`, `Directory`, `Process`, `Program` and `Console` are Spite in `library/`, over
`Memory` and the C runtime, and their C is deleted. Since D80 there is no `CRuntime` class in between: `library/file.spite`
holds what `File` does everywhere, and `library/windows/file.spite`, `library/linux/file.spite` and
`library/mac/file.spite` reopen it with the few functions that call the system's library ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading--partial)), and the same
for `directory`, `process`, `program`, `console`, `string` (`to_double` through `strtod`) and `build`
(`target_operating_system`). `Console`'s `print`, `write`, `error` and
`flush` stay operations the compiler emits (they are variadic, and they must reach the program's own `stdout`),
while `read_line` is Spite over `fgets` on the console's own handle of the input stream. Only `library/windows/` runs
here; the Linux and macOS folders have the same functions with the same signatures, follow `dirent`'s layout on those
systems, and are held only to compiling (`check.sh` writes the compiler out with each) until `check.sh` runs there.
Found on the way: the `'windows'` naming rule is lossy -- Win32 abbreviates some names (`SetCursorPos`) and
not others (`GetFileAttributesA`, which the rule would call `GetFileAttrsA`) -- so the Windows files name kernel32's
functions with `'identity'`. Nothing has moved onto them yet: moving `String`, `File` and the rest waits on this
proposal being accepted, since that is where changing the floor later would cost a rewrite.
The `--debug-memory` live table is Spite too: `library/allocation_table.spite` (`AllocationTable`) keeps the
live allocations in an open-addressing hash set over `Memory`, each live object's class id in the slot beside
its address (forgotten on free, so the leak summary names only what is still alive, the same on every run), and
prints the report; the compiler emits the small functions `SPITE_MALLOC`, `SPITE_REALLOC` and `SPITE_FREE` call
under `--debug-memory`. What stays C, and why, is in the decision log's allocation-table row (proposed by Claude,
unconfirmed).
`List<T>` and `Dictionary<T>` are Spite as well: `library/list.spite` is a generic class over `Memory` (growth,
insertion, removal, `reverse`, `contains`, `join`, `copy`, and releasing its elements when it is dropped), and
`library/dictionary.spite` is a generic class over two lists and a hash index into them; `split` and `lines` moved into
`library/string.spite`. The syntax stays the compiler's (`[]`, list literals, `List<T>()`), mapped onto those
classes' functions. Per element type the compiler still supplies four one-line functions a generic class cannot
write, plus `deep_copy`; the list and the reasons are in the decision log's containers row (proposed by Claude,
unconfirmed). The `<member>` templates are Spite in `library/list.spite` too (D91, [Standard library
metaprogramming](collections.md#standard-library-metaprogramming--partial)).

**The web shim is the one honest exception, and its target is zero hand-written lines.** A file that runs in the
JavaScript virtual machine cannot be Spite by construction -- it is on the far side of a boundary Spite does not
own, and every language targeting the browser has one. What is achievable is that nobody writes it: the imports
come from `external js` declarations and the command-buffer drain loop is a switch over an opcode table, both of
which are data the compiler can emit. The source of truth stays Spite even though the artifact is JavaScript, so
nothing in the repository is written in a second language and nothing rots out of sync. Today that file would be
150-300 hand-written lines; generating it fully is a goal, not a day-one requirement, and it shrinks on its own
as wasm proposals land.

#### examples/calculator  **[implemented]**

A recursive-descent arithmetic interpreter (`token.spite`, `lexer.spite`, `expression.spite` + one file per union
member, `parser.spite`, `evaluator.spite`, `calculator.spite`), reading its program from `program.txt` next to it.
Supports `+ - * /`, unary `-`, parentheses, variable assignment (`x = 1 + 2`) and reference, one statement per
line. The lexer scans `source.split("")` with a `while` loop (accumulating each token's start/end index as a class
field, sliced out on a token boundary); the parser is genuinely recursive, including precedence climbing written
as tail recursion (`parse_expression_rest`/`parse_term_rest`), a style that reads well independently of `for`
being gone. Every place the parser hands a freshly parsed operand to `BinaryExpression(...)` does so inline
(never through an intermediate `var`): `left_expression`/`right_expression` are ordinary by-reference parameters
now (D1), stored directly into the union-typed field with no `Heap<T>` wrapper needed.
