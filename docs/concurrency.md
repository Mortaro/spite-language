# Concurrency: waiting without colouring

> **What is built** (D35, D37, D99, D103; the names and the mechanism are proposed by Claude, unconfirmed):
> `Concurrent(function)` runs a function on a fiber of the program's own thread and `Parallel(function)` runs
> one on a thread of its own; `.wait()` gives back what it returned, and dropping the handle waits for it.
> Where a program already waits -- `Program().sleep`, `Console.read_line`, reading or writing a `File`, a
> `Socket`'s `accept_client` and `read_line`, waiting on a `Concurrent` or a `Parallel` -- the compiler turns the
> wait into a suspension, so another fiber runs meanwhile, and a `--repl-port` build answers its commands there.
> Windows runs all of it; the Linux and macOS folders are held to compiling. **Not built:** a thread pool,
> `parallel_each_` templates, HTTP, and D35's race rule for what a parallel function may touch.

There is no `async` and no `await` in Spite, and there never will be. In JavaScript or C# a function that waits
declares itself `async`, which changes its return type and forces every caller to `await` it, and every caller's
caller, all the way up: the waiting leaks into every signature above it. Spite keeps waiting a property of the
*call site*: any function can be run concurrently by whoever calls it, and a function never says whether it waits.

## `Concurrent`: work that waits

`Concurrent(file.read)` takes a function value (a function bound to its instance, as in
[functions_and_operators.md](functions_and_operators.md)) and starts running it straight away. `.wait()` gives
back what the function returned -- a `String?` for `file.read` -- and the type is worked out from the function,
so it is never written.

```spite title=concurrent_tour/sleeper.spite
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
    Program().sleep(milliseconds)
    log.append("{name} wakes up")
    return name
}
```
```spite title=concurrent_tour/concurrent_tour.spite entry
var console = Console()
var log = List<String>()

func ConcurrentTour() {
    var slow_sleeper = Sleeper("slow", 150, log)
    var fast_sleeper = Sleeper("fast", 10, log)
    var slow = Concurrent(slow_sleeper.nap)
    var fast = Concurrent(fast_sleeper.nap)
    log.append("both are asleep")
    var slow_name = slow.wait()
    var fast_name = fast.wait()
    log.append("{slow_name} and {fast_name} are back")
    var notes = File(".spite-cache/concurrency_notes.txt")
    notes.write("written before the read")
    var reading = Concurrent(notes.read)
    var content = reading.wait()
    if content {
        log.append("read: {content}")
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

### What the compiler does at a wait

A `Concurrent` runs on a **fiber**: a stack of its own on the program's one thread, switched to and from without
the operating system scheduling anything. When code on a fiber reaches a wait, the compiler has already written
that wait as a suspension, and the program's scheduler (`library/scheduler.spite`, Spite over the system's
fibers) runs whatever else is ready:

| The program writes | While it waits |
|---|---|
| `Program().sleep(milliseconds)` | the fiber is parked until its time comes |
| `Console.read_line()`, reading or writing a `File`, a `Socket`'s `accept_client` or `read_line` | the one blocking system call runs on a short-lived helper thread, and the fiber is parked until it returns |
| `a_concurrent.wait()`, or dropping it | the fiber is parked until that function returns |
| `a_parallel.wait()`, or dropping it | the thread is joined, then anything ready runs once |

Only fibers run Spite code on the program's thread, and a fiber only ever stops at one of these points, so no two
pieces of Spite code touch the program's state at the same time. The helper thread runs the system call and
nothing else.

**When blocking is faster, the compiler blocks.** If nothing else could run -- no `Concurrent` is alive and no
REPL is listening -- a wait is the plain blocking call, with no fiber, no helper thread and no scheduler. A script
that only reads files never pays for any of this. It is the same rule as everywhere else in Spite: you write
what you mean, and the compiler picks the fast way to do it.

### Dropping a handle waits for it

A `Concurrent` is reference counted like everything else, and when the last reference goes, its `drop()` waits
for the function to finish. So a block that started work cannot end while the work is still running, and no
result is ever lost or left running in the background: leaving a scope is a join point.

```spite title=joined_on_drop/joined_on_drop.spite entry
var console = Console()

func JoinedOnDrop() {
    start_and_forget()
    console.print("the scope is left only after ring has finished")
}

func start_and_forget() {
    var _ringing = Concurrent(ring)
}

func ring() {
    Program().sleep(10)
    console.print("ring")
}
```
```output
ring
the scope is left only after ring has finished
```

## `Parallel`: work that computes

`Concurrent` never makes a program faster at computing: every fiber shares one thread. For work that keeps a core
busy, `Parallel(function)` runs the function on a thread of its own, and the same `.wait()` and join-on-drop
apply.

```spite title=parallel_tour/summer.spite
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
```spite title=parallel_tour/parallel_tour.spite entry
var console = Console()

func ParallelTour() {
    var small = Summer(1000000)
    var large = Summer(3000000)
    var small_sum = Parallel(small.total)
    var large_sum = Parallel(large.total)
    var small_total = small_sum.wait()
    var large_total = large_sum.wait()
    console.print(small_total, large_total)
}
```
```output
499999500000 4499998500000
```

A program that makes a `Parallel` (or a `Concurrent`, or is built with `--repl-port`) counts references with
atomic operations, because an object can now be shared between threads; every other program keeps the plain,
cheaper counts. What a `Parallel` function may touch is not checked yet: give it an instance of its own, as each
`Summer` above has, and do not change that instance until the result is back.

## The REPL answers at the waits

A `--repl-port` build (see [repl.md](repl.md)) serves its commands **on the program's own thread, at the same
waits**. The socket thread only reads a command and hands it over; the scheduler answers it the next time the
program waits, and hands the answer back. Every command therefore sees the program between two steps, never in
the middle of one: a frame loop that ends in `Program().sleep` is answered between frames, a tool waiting on
`Console.read_line` is answered while it waits, and once the entry constructor returns, the program waits for
nothing but commands until a client sends `exit`. None of this is written by the program.

```spite title=frame_loop/frame_loop.spite entry
var console = Console()
var frame = 0
var running = true

func FrameLoop() {
    while running and frame < 200 {
        frame = frame + 1
        Program().sleep(10)
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

A program that never waits would never be answered, so a `--repl-port` build of one is a compile error. The
rule the compiler applies: when none of the program's own code waits anywhere (none of the calls in the table
above), a `while` loop in it may never let the REPL in, and the error points at that loop:

```
frames.spite:6: error: a --repl-port build answers its REPL where the program waits (Program().sleep,
Console.read_line, reading or writing a File or a Socket, waiting on a Concurrent or a Parallel) and after the
entry constructor returns, and this program never waits: if this loop does not end, the REPL never answers.
Wait somewhere in the loop, such as 'Program().sleep(1)' at the end of a frame
```

A program without a loop always reaches the end of its constructor, where it is served, so it needs no wait.
