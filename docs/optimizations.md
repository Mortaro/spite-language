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

What you can observe at all is short: the counts `--debug_memory` prints, where `.memory` says a value lives, the
order in which a fused chain calls your member functions, and speed. Apart from that order, which only a member
function with a visible effect can show, no optimisation changes what a program prints or computes.

| Optimisation | Status | What you might notice |
|---|---|---|
| [Tree shaking the generated C](#tree-shaking-the-generated-c) | built | smaller C; `--development` keeps everything |
| [Deciding conditions at compile time](#deciding-conditions-at-compile-time) | built | nothing: the branch not taken is not in the program |
| [Reflection, symbols and registries only where read](#reflection-symbols-and-registries-only-where-read) | built | nothing |
| [Template chains run as one loop](#template-chains-run-as-one-loop) | built | fewer allocations; member functions run element by element |
| [Appending to text in place](#appending-to-text-in-place) | built | fewer allocations |
| [The compiler places memory](#the-compiler-places-memory) | built | fewer allocations; `.memory.section` |
| [Singletons: made on first use, never counted](#singletons-made-on-first-use-never-counted) | built | the constructor runs at first use |
| [Singletons that hold nothing are static objects](#singletons-that-hold-nothing-are-static-objects) | built | one allocation fewer each |
| [Atomic reference counts only with threads](#atomic-reference-counts-only-with-threads) | built | nothing |
| [Boxing only where a value travels as a shape](#boxing-only-where-a-value-travels-as-a-shape) | built | one allocation per boxed value |
| [Concurrency machinery only where it is used](#concurrency-machinery-only-where-it-is-used) | built | nothing |
| [REPL, live reload and debug machinery only in those builds](#repl-live-reload-and-debug-machinery-only-in-those-builds) | built | nothing in an ordinary build |
| [Smaller ones](#smaller-ones) | built | nothing |
| [Thread safety for singletons, the cheapest safe form](#thread-safety-for-singletons-the-cheapest-safe-form) | planned | |
| [Copies that cost nothing](#copies-that-cost-nothing) | planned | |
| [Hidden async/await as compile-time state machines](#hidden-asyncawait-as-compile-time-state-machines) | planned | |
| [Proofs that survive a call](#proofs-that-survive-a-call) | planned | |
| [Other planned optimisations](#other-planned-optimisations) | planned | |

## Built

### Tree shaking the generated C

**What it does.** After the program is generated, the compiler keeps only the C that `main` can reach: every
function nothing calls, from your classes, `library/` or the compiler's own prelude, is dropped along with its
prototype (`bootstrap/source/generation/tree_shaker.spite`). A small program's C goes from about 5 800 lines to
about 2 000. The compiler does this itself rather than leaving dead code for the C compiler to find, so it holds
whichever C compiler you bring.

**When.** Every build except `--development`, which keeps everything so live reload has every function to swap
([compiler.md](compiler.md#development-builds-and-tree-shaking)); `--hot_reload` implies `--development`.

**What you notice.** Nothing, except that `--mode=c` prints less. A function nobody calls, outside a generic class, is still
compiled and checked, so a mistake in it is still reported ([D140](../manual.md#decision-log)) -- it just is not in the
binary. **Built** (the tree shaker and `--development` rows of the [decision log](../manual.md#decision-log),
2026-09-24).

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

**When.** Always, `--development` included.

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

Note: manual section 9 says that in development mode conditions on codegen values stay run-time values; the
compiler folds them in every build (`mortaros_missing_decisions.md` item 140).

### Reflection, symbols and registries only where read

**What it does.** Reflection is decided at compile time, so the compiler knows exactly what a program reads and
emits only that ([D42, D57](../manual.md#decision-log)): a class object's `.attributes`, `.functions` and
`.namespace`, `value.attributes`, `value.memory`, `attribute.object`, the per-class `Person.instances` registry (a
class is only tracked when something asks for its instances), `Spite.Class.instances`, and a class's `to_debug()`.
A symbol literal is an entry of a table the compiler writes with only the symbols the program uses; it is
constant text, so storing and comparing symbols allocates nothing ([D70](../manual.md#decision-log)). A Symbol
codegen template exists only for the names a program calls: a program that never calls `sum_price()` has no
`sum_price`.

**When.** Always. **What you notice.** Nothing: reflection may be as detailed as it likes, because a program that
never reads it carries none of it. **Built.**

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

**What you notice.** Fewer allocations under `--debug_memory` (`conformance/stage6/fused_chain_allocations`
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

**What you notice.** Fewer allocations under `--debug_memory`, and `value.memory.section` answering `'stack'`,
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
[classes_and_files.md](classes_and_files.md#singletons)). `--debug_memory` still names an object a program leaked,
even one that points at a singleton. **Built** (D8, D142, D141).

### Singletons that hold nothing are static objects

**What it does.** A singleton with no attributes and no `drop()` -- `Memory.Heap`, `TypedMemory<T>` (one per
element type), and `Build`, whose attributes are all settings folded into the program -- is one static object:
never allocated, never counted, never freed. `Memory.Heap()` costs nothing, and every program allocates once
fewer for each.

**When.** Today, in every build. [D143](../manual.md#decision-log) decided that in `--repl`, `--repl_port`,
`--hot_reload` and `--development` builds these internals are ordinary objects that reflection (`.instances`,
`.attributes`) sees, and are static objects only in production builds; that split is not built yet, and the
compiler makes them static everywhere (`mortaros_missing_decisions.md` item 139).

**What you notice.** One allocation fewer per such singleton under `--debug_memory`. Reading `Build`'s attributes
through reflection answers the folded settings. **Built** (the D108 second-step row, D110's `Build` row, and the
generic singleton row).

### Atomic reference counts only with threads

**What it does.** Retaining and releasing a reference is plain arithmetic, except in a program that can share an
object between threads: one that makes a `Concurrent` or a `Parallel`, or is built with `--repl_port` or
`--hot_reload`. Only those are compiled with atomic counts (and a lock around the `--debug_memory` table).

**When.** Decided per program, from what it uses. **What you notice.** Nothing: the program that never starts a
thread never pays for atomics. **Built** (the "reference counts are atomic only in a program that starts a
thread" row; [concurrency.md](concurrency.md)).

### Boxing only where a value travels as a shape

**What it does.** A number, `Bool`, enum value or `Symbol` is a plain value everywhere the compiler can see its
type. It is put in a box -- one small object, released like any other -- only where it has to travel as a `type`
shape (a `Printable`, a `Debuggable`, an empty `type` that accepts anything) and be called through it
([D109, D164](../manual.md#decision-log)). `String` and class instances are objects already and are never boxed.

**When.** Passing a plain value where a shape is wanted, reading `attribute.object` of a number
attribute, or a class test against a number class.

**What you notice.** One allocation per boxed value under `--debug_memory`. The visible cost today: every value
given to `console.print` is passed as a `Printable`, so printing a number boxes it, and the `...values` of every
variadic call arrive in a `List` ([functions_and_operators.md](functions_and_operators.md)). Whether that list and
those boxes should live in the caller's frame is `mortaros_missing_decisions.md` item 74. **Built** (D109's print
row; D164 is decided and partly built).

### Concurrency machinery only where it is used

**What it does.** The scheduler, the fibers, the helper threads and the wrappers around every call that can wait
(`Program.sleep`, `Console.read_line`, `File.read`/`write`/`append`, `Socket.accept_client`/`read_line`) exist only
in a program that makes a `Concurrent` or is built with `--repl_port` or `--hot_reload`. Every other program's
waits are the plain system calls. Even in a program that has the scheduler, a wait with no `Concurrent` alive and
no REPL listening makes the plain blocking call, because that is faster ([D99](../manual.md#decision-log)): you
never choose between blocking and waiting, and you never see which one ran.

**When.** Decided per program, from what it uses. **What you notice.** Nothing. **Built**, on stackful fibers,
which [D176](../manual.md#decision-log) replaces (below). [concurrency.md](concurrency.md).

### REPL, live reload and debug machinery only in those builds

**What it does.** Everything that exists to look inside a running program is compiled only into the builds that
ask for it:

- `--repl` and `--repl_port`: the loop, the socket thread, the reflection hooks that let a `Spite.Attribute` walk
  and assign live values (outside those builds they answer an empty list, `false` and `null`), and every fitting
  member template instantiated for the classes a list reaches, so the prompt can call `monsters.sum_health()`.
- `--hot_reload`: a function pointer per function and a forwarder in front of it (about a nanosecond a call), the
  file watcher and the reload manifest. Every other build calls functions directly and is tree-shaken.
- `--debug_memory`: the allocation table that names leaked objects. Every other build counts allocations with
  one increment.

**When.** Only in those builds ([D143](../manual.md#decision-log), [D112](../manual.md#decision-log)).
**What you notice.** Nothing in an ordinary build. **Built.** Planned with it: [D174](../manual.md#decision-log)'s
one-flag check at the end of every loop iteration, so a program that never waits still answers its REPL -- in REPL
builds only.

### Smaller ones

All **built**, and none of them needs anything from you:

- `join` writes every piece once into one buffer instead of copying the text so far at each step.
- Converting text to text, in `join` on a `List<String>`, is folded away ([D58](../manual.md#decision-log)).
- A `T?` of a class, list or text is the reference itself, with `null` as the absent case: no wrapper object.
- A generic singleton has one static slot per set of codegen values, so `Column<Health>()` is found without any
  lookup.

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
nothing.

### Copies that cost nothing

Every class is passed by reference and `copy()` gives an independent one; that is the whole API, and the compiler
optimises behind it ([D149](../manual.md#decision-log)): a copy used only once is passed by value instead of
allocated; a copy that is never changed shares the original, when that is cheaper; an object that never escapes
its function is laid out inline or in registers; and reference counting is left out wherever ownership is
provable. [D152](../manual.md#decision-log) adds that setting an object's allocator right after it is made
(`scratch.memory.allocator = frame`) is where it was allocated from the start, never a second allocation and a
move. You keep writing `copy()` where you mean an independent object.

### Hidden async/await as compile-time state machines

Waiting on IO is written as an ordinary call and the compiler turns it into a suspension (D35, D99). Today that is
done with fibers; [D176](../manual.md#decision-log) replaces them with a compile-time transform: each function that
can reach a wait becomes a resumable state machine, and what is left at run time is a minimal loop continuing work
when IO completes -- in a browser, the browser's own. No stack per fiber, no scheduler to ship, and nothing that
bloats a WebAssembly build. The source does not change and no function is coloured. With it,
[D134](../manual.md#decision-log): `File`, `Directory`, `Socket` and the other IO classes start their work
concurrently themselves and hand back values that wait where they are first used, so independent reads overlap
without the program asking.

### Proofs that survive a call

A proof such as `crash list[index]` or a bound in a `while` lets the next read of `list[index]` skip its check.
[D169](../manual.md#decision-log): a call between the proof and the read undoes it only if the compiler, following
the called function and what it calls, finds that it may change the list or anything the index reads. A call that
provably cannot keeps the proof, so no check is repeated and no `const` keyword is needed.

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
