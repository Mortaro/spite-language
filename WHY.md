# Why Spite

Most code is now written by someone who does not fully know what they are doing, and read by someone skimming
it. That someone might be a person in a hurry, a model that has never seen this codebase, or the language's own
author at two in the morning. Spite's answer is to stop expecting the writer to be careful: the language decides
everything it can, refuses what it cannot accept, and turns the plain, readable version of a program into the
fast one.

## Rust has Rustaceans; Spite has morons

A **moron** is anyone who writes Spite: a human, an AI, or Mortaro, who designed it. It is said with affection,
and everyone is included, the author first. The README's tagline puts it plainly: you write your intention, the
compiler reminds you that you are a moron, and emits the fastest possible code.

The point is not that morons are bad programmers. It is that everyone is a moron some of the time (tired, new to
the code, guessing), so a language should be built for that moment: a moron's code should still come out right,
and still come out fast. Every section below is one way Spite does that.

## Optimisation is a tax, and most code never pays it

If you do not know how to optimise, you cannot ask an AI for what you cannot describe. Programs written by prompt
come out correct-looking and slow: loops inside loops, a list built only to be summed, an allocation for every
small answer. The model writes what you asked for, not what the machine wanted.

And the worst slowdowns are not the ones you chose. They are the ones you never knew to look for: the allocation
per element, the temporary list between two steps, the object made on the heap only to be thrown away a line
later. You cannot fix, or prompt for, what you do not know exists. Spite's compiler looks for them in every
build, so you do not have to.

If you do know how, you still cannot optimise everything. Every optimisation costs something: time, readability,
the next change. Hand-fused loops, hand-specialised generics and data rewritten into packed arrays all make code
harder to read and harder to change, so experts pick their battles. They profile, fix the hot path, and leave the
rest naive, because the deadline is real.

Spite does not pick battles. It does those transformations below the source, everywhere, in every build: a chain
of list operations becomes one loop with no list in between, every generic and template is written out for the
exact types it is used with, nothing is looked up by name while the program runs unless it asks for reflection, a
small object that never leaves its function lives in the frame instead of the heap, identical generated functions are folded into one, and whatever
the program does not use is not in the executable. Your source stays the readable version. When you change it,
the optimisations are redone for free, so nothing goes stale because someone forgot the tuned version. Every one of
them is listed in [docs/optimizations.md](docs/optimizations.md).

In other languages you choose elegant or fast. In Spite you write the elegant one, and the compiler ships the
fast one.

**The proof, measured.** Every benchmark in [benchmarks/](benchmarks/README.md) is one program written three
ways: plainly in Spite, plainly in C the way a C programmer would (naive C), and in C tuned by hand (expert C).
Spite is built `--optimized`, both C programs with `clang -O2`. Each number is how long Spite takes to do the work
divided by how long the C program takes: 1.00 is equal, lower is better. The five whole programs, measured on
2026-10-07 on a machine other work was loading:

| program | Spite's time over naive C's | Spite's time over expert C's |
|---|---|---|
| 100 000 particles stepped in place, 300 ticks | 0.74 | 1.25 |
| quicksort of 2 million integers | 1.20 | 7.52 |
| 3 million text appends, a million words joined | 1.77 | 15.48 |
| 5 million steps of `Vector3` maths | 2.40 | 3.23 |
| 5 million lookups in a dictionary of 500 000 integer keys | 3.79 | 6.94 |

Against expert C, the gap is the work the compiler could still do on its own, and each case's README says where
it goes. The `Vector3` row shows both sides of it. `Vector3` is a class, so every `scaled`, `+` and `cross` made a
new object, and the program took 4.33 times as long as C. Then the compiler learned to keep an object that never
leaves its function in the frame, and the same source, unchanged, took 1.25 times as long. Today `normalized()`'s
answer falls back to the heap again, which is why the row is at 2.40: the case found it, and it is a bug to fix.
The 80-odd cases of single optimisations are in the [summary](benchmarks/README.md#every-benchmark-against-c).

## One way to do each thing

Two ways of doing one thing is a decision somebody has to make again at every call site, so Spite keeps one.
There is one loop (`while`), one way to convert a value (`to_<type>()` on the value), one name per function (no
overloading), no lambdas, no destructuring, no macros. A model has fewer ways to be wrong, and a reader never
wonders why this file does it differently.

## The compiler is the formatter and the linter, and it refuses

Every compile rewrites your files into the one style first. There are no warnings: a program compiles, or it
does not and the compiler says why, naming the fix. Style is part of the language, not advice:

```gdscript
var msg = "hello"
```
```
'msg' abbreviates: write 'message' instead of 'msg'. Spite has no abbreviations outside its own keywords
```

The same goes for a local that is never read, an attribute nothing reads, a check that proves what is already
proven, and dozens of habits from other languages (`&&`, `for`, `++`, `new`, `self.`), each with the Spite form in
the message ([docs/style.md](docs/style.md)).

## Nothing fails silently

Anything that can go wrong silently is a bug in Spite. There are no exceptions, and a failure has three outcomes
and no others: a compile error when the compiler can know, `assert` when a missing answer is fine, and `crash`
when it is a developer mistake. A value that may be absent is a `T?`, and using it without checking is a compile
error. A crash carries no prose message: it names the file and line, and shows the values there.

```
spite.crash	64b935f1	crash_report/crash_report.spite:14	CrashReport	check	value=-9	limit=0
```

Even a fault inside a foreign C library prints where it happened and the Spite line that called it
([docs/failure.md](docs/failure.md#nothing-fails-silently)).

## Templates instead of loops

You say what you want, and the compiler writes the loop:

```gdscript
var standing = monsters.filter_alive().sum_health()
```

Nobody wrote `filter_alive` or `sum_health`. `List` has templates, and the compiler writes the two this line calls,
for `Monster`, typed, then fuses the chain into one loop with no list in between. A hand-written `while` that only
does what a template does is refused with the template to write instead:

```
this 'while' walks every element of 'monsters' only to add up 'health': write 'var total = monsters.sum_health()'
```

Classes are ordinary objects too: every class is an instance of `Spite.Class`, so `Monster.attributes.each(show)`
walks every attribute of `Monster`, and a library can write a function once for every attribute of a class. The
compiler unrolls the walk while compiling, and each call becomes an ordinary typed function. Nothing is looked up while the program runs
([docs/metaprogramming.md](docs/metaprogramming.md)).

## Your architecture, proven by the compiler

The same walks let a library or a framework check your code against its architecture while compiling. A rule is
ordinary Spite: walk the classes of a folder, read a question each one answers, and `crash` where the answer is
wrong. A `crash` whose condition is decided while compiling and is false is a compile error, not a run-time
surprise. This rule says that every class in `component/` holds plain values of a known size only, so no lists
and no references:

```gdscript
func check_component(component: Spite.Class) {
    crash component.is_fixed_size
}
```

`Component.classes.each(check_component)`, called once, walks them all. Add `var items = List<String>()` to `component/inventory.spite`
and the program no longer builds:

```
game.spite:10: error: 'crash component.is_fixed_size' always halts: its condition is decided while compiling and is false, ... (in Game.check_component_for_inventory)
```

The error names the rule and the class that broke it, and it costs nothing at run time. A framework can hold you to
"a component holds data only", "a system's functions end in its phase", "a link holds only an entity" or "no
sentinel values" the same way, with a class's functions, a function's arguments, an attribute's class and the
other questions in [docs/metaprogramming.md](docs/metaprogramming.md). Your architecture stops being a convention people
forget and becomes something the compiler proves, which matters twice over when morons, human or AI, write most
of the code.

## Memory you never think about

You do not manage memory, and you do not pick the fast path: the compiler does, per use. There is no collector, no
pause, no lifetimes to annotate and no runtime to ship. What you write is the plain version; what runs is decided
while compiling:

- **Values that do not need the heap do not reach it.** An object that never leaves its function lives on the
  stack; a returned object is built in the caller's frame instead of on the heap; a function value or a list of arguments the callee
  only reads stays in the caller's frame.
- **Counting only where it is needed.** Every object can be reference counted, but the compiler leaves the count
  out wherever it proves the count would only go up and back down: an item read from a list only to test it or use
  it at once, an argument the caller still holds, an attribute a call cannot change. A count that stays is a plain addition,
  atomic only for classes another thread can really reach.
- **Objects of one class sit together.** The objects a list holds come from their class's own pool, side by side in
  memory; a list held only by another list lives inside it; a list that is refilled keeps its room.
- **Nothing to get wrong by hand.** Where the compiler lends you an item stored inline, it checks at compile time
  that you do not keep it past the call, and the error names the fix. Two objects that hold each other would never
  be freed, so a back reference is a `Weak<T>`.

The direction is further still: the layout of a class chosen per loop (columns instead of objects where the loops
read a few fields), copies that are only made when something writes, and allocation in one block for whatever is
proven not to outlive it. Each optimisation, and what it costs when it cannot apply, is in
[docs/optimizations.md](docs/optimizations.md), measured against plain and hand-tuned C in
[benchmarks/](benchmarks/README.md).

## Concurrency you do not write

There is no `async` and no `await`, and there never will be, and a plain program does not need `Parallel` or
`Concurrent` either: the compiler finds the work that can run at once and runs it at once.

- **Calls in a row that share nothing written run at the same time.** So do the calls of a loop over objects of
  different classes, when their work is independent.
- **A loop whose passes each write only their own item is split across the cores**, in bands sized by a cost model,
  so a light loop stays on one thread where splitting would only cost.
- **A wait inside a frame loop does not hold the frame.** A call that reads a file or a socket is started and
  collected later, so the loop keeps running, and only when the compiler proves no write can be lost.
- **What threads touch is made safe by the compiler**, in its cheapest safe form: no lock where no other thread
  reaches, a lock taken once for a whole loop, atomics only where they are needed.

The direction: every loop split into chunks that fit the processor's cache and spread over every core, decided by
proof and by one check when only the run can tell. `Concurrent` and `Parallel` still exist for the library and for
measuring the compiler against a hand-threaded form ([docs/concurrency.md](docs/concurrency.md)); the moron is
never expected to reach for them.

## Fast builds while you work, fast programs when you ship

The default build is for iterating: the C is compiled at `-O0`, because you rebuild it all day. `--optimized` is
for shipping: `-O3`, link-time optimisation, and the C split into units compiled in parallel and cached. The
numbers, from [the translation units' case](benchmarks/the_c_is_compiled_in_parallel_units_and_cached/README.md#compile-time-at-scale) and
[docs/compiler.md](docs/compiler.md#compile-time):

- The compiler (written in Spite) compiles itself to C in 1.7 seconds of CPU time.
- A data-heavy program that becomes 722 000 lines of C compiles to C in 7.3 seconds; compile time grows linearly.
- A default build of a synthetic 209 206-line program, C compiler included, takes about 15 seconds.
- The default build runs 3 to 7 times slower than an optimised one, which is the trade it makes on purpose.

## A running program that answers questions

Run a program with `--hot-reload --repl-port=4000` and it keeps running while you work. Save a file and only the
changed classes are recompiled and swapped in; live objects move to a class's new attributes. Ask it anything by
its path (`World().player.health`, `describe Monster`), set a breakpoint with `break monster.spite:42` and read the
locals there. Every answer is one line of JSON, so a script or a model can drive it as easily as a person
([docs/repl.md](docs/repl.md)).

## Written by AI, decided by a person

Almost every line of this repository, the compiler, the standard library, the docs and the tests, was written by AI
agents. Every rule of the language was decided by one person, Mortaro, or is marked as an agent's proposal waiting
for his confirmation. That split is deliberate, and it is why Spite is not "vibe coded": vibe coding accepts code
because it seems to work. Here nothing is accepted on that basis.

- **The language is the guardrail.** Spite is meant to be written by models. The quickest way to learn what a model
  needs to be stopped from doing is to let models build the language and watch where they go wrong. When agents
  kept re-checking facts they had already proven, re-proving a known fact became a compile error. When a value
  could go missing without anyone noticing, "anything that can go wrong silently is a bug" became the rule every
  change is judged by. Each mistake found while building Spite becomes something the compiler refuses, so no
  later writer can make it again.
- **Decisions are written down, with their reasons.** Over nine hundred decisions are logged, each with its date,
  who made it and why, so nothing is re-argued from memory and nothing changes without a record.
- **The specification is normative and tested.** Every rule has one page in `specs/`; every titled program in
  the docs is compiled and run by the test suite, along with about a thousand conformance programs and over two
  hundred and fifty programs that must fail with an exact error. The compiler builds itself, and two generations
  must come out byte identical.
- **Speed claims are measured against C.** Every optimisation has a benchmark that compares the plain Spite
  program with the same program written plainly in C and tuned by hand in C, with the C the compiler generated
  kept beside them.

So the AI is not trusted; it is constrained. The more the language refuses, the less it matters who, or what, is
writing it, and that is the property Spite is built to have.

## C today, its own backend as the direction

Spite compiles to C and builds it with the C compiler you bring (`cc`, `clang` or `gcc`), so it runs wherever C
does and the compiler can be rebuilt from one committed C file. The compiler is written in Spite and compiles
itself: generation 2 and generation 3 must be byte identical, every time the tests run. The goal is a backend of
Spite's own, without C; optimisations live in the Spite compiler, not in the C compiler, for that reason.

## Not yet

Spite is weeks old: its first decision is dated 2026-09-19. Only Windows runs today; the Linux and macOS parts of
the standard library compile but have never run. The web target, a language server for editors, a report of what
the compiler could not optimise, and the storage that would keep every list of objects packed (today a `List` of a
class is a list of references) are decided or planned, not built. There is no license file yet. What is not built,
page by page, is in [design/status.md](design/status.md).

## Questions

**How fast is it?** On the five whole programs above, Spite takes 0.74 to 3.79 times as long as the same program
written plainly in C, and 1.25 to 15.48 times as long as C tuned by hand (1.00 is equal, lower is better): it beats
plain C on a loop over packed items, and loses most where the C was tuned. No comparison with Rust, Go or Zig has
been measured. [benchmarks/README.md](benchmarks/README.md)

**How fast does it compile?** The compiler compiles itself to C in 1.7 seconds of CPU; a default build of the
compiler, C included, takes about 11 seconds, and of a 209 206-line program about 15.
[docs/compiler.md](docs/compiler.md#compile-time)

**Is it production-ready?** No. The language changes daily (over 340 recorded decisions since 2026-09-19), and
syntax still changes when a decision calls for it. The compiler is real and compiles itself, but treat Spite as an
experiment. [design/decisions.md](design/decisions.md)

**Which platforms?** Windows today. Linux and macOS compile in the test suite but have never run; the web is
planned. [docs/targets.md](docs/targets.md)

**How do I install it and run hello world?** With git, bash and a C compiler (on Windows, the clang in Visual
Studio is found on its own):

```
git clone https://github.com/Mortaro/spite-language.git
cd spite-language
bin/spite examples/hello
```

`bin/spite` builds the compiler from the committed C the first time. [docs/getting_started.md](docs/getting_started.md)

**Can it call C, and can C call it?** Yes and yes: `DynamicLibrary` calls any exported function as a member, with
no binding file, and `ForeignCallback` hands C a Spite function to call back. The same goes for C++, Rust, Zig and
Go libraries, through the C ABI they export. [docs/foreign_libraries.md](docs/foreign_libraries.md)

**Where are the packages?** You don't need a big ecosystem on day one: any mature library with a C ABI is one
binding away, and it reads like Spite. A binding is a small class that wraps the library so only Spite names,
Spite enums and objects come out of it.
[docs/foreign_libraries.md](docs/foreign_libraries.md#a-binding-speaks-spite)

**How do I test?** A test is a function that `crash`es when a fact is wrong, and `spite tests` runs them (one by
name: `spite tests test_append_and_prepend_keep_order`). There is no framework and no assertion prose: the run stops at the
first broken fact, so failures never cascade, and the report points at the line and shows the values there, which
an AI reads directly. Whatever the compiler can prove is a compile error instead of a test.
[docs/testing.md](docs/testing.md)

**How is memory managed, and is it safe?** Reference counting, no garbage collector. Absence is a `T?` you must
check, a read past the end of a list answers nothing instead of reading memory, borrowed items are checked at
compile time, and a fault prints where it happened. Two objects holding each other leak unless one holds a `Weak`.
[docs/memory.md](docs/memory.md)

**Is there garbage collection?** No, and no pause. [docs/memory.md](docs/memory.md)

**How do errors work without exceptions?** A compile error, `assert` (the function answers "nothing"), or `crash`
(the program halts and shows the line and the values). [docs/failure.md](docs/failure.md)

**What does concurrency look like?** `Concurrent(f)` and `Parallel(f)` at the call site, no `async`; reading the
handle is the wait. Built on Windows. [docs/concurrency.md](docs/concurrency.md)

**What tooling is there?** The compiler is the formatter and linter; a REPL, live reload and breakpoints come
with it; `--debug-memory` counts every allocation. No language server or editor highlighting yet.
[docs/repl.md](docs/repl.md)

**Is there a package manager?** No, by design: a dependency is a git URL pinned to a commit in a `load` line, and
the ordinary compile fetches it. No registry, manifest or lockfile. [docs/packages.md](docs/packages.md)

**How big is the standard library?** About a hundred files, all of them Spite: text, lists, dictionaries, files,
folders, processes, sockets, HTTP, JSON and binary, time and time zones, game maths, hashing and compression.
[docs/standard_library.md](docs/standard_library.md)

**Games, servers, command-line tools?** All three: the standard library has game maths and inline storage for
games, sockets and an HTTP server, and the compiler itself is a command-line tool written in Spite.

**Who maintains it, and under what license?** Mortaro designs it and decides every rule; most of the code is
written by AI agents under his direction. There is no license file yet.

**How does it compare?** Rust: memory safety by counting and compile-time borrow checks of stored items, with no
lifetimes and no async colouring. Zig: both take C seriously, but Spite chooses for you where Zig hands you the
allocator. Go: no garbage collector, and generics written out per type. Odin and Jai: aimed at the same game
programmers, but Spite counts references and enforces one style instead of trusting the programmer.

**Why the name?** It is named after the things that cause its author spite, listed with what to do instead in
[SPITE.md](SPITE.md).

**Why trust a language mostly written by AI?** Because nothing is taken on trust: the compiler compiles itself
to byte-identical output twice, and `bash check.sh` compiles and runs every titled program in the docs, every
conformance program and every expected compile error, requiring exact output and balanced allocations.

For a version of this page written for language models, see [WHY_AI.md](WHY_AI.md).
