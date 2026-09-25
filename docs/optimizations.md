# Optimisations the compiler makes on its own

The compiler makes a program faster and smaller without being asked. This page lists every such optimisation: what
it does, when it applies, whether it is **built** or only **planned**, and what, if anything, you could ever
notice. For almost all of them the honest answer is: **you do not need to do anything**. Write the plain program;
the compiler does the rest, and the program means exactly what its source says.

Two rules decide what belongs here.

**Zero runtime, and everything tree-shakeable** ([D177](decisions.md)). A program that does not use
a feature carries none of it. Nothing needs a scheduler, an interpreter or a registry shipped beside the program:
the work is done at compile time instead. REPL, live reload and debugging features may cost something while the
program runs, but only in the builds that ask for them ([D143](decisions.md)).

**Hidden optimisations are good, hidden costs are bad** ([D36](decisions.md)). Code that runs faster
than you expect is a free win, so the compiler optimises silently and never asks you to mark anything. Code that
runs *slower* than you expect is the only real surprise, so every cost that remains is written down on this page,
under the optimisation it belongs to.

**Internals stay ordinary objects where you inspect them** ([D143](decisions.md)). A `--repl`,
`--repl-port`, `--hot-reload` or `--development` build is an *inspectable* build: nothing is tree-shaken, and
the standard library's internals -- `Memory`, `Build`, `TypedMemory<T>` -- are ordinary objects that reflection
(`.instances`, `.attributes`) sees. Every other build, ordinary or `--optimized`, is a production build, and only
there are the two optimisations that hide something applied: tree shaking and static singletons. Everything else
on this page changes how fast the program runs, not what it is made of, so it applies in every build. The
**Builds** column below says which.

What you can observe at all is short: the counts `--debug-memory` prints, where `.memory` says a value lives, when a
singleton's constructor runs, the order in which a fused chain calls your member functions, the point inside a
statement where a `Concurrent` waits, and speed. Apart from those orders, which only code with a visible effect can
show, no optimisation changes what a program prints or computes.

The checks the compiler makes are not on this page: a wider right operand ([D162](decisions.md)), a proof a call
may have undone, an unread name. They are rules of the language, on the pages that teach them, and they cost
nothing at run time because they emit nothing.

| Optimisation | Status | Builds | What you might notice |
|---|---|---|---|
| [Tree shaking the generated C](#tree-shaking-the-generated-c) | built | production | smaller C; fewer symbols looked up; an inspectable build keeps everything |
| [Deciding conditions at compile time](#deciding-conditions-at-compile-time) | built | every | nothing: the branch not taken is not in the program |
| [Reflection, symbols and registries only where read](#reflection-symbols-and-registries-only-where-read) | built | every | nothing |
| [Template chains run as one loop](#template-chains-run-as-one-loop) | built | every | fewer allocations; member functions run element by element |
| [Appending to text in place](#appending-to-text-in-place) | built | every | fewer allocations |
| [The compiler places memory](#the-compiler-places-memory) | built | every | fewer allocations; `.memory.section` |
| [Reading an address is one machine operation](#reading-an-address-is-one-machine-operation) | built | every | nothing |
| [An allocator set after construction is where the object is made](#an-allocator-set-after-construction-is-where-the-object-is-made) | built | every | the arena's blocks are the allocations; sixteen bytes more per object of a class given an allocator |
| [Singletons: made on first use, never counted](#singletons-made-on-first-use-never-counted) | built | every | the constructor runs at first use |
| [Singletons that hold nothing are static objects](#singletons-that-hold-nothing-are-static-objects) | built | production | one allocation fewer each; not in `.instances` |
| [Atomic reference counts only with threads](#atomic-reference-counts-only-with-threads) | built | every, decided per program | nothing |
| [Boxing only where a value travels as a shape](#boxing-only-where-a-value-travels-as-a-shape) | built | every | one allocation per boxed value |
| [Concurrency machinery only where it is used](#concurrency-machinery-only-where-it-is-used) | built | every, decided per program | nothing |
| [Hidden async/await as compile-time state machines](#hidden-asyncawait-as-compile-time-state-machines) | built | every, in programs that make a `Concurrent` | one heap frame per waiting call; a wait inside an expression runs first |
| [Reads in a row overlap](#reads-in-a-row-overlap) | built | every | the reads happen at once; the program carries the scheduler |
| [REPL, live reload and debug machinery only in those builds](#repl-live-reload-and-debug-machinery-only-in-those-builds) | built | the builds that ask for it | nothing in an ordinary build |
| [The thread pool only where a `Parallel` is made](#the-thread-pool-only-where-a-parallel-is-made) | built | every, decided per program | nothing until the first `Parallel` |
| [Singletons a `Parallel` reaches take a lock](#singletons-a-parallel-reaches-take-a-lock) | built (the fallback) | every but `--hot-reload`, decided per program | an uncontended lock per call, only with `Parallel` |
| [Thread safety for singletons, the cheapest safe form](#thread-safety-for-singletons-the-cheapest-safe-form) | built: nothing, atomics, one-thread; planned: the rest | every but `--hot-reload`, decided per program | no lock where one is not needed |
| [Smaller ones](#smaller-ones) | built | every | nothing |
| [Proofs that survive a call](#proofs-that-survive-a-call) | built | every | a proof after a call that may change it is written again |
| [Text joined in one piece](#text-joined-in-one-piece) | built | every | fewer allocations; a text made only of constants is constant |
| [Defaults the constructor replaces are never made](#defaults-the-constructor-replaces-are-never-made) | built | every but `--hot-reload` | fewer allocations |
| [A function value describes its arguments when asked](#a-function-value-describes-its-arguments-when-asked) | built | every | fewer allocations per function value and per `Parallel` |
| [A list's templates read its elements without counting them](#a-lists-templates-read-its-elements-without-counting-them) | built | every but `--hot-reload` | nothing but speed |
| [A number joined into text is written in place](#a-number-joined-into-text-is-written-in-place) | built | every | fewer allocations |
| [Allocation is the C library's, counted only where read](#allocation-is-the-c-librarys-counted-only-where-read) | built | every but `--debug-memory`, decided per program | nothing: `live_allocations()` still answers |
| [A dictionary hashes a key once, cheaply](#a-dictionary-hashes-a-key-once-cheaply) | built | every | nothing but speed |
| [Reading through a `type` without counting](#reading-through-a-type-without-counting) | built | every but `--hot-reload` | nothing but speed |
| [Thread safety for singletons, the rest of the plan](#thread-safety-for-singletons-the-rest-of-the-plan) | planned | | |
| [Copies that cost nothing](#copies-that-cost-nothing) | planned | | |
| [Other planned optimisations](#other-planned-optimisations) | planned | | |

## Built

### Tree shaking the generated C

**What it does.** After the program is generated, the compiler keeps only the C that `main` can reach: every
function nothing calls, from your classes, `library/` or the compiler's own prelude, is dropped along with its
prototype (`bootstrap/source/generation/tree_shaker.spite`). So is every class nothing reachable uses: its
`struct` and the `typedef` that names it, its `___allocate`, `___init`, `___default`, `___retain`, `___release`
and `_copy`, its singleton slot and that slot's lock, its reflection class object and the lines in `main` that
would free that object at exit, and every text literal, static table and prototype only dropped code named. A
program that never makes a `Watcher`, `Socket`, `Process`, `HotReload`, `ThreadPool` or `Scheduler` has none of
their C. The compiler does this itself rather than leaving dead code for the C compiler to find, so it holds
whichever C compiler you bring -- and a C compiler cannot find most of it anyway, since a function it is not told
is private has to stay in the executable.

Measured with `--c-source`, before and after classes were shaken too (the executable is `clang -O2` on Windows):

| Program | C lines | C bytes | `struct`s | Executable |
|---|---|---|---|---|
| `examples/hello` | 5 130 → 1 332 | 233 588 → 62 732 | 75 → 10 | 192 000 → 158 208 |
| `examples/dungeon` | 5 958 → 2 348 | 268 925 → 101 165 | 80 → 21 | 207 360 → 173 568 |
| `conformance/stage3/interpolation` | 5 222 → 1 497 | 241 343 → 72 403 | 76 → 13 | 195 072 → 162 304 |
| `conformance/stage6/parallel_each` | 5 990 → 2 972 | 277 303 → 133 127 | 78 → 34 | 211 968 → 184 320 |
| `conformance/stage6/singleton_guard` | 6 392 → 3 407 | 294 742 → 150 397 | 82 → 40 | 219 136 → 190 464 |

`examples/hello --development` is 14 885 lines both before and after: an inspectable build keeps everything.

**When.** Production builds only. An inspectable build -- `--repl`, `--repl-port`, `--hot-reload` or
`--development` -- keeps everything, so live reload has every function to swap and the REPL can reach every
internal ([D143](decisions.md), [compiler.md](compiler.md#development-builds-and-tree-shaking)).

**What you notice.** Nothing, except that `--c-source` writes less. A function nobody calls, outside a generic class, is still
compiled and checked, so a mistake in it is still reported ([D140](decisions.md)) -- it just is not in the
binary. **Built** (the tree shaker and `--development` rows of the [decision log](decisions.md),
2026-09-24; classes, slots and statics 2026-09-25, proposed by Claude, unconfirmed). `check.sh` holds it: the C of
`examples/hello` must carry no struct, allocate or singleton slot of those library classes.

The same pass decides which native symbols are looked up. A `DynamicLibrary` looks up every symbol the program
calls when it opens, and a symbol is now looked up only when a function that calls it survived the shaking: a
program that never uses `Watcher` does not look up `ReadDirectoryChangesW`, though `library/windows/watcher.spite`
opens the same `kernel32.dll` as `Program.sleep`. **What you notice.** Fewer allocations under `--debug-memory`,
since each lookup makes two short-lived strings, and a missing symbol that only unused code names does not stop
the program when the library opens. An inspectable build is not shaken, so it looks up every symbol.
**Built** (2026-09-25, with `Watcher`; proposed by Claude, unconfirmed).

### Deciding conditions at compile time

**What it does.** A condition the compiler can answer while compiling is answered then, and only the branch taken
is generated. The branch not taken is not in the program at all -- not skipped at run time, *absent* -- so it may
even use things that would not compile for this build. That covers:

- a field of [`Build`](programs.md#compile-time-settings-build): every `Build` field is a constant of the built
  program, set by a flag or taken from its declared default ([D84, D85](decisions.md));
- a codegen value, `if $is_magic { }`, and a test on a codegen type, `if $value_type == List { }`
  ([metaprogramming.md](metaprogramming.md#tree-shaking));
- a class test the value's type already answers, `if item == $wanted_type`, and one that can never be true for one
  instantiation of a generic, which folds to `false` there instead of being an error
  ([D167](decisions.md));
- `$system_type.has_function('run_each')` ([D114](decisions.md));
- `not`, `and`, `or`, `==` and `!=` over any of these.

A function of a generic class is then compiled for one instantiation only when code that survived folding names
it, so a helper reached only from a removed branch is never checked against a type it cannot work with.

**When.** Every build, inspectable ones included. A folded branch hides nothing from the REPL: the branch not
taken is not part of this program, since a `Build` field or a codegen value is a fact of the build, not a value
that could change while it runs.

**Example.** `Describer<Integer>` never contains `value.count()`, which an `Integer` does not have:

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
    var numbers = List<Integer>()
    numbers.append(4)
    numbers.append(9)
    var lists = Describer<List<Integer>>()
    var first = lists.describe(numbers)
    var singles = Describer<Integer>()
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
instantiation. **Built** ([Codegen values](metaprogramming.md#codegen-values---implemented), D84, D85, D167, and the
"only what survives folding is compiled" row).

### Reflection, symbols and registries only where read

**What it does.** Reflection is decided at compile time, so the compiler knows exactly what a program reads and
emits only that ([D42, D57](decisions.md)): a class object's `.attributes`, `.functions` and
`.namespace`, `value.attributes`, `value.memory`, `attribute.value`, the per-class `Person.instances` registry (a
class is only tracked when something asks for its instances), `Spite.Class.instances`, and a class's `to_debug()`.
A symbol literal is an entry of a table the compiler writes with only the symbols the program uses; it is
constant text, so storing and comparing symbols allocates nothing ([D70](decisions.md)). A Symbol
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
added before the next one is read ([D105](decisions.md)).

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

func Team(new_name: String, new_active: Boolean, new_size: Integer) {
    name = new_name
    active = new_active
    size = new_size
}

func is_active(): Boolean {
    console.print("checking", name)
    return active
}

func counted_size(): Integer {
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
(the "appending to a text the variable alone holds" row of the [decision log](decisions.md);
[values_and_types.md](values_and_types.md)).

### The compiler places memory

**What it does.** A program has one way to ask for raw memory, `heap.allocate(bytes)` on `Memory.Heap()`, and one
way to give it back, `heap.free(address)`. Where the bytes live is the compiler's choice ([D108](decisions.md),
[Placement](memory.md#placement-the-compiler-decides-where-memory-lives--implemented-the-rule-proposed-by-claude-unconfirmed)):

- **Register:** a number's own memory (`var _memory = heap.allocate(4)` in `library/integer.spite`) is its C
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

func sum_of_squares(count: Integer): Long {
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
atomics of `Memory.Address` are primitives of the language, like `+` ([D178](decisions.md)): the
compiler writes each one where it is called, as the single load, store or atomic instruction, with no call and
no check. `copy_to` and `compare_bytes` are written the same way, as the C library's copy and comparison.

**When.** Always; `library/` is the only place that may call them, so every `String`, `List` and `Dictionary`
reads its memory this way.

**What you notice.** Nothing: there is no other way these could run. **Built** (D178).

### An allocator set after construction is where the object is made

**What it does.** `var spark = Particle("spark", 1.5)` followed by `spark.memory.allocator = arena` reads as
though it made the particle on the heap and then moved it. The compiler makes it in `arena` from the start: the
two lines become one construction that asks the arena for the memory, with nothing allocated twice and nothing
decided while the program runs ([D152](decisions.md), [memory.md](memory.md#choosing-an-allocator-memoryallocator)).
The same holds for `var kept = ash.copy()` followed by `kept.memory.allocator = arena`.

**When.** Always, for the line right after the one that makes the object; anywhere else setting the allocator is
an error (D153).

**What you notice.** Under `--debug-memory`, an object made in an arena is not an allocation of its own: the
arena's blocks are. A class some line gives an allocator is sixteen bytes larger per object (two hidden pointers:
the allocator, and the function that gives the memory back), on every object of that class, heap ones included;
no other class changes. Setting `Memory.Heap()` is the default and costs nothing. **Built** (D152, D153).

### Singletons: made on first use, never counted

**What it does.** A singleton is made the first time something asks for it, not when the program starts, so a
program pays only for the singletons it reaches. It is never reference counted ([D142](decisions.md)):
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
that singleton in an attribute ([D141](decisions.md),
[classes_and_files.md](classes_and_files.md#singletons)). `--debug-memory` still names an object a program leaked,
even one that points at a singleton. **Built** (D8, D142, D141).

### Singletons that hold nothing are static objects

**What it does.** A singleton with no attributes and no `drop()` -- `Memory.Heap`, `TypedMemory<T>` (one per
element type), and `Build`, whose attributes are all settings folded into the program -- is one static object:
never allocated, never counted, never freed. `Memory.Heap()` costs nothing, and every program allocates once
fewer for each.

**When.** Production builds only ([D143](decisions.md)). In an inspectable build -- `--repl`,
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
object between threads: one that makes a `Concurrent` or a `Parallel` (reads in a row included), runs a
`parallel_each_` pass, or is built with `--repl-port` or `--hot-reload`. Only those are compiled with atomic counts
(and a lock around the `--debug-memory` table).

**When.** Decided per program, from what it uses. **What you notice.** Nothing: the program that never starts a
thread never pays for atomics. **Built** (the "reference counts are atomic only in a program that starts a
thread" row; [concurrency.md](concurrency.md)).

### Boxing only where a value travels as a shape

**What it does.** A number, `Boolean`, enum value or `Symbol` is a plain value everywhere the compiler can see its
type. It is put in a box -- one small object, released like any other -- only where it has to travel as a `type`
shape (a `Printable`, a `Debuggable`, an empty `type` that accepts anything) and be called through it
([D109, D164](decisions.md)). `String` and class instances are objects already and are never boxed.

**When.** Passing a plain value where a shape is wanted, reading `attribute.value` of a number
attribute, or a class test against a number class.

**What you notice.** One allocation per boxed value under `--debug-memory`. The visible cost today: every value
given to `console.print` is passed as a `Printable`, so printing a number boxes it, and the `...values` of every
variadic call arrive in a `List` ([functions_and_operators.md](functions_and_operators.md)). Whether that list and
those boxes should live in the caller's frame is `mortaros_missing_decisions.md` item 74. **Built** (D109's print
row, and D164's `attribute.value`, filled only in a program that reads it).

### Concurrency machinery only where it is used

**What it does.** The scheduler, the state machines, the helper threads and the wrappers around every call that
can wait (`Program.sleep`, `Console.read_line`, `File.read`/`write`/`append`, `Socket.accept_client`/`read_line`/`read_bytes`)
exist only in a program that makes a `Concurrent` (itself, or through [reads in a row](#reads-in-a-row-overlap)) or
is built with `--repl-port` or `--hot-reload`. Every other program's waits are the plain system calls. Even in a program that has the scheduler, a wait with no `Concurrent` alive and
no REPL listening makes the plain blocking call, because that is faster ([D99](decisions.md)): you
never choose between blocking and waiting, and you never see which one ran.

**When.** Decided per program, from what it uses. **What you notice.** Nothing. **Built**, on compile-time state
machines ([below](#hidden-asyncawait-as-compile-time-state-machines)). [concurrency.md](concurrency.md).

### Hidden async/await as compile-time state machines

**What it does.** Waiting on IO is written as an ordinary call and the compiler turns it into a point where other
work runs ([D35, D99](decisions.md)). [D176](decisions.md) asks for that to be done at
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
thousands of connections would feel ([mortaros_missing_decisions.md](../mortaros_missing_decisions.md) item 180
asks whether sockets should move to the system's own readiness). In a browser the loop would be the browser's.

**When.** Only in a program that makes a `Concurrent`, and only for the functions a `Concurrent` can reach that
wait: the plain version of every function stays as it is for the code outside a `Concurrent`, and in a
production build the tree shaker drops whichever version nothing calls. A program that never makes a `Concurrent` has no frames, no step
functions, no event loop and no helper threads; its waits are the plain system calls.

**What you notice.**

- A wait written in the middle of an expression runs before the rest of that statement: in
  `log.append("{name} read {file.read()}")`, the file is read first and `name` is read after it, so a change another
  `Concurrent` makes to `name` during the read is seen. Everywhere else the order is the one written.
- A few waits inside a `Concurrent` are not points it returns from: one in the right side of `and` or `or`, one
  reached through a function value, a union's dispatch or a constructor, and dropping a `Concurrent` there. They
  still wait correctly, by running the event loop where they are, as waits outside a `Concurrent` do: the other
  `Concurrent`s keep going, and this one holds its place until its wait is over. Two such waits that each wait for
  the other would never end, and nothing reports it. A `Concurrent` whose own function cannot be a state machine
  runs to its end when it is started: a function value a standard-library class made and stored before it reached
  `Concurrent`, a shape's function, a singleton function that takes [the lock](#singletons-a-parallel-reaches-take-a-lock),
  or any function of the program in a `--hot-reload` build, which is called through a slot that a reload swaps
  (`mortaros_missing_decisions.md` item 179).
- Under `--debug-memory`, one allocation per waiting call a `Concurrent` makes (its frame), and none of the stacks
  and fiber bookkeeping the earlier design needed: `conformance/stage6/concurrent_waits` went from 191 allocations
  to 171, and its C from 345 825 bytes to 335 275.
- The compiler itself makes no `Concurrent`, so compiling it is unchanged (about 1.7 seconds either way); its own C
  grew by 161 kB, the transform's code.

**Built** (2026-09-25, [D176](decisions.md)); the fibers that came before it, and each system's code
for creating and switching them, are gone. Proposed by Claude, unconfirmed: the reading order above, and which
waits fall back to running the loop in place.

### Reads in a row overlap

**What it does.** Two or more `var name = file.read()` (or `socket.read_line()`) written one after another, none
naming a variable an earlier one declared, are started together: every read but the last becomes a `Concurrent`,
and all of them are joined before the next statement ([D134](decisions.md),
[concurrency.md](concurrency.md#reads-in-a-row-overlap), which has the exact shape).

**When.** Every build, wherever the shape appears. A statement between two reads, a typed `var`, or a name
assigned again later keeps each read where it is.

**What you notice.** Nothing in what the program computes: the values are the same, and a file written by the
next statement is written after the reads. The program waits for the slowest read instead of each in turn. The
cost is that the program now carries the [concurrency machinery](#concurrency-machinery-only-where-it-is-used):
the scheduler, a helper thread per read in flight, a `Concurrent` per overlapped read, and
[atomic reference counts](#atomic-reference-counts-only-with-threads). Two files read in a row allocate 62 times
under `--debug-memory` and compile to 2 865 lines of C; the same reads with a `console.print` between them
allocate 19 times in 1 511 lines. **Built** (the "reads in a row overlap" row; proposed by Claude, unconfirmed).


### REPL, live reload and debug machinery only in those builds

**What it does.** Everything that exists to look inside a running program is compiled only into the builds that
ask for it:

- `--repl` and `--repl-port`: the loop, the socket thread, the reflection hooks that let a `Spite.Attribute` walk
  and assign live values (outside those builds they answer an empty list, `false` and `null`), and every fitting
  member template instantiated for the classes a list reaches, so the prompt can call `monsters.sum_health()`.
- `--hot-reload`: a function pointer per function and a forwarder in front of it (about a nanosecond a call), the
  file watcher and the reload manifest. Every other build calls functions directly and is tree-shaken.
- `--debug-memory`: the allocation table that names leaked objects, and its C (`AllocationTable`, the functions
  that call it, the class-name table) exists only in that build's C. Every other build allocates with the C
  library's own `malloc`, `realloc` and `free` and nothing beside them, unless the program reads
  `live_allocations()` ([Allocation is the C library's](#allocation-is-the-c-librarys-counted-only-where-read)).

- `--repl-port` and `--hot-reload`: a check point at the end of every pass of every loop in the program's own
  code ([D174](decisions.md)), one call that answers a waiting command or reload, so a program that
  never waits still answers.

**When.** Only in those builds ([D143](decisions.md), [D112](decisions.md)). This is
not an optimisation an inspectable build turns off: it is the inspecting itself, present only where it is asked
for.
**What you notice.** Nothing in an ordinary build: its C is byte for byte the same with or without the check
points. **Built.**

### The thread pool only where a `Parallel` is made

**What it does.** `ThreadPool` is a singleton made the first time a `Parallel` (or a `parallel_each_` pass) needs
it, and it starts its worker threads then, once ([D135](decisions.md), [D191](decisions.md)).
A program that never makes one starts no thread and allocates nothing for it; its functions are tree-shaken with
the rest. `ThreadLocal` asks the system for its per-thread slot only when one is made, and `Lock` likewise;
its `get()` never locks, and only a thread's `set` does
([concurrency.md](concurrency.md#a-value-per-thread-and-a-lock)).

**When.** Always. **What you notice.** Nothing until the first `Parallel`, which pays for starting the workers.
**Built.** [concurrency.md](concurrency.md#the-thread-pool).

### Singletons a `Parallel` reaches take a lock

**What it does.** In a program that makes a `Parallel`, every singleton of the program's own that can change after
it is made gets a lock of its own, taken around every one of its functions ([D183](decisions.md)).
It can change when one of its functions assigns one of its attributes outside its constructor, when code in
another class assigns one (`registry.last = name`), or when it holds a list, a dictionary, a function value, or an
object of a class that can change -- an object whose class never assigns its attributes after its constructor,
and holds nothing that can change either, is as read-only as a number, so a `Rules` holding a `Limits` made once
takes no lock. This is the fallback of [D184](decisions.md): the compiler takes it only when none of
the cheaper forms in the next section is proven safe for that singleton. The check that its functions hand out
nothing they own is not built.

How the lock is kept cheap:

- **Each lock has a cache line of its own.** A lock is 64 bytes, aligned to 64 (`_Alignas(64)`, C11), so two
  singletons' locks never share a line and taking one never makes another core reload the other. In SlopEngine's
  stress test (200 000 entities, two `parallel_each_` systems) this took a tick from 108 ms to 45 ms, the same as
  with no locks at all.
- **A call to itself skips the lock.** Inside a locked function, a call to another function of the same singleton
  goes straight to that function's unlocked body (`Registry_count_one___unguarded(self)`), since the lock is
  already held: no atomic load and no depth count per call. A function value of it is not such a call:
  `found.filter(matches)` inside the singleton makes a value that calls the locked function, because a value can be
  kept and called from anywhere; called while the lock is held, that costs one atomic load and a depth count
  (`conformance/stage6/singleton_function_values`).
- **A write from another class takes the lock too.** `registry.last = name` written anywhere but `Registry` stores
  the value under `Registry`'s lock; the value is computed before the lock is taken, and an object it replaces is
  released after the lock is let go, so no other code runs while it is held.
- **A singleton is never counted.** Fetching one (`var registry = Registry()` in a function) is one load, and
  letting go of it is nothing: a singleton's retain and release compile to nothing
  ([D142](decisions.md)), in generic singletons too.

**When.** Only in programs that make a `Parallel` or run a `parallel_each_` pass, and only for a singleton that a
`Parallel` can reach; in any other program a write from another class is a plain store. **What you notice.** An
uncontended lock per call to such a singleton (two atomic operations), and waiting when two threads call it at
once. `conformance/stage6/singleton_lock_calls` holds all four points from its C in `check.sh`: `Registry` is
locked, pads its lock, calls itself unlocked and locks the write `registry.last = ...` from the entry class, and
`Rules` takes no lock. **Built** (the lock; the padding, the unlocked calls to itself, the locked writes from
outside and the read-only held objects 2026-09-25, proposed by Claude, unconfirmed).

### Thread safety for singletons, the cheapest safe form

**What it does.** For each singleton of the program's own that can change after it is made, the compiler picks
the cheapest form that is as safe as the lock, from what that singleton's functions actually do
([D184](decisions.md)). You write nothing; the source is the same in every form. Three forms are
built, tried in this order:

1. **Nothing, because no `Parallel` reaches it.** The compiler walks the program's calls from every function a
   `Parallel` or the thread pool can run -- every function passed or stored as a value, which is how a function
   reaches `Parallel(worker.run)` or `ThreadPool.submit`, and the member and `filter_` members of every
   `parallel_each_` pass -- following each call to the functions it can reach (by the class of the value it is
   made on, or every class's function of that name when the class is not known, and every member a template name
   such as `sum_price` can stand for). A singleton none of those functions calls is used by one thread only, the
   program's own, and takes no lock. A singleton with a function the compiler can call without a call being
   written -- an operator (`sum`, `equals`, ...), a getter or setter (`get_`/`set_`), `get_at`/`set_at`,
   `missing_function`, `to_string` or `to_debug` -- is always counted as reached.
2. **Nothing, because it never changes.** A singleton whose functions never assign one of its attributes after its
   constructor, whose attributes no other class assigns, and that holds no list, dictionary, function value or
   object that can change, is read-only once made: it takes nothing.
3. **Atomics, for counters and flags.** A singleton whose attributes that change are all whole numbers or `Boolean`s
   (every other attribute set only by the constructor, none holding anything that can change, and none assigned
   from another class), where each function touches that changing state at most once -- one read, one
   `count = count + step` or `count = count - step`, or one `flag = value`, where neither `step` nor `value`
   reads the changing state, and not inside a `while` or through another function of its own that touches it
   too -- has every one of those reads and writes compiled as a single atomic instruction (`__atomic_load_n`,
   `__atomic_fetch_add`, `__atomic_store_n`) and takes no lock. One touch per function is what makes this exactly
   as safe as the lock: the lock makes each function one indivisible step, and so does one atomic instruction.

Only when none of these applies does the singleton take [the lock](#singletons-a-parallel-reaches-take-a-lock).

**Example.** `conformance/stage6/singleton_forms`: four `Parallel` workers and the program's own thread record
41 000 hits into a `HitCounter` (atomics), read a `Settings` made once (nothing), while only the program's thread
writes a `Journal` holding a `List` (nothing: no `Parallel` reaches it). Before, `HitCounter` and `Journal` each
took a lock; now no singleton in it does, and `check.sh` holds that from its C.

**When.** Every build that is not `--hot-reload` (which is not guarded at all yet), in programs that make a
`Parallel` or run a `parallel_each_` pass. An atomic singleton in a program without `Parallel`, or one no
`Parallel` reaches, compiles its reads and writes as plain ones: the executable is the same as with no form at
all.

**What you notice.** Nothing in what a program prints or computes, and no waiting on a counter two threads bump at
once. What a lock gave and these forms keep: each function of the singleton is still one indivisible step. A
singleton whose attribute another class assigns is locked instead (its write takes the lock, above); reading a
singleton's attribute directly from another class takes nothing in any form. **Built** (2026-09-25; proposed by
Claude, unconfirmed: which forms, their order, and the one-touch rule).

### Smaller ones

All **built**, and none of them needs anything from you:

- `join` writes every piece once into one buffer instead of copying the text so far at each step.
- Converting text to text, in `join` on a `List<String>`, is folded away ([D58](decisions.md)).
- A `T?` of a class, list or text is the reference itself, with `null` as the absent case: no wrapper object.
- A generic singleton has one static slot per set of codegen values, so `Column<Health>()` is found without any
  lookup.
- A function passed to a template by name, `names.each(say_hello)` or `people.map(greeter.label)`, is not made into
  a function value: the template is written once for that function and its owner, so the call allocates nothing
  and calls it directly ([D148](decisions.md),
  [collections.md](collections.md#passing-a-function-for-each-element)). Only a function held in a variable is
  called through its `Spite.Function`.
- An `assert` in `library/` writes nothing into the crash trace, decided when compiling, so a library guard costs
  what an `if` costs ([D189](decisions.md)). What you notice: a crash report lists only the failed
  asserts of the program and its `load`-ed packages ([failure.md](failure.md#what-a-crash-reports)).
- A program with no `crash` left after tree shaking writes nothing into the crash trace at all: the trace exists
  only to be printed by a crash, so each `assert` of such a program compiles to its test and its `return`, and
  the trace's 32 entries are not in the program ([D177](decisions.md)). A program that can crash records
  exactly as before.
- A foreign library is closed at exit only if the function that opens it is in the program, so a library nothing
  opens leaves neither its handle nor the code to close it in the program ([D177](decisions.md)). `Console` still
  opens the C library when it is made, since D144 binds its `DynamicLibrary` as an attribute: a program that only
  prints opens it too.

### Proofs that survive a call

**Built.** A proof -- `assert target`, `crash list[index]`, a bound in a `while` -- lets the reads after it skip
the null test and the narrowing ([D169](decisions.md)). A call between the proof and the read keeps
it unless the compiler, following the called function and what it calls, finds that the call may assign an
attribute the proof reads through or shrink a list it reads; so no check is repeated after a call that provably
cannot, and no `const` keyword is needed. To know which function a call reaches, it reads the class of the value
the call is made on: an attribute's or variable's declared type, the class a constructor makes, the class the
function that made the value returns (`var address = heap.allocate(8)` is a `Memory.Address`), or the left side
of a `+` (an address plus an offset is an address); a call on a value whose class it cannot tell may reach every
function of that name. It runs entirely while compiling and emits nothing. What you can
observe: a proof after a call that may change it must be written again, and a call through a function value
keeps no proof about attributes or lists ([failure.md](failure.md#a-call-may-undo-a-proof)).

### Text joined in one piece

**What it does.** `"line {index} of {round};"` and `prefix + name + suffix` are one join, not a chain of pairs:
every piece is computed in order, left to right as written, and the text is made once, at its final length. Before,
each `+` (and each `{...}` hole) made a whole new text, so a text of five pieces made four texts and threw three
away. Pieces the compiler already knows -- written text, a symbol's name such as `attribute.name` in a Symbol
walk -- are joined while compiling, and empty ones are dropped, so `"{index}"` is just the number's text and
`"{attribute.name}="` is one constant.

**When.** Every `+` whose left side is text, and every text with `{...}` holes, in every build. `text = "{text}..."`
still grows `text` in place ([above](#appending-to-text-in-place)).

**What you notice.** Fewer allocations under `--debug-memory` (two per piece that used to be joined:
`conformance/stage6/text_building` went from 43 to 39, `benchmarks/reflection_walks` from 16 356 022 to
11 756 022), and a text built only from pieces the compiler knows answers `'constant'` to `.memory.section`
instead of `'heap'`, as a written text does. `"{name}"` where `name` is text is that same text, not a copy -- text
cannot change, so nothing else can tell. **Built** (2026-09-25; proposed by Claude, unconfirmed).

### Defaults the constructor replaces are never made

**What it does.** `var owner = Owner(0)` followed by a constructor whose first lines are `owner = new_owner` used to
make an `Owner`, then throw it away. Now the object is made with that attribute empty, and the constructor's line
fills it: the default is never made. It applies to each attribute the constructor sets in its opening run of lines
that assign an attribute a parameter or a literal -- before anything else can read it -- when the default is a
construction that has no effect but the memory it takes: a `List`, a `Dictionary`, or a class with no `drop()`,
not a singleton, whose own constructor only copies its parameters and literals into its attributes and whose
own defaults are made the same way. `Spite.Class('Nothing')`, the default of every function value's `returns`
and every attribute's and argument's `class`, is one (three allocations: the class object and its two lists); a
default whose constructor prints is not.

**When.** Every build but `--hot-reload` (whose constructors can be swapped for ones that read the attribute first),
for objects made by their constructor; an object given an allocator on the next line still makes its defaults.

**What you notice.** Fewer allocations under `--debug-memory` -- one per discarded object, and those it holds:
`benchmarks/fused_chain`, which makes 100 000 items that each replace their default `Owner`, went from 300 009 to
200 009, and `benchmarks/reflection_walks`, whose `.attributes` walk makes a `Spite.Attribute` per attribute, from
10 620 024 to 7 620 024. Nothing else: the discarded default was never reachable. **Built** (2026-09-25; proposed
by Claude, unconfirmed; `Spite.Function` and `Spite.Attribute` since the third step of `benchmarks/README.md`).

### A function value describes its arguments when asked

**What it does.** A function value is its own reflection object ([D39](decisions.md)), with `.arguments`, a list of
`Spite.Argument`s. That list used to be filled when the value was made -- two objects per argument, and each
argument's class -- though almost no program reads it. Now the value carries a pointer to a function the compiler
wrote for it, and `.arguments` fills the list the first time it is read (under a lock in a program with threads,
so two threads reading it at once see one list). Together with the discarded defaults above, this answers
`mortaros_missing_decisions.md` item 145 without changing what `.arguments` answers.

**When.** Every function value the compiler makes: `Parallel(summer.total)`, `apply(scorer.score, 3)`, a shape's
function passed on. A `.functions` list is reflection read on purpose, so its values are still described at once.

**What you notice.** Fewer allocations: with the defaults above, a `Parallel` makes 10 where it made 25
(`conformance/stage6/singleton_counts`, two `Parallel`s, went from 134 to 104; `benchmarks/parallel_calls` from
52 per round of two `Summer`s and two `Parallel`s to 22), and passing a function value makes 2 instead of 10 --
the value and its empty list (`benchmarks/function_values`, from 2 000 019 to 400 019). `.arguments` answers the
same list, in the same order, whenever it is read. **Built** (2026-09-25; proposed by Claude,
unconfirmed).

### A list's templates read its elements without counting them

**What it does.** Every member template of `List` -- `sum_price()`, `filter_is_active()`, `each(step)`, `copy()` --
and every fused chain reads each element with `values.read_value(items, index)`, which raises the element's
reference count, and lowers it again when the pass over that element ends. Two writes to every object walked,
and on a list of objects spread through memory they are most of the loop's cost. Now the element is read as it
lies in the list, uncounted, whenever nothing that runs while it is held can let go of anything: the compiler
walks the rest of that pass -- the member function it calls, or the function passed in, and everything those call
-- and lets it borrow only when none of them assigns an attribute that holds an object (a number, `Boolean` or text
attribute is fine), removes from or replaces into a list or dictionary, or calls through a function value. A
member read from a borrowed element (`map_owner`) is borrowed the same way. Anything that keeps the element --
`collected.append(item)`, `return item` -- still counts it, as before.

**When.** Every build but `--hot-reload`, in the functions of `List` (the library's templates, a program's own
templates reopening `List`, and fused chains), when the proof above holds; otherwise the element is counted as
before. `parallel_each_` passes borrow too, which saves an atomic increment and decrement per element in a
program with threads.

**What you notice.** Speed: `benchmarks/fused_chain`, four chains over 100 000 objects run 300 times, went from
332 ms to 185 ms. Allocations and everything a program prints are the same. **Built** (2026-09-25; proposed by
Claude, unconfirmed).

### A number joined into text is written in place

**What it does.** `"line {index} of {round};"` used to turn `index` and `round` into texts of their own -- two
allocations each -- only to copy them into the result and free them. An `Integer` or `Long` piece of a text join,
or of an append in place (`text = "{text}{count}"`), is now written as digits into a buffer in the function's own
frame and copied from there: the same digits the library's `to_string()` writes, and no allocation. A program that
reopens `Integer` or `Long` with a `to_string()` of its own keeps calling it.

**When.** Every build, for whole numbers of those two classes. A number alone in a text (`"{index}"`) still makes
one text, since that text is the result.

**What you notice.** Fewer allocations: `conformance/stage6/text_building` went from 39 to 35, and
`benchmarks/text_building` from 5 500 225 to 800 267. **Built** (2026-09-25; proposed by Claude, unconfirmed).

### Allocation is the C library's, counted only where read

**What it does.** Every Spite object is made with `SPITE_MALLOC` and let go with `SPITE_FREE`, and what those are
is decided per program. In an ordinary build they are the C library's `malloc`, `realloc` and `free`, with
nothing beside them: no counter, no table, no list of kept blocks. A program that reads
`Memory.Heap.live_allocations()` (or `Program.live_allocations()`, which asks it) gets a counter beside each call
instead -- atomic in a program that starts a thread -- and the tree shaker decides which: the counter is written
only when `live_allocations` is still in the program after shaking. A `--debug-memory` build routes every call
through its allocation table instead, and only that build's C has the table.

**When.** Every build but `--debug-memory`. An inspectable build (`--development`, `--hot-reload`, `--repl`) is
not shaken, so it counts ([D143](decisions.md)).

**What you notice.** Nothing: `live_allocations()` answers the same wherever it is called. `examples/hello`'s C
went from 1 356 lines to 762 with this, the crash trace and the foreign library changes in
[Smaller ones](#smaller-ones); 922 once `Console` bound its library as an attribute (D144).

**Not built: keeping freed small blocks for reuse.** Keeping each freed object of up to 256 bytes on a per-thread
list for its size, for the next allocation of that size, was built and taken out again (2026-09-25): it was not a
clear, repeatable gain on SlopEngine, the program it was for. Against the plain C allocator, `clang -O2`, best of
nine interleaved runs on Mortaro's machine: `examples/stress` ticks 42.9 ms with it and 41.9 without in parallel,
49.0 and 50.9 single-threaded, 60 ticks after despawning 87.7 and 87.2 ms; only the 200 000 spawns (449 and 504
ms) and `flex_layout` (about 3 ms of 70) were faster; the Vulkan UI tests (`click_counter_test`,
`text_field_test`) did not move beyond noise. It sped up the small benchmarks (`small_allocations` 84 against 140
ms) at the cost of up to 2 MB kept per thread; `benchmarks/README.md` has both sets of numbers.

### A dictionary hashes a key once, cheaply

**What it does.** `library/dictionary.spite` hashed a key with a multiply and a division by a prime for every
character, and on a hit compared the whole key text. It now hashes with a multiply and an exclusive or per
character on an `UnsignedLong` (FNV-1a), keeps 32 bits of that hash in the slot beside the key's position, and
compares key texts only when those bits match.

**When.** Every `Dictionary`, in every build. **What you notice.** Speed: `benchmarks/dictionary_keys` went from
413 ms to 282 ms. Keys, values and their order are the same, and so is every allocation: the slot table is still
one block, twice as large. **Built** (2026-09-25; proposed by Claude, unconfirmed).

### Reading through a `type` without counting

**What it does.** A system's `moving.position.left = moving.position.left + moving.velocity.across` reads
`position` through the `type` `Moving`, which answers the component retained -- or, when the value's class has no
such attribute, a fresh default -- and the component is released as soon as the number is read. Now, when that
component only has a number, `Boolean` or other plain attribute read or written, the compiler asks the `type` for
the component as it lies in the value, uncounted: a read of a class without the attribute answers the attribute's
default, as the fresh default object would have, and a write to one lands in a scratch object in the frame, as it
used to land in a default object that was then thrown away. A write borrows only when computing the value it
stores can let go of nothing (the same proof as [a list's templates](#a-lists-templates-read-its-elements-without-counting-them)),
and only when the value holding the component is itself held for the whole statement.

**When.** Every build but `--hot-reload`, for a plain attribute of a class read through a `type` attribute:
`moving.position.left`, not `moving.position` passed on or kept.

**What you notice.** Speed, in systems that walk components through a `type`: `benchmarks/stress`'s
`update_each` functions no longer count anything. Allocations and results are the same. **Built** (2026-09-25;
proposed by Claude, unconfirmed).

### A proven divisor is not checked

**Built.** A whole-number `/` or `%` checks its divisor for zero (D201, [values_and_types.md](values_and_types.md)),
unless the divisor is a constant other than zero, or a proof in scope says it is not zero: `assert parts != 0`,
`crash parts != 0`, `if parts != 0 { }` or `parts > 0` in a condition, the same proofs D169 keeps across a call
that cannot change `parts` and drops across one that may. The check, where it stays, is one compare and a
branch the CPU predicts. What you can observe: nothing but speed; `check.sh` holds that
`conformance/stage6/division_by_zero`'s proven `whole / pieces` carries no check in its C.

## Planned

Decided by Mortaro, not built yet. When one is built, it moves up to **Built** in the same change.

### Thread safety for singletons, the rest of the plan

A singleton reached from a `Parallel` is made thread-safe by the compiler, with no keyword
([D183](decisions.md)), and the compiler picks the cheapest form that is safe for what that singleton's
functions actually do ([D184](decisions.md)). Built (above): nothing for read-only state or a
singleton no `Parallel` reaches, atomics for counters and flags, and the lock as the fallback. Not built yet: state
that is only appended to (a log, a command queue) gets a buffer per thread merged in order; state each thread
touches its own part of is split per thread; and reads that far outnumber writes take a reader-writer lock. Its
functions may hand out only numbers, text, copies or other singletons made safe the same way, which is a
compile-time check at `return`, also not built. All of it is absent from a program that never makes a `Parallel`.
You will write nothing.

### Copies that cost nothing

Every class is passed by reference and `copy()` gives an independent one; that is the whole API, and the compiler
optimises behind it ([D149](decisions.md)): a copy used only once is passed by value instead of
allocated; a copy that is never changed shares the original, when that is cheaper; an object that never escapes
its function is laid out inline or in registers; and reference counting is left out wherever ownership is
provable. You keep writing `copy()` where you mean an independent object. (D152's allocator set right after
construction, the one part of this already built, is [above](#an-allocator-set-after-construction-is-where-the-object-is-made).)

### Other planned optimisations

- **A list's buffer in its list's allocator** ([D154](decisions.md)): a `List` given an allocator is made there,
  but its buffer of references still comes from the heap; and `Vector<T>`, which holds its items inline, does not
  exist yet ([memory.md](memory.md#allocators-memoryallocator--implemented-for-objects-a-lists-buffer-and-vectort-planned)).
- **Short symbols inline** ([D70](decisions.md)): a short symbol held as a small inline string rather
  than a pointer into the symbol table.
- **Crash text out of the binary** ([D32](decisions.md)): a `crash` or `assert` site's source text
  lives only in the `<output>.crashes` map written beside the program, and an optimised build carries just the id.
  The map is written today, but the binary still carries the text.
- **A build report of what could not be optimised** ([D36](decisions.md)): not "400 copies elided"
  but "3 copies could not be elided, and the callee that writes the field", so every line is actionable.

## Adding one

Every optimisation the compiler starts making on its own is added to this page in the same change, with what it
does, when, whether it is built, and anything a user could observe ([D185](decisions.md),
[D102](decisions.md)). An optimisation with a cost that cannot be removed says so here; one that
contradicts the rules on another page is recorded in `mortaros_missing_decisions.md` for Mortaro.
