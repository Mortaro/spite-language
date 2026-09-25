# Optimisations the compiler makes on its own

The compiler makes a program faster and smaller without being asked. This page lists every such optimisation: what
it does, when it applies, whether it is **built** or only **planned**, and what, if anything, you could ever
notice. For almost all of them the honest answer is: **you do not need to do anything**. Write the plain program;
the compiler does the rest, and the program means exactly what its source says.

Two rules decide what belongs here.

**Zero runtime, and everything tree-shakeable** ([D177](../manual.md#decision-log)). A program that does not use
a feature carries none of it. Nothing needs a scheduler, an interpreter or a registry shipped beside the program:
the work is done at compile time instead. REPL, live reload and debugging features may cost something while the
program runs, but only in the builds that ask for them ([D143](../manual.md#decision-log)).

**Hidden optimisations are good, hidden costs are bad** ([D36](../manual.md#decision-log)). Code that runs faster
than you expect is a free win, so the compiler optimises silently and never asks you to mark anything. Code that
runs *slower* than you expect is the only real surprise, so every cost that remains is written down on this page,
under the optimisation it belongs to.

**Internals stay ordinary objects where you inspect them** ([D143](../manual.md#decision-log)). A `--repl`,
`--repl-port`, `--hot-reload` or `--development` build is an *inspectable* build: nothing is tree-shaken, and
the standard library's internals -- `Memory`, `Build`, `TypedMemory<T>` -- are ordinary objects that reflection
(`.instances`, `.attributes`) sees. Every other build, ordinary or `--optimized`, is a production build, and only
there are the two optimisations that hide something applied: tree shaking and static singletons. Everything else
on this page changes how fast the program runs, not what it is made of, so it applies in every build. The
**Builds** column below says which.

What you can observe at all is short: the counts `--debug-memory` prints, where `.memory` says a value lives, the
order in which a fused chain calls your member functions, and speed. Apart from that order, which only a member
function with a visible effect can show, no optimisation changes what a program prints or computes.

| Optimisation | Status | Builds | What you might notice |
|---|---|---|---|
| [Tree shaking the generated C](#tree-shaking-the-generated-c) | built | production | smaller C; an inspectable build keeps everything |
| [Deciding conditions at compile time](#deciding-conditions-at-compile-time) | built | every | nothing: the branch not taken is not in the program |
| [Reflection, symbols and registries only where read](#reflection-symbols-and-registries-only-where-read) | built | every | nothing |
| [Template chains run as one loop](#template-chains-run-as-one-loop) | built | every | fewer allocations; member functions run element by element |
| [Appending to text in place](#appending-to-text-in-place) | built | every | fewer allocations |
| [The compiler places memory](#the-compiler-places-memory) | built | every | fewer allocations; `.memory.section` |
| [Singletons: made on first use, never counted](#singletons-made-on-first-use-never-counted) | built | every | the constructor runs at first use |
| [Singletons that hold nothing are static objects](#singletons-that-hold-nothing-are-static-objects) | built | production | one allocation fewer each; not in `.instances` |
| [Atomic reference counts only with threads](#atomic-reference-counts-only-with-threads) | built | every, decided per program | nothing |
| [Boxing only where a value travels as a shape](#boxing-only-where-a-value-travels-as-a-shape) | built | every | one allocation per boxed value |
| [Concurrency machinery only where it is used](#concurrency-machinery-only-where-it-is-used) | built | every, decided per program | nothing |
| [Hidden async/await as compile-time state machines](#hidden-asyncawait-as-compile-time-state-machines) | built | programs that make a `Concurrent` | one heap frame per waiting call; a wait inside an expression runs first |
| [The thread pool only where a `Parallel` is made](#the-thread-pool-only-where-a-parallel-is-made) | built | every, decided per program | nothing until the first `Parallel` |
| [Singletons a `Parallel` reaches take a lock](#singletons-a-parallel-reaches-take-a-lock) | built (the fallback) | every, decided per program | an uncontended lock per call, only with `Parallel` |
| [REPL, live reload and debug machinery only in those builds](#repl-live-reload-and-debug-machinery-only-in-those-builds) | built | the builds that ask for it | nothing in an ordinary build |
| [Smaller ones](#smaller-ones) | built | every | nothing |
| [Reading an address is one machine operation](#reading-an-address-is-one-machine-operation) | built | every | nothing |
| [An allocator set after construction is where the object is made](#an-allocator-set-after-construction-is-where-the-object-is-made) | built | every | the arena's blocks are the allocations; sixteen bytes more per object of a class given an allocator |
| [Proofs that survive a call](#proofs-that-survive-a-call) | built | every | a proof after a call that may change it is written again |
| [Thread safety for singletons, the cheapest safe form](#thread-safety-for-singletons-the-cheapest-safe-form) | planned (the lock fallback is built) | | |
| [Copies that cost nothing](#copies-that-cost-nothing) | planned | | |
| [Other planned optimisations](#other-planned-optimisations) | planned | | |

## Built

### Tree shaking the generated C

**What it does.** After the program is generated, the compiler keeps only the C that `main` can reach: every
function nothing calls, from your classes, `library/` or the compiler's own prelude, is dropped along with its
prototype (`bootstrap/source/generation/tree_shaker.spite`). A small program's C goes from about 5 800 lines to
about 2 000. The compiler does this itself rather than leaving dead code for the C compiler to find, so it holds
whichever C compiler you bring.

**When.** Production builds only. An inspectable build -- `--repl`, `--repl-port`, `--hot-reload` or
`--development` -- keeps everything, so live reload has every function to swap and the REPL can reach every
internal ([D143](../manual.md#decision-log), [compiler.md](compiler.md#development-builds-and-tree-shaking)).

**What you notice.** Nothing, except that `--c-source` writes less. A function nobody calls, outside a generic class, is still
compiled and checked, so a mistake in it is still reported ([D140](../manual.md#decision-log)) -- it just is not in the
binary. **Built** (the tree shaker and `--development` rows of the [decision log](../manual.md#decision-log),
2026-09-24).

The same pass decides which native symbols are looked up. A `DynamicLibrary` looks up every symbol the program
calls when it opens, and a symbol is now looked up only when a function that calls it survived the shaking: a
program that never uses `Watcher` does not look up `ReadDirectoryChangesW`, though `library/windows/watcher.spite`
opens the same `kernel32.dll` as `Program.sleep`. **What you notice.** Fewer allocations under `--debug-memory`,
since each lookup made two short-lived strings (`conformance/stage6/singleton_counts` went from 230 to 134), and a
missing symbol that only unused code names no longer stops the program when the library opens. `--development`
builds still look up every symbol. **Built** (2026-09-25, with `Watcher`; proposed by Claude, unconfirmed).

### Deciding conditions at compile time

**What it does.** A condition the compiler can answer while compiling is answered then, and only the branch taken
is generated. The branch not taken is not in the program at all -- not skipped at run time, *absent* -- so it may
even use things that would not compile for this build. That covers:

- a field of [`Build`](programs.md#compile-time-settings-build): every `Build` field is a constant of the built
  program, set by a flag or taken from its declared default ([D84, D85](../manual.md#decision-log));
- a codegen value, `if $is_magic { }`, and a test on a codegen type, `if $value_type == List { }`
  ([metaprogramming.md](metaprogramming.md#tree-shaking));
- a class test the value's type already answers, `if item == $wanted_type`, and one that can never be true for one
  instantiation of a generic, which folds to `false` there instead of being an error
  ([D167](../manual.md#decision-log));
- `$system_type.has_function('run_each')` ([D114](../manual.md#decision-log));
- `not`, `and`, `or`, `==` and `!=` over any of these.

A function of a generic class is then compiled for one instantiation only when code that survived folding names
it, so a helper reached only from a removed branch is never checked against a type it cannot work with.

**When.** Every build, inspectable ones included. A folded branch hides nothing from the REPL: the branch not
taken is not part of this program, since a `Build` field or a codegen value is a fact of the build, not a value
that could change while it runs.

**Example.** `Describer<Int>` never contains `value.count()`, which an `Int` does not have:

```gdscript title=folded_branch/describer.spite
generic $value_type

func describe(value: $value_type): String {
    if $value_type == List {
        var count = value.count()
        return "a list of {count}"
    }
    return "one value, {value}"
}
```
```gdscript title=folded_branch/folded_branch.spite entry
var console = Console()

func FoldedBranch() {
    var numbers = List<Int>()
    numbers.append(4)
    numbers.append(9)
    var lists = Describer<List<Int>>()
    var first = lists.describe(numbers)
    var singles = Describer<Int>()
    var second = singles.describe(7)
    console.print(first)
    console.print(second)
}
```
```output
a list of 2
one value, 7
```

And a `Build` field given as a flag (`--optimized`) folds the same way; the other message is not in the program:

```gdscript title=build_folding/build_folding.spite entry build=optimized:true
var console = Console()
var build = Build()

func BuildFolding() {
    if build.optimized {
        console.print("an optimized build")
    } else {
        console.print("an ordinary build")
    }
}
```
```output
an optimized build
```

**What you notice.** Nothing. Changing a `Build` field means rebuilding, which is what a build fact is; a setting
that should change without rebuilding belongs to [`Environment`](programs.md), which is read when the program runs.
Names used only inside a removed branch still count as used, so the unused rule judges the source, not one
instantiation. **Built** (section 9 of the [manual](../manual.md#decision-log), D84, D85, D167, and the
"only what survives folding is compiled" row).

### Reflection, symbols and registries only where read

**What it does.** Reflection is decided at compile time, so the compiler knows exactly what a program reads and
emits only that ([D42, D57](../manual.md#decision-log)): a class object's `.attributes`, `.functions` and
`.namespace`, `value.attributes`, `value.memory`, `attribute.value`, the per-class `Person.instances` registry (a
class is only tracked when something asks for its instances), `Spite.Class.instances`, and a class's `to_debug()`.
A symbol literal is an entry of a table the compiler writes with only the symbols the program uses; it is
constant text, so storing and comparing symbols allocates nothing ([D70](../manual.md#decision-log)). A Symbol
codegen template exists only for the names a program calls: a program that never calls `sum_price()` has no
`sum_price`. An enum's reflection is the same kind of template (D180): `Symbol<Phase>` and a name pattern's hole
become one call per value, with `phase.value` the constant itself, so no table of an enum's values, names or
order exists at run time -- walking one costs exactly the calls it expands to, and not walking one costs nothing.

**When.** Every build: what a REPL reads is compiled into the REPL build, so it is read there too. **What you
notice.** Nothing: reflection may be as detailed as it likes, because a program that never reads it carries none
of it. The list behind `.instances` is the compiler's bookkeeping, like the list of singletons to destroy at
exit, so `--debug-memory` does not count it. **Built.**

### Template chains run as one loop

**What it does.** `teams.filter_is_active().map_lead().sum_age()` reads as three steps, and that is what it means,
but the compiler writes it as one loop over `teams` with no list in between: each element is tested, mapped and
added before the next one is read ([D105](../manual.md#decision-log)).

**When.** A template called directly on a `filter_` or `map_` call, on a `List` or `Dictionary`; the steps in the
middle are `filter_` and `map_` (to a member that is a class), and the last may be any template. A list you name
and keep (`var active = teams.filter_is_active()`) starts a new chain, and a chain the compiler cannot write as
one loop runs step by step, which means the same. See [collections.md](collections.md#chains-run-as-one-loop).

**Example.** The one thing you could ever see: a member function with a visible effect runs element by element in
a fused chain, where the written-out steps run it on every element before the next step starts.

```gdscript title=fused_order/team.spite
var console = Console()
var name = ""
var active = false
var size = 0

func Team(new_name: String, new_active: Bool, new_size: Int) {
    name = new_name
    active = new_active
    size = new_size
}

func is_active(): Bool {
    console.print("checking", name)
    return active
}

func counted_size(): Int {
    console.print("counting", name)
    return size
}
```
```gdscript title=fused_order/fused_order.spite entry
var console = Console()

func FusedOrder() {
    var teams = List<Team>()
    teams.append(Team("red", true, 3))
    teams.append(Team("blue", false, 5))
    teams.append(Team("green", true, 4))
    var fused = teams.filter_is_active().sum_counted_size()
    console.print("one loop:", fused)
    var active = teams.filter_is_active()
    var stepwise = active.sum_counted_size()
    console.print("step by step:", stepwise)
}
```
```output
checking red
counting red
checking blue
checking green
counting green
one loop: 7
checking red
checking blue
checking green
counting red
counting green
step by step: 7
```

**What you notice.** Fewer allocations under `--debug-memory` (`conformance/stage6/fused_chain_allocations`
pins four chains run a thousand times at 11 allocations, where the steps written out take 16 010), and the order
above. Member functions a template reads should not depend on that order; ones that only compute never do.
**Built** (D105 and its "chain of member templates compiles to one loop" row).

### Appending to text in place

**What it does.** `text = text + piece`, or `text = "{text}{piece}"`, used to copy the whole text on every
append, which makes the most common loop there is quietly quadratic. When nothing but that variable holds the
text, the compiler grows its buffer in place instead (doubling its capacity); when something else holds it too,
it copies once, with room to grow, so the other holder still sees the text it had. 100 000 appends went from
6.9 s to 0.23 s.

**When.** An assignment to a local variable or parameter of type `String` whose new value is a join starting with
the variable itself. Not for attributes (a call among the pieces could replace the attribute mid-append), and not
when a piece mentions the variable again (`"{text}{text}"`); those compile as an ordinary join.

**Example.** Text stays immutable as observed: `kept` still holds the text it was given.

```gdscript title=text_append/text_append.spite entry
var console = Console()

func TextAppend() {
    var line = "a"
    var kept = line
    line = "{line}b"
    var piece = "c"
    line = line + piece
    console.print(line, kept)
}
```
```output
abc a
```

**What you notice.** Fewer allocations and a faster loop. Nothing else: build text the obvious way. **Built**
(the "appending to a text the variable alone holds" row of the [decision log](../manual.md#decision-log);
[values_and_types.md](values_and_types.md)).

### The compiler places memory

**What it does.** A program has one way to ask for raw memory, `heap.allocate(bytes)` on `Memory.Heap()`, and one
way to give it back, `heap.free(address)`. Where the bytes live is the compiler's choice ([D108](../manual.md#decision-log),
[manual section 10](../manual.md#10-memory--implemented)):

- **Register:** a number's own memory (`var _memory = heap.allocate(4)` in `library/int.spite`) is its C
  scalar. A number is never an object.
- **Frame:** an allocation a function frees itself, in the same block, whose address it only reads and writes
  through, copies, compares, turns into `text` or hands to a `TypedMemory` -- never stores, returns, resizes or
  passes anywhere else -- gets a
  slot in the function's own frame: 256 bytes, or exactly a literal size up to 256. A larger size at run time
  still goes to the heap, and the program's text is the same either way.
- **Constant:** the characters of a text literal are part of the program, never counted or freed.
- **Heap:** everything else.

**Example.** The buffer below is in the frame, so the heap does not change while it is used:

```gdscript title=frame_placement/frame_placement.spite entry
var console = Console()
var heap = Memory.Heap()
var longs = TypedMemory<Long>()

func FramePlacement() {
    var total = sum_of_squares(6)
    console.print("sum of squares:", total)
}

func sum_of_squares(count: Int): Long {
    var before = heap.live_allocations()
    var squares = heap.allocate(count * 8)
    var index = 0
    while index < count {
        longs.write_value(squares, index, index * index)
        index = index + 1
    }
    var total: Long = 0
    index = 0
    while index < count {
        total = total + longs.read_value(squares, index)
        index = index + 1
    }
    var during = heap.live_allocations()
    heap.free(squares)
    console.print("heap allocations while summing:", during - before)
    return total
}
```
```output
heap allocations while summing: 0
sum of squares: 55
```

**What you notice.** Fewer allocations under `--debug-memory`, and `value.memory.section` answering `'stack'`,
`'heap'` or `'constant'` ([memory.md](memory.md#where-a-value-lives-memory)). You never choose the stack
yourself: there is no second way to allocate, so there is no address to keep past a return by mistake.
**Built** (D108 and its placement rows).

### Reading an address is one machine operation

**What it does.** `address.read_long(16)`, `address.write_float(8, value)` and the other reads, writes and
atomics of `Memory.Address` are primitives of the language, like `+` ([D178](../manual.md#decision-log)): the
compiler writes each one where it is called, as the single load, store or atomic instruction, with no call and
no check. `copy_to` and `compare_bytes` are written the same way, as the C library's copy and comparison.

**When.** Always; `library/` is the only place that may call them, so every `String`, `List` and `Dictionary`
reads its memory this way.

**What you notice.** Nothing: there is no other way these could run. **Built** (D178).

### An allocator set after construction is where the object is made

**What it does.** `var spark = Particle("spark", 1.5)` followed by `spark.memory.allocator = arena` reads as
though it made the particle on the heap and then moved it. The compiler makes it in `arena` from the start: the
two lines become one construction that asks the arena for the memory, with nothing allocated twice and nothing
decided while the program runs ([D152](../manual.md#decision-log), [memory.md](memory.md#choosing-an-allocator-memoryallocator)).
The same holds for `var kept = ash.copy()` followed by `kept.memory.allocator = arena`.

**When.** Always, for the line right after the one that makes the object; anywhere else setting the allocator is
an error (D153).

**What you notice.** Under `--debug-memory`, an object made in an arena is not an allocation of its own: the
arena's blocks are. A class some line gives an allocator is sixteen bytes larger per object (two hidden pointers:
the allocator, and the function that gives the memory back), on every object of that class, heap ones included;
no other class changes. Setting `Memory.Heap()` is the default and costs nothing. **Built** (D152, D153).

### Singletons: made on first use, never counted

**What it does.** A singleton is made the first time something asks for it, not when the program starts, so a
program pays only for the singletons it reaches. It is never reference counted ([D142](../manual.md#decision-log)):
fetching one is a load from a static slot, with no count to raise or lower, so threads sharing it never contend on
it (two threads fetching one 20 million times each took 0.8 s counted and 0.04 s not). At exit every singleton is
destroyed in reverse creation order, its `drop()` running then, and every `DynamicLibrary` after all of them.

**When.** Every singleton. In a program that starts threads the first fetch takes a lock of its own, so two threads
asking at once still get one object; every later fetch is one load, and a program with no threads has no lock.

**Example.** The `Greeter` is made when the first `Room` is, not before `starting`:

```gdscript title=lazy_singleton/greeter.spite
singleton

var console = Console()
var greetings = 0

func Greeter() {
    console.print("greeter made")
}

func greet(name: String) {
    greetings = greetings + 1
    console.print("hello", name)
}

func drop() {
    console.print("greeter dropped after", greetings, "greetings")
}
```
```gdscript title=lazy_singleton/room.spite
var greeter = Greeter()
var name = ""

func Room(new_name: String) {
    name = new_name
}

func enter() {
    greeter.greet(name)
}
```
```gdscript title=lazy_singleton/lazy_singleton.spite entry
var console = Console()

func LazySingleton() {
    console.print("starting")
    var hall = Room("hall")
    hall.enter()
    var kitchen = Room("kitchen")
    kitchen.enter()
    console.print("done")
}
```
```output
starting
greeter made
hello hall
hello kitchen
done
greeter dropped after 2 greetings
```

**What you notice.** A singleton's constructor runs at its first use, which is only visible if it prints. A
`drop()` that fetches a singleton made after its own (so already destroyed) halts with a message saying to keep
that singleton in an attribute ([D141](../manual.md#decision-log),
[classes_and_files.md](classes_and_files.md#singletons)). `--debug-memory` still names an object a program leaked,
even one that points at a singleton. **Built** (D8, D142, D141).

### Singletons that hold nothing are static objects

**What it does.** A singleton with no attributes and no `drop()` -- `Memory.Heap`, `TypedMemory<T>` (one per
element type), and `Build`, whose attributes are all settings folded into the program -- is one static object:
never allocated, never counted, never freed. `Memory.Heap()` costs nothing, and every program allocates once
fewer for each.

**When.** Production builds only ([D143](../manual.md#decision-log)). In an inspectable build -- `--repl`,
`--repl-port`, `--hot-reload` or `--development` -- each is an ordinary singleton: allocated at first use, one of
its class's `.instances`, and destroyed at exit. Since it holds nothing, it may be made again if something
destroyed after it asks for it at exit, so the order of teardown never matters for it.

**Example.** The same program in both kinds of build: the production build has no `Memory.Heap` object to list.

```gdscript title=production_internals/production_internals.spite entry
var console = Console()
var heap = Memory.Heap()

func ProductionInternals() {
    var bytes = heap.allocate(8)
    heap.free(bytes)
    var heaps = Memory.Heap.instances.count()
    console.print("Memory.Heap objects:", heaps)
}
```
```output
Memory.Heap objects: 0
```

```gdscript title=inspectable_internals/inspectable_internals.spite entry build=development:true
var console = Console()
var heap = Memory.Heap()

func InspectableInternals() {
    var bytes = heap.allocate(8)
    heap.free(bytes)
    var heaps = Memory.Heap.instances.count()
    console.print("Memory.Heap objects:", heaps)
}
```
```output
Memory.Heap objects: 1
```

**What you notice.** One allocation fewer per such singleton under `--debug-memory` in a production build, so the
counts of one program differ between the two kinds of build. Reading `Build`'s attributes through reflection
answers the folded settings in both. **Built** (the D108 second-step row, D110's `Build` row, the generic
singleton row, and D143; `conformance/stage6/development_internals`).

### Atomic reference counts only with threads

**What it does.** Retaining and releasing a reference is plain arithmetic, except in a program that can share an
object between threads: one that makes a `Concurrent` or a `Parallel`, or is built with `--repl-port` or
`--hot-reload`. Only those are compiled with atomic counts (and a lock around the `--debug-memory` table).

**When.** Decided per program, from what it uses. **What you notice.** Nothing: the program that never starts a
thread never pays for atomics. **Built** (the "reference counts are atomic only in a program that starts a
thread" row; [concurrency.md](concurrency.md)).

### Boxing only where a value travels as a shape

**What it does.** A number, `Bool`, enum value or `Symbol` is a plain value everywhere the compiler can see its
type. It is put in a box -- one small object, released like any other -- only where it has to travel as a `type`
shape (a `Printable`, a `Debuggable`, an empty `type` that accepts anything) and be called through it
([D109, D164](../manual.md#decision-log)). `String` and class instances are objects already and are never boxed.

**When.** Passing a plain value where a shape is wanted, reading `attribute.value` of a number
attribute, or a class test against a number class.

**What you notice.** One allocation per boxed value under `--debug-memory`. The visible cost today: every value
given to `console.print` is passed as a `Printable`, so printing a number boxes it, and the `...values` of every
variadic call arrive in a `List` ([functions_and_operators.md](functions_and_operators.md)). Whether that list and
those boxes should live in the caller's frame is `mortaros_missing_decisions.md` item 74. **Built** (D109's print
row; D164 is decided and partly built).

### Concurrency machinery only where it is used

**What it does.** The scheduler, the state machines, the helper threads and the wrappers around every call that
can wait (`Program.sleep`, `Console.read_line`, `File.read`/`write`/`append`, `Socket.accept_client`/`read_line`)
exist only in a program that makes a `Concurrent` or is built with `--repl-port` or `--hot-reload`. Every other program's
waits are the plain system calls. Even in a program that has the scheduler, a wait with no `Concurrent` alive and
no REPL listening makes the plain blocking call, because that is faster ([D99](../manual.md#decision-log)): you
never choose between blocking and waiting, and you never see which one ran.

**When.** Decided per program, from what it uses. **What you notice.** Nothing. **Built**, on compile-time state
machines ([below](#hidden-asyncawait-as-compile-time-state-machines)). [concurrency.md](concurrency.md).

### Hidden async/await as compile-time state machines

**What it does.** Waiting on IO is written as an ordinary call and the compiler turns it into a point where other
work runs ([D35, D99](../manual.md#decision-log)). [D176](../manual.md#decision-log) asks for that to be done at
compile time, with no stacks to switch, and that is how it is built: every function that can reach a wait from
inside a `Concurrent` is compiled a second time as a **state machine**
(`bootstrap/source/generation/state_machine.spite`, and the `emit_state_machines` part of the generator):

- a **frame**, a C struct holding the function's parameters, every local and temporary of its body, and one slot
  per wait for the frame of the function it is waiting on;
- a **step function** that runs the body until it finishes (answering `true`) or reaches a wait that is not over
  (answering `false`). It starts with a jump to the wait it stopped at, so the next step carries on from there. A
  wait inside a `while` or an `if` is jumped back into directly: every local lives in the frame, so nothing is lost.

A call that waits is found wherever it is written -- a statement, a `var`, an argument, a `{...}` inside text, a
`while` condition (which waits again on each pass) -- and becomes: make the callee's frame, step it, and return
from this step while it answers `false`. The waits at the bottom -- `Program.sleep`, the `File`, `Console` and
`Socket` calls, and reading another `Concurrent` -- are small state machines the generator writes itself: a timer,
a helper thread's flag, a finished flag. `Concurrent(function)` makes the function's frame and runs it to its first
wait; `library/scheduler.spite` keeps the frames that are not finished and runs them again when something they
wait on may have happened.

**The event loop.** The loop waits on one operating system event -- an auto-reset event on Windows, a pipe with
`poll` on Linux and macOS -- with a timeout of the nearest timer, and helper threads set it when their system call
returns. It does not use IOCP, epoll or kqueue: an ordinary file is always "ready" to epoll and kqueue, so reading
a file would still need a thread; the Windows console cannot be read through IOCP; and one mechanism keeps each
system's folder to a handful of functions. The cost is a thread per system call in flight, which a server with
thousands of connections would feel ([mortaros_missing_decisions.md](../mortaros_missing_decisions.md) asks
whether sockets should move to the system's own readiness). In a browser the loop would be the browser's.

**When.** Only in a program that makes a `Concurrent`, and only for the functions a `Concurrent` can reach that
wait: the plain version of every function stays as it is for the code outside a `Concurrent`, and the tree
shaker drops whichever version nothing calls. A program that never makes a `Concurrent` has no frames, no step
functions, no event loop and no helper threads; its waits are the plain system calls.

**What you notice.**

- A wait written in the middle of an expression runs before the rest of that statement: in
  `log.append("{name} read {file.read()}")`, the file is read first and `name` is read after it, so a change another
  `Concurrent` makes to `name` during the read is seen. Everywhere else the order is the one written.
- A few waits inside a `Concurrent` are not points it returns from: one in the right side of `and` or `or`, one
  reached through a function value or a constructor, and dropping a `Concurrent` there. They still wait correctly,
  by running the event loop where they are, as waits outside a `Concurrent` do: the other `Concurrent`s keep going,
  and this one holds its place until its wait is over. A `Concurrent` whose own function cannot be a state machine (a
  function value made in another class of the standard library, say, or any function of the program in a
  `--hot-reload` build, which is called through a slot that a reload swaps) runs to its end when it is started
  (`mortaros_missing_decisions.md` item 177).
- Under `--debug-memory`, one allocation per waiting call a `Concurrent` makes (its frame), and none of the stacks
  and fiber bookkeeping the earlier design needed: `conformance/stage6/concurrent_waits` went from 191 allocations
  to 171, and its C from 345 825 bytes to 335 275.
- The compiler itself makes no `Concurrent`, so compiling it is unchanged (about 1.7 seconds either way); its own C
  grew by 161 kB, the transform's code.

**Built** (2026-09-25, [D176](../manual.md#decision-log)); the fibers that came before it, and each system's code
for creating and switching them, are gone. Proposed by Claude, unconfirmed: the reading order above, and which
waits fall back to running the loop in place.


### REPL, live reload and debug machinery only in those builds

**What it does.** Everything that exists to look inside a running program is compiled only into the builds that
ask for it:

- `--repl` and `--repl-port`: the loop, the socket thread, the reflection hooks that let a `Spite.Attribute` walk
  and assign live values (outside those builds they answer an empty list, `false` and `null`), and every fitting
  member template instantiated for the classes a list reaches, so the prompt can call `monsters.sum_health()`.
- `--hot-reload`: a function pointer per function and a forwarder in front of it (about a nanosecond a call), the
  file watcher and the reload manifest. Every other build calls functions directly and is tree-shaken.
- `--debug-memory`: the allocation table that names leaked objects. Every other build counts allocations with
  one increment.

- `--repl-port` and `--hot-reload`: a check point at the end of every pass of every loop in the program's own
  code ([D174](../manual.md#decision-log)), one call that answers a waiting command or reload, so a program that
  never waits still answers.

**When.** Only in those builds ([D143](../manual.md#decision-log), [D112](../manual.md#decision-log)). This is
not an optimisation an inspectable build turns off: it is the inspecting itself, present only where it is asked
for.
**What you notice.** Nothing in an ordinary build: its C is byte for byte the same with or without the check
points. **Built.**

### The thread pool only where a `Parallel` is made

**What it does.** `ThreadPool` is a singleton made the first time a `Parallel` (or a `parallel_each_` pass) needs
it, and it starts its worker threads then, once ([D135](../manual.md#decision-log), [D191](../manual.md#decision-log)).
A program that never makes one starts no thread and allocates nothing for it; its functions are tree-shaken with
the rest. `ThreadLocal` asks the system for its per-thread slot only when one is made, and `Lock` likewise;
its `get()` never locks, and only a thread's `set` does
([concurrency.md](concurrency.md#a-value-per-thread-and-a-lock)).

**When.** Always. **What you notice.** Nothing until the first `Parallel`, which pays for starting the workers.
**Built.** [concurrency.md](concurrency.md#the-thread-pool).

### Singletons a `Parallel` reaches take a lock

**What it does.** In a program that makes a `Parallel`, every singleton of the program's own that can change after
it is made (it assigns one of its attributes outside its constructor, or holds an object, a list or a dictionary)
gets a lock of its own, taken around every one of its functions; a call it makes to itself does not take it again
([D183](../manual.md#decision-log)). A singleton that never changes gets nothing. This is the fallback of the plan
below: [D184](../manual.md#decision-log)'s cheaper forms (atomics, per-thread buffers, reader-writer locks) are
not built, nor is the check that its functions hand out nothing they own.

**When.** Only in programs that make a `Parallel` or run a `parallel_each_` pass. **What you notice.** An
uncontended lock per call to such a singleton (two atomic operations), and waiting when two threads call it at
once. **Built** (the lock).

### Smaller ones

All **built**, and none of them needs anything from you:

- `join` writes every piece once into one buffer instead of copying the text so far at each step.
- Converting text to text, in `join` on a `List<String>`, is folded away ([D58](../manual.md#decision-log)).
- A `T?` of a class, list or text is the reference itself, with `null` as the absent case: no wrapper object.
- A generic singleton has one static slot per set of codegen values, so `Column<Health>()` is found without any
  lookup.
- An `assert` in `library/` writes nothing into the crash trace, decided when compiling, so a library guard costs
  what an `if` costs ([D189](../manual.md#decision-log)). What you notice: a crash report lists only the failed
  asserts of the program and its `load`-ed packages ([failure.md](failure.md#what-a-crash-reports)).

### Proofs that survive a call

**Built.** A proof -- `assert target`, `crash list[index]`, a bound in a `while` -- lets the reads after it skip
the null test and the narrowing ([D169](../manual.md#decision-log)). A call between the proof and the read keeps
it unless the compiler, following the called function and what it calls, finds that the call may assign an
attribute the proof reads through or shrink a list it reads; so no check is repeated after a call that provably
cannot, and no `const` keyword is needed. To know which function a call reaches, it reads the class of the value
the call is made on: an attribute's or variable's declared type, the class a constructor makes, the class the
function that made the value returns (`var address = heap.allocate(8)` is a `Memory.Address`), or the left side
of a `+` (an address plus an offset is an address); a call on a value whose class it cannot tell may reach every
function of that name. It runs entirely while compiling and emits nothing. What you can
observe: a proof after a call that may change it must be written again, and a call through a function value
keeps no proof about attributes or lists ([failure.md](failure.md#a-call-may-undo-a-proof)).

## Planned

Decided by Mortaro, not built yet. When one is built, it moves up to **Built** in the same change.

### Thread safety for singletons, the cheapest safe form

A singleton reached from a `Parallel` is made thread-safe by the compiler, with no keyword
([D183](../manual.md#decision-log)), and the compiler picks the cheapest form that is safe for what that singleton's
functions actually do ([D184](../manual.md#decision-log)): read-only state needs nothing; a single counter or flag
becomes an atomic; state that is only appended to (a log, a command queue) gets a buffer per thread merged in
order; state each thread touches its own part of is split per thread; reads that far outnumber writes take a
reader-writer lock; and only when nothing cheaper is proven safe does every outside call take the singleton's own
lock. Its functions may hand out only numbers, text, copies or other singletons made safe the same way, which is a
compile-time check at `return`. All of it is absent from a program that never makes a `Parallel`. You will write
nothing. The last step, the lock, is built (above); the cheaper forms and the `return` check are not.

### Copies that cost nothing

Every class is passed by reference and `copy()` gives an independent one; that is the whole API, and the compiler
optimises behind it ([D149](../manual.md#decision-log)): a copy used only once is passed by value instead of
allocated; a copy that is never changed shares the original, when that is cheaper; an object that never escapes
its function is laid out inline or in registers; and reference counting is left out wherever ownership is
provable. [D152](../manual.md#decision-log) adds that setting an object's allocator right after it is made
(`scratch.memory.allocator = frame`) is where it was allocated from the start, never a second allocation and a
move. You keep writing `copy()` where you mean an independent object.

### Other planned optimisations

- **A thread pool for `Parallel`** ([D135](../manual.md#decision-log)): today every `Parallel` starts an operating
  system thread of its own; the pool reuses a fixed set, and `list.parallel_each_update()` splits a list across it.
- **Short symbols inline** ([D70](../manual.md#decision-log)): a short symbol held as a small inline string rather
  than a pointer into the symbol table.
- **Crash text out of the binary** ([D32](../manual.md#decision-log)): a `crash` or `assert` site's source text
  lives only in the `<output>.crashes` map written beside the program, and an optimised build carries just the id.
  The map is written today, but the binary still carries the text.
- **A build report of what could not be optimised** ([D36](../manual.md#decision-log)): not "400 copies elided"
  but "3 copies could not be elided, and the callee that writes the field", so every line is actionable.

## Adding one

Every optimisation the compiler starts making on its own is added to this page in the same change, with what it
does, when, whether it is built, and anything a user could observe ([D185](../manual.md#decision-log),
[D102](../manual.md#decision-log)). An optimisation with a cost that cannot be removed says so here; one that
contradicts the manual is recorded in `mortaros_missing_decisions.md` for Mortaro.
