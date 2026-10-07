# Concurrency: waiting without colouring

The specification of [Concurrency: waiting without colouring](../docs/concurrency.md): every rule of this part of the language, with
its edge cases and its exact error texts. The page teaches; this one decides, and where they disagree, the page
is the bug.

## Concurrency: `Concurrent`, `Parallel` and hidden waiting

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
describes that handle's class too, from a stand-in at its defaults ([reflection.md](../docs/reflection.md)). Such a handle
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
  a point the state machine returns from would let another `Concurrent` on the same thread into it) get none. In a
  `--hot-reload` build a program class's state machine starts through a slot a reload swaps, and a reload waits
  while a `Concurrent` is stopped inside a function it swaps
  ([repl.md](repl.md), "A reload waits for the waits of what it swaps").
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
  - A local that holds a `Concurrent`, a list of them, or an object of a program class whose attributes hold one
    (through a `T?`, a list or another such object, however deep, a class seen once on the way), waits at the end
    of its scope for each handle it holds the last reference to, before letting it go; `clear()` on a list of
    `Concurrent`s waits for each of them first. An assignment that replaces such a value (a local assigned again,
    an attribute assigned again, `keeper.job = Concurrent(ring)` a second time) stores the new value first, then
    waits the same way for the old one, then lets it go. The drop that follows then finds the work finished
    (`conformance/stage6/dropped_handle_waits`).
  - A `type`'s function held as a value (`var nap = sleepy.nap`) is a value of its own that holds the shape's value,
    and calling it waits like a call through any function value, by class (`conformance/stage6/type_function_values`).
  - A function of a singleton that holds the singleton's lock has no state machine (a lock held across a point the
    state machine returns from would let another `Concurrent` on the same thread into it), so a wait inside it does
    not run the event loop: it blocks there, as it would outside a `Concurrent`, and every other `Concurrent` waits
    until the call is done, which keeps the call one step (`conformance/stage6/locked_waits`). Each locked call
    counts itself on its thread to decide this, only in a program that has both locks and waits.
  What still waits by running the event loop where it is (holding its `Concurrent` while every other state machine
  goes on): dropping a `Concurrent` held in some other way inside a `Concurrent` (an element taken out of a list
  with `remove_at` or `remove_where`, an object let go by a release no local or assignment makes), a wait through
  a `String`'s or a number's function held as a value (a reopening's function that waits), a join of a
  `Concurrent` inside a function of a singleton that holds its lock. Two such waits that each wait for the other could never end, so a join that
  waits in place for a `Concurrent` whose state machine is running further down the same stack halts at the join
  (`waits_for_its_own_caller=true`); a join that is a point to return from just waits, and the machine below it
  carries on, unless the `Concurrent` it joins waits, through joins of its own, for the one that is joining: two
  `Concurrent`s that each drop the other's last handle would wait for each other for ever, so the join that closes
  the circle halts there (`joins_a_concurrent_that_waits_for_this_one=true`,
  `conformance/stage6/concurrent_wait_cycle`). The scheduler keeps, for each `Concurrent` waiting at such a join,
  the one it waits for, only in a program that makes a `Concurrent`. A `Concurrent` whose function has
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
`Concurrent` made on the caller's thread is alive and no REPL is listening there, nothing else could run on that
thread meanwhile, so the wrapper makes the plain blocking call.

**The scheduler** (`library/scheduler.spite`, a singleton; each operating system's folder reopens it with the
thread, event and clock calls) keeps one loop per thread (`library/scheduler_loop.spite`, made the first time a
thread waits and found again through a `ThreadLocal`), so a `Concurrent` made inside a `Parallel`'s work runs on that
pool thread's own loop and overlaps with the others made there, with nothing for the program to write
(`conformance/stage6/concurrent_in_parallel`). The REPL's commands and reloads are answered only on the program's
own thread. Each loop keeps the frames of the `Concurrent`s that have not finished, the deadlines of the
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
  here.unfinished_polls_with_no_frame_stepped=1000000` (`check.sh`, "polling"). Where the compiler can see it, it is a
  compile error first: a `while` whose condition reads `finished` of a `Concurrent` held in a local, and which calls
  nothing in its condition or its body, is `this loop reads 'napping.finished' and calls nothing, so nothing steps
  'napping' between two reads and it never finishes` (`diagnostics/unstepped_poll`). A loop that calls anything is
  left to the count, since the call may step it.
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
[optimizations.md](../docs/optimizations.md#thread-safety-for-singletons-the-cheapest-safe-form):

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
   same object for the rest of the program ([optimizations.md](../docs/optimizations.md#a-singletons-attribute-that-never-changes-is-read-in-place)).

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
[optimizations.md](../docs/optimizations.md#a-counted-loop-of-calls-to-one-singleton-takes-its-lock-once)
(`conformance/stage6/coarse_locks`, `benchmarks/a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once`). The compiler cannot take no lock at all where one
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
compiler uses, measured on the shape of a game engine's columns
(`benchmarks/a_singletons_reading_functions_do_not_exclude_each_other`: eight systems reading one singleton per row,
17 ms against 103 ms for C taking a mutex per read). `conformance/stage6/singleton_reads` reads while the
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
compile error** too ([memory.md](memory.md), `diagnostics/weak_across_threads`).

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

**A task writes a value attribute only of an object handed to it.** When the work of `Parallel(...)` writes an
attribute holding a value (a number, a `Boolean`, an enum or text) of an object that was not handed over as above
(`this` for `Parallel(own_function)`, a parameter, an attribute, or a local named again or passed before the
`Parallel` line), it is a compile error: another thread may write the same attribute at the same time, and one write
would be lost. `'Parallel(kept.bump)' writes 'count' of 'kept' on another thread, and 'kept' is not handed over to
it, ...` names the ways out: hand over a new object, answer the value from the work and read the task's result, or
keep the value in a singleton, which is made safe for threads. A write the work makes inside a function it passes to
a `Lock` attribute's `while_locked(...)` is not counted, since the lock makes it one step. A singleton's own
attributes are never counted either. `diagnostics/parallel_shared_value_writes`.

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

**Calls in a row run at once.** Two or more statements in a row, each `receiver.function()` with no arguments and
its value unused, where `receiver` is a name or a path of attributes, the function answers nothing and belongs to a
class of the program that is not a singleton, run at once when every pair of them is independent and each reaches
a loop: every one but the last runs on the thread pool, the last on the calling thread, and all are joined before
the next statement. Two calls are independent when neither writes what the other reads or writes, taken through
every function each one reaches, the compiler's own code included: an attribute of a class (two objects of one
class count as one), the items of a list or dictionary (two attributes holding lists count apart only when each
holds a list made for it and never handed anywhere else, and two lists of one kind count as one otherwise), or
memory reached through an address; and when neither prints, reads or writes a `File`, a `Directory` or a
`Socket`, waits, calls anything outside the program but plain arithmetic, the clock and memory, or calls through a
function value. Classes the program never makes are left out of the comparison. Nothing is overlapped inside a
singleton's own functions, inside a counted loop that holds a singleton's lock, in the standard library, or in a
`--hot-reload`, `--repl` or `--development` build. When an overlapped call crashes, the program halts with that
call's report; if two crash at once, one of the two reports is printed. A program in which no calls run at once
compiles exactly as before, and one in which some do uses the thread pool, as a program that writes `Parallel`
does.

---

Next: [Standard library](standard_library.md).
