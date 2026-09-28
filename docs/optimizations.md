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
| [A counted loop of calls to one singleton takes its lock once](#a-counted-loop-of-calls-to-one-singleton-takes-its-lock-once) | built | every but `--hot-reload`, `--repl`, `--repl-port`, decided per loop | one lock for the loop instead of one per call, only with `Parallel` |
| [A singleton's reading functions do not exclude each other](#a-singletons-reading-functions-do-not-exclude-each-other) | built | every but `--hot-reload`, decided per singleton | readers count on their own cache line; 2 KiB of counts per such singleton |
| [While no task runs, a singleton's lock is skipped](#while-no-task-runs-a-singletons-lock-is-skipped) | built | every but `--hot-reload`, only with `Parallel` | one load per locked call, two atomic additions per task |
| [The fault handler is in every program](#the-fault-handler-is-in-every-program) | built (a cost, not an optimisation) | every | about 3.7 KB of code and 32 bytes and a name per function; one store per foreign call |
| [Smaller ones](#smaller-ones) | built | every | nothing |
| [Proofs that survive a call](#proofs-that-survive-a-call) | built | every | a proof after a call that may change it is written again |
| [Text joined in one piece](#text-joined-in-one-piece) | built | every | fewer allocations; a text made only of constants is constant |
| [Defaults the constructor replaces are never made](#defaults-the-constructor-replaces-are-never-made) | built | every but `--hot-reload` | fewer allocations |
| [A function value describes its arguments when asked](#a-function-value-describes-its-arguments-when-asked) | built | every | fewer allocations per function value and per `Parallel` |
| [A list's templates read its elements without counting them](#a-lists-templates-read-its-elements-without-counting-them) | built | every but `--hot-reload` | nothing but speed |
| [A number joined into text is written in place](#a-number-joined-into-text-is-written-in-place) | built | every | fewer allocations |
| [Allocation is the C library's, counted only where read](#allocation-is-the-c-librarys-counted-only-where-read) | built | every but `--debug-memory`, decided per program | nothing: `live_allocations()` still answers |
| [A dictionary hashes a key once, cheaply](#a-dictionary-hashes-a-key-once-cheaply) | built | every | nothing but speed |
| [A dictionary keyed by numbers hashes the numbers](#a-dictionary-keyed-by-numbers-hashes-the-numbers) | built | every | fewer allocations; compiling takes a second pass |
| [Reading through a `type` without counting](#reading-through-a-type-without-counting) | built | every but `--hot-reload` | nothing but speed |
| [Short text lives inside the `String`](#short-text-lives-inside-the-string) | built | every | fewer allocations; `.memory.section` of built text; a box when text travels as a shape |
| [Maths on constants is worked out while compiling](#maths-on-constants-is-worked-out-while-compiling) | built | every | nothing but speed; a folded call is the compiling machine's C library's answer |
| [A local list of known size lives in the frame](#a-local-list-of-known-size-lives-in-the-frame) | built | every but the inspectable ones | fewer allocations |
| [A loop over plain values reads its count once and its items unchecked](#a-loop-over-plain-values-reads-its-count-once-and-its-items-unchecked) | built | every but `--repl`, `--repl-port` and `--hot-reload` | nothing but speed |
| [Objects that never leave their function live in the frame](#objects-that-never-leave-their-function-live-in-the-frame) | built | every but the inspectable ones | fewer allocations; `.memory.section` answers `'stack'` |
| [The C is compiled in parallel units, and cached](#the-c-is-compiled-in-parallel-units-and-cached) | built | `--optimized` (any build given `--translation-units`), but not `--hot-reload` | nothing but build time; `.spite-cache/objects` grows |
| [A release build is `-O3` with link-time optimisation](#a-release-build-is--o3-with-link-time-optimisation) | built | `--optimized` | nothing but speed, and a slower link |
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
- `$system_type.has_function("run_each")` ([D114](decisions.md));
- `$system_type.function_waits("update_each")` ([D209](decisions.md)), answered from the functions the compiler
  turns into state machines;
- `phase.argument_count()` and `$system_type.argument_count("update_each")` ([D219](decisions.md)), a whole
  number compared with `==`, `!=`, `<`, `<=`, `>` or `>=`, so a runner compiles only the branch that fits a
  system's arity;
- `$component_type.fits_vector()` and `attribute.class.fits_vector()` ([D217](decisions.md)), which is how
  `Items<T>` picks inline or reference storage ([below](#an-items-storage-is-chosen-while-compiling));
- a test on a codegen value's own codegen values, `$list_type.element_type == Float`,
  `$map_type.value_type == Item`, `$holder_type.held_type == String`, to any depth, in every branch of an
  `else if` chain (`conformance/stage6/codegen_member_fold`; fixed 2026-09-26, when only a plain `$T` folded);
- `attribute.class == X` in an attribute walk, for an attribute of any type (D237); and where a value known only at
  run time is compared with a `.class` known while compiling (`given == known.class`), the comparison is a class-id
  test and no `Spite.Class` object is made (`conformance/stage6/walked_class_fold`);
- `not`, `and`, `or`, `==` and `!=` over any of these. An `and` whose left side folds to `false`, and an `or` whose
  left side folds to `true`, fold whatever the right side is, as the run-time `and` and `or` would never look at
  it: so `$list_type.element_type == List and $list_type.element_type.element_type == Float` folds for a list of
  text, whose items have no `element_type` to ask about.

An `assert` or `crash` whose condition is one of these folds the same way: a check that holds writes nothing,
and one that fails writes its failure (the default returned, or the crash report) with no test, the rest of its
block not compiled ([metaprogramming.md](metaprogramming.md#codegen-values---implemented)).

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
    var red = Team("red", true, 3)
    teams.append(red)
    var blue = Team("blue", false, 5)
    teams.append(blue)
    var green = Team("green", true, 4)
    teams.append(green)
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
  through, copies, compares, turns into `text`, hands to a `TypedMemory` or lends to a function of its own class
  proven to keep nothing ([D211](decisions.md)) -- or, in `library/`, lends to a function of a `DynamicLibrary`
  the class holds, the operating system call that fills it -- never stores, returns, resizes or passes anywhere
  else -- gets a slot in the function's own frame: 256 bytes, or exactly a literal size up to 256. A larger size at run time
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

A buffer lent to a helper (`fill_squares(squares, count)`, `add_up(squares, count)` in
`conformance/stage6/lent_buffers`) stays in the frame when the helper only reads and writes through it; one that
returns or stores it stays on the heap. Measured (a function that allocates 64 to 96 bytes, lends them to two
helpers and frees them, 5 000 000 times): 210 ms against 106 ms with the helpers not inlined (`clang -O1
-fno-inline-functions`); with `clang -O2` both are 0 ms, since clang inlines the helpers and removes the heap call
itself.

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

**What it does.** A number, `Boolean`, enum value, `Symbol` or `String` is a plain value everywhere the compiler
can see its type. It is put in a box -- one small object, released like any other -- only where it has to travel as
a `type` shape (a `Printable`, a `Debuggable`, an empty `type` that accepts anything) and be called through it
([D109, D164](decisions.md)). A written text's box is part of the program and allocates nothing
([short text](#short-text-lives-inside-the-string)). Class instances are objects already and are never boxed.

**When.** Passing a plain value where a shape is wanted, reading `attribute.value` of a number or text
attribute, or a class test against a number class.

**What you notice.** One allocation per boxed value under `--debug-memory`. The visible cost today: every value
given to `console.print` is passed as a `Printable`, so printing a number boxes it; text made while the
program runs is boxed too, since the sixteen bytes of a `String` are not an object. **Built** (D109's print
row, and D164's `attribute.value`, filled only in a program that reads it).

### A variadic list the callee only reads lives in the caller's frame

**What it does.** The `...values` of a variadic call arrive in a `List`. When the call is a statement of its own
(`console.print(name, count)`), outside `and`/`or` and outside a function that waits, and the function called only
reads its list -- `count()`, `is_empty()`, `[index]`, `get_at`, `find_at`, `first`, `last`, `contains`, `join`, or
passing it to a function of its own class that only reads it too -- the list and its items are in the caller's
frame: no allocation for the list or its items, and its elements (the boxes above) are released after the call
([D211](decisions.md)). A function that stores, returns, grows or passes on its list anywhere else gets a list on
the heap as before, and so does a function of a `--hot-reload` build's own classes, which can be swapped.

**What you notice.** Two allocations fewer per such call under `--debug-memory`: `text_building` allocates 19 times
(31 before), `short_text` 73 (100), `fused_chain_allocations` 11 (13), `folded_function_value` 9 (13).
**Built** (2026-09-26, D211 item 74).

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
  code ([D174](decisions.md)), so a program that never waits still answers. It is one relaxed load of a flag
  the REPL's thread and the file watcher raise when they have something ([D211](decisions.md)); only then does
  the pass call `Scheduler.check_point()`. Measured on a loop of 400 000 000 passes in a `--repl-port` build: 1 566 ms
  when every pass called it, 106 ms with the flag.

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
it is made gets a lock of its own, taken around every one of its functions that touches what can change
([D183](decisions.md); which functions, below).
It can change when one of its functions assigns one of its attributes outside its constructor, when code in
another class assigns one (`registry.last = name`), or when it holds a list, a dictionary, a function value, or an
object of a class that can change -- an object whose class never assigns its attributes after its constructor,
and holds nothing that can change either, is as read-only as a number, so a `Rules` holding a `Limits` made once
takes no lock. This is the fallback of [D184](decisions.md): the compiler takes it only when none of
the cheaper forms in the next section is proven safe for that singleton. The check that its functions hand out
nothing they own is not built.

How the lock is kept cheap:

- **A function that touches none of the changing state takes no lock.** Only a function that reads or writes an
  attribute that can change (one assigned after the constructor, here or from another class, or holding a list,
  a dictionary, a function value or an object that can change), or calls one of its own functions that does, or
  calls out to code that can call back into the singleton (from the call-effects facts), is wrapped in the lock.
  `make_piece(size)`, which only computes from its arguments, or a function that only reads an attribute set once
  in the constructor, runs as written. This is as safe as locking it -- a function that touches nothing that
  changes is one indivisible step on its own -- and it removes a deadlock: a locked function that polls a
  `Parallel` whose work calls a stateless function of the same singleton used to wait for that work while the work
  waited for the lock. The compiler decides it from the function's C: every use of the singleton that is not a
  read of an attribute that never changes, or a call to one of its own functions, counts as touching it
  (`conformance/stage6/singleton_stateless_calls`, whose C `check.sh` holds: `Workshop.build` is locked and
  `Workshop.make_piece` is not).

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
  released after the lock is let go, so no other code runs while it is held. A read from another class
  (`var last = registry.last`) takes it too, around the one load (and the count of what it reads), since D211.
- **A singleton is never counted.** Fetching one (`var registry = Registry()` in a function) is one load, and
  letting go of it is nothing: a singleton's retain and release compile to nothing
  ([D142](decisions.md)), in generic singletons too.

**When.** Only in programs that make a `Parallel` or run a `parallel_each_` pass, and only for a singleton that a
`Parallel` can reach; in any other program a write from another class is a plain store. **What you notice.** An
uncontended lock per call to such a singleton (two atomic operations), and waiting when two threads call it at
once. `conformance/stage6/singleton_lock_calls` holds these points from its C in `check.sh`: `Registry` is
locked, pads its lock, calls itself unlocked and locks the write `registry.last = ...` and the read of
`registry.last` from the entry class, and
`Rules` takes no lock. **Built** (the lock; the padding, the unlocked calls to itself, the locked writes from
outside and the read-only held objects 2026-09-25, proposed by Claude, unconfirmed; no lock for a function that
touches no changing state 2026-09-26, proposed by Claude, unconfirmed).

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
singleton's attribute directly from another class takes the singleton's form too ([D211](decisions.md)): in a
program where a `Parallel` reaches it, `registry.last` read from another class takes its lock, an atomic
counter's attribute is read with one atomic load, and a singleton that never changes is read plainly. `counter.hits
= counter.hits + 1` from outside is still a read and a write, two steps. **Built** (2026-09-25; proposed by
Claude, unconfirmed: which forms, their order, and the one-touch rule; the reads 2026-09-26).

### While no task runs, a singleton's lock is skipped

**What it does.** Every function a singleton's lock wraps first asks whether any work is on the thread pool: the pool
counts every task from the moment it is handed out until it has run (`spite_tasks_in_flight`, one add and one
subtract per task), and when the count is zero -- before the first `Parallel`, after the last one has been read,
between an engine's stages -- only the program's own thread runs the program, so the function runs without the
lock ([D267](decisions.md); proposed by Claude, unconfirmed). It notes on a small stack of its thread's that it
skipped this singleton's lock, and if it starts a task itself before it returns, the pool takes every lock the
thread skipped before the task is counted, and the function lets it go when it returns: so the task finds the
singleton locked exactly as it would have, and nothing it can see differs.

**When.** In every wrapper D183's lock gives a function, writing and reading ones alike, in a program that makes a
`Parallel` or a `parallel_each_` pass; sixteen skipped singletons deep at most, beyond which the lock is taken as
before. Writes and reads of attributes from other classes and counted loops (D265) keep their lock.

**What you notice.** Speed where the program's own thread calls a locked singleton while nothing runs on the pool
-- an engine applying queued inserts between stages: `benchmarks/singleton_unshared`, ten million calls on the
program's thread with no task in flight, 19 ns a call with the lock, 9 ns now, against 4 ns with no lock at all
written by hand. In a spawn-shaped program doing real work per call (`Column<T>` inserts: growing a list, appending
to an `Items`, reading a row) the lock was about 1 ns of 14 ns a call, so there it measures 13 ns. Each call reads
one counter no thread writes while it is zero, and each task costs two atomic additions. **Built** (2026-09-27).

A `copy()` or `deep_copy()` of a class that holds nothing counted as one `memcpy` of its attributes, which
SlopEngine asked for at the same time, was measured and not built: 200 000 copies of a two-`Float` class took
31-32 ns each before and 36-38 ns after, the allocation being nearly all of it. Measured again on a six-attribute
class (five `Float`s and an `Integer`), 20 000 000 copies at `clang -O2`: 785-797 ms written attribute by attribute,
783 ms and more as one `memcpy` -- the C compiler already turns the attribute copies into a handful of wide moves
(five instructions after the allocation either way), so a `copy()` of a class whose attributes all fit a `Vector` is
a `memcpy` in the machine code today, and what is left is its allocation (about 39 ns of a copy that is kept).

### A singleton's reading functions do not exclude each other

**What it does.** A locked singleton's function that only reads its state -- an engine column's `at(row)`, a lookup --
no longer takes the lock itself. It takes the readers' side: it adds one to a count of its own thread's (one of 32
counts, each on a cache line of its own), checks that no function that writes holds the lock, reads, and takes the
one away. A function that writes takes the lock as before and then waits until every count is zero, so it never
runs beside a reader, and readers never run beside it ([D266](decisions.md); proposed by Claude, unconfirmed). Readers
on different threads touch no line in common, so eight systems reading one column per row no longer hand a cache
line from core to core on every call.

**When.** For each function the lock would wrap (D183's fourth form), in a program that makes a `Parallel`, when the
compiler proves from its source that it changes nothing: its statements declare and assign only its own locals,
branch, loop, `return`, `assert` or `crash`; it calls only reading functions of its own class, the reading members
of a `List` or `Dictionary` it holds or is passed (`count`, `get_at`, `[]`, `get`, `has`, `is_empty`, `first`,
`last`, `contains`, `keys`, `values`, `join`, `copy`), anything on text and numbers but a `write_` or `copy_to`,
`read_value`-like reads of `TypedMemory` and `InlineMemory`, and reading functions of a singleton that holds no
state (SlopEngine's `Raw.read_long`); it reads attributes that have no getter; its text has no holes (a hole could
call `to_string`); and its operators are on numbers, or the program declares no operator function at all. Anything
else is a writing function. And it is only for a singleton that the work of a `Parallel` only reads: none of its
writing functions, and no function that writes its attributes from another class, is reachable from what a
`Parallel` or a `parallel_each_` pass runs -- SlopEngine's columns, written between stages and read by the systems
of a stage. A singleton that is also written from the pool keeps the plain lock: a write there would scan the counts
on every call (measured: `Row.advance()` per entity took SlopEngine's `stress` tick from 8 ms to 25 ms). A
singleton with reading functions that qualifies gets the counts; its writing functions,
writes and reads of its attributes from other classes, and counted loops of calls (below) use the matching side, and
a counted loop that calls only reading functions takes the readers' side once for the whole loop. The thread that
holds the writers' side reads without counting, so a writing function calling out to code that reads back is not
held up by itself.

**What you notice.** Speed where several threads read one singleton: `benchmarks/singleton_reads`, eight `Parallel`
systems each reading 15 000 rows of one column through `at(row)`, went from 202 ns to 6 ns per row read (24 ms to
0.75 ms a tick). A reading call on one thread costs about the same as before (a locked add on a line only that thread
writes, instead of a compare-and-swap on a shared one). A write now also looks at the 32 counts once per outermost
call: about 32 loads that stay in the writing core's cache while nothing reads. Each singleton with reading
functions carries 2 KiB of counts. What a program computes is unchanged: a reading function still sees the
singleton's state whole, never half-way through a write. **Built** (2026-09-26).

### A counted loop of calls to one singleton takes its lock once

**What it does.** A `while` that calls functions of one locked singleton many times -- a worker removing rows through
`columns.remove_row(entity)`, a system adding to a tally -- takes that singleton's lock once around the whole loop
and calls the functions' unlocked bodies inside it, instead of taking and letting go of the lock on every call
([D265](decisions.md); proposed by Claude, unconfirmed). One of those per call is two atomic operations when no
other thread wants the lock, and when other threads do, every call hands the lock's cache line from core to core.

**When.** In a program that makes a `Parallel`, for a `while` in any class but the singleton itself when all of this
holds, so that the lock held longer can neither deadlock nor wait on anything:

- **It ends on its own.** It is counted: `index < bound`, `index` a local stepped by `index = index + 1` as the
  body's last line and assigned nowhere else in it, and `bound` a whole number, a name the body never assigns, or
  `.count()` of a list of plain values. So it never waits for another thread to change the singleton (a poll such
  as `while got == 0 { got = mailbox.take() }` keeps a lock per call), and every `while` inside it is counted too.
- **It locks nothing else.** Its calls are functions of that one singleton, reached through the attribute that binds
  it, and the reads and writes of a list of plain values (`count`, `get_at`, `set_at`, `append`, `contains`,
  `is_empty`, `[]`); what it computes is numbers, `Boolean`, text without holes and enum values, so no operator,
  getter or `to_string` of a class of the program's can run in it. A lock it takes nothing else under adds no new
  order between two locks: whatever the singleton's functions lock, they lock under its lock already.
- **It waits for nothing.** No `Parallel` or `Concurrent` is read in it (none is even named), it cannot `return`,
  and the singleton's functions it calls can reach no wait (the facts `function_waits` uses) and no `Parallel`,
  `Concurrent`, `ThreadPool`, `Scheduler`, `Lock`, `Program`, `Console`, `File` or `Socket`.
- **The lock is real.** At least one of the functions it calls takes the lock (D183's fourth form); a loop calling
  only functions that touch no changing state, or a singleton that takes no lock, is left as it was.

Not in a `--hot-reload`, `--repl` or `--repl-port` build, nor in the resumable copy of a function a `Concurrent`
runs. **What you notice.** Speed: `benchmarks/singleton_locks`, 800 000 calls from eight workers, went from 2.3 ms to
0.35 ms with a singleton per worker and from 51 ms to 0.6 ms with one singleton for all of them (the lock handed
between cores on every call); one thread, 1.7 ms to 0.4 ms. Nothing a program prints changes: the loop's calls run
exactly as before, and other threads' calls on the singleton wait until the loop is over instead of slipping in
between two of its calls, which is one of the orders they could already run in. A long counted loop keeps other
threads that want the singleton waiting for all of it. In the C, the loop is between `spite_coarse_<n>_enter()` and
`spite_coarse_<n>_leave()` and calls `<function>___unguarded`; `conformance/stage6/coarse_locks` holds that in
`check.sh`. **Built** (2026-09-26).

### An argument its caller holds is passed without counting

**What it does.** Passing an object to a function counts it once more for the callee's own name and lets that
count go when the callee returns: two atomic operations on the object's header in a program with threads. When
the argument is a name the caller already holds for the whole call -- one of its own parameters, or a local it
owns -- the count adds nothing: nothing the call runs can reassign the caller's name. So the call goes to a copy of
the function, `<name>___held_<positions>`, in which those parameters are not released at its end, and the caller
passes them as they are ([D270](decisions.md)). SlopEngine's `run_positions(..., rows)` calling
`matcher.match_into(entity, rows)`, which hands `found` on to `find_row(index, entity, found)` for every column,
counts nothing per entity now: each is a `___held_` copy passing its own parameter on to the next.

**When.** Every build but `--hot-reload`, `--repl`, `--repl-port` and the resumable copy of a function a
`Concurrent` runs, for a call written by name to a program's own function (not the library's), at a place whose
parameter is a class, list or dictionary (not text, a union or a variadic list), when the callee never assigns that
parameter itself; not for a constructor, and not where the call's result is made in the caller's frame (the
frame-made result keeps the ordinary function). Keeping the parameter needs nothing extra: storing it, returning it
or passing it to a function that keeps it counts it there, as storing any name does.

**What you notice.** Speed: `benchmarks/held_arguments`, a `Vector` passed on through two calls for each of a
million entities in a program with threads, 18 ns to 13 ns an entity. A `___held_` function appears in the C
beside the ordinary one for each set of positions a program passes this way (about 3% more C in the compiler
compiling itself), and the ordinary one is shaken out when nothing else calls it. Allocations and results are the
same (`conformance/stage6/held_arguments`, whose C `check.sh` reads: a bag passed on, kept, returned and assigned
over). **Built** (2026-09-28; proposed by Claude, unconfirmed).

### A singleton's attribute that never changes is read in place

**What it does.** Reading an attribute of a singleton from another class -- `Column<Heat>().values[rows[0]]`,
`column.values.count()` -- counted the attribute's object for the expression and, in a program where a `Parallel`
reaches the singleton, took its lock or readers' side around the read ([D211](decisions.md)). When the attribute
holds an object (a class, list or dictionary, not text or a number) that nothing assigns after the singleton is
made -- no function of the singleton outside its constructor and no other class assigns it -- the object stays the
singleton's for the rest of the program, so the read is the attribute's address: no count and no lock
([D271](decisions.md)). What is then done with the object is unchanged: a call on it takes whatever that object's
functions take, and keeping it in a name or an attribute counts it there. This also takes the lock out of a loop
that only reads through such an attribute, since there is none left to hoist.

**When.** Every build but `--hot-reload`, `--repl` and `--repl-port`, for a singleton of the program's own whose
attribute has no getter. Whether anything assigns the attribute is read from the call effects of the whole program
(`generation/call_effects.spite`), the same facts D183 decides its locks from.

**What you notice.** Speed: `benchmarks/singleton_attributes`, a million items read through a column's attribute
in a `Parallel`, 6.0 ns to 3.2 ns a read; in SlopEngine's `stream_bench` each inline component of a row was a
count, a lock and a release per row. Nothing a program prints or allocates changes
(`conformance/stage6/singleton_attribute_reads`, whose C `check.sh` reads: `names` read in place from a `Parallel`,
`current`, which a function replaces, still read under the lock). **Built** (2026-09-28; proposed by Claude,
unconfirmed).

Two readings of the call effects became more exact on the way, since both proofs above depend on them. A plural
call to a template of the same class (`fill_attributes(row, found)`, `run_phases_each()`) is now followed into its
template, so what the template's lines call -- a reference column's `at` -- counts as reached from the caller, from
a `Parallel` included; before, it was reached by nothing, so such a column could take no lock at all while pool
work called it (a bug under [D244](decisions.md)). And an attribute read through a `type`
(`moving.trail.lefts.append(...)`) is known to be the class the `type` declares for it, and assigning an attribute
that holds a plain value in every class of the program (`moving.position.left = ...`) lets go of nothing, so a
system writing its components' numbers no longer counts as letting go of objects.

### The fault handler is in every program

**What it is.** The one piece of C nothing tree-shakes: a program can meet a native fault -- a null read inside a
foreign library, a stack overflow -- whatever it uses, and [D244](decisions.md) makes a silent end a bug, so every
program installs a handler that reports it ([failure.md](failure.md#what-a-native-fault-reports)). It is fixed
code, not a runtime system ([D177](decisions.md)): nothing runs until a fault, and it is written once, after every
other function, from what tree shaking kept.

**What it costs**, measured on x64 Windows with the C compiler `check.sh` uses:

- **Code**: about 3.7 KB of machine code (3 707 bytes at `-O0`, 3 678 at `-O2`), 0.4 KB of fixed text, and its unwind
  data. Installing it is two system calls at start (three on Windows, where a vectored handler also catches a
  corrupted heap, D260), and one more in each thread the program starts. The vectored handler runs for every
  exception the process raises and returns after one comparison unless it is `0xC0000374`; Spite raises none, so
  only a foreign library that uses exceptions of its own ever pays it.
- **The function table**: 32 bytes per function the C keeps, plus its name; the file and class text is shared by a
  class's functions. `examples/hello` keeps 73 functions, about 4 KB; the compiler keeps about 3 360, about 210 KB of
  its 4.3 MB. In an `--optimized` build, taking every function's address keeps an out-of-line copy of a small
  `static` function the C compiler would otherwise have inlined everywhere and dropped: 1.7 KB more code in
  `examples/hello`; calls to it stay inlined.
- **One store per foreign call**: each call writes a pointer to a fixed text, naming what it calls and from where,
  into a thread-local before it goes in, in every build. A loop of 300 000 000 calls into a one-line C function
  measured 1.33 ns a call with the store and 1.33 ns without (thread-local or not; best of seven runs each), so it
  is not kept to inspectable builds ([D214](decisions.md)).
- **Frame pointers**, which the stack walk needs on Linux and macOS, are kept only where they are free or asked
  for: a build without `--optimized` has them anyway, and an inspectable build is compiled with
  `-fno-omit-frame-pointer` there. The compiler compiling itself at `-O2` took 1 847-1 879 ms without them and
  1 856-1 918 ms with them (about 1% slower, four runs each, alternating), so an `--optimized` production build does
  not keep them and its report names the faulting function without the chain. Windows walks the stack from the
  unwind data every 64-bit program carries, in every build, at no cost.
- **Stack**: 16 KB of each thread's stack on Windows, and a 64 KB alternate signal stack per thread on Linux and
  macOS, are kept back so a stack overflow can still be reported.

**When.** Every build. **What you notice.** A fault prints a report instead of nothing, and the program is a few
kilobytes larger. **Built.**

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
- An attribute written through a local or a parameter, `item.index = 293`, is written through that local,
  `(item_)->index_ = 293;`, with no temporary holding the reference first: a local cannot change while the value
  is worked out. An object that is any other expression is still evaluated once into a temporary. Nothing a
  program can observe changes; the C is shorter, by about one line in twenty for a folder of data records.
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
of a `+` (an address plus an offset is an address), and `List`, `Dictionary` or `String` for a value of those
types. A receiver that is an expression has its type's class too: `File(path).read()` reaches only `File.read`,
`names.copy().count()` and `lists[0].count()` only `List.count`, and an item of a `Vector` or `Dictionary` its
element's class, so a program's own `read` or `count` no longer undoes a proof it cannot touch. A call on a value
whose class it cannot tell (a shape, a type parameter, a function value) may reach every function of that name,
and a `clear` or `remove_...` on a list reached through `[]` or a call may be any list, so it keeps no proof
about a list. A `return`'s own
calls keep every proof, since nothing after the `return` runs. It runs entirely while compiling and emits nothing. What you can
observe: a proof after a call that may change it must be written again, and a call through a function value
keeps no proof about attributes or lists ([failure.md](failure.md#a-call-may-undo-a-proof)).

### Text joined in one piece

**What it does.** `"line {index} of {round};"` and `prefix + name + suffix` are one join, not a chain of pairs:
every piece is computed in order, left to right as written, and the text is made once, at its final length. Before,
each `+` (and each `{...}` hole) made a whole new text, so a text of five pieces made four texts and threw three
away. Pieces the compiler already knows -- written text, a symbol's name such as `attribute.name` in a Symbol
walk -- are joined while compiling, and empty ones are dropped, so `"{index} of"` is two pieces, not three, and
`"{attribute.name}="` is one constant.

**When.** Every `+` whose left side is text, and every text with `{...}` holes, in every build. `text = "{text}..."`
still grows `text` in place ([above](#appending-to-text-in-place)).

**What you notice.** Fewer allocations under `--debug-memory` (two per piece that used to be joined:
`conformance/stage6/text_building` went from 43 to 39, `benchmarks/reflection_walks` from 16 356 022 to
11 756 022), and a text built only from pieces the compiler knows answers `'constant'` to `.memory.section`
instead of `'heap'`, as a written text does. **Built** (2026-09-25; proposed by Claude, unconfirmed).

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

A function called on the element is looked up on the element's own class. Until 2026-09-26 it was looked up by
its name among every class, so `List<Particle>.each_step()` counted its elements only because the library's
`ReadEvaluatePrintLoop` also has a `step` that lets go of things; `benchmarks/one_list`'s `each_step` over 200 000
particles, in a program whose counts are atomic, went from 833-896 µs to 211-252 µs a pass.

### A number joined into text is written in place

**What it does.** `"line {index} of {round};"` used to turn `index` and `round` into texts of their own -- two
allocations each -- only to copy them into the result and free them. An `Integer` or `Long` piece of a text join,
or of an append in place (`text = "{text}{count}"`), is now written as digits into a buffer in the function's own
frame and copied from there: the same digits the library's `to_string()` writes, and no allocation. A program that
reopens `Integer` or `Long` with a `to_string()` of its own keeps calling it.

**When.** Every build, for whole numbers of those two classes. A number cast to text on its own (`var key: String =
index`, [D223](decisions.md)) still makes one text, since that text is the result.

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

### A dictionary keyed by numbers hashes the numbers

**What it does.** A `Dictionary` the program gives whole-number keys ([collections.md](collections.md#keyed-by-numbers),
D224) is compiled as its own form of `library/dictionary.spite`, whose bodies fold on the key's type as `Items`
folds on `fits_vector()`: its keys are a `List` of the numbers, a key is hashed by one multiply (Knuth's
6364136223846793005) instead of a loop over characters, and a slot's key is compared directly, without the 32 bits
of hash a text key keeps beside it. No `String` is made for a key, in a lookup or in the table. A dictionary given
text keys compiles to exactly the code it did before.

**When.** Every build, for each dictionary whose keys are whole numbers. Which dictionaries those are is worked out
while compiling: every place a dictionary is made or named (a `Dictionary<T>()`, an attribute, a parameter, a
return type) is a site, sites a dictionary flows between are joined wherever the compiler checks that one
dictionary type fits another, and a site's kind is that of the keys given to any site it is joined with. The key
kinds are known only once the whole program has been compiled, so a program that gives some dictionary a number
key is compiled a second time with them known (a third or more only when a dictionary's kind changes which
generic classes are made, at most eight); a program with only text keys is compiled once, the compiler itself
included.

**What you notice.** Speed and allocations: `benchmarks/number_keys` does a million lookups in 8 ms keyed by the
number, against 66 ms keyed by `index.to_string()` and 106 ms before D224, when a number key was quietly turned
into text; `conformance/stage6/number_keys` pins its 158 allocations, with a thousand number keys making none.
`keys()` answers the numbers, and a mixed dictionary is a compile error. Compiling a program with number keys
costs the second pass: `benchmarks/number_keys` compiles to C in about 300 ms against about 200 ms in one pass
(reading and parsing are not repeated). **Built** (2026-09-26; decided by Claude under D205, the readings proposed by Claude,
unconfirmed).

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

### A row of borrowed items lives in the frame

**What it does.** An object literal of borrowed `Vector` items (a row, [memory.md](memory.md#a-row-of-borrowed-items-for-one-call),
D206) is not allocated: it is a struct in the frame of the function that makes it, its header's count set once
and never touched, and its attributes the items' addresses, uncounted. The function it is passed to is compiled a
second time for that call, as `<name>___lent_<positions>`, in which reading an attribute of the row is the
`type`'s uncounted read (as in [Reading through a `type` without counting](#reading-through-a-type-without-counting))
and the parameter is neither retained by the caller nor released by the callee.

**When.** Every build, for every row; the rules that make it safe are compile errors, not conditions of the
optimisation.

**What you notice.** No allocation per row (`conformance/stage6/vector_rows` pins its count), and
`benchmarks/vector_rows`: 2.4 ms a tick over 200 000 entities with `Vector` columns and rows, against 6.0 ms with
`List` columns and a row object reused across the tick. A `__lent_` function appears in the C beside the ordinary
one, which is shaken out when no ordinary call reaches it. **Built** (2026-09-26; proposed by Claude, unconfirmed).

A row filled by a `Symbol` walk (D212) is the same struct: the compiler writes the walk out as the literal it
amounts to, in the caller, so the walk's template is not called and is not compiled for that walk, and
`--final-classes` shows no `fill_<attribute>` function for it. **Built** (2026-09-26; proposed by Claude,
unconfirmed).

A walked row over sparse columns (D217) is the same struct again. Its line is chosen for each attribute while
compiling (`attribute.class == Entity`, `attribute.class.fits_vector()`), `attribute.index` is written in as a
constant, and `Column<attribute.class>()` is the one singleton for that class, so nothing is looked up by name or
place at run time. An attribute made by a construction of a class that could be a `Vector` item and holds nothing
counted (`Entity(entity)`) is **made in the frame**: a struct beside the row, its defaults set and its
constructor run on it, never allocated and never counted, living exactly as long as the row. Any other counted
attribute, such as a reference read from a reference column, is counted once when the row is made and let go at
the end of the row's block. **What you notice.** No allocation per row for a frame-made attribute
(`conformance/stage6/sparse_rows` pins its count), and `benchmarks/sparse_rows`: 200 000 entities, two systems,
7.4 ms a tick against 24.0 ms with reference columns and a reused row object. A frame-made object is not
registered with `--debug-memory`'s table, like the row itself, and is not in `.instances`. **Built** (2026-09-26;
proposed by Claude, unconfirmed).

**A reference column's element is lent to the row** ([D269](decisions.md)). When that counted attribute is the
result of a function that only returns an element of its singleton's `List` -- a reference column's `at(row)`,
`return references.get_at(row)` or `crash references[row]` then `return references[row]`, with a whole-number
parameter as the index and a list attribute that nothing assigns after the singleton is made -- and nothing the
rest of the row's block runs can let go of anything, the row takes the element uncounted: the compiler writes a
copy of the function, `<name>___lent_element`, that returns the element as it lies in the list (it answers exactly
what the function answers, crash included, when the index is out of range), and neither retains it nor releases
it at the end of the block. The proof walks what the rest of the block runs: every call must be one the compiler
can name (a phase template's `system.phase_each(row)` is named as the phase's own `update_each`), and none of them,
nor anything they call, may let go of an object, remove from or replace into a list, or call through a function
value (the proof of [a list's templates](#a-lists-templates-read-its-elements-without-counting-them), stricter);
no class of the program whose objects can be let go while it runs may have a `drop()` that reaches the column. In
a program with threads the element is also kept from other threads' writes for the rest of the block: the block
takes the column's readers' side ([D266](decisions.md)) or its lock once, where a call would have taken it once
anyway, and only when what the block runs reaches no singleton at all and waits for nothing (the conditions of
[a counted loop](#a-counted-loop-of-calls-to-one-singleton-takes-its-lock-once)), so holding it cannot deadlock;
otherwise the row calls the ordinary function and counts the element, as before. Which of the three it is is
decided with the column's lock, when the program is finished: the C calls `spite_lend_<n>_read(...)` between
`spite_lend_<n>_enter()` and `spite_lend_<n>_leave(...)`, macros that are nothing, the readers' side or lock, or the
ordinary counted call and its release. **What you notice.** Speed, most where the system does not touch the
reference: `benchmarks/lent_elements`, 100 000 rows of an inline and a reference component run in a `Parallel`,
83 ns to 14 ns a row reading the reference and 35 ns to 9 ns not reading it (with the two optimisations below;
best of twenty ticks, `--optimized`). Nothing a program prints or allocates changes (`conformance/stage6/lent_list_elements`,
`conformance/stage6/lent_list_elements_parallel`, which `check.sh` also reads the C of: a system that removes from
the column, or reads the column itself, keeps its count). **Built** (2026-09-28; proposed by Claude, unconfirmed).

The arguments a plural value template fills (D220, `system.phase_each(made_arguments(found))`) are written out
the same way, in the caller: when the template's body, folded for an argument, is one `return` of a walked line
(`Column<argument.class>().values[rows[argument.index]]`) or a walked row declared, filled and returned, the
compiler writes that value as a local of a C block around the call, one per argument in order, and calls the
function's `___lent_<positions>` copy, in which a borrowed item's parameter is neither retained nor released. The
template is not called for that call, so it is not compiled for it; an argument whose body has any other shape
is its template's ordinary call, as before, and a call none of whose arguments borrows is left exactly as it was
(SlopEngine's `examples/stress` compiles to the same C). **What you notice.** No copy, no allocation and no count
per argument that borrows (`conformance/stage6/lent_arguments` pins its count), and `benchmarks/lent_arguments`:
200 000 entities in sparse sets over `Items` columns, systems of one and two component arguments, 6.9 ms a tick
against 34.3 ms when each argument is copied out of its column, passed and stored back. **Built** (2026-09-26;
proposed by Claude, unconfirmed).

Any borrowed item passed as an ordinary argument (D257, [memory.md](memory.md#an-item-lent-to-a-call)) takes the
same `___lent_<positions>` copy: `apply(event, mouse, keyboard)` with `mouse` and `keyboard` read from a row calls
`apply___lent_1_2`, which receives the items' addresses and neither retains nor releases them, and a lent
parameter passed on (`press(mouse)`) calls `press___lent_0` in turn. One copy is written per function and set of
lent positions, and only for those a program reaches, so a program that lends nothing carries none. **What you
notice.** No copy and no count per lent argument (`conformance/stage6/lent_to_calls` balances with the writes
read back from the vectors); the `___lent_` functions appear in the C, and the ordinary function is shaken out
when no caller passes it a counted object. **Built** (2026-09-27; proposed by Claude, unconfirmed).

### An `Items`' storage is chosen while compiling

**What it does.** `Items<T>` ([collections.md](collections.md#itemst-the-storage-chosen-for-you), D218) is one
class in `library/items.spite` whose every body that touches an item folds on `$element_type.fits_vector()`.
For a `T` that fits, only the inline branches are compiled (the item functions of `InlineMemory<T>`, borrowed
reads, a `Vector`'s layout); for any other, only the reference branches (`TypedMemory<T>`, counted references,
a `List`'s layout). A chain of its templates is fused into one loop and `parallel_each_` is split across the
pool as for a `Vector`, and the reference kind's templates read their elements uncounted as a `List`'s do
([above](#a-lists-templates-read-its-elements-without-counting-them)). `[]` checks its range with one comparison
and keeps the crash report in a separate function, `_out_of_range`, so that the read itself is small enough for
the C compiler to inline where it is called.

**When.** Every build, for every `Items<T>`; the choice is a fact of `T`, so it cannot change while the program
runs, and a program that makes no `Items` carries none of it.

**What you notice.** Speed the same as the storage chosen, or better: `benchmarks/items_storage` (200 000 items,
`clang -O2`, three rounds) runs the member templates of `Items<Velocity>` in 193-212 µs a tick against 201-218 µs
for `Vector<Velocity>`, and of `Items<Trail>` in 515-668 µs against 534-645 µs for `List<Trail>` over the same
objects; 200 000 random `[]` reads and writes take 510-533 µs against a `Vector`'s 573-601 µs, and 818-948 µs
against `List.get_at`'s 911-1518 µs, the difference being the inlined read. A walked-row runner over
`Items` columns (`benchmarks/sparse_rows` with `Column`'s `Vector` swapped for an `Items`) ran 6.9 ms a tick
against 7.2 ms. An `Items` object holds one pointer more than a `Vector` or `List` (both helper singletons are
attributes); nothing is added per item. A crash out of range is reported from `Items._out_of_range`.
**Built** (2026-09-26; proposed by Claude, unconfirmed).

### A proven divisor is not checked

**Built.** A whole-number `/` or `%` checks its divisor for zero (D201, [values_and_types.md](values_and_types.md)),
unless the divisor is a constant other than zero, or a proof in scope says it is not zero: `assert parts != 0`,
`crash parts != 0`, `if parts != 0 { }` or `parts > 0` in a condition, the same proofs D169 keeps across a call
that cannot change `parts` and drops across one that may. The check, where it stays, is one compare and a
branch the CPU predicts. What you can observe: nothing but speed; `check.sh` holds that
`conformance/stage6/division_by_zero`'s proven `whole / pieces` carries no check in its C.

### Signed arithmetic is checked only while developing

**Built.** In a `--debug-memory` or an inspectable build, every `+`, `-` and `*` done in `Tiny`, `Short`,
`Integer` or `Long` is the C compiler's overflow builtin in that type, and an answer that does not fit halts
([values_and_types.md](values_and_types.md#numeric-types--implemented-provisional)). A production build -- the
ordinary one and `--optimized` -- emits the plain operator, so its C is the same as before the check existed and
the answer wraps. The unsigned whole numbers are never checked. What you can observe: in a development build, a
halt instead of a wrapped answer; in a production build, nothing. **Cost, measured** (best of seven runs, each
benchmark built with `--development` by the compiler before and after the check, C at `-O2`, on a machine other
sessions were loading): `plain_loops` 201 -> 216 ms, `fused_chain` 240 -> 345 ms, `game_maths` 180 -> 189 ms,
`dictionary_keys` 177 -> 179 ms. The compiler built with `--debug-memory` compiling itself carries 1 506 checked
operations and took 5.2 s before and 4.7 s after, best of eight: inside the noise. **Planned:** leave out the check
where a proof already bounds the operands, as a proven divisor leaves out its zero check.

### Short text lives inside the `String`

**What it does.** A `String` is sixteen bytes wherever it is kept -- a local, an attribute, a list's element, a
parameter -- and text of up to 15 bytes of UTF-8 is kept in those sixteen bytes themselves: no allocation, no
reference count, and no pointer to follow to read it, so a name in a component column is read where the column
already is in the CPU cache ([D203](decisions.md)). Longer text is one block on the heap -- its count, its capacity
and its characters, with a 0 after them for C -- that the sixteen bytes point at, next to the length; it used to be
two, the `String` object and its characters. A written text (`"hello"`) is part of the program as before, whatever
its length: the sixteen bytes point at it, and nothing is counted or freed.

**When.** Every `String`, in every build. Whatever makes text -- a join, `slice`, `upper_case()`, a number's
`to_string()`, reading a file, the program's arguments, a foreign function's result -- keeps it inside the value when
it fits. [Appending in place](#appending-to-text-in-place) fills the sixteen bytes first and moves the text into a
block, with room to grow, once it passes 15 bytes. Reading a character (`code_at`) looks at the form where the
`String` is kept rather than in a copy, so a loop over the characters of one text -- a dictionary hashing its key,
`index_of`, `trim` -- decides the form once and then reads one byte per character, as it did before.

**What you notice.** Fewer allocations under `--debug-memory`: none for short text, one instead of two for long
text (`conformance/stage6/text_building` went from 35 to 31, `singleton_counts` from 200 104 to 200 069 and
`fused_chain_allocations` from 15 to 13, most of it numbers turned into text to be printed). `.memory.section` of
text made while the program runs answers `'stack'` when the text is short and held in a local (`'heap'` when it is
read from an attribute, where the value lives in its object) and `'heap'` when it is long; written text is
`'constant'`, as before. Text passed where a `type` shape is wanted -- a `Printable` given to `console.print`,
`attribute.value` -- is put in a box, one allocation, as a number is (written text has a box in the program and
allocates nothing) ([below](#boxing-only-where-a-value-travels-as-a-shape)). A `List<String>` holds sixteen bytes
per element instead of an eight-byte pointer. Speed: `benchmarks/dictionary_keys` allocates 1 032 times instead of
1 104 014, `text_building` 165 instead of 800 227 and `reflection_walks` 3 999 918 instead of 7 596 024, and each
is 15-25% faster; SlopEngine's `stress` allocates 5.6 million times instead of 7.2. The cost that remains: a
`Dictionary` looked up by a key longer than 15 bytes is about 10% slower, since the key travels as sixteen bytes
and is compared through its form ([benchmarks/README.md](../benchmarks/README.md)). Why 15 and not 22: of the 4.7
million texts the compiler makes compiling itself, 68% are 15 bytes or fewer and 78% are 22 or fewer, and 22 would
take a third machine word in every `String` -- a `List<String>` half as large again -- where 15 fits in the two a
long text needs anyway (where its characters are, and how many). **Built** (2026-09-26; the size and the layout
proposed by Claude, unconfirmed).

### Maths on constants is worked out while compiling

**What it does.** A maths function of a number class ([standard_library.md](standard_library.md#maths--implemented))
whose operands are all constants is worked out by the compiler, and the C gets the answer: `(0.5).sine()` is
`(0x1.eaee880000000p-2f)` in the C, not a call. A constant here is a decimal or whole literal, a negated one, a
number class's constant (`Float.pi()`), or another folded call, so `Float.pi().sine()` and
`(2.0).square_root().square_root()` fold too. The answer is written as a hexadecimal float, which the C compiler
reads back to exactly those bits, and infinity and not-a-number as `__builtin_inf()` and `__builtin_nan("0x...")`
with the same sign and payload.

**How the answer is the C library's.** The compiler works it out by calling the very function it would have
written: it runs the same member on its own `Float` or `Double` (`bootstrap/source/generation/maths_primitives.spite`),
and that member is lowered, in the compiler as in any program, to the C library's `sinf`, `sqrt`, and so on. The
operands are rounded as the C would round them first -- a literal to a `Float` for a `Float` receiver, a `Float`
constant widened exactly for a `Double` one -- so the folded bits are the ones the program would have computed at
run time, `nan` sign and all. `conformance/stage6/maths_folding` holds every function, folded against the same call
on a value the compiler cannot see, to be the same bits.

**When.** Every build, when the receiver and every argument are constants as above. A variable is not a constant
here, even one never assigned again: `var angle = 0.5` then `angle.sine()` is a call (which the C compiler may
still fold itself). The whole-number `absolute`, `minimum`, `maximum` and `clamp` are left to the C compiler,
whose integer arithmetic has only one answer.

**What you notice.** Nothing but speed, and no `#include <math.h>` in a program whose only maths is folded. One
thing to know: the answer is the C library of the machine that compiles. A program compiled on one system and run
on another whose C library rounds a last bit differently gets the compiling system's answer for a folded call and
its own for the rest; `sqrt`, `floor`, `ceil`, `round`, `trunc`, `fabs`, `fmin` and `fmax` are exact everywhere, so
only the transcendental functions can differ, by at most that last bit. **Built.**

### A binary schema is a constant

**What it does.** `BinaryWriter<T>.schema()` and `BinaryReader<T>.schema()` ([json.md](json.md#the-schema-hash),
[D215](decisions.md)) are worked out while compiling: the compiler writes the attribute walk of `T` as text, hashes
it with FNV-1a, and the C gets a macro that is the number, with the text beside it in a comment. **When.** Every
build, for each `T` a writer or reader is made for and whose `schema()` is called; nothing is emitted otherwise.
**What you notice.** Nothing: no walk runs and nothing is allocated when a program asks. **Built.**

### A number's bits are read in place

**What it does.** `Float.bits()`, `Double.bits()`, `UnsignedInteger.bits_as_float()`, `Long.bits_as_double()` and
`UnsignedLong.bits_as_double()` are C macros over a union of the two types
([D215](decisions.md), [values_and_types.md](values_and_types.md#rules-in-full)): the call is written where it is made
and the value's bits are read as the other type, with no memory written and read back and nothing allocated. Before,
each went through a 4- or 8-byte block that the frame slot kept off the heap, so they allocated nothing then either,
but the C held a block, a write and a read for the C compiler to see through.

**When.** Every build, for every call; a program that calls none carries none of them. **What you notice.** Nothing
but speed in an unoptimised build: `benchmarks/half_precision` (ten million `to_half_precision` and back) takes
416 ms against 482 ms with `clang -O0`, and 13 ms either way from `-O1`, where clang already saw through the block;
it allocates nothing per conversion before and after. **Built.**

### A local list of known size lives in the frame

**What it does.** A local list made by a literal (`var sizes = [3, 5, 8]`) or by `List<T>()` whose size is known
while compiling and which never leaves its function is not allocated: its header and its items are in the
function's own frame, the way [a variadic list](#a-variadic-list-the-callee-only-reads-lives-in-the-callers-frame)
already is. Known size means the literal's items plus the `append`s written as statements of their own in the
same block after it -- not inside a loop, a branch or another statement -- since each of those runs at most once;
the items get exactly that many slots. Never leaving means every later statement of the block only reads it:
`count()`, `is_empty()`, `[index]`, `get_at`, `find_at`, `first`, `last`, `contains`, `join`, or passing it to a
function of its own class that only reads it too. A list that is returned, stored, assigned, put into another list
or object, changed with `set_at`, `insert`, a `remove_...` or `clear`, handed to a template, or whose `.memory` is
read is made on the heap as before. At the end of the block its items are let go, and nothing else.

A literal of constants -- numbers, `Boolean`, text in quotes -- that nothing appends to goes further: its items are
part of the program, in read-only constant data, written once by the C compiler and never while the program runs,
and only the header (a count and the item address) is in the frame.

**Example.** No allocation is made while the two lists are used:

```gdscript title=frame_lists/frame_lists.spite entry
var console = Console()
var heap = Memory.Heap()

func FrameLists() {
    var before = heap.live_allocations()
    var sizes = [3, 5, 8]
    var names = List<String>()
    names.append("ann")
    names.append("bob")
    var biggest = sizes.get_at(2)
    var joined = names.join(" and ")
    var during = heap.live_allocations()
    console.print(joined, biggest, "allocations while listing:", during - before)
}
```
```output
ann and bob 8 allocations while listing: 0
```

**When.** Every build but the inspectable ones (`--repl`, `--repl-port`, `--hot-reload`, `--development`), where a
list stays an ordinary object, and not in a function that waits. At most 16 items in the frame (256 bytes of text)
and 256 in constant data; a bigger list is made on the heap as before.

**What you notice.** Two allocations fewer per such list under `--debug-memory` (more for a literal of more than
four items, which grew its buffer while it was filled): `conformance/stage6/text_building` allocates 17 times
(19 before), `plain_items` 100 (102). The compiler has 115 such lists, 52 of them constant.
**Built** (2026-09-26, the first part of [D222](decisions.md); proposed by Claude, unconfirmed).

### A loop over plain values reads its count once and its items unchecked

**What it does.** A `while index < values.count()` over a `List` or `Vector` of numbers or `Boolean` is written as
the plain C loop a C compiler can turn into vector instructions (SIMD), when the compiler can prove three things:

- **The counter stays in range.** `index` is a local whose every assignment in the function is a whole-number
  literal of 0 or more, or `index = index + 1` as the last statement of a loop bounded by `index < ....count()` or a
  literal, with no other write to it in that loop. So it is never negative, never wraps, and inside the loop it is
  below `values.count()`.
- **Nothing in the loop changes a list's size.** The body only declares and assigns numbers and `Boolean`s (its own
  locals, not attributes), reads and writes the items of lists of plain values (`[index]`, `get_at`, `set_at`,
  `find_at`, `first`, `last`, `contains`, `count`, `is_empty`), and calls the maths functions of the number
  classes. Any other call, even one to a function that looks harmless, keeps the loop as it was: a call is where a
  list could be resized through another name.
- **The loop cannot be interrupted.** A `--repl`, `--repl-port` or `--hot-reload` build may run code between two
  passes, so there the loop stays as it was, as it does in a function that waits.

Then `values.count()` is read once before the loop, the address of its items once, and `values[index]`,
`values.get_at(index)`, `values[index] = x` and `values.set_at(index, x)` read and write the item directly, with no
range check: the loop's own condition is the proof. A second list indexed by the same counter
(`into[index] = from[index] * 1.5`) is checked once instead, before the loop: when it holds at least as many items
as the loop runs, the loop runs without its checks; otherwise the loop as it was runs, with every check, so what an
out-of-range write does (nothing on a `List`, a halt on a `Vector`) is unchanged. The loop is written twice for that,
so a body with an `assert` or `crash` is not (its crash report would be written twice).

**What the C compiler then does.** An element-by-element loop (a map in place, into another list, a filter's test)
and a whole-number sum are vectorised, which clang's `-Rpass=loop-vectorize` confirms. A `Float` or `Double`
**sum** is not, and must not be: adding in a different order changes the last bits of the answer, and an
optimisation may not change what a program computes ([D36](decisions.md)). The compiler adds no `restrict`, since
two names may hold the same list and the C compiler checks for overlap once, before the loop, itself; and no
alignment claim, which nothing proves.

**What you notice.** Speed only. `benchmarks/plain_loops` (a million items, `clang -O2`, µs a pass): `into[index] =
from.get_at(index) * 1.5 + 0.25` over two `List<Float>` 668-697 before, 181-207 after; the same in place over a
`Vector<Float>` 2 628-2 782 before, 178-195 after; a `Float` sum 640-673 either way (it stays in order); an
`Integer` sum 112-137 either way (it was already vectorised). A `Float` expression with a decimal literal is
still worked out in `double` precision in the C, as it always was, which halves the vector width: in `float` the
two loops would take about 100 and 70 µs, but some results would change in their last bits, so that is a question
for Mortaro and is not done (`mortaros_missing_decisions.md`, item 210). **Built** (2026-09-26, the second part of
[D222](decisions.md); proposed by Claude, unconfirmed).

### Objects that never leave their function live in the frame

**What it does.** Every class is passed by reference ([D149](decisions.md)), so `var moved = position +
velocity.scaled(delta)` reads as two new objects. When the compiler can prove an object never outlives the call
that made it, it is not made on the heap at all: it gets a slot in the function's own frame, the way a buffer
([D108](decisions.md), [D211](decisions.md)) and a list ([D222](decisions.md)) already do. Four places use it:

- **A local.** `var name = <a fresh object>` gets a frame slot when nothing after it in its block lets the object
  go: it is only read and written through its attributes, handed as the receiver or as an argument to functions
  proven to keep nothing, compared, or asked for its `.memory`. It may be given a new fresh object
  (`position = position + moved`), which is worked out in a second slot and copied into the first; and when the
  function returns its class, `return name` copies it to the heap once, at the return, instead of once per step.
  Anything else lets it go and keeps it on the heap as before: storing it in an attribute, a list or a dictionary,
  returning it from a function that answers some other type, naming it in another variable (`var other = name`),
  passing it to a function that keeps it, to a `Parallel` or a `Concurrent` (their constructors keep it), as a
  function value (`name.update`, which holds the object), to a variadic list (`console.print(name)`), or naming it
  in a text's hole other than as `{name.attribute}`.
- **A result, into the caller's slot.** A function whose every `return` gives a fresh object -- `return
  Vector3(...)`, a local that lives in the frame, or another such call -- gets a second, hidden version that writes
  its answer into a slot its caller passes, a calling convention chosen per call site ([D36](decisions.md): both
  versions may exist, and neither is visible). So `var moved = velocity.scaled(delta)`, whose `moved` stays in the
  frame, calls the hidden version with `moved`'s slot, and nothing is allocated. `Matrix4.multiply`, which builds
  its product in a local and returns it, becomes the same.
- **A temporary.** In `a + b + c`, `(first + second).length_squared()` or `transform.transform_point(point)` passed
  to a function that keeps nothing, each intermediate answer is written into a frame slot of its own.
- **A copy used as a value.** This is D149's answer to value classes, which Spite does not have: "instead we can
  copy() a instance because thats user intention, but if a copy is only used as a value, we internally compile it
  as a value, the compiler is smart, the users arent". `var local = other.copy()` (or `other.deep_copy()`, the
  same thing for a class of numbers) whose `local` never leaves the function is a frame slot filled by copying the
  attributes -- a `memcpy` of the object's numbers after the C compiler is done -- with no heap allocation and no
  count kept anywhere. Changing `local` never changes `other`, as with any copy.
  `conformance/stage6/frame_objects` pins it: two such copies change `heap.live_allocations()` by 0 (by 2 before).

**Which objects.** An instance of a class whose attributes are all numbers, `Boolean`s, enum values or singletons
([D144](decisions.md) binds a singleton as an attribute, and it is never counted) -- `Vector3`, `Matrix4`,
`Quaternion`, a program's own `Velocity`, SlopEngine's `Math.Matrix4` -- with no `drop()`, that is not a singleton and whose
constructor keeps nothing, and whose class is not read with `.instances` anywhere in the program. What "keeps
nothing" means is proven from the source of each function, parameter by parameter and for the object it is called
on: a parameter kept nowhere in the body -- not stored, returned, captured, named in another variable, or passed on
to a function that keeps it -- is lent. A function without a body the compiler reads (a foreign or built-in one) and
a recursive call are assumed to keep.

**How it stays safe.** A frame object starts with a reference count of 2^30 that its frame never lets go, so the
count's ordinary ups and downs around calls never free it, and it is never on the heap to be freed. Nothing is kept
anywhere, so no reference outlives the frame, and none reaches another thread: a program cannot tell where it lives
except by asking.

**When.** Every build but the inspectable ones (`--repl`, `--repl-port`, `--hot-reload`, `--development`), where
every object stays an ordinary heap object that reflection and reloading can see, and not in a function that
waits. Objects of classes holding text, lists or other objects are not placed yet: their attributes would have to
be let go at the end of the frame, which is planned (below).

**What you notice.** Fewer allocations under `--debug-memory`, and `value.memory.section` answering `'stack'` for a
local that lives in the frame ([memory.md](memory.md#where-a-value-lives-memory)). `benchmarks/game_maths` (a
million `position + velocity.scaled(delta)` steps, 200 000 `Matrix4` products, a million `transform_point`s): 3 200
046 allocations before, 37 after, and the hand-written C with the same structs in `benchmarks/game_maths/game_maths.c`
is the measure of speed -- see [the benchmarks](../benchmarks/README.md#game-maths-d213); `small_allocations` makes
3 004 007 (9 004 007), and every other benchmark the same as before. On a copy of SlopEngine,
whose `Math.Matrix4` holds its two singletons as attributes: `flex_layout` makes 99 789 allocations (100 514
before), `scene_probe` 32 413 (32 518), `render_parity` 14 142 (14 171), with the same output; `stress` keeps
its components in columns and makes the same 5 606 191. The pins that moved in `conformance/`: `lent_arguments`
allocates 122 times (130), because the `Entity` a generic runner makes for each entity it hands to a system that
keeps nothing is now in the frame; `singleton_counts` 65 times (200 065), because the `TallyHolder` each of its
200 000 passes makes holds only a singleton and is only read, so it is in the frame; `frame_objects` pins the
rest (2 098 allocations before, 86 after). In a program that starts
threads, passing a frame object to a function still counts it up and down atomically, as it does any object; the
count is never read. **Built** (2026-09-26, extending D108/D211 placement to objects under [D149](decisions.md);
proposed by Claude, unconfirmed, decided under D205/D214).

### The C is compiled in parallel units, and cached

(Proposed by Claude, unconfirmed.) In an `--optimized` build, the C of a program bigger than 1.5 MB is split into
a header and up to 64 translation units, compiled as many at once as the machine has processors and linked, and each unit's object is
kept under the hash of what it was compiled from, so a build that changed nothing only links and a build that
changed one function's body compiles one unit ([compiler.md](compiler.md#translation-units-the-c-compiled-in-parallel-and-cached),
where the rules are). It changes nothing a program does: the same functions and variables, with `static` dropped so
another unit can call them. What you could notice: in a build without link-time optimisation a call from one unit
into another is not inlined by the C compiler -- which the default `-O0` build never does anyway, and which
`--optimized` recovers ([below](#a-release-build-is--o3-with-link-time-optimisation)); and the object cache in
`.spite-cache/objects` grows until it is deleted. `--translation-units=1` builds from one file as before.

### A release build is `-O3` with link-time optimisation

(Proposed by Claude, unconfirmed.) `--optimized` asks the C compiler for `-O3`, and a build from several units adds
ThinLTO (`-flto=thin`, clang) or `-flto=auto` (gcc) so functions are still inlined across units. What you could
notice: the link takes longer, since it is where the optimisation across units happens. The default build is `-O0`,
Mortaro's choice to keep; `--tune-for-this-machine` adds `-march=native`, which makes the executable specific to
processors like the one that built it ([compiler.md](compiler.md#release-builds)). The measurements are in
[benchmarks/README.md](../benchmarks/README.md#release-builds).

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
construction is [above](#an-allocator-set-after-construction-is-where-the-object-is-made), and so is the first
part of objects that never escape: [objects of numbers in the frame](#objects-that-never-leave-their-function-live-in-the-frame),
including a `copy()` of one.) Not built yet: frame objects of classes that hold text, lists or other objects
(their attributes let go at the end of the frame), an attribute object laid inline in a frame-held object where
the attribute is never shared, and leaving out the count on a frame object passed to a function, which needs
callees that borrow their parameters rather than taking a count.

### Other planned optimisations

- **A list's buffer in its list's allocator** ([D154](decisions.md)): a `List` given an allocator is made there,
  but its buffer of references still comes from the heap, and so does a `Vector<T>`'s block of items
  ([memory.md](memory.md#allocators-memoryallocator--implemented-for-objects-a-lists-buffer-and-a-vectors-block-planned)).
- **An appended item made in place**: `var slow = Velocity(1.0, 0.5)` and then `velocities.append(slow)` makes an
  ordinary object, copies its attributes into the vector's block and lets the object go, so filling a vector
  allocates once per item for a moment ([collections.md](collections.md#vectort--implemented)). Writing the
  constructor's attributes straight into the block, when the object is used for nothing else, would make filling
  it allocate only when the block grows.
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
