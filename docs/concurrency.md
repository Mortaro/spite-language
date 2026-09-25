# Concurrency: waiting without colouring

> **What is built:**
> `Concurrent(function)` runs a function as a state machine the compiler writes, on the program's own thread, and
> `Parallel(function)` runs one on a fixed pool of worker threads; the handle stands in for what the function
> returns, and reading it is what waits. `finished` answers whether the work is done without waiting, and dropping
> the handle waits for it. Where a program already waits -- `Program.sleep`, `Console.read_line`, reading or
> writing a `File`, a `Socket`'s `accept_client` and `read_line`, reading a `Concurrent` or a `Parallel` -- the
> compiler turns the wait into a point the state machine returns from, so other work runs meanwhile, and a
> `--repl-port` build answers its commands there.
> `list.parallel_each_update()` runs a member on every element across the pool, and the compiler checks that the
> member reaches only its own element. Reads written one after another overlap without being asked.
> Windows runs all of it; the Linux and macOS folders are held to compiling. **Not built:** HTTP, cancelling a
> `Concurrent`, and a check for a `Parallel` function made from a function value
> ([manual section 15](../manual.md#15-standard-library--partial)).

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

func Sleeper(starting_name: String, starting_milliseconds: Int, shared_log: List<String>) {
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

There is no `.wait()`. A `Concurrent<String>` goes wherever a `String` is expected, and the first place it is
used is where the program waits for it: an argument, a `var` with the result's type written, a `return`, an
operand of `+` or `==`, a `{...}` inside text, or a function of the result called on it (`slow.length()`). A
function the handle has itself -- only `finished` -- is the handle's. A result that may be null is narrowed on
the handle itself, `if reading { ... }`, `crash reading`, as any `T?` is. Reading it a second time does not wait
again: the value is kept.

A `var` written without a type keeps the handle, so `var reading = Concurrent(notes.read)` is still the running
work and can be asked whether it has `finished`.

### Reads in a row overlap

The IO classes do this themselves, so most programs never write `Concurrent`: when two or more declarations in a
row each read a `File` (`read()`) or a `Socket` (`read_line()`) held in a name or an attribute, and none of them
names a variable declared by an earlier one, the compiler starts every read but the last as a `Concurrent`, runs
the last one, and waits for all of them before the next statement. The program waits for the slowest file rather
than for each in turn, and nothing it does afterwards can tell: the values are the same, and a file written by the
next statement is written after the reads.

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
read followed by other work waits where it is written.

### What the compiler does at a wait

A `Concurrent` is a **state machine** the compiler writes while compiling
([D176](../manual.md#decision-log)). Every function that can reach a wait -- `nap` above, and whatever `nap` calls
that waits -- is compiled a second time as a resumable version: its locals and parameters live in a small frame on
the heap instead of on the C stack, and every wait inside it is a numbered point the function can return from and
later jump back to. Starting a `Concurrent` makes that frame and runs it to its first wait; when the wait is over,
the program's event loop (`library/scheduler.spite`) runs it again from where it stopped. There is no stack per
`Concurrent`, no stack switching and nothing to ship that a WebAssembly build could not carry:

| The program writes | While it waits |
|---|---|
| `program.sleep(milliseconds)` | the state machine returns, and is run again once its time has come |
| `Console.read_line()`, reading or writing a `File`, a `Socket`'s `accept_client` or `read_line` | the one blocking system call runs on a short-lived helper thread, and the state machine is run again when it returns |
| reading a `Concurrent`'s value, or dropping it | the state machine is run again once that one has finished |
| reading a `Parallel`'s value, or dropping it | the pool finishes it (the waiting thread runs it itself if no worker has started it), then anything ready runs once |

Code that is not inside a `Concurrent` -- the entry constructor and everything it calls -- waits where it is: its
wait runs the event loop itself, so the state machines keep going around it until it is done. Only one piece of
Spite code runs on the program's thread at a time, and a state machine only stops at one of these points, so no
two pieces of Spite code touch the program's state at the same time. The helper thread runs the system call and
nothing else.

**What waits inside a state machine.** A wait written as a call -- `program.sleep(5)`, `file.read()`,
`reading.length()` on a `Concurrent`, a function of your own that waits -- is a point the state machine returns
from, wherever the call is written: in a `var`, an argument, a `{...}` inside text, a `while` condition (which
waits again on every pass). A wait inside the right side of `and`/`or`, a call through a function value, a
constructor, and dropping a `Concurrent` inside a `Concurrent` still wait correctly, but by running the event loop
right there, as code outside a `Concurrent` does: the other `Concurrent`s keep running, while this one holds its
place until the wait is over.

**When blocking is faster, the compiler blocks.** A program that never makes a `Concurrent` has no state machines,
no event loop and no helper threads: every wait is the plain blocking call, and its C is what it was before any of
this existed. Inside a program that has them, a wait is the plain call whenever nothing else could run -- no
`Concurrent` is alive and no REPL is listening. It is the same rule as everywhere else in Spite: you write what you
mean, and the compiler picks the fast way to do it.

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

## `Parallel`: work that computes

`Concurrent` never makes a program faster at computing: every `Concurrent` runs on the program's one thread. For work that keeps a core
busy, `Parallel(function)` runs the function on the program's **thread pool**, and the same rules apply: the
handle stands in for the result, `finished` never waits, and dropping it waits.

```gdscript title=parallel_tour/summer.spite
var limit = 0

func Summer(starting_limit: Int) {
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
program's own thread keeps that one -- the first time a `Parallel` is made, and never another: a thousand `Parallel`s a second reuse the same threads, and a
program that makes none starts none. The workers take work in the order it was started. Reading a `Parallel` that
no worker has picked up yet runs it on the reading thread instead of waiting behind the queue, so a `Parallel`
started from inside another one cannot wait for a worker that is busy waiting for it. At exit the pool finishes
what is queued and stops its threads.

| `ThreadPool()` | |
|---|---|
| `size(): Int` | how many worker threads it runs (starting them if it has not) |
| `worker_index(): Int` | which worker is running this code, from `0`, or `-1` on a thread that is not one of them -- for scratch memory kept per worker |

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
    var runs = List<Parallel<Int>>()
    var index = 0
    while index < 8 {
        runs.append(Parallel(record_commands))
        index = index + 1
    }
    var recorded = 0
    index = 0
    while index < runs.count() {
        var count: Int = runs.get_at(index)
        recorded = recorded + count
        index = index + 1
    }
    console.print(recorded, "commands recorded,", commands, "counted under the lock")
}

func record_commands(): Int {
    var buffer = buffers.get()
    if not buffer {
        buffers.set(List<String>())
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

A `ThreadLocal` keeps every thread's value until it is dropped itself, so a thread that ends does not take its
value with it. `get()` takes no lock: it reads this thread's slot number and then that slot, in an array read with
one atomic load, so it costs the same with one thread or thirty. Only a thread's first `set` locks, and when the
array is full it copies it into one twice the size and publishes that; a thread still reading the old one reads
the same value there, and the old arrays are freed with the `ThreadLocal` (together never larger than the one in
use). A later `set` takes the lock too, briefly, so it cannot land while the array is being copied. Both classes are the operating system's own (`TlsAlloc` and `SRWLOCK` on Windows, `pthread_key_t`
and `pthread_mutex_t` elsewhere), reached through each system's folder.

A program that makes a `Parallel` (or a `Concurrent`, or is built with `--repl-port`) counts references with
atomic operations, because an object can now be shared between threads; every other program keeps the plain,
cheaper counts. What a `Parallel(function)` touches is not checked: give it an instance of its own, as each
`Summer` above has, and do not change that instance until the result is back. The pass below is checked.

## `parallel_each_`: a member on every element

`list.parallel_each_update()` calls `update()` on every element of the list, split across the pool: the list is
cut into a few pieces per thread, the calling thread runs the first piece and whatever piece no worker has taken
yet, and the call returns when every element is done. `filter_` steps before it run in the same pass, piece by
piece, with no list in between: `entities.filter_alive().parallel_each_update()`.

```gdscript title=parallel_pass/particle.spite
var position: Long = 0
var speed = 0
var awake = false

func Particle(starting_speed: Int) {
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
        particles.append(Particle(index))
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
own attributes that hold a value (a number, a `Bool`, text, an enum or a `Symbol`), any singleton and the
member's locals. A singleton of the program's own that can change is locked for you in a program that uses
`Parallel` ([optimizations.md](optimizations.md#singletons-a-parallel-reaches-take-a-lock)), so a `Parallel` or a
pass may call it while the program's thread does too. An attribute holding another object
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
    boids.append(Boid())
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
{"ok":true,"value":"true","type":"Bool"}

# Answered between two frames: the loop sees the change at its next test.
$ spite connect 4000 --command="program.running = false"
{"ok":true,"value":"false","type":"Bool"}

$ spite connect 4000 --command="exit"
{"ok":true,"value":"","type":""}
```

A program that never waits is answered too. In a `--repl-port` or `--hot-reload` build, and only there, the
compiler ends every pass of every `while` in the program's own code with a **check point**: one call that looks
at whether a command (or a changed file) is waiting and, if one is, answers it there, on the program's thread,
between two passes of the loop. A busy loop is served between two passes the way a frame loop is served between
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
{"ok":true,"value":"false","type":"Bool"}

$ spite connect 4000 --command="exit"
{"ok":true,"value":"","type":""}
```

A build without those flags has no check points at all: its C is the same as before they existed. The standard
library's own loops have none either, so a command waits for a library call to return.
