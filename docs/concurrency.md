# Concurrency: waiting without colouring

There is no `async` and no `await` in Spite, and there never will be. In JavaScript or C# a function that waits
declares itself `async`, which changes its return type and forces every caller to `await` it, and every caller's
caller, all the way up: the waiting leaks into every signature above it. Spite keeps waiting a property of the
*call site*: any function can be run concurrently by whoever calls it, and a function never says whether it waits.

## `Concurrent`: work that waits

`Concurrent(file.read)` takes a function value (a function bound to its instance, as in
[functions_and_operators.md](functions_and_operators.md)) and starts running it straight away. The handle it
gives back stands in for what the function returns (a `String?` for `file.read`), and the type is worked out
from the function, so it is never written.

```gdscript title=concurrent_tour/sleeper.spite
var program = Program()
var name = ""
var milliseconds = 0
var log = List<String>()

func Sleeper(starting_name: String, starting_milliseconds: Integer, shared_log: List<String>) {
    name = starting_name
    milliseconds = starting_milliseconds
    log = shared_log
}

func nap(): String {
    log.append("{name} falls asleep")
    program.sleep(milliseconds)
    log.append("{name} wakes up")
    return name
}
```
```gdscript title=concurrent_tour/concurrent_tour.spite entry
var console = Console()
var log = List<String>()

func ConcurrentTour() {
    var slow_sleeper = Sleeper("slow", 150, log)
    var fast_sleeper = Sleeper("fast", 10, log)
    var slow = Concurrent(slow_sleeper.nap)
    var fast = Concurrent(fast_sleeper.nap)
    log.append("both are asleep")
    log.append("{slow} and {fast} are back")
    var notes = File(".spite/concurrency_notes.txt")
    notes.write("written before the read")
    var reading = Concurrent(notes.read)
    if reading {
        log.append("read: {reading}")
    }
    var joined_log = log.join("\n")
    console.print(joined_log)
}
```
```output
slow falls asleep
fast falls asleep
both are asleep
fast wakes up
slow wakes up
slow and fast are back
read: written before the read
```

`nap` and `notes.read` are ordinary functions: they sleep and read the way any function does. What made the two naps overlap is the
caller writing `Concurrent(...)`, and nothing else. The fast one woke first although it was started second.

### Reading the value is the wait

There is no `.wait()` and no `.join()`: on a `Concurrent<Integer>`, `.wait()` is the error `an Integer has no
function 'wait'`, because the handle already stands for its result. A `Concurrent<String>` goes wherever a `String` is expected, and the first place it
is used as one is where the program waits for it: `"{slow} and {fast}"` above, `console.print(slow)`,
`slow.length()`. Reading it a second time does not wait again: the value is kept. A result that may be null is
narrowed on the handle itself, `if reading { ... }`, as any `T?` is.

A `var` written without a type keeps the handle, so `var reading = Concurrent(notes.read)` is still the running
work and can be asked whether it has `finished`, the one member the handle has of its own. Every place a handle
becomes its value is listed in [the rules](#concurrency-concurrent-parallel-and-hidden-waiting).

### Reads in a row overlap

Outside a `Concurrent`, a read waits where it is written, one after another. The one exception the IO classes make
on their own is reads side by side: when two or more declarations in a
row each read a `File` (`read()`) or a `Socket` (`read_line()`), the compiler starts every read but the last as a
`Concurrent`, runs the last one, and waits for all of them before the next statement. The program waits for the
slowest file rather than for each in turn, and nothing it does afterwards can tell: the values are the same, and a
file written by the next statement is written after the reads.

```gdscript title=reads_in_a_row/reads_in_a_row.spite entry
var console = Console()

func ReadsInARow() {
    var settings_file = File(".spite/documentation_settings.txt")
    var scores_file = File(".spite/documentation_scores.txt")
    settings_file.write("volume=7")
    scores_file.write("ada=12")
    var settings = settings_file.read()
    var scores = scores_file.read()
    settings_file.write("volume=8")
    crash settings
    crash scores
    console.print(settings, scores)
}
```
```output
volume=7 ada=12
```

The two reads above ran at once. The rule is deliberately narrow: only reads, only side by side, and only when
the name read into is not assigned again later, so starting them early cannot change what the program sees. A
read followed by other work waits where it is written. The exact conditions are in
[the rules](#concurrency-concurrent-parallel-and-hidden-waiting) ("Reads in a row overlap");
a program that has two such reads pays for the scheduler, as one that writes `Concurrent` does
([what it costs](#what-it-costs)).

### What the compiler does at a wait

A `Concurrent` is a **state machine** the compiler writes while compiling.
Every function that can reach a wait (`nap` above, and whatever `nap` calls that waits) is compiled a second time
as a resumable version: its locals and parameters live in a small frame on
the heap instead of on the C stack, and every wait inside it is a numbered point the function can return from and
later jump back to. Starting a `Concurrent` makes that frame and runs it to its first wait; when the wait is over,
the program's event loop (`library/scheduler.spite`) runs it again from where it stopped. There is no stack per
`Concurrent`, no stack switching and nothing to ship that a WebAssembly build could not carry:

| The program writes | While it waits |
|---|---|
| `program.sleep(milliseconds)` | the state machine returns, and is run again once its time has come |
| `Console.read_line()`, reading or writing a `File`, a `Socket`'s `accept_client`, `read_line` or `read_bytes` | the one blocking system call runs on a short-lived helper thread, and the state machine is run again, on the program's thread, once it has returned |
| reading a `Concurrent`'s value, or dropping it | the state machine is run again once that one has finished |
| reading a `Parallel`'s value, or dropping it | the pool finishes it (the waiting thread runs it itself if no worker has started it), then anything ready runs once |

Code that is not inside a `Concurrent` (the entry constructor and everything it calls) waits where it is: its
wait runs the event loop itself, so the state machines keep going around it until it is done, unless the program
has chosen [where they resume](#choosing-where-concurrents-resume). Only one piece of
Spite code runs on the program's thread at a time, and a state machine only stops at one of these points, so no
two pieces of Spite code touch the program's state at the same time. The helper thread runs the system call and
nothing else.

**What waits inside a state machine.** A wait written as a call (`program.sleep(5)`, `file.read()`,
`reading.length()` on a `Concurrent`, a function of your own that waits) is a point the state machine returns
from, wherever the call is written: in a `var`, an argument, a `{...}` inside text, a `while` condition (which
waits again on every pass), the right side of `and` or `or`, a call through a function value, a union or a `type`,
a constructor. Leaving a scope that drops a `Concurrent` waits for it there too. While one `Concurrent` waits, the
others run, so a value can change during a wait; the order you wrote still holds. Whatever an expression works out
before its wait is worked out first and kept, and what comes after is worked out after the wait:

```gdscript title=written_order/written_order.spite entry
var console = Console()
var program = Program()
var count = 1

func WrittenOrder() {
    var adding = Concurrent(add_after_a_bump)
    var total: Integer = adding
    console.print("read before the wait:", total, "read after it:", count)
}

func add_after_a_bump(): Integer {
    return count + bump()
}

func bump(): Integer {
    count = 100
    program.sleep(1)
    return 2
}
```
```output
read before the wait: 3 read after it: 100
```

`count` is read before `bump()` runs, as written, so the sum is `1 + 2`. A local or a parameter is not copied
for this, since nothing else can change it while the `Concurrent` waits.

**A call that never waits is not a wait.** A `Socket`'s `accept_client_now`, `read_line_now`, `read_bytes_now`
and `write_bytes_now` answer at once with what is there ([standard_library.md](standard_library.md#socket)), so
they are ordinary calls: a game loop that polls its connections every frame has no state machine and no helper
thread for them, and it waits only where it sleeps until the next frame.

**When blocking is faster, the compiler blocks.** A program that never makes a `Concurrent` has no state machines,
no event loop and no helper threads: every wait is the plain blocking call. Inside a program that has them, a wait
is the plain call whenever nothing else could run (no `Concurrent` is alive and no REPL is listening). It is the
same rule as everywhere else in Spite: you write what you mean, and the compiler picks the fast way to do it.

### Dropping a handle waits for it

A `Concurrent` is reference counted like everything else, and when the last reference goes, its `drop()` waits
for the function to finish. So a block that started work cannot end while the work is still running, and no
result is ever lost or left running in the background: leaving a scope is a join point. That is also how work
that returns nothing is waited for: keep its handles in a list, and clearing the list, or leaving the function
that holds it, waits for all of them.

```gdscript title=joined_on_drop/joined_on_drop.spite entry
var console = Console()
var program = Program()

func JoinedOnDrop() {
    start_and_forget()
    console.print("the scope is left only after ring has finished")
}

func start_and_forget() {
    var ringing = Concurrent(ring)
    console.print("ring has finished: {ringing.finished}")
}

func ring() {
    program.sleep(10)
    console.print("ring")
}
```
```output
ring has finished: false
ring
the scope is left only after ring has finished
```

### `finished` never waits

`finished` answers whether the function has returned, and it never waits, so a frame loop can start work, keep
drawing frames, and pick the result up on the first frame after it is ready; the value is then read without
waiting at all:

```gdscript title=polled_loading/polled_loading.spite entry
var console = Console()
var program = Program()

func PolledLoading() {
    var level = LevelFile("forest")
    var loading = Parallel(level.read_tiles)
    var frames = 0
    while not loading.finished {
        frames = frames + 1
        program.sleep(1)
    }
    console.print("loaded", loading, "while frames kept running:", frames >= 0)
}
```
```gdscript title=polled_loading/level_file.spite
var name = ""

func LevelFile(starting_name: String) {
    name = starting_name
}

func read_tiles(): String {
    var tiles = 0
    while tiles < 100000 {
        tiles = tiles + 1
    }
    return "{name} with {tiles} tiles"
}
```
```output
loaded forest with 100000 tiles while frames kept running: true
```

It is the same on every system: it reads a flag the worker sets when the function returns, with no system call.

### Choosing where `Concurrent`s resume

A `Concurrent` moves on wherever the program waits: a `program.sleep` on the program's thread, a read outside any
`Concurrent`, reading a `Parallel`. That suits a tool, and not a game: a state machine that resumes in the middle
of a stage sees the world half updated. So an engine says where they resume:

- `Scheduler().resume_only_when_asked()` makes every other wait on the program's thread leave the `Concurrent`s
  alone from then on: a sleep only sleeps, a read only blocks, reading a `Parallel` only joins it.
- `Scheduler().run_ready()` runs every `Concurrent` whose wait is over, once each, and returns at once; the
  engine calls it between two frames.
- Reading a `Concurrent`'s value, or dropping it, still runs them all until that one is done. The program asked for
  that one, and it may be waiting on another, so a join never waits for something only the loop could finish.

The IO itself goes on meanwhile: a blocking read inside a `Concurrent` (a file, or a `Socket`'s `read_bytes`)
runs on its helper thread, and the state machine continues on the program's thread at the next `run_ready()`.

```gdscript title=frame_io/save_game.spite
var console = Console()
var save = File(".spite/documentation_save.txt")

func update_each() {
    var loads = 0
    while loads < 3 {
        var text = save.read()
        crash text
        loads = loads + 1
        console.print("load", loads, "read", text)
    }
}
```
```gdscript title=frame_io/frame_io.spite entry
var console = Console()
var program = Program()
var scheduler = Scheduler()

func FrameIo() {
    scheduler.resume_only_when_asked()
    var save_file = File(".spite/documentation_save.txt")
    save_file.write("level 3")
    var game = SaveGame()
    var loading = Concurrent(game.update_each)
    var frames = 0
    while not loading.finished {
        frames = frames + 1
        program.sleep(1)
        scheduler.run_ready()
    }
    console.print("each load took a frame of its own:", frames >= 3)
}
```
```output
load 1 read level 3
load 2 read level 3
load 3 read level 3
each load took a frame of its own: true
```

The `program.sleep(1)` in the frame never resumes the loads; only `run_ready()` does. That also means a loop that
polls `finished` and never calls `run_ready()` could never end, since nothing moves the `Concurrent` on, so it
halts instead: a `Concurrent` found unfinished a million times in a row while no `Concurrent` was stepped stops the
program with a crash report at the scheduler's count. Which systems to start this way is asked while compiling, with
`$system_type.functions['update_each'].is_resumable` ([metaprogramming.md](metaprogramming.md#asking-a-question-while-compiling)),
so a system never says that it does IO.

## `Parallel`: work that computes

`Concurrent` never makes a program faster at computing: every `Concurrent` runs on the program's one thread. For work that keeps a core
busy, `Parallel(function)` runs the function on the program's **thread pool**, and the same rules apply: the
handle stands in for the result, `finished` never waits, and dropping it waits.

```gdscript title=parallel_tour/summer.spite
var limit = 0

func Summer(starting_limit: Integer) {
    limit = starting_limit
}

func total(): Long {
    var sum: Long = 0
    var index = 0
    while index < limit {
        sum = sum + index
        index = index + 1
    }
    return sum
}
```
```gdscript title=parallel_tour/parallel_tour.spite entry
var console = Console()

func ParallelTour() {
    var small = Summer(1000000)
    var large = Summer(3000000)
    var small_sum = Parallel(small.total)
    var large_sum = Parallel(large.total)
    console.print(small_sum, large_sum)
}
```
```output
499999500000 4499998500000
```

### The thread pool

`ThreadPool()` (`library/thread_pool.spite`, a singleton) starts a worker thread for every core but one (the
program's own thread keeps that one) the first time a `Parallel` is made, and never another: a thousand
`Parallel`s a second reuse the same threads, and a program that makes none starts none. There is one pool per
program, and any code (an engine system, an asset loader, a library class) hands work to it the same way, by
`Parallel(function)`: no submission starts a thread of its own, and there is no second pool
to make or pass around. The workers take work in the order it was started. Reading a `Parallel` that no worker has
picked up yet runs it on the reading thread instead of waiting behind the queue, so a `Parallel` started from
inside another one cannot wait for a worker that is busy waiting for it. At exit the pool finishes what is queued
and stops its threads.

**Work that can never finish is a compile error.** A pool thread is lent to a piece of work until it returns, so
work that runs forever takes a thread away for good, and there is no `Thread(function)` for it: a loop tied to
one thread (a window's messages) runs on the program's own thread as one step per frame, and whatever waits
(accepting connections, reading a socket) waits through a call that gives its thread back. So a `while true` with
no `return`, `assert` or `crash` inside it, in the function a `Parallel` runs or in any function that work
reaches, whose function never waits, is `'Parallel(counter.count_forever)' runs 'Counter.count_forever' on the
thread pool, and this loop never ends and never waits, so the work never finishes and keeps its thread for good:
return from the loop once the work is done, or run the loop on the program's own thread`
(`diagnostics/endless_parallel_work`). It is reported at the loop, once per function.

| `ThreadPool()` | |
|---|---|
| `size(): Integer` | how many worker threads it runs (starting them if it has not) |
| `worker_index(): Integer` | which worker is running this code, from `0`, or `-1` on a thread that is not one of them, for scratch memory kept per worker |

### A value per thread, and a lock

Work split across threads often wants something of its own on each thread (a command buffer per runner, a
scratch list) and a short section only one thread may be in at a time. `ThreadLocal<T>()` holds one value per
thread: `get(): T?` answers this thread's (`null` until this thread has called `set`), and `set(value)` changes
only this thread's. `Lock()` is the lock: `while_locked(function)` runs a function value holding it, and `lock()` and
`unlock()` are there for the rare case that does not fit one function.

```gdscript title=per_thread_buffers/per_thread_buffers.spite entry
var console = Console()
var buffers = ThreadLocal<List<String>>()
var totals_lock = Lock()
var commands = 0

func PerThreadBuffers() {
    var runs = List<Parallel<Integer>>()
    var index = 0
    while index < 8 {
        var run = Parallel(record_commands)
        runs.append(run)
        index = index + 1
    }
    var recorded = 0
    index = 0
    while index < runs.count() {
        var count: Integer = runs[index]
        recorded = recorded + count
        index = index + 1
    }
    console.print(recorded, "commands recorded,", commands, "counted under the lock")
}

func record_commands(): Integer {
    var buffer = buffers.get()
    if not buffer {
        var fresh_buffer = List<String>()
        buffers.set(fresh_buffer)
    }
    var own = buffers.get()
    crash own
    own.append("spawn")
    own.append("move")
    totals_lock.while_locked(count_two)
    return 2
}

func count_two() {
    commands = commands + 2
}
```
```output
16 commands recorded, 16 counted under the lock
```

A lock of your own is rarely needed for a singleton: one of the program's own that a `Parallel` reaches is made
safe by the compiler. `HitCounter` below only counts, so each `record()` becomes one
atomic add, with no lock and nothing written in the source:

```gdscript title=shared_counter/hit_counter.spite
singleton

var hits = 0

func record() {
    hits = hits + 1
}
```
```gdscript title=shared_counter/shared_counter.spite entry
var console = Console()
var counter = HitCounter()

func SharedCounter() {
    var first = Parallel(record_many)
    var second = Parallel(record_many)
    var own: Integer = record_many()
    console.print(first + second + own, "recorded,", counter.hits, "counted")
}

func record_many(): Integer {
    var index = 0
    while index < 1000 {
        counter.record()
        index = index + 1
    }
    return index
}
```
```output
3000 recorded, 3000 counted
```

A singleton that takes a lock holds it for the whole of each call, so one of its functions must not wait for a
`Parallel` whose work calls back into the same singleton: the work would wait for the lock, and the function holding
it would wait for the work, forever. When the compiler can see it (the `Parallel` made and read, or dropped, which
waits, inside the locked function, and its work calling a locked function of that singleton) it is an error
naming both functions:

```gdscript title=locked_wait_doc/remover.spite
var columns = Columns()
var last = 0

func Remover(new_last: Integer) {
    last = new_last
}

func run(): Integer {
    var entity = 0
    while entity < last {
        columns.remove_row(entity)
        entity = entity + 1
    }
    return last
}
```
```gdscript title=locked_wait_doc/columns.spite
singleton

var removed = List<Integer>()

func despawn_all(last: Integer): Integer {
    var remover = Remover(last)
    var removing = Parallel(remover.run)
    var done: Integer = removing
    return done
}

func remove_row(entity: Integer) {
    removed.append(entity)
}
```
```gdscript title=locked_wait_doc/locked_wait_doc.spite entry error
var console = Console()
var columns = Columns()

func LockedWaitDoc() {
    var removed = columns.despawn_all(10)
    console.print(removed)
}
```
```diagnostic
'Columns.despawn_all' waits for 'Parallel(remover.run)' while it holds Columns's lock
```

Start the `Parallel` and read it in the class that asks for the despawn, and let only the work call `Columns`.

### What a singleton's lock costs

The lock is a word of its own on a cache line of its own, taken with one compare-and-swap and let go with one
store, re-entered for free by the thread that holds it. Taken by one thread only, it costs about 2 ns a call:
800 000 calls to a locked function took 1.7 ms on one thread against 0.4 ms with the lock taken once for the
whole loop (`benchmarks/singleton_locks`).
Taken by several threads at once it costs far more, because each call hands the line from core to core and the
threads waiting spin: eight workers making 100 000 calls each to one shared singleton took 51 ms, against 2 ms for
the same calls on one thread. So:

- **Give each worker its own singleton** where the state splits (a generic singleton per column, `Column<T>`,
  rather than one `Columns` every worker calls), and the locks are never contended.
- **Call it from a counted loop.** A `while index < count` loop whose calls are all to one singleton (and to lists
  of plain values), with nothing else locked or waited on in it, takes the lock once around the whole loop and
  calls the unlocked bodies inside it ([optimizations.md](optimizations.md#a-counted-loop-of-calls-to-one-singleton-takes-its-lock-once)):
  the eight workers above then take 0.6 ms for the shared singleton and 0.35 ms for their own, as if unlocked.
- **Keep its reads apart from its writes.** A function that only reads the singleton's state (`at(row)`, a lookup)
  takes the readers' side of the lock: a count on its own thread's cache line, so readers on different threads
  never slow each other down, and only a function that writes waits for them
  ([optimizations.md](optimizations.md#a-singletons-reading-functions-do-not-exclude-each-other)). Eight systems
  reading 15 000 rows each of one column through `at(row)` went from 202 ns to 6 ns a row
  (`benchmarks/singleton_reads`). Anything a function calls or assigns beyond reading makes it a writing function,
  so keep lookups small and separate from the functions that change the singleton.
- **Nothing to do between stages.** While no task is on the thread pool, a locked function runs without its lock
  (19 ns to 9 ns a call, `benchmarks/singleton_unshared`), so code that fills or flushes a singleton on the
  program's own thread between `Parallel`s pays almost nothing for it.
- **Or let it take no lock.** A singleton whose changing state is one counter or flag per function becomes atomics,
  one that never changes takes nothing, and one no `Parallel` reaches takes nothing
  ([the forms](optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form)).

A `ThreadLocal` keeps every thread's value until it is dropped itself, so a thread that ends does not take its
value with it. `get()` takes no lock and costs the same with one thread or thirty; `set` takes the lock briefly.
Both classes are the operating system's own (`TlsAlloc` and `SRWLOCK` on Windows, `pthread_key_t` and
`pthread_mutex_t` elsewhere), reached through each system's folder, and a program that makes neither carries
neither.

### A number every thread shares: `Atomic<T>`

A counter or a flag that several threads change without a lock is an `Atomic<T>`, for a whole number or a
`Boolean`: `read()`, `write(value)`, `add(amount)`
(answering the value after the add), `exchange(value)` (answering the one before) and
`compare_and_swap(expected, desired)` (answering whether it held `expected` and now holds `desired`). Each is one
atomic instruction, sequentially consistent, with no lock and no call, and a `Parallel` may reach one like a
`Lock`:

```gdscript title=atomic_counter/atomic_counter.spite entry
var console = Console()
var jobs_done = Atomic<Integer>(0)

func AtomicCounter() {
    var first = Parallel(finish_jobs)
    var second = Parallel(finish_jobs)
    var finished = first + second
    var counted = jobs_done.read()
    var claimed = jobs_done.compare_and_swap(200, 0)
    var now_held = jobs_done.read()
    console.print(finished, "finished,", counted, "counted, reset:", claimed, now_held)
}

func finish_jobs(): Integer {
    var index = 0
    while index < 100 {
        jobs_done.add(1)
        index = index + 1
    }
    return index
}
```
```output
200 finished, 200 counted, reset: true 0
```

`add` halts when its answer does not fit the type, as `+` does, in every build. An `Atomic` of anything else (a
`Float`, a `String`, an object) and `add` on an `Atomic<Boolean>` are compile errors where the program calls them
([failure.md](failure.md#crash)): share an object through a `Lock`, or give each thread its own.

A program that makes a `Parallel` (or a `Concurrent`, or a `ForeignCallback` C may call from a thread of its own
([foreign_libraries.md](foreign_libraries.md#calling-back-into-spite)), or is built with `--repl-port` or
`--hot-reload`) counts references with atomic operations, because an object can now be shared between threads; every other program keeps
the plain, cheaper counts. `Parallel(work)` is checked while compiling: the function it
runs, and every function of the same class that function calls by name, may read and write only the attributes of
its own instance that hold values (numbers, `Boolean`, enums, text), its locals and parameters, singletons (which
the compiler makes safe) and a `Lock`, `ThreadLocal` or `Atomic`, which are made to be shared. An attribute holding a
list, a dictionary or another object is an error naming it, since another thread may hold the same object:
`'Parallel(tally.count_up)' runs 'record' on another thread, so it may reach only the attributes of its own
'Tally' that hold values, its locals and singletons: 'marks' holds a List<Integer>, which another thread may
share` (`diagnostics/parallel_function_reach`, where the maker kept `tally.marks` for itself). Make what the work
needs a local, or give the work an object of its own whose attributes are values, as each `Summer` above is. The
check is the one `parallel_each_` uses, and costs nothing at run time.

An object made for the work and **handed over** may keep more: when it is made on its own
line in the same block as the `Parallel(...)`, nothing else is given it before, and the code that made it never
touches it again, its task may keep lists of values and objects it made itself. A `Crafter` can collect its recipe
ids as a `List<Integer>` and its names as a `List<String>`, with no text joined to smuggle them across:

```gdscript title=handed_over_doc/crafter.spite
var first = 0
var recipe_ids = List<Integer>()
var names = List<String>()

func Crafter(starting_first: Integer) {
    first = starting_first
}

func want(recipe_id: Integer) {
    recipe_ids.append(recipe_id)
}

func craft(): Integer {
    var index = 0
    while index < recipe_ids.count() {
        var recipe_id = recipe_ids[index] + first
        names.append("recipe {recipe_id}")
        index = index + 1
    }
    return names.count()
}
```
```gdscript title=handed_over_doc/handed_over_doc.spite entry
var console = Console()

func HandedOverDoc() {
    var crafter = Crafter(100)
    crafter.want(1)
    crafter.want(2)
    var crafted = Parallel(crafter.craft)
    console.print("crafted", crafted)
}
```
```output
crafted 2
```

Reading `crafter` after the `Parallel` line would be `'crafter' was handed to 'Parallel(crafter.craft)', which
keeps its 'recipe_ids', 'names' on another thread, so it is not used after that line: make another 'Crafter' for
what follows, or read the task's result` (`diagnostics/parallel_handover`; [the rules](#concurrency-concurrent-parallel-and-hidden-waiting)).

## `parallel_each_`: a member on every element

`list.parallel_each_update()` calls `update()` on every element of the list, split across the pool: the list is
cut into a few pieces per thread, the calling thread runs the first piece and whatever piece no worker has taken
yet, and the call returns when every element is done. `filter_` steps before it run in the same pass, piece by
piece, with no list in between: `entities.filter_alive().parallel_each_update()`.

```gdscript title=parallel_pass/particle.spite
var position: Long = 0
var speed = 0
var awake = false

func Particle(starting_speed: Integer) {
    speed = starting_speed
    awake = starting_speed % 3 == 0
}

func step() {
    position = position + speed
}
```
```gdscript title=parallel_pass/parallel_pass.spite entry
var console = Console()

func ParallelPass() {
    var particles = List<Particle>()
    var index = 0
    while index < 3000 {
        var particle = Particle(index)
        particles.append(particle)
        index = index + 1
    }
    particles.parallel_each_step()
    particles.filter_awake().parallel_each_step()
    var moved = particles.sum_position()
    console.print("the particles moved", moved)
}
```
```output
the particles moved 5997000
```

**The member may reach only its own element.** Every element runs at the same time as the others, so the
compiler reads the member (and every function of the element's class it calls) and allows only the element's
own attributes that hold a value (a number, a `Boolean`, text, an enum or a `Symbol`), any singleton and the
member's locals. A singleton of the program's own that can change is made safe for you in a program that uses
`Parallel`: nothing when it is read-only, atomics when it only counts or flags, a
lock otherwise ([optimizations.md](optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form)). So a
`Parallel` or a pass may call it while the program's thread does too, and you write nothing. An attribute holding another object
may be shared by several elements, so reading it is an error that names it:

```gdscript title=parallel_reach/boid.spite error
var speed = 1
var leader: Boid? = null

func follow() {
    assert leader
    speed = leader.speed
}
```
```gdscript title=parallel_reach/parallel_reach.spite error entry
var console = Console()

func ParallelReach() {
    var boids = List<Boid>()
    var boid = Boid()
    boids.append(boid)
    boids.parallel_each_follow()
    var count = boids.count()
    console.print(count)
}
```
```diagnostic
'parallel_each_follow' runs 'follow' on many elements at once, so it may reach only its own 'Boid' attributes that hold values, and its locals: 'leader' holds a 'Boid?', which another element may share
```

For the same reason only `filter_` steps may come before it: a `map_` reaches another object. The check cannot
see two elements that are the same object: a list holding one instance twice runs it on two threads at once.

A pass costs one allocation per call and none per element, and a list of fewer than two elements runs on the
calling thread without touching the pool.

## The REPL answers at the waits

A `--repl-port` build (see [repl.md](repl.md)) serves its commands **on the program's own thread, at the same
waits**. The socket thread only reads a command and hands it over; the scheduler answers it the next time the
program waits, and hands the answer back. Every command therefore sees the program between two steps, never in
the middle of one: a frame loop that ends in `program.sleep` is answered between frames, a tool waiting on
`Console.read_line` is answered while it waits, and once the entry constructor returns, the program waits for
nothing but commands until a client sends `exit`. None of this is written by the program.

```gdscript title=frame_loop/frame_loop.spite entry
var console = Console()
var program = Program()
var frame = 0
var running = true

func FrameLoop() {
    while running and frame < 200 {
        frame = frame + 1
        program.sleep(10)
    }
    console.print("stopped")
}
```
```output
stopped
```

This session runs against the program above while its loop runs:

```wire frame_loop
$ spite connect 4000 --command="program.running"
{"ok":true,"value":"true","type":"Boolean"}

# Answered between two frames: the loop sees the change at its next test.
$ spite connect 4000 --command="program.running = false"
{"ok":true,"value":"false","type":"Boolean"}

$ spite connect 4000 --command="exit"
{"ok":true,"value":"","type":""}
```

A program that never waits is answered too. In a `--repl-port` or `--hot-reload` build, and only there, the
compiler ends every pass of every `while` in the program's own code with a **check point**: one load of a flag
that the REPL's thread (or the file watcher) raises when a command (or a changed file) is waiting; only then does it
answer it there, on the program's thread, between two passes of the loop. A busy loop is served between two passes the way a frame loop is served between
two frames:

```gdscript title=busy_loop/busy_loop.spite entry
var console = Console()
var passes: Long = 0
var running = true

func BusyLoop() {
    while running and passes < 300000000 {
        passes = passes + 1
    }
    console.print("stopped after at least one pass:", passes > 0)
}
```
```output
stopped after at least one pass: true
```

```wire busy_loop
# The loop never waits: the command is answered at the end of a pass.
$ spite connect 4000 --command="program.running = false"
{"ok":true,"value":"false","type":"Boolean"}

$ spite connect 4000 --command="exit"
{"ok":true,"value":"","type":""}
```

A build without those flags has no check points at all, and its C is byte for byte what it would be if check
points did not exist. The standard library's own loops have none either, so a command waits
for a library call to return.

## What it costs

Everything on this page is chosen per program, from what the program uses, and a program that uses none of it
carries none of it; the compiler's side of each is on
[optimizations.md](optimizations.md#concurrency-machinery-only-where-it-is-used).

| The program | Carries at run time |
|---|---|
| uses none of it | nothing: no scheduler, no state machine, no helper thread, no pool, no lock, plain reference counts; every wait is the plain blocking call |
| makes a `Concurrent`, or has two reads in a row | the `Scheduler` singleton and its event loop; a second, resumable copy of each function a `Concurrent` reaches that waits (the plain copy is dropped when nothing calls it); one heap frame per waiting call made inside a `Concurrent`; one short-lived operating-system thread per blocking call made while something else could run; atomic reference counts everywhere; one count the scheduler adds to at each read of `finished` that answers `false` and clears at each step |
| makes a `Parallel` or runs a `parallel_each_` pass | the `ThreadPool` singleton, whose workers start at the first `Parallel` and never again; about two dozen allocations per `Parallel` and one per pass; atomic reference counts everywhere; for each program singleton a `Parallel` can reach and that changes, atomics or a lock per call (about 2 ns when one thread takes it, far more when several contend: [what it costs](#what-a-singletons-lock-costs)), or one per counted loop of calls |
| calls `Scheduler().resume_only_when_asked()` or `run_ready()` | one `Boolean` the scheduler reads at each wait, and each function it calls; neither is there otherwise |
| makes a `ThreadLocal` or a `Lock` | one system per-thread slot or lock each, freed with it |
| is built with `--repl-port` or `--hot-reload` | the scheduler, and one check-point call at the end of every pass of every loop of its own code; nothing of either in any other build |

What is never there: a stack per `Concurrent`, a stack switch, a runtime that a WebAssembly build could not carry.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. Where the teaching above and these rules disagree,
the rules win.

### Concurrency: `Concurrent`, `Parallel` and hidden waiting

No function is coloured, and dropping a handle joins. The compiler injects the REPL's drain points where the program
already waits. IO never blocks the program by the program's own hand. Async waiting and threads are two different
things, so they are two classes, `Concurrent` and `Parallel`, and one scheduler.

- **`Concurrent(function)`** (`library/concurrent.spite`) runs a function value as a **state machine**
  the compiler writes (below), on the program's own thread, starting straight away; reading the handle is what waits for its value (below) and
  `drop()` waits for it, so scope exit is a join point. It is for work that waits: IO, sleeps, database calls.
- **`Parallel(function)`** (`library/parallel.spite`) runs one on the program's **thread pool** (below), with
  the same reading and join on drop. It is for work that computes.
- The type is never written: `Concurrent(file.read)` is a `Concurrent<String?>`, worked out from the function's
  return. Writing it on the `var` that makes the handle is an error,
  `the type of a 'Parallel' handle is never written: it is worked out from the function it runs, so write 'var
  total = Parallel(count)'` (`diagnostics/written_handle_type`); where no function is there to infer from (the
  element type of `List<Parallel<Integer>>()`, a parameter) it is still written: the rule covers only the declaration
  that starts the work. A function that returns nothing gives a `Concurrent<Nothing>`,
  which has no value to read and is waited for by dropping it: keep such handles in a list, and clearing the list
  or leaving its function waits for all of them.
- **Every attribute of both classes is private** (`_work`, `_results`, `_state`, ...), and the one public member is
  `finished: Boolean` (below). Nothing outside the class reaches the pool's job, the state machine or the work.

**A concurrent result joins on first use.** There is no `wait()` and no `join()`: **the handle stands in for its
result wherever the result's type is expected, and the compiler inserts the join at each such use** (the first
one waits; the value is kept, so later ones do not). Exactly, a `Concurrent<T>` or `Parallel<T>` becomes its `T`:

- where it is stored or passed as a `T`: a `var` whose type is written, an assignment, an argument (a variadic
  `Printable` one included, so `console.print(sum)` prints the value), a `return`, an element of a list literal;
- as an operand (`+`, `==`, `and`, `not`, any operator) and inside text, `"total {sum}"`;
- as the receiver of a member the handle does not have: `greeting.upper_case()`, `greeting.length()`;
- as a condition: `if ready`, `while`, `assert`, `crash`; and when `T` is nullable, narrowing the handle narrows its
  value (`if reading { use(reading) }`, `crash reading`), because narrowing applies to a name itself rather than a copy.
  The value is read once, into a hidden local, and the narrowed name reads that. A handle whose `T` is `Boolean?` is refused as a
  condition as any `Boolean?` is, since it would test only that a value came back: compare it, `if ready == true`.

It stays a handle where a handle is expected (`List<Parallel<T>>.append(handle)`), in a `var` without a written
type (`var loading = Parallel(asset.load)` is the running work), and for the handle's own members, `finished` and
`finished_value()`, and `class`, `attributes` and `functions`, which every object has, so a member template over a
`List<Parallel<T>>` reads the handles (`runs.all_finished()`, `conformance/stage6/kept_templates`). A
`T` of `Nothing` is never read, so such a handle only joins on drop. Comparing two handles with `==` compares their
values; there is no way to compare the handles themselves. The compiler writes
each join as a call to the class's private `_result()`, which `library/concurrent.spite` and
`library/parallel.spite` declare in Spite; only a join the compiler inserted may call it.

**`finished` never waits.** `handle.finished` is `true` once the function has
returned. On a `Parallel` it reads the job's state with one atomic load; on a `Concurrent` it reads the flag its
state machine's frame carries once it finishes, so a state machine only makes progress when the program waits
somewhere (`while not reading.finished { program.sleep(1) }` is the polling loop). The same on every system
(`conformance/stage6/finished_polling`).

**`finished_value()` takes a finished result without waiting.** `handle.finished_value(): T?` answers the value when the work has finished and `null` otherwise, on a
`Concurrent` and on a `Parallel`: `finished`, then the value, in one call (`library/concurrent.spite`,
`library/parallel.spite`). It is never a wait point, so `is_resumable` answers `false` for a function
whose only handle read is `finished_value()`, while reading the handle as its value still waits and still counts:
an engine system that only collects finished work is not taken for one that does IO. It costs what `finished`
followed by a read costs, and a program that never calls it carries none of it
(`conformance/stage6/finished_values`).

**A handle's debug text never waits.** `Concurrent` and `Parallel` have their own `to_debug()`: `running` while the work runs,
otherwise the result's own debug text (through `finished_value()`), so `console.debug` of a class holding a
`Parallel<Socket?>?`, and a crash report that shows one, neither join the work nor forward `to_debug` to a
`Socket?` that may be null (`conformance/stage6/debug_handles`).

**A handle made at its defaults started nothing, so it is finished and joins nothing.** Reflection makes one: describing a class whose function takes a `Concurrent<T>` or a `Parallel<T>`
describes that handle's class too, from a stand-in at its defaults ([reflection.md](reflection.md)). Such a handle
has no frame and no pool job: `finished` is `true`, `finished_value()` is `null`, and dropping it waits for nothing
and frees nothing. A `Lock` or `ThreadSlot` made that way likewise gives back no lock or key it never took. Nothing
is added to pay for it: a `Concurrent` starts with its `finished` flag set and clears it only when it starts a
frame, and a `Parallel` allocates its job's state only when it submits the job
(`conformance/stage6/default_handles`, which describes a function taking each of the four).

**The thread pool.** `library/thread_pool.spite` is a singleton, `ThreadPool()`, that the `Parallel`s share.
It is the program's one pool: any code hands work to it by `Parallel(function)` and gets a handle whose `finished`
never blocks and whose value joins on first use; no submission starts an operating-system thread of its own, and
there is no second pool to create or pass around.

- **Work that never finishes.** For each `Parallel(function)` in the program's own code (not `library/`), every
  function the work reaches through calls whose class is known is read for a `while true` with no `return`,
  `assert` or `crash` anywhere inside it. Such a loop in a function that never waits (none of its calls reaches a call that waits and gives
  its thread back, such as a socket read) is the error at the loop, once per function:
  `'Parallel(<work>)' runs '<Class>.<function>' on the thread pool, and this loop never ends and never waits, so the
  work never finishes and keeps its thread for good: return from the loop once the work is done, or run the loop
  on the program's own thread` (`diagnostics/endless_parallel_work`). A function that waits anywhere is left alone,
  as is a loop that can leave. Checked while compiling; nothing runs for it.
- **Size and start.** The first `Parallel` starts one worker thread for every core but one
  (`GetActiveProcessorCount`, `sysconf`), at least one; the program's own thread keeps the last core. It never
  starts another, and a program that makes no `Parallel` starts none. `size()` says how many; `worker_index()`
  answers `0` to `size() - 1` on a worker (read from the thread's identity, under the queue's lock) and `-1`
  elsewhere, for scratch memory kept per worker.
- **The queue.** One lock and two condition variables (`SRWLOCK` and `CONDITION_VARIABLE` on Windows,
  `pthread_mutex_t` and `pthread_cond_t` elsewhere, each system's folder reopening the class). A job is a
  `Spite.Function<Integer, Integer, Nothing>` with the two numbers it is given and the address of an 8-byte state its
  owner holds (queued, running, done). Workers take jobs in order; `finished` reads the state with an atomic load.
- **Joining claims.** Waiting for a job that no worker has taken yet takes it out of the queue and runs it on the
  waiting thread, so a job that starts and waits for another job (a `Parallel` inside a `Parallel`, or a
  `parallel_each_` inside one) cannot wait on workers that are all waiting for it. A job already running is waited
  for on the done condition. This is also the only waiting `ThreadPool.join` does, and it is the wait the scheduler
  wraps (below).
- **Join on drop is kept.** The queue holds the job's function bound to a small `ParallelCall<T>` that owns the
  work and the result, never the `Parallel` itself, so dropping the last handle runs its `drop()` straight away,
  and that waits.
- **At exit** the pool is a singleton destroyed in reverse creation order: it lets the workers finish what
  is queued, joins them and frees its lock. A `Parallel` whose `drop()` runs later finds its job done and does not
  touch the pool.
- **What the compiler supplies.** Starting a worker needs the address of a C function that calls the pool's
  private `_serve()`: `ThreadPool` declares two members without a body that the generator writes,
  `entry_address()` and `address()`. `Concurrent` has three more, `_start_frame()`, `_frame_result()` and `_free_frame()` (the frame lives until the handle is dropped, so a second join still waiting on it reads it safely), and
  `Scheduler` has `step_frame(frame)` and `release_work(frame)`, whose bodies are a line of C in the compiler. `--final-classes` prints each
  declaration. The rule is that no compiler-supplied function stays bodiless and no Spite body holds C, so each of
  these is Spite over the backend's primitives. Everything else (the queue, the claim, the split) is Spite.
- **Cost.** A `Parallel` makes about two dozen allocations (25 under `--debug-memory`), most of them the two
  function values (a `Spite.Function` is its own reflection object), and no thread.
  `conformance/stage6/thread_pool_reuse` runs a thousand `Parallel`s and checks every one ran on one of the pool's
  workers or on the thread that read it.

**A value per thread, and a lock.** Three small
classes, each system's folder supplying the calls, and each carried only by a program that makes one:

- **`Lock()`** (`library/lock.spite`): `while_locked(work: Spite.Function<Nothing>)` runs the function holding the
  lock, so the unlock cannot be forgotten; `lock()` and `unlock()` stay for a section that is not one function.
  `SRWLOCK` on Windows, `pthread_mutex_t` elsewhere; `drop()` frees it. Not reentrant.
- **`ThreadSlot()`** (`library/thread_slot.spite`): one `Long` per thread, `0` until written (`TlsAlloc` or
  `pthread_key_create`). `read()`, `write(value)`; `drop()` gives the key back.
- **`ThreadLocal<T>()`** (`library/thread_local.spite`): a value of any type per thread. Its `ThreadSlot` holds
  each thread's position in an array of `T` the `ThreadLocal` owns, so values stay reference counted
  and are all released when the `ThreadLocal` is dropped, whichever threads set them and whether or not those
  threads still run. `get(): T?` is `null` on a thread that has not called `set(value)`. `get()` takes no lock: the
  array's address is read with one atomic load. `set` takes the `Lock`; a thread's first `set` may grow the array,
  copying it into one twice the size and publishing that with an atomic store, and the arrays it replaced are freed
  with the `ThreadLocal`, so a thread still reading one reads its own unchanged value there.

- **`Atomic<T>`** (`library/atomic.spite`): a whole
  number or a `Boolean` that threads share, in an 8-byte cell of its own that the `Atomic` frees when it is dropped.
  `read()`, `write(value)`, `add(amount)`, `exchange(value)` and `compare_and_swap(expected, desired)` are
  the primitives `read_long_atomically`, `write_long_atomically`, `add_long_atomically`, `exchange_long` and
  `compare_and_swap_long` on that cell, each one sequentially consistent atomic instruction; a narrower `T` is
  stored widened and read back at its width; `add` works its answer out in `T` from the value the cell held, so an
  answer that does not fit halts like `+` (`Atomic<Byte>(250).add(10)` halts, `add(5)` is `255`), and
  `compare_and_swap` compares the value as a `T`, retrying when another thread changed the cell's upper bits in
  between. The constructor holds a `crash` on `$value_type` and `add` one on `Boolean`, which fold, so a wrong `T`
  is a compile error where the program makes it (`diagnostics/atomic_misuse`). A `Parallel` may reach an `Atomic`
  attribute, as it may a `Lock` (`conformance/stage6/atomic_values`). A program that makes none carries none of it.

A value per pool worker (`worker_index()` into a list) needs no system call but covers only the pool's workers,
not the program's thread or a `Concurrent`'s helpers. `conformance/stage6/thread_locals`,
`conformance/stage6/thread_local_growth`.

**The mechanism: compile-time state machines, and a helper thread per blocking call.** The aim is no runtime, only
compile time. A C target has no coroutines, so the compiler writes them. Every function that can
reach a wait from inside a `Concurrent` is compiled twice: the plain function, for code outside a `Concurrent`, and
a resumable version (`bootstrap/source/generation/state_machine.spite`):

- **Which functions.** A function *waits* when it is one of the waits below or calls a function that waits: by
  name, through a constructor, through a union's or a `type`'s function that waits for one of its classes, or
  through a function value of a signature some function made into a value with that signature waits for (found
  over the calls the plain bodies make, to a fixpoint). A `Concurrent`'s function gets a state machine when it
  waits and is a function value made in an argument of `Concurrent(...)`, or of a program class; every function a
  state machine calls that waits gets one too, constructors included. A singleton's constructor, a function of a
  singleton that takes the singleton's lock (only in a program a `Parallel` reaches it in, since a lock held across
  a point the state machine returns from would let another `Concurrent` on the same thread into it), and every
  function of a program class in a `--hot-reload` build (called through a slot a reload swaps) get none.
- **The frame** is a C struct on the heap: a header (the step function, the wait it stopped at, whether it runs or
  has finished, and, for a `Concurrent`'s own frame, the function value it runs), the result, `self`, the parameters, every local and temporary of the body (a name declared twice in
  nested blocks gets two fields), and one slot per wait for the frame it waits on.
- **A `Concurrent` holds its function only while it runs.** The handle lets go of
  the function value as soon as the work has started: its frame holds it instead, and the scheduler releases it
  when the frame finishes (work with no state machine has already finished by then). A class that keeps
  `Concurrent(drain)` of its own function in a list is therefore a cycle only while that work runs, not after, even
  when nobody reads `finished` again (`conformance/stage6/finished_workers`). It costs one pointer in each frame's
  header and a retain and a release per `Concurrent`.
- **The step function** runs the body with every local read from the frame, starts with a jump to the wait it
  stopped at, and answers `true` when the body returns and `false` at a wait that is not over. Jumping into a
  `while` or an `if` needs nothing, since no local lives on the C stack.
- **A wait** written as a call to a function that waits, anywhere an unconditional value is computed (a
  statement, a `var`, an argument, an operand, a `{...}` in text, a condition), becomes: hold the receiver, make
  the callee's frame with the arguments, step it until it answers `true` (returning `false` from this step while it
  does not), take its result and free it. It runs before the rest of the statement it is written in (a C statement
  expression cannot be jumped back into). A `while` whose condition waits becomes a loop that waits at the top of
  every pass and leaves when the condition is false. A call inside a template is a call by name like any other:
  `system.phase_each(made_arguments())`, whose arguments a plural template makes, holds the receiver and then
  makes each argument in order into the frame before the wait, and a helper the template passes its symbol to,
  `nap_again(phase)`, waits the same way (`conformance/stage6/template_argument_wait`).
- **Waits in a condition, a comparison, or behind a choice made while running** are points the state machine
  returns from too, so two waits never hold each other on the C stack:
  - The right side of `and`/`or`, and every side of an `if`'s conditions after the first, is computed inside a C
    `if` on what came before, with its waits inside it: `bool decided = left; if (decided) { ...waits...; decided =
    right; }`. A jump back into the `if` needs nothing more.
  - `==` and `!=` on a nullable left side whose right side waits keep the left side, wait for the right side once,
    then compare the two kept values.
  - A call through a function value makes the frame of whichever function the value holds: a small function per
    signature compares the value's function with each function made into a value with that signature that has a
    state machine, and makes its frame, or answers none, and the value is then called plainly. A union's or a
    `type`'s function does the same by class.
  - A constructor that waits makes the object, then steps the constructor's own state machine on it.
  - A local that holds a `Concurrent`, or a list of them, waits at the end of its scope for each handle it holds the
    last reference to, before letting it go; `clear()` on a list of `Concurrent`s waits for each of them first. The
    drop that follows then finds the work finished.
  What still waits by running the event loop where it is (holding its `Concurrent` while every other state machine
  goes on): dropping a `Concurrent` in any other way inside a `Concurrent` (the last reference held by an object,
  an attribute or an element of a list that is not a local's, or by a local that is assigned again), a wait reached
  through a function value or a `type` whose function has no state machine (a value of a value class's function, or
  of a `type`'s function itself), the waits of a function that has none (above), and a wait inside the values a
  `crash` or `assert` report prints. Two such waits that each wait for the other could never end, so a join that
  waits in place for a `Concurrent` whose state machine is running further down the same stack halts at the join
  (`waits_for_its_own_caller=true`, `conformance/stage6/concurrent_wait_cycle`); a join that is a point to return
  from just waits, and the machine below it carries on. A `Concurrent` whose function has
  no state machine runs it to the end when it is made. `is_resumable` answers `true` for a function whose only wait
  is one of these, since it does wait. `conformance/stage6/waits_never_hang` starts a `Concurrent` through each form
  and finds it unfinished straight after, and joins, through a function value, work that a wait below it finishes.
- **A wait inside an expression keeps the written order.** What the expression computes before the wait (the
  left operand of an operator, the earlier pieces of a text, the earlier arguments of a call, the receiver of a
  call, the left side of an `and`/`or` or a comparison) is computed before it, into a temporary in the frame, and
  what comes after it is computed after it. A reference read that way is held for the wait and let go once the
  expression is done, so a change another `Concurrent` makes during the wait (assigning the attribute anew, say)
  neither frees it nor is seen by it. A local, a parameter, a constant, `self` or another wait's result is not
  copied, since nothing can change it while the state machine waits. An assignment works out its value before its
  target. `conformance/stage6/wait_order`.
- **The waits at the bottom** are small state machines the generator writes: `Program.sleep` registers a deadline
  and is over when the clock passes it; `Console.read_line_into`, `File.read_into`, `File.write_text`,
  `File.write_from`, `Socket.accept_handle`, `Socket.receive_into` and `Socket.first_readable` start their one system call on a helper thread
  and are over when it flags that it returned; `Scheduler.wait_for(frame)`, under reading or dropping a
  `Concurrent`, is over when that frame has finished.

Files cannot be waited on without blocking on any of the three systems, so a blocking call is handed to a
short-lived helper thread (what libuv does for files). The alternative, each system's own readiness (IOCP, epoll,
kqueue), is not used: an ordinary file is always "ready" to epoll and kqueue, the Windows console cannot be read
through IOCP, and one mechanism keeps each system's folder small.

**What the compiler writes for code outside a `Concurrent`.** Each operating system's folder names the calls that
block, and the compiler knows them by class and function (the list above, and `ThreadPool.join`, the wait under
reading or dropping a `Parallel`). In a program that uses the scheduler (one that makes a `Concurrent`, or is
built with `--repl-port` or `--hot-reload`) each of them is emitted under a `_waiting` name with a small wrapper
in front: a sleep runs the event loop until its time, a blocking call runs on a helper thread while the event loop
runs, and a `Parallel` join joins and then runs whatever is ready once (none of which steps a state machine after
`resume_only_when_asked()`, below). Every other program gets none of it: no
wrapper, no scheduler, no state machine, the same C as before.

**Blocking is what the compiler picks when it is faster.** The wrapper asks the scheduler first: when no
`Concurrent` is alive and no REPL is listening, or the caller is not on the scheduler's thread (a `Parallel`, a
helper, the REPL's socket thread), nothing else could run meanwhile, so the wrapper makes the plain blocking call.

**The scheduler** (`library/scheduler.spite`, a singleton; each operating system's folder reopens it with the
thread, event and clock calls) keeps the frames of the `Concurrent`s that have not finished, the deadlines of the
sleeps in them and a count of helper threads in flight. Its loop answers the REPL's pending command and a pending
reload, if any, steps every frame that is not already running further down the C stack, and, when none finished,
waits on one event (an auto-reset event on Windows, a pipe read with `poll` elsewhere) that the helper threads
and the REPL's socket thread signal, with the nearest deadline as its timeout. Waiting forever on nothing is a
deadlock, and a crash.

**Where `Concurrent`s resume.** An engine runs IO tasks between frames, so `Scheduler` has two public
functions for a program that wants `Concurrent`s to resume only between its own stages:

- **`run_ready(): Integer`** steps once every `Concurrent` frame that is not already running, and answers how many
  finished. It never blocks. On any thread but the scheduler's, and before any `Concurrent` exists, it does
  nothing and answers `0`, so a `Parallel` cannot touch the scheduler's frames through it.
- **`resume_only_when_asked()`** holds for the rest of the run. A wait
  on the program's thread outside a state machine then steps no frame: with no REPL listening, the waiting
  wrapper makes the plain blocking call; in a `--repl-port` build the wait still answers commands; after joining a
  `Parallel`, nothing ready runs. A wait that runs the loop in place inside a state machine (the list above) steps
  no other frame either. Frames are then stepped only by `run_ready()` and by joining a `Concurrent` outside a
  state machine (reading its value or dropping it), which steps every frame until that one has finished,
  because it may be waiting on another `Concurrent`: a join never waits for a loop the program has stopped.
  Inside a state machine a wait is a point it returns from, as before.
- **A poll that can never see the end halts.** Every read of `finished` that answers `false` counts, and stepping
  any `Concurrent` (`run_ready()`, a join, or a wait that resumes them) sets the count back to zero. Between two
  steps a `Concurrent` cannot move, so a loop that polls `finished` without stepping one, under
  `resume_only_when_asked()` or with no wait in it at all, could only spin forever: at a million polls in a row
  it halts with `spite.crash ... library/scheduler.spite:<line> Scheduler polled_unfinished
  unfinished_polls_with_no_frame_stepped=1000000` (`check.sh`, "polling").
- `conformance/stage6/frames_between_waits`: an engine-shaped loop whose stages join a `Parallel`, sleep and
  write to a socket while a `Concurrent` reads that socket with `read_bytes` three times; the reader resumes only
  at `run_ready()`, never inside a stage. Without `resume_only_when_asked()` the same program's reader resumes
  inside a stage's sleep.

**Soundness.** A program that starts a thread (a `Concurrent`'s helpers, a `Parallel`, or `--repl-port`), or hands C
a `ForeignCallback` it may call from one, is compiled with `SPITE_THREADS`: every retain and release is an atomic operation, and the `--debug-memory` table takes a lock.
Every other program keeps the plain counts. Spite code on the program's thread only ever changes hands at a wait,
so state machines need nothing more. A singleton first used from two threads at once is made once: the fetch takes
the singleton's own lock only while its slot is empty (`conformance/stage6/singleton_race`). The race rule is checked
for `parallel_each_` (below) and for a `Parallel(function)` (above): neither may reach a list or object
another thread could hold, though two threads writing one number attribute of a shared instance is still the
program's mistake. A
program's own singletons are made safe (below); the library's are not guarded, so two threads printing at
once may interleave the pieces of their lines. One smaller gap: a REPL client's `exit` while a helper
thread is blocked reading the console may wait on the C runtime's lock on that stream when the process exits.

**`parallel_each_`.** `list.parallel_each_update()` calls `update()` on every element,
split across the pool, and returns when all are done. The generator writes two functions on the list's class, as
it writes a fused chain: `parallel_each_update_piece(first: Integer, end: Integer)`, a `while` over that range of
the buffer, and `parallel_each_update()`, which hands the piece function to `ThreadPool.run_pieces(piece, count)`.
That cuts the list into up to four pieces per thread (the workers and the caller), queues all but the first, runs
the first on the calling thread and joins the rest, claiming any no worker has taken. One allocation per call for
the pieces' states, one function value, and none per element. A list of fewer than two elements runs on the
calling thread. `filter_` steps before it fuse into the piece loop (`entities.filter_alive().parallel_each_update()`
is one pass with no list in between); a `map_` step is an error, since it reaches another object that several
elements may share: `'parallel_each_grow' can follow only 'filter_' steps: 'map_next' reaches another object,
which several elements may share, and a parallel pass may touch only each element's own attributes`.

- **The race rule, as a compile check.** The member, every function of the element's class it calls
  (transitively, by name), and every `filter_` member in the chain may read and write only the element's
  attributes that hold a plain value (a number, `Boolean`, `String`, enum or `Symbol`, or a `T?` of one), any
  singleton (the library's, and the program's own, which the compiler makes safe below, including one bound as an
  attribute of the element), and their own parameters and locals. An attribute holding any other object, a list,
  a dictionary or a function value is an error naming the attribute, its type and the one-thread form
  (`diagnostics/parallel_reach`):
  `'parallel_each_follow' runs 'follow' on many elements at once, so it may reach only its own 'Boid' attributes
  that hold values, and its locals: 'leader' holds a 'Boid?', which another element may share. ...`. A list
  of numbers or text has no members, so `parallel_each_` on one is an error naming `each_`:
  `'parallel_each_to_string' runs a member of each element on many threads, and 'Integer' is not a class: only a
  list of a class has members to run there (write 'each_to_string' to run it on one thread)`.
- **What it cannot see:** one object listed twice runs on two threads at once; a local made from something the
  member was handed (it is handed nothing, so only through a library singleton such as `Memory`); a list of a
  `type` or union (the element's class is not known, so it is an error).
- `conformance/stage6/parallel_each` runs ten thousand elements twice, filtered once, and a list of one and of none.

**A singleton a `Parallel` reaches is made thread-safe by the compiler, in the cheapest safe form.** There is no
keyword and no `shared` header line. In a program that makes a `Parallel` or runs a
`parallel_each_` pass, each singleton of the program's own (not `library/`'s) gets the first of these that is
proven safe for what its functions actually do; the exact conditions are on
[optimizations.md](optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form):

1. **Nothing**, when no function a `Parallel` or the pool can run reaches it (a walk over the calls from every
   function taken as a value and every `parallel_each_`/`filter_` member).
2. **Nothing**, when it never changes after it is made: no function of its own assigns its attributes outside the
   constructor, no other class assigns them, and it holds no list, dictionary, function value or object that can
   change.
3. **Atomics**, when what changes is whole numbers or `Boolean`s and each function touches that state once: each
   read and write is one atomic instruction.
4. **Its own lock**, the fallback: each function that touches its changing state (reads or writes an attribute
   that can change, calls a function of its own that does, or calls out to code that can call back into it) is
   emitted as `<name>___unguarded` behind a wrapper that takes the singleton's lock (reentrant by owner thread,
   padded to 64 bytes), and a function that touches none runs unlocked, so pool work calling it never waits on a
   locked function polling that work (`conformance/stage6/singleton_stateless_calls`); a call it makes to itself
   goes straight to the unguarded body, and a write to its attribute from another class takes the lock too, and so does a read
   of it (`var last = registry.last` in another class loads it under the lock; an atomic singleton's
   attribute is read with one atomic load; `conformance/stage6/singleton_lock_calls`). An attribute holding an
   object that nothing assigns after the singleton is made is read with no lock and no count, since it stays the
   same object for the rest of the program ([optimizations.md](optimizations.md#a-singletons-attribute-that-never-changes-is-read-in-place)).

**A loop that cannot end inside a locked singleton function is a compile error.** A `while true` with no `return`, `assert` or `crash` inside it, in a function of
a singleton that takes the lock (the fourth form above), would hold the lock for good, so every other call on the
singleton would wait forever: `'Window.run' holds Window's lock for the whole call, since a Parallel reaches Window,
and this loop never ends, so every other call on Window would wait forever: move the loop into a class that is not a
singleton, and call Window from it` (`diagnostics/endless_locked_loop`).

**A locked singleton function that waits for work calling back into it is a compile error.** A function of a singleton
that takes the lock (the fourth form above) holds it while it waits, and a `Parallel`'s work runs on a pool thread,
so work that calls a locked function of the same singleton waits for the lock while the function waits for the
work, and the program hangs silently. The compiler refuses it where it can prove it:

- **The wait.** The locked function makes the `Parallel` in a `var` of its own (`var removing =
  Parallel(remover.run)`, at any depth of `if` and `while`) and the handle stays there (it is never passed as an
  argument, assigned to anything, returned, or put in a list literal), so the function reads it (a join, whether
  or not it polled `finished` or `finished_value()` first) or drops it at its end, which waits too.
- **The call back.** What the work runs reaches a function of the same singleton that takes the lock, following
  the call effects through every call whose class is known (`columns.remove_row(entity)` on an attribute bound
  to `Columns()`, a constructor, a function of the work's own class), the work's function itself included when it
  is one of the singleton's.
- **The error** is at the `var` line and names both functions and both ways out: `'Columns.despawn_all' waits for
  'Parallel(remover.run)' while it holds Columns's lock, since a Parallel reaches Columns, and what 'remover.run'
  runs calls 'Columns.remove_row', which takes that lock: each would wait for the other forever. Wait for the
  Parallel outside Columns's functions, or do not call Columns from its work` (`diagnostics/locked_wait`).

What it does not see, and so still leaves to the program: a handle that escapes the function (stored in an
attribute or a list, passed, returned) and is read by another locked function later; a call the call effects
cannot place on a class (through a function value, a union, or a receiver whose class is not known); work that
reaches the singleton only by reading or writing its attributes from outside (those take the lock too);
`parallel_each_` passes, whose members are found by name on every class; and generic singletons (`Column<T>`),
whose call effects do not tell one instance's lock from another's. A `Concurrent` cannot hang this way: it runs on
the thread that waits for it, and the lock is re-entered by the thread that owns it.

**A counted loop of calls to one locked singleton takes the lock once.** The per-call lock is two atomic operations uncontended
(about 2 ns) and a cache line handed between cores contended; a game engine's workload took 44 ms on eight `Parallel`s against
25.5 ms on one thread for 800 000 such calls. A `while` outside the singleton that is counted (`index < bound`,
stepped only by its last line), calls only that singleton's functions through the attribute binding it and a
list of plain values, computes only plain values, cannot return, and reaches no wait nor any other lock through the
singleton's functions it calls, is compiled between `spite_coarse_<n>_enter()` and `spite_coarse_<n>_leave()`
(the singleton's lock), and calls the unlocked bodies. It cannot deadlock: it ends on its own, takes no other lock
while holding this one (the singleton's own functions already take theirs under it), and holds it across no wait,
which is also what the locked-wait check above enforces from the other side. The full conditions and measurements are in
[optimizations.md](optimizations.md#a-counted-loop-of-calls-to-one-singleton-takes-its-lock-once)
(`conformance/stage6/coarse_locks`, `benchmarks/singleton_locks`). The compiler cannot take no lock at all where one
`Parallel`'s work is the only thread touching a singleton while it runs: which other threads run at the same time
is not known while compiling, since any function a `Parallel` reaches may be running on another worker.

**Reading functions share the lock.** A function the lock would wrap that provably changes nothing (its own
locals only, calls only to reading functions of its class, to the reading members of lists, dictionaries, text and
numbers, to `TypedMemory`/`InlineMemory` reads and to reading functions of a singleton that holds no state,
attributes read without a getter, no text holes, operators only on numbers unless the program declares none) takes
the readers' side, when no writing function of the singleton (and no write to its attributes from another class) is
reachable from what a `Parallel` or `parallel_each_` pass runs. That is the shape of a column written between
stages and read by the systems of a stage; one also written from the pool keeps the plain lock, since each write would scan the
counts. `spite_read_enter` adds one to its thread's count (one of 32, each on a cache line of its own) and waits only
while a writing function holds the lock; a writing function takes the lock and then waits for every count to be
zero (`spite_guard_enter_writing`). Reads and writes of its attributes from other classes and counted loops
(the readers' side when every call in the loop reads) take the matching side. It is as safe as the lock: a
writing function still runs alone, and a reading one never beside it. No lock at all while every `Parallel`
reaching a singleton only reads it, with writes only between stages, cannot be proven at compile time (a handle
kept in an attribute or a list may still be running when the program writes), so the readers' side is what the
compiler uses, measured on the shape of a game engine's columns (`benchmarks/singleton_reads`: eight
systems reading one column per row, 202 ns to 6 ns a row). `conformance/stage6/singleton_reads` reads while the
main thread writes.

**While no task is in flight, the lock is skipped.** The pool counts each task from `submit` until its work has returned
(`ThreadPool._task_begun`/`_task_ended`, one atomic addition each). A wrapper that finds the count zero runs the
function unlocked, since only the program's own thread runs the program then, and pushes the lock it skipped on a
stack of its thread's (sixteen deep; deeper, it locks as before). A task started while such a call is running
(by the function itself, or anything it calls) first takes every lock on that stack for the thread, before it is
counted; the wrapper lets each go as its call returns. So a task never sees a singleton's function half-way through
unlocked. This is sound without any analysis of where tasks start, which the call effects cannot always see (a
function value passed to a list's `each`). Proving "no `Parallel` live" per call site is not possible at
compile time, since handles kept in attributes and lists make it unprovable. `conformance/stage6/unshared_locks` starts
work inside a skipped call that writes the same singleton. **A `Weak` a `Parallel` reaches is a
compile error** too ([memory.md](memory.md#rules-in-full), `diagnostics/weak_across_threads`).

A program without `Parallel` gets none of it: an atomic singleton compiles to plain reads and writes, and no
singleton has a lock.
`conformance/stage6/singleton_guard`, `singleton_forms`, `singleton_lock_calls`. The rule for
`Parallel(function)` is checked like `parallel_each_`'s (`diagnostics/parallel_function_reach`).

**A task may keep what was handed to it.** `Parallel(maker.work)` may reach an attribute of
`maker` that holds a list or an object (which is otherwise refused) when the compiler proves the task is its only
holder. Everything else stays refused, with an error that names this way out. The proof, all
of it at compile time over the source:

- **The instance is handed over.** `maker` is a local made by `var maker = Maker(...)` earlier in the same block as
  the `Parallel(...)`; between the two lines it is used only to call its functions and to read or write its
  attributes that hold values (not passed, stored, given another name, or read for an attribute that holds an
  object); and no function of `Maker` uses `this` as a value (naming one of its own functions as the argument
  of `each`, `filter`, `any`, `all`, `count`, `find`, `sort_by` or `sum` does not count, since those call it
  and let it go: `stale.each(rebuild)`). `this` itself (`Parallel(own_function)`), a
  parameter, an attribute, or an object made in an enclosing block is never handed over.
- **The attribute is its own.** Its default is `null` or something made there (a constructor, `List<T>()`, a list
  literal); every assignment to it in `Maker` makes a new one; no other class writes it; and no function of
  `Maker` hands it out (returns it, passes it, stores it, names it again, or reads a member of it that is not a
  call or an attribute holding a value).
- **What it holds.** A `List` or `Dictionary` of plain values (numbers, `Boolean`, enums, text) needs nothing more. An object needs its class to be the program's own, never to use
  `this` as a value (with the same allowance), and every attribute of its own that is not a value to pass the same
  two checks. A list of
  objects, a function value, a union or a `type` stays refused: its elements could be held elsewhere.
- **The maker lets go.** Any later statement of that block that names `maker` is `'crafter' was handed to
  'Parallel(crafter.craft)', which keeps its 'recipe_ids' on another thread, so it is not used after that line:
  make another 'Crafter' for what follows, or read the task's result`, on that statement. The same holds when the
  task writes an attribute of `maker` that holds a value, since the two threads would write it at once and one write
  would be lost: `'counter' was handed to 'Parallel(counter.bump)', which writes its 'count' on another thread, so it
  is not used after that line: ...` (`diagnostics/parallel_shared_writes`). Each pass of a loop makes
  its own, so `var crafter = Crafter(first)` then `Parallel(crafter.craft)` inside a `while` hands over a new one
  every pass.

The task runs exactly as it did; nothing is added at run time. `conformance/stage6/parallel_handover`,
`diagnostics/parallel_handover`.

**Reads in a row overlap.** All the IO classes use it, and a `File` or `Socket` is not changed: the compiler does it
at the call site. When two or more statements in a row are each `var name = receiver.read()` on a `File` (or
`receiver.read_line()` on a `Socket`), with no written type, the receiver a name or an attribute path, no statement
naming a variable an earlier one declared, and the name never assigned again in the function, every one but the
last is compiled as `var name = Concurrent(receiver.read)`, and after the last each is joined (its private `_join()`)
before the next statement runs. Every later use of the name is an implicit join of a finished handle (above),
so narrowing, printing and passing it are unchanged. Only reads, only side by side, and all finished before
anything else runs, so nothing the program does next (writing one of those files, say) can see a difference;
reads followed by other work, or a read into a name that is assigned later, stay where they are. Such a program
uses the scheduler, so it is compiled with `SPITE_THREADS`; a program without two reads in a row compiles as
before. A `Directory` listing is not a waiting call (it has no helper thread), so starting it early would not overlap
anything, and it is left alone. `conformance/stage6/overlapped_reads`.

---

Next: [Standard library](standard_library.md), the classes every program can name, and what each one does.
