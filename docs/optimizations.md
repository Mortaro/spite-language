# Optimisations the compiler makes on its own

The compiler makes a program faster and smaller without being asked. This page lists every such optimisation: what it
does, when it applies, and what, if anything, you could ever notice. For almost all of them the honest answer is:
**you do not need to do anything**. Write the plain program; the compiler does the rest, and the program means exactly
what its source says.

Why the compiler does this instead of you is on [Write it plainly](write_it_plainly.md). Here is what it does.

**Every optimisation stands on a proof.** The compiler only changes how a program runs where it has proven, while
compiling, that the change cannot alter what the program prints, computes or crashes on. Each section below names
that proof, and [Proofs](proofs.md) states each proof in full: its rule, when it does not hold, and what the
compiler generates then. Where the proof fails, the plain form is generated, which is slower but never wrong.

Two rules decide what belongs here.

**Zero runtime, and everything tree-shakeable.** A program that does not use a feature carries none of it.
Nothing needs a scheduler, an interpreter or a registry shipped beside the program: the work is done at compile
time instead. REPL, live reload and debugging features may cost something while the program runs, but only in
the builds that ask for them. Those builds are slower on purpose, so measure performance only with a production
build ([Measure only a production build](compiler.md#measure-only-a-production-build)).

**Hidden optimisations are good, hidden costs are bad.** Code that runs faster than you expect is a free win, so
the compiler optimises silently and never asks you to mark anything. Code that runs *slower* than you expect is
the only real surprise, so every cost that remains is written down on this page, under the optimisation it
belongs to, and `--optimization-report` lists every place in your program where one of them fell back
([compiler.md](compiler.md#read-what-was-not-optimised)).

**Internals stay ordinary objects where you inspect them.** A `--repl`, `--repl-port`, `--hot-reload` or
`--development` build is an *inspectable* build: nothing is tree-shaken, and the standard library's internals
(`Memory`, `Build`, `TypedMemory<T>`) are ordinary objects that reflection (`.instances`, `.attributes`) sees. Every
other build, ordinary or `--optimized`, is a production build, and only there are the two optimisations that hide
something applied: tree shaking and static singletons. Everything else on this page changes how fast the program runs,
not what it is made of, so it applies in every build. The **Builds** column below says which.

What you can observe at all is short: the counts `--debug-memory` prints, where `.memory` says a value lives, when a
singleton's constructor runs, the order in which a fused chain calls your member functions, the point inside a
statement where a `Concurrent` waits, and speed. Apart from those orders, which only code with a visible effect can
show, no optimisation changes what a program prints or computes.

The checks the compiler makes are not on this page: a wider right operand, a proof a call may have undone, an unread
name. They are rules of the language, on the pages that teach them, and they cost nothing at run time because they
emit nothing.

| Optimisation | Builds | What you might notice |
|---|---|---|
| [Tree shaking the generated C](#tree-shaking-the-generated-c) | production | smaller C; fewer symbols looked up; an inspectable build keeps everything |
| [Deciding conditions at compile time](#deciding-conditions-at-compile-time) | every | nothing: the branch not taken is not in the program |
| [Reflection, symbols and registries only where read](#reflection-symbols-and-registries-only-where-read) | every | nothing |
| [Template chains run as one loop](#template-chains-run-as-one-loop) | every | fewer allocations; member functions run element by element |
| [Appending to text in place](#appending-to-text-in-place) | every | fewer allocations |
| [The compiler places memory](#the-compiler-places-memory) | every | fewer allocations; `.memory.section` |
| [Reading an address is one machine operation](#reading-an-address-is-one-machine-operation) | every | nothing |
| [An allocator set after construction is where the object is made](#an-allocator-set-after-construction-is-where-the-object-is-made) | every | the arena's blocks are the allocations; sixteen bytes more per object of a class given an allocator |
| [Singletons: made on first use, never counted](#singletons-made-on-first-use-never-counted) | every | the constructor runs at first use |
| [Singletons that hold nothing are static objects](#singletons-that-hold-nothing-are-static-objects) | production | one allocation fewer each; not in `.instances` |
| [Atomic reference counts only with threads](#atomic-reference-counts-only-with-threads) | every, decided per program | nothing |
| [Plain reference counts where no thread reaches a class](#plain-reference-counts-where-no-thread-reaches-a-class) | production, decided per class | nothing but speed |
| [A function taking a `type` is compiled per class](#a-function-taking-a-type-is-compiled-per-class) | every but `--repl`, `--repl-port`, `--hot-reload` | no box for a value passed to it; direct calls; a copy per class |
| [Boxing only where a value travels as a shape](#boxing-only-where-a-value-travels-as-a-shape) | every | numbers, `Boolean` and enum values are never boxed; one allocation per boxed text |
| [Concurrency machinery only where it is used](#concurrency-machinery-only-where-it-is-used) | every, decided per program | nothing |
| [Hidden async/await as compile-time state machines](#hidden-asyncawait-as-compile-time-state-machines) | every, in programs that make a `Concurrent` | one heap frame per waiting call; a temporary for what an expression computes before its wait |
| [Reads in a row overlap](#reads-in-a-row-overlap) | every | the reads happen at once; the program carries the scheduler |
| [A wait in a frame does not hold the frame](#a-wait-in-a-frame-does-not-hold-the-frame) | every | the work finishes in a later frame; the program carries the scheduler |
| [REPL, live reload and debug machinery only in those builds](#repl-live-reload-and-debug-machinery-only-in-those-builds) | the builds that ask for it | nothing in an ordinary build |
| [The thread pool only where a `Parallel` is made](#the-thread-pool-only-where-a-parallel-is-made) | every, decided per program | nothing until the first `Parallel` |
| [Singletons a `Parallel` reaches take a lock](#singletons-a-parallel-reaches-take-a-lock) | every but `--hot-reload`, decided per program | an uncontended lock per call, only with `Parallel` |
| [Thread safety for singletons, the cheapest safe form](#thread-safety-for-singletons-the-cheapest-safe-form) | every but `--hot-reload`, decided per program | no lock where one is not needed |
| [A counted loop of calls to one singleton takes its lock once](#a-counted-loop-of-calls-to-one-singleton-takes-its-lock-once) | every but `--hot-reload`, `--repl`, `--repl-port`, decided per loop | one lock for the loop instead of one per call, only with `Parallel` |
| [A singleton's reading functions do not exclude each other](#a-singletons-reading-functions-do-not-exclude-each-other) | every but `--hot-reload`, decided per singleton | readers count on their own cache line; 2 KiB of counts per such singleton |
| [While no task runs, a singleton's lock is skipped](#while-no-task-runs-a-singletons-lock-is-skipped) | every but `--hot-reload`, only with `Parallel` | one load per locked call, two atomic additions per task |
| [The fault handler is in every program](#the-fault-handler-is-in-every-program) | every | a cost, not an optimisation: about 3.7 KB of code and 32 bytes and a name per function; one store per foreign call |
| [A foreign name is never copied](#a-foreign-name-is-never-copied) | every | fewer allocations when a foreign library is opened |
| [Smaller ones](#smaller-ones) | every | nothing |
| [Proofs that survive a call](#proofs-that-survive-a-call) | every | a proof after a call that may change it is written again |
| [Text joined in one piece](#text-joined-in-one-piece) | every | fewer allocations; a text made only of constants is constant |
| [Defaults the constructor replaces are never made](#defaults-the-constructor-replaces-are-never-made) | every but `--hot-reload` | fewer allocations |
| [A function value describes its arguments when asked](#a-function-value-describes-its-arguments-when-asked) | every | fewer allocations per function value and per `Parallel` |
| [A list's templates read its elements without counting them](#a-lists-templates-read-its-elements-without-counting-them) | every but `--hot-reload` | nothing but speed |
| [A number joined into text is written in place](#a-number-joined-into-text-is-written-in-place) | every | fewer allocations |
| [Allocation is the C library's, counted only where read](#allocation-is-the-c-librarys-counted-only-where-read) | every but `--debug-memory`, decided per program | nothing: `live_allocations()` still answers |
| [A dictionary hashes a key once, cheaply](#a-dictionary-hashes-a-key-once-cheaply) | every | nothing but speed |
| [A dictionary keyed by numbers hashes the numbers](#a-dictionary-keyed-by-numbers-hashes-the-numbers) | every | fewer allocations |
| [Reading through a `type` without counting](#reading-through-a-type-without-counting) | every but `--hot-reload` | nothing but speed |
| [Arithmetic a range proves is not checked](#arithmetic-a-range-proves-is-not-checked) | every but `--repl`, `--repl-port` and `--hot-reload` | nothing but speed; `--optimization-report` lists every check kept |
| [Short text lives inside the `String`](#short-text-lives-inside-the-string) | every | fewer allocations; `.memory.section` of built text; a box when text travels as a shape |
| [Maths on constants is worked out while compiling](#maths-on-constants-is-worked-out-while-compiling) | every | nothing but speed; a folded call is the compiling machine's C library's answer |
| [A local list of known size lives in the frame](#a-local-list-of-known-size-lives-in-the-frame) | every but the inspectable ones | fewer allocations |
| [A loop over plain values reads its count once and its items unchecked](#a-loop-over-plain-values-reads-its-count-once-and-its-items-unchecked) | every but `--repl`, `--repl-port` and `--hot-reload` | speed; the last bits of a decimal sum or multiply-add in such a loop |
| [A decimal literal beside a `Float` is a `Float`](#a-decimal-literal-beside-a-float-is-a-float) | every | speed; the last bits of a `Float` result |
| [A number read from bytes is one load](#a-number-read-from-bytes-is-one-load) | every | nothing but speed |
| [A proven read tests only its bounds](#a-proven-read-tests-only-its-bounds) | every | nothing but speed; a read outside its list halts |
| [A walked `crash` line's read is the row's read](#a-walked-crash-lines-read-is-the-rows-read) | every | nothing but speed |
| [Objects that never leave their function live in the frame](#objects-that-never-leave-their-function-live-in-the-frame) | every but the inspectable ones | fewer allocations; `.memory.section` answers `'stack'` |
| [The C is compiled in parallel units, and cached](#the-c-is-compiled-in-parallel-units-and-cached) | every but `--hot-reload` | nothing but build time; `.spite/objects` grows to 1 GiB |
| [A release build is `-O3` with link-time optimisation](#a-release-build-is--o3-with-link-time-optimisation) | `--optimized` | nothing but speed, and a slower link |
| [A release is inlined in every unit](#a-release-is-inlined-in-every-unit) | every but `--hot-reload`, `--repl`, `--repl-port` | nothing but speed |
| [Thread safety for singletons, the rest of the plan](#thread-safety-for-singletons-the-rest-of-the-plan) | every but `--hot-reload`, decided per program | no lock where one is not needed |
| [Copies that cost nothing](#copies-that-cost-nothing) | every | fewer allocations |
| [Other optimisations](#other-optimisations) | every | fewer allocations |

## The optimisations

### Tree shaking the generated C

**The case:** [benchmarks/tree_shaking_the_generated_c](../benchmarks/tree_shaking_the_generated_c/).

**What it does.** After the program is generated, the compiler keeps only the C that `main` can reach: every
function nothing calls, from your classes, `library/` or the compiler's own prelude, is dropped along with its
prototype (`bootstrap/source/generation/tree_shaker.spite`). So is every class nothing reachable uses: its
`struct` and the `typedef` that names it, its `___allocate`, `___init`, `___default`, `___retain`, `___release`
and `_copy`, its singleton slot and that slot's lock, its reflection class object and the lines in `main` that
would free that object at exit, every text literal, static table and prototype only dropped code named, and the
`T?` of a number (`Nullable_Short`) that only dropped code answered. A
program that never makes a `FileSystemWatcher`, `Socket`, `Process`, `HotReload`, `ThreadPool` or `Scheduler` has none of
their C. The compiler does this itself rather than leaving dead code for the C compiler to find, so it holds
whichever C compiler you bring, and a C compiler cannot find most of it anyway, since a function it is not told
is private has to stay in the executable.

Measured on the generated C, before and after classes were shaken too (the executable is `clang -O2` on Windows):

| Program | C lines | C bytes | `struct`s | Executable |
|---|---|---|---|---|
| `examples/hello` | 5 130 → 1 332 | 233 588 → 62 732 | 75 → 10 | 192 000 → 158 208 |
| `examples/dungeon` | 5 958 → 2 348 | 268 925 → 101 165 | 80 → 21 | 207 360 → 173 568 |
| `conformance/stage3/interpolation` | 5 222 → 1 497 | 241 343 → 72 403 | 76 → 13 | 195 072 → 162 304 |
| `conformance/stage6/parallel_each` | 5 990 → 2 972 | 277 303 → 133 127 | 78 → 34 | 211 968 → 184 320 |
| `conformance/stage6/singleton_guard` | 6 392 → 3 407 | 294 742 → 150 397 | 82 → 40 | 219 136 → 190 464 |

`examples/hello --development` is 14 885 lines both before and after: an inspectable build keeps everything.

**When.** Production builds only. An inspectable build (`--repl`, `--repl-port`, `--hot-reload` or
`--development`) keeps everything, so live reload has every function to swap and the REPL can reach every
internal ([compiler.md](compiler.md#development-builds-and-tree-shaking)).
It stays in the default build because it makes the whole build faster, not only the program: the C compiler
reads far less. The compiler built at `-O0` took 16.0 s shaken against 19.7 s unshaken, `examples/battle` 3.0 s
against 5.5 (Windows, clang 19.1.5, a busy machine).

**What you notice.** Nothing, except a smaller executable. A function nobody calls, outside a generic
class, is still compiled and checked, so a mistake in it is still reported; it just is not in the binary.

The same pass decides which native symbols are looked up. A `DynamicLibrary` looks up every symbol the program
calls when it opens, and a symbol is looked up only when a function that calls it survived the shaking: a
program that never uses `FileSystemWatcher` does not look up `ReadDirectoryChangesW`, though `library/windows/file_system_watcher.spite`
opens the same `kernel32.dll` as `Program.sleep`. **What you notice.** Fewer allocations under `--debug-memory`,
since each lookup makes two short-lived strings, and a missing symbol that only unused code names does not stop
the program when the library opens. An inspectable build is not shaken, so it looks up every symbol.

### Deciding conditions at compile time

**The case:** [benchmarks/deciding_conditions_at_compile_time](../benchmarks/deciding_conditions_at_compile_time/).

**What it does.** A condition the compiler can answer while compiling is answered then, and only the branch taken
is generated. The branch not taken is not in the program at all, not skipped at run time but *absent*, so it may
even use things that would not compile for this build. That covers:

- a field of [`Build`](programs.md#compile-time-settings-build): every `Build` field is a constant of the built
  program, set by a flag or taken from its declared default;
- a codegen value, `if $is_magic { }`, and a test on a codegen type, `if $value_type == List { }`
  ([metaprogramming.md](metaprogramming.md#tree-shaking));
- a class test the value's type already answers, `if item == $wanted_type`, and one that can never be true for one
  instantiation of a generic, which folds to `false` there instead of being an error;
- `$system_type.functions['run_each']`;
- `attribute.name.starts_with("_")` or `ends_with` with a literal, on the attribute a walk is compiled for, which
  is how the serializers and `to_debug()` leave private attributes out;
- `$system_type.functions['update_each'].is_resumable`, answered from the functions the compiler
  turns into state machines;
- `phase.arguments.count()` and `$system_type.functions['update_each'].arguments.count()`, a whole
  number compared with `==`, `!=`, `<`, `<=`, `>` or `>=`, so a runner compiles only the branch that fits a
  system's arity;
- `$component_type.is_fixed_size` and `attribute.class.is_fixed_size`, which is how
  `Items<T>` picks inline or reference storage ([below](#an-items-storage-is-chosen-while-compiling));
- a test on a codegen value's own codegen values, `$list_type.element_type == Float`,
  `$map_type.value_type == Item`, `$holder_type.held_type == String`, to any depth, in every branch of an
  `else if` chain (`conformance/stage6/codegen_member_fold`);
- `attribute.class == X` in an attribute walk, for an attribute of any type; and where a value known only at
  run time is compared with a `.class` known while compiling (`given == known.class`), the comparison is a class-id
  test and no `Spite.Class` object is made (`conformance/stage6/walked_class_fold`);
- `not`, `and`, `or`, `==` and `!=` over any of these. An `and` whose left side folds to `false`, and an `or` whose
  left side folds to `true`, fold whatever the right side is, as the run-time `and` and `or` would never look at
  it: so `$list_type.element_type == List and $list_type.element_type.element_type == Float` folds for a list of
  text, whose items have no `element_type` to ask about.

An `assert` or `crash` folds too, when its condition asks a question answered while compiling: a `$` value or a
test of a codegen type, `attribute.class`, `functions[...]`, `.is_resumable`, `.is_fixed_size`,
`any_attribute_fits_vector`, `.arguments.count()` or `.is_mutated`, a `Build` field, or a class test the value's
type already answers (`item == $wanted_type`), and `not`, `and` and `or` over them. A check that holds writes
nothing, and one that fails writes its failure (the default returned, or the crash report) with no test, the rest
of its block not compiled ([metaprogramming.md](../specs/metaprogramming.md#codegen-values-)). So a `crash` on a `Build`
field that is false, or on a class test that is false for one instantiation, in a function the program reaches, is
the compile error a folded `crash` is (`diagnostics/folded_build_crash`), rather than a halt when it runs.

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
instantiation.

### Reflection, symbols and registries only where read

**The case:** [benchmarks/reflection_symbols_and_registries_only_where_read](../benchmarks/reflection_symbols_and_registries_only_where_read/).

**What it does.** Reflection is decided at compile time, so the compiler knows exactly what a program reads and
emits only that: a class object's `.attributes`, `.functions` and
`.namespace`, `value.attributes`, `value.memory`, `attribute.value`, the per-class `Person.instances` registry (a
class is only tracked when something asks for its instances), `Spite.Class.instances`, and a class's `to_debug()`.
A symbol literal is an entry of a table the compiler writes with only the symbols the program uses; it is
constant text, so storing and comparing symbols allocates nothing. A template
exists only for the names a program calls: a program that never calls `sum_price()` has no
`sum_price`. An enum's reflection folds the same way: a walk over `Phase.values`
becomes one call per value, each value a constant, so no table of an enum's values, names or
order exists at run time: walking one costs exactly the calls it expands to, and not walking one costs nothing.
A read counts only where it runs: a class object lists its `.functions` (and answers `has_function`,
`function_waits` and `argument_count`) only when a function that asks is part of the program, so a read in a
function nothing calls, the standard library's included, costs nothing
(`conformance/stage6/unreached_function_reads`).

**When.** Every build: what a REPL reads is compiled into the REPL build, so it is read there too. **What you
notice.** Nothing: reflection may be as detailed as it likes, because a program that never reads it carries none
of it. The list behind `.instances` is the compiler's bookkeeping, like the list of singletons to destroy at
exit, so `--debug-memory` does not count it.

### Reflection on constants folds and unrolls

**The case:** [benchmarks/reflection_on_constants_folds_and_unrolls](../benchmarks/reflection_on_constants_folds_and_unrolls/).

**What it does.** A reflection object the compiler can identify (a class named in the code, `$T`, `value.class`
of a class-typed value, and everything read from them) is a constant
([reflection.md](reflection.md#known-while-compiling)). A question asked of it is a literal, `each` over
its list is one call per element, `map` is a list literal, and a function handed one is compiled once for it, so
`Monster.attributes.each(show)` makes no list, no `Spite.Attribute` objects and no boxed values: `show_for_health`
reads `health` directly. A local that holds a constant is not made at all when nothing needs its value at run time.

**When.** Every build. **What you notice.** Fewer allocations under `--debug-memory` than the same walk over a
run-time list, and one copy per element in `--final-classes`.

### Template chains run as one loop

**The case:** [benchmarks/template_chains_run_as_one_loop](../benchmarks/template_chains_run_as_one_loop/).

**What it does.** `teams.filter_is_active().map_leads().sum_age()` reads as three steps, and that is what it means,
but the compiler writes it as one loop over `teams` with no list in between: each element is tested, mapped and
added before the next one is read.

**When.** A template called directly on a `filter_` or `map_` call, on a `List` or `Dictionary`; the steps in the
middle are `filter_` and `map_` (to a member that is a class), and the last may be any template. A list you name
and keep (`var active = teams.filter_is_active()`) starts a new chain, and a chain the compiler cannot write as
one loop runs step by step, which means the same. On a `Vector` or an `Items`, a chain that ends in `sort_by_` or
`find_by_` on the items themselves is one of those: it runs step by step, so `velocities.filter_moving().sort_by_across()`
makes the filtered vector and then the sorted one. So is a chain whose last template is one a program wrote
itself ([collections.md](collections.md#write-your-own-member-template)). See [collections.md](collections.md#chains-run-as-one-loop).

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

### Appending to text in place

**The case:** [benchmarks/appending_to_text_in_place](../benchmarks/appending_to_text_in_place/).

**What it does.** `text = text + piece`, or `text = "{text}{piece}"`, would copy the whole text on every
append, which makes the most common loop there is quietly quadratic. When nothing but that variable holds the
text, the compiler grows its buffer in place instead (doubling its capacity); when something else holds it too,
it copies once, with room to grow, so the other holder still sees the text it had. 100 000 appends take
0.23 s, against 6.9 s with a copy on every append.

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

**What you notice.** Fewer allocations and a faster loop. Nothing else: build text the obvious way
([values_and_types.md](values_and_types.md)).

### The compiler places memory

**The case:** [benchmarks/the_compiler_places_memory](../benchmarks/the_compiler_places_memory/).

**What it does.** A program has one way to ask for raw memory, `heap.allocate(bytes)` on `Memory.Heap()`, and one way
to give it back, `heap.free(address)`. Where the bytes live is the compiler's choice
([Placement](../specs/memory.md#placement-the-compiler-decides-where-memory-lives)):

- **Register:** a number's own memory (`var _memory = Memory.Bytes(4)` in `library/integer.spite`) is its C
  scalar. A number is never an object.
- **Frame:** an allocation a function frees itself, in the same block, whose address it only reads and writes
  through, copies, compares, turns into `text`, hands to a `TypedMemory` or lends to a function of its own class
  proven to keep nothing (or, in `library/`, lends to a function of a `DynamicLibrary` the class holds, the
  operating system call that fills it), and never stores, returns, resizes or passes anywhere else, gets a slot in
  the function's own frame: 256 bytes, or exactly a literal size up to 256. A larger size at run time still goes
  to the heap, and the program's text is the same either way.
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

### Reading an address is one machine operation

**The case:** [benchmarks/reading_an_address_is_one_machine_operation](../benchmarks/reading_an_address_is_one_machine_operation/).

**What it does.** `address.read_long(16)`, `address.write_float(8, value)` and the other reads, writes and atomics of
`Memory.Address` are primitives of the language, like `+`: the compiler writes each one where it is called, as the
single load, store or atomic instruction, with no call and no check. `copy_to` and `compare_bytes` are written the
same way, as the C library's copy and comparison.

**When.** Always; `library/` is the only place that may call them, so every `String`, `List` and `Dictionary`
reads its memory this way.

**What you notice.** Nothing: there is no other way these could run.

### An allocator set after construction is where the object is made

**The case:** [benchmarks/an_allocator_set_after_construction_is_where_the_object_is_made](../benchmarks/an_allocator_set_after_construction_is_where_the_object_is_made/).

**What it does.** `var spark = Particle("spark", 1.5)` followed by `spark.memory.allocator = arena` reads as
though it made the particle on the heap and then moved it. The compiler makes it in `arena` from the start: the
two lines become one construction that asks the arena for the memory, with nothing allocated twice and nothing
decided while the program runs ([memory.md](memory.md#choosing-an-allocator-memoryallocator)).
The same holds for `var kept = ash.copy()` followed by `kept.memory.allocator = arena`.

**When.** Always, for the line right after the one that makes the object; anywhere else setting the allocator is
an error.

**What you notice.** Under `--debug-memory`, an object made in an arena is not an allocation of its own: the
arena's blocks are. A class some line gives an allocator is sixteen bytes larger per object (two hidden pointers:
the allocator, and the function that gives the memory back), on every object of that class, heap ones included;
no other class changes. Setting `Memory.Heap()` is the default and costs nothing.

A `List`, a `Vector<T>` and an `Items<T>` given an allocator take their buffer from it too, through
`memory.allocator` ([memory.md](memory.md#an-object-reads-its-own-allocator)): filling one allocates nothing
on the heap past the arena's blocks. On the heap a buffer still grows in place (`resize`); in any other
allocator it grows by allocating the new size, copying and freeing the old, so in an arena the smaller buffers
stay until the arena goes. A class that never reads its allocator is unchanged, and one that reads it but is
never given one answers `Memory.Heap` with no hidden pointers: reading it is one comparison with the heap.

### Singletons: made on first use, never counted

**The case:** [benchmarks/singletons_made_on_first_use_never_counted](../benchmarks/singletons_made_on_first_use_never_counted/).

**What it does.** A singleton is made the first time something asks for it, not when the program starts, so a
program pays only for the singletons it reaches. It is never reference counted:
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
that singleton in an attribute ([classes_and_files.md](classes_and_files.md#singletons)). `--debug-memory` still
names an object a program leaked, even one that points at a singleton.

### Singletons that hold nothing are static objects

**The case:** [benchmarks/singletons_that_hold_nothing_are_static_objects](../benchmarks/singletons_that_hold_nothing_are_static_objects/).

**What it does.** A singleton with no attributes and no `drop()` (`Memory.Heap`, `TypedMemory<T>`, one per element
type, and `Build`, whose attributes are all settings folded into the program) is one static object: never
allocated, never counted, never freed. `Memory.Heap()` costs nothing, and every program allocates once
fewer for each.

**When.** Production builds only. In an inspectable build (`--repl`, `--repl-port`, `--hot-reload` or
`--development`) each is an ordinary singleton: allocated at first use, one of
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
answers the folded settings in both.

### Atomic reference counts only with threads

**The case:** [benchmarks/atomic_reference_counts_only_with_threads](../benchmarks/atomic_reference_counts_only_with_threads/).

**What it does.** Retaining and releasing a reference is plain arithmetic, except in a program that can share an
object between threads: one that makes a `Concurrent` or a `Parallel` (reads in a row included), runs a
`parallel_each_` pass, or is built with `--repl-port` or `--hot-reload`. Only those are compiled with atomic counts
(and a lock around the `--debug-memory` table).

**When.** Decided per program, from what it uses. **What you notice.** Nothing: the program that never starts a
thread never pays for atomics. In a program that does, only the classes another thread can reach pay for them:
[below](#plain-reference-counts-where-no-thread-reaches-a-class).

### Plain reference counts where no thread reaches a class

**The case:** [benchmarks/plain_reference_counts_where_no_thread_reaches_a_class](../benchmarks/plain_reference_counts_where_no_thread_reaches_a_class/).

**What it does.** An atomic count costs a locked instruction at every retain and release, and most classes of a
program with threads never need one: an engine's components are moved by the program's own thread while a worker
reads a file. So the compiler decides it class by class. Once every generic class is made for its values and the
program is tree shaken, it finds the code that can run on another thread and every call that code can make, and a
class whose objects that code never retains or releases is counted with plain arithmetic, in the whole program.

```gdscript title=plain_counts/summer.spite
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
```gdscript title=plain_counts/point.spite
var across = 0
var down = 0

func Point(starting_across: Integer, starting_down: Integer) {
    across = starting_across
    down = starting_down
}
```
```gdscript title=plain_counts/plain_counts.spite entry
var console = Console()

func PlainCounts() {
    var summer = Summer(1000000)
    var sum = Parallel(summer.total)
    var points = List<Point>()
    var index = 0
    while index < 1000 {
        var point = Point(index, index * 2)
        points.append(point)
        index = index + 1
    }
    var far = 0
    var position = 0
    while position < points.count() {
        var point = points[position]
        if point.across + point.down > 1500 {
            far = far + 1
        }
        position = position + 1
    }
    console.print(sum, far)
}
```
```output
499999500000 499
```

`summer.total` runs on a worker, and the worker lets go of `summer` when it is done, so `Summer` is counted
atomically. Only the program's own thread ever counts a `Point` or the `List<Point>`, so a thousand appends and
reads count them with a plain addition and subtraction, while the worker sums.

Code that can run on another thread is everything a thread the program starts runs (the thread pool's workers and the
scheduler's), every function a foreign library may call (a `ForeignCallback`), every function
whose address the program keeps anywhere else (the release a function value calls for its owner, say), and every
function made into a value that such code may call. A function value is called only by a call with as many
arguments, so a value is counted as run on another thread when code that runs there calls some value of that many
arguments: a `Parallel`'s function, a `parallel_each_` pass and a row of calls run at once are, and a filter of one
argument that only the program's thread calls is not. A class is atomic when that code retains or releases an object
of it, or releases anything that may hold one (an object with an attribute of the class, a list of it, a shape or
union that can be it). A count the compiler cannot place on a class makes every class atomic, as before.

**When.** Every production build of a program with threads, ordinary or `--optimized`. An inspectable build
(`--repl`, `--repl-port`, `--hot-reload`, `--development`) counts every class atomically, since a reload or the
REPL can add code that runs anywhere. **What you notice.** Speed. A class used by work on another thread is atomic
everywhere, also for the objects only the program's thread ever sees: a `List<Integer>` a `Parallel` builds while
reading a file makes every `List<Integer>` of the program atomic. In a game engine's stress test (200 000 entities
moved by two systems, while one `Parallel` loads assets) a tick went from 105 ms to 73 ms, and a physics
step of 5 000 characters from 13.3 to 11.5 ms.
[Proofs](proofs.md#no-other-thread-counts-a-class) states the proof; `conformance/stage6/plain_counts` is the program
above, whose C `check.sh` reads.

### A function taking a `type` is compiled per class

**The case:** [benchmarks/a_function_taking_a_type_is_compiled_per_class](../benchmarks/a_function_taking_a_type_is_compiled_per_class/).

**What it does.** A function whose parameter is a `type` (`Anything`, `Printable`, a shape of your own) is compiled
once for each class that reaches it, following calls through the whole program. A call whose argument's class is
known while compiling calls that class's copy, so a number, `Boolean` or enum value is passed as itself, every
call through the parameter is a direct call, each class test on it is decided while compiling, and an operator on
a parameter typed `Number` is the class's own arithmetic.
A value whose class is known only at run time, read from a `List<Anything>` for example, is passed to the
function's own name, which is a `switch` over the value's class among the closed set of classes the program admits
to the `type`, calling the matching copy and unboxing a plain value on the way. The function as written is still
compiled, under another name, so every mistake in it is reported, and
[tree shaking](#tree-shaking-the-generated-c) drops it.

**When.** Every build but `--repl`, `--repl-port` and `--hot-reload`, where a class the compiler has not seen may
arrive later and the function is compiled as written. Not for a parameter the function assigns to, a `T?` of a
`type`, a row of borrowed items passed to it, a function that waits, a value read at run time in a program that
runs a `Concurrent` (it reaches the function as written), or the functions of `List`, `Dictionary` and the
library's other containers, which store what they are given. Where the function as written runs, an operator on a
value typed by a `type` (`Number`, or a shape of your own requiring `sum`) is a `switch` over the classes of both
sides that reaches the left side's class's own operator
([values_and_types.md](values_and_types.md#every-number-fits-number)).

**What you notice.** More functions in the generated C, named `<function>___for_<position>_<class>`, one per class that
reaches it; a class that never does gets none, even when the function tests for it (`item == Ghost`), and a class the
program only tests for is not in the C at all.

### Boxing only where a value travels as a shape

**The case:** [benchmarks/boxing_only_where_a_value_travels_as_a_shape](../benchmarks/boxing_only_where_a_value_travels_as_a_shape/).

**What it does.** A number, `Boolean`, enum value, `Symbol` or `String` is a plain value everywhere the compiler can
see its type, including where it is passed to a function that takes a `type`
([above](#a-function-taking-a-type-is-compiled-per-class)). A `type` that requires no attributes (`Anything`,
`Printable`, `Debuggable`, a shape of functions only) holds its value in sixteen bytes: the class's id as a tag, and
either the object or, for a number, `Boolean` or enum value, the value itself. So a number, `Boolean` or enum value
is never boxed: it travels as a shape, in a `List<Anything>`, in `console.print`'s list or in `attribute.value`,
with nothing allocated, and a call through the shape reaches its class's function with the plain value. Text and
a `Symbol` are put in a box (one small object, released like any other) where they travel as a shape, since the
sixteen bytes of a `String` do not fit beside a tag; a written text's box is part of the program and allocates
nothing ([short text](#short-text-lives-inside-the-string)). Class instances are objects already and are never
boxed.

**When.** A box: storing text or a `Symbol` where a shape is wanted (a list's element, an attribute, the list of a
variadic call), and reading `attribute.value` of a text attribute.

**What you notice.** Printing a number allocates nothing; text made while the program runs is boxed when it is
printed. A `List` of a `type` without attributes holds sixteen bytes per element instead of an eight-byte
pointer.

### A variadic list the callee only reads lives in the caller's frame

**The case:** [benchmarks/a_variadic_list_the_callee_only_reads_lives_in_the_callers_frame](../benchmarks/a_variadic_list_the_callee_only_reads_lives_in_the_callers_frame/).

**What it does.** The `...values` of a variadic call arrive in a `List`. When the call is a statement of its own
(`console.print(name, count)`), the value of a `var` or of an assignment (`var biggest = largest(a, b, c)`) or what a
`return` answers, outside `and`/`or` and outside a function that waits, and the function called only
reads its list (`count()`, `is_empty()`, `[index]`, `get_at`, `first`, `last`, `contains`, `index_of`, `join`, or passing it to a
function of its own class that only reads it too), the list and its items are in the caller's frame: no allocation for
the list or its items, and its elements (the boxes of text above) are released after the call. A function that stores,
returns, grows or passes on its list anywhere else gets a list on the heap as before, and so does a function of a
`--hot-reload` build's own classes, which can be swapped.

**What you notice.** Two allocations fewer per such call under `--debug-memory`: `text_building` allocates 19 times
(31 before), `short_text` 73 (100), `fused_chain_allocations` 11 (13), `folded_function_value` 9 (13). The case's
`var biggest = largest(...)` made its list on the heap at every call and took 31.6 times plain C's time; in the
caller's frame it takes as long as plain C (1.01).

### Concurrency machinery only where it is used

**The case:** [benchmarks/concurrency_machinery_only_where_it_is_used](../benchmarks/concurrency_machinery_only_where_it_is_used/).

**What it does.** The scheduler, the state machines, the helper threads and the wrappers around every call that can
wait (`Program.sleep`, `Console.read_line`, `File.read`/`write`/`append`,
`Socket.accept_client`/`read_line`/`read_bytes`) exist only in a program that makes a `Concurrent` (itself, or through
[reads in a row](#reads-in-a-row-overlap)) or is built with `--repl-port` or `--hot-reload`. Every other program's
waits are the plain system calls. Even in a program that has the scheduler, a wait with no `Concurrent` alive and no
REPL listening makes the plain blocking call, because that is faster: you never choose between blocking and waiting,
and you never see which one ran.

**When.** Decided per program, from what it uses. **What you notice.** Nothing. The machinery is compile-time state
machines ([below](#hidden-asyncawait-as-compile-time-state-machines)). [concurrency.md](concurrency.md).

### Hidden async/await as compile-time state machines

**The case:** [benchmarks/hidden_async_await_as_compile_time_state_machines](../benchmarks/hidden_async_await_as_compile_time_state_machines/).

**What it does.** Waiting on IO is written as an ordinary call and the compiler turns it into a point where other
work runs. It is done at compile time, with no stacks to switch: every function that can reach a wait from inside
a `Concurrent` is compiled a second time as a **state machine**
(`bootstrap/source/generation/state_machine.spite`, and the `emit_state_machines` part of the generator):

- a **frame**, a C struct holding the function's parameters, every local and temporary of its body, and one slot
  per wait for the frame of the function it is waiting on;
- a **step function** that runs the body until it finishes (answering `true`) or reaches a wait that is not over
  (answering `false`). It starts with a jump to the wait it stopped at, so the next step carries on from there. A
  wait inside a `while` or an `if` is jumped back into directly: every local lives in the frame, so nothing is lost.

A call that waits is found wherever it is written (a statement, a `var`, an argument, a `{...}` inside text, a
`while` condition, which waits again on each pass) and becomes: make the callee's frame, step it, and return
from this step while it answers `false`. The waits at the bottom (`Program.sleep`, the `File`, `Console` and
`Socket` calls, and reading another `Concurrent`) are small state machines the generator writes itself: a timer,
a helper thread's flag, a finished flag. `Concurrent(function)` makes the function's frame and runs it to its first
wait; `library/scheduler.spite` keeps the frames that are not finished and runs them again when something they
wait on may have happened.

**The event loop.** The loop waits on one operating system event (an auto-reset event on Windows, a pipe with
`poll` on Linux and macOS) with a timeout of the nearest timer, and helper threads set it when their system call
returns. It does not use IOCP, epoll or kqueue: an ordinary file is always "ready" to epoll and kqueue, so reading
a file would still need a thread; the Windows console cannot be read through IOCP; and one mechanism keeps each
system's folder to a handful of functions. The cost is a thread per system call in flight, which a server with
thousands of connections would feel. In a browser the loop would be the browser's.

**When.** Only in a program that makes a `Concurrent`, and only for the functions a `Concurrent` can reach that wait:
the plain version of every function stays as it is for the code outside a `Concurrent`, and in a production build the
tree shaker drops whichever version nothing calls. A program that never makes a `Concurrent` has no frames, no step
functions, no event loop and no helper threads; its waits are the plain system calls.

**What you notice.**

- A wait written in the middle of an expression keeps the written order: in
  `log.append("{name} read {file.read()}")`, `name` is read and kept before the file is read, so a change another
  `Concurrent` makes to `name` during the read is not seen. Only what is computed before the wait and could change
  is kept in a temporary: a local, a parameter, a constant or `self` is read where it is used, since nothing can
  change it while the state machine waits. A reference kept that way is retained for the wait and released after
  the expression, one count each way.
- Every wait inside a `Concurrent` is a point it returns from, including the right side of `and` or `or`, a call
  through a function value, a union or a `type`, a constructor, and dropping a handle a local holds. A call
  through a function value in a state machine costs a comparison of its function against each function made into a
  value of that signature that waits; a union's or a `type`'s call, one class test per class. The few waits that
  still run the event loop where they are, as waits outside a `Concurrent` do
  ([concurrency.md](../specs/concurrency.md#concurrency-concurrent-parallel-and-hidden-waiting) lists them), hold their place
  until the wait is over while the other `Concurrent`s keep going. Two such waits that each wait for the other could
  never end, since the one further down the stack resumes only once the one above it returns, so a join that waits
  in place for a `Concurrent` whose state machine is running further down the same stack halts at the join
  (`waits_for_its_own_caller=true`), even while other `Concurrent`s keep the program busy; and a circle of joins
  that are points to return from halts at the join that closes it (`joins_a_concurrent_that_waits_for_this_one=true`,
  `conformance/stage6/concurrent_wait_cycle`). A `Concurrent` whose own function cannot be a state machine runs to its end when it is
  started: a function value a standard-library class made and stored before it reached `Concurrent`, a shape's
  function, a singleton function that takes [the lock](#singletons-a-parallel-reaches-take-a-lock), or any function
  of the program in a `--hot-reload` build, which is called through a slot that a reload swaps.
- Under `--debug-memory`, one allocation per waiting call a `Concurrent` makes (its frame), and no stacks or fiber
  bookkeeping: `conformance/stage6/concurrent_waits` allocates 171 times where a design with stacks allocated 191,
  and its C is 335 275 bytes instead of 345 825.
- The compiler itself makes no `Concurrent`, so compiling it is unchanged (about 1.7 seconds either way); its own C
  grew by 161 kB, the transform's code.

### Reads in a row overlap

**The case:** [benchmarks/reads_in_a_row_overlap](../benchmarks/reads_in_a_row_overlap/).

**What it does.** Two or more `var name = file.read()` (or `socket.read_line()`) written one after another, none
naming a variable an earlier one declared, are started together: every read but the last becomes a `Concurrent`,
and all of them are joined before the next statement
([concurrency.md](concurrency.md#reads-in-a-row-overlap), which has the exact shape).

**When.** Every build, wherever the shape appears. A statement between two reads, a typed `var`, or a name
assigned again later keeps each read where it is.

**What you notice.** Nothing in what the program computes: the values are the same, and a file written by the
next statement is written after the reads. The program waits for the slowest read instead of each in turn. The
cost is that the program carries the [concurrency machinery](#concurrency-machinery-only-where-it-is-used):
the scheduler, a helper thread per read in flight, a `Concurrent` per overlapped read, and
[atomic reference counts](#atomic-reference-counts-only-with-threads). Two files read in a row allocate 62 times
under `--debug-memory` and compile to 2 865 lines of C; the same reads with a `console.print` between them
allocate 19 times in 1 511 lines.

### A wait in a frame does not hold the frame

**The case:** [benchmarks/a_wait_in_a_frame_does_not_hold_the_frame](../benchmarks/a_wait_in_a_frame_does_not_hold_the_frame/).

**What it does.** Inside a frame loop (a `while` whose passes sleep, in the loop or in a function of its own object
it calls), a statement that calls a function answering nothing on another object of the program, with no
arguments, is started as a `Concurrent` when that function can wait
([concurrency.md](concurrency.md#a-wait-in-a-frame-does-not-hold-the-frame)). The handle goes to
`library/waits_in_flight.spite`, a singleton that keeps the unfinished ones and lets the finished ones go each time
it is handed another. The loop's next sleep runs the event loop, so the started work carries on between frames.

```gdscript title=a_wait_in_a_frame_doc/backup.spite
var program = Program()
var copied = 0

func copy_next() {
    program.sleep(20)
    var file = File(".spite/documentation_backup.txt")
    file.write("part {copied}")
    copied = copied + 1
}
```
```gdscript title=a_wait_in_a_frame_doc/a_wait_in_a_frame_doc.spite entry
var console = Console()
var program = Program()
var backup = Backup()
var redraws = 0

func AWaitInAFrameDoc() {
    while backup.copied < 3 {
        redraws = redraws + 1
        if redraws <= 3 {
            backup.copy_next()
        }
        program.sleep(1)
    }
    console.print("copied", backup.copied, "parts, redrawing the progress bar meanwhile:", redraws > 3)
}
```
```output
copied 3 parts, redrawing the progress bar meanwhile: true
```

The three copies are in flight at once, each started on its own redraw, and the bar is redrawn every millisecond
while they wait.

**When.** Every build, in the program's own classes (never the standard library's), in the plain copy of a function
(a function a `Concurrent` runs waits in its state machine as before), and only when the called function can reach
a wait, which is known once every function it reaches is compiled: so the functions holding such a call are
compiled after the others, and if a function compiled later changes the answer, the build fails and says where.

**What can stop it.** The started work and the frame take turns at their waits, so the compiler reads the plain C
of every function either side reaches that can wait, line by line and twice over for loops. A number, `Boolean`,
enum, text or value-class local computed from an attribute and still held after a wait is carried; writing it
back into an attribute of the same name (directly, through a function that writes one, or through a returned
value), or one line that reads an attribute, waits and writes it, keeps the call waiting in place, exactly as it
would have without this optimisation. It is listed under "Waits that hold the frame" in `--optimization-report`.

**What you notice.** The started work finishes in a later frame, which is the point. A program with no frame loop,
or whose frame loops call nothing that waits on another object, compiles exactly as before. One that starts a wait
carries what a `Concurrent` carries ([concurrency machinery](#concurrency-machinery-only-where-it-is-used)), plus
`WaitsInFlight`: a list of the handles in flight and their sites, scanned each time a wait is started. Under
`--debug-memory` each started call costs what `Concurrent(function)` costs.

### REPL, live reload and debug machinery only in those builds

**The case:** [benchmarks/repl_live_reload_and_debug_machinery_only_in_those_builds](../benchmarks/repl_live_reload_and_debug_machinery_only_in_those_builds/).

**What it does.** Everything that exists to look inside a running program is compiled only into the builds that
ask for it:

- `--repl` and `--repl-port`: the loop, the socket thread, the reflection hooks that let a `Spite.Attribute` walk
  and assign live values (outside those builds they answer an empty list, `false` and `null`), and every fitting
  member template instantiated for the classes a list reaches, so the prompt can call `monsters.sum_health()`.
- `--hot-reload`: a function pointer per function and a forwarder in front of it (about a nanosecond a call), the
  standard library's functions and the compiler's helpers included, so none of them is inlined into its caller;
  the program's own `Build` fields read from the `Build` singleton instead of folded; the file watcher and the
  reload manifest. Such a build is expected to be slower: it exists to give information while the program runs,
  and benchmarks measure production builds only. Every other build calls functions directly
  and is tree-shaken.
- `--debug-memory`: the allocation table that names leaked objects, and its C (`AllocationTable`, the functions
  that call it, the class-name table) exists only in that build's C. Every other build allocates with the C
  library's own `malloc`, `realloc` and `free` and nothing beside them, unless the program reads
  `live_allocations()` ([Allocation is the C library's](#allocation-is-the-c-librarys-counted-only-where-read)).
- `--repl-port` and `--hot-reload`: a check point at the end of every pass of every loop in the program's own
  code, so a program that never waits still answers. It is one relaxed load of a flag the REPL's thread and the
  file watcher raise when they have something; only then does the pass call `Scheduler.check_point()`. Measured
  on a loop of 400 000 000 passes in a `--repl-port` build: 1 566 ms when every pass called it, 106 ms with the
  flag.

**When.** Only in those builds. This is not an optimisation an inspectable build turns off: it is the inspecting
itself, present only where it is asked for.

**What you notice.** Nothing in an ordinary build: its C is byte for byte the same with or without the check
points.

### A reload compiles only the classes that changed

**The case:** [benchmarks/a_reload_compiles_only_the_classes_that_changed](../benchmarks/a_reload_compiles_only_the_classes_that_changed/).

**What it does.** `spite reload` (the running program's `reload`, and its file watcher) compiles the functions of
the classes the changed files declare and nothing else of the program: every class is still read and checked,
but every function the running program already has with the same prototype is left out, and the library reaches
it in the running program. What compiling one class depends on in the others (the generic instances and
functions the compiler made, which classes fit a shape, which functions can wait, which attributes are read, the
call effects) comes from the manifest the build wrote beside the executable
([repl.md](repl.md#how-it-works)). The library's C carries only the types and declarations its functions use.

**When.** In every reload of a `--hot-reload` build, unless the changed code changes one of those facts (then the
reload compiles the whole program, as before, and says why on the error output). A reload that compiled the whole
program becomes the baseline the next one compares with, so the save after a change to a class's attributes is
fast again ([repl.md](repl.md#how-it-works)).

**What you notice.** A changed system of a large game's server swaps in about 6 seconds after the save, where a
whole compile took 20 to 30. Nothing else: what swaps in is what a whole compile writes. The `--hot-reload`
build writes a larger manifest (about 40 MB for that server) and spends about a second more writing it, and
each class's `functions` list is written once more at the end of compiling when the class gained a function late
([reflection.md](reflection.md)).

### What a `--hot-reload` build carries so its objects can move

**The case:** [benchmarks/what_a_hot_reload_build_carries_so_its_objects_can_move](../benchmarks/what_a_hot_reload_build_carries_so_its_objects_can_move/).

**What it does.** Nothing faster: this is the price of [moving live objects to new
attributes](repl.md#changing-a-classs-attributes), paid only in a `--hot-reload` build. Each object
of a program class carries two hidden words after its header, and each allocation and release of one adds it to
or takes it from its class's list of live objects (a lock when the program has threads). An `Items` or `Vector`
that keeps a program class's objects is listed the same way, whether it keeps them in its own memory or as
references, and items kept in its own memory carry the two words too. Every function that reads a program class's attributes without being its own (its allocation, release,
copy and deep copy, the REPL's reflection and assignment, a union's dispatch, the functions of a standard-library
template made for it such as `List<Monster>` or `Items<Step>`) is called through a slot, one indirect call, like the
class's own functions. A slot is read with an acquiring atomic load, so a C compiler optimising at any level
can neither fold a call through it to the function the build started with nor hoist the read out of a loop. Each
class has a table of its layout. Until a reload changes a class's attributes, reading
one is a plain load (`<Class>___fields(object)` is the object); after, the class's code tests whether the object
moved first.

**When.** In every `--hot-reload` build; a normal build has none of it.

**What you notice.** Sixteen more bytes in each object of a program class and in each item of an `Items<T>` of one,
an indirect call where a standard-library template made for a program class was a direct one, and a little work
per allocation and release. A `--hot-reload` build of a large game's server behaves the same; nothing is visible
to the program, since the hidden words are not attributes: reflection, `to_debug` and JSON see only the class's
own.

### The thread pool only where a `Parallel` is made

**The case:** [benchmarks/the_thread_pool_only_where_a_parallel_is_made](../benchmarks/the_thread_pool_only_where_a_parallel_is_made/).

**What it does.** `ThreadPool` is a singleton made the first time a `Parallel` (or a `parallel_each_` pass) needs
it, and it starts its worker threads then, once.
A program that never makes one starts no thread and allocates nothing for it; its functions are tree-shaken with
the rest. `ThreadLocal` asks the system for its per-thread slot only when one is made, and `Lock` likewise;
its `get()` never locks, and only a thread's `set` does
([concurrency.md](concurrency.md#a-value-per-thread-and-a-lock)).

**When.** Always. **What you notice.** Nothing until the first `Parallel`, which pays for starting the workers.
[concurrency.md](concurrency.md#the-thread-pool).

### Singletons a `Parallel` reaches take a lock

**The case:** [benchmarks/singletons_a_parallel_reaches_take_a_lock](../benchmarks/singletons_a_parallel_reaches_take_a_lock/).

**What it does.** In a program that makes a `Parallel`, every singleton of the program's own that can change after it
is made gets a lock of its own, taken around every one of its functions that touches what can change (which functions,
below). It can change when one of its functions assigns one of its attributes outside its constructor, when code in
another class assigns one (`registry.last = name`), or when it holds a list, a dictionary, a function value, or an
object of a class that can change (an object whose class never assigns its attributes after its constructor, and holds
nothing that can change either, is as read-only as a number, so a `Rules` holding a `Limits` made once takes no lock).
This is the fallback: the compiler takes it only when none of the cheaper forms in the next section is proven safe for
that singleton. The compiler checks that its functions hand out nothing they own.

How the lock is kept cheap:

- **A function that touches none of the changing state takes no lock.** Only a function that reads or writes an
  attribute that can change (one assigned after the constructor, here or from another class, or holding a list,
  a dictionary, a function value or an object that can change), or calls one of its own functions that does, or
  calls out to code that can call back into the singleton (from the call-effects facts), is wrapped in the lock.
  `make_piece(size)`, which only computes from its arguments, or a function that only reads an attribute set once
  in the constructor, runs as written. This is as safe as locking it (a function that touches nothing that
  changes is one indivisible step on its own), and it removes a deadlock: a locked function that polls a
  `Parallel` whose work calls a stateless function of the same singleton would wait for that work while the work
  waited for the lock. The compiler decides it from the function's C: every use of the singleton that is not a
  read of an attribute that never changes, or a call to one of its own functions, counts as touching it
  (`conformance/stage6/singleton_stateless_calls`: `Workshop.build` is locked and `Workshop.make_piece` is not).

- **Each lock has a cache line of its own.** A lock is 64 bytes, aligned to 64 (`_Alignas(64)`, C11), so two
  singletons' locks never share a line and taking one never makes another core reload the other. In a game
  engine's stress test (200 000 entities, two `parallel_each_` systems) this took a tick from 108 ms to 45 ms, the
  same as with no locks at all.
- **A call to itself skips the lock.** Inside a locked function, a call to another function of the same singleton
  goes straight to that function's unlocked body (`Registry_count_one___unguarded(self)`), since the lock is
  already held: no atomic load and no depth count per call. A function value of it is not such a call:
  `found.filter(matches)` inside the singleton makes a value that calls the locked function, because a value can be
  kept and called from anywhere; called while the lock is held, that costs one atomic load and a depth count
  (`conformance/stage6/singleton_function_values`).
- **A write from another class takes the lock too.** `registry.last = name` written anywhere but `Registry` stores
  the value under `Registry`'s lock; the value is computed before the lock is taken, and an object it replaces is
  released after the lock is let go, so no other code runs while it is held. A read from another class
  (`var last = registry.last`) takes it too, around the one load (and the count of what it reads).
- **A singleton is never counted.** Fetching one (`var registry = Registry()` in a function) is one load, and
  letting go of it is nothing: a singleton's retain and release compile to nothing, in generic singletons too.
- **A thread that finds the lock taken backs off.** The lock is taken with one compare-and-swap when it is free.
  When it is not, the thread waits with the processor's pause instruction, 1, 2, 4 and up to 1 024 pauses between
  looks, reading the lock before it tries to take it again, and past that gives its turn to the system
  (`SwitchToThread`, `sched_yield`) between looks. The thread holding the lock keeps its cache line instead of
  losing it to every waiter's attempt, so it finishes sooner. Measured against the plain compare-and-swap loop it
  replaced and the system's own locks, on a million calls that append to a list (best of seven, on a sixteen-core
  machine): one thread 2.9 ms for the plain loop and 3.1 ms backing off; two threads 30.8 and 5.0 ms; four 78.9 and
  5.7 ms; eight 165.7 and 9.6 ms, where a slim reader-writer lock took 7.4, 10.9, 17.3 and 40.1 ms and a critical
  section 7.1, 11.0, 27.9 and 66.1 ms. The waiting is the only change: who holds the lock, and in what order, is
  what it was, and the lock is still re-entered for free by the thread that holds it.

**When.** Only in programs that make a `Parallel`, run a `parallel_each_` pass or make a `ForeignCallback` (C may
call one from a thread of its own), and only for a singleton that such a thread can reach; in any other program a
write from another class is a plain store.

**What you notice.** An uncontended lock per call to such a singleton (two atomic operations), and waiting when
two threads call it at once (`benchmarks/singletons_a_parallel_reaches_take_a_lock`, four threads on one
singleton: 113 ms with a plain compare-and-swap loop, 7.7 ms backing off, against 21.5 ms for the same program in C
behind a critical section). In `conformance/stage6/singleton_lock_calls`, `Registry` is locked, pads its lock,
calls itself unlocked and locks the write `registry.last = ...` and the read of `registry.last` from the entry
class, and `Rules` takes no lock.

### Thread safety for singletons, the cheapest safe form

**The case:** [benchmarks/thread_safety_for_singletons_the_cheapest_safe_form](../benchmarks/thread_safety_for_singletons_the_cheapest_safe_form/).

**What it does.** For each singleton of the program's own that can change after it is made, the compiler picks
the cheapest form that is as safe as the lock, from what that singleton's functions actually do. You write
nothing; the source is the same in every form. The forms are tried in this order (the first four here, the rest
[below](#thread-safety-for-singletons-the-rest-of-the-plan)):

1. **Nothing, because no `Parallel` reaches it.** The compiler walks the program's calls from every function a
   `Parallel` or the thread pool can run (every function passed or stored as a value, which is how a function
   reaches `Parallel(worker.run)` or `ThreadPool.submit`, and the member and `filter_` members of every
   `parallel_each_` pass), following each call to the functions it can reach (by the class of the value it is
   made on, or every class's function of that name when the class is not known, and every member a template name
   such as `sum_price` can stand for). A singleton none of those functions calls is used by one thread only, the
   program's own, and takes no lock. A singleton with a function the compiler can call without a call being
   written (an operator such as `sum` or `equals`, a getter or setter `get_`/`set_`, `get_at`/`set_at`,
   `missing_function`, `to_string` or `to_debug`) is always counted as reached.
2. **Nothing, because it never changes.** A singleton whose functions never assign one of its attributes after its
   constructor, whose attributes no other class assigns, and that holds no list, dictionary, function value or
   object that can change, is read-only once made: it takes nothing.
3. **Atomics, for counters and flags.** A singleton whose attributes that change are all whole numbers or `Boolean`s
   (every other attribute set only by the constructor, none holding anything that can change, and none assigned
   from another class), where each function touches that changing state at most once (one read, one
   `count = count + step` or `count = count - step`, or one `flag = value`, where neither `step` nor `value`
   reads the changing state, and not inside a `while` or through another function of its own that touches it
   too), has every one of those reads and writes compiled as a single atomic instruction (`__atomic_load_n`,
   `__atomic_fetch_add`, `__atomic_store_n`) and takes no lock. One touch per function is what makes this exactly
   as safe as the lock: the lock makes each function one indivisible step, and so does one atomic instruction.

Only when none of these applies does the singleton take [the lock](#singletons-a-parallel-reaches-take-a-lock).

4. **The lock, with one attribute read past it.** A locked singleton's whole-number or `Boolean` attribute that each
   of its functions writes at most once (not inside a `while`, and not through another of its functions that writes
   it too) is atomic on its own. Reading it from another class is one atomic load and takes no lock; the
   singleton's functions still take their lock and read and write it atomically; a write to it from another class
   still takes the lock. Since no write leaves it half done and every writer holds the lock, a reader that takes no
   lock sees a value some locked step left, exactly as if it had waited for the lock and read it.

```gdscript
singleton

var stage = 0
var notes = Dictionary<String, Integer>()

func note(key: String) {
    notes[key] = stage
}
```

`notes` keeps `Ledger` locked, while a task reading `ledger.stage` a thousand times takes no lock at all
(`conformance/stage6/lone_atomic_counter`, where `marks`, written twice by one function, stays behind the lock).
A game engine's stress example, 200 000 entities moved by two systems at once, reads its stage counter once per
entity and component: with the counter read past the lock, a tick went from about 63 ms to about 20 ms on a 4-core
machine, since both systems had been queuing for one lock to read one number.

**Example.** `conformance/stage6/singleton_forms`: four `Parallel` workers and the program's own thread record
41 000 hits into a `HitCounter` (atomics), read a `Settings` made once (nothing), while only the program's thread
writes a `Journal` holding a `List` (nothing: no `Parallel` reaches it). Before, `HitCounter` and `Journal` each
took a lock with the lock as the only form; with the forms above no singleton in it does.

**When.** Every build that is not `--hot-reload`, in programs that make a `Parallel`, run a `parallel_each_` pass or
make a `ForeignCallback`. An atomic singleton in a program without `Parallel`, or one no `Parallel` reaches, compiles
its reads and writes as plain ones: the executable is the same as with no form at all.

**What you notice.** Nothing in what a program prints or computes, and no waiting on a counter two threads bump at
once. What a lock gave and these forms keep: each function of the singleton is still one indivisible step. A singleton
whose attribute another class assigns is locked instead (its write takes the lock, above); reading a singleton's
attribute directly from another class takes the singleton's form too: in a program where a `Parallel` reaches it,
`registry.last` read from another class takes its lock, an atomic counter's attribute is read with one atomic load,
and a singleton that never changes is read plainly. `counter.hits = counter.hits + 1` from outside is still a read and
a write, two steps.

### While no task runs, a singleton's lock is skipped

**The case:** [benchmarks/while_no_task_runs_a_singletons_lock_is_skipped](../benchmarks/while_no_task_runs_a_singletons_lock_is_skipped/).

**What it does.** Every function a singleton's lock wraps first asks whether any work is on the thread pool: the pool
counts every task from the moment it is handed out until it has run (`spite_tasks_in_flight`, one add and one
subtract per task), and when the count is zero (before the first `Parallel`, after the last one has been read,
between an engine's stages) only the program's own thread runs the program, so the function runs without the
lock. It notes on a small stack of its thread's that it
skipped this singleton's lock, and if it starts a task itself before it returns, the pool takes every lock the
thread skipped before the task is counted, and the function lets it go when it returns: so the task finds the
singleton locked exactly as it would have, and nothing it can see differs.

**When.** In every wrapper the lock gives a function, writing and reading ones alike, in a program that makes a
`Parallel` or a `parallel_each_` pass; sixteen skipped singletons deep at most, beyond which the lock is taken as
before. Writes and reads of attributes from other classes and counted loops keep their lock.

**What you notice.** Speed where the program's own thread calls a locked singleton while nothing runs on the pool, an
engine applying queued inserts between stages for example: [its case](../benchmarks/while_no_task_runs_a_singletons_lock_is_skipped/),
ten million calls on the program's thread with no task in flight, takes 52.6 ms, against 76.4 ms for naive C, which
takes a mutex on every call, and 2.8 ms for expert C, which keeps the tally in registers. In a spawn-shaped program doing real work per call (`Column<T>` inserts: growing a list,
appending to an `Items`, reading a row) the lock was about 1 ns of 14 ns a call, so there it measures 13 ns. Each call
reads one counter no thread writes while it is zero, and each task costs two atomic additions.

A `copy()` or `deep_copy()` of a class that holds nothing counted is not written as one `memcpy` of its
attributes. Measured: 200 000 copies of a two-`Float` class took
31-32 ns each before and 36-38 ns after, the allocation being nearly all of it. Measured again on a six-attribute
class (five `Float`s and an `Integer`), 20 000 000 copies at `clang -O2`: 785-797 ms written attribute by attribute,
783 ms and more as one `memcpy`. The C compiler already turns the attribute copies into a handful of wide moves
(five instructions after the allocation either way), so a `copy()` of a class whose attributes all fit a `Vector` is
a `memcpy` in the machine code, and what is left is its allocation (about 39 ns of a copy that is kept).

### A singleton no other thread reaches takes no lock

**The case:** [benchmarks/a_singleton_no_other_thread_reaches_takes_no_lock](../benchmarks/a_singleton_no_other_thread_reaches_takes_no_lock/).

**What it does.** Once the whole program is written out, the compiler looks again at which code can run on another
thread, this time in the generated program itself, with every generic class made and everything unused shaken out,
the same walk that decides [plain reference counts](#plain-reference-counts-where-no-thread-reaches-a-class). A
singleton that none of that code calls, reads or writes keeps no lock: its functions call their bodies directly,
its attributes read and written from other classes are plain loads and stores, and an attribute that would have been
atomic on its own is a plain number again. The first walk, made while the program is still being compiled, has to
count every function made into a value as something a thread might run; this one sees which values a thread
actually calls.

```gdscript title=unshared_ledger/ledger.spite
singleton

var total = 0
var marks = List<Integer>()

func note(amount: Integer): Integer {
    total = total + amount
    if amount % 100 == 0 {
        marks.append(amount)
    }
    return amount + 1
}

func is_large(amount: Integer): Boolean {
    return amount > 10
}
```
```gdscript title=unshared_ledger/counter.spite
func run(): Integer {
    var sum = 0
    var index = 0
    while index < 100 {
        sum = sum + index % 7
        index = index + 1
    }
    return sum
}
```
```gdscript title=unshared_ledger/unshared_ledger.spite entry
var console = Console()
var ledger = Ledger()

func UnsharedLedger() {
    var counter = Counter()
    var counted = Parallel(counter.run)
    var amounts = [4, 12, 30, 7]
    var large = amounts.filter(ledger.is_large)
    var index = 0
    while index < 1000 {
        index = ledger.note(index)
    }
    var marked = ledger.marks.count()
    var large_count = large.count()
    console.print("counted", counted, "large", large_count, "total", ledger.total, "marked", marked)
}
```
```output
counted 295 large 2 total 499500 marked 10
```

`ledger.is_large` is made into a value for `filter`, so while compiling, `Ledger` counts as something a `Parallel`
might reach and is given a lock. In the program as written out, the only code that can run on another thread is
`Counter.run` and what it calls, which is none of `Ledger`; and the value `filter` calls is called on the program's
own thread. So `Ledger_note` is only its body:

```c
int32_t Ledger_note(Ledger* self, int32_t amount_) {
SPITE_GUARDS_COUNT(1);
int32_t spite_unshared = Ledger_note___unguarded(self, amount_);
SPITE_GUARDS_COUNT(-1);
return spite_unshared;
}
```

where it was the test for work in flight and the lock behind it, and the read of `ledger.total` is a plain load
where it was an atomic one. The count of locks held stays, so a wait inside such a function behaves as before.

**When.** In a program with threads, every build that decides [plain
counts](#plain-reference-counts-where-no-thread-reaches-a-class): not `--repl`, `--repl-port`, `--hot-reload` or
`--development`, which keep every lock. For each singleton that took a lock, or an atomic attribute, while
compiling: none of the code that can run on another thread (from every function whose address the program keeps, a
function value's target only where that code calls a value with as many arguments) names its lock, its readers'
counts or its atomic attributes. A singleton any of that code calls keeps the form it had.

**What you notice.** Speed, and nothing else: the singleton's functions were already one indivisible step for the
only thread that calls them. The case's ten million calls take 24.3 ms instead of 41.1 (medians of seven). The naive engine's stress
test, whose matchers and columns are singletons only the program's thread calls while a recipe catalog loads on the
pool, ticks in 36.9 ms instead of 43.3 as one C file (42.7 instead of 50.3 split), and its physics step takes 12.6
ms instead of 13.4.

### A singleton's reading functions do not exclude each other

**The case:** [benchmarks/a_singletons_reading_functions_do_not_exclude_each_other](../benchmarks/a_singletons_reading_functions_do_not_exclude_each_other/).

**What it does.** A locked singleton's function that only reads its state (an engine column's `at(row)`, a lookup)
does not take the lock itself. It takes the readers' side: it adds one to a count of its own thread's (one of 32
counts, each on a cache line of its own), checks that no function that writes holds the lock, reads, and takes the
one away. A function that writes takes the lock as before and then waits until every count is zero, so it never
runs beside a reader, and readers never run beside it. Readers on different threads touch no line in common, so
eight systems reading one column per row do not hand a cache line from core to core on every call.

**When.** For each function the lock would wrap, in a program that makes a `Parallel`, when the
compiler proves from its source that it changes nothing: its statements declare and assign only its own locals,
branch, loop, `return`, `assert` or `crash`; it calls only reading functions of its own class, the reading members
of a `List` or `Dictionary` it holds or is passed (`count`, `get_at`, `[]`, `get`, `has`, `is_empty`, `first`,
`last`, `contains`, `index_of`, `keys`, `values`, `join`, `copy`), anything on text and numbers but a `write_` or `copy_to`,
`read_value`-like reads of `TypedMemory` and `InlineMemory`, and reading functions of a singleton that holds no
state (an engine's `Raw.read_long`); it reads attributes that have no getter; its text has no holes (a hole could
call `to_string`); and its operators are on numbers, or the program declares no operator function at all. Anything
else is a writing function. And it is only for a singleton that the work of a `Parallel` only reads: none of its
writing functions, and no function that writes its attributes from another class, is reachable from what a
`Parallel` or a `parallel_each_` pass runs (an engine's columns, written between stages and read by the systems
of a stage). A singleton that is also written from the pool keeps the plain lock: a write there would scan the counts
on every call (measured: `Row.advance()` per entity took an engine's `stress` tick from 8 ms to 25 ms). A
singleton with reading functions that qualifies gets the counts; its writing functions,
writes and reads of its attributes from other classes, and counted loops of calls (below) use the matching side, and
a counted loop that calls only reading functions takes the readers' side once for the whole loop. The thread that
holds the writers' side reads without counting, so a writing function calling out to code that reads back is not
held up by itself.

**What you notice.** Speed where several threads read one singleton:
[its case](../benchmarks/a_singletons_reading_functions_do_not_exclude_each_other/), eight `Parallel` systems each
reading 15 000 rows of one singleton through `at(row)` for 20 ticks, takes 17.4 ms, against 103.0 ms for naive C,
whose eight threads queue on one mutex for every read, and 5.8 ms for expert C, which reads a column with no lock. A reading call on one thread costs about the same as a plain lock (a locked add on a line only that thread
writes, instead of a compare-and-swap on a shared one). A write also looks at the 32 counts once per outermost call:
about 32 loads that stay in the writing core's cache while nothing reads. Each singleton with reading functions
carries 2 KiB of counts. What a program computes is unchanged: a reading function still sees the singleton's state
whole, never half-way through a write.

### A counted loop of calls to one singleton takes its lock once

**The case:** [benchmarks/a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once](../benchmarks/a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once/).

**What it does.** A `while` that calls functions of one locked singleton many times (a worker removing rows through
`columns.remove_row(entity)`, a system adding to a tally) takes that singleton's lock once around the whole loop
and calls the functions' unlocked bodies inside it, instead of taking and letting go of the lock on every call.
One of those per call is two atomic operations when no
other thread wants the lock, and when other threads do, every call hands the lock's cache line from core to core.

**When.** In a program that makes a `Parallel`, for a `while` in any class but the singleton itself when all of this
holds, so that the lock held longer can neither deadlock nor wait on anything:

- **It ends on its own.** It is counted: `index < bound`, `index` a local stepped by `index = index + 1` as the
  body's last line and assigned nowhere else in it, and `bound` a whole number, a name the body never assigns, or
  `.count()` of a list of plain values. So it never waits for another thread to change the singleton (a poll such
  as `while got == 0 { got = mailbox.take() }` keeps a lock per call), and every `while` inside it is counted too.
- **It locks nothing else.** Its calls are functions of that one singleton, reached through the attribute that binds
  it, and the reads and writes of a list of plain values (`count`, `get_at`, `set_at`, `append`, `contains`, `index_of`,
  `is_empty`, `[]`); what it computes is numbers, `Boolean`, text without holes and enum values, so no operator,
  getter or `to_string` of a class of the program's can run in it. A lock it takes nothing else under adds no new
  order between two locks: whatever the singleton's functions lock, they lock under its lock already.
- **It waits for nothing.** No `Parallel` or `Concurrent` is read in it (none is even named), it cannot `return`,
  and the singleton's functions it calls can reach no wait (the facts `is_resumable` uses) and no `Parallel`,
  `Concurrent`, `ThreadPool`, `Scheduler`, `Lock`, `Program`, `Console`, `File` or `Socket`.
- **The lock is real.** At least one of the functions it calls takes the lock; a loop calling
  only functions that touch no changing state, or a singleton that takes no lock, is left as it was.

Not in a `--hot-reload`, `--repl` or `--repl-port` build, nor in the resumable copy of a function a `Concurrent`
runs. **What you notice.** Speed:
[its case](../benchmarks/a_counted_loop_of_calls_to_one_singleton_takes_its_lock_once/), four workers each making 5
million calls on one shared singleton, takes 77.2 ms, against 500.5 ms for naive C, which takes the mutex on every
call and hands it between cores, and 3.4 ms for expert C, where each thread keeps a tally of its own. Nothing a program prints changes: the loop's calls run
exactly as before, and other threads' calls on the singleton wait until the loop is over instead of slipping in
between two of its calls, which is one of the orders they could already run in. A long counted loop keeps other
threads that want the singleton waiting for all of it. In the C, the loop is between `spite_coarse_<n>_enter()` and
`spite_coarse_<n>_leave()` and calls `<function>___unguarded` (`conformance/stage6/coarse_locks`).

### An argument its caller holds is passed without counting

**The case:** [benchmarks/an_argument_its_caller_holds_is_passed_without_counting](../benchmarks/an_argument_its_caller_holds_is_passed_without_counting/).

**What it does.** Passing an object to a function counts it once more for the callee's own name and lets that
count go when the callee returns: two atomic operations on the object's header in a program with threads. When
the argument is a name the caller already holds for the whole call (one of its own parameters, or a local it
owns), the count adds nothing: nothing the call runs can reassign the caller's name. So the call goes to a copy of
the function, `<name>___held_<positions>`, in which those parameters are not released at its end, and the caller
passes them as they are. A game engine's `run_positions(..., rows)` calling
`matcher.match_into(entity, rows)`, which hands `found` on to `find_row(index, entity, found)` for every column,
counts nothing per entity: each is a `___held_` copy passing its own parameter on to the next.

**When.** Every build but `--hot-reload`, `--repl`, `--repl-port`, `--development` and the resumable copy of a
function a `Concurrent` runs, for a call written by name to a program's own function (not the library's), at a place
whose parameter is a class, list or dictionary (not text, a union or a variadic list), when the callee never assigns
that parameter itself; not for a constructor, and not where the call's result is made in the caller's frame (the
frame-made result keeps the ordinary function). Keeping the parameter needs nothing extra: storing it, returning it or
passing it to a function that keeps it counts it there, as storing any name does.

**What you notice.** Speed:
[its case](../benchmarks/an_argument_its_caller_holds_is_passed_without_counting/), a list passed on through two calls
for each of 3 million entities in a `Parallel`, takes 13.5 ms, against 38.2 ms for naive C, which counts the list up
and down atomically in every function it is handed to, and 0.7 ms for expert C, which keeps it on the stack. A `___held_` function appears in the C
beside the ordinary one for each set of positions a program passes this way (about 3% more C in the compiler
compiling itself), and the ordinary one is shaken out when nothing else calls it. Allocations and results are the
same (`conformance/stage6/held_arguments`: a bag passed on, kept, returned and assigned over).

### An attribute a call cannot assign is passed without counting

**The case:** [benchmarks/an_attribute_a_call_cannot_assign_is_passed_without_counting](../benchmarks/an_attribute_a_call_cannot_assign_is_passed_without_counting/).

**What it does.** [The held argument](#an-argument-its-caller-holds-is-passed-without-counting) is not only a
name: an attribute of the object a function runs on (`amounts`), or a path of attributes from it or from a name the
caller holds (`customer.terms`, `row.position`), is passed to a call without counting it when nothing the call can
run assigns any attribute along that path. The object that holds the attribute keeps holding the same value for the
whole call, so the count added for the callee and taken away when it returns is two writes for nothing.

```gdscript title=held_attributes/invoice.spite
var amounts = List<Integer>()
var customer = Customer()

func add(amount: Integer) {
    amounts.append(amount)
}

func due(): Integer {
    return total_of(amounts) - discount_of(customer.terms)
}

func total_of(values: List<Integer>): Integer {
    var total = 0
    var index = 0
    while index < values.count() {
        total = total + values[index]
        index = index + 1
    }
    return total
}

func discount_of(terms: Terms): Integer {
    return terms.discount
}
```
```gdscript title=held_attributes/customer.spite
var terms = Terms()
```
```gdscript title=held_attributes/terms.spite
var discount = 15
```
```gdscript title=held_attributes/held_attributes.spite entry
var console = Console()

func HeldAttributes() {
    var invoice = Invoice()
    invoice.add(120)
    invoice.add(80)
    var due = invoice.due()
    console.print("due", due)
}
```
```output
due 185
```

Neither `total_of` nor `discount_of`, nor anything they call, assigns `Invoice.amounts`, `Invoice.customer` or
`Customer.terms`, so both go to their `___held_0` copies with the attribute read in place:

```c
Invoice_total_of___held_0(self, self->amounts_)
Invoice_discount_of___held_0(self, (self->customer_)->terms_)
```

where they were `Invoice_total_of(self, List_Integer___retain(self->amounts_))` and the same for `terms`. What a call
can assign is read from the call effects every function already has, followed through every call it makes: an
attribute assigned anywhere it reaches (`current = Pair()` two calls down) keeps the count, and so does a call
whose effects are not known. A path may end in an attribute of a shape (a template's `row.attributes[attribute]`),
which is then read with the shape's reading function that takes no count. A function's held parameter is held
for the calls it makes in turn, so a chain of calls passes one object down without counting it at any step.

**When.** Every build but `--hot-reload`, `--repl`, `--repl-port`, `--development` and the resumable copy of a
function a `Concurrent` runs, for a call to a program's own function, as for a held name. The path is attributes
only (no `[]` read but a template's attribute, no getter, no call), from the object the function runs on or from a
name the caller holds, each attribute after the first in a class of the program's own that is not a singleton; a value its parameter may
take as a shape that the callee would otherwise compile once per class keeps its count, so the per-class copy is not
lost. Not for text, unions of values or a variadic list.

**What you notice.** Speed. The naive engine's stress test, whose matchers pass their own `rows` list and their
`current` row to the functions that fill and store it, and each row's components to the slot that fetches and
stores them, ticks in 30.5 ms instead of 36.0 as one C file (36.7 instead of 42.3 split); its physics step takes
12.0 ms instead of 12.5.

### Storing an object into a list counts it only when it changes the slot

**The case:** [benchmarks/storing_an_object_into_a_list_counts_it_only_when_it_changes_the_slot](../benchmarks/storing_an_object_into_a_list_counts_it_only_when_it_changes_the_slot/).

**What it does.** `list[index] = crate`, where the function holds `crate` for the whole call (its own parameter, or
a local it owns), is written in place instead of calling the list's `set_at`: the index is checked, the slot's
object is compared with the new one, and only when they differ is the new one counted, stored, and the old one let
go. A store of the object the slot already holds, the common case of reading an item, changing its attributes and
writing it back from another function, then changes no count at all.

```gdscript title=stored_in_place/rack.spite
var crates = List<Crate>()

func Rack() {
    var index = 0
    while index < 3 {
        var empty = Crate()
        crates.append(empty)
        index = index + 1
    }
}

func place(at: Integer, crate: Crate) {
    crates[at] = crate
}

func total(): Integer {
    return crates.sum(weight_of)
}

func weight_of(crate: Crate): Integer {
    return crate.weight
}
```
```gdscript title=stored_in_place/crate.spite
var weight = 0
```
```gdscript title=stored_in_place/stored_in_place.spite entry
var console = Console()
var rack = Rack()

func StoredInPlace() {
    var first = Crate()
    first.weight = 5
    var second = Crate()
    second.weight = 7
    rack.place(0, first)
    rack.place(1, second)
    rack.place(0, first)
    rack.place(1, first)
    rack.place(2, second)
    var total = rack.total()
    console.print("total", total)
    rack.place(0, second)
    rack.place(1, second)
    var again = rack.total()
    console.print("again", again)
}
```
```output
total 17
again 21
```

`place` compiles its store to:

```c
{ List_Crate* list = self->crates_; int32_t at = at_; Crate* stored = crate_; if (__builtin_expect(at < 0 || at >= (list)->item_count_, 0)) spite_outside_list("crates[at]", ...); Crate* old = ((Crate**)(intptr_t)(list)->items_)[at]; if (old != stored) { ((Crate**)(intptr_t)(list)->items_)[at] = Crate___retain(stored); Crate___release(old); } }
```

where it was `List_Crate_set_at(self->crates_, at_, Crate___retain(crate_))`, which let go of the old crate, counted
the new one again and let go of its own parameter: four counts per store, all on one object when the slot already
held it. Whether the slot holds the same object is known only while the program runs (the compiler has no way to see
that `place(0, first)` stores what an earlier call put there), so this is a test the run makes; it replaces calls the
store made anyway, and when the objects differ it makes two counts where it made four. The new object is stored
before the old one is let go, so a `drop()` the old object runs sees the list already holding the new one.

**When.** Every build but `--hot-reload`, `--repl`, `--repl-port`, `--development` and the resumable copy of a
function a `Concurrent` runs, in a class of the program's own, for a `List` named by a path of names and attributes,
at an index that is a name, a number or a sum or difference of those, whose item is a class or a list (not text, a
union, a nullable item or a function value), stored from a name the function holds. Not for a value that is made or
computed in the store itself (`crates[at] = Crate()`, `crates[at] = pick()`), which the list's `set_at` takes as it
is, nor for a `Dictionary`.

**What you notice.** Speed, and a different report for an index outside the list: `crates[at]` is named as the read
outside the list, where `set_at`'s own check reported it before. The case's ten million stores take 9.8 ms instead of
39.6. The naive engine's runner writes each component back into its column with `values[row] = value` after a
system changed it in place, and its stress tick goes from 38.0 to 33.3 ms as one C file (45.7 to 43.3 split; medians
of nine on a busy machine).

### An item passed to a call that cannot change its list is not counted

**The case:** [benchmarks/an_item_passed_to_a_call_that_cannot_change_its_list_is_not_counted](../benchmarks/an_item_passed_to_a_call_that_cannot_change_its_list_is_not_counted/).

**What it does.** A list item used at once, without a name, is read from its slot without counting it: passed to
a call that cannot change the list (`length_of(tracks[first])`), read for one of its attributes
(`tracks[first].seconds`), or asked one of a list's reading functions when the item is itself a list
(`plays[at].count()`, `plays[at][0]`). The list holds the item through all of it, so a count up and down around
the use is two writes for nothing.

```gdscript title=items_used_at_once/playlist.spite
var tracks = List<Track>()
var plays = List<List<Integer>>()

func add(seconds: Integer) {
    var track = Track()
    track.seconds = seconds
    tracks.append(track)
    var heard = List<Integer>()
    heard.append(seconds / 2)
    plays.append(heard)
}

func longer_of(first: Integer, second: Integer): Integer {
    crash tracks[first]
    crash tracks[second]
    if tracks[first].seconds > tracks[second].seconds {
        return length_of(tracks[first])
    }
    return length_of(tracks[second])
}

func length_of(track: Track): Integer {
    return track.seconds
}

func times_heard(at: Integer): Integer {
    crash plays[at]
    return plays[at].count()
}
```
```gdscript title=items_used_at_once/track.spite
var seconds = 0
```
```gdscript title=items_used_at_once/items_used_at_once.spite entry
var console = Console()

func ItemsUsedAtOnce() {
    var playlist = Playlist()
    playlist.add(185)
    playlist.add(240)
    var longest = playlist.longer_of(0, 1)
    var heard = playlist.times_heard(1)
    console.print("longest", longest, "heard", heard)
}
```
```output
longest 240 heard 1
```

Each item is the slot's pointer, read after the index is checked against the list:

```c
Playlist_length_of___held_0(self, ({ List_Track* list = self->tracks_; int32_t at = first_; if (__builtin_expect(at < 0 || at >= (list)->item_count_, 0)) spite_outside_list("tracks[first]", ...); ((Track**)(intptr_t)(list)->items_)[at]; }))
List_Integer_count(({ ... ((List_Integer**)(intptr_t)(list)->items_)[at]; }))
```

where each was `List_Track_get_at` with the item counted and let go after. A call the item is passed to goes to
the copy that takes it held, and only when nothing it can reach writes into that list, shrinks or reorders it,
assigns an attribute along the list's path or its index, or lets go of what its slots hold
([the same facts](#an-item-a-name-holds-from-its-list-is-not-counted) a named item needs), so
`read_after_dropping(shelves[0])`, which removes `shelves[0]`, still counts it. An attribute read or a list's
`count`, `is_empty`, `get_at` (`[]`), `first`, `last`, `contains` or `index_of` runs nothing of the program's in
between, so it needs no facts at all.

**When.** Every build but `--hot-reload`, `--repl`, `--repl-port`, `--development` and the resumable copy of a
function a `Concurrent` runs, in a class of the program's own, for an item of a `List` named by a path of names and
attributes, at an index that is a name or a number, whose item is a class or a list (not text, a union, a nullable
item or a function value), and only where the read is already proven (narrowed, or by a bound). Not for a
`Dictionary` entry, an index that is computed, an attribute with a getter, or an item used as the receiver of any
other function.

**What you notice.** Speed. The naive engine's runner reads each candidate list of its systems with
`candidates[index].count()` and `candidates[index][picked]` for every entity, and stamps every column through
`changes.stamp_written(headers[index], entity)`: its stress tick goes from 27.3 to 25.9 ms as one C file (medians of
nine).

### A singleton's attribute that never changes is read in place

**The case:** [benchmarks/a_singletons_attribute_that_never_changes_is_read_in_place](../benchmarks/a_singletons_attribute_that_never_changes_is_read_in_place/).

**What it does.** Reading an attribute of a singleton from another class (`Column<Heat>().values[place]`,
`column.values.count()`) would count the attribute's object for the expression and, in a program where a `Parallel`
reaches the singleton, take its lock or readers' side around the read. When the attribute holds an object (a class,
list or dictionary, not text or a number) that nothing assigns after the singleton is made (no function of the
singleton outside its constructor and no other class assigns it), the object stays the singleton's for the rest of the
program, so the read is the attribute's address: no count and no lock. What is then done with the object is unchanged:
a call on it takes whatever that object's functions take, and keeping it in a name or an attribute counts it there.
This also takes the lock out of a loop that only reads through such an attribute, since there is none left to hoist.

**When.** Every build but `--hot-reload`, `--repl` and `--repl-port`, for a singleton of the program's own whose
attribute has no getter. Whether anything assigns the attribute is read from the call effects of the whole program
(`generation/call_effects.spite`), the same facts the singleton locks are decided from.

**What you notice.** Speed:
[its case](../benchmarks/a_singletons_attribute_that_never_changes_is_read_in_place/), a million boxes read four times
through a singleton's list from a `Parallel`, takes 147.8 ms, against 244.5 ms for naive C, which takes a mutex and
counts the box for every read, and 3.1 ms for expert C, which reads a column of weights; in a game engine's `stream_bench` each inline component of a row cost a
count, a lock and a release per row. Nothing a program prints or allocates changes
(`conformance/stage6/singleton_attribute_reads`: `names` is read in place from a `Parallel`, while `current`,
which a function replaces, is still read under the lock).

The call effects that both proofs above depend on are exact in two more ways. A plural call to a template of the
same class (`fill_attributes(row, found)`, `run_phases_each()`) is followed into its template, so what the
template's lines call (a reference column's `at`) counts as reached from the caller, from a `Parallel` included,
and such a column takes its lock when pool work calls it. And an attribute read through a `type`
(`moving.trail.lefts.append(...)`) is known to be the class the `type` declares for it, and assigning an attribute
that holds a plain value in every class of the program (`moving.position.left = ...`) lets go of nothing, so a
system writing its components' numbers does not count as letting go of objects.

### The fault handler is in every program

**The case:** [benchmarks/the_fault_handler_is_in_every_program](../benchmarks/the_fault_handler_is_in_every_program/).

**What it is.** The one piece of C nothing tree-shakes: a program can meet a native fault (a null read inside a
foreign library, a stack overflow) whatever it uses, and a silent end is a bug, so every
program installs a handler that reports it ([failure.md](failure.md#what-a-native-fault-reports)). It is fixed
code, not a runtime system: nothing runs until a fault, and it is written once, after every
other function, from what tree shaking kept.

**What it costs**, measured on x64 Windows with clang:

- **Code**: about 3.7 KB of machine code (3 707 bytes at `-O0`, 3 678 at `-O2`), 0.4 KB of fixed text, and its unwind
  data. Installing it is two system calls at start (three on Windows, where a vectored handler also catches a
  corrupted heap), and one more in each thread the program starts. The vectored handler runs for every
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
  is not kept to inspectable builds.
- **Frame pointers**, which the stack walk needs on Linux and macOS, are kept only where they are free or asked
  for: a build without `--optimized` has them anyway, and an inspectable build is compiled with
  `-fno-omit-frame-pointer` there. The compiler compiling itself at `-O2` took 1 847-1 879 ms without them and
  1 856-1 918 ms with them (about 1% slower, four runs each, alternating), so an `--optimized` production build does
  not keep them and its report names the faulting function without the chain. Windows walks the stack from the
  unwind data every 64-bit program carries, in every build, at no cost.
- **Stack**: 16 KB of each thread's stack on Windows, and a 64 KB alternate signal stack per thread on Linux and
  macOS, are kept back so a stack overflow can still be reported.

**When.** Every build. **What you notice.** A fault prints a report instead of nothing, and the program is a few
kilobytes larger.

### Short symbols are inline text

**The case:** [benchmarks/short_symbols_are_inline_text](../benchmarks/short_symbols_are_inline_text/).

**What it is.** A symbol whose text is 15 bytes or fewer is written into the symbol table as an inline `String`
(its bytes inside the 16-byte value, the form any short text built at run time takes) rather than as a pointer
to constant text. Reading its bytes follows no pointer, and the executable holds no separate
copy of the text.

**When it applies.** Every symbol literal and every reflection name of 15 bytes or fewer, in every build. Longer
ones stay constant text; both forms release and retain as nothing.

**What a user can observe.** Nothing: a symbol compares, prints and converts the same in either form. The C
shows it: `static SpiteString spite_symbol_4 = { (int64_t)0x00000065756c6176ULL, ... }` for `'value'`.

### A foreign name is never copied

**The case:** [benchmarks/a_foreign_name_is_never_copied](../benchmarks/a_foreign_name_is_never_copied/).

**What it does.** The first call into a foreign library opens it and looks up every function the program calls in
it, all at once. The library's file name and each function's name, and the name of the Spite function that calls it
(which a missing function's error message shows), are known while compiling, so each is a `String` pointing at text
written into the program, never copied onto the heap first. Opening `kernel32.dll` for a `Lock` and a `ThreadSlot`
used to copy nine names of more than 15 bytes, like `AcquireSRWLockExclusive` and `ThreadSlot.create_key`, and
free each one a moment later; it copies none now.

**When.** Every foreign library a program opens, in every build.

**Example.** Locking opens the system's thread library on its first call, and its names cost nothing:

```gdscript title=foreign_names/foreign_names.spite entry
var console = Console()
var guard = Lock()

func ForeignNames() {
    guard.lock()
    console.print("locked")
    guard.unlock()
}
```
```output
locked
```

**What you notice.** Fewer allocations under `--debug-memory`, where a library is opened after the allocation
table started counting: on Windows this program makes 5 allocations rather than 10 (the `Lock`, its handle, the
library and the program's two objects). On Linux every library the standard one opens is the C library, which the
allocation table itself opens before it counts, so there the counts are as they were. The C shows each name as
`((SpiteString)SPITE_STATIC_STRING("AcquireSRWLockExclusive", 23))`.

### Crash text out of the binary

**The case:** [benchmarks/crash_text_out_of_the_binary](../benchmarks/crash_text_out_of_the_binary/).

**What it is.** A `crash` or `assert` site's condition text lives only in the `<output>.crashes` map written
beside the executable: no build writes it into the program. An
`--optimized` build also leaves out each site's place, class and function, so a site is its 8-digit id and the
values its report prints; other builds keep the place, so a local run needs no lookup.

**When it applies.** Every `crash` and every program `assert` (a library `assert` records nothing).

**What a user can observe.** The report lines ([failure.md](failure.md#what-a-crash-reports)): `spite.crash<TAB>id`
and `spite.assert<TAB>id<TAB>answered=...` in an optimised build, and `grep <id> program.crashes` gives the rest.
`conformance/stage6/trace_asserts_optimized`.

### Smaller ones

**The case:** [benchmarks/smaller_ones](../benchmarks/smaller_ones/).

None of these needs anything from you:

- `join` writes every piece once into one buffer instead of copying the text so far at each step.
- Converting text to text, in `join` on a `List<String>`, is folded away.
- A `T?` of a class, list or text is the reference itself, with `null` as the absent case: no wrapper object.
- A generic singleton has one static slot per set of codegen values, so `Column<Health>()` is found without any
  lookup.
- A function passed to a template by name, `names.each(say_hello)` or `people.each(greeter.greet)`, is not made into a
  function value: the template is written once for that function and its owner, so the call allocates nothing and
  calls it directly ([collections.md](collections.md#passing-a-function-for-each-element)). Only a function held in a
  variable is called through its `Spite.Function`.
- An `assert` in `library/` writes nothing into the crash trace, decided when compiling, so a library guard costs
  what an `if` costs. What you notice: a crash report lists only the failed
  asserts of the program and its `load`-ed packages ([failure.md](failure.md#what-a-crash-reports)).
- A program with no `crash` left after tree shaking writes nothing into the crash trace at all: the trace exists
  only to be printed by a crash, so each `assert` of such a program compiles to its test and its `return`, and
  the trace's 32 entries are not in the program. A program that can crash records
  exactly as before.
- An attribute written through a local or a parameter, `item.index = 293`, is written through that local,
  `(item_)->index_ = 293;`, with no temporary holding the reference first: a local cannot change while the value
  is worked out. An object that is any other expression is still evaluated once into a temporary. Nothing a
  program can observe changes; the C is shorter, by about one line in twenty for a folder of data records.
- A foreign library is closed at exit only if the function that opens it is in the program, so a library nothing
  opens leaves neither its handle nor the code to close it in the program. `Console` opens the C library when it
  is made, since it binds its `DynamicLibrary` as an attribute: a program that only prints opens it too.

### Proofs that survive a call

**The case:** [benchmarks/proofs_that_survive_a_call](../benchmarks/proofs_that_survive_a_call/).

A proof (`assert target`, `crash list[index]`, a bound in a `while`) lets the reads after it skip the null test and
the narrowing. A call between the proof and the read keeps it unless the compiler, following the called function and
what it calls, finds that the call may assign an attribute the proof reads through or shrink a list it reads; so no
check is repeated after a call that provably cannot, and no `const` keyword is needed. To know which function a call
reaches, it reads the class of the value the call is made on: an attribute's or variable's declared type, the class a
constructor makes, the class the function that made the value returns (`var address = heap.allocate(8)` is a
`Memory.Address`), or the left side of a `+` (an address plus an offset is an address), and `List`, `Dictionary` or
`String` for a value of those types. A receiver that is an expression has its type's class too: `File(path).read()`
reaches only `File.read`, `names.copy().count()` and `lists[0].count()` only `List.count`, and an item of a `Vector`
or `Dictionary` its element's class, so a program's own `read` or `count` does not undo a proof it cannot touch. A
call on a value whose class it cannot tell (a shape, a type parameter, a function value) may reach every function of
that name, and a `clear` or `remove_...` on a list reached through `[]` or a call may be any list, so it keeps no
proof about a list. A `return`'s own calls keep every proof, since nothing after the `return` runs. It runs entirely
while compiling and emits nothing. What you can observe: a proof after a call that may change it must be written
again, and a call through a function value keeps no proof about attributes or lists
([failure.md](failure.md#a-call-may-undo-a-proof)).

### Text joined in one piece

**The case:** [benchmarks/text_joined_in_one_piece](../benchmarks/text_joined_in_one_piece/).

**What it does.** `"line {index} of {round};"` and `prefix + name + suffix` are one join, not a chain of pairs:
every piece is computed in order, left to right as written, and the text is made once, at its final length. Before,
each `+` (and each `{...}` hole) made a whole new text, so a text of five pieces made four texts and threw three
away. Pieces the compiler already knows (written text, a symbol's name such as `attribute.name` in a Symbol
walk) are joined while compiling, and empty ones are dropped, so `"{index} of"` is two pieces, not three, and
`"{attribute.name}="` is one constant.

**When.** Every `+` whose left side is text, and every text with `{...}` holes, in every build. `text = "{text}..."`
still grows `text` in place ([above](#appending-to-text-in-place)).

**What you notice.** Fewer allocations under `--debug-memory` (two per piece that a chain of joins would make:
`conformance/stage6/text_building` allocates 39 times instead of 43), and a text built only from pieces the compiler knows answers `'constant'` to `.memory.section`
instead of `'heap'`, as a written text does.

### Defaults the constructor replaces are never made

**The case:** [benchmarks/defaults_the_constructor_replaces_are_never_made](../benchmarks/defaults_the_constructor_replaces_are_never_made/).

**What it does.** A class with `var owner = Owner(0)` and a constructor whose first lines are `owner = new_owner`
would make an `Owner`, then throw it away. The object is made with that attribute empty instead, and the constructor's
line fills it: the default is never made. It applies to each attribute the constructor sets in its opening run of
lines that assign an attribute a parameter or a literal (before anything else can read it) when the default is a
construction that has no effect but the memory it takes: a `List`, a `Dictionary`, or a class with no `drop()`, not a
singleton, whose own constructor only copies its parameters and literals into its attributes and whose own defaults
are made the same way. `Spite.Class('Nothing')`, the default of every function value's `returns` and every attribute's
and argument's `class`, is one (three allocations: the class object and its two lists); a default whose constructor
prints is not.

**When.** Every build but `--hot-reload` (whose constructors can be swapped for ones that read the attribute first),
for objects made by their constructor; an object given an allocator on the next line still makes its defaults.

**What you notice.** Fewer allocations under `--debug-memory`, one per discarded object and those it holds:
[its case](../benchmarks/defaults_the_constructor_replaces_are_never_made/), which makes 500 000 items that each
replace their default `Owner`, makes 1 000 026 allocations, one per item and one per owner it is given, and none for
the defaults. Nothing else: the discarded default was never reachable.

### A function value describes its arguments when asked

**The case:** [benchmarks/a_function_value_describes_its_arguments_when_asked](../benchmarks/a_function_value_describes_its_arguments_when_asked/).

**What it does.** A function value is its own reflection object, with `.arguments`, a list of
`Spite.Argument`s. Filling that list when the value is made would cost two objects per argument, and each
argument's class, though almost no program reads it. So the value carries a pointer to a function the compiler
wrote for it, and `.arguments` fills the list the first time it is read (under a lock in a program with threads,
so two threads reading it at once see one list).

**When.** Every function value the compiler makes: `Parallel(summer.total)`, `apply(scorer.score, 3)`, a shape's
function passed on. A `.functions` list is reflection read on purpose, so its values are still described at once.

**What you notice.** Fewer allocations: with the defaults above, a `Parallel` makes 10 where it made 25
(`conformance/stage6/singleton_counts`, two `Parallel`s, went from 134 to 104), and passing a function value makes
2 instead of 10: the value and its empty list. `.arguments` answers the
same list, in the same order, whenever it is read.

**A value its callee only calls is not made at all.** When the function a value is passed to does nothing with
that parameter but call it (`func apply(change: Spite.Function<Integer, Integer>, value: Integer): Integer { return
change(value) }`), and the value is a function of an object the caller holds for the call, `apply(scorer.score,
index)`, or of the caller's own class, `apply(add_step, index)`, the value is a struct in the caller's frame that
holds only the owner and the function: no allocation, no count, no description, since nothing can ask for one
([the proof](proofs.md#a-function-value-its-callee-only-calls-lives-in-the-frame)). The call goes to the callee's
`___held_` copy, which lets nothing go, and once that copy is inlined the C compiler sees which function the value
holds and calls it directly. A callee that reads `.name` or `.arguments`, keeps the value, passes it on or names it
in text gets a value on the heap as before, and so does a value whose owner is a temporary.
`benchmarks/a_function_value_describes_its_arguments_when_asked` makes 2 000 000 such calls: 4 000 014
allocations and 66.8 times plain C's time before, 14 allocations and 0.65 of plain C's time now.

### A list's templates read its elements without counting them

**The case:** [benchmarks/a_lists_templates_read_its_elements_without_counting_them](../benchmarks/a_lists_templates_read_its_elements_without_counting_them/).

**What it does.** Every member template of `List` (`sum_price()`, `filter_is_active()`, `each(step)`, `copy()`)
and every fused chain would read each element with `values.read_value(items, index)`, which raises the element's
reference count, and lowers it again when the pass over that element ends: two writes to every object walked,
which on a list of objects spread through memory are most of the loop's cost. Instead the element is read as it
lies in the list, uncounted, whenever nothing that runs while it is held can let go of anything. The compiler
walks the rest of that pass (the member function it calls, or the function passed in, and everything those call)
and lets it borrow only when none of them assigns an attribute that holds an object (a number, `Boolean` or text
attribute is fine), removes from or replaces into a list or dictionary, or calls through a function value. An
object the pass makes and lets go of runs its `drop()` while the element is held, so when a `drop()` of the
program's own classes may do any of that, the pass borrows only if nothing it runs makes an object of the
program's own classes or copies one (`conformance/stage6/template_lend_drop`); a `drop()` somewhere else in the
program costs nothing to a pass that only reads. A
member read from a borrowed element (`map_owner`) is borrowed the same way. Anything that keeps the element
(`collected.append(item)`, `return item`) still counts it.

**When.** Every build but `--hot-reload`, in the functions of `List` (the library's templates, a program's own
templates reopening `List`, and fused chains), when the proof above holds; otherwise the element is counted as
before. `parallel_each_` passes borrow too, which saves an atomic increment and decrement per element in a
program with threads.

**What you notice.** Speed: [its case](../benchmarks/a_lists_templates_read_its_elements_without_counting_them/),
100 rounds of two template passes over 200 000 bodies spread through memory, takes 29.3 ms, against 39.9 ms for
naive C, which walks the same pointers and counts nothing, and 2.6 ms for expert C, which walks columns of numbers.
Allocations and everything a program prints are the same.

A function called on the element is looked up on the element's own class, not by its name among every class, so
`List<Particle>.each_step()` does not count its elements merely because another class, such as the library's
`ReadEvaluatePrintLoop`, has a `step` that lets go of things.

### An item written back to its own slot is not written

**The case:** [benchmarks/an_item_written_back_to_its_own_slot_is_not_written](../benchmarks/an_item_written_back_to_its_own_slot_is_not_written/).

**What it does.** A plain loop over a list often reads an item into a name, changes it, and puts it back:

```gdscript title=slot_write_back/particle.spite
var left = 0
var speed = 1

func Particle(starting_speed: Integer) {
    speed = starting_speed
}
```
```gdscript title=slot_write_back/slot_write_back.spite entry
var console = Console()
var particles = List<Particle>()

func SlotWriteBack() {
    var index = 0
    while index < 3 {
        var made = Particle(index + 1)
        particles.append(made)
        index = index + 1
    }
    var tick = 0
    while tick < 10 {
        step()
        tick = tick + 1
    }
    crash particles[2]
    console.print(particles[2].left)
}

func step() {
    var index = 0
    while index < particles.count() {
        var particle = particles[index]
        particle.left = particle.left + particle.speed
        particles[index] = particle
        index = index + 1
    }
}
```
```output
30
```

`particle` names the object the list holds, so `particle.left = ...` already changed the item, and
`particles[index] = particle` puts back the object that is already there. When nothing between the read and the
write can change that slot, the compiler writes no code for the write-back: no bounds check, no count up for the
new value and down for the old one. And when nothing from the read to the end of the block can let the item go,
the read takes no count either: `particle` is the item's address, read after the same bounds check, and is not
released when the block ends. The loop above becomes:

```c
Particle* particle_ = ({ List_Particle* list = self->particles_; int32_t at = index_;
    if (__builtin_expect(at < 0 || at >= list->item_count_, 0)) spite_outside_list("particles[index]", ...);
    ((Particle**)(intptr_t)list->items_)[at]; });
(particle_)->left_ = /* particle.left + particle.speed, overflow checked */;
index_ = (index_ + 1);
```

What may come between, and what stops it, is [the proof](proofs.md#an-item-written-back-to-its-own-slot-is-the-slot):
in short, nothing there may assign the name, the index or the path of the list, write into this list with `[]`,
remove from or reorder a list, call through a function value or wait. When one of those is there,
the write-back and the counted read are kept exactly as written, so the program means the same either way: a
`points.reverse()` between the read and the write still puts the read object back at its index.

**When.** Every build but `--hot-reload`, `--repl`, `--repl-port` and `--development`, in the program's own
classes, for a `List` item read with `[]` into a `var` in the same block as the write-back. An item of a
`Vector` or an `Items` is borrowed already and needs no write-back.

**What you notice.** Speed. The loop above over 200,000 particles, 200 passes, goes from 112 ms to 44 ms (about
2.8 ns an item to 1.1). Allocations, the order of everything a program can see and what it prints are the same.

### A list item read only to test it is not counted

**The case:** [benchmarks/a_list_item_read_only_to_test_it_is_not_counted](../benchmarks/a_list_item_read_only_to_test_it_is_not_counted/).

**What it does.** `crash list[index]`, `assert list[index]`, `if list[index]` and `if not list[index]` read an item
only to ask whether it is there:

```gdscript title=tested_items/tested_items.spite entry
var console = Console()
var names = List<String>()

func TestedItems() {
    names.append("first")
    names.append("second")
    var index = 0
    while index < 3 {
        if names[index] {
            console.print(names[index])
        } else {
            console.print("nothing at", index)
        }
        index = index + 1
    }
    crash names[1]
    var length = names[1].length()
    console.print(length)
}
```
```output
first
second
nothing at 2
6
```

Read as a value, the item would be counted up for the test and down again right after it: two writes to the
item's object, which for an object in a cache line of its own cost more than the test. Nothing runs between that
read and the test, so the compiler tests the slot itself: the index inside the list, and the slot holding an item.
`crash names[1]` becomes:

```c
if (!(({ List_String* list = self->names_; int32_t at = 1;
    (at >= 0 && at < list->item_count_) && !SPITE_STRING_IS_NULL(((SpiteString*)(intptr_t)list->items_)[at]); })))
    spite_failed_12(self);
```

In a path (`crash rows[index].owner`), the test of `rows[index]` on the way is the slot's too, and the attribute is
read as before. The read after the test
(`names[1].length()`) is an ordinary read, left uncounted only where
[the section above](#an-item-written-back-to-its-own-slot-is-not-written) proves it.

**When.** Every build, for a test of an item of a `List` of objects or text, read with `[]` from a path of names
and attributes, with an index that is a name, an attribute, a number or a sum or difference of those. An item of
a `Vector` or an `Items` is borrowed already, a list of numbers is never counted, and a `Dictionary`'s entry is
still read and counted.

**What you notice.** Speed: the naive engine's stress test, whose matcher tests three items of its rows' lists
for every entity and component, ticks in 43.8 ms instead of 48.1 (40.3 instead of 43.3 as one C file). What a
crash report says, allocations and everything a program prints are the same.

### An item a name holds from its list is not counted

**The case:** [benchmarks/an_item_a_name_holds_from_its_list_is_not_counted](../benchmarks/an_item_a_name_holds_from_its_list_is_not_counted/).

**What it does.** A name read from a list and used for a while is the commonest way a plain program looks at an
item:

```gdscript title=held_names/column.spite
var name = ""
var hits = 0

func Column(column_name: String) {
    name = column_name
}

func hit() {
    hits = hits + 1
}
```
```gdscript title=held_names/held_names.spite entry
var console = Console()
var columns = List<Column>()
var weights = List<Integer>()

func HeldNames() {
    var first = Column("position")
    var second = Column("velocity")
    columns.append(first)
    columns.append(second)
    weights.append(0)
    weights.append(0)
    var round = 0
    while round < 3 {
        visit(0)
        visit(1)
        round = round + 1
    }
    crash weights[1]
    console.print(first.hits, second.hits, weights[1])
}

func visit(index: Integer) {
    crash columns[index]
    var column = columns[index]
    column.hit()
    weights[index] = weight(column)
}

func weight(column: Column): Integer {
    return column.hits * 10
}
```
```output
3 3 30
```

`column` would count the item up when it is read and down when `visit` ends. The list holds the item the whole
time: nothing in `visit` writes `columns`, and nothing it calls can (`hit()` changes a number, `weight()` only
reads, and the write into `weights` is into a list of numbers, which can never be `columns`). So the read is the
item's address, and `weight(column)` passes it the way a caller passes a name it holds:

```c
Column* column_ = ({ List_Column* list = self->columns_; int32_t at = index_;
    if (__builtin_expect(at < 0 || at >= list->item_count_, 0)) spite_outside_list("columns[index]", ...);
    ((Column**)(intptr_t)list->items_)[at]; });
Column_hit(column_);
List_Integer_set_at(self->weights_, index_, HeldNames_weight___held_0(self, column_));
```

The proof is the [write-back's](#an-item-written-back-to-its-own-slot-is-not-written), from the read to the end of
the block ([the rule](proofs.md#an-item-a-name-holds-from-its-list-is-not-counted)). Assigning another object's
attribute is allowed between, since that lets go of another object, never of the one the slot holds. A call that
writes into a list that may be this one keeps the count: `columns[index] = other`, a function handed `columns` (or
a local naming it) that writes into what it is handed, and a generic class writing into a list of its own item,
which could be any class. So does keeping the name: assigning it, returning it, or naming it in another `var`.

**When.** Every build but `--hot-reload`, `--repl`, `--repl-port` and `--development`, in the program's own
classes, for a `List` of a class read with `[]` into a `var`.

**What you notice.** Speed: the naive engine's stress test, whose matcher and row filler hold a column's header
from a list while they look an entity up, ticks in 41.2 ms instead of 43.6 (38.1 instead of 40.0 as one C file).
Allocations, the order of everything a program can see and what it prints are the same.

### A test against a value a list never holds is decided while compiling

**The case:** [benchmarks/a_test_against_a_value_a_list_never_holds_is_decided_while_compiling](../benchmarks/a_test_against_a_value_a_list_never_holds_is_decided_while_compiling/).

**What it does.** A list a program fills with a few known values, and from then on only reads, is a table: a
pipeline's steps, a tokenizer's character classes, the kinds a matcher checks. Every test of one of its items
against a value it can never hold is answered while compiling, so the branch behind it costs nothing:

```gdscript title=listed_steps/pipeline.spite
enum Step {
    'discount'
    'tax'
    'rounding'
}

var steps = List<Step>()

func add(step: Step) {
    steps.append(step)
}

func price(amount: Integer): Integer {
    var total = amount
    var index = 0
    while index < steps.count() {
        if steps[index] == 'discount' {
            total = total - total / 10
        }
        if steps[index] == 'tax' {
            total = total + total / 5
        }
        if steps[index] == 'rounding' {
            total = total / 100 * 100
        }
        index = index + 1
    }
    return total
}
```
```gdscript title=listed_steps/listed_steps.spite entry
var console = Console()

func ListedSteps() {
    var checkout = Pipeline()
    checkout.add('discount')
    checkout.add('tax')
    var total = checkout.price(1250)
    console.print("total", total)
}
```
```output
total 1350
```

Only `add` puts anything into `steps`, and every call of `add` in the program passes `'discount'` or `'tax'`, so
no item is ever `'rounding'`. The third test is `false` without looking, and the C compiler drops the branch:

```c
if ((((void)((({ ... List_Pipeline_Step_get_at(self->steps_, index_) ... }) == Pipeline_Step_rounding)), 0))) {
```

The read stays, so an index outside the list still halts as it would have; only the answer is known. The values a
list can hold come from the program's own code: a whole number or an enum value written out, an item of another
such list, a parameter of a function whose every call the compiler sees, a local (everything assigned to it), or
what a function returns, following the branches a codegen question decides, so `Slot<Position>`'s
`if $slot_type.has_function("tracking_kind") { ... } return 0` returns 0. The same test of such a parameter or
local folds too. [Proofs](proofs.md#a-list-only-its-class-fills-holds-only-what-it-fills) has the rule.

**When.** Every build but `--hot-reload`, `--repl`, `--repl-port` and `--development`, for a list of whole numbers
or of an enum declared `var name = List<T>()` in one of the program's own classes, tested with `==` or `!=`
against a value written out. Not for a list anything else can reach (`var alias = steps`, a function handed it that
appends to it), a list filled with a value the compiler cannot list (`index * 2`, a value read from a file), a
parameter some call reaches in a way it cannot follow (a function value, a union, `Concurrent`, a class-level
function), a class whose attributes reflection walks, or a `switch`.

**What you notice.** Speed, and nothing else: what the test would have answered is what it answers. The naive
engine's stress test, whose matcher tests each column's tracking kind against every kind it knows while its rows
only ever hold two of them, ticks in 35.4 ms instead of 39.0 as one C file (41.3 instead of 41.9 split), and its
physics step takes 8.9 ms instead of 9.2. A function only a folded branch calls is still in the C, since the branch
is removed by the C compiler and not before tree shaking.

### A table filled once is read as constants

**The case:** [benchmarks/a_table_filled_once_is_read_as_constants](../benchmarks/a_table_filled_once_is_read_as_constants/).

**What it does.** When the values of a table are known while compiling and so are their number and their order
(written out in the function that sets the object up), the functions that read the table get a copy in which the
table is a constant: its count is a number and each item is the value it holds. A test of the object when one of
those functions is called sends the call to the copy, where the C compiler unrolls the loop over the table and keeps
only the branches the values take:

```gdscript title=filled_tariff/tariff.spite
enum Step {
    'discount'
    'tax'
    'rounding'
}

var steps = List<Step>()

func Tariff() {
    steps.append('discount')
    steps.append('tax')
    steps.append('discount')
}

func price(amount: Integer): Integer {
    var total = amount
    var index = 0
    while index < steps.count() {
        if steps[index] == 'discount' {
            total = total - total / 10
        }
        if steps[index] == 'tax' {
            total = total + total / 5
        }
        if steps[index] == 'rounding' {
            total = total / 100 * 100
        }
        index = index + 1
    }
    return total
}
```
```gdscript title=filled_tariff/filled_tariff.spite entry
var console = Console()

func FilledTariff() {
    var tariff = Tariff()
    var total = tariff.price(1250)
    console.print("total", total)
}
```
```output
total 1215
```

`Tariff()` puts three steps into `steps`, one after the other, and nothing else ever puts one in or takes one out,
so a `Tariff` that was made holds `'discount'`, `'tax'`, `'discount'`. `price` is written twice in the C: once as
you wrote it, and once with `steps` read from a constant list of those three values. The function you call
starts with the test that picks:

```c
int32_t Tariff_price(Tariff* self, int32_t amount_) {
if (self->steps_->item_count_ == 3 && ((Tariff_Step*)(intptr_t)self->steps_->items_)[0] == Tariff_Step_discount
    && ((Tariff_Step*)(intptr_t)self->steps_->items_)[1] == Tariff_Step_tax
    && ((Tariff_Step*)(intptr_t)self->steps_->items_)[2] == Tariff_Step_discount) return Tariff_price___configured_0(self, amount_);
/* price as written */
}
```

The test is a few reads of the object's own memory, made once per call from outside: a function the copy calls on
the same object, when it reads the table too, has a copy of its own and is called directly. The copy is only taken
when the table holds exactly what the copy was written for, so a table that holds something else (an object whose
setup has not run, a value the compiler listed but this object does not hold) runs the function as written, and
what it computes is the same either way. Where an item can be one of a few values (a step chosen by a parameter of
the setup, a local set in a branch), there is one copy for each combination they can make, up to four; past four,
or when an item's values are not known at all, the copy keeps only the count. [Proofs](proofs.md#a-table-filled-once-holds-what-its-setup-put-in)
has the rule.

**When.** Every build but `--hot-reload`, `--repl`, `--repl-port` and `--development`, for a list declared
`var name = List<T>()` in one of the program's own classes, that [the rule for a list only its class fills](#a-test-against-a-value-a-list-never-holds-is-decided-while-compiling)
lets nobody else reach, that nothing ever shrinks (`clear()`, `truncate`, the `remove_` forms), filled only with
`append`, each at the top level of its function, from one function that runs once for each object: its constructor,
or a function that starts with `assert not prepared` and then `prepared = true`, directly or through functions of
its own that it calls at their top level. The items' values are known for a list of whole numbers or of an enum
(the copy reads them as constants); for a list of anything else only the count is, and only beside a table whose
items are known (a copy that knew only counts would grow the program for little). In a program that runs threads,
only for a singleton or a class no other thread counts. Not for a function that a function writing the list could
reach, nor, when a function on the way to a writer is passed as a value, for one that calls a function value; nor
for a lock's wrapper of a singleton's function (the copy is taken inside the lock).

**What you notice.** Speed, and a bigger executable: each function copied is in the C once more for each
combination. The case's ten million prices take about as long either way (28 ms, measured while the machine was
in other use): there the C compiler already keeps the three steps' reads out of the loop, and the copy only saves the
loop over them.
The naive engine's stress test, whose matchers' kinds and keys are filled once by `prepare()`, ticks in about 7%
less time (also measured while the machine was in other use); its C grows by 20% and its executable by 13%, for 440
copies of the functions of its six matchers and their runners, four combinations each, of which this program runs
one. In a build of several [translation units](#the-c-is-compiled-in-parallel-units-and-cached) each unit has its
own copy of the constant lists, so the C compiler folds them in every unit.

### A number joined into text is written in place

**The case:** [benchmarks/a_number_joined_into_text_is_written_in_place](../benchmarks/a_number_joined_into_text_is_written_in_place/).

**What it does.** `"line {index} of {round};"` would turn `index` and `round` into texts of their own (two
allocations each) only to copy them into the result and free them. An `Integer` or `Long` piece of a text join,
or of an append in place (`text = "{text}{count}"`), is instead written as digits into a buffer in the function's own
frame and copied from there: the same digits the library's `to_string()` writes, and no allocation. A program that
reopens `Integer` or `Long` with a `to_string()` of its own keeps calling it.

**When.** Every build, for whole numbers of those two classes. A number cast to text on its own (`var key: String =
index`) still makes one text, since that text is the result.

**What you notice.** Fewer allocations, and speed: `conformance/stage6/text_building` went from 39 to 35, and
[its case](../benchmarks/a_number_joined_into_text_is_written_in_place/), two million lines
`"line {index} of {round};"`, takes 84.3 ms, against 350.7 ms for naive C, which makes each number a text of its
own with `snprintf`, and 16.8 ms for expert C, which writes each line into one buffer on the stack.

### Allocation is the C library's, counted only where read

**The case:** [benchmarks/allocation_is_the_c_librarys_counted_only_where_read](../benchmarks/allocation_is_the_c_librarys_counted_only_where_read/).

**What it does.** Every Spite object is made with `SPITE_MALLOC` and let go with `SPITE_FREE`, and what those are is
decided per program. In an ordinary build they are the C library's `malloc`, `realloc` and `free`, with nothing beside
them: no counter, no table, no list of kept blocks. A program that reads `Memory.Heap.live_allocations()` (or
`Program.live_allocations()`, which asks it) gets a counter beside each call instead (atomic in a program that starts
a thread), and the tree shaker decides which: the counter is written only when `live_allocations` is still in the
program after shaking. The same counted allocator adds and subtracts each block's usable size for `live_bytes()`,
which keeps `live_allocations` for that purpose, so a program that reads either pays for both: a usable-size lookup
and an atomic add per allocation and free. A `--debug-memory` build routes every call through its allocation table
instead, and only that build's C has the table.

**When.** Every build but `--debug-memory`. An inspectable build (`--development`, `--hot-reload`, `--repl`) is
not shaken, so it counts.

**What you notice.** Nothing: `live_allocations()` answers the same wherever it is called. `examples/hello`'s C
is 922 lines with this, the crash trace and the foreign library changes in [Smaller ones](#smaller-ones),
against 1 356 without them.

**Freed small blocks are not kept by size.** Keeping each freed object of up to 256 bytes on a per-thread
list for its size, for the next allocation of that size, was tried and is not done (objects are kept by class
instead, [below](#objects-of-one-class-sit-together)): it was not a clear,
repeatable gain on a game engine, the program it was for. Against the plain C allocator, `clang -O2`, best of
nine interleaved runs on one machine: `examples/stress` ticks 42.9 ms with it and 41.9 without in parallel,
49.0 and 50.9 single-threaded, 60 ticks after despawning 87.7 and 87.2 ms; only the 200 000 spawns (449 and 504
ms) and `flex_layout` (about 3 ms of 70) were faster; the Vulkan UI tests (`click_counter_test`,
`text_field_test`) did not move beyond noise. It sped up small programs that make and drop objects in a loop, at
the cost of up to 2 MB kept per thread.

### Objects of one class sit together

**The case:** [benchmarks/objects_of_one_class_sit_together](../benchmarks/objects_of_one_class_sit_together/).

**What it does.** Every object of a class made and let go on the program's own thread is made from that class's own
pool: blocks the size of one object, side by side, handed out in order, and taken back by the class when an object is let go, for its next
object. The C library's allocator puts each object wherever a block of its size is free, so objects of different
classes made in turn (an entity's position, then its velocity, then its health) end up interleaved, and a loop over
one class's list touches a new cache line for every object. From their class's pool, the positions sit side by
side, four 16-byte objects to a 64-byte line, and the loop reads its memory in order. A class made and let go over
and over, such as the links of a chain made and dropped each round, is made and given back with a few plain
writes instead of a call into the C library each time.

```gdscript title=nearby_points/point.spite
var across = 0
var down = 0

func Point(starting_across: Integer, starting_down: Integer) {
    across = starting_across
    down = starting_down
}
```
```gdscript title=nearby_points/note.spite
var text = ""

func Note(starting_text: String) {
    text = starting_text
}
```
```gdscript title=nearby_points/nearby_points.spite entry
var console = Console()
var points = List<Point>()
var notes = List<Note>()

func NearbyPoints() {
    var index = 0
    while index < 1000 {
        var point = Point(index, index * 2)
        points.append(point)
        var note = Note("made")
        notes.append(note)
        index = index + 1
    }
    var total = 0
    var position = 0
    while position < points.count() {
        var point = points[position]
        total = total + point.across + point.down
        position = position + 1
    }
    var noted = notes.count()
    console.print("total", total, "notes", noted)
}
```
```output
total 1498500 notes 1000
```

A point and a note are made in turn, a thousand times. Each `Point` comes from `Point`'s pool and each `Note` from
`Note`'s, so the loop that sums the points reads one run of points after another and never a note. A point let go
goes back to `Point`'s pool, and the next `Point` made takes its place.

No list has to hold a class's objects for it to have a pool. Each round below makes a chain of a hundred links and
lets it go: every `Link` after the first round is one a previous chain gave back, and nothing is asked of the C
library at all.

```gdscript title=chain_links/link.spite
var value = 0
var next: Link? = null

func Link(new_value: Integer, new_next: Link?) {
    value = new_value
    next = new_next
}
```
```gdscript title=chain_links/chain_links.spite entry
var console = Console()

func ChainLinks() {
    var total = 0
    var round = 0
    while round < 100 {
        var head = Link(0, null)
        var index = 1
        while index < 100 {
            var added = Link(index, head)
            head = added
            index = index + 1
        }
        var current: Link? = head
        while current {
            total = total + current.value
            current = current.next
        }
        round = round + 1
    }
    console.print("total", total)
}
```
```output
total 495000
```

A pool starts with room for 16 objects and doubles the room it takes each time it runs out, until a run of
blocks is at least 256 KiB, each run starting on a cache line. Taking an object is a read of the last one given back, or the next free block; giving
one back is two writes. Nothing else runs: there is no collector and no table.

**When.** Production builds, ordinary or `--optimized`, that allocate with the C library's `malloc` and `free`:
not a `--debug-memory` build, whose table names every object, not a program that reads
`Memory.Heap().live_allocations()` or `live_bytes()`, which count the C library's blocks, and not an inspectable
build (`--repl`, `--repl-port`, `--hot-reload`, `--development`). A class qualifies when it is not a singleton and
frees its objects through its own release alone. One that holds memory of its own (an attribute that is a
`Memory.Address`, as a `List`, a `Vector`, an `Items` and a `Dictionary` do) qualifies only when the program keeps
its objects in a list (a `List`, or anything built on `TypedMemory<T>` the same way, such as a `Dictionary`'s
values): a container's own object is read beside its items, and taken from a pool it slowed `particles` by a tenth.
In a program with threads, a class qualifies only when no code that can run on another thread counts, makes or
frees one of its objects ([Proofs](proofs.md#objects-of-a-class-made-on-one-thread)); a pool has no lock.

**What you notice.** Speed, and less memory: a pooled object carries no block header of the C library's. In a game
engine's stress test (200 000 entities moved by two systems) a tick went from 64.0 to 47.6 ms (60.5 to 43.2 built
as one C file), sixty ticks after despawning every entity from 113 to 21 ms, spawning them from 163 to 98 ms, and
the program's peak memory from 96 to 67 MB; a physics step of 5 000 characters from 9.5 to 8.5 ms.
[allocation_is_the_c_librarys_counted_only_where_read](../benchmarks/allocation_is_the_c_librarys_counted_only_where_read/),
which makes and drops a chain of 1 000 links 2 000 times, went from 81 to 17 ms, and
[defaults_the_constructor_replaces_are_never_made](../benchmarks/defaults_the_constructor_replaces_are_never_made/)
from 30 to 7 ms. The cost is
that memory a class used stays that class's: an object given back is kept for the next object of its class, never
for another class and never returned to the system while the program runs. A program that makes a million objects
of one class, lets them all go and then makes a million of another holds room for both.
`conformance/stage6/class_pools` is a program whose C `check.sh` reads, built `--optimized` and run.

### A list held only by another list lives in its slot

**The case:** [benchmarks/a_list_held_only_by_another_list_lives_in_its_slot](../benchmarks/a_list_held_only_by_another_list_lives_in_its_slot/).

**What it does.** A `List<List<T>>` holds a reference to each list in it, so every inner list is an object of its
own somewhere on the heap, and reading `groups[length]` reads a pointer from the outer list's block and then the
inner list's object behind it. When nothing in the program ever names an inner list except through its slot, the
compiler keeps each inner list's whole object inside the outer list's block instead: the slot is the list.

```gdscript title=words_by_length/words_by_length.spite entry
var console = Console()
var words = ["sea", "a", "tide", "of", "harbour", "is", "salt"]
var groups = List<List<String>>()

func WordsByLength() {
    var size = 0
    while size < 8 {
        var made = List<String>()
        groups.append(made)
        size = size + 1
    }
    var index = 0
    while index < words.count() {
        var word = words[index]
        var length = word.length()
        crash groups[length]
        groups[length].append(word)
        index = index + 1
    }
    var shown = 0
    while shown < groups.count() {
        var group = groups[shown]
        var count = group.count()
        if count > 0 {
            console.print(shown, "letters:", count)
        }
        shown = shown + 1
    }
}
```
```output
1 letters: 1
2 letters: 2
3 letters: 1
4 letters: 2
7 letters: 1
```

Each `List<String>` of `groups` is stored in `groups`' own block, one after the other. `groups.append(made)` moves
`made` into its slot (its count, its room and its block of items) and lets go of the empty object `made` leaves
behind; `groups[length]` and `group` are the address of the slot, with no pointer to follow; removing an item,
clearing `groups` or letting it go frees the inner list's items where it lies. Swapping, removing, inserting and
reversing items of `groups` move the slots themselves.

**When.** Every build but the inspectable ones, decided for each element type: every `List<List<T>>` of the
program for one `T` keeps its lists in its slots, or none does. It applies when, everywhere in the program, each
list put into one is made empty for it (`List<T>()`, then changed only through its own name, and never named again
after the line that puts it in), and each item read from one is only tested (`crash groups[length]`), used at once
as the receiver of one of its own functions (`groups[length].append(word)`), or given one name that is used only
that way, until no line can grow or shrink the list of lists ([the proof](proofs.md#a-list-held-only-in-a-slot-of-another-list)).
The list of lists itself is only named by the path it was made in: never given a second name, passed or returned.

**What you notice.** Speed: in [its case](../benchmarks/a_list_held_only_by_another_list_lives_in_its_slot/), 400 000
orders grouped by customer into 65 536 lists ten times and asked 400 000 questions each time, and in a game engine's
physics step, whose broad phase keeps its colliders in a grid of lists. What a program prints and every crash report
are the same, and so are `--debug-memory`'s allocations and frees, but for a list of lists filled again
([below](#a-list-of-lists-filled-again-keeps-each-lists-room)): the object `List<T>()` makes is still made and let go.

**When it does not apply.** The list of lists keeps references, as before, when an inner list is put in from
anywhere but a fresh `List<T>()`, read and kept, passed, compared, returned or named twice, used past a line that
may grow or shrink the list of lists, or written with `groups[index] = list`; when the list of lists is passed,
returned or given a second name; when the program asks it anything else than `count`, `is_empty`, `append`,
`prepend`, `insert`, `[ ]`, `get_at`, `first`, `last`, `remove_at`, `remove_swapping`, `truncate`, `swap`, `reverse`
or `clear`; when a `Dictionary` or `Items` holds lists of the same `T`; and when the compiler writes code of its own
that reads such lists (a deep copy, reflection, serialisers).

### A list of lists filled again keeps each list's room

**The case:** [benchmarks/a_list_of_lists_filled_again_keeps_each_lists_room](../benchmarks/a_list_of_lists_filled_again_keeps_each_lists_room/).

**What it does.** A list of lists whose lists live in its slots ([above](#a-list-held-only-by-another-list-lives-in-its-slot))
is often cleared and filled again with empty lists, every pass of a program: a grid of buckets, an index by key, the
groups of a report. Each slot already holds a block of items its list grew into, so clearing the list of lists lets
go of each list's items but keeps its block where the slot is, and the next empty list put in that slot takes the
block instead of growing from nothing.

```gdscript title=letters_twice/letters_twice.spite entry
var console = Console()
var monday = ["ant", "bee", "asp", "cat"]
var tuesday = ["bat", "cow", "cod", "ape"]
var groups = List<List<String>>()

func LettersTwice() {
    group(monday)
    group(tuesday)
}

func group(words: List<String>) {
    groups.clear()
    var made = 0
    while made < 3 {
        var letters = List<String>()
        groups.append(letters)
        made = made + 1
    }
    var index = 0
    while index < words.count() {
        var word = words[index]
        var letter = word.code_at(0) - 97
        crash groups[letter]
        groups[letter].append(word)
        index = index + 1
    }
    crash groups[0]
    crash groups[1]
    crash groups[2]
    var a_words = groups[0].count()
    var b_words = groups[1].count()
    var c_words = groups[2].count()
    console.print("a", a_words, "b", b_words, "c", c_words)
}
```
```output
a 2 b 1 c 1
a 1 b 1 c 2
```

The second `group` finds the three slots `groups.clear()` left with their blocks, and `groups.append(letters)` puts
each new list in its slot with that block: `groups[letter].append(word)` writes into memory the first call already
had. A slot keeps its block until a list is put there again or the list of lists is let go, which frees it; putting
in a list that already has items of its own, or inserting before the end, frees the kept block first.

**When.** Wherever a list of lists keeps its lists in its slots, and its block grows only by appending (the
library's own growth, never `reserve`): every build but the inspectable ones.

**What you notice.** Speed, and fewer allocations: `--debug-memory` counts one allocation fewer for each slot whose
block is taken again (`conformance/stage6/nested_lists` makes 84 instead of 94), and every one is still freed. In
[its case](../benchmarks/a_list_of_lists_filled_again_keeps_each_lists_room/), 200 000 orders grouped into 32 768
lists forty times, the work takes 0.34 of the time it took with the lists in their slots alone; the naive engine's
physics grid sorts its colliders in 448 µs instead of 691. The cost is memory: a list of lists cleared and filled
with fewer lists than before keeps the other slots' blocks until it is filled that far again or let go.

### A dictionary hashes a key once, cheaply

**The case:** [benchmarks/a_dictionary_hashes_a_key_once_cheaply](../benchmarks/a_dictionary_hashes_a_key_once_cheaply/).

**What it does.** A dictionary hashes a key with a multiply and an exclusive or per character on an
`UnsignedLong` (FNV-1a), keeps 31 bits of that hash in the slot beside the key's position, and
compares key texts only when those bits match, where hashing with a multiply and a division by a prime for every
character and comparing the whole key text on a hit is slower.

**A key tested and then read is looked up once.** `if scores[key] { total = total + scores[key] }` keeps what the
test found, when the values are numbers, `Boolean`s or enum values, and the read inside the block is that value,
with no second lookup. It is kept only while nothing can change it: from the test to the first statement that calls
anything, uses an operator on an object, assigns anything but a plain name, gives a new value to a name the key or
the dictionary is written with, or starts a loop; from there on each read looks the key up again, as it reads.

**When.** Every `Dictionary`, in every build. **What you notice.** Speed:
[its case](../benchmarks/a_dictionary_hashes_a_key_once_cheaply/), two million lookups by name in a dictionary of
5 000 names, takes 102.6 ms, against 99.6 ms for naive C, a chained table with `hash * 31 + character` and `strcmp`,
and 30.8 ms for expert C. Keys, values and their order are the same, and so is every allocation: the slot
table is one block, twice as large.

### A deep copy is written per class, with a table only where a graph needs one

**The case:** [benchmarks/a_deep_copy_is_written_per_class_with_a_table_only_where_a_graph_needs_one](../benchmarks/a_deep_copy_is_written_per_class_with_a_table_only_where_a_graph_needs_one/).

**What it does.** The compiler writes one deep copy function per class it is used on. A class whose attributes can
lead back to itself, lead to a `Weak`, or that some `Weak` holds, gets a copy that looks each object up in a table
of the objects copied during that `deep_copy()` call, so an object reached twice is copied once and the copy keeps
the original's cycles and weak references ([memory.md](../specs/memory.md#the-memory-model)). Every other class gets a plain
copy: allocate, copy each attribute, return, with no table and no lookup.

**When.** Every build, for each class `deep_copy()` reaches. **What you notice.** A tree or a list of plain records
copies as fast as before. A copy of a graph allocates its table once per outermost `deep_copy()` call, outside the
counted allocations `--debug-memory` reports, and frees it before `deep_copy()` returns. A program that never deep
copies a class that needs the table carries none of it.

### A deep copy nothing changes is the original

**The case:** [benchmarks/a_deep_copy_is_written_per_class_with_a_table_only_where_a_graph_needs_one](../benchmarks/a_deep_copy_is_written_per_class_with_a_table_only_where_a_graph_needs_one/).

**What it does.** A copy is only worth making if something tells it from the original: a write to one that the
other must not see, or a question about which object it is. When the compiler proves that nothing writes the copy
or the original for as long as the copy lives, and that nothing in the program asks one of the copied classes for
its identity, `var copies = orders.deep_copy()` makes nothing: `copies` is `orders`, counted once more, and is let
go at the end of its block like any copy ([Proofs](proofs.md#a-copy-nothing-changes-is-the-original)).

```gdscript title=shared_copies/order.spite
var total = 0

func Order(new_total: Integer) {
    total = new_total
}
```
```gdscript title=shared_copies/shared_copies.spite entry
var console = Console()
var orders = List<Order>()

func SharedCopies() {
    var index = 0
    while index < 3 {
        var order = Order(index * 10)
        orders.append(order)
        index = index + 1
    }
    var sum = 0
    var round = 0
    while round < 4 {
        var copies = orders.deep_copy()
        sum = sum + copies.sum_total()
        round = round + 1
    }
    console.print("sum", sum)
}
```
```output
sum 120
```

Each round only reads its copy, so no round makes one: the four copies of three orders are the list itself. The
copy is decided per place it is made, so the same class can be copied for real in another function that changes
its copy.

**When.** Every build but the inspectable ones, in a program that starts no thread and no `Concurrent`, for a copy
the proof holds for. The decision about identity is made once the whole program is written out, so a comparison
of the copied class anywhere keeps every copy of it real.

**What you notice.** Fewer allocations under `--debug-memory`: none for the copy. Nothing else: what the program
prints, and every value it reads through the copy, is the same.

### A word inflected while compiling

**The case:** [benchmarks/a_word_inflected_while_compiling](../benchmarks/a_word_inflected_while_compiling/).

**What it does.** `pluralize()` or `singularize()` called on a text literal of lower-case letters and underscores
(`"cactus".pluralize()`) is worked out while compiling, through the same rules and the same
[`String.Inflection`](../specs/standard_library.md#the-string-class) table the call would read at run time, and the program
holds the answer as a constant (`"cacti"`): no call, no table read, no allocation.

**When.** Every build, for a literal receiver of lower-case letters and underscores, while the program's
`String.Inflection` table is the library's own. A literal with capitals, spaces or other characters, a receiver that
is not a literal, and every call in a program that reopens `String.Inflection` with other words run at run time.
**What you notice.** Nothing but speed and size: the answer is the one the call gives, and a program whose only
inflections are literals carries none of the inflection code or its table (`conformance/stage6/inflection_fold`
makes 5 allocations, against 19 when the calls run).

### A dictionary written out and only read by literal keys is folded

**The case:** [benchmarks/a_dictionary_written_out_and_only_read_by_literal_keys_is_folded](../benchmarks/a_dictionary_written_out_and_only_read_by_literal_keys_is_folded/).

**What it does.** A local made from a dictionary literal whose keys and values are all literals of one kind, and that
the rest of its block only ever reads with a literal key (`plurals["cactus"]`), is never made: each read is replaced
by the value written for that key, or by `null` for a key the literal does not have. The read keeps its type, a
`T?`, so `crash plurals["cactus"]` and `if` narrow it as before. The keys no read names cost nothing, since no
dictionary exists to hold them.

**When.** Every build except `--hot-reload` and the inspectable ones, for a local whose literal has only literal
entries of one kind and whose every later mention in its block is a read by a literal key. Any other use (a
variable key, a write, passing it, a loop over it, `count()`) keeps the dictionary as written, and so does a
dictionary held in an attribute, which reflection could write. **What you notice.** No dictionary, no allocation and
no hashing for the folded ones (`conformance/stage6/dictionary_folding`), and nothing else: each read answers what
the dictionary would.

### A dictionary keyed by numbers hashes the numbers

**The case:** [benchmarks/a_dictionary_keyed_by_numbers_hashes_the_numbers](../benchmarks/a_dictionary_keyed_by_numbers_hashes_the_numbers/).

**What it does.** A `Dictionary` keyed by a whole-number type (`Dictionary<Integer, Monster>`,
[collections.md](collections.md#any-type-is-a-key)) is compiled as its own form of `library/dictionary.spite`, whose
bodies fold on the key's type as `Items` folds on `is_fixed_size`: its keys are a `List` of the numbers, a key is
hashed by one multiply (Knuth's 6364136223846793005) instead of a loop over characters, and no `String` is made for a
key, in a lookup or in the table. Where a text key keeps 31 bits of its hash in the slot beside the key's position, a
key of 32 bits or fewer (`Integer`, `Short`, `Tiny`) keeps the key itself there, so a probe compares the slot with the
key and never reads the list of keys; a `Long` key keeps the hash bits as text does, and is compared with the list
only when they match. A key of any other type that is not text (a fraction, a `Boolean`, an enum value, an object)
is hashed the same way from its bits, an object's being its address, and compared with the list when the bits in
the slot match. A text-keyed dictionary compiles to the code it always did.

**When.** Every build, for each dictionary whose key type is a whole number. The type is written, so the form is
chosen where the dictionary type is, and the program is compiled once.

**What you notice.** Speed and allocations:
[its case](../benchmarks/a_dictionary_keyed_by_numbers_hashes_the_numbers/), four million lookups in a dictionary of
100 000 number keys, takes 39.0 ms, against 18.2 ms for naive C, a chained table with the key modulo the bucket
count, and 17.2 ms for expert C, an open-addressed table made once at its size; the whole program
[number_dictionary](../benchmarks/number_dictionary/) measures the same against a C programmer's open-addressed
table. `conformance/stage6/number_keys` pins its 137 allocations, with a thousand number keys making none.
`keys()` answers the numbers.

### Reading through a `type` without counting

**The case:** [benchmarks/reading_through_a_type_without_counting](../benchmarks/reading_through_a_type_without_counting/).

**What it does.** A system's `moving.position.left = moving.position.left + moving.velocity.across` reads `position`
through the `type` `Moving`, which answers the component retained (or, when the value's class has no such attribute, a
fresh default), and the component is released as soon as the number is read. When that component only has a number,
`Boolean` or other plain attribute read or written, the compiler asks the `type` for the component as it lies in the
value, uncounted: a read of a class without the attribute answers the attribute's default, as the fresh default object
would have, and a write to one lands in a scratch object in the frame, as it would land in a default object that was
then thrown away. A write borrows only when computing the value it stores can let go of nothing (the same proof as [a
list's templates](#a-lists-templates-read-its-elements-without-counting-them)), and only when the value holding the
component is itself held for the whole statement. When the whole program admits only one class to the `type` (a
row of borrowed items is often the only thing a system is handed), asking for the component is reading that class's
attribute, with no test of the value's class: the test read the class from the value after every write through a
component, since the C compiler cannot tell a component's numbers from the value's class.

**When.** Every build but `--hot-reload`, for a plain attribute of a class read through a `type` attribute:
`moving.position.left`, not `moving.position` passed on or kept.

**What you notice.** Speed, in systems that walk components through a `type`: their functions no longer count
anything. [Its case](../benchmarks/reading_through_a_type_without_counting/), 200 ticks over 100 000 entities, takes
26.6 ms, against 9.6 ms for naive C and 5.5 ms for expert C (38.3 ms with the class tested at every read, measured
on the same loaded machine). Allocations and results are the same.

### A row of borrowed items lives in the frame

**The case:** [benchmarks/a_row_of_borrowed_items_lives_in_the_frame](../benchmarks/a_row_of_borrowed_items_lives_in_the_frame/).

**What it does.** An object literal of borrowed `Vector` items (a row,
[memory.md](memory.md#a-row-of-borrowed-items-for-one-call)) is not allocated: it is a struct in the frame of the
function that makes it, its header's count set once and never touched, and its attributes the items' addresses,
uncounted. The function it is passed to is compiled a second time for that call, as `<name>___lent_<positions>`, in
which reading an attribute of the row is the `type`'s uncounted read (as in [Reading through a `type` without
counting](#reading-through-a-type-without-counting)) and the parameter is neither retained by the caller nor released
by the callee.

**When.** Every build, for every row; the rules that make it safe are compile errors, not conditions of the
optimisation.

**What you notice.** No allocation per row (`conformance/stage6/vector_rows` pins its count).
[Its case](../benchmarks/a_row_of_borrowed_items_lives_in_the_frame/), 100 000 entities in four `Vector` columns
and two systems, takes 46.6 ms (20 000 000 rows, none allocated), against 8.4 ms for naive C, whose `malloc` per
row clang removes once it inlines the system, and 3.3 ms for expert C; the cost Spite still pays is around the row,
not in it, as the case's README lists. A `__lent_` function appears in the C beside the ordinary
one, which is shaken out when no ordinary call reaches it.

A row filled by a `Symbol` walk is the same struct: the compiler writes the walk out as the literal it
amounts to, in the caller, so the walk's template is not called and is not compiled for that walk, and
`--final-classes` shows no `fill_<attribute>` function for it.

A walked row over sparse columns is the same struct again. Its line is chosen for each attribute while
compiling (`attribute.class == Entity`, `attribute.class.is_fixed_size`), `attribute.index` is written in as a
constant, and `Column<attribute.class>()` is the one singleton for that class, so nothing is looked up by name or
place at run time. An attribute made by a construction of a class that could be a `Vector` item and holds nothing
counted (`Entity(entity)`) is **made in the frame**: a struct beside the row, its defaults set and its
constructor run on it, never allocated and never counted, living exactly as long as the row. The same holds for a
`var` in the template that later lines set or call functions of (`var own = Entity()`, `own.id = entity`,
then the attribute is `own`) when nothing those lines run uses `this` as a value: it is made in the frame before the
row ([memory.md](memory.md#a-row-of-borrowed-items-for-one-call); `conformance/stage6/walked_row_locals` pins the
allocations). Any other counted
attribute, such as a reference read from a reference column, is counted once when the row is made and let go at
the end of the row's block. **What you notice.** No allocation per row for a frame-made attribute
(`conformance/stage6/sparse_rows` pins its count); the walked rows over sparse columns of
[a walked crash line's case](../benchmarks/a_walked_crash_lines_read_is_the_rows_read/), 100 000 entities and 50
ticks, take 81.0 ms, against 21.7 ms for naive C and 4.8 ms for expert C, the gap being finding the places, not the
row. A frame-made object is not
registered with `--debug-memory`'s table, like the row itself, and is not in `.instances`.

**A reference column's element is lent to the row.** When that counted attribute is the result of a function that only
returns an element of its singleton's `List` (a reference column's `at(row)`, `crash references[row]` then `return
references[row]`, since the read is a `T?` and the `crash` is needed, with a whole-number parameter as the index and a
list attribute that nothing assigns after the singleton is made), and nothing the rest of the row's block runs can let
go of anything, the row takes the element uncounted: the compiler writes a copy of the function,
`<name>___lent_element`, that returns the element as it lies in the list (it answers exactly what the function
answers, crash included, when the index is out of range), and neither retains it nor releases it at the end of the
block. The proof walks what the rest of the block runs: every call must be one the compiler can name (a phase
template's `system.phase_each(row)` is named as the phase's own `update_each`), and none of them, nor anything they
call, may let go of an object, remove from or replace into a list, or call through a function value (the proof of [a
list's templates](#a-lists-templates-read-its-elements-without-counting-them), stricter); no class of the program
whose objects can be let go while it runs may have a `drop()` that reaches the column. In a program with threads the
element is also kept from other threads' writes for the rest of the block: the block takes the column's readers' side
or its lock once, where a call would have taken it once anyway, and only when what the block runs reaches no singleton
at all and waits for nothing (the conditions of [a counted
loop](#a-counted-loop-of-calls-to-one-singleton-takes-its-lock-once)), so holding it cannot deadlock; otherwise the
row calls the ordinary function and counts the element, as before. Which of the three it is is decided with the
column's lock, when the program is finished: the C calls `spite_lend_<n>_read(...)` between `spite_lend_<n>_enter()`
and `spite_lend_<n>_leave(...)`, macros that are nothing, the readers' side or lock, or the ordinary counted call and
its release. **What you notice.** Speed, most where the system does not touch the reference: no count, and no
lock taken per row. Nothing a program prints or allocates changes (`conformance/stage6/lent_list_elements`,
`conformance/stage6/lent_list_elements_parallel`: a system that removes from the column, or reads the column itself,
keeps its count).

The arguments a plural value template fills (`system.phase_each(made_arguments(found))`) are written out the same way,
in the caller: when the template's body, folded for an argument, is one `return` of a walked line
(`Column<argument.class>().values[stored_row]`, after `var stored_row = rows[argument.index]`) or a walked row
declared, filled and returned, the compiler writes that value as a local of a C block around the call, one per
argument in order, and calls the function's `___lent_<positions>` copy, in which a borrowed item's parameter is
neither retained nor released. The template is not called for that call, so it is not compiled for it; an argument
whose body has any other shape is its template's ordinary call, as before, and a call none of whose arguments borrows
is left exactly as it was (a game engine's `examples/stress` compiles to the same C). **What you notice.** No copy, no
allocation and no count per argument that borrows (`conformance/stage6/lent_arguments` pins its count).

Any borrowed item passed as an ordinary argument ([memory.md](memory.md#an-item-lent-to-a-call)) takes the
same `___lent_<positions>` copy: `apply(event, mouse, keyboard)` with `mouse` and `keyboard` read from a row calls
`apply___lent_1_2`, which receives the items' addresses and neither retains nor releases them, and a lent
parameter passed on (`press(mouse)`) calls `press___lent_0` in turn. One copy is written per function and set of
lent positions, and only for those a program reaches, so a program that lends nothing carries none. **What you
notice.** No copy and no count per lent argument (`conformance/stage6/lent_to_calls` balances with the writes
read back from the vectors); the `___lent_` functions appear in the C, and the ordinary function is shaken out
when no caller passes it a counted object.

### An `Items`' storage is chosen while compiling

**The case:** [benchmarks/an_items_storage_is_chosen_while_compiling](../benchmarks/an_items_storage_is_chosen_while_compiling/).

**What it does.** `Items<T>` ([collections.md](collections.md#itemst-the-storage-chosen-for-you)) is one
class in `library/items.spite` whose every body that touches an item folds on `$element_type.is_fixed_size`.
For a `T` that fits, only the inline branches are compiled (the item functions of `InlineMemory<T>`, borrowed
reads, a `Vector`'s layout); for any other, only the reference branches (`TypedMemory<T>`, counted references,
a `List`'s layout). A chain of its templates is fused into one loop and `parallel_each_` is split across the
pool as for a `Vector`, and the reference kind's templates read their elements uncounted as a `List`'s do
([above](#a-lists-templates-read-its-elements-without-counting-them)). `[]` checks its range with one comparison
and keeps the crash report in a separate function, `_out_of_range`, so that the read itself is small enough for
the C compiler to inline where it is called.

**When.** Every build, for every `Items<T>`; the choice is a fact of `T`, so it cannot change while the program
runs, and a program that makes no `Items` carries none of it.

**What you notice.** Speed the same as the storage chosen: the C of an `Items<Velocity>` is a `Vector`'s and that
of an `Items<Trail>` a `List`'s, with `[]` inlined where it is called. [Its case](../benchmarks/an_items_storage_is_chosen_while_compiling/),
a generic column holding an `Items` made for an inline and a reference component, 100 000 of each and 100 ticks,
takes 33.5 ms, against 42.9 ms for naive C, a generic array of `malloc`ed components, and 2.4 ms for expert C. An
`Items` object holds one pointer more than a `Vector` or `List` (both helper singletons are
attributes); nothing is added per item. A crash out of range is reported from `Items._out_of_range`.

### A proven divisor is not checked

**The case:** [benchmarks/a_proven_divisor_is_not_checked](../benchmarks/a_proven_divisor_is_not_checked/).

A whole-number `/` or `%` checks its divisor for zero ([values_and_types.md](values_and_types.md)),
unless the divisor is a constant other than zero, or a proof in scope says it is not zero: `assert parts != 0`,
`crash parts != 0`, `if parts != 0 { }` or `parts > 0` in a condition, the same proofs that survive a call that
cannot change `parts` and are dropped across one that may. The check, where it stays, is one compare and a
branch the CPU predicts. What you can observe: nothing but speed; in `conformance/stage6/division_by_zero`, the
proven `whole / pieces` carries no check in its C.

### Arithmetic is checked in every build

**The case:** [benchmarks/arithmetic_is_checked_in_every_build](../benchmarks/arithmetic_is_checked_in_every_build/).

Every `+`, `-` and `*` done in a whole number, signed or unsigned, is the C compiler's overflow builtin in that type,
and an answer that does not fit halts ([values_and_types.md](../specs/values_and_types.md#numeric-types)); so is a `-` in
front of a whole number, the smallest signed value divided by `-1`, and a value put into a narrower name (a compare
against the narrower type's range and a branch). The ordinary build and `--optimized` check exactly as
`--debug-memory` does: there is no unchecked mode, so a benchmark measures the program as it ships. The
`wrapping_sum`, `wrapping_subtract` and `wrapping_multiply` functions are one plain C operation on the bits, so a
hash written with them costs what it did with the wrapping operator. The check is left out of a local counter stepped by one while a `<` on it is in force (`index = index + 1` in a
`while index < count` loop), of one stepped down while a `>` is, and of arithmetic on constants
([proofs.md](proofs.md#arithmetic-that-does-not-fit-halts)). A call between the comparison and the step keeps it
out, whatever the call does, since no call can change a local or a parameter's number: `apply(scorer.score, index)`
before `index = index + 1`, a call through a function value, leaves the step plain. In the compiler's own C these remove 1 145 of its
2 595 checks. It is left out too wherever the ranges the compiler works out for the operands prove the answer fits
([below](#arithmetic-a-range-proves-is-not-checked)), which in the compiler's own C removes 250 of the 1 513 checks
the counter's proof leaves. What you can observe: a halt instead of a wrapped answer, and a loop whose sum the C compiler
vectorised before may no longer be vectorised, since each addition can now stop the program. **Cost, measured** in
[its case](../benchmarks/arithmetic_is_checked_in_every_build/): 400 rounds of adding up `value * 3 + round` over
100 000 numbers take 19.9 ms, against 3.1 ms for naive C and 2.8 ms for expert C, whose unchecked sums the C compiler
vectorises: no range bounds `values[index]`, a number the list was given somewhere else, so every round's checks stay. Where a loop does more than add, the checks cost a few percent; the whole programs
[number_dictionary](../benchmarks/number_dictionary/), [text_building](../benchmarks/text_building/),
[sorting](../benchmarks/sorting/), [vector_maths](../benchmarks/vector_maths/) and
[particles](../benchmarks/particles/) are built with them and timed against C.

### Arithmetic a range proves is not checked

**The case:** [benchmarks/report_over_records](../benchmarks/report_over_records/), whose profit sum and
fingerprints are `Long` totals of `Integer` terms.

**What it does.** The compiler works out, while compiling, the range of values every whole-number local and
parameter can hold at each point of a function, and leaves the overflow check out of a `+`, `-`, `*` or a `-` in
front of a number whose answer, worked out from the ranges of its operands, fits its type. The operator is the same
plain C operator the check was wrapped around; only the compare and the branch that could never be taken go.

The ranges come from what the code says: a literal; a `var` and every assignment to it; a remainder by a constant
(`seed % 16` runs from -15 to 15, and from 0 to 15 when `seed` is never negative); a division by a constant;
`minimum`, `maximum`, `clamp` and `absolute`; a `count()` or `length()`, which is never negative; and a condition
in force, so inside `if value < 10` the value is at most 9, and inside `while index < values.count()` the index is
below the largest count. A loop gives every local it assigns a range that holds on every pass before its condition
is even compiled: a counter only stepped up keeps its starting floor and, under a `<` on it, its bound plus its
steps; anything else is widened to what its assignments can give whatever the loop's own locals hold. A loop whose
counter is stepped once a pass, as one of the body's own statements, under a `<` on it, runs at most a known number
of passes, and a total added to once a pass, `total = total + term`, then holds at most that many terms: a `Long`
total of `Integer` terms over a list, whose count is an `Integer`, can never overflow, so its addition is plain.
An attribute, an item of a list and the answer of a call have their type's range and nothing narrower, since
another function or another pass could change them. The rules in full, with what narrows and what does not, are in
[values_and_types.md](../specs/values_and_types.md#a-range-proves-a-check-unneeded).

**When.** Every build but `--repl`, `--repl-port` and `--hot-reload`, which can run code between two steps of a
loop. Where a range does not prove it, the check stays, and `--optimization-report` lists it with the ranges it
found ("`'product' runs from -2147483648 to 2147483647, so the answer may not fit in an Integer`").

**Example.** No operation below is checked: the remainder and the clamp bound their answers, the counters stay
under their loops' bounds, and each total adds at most as many terms as its loop has passes, so the loop over
`prices` is a plain sum the C compiler can vectorise.

```gdscript title=proven_sum/proven_sum.spite entry
var console = Console()

func ProvenSum() {
    var prices = List<Integer>()
    var index = 0
    while index < 1000 {
        prices.append(index % 250 * 4)
        index = index + 1
    }
    var total: Long = 0
    var at = 0
    while at < prices.count() {
        total = total + prices[at]
        at = at + 1
    }
    var hours = 0
    var week = 0
    while week < 52 {
        var shift = week.clamp(0, 6) + 8
        hours = hours + shift * 5
        week = week + 1
    }
    console.print(total, hours)
}
```
```output
498000 3535
```

**What you notice.** Nothing but speed: the halt where an answer does not fit is unchanged, since the check is left
out only where it could never fire. In [its case](../benchmarks/report_over_records/), the profit of each region in
each quarter is a `Long` sum of `Integer` terms and its additions are plain, as are the seed's multiplication and
remainders and the fingerprint's `Long` sums; the `Integer` products of the attributes stay checked, so the time
is what it was (an attribute's range is its type's). The compiler's own C loses 250 of its 1 513 checks.

### Short text lives inside the `String`

**The case:** [benchmarks/short_text_lives_inside_the_string](../benchmarks/short_text_lives_inside_the_string/).

**What it does.** A `String` is sixteen bytes wherever it is kept (a local, an attribute, a list's element, a
parameter), and text of up to 15 bytes of UTF-8 is kept in those sixteen bytes themselves: no allocation, no
reference count, and no pointer to follow to read it, so a name in a component column is read where the column
already is in the CPU cache. Longer text is one block on the heap (its count, its capacity
and its characters, with a 0 after them for C) that the sixteen bytes point at, next to the length; there is no
separate `String` object. A written text (`"hello"`) is part of the program, whatever
its length: the sixteen bytes point at it, and nothing is counted or freed.

**When.** Every `String`, in every build. Whatever makes text (a join, `slice`, `upper_case()`, a number's
`to_string()`, reading a file, the program's arguments, a foreign function's result) keeps it inside the value when
it fits. [Appending in place](#appending-to-text-in-place) fills the sixteen bytes first and moves the text into a
block, with room to grow, once it passes 15 bytes. Reading a character (`code_at`) looks at the form where the
`String` is kept rather than in a copy, so a loop over the characters of one text (a dictionary hashing its key,
`index_of`, `trim`) decides the form once and then reads one byte per character.

**What you notice.** Fewer allocations under `--debug-memory`: none for short text, one instead of two for long
text (`conformance/stage6/text_building` went from 35 to 31, `singleton_counts` from 200 104 to 200 069 and
`fused_chain_allocations` from 15 to 13, most of it numbers turned into text to be printed). `.memory.section` of
text made while the program runs answers `'stack'` when the text is short and held in a local (`'heap'` when it is
read from an attribute, where the value lives in its object) and `'heap'` when it is long; written text is
`'constant'`, as before. Text passed where a `type` shape is wanted (a `Printable` given to `console.print`,
`attribute.value`) is put in a box, one allocation (written text has a box in the program and allocates nothing) ([below](#boxing-only-where-a-value-travels-as-a-shape)). A `List<String>` holds sixteen bytes
per element instead of an eight-byte pointer. Speed: [its case](../benchmarks/short_text_lives_inside_the_string/),
20 rounds of 100 000 short labels, takes 56.2 ms, against 351.7 ms for naive C, which `malloc`s each label, and
19.7 ms for expert C, which writes them into one array of 16-byte slots; a game engine's `stress` allocates 5.6
million times instead of 7.2. The cost that remains: a `Dictionary` looked up by a key longer than 15 bytes is about
10% slower, since the key travels as sixteen bytes and is compared through its form. Why 15 and not 22: of the 4.7
million texts the compiler makes compiling itself, 68% are 15 bytes or fewer and 78% are 22 or fewer, and 22 would
take a third machine word in every `String` (a `List<String>` half as large again), where 15 fits in the two a
long text needs anyway (where its characters are, and how many).

### Maths on constants is worked out while compiling

**The case:** [benchmarks/maths_on_constants_is_worked_out_while_compiling](../benchmarks/maths_on_constants_is_worked_out_while_compiling/).

**What it does.** A maths function of a number class ([standard_library.md](../specs/standard_library.md#maths))
whose operands are all constants is worked out by the compiler, and the C gets the answer: `(0.5).sine()` is
`(0x1.eaee880000000p-2f)` in the C, not a call. A constant here is a decimal or whole literal, a negated one, a
number class's constant (`Float.pi`), or another folded call, so `Float.pi.sine()` and
`(2.0).square_root().square_root()` fold too. The answer is written as a hexadecimal float, which the C compiler
reads back to exactly those bits, and infinity and not-a-number as `__builtin_inf()` and `__builtin_nan("0x...")`
with the same sign and payload.

**How the answer is the C library's.** The compiler works it out by calling the very function it would have written:
it runs the same member on its own `Float` or `Double` (`bootstrap/source/generation/maths_primitives.spite`), and
that member is lowered, in the compiler as in any program, to the C library's `sinf`, `sqrt`, and so on. The operands
are rounded as the C would round them first (a literal to a `Float` for a `Float` receiver, a `Float` constant widened
exactly for a `Double` one), so the folded bits are the ones the program would have computed at run time, `nan` sign
and all. `conformance/stage6/maths_folding` holds every function, folded against the same call on a value the compiler
cannot see, to be the same bits.

**When.** Every build, when the receiver and every argument are constants as above. A variable is not a constant
here, even one never assigned again: `var angle = 0.5` then `angle.sine()` is a call (which the C compiler may
still fold itself). The whole-number `absolute`, `minimum`, `maximum` and `clamp` are left to the C compiler,
whose integer arithmetic has only one answer.

**What you notice.** Nothing but speed, and no `#include <math.h>` in a program whose only maths is folded. One
thing to know: the answer is the C library of the machine that compiles. A program compiled on one system and run
on another whose C library rounds a last bit differently gets the compiling system's answer for a folded call and
its own for the rest; `sqrt`, `floor`, `ceil`, `round`, `trunc`, `fabs`, `fmin` and `fmax` are exact everywhere, so
only the transcendental functions can differ, by at most that last bit.

### A binary schema is a constant

**The case:** [benchmarks/a_binary_schema_is_a_constant](../benchmarks/a_binary_schema_is_a_constant/).

**What it does.** `BinaryWriter<T>.schema()` and `BinaryReader<T>.schema()` ([json.md](json.md#the-schema-hash)) are
worked out while compiling: the compiler writes the attribute walk of `T` as text, hashes it with FNV-1a, and the C
gets a macro that is the number, with the text beside it in a comment. **When.** Every build, for each `T` a writer or
reader is made for and whose `schema()` is called; nothing is emitted otherwise. **What you notice.** Nothing: no walk
runs and nothing is allocated when a program asks.

### A number's bits are read in place

**The case:** [benchmarks/a_numbers_bits_are_read_in_place](../benchmarks/a_numbers_bits_are_read_in_place/).

**What it does.** `Float.bits()`, `Double.bits()`, `UnsignedInteger.bits_as_float()`, `Long.bits_as_double()`,
`UnsignedLong.bits_as_double()`, and every whole number's `bits_as_unsigned()` or `bits_as_signed()`, are C macros over a union of the two types
([values_and_types.md](../specs/values_and_types.md)): the call is written where it is made
and the value's bits are read as the other type, with no memory written and read back and nothing allocated. Going
through a 4- or 8-byte block instead would also allocate nothing (the frame holds it), but the C would hold a block,
a write and a read for the C compiler to see through.

**When.** Every build, for every call; a program that calls none carries none of them. **What you notice.** Nothing
but speed in an unoptimised build, since from `-O1` clang already sees through either form; it allocates nothing
per conversion either way. [Its case](../benchmarks/a_numbers_bits_are_read_in_place/), thirty million round trips
through a half, takes 73.2 ms, against 96.0 ms for naive C, which reads the bits with `memcpy`, and 24.7 ms for
expert C, which uses the processor's half-precision instructions.

### A local list of known size lives in the frame

**The case:** [benchmarks/a_local_list_of_known_size_lives_in_the_frame](../benchmarks/a_local_list_of_known_size_lives_in_the_frame/).

**What it does.** A local list made by a literal (`var sizes = [3, 5, 8]`) or by `List<T>()` whose size is known
while compiling and which never leaves its function is not allocated: its header and its items are in the
function's own frame, the way [a variadic list](#a-variadic-list-the-callee-only-reads-lives-in-the-callers-frame)
already is. Known size means the literal's items plus the `append`s written as statements of their own in the
same block after it (not inside a loop, a branch or another statement), since each of those runs at most once;
the items get exactly that many slots. Never leaving means every later statement of the block only reads it:
`count()`, `is_empty()`, `[index]`, `get_at`, `first`, `last`, `contains`, `index_of`, `join`, or passing it to a
function of its own class that only reads it too. A list that is returned, stored, assigned, put into another list
or object, changed with `set_at`, `insert`, a `remove_...` or `clear`, handed to a template, or whose `.memory` is
read is made on the heap as before. At the end of the block its items are let go, and nothing else.

A literal of constants (numbers, `Boolean`, text in quotes) that nothing appends to goes further: its items are
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
    var biggest = sizes[2]
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
four items, which would grow its buffer while it was filled): `conformance/stage6/text_building` allocates 17 times
(19 without), `plain_items` 100 (102). The compiler has 115 such lists, 52 of them constant.

### A loop over plain values reads its count once and its items unchecked

**The case:** [benchmarks/a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked](../benchmarks/a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked/).

**What it does.** A `while index < values.count()` over a `List` of numbers or `Boolean` (the only list that holds
them), with `values` a local or a parameter named bare, not an attribute or any other expression, is written as
the plain C loop a C compiler can turn into vector instructions (SIMD), when the compiler can prove three things:

- **The counter stays in range.** `index` is a local `Integer` (not a parameter, and not a `Long` or other
  width) whose every assignment in the function is a whole-number
  literal of 0 or more, or `index = index + 1` as the last statement of a loop bounded by `index < ....count()` or a
  literal, with no other write to it in that loop. So it is never negative, never wraps, and inside the loop it is
  below `values.count()`. Under a window (`while at + 2 < values.count()`, below) the step may be any literal up to
  one more than the window, `at = at + 3` here, so a loop over records of three steps by a record.
- **Nothing in the loop changes a list's size.** The body only declares and assigns numbers and `Boolean`s (its own
  locals, not attributes), reads and writes the items of lists of plain values (`[index]`, `get_at`, `set_at`,
  `first`, `last`, `contains`, `index_of`, `count`, `is_empty`, and on a `List<Byte>` its `read_` and `write_`
  number functions), and calls the maths functions of the number
  classes. Any other call, even one to a function that looks harmless, keeps the loop as it was: a call is where a
  list could be resized through another name.
- **The loop cannot be interrupted.** A `--repl`, `--repl-port` or `--hot-reload` build may run code between two
  passes, so there the loop stays as it was, as it does in a function that waits.

Then `values.count()` is read once before the loop, the address of its items once, and `values[index]`, `values[index]
= x` and `values.set_at(index, x)` read and write the item directly, with no range check: the loop's own condition is
the proof, which is also what narrows the `T?` every `[]` answers, so the read is a plain value with nothing to test.
A window over the list, `while at + 2 < values.count()` (a literal from 1 to 63 added to the counter), is the same
loop: it runs while `at < count - 2`, `values[at]` to `values[at + 2]` are read and written directly, and the step
`at = at + 1` is not checked, since `at + 2` is below the count.
`values.get_at(index)` written by name answers that `T?` and is an ordinary call; write `values[index]`. A second list
indexed by the same counter (`into[index] = from[index] * 1.5`) is checked once instead, before the loop: when it
holds at least as many items as the loop runs, the loop runs without its checks; otherwise the loop as it was runs,
with every check, so what an out-of-range write does (nothing on a `List`) is unchanged. A read of that second list
must be narrowed in the source like any other, since the check before the loop is not a proof the reader wrote. The
loop is written twice for that, so a body with an `assert` or `crash` is not (its crash report would be written
twice).

**What the C compiler then does.** An element-by-element loop (a map in place, into another list, a filter's test)
and a whole-number sum are vectorised, which clang's `-Rpass=loop-vectorize` confirms. The loop's body also tells
clang it may fuse a multiply and an add and reorder the additions of a decimal sum (`#pragma clang fp
contract(fast) reassociate(on)`), so a `Float` or `Double` **sum** is vectorised too: its last bits may differ from
adding in the order written, which the language never promises ([values_and_types.md](values_and_types.md#numbers-are-classes)).
A body that compares a decimal with `==` or `!=`, or a value whose type is not known there, does not get it, so an
exact comparison sees exact values; gcc has no such setting for one loop, so with gcc the sum stays in order. The
compiler adds no `restrict`, since
two names may hold the same list and the C compiler checks for overlap once, before the loop, itself; and no
alignment claim, which nothing proves.

**What you notice.** Speed only. [Its case](../benchmarks/a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked/),
2 000 rounds of scaling one list of 100 000 `Float`s into another and adding it up, takes 39.5 ms, against 159.9 ms
for naive C, which reads each item through the list's pointer and count, and 17.7 ms for expert C, which fuses the
scaling and the sum into one pass over `restrict` arrays.

### A decimal literal beside a `Float` is a `Float`

**The case:** [benchmarks/a_decimal_literal_beside_a_float_is_a_float](../benchmarks/a_decimal_literal_beside_a_float_is_a_float/).

**What it does.** `value * 1.5 + 0.25` with a `Float` `value` is written `value * 1.5f + 0.25f` in the C, so the
arithmetic stays in `Float` precision instead of being widened to `Double` and back, which would halve how many
values a vector instruction holds. A literal beside a `Double`, and two literals together, are written as before.

**When.** Every build, for an operator or comparison whose one side is a decimal literal (or its negation) and whose
other side is a `Float` that is not a literal.

**What you notice.** Speed, and a `Float` result's last bits, which may differ from working it out in `Double`. A
comparison means what it reads as: after `var tenth: Float = 0.1`, `tenth == 0.1` is `true`, where widening
`tenth` made it `false` (`conformance/stage6/float_precision`).

### A proven read tests only its bounds

**The case:** [benchmarks/a_proven_read_tests_only_its_bounds](../benchmarks/a_proven_read_tests_only_its_bounds/).

**What it does.** Every `[]` answers a `T?`, and a read the compiler proves (a loop bound `index < list.count()`, a
proven count, a list literal's indices, the effects of calls, a bound past the index such as `at + 2 < list.count()`,
or a count kept in a `var`) needs nothing written. It also costs no presence test of the `T?`: the compiler reads the
element through the collection's `get_at` and takes the value directly, with one branch the C compiler is told is
never taken, which halts naming the read if the index was outside the list after all (a bound proves only the top of
an index, so a counter that went negative is caught, [failure.md](failure.md#reading-with--answers-t)). When the
index is also known not to be negative (a local only ever set to a literal of 0 or more, or stepped up by one), a
read of a `List` of numbers is the item itself, `items[index_]`, behind the one compare of its top. A read inside
a counted loop ([above](#a-loop-over-plain-values-reads-its-count-once-and-its-items-unchecked)) proves both ends and
is a plain indexed load, `spite_temp[index_]`, with no call and no test at all (`counted_loops`), and the two loops
of [that section's case](../benchmarks/a_loop_over_plain_values_reads_its_count_once_and_its_items_unchecked/)
compile to the same plain loads.

**When.** Every build, for a read of a `List`, `Vector` or `Items` that a range proof narrows. A read narrowed by
`crash`, `assert` or `if` is unwrapped with no test, since that line tested it. A program's own `get_at` is never
unwrapped by a range proof (its `count()` is not known to mean anything).

**What you notice.** Nothing but speed: the same compare `List.get_at` made when it halted out of range, and
the same halt, named by the read (`conformance/stage6/proven_read_outside`).

### A number read from bytes is one load

**The case:** [benchmarks/a_number_read_from_bytes_is_one_load](../benchmarks/a_number_read_from_bytes_is_one_load/).

**What it does.** `bytes.read_integer(position)` and the other reads of a `List<Byte>`
([standard_library.md](standard_library.md#numbers-in-bytes)) are library functions that compare the position with the
count and answer a `T?`. Where a proof covers the read
([proofs.md](proofs.md#a-proven-count-or-bound-proves-a-read-of-a-width)), the compiler writes the read itself in
place of the call: one unaligned load from the list's block (a `memcpy` of the number's width, which the C compiler
makes one instruction), and for a `_big_endian` read one byte swap (`__builtin_bswap32` and its kin) after it. Inside
a counted loop that is all there is, the list's block and count read once before the loop, so a decoder walking
records,

```gdscript
func decode(records: List<Byte>): Long {
    var total: Long = 0
    var position = 0
    while position + 15 < records.count() {
        var identity = records.read_integer_big_endian(position)
        var kind = records.read_short_big_endian(position + 12)
        total = total + identity + kind
        position = position + 16
    }
    return total
}
```

compiles to the loop a C programmer writes over a byte pointer. A proven `write_<number>` in a counted loop is one
store the same way. A counted loop may step by more than one for this: `position = position + 16` last, under a
window of `+ 15`.

**When.** Every build, for a read on a `List<Byte>` named bare (a local, a parameter or an attribute). Outside a
counted loop the read keeps one compare of its top, which the C compiler is told is never taken, since the list
could have been shrunk through another name since the bound; a position not known to be non-negative adds the
compare of its low end. `bytes[index]` on a list of numbers gets the same single compare when its index is known not
to be negative.

**What you notice.** Speed only. [Its case](../benchmarks/a_number_read_from_bytes_is_one_load/) decodes 100 000
big-endian records 300 times in 41.1 ms, against 54.5 ms for naive C, which puts each number together byte by byte,
and 33.0 ms for expert C, which loads and swaps over a `restrict` block; what is left is the checked arithmetic of the
sum.

### A walked `crash` line's read is the row's read

**The case:** [benchmarks/a_walked_crash_lines_read_is_the_rows_read](../benchmarks/a_walked_crash_lines_read_is_the_rows_read/).

**What it does.** Every `[]` answers a `T?`, so a walked row's template states each read with
a `crash` line before the fill ([memory.md](memory.md#a-row-of-borrowed-items-for-one-call)): `var stored_row =
rows[attribute.index]`, `crash Column<attribute.class>().values[stored_row]`, then `row.attributes[attribute] =
Column<attribute.class>().values[stored_row]`. Written out plainly that would read each item twice,
once to test it and once to fill the row, and each read of a generic singleton's column enters its guard. The
compiler reads it once: the walked `crash` line keeps the answer in a local, tests it, and the literal (or a lent
argument, `made_arguments(found)`) takes the same read from that local.

**When.** For the `crash` lines a walk writes out, and only until the row or the call they stand before is written.
A read that answers a counted reference (an element of a `List<T>` column, or of an `Items` whose class does not
fit a `Vector`) is kept counted in that local and let go once the row or call is written, so the row counts it once
more for itself as it always did (`conformance/stage6/sparse_rows` reads its reference column as
`ReferenceColumn<attribute.class>().at(stored_row)`).

**What you notice.** Speed only: a walked row costs the one compare per read that a halting `[]` makes.

### Objects that never leave their function live in the frame

**The case:** [benchmarks/objects_that_never_leave_their_function_live_in_the_frame](../benchmarks/objects_that_never_leave_their_function_live_in_the_frame/).

**What it does.** Every class is passed by reference, so `var moved = position +
velocity.scaled(delta)` reads as two new objects. When the compiler can prove an object never outlives the call
that made it, it is not made on the heap at all: it gets a slot in the function's own frame, the way a buffer
and a list do. Four places use it:

- **A local.** `var name = <a fresh object>` gets a frame slot when nothing after it in its block lets the object
  go (a constructor written with its codegen values counts, `var writer = BinaryWriter<Order>()` or
  `var tally = Tally<Integer>()`, as one whose values are inferred does): it is only read and written through its attributes, handed as the receiver or as an argument to functions
  proven to keep nothing, compared, or asked for its `.memory`. It may be given a new fresh object
  (`position = position + moved`), which is worked out in a second slot and copied into the first; and when the
  function returns its class, `return name` copies it to the heap once, at the return, instead of once per step.
  Anything else lets it go and keeps it on the heap as before: storing it in an attribute, a list or a dictionary,
  returning it from a function that answers some other type, naming it in another variable (`var other = name`),
  passing it to a function that keeps it, to a `Parallel` or a `Concurrent` (their constructors keep it), as a
  function value (`name.update`, which holds the object), to a variadic list (`console.print(name)`), or naming it
  in a text's hole other than as `{name.attribute}`.
- **A result, into the caller's slot.** A function whose every `return` gives a fresh object (`return
  Vector3(...)`, a local that lives in the frame, or another such call; a generic class's constructor whose
  codegen values are inferred counts, both where its arguments' types are known in the caller and in a `return`
  of a function answering that class, and so does one that writes the class's own codegen value out inside its
  own functions, `return Vector3<$number_type>(0, 0, 0)` or `var product = Matrix4<$number_type>()`, which in
  `Vector3<Float>` makes exactly a `Vector3<Float>`) gets a second, hidden version that writes
  its answer into a slot its caller passes, a calling convention chosen per call site (both versions may exist,
  and neither is visible). So `var moved = velocity.scaled(delta)`, whose `moved` stays in the
  frame, calls the hidden version with `moved`'s slot, and nothing is allocated. `Matrix4.multiply`, which builds
  its product in a local and returns it, becomes the same, and so does `velocity = nudged.normalized()`. A
  `return` in the hidden version whose value is not fresh (an attribute, an object read from a list) makes it on
  the heap and copies it into the slot, and `--optimization-report` lists that line under "Objects not in the
  frame".
- **A temporary.** In `a + b + c`, `first + second - third` or `transform.transform_point(point)` passed
  to a function that keeps nothing, each intermediate answer is written into a frame slot of its own.
- **A copy used as a value.** Spite has no value classes: `copy()` is how a program asks for an independent
  object, and a copy that is only used as a value is compiled as a value. `var local = other.copy()` (or
  `other.deep_copy()`, the same thing for a class of numbers) whose `local` never leaves the function is a frame
  slot filled by copying the attributes (a `memcpy` of the object's numbers after the C compiler is done), with no
  heap allocation and no count kept anywhere. Changing `local` never changes `other`, as with any copy.
  `conformance/stage6/frame_objects` pins it: two such copies change `heap.live_allocations()` by 0 (by 2
  without the frame slot).

**Which objects.** An instance of a class whose attributes are all numbers, `Boolean`s, enum values or singletons (a
singleton is bound as an attribute and is never counted), such as `Vector3<Float>`, `Matrix4`, `Quaternion`, a
program's own `Velocity` or a game engine's `Math.Matrix4`, with no `drop()`, that is not a singleton and whose constructor keeps
nothing, and whose class is not read with `.instances` anywhere in the program. What "keeps nothing" means is proven
from the source of each function, parameter by parameter and for the object it is called on: a parameter kept nowhere
in the body (not stored, returned, captured, named in another variable, or passed on to a function that keeps it) is
lent. A function without a body the compiler reads (a foreign or built-in one) and a recursive call are assumed to
keep.

**How it stays safe.** A frame object starts with a reference count of 2^30 that its frame never lets go, so the
count's ordinary ups and downs around calls never free it, and it is never on the heap to be freed. Nothing is kept
anywhere, so no reference outlives the frame, and none reaches another thread: a program cannot tell where it lives
except by asking.

**When.** Every build but the inspectable ones (`--repl`, `--repl-port`, `--hot-reload`, `--development`), where every
object stays an ordinary heap object that reflection and reloading can see, and not in a function that waits. Objects
of classes holding text, lists or other objects are placed in the frame only as a local (below).

**Locals holding text, lists or objects.** An instance of a class whose attributes include text, a list, a
dictionary or another object lives in the frame too when it is a **local** made by its constructor (`var badge =
Badge("visitor")`), under the same conditions otherwise: no `drop()`, not a singleton, not a container, a
constructor that keeps nothing, no `.instances` read, and the same rule for what lets it go. What it holds is
counted as always; only the object itself is not allocated. Its attributes are let go where the local's scope
ends (at the end of its block, a `return` or each pass of a loop), and before a new fresh object is copied in
when the local is given one. `return badge` from a function that answers its class moves the object to the heap
in one allocation, its attributes with it, and nothing is counted up or down. A result written into the caller's
slot, a temporary and a copy used as a value stay limited to classes of numbers.
`conformance/stage6/frame_held_attributes` pins it: a loop making 1 000 such objects, each with a list of tags
and a held object, allocates 3 029 times in all (4 032 without).

**What you notice.** Fewer allocations under `--debug-memory`, and `value.memory.section` answering `'stack'` for a
local that lives in the frame ([memory.md](memory.md#where-a-value-lives-memory)).
[Its case](../benchmarks/objects_that_never_leave_their_function_live_in_the_frame/), 20 000 000 steps of a falling
point, makes 6 allocations in all and takes 15.3 ms, against 1 132.8 ms for naive C, which `malloc`s every answer,
and 15.2 ms for expert C, which holds the numbers by value. The whole program
[game_maths](../benchmarks/game_maths/) (a million `position + velocity.scaled(delta)` steps, 200 000 `Matrix4`
products, a million `transform_point`s) measures the library's `Vector3` and `Matrix4` against the same passes in C
with plain structs: 1.40 times naive C's time, where it took 3.19 while each `accumulated * step_matrix` made its
product on the heap; [vector_maths](../benchmarks/vector_maths/) takes 1.30 times naive C's time, 2.40 while each
`nudged.normalized()` did. On a game engine whose `Math.Matrix4` holds its two singletons as attributes:
`flex_layout` makes 99 789 allocations (100 514 without), `scene_probe` 32 413 (32 518), `render_parity` 14 142 (14
171), with the same output; `stress` keeps its components in columns and makes the same 5 606 191. In `conformance/`,
`lent_arguments` allocates 122 times (130 without), because the `Entity` a generic runner makes for each entity it
hands to a system that keeps nothing is in the frame; `singleton_counts` 64 times (200 065), because the `TallyHolder`
each of its 200 000 passes makes holds only a singleton and is only read, so it is in the frame; `frame_objects` pins
the rest (2 098 allocations without, 86 with). A frame object passed by name to a program function that never assigns
that parameter is not counted at all: the call goes to the function's `___held_` copy
([below](#an-argument-its-caller-holds-is-passed-without-counting)). Passed anywhere else (a library function, one
that assigns the parameter, an inspectable or resumable build) it is counted up and down as any object is (atomically
in a program that starts threads), and the count is never read.

### The C is compiled in parallel units, and cached

**The case:** [benchmarks/the_c_is_compiled_in_parallel_units_and_cached](../benchmarks/the_c_is_compiled_in_parallel_units_and_cached/).

In a default build and an `--optimized` one alike, the C of a program bigger than 1.5 MB is split into a header and up to 64 translation
units, compiled as many at once as the machine has processors and linked, and each unit's object is kept under the
hash of what it was compiled from, so a build that changed nothing only links and a build that changed one function's
body compiles one unit ([compiler.md](compiler.md#translation-units-the-c-compiled-in-parallel-and-cached), where the
rules are). It changes nothing a program does: the same functions and variables, with `static` dropped so another unit
can call them. What you could notice: in a build without link-time optimisation a call from one unit into another is
not inlined by the C compiler (which the default `-O0` build never does anyway, and which `--optimized` recovers,
[below](#a-release-build-is--o3-with-link-time-optimisation)); and the object cache in `.spite/objects` grows to
1 GiB, past which each build removes what was used least recently.

### A release build is `-O3` with link-time optimisation

**The case:** [benchmarks/a_release_build_is_o3_with_link_time_optimisation](../benchmarks/a_release_build_is_o3_with_link_time_optimisation/).

`--optimized` asks the C compiler for `-O3`, and a build from several units adds
ThinLTO (`-flto=thin`, clang) or `-flto=auto` (gcc) so functions are still inlined across units. What you could
notice: the link takes longer, since it is where the optimisation across units happens. The default build is `-O0`,
and it and a `--hot-reload` build add `-march=native`, so they use every instruction of the machine that built them,
where they run; an `--optimized` build, the one shipped, adds nothing of the kind and runs on any processor
([compiler.md](compiler.md#release-builds)). The measurements of each optimisation level are in
[its case's README](../benchmarks/a_release_build_is_o3_with_link_time_optimisation/#each-optimisation-level).

### A release is inlined in every unit

**The case:** [benchmarks/a_release_is_inlined_in_every_unit](../benchmarks/a_release_is_inlined_in_every_unit/).

**What it does.** Letting go of a reference is a count-down and, rarely, the freeing of the object and everything it
holds. The compiler writes each class's retain and release as a small `static inline` function in the header that
every [translation unit](#the-c-is-compiled-in-parallel-units-and-cached) includes, and the freeing as a function
of its own beside them. So the C compiler copies the count-down into every call, in every unit, and calls out only
when the count reaches zero. Before, a release was one function holding both paths, too big for link-time
optimisation to copy into another unit, so a loop that let go of an object in another unit paid a call for every
count-down.

```gdscript title=inlined_release/mover.spite
var speed = 0

func Mover(starting_speed: Integer) {
    speed = starting_speed
}
```
```gdscript title=inlined_release/inlined_release.spite entry
var console = Console()

func InlinedRelease() {
    var movers = List<Mover>()
    var index = 0
    while index < 1000 {
        var mover = Mover(index)
        movers.append(mover)
        index = index + 1
    }
    var every_other = List<Mover>()
    var total = 0
    var position = 0
    while position < movers.count() {
        var mover = movers[position]
        total = total + mover.speed
        every_other.append(mover)
        position = position + 2
    }
    movers.clear()
    var kept = every_other.count()
    console.print(kept, total)
}
```
```output
500 249500
```

Each `Mover` kept in the second list is counted up, and `movers.clear()` counts every `Mover` down, freeing the ones
not kept. The release is written as:

```c
static inline void Mover___release(Mover* self) {
if (self == 0) return;
if (SPITE_COUNT_DOWN(self->header.ref_count) > 0) return;
Mover___free(self);
}
```

and `Mover___free` releases its attributes and frees it, out of line.

**When.** Every build but `--hot-reload`, `--repl` and `--repl-port`, whose releases stay ordinary functions a reload
can replace. A singleton keeps its release, which does nothing. [Identical functions are folded into
one](#identical-functions-are-folded-into-one) leaves the inline functions alone, so a call to one is never made
through a pointer. **What you notice.** Speed, and only in a build of several units: the game engine's stress test
went from 73 ms a tick to 66 ms, and its physics step from 11.5 to 10.1 ms.

### Thread safety for singletons, the rest of the plan

**The case:** [benchmarks/thread_safety_for_singletons_the_rest_of_the_plan](../benchmarks/thread_safety_for_singletons_the_rest_of_the_plan/).

A singleton reached from a `Parallel` is made thread-safe by the compiler, with no keyword, and the compiler picks
the cheapest form that is safe for what that singleton's functions actually do. The forms above are nothing for
read-only state or a singleton no `Parallel` reaches, atomics for counters and flags, the lock as the fallback, and
its readers' side for functions that only read. State that is only appended to (a log, a command queue) gets a
buffer per thread merged in order; state each thread touches its own part of is split per thread. A singleton's
functions may hand out only numbers, text, copies or other singletons made safe the same way, which is a
compile-time check at `return`. All of it is absent from a program that never makes a `Parallel`. You will write
nothing.

### Copies that cost nothing

**The case:** [benchmarks/copies_that_cost_nothing](../benchmarks/copies_that_cost_nothing/).

Every class is passed by reference and `copy()` gives an independent one; that is the whole API, and the compiler
optimises behind it: a copy used only once is passed by value instead of allocated; a copy that is never changed
shares the original, when that is cheaper; an object that never escapes its function is laid out inline or in
registers; and reference counting is left out wherever ownership is provable. You keep writing `copy()` where you mean
an independent object. (An allocator set right after construction is
[above](#an-allocator-set-after-construction-is-where-the-object-is-made), and so is the first part of objects that
never escape: [objects of numbers in the frame](#objects-that-never-leave-their-function-live-in-the-frame), including
a `copy()` of one, and leaving out the count on a frame object passed to a function whose callee never assigns the
parameter.) Frame objects also cover classes that hold text, lists or other objects (their attributes are let go at
the end of the frame), and an attribute object is laid inline in a frame-held object where the attribute is never
shared.

### Identical functions are folded into one

**The case:** [benchmarks/identical_functions_are_folded_into_one](../benchmarks/identical_functions_are_folded_into_one/).

Two versions of one dependency are two different libraries
([packages.md](packages.md#two-versions-of-one-repository)), and what that duplicates must cost nothing. So the
compiler folds every function it generates that is identical to another once both are normalised: the same statements
over types of the same layout, calling functions that are themselves folded together. Every call and every function
value then goes to the one that is kept, cast to the folded function's type where the two are written over different
types; a call of such a function goes through a pointer to the kept one that nothing writes, which every C compiler
accepts without a warning and turns back into a direct call when it optimises. The compiler does this itself, in
every build, rather than leaving it to the C compiler or the linker.

- **When it applies**: to every generated function, whoever wrote it: two versions of one package, two instances of a
  generic class over classes of the same layout (`Column<Position>` and `Column<Velocity>` when both hold the same
  attributes), two classes with the same helper. Before comparing, the compiler sets aside what differs between
  two copies of one function without changing what it does:
  - the function's own name, and the names of the functions it calls, which compare by the group they fold into
    (worked out by splitting groups until every member calls the same groups, so functions that call each other
    fold too);
  - a class's name, which compares by its layout: the same attributes in the same order, each of the same type or
    of a class of the same layout;
  - the numbers of the compiler's own temporaries, and which constant holds a text, which compares by the text;
  - where a failure happened: a crash site and a failed check name their place through the site, not by writing
    it into the function, and a site in two versions of one file, or in one generic function, is one site.
- **When it does not**: functions that differ in a statement, a constant, an attribute or a type's layout are both
  kept, and so is a function with a `static` variable of its own. A `--hot-reload` build and the REPL fold nothing,
  since each function there must be replaceable on its own.
- **What you could notice**: nothing a program can observe. A folded function has one address, so two function values
  of folded functions compare equal where they would compare unequal otherwise, and a native fault's `spite.frame`
  line or a debugger names the kept function, which may be the other version's or the other instance's. A crash in
  code two versions share reports the place in the version the compiler met first, which holds the same line.
  `--final-classes` is unchanged: it prints Spite, not C.
- **Cost**: compile time only; the executable gets smaller.

### Calls in a row run at once

**The case:** [benchmarks/calls_in_a_row_run_at_once](../benchmarks/calls_in_a_row_run_at_once/).

**What it does.** Statements in a row that each call a function on an object of the program, and share nothing
one of them writes, run on the thread pool at once ([concurrency.md](concurrency.md#calls-in-a-row-run-at-once)).
The compiler writes each such row of calls twice, once in order and once overlapped, and keeps one once the whole
program is written: it works out what each call reads and writes from the code of every function it reaches,
after every generic class is made for its values, so `Column<Position>` and `Column<Health>` are told apart.

```gdscript title=calls_in_a_row/evens.spite
var total: Long = 0

func count() {
    var index = 0
    while index < 3000000 {
        total = total + index * 2
        index = index + 1
    }
}
```
```gdscript title=calls_in_a_row/odds.spite
var total: Long = 0

func count() {
    var index = 0
    while index < 3000000 {
        total = total + index * 2 + 1
        index = index + 1
    }
}
```
```gdscript title=calls_in_a_row/calls_in_a_row.spite entry
var console = Console()
var evens = Evens()
var odds = Odds()

func CallsInARow() {
    evens.count()
    odds.count()
    console.print(evens.total, odds.total)
}
```
```output
8999997000000 9000000000000
```

`evens.count()` runs on a worker while `odds.count()` runs on the program's own thread, and the `print` waits for
both. `console.print` is not part of the row: it prints, so it keeps its place.

**When.** Every build but `--hot-reload`, `--repl` and `--development`, for the rows the rules allow. A program
whose rows all stay in order never starts the thread pool for them.

**What you notice.** Speed, when the calls are big enough: an entity system written with no `Parallel`, two systems
over 200 000 entities, runs both at once. A program in which some row overlaps counts atomically the classes the
overlapped calls count, as every program with threads does
([plain counts](#plain-reference-counts-where-no-thread-reaches-a-class)). Nothing a program prints changes.

### A loop over a list of different classes runs them at once

**The case:** [benchmarks/a_loop_over_a_list_of_different_classes_runs_them_at_once](../benchmarks/a_loop_over_a_list_of_different_classes_runs_them_at_once/).

**What it does.** A loop that calls one function on every element of a list of a `type`, `voices.each_render()`,
runs the elements on the thread pool at once when their classes share nothing one of them writes
([concurrency.md](concurrency.md#calls-in-a-row-run-at-once)). It is [calls in a row](#calls-in-a-row-run-at-once)
for a row the program builds while it runs: the compiler cannot know which objects the list will hold, so it
works out, for every two classes the `type`'s call reaches, whether the two calls are independent, from the code of
every function each one reaches, and writes that down as a small table. The loop reads its elements' classes when
it starts (a few comparisons) and runs them at once only when the table allows every two of them; otherwise it is
the loop as written.

```gdscript title=row_of_voices/sine.spite
var level: Long = 0
var phase = 0

func render() {
    var sample = 0
    while sample < 3000000 {
        phase = (phase + 7) % 1000
        level = level + phase
        sample = sample + 1
    }
}
```
```gdscript title=row_of_voices/saw.spite
var level: Long = 0
var phase = 0

func render() {
    var sample = 0
    while sample < 3000000 {
        phase = (phase + 13) % 1000
        level = level + phase % 97
        sample = sample + 1
    }
}
```
```gdscript title=row_of_voices/row_of_voices.spite entry
type Voice {
    render()
}

var console = Console()
var sine = Sine()
var saw = Saw()
var voices = List<Voice>()

func RowOfVoices() {
    voices.append(sine)
    voices.append(saw)
    voices.each_render()
    console.print(sine.level, saw.level)
}
```
```output
1498500000 140985000
```

`sine.render()` runs on a worker while `saw.render()` runs on the program's own thread, and the `print` waits for
both. A list holding the same voice twice, or a voice that prints or reads another's `level`, runs in order.

**When.** Every build but `--hot-reload`, `--repl` and `--development`, for the loops the rules allow. When no two
classes the list could hold may run together, the table is never written and the loop is the plain loop, so a
program that does not have such a loop carries none of it.

**Why at run time.** Which objects a list holds is known only once the program has built it, and a program often
builds it from what it reads or computes (an engine sorts its systems into stages by what each says it touches).
So only the reading of the classes and the table's look-ups run, a few comparisons each time the loop starts; which
classes may run together, which of them are worth a thread, and the code of both forms are all decided while
compiling.

**What you notice.** Speed, when the calls are big enough, and the same output. A program whose loop can run at
once counts atomically the classes the overlapped calls count, as every program with threads does
([plain counts](#plain-reference-counts-where-no-thread-reaches-a-class)), also on the times the table sends it in
order.

### A loop whose passes write only their own item runs in bands

**The case:** [benchmarks/a_loop_whose_passes_write_only_their_own_item_runs_in_bands](../benchmarks/a_loop_whose_passes_write_only_their_own_item_runs_in_bands/).

**What it does.** A loop that calls one function on every element of a list of a class, `orbits.each_advance()`,
runs on the thread pool in bands, one band for each thread, when every pass writes only its own element: the
function (and every function it reaches on its element) writes the element's own attributes and nothing else, and
reads nothing a pass writes on another element. It is [`parallel_each_`](concurrency.md#parallel_each_-a-member-on-every-element)
without writing it: the compiler proves what that page asks the programmer to keep true, and when it cannot, the
loop is the loop as written.

```gdscript title=bands_of_orbits/orbit.spite
var angle = 0.0
var speed = 0.0
var turns = 0

func Orbit(seed: Integer) {
    speed = seed % 17 + 1
}

func advance() {
    var step = 0
    while step < 2000 {
        angle = angle + speed * 0.001
        if angle > 6.28318 {
            angle = angle - 6.28318
            turns = turns + 1
        }
        step = step + 1
    }
}
```
```gdscript title=bands_of_orbits/bands_of_orbits.spite entry
var console = Console()

func BandsOfOrbits() {
    var orbits = List<Orbit>()
    var index = 0
    while index < 3000 {
        var orbit = Orbit(index)
        orbits.append(orbit)
        index = index + 1
    }
    orbits.each_advance()
    var turns = orbits.sum_turns()
    console.print("the orbits turned", turns, "times")
}
```
```output
the orbits turned 7047 times
```

`advance` writes only `angle` and `turns` of the orbit it runs on and reads only its own `speed`, so the 3000
passes are cut into a band for each thread and run at once; `sum_turns` runs after all of them, as written.

**When.** Every build but `--hot-reload`, `--repl` and `--development`, for a `List` or a `Vector` of a class of
the program, when every pass writes only its own element's attributes (or the items of a list attribute made for
it), reads nothing another pass writes, counts no reference, prints, waits, calls out of the program or through a
function value, and does not reach a `Weak`.

**What it costs, and when it is not worth it.** Starting the bands costs about as much as a few thousand light
passes, so the compiler weighs the function while compiling (each statement 1, a loop's body its bound or 8 times,
a call what its function weighs). A pass that weighs less than 256 (a few statements and no loop) is never run in
bands: it is bound by memory, not by the processor, and bands made such passes slower even over four million
elements. For a heavier pass the compiler writes the smallest count at which the bands pay (the count times the
weight reaching a million, about a fifth of a millisecond of work on the machine it was measured on); the loop
compares the list's count with it when it starts and runs in order below it. A list of a class may hold one object twice, and two
bands would then write it at once, so before running in bands the loop checks that each element is held by the
list alone (one pass over the elements' headers, only above that count); a `Vector` holds its elements in place
and needs no check. Above the count, the passes run in bands; below it, or when an element is held elsewhere too,
the loop is the loop as written.

**Why at run time.** How many elements the list holds and who else holds them are known only when the loop runs;
which functions qualify, what they weigh and the count they need are decided while compiling.

**What you notice.** Speed on heavy passes over many elements, and the same output. A pass that counts a
reference (it reads an object out of a list, or makes one) stays in order: counting from several threads would
make the class's counts atomic everywhere ([plain counts](#plain-reference-counts-where-no-thread-reaches-a-class)).

### A crash's report is kept out of the way

**The case:** [benchmarks/a_crashs_report_is_kept_out_of_the_way](../benchmarks/a_crashs_report_is_kept_out_of_the_way/).

**What it does.** Every `crash`, failed `assert`, read outside a list and overflow has code that writes its report:
the place, the values that failed, the attributes of the object it ran on. That code runs at most once, so the
compiler moves each report out of the function that holds it, into a function of its own that is marked cold and
is given exactly the values the report prints. What is left where the check is written is one comparison and a
call that is never made.

```gdscript title=cold_crash/cold_crash.spite entry
var console = Console()

func ColdCrash() {
    var values = [3, 1, 4, 1, 5]
    var picks = [4, 0, 2]
    var total = 0
    var index = 0
    while index < picks.count() {
        var pick = picks[index]
        crash values[pick]
        total = total + values[pick]
        index = index + 1
    }
    console.print(total)
}
```
```output
12
```

The loop above is a comparison, two loads and an addition per pick; the report `crash values[pick]` would print
(the place, `pick`, `index`, `total`) is a separate function, never loaded unless it runs.

**When.** Every build but `--hot-reload`. A report that prints a value the compiler cannot name outside the
function stays where it is.

**What you notice.** Speed, and nothing else: a function with checks is small, so the C compiler copies more of
them into their callers. `List.set_at`, whose two checks had kept it a call of its own, is now copied into every
caller. An engine's row lookup, three `crash` lines and a few reads, went from 551 instructions to 235, and a
naive entity system took 14% fewer instructions per tick. A report reads exactly as before.

### Other optimisations

**The case:** [benchmarks/other_optimisations](../benchmarks/other_optimisations/).

- **An appended item made in the frame**: `var slow = Velocity(1.0, 0.5)` and then `velocities.append(slow)` makes
  `slow` in the frame, when the object is used for nothing else, and `append` copies its attributes into the
  vector's block, as it copies any item: a `Vector`'s or an inline `Items`' `append` keeps nothing of the object it
  is given, so filling one allocates only when the block grows
  ([collections.md](../specs/collections.md#vectort)). `removing_many_at_once`, which fills an `Items` of 200 000
  velocities forty times, went from 3.57 times naive C's time to 2.90.
- **A build report of what could not be optimised**, written only when asked with
  `--optimization-report=file` ([compiler.md](compiler.md#read-what-was-not-optimised)): not "400 copies elided"
  but "3 copies could not be elided, and the line that lets each go", so every line is actionable. It lists each
  `List<T>` that holds references and why `T` cannot be laid inline, each local list and each local object that
  stays on the heap with the line that lets it go, and each `copy()` that allocates. Building it costs compile time
  only, and only with the flag; the program built is the same.

---

Next: [Proofs the compiler makes](proofs.md), every fact the compiler establishes while compiling.
