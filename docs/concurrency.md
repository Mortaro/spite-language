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
> ([the rules in full](#concurrency-concurrent-parallel-and-hidden-waiting--implemented-on-windows-names-and-mechanism-proposed-by-claude-unconfirmed)).

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
([D176](decisions.md)). Every function that can reach a wait -- `nap` above, and whatever `nap` calls
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
program's own thread keeps that one -- the first time a `Parallel` is made, and never another: a thousand `Parallel`s a second reuse the same threads, and a
program that makes none starts none. The workers take work in the order it was started. Reading a `Parallel` that
no worker has picked up yet runs it on the reading thread instead of waiting behind the queue, so a `Parallel`
started from inside another one cannot wait for a worker that is busy waiting for it. At exit the pool finishes
what is queued and stops its threads.

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
        runs.append(Parallel(record_commands))
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
own attributes that hold a value (a number, a `Boolean`, text, an enum or a `Symbol`), any singleton and the
member's locals. A singleton of the program's own that can change is made safe for you in a program that uses
`Parallel` -- with atomics when it only counts or flags, with a lock otherwise
([optimizations.md](optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form)) -- so a `Parallel` or a
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
{"ok":true,"value":"true","type":"Boolean"}

# Answered between two frames: the loop sees the change at its next test.
$ spite connect 4000 --command="program.running = false"
{"ok":true,"value":"false","type":"Boolean"}

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
{"ok":true,"value":"false","type":"Boolean"}

$ spite connect 4000 --command="exit"
{"ok":true,"value":"","type":""}
```

A build without those flags has no check points at all: its C is the same as before they existed. The standard
library's own loops have none either, so a command waits for a library call to return.

## Rules in full

The normative rules for this part of the language, in full: what the sections above teach, with the edge
cases, the exact error texts and the notes on how it is built. They were moved here whole from the language
manual when [D193](decisions.md) dissolved it into these pages (its section numbers became links), so each
rule has one home. Where the teaching above and these rules disagree, the rules win and the page has a bug to
fix. A `D` number is a row of the [decision log](decisions.md).

### Concurrency: `Concurrent`, `Parallel` and hidden waiting  **[implemented on Windows; names and mechanism proposed by Claude, unconfirmed]**

D35 (no function colouring, join on drop), D37 (the compiler injects the REPL's drain points where the program
already waits), D99 (IO never blocks the program by the program's own hand) and D103 (async waiting and threads are
two things, and `Task` is too generic a name) are built as two classes and one scheduler. `docs/concurrency.md` is
the user's page for all of it.

- **`Concurrent(function)`** (`library/concurrent.spite`) runs a function value (D17, D39) as a **state machine**
  the compiler writes (D176, below), on the program's own thread, starting straight away; reading the handle is what waits for its value (D134, below) and
  `drop()` waits for it, so scope exit is a join point. It is for work that waits: IO, sleeps, database calls
  later.
- **`Parallel(function)`** (`library/parallel.spite`) runs one on the program's **thread pool** (D135, below), with
  the same reading and join on drop. It is for work that computes.
- The type is never written: `Concurrent(file.read)` is a `Concurrent<String?>`, worked out from the function's
  return (see the inference row in the decision log). A function that returns nothing gives a `Concurrent<Nothing>`,
  which has no value to read and is waited for by dropping it: keep such handles in a list, and clearing the list
  or leaving its function waits for all of them.
- **Every attribute of both classes is private** (`_work`, `_results`, `_state`, ...), and the one public member is
  `finished: Boolean` (below). Nothing outside the class reaches the thread, the state machine or the work.

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

**`finished` never waits** (proposed by Claude, unconfirmed; SlopEngine's loaders polled
`WaitForSingleObject(parallel.thread, 0)` themselves, which was Windows only and reached into a field).
`handle.finished` is `true` once the function has returned. On a `Parallel` it reads the job's state with one
atomic load; on a `Concurrent` it reads the flag its state machine's frame carries once it finishes, so a state
machine only makes progress when the program waits somewhere (`while not reading.finished { program.sleep(1) }` is the polling loop). The same on
every system (`conformance/stage6/finished_polling`).

**The thread pool** (D135, decided by Mortaro; the shape below is proposed by Claude, unconfirmed).
**[implemented on Windows]** `library/thread_pool.spite` is a singleton, `ThreadPool()`, that the `Parallel`s share:

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
  wraps (below) instead of `Parallel.join_thread`.
- **Join on drop is kept.** The queue holds the job's function bound to a small `ParallelCall<T>` that owns the
  work and the result, never the `Parallel` itself, so dropping the last handle runs its `drop()` straight away,
  and that waits.
- **At exit** the pool is a singleton destroyed in reverse creation order (D142): it lets the workers finish what
  is queued, joins them and frees its lock. A `Parallel` whose `drop()` runs later finds its job done and does not
  touch the pool.
- **Hidden code (D147).** Starting a worker needs the address of a C function that calls the pool's private
  `_serve()`; that is the same pair of bodiless functions `Concurrent` uses, `entry_address()` and `address()`,
  moved from `Parallel` to `ThreadPool`, so the pool adds no new kind of compiler-supplied code. Everything else --
  the queue, the claim, the split -- is Spite.
- **Cost.** A `Parallel` makes about two dozen allocations, most of them the two function values (a `Spite.Function`
  is its own reflection object, D39), and no thread; the one-thread-per-call version it replaced made fewer
  allocations and one operating-system thread each. `conformance/stage6/thread_pool_reuse` runs a thousand
  `Parallel`s and checks every one ran on one of the pool's workers or on the thread that read it.
- **Not tested:** the Linux and macOS folders compile but have never run; a `Parallel` made on two non-worker
  threads at once before the pool has started (each could start it; the program's thread and a `Concurrent`'s
  helper are the only candidates).

**A value per thread, and a lock** (proposed by Claude, unconfirmed; SlopEngine keeps a command buffer per runner
through `TlsAlloc`/`TlsGetValue` and guards its entity ids with an `SRWLOCK` of its own).  **[implemented on
Windows]** Three small classes, each system's folder supplying the calls:

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

The alternative considered was a value per pool worker (`worker_index()` into a list): no system call, but it
covers only the pool's workers, not the program's thread or a `Concurrent`'s helpers. `conformance/stage6/thread_locals`,
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
  has finished), the result, `self`, the parameters, every local and temporary of the body (a name declared twice in
  nested blocks gets two fields), and one slot per wait for the frame it waits on.
- **The step function** runs the body with every local read from the frame, starts with a jump to the wait it
  stopped at, and answers `true` when the body returns and `false` at a wait that is not over. Jumping into a
  `while` or an `if` needs nothing, since no local lives on the C stack.
- **A wait** written as a call to a function that waits, anywhere an unconditional value is computed -- a
  statement, a `var`, an argument, an operand, a `{...}` in text, a condition -- becomes: hold the receiver, make
  the callee's frame with the arguments, step it until it answers `true` (returning `false` from this step while it
  does not), take its result and free it. It runs before the rest of the statement it is written in (a C statement
  expression cannot be jumped back into). A `while` whose condition waits becomes a loop that waits at the top of
  every pass and leaves when the condition is false.
- **Waits that run the loop in place.** A wait inside the right side of `and`/`or` or `==` on a nullable value, one
  reached through a function value, a union's dispatch or a constructor, and a `Concurrent` dropped inside a
  `Concurrent` are the plain calls: they wait by running the event loop where they are, as code outside a
  `Concurrent` does, which keeps every other state machine going but holds this one until the wait is over
  (`mortaros_missing_decisions.md` item 179). A `Concurrent` whose function has no state machine runs it to the end
  when it is made.
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
runs, and a `Parallel` join joins and then runs whatever is ready once. Every other program gets none of it: no
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

**Soundness.** A program that starts a thread (a `Concurrent`'s helpers, a `Parallel`, or `--repl-port`) is compiled
with `SPITE_THREADS`: every retain and release is an atomic operation, and the `--debug-memory` table takes a lock.
Every other program keeps the plain counts. Spite code on the program's thread only ever changes hands at a wait,
so state machines need nothing more. D35's rule is checked for `parallel_each_` (below); what a `Parallel(function)`
touches is not, so two threads writing one field, or one writing a field another reads, is still the program's
mistake there -- and with reference-counted fields it can free a value another thread is reading.
Two smaller gaps, untested: a singleton's first use from two threads at once is not guarded (each could make
one), and a REPL client's `exit` while a helper thread is blocked reading the console may wait on the C runtime's
lock on that stream when the process exits.

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
elements may share.

- **The race rule, D35 as a compile check.** The member, every function of the element's class it calls
  (transitively, by name), and every `filter_` member in the chain may read and write only the element's
  attributes that hold a plain value -- a number, `Boolean`, `String`, enum or `Symbol`, or a `T?` of one -- any
  singleton (the library's, and the program's own, which D183 makes safe below), and their own parameters and
  locals. An attribute holding an object, a list, a dictionary, a function value or a program's own singleton is an
  error naming the attribute, its type and the one-thread form (`diagnostics/parallel_reach`):
  `'parallel_each_follow' runs 'follow' on many elements at once, so it may reach only its own 'Boid' attributes
  that hold values, and its locals (D35): 'leader' holds a 'Boid?', which another element may share. ...`. A list
  of numbers or text has no members, so `parallel_each_` on one is an error naming `each_`.
- **What it cannot see:** one object listed twice runs on two threads at once; a local made from something the
  member was handed (it is handed nothing, so only through a library singleton such as `Memory`); a list of a
  `type` or union (the element's class is not known, so it is an error for now). D35's open questions -- shared
  state across threads and cross-element reads -- stay open.
- `conformance/stage6/parallel_each` runs ten thousand elements twice, filtered once, and a list of one and of none.

**A singleton a `Parallel` reaches takes a lock** (D183, decided by Mortaro; the fallback of D184's plan, the
details proposed by Claude, unconfirmed).  **[implemented]** In a program that makes a `Parallel` or runs a
`parallel_each_` pass, each function of a program singleton (not `library/`'s) that can change after it is made --
one of its functions assigns one of its attributes outside the constructor, or it holds an object, a list, a
dictionary or a function value -- is emitted as `<name>___unguarded`, and `<name>` becomes a wrapper that takes the
singleton's own lock around the call. The lock is reentrant by owner thread (a `_Thread_local` marker's address),
so a singleton calling itself does not take it twice. A singleton that never changes gets nothing, and a program
without `Parallel` gets no lock at all; a `--hot-reload` build is not guarded yet. Every call from anywhere locks,
not only the ones from a `Parallel`: telling them apart needs a whole-program walk that is not built. Not built:
D184's cheaper forms, and D183's check that such a singleton hands out only numbers, text, copies or other safe
singletons. `conformance/stage6/singleton_guard` (four `Parallel`s filling a program singleton's `Dictionary` while
the program's thread keeps writing to it). D179's rule for `Parallel(function)` itself -- only its own instance and
its locals -- is not checked; only `parallel_each_` is.

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

**Not built:** HTTP, cancelling a `Concurrent`, a `Concurrent` made on
a thread that is not the scheduler's (it runs on the spot instead), and running any of this on Linux or macOS,
whose folders are held to compiling.
