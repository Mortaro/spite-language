# Standard library

The standard library is ordinary Spite in `library/`, and a program reads it the way it reads its own code:
every class a program can name (`String`, `List`, `Integer`, `File`, `Memory.Heap`) is a file there, and
`--final-classes` prints each one as the program uses it. Its values are reference counted like every other
object ([memory.md](memory.md)). When an operation cannot succeed it says so in its type rather than crashing:
an index or a key that is not there reads as `T?`, a file that cannot be read answers `null`, and text that does
not parse as a number reads as `0`.

`library/` holds what every operating system shares; `library/windows/`, `library/linux/` and `library/mac/`
reopen the classes each system does differently, and the launcher loads the one the program is compiled for
([foreign_libraries.md](foreign_libraries.md#each-operating-system-reopens-what-it-changes)).

**A program carries only the classes it uses.** The library is part of every program's
source, but a production build keeps only the C that `main` can reach: a program that never makes a `FileSystemWatcher`,
a `Socket`, a `Process` or a `ThreadPool` has none of their code, and none of the operating-system functions only
they call is looked up when the program starts ([optimizations.md](optimizations.md#tree-shaking-the-generated-c)).
What a class costs when it is used is what its Spite does, and each section below says where that is more than a
call. An inspectable build (`--repl`, `--repl-port`, `--hot-reload`, `--development`) keeps everything, so the
REPL can look at any of it.

## What is in it

| Class | What it is | Page |
|---|---|---|
| `String` | immutable text | [below](#string) |
| `Integer`, `Long`, `Float`, `Double`, `Boolean`, ... | numbers, as classes, with their maths (`square_root()`, `sine()`, `Float.pi`, ...) | [values_and_types.md](values_and_types.md#numbers-are-classes), [maths](values_and_types.md#maths-functions) |
| `Nothing`, `Anything` | what a function returns when it returns nothing; the empty `type` every class fits | [functions_and_operators.md](functions_and_operators.md#calling-one) |
| `Number` | the `type` every number class fits: its operators and `to_long()`, `to_double()` | [values_and_types.md](values_and_types.md#every-number-fits-number) |
| `List<T>`, `Dictionary<T>` | containers, and the member templates | [collections.md](collections.md) |
| `Console` | the terminal: print, read a line | [below](#console) |
| `File`, `Directory` | files and folders | [below](#read-and-write-a-file) |
| `FileSystemWatcher` | the paths that changed under a file or a folder, told by the operating system | [below](#watch-files-and-folders) |
| `Process` | run another program | [below](#run-a-process) |
| `Program` | this program: exit, sleep, environment variables, its own path | [below](#program) |
| `Clock` | elapsed time for measuring, and the wall clock | [below](#clock) |
| `Instant`, `Duration`, `Date`, `Time`, `DateTime`, `Period`, `TimeZone`, `TimeZones`, `TimeText` | exact time, the calendar, time zones as presentation, ISO 8601 text | [time.md](time.md) |
| `Environment`, `Build`, `Arguments` | settings and the command line | [programs.md](programs.md) |
| `Reload` | how many reloads a `--hot-reload` program has swapped in, and which classes they rebuilt | [repl.md](repl.md#knowing-what-a-reload-rebuilt) |
| `JsonWriter<T>`, `JsonReader<T>` | any value to JSON text and back | [json.md](json.md) |
| `BinaryWriter<T>`, `BinaryReader<T>` | any value to compact bytes (a `List<Byte>`) and back, for Spite programs talking to each other and for files | [json.md](json.md#write-and-read-bytes) |
| `Vector2`, `Vector3`, `Vector4`, `Matrix3`, `Matrix4`, `Quaternion`, `AxisAlignedBox`, `Plane`, `Frustum`, `Ray`, `Color`, `ColorText`, `CubicBezier`, `Easing`, `Noise` | game maths: points, directions, transforms, rotations, culling and picking, colours in the web's formats, curves, easings and noise | [game_maths.md](game_maths.md) |
| `Concurrent`, `Parallel`, `ThreadPool` | run a function while waiting, or on the thread pool; the handle is the value | [concurrency.md](concurrency.md) |
| `ThreadLocal<T>`, `Lock`, `ThreadSlot` | a value per thread, and a lock | [concurrency.md](concurrency.md#a-value-per-thread-and-a-lock) |
| `Atomic<T>` | a whole number or `Boolean` threads share without a lock | [concurrency.md](concurrency.md#a-number-every-thread-shares-atomict) |
| `Socket` | TCP over IPv4: listen, connect, lines and bytes, waiting or not | [below](#socket) |
| `UdpSocket` | UDP datagrams over IPv4 | [below](#udpsocket) |
| `HttpServer`, `HttpClient`, `HttpRequest`, `HttpResponse` | HTTP/1.1, one exchange per connection | [below](#http) |
| `Base64`, `Deflate`, `Zlib`, `Gzip` | bytes as base64 and base64url text; bytes compressed as raw DEFLATE, zlib and gzip | [below](#bytes-base64-compression-hashes-and-passwords) |
| `Sha256`, `SecureRandom`, `Argon2` | SHA-256 and HMAC-SHA256; bytes from the operating system's secure random source; password hashes as PHC strings | [below](#bytes-base64-compression-hashes-and-passwords) |
| `Memory.Address`, `Memory.Heap`, `Memory.Arena`, `TypedMemory<T>` | a place in memory, the allocators that own it, and values of any type there: the floor every other type is built on | [memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it) |
| `DynamicLibrary` | call a native library | [foreign_libraries.md](foreign_libraries.md) |
| `Spite.Class`, `Spite.Function`, ... | reflection | [reflection.md](reflection.md) |
| `Spite.Memory` | where a value lives, `value.memory` | [memory.md](memory.md#where-a-value-lives-memory) |

The other files of `library/` are the machinery these are built from: `Scheduler` (the event loop under
`Concurrent`), `HotReload`, `ReadEvaluatePrintLoop`, `AllocationTable` (`--debug-memory`'s table), `NumberText`,
`JsonCursor`, `BinaryFormat` and the time-zone readers. They are ordinary classes a program can read and the REPL
can inspect, which a program has no reason to call.

## What belongs in the standard library

Anything that no company owns belongs here: a protocol, a file format, an algorithm, a piece of maths. HTTP, TLS,
SHA-256 and HMAC, Argon2, secure random bytes, base64 and gzip are standard, and the library grows to cover them
as programs need them. A client for a branded product (a database such as MongoDB or Postgres, a vendor's API) is
never standard: it is a package, kept by whoever maintains it, and a program loads it pinned to a commit like any
other repository
([packages.md](packages.md#loading-a-repository-pinned-to-a-commit)). `library/windows/`, `library/linux/` and
`library/mac/` are not brand packages: they are how a standard class is written for one system.

The library has UDP and HTTP/1.1 ([below](#udpsocket)), SHA-256 and HMAC, Argon2id, secure random bytes, base64
and base64url, and DEFLATE, zlib and gzip ([below](#bytes-base64-compression-hashes-and-passwords)). WebSocket,
TLS and IPv6 belong to it as well.

## `String`

Immutable. Text of up to 15 bytes is kept inside the `String` value itself and allocates nothing; longer text is
one reference-counted block ([optimizations.md](optimizations.md#short-text-lives-inside-the-string)), and none of
it changes what a program means. A value is placed inside written text, `"hello {name}"`, and two values join
with `+` ([values_and_types.md](values_and_types.md#string)). `==`, `!=`, `<` and `>` compare by content.

No member of a `String` halts: an index past the end answers `""` or `0`, a range is clamped to the text, and a
search that finds nothing answers `-1`. Reading a number is the one that can find nothing to answer, so
`to_integer()` and the other `to_<type>()` readings answer a `T?`: `null` when the text is not a number that fits
the type. Assigning text to a number calls the matching one, so it is allowed only into a `T?`:
`var age: Integer? = "42"` is `42`, and `var age: Integer = "42"` is an error naming `to_integer()`. Every member,
with what it answers at the edges, is in [the rules below](#the-string-class).

```gdscript title=string_members/string_members.spite entry
var console = Console()

func StringMembers() {
    var line = "  ada, grace, linus  "
    var names = line.trim().split(", ")
    var first = names.first()
    crash first
    var shouted = first.upper_case()
    var count = "3".to_integer()
    crash count
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

`File(path)` is a value, a path, and several may exist at once.

| Member | Result | Notes |
|---|---|---|
| `path` | `String` | |
| `to_string()` | `String` | the path, so a `File` prints as its path and a crash on one names it ([failure.md](failure.md#what-a-crash-reports)) |
| `read()` | `String?` | `null` when the file cannot be read |
| `write(text)` / `append(text)` | `Boolean` | replaces the content / adds to its end |
| `exists()` / `remove()` | `Boolean` | |
| `size()` | `Long?` | how many bytes it holds; `null` when it cannot be opened |
| `modified()` | `Instant?` | when it was last written ([time.md](time.md)); `null` when it does not exist |
| `read_bytes(position, count, address)` | `Long?` | reads up to `count` bytes starting `position` bytes in, into memory at `address`; answers how many it read (`0` at the end), `null` when it cannot be opened |
| `write_bytes(address, count)` | `Boolean` | replaces the content with `count` bytes from memory at `address` |
| `append_bytes(address, count)` | `Long?` | adds `count` bytes to the end; answers the position they start at, `null` when they could not all be written |
| `map()` | `MappedFile?` | the whole file mapped into memory, read-only ([below](#a-file-larger-than-memory-map-it)); `null` when it cannot be opened |

```gdscript title=file_tasks/file_tasks.spite entry
var console = Console()

func FileTasks() {
    var log_file = File(".spite/documentation_demo_log.txt")
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

`read()` is a `String?`: narrow it with `if content { }`, `assert content` (which returns the function's
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
    var store = File(".spite/documentation_byte_records.bin")
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

### A file larger than memory: map it

`file.map()` maps the whole file into the program's memory, read-only, without reading it: the operating system
brings each page in the first time it is read and may drop it again, so a file of many gigabytes is walked as if
it were in memory. A `MappedFile` answers `size(): Long`,
and reads by position (`mapped[position]` (a `Byte`), `read_short`, `read_integer`, `read_long`, `read_float`,
`read_double` and `text(position, count)`), each a `T?` that is `null` when the bytes it would read are not all
inside the file, since a position read from a file is data from outside ([failure.md](failure.md)). The mapping
is undone when the `MappedFile` is dropped, and a file that is mapped cannot be removed on Windows until then.

```gdscript title=mapped_records/mapped_records.spite entry
var console = Console()
var heap = Memory.Heap()
var longs = TypedMemory<Long>()

func MappedRecords() {
    var store = File(".spite/documentation_mapped_records.bin")
    var record = heap.allocate(16)
    longs.write_value(record, 0, 1111)
    longs.write_value(record, 1, 2222)
    store.write_bytes(record, 16)
    heap.free(record)
    total_records(store)
    store.remove()
}

func total_records(store: File) {
    var mapped = store.map()
    crash mapped
    var total: Long = 0
    var position: Long = 0
    while position < mapped.size() {
        var value = mapped.read_long(position)
        crash value
        total = total + value
        position = position + 8
    }
    var past_end = mapped.read_long(16)
    console.print("the records add up to", total, "and a read past the end is null:", not past_end)
}
```
```output
the records add up to 3333 and a read past the end is null: true
```

To stream instead (a file read once, front to back, with no mapping), `read_bytes(position, count, address)`
already covers it: each call opens, seeks and closes the file, which measured about 50 microseconds a call on
Windows, so a loop reading a megabyte at a time spends almost nothing on it (512 MB in 169 ms, against 572 ms in
64 KB pieces, warm cache). Read a megabyte or more per call; there is no separate reader to open and close.

**Independent reads overlap.** Two or more untyped `var name = file.read()` in a row (or `socket.read_line()`),
each on a name or an attribute and none naming an earlier one, are started together and all finished before the next
statement: the compiler starts each but the last as a `Concurrent` of its own, so the program waits once for the
slowest instead of once per file. Nothing is written for it, and nothing after the reads can see a difference:
a file written by the next statement is written after every read has finished
([concurrency.md](concurrency.md#reads-in-a-row-overlap)). What it costs is what a `Concurrent` costs, a frame on
the heap for each read but the last and the event loop that finishes them, and only in a program that reads this
way ([optimizations.md](optimizations.md#concurrency-machinery-only-where-it-is-used)). A read or write of a
`File`, `console.read_line()`, a `Socket`'s `accept_client()`, `read_line()` and `read_bytes()`, and
`program.sleep()` are the library's waits: inside a `Concurrent` each lets other work run. Everything else,
`Process.run()` included, blocks the thread that calls it, except a `Socket`'s `_now` calls, which never wait.

## List a directory

| Member | Result | Notes |
|---|---|---|
| `path` | `String` | |
| `entries()` | `List<Directory.Entry>` | every folder and file inside it, as `Directory` and `File` values |
| `folders()` / `files()` | `List<String>` | names only, sorted by a merge sort (`n log n`), so a folder of thousands of files lists quickly |
| `exists()` / `create()` | `Boolean` | |

```gdscript title=directory_tasks/directory_tasks.spite entry
var console = Console()

func DirectoryTasks() {
    var target = Directory(".spite/documentation_demo_dir")
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
    var root = Directory(".spite/documentation_walk")
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
        var entry = entries[index]
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
folder .spite/documentation_walk/inner
file .spite/documentation_walk/inner/deep.txt
file .spite/documentation_walk/top.txt
```

Folders come first, then files, each sorted by name, and `.` and `..` are never listed.

## Watch files and folders

`FileSystemWatcher()` is told by the operating system which paths changed, so nothing reads the disk over and over.
Making one takes no arguments and starts nothing: no thread and no job exist until something is watched. One watcher
watches any number of files and folders, and `watch_for_changes` takes either, so a folder or a file a program
already holds is passed as it is.

| Member | Result | Notes |
|---|---|---|
| `watch_for_changes(target: Directory or File)` | `Boolean` | a `File`, or a `Directory` with everything below it; `false` when there is nothing there to watch |
| `changes()` | `List<String>` | never waits: the paths changed since the last call, each once, or none |
| `wait_for_changes()` | | blocks this thread until `changes()` has something to answer |

A change is reported once the watcher has seen none for 100 ms, so a burst (a save that writes a file in pieces,
a checkout that touches a hundred) comes back as one list, each path once, in the order they first changed.
Each path is the `path` of the watched `Directory` or `File` joined with what is below it, with `/` between. A
folder is reported when it is created, removed or renamed, not when what is inside it changes: the files inside
are reported instead. The 100 ms are counted from when the watcher sees the change, which is in a call, so a
program that calls `changes()` once a frame sees a save about 100 ms after it happened.

```gdscript title=watch_folder/watch_folder.spite entry
var console = Console()
var program = Program()

func WatchFolder() {
    var folder = Directory(".spite/documentation_watch")
    folder.create()
    var watcher = FileSystemWatcher()
    watcher.watch_for_changes(folder)
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
changed: .spite/documentation_watch/level.txt
changed since: 0
```

`changes()` suits a program that already runs a loop, a game once a frame, a tool between steps. A thread with
nothing else to do calls `wait_for_changes()` and then `changes()`, which is what `--hot-reload` does
([repl.md](repl.md#how-it-works)); on the program's main thread it would stop everything else, `Concurrent` work
included, until something changes.

Each system's folder asks its own kernel: `ReadDirectoryChangesW` on the folder, with its sub-folders, on Windows;
an `inotify` watch on each folder on Linux, adding one for each new folder; and a `kqueue` entry on each file and
folder on macOS, adding new files when their folder changes. A file is watched through its folder on Windows and
Linux, so a save that replaces the file is still seen. Nothing of it is in a program that never calls `FileSystemWatcher()`:
not the code, and not the lookups of the system's functions ([optimizations.md](optimizations.md#tree-shaking-the-generated-c)).

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
| `Process(command, arguments)` | | `arguments` is a `List<String>`, each one argument of the child, quoted for you (below) |
| `working_directory` | `String` | the folder the child runs in; `""`, the default, is the program's own |
| `environment_variables` | `Dictionary<String>` | variables set for the child alone, on top of the program's own: `process.environment_variables["LOG"] = "1"` |
| `run()` | `Integer` | runs it through the system's shell, waits for it and answers its exit code (`-1` when it could not start) |
| `output()` | `String` | what it wrote to its standard output, valid after `run()` |
| `run_attached()` | `Integer` | runs it with the program's own terminal, so what it writes and reads is the user's, and answers its exit code |

Each argument reaches the child whole, as one argument, whatever it holds. On Linux and macOS each is put in single
quotes for the shell, so the child's `argv` holds exactly the text given. On Windows a program reads its own
command line, so an argument is quoted the way Windows splits one back apart (the rules of `CommandLineToArgvW`):
an argument with no space, tab, quote or `& | < > ^` is written as it is; one with them is put in double quotes,
with a `"` inside written `\"` and the backslashes before a quote doubled; and one shaped `key=value` quotes only
its value, so `-script=a b` reaches the child's command line as `-script="a b"` (the form Unreal and other
programs that read `-key=value` themselves expect) and still splits back into `-script=a b`
(`conformance/stage4/process_settings`). A `%` is still read by the Windows shell.

`working_directory` and `environment_variables` are set by the shell line itself, before the command: a
`cd` into the folder and one assignment per variable, so a relative command or path is read from that folder, and
a folder that does not exist fails the run with the shell's exit code. `output()` is read 4 KiB at a time and each
piece appended in place, so reading 50 MB takes as long as the child takes to write it.

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

**Every line is written out as it is printed.** `print`, `error` and `debug` end a line and hand it to the
operating system at once, whether the output is a terminal, a file or a pipe, so a server's log redirected to a
file shows each line when it happens rather than when the program ends. `write` leaves its text for the next line
end or `flush()`. It costs one system call per line when the output is a file or a pipe
([the rule and its measurement](#system-classes)).

`print`, `write` and `error` are ordinary functions in `library/console.spite`, taking
`...values: List<Printable>`, where `Printable` is a `type` that requires `to_string(): String`. Every number,
`Boolean`, `String`, `Symbol`, enum value, `Spite.Class` and `Spite.Namespace` answers it, and a class of yours
prints once it declares `to_string()`. Passing one that does not is a compile error naming `to_string`; a `List`
or `Dictionary` has none either, so show it with `debug` or `join` it first
([the exact errors](#system-classes)).

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

`debug` is for reading a program's state rather than for its output. It takes
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

`Clock()` is a singleton with two readings:

| Member | Does |
|---|---|
| `elapsed_nanoseconds(): Long` | the monotonic clock, in nanoseconds from an arbitrary start: subtract two readings to measure |
| `elapsed_milliseconds(): Long` | the same, in milliseconds |
| `now(): Instant` | the wall clock, as an exact [`Instant`](time.md): show it through a time zone |

Each system reads its own clock, in `library/windows/clock.spite` (`QueryPerformanceCounter`,
`GetSystemTimeAsFileTime`) and the `linux` and `mac` folders (`clock_gettime` with `CLOCK_MONOTONIC` and
`CLOCK_REALTIME`), through `DynamicLibrary`. A reading allocates nothing: the operating system writes it into a slot
in the calling function's frame ([placement](memory.md#placement-the-compiler-decides-where-memory-lives)),
and Windows' counter frequency, fixed at boot, is read once when `Clock()` is first made. The unit is the
nanosecond on every system; the resolution is the hardware's (100 ns for Windows' usual 10 MHz counter, 1 ns on
Linux and macOS). A profiler can read it twice per system per frame and measure nothing but the reads
(`conformance/stage6/clock_reads` pins 1 000 readings at no allocation).

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

`Socket()` is a TCP connection over IPv4, the same on Windows (winsock), Linux and macOS. `--repl-port` and
`spite connect` are written with it, and so is a game server. It is public library surface.

| Member | Does |
|---|---|
| `listen_locally(port)`, `listen_everywhere(port)`, `listen_at(host, port)` | listens on `127.0.0.1`, on every interface, or on the one interface a host name or IPv4 address names; `false` when it cannot |
| `connect_locally(port)`, `connect(host, port)` | connects to `127.0.0.1`, or to a host name (`"example.com"`, `"localhost"`) or IPv4 address (`"192.168.1.20"`); `false` when the name does not resolve or nobody answers |
| `accept_client(): Socket?` | waits for the next client |
| `read_line(): String?` | waits for a whole line, without its line break |
| `read_bytes(address, count): Integer` | waits until at least one byte has arrived, puts up to `count` at `address`, and answers how many |
| `write_line(text): Boolean`, `write_bytes(address, count): Boolean` | sends all of it, waiting while the system's buffer is full |
| `accept_client_now(): Socket?` | the next client if one is already connecting, otherwise `null` at once |
| `read_line_now(): String?` | a whole line if one has arrived, otherwise `null` at once |
| `read_bytes_now(address, count): Integer` | puts what has already arrived, up to `count`, at `address` and answers how many: `0` is nothing yet |
| `write_bytes_now(address, count): Integer` | hands the system as much as it takes now and answers how many; the rest is the caller's to send later |
| `closed: Boolean` | `true` once the other end has closed the connection (or it broke, or `close()` was called) |
| `close()` | closes it |

**Waiting and not waiting.** The calls without `_now` wait the way a program waits anywhere: inside a
`Concurrent`, `accept_client`, `read_line` and `read_bytes` are points the state machine returns from, so other
work runs meanwhile ([concurrency.md](concurrency.md#what-the-compiler-does-at-a-wait)). The `_now` calls never
wait and are plain calls, for a loop that polls every connection once a tick (a game server, which has its own
frame to keep). Resolving a host name and connecting always wait in place.

**A closed peer is an answer, not a failure.** `read_line()` answers `null` when the
connection ended before a whole line; `read_line_now()` answers `null` for that and when no whole line has arrived
yet, and `read_bytes_now` answers `0` both for "nothing yet" and for "closed". `closed` is what tells them apart
(rather than a count of `-1`), so a count stays a count:

```gdscript
func poll(connection: Socket) {
    var received = connection.read_bytes_now(buffer, 4096)
    if received > 0 {
        handle(received)
    } else if connection.closed {
        forget(connection)
    }
}
```

A write to a closed connection sends nothing, answers `false` or `0`, and sets `closed` too; nothing crashes.
Addresses are `Memory.Address`es, so a buffer is `heap.allocate(bytes)` and its bytes are written with
`TypedMemory<Byte>` or a class of the program's own over the heap ([memory.md](memory.md)).
`UdpSocket` and HTTP are [below](#udpsocket); WebSocket grows from this class.

## `UdpSocket`

`UdpSocket()` sends and receives UDP datagrams over IPv4, the same on Windows, Linux and macOS. A datagram is a `List<Byte>`, and one call is one datagram.

| Member | Does |
|---|---|
| `open()`, `open_locally(port)`, `open_everywhere(port)` | opens on a port the system picks (a client), or on `port` of `127.0.0.1` or of every interface; `false` when the port is taken |
| `send_to(host, port, bytes): Boolean` | sends one datagram to a host name or IPv4 address; `false` when the name does not resolve or the system refuses it |
| `receive(): List<Byte>?` | waits for the next datagram; `null` when the socket is closed |
| `receive_now(): List<Byte>?` | the next datagram if one has arrived, otherwise `null` at once |
| `sender_host(): String`, `sender_port(): Integer` | who sent the datagram received last, to answer with `send_to` |
| `close()` | closes it |

A datagram can arrive empty, which is a list of no bytes, not `null`. UDP itself neither orders nor repeats lost
datagrams; reliable channels are the program's.

## HTTP

`HttpServer` and `HttpClient` speak HTTP/1.1 over `Socket`.
A message is an `HttpRequest` (`method`, `path`, `headers`, `body`) or an `HttpResponse` (`status`, `headers`,
`body`); `header(name)` reads a header whatever its case and `set_header(name, value)` writes one, and
`reason()` is the response's reason phrase. Bodies are text.

| Member | Does |
|---|---|
| `HttpServer.listen_locally(port)`, `listen_everywhere(port)` | listens, like `Socket` |
| `HttpServer.next_request(): HttpRequest?` | waits for the next well-formed request; a malformed one is answered `400 Bad Request` and skipped, and a body over `largest_body` (1 MiB) counts as malformed; `null` when the listener fails |
| `HttpServer.respond(request, response)` | writes the response with its `content-length` and closes the connection |
| `HttpClient.send(host, port, request): HttpResponse?` | connects, sends the request with `host` and `content-length`, and reads the response, whether its body is sized, chunked or ends when the connection closes; `null` when nobody answers or the response is malformed |

Each exchange is one connection (`connection: close`): no keep-alive, no pipelining, and a server's requests
with a chunked body are refused. Inside a `Concurrent`, `next_request` and `send` wait the way `Socket` does,
so a server serves while its program runs ([concurrency.md](concurrency.md#what-the-compiler-does-at-a-wait)).

## Bytes: base64, compression, hashes and passwords

These classes work on `List<Byte>`, and `text.to_bytes()` gives a text's bytes. Each is written in Spite in
`library/`, so a program that uses none of them carries none of them.

| Class | Member | Does |
|---|---|---|
| `Base64` | `encode(bytes): String`, `encode_url(bytes): String` | RFC 4648 base64 with `=` padding; base64url (`-` and `_`) without it, as JWTs write it |
| | `decode(text): List<Byte>?`, `decode_url(text): List<Byte>?` | the bytes back; `null` for a character outside the alphabet, a length no encoding has, missing padding in `decode`, or set bits after the last byte |
| `Deflate`, `Zlib`, `Gzip` | `compress(bytes): List<Byte>` | raw DEFLATE (RFC 1951), zlib (RFC 1950, Adler-32) or gzip (RFC 1952, CRC-32 and size) |
| | `decompress(bytes): List<Byte>?` | the bytes back, with no length given in advance; `null` for a truncated or malformed stream, a checksum or size that does not match, or bytes after the end |
| `Sha256` | `hash(bytes): List<Byte>` | the 32-byte SHA-256 digest (FIPS 180-4) |
| | `hmac(key, message): List<Byte>` | HMAC-SHA256 (RFC 2104), a key longer than 64 bytes hashed first |
| `SecureRandom` | `bytes(count): List<Byte>` | bytes from the operating system's secure source: `BCryptGenRandom` on Windows, `getrandom` on Linux, `getentropy` on macOS; a failure of the source crashes |
| `Argon2` | `hash(password): String` | a new Argon2id (RFC 9106) hash as a PHC string, `$argon2id$v=19$m=65536,t=3,p=1$<salt>$<tag>`: 64 MiB, 3 passes, 1 lane, a 16-byte random salt and a 32-byte tag |
| | `verify(password, stored): Boolean?` | whether the password is the one `stored` was made from, for `argon2id`, `argon2i` and `argon2d` PHC strings of version 19 with any cost; `null` when `stored` is not such a string, so a damaged record is not taken for a wrong password; the tags are compared in constant time |
| | `derive(password, salt, time_cost, memory_cost, lanes, tag_length): List<Byte>` | the raw Argon2id tag, `memory_cost` in KiB |

A hash takes about 150 ms in an ordinary build and a few seconds in a `--debug-memory` one, whose arithmetic is
checked; it waits in place, so a server runs it on the thread pool. The `Argon2` hash tree-shakes to Blake2b and
the fill of its memory, both private to the class.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. Where the teaching above and these rules disagree,
the rules win.

### Standard library

Every value below is reference counted, exactly like a user class: a `String`/`List<T>`/`Dictionary<T>` is freed
the moment its last reference goes, not by a single-owner convention. See [Memory](memory.md#memory) for the
retain/release rules and the cycle caveat.

**What no company owns is standard; what is built on a branded product is a package.** A protocol, a file format,
an algorithm or maths that no company owns belongs in `library/`, and the library grows to cover it when a program
needs it (HTTP, TLS, SHA-256/HMAC, Argon2, secure random bytes, base64, gzip, and the maths games need). A client
for a branded product (a database such as MongoDB or Postgres, a vendor's API or service) is never added to
`library/`: it is a package, maintained by whoever owns it, loaded pinned to a commit. An operating system's
folder is how a standard class is written for that system, not a brand package. Nothing in `library/` breaks the
rule.

Every class here is Spite in `library/`, compiled with the program, and tree-shaken like the program's own code,
so a program carries only what it reaches ([Pure Spite](#pure-spite-dissolving-the-runtime) says what is still the
compiler's).

#### The String class

An immutable value with a length (not a bare `char*`), sixteen bytes wherever it is kept. A literal is static and
never allocates; other text of up to 15 bytes is kept in those sixteen bytes and never allocates either; longer
text is one block on the heap, counted and freed with its last holder. **Its storage is Spite**:
`library/string.spite` declares `_bytes: Memory.Address` (where the characters are, with a 0 after the last) and
`_length: Long`, and nothing else (the compiler lays the sixteen bytes out and reads them through the form the
text takes), and every member but three is a Spite function reading them
([memory.md](memory.md#where-string-and-integer-keep-their-memory)). The three the compiler supplies are the ones
that depend on where the characters are: `code_at` (reading one), `sum` (`+`), and
`Memory.Address.text(length)`, which is how every `String` is made from bytes (`slice`, `terminated_text()` and
anything that fills a buffer itself end in it). The right side of `+` casts toward
`String` (every numeric type, `Boolean`, and enum all format to text; see
[Numeric types](values_and_types.md#numeric-types) for `Float`/`Double`'s
shortest-round-trip printing); assigning a `String` to any numeric variable parses it, defaulting to `0`/`0.0` on
failure. `==`/`!=`/`<`/`>` compare by content, and
([Operators](functions_and_operators.md#operators)'s table) also work as
`equals(other)`/`less_than(other)`/`greater_than(other)`; `+` also works as `sum(other)`.

**Building text in a loop is linear.** `text = text +
piece`, `text = "{text}{piece}"` and any longer join that starts with the variable it is stored back into append
to that text in place when nothing else holds it, and otherwise copy it once with room to grow, so a `String`
shared by two names never changes under the other one. Short text grows inside its sixteen bytes and moves into a
block once it passes 15 bytes. When it applies and what it saves is
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
| `pluralize()` / `singularize()` | `String` | the English plural or singular of the last word, by the rules below |
| `to_tiny()` / `to_short()` / `to_integer()` / `to_long()` | `Tiny?` / `Short?` / `Integer?` / `Long?` | digits with an optional `+` or `-`, spaces around them allowed; `null` for anything else (`"12abc"`, `""`) or a number the type cannot hold |
| `to_byte()` / `to_unsigned_short()` / `to_unsigned_integer()` / `to_unsigned_long()` | `Byte?` / `UnsignedShort?` / `UnsignedInteger?` / `UnsignedLong?` | the same, and `null` for a negative number |
| `to_float()` / `to_double()` | `Float?` / `Double?` | digits with an optional sign, `.` and exponent (`-1.5e2`, `.5`), spaces around them allowed; `null` for anything else (`"1e"`, `"nan"`) |
| `sum(other)` / `equals(other)` / `less_than(other)` / `greater_than(other)` | `String` / `Boolean` | the explicit call form of `+`/`==`/`<`/`>` |
| `to_string()` / `to_debug()` | `String` | the text itself / the text quoted, with `"`, `\` and a line feed escaped ([`Console.debug`](#console)) |
| `to_bytes()` | `List<Byte>` | the text's bytes, one per byte of its UTF-8, for [hashing, encoding and compressing](#bytes-base64-compression-hashes-and-passwords) |

**`pluralize()` and `singularize()` inflect English the way Rails' ActiveSupport does**, so a
name and its collection read as a pair: `"active_quest".pluralize()` is `"active_quests"`, and
`"map_names".singularize()` is `"map_name"`. Only the last word changes: the text after the last `_` or space, or
from the last capital that follows a lower-case letter (`"ActiveQuest"` → `"ActiveQuests"`). The word is inflected
in lower case and handed back in the case it came in: `"Person"` → `"People"`, `"ACTIVE_QUEST"` →
`"ACTIVE_QUESTS"`. In order, a whole last word that is **uncountable** stays as it is (`data`, `equipment`,
`information`, `rice`, `money`, `species`, `series`, `fish`, `sheep`, `jeans`, `police`); an **irregular** one
swaps with its pair (`person`/`people`, `man`/`men`, `woman`/`women`, `child`/`children`, `sex`/`sexes`,
`move`/`moves`, `zombie`/`zombies`); otherwise the first of ActiveSupport's ending rules that matches applies
(`box` → `boxes`, `enemy` → `enemies`, `wolf` → `wolves`, `knife` → `knives`, `analysis` → `analyses`, `medium`
→ `media`, `index` → `indices`, `matrix` → `matrices`, `vertex` → `vertices`, `mouse` → `mice`, `ox` → `oxen`,
`octopus` → `octopi`, `quiz` → `quizzes`, `status` → `statuses`), and a word no rule names takes or loses an `s`.
A word already in the asked number is left alone (`"quests".pluralize()` is `"quests"`, `"people".pluralize()` is
`"people"`), and `""` answers `""`. The rules are ActiveSupport's own, oddities included (`"potato".pluralize()` is
`"potatos"`: only `buffalo` and `tomato` take `-oes`). They are ordinary Spite in `library/string.spite`, so the
compiler, itself a Spite program, can call them while compiling; a user program has no compile-time call of its own.
They are functions, like `upper_case()`, because a `String` has no attributes for a getter to answer.

#### Maths

The maths of a 3D engine (skinning, animation, lighting) as members of the number classes, so it adds no
syntax and binds no singleton ([values_and_types.md](values_and_types.md#maths-functions) teaches them). On `Float` and `Double`,
each answering the receiver's type unless it says otherwise, with every other operand cast to that type like any
argument:

| Member | `Float` / `Double` in C | Answers |
|---|---|---|
| `square_root()` | `sqrtf` / `sqrt` | not-a-number below zero; `-0` for `-0` |
| `sine()`, `cosine()`, `tangent()` | `sinf`, `cosf`, `tanf` / `sin`, `cos`, `tan` | of an angle in radians |
| `arc_sine()`, `arc_cosine()` | `asinf`, `acosf` / `asin`, `acos` | not-a-number outside -1 to 1 |
| `arc_tangent()` | `atanf` / `atan` | between -pi/2 and pi/2 |
| `rise.arc_tangent_over(run)` | `atan2f` / `atan2` | the angle of the point `(run, rise)`, between -pi and pi, in the right quadrant; `0` for `(0, 0)` |
| `power(exponent)` | `powf` / `pow` | |
| `exponential()` | `expf` / `exp` | infinity once it overflows |
| `logarithm()`, `logarithm_base_2()`, `logarithm_base_10()` | `logf`, `log2f`, `log10f` / `log`, `log2`, `log10` | the natural logarithm and the other two; minus infinity at `0`, not-a-number below it |
| `floor()`, `ceiling()`, `truncate()` | `floorf`, `ceilf`, `truncf` / `floor`, `ceil`, `trunc` | a whole value, still in the receiver's type: `-2.5` gives `-3`, `-2` and `-2` |
| `round()` | `roundf` / `round` | half away from zero: `2.5` is `3`, `-2.5` is `-3` |
| `absolute()` | `fabsf` / `fabs` | |
| `minimum(other)`, `maximum(other)` | `fminf`, `fmaxf` / `fmin`, `fmax` | an operand that is not a number is ignored, as C's are: `nan.minimum(0.0)` is `0` |
| `clamp(low, high)` | `fminf(fmaxf(value, low), high)` | `maximum(low)` then `minimum(high)`: not-a-number gives `low`, and `high` wins when `low` is above it |
| `is_finite()`, `is_infinite()`, `is_not_a_number()` | `isfinite`, `isinf`, `isnan` | a `Boolean` |

On every whole number, `Tiny` to `UnsignedLong`, answering the receiver's type:

| Member | Answers |
|---|---|
| `absolute()` | the value without its sign; the smallest signed value wraps to itself, as its negation does (`Integer.smallest.absolute()` is -2147483648), and an unsigned value is itself |
| `minimum(other)`, `maximum(other)` | the smaller or larger, `other` cast first: a `Byte`'s `minimum(300)` compares with 44 |
| `clamp(low, high)` | `value` held between `low` and `high`, with `high` winning when `low` is above it, as on a float |

**Constants are get-only attributes of the class itself** (a class is an object): `Float.pi`, `tau`, `euler_number`,
`infinity`, `not_a_number`, `largest` (the largest finite value) and `smallest` (the most negative finite
one) on `Float` and `Double`, and `largest` and `smallest` on each whole number (`UnsignedLong.largest` is
18446744073709551615, its `smallest` 0). They are read without parentheses, and nothing can assign one:
`Float.pi = 3.0` is "'Float.pi' is a constant, and a constant is get-only: nothing can assign it", and
`Float.pi()` is "'Float.pi' is a constant, read as an attribute and never called: write 'Float.pi'"
(`diagnostics/maths_constant_assigned`). Each is the value itself in the C, written exactly (`0x1.921fb6p+1f`
for `Float.pi`, `INT32_MAX` for `Integer.largest`). On a value, `angle.pi` is "'pi' is a constant of the
class Float, not of a value: write 'Float.pi'"; on the class, a function of a value is "'Float.square_root()'
calls a function of a value on the class: the class Float answers only its constants, ..." and a name that is
neither is "the class Integer answers only its constants, largest and smallest, and 'pi' is not one of them"
(`diagnostics/maths_constant_on_value`).

**Nothing here halts.** Floats keep infinity and not-a-number, so every edge answers the IEEE 754 value the
C library gives (`(-1.0).square_root()` is not-a-number) and `errno` is never read. The whole-number
division checks are untouched: none of these divides.

**Each is a primitive of the language, lowered by the backend** (named in Spite, lowered in one
place, so no Spite changes with the backend). `--final-classes` prints them as bodiless declarations in each number
class, and `bootstrap/source/generation/maths_primitives.spite` is the one place that says what C each becomes:
a macro written where it is called, so `angle.sine()` is `sinf(angle)` in the C, with no function of Spite's own
around it, and a whole number's `clamp` is two comparisons in one statement. None has hand-written C in a `.spite`
file of `library/`. A call whose operands are all constants, `(0.5).sine()` or `Float.pi.cosine()`, is worked out
while compiling with the same C library function, so its answer is bit for bit the one the program would have
computed ([optimizations.md](optimizations.md#maths-on-constants-is-worked-out-while-compiling);
`conformance/stage6/maths_folding`).

**Tree-shaken, and nothing at run time.** A member a program never calls is not in its C; `<math.h>` is
included only when a member that calls the C library survives tree shaking, and only then does a build on Linux or
macOS link it (`-lm`; Windows has it in the C runtime). A program that uses none of it, `examples/hello`, carries
no `#include <math.h>`. There is no `fused_multiply_add`: without a processor flag telling the C compiler the
machine has the instruction, `fmaf` is a slow exact routine in the C library, a cost a reader would not expect
from one multiply and add.

Other languages' short names are errors naming the Spite one, on the class that has it: `side.sqrt()` is "Float
has no function 'sqrt': Spite spells it 'square_root', since no name is abbreviated", and likewise `sin`, `cos`,
`tan`, `asin`, `acos`, `atan`, `atan2`, `pow`, `exp`, `log`, `ln`, `log2`, `log10`, `ceil`, `trunc`, `abs`,
`fabs`, `min`, `max`, `isnan`, `isinf` and `isfinite` (`diagnostics/maths_other_spellings`).
`conformance/stage6/maths_functions` pins the values and the edges, and `maths_precision` what `Float` loses
against `Double`. `benchmarks/maths_stopgaps` times them against the pure-Spite versions a game engine package wrote while
they were missing ([benchmarks/README.md](../benchmarks/README.md)).

#### System classes

Ordinary classes in `library/`, each written once for every system, with the few functions that call the system
in `library/windows/`, `library/linux/` and `library/mac/`, which reopen the class and reach the system's own
library through `DynamicLibrary`. What each member answers is in the section of this page that teaches it.

| Class | Members |
|---|---|
| `File(path)` | `path`, `to_string(): String` (the path), `read(): String?`, `write(text): Boolean`, `append(text): Boolean`, `exists(): Boolean`, `remove(): Boolean`; bytes: `size(): Long?`, `modified(): Instant?`, `read_bytes(position, count, address): Long?`, `write_bytes(address, count): Boolean`, `append_bytes(address, count): Long?`, `map(): MappedFile?`; see [Read and write a file](#read-and-write-a-file). A `File` is a path with no open handle: each call opens and closes the file, positions replace seeking (`_fseeki64`/`_ftelli64` on Windows, so a position past 2 GB works), and `modified()` reads `GetFileAttributesExA`'s `ftLastWriteTime` on Windows and `stat`'s modification time on Linux and macOS |
| `MappedFile` | made by `File.map()`: a read-only mapping of the whole file (`CreateFileMappingA`/`MapViewOfFile` on Windows, `mmap` with `PROT_READ` and `MAP_PRIVATE` on Linux and macOS, the file itself closed once mapped), undone by `drop()` (`UnmapViewOfFile`, `munmap`). `size(): Long`; `get_at(position): Byte?` (`mapped[position]`), `read_short`, `read_integer`, `read_long`, `read_float`, `read_double` (each `(position): T?`) and `text(position, count): String?` answer `null` unless every byte they would read is inside the file, so a bad offset read from the file cannot read outside it; a read inside is one load from the mapping. An empty file maps to a `MappedFile` of size 0, since the operating systems refuse to map nothing. `conformance/stage6/mapped_files`; see [A file larger than memory](#a-file-larger-than-memory-map-it) |
| `Directory(path)` | `path`, `entries(): List<Directory.Entry>` (below), `files(): List<String>`, `folders(): List<String>`, `exists(): Boolean`, `create(): Boolean`; see [List a directory](#list-a-directory) |
| `Process(command, arguments)` | `working_directory`, `environment_variables`, `run(): Integer`, `output(): String`, `run_attached(): Integer`; see [Run a process](#run-a-process). Each argument reaches the child whole: single-quoted for the shell on Linux and macOS, and quoted by the `CommandLineToArgvW` rules on Windows, a `key=value` argument quoting only its value (`-script="a b"`). `run()` reads the child's standard output through `_popen`/`popen`; `run_attached()` is the C library's `system` |
| `Program()` | a singleton: `exit(code)`, `sleep(milliseconds)`, `environment(name): String?`, `executable_path(): String`, `live_allocations(): Integer`; see [Program](#program). `exit` flushes `Console` first, since the C library's `exit` would drop what the program's own standard output still buffers |
| `Clock()` | a singleton: `elapsed_nanoseconds(): Long`, `elapsed_milliseconds(): Long`, `now(): Instant`; `elapsed_nanoseconds()` is the monotonic clock, with a nanosecond unit and no allocation per reading; see [Clock](#clock), [Time](time.md#time-one-stored-instant-zones-for-presentation) |
| `Console()` | a singleton: `print(...values)`, `write(...values)`, `error(...values)`, `debug(...values)`, `flush()`, `read_line(): String?`; see [Console](#console), and below |
| `FileSystemWatcher()` | `watch_for_changes(target: Directory or File): Boolean`, `changes(): List<String>`, `wait_for_changes()`; see [Watch files and folders](#watch-files-and-folders), which is the rule. `HotReload` is built on it ([REPL and live reload](repl.md#repl-and-live-reload)) |
| `Socket()` | public library surface: `listen_locally(port): Boolean`, `listen_everywhere(port): Boolean`, `listen_at(host, port): Boolean`, `connect_locally(port): Boolean`, `connect(host, port): Boolean`, `accept_client(): Socket?`, `read_line(): String?`, `read_bytes(address, count): Integer`, `write_line(text): Boolean`, `write_bytes(address, count): Boolean`, the calls that never wait `accept_client_now(): Socket?`, `read_line_now(): String?`, `read_bytes_now(address, count): Integer` and `write_bytes_now(address, count): Integer`, `closed: Boolean`, `close()`: TCP over IPv4 on every system ([Socket](#socket), and below); `--repl-port` and `spite connect` use `listen_locally` and `connect_locally` ([REPL and live reload](repl.md#repl-and-live-reload)) |
| `Concurrent(function)`, `Parallel(function)` | the handle stands in for what the function returned, and reading it is the wait; `finished: Boolean` never waits; dropping the handle waits for it; there is no `wait()` and no `join()`; see [concurrency.md](concurrency.md) |
| `ThreadPool()` | the singleton every `Parallel` runs on: `size(): Integer` worker threads, `worker_index(): Integer` (`-1` off the pool); see [The thread pool](concurrency.md#the-thread-pool) |
| `ThreadLocal<T>()`, `Lock()`, `ThreadSlot()` | one value per thread, `get(): T?`, `set(value)`; a lock, `while_locked(function)`, `lock()`, `unlock()`; the raw per-thread `Long` both are built on, `read()`, `write(value)`; see [A value per thread, and a lock](concurrency.md#a-value-per-thread-and-a-lock) |
| `DynamicLibrary(file_name, naming, header)` | every foreign function, constant and type of a native library; see [Foreign libraries](foreign_libraries.md#foreign-libraries). `library/dynamic_library.spite` holds its `file_name` and `handle`, its constructor and `drop()`; opening, closing and finding a symbol are the compiler's reopening |
| `Memory.Heap()`, `Memory.Arena(block_bytes)`, `Memory.Address` | the floor every other type is built on: allocators and the place they hand out; see [Memory](memory.md#memory-is-the-floor-and-you-can-build-on-it), whose rules they are ([Memory](memory.md#memory)) |
| `TypedMemory<$value_type>()` | `read_value(address, index)`, `write_value(address, index, value)`, `release_value(address, index)`, `value_bytes()`: values of any type in raw memory, reference counts kept right; what `List<T>` keeps its elements with, and what a container of your own uses |

**`Console` is a singleton:** `Console()` is the same instance everywhere, `Console` is an ordinary class
name rather than a reserved word, and `console.class.name` is `"Console"` like any other class. Calling a member
on `Console()` directly is an error: `'Console' is a singleton: bind it once beside the attributes, 'var console =
Console()', and use 'console.print'`. `read_line()` answers `null` only at the end of the input with nothing
read, and drops a `\r` before the line feed. The entry constructor returning normally is exit code `0`.

**Printing is `to_string()`.** `print`, `write` and `error` are Spite in
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
`conformance/stage6/printable_values`). A `type` writes a required function as `to_string(): String`. What stays
the compiler's is only the floor: `_write_output(text)`, `_write_error(text)` and `flush()` are declared nowhere in
Spite, and the compiler supplies their C (`fwrite` and `fflush`), as it does `Memory.Heap`'s.

**A printed line is written out at once.**
`print`, `error` and `debug` call `flush()` after their line break, so output redirected to a file or a pipe
shows each line when it is printed instead of when the C library's buffer fills or the program exits; `write`
does not, so a prompt or a line built from pieces goes out with the next line end. The C library already writes
a terminal's output promptly, so nothing changes there. Chosen as the cheapest way that shows every line:
`benchmarks/console_lines` prints 200 000 lines, and on Windows to a file it takes about 700 ms flushed per line
against about 140 ms buffered until exit (to a pipe or the null device the two cost the same, about 450 and 530
ms): about 3 microseconds per line, one `WriteFile` or `write`. The alternatives cost the same or do not show
every line: line buffering (`setvbuf` with `_IOLBF`) is one system call per line too, and the Windows C library
treats it as full buffering; flushing on a timer, or only where the program waits, leaves the last line of a
program that computes without waiting in the buffer. A program that prints a great deal to a file and does not
need to be watched builds its text and prints it in fewer, longer lines.

Printing a value costs what the call says: the list of values
is a `List` like any variadic call's, a number is held in the list with its class and goes through its `to_string()`, and the text is written
with its length rather than up to its first zero byte. `conformance/stage6/text_building` and
`fused_chain_allocations` pin those allocations. A `crash` writes its operands itself, through each value's
`to_string()`, because it reports on the way out of a program that is stopping. `flush()` writes out what the
output and error streams hold and nothing more.

**`Console.debug` and `to_debug()`.** Each class has an automatic `to_debug(): String` that returns something like
`Class {attribute: value, other: value}` by nesting `to_debug()`s. `debug(...values: List<Debuggable>)`, where `type Debuggable { to_debug():
String }`, writes each value's `to_debug()` separated by a space, then a line break. Every value answers it:

```text
Player { name: "hero", scores: [3, 7], bag: {"gold": 2}, partner: null, mood: 'calm' }
```

In detail:

- **A class shows its name and its attributes**, in declaration order, as `Name { attribute: value }`, and
  `Name {}` when it has none to show. An attribute that is a class is shown by *its* `to_debug()`, so a class that
  declares its own is shown by it wherever it appears. A `List` is `[a, b]`, a `Dictionary` is `{"key": value}`,
  text is quoted with `\"`, `\\` and `\n` escaped, a `Symbol` or enum value is written the way Spite writes it
  (`'calm'`), a number and a `Boolean` as they print, and an absent `T?` is `null`. A `Spite.Class` is its name.
- **Private attributes are left out.** The walk is the plural attribute template ([Symbol codegen](metaprogramming.md#templates)) run from
  `Spite.DebugInstance`, and a plural over another class's attributes ranges over the ones that class lets
  others read: a `_` attribute is its own business, and reading it from outside would be the ordinary private
  error. The JSON and binary writers and readers follow the same rule.
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
  lists as `List<String>(...)`), which it builds from `Spite.Attribute`'s `.value` through `value.to_string()`,
  and the documented sessions depend on it.
- **What it costs**: in a program that calls `debug`, one generated `to_debug()` per class it shows, which
  builds the text as `String`s; `to_debug()` reads fields through Symbol templates, not through `Spite.Attribute`,
  so it boxes nothing. Every attribute it shows counts as read.

`conformance/stage6/debug_values`.

**A directory is navigated through its entries.** `Directory` has a `path` exactly as `File` does, and `entries()` answers a `List<Directory.Entry>`, where
`Directory.Entry` is the union of `Directory` and `File` declared in `library/directory.spite`. Each entry's `path`
is its parent's joined with its name, so a `switch` tells the two apart and a folder is walked by calling the same
function on it again ([Walk a directory tree](#walk-a-directory-tree); `conformance/stage4/directory_entries`).

In detail:

- **The name is `Entry`**, namespaced as `Directory.Entry`, because it is what a directory listing calls each of
  its items and it says nothing the class does not: `DirectoryEntry` would repeat the class it already lives in,
  and `Path` would claim a text value it is not.
- **Folders come first, then files, each sorted by name**; `.` and `..` are never listed. It is the order
  `folders()` then `files()` already give, so the three agree.
- Each operating system's folder lists a directory its own way, through `entry_names(want_folders)`, since
  `entries()` is the public listing and Spite has no overloading.
- `--final-classes` prints `Directory` back out with its `union Entry`, which compiles because a union declared
  again replaces the earlier one ([Packages, namespaces and loading](packages.md#packages-namespaces-and-loading)).

**`Socket` does what a game server needs, on every system.**
`library/socket.spite` holds everything but the calls into each system's library, which `library/windows/`,
`linux/` and `mac/socket.spite` reopen the class with:

- **Addresses.** `listen_locally` and `connect_locally` build `127.0.0.1` themselves, `listen_everywhere` builds
  `0.0.0.0`, and `listen_at` and `connect` resolve the host with the system's `getaddrinfo`, asking for IPv4 and a
  stream socket and taking the first answer; a name that does not resolve answers `false`. Resolving waits in
  place, like connecting. The listening queue is 64 connections deep.
- **Waiting calls.** `accept_client`, `read_line` and `read_bytes` reach `Socket.accept_handle` and
  `Socket.receive_into`, which are the compiler's waits ([concurrency.md](concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting)):
  in a `Concurrent` they return to the event loop while a helper thread makes the call. `write_line` and
  `write_bytes` send until everything is sent, in place, as before.
- **Calls that never wait** (`_now`) are ordinary calls, not waits, so they add nothing to a state machine. Linux
  and macOS pass `MSG_DONTWAIT` to `recv` and `send` and ask `poll` with a timeout of 0 before `accept`, so a socket
  stays in blocking mode and the waiting calls keep working on it. Windows has no such flag: a socket switches to
  non-blocking mode (`ioctlsocket` with `FIONBIO`) the first time a `_now` call reaches it and back when a waiting
  call does, so a loop that only polls pays for one switch, not one per call. A client that `accept_client_now`
  hands back is in blocking mode, like every new `Socket`. "Nothing yet" is `EWOULDBLOCK` (`WSAEWOULDBLOCK` on
  Windows, `EAGAIN` or `EINTR` elsewhere, read through `__errno_location` or `__error`); any other failure, or a
  read of 0 bytes, is the end of the connection.
- **`closed`** is an attribute: `false` from `listen_*` or `connect*`, `true` once a read sees the end of the
  connection or a failure, once a write fails, or after `close()`. A read on a closed socket answers `0` or `null`
  without calling the system, a write sends nothing, and nothing crashes. `read_line` answers
  `null` for a closed connection, and sets `closed`.
- **Lines and bytes mix.** Text `read_line` read past its line waits in the socket, and `read_bytes` and
  `read_bytes_now` hand those bytes out first.
- **What it costs**: a `Boolean` per `Socket` (and on Windows one more for the mode), and the functions a
  program calls: one that never polls has no `_now` code, and a system function only an unused call reaches
  (`getaddrinfo`, `ioctlsocket`, ...) is never looked up. `conformance/stage6/socket_bytes` (a server and a client in one
  program, bytes both ways and a line without waiting, then the close) and `socket_waits` (the waiting calls,
  inside a `Concurrent`) are the proof.

#### Pure Spite: dissolving the runtime

**The standard library is Spite, not hand-written C, and what the compiler supplies below it is a short list of
operations, not a file someone maintains.** `Spite.Class` is an ordinary standard library class with an ordinary
declaration, which cannot be true of a library written in C. The target is **zero hidden code**: everything the
compiler supplies ends up as explicit Spite, and nothing ties Spite to C; what cannot be Spite, the reads, writes
and atomics at an address, are language primitives each backend lowers.

- **Purity costs no performance**: the output is still C, so a Spite-written `trim()` goes through the same
  optimiser a hand-written one would. It is the same machine code from a better source.
- **Library classes stop being special**: they are reopenable like any class, and an inspectable build shows
  every one of them to the REPL, while a production build carries only those the program reaches.

**What is Spite**, in `library/`:

| Part | How |
|---|---|
| `String`, `List<T>`, `Dictionary<T>`, number to text (`library/number_text.spite`) | over `Memory.Heap` and `TypedMemory<T>`. The syntax stays the compiler's (`[]`, list literals, `List<T>()`), mapped onto these classes' functions, and per element type the compiler still writes four one-line functions a generic class cannot, plus `deep_copy`; the member templates are Spite in `library/list.spite` ([collections.md](collections.md#standard-library-metaprogramming)) |
| `File`, `Directory`, `Process`, `Program`, `Console.read_line`, `Clock`, `Socket`, `FileSystemWatcher`, `ThreadPool`, `Lock`, `ThreadSlot`, the time-zone database | written once in `library/`; each operating system's folder reopens the class with the few functions that call its own library (`ucrtbase.dll`/`kernel32.dll`, `libc.so.6`, `libSystem.dylib`) through `DynamicLibrary` |
| `--debug-memory`'s live table | `AllocationTable` (`library/allocation_table.spite`), an open-addressing set of live addresses with each object's class id beside it, which prints the leak report; the compiler emits only the small functions `SPITE_MALLOC`, `SPITE_REALLOC` and `SPITE_FREE` call under `--debug-memory` |
| the REPL, live reload, the event loop | `ReadEvaluatePrintLoop`, `HotReload` and `Scheduler`, over reflection tables the compiler generates |
| printing, `to_debug()`, JSON and bytes | `Console`, `Spite.Debug<T>`, `Spite.DebugInstance<T>`, `JsonWriter<T>`, `JsonReader<T>`, `BinaryWriter<T>` and `BinaryReader<T>` ([json.md](json.md)) |

**What is the compiler's**: the floor, and the functions it supplies below it:

1. **`Memory.Address`'s reads, writes and atomics** are language primitives, each one machine operation the
   backend writes where it is called, like `+` ([memory.md](memory.md#memory-is-the-floor-and-you-can-build-on-it)).
   This is the floor that stays.
2. **Supplied bodies**: functions declared without a body whose C the compiler writes
   (`bootstrap/source/generation/prelude.spite`): `Console`'s `_write_output`, `_write_error` and `flush`
   (`fwrite`, `fflush`); `Memory.Heap`'s `allocate`, `resize`, `free` and `live_allocations` (the C library's
   `malloc`, or the `--debug-memory` table) and `Memory.Address`'s `copy_to` and `compare_bytes` (`memmove`,
   `memcmp`), `text(length)`, `String.sum` and `String.code_at` (which depend on whether text fits inside the
   `String`);
   `TypedMemory<T>`; the number classes' bit operations; `DynamicLibrary`'s opening, closing and
   symbol lookup; the entry points and frames of `Concurrent`, `ThreadPool` and `Scheduler`; `HotReload`'s
   compiler hand-off; and `Spite.Attribute`'s and `Spite.Function`'s dispatch.
3. **The object header**: retain, release and the class id are code the compiler emits, not functions anyone
   calls. For `String` that includes the two decisions only the header can make: a `'constant'` text is never
   counted, and `text = text + piece` grows the text in place only when its block's count is one, or inside the
   `String` itself while it is 15 bytes or fewer.
4. **Entry and exit**: `main`, and text literals, which are static data in the `'constant'` section, as symbols
   are.

**WebAssembly.** There, `Memory.Heap`'s `allocate` grows linear memory with `memory.grow`, and the
C runtime library is the JavaScript host's. **The web shim is the one honest exception, and its target is zero
hand-written lines.** A file that runs in the JavaScript virtual machine cannot be Spite by construction: it is
on the far side of a boundary Spite does not own. What is achievable is that nobody writes it: the imports come
from `external js` declarations and the command-buffer drain loop is a switch over an opcode table, both data the
compiler can emit, so the source of truth stays Spite and nothing in the repository is written in a second
language ([targets.md](targets.md)).

#### examples/calculator

A recursive-descent arithmetic interpreter (`token.spite`, `lexer.spite`, `expression.spite` + one file per union
member, `parser.spite`, `evaluator.spite`, `calculator.spite`), reading its program from `program.txt` next to it.
Supports `+ - * /`, unary `-`, parentheses, variable assignment (`x = 1 + 2`) and reference, one statement per
line. The lexer scans `source.split("")` with a `while` loop that passes each character and its index on (so it
is not a loop a member template replaces), keeping the current token's start as an attribute and slicing
it out on a token boundary; the parser is recursive, including precedence climbing written as tail recursion
(`parse_expression_rest`/`parse_term_rest`). The parser makes each `BinaryExpression(...)` on a line
of its own and hands it on by name, since no constructor is an argument:
`left_expression`/`right_expression` are ordinary by-reference parameters, stored directly into the
union-typed attribute.

---

Next: [Lists, dictionaries and member templates](collections.md), the containers every program uses and how they are extended.
