# Concurrency: waiting without colouring

> **What is built:**
> `Concurrent(function)` runs a function as a state machine the compiler writes, on the program's own thread, and
> `Parallel(function)` runs one on a fixed pool of worker threads; the handle stands in for what the function
> returns, and reading it is what waits. `finished` answers whether the work is done without waiting, and dropping
> the handle waits for it. Where a program already waits -- `Program.sleep`, `Console.read_line`, reading or
> writing a `File`, a `Socket`'s `accept_client`, `read_line` and `read_bytes`, reading a `Concurrent` or a `Parallel` -- the
> compiler turns the wait into a point the state machine returns from, so other work runs meanwhile, and a
> `--repl-port` build answers its commands there.
> `list.parallel_each_update()` runs a member on every element across the pool, and the compiler checks that the
> member reaches only its own element. Reads written one after another overlap without being asked.
> A program asks while compiling which functions can wait (`$system_type.function_waits("update_each")`), and an
> engine keeps `Concurrent`s out of its stages with `Scheduler().resume_only_when_asked()` and
> `Scheduler().run_ready()` between frames ([D209](decisions.md)).
> A singleton of the program's own that a `Parallel` reaches is made thread-safe by the compiler, in the cheapest
> form it can prove safe. A program that uses none of this carries none of it ([what it costs](#what-it-costs)).
> Windows runs all of it; the Linux and macOS folders are held to compiling. **Not built:** HTTP, cancelling a
> `Concurrent`, and D184's per-thread and reader-writer forms
> ([the rules in full](#concurrency-concurrent-parallel-and-hidden-waiting--implemented-on-windows)).

There is no `async` and no `await` in Spite, and there never will be. In JavaScript or C# a function that waits
declares itself `async`, which changes its return type and forces every caller to `await` it, and every caller's
caller, all the way up: the waiting leaks into every signature above it. Spite keeps waiting a property of the
*call site*: any function can be run concurrently by whoever calls it, and a function never says whether it waits.

## `Concurrent`: work that waits

`Concurrent(file.read)` takes a function value (a function bound to its instance, as in
[functions_and_operators.md](functions_and_operators.md)) and starts running it straight away. The handle it
gives back stands in for what the function returns -- a `String?` for `file.read` -- and the type is worked out
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
    var notes = File(".spite-cache/concurrency_notes.txt")
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
is used as one is where the program waits for it -- `"{slow} and {fast}"` above, `console.print(slow)`,
`slow.length()`. Reading it a second time does not wait again: the value is kept. A result that may be null is
narrowed on the handle itself, `if reading { ... }`, as any `T?` is.

A `var` written without a type keeps the handle, so `var reading = Concurrent(notes.read)` is still the running
work and can be asked whether it has `finished`, the one member the handle has of its own. Every place a handle
becomes its value is listed in [the rules](#concurrency-concurrent-parallel-and-hidden-waiting--implemented-on-windows).

### Reads in a row overlap

The IO classes do this themselves, so most programs never write `Concurrent`: when two or more declarations in a
row each read a `File` (`read()`) or a `Socket` (`read_line()`), the compiler starts every read but the last as a
`Concurrent`, runs the last one, and waits for all of them before the next statement. The program waits for the
slowest file rather than for each in turn, and nothing it does afterwards can tell: the values are the same, and a
file written by the next statement is written after the reads.

```gdscript title=reads_in_a_row/reads_in_a_row.spite entry
var console = Console()

func ReadsInARow() {
    var settings_file = File(".spite-cache/documentation_settings.txt")
    var scores_file = File(".spite-cache/documentation_scores.txt")
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
[the rules](#concurrency-concurrent-parallel-and-hidden-waiting--implemented-on-windows) ("Reads in a row overlap");
a program that has two such reads pays for the scheduler, as one that writes `Concurrent` does
([what it costs](#what-it-costs)).

### What the compiler does at a wait

A `Concurrent` is a **state machine** the compiler writes while compiling
([D176](decisions.md)). Every function that can reach a wait -- `nap` above, and whatever `nap` calls
that waits -- is compiled a second time as a resumable version: its locals and parameters live in a small frame on
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

Code that is not inside a `Concurrent` -- the entry constructor and everything it calls -- waits where it is: its
wait runs the event loop itself, so the state machines keep going around it until it is done -- unless the program
has chosen [where they resume](#choosing-where-concurrents-resume). Only one piece of
Spite code runs on the program's thread at a time, and a state machine only stops at one of these points, so no
two pieces of Spite code touch the program's state at the same time. The helper thread runs the system call and
nothing else.

**What waits inside a state machine.** A wait written as a call -- `program.sleep(5)`, `file.read()`,
`reading.length()` on a `Concurrent`, a function of your own that waits -- is a point the state machine returns
from, wherever the call is written: in a `var`, an argument, a `{...}` inside text, a `while` condition (which
waits again on every pass). It runs before the rest of the statement it is written in. A few waits -- inside the
right side of `and`/`or`, through a function value or a constructor -- still wait correctly but hold their
`Concurrent` in place while the others keep running; [the rules](#concurrency-concurrent-parallel-and-hidden-waiting--implemented-on-windows)
list them.

**A call that never waits is not a wait.** A `Socket`'s `accept_client_now`, `read_line_now`, `read_bytes_now`
and `write_bytes_now` answer at once with what is there ([standard_library.md](standard_library.md#socket)), so
they are ordinary calls: a game loop that polls its connections every frame has no state machine and no helper
thread for them, and it waits only where it sleeps until the next frame.

**When blocking is faster, the compiler blocks.** A program that never makes a `Concurrent` has no state machines,
no event loop and no helper threads: every wait is the plain blocking call. Inside a program that has them, a wait
is the plain call whenever nothing else could run -- no `Concurrent` is alive and no REPL is listening. It is the
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
drawing frames, and pick the result up on the first frame after it is ready -- the value is then read without
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
of a stage sees the world half updated. So an engine says where they resume ([D209](decisions.md); both spellings
are proposed by Claude, unconfirmed):

- `Scheduler().resume_only_when_asked()` makes every other wait on the program's thread leave the `Concurrent`s
  alone from then on: a sleep only sleeps, a read only blocks, reading a `Parallel` only joins it.
- `Scheduler().run_ready()` runs every `Concurrent` whose wait is over, once each, and returns at once -- the
  engine calls it between two frames.
- Reading a `Concurrent`'s value, or dropping it, still runs them all until that one is done. The program asked for
  that one, and it may be waiting on another, so a join never waits for something only the loop could finish.

The IO itself goes on meanwhile: a blocking read inside a `Concurrent` -- a file, or a `Socket`'s `read_bytes` --
runs on its helper thread, and the state machine continues on the program's thread at the next `run_ready()`.

```gdscript title=frame_io/save_game.spite
var console = Console()
var save = File(".spite-cache/documentation_save.txt")

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
    var save_file = File(".spite-cache/documentation_save.txt")
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
polls `finished` and never calls `run_ready()` never ends, since nothing moves the `Concurrent` on -- the one
mistake this mode allows. Which systems to start this way is asked while compiling, with
`$system_type.function_waits("update_each")` ([metaprogramming.md](metaprogramming.md#asking-whether-a-function-waits)),
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

`ThreadPool()` (`library/thread_pool.spite`, a singleton) starts a worker thread for every core but one -- the
program's own thread keeps that one -- the first time a `Parallel` is made, and never another: a thousand
`Parallel`s a second reuse the same threads, and a program that makes none starts none. There is one pool per
program, and any code -- an engine system, an asset loader, a library class -- hands work to it the same way, by
`Parallel(function)` ([D191](decisions.md)): no submission starts a thread of its own, and there is no second pool
to make or pass around. The workers take work in the order it was started. Reading a `Parallel` that no worker has
picked up yet runs it on the reading thread instead of waiting behind the queue, so a `Parallel` started from
inside another one cannot wait for a worker that is busy waiting for it. At exit the pool finishes what is queued
and stops its threads.

| `ThreadPool()` | |
|---|---|
| `size(): Integer` | how many worker threads it runs (starting them if it has not) |
| `worker_index(): Integer` | which worker is running this code, from `0`, or `-1` on a thread that is not one of them -- for scratch memory kept per worker |

### A value per thread, and a lock

Work split across threads often wants something of its own on each thread -- a command buffer per runner, a
scratch list -- and a short section only one thread may be in at a time. `ThreadLocal<T>()` holds one value per
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
        var count: Integer = runs.get_at(index)
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
safe by the compiler ([D183, D184](decisions.md)). `HitCounter` below only counts, so each `record()` becomes one
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

A `ThreadLocal` keeps every thread's value until it is dropped itself, so a thread that ends does not take its
value with it. `get()` takes no lock and costs the same with one thread or thirty; `set` takes the lock briefly.
Both classes are the operating system's own (`TlsAlloc` and `SRWLOCK` on Windows, `pthread_key_t` and
`pthread_mutex_t` elsewhere), reached through each system's folder, and a program that makes neither carries
neither.

A program that makes a `Parallel` (or a `Concurrent`, or is built with `--repl-port` or `--hot-reload`) counts
references with atomic operations, because an object can now be shared between threads; every other program keeps
the plain, cheaper counts. `Parallel(work)` is checked while compiling (D179; the reading proposed by Claude, unconfirmed): the function it
runs, and every function of the same class that function calls by name, may read and write only the attributes of
its own instance that hold values -- numbers, `Boolean`, enums, text -- its locals and parameters, singletons (which
[D183](decisions.md) makes safe) and a `Lock` or `ThreadLocal`, which are made to be shared. An attribute holding a
list, a dictionary or another object is an error naming it, since another thread may hold the same object:
`'Parallel(tally.count_up)' runs 'record' on another thread, so it may reach only the attributes of its own
'Tally' that hold values, its locals and singletons (D179): 'marks' holds a List<Integer>, which another thread may
share` (`diagnostics/parallel_function_reach`, where the maker kept `tally.marks` for itself). Make what the work
needs a local, or give the work an object of its own whose attributes are values, as each `Summer` above is. The
check is the one `parallel_each_` uses, and costs nothing at run time.

An object made for the work and **handed over** may keep more ([D207](decisions.md)): when it is made on its own
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
what follows, or read the task's result` (`diagnostics/parallel_handover`; [the rules](#concurrency-concurrent-parallel-and-hidden-waiting--implemented-on-windows)).

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

**The member may reach only its own element** (D35). Every element runs at the same time as the others, so the
compiler reads the member -- and every function of the element's class it calls -- and allows only the element's
own attributes that hold a value (a number, a `Boolean`, text, an enum or a `Symbol`), any singleton and the
member's locals. A singleton of the program's own that can change is made safe for you in a program that uses
`Parallel` ([D183, D184](decisions.md)) -- nothing when it is read-only, atomics when it only counts or flags, a
lock otherwise ([optimizations.md](optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form)) -- so a
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
'parallel_each_follow' runs 'follow' on many elements at once, so it may reach only its own 'Boid' attributes that hold values, and its locals (D35): 'leader' holds a 'Boid?', which another element may share
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

This session is replayed by `check.sh` against the program above while its loop runs:

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
answer it there, on the program's thread, between two passes of the loop (D211). A busy loop is served between two passes the way a frame loop is served between
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
points did not exist ([D174](decisions.md)). The standard library's own loops have none either, so a command waits
for a library call to return.

## What it costs

Everything on this page is chosen per program, from what the program uses, and a program that uses none of it
carries none of it ([D177](decisions.md)); the compiler's side of each is on
[optimizations.md](optimizations.md#concurrency-machinery-only-where-it-is-used).

| The program | Carries at run time |
|---|---|
| uses none of it | nothing: no scheduler, no state machine, no helper thread, no pool, no lock, plain reference counts; every wait is the plain blocking call |
| makes a `Concurrent`, or has two reads in a row | the `Scheduler` singleton and its event loop; a second, resumable copy of each function a `Concurrent` reaches that waits (the plain copy is dropped when nothing calls it); one heap frame per waiting call made inside a `Concurrent`; one short-lived operating-system thread per blocking call made while something else could run; atomic reference counts everywhere |
| makes a `Parallel` or runs a `parallel_each_` pass | the `ThreadPool` singleton, whose workers start at the first `Parallel` and never again; about two dozen allocations per `Parallel` and one per pass; atomic reference counts everywhere; for each program singleton a `Parallel` can reach and that changes, atomics or an uncontended lock per call |
| calls `Scheduler().resume_only_when_asked()` or `run_ready()` | one `Boolean` the scheduler reads at each wait, and each function it calls; neither is there otherwise |
| makes a `ThreadLocal` or a `Lock` | one system per-thread slot or lock each, freed with it |
| is built with `--repl-port` or `--hot-reload` | the scheduler, and one check-point call at the end of every pass of every loop of its own code; nothing of either in any other build |

What is never there: a stack per `Concurrent`, a stack switch, a runtime that a WebAssembly build could not carry
([D176](decisions.md)).

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

<a id="concurrency-concurrent-parallel-and-hidden-waiting--implemented-on-windows-names-and-mechanism-proposed-by-claude-unconfirmed"></a>

### Concurrency: `Concurrent`, `Parallel` and hidden waiting  **[implemented on Windows]**

D35 (no function colouring, join on drop), D37 (the compiler injects the REPL's drain points where the program
already waits), D99 (IO never blocks the program by the program's own hand) and D103 (async waiting and threads are
two things, and `Task` is too generic a name) are built as two classes and one scheduler. The names are decided
(D133, which keeps `Concurrent` and `Parallel`; its "fiber" and `wait()` are superseded by D176 and D134), and so
is the mechanism (D176, compile-time state machines); the details below marked so are Claude's readings.

- **`Concurrent(function)`** (`library/concurrent.spite`) runs a function value (D17, D39) as a **state machine**
  the compiler writes (D176, below), on the program's own thread, starting straight away; reading the handle is what waits for its value (D134, below) and
  `drop()` waits for it, so scope exit is a join point. It is for work that waits: IO, sleeps, database calls
  later.
- **`Parallel(function)`** (`library/parallel.spite`) runs one on the program's **thread pool** (D135, below), with
  the same reading and join on drop. It is for work that computes.
- The type is never written: `Concurrent(file.read)` is a `Concurrent<String?>`, worked out from the function's
  return (see the inference row in the decision log). Writing it on the `var` that makes the handle is an error,
  `the type of a 'Parallel' handle is never written: it is worked out from the function it runs, so write 'var
  total = Parallel(count)'` (`diagnostics/written_handle_type`); where no function is there to infer from -- the
  element type of `List<Parallel<Integer>>()`, a parameter -- it is still written (proposed by Claude,
  unconfirmed: the rule read as covering only the declaration that starts the work). A function that returns nothing gives a `Concurrent<Nothing>`,
  which has no value to read and is waited for by dropping it: keep such handles in a list, and clearing the list
  or leaving its function waits for all of them.
- **Every attribute of both classes is private** (`_work`, `_results`, `_state`, ...), and the one public member is
  `finished: Boolean` (below). Nothing outside the class reaches the pool's job, the state machine or the work.

**A concurrent result joins on first use** (D134, decided by Mortaro; the reading below is proposed by Claude,
unconfirmed).  **[implemented]** There is no `wait()` and no `join()` any more: **the handle stands in for its
result wherever the result's type is expected, and the compiler inserts the join at each such use** (the first
one waits; the value is kept, so later ones do not). Exactly, a `Concurrent<T>` or `Parallel<T>` becomes its `T`:

- where it is stored or passed as a `T` -- a `var` whose type is written, an assignment, an argument (a variadic
  `Printable` one included, so `console.print(sum)` prints the value), a `return`, an element of a list literal;
- as an operand -- `+`, `==`, `and`, `not`, any operator -- and inside text, `"{sum}"`;
- as the receiver of a member the handle does not have: `greeting.upper_case()`, `greeting.length()`;
- as a condition: `if ready`, `while`, `assert`, `crash`; and when `T` is nullable, narrowing the handle narrows its
  value -- `if reading { use(reading) }`, `crash reading` -- because D63 narrows a name itself rather than a copy.
  The value is read once, into a hidden local, and the narrowed name reads that.

It stays a handle where a handle is expected (`List<Parallel<T>>.append(handle)`), in a `var` without a written
type (`var loading = Parallel(asset.load)` is the running work), and for the handle's own member, `finished`. A
`T` of `Nothing` is never read, so such a handle only joins on drop. Comparing two handles with `==` compares their
values; there is no way to compare the handles themselves (proposed: nothing has needed it). The compiler writes
each join as a call to the class's private `_result()`, which `library/concurrent.spite` and
`library/parallel.spite` declare in Spite; only a join the compiler inserted may call it.

**`finished` never waits** (proposed by Claude, unconfirmed). `handle.finished` is `true` once the function has
returned. On a `Parallel` it reads the job's state with one atomic load; on a `Concurrent` it reads the flag its
state machine's frame carries once it finishes, so a state machine only makes progress when the program waits
somewhere (`while not reading.finished { program.sleep(1) }` is the polling loop). The same on every system
(`conformance/stage6/finished_polling`).

**The thread pool** (D135 and D191, decided by Mortaro; the shape below is proposed by Claude, unconfirmed).
**[implemented on Windows]** `library/thread_pool.spite` is a singleton, `ThreadPool()`, that the `Parallel`s share.
It is the program's one pool: any code hands work to it by `Parallel(function)` and gets a handle whose `finished`
never blocks and whose value joins on first use; no submission starts an operating-system thread of its own, and
there is no second pool to create or pass around (D191).

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
  waiting thread, so a job that starts and waits for another job -- a `Parallel` inside a `Parallel`, or a
  `parallel_each_` inside one -- cannot wait on workers that are all waiting for it. A job already running is waited
  for on the done condition. This is also the only waiting `ThreadPool.join` does, and it is the wait the scheduler
  wraps (below).
- **Join on drop is kept.** The queue holds the job's function bound to a small `ParallelCall<T>` that owns the
  work and the result, never the `Parallel` itself, so dropping the last handle runs its `drop()` straight away,
  and that waits.
- **At exit** the pool is a singleton destroyed in reverse creation order (D142): it lets the workers finish what
  is queued, joins them and frees its lock. A `Parallel` whose `drop()` runs later finds its job done and does not
  touch the pool.
- **What the compiler supplies (D147).** Starting a worker needs the address of a C function that calls the pool's
  private `_serve()`: `ThreadPool` declares two members without a body that the generator writes,
  `entry_address()` and `address()`. `Concurrent` has two more, `_start_frame()` and `_frame_result()`, and
  `Scheduler` has `step_frame(frame)` and `release_work(frame)`, whose bodies are a line of C in the compiler. `--final-classes` prints each
  declaration. D147 decides that no compiler-supplied function stays bodiless and no Spite body holds C; turning
  these into Spite over the backend's primitives is **not built**. Everything else -- the queue, the claim, the
  split -- is Spite.
- **Cost.** A `Parallel` makes about two dozen allocations (25 under `--debug-memory`), most of them the two
  function values (a `Spite.Function` is its own reflection object, D39), and no thread.
  `conformance/stage6/thread_pool_reuse` runs a thousand `Parallel`s and checks every one ran on one of the pool's
  workers or on the thread that read it.
- **Not tested:** the Linux and macOS folders compile but have never run; a `Parallel` made on two non-worker
  threads at once before the pool has started (each could start it; the program's thread and a `Concurrent`'s
  helper are the only candidates).

**A value per thread, and a lock** (proposed by Claude, unconfirmed).  **[implemented on Windows]** Three small
classes, each system's folder supplying the calls, and each carried only by a program that makes one:

- **`Lock()`** (`library/lock.spite`): `while_locked(work: Spite.Function<Nothing>)` runs the function holding the
  lock, so the unlock cannot be forgotten; `lock()` and `unlock()` stay for a section that is not one function.
  `SRWLOCK` on Windows, `pthread_mutex_t` elsewhere; `drop()` frees it. Not reentrant.
- **`ThreadSlot()`** (`library/thread_slot.spite`): one `Long` per thread, `0` until written -- `TlsAlloc` or
  `pthread_key_create`. `read()`, `write(value)`; `drop()` gives the key back.
- **`ThreadLocal<T>()`** (`library/thread_local.spite`): a value of any type per thread. Its `ThreadSlot` holds
  each thread's position in an array of `T` the `ThreadLocal` owns, so values stay reference counted
  and are all released when the `ThreadLocal` is dropped, whichever threads set them and whether or not those
  threads still run. `get(): T?` is `null` on a thread that has not called `set(value)`. `get()` takes no lock: the
  array's address is read with one atomic load. `set` takes the `Lock`; a thread's first `set` may grow the array,
  copying it into one twice the size and publishing that with an atomic store, and the arrays it replaced are freed
  with the `ThreadLocal`, so a thread still reading one reads its own unchanged value there.

A value per pool worker (`worker_index()` into a list) needs no system call but covers only the pool's workers,
not the program's thread or a `Concurrent`'s helpers. `conformance/stage6/thread_locals`,
`conformance/stage6/thread_local_growth`.

**The mechanism: compile-time state machines, and a helper thread per blocking call** (D176, decided by Mortaro:
"our goal is to have no runtime only compile time"; the details below are proposed by Claude, unconfirmed).
**[implemented on Windows]** A C target has no coroutines, so the compiler writes them. Every function that can
reach a wait from inside a `Concurrent` is compiled twice: the plain function, for code outside a `Concurrent`, and
a resumable version (`bootstrap/source/generation/state_machine.spite`):

- **Which functions.** A function *waits* when it is one of the waits below or calls, by name, a function that
  waits (found over the calls the plain bodies make, to a fixpoint). A `Concurrent`'s function gets a state machine
  when it waits and is a function value made in an argument of `Concurrent(...)`, or of a program class; every
  function a state machine calls by name that waits gets one too. Constructors, a singleton function that takes
  D183's lock, and every function of a program class in a `--hot-reload` build (called through a slot a reload
  swaps) get none.
- **The frame** is a C struct on the heap: a header (the step function, the wait it stopped at, whether it runs or
  has finished, and, for a `Concurrent`'s own frame, the function value it runs), the result, `self`, the parameters, every local and temporary of the body (a name declared twice in
  nested blocks gets two fields), and one slot per wait for the frame it waits on.
- **A `Concurrent` holds its function only while it runs** (proposed by Claude, unconfirmed). The handle lets go of
  the function value as soon as the work has started: its frame holds it instead, and the scheduler releases it
  when the frame finishes (work with no state machine has already finished by then). A class that keeps
  `Concurrent(drain)` of its own function in a list is therefore a cycle only while that work runs, not after, even
  when nobody reads `finished` again (`conformance/stage6/finished_workers`). It costs one pointer in each frame's
  header and a retain and a release per `Concurrent`.
- **The step function** runs the body with every local read from the frame, starts with a jump to the wait it
  stopped at, and answers `true` when the body returns and `false` at a wait that is not over. Jumping into a
  `while` or an `if` needs nothing, since no local lives on the C stack.
- **A wait** written as a call to a function that waits, anywhere an unconditional value is computed -- a
  statement, a `var`, an argument, an operand, a `{...}` in text, a condition -- becomes: hold the receiver, make
  the callee's frame with the arguments, step it until it answers `true` (returning `false` from this step while it
  does not), take its result and free it. It runs before the rest of the statement it is written in (a C statement
  expression cannot be jumped back into). A `while` whose condition waits becomes a loop that waits at the top of
  every pass and leaves when the condition is false. A call inside a template is a call by name like any other:
  `system.phase_each(made_arguments())`, whose arguments a plural template makes, holds the receiver and then
  makes each argument in order into the frame before the wait, and a helper the template passes its symbol to,
  `nap_again(phase)`, waits the same way (`conformance/stage6/template_argument_wait`).
- **Waits that run the loop in place.** A wait inside the right side of `and`/`or` or `==` on a nullable value, one
  reached through a function value, a union's dispatch or a constructor, and a `Concurrent` dropped inside a
  `Concurrent` are the plain calls: they wait by running the event loop where they are, as code outside a
  `Concurrent` does, which keeps every other state machine going but holds this one until the wait is over
  (`mortaros_missing_decisions.md` item 179). A `Concurrent` whose function has no state machine runs it to the end
  when it is made. `function_waits` still answers `true` for a function whose only wait is one of these, since it
  does wait: the wait just holds whoever stepped the frame, such as the loop calling `run_ready()`, until it is
  over.
- **The waits at the bottom** are small state machines the generator writes: `Program.sleep` registers a deadline
  and is over when the clock passes it; `Console.read_line_into`, `File.read_into`, `File.write_text`,
  `File.write_from`, `Socket.accept_handle` and `Socket.receive_into` start their one system call on a helper thread
  and are over when it flags that it returned; `Scheduler.wait_for(frame)`, under reading or dropping a
  `Concurrent`, is over when that frame has finished.

Files cannot be waited on without blocking on any of the three systems, so a blocking call is handed to a
short-lived helper thread (what libuv does for files). The alternative, each system's own readiness (IOCP, epoll,
kqueue), is not used: an ordinary file is always "ready" to epoll and kqueue, the Windows console cannot be read
through IOCP, and one mechanism keeps each system's folder small (item 180 asks about sockets).

**What the compiler writes for code outside a `Concurrent`.** Each operating system's folder names the calls that
block, and the compiler knows them by class and function (the list above, and `ThreadPool.join`, the wait under
reading or dropping a `Parallel`). In a program that uses the scheduler -- one that makes a `Concurrent`, or is
built with `--repl-port` or `--hot-reload` -- each of them is emitted under a `_waiting` name with a small wrapper
in front: a sleep runs the event loop until its time, a blocking call runs on a helper thread while the event loop
runs, and a `Parallel` join joins and then runs whatever is ready once (none of which steps a state machine after
`resume_only_when_asked()`, below). Every other program gets none of it: no
wrapper, no scheduler, no state machine, the same C as before.

**Blocking is what the compiler picks when it is faster** (D99). The wrapper asks the scheduler first: when no
`Concurrent` is alive and no REPL is listening, or the caller is not on the scheduler's thread (a `Parallel`, a
helper, the REPL's socket thread), nothing else could run meanwhile, so the wrapper makes the plain blocking call.

**The scheduler** (`library/scheduler.spite`, a singleton; each operating system's folder reopens it with the
thread, event and clock calls) keeps the frames of the `Concurrent`s that have not finished, the deadlines of the
sleeps in them and a count of helper threads in flight. Its loop answers the REPL's pending command and a pending
reload, if any, steps every frame that is not already running further down the C stack, and, when none finished,
waits on one event -- an auto-reset event on Windows, a pipe read with `poll` elsewhere -- that the helper threads
and the REPL's socket thread signal, with the nearest deadline as its timeout. Waiting forever on nothing is a
deadlock, and a crash.

**Where `Concurrent`s resume** (D209, decided by Mortaro: an engine runs IO tasks between frames; the two
functions below are proposed by Claude, unconfirmed).  **[implemented on Windows]** `Scheduler` has two public
functions for a program that wants `Concurrent`s to resume only between its own stages:

- **`run_ready(): Integer`** steps once every `Concurrent` frame that is not already running, and answers how many
  finished. It never blocks. On any thread but the scheduler's, and before any `Concurrent` exists, it does
  nothing and answers `0`, so a `Parallel` cannot touch the scheduler's frames through it.
- **`resume_only_when_asked()`** holds for the rest of the run (proposed: nothing has needed a way back). A wait
  on the program's thread outside a state machine then steps no frame: with no REPL listening, the waiting
  wrapper makes the plain blocking call; in a `--repl-port` build the wait still answers commands; after joining a
  `Parallel`, nothing ready runs. A wait that runs the loop in place inside a state machine (the list above) steps
  no other frame either. Frames are then stepped only by `run_ready()` and by joining a `Concurrent` outside a
  state machine -- reading its value or dropping it -- which steps every frame until that one has finished,
  because it may be waiting on another `Concurrent`: a join never waits for a loop the program has stopped.
  Inside a state machine a wait is a point it returns from, as before.
- **Not detected:** a loop that polls `finished` and never calls `run_ready()` does not end. It is not a deadlock
  the scheduler can see -- the program is running -- so it is documented rather than caught.
- `conformance/stage6/frames_between_waits`: an engine-shaped loop whose stages join a `Parallel`, sleep and
  write to a socket while a `Concurrent` reads that socket with `read_bytes` three times; the reader resumes only
  at `run_ready()`, never inside a stage. Without `resume_only_when_asked()` the same program's reader resumes
  inside a stage's sleep.

**Soundness.** A program that starts a thread (a `Concurrent`'s helpers, a `Parallel`, or `--repl-port`) is compiled
with `SPITE_THREADS`: every retain and release is an atomic operation, and the `--debug-memory` table takes a lock.
Every other program keeps the plain counts. Spite code on the program's thread only ever changes hands at a wait,
so state machines need nothing more. A singleton first used from two threads at once is made once: the fetch takes
the singleton's own lock only while its slot is empty (`conformance/stage6/singleton_race`). D35's rule is checked
for `parallel_each_` (below), and D179's for a `Parallel(function)` (above): neither may reach a list or object
another thread could hold, though two threads writing one number attribute of a shared instance is still the
program's mistake. A
program's own singletons are made safe (D183, below); the library's are not guarded, so two threads printing at
once may interleave the pieces of their lines. One smaller gap, untested: a REPL client's `exit` while a helper
thread is blocked reading the console may wait on the C runtime's lock on that stream when the process exits.

**`parallel_each_`** (D135, decided by Mortaro; D35's race rule, which Claude proposed, is built as below and is
still unconfirmed).  **[implemented on Windows]** `list.parallel_each_update()` calls `update()` on every element,
split across the pool, and returns when all are done. The generator writes two functions on the list's class, as
it writes a fused chain (D105): `parallel_each_update_piece(first: Integer, end: Integer)`, a `while` over that range of
the buffer, and `parallel_each_update()`, which hands the piece function to `ThreadPool.run_pieces(piece, count)`.
That cuts the list into up to four pieces per thread (the workers and the caller), queues all but the first, runs
the first on the calling thread and joins the rest, claiming any no worker has taken. One allocation per call for
the pieces' states, one function value, and none per element. A list of fewer than two elements runs on the
calling thread. `filter_` steps before it fuse into the piece loop (`entities.filter_alive().parallel_each_update()`
is one pass with no list in between); a `map_` step is an error, since it reaches another object that several
elements may share: `'parallel_each_grow' can follow only 'filter_' steps: 'map_next' reaches another object,
which several elements may share, and a parallel pass may touch only each element's own attributes (D35)`.

- **The race rule, D35 as a compile check.** The member, every function of the element's class it calls
  (transitively, by name), and every `filter_` member in the chain may read and write only the element's
  attributes that hold a plain value -- a number, `Boolean`, `String`, enum or `Symbol`, or a `T?` of one -- any
  singleton (the library's, and the program's own, which D183 makes safe below, including one bound as an
  attribute of the element), and their own parameters and locals. An attribute holding any other object, a list,
  a dictionary or a function value is an error naming the attribute, its type and the one-thread form
  (`diagnostics/parallel_reach`):
  `'parallel_each_follow' runs 'follow' on many elements at once, so it may reach only its own 'Boid' attributes
  that hold values, and its locals (D35): 'leader' holds a 'Boid?', which another element may share. ...`. A list
  of numbers or text has no members, so `parallel_each_` on one is an error naming `each_`:
  `'parallel_each_to_string' runs a member of each element on many threads, and 'Integer' is not a class: only a
  list of a class has members to run there (write 'each_to_string' to run it on one thread)`.
- **What it cannot see:** one object listed twice runs on two threads at once; a local made from something the
  member was handed (it is handed nothing, so only through a library singleton such as `Memory`); a list of a
  `type` or union (the element's class is not known, so it is an error for now). D35's open questions -- shared
  state across threads and cross-element reads -- stay open.
- `conformance/stage6/parallel_each` runs ten thousand elements twice, filtered once, and a list of one and of none.

**A singleton a `Parallel` reaches is made thread-safe by the compiler, in the cheapest safe form** (D183 and
D184, decided by Mortaro: no keyword and no `shared` header line; which forms are built, their order and exact
conditions proposed by Claude, unconfirmed).  **[partial]** In a program that makes a `Parallel` or runs a
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
4. **Its own lock**, the fallback: each function is emitted as `<name>___unguarded` behind a wrapper that takes
   the singleton's lock (reentrant by owner thread, padded to 64 bytes); a call it makes to itself goes straight
   to the unguarded body, and a write to its attribute from another class takes the lock too, and so does a read
   of it (D211: `var last = registry.last` in another class loads it under the lock; an atomic singleton's
   attribute is read with one atomic load; `conformance/stage6/singleton_lock_calls`).

**A loop that cannot end inside a locked singleton function is a compile error** (D211, decided by Claude under
D205: the lock stays whole-call). A `while true` with no `return`, `assert` or `crash` inside it, in a function of
a singleton that takes the lock (the fourth form above), would hold the lock for good, so every other call on the
singleton would wait forever: `'Window.run' holds Window's lock for the whole call, since a Parallel reaches Window,
and this loop never ends, so every other call on Window would wait forever: move the loop into a class that is not a
singleton, and call Window from it` (`diagnostics/endless_locked_loop`). **A `Weak` a `Parallel` reaches is a
compile error** too ([memory.md](memory.md#rules-in-full), `diagnostics/weak_across_threads`).

A program without `Parallel` gets none of it: an atomic singleton compiles to plain reads and writes, and no
singleton has a lock. **Not built:** D184's buffer per thread for state only appended to, splitting state each
thread touches its own part of, and a reader-writer lock; D183's compile-time check at `return` that such a
singleton hands out only numbers, text, copies or other safe singletons; guarding in a `--hot-reload` build; and
library singletons (`Console`, input, `Clock`) doing better by hand, which D183 allows.
`conformance/stage6/singleton_guard`, `singleton_forms`, `singleton_lock_calls`. D179's rule for
`Parallel(function)` is checked like `parallel_each_`'s (`diagnostics/parallel_function_reach`).

**A task may keep what was handed to it** (D207, decided by Claude under D205; the conditions below and the error
text are proposed by Claude, unconfirmed).  **[implemented]** `Parallel(maker.work)` may reach an attribute of
`maker` that holds a list or an object -- which D179 refuses -- when the compiler proves the task is its only
holder. Everything else D179 refused stays refused, with its error, which now names this way out. The proof, all
of it at compile time over the source:

- **The instance is handed over.** `maker` is a local made by `var maker = Maker(...)` earlier in the same block as
  the `Parallel(...)`; between the two lines it is used only to call its functions and to read or write its
  attributes that hold values (not passed, stored, given another name, or read for an attribute that holds an
  object); and no function of `Maker` uses `this` as a value -- naming one of its own functions as the argument
  of `each`, `map`, `filter`, `any`, `all`, `count`, `find`, `sort_by` or `sum` does not count, since those call it
  and let it go (`stale.each(rebuild)`). `this` itself (`Parallel(own_function)`), a
  parameter, an attribute, or an object made in an enclosing block is never handed over.
- **The attribute is its own.** Its default is `null` or something made there (a constructor, `List<T>()`, a list
  literal); every assignment to it in `Maker` makes a new one; no other class writes it; and no function of
  `Maker` hands it out -- returns it, passes it, stores it, names it again, or reads a member of it that is not a
  call or an attribute holding a value.
- **What it holds.** A `List` or `Dictionary` of plain values (numbers, `Boolean`, enums, text) needs nothing more:
  "may always keep a `List` of plain values". An object needs its class to be the program's own, never to use
  `this` as a value (with the same allowance), and every attribute of its own that is not a value to pass the same
  two checks. A list of
  objects, a function value, a union or a `type` stays refused: its elements could be held elsewhere.
- **The maker lets go.** Any later statement of that block that names `maker` is `'crafter' was handed to
  'Parallel(crafter.craft)', which keeps its 'recipe_ids' on another thread, so it is not used after that line:
  make another 'Crafter' for what follows, or read the task's result`, on that statement. Each pass of a loop makes
  its own, so `var crafter = Crafter(first)` then `Parallel(crafter.craft)` inside a `while` hands over a new one
  every pass.

The task runs exactly as it did; nothing is added at run time. `conformance/stage6/parallel_handover`,
`diagnostics/parallel_handover`.

**Reads in a row overlap** (D134's IO half, decided by Mortaro: "all our IO classes should use it"; this reading is
proposed by Claude, unconfirmed).  **[implemented]** A `File` or `Socket` is not changed: the compiler does it at
the call site. When two or more statements in a row are each `var name = receiver.read()` on a `File` (or
`receiver.read_line()` on a `Socket`), with no written type, the receiver a name or an attribute path, no statement
naming a variable an earlier one declared, and the name never assigned again in the function, every one but the
last is compiled as `var name = Concurrent(receiver.read)`, and after the last each is joined (its private `_join()`)
before the next statement runs. Every later use of the name is an implicit join of a finished handle (D134 above),
so narrowing, printing and passing it are unchanged. Only reads, only side by side, and all finished before
anything else runs, so nothing the program does next -- writing one of those files, say -- can see a difference;
reads followed by other work, or a read into a name that is assigned later, stay where they are. Such a program
uses the scheduler, so it is compiled with `SPITE_THREADS`; a program without two reads in a row compiles as
before. `Directory` listing is not a waiting call yet (no helper thread), so starting it early would not overlap
anything, and it is left alone. `conformance/stage6/overlapped_reads`.

**Not built:** HTTP, cancelling a `Concurrent`, a `Concurrent` made on a thread that is not the scheduler's (it
runs on the spot instead), the rest of D184 (above), D147 for the
generator-written members (above), and running any of this on Linux or macOS, whose folders are held to compiling.
