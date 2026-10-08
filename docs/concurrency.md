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
becomes its value is listed in [the rules](../specs/concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting).

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
[the rules](../specs/concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting) ("Reads in a row overlap");
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

### Calls in a row run at once

A program never has to write `Parallel` to run independent work side by side. When two or more calls in a row
share nothing one of them writes, the compiler runs them on the thread pool at once and waits for all of them
before the next statement:

```gdscript
func tick() {
    physics.update_each()
    animation.update_each()
    audio.update_each()
    renderer.draw_all()
}
```

If `physics`, `animation` and `audio` each read and write only their own state, the first three run at once, and
`renderer.draw_all()` starts after all three have finished, as written. The program reads as one step after
another and runs as fast as the machine allows. The compiler decides it all while compiling, from what each call
reads and writes through every function it reaches, so a program with nothing to overlap carries none of it and
no call says it may run in parallel.

Calls whose order can be seen stay in order: anything that prints, touches a file or a socket, or reads and writes
the same state as another call. A call that does little (a few additions, no loop) is never handed to another
thread, since starting it there would cost more than running it. Two objects of one class count as one: two
`Counter`s each counting their own total share the attribute `total` as far as this rule sees, so they stay in
order, while a `Physics` and an `Audio` overlap. The exact conditions are in
[the rules](../specs/concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting) ("Calls in a row run at once"), and
[optimizations.md](optimizations.md#calls-in-a-row-run-at-once) shows one.

A loop over a list of different objects is the same row, built while the program runs: `systems.each_update()`
over a `List<System>` holding a `Physics`, an `Animation` and an `Audio` runs the three at once when their classes
share nothing written, and in order otherwise. The compiler decides which classes may run together while compiling;
the loop only looks at which objects the list holds when it starts. A list holding two objects of one class runs
in order ([optimizations.md](optimizations.md#a-loop-over-a-list-of-different-classes-runs-them-at-once)).

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
    return Parallel(remover.run)
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
store, re-entered for free by the thread that holds it. Taken by one thread only, it costs a few nanoseconds a call.
Taken by several threads at once it costs more, because each call hands the line from core to core. A thread that
finds the lock taken does not hammer it: it waits a few pauses of the processor, twice as many each time it looks
again, and past a thousand it gives its turn to the system between looks, so the thread holding the lock runs on
and the line stays where it is. Four threads making 250 000 calls each to one shared singleton take 7.7 ms
([`benchmarks/singletons_a_parallel_reaches_take_a_lock`](../benchmarks/singletons_a_parallel_reaches_take_a_lock/)),
a third of the time of the same program in C behind a system mutex, and still fifteen times the time of the same
work split between the threads with no lock at all. So:

- **Give each worker its own singleton** where the state splits (a generic singleton per column, `Column<T>`,
  rather than one `Columns` every worker calls), and the locks are never contended.
- **Call it from a counted loop.** A `while index < count` loop whose calls are all to one singleton (and to lists
  of plain values), with nothing else locked or waited on in it, takes the lock once around the whole loop and
  calls the unlocked bodies inside it ([optimizations.md](optimizations.md#a-counted-loop-of-calls-to-one-singleton-takes-its-lock-once)):
  four workers making 5 million calls each to one shared singleton take 77 ms that way, where C taking a mutex on
  every call takes 501 ms ([the case](../benchmarks/a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once/)).
- **Keep its reads apart from its writes.** A function that only reads the singleton's state (`at(row)`, a lookup)
  takes the readers' side of the lock: a count on its own thread's cache line, so readers on different threads
  never slow each other down, and only a function that writes waits for them
  ([optimizations.md](optimizations.md#a-singletons-reading-functions-do-not-exclude-each-other)). Eight systems
  reading 15 000 rows each of one singleton through `at(row)`, 20 ticks, take 17 ms, where C taking a mutex for
  every read takes 103 ms ([the case](../benchmarks/a_singletons_reading_functions_do_not_exclude_each_other/)). Anything a function calls or assigns beyond reading makes it a writing function,
  so keep lookups small and separate from the functions that change the singleton.
- **Nothing to do between stages.** While no task is on the thread pool, a locked function runs without its lock
  ([the case](../benchmarks/while_no_task_runs_a_singletons_lock_is_skipped/): ten million calls in 53 ms, where C
  taking a mutex on every call takes 76 ms), so code that fills or flushes a singleton on the
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
the plain, cheaper counts, and so does every class whose objects no other thread can count
([optimizations.md](optimizations.md#plain-reference-counts-where-no-thread-reaches-a-class)). `Parallel(work)` is checked while compiling: the function it
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
what follows, or read the task's result` (`diagnostics/parallel_handover`; [the rules](../specs/concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting)).

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
| makes a `Concurrent`, or has two reads in a row | the `Scheduler` singleton and its event loop; a second, resumable copy of each function a `Concurrent` reaches that waits (the plain copy is dropped when nothing calls it); one heap frame per waiting call made inside a `Concurrent`; one short-lived operating-system thread per blocking call made while something else could run; atomic reference counts for the classes another thread can count; one count the scheduler adds to at each read of `finished` that answers `false` and clears at each step |
| makes a `Parallel` or runs a `parallel_each_` pass | the `ThreadPool` singleton, whose workers start at the first `Parallel` and never again; about two dozen allocations per `Parallel` and one per pass; atomic reference counts for the classes another thread can count; for each program singleton a `Parallel` can reach and that changes, atomics or a lock per call (about 2 ns when one thread takes it, far more when several contend: [what it costs](#what-a-singletons-lock-costs)), or one per counted loop of calls |
| calls `Scheduler().resume_only_when_asked()` or `run_ready()` | one `Boolean` the scheduler reads at each wait, and each function it calls; neither is there otherwise |
| makes a `ThreadLocal` or a `Lock` | one system per-thread slot or lock each, freed with it |
| is built with `--repl-port` or `--hot-reload` | the scheduler, and one check-point call at the end of every pass of every loop of its own code; nothing of either in any other build |

What is never there: a stack per `Concurrent`, a stack switch, a runtime that a WebAssembly build could not carry.

---

Next: [Standard library](standard_library.md), the classes every program can name, and what each one does.
